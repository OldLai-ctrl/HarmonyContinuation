"""Freeze one symbolically timed, dual-annotator RS200 passage per unseen work."""
import argparse
import re
import sys
import zipfile
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "b5"))
from b5_data import read_jsonl, sha256, stable, title_key, write_json, write_jsonl

SCALE = dict(I=0, II=2, III=4, IV=5, V=7, VI=9, VII=11)
NAMES = dict(C=0, D=2, E=4, F=5, G=7, A=9, B=11)
R = r"[b#]?(?:VII|III|VI|IV|II|V|I)"
# RS200 treats the dominant V7 specially. Other capital sevenths are major.
RECORD = re.compile(fr"({R})([a-z0-9]*)(?:/({R}))?", re.I)
SHAPES = {
    "maj": ("maj", [0, 4, 7]), "min": ("min", [0, 3, 7]),
    "maj7": ("maj", [0, 4, 7, 11]), "min7": ("min", [0, 3, 7, 10]),
    "7": ("dom", [0, 4, 7, 10]), "dim": ("dim", [0, 3, 6]),
    "dim7": ("dim", [0, 3, 6, 9]), "hdim7": ("hdim", [0, 3, 6, 10]),
    "aug": ("aug", [0, 4, 8]), "sus4": ("sus", [0, 5, 7]),
}
USED_ROCK = {"a_day_in_the_life", "all_along_the_watchtower", "bohemian_rhapsody",
             "purple_haze", "no_woman_no_cry", "life_on_mars", "god_only_knows"}


def roman(text):
    return SCALE[text.lstrip("b#").upper()] + text.startswith("#") - text.startswith("b")


def chord(token, key):
    match = RECORD.fullmatch(token)
    if not match:
        return None
    rn, suffix, applied = match.groups()
    base = rn.lstrip("b#")
    detail = "maj" if base.isupper() else "min"
    inversion = None
    for mark, inv in (("65", 1), ("43", 2), ("42", 3), ("64", 2), ("63", 1), ("6", 1)):
        if suffix.endswith(mark):
            inversion = inv
            suffix = suffix[:-len(mark)] + ("7" if mark in ("65","43","42") else "")
            break
    detail = {
        "": detail, "7": "7" if base == "V" else detail + "7", "d7": "7", "o": "dim",
        "x7": "dim7", "h7": "hdim7", "a": "aug", "s4": "sus4",
    }.get(suffix)
    if detail is None or (inversion is not None and inversion >= len(SHAPES[detail][1])):
        return None
    family, intervals = SHAPES[detail]
    root = (key + roman(rn) + (roman(applied) if applied else 0)) % 12
    return dict(root=root, quality=family, detail=detail, intervals=intervals,
                bass=None if inversion is None else (root + intervals[inversion]) % 12,
                tonic=key, mode=None, symbol=token, unit="qn", meter="4/4")


def bars(text):
    meter, key, last = "4/4", None, None
    position = 0
    measures = text.split("|")
    for index, raw in enumerate(measures):
        if index == len(measures) - 1 and not raw.strip():
            break
        tokens = []
        for token in raw.split():
            if re.fullmatch(r"\[\d+/\d+\]|\[0\]", token):
                meter = token[1:-1]
            elif re.fullmatch(r"\[[A-G][#b]?\]", token):
                note = token[1:-1]
                if tokens:
                    tokens.append("?")  # source key changes inside a bar are unsupported
                key = (NAMES[note[0]] + note.count("#") - note.count("b")) % 12
            else:
                tokens.append(token)
        events = []
        if meter == "4/4" and key is not None and len(tokens) in (0, 1, 2, 4):
            tokens = tokens or ["."]
            step = 4 / len(tokens)
            for i, token in enumerate(tokens):
                last = last if token == "." else chord(token, key)
                if last is None:
                    events = []
                    break
                events.append(dict(last, onset=position + i*step, phase=i*step, duration=step))
        else:
            last = None
        yield events
        position += 4


def event_signature(event):
    return tuple(event[k] for k in ("root","quality","detail","bass","tonic","onset","duration"))


def collapse(events):
    distinct = []
    for event in events:
        if distinct and all(distinct[-1][k] == event[k] for k in ("root", "detail", "bass", "tonic")):
            distinct[-1]["duration"] += event["duration"]
        else:
            distinct.append(dict(event))
    return distinct


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("sources", "b5", "a2", "output"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    import csv
    with args.a2.open(encoding="utf-8-sig", newline="") as stream:
        a2 = list(csv.DictReader(stream))
    outside = {title_key(r["title"]) for r in a2}
    outside |= {title_key(row["index"]["title"]) for row in read_jsonl(args.b5 / "sources/billboard_events.jsonl")}
    outside |= {title_key(row["title"]) for row in read_jsonl(args.b5 / "benchmark/cross_source.jsonl")}
    usage = Counter()
    sources = args.sources
    index = sources / "rs200.txt"
    archive = sources / "rs200_harmony_exp.zip"
    selected = []
    with zipfile.ZipFile(archive) as data:
        for line in index.read_text(encoding="utf-8-sig").splitlines():
            if not line.strip():
                continue
            slug, _, title, artist, *_ = line.split("\t")
            if slug in USED_ROCK or title_key(title) in outside:
                usage["title_or_A2_overlap"] += 1
                continue
            outside.add(title_key(title))
            names = [f"rs200_harmony_exp/{slug}_{author}.txt" for author in ("dt", "tdc")]
            try:
                analyses = [list(bars(data.read(name).decode("utf-8-sig", errors="replace"))) for name in names]
            except KeyError:
                usage["missing_paired_annotation"] += 1
                continue
            choices = []
            for i in range(2, min(map(len, analyses)) - 3):
                spans = [analysis[i-2:i+4] for analysis in analyses]
                if not all(all(span) for span in spans):
                    continue
                if [[event_signature(e) for bar in span for e in bar] for span in spans][0] != [
                        event_signature(e) for bar in spans[1] for e in bar]:
                    continue
                prefix = collapse([event for bar in spans[0][:2] for event in bar])[-4:]
                following = collapse([event for bar in spans[0][2:] for event in bar])
                if not 2 <= len(prefix) <= 4 or not 3 <= len(following) <= 12:
                    continue
                if prefix[-1]["root"] == following[0]["root"] and prefix[-1]["detail"] == following[0]["detail"]:
                    continue
                choices.append((i, prefix, following))
            if not choices:
                usage["no_agreed_eligible_4_4_window"] += 1
                continue
            i, prefix, following = min(choices, key=lambda row: stable("v9a-rock-v1", slug, row[0]))
            beginning = prefix[0]["onset"] - prefix[0]["phase"]
            prefix = [dict(event, onset=event["onset"]-beginning) for event in prefix]
            selected.append(dict(id=f"rs200:{slug}:{i}", group=f"rs200:{slug}", title=title, artist=artist,
                                 kind="agreed_4bar_window", source_bar=i, sources=names,
                                 prefix=prefix, target=following))
    args.output.mkdir(parents=True, exist_ok=True)
    path = args.output / "confirmation.jsonl"
    write_jsonl(path, selected)
    write_json(args.output / "freeze.json", dict(seed="v9a-rock-v1", independent_works=len(selected),
                                                excluded=usage, archive_sha256=sha256(archive),
                                                source_index_sha256=sha256(index), a2_sha256=sha256(args.a2),
                                                query_sha256=sha256(path)))


if __name__ == "__main__":
    main()
