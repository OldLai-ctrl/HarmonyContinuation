"""Join V9-A's frozen native-seam witnesses to its actual Worker trace."""

import argparse
import json
import sqlite3
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "b5"))
from b5_data import read_jsonl, sha256, write_json
from b5_factory import distance, motif


def core(quality):
    return {"dom": "maj", "hdim": "dim"}.get(quality, quality)


def bass(chord):
    return chord["root"] if chord["bass"] is None else chord["bass"]


def tones(chord, shift=0):
    return {(chord["root"] + i + shift) % 12 for i in chord["intervals"]} | {
        (bass(chord) + shift) % 12
    }


def chord_conflicts(actual, source, shift):
    """Static cross-check of Contextual compatible(), with exported pitches."""
    checks = {
        "root": actual["root"] != (source["root"] + shift) % 12,
        "quality": core(actual["quality"]) != core(source["quality"]),
        "explicit_bass": actual["bass"] is not None and actual["bass"] != (bass(source) + shift) % 12,
        "actual_tones_not_subset": not tones(actual) <= tones(source, shift),
    }
    return [name for name, found in checks.items() if found]


def project_key(source, actual_root, source_root):
    shift = (actual_root - source_root) % 12
    return (source["tonic"] + shift) % 12 + 12 * source["mode"], shift


def explain_path(query, replay, source, next_index, key, shift, near_distance):
    source_id = source["source"]
    exact = [item for item in replay["exact_trace"] if item["source"] == source_id and item["key"] == key]
    contextual = [item for item in replay["context_trace"] if item["source"] == source_id
                  and item["key"] == key and item["next"] == next_index]
    at_end = [item for item in exact if item["next"] == next_index]
    retained = {"TARGET_RETAINED", "EXPLORATORY_RETAINED"}
    exact_reason = at_end[0]["disposition"] if at_end else ("alternate_endpoint" if exact else "untried_key")
    contextual_reason = (contextual[0]["disposition"] if contextual else
                         "no_internal_anchor" if next_index < 2 else "untried_key")
    primary = [item for item in replay["candidates"] if item["source"] == source_id
               and item["next"] == next_index and item["key"] == key]
    evidence = [item for item in replay["candidates"] if any(entry["source"] == source_id
                and entry["next"] == next_index and entry["key"] == key
                for entry in item["evidence"])]
    conflicts = [chord_conflicts(actual, expected, shift) for actual, expected in zip(
        reversed(query["prefix"][-next_index:]), reversed(source["events"][:next_index]))]
    return dict(source=source_id, next=next_index, key=key, near_distance=near_distance,
                exact_reason=exact_reason, contextual_reason=contextual_reason,
                exact_aligned=exact, contextual_trace=contextual,
                exact_qualified=exact_reason in retained,
                contextual_qualified=contextual_reason == "qualified-before-structure",
                primary_ranks=[item["product_rank"] for item in primary],
                evidence_ranks=[item["product_rank"] for item in evidence],
                context_conflicts_from_latest=conflicts,
                two_event_context_compatible=len(conflicts) >= 2 and not any(conflicts[:2]))


def explain_witnesses(queries, replays, sources):
    witnesses = []
    for query, replay in zip(read_jsonl(queries), read_jsonl(replays), strict=True):
        assert query["id"] == replay["id"]
        anchor = query["prefix"][-1]["root"]
        target = motif(query["target"], anchor)
        details = []
        for source in sources.values():
            events = source["events"]
            for next_index in range(1, len(events)):
                current, following = events[next_index - 1:next_index + 1]
                if (current["root"], current["quality"]) == (following["root"], following["quality"]):
                    continue  # same L1 event; matches V9-A catalog()
                near = distance(target, motif(events[next_index:], current["root"]))
                if near > 1:
                    continue
                key, shift = project_key(source, anchor, current["root"])
                details.append(explain_path(query, replay, source, next_index, key, shift, near))
        if details:
            witnesses.append(dict(id=query["id"], routes=details, all_candidates=len(replay["candidates"])))
    return witnesses


def explain_controls(queries, replay, sources, failed):
    results = []
    for query, played in zip(read_jsonl(queries), read_jsonl(replay), strict=True):
        assert query["id"] == played["id"]
        if query["id"] not in failed:
            continue
        source = sources[query["id"]]
        index = len(query["prefix"])
        key = source["tonic"] + 12 * source["mode"]
        details = explain_path(query, played, source, index, key, 0, 0)
        current, following = query["prefix"][-1], query["target"][0]
        details["source_boundary"] = dict(from_event=current, to_event=following,
                                          common_tone_count=len(tones(current) & tones(following)),
                                          bass_leap=min((bass(following) - bass(current)) % 12,
                                                        (bass(current) - bass(following)) % 12))
        results.append(details)
    return results


def read_db(path):
    with sqlite3.connect(path.resolve().as_uri() + "?immutable=1", uri=True) as conn:
        return dict(conn.execute("SELECT id, payload FROM progressions"))


def projection_summary(v9a, directory, sources, outcomes):
    baseline_db = v9a / "library/factory.db"
    rewritten_db = directory / "diagnosis_projection.db"
    raw_before = read_db(baseline_db)
    raw_after = read_db(rewritten_db)
    assert raw_before.keys() == raw_after.keys()
    functions, changed_sources = Counter(), []
    for source, payload in raw_before.items():
        rewritten = raw_after[source]
        if payload == rewritten:
            continue
        assert source in sources
        original = json.loads(payload)
        updated = json.loads(rewritten)
        source_notes = json.loads(original.pop("typedData"))
        changed_notes = json.loads(updated.pop("typedData"))
        assert original == updated
        for a, b in zip(source_notes["full"], changed_notes["full"], strict=True):
            if a["function"] != b["function"]:
                assert a["function"] == 0
                functions[str(b["function"])] += 1
                a["function"] = b["function"]
        assert source_notes == changed_notes
        changed_sources.append(source)
    control_queries = v9a / "materials/controls.jsonl"
    controls = directory / "diagnosis_projection_replay/factory_replay.jsonl"
    native_hits = set()
    for query, replay in zip(read_jsonl(control_queries), read_jsonl(controls), strict=True):
        assert query["id"] == replay["id"]
        source = sources[query["id"]]
        target = query["target"]
        if any(candidate["key"] == source["tonic"] + 12 * source["mode"] and
               candidate["source"] == source["source"] and candidate["next"] == len(query["prefix"]) and
               candidate["source_end_preserved"] and len(candidate["events"]) == len(target) and
               all((a["root"], bass(a), a["intervals"]) == (b["root"], bass(b), b["intervals"]) and
                   abs(a["duration"] - b["duration"]) <= 1e-6
                   for a, b in zip(candidate["events"], target, strict=True))
               for candidate in replay["candidates"]):
            native_hits.add(query["id"])
    queries = v9a / "confirmation/confirmation.jsonl"
    before = v9a / "confirmation-v9-trace/factory_replay.jsonl"
    after = directory / "diagnosis_projection_development/factory_replay.jsonl"
    native = explain_witnesses(queries, after, sources)
    near_before, near_after, lost = set(), set(), []
    count_before, count_after = 0, 0
    for query, old, new in zip(read_jsonl(queries), read_jsonl(before), read_jsonl(after), strict=True):
        assert query["id"] == old["id"] == new["id"]
        a = {c["fingerprint"]: c for c in old["candidates"]}
        b = {c["fingerprint"]: c for c in new["candidates"]}
        target = motif(query["target"], query["prefix"][-1]["root"])
        for candidates, near in ((a, near_before), (b, near_after)):
            if any(distance(target, motif(c["events"], query["prefix"][-1]["root"])) <= 1
                   for c in candidates.values()):
                near.add(query["id"])
        count_before += len(a)
        count_after += len(b)
        for identity in sorted(a.keys() - b.keys()):
            lost.append(dict(query=query["id"], source=a[identity]["source"], fingerprint=identity))
    return dict(input_sha256={"baseline_db": sha256(baseline_db), "rewritten_db": sha256(rewritten_db),
                             "queries": sha256(queries)},
                output_replay_sha256={"controls": sha256(controls), "development": sha256(after)},
                only_inferred_fields_changed=functions,
                changed_sources=changed_sources,
                passed_baseline=sum(bool(row["exact_performance"]) for row in outcomes["controls"]),
                counterfactual_exact_performance_controls=sorted(native_hits),
                development_works_near_before=len(near_before),
                development_works_near_after=len(near_after),
                development_new_near_works=sorted(near_after - near_before),
                development_lost_near_works=sorted(near_before - near_after),
                development_candidates_before=count_before,
                development_candidates_after=count_after,
                development_lost_candidates=lost,
                development_native_witnesses=[
                    dict(id=row["id"], routes=[route for route in row["routes"] if route["primary_ranks"]])
                    for row in native if any(route["primary_ranks"] for route in row["routes"])])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--v9a", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--projection-dir", type=Path)
    args = parser.parse_args()
    root = args.v9a
    evaluation = root / "evaluation.json"
    queries = root / "confirmation/confirmation.jsonl"
    confirm = root / "confirmation-v9-trace/factory_replay.jsonl"
    catalog = root / "confirmation-v9-trace/factory_catalog.jsonl"
    controls = root / "materials/controls.jsonl"
    controls_replay = root / "controls-v9/factory_replay.jsonl"
    files = (evaluation, queries, confirm, catalog, controls, controls_replay)
    outcomes = json.loads(evaluation.read_text(encoding="utf-8"))
    assert sha256(confirm) == outcomes["trace"]["sha256"]
    sources = {item["source"]: item for item in read_jsonl(catalog) if item["template_version"] == 9}
    if args.projection_dir:
        write_json(args.output, projection_summary(root, args.projection_dir, sources, outcomes))
        return
    witnesses = explain_witnesses(queries, confirm, sources)
    expected = {item["id"] for item in outcomes["per_work"] if item["increment_seam_near"]}
    assert {item["id"] for item in witnesses} == expected
    failed = {item["id"] for item in outcomes["controls"] if not item["exact_performance"]}
    failed_controls = explain_controls(controls, controls_replay, sources, failed)
    assert {item["source"] for item in failed_controls} == failed
    routes = [route for item in witnesses for route in item["routes"]]
    report = dict(
        input_sha256={path.relative_to(root).as_posix(): sha256(path) for path in files},
        frozen_overall=outcomes["trace"],
        native_seam_works=len(witnesses),
        native_seam_routes=len(routes),
        exact_first_exits=Counter(item["exact_reason"] for item in routes),
        contextual_first_exits=Counter(item["contextual_reason"] for item in routes),
        exact_exit_works=Counter(reason for item in witnesses for reason in
                                 {route["exact_reason"] for route in item["routes"]}),
        contextual_exit_works=Counter(reason for item in witnesses for reason in
                                      {route["contextual_reason"] for route in item["routes"]}),
        alternate_endpoint_qualified=sum(route["exact_reason"] == "alternate_endpoint" and any(
            item["disposition"] in ("TARGET_RETAINED", "EXPLORATORY_RETAINED") for item in route["exact_aligned"])
            for route in routes),
        native_two_event_context_compatible=sum(item["two_event_context_compatible"] for item in routes),
        native_terminal_compatible=sum(not item["context_conflicts_from_latest"][0] for item in routes),
        native_qualified_routes=sum(item["exact_qualified"] or item["contextual_qualified"] for item in routes),
        native_emitted_routes=sum(bool(item["primary_ranks"] or item["evidence_ranks"]) for item in routes),
        native_witnesses=witnesses,
        failed_controls=failed_controls,
    )
    write_json(args.output, report)


if __name__ == "__main__":
    main()
