"""Compare a frozen B5 model to the actual Factory-only Worker order."""

import argparse
import hashlib
import json
import statistics
import time
from collections import Counter
from pathlib import Path

from b5_data import quality_detail


def read_jsonl(path):
    with path.open(encoding="utf-8") as stream:
        for line in stream:
            yield json.loads(line)


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def motif(events, anchor):
    """Collapse adjacent equal L1 symbols, retaining the input-relative seam."""
    symbols = []
    for event in events:
        root = event["root"]
        symbol = ((root - anchor) % 12 if root is not None else None, event["quality"])
        if not symbols or symbol != symbols[-1]:
            symbols.append(symbol)
    return tuple(symbols)


def distance(left, right):
    """Unweighted Levenshtein distance over root/quality tuples."""
    if left == right:
        return 0
    previous = list(range(len(right) + 1))
    for index, left_event in enumerate(left, start=1):
        current = [index]
        for j, right_event in enumerate(right, start=1):
            current.append(min(current[-1] + 1, previous[j] + 1,
                               previous[j - 1] + (left_event != right_event)))
        previous = current
    return previous[-1]


def catalog_index(path):
    content, with_seam = set(), set()
    rows = list(read_jsonl(path))
    if len({row["source"] for row in rows}) != 1288 or len(rows) != 1288:
        raise ValueError("expected 1288 unique Factory sources")
    for row in rows:
        events = row["events"]
        # Compare complete authored tails. A change in detail alone cannot
        # create a new boundary in the L1 inventory.
        for i, event in enumerate(events):
            if event["root"] is None:
                continue
            if i and (event["root"], event["quality"]) == (
                events[i - 1]["root"], events[i - 1]["quality"]
            ):
                continue
            content.add(motif(events[i:], event["root"]))
            if i and events[i - 1]["root"] is not None:
                with_seam.add(motif(events[i:], events[i - 1]["root"]))
    return content, with_seam


def is_near(target, pool):
    if target in pool:
        return True
    # Look up one deletion, insertion or substitution. Every member of the
    # pool still points to an entire authored tail or returned continuation.
    return any(abs(len(candidate) - len(target)) <= 1
               and distance(target, candidate) <= 1 for candidate in pool)


def first_hit(distances, cutoff):
    return next((i for i, value in enumerate(distances, start=1) if value <= cutoff), None)


def rank_metrics(results, cut, ranking):
    rows = [row for row in results if row[f"original_{cut}"] is not None]
    ranks = [row[f"{ranking}_{cut}"] for row in rows]
    if not ranks:
        return {"n": 0}
    return {
        "n": len(ranks),
        "mrr": statistics.mean(1 / rank for rank in ranks),
        "recall_at_1": statistics.mean(rank <= 1 for rank in ranks),
        "recall_at_3": statistics.mean(rank <= 3 for rank in ranks),
        "recall_at_10": statistics.mean(rank <= 10 for rank in ranks),
        "median_rank": statistics.median(ranks),
    }


def summarize(results, rankings):
    coverage = Counter()
    for row in results:
        for cut in ("exact", "near"):
            content = row[f"catalog_{cut}"]
            reachable = row[f"original_{cut}"] is not None
            if reachable:
                stage = "reachable"
            elif content:
                stage = "catalog_only"
            else:
                stage = "absent_from_catalog_and_replay"
            coverage[f"{cut}_{stage}"] += 1
            coverage[f"{cut}_catalog"] += content
            coverage[f"{cut}_catalog_source_seam"] += row[f"catalog_seam_{cut}"]
    return {
        "n": len(results),
        "coverage": dict(coverage),
        "ranking_conditional_on_reachability": {
            cut: {ranking: rank_metrics(results, cut, ranking) for ranking in rankings}
            for cut in ("exact", "near")
        },
    }


def score_events(model, prefix, events):
    if not events or any(event["root"] is None for event in events):
        return None
    tonic = prefix[-1].get("tonic")
    changes = []
    for event in events:
        event = dict(event, detail=quality_detail(event["detail"]), tonic=tonic)
        fields = ("root", "quality", "detail", "bass")
        if changes and all(changes[-1][key] == event[key] for key in fields):
            previous = changes[-1]
            if previous["duration"] is not None and event["duration"] is not None:
                previous["duration"] += event["duration"]
            else:
                previous["duration"] = None
        else:
            changes.append(event)
    return model.phrase_scores(prefix, changes)


def evaluate(query, replay, content, with_seam, model, score_key):
    prefix, target = query["prefix"], query["target"]
    anchor = prefix[-1]["root"]
    target_content = motif(target, target[0]["root"])
    target_seam = motif(target, anchor)
    if len(target_seam) < 3:
        raise ValueError(f"{query['id']}: Factory target requires three L1 changes/events")
    row = {
        "id": query["id"],
        "group": query["group"],
        "worker_ms": replay["worker_ms"],
        "generated_count": replay["generated_count"],
        "target_length": len(target_seam),
        "catalog_exact": target_content in content,
        "catalog_near": is_near(target_content, content),
        "catalog_seam_exact": target_seam in with_seam,
        "catalog_seam_near": is_near(target_seam, with_seam),
    }
    candidates = replay["candidates"]
    row["factory_count"] = len(candidates)
    distances = [distance(target_seam, motif(candidate["events"], anchor)) for candidate in candidates]
    row["best_reachable_distance"] = min(distances, default=None)
    row["original_exact"] = first_hit(distances, 0)
    row["original_near"] = first_hit(distances, 1)
    for cut in ("exact", "near"):
        rank = row[f"original_{cut}"]
        row[f"original_{cut}_ui_row"] = candidates[rank - 1]["product_rank"] if rank else None

    if model is not None:
        # Offline comparison only. Product order is the deterministic tie
        # break, and an unsupported export never removes a legal candidate.
        start = time.perf_counter()
        scores = [score_events(model, prefix, c["events"]) for c in candidates]
        row["python_score_ms"] = 1000 * (time.perf_counter() - start)
        row["scored"] = sum(score is not None for score in scores)
        row["unseen_seam"] = sum(score is not None and score["support"] == 0 for score in scores)
        order = sorted(range(len(candidates)),
                       key=lambda i: -scores[i][score_key] if scores[i] is not None else float("inf"))
        # Preserve eligibility, and only reorder supported positions in the
        # original top ten. Equal rank sum avoids mixing incomparable scores.
        eligible = [i for i in range(min(10, len(candidates)))
                    if scores[i] is not None and scores[i]["support"] >= 10]
        stat_rank = {i: rank for rank, i in enumerate(sorted(eligible, key=lambda i: -scores[i][score_key]))}
        hybrid = list(range(len(candidates)))
        for slot, index in zip(eligible, sorted(eligible, key=lambda i: i + stat_rank[i]), strict=True):
            hybrid[slot] = index
        for label, sequence in (("statistical", order), ("hybrid", hybrid)):
            if sorted(sequence) != list(range(len(candidates))):
                raise ValueError("ranking changed candidate identity")
            ranked_distances = [distances[i] for i in sequence]
            row[f"{label}_exact"] = first_hit(ranked_distances, 0)
            row[f"{label}_near"] = first_hit(ranked_distances, 1)
            row[f"{label}_top3"] = [candidates[i]["id"] for i in sequence[:3]]
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--queries", type=Path, required=True)
    parser.add_argument("--replay", type=Path, required=True,
                        help="Directory created by b5_factory_replay")
    parser.add_argument("--selection", type=Path, help="Frozen validation selection.json")
    args = parser.parse_args()
    catalog_path = args.replay / "factory_catalog.jsonl"
    replay_path = args.replay / "factory_replay.jsonl"
    content, with_seam = catalog_index(catalog_path)
    model = score_key = None
    rankings = ["original"]
    identity = {}
    if args.selection:
        from b5_model import Model

        selection = json.loads(args.selection.read_text(encoding="utf-8"))
        model_path = args.selection.parent / selection["model"]
        if selection["sha256"] != sha256(model_path):
            raise ValueError("frozen model identity differs")
        score_key = selection["score"]
        model = Model.load(model_path)
        identity = {
            "model": str(selection["model"]),
            "model_sha256": sha256(model_path),
            "selection_sha256": sha256(args.selection),
            "score": score_key,
        }
        rankings.extend(("statistical", "hybrid"))

    queries = list(read_jsonl(args.queries))
    replays = list(read_jsonl(replay_path))
    query_ids = [query["id"] for query in queries]
    if not queries or query_ids != [replay["id"] for replay in replays] or len(set(query_ids)) != len(query_ids):
        raise ValueError("query and replay identities differ")
    results = [evaluate(query, replay, content, with_seam, model, score_key)
               for query, replay in zip(queries, replays, strict=True)]
    groups = sorted({row["group"] for row in results})
    worker_ms = sorted(row["worker_ms"] for row in results)
    report = {
        "inputs": {
            "query_sha256": sha256(args.queries),
            "replay_sha256": sha256(replay_path),
            "catalog_sha256": sha256(catalog_path),
            **identity,
        },
        "catalog_content_identities": len(content),
        "catalog_source_seam_identities": len(with_seam),
        "worker_ms": {
            "median": statistics.median(worker_ms),
            "p95_nearest_rank": worker_ms[(95 * len(worker_ms) - 1) // 100],
            "maximum": worker_ms[-1],
        },
        "all": summarize(results, rankings),
        "query_groups": len(groups),
        "per_query": results,
    }
    report_path = args.replay / ("factory_evaluation.json" if model else "factory_coverage.json")
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report["all"], ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
