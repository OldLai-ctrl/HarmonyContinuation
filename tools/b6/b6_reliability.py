"""Measure bounded probability backoff on the frozen B5 benchmark."""

import argparse
import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "b5"))

from b5_benchmark import evaluate, paired_interval, verify_freeze
from b5_data import read_jsonl, sha256, write_json
from b5_model import Model, VOCABULARY

BASELINE = "b5_original"
VARIANTS = ("fixed_backoff", "support_backoff")
CONFIGURATIONS = {
    "fixed_backoff": "1/8 weight on the B5 marginal when a context exists",
    "support_backoff": "4/(8+n) weight on the B5 marginal, n = works supporting the longest context",
}
MRR_TOLERANCE = 0.01
NLL_TOLERANCE = 0.02


class BackoffModel(Model):
    def __init__(self, base, variant):
        if variant not in VARIANTS:
            raise ValueError(f"Unknown backoff variant: {variant}")
        super().__init__(base.config, base.tables)
        self.variant = variant

    def probability(self, history, outcome, contexts=None):
        if contexts is None:
            contexts = self.contexts(history)
        conditional = super().probability(history, outcome, contexts)
        if not contexts:
            return conditional
        support = contexts[-1][1][0]
        mix = 0.125 if self.variant == "fixed_backoff" else 4 / (8 + support)
        marginal = super().probability((), outcome, ())
        return (1 - mix) * conditional + mix * marginal


def read_selection(path):
    return json.loads(path.read_text(encoding="utf-8"))


def load_adapted(benchmark, selection_path):
    """Load the validation-selected variant for a separate offline Factory replay."""
    selection = read_selection(selection_path)
    path = benchmark / selection["model"]
    if (sha256(path) != selection["model_sha256"]
            or sha256(benchmark / "freeze.json") != selection["freeze_sha256"]):
        raise ValueError("B6 reliability selection refers to different B5 inputs")
    return BackoffModel(Model.load(path), selection["variant"])


def check_distributions(model, queries):
    histories = [[], [{"root": 0, "quality": "other"}] * 4]
    histories.extend(query["prefix"] for query in queries[:20])
    for history in histories:
        contexts = model.contexts(history)
        probabilities = [model.probability(history, outcome, contexts) for outcome in VOCABULARY]
        if min(probabilities) <= 0 or not math.isclose(sum(probabilities), 1.0, abs_tol=1e-12):
            raise ValueError("Probability distribution is not positive and normalized")
    for query in queries[:20]:
        score = model.phrase_scores(query["prefix"], query["target"])
        if score != model.phrase_scores(query["prefix"], query["target"]):
            raise ValueError("Repeated phrase evaluation changed its score")


def metrics(result):
    return {
        "nll": result["next"]["all"]["nll"],
        "rare_nll": result["next"]["low_frequency_transition"]["nll"],
        "hard_mrr": result["hard:lift"]["all"]["mrr"],
        "ordinary_mrr": result["ordinary:lift"]["all"]["mrr"],
    }


def qualifies(base, variant):
    return (
        variant["rare_nll"] < base["rare_nll"]
        and variant["nll"] <= base["nll"] + NLL_TOLERANCE
        and variant["hard_mrr"] >= base["hard_mrr"] - MRR_TOLERANCE
    )


def evaluate_models(benchmark, split, models, frequency):
    queries = list(read_jsonl(benchmark / f"queries_{split}.jsonl"))
    results = {}
    work_metrics = {}
    for name, model in models.items():
        results[name], work_metrics[name] = evaluate(model, queries, frequency)
        print(f"{split} {name}: {json.dumps(metrics(results[name]))}", flush=True)
    return results, work_metrics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--benchmark", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    benchmark = args.benchmark
    output = args.output
    frozen = verify_freeze(benchmark)
    b5_selection = read_selection(benchmark / "selection.json")
    model_path = benchmark / b5_selection["model"]
    model_sha256 = sha256(model_path)
    freeze_sha256 = sha256(benchmark / "freeze.json")
    if (b5_selection["sha256"] != model_sha256
            or b5_selection["freeze_sha256"] != freeze_sha256
            or b5_selection["score"] != "lift"):
        raise ValueError("The B5 selection and frozen benchmark identities differ")
    original = Model.load(model_path)
    if original.config["train_sha256"] != frozen["files"]["train.jsonl"]:
        raise ValueError("The B5 model refers to a different training split")

    frequency = Model.load(benchmark / "unigram_interval.json.gz").tables[""][2]
    models = {BASELINE: original, **{variant: BackoffModel(original, variant) for variant in VARIANTS}}
    output.mkdir(parents=True, exist_ok=True)
    validation, _ = evaluate_models(benchmark, "validation", models, frequency)
    validation_metrics = {name: metrics(values) for name, values in validation.items()}
    candidates = [variant for variant in VARIANTS
                  if qualifies(validation_metrics[BASELINE], validation_metrics[variant])]
    # Freeze the closest variant for a confirmatory comparison even if the
    # validation gate rules it out for recommendation use.
    selected = min(candidates or VARIANTS, key=lambda name: (
        validation_metrics[name]["rare_nll"], -validation_metrics[name]["hard_mrr"],
        validation_metrics[name]["nll"],
    ))
    selection = dict(
        variant=selected, model=b5_selection["model"], model_sha256=model_sha256,
        freeze_sha256=freeze_sha256, score="lift", configuration=CONFIGURATIONS,
        validation_gate_passed=bool(candidates), validation_eligible=candidates,
        selection_rule="minimize rare NLL among variants satisfying the validation gate",
        gate=dict(max_hard_mrr_loss=MRR_TOLERANCE, max_nll_increase=NLL_TOLERANCE,
                  lower_rare_nll_required=True),
        validation_metrics=validation_metrics,
    )
    selection_path = output / "selection.json"
    write_json(selection_path, selection)
    write_json(output / "validation_metrics.json", validation)
    print(f"validation selection: {selected}; gate: {bool(candidates)}", flush=True)

    variant = load_adapted(benchmark, selection_path)
    sample = []
    for query in read_jsonl(benchmark / "queries_validation.jsonl"):
        sample.append(query)
        if len(sample) == 20:
            break
    check_distributions(variant, sample)
    results = {}
    independent_pass = []
    for split in ("test", "cross_source"):
        observed, work_metrics = evaluate_models(
            benchmark, split, {BASELINE: original, selected: variant}, frequency)
        comparisons = {
            key: paired_interval(work_metrics[selected][key], work_metrics[BASELINE][key], metric)
            for key, metric in (("next", "nll"), ("hard:lift", "mrr"), ("ordinary:lift", "mrr"))
        }
        passed = qualifies(metrics(observed[BASELINE]), metrics(observed[selected]))
        independent_pass.append(passed)
        results[split] = dict(metrics=observed, paired_delta=comparisons, gate_passed=passed)
    verdict = bool(candidates) and all(independent_pass)
    report = dict(selection=selection, selection_sha256=sha256(selection_path), splits=results,
                  reliability_gate_passed=verdict,
                  eligible_for_offline_factory_replay=verdict)
    write_json(output / "test_metrics.json", report)
    print(f"independent reliability gate: {verdict}", flush=True)


if __name__ == "__main__":
    main()
