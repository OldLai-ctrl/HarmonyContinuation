"""Compare frozen V8 and V9-A replays without changing candidate order."""
import argparse
import json
import math
import statistics
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "b5"))
from b5_data import read_jsonl, sha256, write_json, write_jsonl
from b5_factory import distance, first_hit, is_near, motif


def collapse(events):
    result = []
    for event in events:
        if result and (result[-1]["root"], result[-1]["quality"]) == (event["root"], event["quality"]):
            result[-1]["duration"] += event["duration"]
        else:
            result.append(dict(event))
    return result


def catalog(path, source_ids):
    content, seam = set(), set()
    detailed = defaultdict(list)
    for row in read_jsonl(path):
        if source_ids is not None and row["source"] not in source_ids:
            continue
        events = row["events"]
        for i, event in enumerate(events):
            if event["root"] is None or (i and (event["root"], event["quality"]) ==
                                        (events[i-1]["root"], events[i-1]["quality"])):
                continue
            tail = collapse(events[i:])
            content.add(motif(tail, tail[0]["root"]))
            if i and events[i-1]["root"] is not None:
                relative = motif(tail, events[i-1]["root"])
                seam.add(relative)
                detailed[relative].append((row["meter"], tail))
    return content, seam, detailed


def equivalent_detail(left, right, require_timing):
    if len(left) != len(right):
        return False
    for a, b in zip(left, right):
        if require_timing and not math.isclose(a["duration"], b["duration"], abs_tol=0.125):
            return False
        if a.get("intervals") != b.get("intervals"):
            return False
        if a.get("bass") is not None:
            if b.get("bass") is None or (a["bass"]-a["root"]) % 12 != (b["bass"]-b["root"]) % 12:
                return False
    return True


def probe(target, anchor, replay, material, known_meter, new_ids, source_events):
    target_content = motif(target, target[0]["root"])
    target_seam = motif(target, anchor)
    content, seam, detailed = material
    candidates = replay["candidates"]
    distances = [distance(target_seam, motif(candidate["events"], anchor)) for candidate in candidates]
    added = [i for i, candidate in enumerate(candidates) if candidate["source"] in new_ids]
    kept_times = 0
    for i in added:
        candidate = candidates[i]
        authored = source_events[candidate["source"]][candidate["next"]:]
        kept_times += len(authored) == len(candidate["events"]) and all(
            abs(a["duration"] - b["duration"]) <= 1e-6
            for a, b in zip(authored, candidate["events"], strict=True))
    first = first_hit(distances, 1)
    return dict(catalog_near=is_near(target_content, content), seam_near=is_near(target_seam, seam),
                catalog_exact_timing=any(meter == known_meter and len(tail) == len(target)
                                         and all(abs(a["duration"]-b["duration"]) <= .125
                                                 for a,b in zip(target, tail))
                                         for meter, tail in detailed.get(target_seam, ())),
                catalog_exact_full=any(meter == known_meter and equivalent_detail(target, tail, True)
                                       for meter, tail in detailed.get(target_seam, ())),
                reachable_near=first, reachable_exact=first_hit(distances, 0),
                mixed_near=candidates[first-1]["product_rank"] if first else None,
                actual_exact_full=sum(d == 0 and equivalent_detail(target, collapse(c["events"]), True)
                                      for d,c in zip(distances,candidates)),
                candidates=len(candidates), source_end=sum(c["source_end_preserved"] for c in candidates),
                structure_qualified=sum(c["structure_qualified"] for c in candidates),
                long_count=sum(len(motif(c["events"], anchor)) >= 6 and c["source_end_preserved"]
                               for c in candidates),
                new_candidate_count=len(added), new_near_count=sum(distances[i] <= 1 for i in added),
                new_authored_duration_count=kept_times,
                new_source_end=sum(candidates[i]["source_end_preserved"] for i in added),
                new_long_count=sum(len(motif(candidates[i]["events"], anchor)) >= 6
                                   and candidates[i]["source_end_preserved"] for i in added),
                exact_qualified=replay["exact_qualified"], context_qualified=replay["context_qualified"],
                distinct_top3=len({motif(c["events"], anchor) for c in candidates[:3]}),
                worker_ms=replay["worker_ms"])


def tags(query):
    events, prefix = query["target"], query["prefix"]
    all_events = prefix + events
    tonic = prefix[-1]["tonic"]
    roots = {(event["root"]-tonic) % 12 for event in events} if tonic is not None else set()
    host = ["long_6_plus" if len(motif(events, prefix[-1]["root"])) >= 6 else "short_3_5",
            "variable_qn" if len({e["duration"] for e in events}) > 1 else "uniform_qn",
            "extended_or_special" if any(len(e["intervals"]) > 3 or e["quality"] in ("aug", "dim", "sus")
                                         for e in events) else "basic",
            "explicit_inversion" if any(e["bass"] not in (None, e["root"]) for e in events) else "bass_unspecified",
            "tonic_arrival" if events[-1]["root"] == tonic else "non_tonic_end",
            "tonic_annotation_changes" if len({event["tonic"] for event in all_events}) > 1
            else "stable_tonic_annotation"]
    if roots - {0, 2, 3, 4, 5, 7, 8, 9, 10, 11}:
        host.append("roots_outside_both_major_minor")
    tonic_sonorities = {e["quality"] for e in all_events if e["root"] == tonic and e["quality"] in ("maj", "min")}
    host.append("major_tonic_marker" if tonic_sonorities == {"maj"} else
                "minor_tonic_marker" if tonic_sonorities == {"min"} else "tonic_mode_unresolved")
    if len(tonic_sonorities) == 1 and len({e["tonic"] for e in all_events}) == 1:
        mode = next(iter(tonic_sonorities))
        scale = ({0, 2, 4, 5, 7, 9, 11} if mode == "maj" else {0, 2, 3, 5, 7, 8, 10})
        host.append("outside_tonic_marker_scale" if roots - scale else "within_tonic_marker_scale")
    return host


def aggregate(rows, key):
    total = len(rows)
    states = [row[key] for row in rows]
    result = {"works": total}
    for name in ("catalog_near", "seam_near", "catalog_exact_timing", "catalog_exact_full",
                 "reachable_exact", "reachable_near", "actual_exact_full", "long_count", "new_candidate_count",
                 "source_end", "new_long_count", "new_near_count", "new_source_end", "new_authored_duration_count"):
        result[f"{name}_works"] = sum(bool(item[name]) for item in states)
    for name in ("candidates", "source_end", "structure_qualified", "long_count", "new_candidate_count",
                 "new_source_end", "new_long_count", "new_near_count", "new_authored_duration_count",
                 "exact_qualified", "context_qualified"):
        result[name] = sum(item[name] for item in states)
    result["top3_near"] = sum(0 < (item["reachable_near"] or 0) <= 3 for item in states)
    result["top10_near"] = sum(0 < (item["reachable_near"] or 0) <= 10 for item in states)
    result["mixed_top3_near"] = sum(0 < (item["mixed_near"] or 0) <= 3 for item in states)
    result["mixed_top10_near"] = sum(0 < (item["mixed_near"] or 0) <= 10 for item in states)
    result["mrr_near"] = statistics.mean(1 / item["reachable_near"] if item["reachable_near"] else 0 for item in states)
    result["top3_mean_l1_diversity"] = statistics.mean(item["distinct_top3"] for item in states)
    ts = sorted(item["worker_ms"] for item in states)
    result["worker_ms_median"], result["worker_ms_p95"] = statistics.median(ts), ts[math.ceil(.95*total)-1]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--prepare-trace", action="store_true")
    parser.add_argument("--trace", type=Path)
    args = parser.parse_args()
    e = args.evidence
    queries = e / "confirmation/confirmation.jsonl"
    freeze = json.loads((e / "confirmation/freeze.json").read_text())
    if freeze["query_sha256"] != sha256(queries):
        raise ValueError("Independence freeze mismatch")
    new_ids = {row["id"] for row in json.loads((e / "materials/manifest.json").read_text())["entries"]}
    if args.prepare_trace:
        write_jsonl(args.output, (dict(row, trace_sources=sorted(new_ids)) for row in read_jsonl(queries)))
        return
    old = e / "confirmation-v8"
    new = e / "confirmation-v9"
    old_catalog = old / "factory_catalog.jsonl"
    new_catalog = new / "factory_catalog.jsonl"
    lib = (catalog(old_catalog, None), catalog(new_catalog, None))
    increment = catalog(new_catalog, new_ids)
    source_events = {row["source"]: row["events"] for row in read_jsonl(new_catalog)
                     if row["source"] in new_ids}
    rows = []
    groups = defaultdict(list)
    for query, a, b in zip(read_jsonl(queries), read_jsonl(old / "factory_replay.jsonl"),
                           read_jsonl(new / "factory_replay.jsonl"), strict=True):
        assert query["id"] == a["id"] == b["id"]
        target = collapse(query["target"])
        anchor = query["prefix"][-1]["root"]
        original = {c["fingerprint"]: c for c in a["candidates"]}
        changed = {c["fingerprint"]: c for c in b["candidates"]}
        for identity in original.keys() & changed.keys():
            assert original[identity]["events"] == changed[identity]["events"], (query["id"], identity)
        row = dict(id=query["id"], groups=tags(query),
                   v8=probe(target, anchor, a, lib[0], "4/4", new_ids, source_events),
                   v9=probe(target, anchor, b, lib[1], "4/4", new_ids, source_events),
                   increment_catalog_near=is_near(motif(target, target[0]["root"]), increment[0]),
                   increment_seam_near=is_near(motif(target, anchor), increment[1]),
                   lost_v8_fingerprints=sorted(original.keys()-changed.keys()),
                   generated_same=a["generated_identities"] == b["generated_identities"],
                   omitted_generated_now_in_factory=sorted(set(a["generated_identities"]) &
                                                          (changed.keys()-original.keys())),
                   new_sources=sorted({c["source"] for c in b["candidates"] if c["source"] in new_ids}))
        rows.append(row)
        groups["all"].append(row)
        for name in row["groups"]:
            groups[name].append(row)
    controls = []
    for query, replay in zip(read_jsonl(e / "materials/controls.jsonl"),
                             read_jsonl(e / "controls-v9/factory_replay.jsonl"), strict=True):
        assert query["id"] == replay["id"]
        joined = [c for c in replay["candidates"] if c["source"] == query["id"] or query["id"] in c["support_sources"]]
        intended = [c for c in joined if c["next"] == len(query["prefix"]) and
                    len(c["events"]) == len(query["target"]) and
                    c["source_end_preserved"]]
        exact = [c for c in intended if all(
            a["root"] == b["root"] and (a["bass"] if a["bass"] is not None else a["root"]) ==
            (b["bass"] if b["bass"] is not None else b["root"]) and
            a["intervals"] == b["intervals"] and abs(a["duration"]-b["duration"]) < 1e-6
            for a,b in zip(c["events"],query["target"],strict=True))]
        controls.append(dict(id=query["id"], source_qualified=len(joined),
                             intended_complete_tail=len(intended), exact_performance=len(exact),
                             exact_dispositions=Counter(r["disposition"] for r in replay["exact_trace"]),
                             contextual_dispositions=Counter(r["disposition"] for r in replay["context_trace"])))
    report = dict(results={g: {key: aggregate(selected, key) for key in ("v8", "v9")}
                           for g, selected in groups.items()}, per_work=rows, controls=controls,
                  increment_catalog_near_works=sum(row["increment_catalog_near"] for row in rows),
                  increment_seam_near_works=sum(row["increment_seam_near"] for row in rows))
    if args.trace:
        exact, contextual = Counter(), Counter()
        by_work = Counter()
        traced = args.trace / "factory_replay.jsonl"
        for plain, observed in zip(read_jsonl(new / "factory_replay.jsonl"),
                                   read_jsonl(traced), strict=True):
            assert plain["id"] == observed["id"]
            assert [c["fingerprint"] for c in plain["candidates"]] == [
                c["fingerprint"] for c in observed["candidates"]]
            ex = Counter(x["disposition"] for x in observed["exact_trace"])
            co = Counter(x["disposition"] for x in observed["context_trace"])
            exact.update(ex)
            contextual.update(co)
            by_work["aligned_new"] += bool(ex)
            by_work["exact_candidate_constructed"] += any(x["eligibility"] >= 0 for x in observed["exact_trace"])
            by_work["exact_qualified"] += any(ex[state] for state in ("TARGET_RETAINED", "EXPLORATORY_RETAINED"))
            by_work["contextual_qualified"] += bool(co["qualified-before-structure"])
        report["trace"] = dict(exact=exact, contextual=contextual, works=by_work, sha256=sha256(traced))
    write_json(args.output, report)


if __name__ == "__main__":
    main()
