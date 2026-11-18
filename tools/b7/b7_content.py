"""Audit complete source-row coverage against the frozen Factory V8 catalog."""

import argparse
import json
import math
import sqlite3
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "b5"))
from b5_data import read_jsonl, sha256, stable, write_json, write_jsonl
from b5_factory import distance, motif


FACTORY_SHA = "c32257f878eead44e0df6febe14340ac08eab4a66d7c4db34bf6af911ed6ce9e"
FREEZE_SHA = "89f0121b793a7021eac95df9b155be44ff63dc2ffd3568a039ea886a4c5723de"
HARMONIC_STEPS = {0: "tonic", 2: "predominant", 5: "predominant",
                  7: "dominant", 11: "dominant"}


def catalog_metadata(database):
    with sqlite3.connect(database.resolve().as_uri() + "?immutable=1", uri=True) as connection:
        payloads = [json.loads(payload) for (payload,) in connection.execute(
            "SELECT payload FROM progressions ORDER BY id"
        )]
    result = {}
    for source in payloads:
        key = json.loads(source["typedData"])["key"]
        result[source["id"]] = {
            "meter": source["meter"], "tonic": key[0] if key else 0,
            "mode": source["mode"], "intent": source["intent"], "loopable": source["loopable"],
        }
    return result


def collapse(events):
    reduced = []
    for event in events:
        if reduced and (event["root"], event["quality"]) == (reduced[-1]["root"], reduced[-1]["quality"]):
            reduced[-1]["duration"] += event["duration"]
            reduced[-1]["single_event"] = False
        else:
            reduced.append(dict(event, single_event=True))
    return reduced


def roles(events, tonic):
    return tuple(HARMONIC_STEPS.get((event["root"] - tonic) % 12, "other") for event in events)


def known_rhythm(events):
    return all(event.get("unit") == "qn" and (event.get("duration") or 0) > 0 for event in events)


def shell_shape(events):
    """One controlled harmonic sensitivity: collapse seventh-family distinctions."""
    family = {"dom": "maj", "hdim": "dim"}
    return motif([dict(event, quality=family.get(event["quality"], event["quality"])) for event in events],
                 events[0]["root"])


def catalog_indices(catalog, metadata, lengths):
    tails = defaultdict(list)
    shapes = defaultdict(set)
    shell_tails = defaultdict(set)
    fragments = defaultdict(list)
    function_tails = defaultdict(list)
    all_tails = 0
    for row in read_jsonl(catalog):
        source = row["source"]
        events = row["events"]
        if not events or not known_rhythm(events) or any(event["root"] is None for event in events):
            raise ValueError(f"Missing playable symbolic catalog annotation: {source}")
        # Keep the original source index. A detail-only repeat within the same
        # root/family does not create a new L1 boundary.
        starts = [index for index, event in enumerate(events)
                  if not index or (event["root"], event["quality"]) !=
                  (events[index-1]["root"], events[index-1]["quality"])]
        for start_number, index in enumerate(starts):
            tail = collapse(events[index:])
            shape = motif(tail, tail[0]["root"])
            seam = motif(tail, events[index-1]["root"]) if index else None
            item = (source, index, shape, seam, tail)
            tails[len(shape)].append(item)
            shapes[len(shape)].add(shape)
            shell = shell_shape(tail)
            shell_tails[len(shell)].add(shell)
            all_tails += 1
            if len(tail) >= 3:
                tonic = metadata[source]["tonic"]
                role_pattern = roles(tail, tonic)
                destination = ((tail[-1]["root"] - tonic) % 12, tail[-1]["quality"])
                function_tails[(role_pattern, destination)].append(item)
            for size in lengths:
                end_number = start_number + size
                # A strict interior fragment needs source material after it.
                # Complete tails with similar beginnings/endings stay in the
                # primary full-tail metric and are handled independently.
                if end_number >= len(starts):
                    continue
                end = starts[end_number]
                fragment = motif(events[index:end], events[index]["root"])
                fragments[fragment].append((source, index, end))
    return tails, shapes, shell_tails, fragments, function_tails, all_tails


def catalog_witnesses(tails, shape):
    for length in range(len(shape) - 1, len(shape) + 2):
        yield from (item for item in tails.get(length, ()) if distance(shape, item[2]) <= 1)


def duration_close(left, right):
    return all(math.isclose(a["duration"], b["duration"], abs_tol=0.125)
               for a, b in zip(left, right, strict=True))


def exact_rhythm_and_tones(target, tail):
    target_anchor, tail_anchor = target[0]["root"], tail[0]["root"]
    rhythm = duration_close(target, tail)
    # Compare detail only where each L1 token represents one raw change and
    # both sides specify intervals. Null bass is never interpreted as root.
    tones_known = all(left["single_event"] and right["single_event"] and
                      left.get("intervals") and right.get("intervals") for left, right in zip(target, tail))
    tones = tones_known and all(left["intervals"] == right["intervals"]
                                for left, right in zip(target, tail, strict=True))
    explicit_bass = any(event.get("bass") is not None for event in target)
    bass_known = explicit_bass and all(left.get("bass") is None or right.get("bass") is not None
                                       for left, right in zip(target, tail, strict=True))
    bass = bass_known and all(left.get("bass") is None or
                              (left["bass"] - target_anchor) % 12 == (right["bass"] - tail_anchor) % 12
                              for left, right in zip(target, tail, strict=True))
    return rhythm, tones_known, tones, bass_known, bass


def features(query, source_bars):
    target = query["target"]
    context = [query["prefix"][-1], *target]
    tonic = context[0].get("tonic")
    stable_tonic = tonic is not None and all(event.get("tonic") == tonic for event in context)
    meter = {event["meter"] for event in context}
    durations = {round(event["duration"], 5) for event in target}
    final_tonic = target[-1].get("tonic")
    target_color = any(len(event.get("intervals") or ()) > 3 for event in target)
    tonic_minor = stable_tonic and any(event["root"] == tonic and event["quality"] == "min"
                                       for event in target)
    results = [f"length:{len(motif(target, target[0]['root']))}",
               f"meter:{next(iter(meter))}" if len(meter) == 1 else "meter:mixed",
               "duration_mixed" if len(durations) > 1 else "duration_uniform",
               "four_or_eight_source_bars" if source_bars in (4, 8) else "other_source_bar_count",
               "color_intervals" if target_color else "basic_intervals",
               "annotated_tonic_changes" if not stable_tonic else "stable_annotated_tonic",
               "non_tonic_terminal_root" if final_tonic is not None and target[-1]["root"] != final_tonic
               else "tonic_terminal_root"]
    if source_bars > 1 and source_bars % 2:
        results.append("odd_source_bar_count")
    if any(event["quality"] in ("1", "5") for event in target):
        results.append("root_or_fifth_only_symbol")
    if any(event.get("bass") is not None and event["bass"] != event["root"] for event in target):
        results.append("explicit_nonroot_bass")
    if target[0].get("section") and context[0].get("section") and target[0]["section"] != context[0]["section"]:
        results.append("source_section_label_change")
    if tonic_minor:
        results.append("minor_sonority_on_annotated_tonic")
    if stable_tonic:
        major_tonic = any(event["tonic"] == tonic and event["root"] == tonic and
                          event["quality"] == "maj" for event in query["prefix"])
        if major_tonic and any(((event["root"] - tonic) % 12, event["quality"]) in
                               ((3, "maj"), (5, "min"), (8, "maj"), (10, "maj")) for event in target):
            results.append("minor_mode_sonority_after_major_tonic")
        if any(event["root"] in ((tonic + 1) % 12, (tonic + 6) % 12) for event in target):
            results.append("root_outside_both_natural_major_minor")
        if any(left["quality"] == "dom" and (right["root"] - left["root"]) % 12 == 5 and right["root"] != tonic
               for left, right in zip(context, context[1:])):
            results.append("dominant_fifth_to_nontonic_root")
    return results, tonic if stable_tonic else None


def evaluate(query, indices, metadata, history, source_bars):
    tails, shapes, shell_tails, fragments, function_tails, _ = indices
    target = collapse(query["target"])
    prefix = query["prefix"]
    source_shape = motif(target, target[0]["root"])
    goal = motif(target, prefix[-1]["root"])
    tags, tonic = features(query, source_bars)
    candidate_tails = list(catalog_witnesses(tails, source_shape))
    near = bool(candidate_tails)
    exact = [item for item in candidate_tails if item[2] == source_shape]
    near_anchored = [item for length in range(len(goal) - 1, len(goal) + 2)
                     for item in tails.get(length, ()) if item[3] is not None and distance(goal, item[3]) <= 1]
    anchored = bool(near_anchored)
    relative_cap = max(1, len(source_shape) // 4)
    relative_near = near or (relative_cap > 1 and any(
        distance(source_shape, candidate) <= relative_cap
        for length in range(len(source_shape) - relative_cap, len(source_shape) + relative_cap + 1)
        for candidate in sorted(shapes.get(length, ()))
    ))
    shape = shell_shape(target)
    shell_near = any(distance(shape, candidate) <= 1 for length in range(len(shape) - 1, len(shape) + 2)
                     for candidate in shell_tails.get(length, ()))
    rhythm_pairs = [(item, exact_rhythm_and_tones(target, item[4])) for item in exact]
    rhythm_and_meter = [item for item, info in rhythm_pairs if info[0] and
                        {event["meter"] for event in query["target"]} == {metadata[item[0]]["meter"]}]
    fragment_size = max(2, math.ceil(len(target) / 2))
    related = []
    for offset in range(len(target) - fragment_size + 1):
        fragment = motif(target[offset:offset+fragment_size], target[offset]["root"])
        related.extend((offset, *item) for item in fragments.get(fragment, ()))
    role_witnesses = []
    if tonic is not None:
        role_pattern = roles(target, tonic)
        # "Other" is deliberately not a functional equivalence class.
        if "other" not in role_pattern and len(set(role_pattern)) >= 2:
            last = ((target[-1]["root"] - tonic) % 12, target[-1]["quality"])
            role_witnesses = function_tails.get((role_pattern, last), [])
    final_pair = motif(target[-2:], target[-2]["root"])
    matching_terminal = [item for item in candidate_tails
                         if motif(item[4][-2:], item[4][-2]["root"]) == final_pair]
    shared_start = [item for item in candidate_tails
                    if motif(item[4][:2], item[4][0]["root"]) == motif(target[:2], target[0]["root"])]
    matching_meter = [item for item in candidate_tails
                      if {event["meter"] for event in query["target"]} == {metadata[item[0]]["meter"]}]
    qa = history.get(query["id"], {})
    result = dict(id=query["id"], group=query["group"], work=query["work"],
                  line=query["target"][0]["line"], source_bars=source_bars, artist=query["artist"],
                  symbols=[event["symbol"] for event in query["target"]],
                  target_qn=[event["duration"] for event in query["target"]],
                  prefix_symbols=[event["symbol"] for event in prefix],
                  prefix_qn=[event["duration"] for event in prefix],
                  features=tags, tonic=tonic, target_length=len(target),
                  optimistic_complete_near=near, optimistic_complete_exact=bool(exact),
                  native_seam_near=anchored, same_meter_complete_near=bool(matching_meter),
                  same_ending_pair_complete_near=bool(matching_terminal),
                  comparable_role_sequence=bool(role_witnesses),
                  shell_family_complete_near=shell_near,
                  length_scaled_complete_near=relative_near,
                  same_start_different_ending=bool(shared_start) and not matching_terminal,
                  exact_L1_same_durations=any(info[0] for _, info in rhythm_pairs),
                  exact_L1_same_durations_and_meter=bool(rhythm_and_meter),
                  exact_L1_tones_comparable=any(info[1] for _, info in rhythm_pairs),
                  exact_L1_same_tones=any(info[2] for _, info in rhythm_pairs),
                  exact_L1_bass_comparable=any(info[3] for _, info in rhythm_pairs),
                  exact_L1_same_annotated_bass=any(info[4] for _, info in rhythm_pairs),
                  fragment=bool(related), required_fragment_length=fragment_size,
                  classification=("complete_tail" if near else "B_fragment_only" if related else "A_no_full_or_fragment"),
                  witnesses={
                      "full": [[source, index] for source, index, *_ in (exact or candidate_tails)[:8]],
                      "same_ending": [[source, index] for source, index, *_ in matching_terminal[:8]],
                      "same_meter": [[source, index] for source, index, *_ in matching_meter[:8]],
                      "source_seam": [[source, index] for source, index, *_ in near_anchored[:8]],
                      "fragment": [list(item) for item in sorted(related)[:8]],
                  }, history=qa)
    return result


def summarize(rows):
    counts = Counter()
    work_hits = defaultdict(set)
    rates = defaultdict(list)
    for row in rows:
        keys = [row["classification"]] + [key for key, value in row.items() if value is True]
        for key in keys:
            counts[key] += 1
            work_hits[key].add(row["group"])
        rates[row["group"]].append(set(keys))
    return {
        "rows": len(rows), "works": len(rates), "hits": counts,
        "works_with_hit": {key: len(groups) for key, groups in work_hits.items()},
        "work_macro_rate": {key: round(sum(sum(key in hits for hits in values) / len(values)
                                         for values in rates.values()) / len(rates), 6)
                            for key in counts},
    }


def read_history(b6, queries):
    original = {row["id"]: row for row in read_jsonl(queries)}
    confirm = {row["id"]: row for row in read_jsonl(b6 / "cohorts/confirmation.jsonl")
               if row["kind"] != "mpb_equal_grid"}
    diagnoses = {row["id"]: row["category"] for row in json.loads(
        (b6 / "diagnosis.json").read_text(encoding="utf8"))["cases"]}
    history = {}
    for name, path, group in (
        ("B6_development", b6 / "development-comparison.json", original),
        ("B6_confirmation_billboard", b6 / "holdout-comparison.json", confirm),
    ):
        for row in json.loads(path.read_text(encoding="utf8"))["cases"]:
            if row["id"] in group:
                history[row["id"]] = {
                    "cohort": name, "diagnosis": diagnoses.get(row["id"]),
                    "baseline_near_rank": row["original_near"], "B6_near_rank": row["retrieved_near"],
                }
    return history, {item["group"] for item in original.values()}


def source_rows(query_path, selected_ids=None):
    return [query for query in read_jsonl(query_path)
            if (selected_ids is None or query["id"] in selected_ids)
            and query["kind"] == "source_line" and known_rhythm(query["prefix"] + query["target"])
            and len(motif(query["target"], query["target"][0]["root"])) >= 3]


def source_bar_count(work, line):
    for row, start, end in work["sections"]:
        if row == line:
            return len({event["bar"] for event in work["stream"][start:end]})
    raise ValueError(f"{work['id']} does not contain source row {line}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--b5", type=Path, required=True)
    parser.add_argument("--b6", type=Path, required=True)
    parser.add_argument("--factory", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--replay-queries", type=Path,
                        help="Optional B7 real-QN cohort to join by frozen query ID")
    parser.add_argument("--replication-queries", type=Path,
                        help="Optional B7 validation real-QN cohort, reported separately")
    args = parser.parse_args()
    freeze = args.b5 / "benchmark/freeze.json"
    query_path = args.b5 / "benchmark/queries_test.jsonl"
    old_queries = args.b5 / "benchmark/factory_queries.jsonl"
    catalog = args.b5 / "factory-replay/factory_catalog.jsonl"
    recorded = json.loads(freeze.read_text(encoding="utf8"))
    hashes = {
        "factory": sha256(args.factory), "freeze": sha256(freeze),
        "test_queries": sha256(query_path), "development_queries": sha256(old_queries),
        "catalog": sha256(catalog),
    }
    if hashes["factory"] != FACTORY_SHA or hashes["freeze"] != FREEZE_SHA:
        raise ValueError("The B5 freeze or Factory V8 identity differs")
    if hashes["test_queries"] != recorded["files"]["queries_test.jsonl"] or (
        hashes["development_queries"] != recorded["files"]["factory_queries.jsonl"]
    ):
        raise ValueError("Factory query identity differs")
    catalog_hash = json.loads((args.b5 / "factory-replay/factory_evaluation.json").read_text(
        encoding="utf8"))["inputs"]["catalog_sha256"]
    if hashes["catalog"] != catalog_hash:
        raise ValueError("Factory catalog identity differs")
    test = source_rows(query_path)
    replication = []
    if args.replication_queries:
        validation_queries = args.b5 / "benchmark/queries_validation.jsonl"
        hashes["validation_queries"] = sha256(validation_queries)
        hashes["replication_queries"] = sha256(args.replication_queries)
        if hashes["validation_queries"] != recorded["files"]["queries_validation.jsonl"]:
            raise ValueError("Frozen validation query identity differs")
        replication_ids = {row["id"] for row in read_jsonl(args.replication_queries)}
        replication = source_rows(validation_queries, replication_ids)
        if {row["id"] for row in replication} != replication_ids:
            raise ValueError("Replication rows are not all in frozen validation source queries")
    history, seen_work = read_history(args.b6, old_queries)
    metadata = catalog_metadata(args.factory)
    lengths = {max(2, math.ceil(len(motif(row["target"], row["target"][0]["root"])) / 2))
               for row in test + replication}
    indices = catalog_indices(catalog, metadata, lengths)
    work_index = {}
    for split in ("test", "validation") if replication else ("test",):
        path = args.b5 / f"benchmark/{split}.jsonl"
        hashes[f"{split}_source_works"] = sha256(path)
        if hashes[f"{split}_source_works"] != recorded["files"][f"{split}.jsonl"]:
            raise ValueError("Frozen source work identity differs")
        work_index.update((row["id"], row) for row in read_jsonl(path))
    replay_ids = set()
    if args.replay_queries:
        hashes["replay_queries"] = sha256(args.replay_queries)
        replay_ids = {query["id"] for query in read_jsonl(args.replay_queries)}
    groups = defaultdict(list)
    rows = []
    for split, queries in (("test", test), ("validation", replication)):
        for query in queries:
            work = work_index[query["work"]]
            bars = source_bar_count(work, query["target"][0]["line"])
            row = evaluate(query, indices, metadata, history, bars)
            row["title"] = work["title"]
            row["split"] = split
            row["B6_development_work_seen"] = row["group"] in seen_work
            row["in_B7_real_replay"] = row["id"] in replay_ids
            rows.append(row)
            if split == "validation":
                groups["B7_validation_replication"].append(row)
                for tag in row["features"]:
                    groups["validation:" + tag].append(row)
                continue
            groups["all"].append(row)
            groups["B6_development_work" if row["B6_development_work_seen"] else
                   "work_outside_B6_development"].append(row)
            groups["B6_replayed_query" if row["history"] else "query_outside_B6_replay"].append(row)
            if row["in_B7_real_replay"]:
                groups["B7_real_replay"].append(row)
            for tag in row["features"]:
                groups[tag].append(row)
    args.output.mkdir(parents=True, exist_ok=True)
    write_jsonl(args.output / "source_rows.jsonl", rows)
    reports = {}
    for name, members in groups.items():
        result = summarize(members)
        result["examples"] = {}
        for key in ("A_no_full_or_fragment", "B_fragment_only", "complete_tail"):
            by_work = {}
            for row in sorted(members, key=lambda row: stable("b7-content", row["id"])):
                if row["classification"] == key:
                    by_work.setdefault(row["group"], row["id"])
            result["examples"][key] = list(by_work.values())[:6]
        reports[name] = result
    write_json(args.output / "summary.json", {
        "inputs": hashes, "catalog_tails": indices[-1], "catalog_meters": Counter(
            item["meter"] for item in metadata.values()), "groups": reports,
        "notes": {
            "near": "B5 L1 Levenshtein distance <=1, complete authored source tail, first root aligned",
            "native_seam": "L1 distance <=1 aligning each tail to its actual preceding source root",
            "fragment": "exact internal (non-tail) contiguous span of at least half the target and >=2 L1 events",
            "duration": "abs difference <=0.125 QN per corresponding event on exact L1 tails, original tempo",
            "length_scaled": "same full tails; maximum of one edit and floor(0.25 x target L1 length)",
            "role_sequence": "coarse tonic/predominant/dominant root groups, matching terminal degree and family",
            "support": "A feature can repeat across rows of one work; use work counts and work-macro rates",
        },
    })


if __name__ == "__main__":
    main()
