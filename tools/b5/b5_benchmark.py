"""Prepare fixed work splits, select on validation, and evaluate untouched songs."""

import argparse
import json
import math
import random
import statistics
from collections import Counter, defaultdict
from pathlib import Path

from b5_data import (SEED, SOURCE_HASHES, identity, partition, read_jsonl, read_sources, runs,
                     sha256, stable, stats, write_json, write_jsonl)
from b5_model import Model, duration_bucket

SCORES = ("total", "mean", "lift")
SAMPLE_LIMIT = 8
DISTRACTORS = 9
FROZEN_FILES = ("splits.json", "train.jsonl", "validation.jsonl", "test.jsonl", "cross_source.jsonl",
                "queries_validation.jsonl", "queries_test.jsonl", "queries_cross_source.jsonl",
                "factory_queries.jsonl")
CONFIGS = [
    ("unigram_abs", "fixed", 0, "absolute", "L1", 8),
    ("unigram_interval", "fixed", 0, "anchor", "L1", 8),
    ("fixed_1", "fixed", 1, "anchor", "L1", 8),
    ("fixed_2", "fixed", 2, "anchor", "L1", 8),
    ("fixed_3", "fixed", 3, "anchor", "L1", 8),
    ("variable_abs", "variable", 4, "absolute", "L1", 8),
    ("variable_anchor", "variable", 4, "anchor", "L1", 8),
    ("variable_interval", "variable", 4, "interval", "L1", 8),
    ("variable_tonic", "variable", 4, "tonic", "L1", 8),
    ("variable_l2", "variable", 4, "anchor", "L2", 8),
    ("variable_l3", "variable", 4, "anchor", "L3", 8),
]


def relative_shape(events, anchor):
    return tuple(((item["root"] - anchor) % 12, item["quality"]) for item in events)


def l1_length(items):
    symbols = [identity(item, "L1") for item in items]
    return sum(i == 0 or symbol != symbols[i - 1] for i, symbol in enumerate(symbols))


def query(work, prefix, target, kind, coordinate):
    return dict(id=f"{work['id']}:{kind}:{coordinate}", group=work["group"], work=work["id"],
                source=work["source"], artist=work["artist"], kind=kind, prefix=prefix, target=target)


def candidates_for(work):
    options = defaultdict(list)
    lines = {line: work["stream"][start:end] for line, start, end in work["sections"]}
    for run_index, run in enumerate(runs(work["stream"])):
        for i in range(4, len(run) - 2):
            coordinate = f"{run_index}:{i}"
            length = 3 + int(stable(work["id"], coordinate), 16) % 4
            target = run[i:i + length]
            if len(target) == length:
                options["window"].append(query(work, run[i-4:i], target, "window", coordinate))
        if work["source"] != "billboard":
            continue
        # Complete source annotation rows, beginning at a change event.
        for i in range(4, len(run)):
            line = run[i].get("line")
            if run[i - 1]["end_line"] >= line:
                continue
            j = i
            while j < len(run) and run[j]["line"] == line and run[j]["end_line"] == line:
                j += 1
            if not 3 <= j - i <= 12:
                continue
            # Check that no gap or merged event cut off the end of this row.
            if j < len(run) and run[j]["line"] == line:
                continue
            source_runs = list(runs(lines[line]))
            if len(source_runs) != 1 or list(map(identity, source_runs[0])) != list(map(identity, run[i:j])):
                continue
            if any(event["root"] is None or event["duration"] is None for event in lines[line]):
                continue
            options["source_line"].append(query(work, run[i-4:i], run[i:j], "source_line", f"{run_index}:{i}"))
    return options


def transplant(donor, anchor):
    offset = (anchor - donor["prefix"][-1]["root"]) % 12
    events = []
    for item in donor["target"]:
        event = dict(item)
        for field in ("root", "bass"):
            if event.get(field) is not None:
                event[field] = (event[field] + offset) % 12
        # The product would continue in the input key context. Keep donor
        # annotations in the normalized source, but never pass them as hints.
        event["tonic"] = None
        event["mode"] = None
        events.append(event)
    return events


def freeze_queries(pool):
    by_kind = defaultdict(list)
    for item in pool:
        target = item["target"]
        shape = relative_shape(target, item["prefix"][-1]["root"])
        timing = tuple(duration_bucket(event["duration"]) for event in target)
        by_kind[item["kind"]].append((item, shape, timing))
    for item in pool:
        prefix, target = item["prefix"], item["target"]
        anchor = prefix[-1]["root"]
        shape = relative_shape(target, anchor)
        target_duration = [duration_bucket(event["duration"]) for event in target]
        comparison = []
        for other, other_shape, other_timing in by_kind[item["kind"]]:
            if item["group"] == other["group"]:
                continue
            if shape == other_shape:
                continue
            length = abs(len(other_shape) - len(shape))
            seam = (other_shape[0][0] != shape[0][0]) + (other_shape[0][1] != shape[0][1])
            rhythm = sum(left != right for left, right in zip(target_duration, other_timing))
            structure = sum(left[1] != right[1] for left, right in zip(shape, other_shape))
            tie = stable(item["id"], other["id"])
            comparison.append(((seam, length, rhythm, structure, tie), other_shape, other))
        ordinary_order = sorted(comparison, key=lambda record: record[0][-1])
        difficult_order = sorted(comparison, key=lambda record: record[0])
        item["alternatives"] = {}
        for label, order in (("ordinary", ordinary_order), ("hard", difficult_order)):
            seen_works, seen_shapes = set(), set()
            alternatives = []
            for match, motif, donor in order:
                if donor["group"] in seen_works or motif in seen_shapes:
                    continue
                seen_works.add(donor["group"])
                seen_shapes.add(motif)
                alternatives.append(dict(id=donor["id"], group=donor["group"],
                                         match=match[:4], events=transplant(donor, anchor)))
                if len(alternatives) == DISTRACTORS:
                    break
            item["alternatives"][label] = alternatives
        if all(len(options) == DISTRACTORS for options in item["alternatives"].values()):
            yield item


def factory_inputs(queries):
    selected = {}
    for item in sorted(queries, key=lambda row: (row["kind"] != "source_line", stable(row["id"]))):
        prefix = item["prefix"]
        if (item["group"] in selected or not 3 <= l1_length(item["target"]) <= 6
                or any(event["unit"] != "qn" for event in prefix)
                or len({event["meter"] for event in prefix}) != 1):
            continue
        start = prefix[0]["phase"]
        adjusted = []
        for event in prefix:
            adjusted.append(dict(event, onset=start))
            start += event["duration"]
        selected[item["group"]] = dict(id=item["id"], group=item["group"], kind=item["kind"],
                                       prefix=adjusted, target=item["target"])
    return sorted(selected.values(), key=lambda row: stable(row["group"]))[:100]


def prepare(source_dir, output):
    works = read_sources(source_dir)
    reasons = partition(works)
    split_counts = stats(works)
    manifest = dict(seed=SEED, source_hashes=SOURCE_HASHES, grouping_links=reasons, counts=split_counts,
                    assignments=[{key: work[key] for key in ("id", "group", "split")} for work in works])
    write_json(output / "splits.json", manifest)
    query_counts = {}
    for split in ("train", "validation", "test", "cross_source"):
        members = [work for work in works if work["split"] == split]
        write_jsonl(output / f"{split}.jsonl", members)
        if split == "train":
            continue
        pool = []
        for work in members:
            for kind, items in candidates_for(work).items():
                limit = SAMPLE_LIMIT if kind == "window" else SAMPLE_LIMIT // 2
                pool.extend(sorted(items, key=lambda item: stable(item["id"]))[:limit])
        queries = list(freeze_queries(pool))
        write_jsonl(output / f"queries_{split}.jsonl", queries)
        if split == "test":
            write_jsonl(output / "factory_queries.jsonl", factory_inputs(queries))
        query_counts[split] = {
            "queries": dict(Counter(row["kind"] for row in queries)),
            "groups": len({row["group"] for row in queries}),
            "hard_same_seam": sum(alt["match"][0] == 0 for row in queries for alt in row["alternatives"]["hard"]),
            "hard_same_length": sum(alt["match"][1] == 0 for row in queries for alt in row["alternatives"]["hard"]),
            "total_hard": len(queries) * DISTRACTORS,
        }
    write_json(output / "freeze.json", dict(seed=SEED, files={name: sha256(output / name) for name in FROZEN_FILES},
                                           query_counts=query_counts))
    print(json.dumps(query_counts, indent=2))


def verify_freeze(output):
    frozen = json.loads((output / "freeze.json").read_text(encoding="utf-8"))
    for name, expected in frozen["files"].items():
        if sha256(output / name) != expected:
            raise ValueError(f"Frozen data hash differs: {name}")
    return frozen


def cohorts(item, train_frequency):
    prefix, target = item["prefix"], item["target"]
    first = target[0]
    tags = [item["kind"], f"length:{len(target)}", f"source:{item['source']}"]
    if first["mode"]:
        tags.append(f"mode:{first['mode']}")
        tonic = prefix[-1]["tonic"]
        if tonic is not None:
            scale = (0, 2, 3, 5, 7, 8, 10) if prefix[-1]["mode"] == "minor" else (0, 2, 4, 5, 7, 9, 11)
            tags.append("root_diatonic" if (first["root"] - tonic) % 12 in scale else "root_chromatic")
    complex_ = any(item["detail"] not in ("maj", "min", "5", "1", "7") for item in target)
    tags.append("extended_or_special" if complex_ else "basic")
    first_change = identity(prefix[-1], "L1") != identity(first, "L1")
    tags.append("L1_change" if first_change else "L1_repeat")
    if first["duration"] is not None:
        tags.append(f"qn:{duration_bucket(first['duration'])}")
    previous = prefix[-1]
    frequency = train_frequency.get(f"{(first['root'] - previous['root']) % 12}:{first['quality']}", 0)
    tags.append("low_frequency_transition" if frequency < 30 else "supported_transition")
    if item["source"] == "mpb":
        tags.append(f"composer:{item['artist']}")
    return tags


def ranking_metrics(scores, score_name):
    positive, *alternatives = [score[score_name] for score in scores]
    better = sum(value > positive + 1e-12 for value in alternatives)
    tied = sum(abs(value - positive) <= 1e-12 for value in alternatives)
    ranks = range(better + 1, better + tied + 2)
    return dict(mrr=statistics.mean(1 / rank for rank in ranks),
                recall1=statistics.mean(rank <= 1 for rank in ranks),
                recall3=statistics.mean(rank <= 3 for rank in ranks),
                recall5=statistics.mean(rank <= 5 for rank in ranks),
                expected_rank=statistics.mean(ranks))


def work_average(rows):
    by_work = defaultdict(list)
    for group, metrics in rows:
        by_work[group].append(metrics)
    return {group: {metric: statistics.mean(row[metric] for row in records)
                    for metric in records[0]} for group, records in by_work.items()}


def summary(rows):
    by_work = work_average(rows)
    return dict(n=len(rows), works=len(by_work),
                **{key: statistics.mean(row[key] for row in by_work.values())
                   for key in next(iter(by_work.values()), {})})


def evaluate(model, queries, train_frequency):
    score_rows = defaultdict(lambda: defaultdict(list))
    for item in queries:
        prefix = item["prefix"]
        # Every candidate uses the same key evidence, from the last input
        # event. Actual future annotation keys never enter the scorer.
        def score(events):
            blinded = [dict(event, tonic=prefix[-1]["tonic"], mode=prefix[-1]["mode"]) for event in events]
            return model.phrase_scores(prefix, blinded)

        actual = score(item["target"])
        probability, better, tied, order, support = model.next_event(prefix, item["target"][0])
        tags = ["all", *cohorts(item, train_frequency), f"order:{order}",
                "no_context" if not support else "low_support" if support < 10 else "supported_context"]
        prediction = dict(nll=-math.log(probability), top1=max(0, min(1, (1 - better) / tied)),
                          top3=max(0, min(1, (3 - better) / tied)),
                          top5=max(0, min(1, (5 - better) / tied)),
                          mean_order=order, mean_support=support, fallback=(support == 0))
        for tag in tags:
            score_rows["next"][tag].append((item["group"], prediction))
        for kind, alternatives in item["alternatives"].items():
            scores = [actual, *[score(option["events"]) for option in alternatives]]
            for measure in SCORES:
                metrics = ranking_metrics(scores, measure)
                for tag in tags:
                    score_rows[f"{kind}:{measure}"][tag].append((item["group"], metrics))
    aggregated = {name: {tag: summary(rows) for tag, rows in categories.items()}
                  for name, categories in score_rows.items()}
    primary = {name: work_average(categories["all"]) for name, categories in score_rows.items()}
    return aggregated, primary


def training_groups(path):
    grouped = defaultdict(list)
    for work in read_jsonl(path):
        grouped[work["group"]].extend(runs(work["stream"]))
    return grouped


def fit(output):
    frozen = verify_freeze(output)
    train = training_groups(output / "train.jsonl")
    validation = list(read_jsonl(output / "queries_validation.jsonl"))
    results = {}
    train_frequency = None
    for name, kind, order, representation, level, strength in CONFIGS:
        config = dict(name=name, kind=kind, order=order, representation=representation,
                      level=level, strength=strength, train_sha256=frozen["files"]["train.jsonl"])
        model = Model.fit(config, train.values())
        if name == "unigram_interval":
            train_frequency = model.tables[""][2]
        model.save(output / f"{name}.json.gz")
    for name, *_ in CONFIGS:
        model = Model.load(output / f"{name}.json.gz")
        results[name], _ = evaluate(model, validation, train_frequency)
        print(f"validation {name}: NLL {results[name]['next']['all']['nll']:.4f}, "
              f"hard mean MRR {results[name]['hard:mean']['all']['mrr']:.4f}", flush=True)
    write_json(output / "validation_metrics.json", results)
    # Compare a validation-selected variable model to the best lower-order
    # baseline. Test never chooses either model or its sequence scoring rule.
    selection = {}
    for label, predicate in (("candidate", lambda name: name.startswith("variable")),
                             ("baseline", lambda name: not name.startswith("variable"))):
        name, score = max(((name, score) for name in results if predicate(name) for score in SCORES),
                          key=lambda pair: (results[pair[0]][f"hard:{pair[1]}"]["all"]["mrr"],
                                            results[pair[0]][f"hard:{pair[1]}"]["source_line"]["mrr"],
                                            -results[pair[0]]["next"]["all"]["nll"]))
        path = output / f"{name}.json.gz"
        selection[label] = dict(model=path.name, score=score, sha256=sha256(path))
    write_json(output / "selection.json", dict(**selection["candidate"], baseline=selection["baseline"],
                                               rule="validation work-macro hard MRR, line MRR, next-event NLL",
                                               freeze_sha256=sha256(output / "freeze.json")))


def paired_interval(candidate, baseline, metric):
    assert set(candidate) == set(baseline)
    values = [candidate[group][metric] - baseline[group][metric] for group in sorted(candidate)]
    randomizer = random.Random(SEED)
    samples = sorted(statistics.mean(randomizer.choices(values, k=len(values))) for _ in range(1500))
    return dict(delta=statistics.mean(values), low95=samples[37], high95=samples[1462], works=len(values))


def test(output):
    verify_freeze(output)
    selection = json.loads((output / "selection.json").read_text(encoding="utf-8"))
    assert selection["freeze_sha256"] == sha256(output / "freeze.json")
    for info in (selection, selection["baseline"]):
        assert info["sha256"] == sha256(output / info["model"])
    frequency = Model.load(output / "unigram_interval.json.gz").tables[""][2]
    results = {}
    for split in ("test", "cross_source"):
        queries = list(read_jsonl(output / f"queries_{split}.jsonl"))
        scores = {}
        group_metrics = {}
        for name, *_ in CONFIGS:
            scores[name], group_metrics[name] = evaluate(Model.load(output / f"{name}.json.gz"), queries, frequency)
            print(f"{split} {name}: NLL {scores[name]['next']['all']['nll']:.4f}", flush=True)
        candidate = selection["model"].removesuffix(".json.gz")
        base = selection["baseline"]["model"].removesuffix(".json.gz")
        comparisons = {}
        for kind, metric in (("hard", "mrr"), ("ordinary", "mrr"), ("next", "nll")):
            candidate_key = f"{kind}:{selection['score']}" if kind != "next" else kind
            base_key = f"{kind}:{selection['baseline']['score']}" if kind != "next" else kind
            comparisons[kind] = paired_interval(group_metrics[candidate][candidate_key],
                                                 group_metrics[base][base_key], metric)
        results[split] = dict(metrics=scores, paired_candidate_minus_baseline=comparisons)
    write_json(output / "test_metrics.json", results)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage", choices=("prepare", "fit", "test"))
    parser.add_argument("--sources", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    if args.stage == "prepare":
        if args.sources is None:
            parser.error("prepare requires --sources")
        prepare(args.sources, args.output)
    elif args.stage == "fit":
        fit(args.output)
    else:
        test(args.output)


if __name__ == "__main__":
    main()
