"""Paired symbolic-time Factory replay using the frozen B5 work split."""

import argparse
import json
import math
import statistics
import sys
from collections import Counter, defaultdict
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(TOOLS / "b5"), str(TOOLS / "b6")]
from b5_benchmark import l1_length, verify_freeze
from b5_data import read_jsonl, sha256, stable, write_json, write_jsonl
from b5_factory import catalog_index, distance, evaluate, first_hit, motif
from b5_model import Model
from b6_factory import catalog_windows, diagnose
from b6_reliability import load_adapted

CONDITIONS = ("real", "grid", "equal_span", "b6_on")


def close(a, b):
    return math.isclose(a, b, rel_tol=0, abs_tol=1e-7)


def make_input(row, durations):
    prefix = []
    onset = row["prefix"][0]["phase"]
    for event, duration in zip(row["prefix"], durations, strict=True):
        prefix.append(dict(event, onset=onset, duration=duration))
        onset += duration
    return dict(row, prefix=prefix)


def prepare(args):
    manifest = verify_freeze(args.benchmark)
    development = {row["group"] for row in read_jsonl(args.benchmark / "factory_queries.jsonl")}
    used_b6 = {row["id"] for path in (args.benchmark / "factory_queries.jsonl",
                                      args.b6 / "cohorts/confirmation.jsonl") for row in read_jsonl(path)}
    # Reuse complete source lines from the frozen test group. De-duplicate
    # literal repeats within a work while keeping repeated structures across works.
    split = args.split
    query_file = f"queries_{split}.jsonl"
    rows = [dict(id=row["id"], group=row["group"], kind=row["kind"],
                 prefix=row["prefix"], target=row["target"])
            for row in read_jsonl(args.benchmark / query_file)
            if row["kind"] == "source_line"]
    seen, selected_works, excluded = set(), set(), Counter()
    conditions = defaultdict(list)
    rows.sort(key=lambda row: stable("b7-timing", row["id"]))
    for row in rows:
        prefix, target = row["prefix"], row["target"]
        events = prefix + target
        if l1_length(target) < 3:
            excluded["fewer_than_three_L1_events"] += 1
            continue
        meters = {event["meter"] for event in events}
        if len(meters) != 1 or None in meters:
            excluded["meter_change_or_unknown"] += 1
            continue
        if any(event["unit"] != "qn" or event["phase"] is None or not event["duration"]
               or event["duration"] <= 0 for event in events):
            excluded["unsupported_qn"] += 1
            continue
        # Some earlier unmeasured gaps invalidate whole-song onset. Source
        # runs are contiguous by construction. Re-check the locally known
        # metric phase and any absolute onsets available in the frozen data.
        n, d = map(int, next(iter(meters)).split("/"))
        bar = 4 * n / d
        if any((not close((a["phase"] + a["duration"] - b["phase"]) % bar, 0)
                and not close((a["phase"] + a["duration"] - b["phase"]) % bar, bar))
               or (a["onset"] is not None and b["onset"] is not None
                   and not close(a["onset"] + a["duration"], b["onset"]))
               for a, b in zip(events, events[1:])):
            excluded["discontinuous_metric_phase"] += 1
            continue
        # Verify the source line starts on a bar boundary and ends on one.
        if not close(target[0]["phase"], 0) or not close(sum(e["duration"] for e in target) % bar, 0):
            excluded["source_row_not_whole_bars"] += 1
            continue
        if split == "validation":
            if meters != {"4/4"} or row["group"] in selected_works:
                excluded["outside_validation_4_4_one_per_work"] += 1
                continue
            selected_works.add(row["group"])
        signature = (row["group"], n, d, len(prefix),
                     tuple((e["root"], e["detail"], e["bass"], e["tonic"], round(e["phase"], 8),
                            round(e["duration"], 8)) for e in events))
        if signature in seen:
            excluded["literal_repeat_within_work"] += 1
            continue
        seen.add(signature)
        row["b6_group_role"] = "development_group" if row["group"] in development else "other_test_group"
        row["exact_b6_replay_query"] = row["id"] in used_b6
        durations = [e["duration"] for e in prefix]
        mean = sum(durations) / len(durations)
        conditions["real"].append(make_input(row, durations))
        conditions["grid"].append(make_input(row, [4.0] * len(prefix)))
        conditions["equal_span"].append(make_input(row, [mean] * len(prefix)))
    args.output.mkdir(parents=True, exist_ok=True)
    paths = {name: args.output / f"{name}.jsonl" for name in conditions}
    for name, cohort in conditions.items():
        write_jsonl(paths[name], cohort)
    real = conditions["real"]
    if split == "test":
        by_work = {}
        for row in real:
            if row["prefix"][0]["meter"] == "4/4" and row["prefix"][-1]["duration"] != 4:
                by_work.setdefault(row["group"], row)
        open_rows = [dict(row, prefix=[*row["prefix"][:-1], dict(row["prefix"][-1], duration=None)])
                     for row in list(by_work.values())[:24]]
        paths["open"] = args.output / "open.jsonl"
        write_jsonl(paths["open"], open_rows)
    write_json(args.output / "cohort.json", dict(
        seed="b7-timing", split=split, source_lines=len(rows), queries=len(real),
        groups=len({row["group"] for row in real}), excluded=excluded,
        frozen_inputs=manifest["files"][query_file],
        files={name: sha256(path) for name, path in paths.items()}))


def work_average(rows, measure):
    groups = defaultdict(list)
    for row in rows:
        groups[row["group"]].append(measure(row))
    return statistics.mean(statistics.mean(values) for values in groups.values())


def measures(rows):
    result = {"queries": len(rows), "groups": len({row["group"] for row in rows})}
    names = [name for name in CONDITIONS if name in rows[0]]
    for name in names:
        samples = [row[name] for row in rows]
        times = sorted(s["worker_ms"] for s in samples)
        result[name] = dict(
            exact=sum(s["original_exact"] is not None for s in samples),
            near=sum(s["original_near"] is not None for s in samples),
            recall3=sum(0 < (s["original_near"] or 0) <= 3 for s in samples),
            recall10=sum(0 < (s["original_near"] or 0) <= 10 for s in samples),
            ui_recall3=sum(0 < (s["original_near_ui_row"] or 0) <= 3 for s in samples),
            ui_recall10=sum(0 < (s["original_near_ui_row"] or 0) <= 10 for s in samples),
            mrr=work_average(rows, lambda r: 1 / r[name]["original_near"] if r[name]["original_near"] else 0),
            work_macro_near=work_average(rows, lambda r: int(r[name]["original_near"] is not None)),
            factory_count=sum(s["factory_count"] for s in samples),
            exact_qualified=sum(s["exact_qualified"] for s in samples),
            context_qualified=sum(s["context_qualified"] for s in samples),
            structure_qualified=sum(s["structure_qualified"] for s in samples),
            median_ms=statistics.median(times), p95_ms=times[(95 * len(times) - 1) // 100],
        )
    result["catalog_near"] = sum(row["real"]["catalog_near"] for row in rows)
    result["catalog_source_seam_near"] = sum(row["real"]["catalog_seam_near"] for row in rows)
    for name in names[1:]:
        result[f"paired_real_vs_{name}"] = {
            "real_only": sum(bool(row["real"]["original_near"]) and not row[name]["original_near"] for row in rows),
            "comparison_only": sum(not row["real"]["original_near"] and bool(row[name]["original_near"]) for row in rows),
            "same_source_position_count": sum(row[f"shared_source_positions_{name}"] for row in rows),
            "real_only_source_position_count": sum(row[f"real_only_source_positions_{name}"] for row in rows),
            "comparison_only_source_position_count": sum(row[f"other_only_source_positions_{name}"] for row in rows),
        }
    return result


def compare(args):
    cohort = json.loads((args.cohorts / "cohort.json").read_text(encoding="utf-8"))
    query_file = args.cohorts / "real.jsonl"
    for name, expected in cohort["files"].items():
        if sha256(args.cohorts / f"{name}.jsonl") != expected:
            raise ValueError(f"{name}: cohort identity differs")
    catalog_path = args.replays / "real" / "factory_catalog.jsonl"
    content, with_seam = catalog_index(catalog_path)
    conditions = ("real", "grid") if cohort["split"] == "validation" else CONDITIONS
    for condition in conditions:
        if sha256(args.replays / condition / "factory_catalog.jsonl") != sha256(catalog_path):
            raise ValueError(f"{condition}: catalog identity differs")
    readouts = [read_jsonl(args.replays / condition / "factory_replay.jsonl") for condition in conditions]
    open_rows = {}
    if cohort["split"] == "test":
        source = args.cohorts / "open.jsonl"
        output = args.replays / "open" / "factory_replay.jsonl"
        for probe, replay in zip(read_jsonl(source), read_jsonl(output), strict=True):
            assert probe["id"] == replay["id"]
            open_rows[probe["id"]] = replay
    select = json.loads((args.benchmark / "selection.json").read_text(encoding="utf-8"))
    model_path = args.benchmark / select["model"]
    if sha256(model_path) != select["sha256"]:
        raise ValueError("B5 model identity differs")
    original = Model.load(model_path)
    adapted = load_adapted(args.benchmark, args.b6 / "reliability/selection.json")
    rows = []
    for query, *replays in zip(read_jsonl(query_file), *readouts, strict=True):
        anchor = query["prefix"][-1]["root"]
        target = motif(query["target"], anchor)
        row = dict(id=query["id"], group=query["group"], role=query["b6_group_role"],
                   b6_query=query["exact_b6_replay_query"], meter=query["prefix"][0]["meter"],
                   target_length=len(target),
                   uniform_prefix=len({event["duration"] for event in query["prefix"]}) == 1)
        locations = {}
        real = replays[0]
        if "b6_on" in conditions:
            on = replays[-1]
            row["b6_generated_identity_same"] = real["generated_identities"] == on["generated_identities"]
            previous = {candidate["fingerprint"]: candidate for candidate in real["candidates"]}
            changed = {candidate["fingerprint"]: candidate for candidate in on["candidates"]}
            for identity in previous.keys() & changed.keys():
                assert previous[identity]["events"] == changed[identity]["events"], (query["id"], identity)
            row["b6_added"] = len(changed.keys() - previous.keys())
            row["b6_removed"] = len(previous.keys() - changed.keys())
        for condition, replay in zip(conditions, replays, strict=True):
            assert replay["id"] == query["id"]
            positions = {(c["source"], c["key"], c["next"]) for c in replay["candidates"]}
            locations[condition] = positions
            row[condition] = evaluate(query, replay, content, with_seam, None, None)
            row[condition].update(exact_qualified=replay["exact_qualified"], context_qualified=replay["context_qualified"],
                                  structure_qualified=sum(c["structure_qualified"] for c in replay["candidates"]))
            hit = row[condition]["original_near"]
            row[condition]["nearest_source"] = list((c["source"], c["key"], c["next"])
                for c in replay["candidates"][hit - 1:hit]) if hit else []
            if condition != "real":
                row[f"shared_source_positions_{condition}"] = len(positions & locations["real"])
                row[f"real_only_source_positions_{condition}"] = len(locations["real"] - positions)
                row[f"other_only_source_positions_{condition}"] = len(positions - locations["real"])
        # Require competing near candidates as well as a usable rank depth.
        # This diagnostic cohort is not a population-wide uplift estimate.
        candidates = real["candidates"]
        near = [c for c in candidates if distance(target, motif(c["events"], anchor)) <= 1]
        row["qualified_near_count"] = len(near)
        if cohort["split"] == "test" and len(candidates) >= 10 and len(near) >= 2:
            row["ranking"] = {"original": row["real"]["original_near"]}
            for label, model in (("b5", original), ("b6", adapted)):
                scored = evaluate(query, real, content, with_seam, model, select["score"])
                row["ranking"][label] = scored["statistical_near"]
                row["ranking"][f"{label}_hybrid"] = scored["hybrid_near"]
        if query["id"] in open_rows:
            reopened = open_rows[query["id"]]
            probe = evaluate(query, reopened, content, with_seam, None, None)
            row["open_probe"] = dict(factory_count=probe["factory_count"], near=probe["original_near"],
                                     closed_near=row["real"]["original_near"])
        rows.append(row)
    cohorts = defaultdict(list)
    for row in rows:
        for tag in ("all", row["role"], f"meter:{row['meter']}",
                    "uniform_prefix" if row["uniform_prefix"] else "varied_prefix",
                    "target_3_4" if row["target_length"] <= 4 else "target_5_plus"):
            cohorts[tag].append(row)
        if row["meter"] == "4/4":
            cohorts["primary_4_4"].append(row)
            cohorts["primary_4_4_varied" if not row["uniform_prefix"] else "primary_4_4_uniform"].append(row)
        if not row["b6_query"]:
            cohorts["new_queries"].append(row)
            if row["role"] == "other_test_group":
                cohorts["new_queries_other_test_group"].append(row)
    ranking = {}
    for cohort_name in ("all", "primary_4_4", "new_queries", "new_queries_other_test_group"):
        selected = [row for row in cohorts.get(cohort_name, []) if "ranking" in row]
        ranking[cohort_name] = dict(
            eligible=len(selected), groups=len({row["group"] for row in selected}),
            results={name: dict(top3=sum(row["ranking"][name] <= 3 for row in selected),
                                top10=sum(row["ranking"][name] <= 10 for row in selected),
                                mrr=work_average(selected, lambda row: 1 / row["ranking"][name]))
                     for name in selected[0]["ranking"]} if selected else {})
    report = dict(cohorts={name: measures(items) for name, items in cohorts.items()}, per_query=rows, ranking=ranking)
    if open_rows:
        probes = [row["open_probe"] for row in rows if "open_probe" in row]
        report["open_probe"] = dict(n=len(probes), closed_near=sum(bool(row["closed_near"]) for row in probes),
                                    open_near=sum(bool(row["near"]) for row in probes),
                                    closed_only=sum(bool(row["closed_near"]) and not row["near"] for row in probes),
                                    open_only=sum(bool(row["near"]) and not row["closed_near"] for row in probes))
    write_json(args.output, report)


def trace_inputs(args):
    """Target-aware probes for exit attribution only. Worker ignores witnesses."""
    catalog = args.replays / "real" / "factory_catalog.jsonl"
    windows = catalog_windows(catalog)
    report = json.loads((args.replays.parent / "timing-results.json").read_text(encoding="utf-8"))
    query_file = args.cohorts / "real.jsonl"
    cohorts = json.loads((args.cohorts / "cohort.json").read_text(encoding="utf-8"))
    if sha256(query_file) != cohorts["files"]["real"]:
        raise ValueError("trace input identity differs")
    inputs = []
    for query, case in zip(read_jsonl(query_file), report["per_query"], strict=True):
        assert query["id"] == case["id"]
        if case["meter"] != "4/4" or case["real"]["original_near"]:
            continue
        if not (case["real"]["catalog_near"] or case["real"]["catalog_seam_near"]):
            continue
        goal = motif(query["target"], query["target"][0]["root"])
        seam_goal = motif(query["target"], query["prefix"][-1]["root"])
        witnesses = [[source, index] for source, index, shape, seam, _, _ in windows
                     if (abs(len(goal) - len(shape)) <= 1 and distance(goal, shape) <= 1)
                     or (seam is not None and abs(len(seam_goal) - len(seam)) <= 1
                         and distance(seam_goal, seam) <= 1)]
        inputs.append(dict(query, witnesses=witnesses,
                           trace_sources=sorted({source for source, _ in witnesses})))
    write_jsonl(args.output, inputs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage", choices=("prepare", "compare", "trace_inputs", "diagnose"))
    for option in ("benchmark", "b6", "cohorts", "replays", "output"):
        parser.add_argument(f"--{option}", type=Path, required=option == "output")
    parser.add_argument("--queries", type=Path)
    parser.add_argument("--replay", type=Path)
    parser.add_argument("--split", choices=("test", "validation"), default="test")
    args = parser.parse_args()
    {"prepare": prepare, "compare": compare, "trace_inputs": trace_inputs, "diagnose": diagnose}[args.stage](args)


if __name__ == "__main__":
    main()
