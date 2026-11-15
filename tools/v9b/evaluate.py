"""Compare ordered, qualified Factory routes across frozen libraries."""
import argparse
import json
import statistics
import sys
from collections import Counter, defaultdict
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(TOOLS / "b5"), str(TOOLS / "v9a")]
import evaluate as v9a
from b5_data import read_jsonl, sha256, write_json, write_jsonl
from b5_factory import motif

LIBRARIES = ("v8", "v9a", "v9b", "v9b-functions")


def tested_performance(events):
    return [(
        item["root"], item["bass"] if item["bass"] is not None else item["root"],
        item["intervals"], round(item["duration"], 5)
    ) for item in events]


def route_motion(route, tonic):
    def relation(root, quality):
        pc = (root - tonic) % 12
        if pc == 0: return "home"
        if pc == 7 and quality in ("dom", "maj", "sus"): return "dominant"
        if pc in (2, 5) and quality in ("min", "maj", "dim", "hdim"): return "predominant"
        if pc in (3, 4, 8, 9, 10): return "mediant/submediant/subtonic"
        return "other"

    phases = []
    for root, quality in route:
        phase = relation(root, quality)
        if not phases or phases[-1] != phase:
            phases.append(phase)
    # Retain the final degree/core quality so unlike endings do not merge.
    return tuple(phases), ((route[-1][0] - tonic) % 12, route[-1][1])


def route_data(candidates, anchor, new, authored):
    evidence = Counter()
    shapes = set()
    motion = set()
    long_shapes = set()
    time = defaultdict(set)
    qualified_sources = set()
    split = Counter()
    for candidate in candidates:
        events = candidate["events"]
        relative = motif(events, anchor)
        if candidate["source"] in new:
            split["new_candidates"] += 1
            qualified_sources.add(candidate["source"])
        if not candidate["source_end_preserved"] or len(relative) < 3:
            continue
        shapes.add(relative)
        if len(relative) >= 6:
            long_shapes.add(relative)
        tonic = candidate["key"] % 12
        motion.add(route_motion(motif(events, 0), tonic))
        timing = tuple(round(item["duration"], 6) for item in v9a.collapse(events))
        time[relative].add(timing)
        if candidate["source"] in authored and candidate["next"] == authored[candidate["source"]]["boundary"]:
            evidence["authored_boundary"] += 1
        elif candidate["structure_qualified"] and candidate["reviewed_ending"]:
            evidence["worker_reviewed"] += 1
        elif candidate["complete_source_phrase"]:
            evidence["whole_source_unconfirmed_structure"] += 1
        elif candidate["provenance"] == 0:
            evidence["internal_source_tail"] += 1
        else:
            evidence["algorithm_assessed"] += 1
    for n in (3, 10):
        first = candidates[:n]
        split[f"top{n}_new"] = sum(x["source"] in new for x in first)
        split[f"top{n}_route_count"] = len({motif(x["events"], anchor) for x in first})
        split[f"top{n}_editorial_families"] = len({x["family"] or x["source"] for x in first})
        split[f"top{n}_repeated_primary_sources"] = len(first) - len({x["source"] for x in first})
    return dict(routes=len(shapes), motion_routes=len(motion), long_routes=len(long_shapes),
                rhythmic_variants=sum(len(variants)-1 for variants in time.values()),
                evidence=dict(evidence), qualified_new_sources=sorted(qualified_sources), **split)


def measure(query, replay, catalog, manifest, authored, source_events):
    anchor = query["prefix"][-1]["root"]
    target = v9a.collapse(query["target"])
    base = v9a.probe(target, anchor, replay, catalog, query["prefix"][-1].get("meter"), manifest, source_events)
    return base | route_data(replay["candidates"], anchor, manifest, authored)


def stats(rows, edition):
    result = v9a.aggregate(rows, edition)
    items = [row[edition] for row in rows]
    for label in ("routes", "motion_routes", "long_routes", "rhythmic_variants", "top3_route_count",
                  "top10_route_count", "top3_editorial_families", "top10_editorial_families",
                  "top3_repeated_primary_sources", "top10_repeated_primary_sources"):
        result[f"median_{label}"] = statistics.median(item.get(label, 0) for item in items)
    result["works_at_least_two_routes"] = sum(item["routes"] >= 2 for item in items)
    result["works_at_least_two_long_routes"] = sum(item["long_routes"] >= 2 for item in items)
    result["works_with_route_gain_vs_v9a"] = sum(item["new_source_routes_vs_v9a"] > 0 for item in items)
    result["new_source_routes_vs_v9a"] = sum(item["new_source_routes_vs_v9a"] for item in items)
    result["works_with_motion_gain_vs_v9a"] = sum(item["motion_routes"] > row["v9a"]["motion_routes"]
                                                 for item, row in zip(items, rows, strict=True))
    result["top3_new"] = sum(item.get("top3_new", 0) for item in items)
    result["top10_new"] = sum(item.get("top10_new", 0) for item in items)
    result["observed_new_sources"] = len({source for item in items for source in item["qualified_new_sources"]})
    result["lost_v9a_music"] = sum(item["lost_v9a_music"] for item in items)
    result["lost_content_only_music"] = sum(item["lost_content_only_music"] for item in items)
    result["generated_identity_changes"] = sum(not item["generated_same_as_v9a"] for item in items)
    categories = Counter()
    for item in items:
        categories.update(item["evidence"])
    result["evidence"] = dict(categories)
    return result


def compare(queries, root, name, manifest, authored):
    query_path = Path(queries)
    freeze_path = query_path.with_name(f"{name}-freeze.json")
    freeze = json.loads(freeze_path.read_text(encoding="utf-8"))
    query_hash = sha256(query_path)
    if query_hash != freeze["sha256"]:
        raise ValueError(f"{name}: confirmation input differs from freeze")
    inputs = [root / f"{name}-{edition}" for edition in LIBRARIES]
    identities = [json.loads((path / "factory_identity.json").read_text(encoding="utf-8")) for path in inputs]
    for declared, version, count in zip(identities, (8, 9, 9, 9), (1288, 1312, 1332, 1332), strict=True):
        if (declared["factory_schema"] != 2 or declared["factory_version"] != version
                or declared["factory_count"] != count or declared["queries"] != freeze["works"]
                or declared["user_count"] != 0 or declared["recover_consumed_tail"]):
            raise ValueError(f"{name}: unexpected Worker identity {declared}")
    catalogs = [v9a.catalog(path / "factory_catalog.jsonl", None) for path in inputs]
    new_ids = set(manifest)
    source_events = {row["source"]: row["events"] for row in read_jsonl(inputs[-1] / "factory_catalog.jsonl")
                     if row["source"] in new_ids}
    rows, cohorts = [], defaultdict(list)
    ids, works = set(), set()
    for query, *replays in zip(read_jsonl(query_path),
                               *(read_jsonl(path / "factory_replay.jsonl") for path in inputs), strict=True):
        identity = query["id"]
        assert (identity not in ids and query["group"] not in works
                and all(replay["id"] == identity for replay in replays))
        ids.add(identity)
        works.add(query["group"])
        row = dict(id=identity, groups=["all", *v9a.tags(query)])
        baseline = {c["fingerprint"]: c for c in replays[1]["candidates"]}
        content_only = {c["fingerprint"] for c in replays[2]["candidates"]}
        anchor = query["prefix"][-1]["root"]
        baseline_routes = {motif(c["events"], anchor) for c in baseline.values() if c["source_end_preserved"]}
        for edition, replay, catalog in zip(LIBRARIES, replays, catalogs, strict=True):
            data = measure(query, replay, catalog, manifest, authored, source_events)
            existing = {c["fingerprint"]: c for c in replay["candidates"]}
            for fingerprint in existing.keys() & baseline.keys():
                assert tested_performance(existing[fingerprint]["events"]) == tested_performance(
                    baseline[fingerprint]["events"]), (identity, edition, fingerprint)
            data["lost_v9a_music"] = len(baseline.keys() - existing.keys())
            data["lost_content_only_music"] = len(content_only - existing.keys()) if edition == LIBRARIES[-1] else 0
            data["generated_same_as_v9a"] = replay["generated_identities"] == replays[1]["generated_identities"]
            new_routes = {motif(c["events"], anchor) for c in existing.values()
                          if c["source"] in new_ids and c["source_end_preserved"]}
            data["new_source_routes_vs_v9a"] = sum(len(route) >= 3 for route in new_routes - baseline_routes)
            row[edition] = data
        rows.append(row)
        for label in row["groups"]:
            cohorts[label].append(row)
    assert len(works) == freeze["works"]
    return dict(input_sha256=query_hash, works=len(rows), libraries=dict(zip(LIBRARIES, identities, strict=True)),
                cohorts={label: {edition: stats(group, edition) for edition in LIBRARIES}
                         for label, group in cohorts.items()}, per_work=rows)


def controls(queries, root, name):
    records = []
    path = root / name / "factory_replay.jsonl"
    for query, replay in zip(read_jsonl(queries), read_jsonl(path), strict=True):
        assert query["id"] == replay["id"]
        planned = tested_performance(query["target"])
        candidates = [c for c in replay["candidates"]
                      if c["source"] == query["id"] and c["source_end_preserved"] and
                      c["next"] == len(query["prefix"])]
        records.append(dict(id=query["id"], observed_source=any(c["source"] == query["id"] for c in replay["candidates"]),
                            intended_tail=bool(candidates), intended_performance=any(
                                tested_performance(c["events"]) == planned for c in candidates),
                            snapshot_sources=replay.get("snapshot_sources", 0),
                            exact_dispositions=Counter(row["disposition"] for row in replay["exact_trace"]),
                            contextual_dispositions=Counter(row["disposition"] for row in replay["context_trace"])))
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", required=True, type=Path)
    parser.add_argument("--cohort", action="append", default=[])
    parser.add_argument("--v9a", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--prepare-hard-control", action="store_true")
    args = parser.parse_args()
    root = args.evidence
    queries = root / "materials/controls.jsonl"
    if args.prepare_hard_control:
        control = next(item for item in read_jsonl(queries) if item["id"] == "V9B_004")
        # C natural conflicts with the first authored D-major triad.
        control["hard_melody"] = dict(pitch_class=0, onset=control["target"][0]["onset"], duration=1)
        write_jsonl(args.output, [control])
        return
    manifest = {row["id"]: row for row in json.loads((root / "materials/manifest.json").read_text())["entries"]}
    if args.v9a is None:
        parser.error("--v9a is required for evaluation")
    v9a_entries = json.loads((args.v9a / "materials/manifest.json").read_text())["entries"]
    authored = {row["id"]: row for row in v9a_entries} | manifest
    report = dict(results={name: compare(root / "confirmation" / f"{name}.jsonl", root, name, manifest, authored)
                           for name in args.cohort},
                  controls={edition: controls(queries, root, f"controls-{edition}")
                            for edition in LIBRARIES[2:]})
    write_json(args.output, report)


if __name__ == "__main__":
    main()
