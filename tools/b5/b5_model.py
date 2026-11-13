"""Work-weighted interpolated N-grams with an explicit, finite chord alphabet."""

import gzip
import json
import math
from collections import Counter, defaultdict

FAMILIES = ("maj", "min", "dom", "hdim", "dim", "aug", "sus", "5", "1", "other")
VOCABULARY = tuple(f"{root}:{family}" for root in range(12) for family in FAMILIES)
ALPHABET_SIZE = len(VOCABULARY)
VERSION = "b5-ngram-v1"


def duration_bucket(duration):
    if duration is None:
        return "?"
    return "short" if duration < 1 else "beat" if duration < 2 else "two" if duration < 4 else "long"


def phase_bucket(item):
    phase = item.get("phase")
    if phase is None:
        return "?"
    return "down" if phase == 0 else "beat" if phase % 1 == 0 else "off"


def token(item, anchor):
    return f"{(item['root'] - anchor) % 12}:{item['quality']}"


def anchor_for(history, representation):
    if representation == "absolute" or not history:
        return 0
    if representation == "tonic" and history[-1].get("tonic") is not None:
        return history[-1]["tonic"]
    return history[-1]["root"]


def context_key(history, order, representation, level):
    anchor = anchor_for(history, representation)
    items = history[-order:]
    parts = []
    for offset, item in enumerate(items, start=len(history) - order):
        if representation == "interval":
            local_anchor = history[offset - 1]["root"] if offset else item["root"]
        else:
            local_anchor = anchor
        part = token(item, local_anchor)
        if level in ("L2", "L3"):
            bass = item.get("bass")
            bass = "?" if bass is None else (bass - item["root"]) % 12
            part += f"/{item.get('detail') or '?'}/{bass}"
        if level == "L3":
            part += f"/{duration_bucket(item.get('duration'))}/{phase_bucket(item)}"
        parts.append(part)
    if representation == "tonic" and history[-1].get("tonic") is None:
        parts.insert(0, "tonic_missing")
    return "|".join(parts)


class Model:
    def __init__(self, config, tables):
        self.config = config
        self.tables = tables

    def keys(self, history):
        order = min(self.config["order"], len(history))
        if not order:
            return []
        representation = self.config["representation"]
        if self.config["kind"] == "fixed":
            return [f"L1:{order}:{context_key(history, order, representation, 'L1')}"] if order == self.config["order"] else []
        levels = ("L1", "L2", "L3")[:int(self.config["level"][-1])]
        return [f"{level}:{n}:{context_key(history, n, representation, level)}"
                for n in range(1, order + 1) for level in levels]

    def anchor(self, history):
        return anchor_for(history, self.config["representation"])

    @classmethod
    def fit(cls, config, work_runs):
        counts = defaultdict(Counter)
        supports = Counter()
        model = cls(config, {})
        for runs in work_runs:
            # At most one vote per work group for each context/outcome pair.
            observed = defaultdict(set)
            for run in runs:
                for i, item in enumerate(run):
                    history = run[max(0, i - config["order"] - 1):i]
                    outcome = token(item, model.anchor(history))
                    # Absolute unigram is counted only for absolute models. The
                    # relational unigram uses transitions with an actual predecessor.
                    if history or config["representation"] == "absolute" or config["order"] == 0:
                        observed[""].add(outcome)
                    for key in model.keys(history):
                        observed[key].add(outcome)
            for key, outcomes in observed.items():
                supports[key] += 1
                counts[key].update(sorted(outcomes))
        model.tables = {key: [supports[key], sum(count.values()), dict(count)]
                        for key, count in counts.items()}
        return model

    def contexts(self, history):
        supported = []
        for key in self.keys(history):
            row = self.tables.get(key)
            if row and row[0] >= 2:
                supported.append((key, row))
        return supported

    def probability(self, history, outcome, contexts=None):
        support, total, counts = self.tables[""]
        probability = (counts.get(outcome, 0) + 0.5) / (total + 0.5 * ALPHABET_SIZE)
        if contexts is None:
            contexts = self.contexts(history)
        for _, (support, total, counts) in contexts:
            # Dirichlet interpolation, with the pseudo-count increasing for
            # sparse contexts whose counts are concentrated in few works.
            prior_mass = self.config["strength"] * total / support
            probability = (counts.get(outcome, 0) + prior_mass * probability) / (total + prior_mass)
        return probability

    def next_event(self, history, item):
        contexts = self.contexts(history)
        outcome = token(item, self.anchor(history))
        probability = self.probability(history, outcome, contexts)
        # Half credit for ties avoids arbitrary lexical tie breaks.
        distribution = [self.probability(history, symbol, contexts) for symbol in VOCABULARY]
        better = sum(value > probability for value in distribution)
        tied = sum(value == probability for value in distribution)
        key, row = contexts[-1] if contexts else ("L1:0:", self.tables[""])
        support = row[0] if contexts else 0
        return probability, better, tied, int(key.split(":", 2)[1]), support

    def sequence_log(self, history, events):
        context = list(history[-self.config["order"] - 1:])
        logp = 0.0
        for item in events:
            outcome = token(item, self.anchor(context))
            logp += math.log(self.probability(context, outcome))
            context.append(item)
            if len(context) > self.config["order"] + 1:
                context.pop(0)
        return logp

    def phrase_scores(self, prefix, events):
        if not events:
            return {"total": -math.inf, "mean": -math.inf, "lift": -math.inf, "support": 0}
        contexts = self.contexts(prefix)
        support = contexts[-1][1][0] if contexts else 0
        total = self.sequence_log(prefix, events)
        if len(events) > 1:
            # Compare the within-phrase transitions with and without the input
            # context. At the seam use the same outcome's unigram prior.
            first = token(events[0], self.anchor(prefix))
            unconditional = math.log(self.probability((), first)) + self.sequence_log(events[:1], events[1:])
        else:
            unconditional = math.log(self.probability((), token(events[0], self.anchor(prefix))))
        return dict(total=total, mean=total / len(events),
                    lift=(total - unconditional) / len(events), support=support)

    def save(self, path):
        payload = json.dumps(dict(version=VERSION, config=self.config, tables=self.tables),
                             sort_keys=True, separators=(",", ":")).encode()
        path.write_bytes(gzip.compress(payload, compresslevel=6, mtime=0))

    @classmethod
    def load(cls, path):
        data = json.loads(gzip.decompress(path.read_bytes()))
        if data["version"] != VERSION:
            raise ValueError(f"Unsupported model format {data['version']}")
        return cls(data["config"], data["tables"])
