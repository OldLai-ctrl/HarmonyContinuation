"""Encode authored bar plans using the existing V8 typed performance projection."""
import argparse
import csv
import importlib.util
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/b5"))
from b5_data import read_jsonl, sha256, write_json

spec = importlib.util.spec_from_file_location("v8_encoder", ROOT / "data/factory-v8/encode.py")
v8 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(v8)
v8.QUALITIES.update({"dim": (6, [0, 3, 6], []), "dim7": (7, [0, 3, 6, 9], [])})
FAMILIES = {1: "maj", 2: "min", 3: "dom", 4: "maj", 5: "min", 6: "dim", 7: "dim",
            8: "hdim", 9: "sus", 10: "sus", 11: "aug"}


def relative_performance(events, meter):
    root = events[0]["root"]
    return (meter, tuple(((event["root"] - root) % 12,
                          ((event["bass"] if event["bass"] is not None else event["root"]) - root) % 12,
                          tuple(event["intervals"]), round(event["duration"], 8)) for event in events))


def l1(events):
    root = events[0]["root"]
    result = []
    for event in events:
        symbol = ((event["root"] - root) % 12, event["quality"])
        if not result or result[-1] != symbol:
            result.append(symbol)
    return tuple(result)


def symbols_and_durations(bars, meter):
    numerator, denominator = map(int, meter.split("/"))
    span = numerator * 4 / denominator
    for bar in bars.split("|"):
        tokens = bar.split()
        if not tokens:
            raise ValueError("empty authored bar")
        durations = []
        for token in tokens:
            symbol, _, duration = token.partition("@")
            durations.append((symbol, float(duration) if duration else span / len(tokens)))
        if abs(sum(time for _, time in durations) - span) > 1e-8 or any(t <= 0 for _, t in durations):
            raise ValueError(f"incomplete bar in {bars}")
        yield from durations


def encode(row, edition="A"):
    mode, meter = row["mode"], row["meter"]
    tonic = "C" if mode == "major" else "A"
    events, full, raw, plain = [], [], [], []
    onset = 0.0
    for i, (symbol, duration) in enumerate(symbols_and_durations(row["bars"], meter)):
        match, detail, performed = v8.parse_chord(symbol, duration, i, tonic, mode)
        performed["startQN"] = onset
        onset += duration
        full.append(match)
        events.append(detail)
        raw.append(performed)
        plain.append(dict(root=detail["rootPC"], bass=performed["bass"], duration=duration,
                          intervals=sorted(detail["baseIntervals"] + detail["colorIntervals"]),
                          quality=FAMILIES[match["quality"]]))
    v8.validate_projection(row["id"], tonic, mode, full, events, raw)
    tonic_pc = v8.pitch(tonic)
    for previous, successor, event, next_event in zip(full, full[1:], plain, plain[1:]):
        if previous["quality"] == 3 and (next_event["root"] - event["root"]) % 12 == 5:
            previous["function"] = 4 if next_event["root"] == tonic_pc else 6
            if next_event["root"] != tonic_pc:
                previous["target"] = successor["degree"]
                previous["roles"] |= 16
    tags = [row["family"], f"V9{edition}-original-editorial"]
    techniques = ["SlashBass"] if any("/" in item["symbol"] for item in events) else []
    if any(item["colorIntervals"] for item in events):
        techniques.append("ExtendedColor")
    built_in = [row["intent"], f"common-{mode}"]
    encoded = dict(id=row["id"], name=row["name_zh"], nameZh=row["name_zh"], nameEn=row["name_en"],
                   aliases=row["id"], builtInTags=",".join(built_in), techniques=",".join(techniques),
                   complexityLevel="rich", description="Original editorial phrase. " + row["ending"],
                   note=f"Independent authored notation. Source references in the V9-{edition} manifest.",
                   mode=mode, sequence=" ".join(v8.sequence_token(item) for item in full),
                   rhythm=" ".join(str(item["durationQN"]) for item in full), cadence="none",
                   intent=row["intent"], loopable=row["intent"] == "loop", styles="pop:0.5,rock:0.3",
                   tags=",".join(tags), skeleton=",".join(map(str, range(len(full)))), meter=meter,
                   sourceType="factory", priorWeight=0.5, complexity=0.5, version=9,
                   phraseLength=len(full), secondaryIntents=0,
                   typedData=json.dumps(dict(events=raw, key=[tonic_pc, int(mode == "minor")],
                                             full=full, tags=tags, aliases=[row["id"]],
                                             builtInTags=built_in, techniques=techniques),
                                        ensure_ascii=False, separators=(",", ":")))
    return encoded, raw, plain


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("a2", "catalog", "output"):
        parser.add_argument(f"--{name}", required=True, type=Path)
    parser.add_argument("--edition", choices=("A", "B"), default="A")
    args = parser.parse_args()
    edition = args.edition
    phrase_path = ROOT / f"data/factory-v9-{edition.lower()}/phrases.tsv"
    with args.a2.open(encoding="utf-8-sig", newline="") as stream:
        reference = {row["candidate_id"]: row for row in csv.DictReader(stream)}
    with phrase_path.open(encoding="utf8", newline="") as stream:
        phrases = list(csv.DictReader(stream, delimiter="\t"))
    catalog = list(read_jsonl(args.catalog))
    expected = 1288 if edition == "A" else 1312
    if len(catalog) != expected or len({row["source"] for row in catalog}) != expected:
        raise ValueError(f"Expected frozen {expected}-entry replay catalog")
    performances = {relative_performance(row["events"], row["meter"]): row["source"] for row in catalog}
    families = {l1(row["events"]): row["source"] for row in catalog}
    tails = {}
    for item in catalog:
        for i in range(len(item["events"]) - (5 if edition == "A" else 2)):
            tails.setdefault(l1(item["events"][i:]), item["source"])
    candidates, records, fixtures = [], [], []
    ids = set()
    for row in phrases:
        encoded, raw, performed = encode(row, edition)
        if row["id"] in ids:
            raise ValueError(f"Duplicate ID {row['id']}")
        ids.add(row["id"])
        n = int(row["context_events"])
        tail = l1(performed[n:])
        min_tail, max_tail = (6, 12) if edition == "A" else (3, 16)
        if not 2 <= n <= 4 or not min_tail <= len(tail) <= max_tail:
            raise ValueError(f"{row['id']}: invalid intended seam or tail length")
        if edition == "B":
            groups = [int(value) for value in row["group_bars"].split("+")]
            prefix_qn = sum(event["duration"] for event in performed[:n])
            numerator, denominator = map(int, row["meter"].split("/"))
            if min(groups) <= 0 or sum(groups) != len(row["bars"].split("|")) or prefix_qn != groups[0] * numerator * 4 / denominator:
                raise ValueError(f"{row['id']}: inconsistent authored grouping")
            if tail in tails:
                raise ValueError(f"{row['id']}: complete L1 tail already exists in {tails[tail]}")
            tails[tail] = row["id"]
        signature = relative_performance(performed, row["meter"])
        shape = l1(performed)
        older = performances.get(signature)
        if older is not None:
            raise ValueError(f"{row['id']}: transposed full performance duplicate of {older}")
        previous = families.get(shape)
        if row["relationship"] == "performance-variant" and not previous:
            raise ValueError(f"{row['id']}: performance variant lacks earlier L1 parent")
        if row["relationship"] != "performance-variant" and previous:
            raise ValueError(f"{row['id']}: L1 identity already exists in {previous}")
        performances[signature] = row["id"]
        families.setdefault(shape, row["id"])
        donor = []
        for name in row["references"].split(";"):
            a2 = reference[name]
            donor.append(dict(id=name, source=a2["source"], work=a2["source_id"], position=a2["position"],
                              use="harmonic relationship only, independently authored realization"))
        record = dict(id=row["id"], family=row["family"], relationship=row["relationship"],
                            same_l1_performance_parent=previous,
                            v8_same_l1_complete_tail=tails.get(l1(performed[n:])),
                            evidence=donor, rights="original editorial; no source passage, audio, or melody carried",
                            mode=row["mode"], meter=row["meter"], boundary=n,
                            ending=row["ending"], authored_qn=sum(x["duration"] for x in performed),
                            target_l1_events=len(l1(performed[n:])),
                            performance_sha256=v8.fingerprint(signature))
        if edition == "B":
            record.pop("v8_same_l1_complete_tail")
            record.update(group_bars=groups, source_relation="contiguous original editorial plan",
                          development_tail_sha256=v8.fingerprint(tail))
        records.append(record)
        fixture = []
        for plain, item in zip(performed, raw, strict=True):
            base, _, bass = item["name"].partition("/")
            suffix = v8.ROOT_RE.fullmatch(base)[2]
            detail = {"":"maj","m":"min","6":"maj6","m6":"min6","m7":"min7","m9":"min9",
                      "m11":"min11","m7b5":"hdim7","add9":"maj(9)"}.get(suffix, suffix)
            fixture.append(dict(plain, detail=detail, onset=item["startQN"], unit="qn", meter=row["meter"]))
        fixtures.append(dict(id=row["id"], group=row["family"], kind="authored_seam_control",
                             prefix=fixture[:n], target=fixture[n:], trace_sources=[row["id"]]))
        candidates.append(encoded)
    args.output.mkdir(parents=True, exist_ok=True)
    write_json(args.output / "candidates.json", candidates)
    write_json(args.output / "manifest.json", dict(version=9, schema=2, count=len(records), entries=records,
                                                  authored_source_sha256=sha256(phrase_path),
                                                  a2_sha256=sha256(args.a2), catalog_sha256=sha256(args.catalog)))
    from b5_data import write_jsonl
    write_jsonl(args.output / "controls.jsonl", fixtures)


if __name__ == "__main__":
    main()
