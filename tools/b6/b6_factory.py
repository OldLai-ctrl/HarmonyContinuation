"""Prepare work-disjoint Factory probes and account for retrieval exits."""

import argparse
import json
import statistics
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "b5"))
from b5_benchmark import factory_inputs, l1_length, verify_freeze
from b5_data import read_jsonl, sha256, stable, write_json, write_jsonl
from b5_factory import distance, evaluate, motif
from b5_model import Model


def catalog_windows(path):
    windows = []
    for row in read_jsonl(path):
        events = row["events"]
        for index, item in enumerate(events):
            if item["root"] is None:
                continue
            if index and (events[index-1]["root"], events[index-1]["quality"]) == (item["root"], item["quality"]):
                continue
            tail = events[index:]
            shape = motif(tail, item["root"])
            seam = motif(tail, events[index-1]["root"]) if index and events[index-1]["root"] is not None else None
            key = row.get("tonic", 0) + 12 * row.get("mode", 0)
            windows.append((row["source"], index, shape, seam, key, tail))
    return windows


def prepare(args):
    frozen = verify_freeze(args.benchmark)
    args.output.mkdir(parents=True, exist_ok=True)
    inputs = list(read_jsonl(args.benchmark / "factory_queries.jsonl"))
    by_id = {query["id"]: query for query in inputs}
    coverage = json.loads((args.b5_replay / "factory_evaluation.json").read_text(encoding="utf8"))
    if coverage["inputs"]["query_sha256"] != frozen["files"]["factory_queries.jsonl"]:
        raise ValueError("B5 Factory result uses a different freeze")
    windows = catalog_windows(args.b5_replay / "factory_catalog.jsonl")
    diagnostic = []
    for result in coverage["per_query"]:
        if not result["catalog_near"] or result["original_near"] is not None:
            continue
        query = by_id[result["id"]]
        goal = motif(query["target"], query["target"][0]["root"])
        witnesses = [[source, index] for source, index, shape, _, _, _ in windows
                     if abs(len(goal) - len(shape)) <= 1 and distance(goal, shape) <= 1]
        diagnostic.append(dict(query, witnesses=witnesses, trace_sources=sorted({item[0] for item in witnesses})))
    if len(diagnostic) != 62:
        raise ValueError("expected 62 frozen Factory misses")
    write_jsonl(args.output / "diagnostic.jsonl", diagnostic)
    used = {row["group"] for row in inputs}
    billboard = factory_inputs(query for query in read_jsonl(args.benchmark / "queries_test.jsonl")
                               if query["group"] not in used)
    mpb = {}
    for query in sorted(read_jsonl(args.benchmark / "queries_cross_source.jsonl"),
                        key=lambda row: stable("b6", row["id"])):
        if query["group"] in used or query["group"] in mpb or not 3 <= l1_length(query["target"]) <= 6:
            continue
        # MPB supplies chord order only. The equal grid is a test fixture,
        # independent of measure positions, and carries no inferred meter.
        prefix = [dict(event, unit="qn", onset=4.0 * i, duration=4.0, meter=None, phase=None)
                  for i, event in enumerate(query["prefix"])]
        mpb[query["group"]] = dict(id=query["id"], group=query["group"], kind="mpb_equal_grid",
                                   prefix=prefix, target=query["target"])
    confirmation = [*billboard, *list(mpb.values())[:60]]
    assert len({item["group"] for item in confirmation}) == len(confirmation)
    assert not used.intersection(item["group"] for item in confirmation)
    write_jsonl(args.output / "confirmation.jsonl", confirmation)
    write_json(args.output / "cohorts.json",
               dict(seed="b6", b5_freeze_sha256=sha256(args.benchmark / "freeze.json"),
                    diagnostics=len(diagnostic), billboard_confirmation=len(billboard),
                    mpb_confirmation=len(confirmation) - len(billboard),
                    confirmation_sha256=sha256(args.output / "confirmation.jsonl")))


def diagnose(args):
    queries = list(read_jsonl(args.queries))
    rows = list(read_jsonl(args.replay / "factory_replay.jsonl"))
    catalog = {(source, index): (shape, seam, key, tail)
               for source, index, shape, seam, key, tail in catalog_windows(args.replay / "factory_catalog.jsonl")}
    categories = Counter()
    exact_reasons, contextual_reasons = Counter(), Counter()
    report = []
    for query, row in zip(queries, rows, strict=True):
        assert query["id"] == row["id"]
        source_witnesses = {tuple(witness) for witness in query["witnesses"]}
        anchor = query["prefix"][-1]["root"]
        goal = motif(query["target"], anchor)
        related_keys = set()
        for source, index in source_witnesses:
            shape, seam, key, tail = catalog[source, index]
            for tonic in range(12):
                shift = (tonic - key % 12) % 12
                projected = tuple(((root + tail[0]["root"] + shift - anchor) % 12, quality)
                                  for root, quality in shape)
                if distance(goal, projected) <= 1:
                    related_keys.add((source, index, tonic + 12 * (key // 12)))
        def eligible_trial(trial):
            return (trial["source"], trial["next"], trial["key"]) in related_keys

        exact = [trial for trial in row["exact_trace"] if eligible_trial(trial)]
        context = [trial for trial in row["context_trace"] if eligible_trial(trial)
                   or (trial["disposition"] == "meter" and (trial["source"], trial["next"]) in source_witnesses)]
        context_constructed = [trial for trial in context if trial["disposition"] not in
                               ("internal-center", "source-context", "terminal-prototype", "center-pivot", "meter", "retrieved")]
        exact_reasons.update(trial["disposition"] for trial in exact)
        contextual_reasons.update(trial["disposition"] for trial in context)
        qualifying = [trial for trial in exact if trial["disposition"] in ("TARGET_RETAINED", "EXPLORATORY_RETAINED")]
        qualifying += [trial for trial in context if trial["disposition"] == "qualified-before-structure"]
        if qualifying:
            # Raw catalogue and actual realized events can differ. Trace
            # identity alone cannot prove a lost near-match.
            category = "qualified_source_projection_check"
        elif exact or context_constructed:
            category = "examined_then_rejected"
        elif row["exact_trace"] or context:
            category = "no_matching_window_or_center"
        else:
            category = "no_source_trace"
        seam = any(catalog[w][1] is not None and distance(goal, catalog[w][1]) <= 1 for w in source_witnesses)
        categories[category] += 1
        report.append(dict(id=query["id"], category=category, source_seam=seam,
                           exact=exact, contextual=context, qualified=qualifying,
                           alternate_endpoints=len(row["exact_trace"]) - len(exact)))
    write_json(args.output, dict(cases=report, categories=categories,
                                 exact_dispositions=exact_reasons, contextual_dispositions=contextual_reasons))


def summarize(rows, method):
    result = {}
    for cut in ("exact", "near"):
        key = f"{method}_{cut}"
        ranks = [row[key] for row in rows]
        reachable = [rank for rank in ranks if rank is not None]
        result[cut] = dict(n=len(ranks), reachable=len(reachable),
                           top3=sum(rank <= 3 for rank in reachable), top10=sum(rank <= 10 for rank in reachable),
                           mrr_all=sum(1 / rank for rank in reachable) / len(rows),
                           mrr_reachable=statistics.mean(1 / rank for rank in reachable) if reachable else None)
    return result


def compare(args):
    from b6_reliability import load_adapted
    queries = list(read_jsonl(args.queries))
    baseline = list(read_jsonl(args.baseline / "factory_replay.jsonl"))
    changed = list(read_jsonl(args.replay / "factory_replay.jsonl"))
    selection = json.loads((args.benchmark / "selection.json").read_text())
    model_path = args.benchmark / selection["model"]
    if sha256(model_path) != selection["sha256"]:
        raise ValueError("B5 model hash differs")
    model = Model.load(model_path)
    adapted = load_adapted(args.benchmark, args.reliability / "selection.json")
    from b5_factory import catalog_index
    content, seam = catalog_index(args.replay / "factory_catalog.jsonl")
    cases = []
    for query, old, new in zip(queries, baseline, changed, strict=True):
        assert query["id"] == old["id"] == new["id"]
        assert old["generated_identities"] == new["generated_identities"], query["id"]
        old_music = {row["fingerprint"]: row for row in old["candidates"]}
        new_music = {row["fingerprint"]: row for row in new["candidates"]}
        common = old_music.keys() & new_music.keys()
        for identity in common:
            assert old_music[identity]["events"] == new_music[identity]["events"], (query["id"], identity)
        before = evaluate(query, old, content, seam, None, None)
        after = evaluate(query, new, content, seam, model, selection["score"])
        reliable = evaluate(query, new, content, seam, adapted, selection["score"])
        cases.append(dict(id=query["id"], kind=query["kind"], original_exact=before["original_exact"],
                          original_near=before["original_near"], retrieved_exact=after["original_exact"],
                          retrieved_near=after["original_near"], statistical_exact=after["statistical_exact"],
                          statistical_near=after["statistical_near"], hybrid_exact=after["hybrid_exact"],
                          hybrid_near=after["hybrid_near"], adapted_exact=reliable["hybrid_exact"],
                          adapted_near=reliable["hybrid_near"], adapted_pure_near=reliable["statistical_near"],
                          adapted_pure_exact=reliable["statistical_exact"],
                          added=len(new_music.keys() - old_music.keys()),
                          removed=len(old_music.keys() - new_music.keys()),
                          primary_source_changes=sum(old_music[key]["source"] != new_music[key]["source"]
                                                     for key in common),
                          exact_aligned_before=old["aligned"], exact_aligned_after=new["aligned"],
                          exact_qualified_before=old["exact_qualified"], exact_qualified_after=new["exact_qualified"],
                          context_qualified_before=old["context_qualified"], context_qualified_after=new["context_qualified"],
                          worker_ms_before=old["worker_ms"], worker_ms_after=new["worker_ms"]))
    cohorts = {"all": cases}
    for kind in sorted({row["kind"] for row in cases}):
        cohorts[kind] = [row for row in cases if row["kind"] == kind]
    methods = ("original", "retrieved", "statistical", "hybrid", "adapted", "adapted_pure")
    write_json(args.output, dict(cases=cases, cohorts={kind: {method: summarize(rows, method) for method in methods}
                                                     for kind, rows in cohorts.items()}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage", choices=("prepare", "diagnose", "compare"))
    for option in ("benchmark", "b5-replay", "queries", "baseline", "replay", "reliability", "output"):
        parser.add_argument(f"--{option}", type=Path, required=option == "output")
    args = parser.parse_args()
    {"prepare": prepare, "diagnose": diagnose, "compare": compare}[args.stage](args)


if __name__ == "__main__":
    main()
