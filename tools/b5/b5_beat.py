"""Evaluate quarter-note-grid prediction, stratified by real harmonic change."""

import argparse
from collections import defaultdict
from pathlib import Path

from b5_benchmark import summary, verify_freeze
from b5_data import identity, read_jsonl, runs, write_json
from b5_model import Model


def grid_runs(work):
    for run in runs(work["stream"]):
        samples = []
        position = 0.0
        tick = 0
        for item in run:
            if item["duration"] is None or item["meter"] != "4/4":
                if samples:
                    yield samples
                samples = []
                position = 0.0
                tick = 0
                continue
            if position == 0:
                # Align the sampling grid with the original bar, even if a
                # valid run starts between beats after an annotation gap.
                position = item["phase"]
                tick = int(position)
                if tick < position:
                    tick += 1
            position += item["duration"]
            while tick < position - 1e-8:
                samples.append(item)
                tick += 1
        if samples:
            yield samples


def evaluate(model, path):
    rows = defaultdict(list)
    for work in read_jsonl(path):
        for run in grid_runs(work):
            for i in range(4, len(run)):
                history, item = run[i-4:i], run[i]
                probability, better, tied, _, _ = model.next_event(history, item)
                same = identity(item, "L1") == identity(history[-1], "L1")
                record = (work["group"], {"top1": max(0, min(1, (1 - better) / tied)), "hold": int(same)})
                rows["all"].append(record)
                rows["same" if same else "change"].append(record)
    return {category: summary(values) for category, values in rows.items()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    frozen = verify_freeze(args.output)
    groups = defaultdict(list)
    for work in read_jsonl(args.output / "train.jsonl"):
        groups[work["group"]].extend(grid_runs(work))
    config = dict(name="beat_grid", kind="variable", order=4, representation="anchor",
                  level="L1", strength=8, train_sha256=frozen["files"]["train.jsonl"])
    model = Model.fit(config, groups.values())
    model.save(args.output / "beat_grid.json.gz")
    results = evaluate(model, args.output / "test.jsonl")
    write_json(args.output / "beat_metrics.json", results)


if __name__ == "__main__":
    main()
