"""Normalize the G13 Billboard extraction and the official MPB event table."""

import csv
import hashlib
import json
import re
import unicodedata
from collections import Counter, defaultdict

SEED = "b5-v1-20261113"
SOURCE_HASHES = {
    "billboard_events.jsonl": "a6a15baf155aaf454392c702334eff01f5f67ef66a1fac61141a7ab57f7347a2",
    "mpb-v1.1-harmony.csv": "ab3ffdf8aeba3302ed1093a3dfb62a579bfbcbfbf56c3c306f06e1d826a69620",
}


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def stable(*parts):
    return hashlib.sha256("|".join(map(str, (SEED, *parts))).encode()).hexdigest()


def read_jsonl(path):
    with path.open(encoding="utf-8-sig") as source:
        for line in source:
            yield json.loads(line)


def write_jsonl(path, rows):
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        for row in rows:
            stream.write(json.dumps(row, ensure_ascii=False, sort_keys=True) + "\n")


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n",
                    encoding="utf-8", newline="\n")


def title_key(title):
    title = unicodedata.normalize("NFKD", title.casefold())
    return re.sub("[^a-z0-9]", "", title)


def quality_detail(detail):
    base, _, alterations = detail.partition("(")
    base, extensions = {
        "maj9": ("maj7", ["9"]), "maj13": ("maj7", ["9", "13"]),
        "min9": ("min7", ["9"]), "min11": ("min7", ["9", "11"]),
        "min13": ("min7", ["9", "11", "13"]), "9": ("7", ["9"]),
        "11": ("7", ["9", "11"]), "13": ("7", ["9", "11", "13"]),
    }.get(base, (base, []))
    if alterations:
        extensions += re.split("[,.]", alterations.rstrip(")"))
    return base + ("(" + ",".join(sorted(set(extensions))) + ")" if extensions else "")


def mpb_quality(row):
    code = row["chord_type"][:1]
    symbol = row["chord_symbol"].removeprefix("*")
    family = dict(zip("VZYXWvzwxy", ("maj", "maj", "dom", "dom", "dom",
                                    "min", "min", "min", "dim", "hdim"))).get(code, "other")
    if symbol.startswith("4"):
        family, base = "sus", "sus4"
        if ".7" in symbol:
            base = "sus4(b7)"
    elif symbol == "(#5)":
        family, base = "aug", "aug"
    elif symbol == "m(b5)":
        family, base = "dim", "dim"
    else:
        base = {"maj": "maj7" if "M7" in symbol else "maj6" if symbol.startswith("6") else "maj",
                "min": "minmaj7" if "mM7" in symbol else "min7" if symbol.startswith("m7")
                else "min6" if symbol.startswith("m6") else "min",
                "dom": "7", "dim": "dim7", "hdim": "hdim7", "other": "other"}[family]
    extras = re.findall(r"[b#]\d+|(?:9|11|13)(?!\d)", symbol)
    base = base.replace("(b7)", "")
    if family == "sus" and ".7" in symbol:
        extras.append("b7")
    return family, base + ("(" + ",".join(sorted(set(extras))) + ")" if extras else "")


def event(root, quality, detail, bass, tonic, mode, symbol, **source_fields):
    result = dict(root=root, quality=quality, detail=detail, bass=bass, tonic=tonic, mode=mode,
                  symbol=symbol, unit=None, duration=None, onset=None, meter=None, phase=None,
                  confidence=None)
    result.update(source_fields)
    result["missing"] = [field for field in
                         ("root", "bass", "tonic", "mode", "duration", "onset", "meter",
                          "phase", "confidence") if result[field] is None]
    return result


def billboard(path):
    for raw in read_jsonl(path):
        sections = []
        stream = []
        position = 0.0
        for phrase in raw["phrases"]:
            if phrase["break_before"] or phrase["elided"]:
                stream.append(None)
            bar_position = 0.0
            previous_bar = None
            line_events = []
            for annotation in phrase["events"]:
                bar = (annotation["repeat_index"], annotation["bar_index"])
                if bar != previous_bar:
                    bar_position = 0.0
                    previous_bar = bar
                duration = annotation["qn"]
                meter = annotation["metre"]
                known = duration is not None and meter is not None
                # The grid follows the source's symbolic bar slots, independently of its audio timestamps.
                item = event(annotation.get("root_pc"), annotation.get("family"),
                             quality_detail(annotation.get("quality", "")),
                             annotation.get("bass_pc") if annotation.get("bass_source") == "explicit_slash_label" else None,
                             annotation.get("tonic"), None, annotation["symbol"],
                             unit="qn" if known else None, duration=duration, onset=position if known else None,
                             meter=meter, phase=bar_position if known else None,
                             line=phrase["line"], section=phrase["section"], repeat=bar[0], bar=bar[1],
                             annotation_seconds=phrase["timestamp_seconds"], time_basis="symbolic_bar_slots",
                             raw_quality=annotation.get("quality"), bass_source=annotation.get("bass_source"),
                             intervals=harte_intervals(annotation.get("quality", "")))
                line_events.append(item)
                stream.append(item)
                if known:
                    if position is not None:
                        position += duration
                    bar_position += duration
                else:
                    position = None
            sections.append((phrase["line"], len(stream) - len(line_events), len(stream)))
            if phrase["elided"]:
                stream.append(None)
        yield dict(id=f"bb:{raw['id']}", source="billboard", title=raw["index"]["title"],
                   artist=raw["index"]["artist"], slots=raw["slots"], stream=stream, sections=sections)


def mpb(path):
    works = defaultdict(list)
    with path.open(encoding="utf-8-sig", newline="") as source:
        for row in csv.DictReader(source):
            works[(row["corpus_id"], row["composition_id"], row["composition_name"])].append(row)
    for (composer, id_, title), rows in works.items():
        rows.sort(key=lambda row: int(row["chord_index"]))
        assert [int(row["chord_index"]) for row in rows] == list(range(1, len(rows) + 1))
        items = []
        for row in rows:
            family, detail = mpb_quality(row)
            items.append(event(int(row["root"]), family, detail, int(row["bass"]), int(row["key"]),
                               row["mode"], row["chord_symbol"], measure_position=float(row["position"]),
                               annotation_index=int(row["chord_index"]), raw_quality=row["chord_type"],
                               bass_source="annotated_pc", intervals=harte_intervals(detail)))
        yield dict(id=f"mpb:{composer}:{id_}", source="mpb", title=title, artist=composer,
                   stream=items, sections=[])


def identity(item, level="L2"):
    if level == "L1":
        return item["root"], item["quality"]
    return item["root"], item["quality"], item["detail"], item["bass"]


def harte_intervals(detail):
    """Interpret explicit chord degrees only, independent of voicing and bass."""
    base, _, suffix = detail.partition("(")
    shorthand = {
        "1": "1", "5": "1 5", "maj": "1 3 5", "min": "1 b3 5",
        "maj6": "1 3 5 6", "min6": "1 b3 5 6",
        "maj7": "1 3 5 7", "min7": "1 b3 5 b7", "7": "1 3 5 b7",
        "minmaj7": "1 b3 5 7", "dim": "1 b3 b5", "dim7": "1 b3 b5 bb7",
        "hdim7": "1 b3 b5 b7", "aug": "1 3 #5",
        "sus2": "1 2 5", "sus4": "1 4 5",
        "maj9": "1 3 5 7 9", "maj13": "1 3 5 7 9 13",
        "min9": "1 b3 5 b7 9", "min11": "1 b3 5 b7 9 11",
        "min13": "1 b3 5 b7 9 11 13",
        "9": "1 3 5 b7 9", "11": "1 3 5 b7 9 11", "13": "1 3 5 b7 9 11 13",
    }
    if base not in shorthand and base:
        return None
    degrees = shorthand.get(base, "").split()
    for addition in suffix.rstrip(")").split(","):
        if not addition:
            continue
        if addition.startswith("*"):
            degree = addition[1:]
            degrees = [existing for existing in degrees if existing != degree]
        else:
            degrees.append(addition)
    result = set()
    for degree in degrees:
        match = re.fullmatch(r"([b#]*)([1-9]\d*)", degree)
        if not match:
            return None
        alteration, number = match.groups()
        result.add(((0, 2, 4, 5, 7, 9, 11)[(int(number) - 1) % 7]
                    + alteration.count("#") - alteration.count("b")) % 12)
    return sorted(result) or None


def runs(stream, level="L2"):
    current = []
    for item in stream:
        if item is None or item["root"] is None:
            if current:
                yield current
                current = []
            continue
        if (current and identity(current[-1], level) == identity(item, level)
                and all(current[-1][field] == item[field] for field in ("tonic", "mode", "meter"))):
            previous = current[-1]
            previous["end_line"] = item.get("line")
            if previous["duration"] is not None and item["duration"] is not None:
                previous["duration"] += item["duration"]
            else:
                previous["duration"] = None
                previous["missing"] = sorted(set(previous["missing"]) | {"duration"})
        else:
            current.append(dict(item, end_line=item.get("line")))
    if current:
        yield current


def fingerprints(work):
    signatures = []
    for run in runs(work["stream"], "L1"):
        # Retain musical changes and ignore absolute transposition.
        reduced = [(0, run[0]["quality"])]
        for item in run[1:]:
            token = ((item["root"] - run[0]["root"]) % 12, item["quality"])
            if token != reduced[-1]:
                reduced.append(token)
        if len(reduced) >= 12:
            signatures.append(tuple(reduced))
    return signatures


def partition(works):
    parent = {work["id"]: work["id"] for work in works}

    def find(key):
        while parent[key] != key:
            key = parent[key]
        return key

    key_owner = {}
    reasons = Counter()
    for work in works:
        title = title_key(work["title"])
        keys = [("title", title)] if title else []
        keys += [("sequence", pattern) for pattern in fingerprints(work)]
        for key in keys:
            left, right = find(work["id"]), find(key_owner.setdefault(key, work["id"]))
            if left != right:
                parent[max(left, right)] = min(left, right)
                reasons[key[0]] += 1
    groups = defaultdict(list)
    for work in works:
        work["group"] = find(work["id"])
        groups[work["group"]].append(work)
    # Assign Billboard groups in deterministic hash order. External MPB is never fitted or tuned.
    primary = sorted((group for group, members in groups.items()
                      if any(work["source"] == "billboard" for work in members)), key=stable)
    boundaries = int(len(primary) * 0.70), int(len(primary) * 0.85)
    assignments = {}
    for index, group in enumerate(primary):
        assignments[group] = ("train" if index < boundaries[0] else
                              "validation" if index < boundaries[1] else "test")
    for work in works:
        group = work["group"]
        if work["source"] == "billboard":
            work["split"] = assignments[group]
        else:
            work["split"] = "cross_source" if group not in assignments else "overlap_excluded"
    assert len({(work["group"], work["split"]) for work in works if work["source"] == "billboard"}) == len(primary)
    return reasons


def stats(works):
    result = {}
    for split in sorted({work["split"] for work in works}):
        selected = [work for work in works if work["split"] == split]
        events = [item for work in selected for item in work["stream"] if item is not None]
        valid = [item for item in events if item["root"] is not None]
        result[split] = dict(works=len(selected), groups=len({work["group"] for work in selected}),
                             raw_events=len(events), L1=len(valid), L2=len(valid),
                             explicit_bass=sum(item["bass"] is not None for item in valid),
                             L3=sum(item["duration"] is not None and item["phase"] is not None for item in valid),
                             known_tonic=sum(item["tonic"] is not None for item in valid),
                             known_mode=sum(item["mode"] is not None for item in valid),
                             L1_changes=sum(len(run) for work in selected for run in runs(work["stream"], "L1")),
                             L2_changes=sum(len(run) for work in selected for run in runs(work["stream"])))
    return result


def read_sources(directory):
    for name, expected in SOURCE_HASHES.items():
        actual = sha256(directory / name)
        if actual != expected:
            raise ValueError(f"{name}: expected SHA-256 {expected}, got {actual}")
    works = list(billboard(directory / "billboard_events.jsonl"))
    works.extend(mpb(directory / "mpb-v1.1-harmony.csv"))
    return works
