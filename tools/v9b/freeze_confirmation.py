"""Freeze new title-disjoint metrical confirmation windows for Factory V9-B."""

import argparse
import csv
import re
import sqlite3
import sys
import zipfile
from collections import Counter, defaultdict
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(TOOLS / "v9a"), str(TOOLS / "b5")]
from b5_data import read_jsonl, sha256, stable, title_key, write_json, write_jsonl
from freeze_rock import bars as rock_bars, collapse

SEED = "v9b-work-confirmation-v1"
NOTES = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
PITCH = r"[A-G][#b]?"
CHORD = re.compile(fr"({PITCH})([^/]*)(?:/({PITCH}))?")
KEY = re.compile(fr"({PITCH})-([a-z]+)")
EXTENSIONS = {"9": 2, "11": 5, "13": 9}
BASE = {
    "maj": ("maj", "maj", [0, 4, 7]),
    "maj6": ("maj", "maj6", [0, 4, 7, 9]),
    "maj7": ("maj", "maj7", [0, 4, 7, 11]),
    "min": ("min", "min", [0, 3, 7]),
    "min6": ("min", "min6", [0, 3, 7, 9]),
    "min7": ("min", "min7", [0, 3, 7, 10]),
    "minmaj7": ("min", "minmaj7", [0, 3, 7, 11]),
    "dom": ("dom", "7", [0, 4, 7, 10]),
    "dim": ("dim", "dim", [0, 3, 6]),
    "dim7": ("dim", "dim7", [0, 3, 6, 9]),
    "hdim7": ("hdim", "hdim7", [0, 3, 6, 10]),
    "aug": ("aug", "aug", [0, 4, 8]),
    "aug7": ("aug", "aug(b7)", [0, 4, 8, 10]),
    "sus": ("sus", "sus4", [0, 5, 7]),
    "sus7": ("sus", "sus4(b7)", [0, 5, 7, 10]),
}


def pitch(note):
    return (NOTES[note[0]] + note.count("#") - note.count("b")) % 12


def window_fingerprint(prefix, target):
    events = prefix + target
    if len(target) < 6 or len(events) < 8:
        return None
    return (len(prefix), tuple(((event["root"] - events[0]["root"]) % 12, event["quality"],
                                event["duration"]) for event in events))


def query_windows(bars, prefix_bars):
    """Full contiguous bars, with a harmonic change at each split boundary."""
    for split in range(prefix_bars, len(bars) - 3):
        span = bars[split - prefix_bars:split + 4]
        if not all(span):
            continue
        if len({event.get("chorus") for bar in span for event in bar}) != 1:
            continue
        prefix = collapse([event for bar in span[:prefix_bars] for event in bar])
        target = collapse([event for bar in span[prefix_bars:] for event in bar])
        if not 2 <= len(prefix) <= 6 or not 3 <= len(target) <= 12:
            continue
        if (prefix[-1]["root"], prefix[-1]["detail"]) == (target[0]["root"], target[0]["detail"]):
            continue
        events = prefix + target
        if not all(b["onset"] == a["onset"] + a["duration"] for a, b in zip(events, events[1:])):
            continue
        start = prefix[0]["onset"]
        yield split, [dict(event, onset=event["onset"]-start) for event in prefix], [
            dict(event, onset=event["onset"]-start) for event in target]


def rock_leftovers(archive, index, excluded_titles, excluded_fingerprints, counts):
    """Use separate original analyst charts for works V9-A did not replay."""
    with zipfile.ZipFile(archive) as data:
        for line in index.read_text(encoding="utf-8-sig").splitlines():
            if not line.strip():
                continue
            slug, _, title, artist, *_ = line.split("\t")
            if title_key(title) in excluded_titles:
                counts["rock_title_overlap"] += 1
                continue
            choices = []
            for author in ("dt", "tdc"):
                member = f"rs200_harmony_exp/{slug}_{author}.txt"
                try:
                    raw = data.read(member).decode("utf-8-sig")
                except (KeyError, UnicodeDecodeError):
                    continue
                bars = list(rock_bars(raw))
                for cut, prefix, target in query_windows(bars, 3):
                    fingerprint = window_fingerprint(prefix, target)
                    if fingerprint and fingerprint in excluded_fingerprints:
                        continue
                    choices.append((stable(SEED, "rock", slug, author, cut), author, member, cut, prefix, target))
            if not choices:
                counts["rock_no_eligible_window"] += 1
                continue
            _, author, member, cut, prefix, target = min(choices)
            fingerprint = window_fingerprint(prefix, target)
            if fingerprint:
                excluded_fingerprints.add(fingerprint)
            excluded_titles.add(title_key(title))
            yield dict(id=f"rs200:{slug}:{cut}", group=f"rs200:{slug}", title=title, artist=artist,
                       source="RS200", kind="independent_rock_window", author=author, sources=[member],
                       source_bar=cut, prefix=prefix, target=target)


def jazz_chord(symbol):
    match = CHORD.fullmatch(symbol)
    if not match:
        return None
    note, suffix, slash = match.groups()
    suffix = {"maj7": "j7", "7#9": "79#", "7sus4": "sus7", "m7b5": "h7"}.get(suffix, suffix)
    for names, base in (
        (("h7",), "hdim7"), (("-j7", "mj7", "minj7"), "minmaj7"),
        (("-7", "m7", "min7"), "min7"), (("-6", "m6", "min6"), "min6"),
        (("o7", "dim7"), "dim7"), (("+7", "aug7"), "aug7"),
        (("sus7",), "sus7"), (("sus4", "sus"), "sus"), (("j7",), "maj7"),
        (("dim", "o"), "dim"), (("aug", "+"), "aug"), (("-", "min", "m"), "min"),
        (("6",), "maj6"), (("7",), "dom"), (("maj", ""), "maj"),
    ):
        prefix = next((name for name in names if suffix.startswith(name)), None)
        if prefix is not None:
            suffix = suffix[len(prefix):]
            break
    quality, detail, core = BASE[base]
    intervals = set(core)
    alterations = []
    if suffix == "b5" and base == "dom":
        intervals.remove(7)
        intervals.add(6)
        alterations.append("b5")
        suffix = ""
    while suffix:
        tension = re.match(r"(9|11|13)([#b]?)", suffix)
        if not tension:
            return None
        degree, accidental = tension.groups()
        intervals.add((EXTENSIONS[degree] + accidental.count("#") - accidental.count("b")) % 12)
        alterations.append(accidental + degree)
        suffix = suffix[tension.end():]
    if alterations:
        shorthand, _, extension = detail.partition("(")
        if extension:
            alterations.insert(0, extension.rstrip(")"))
        detail = shorthand + "(" + ",".join(alterations) + ")"
    return dict(root=pitch(note), bass=pitch(slash) if slash else None, quality=quality,
                detail=detail, intervals=sorted(intervals), symbol=symbol, unit="qn", meter="4/4")


def jazz_bars(beat_rows, tonal_center):
    """Read marked changes and verify all four beats of every retained bar."""
    meter, chord, form = None, None, None
    key = KEY.fullmatch(tonal_center or "")
    tonic = pitch(key[1]) if key else None
    mode = {"maj": "major", "min": "minor"}.get(key[2]) if key else None
    slices = defaultdict(list)
    for bar, beat, signature, label, form_change, chorus in beat_rows:
        meter = signature or meter
        # Blank chord annotations extend the previous label. NC and unknown
        # labels interrupt usable harmony until a new explicit annotation.
        if label:
            chord = jazz_chord(label)
        form = form_change or form
        event = dict(chord, tonic=tonic, mode=mode, onset=4*bar+beat-1, phase=beat-1, duration=1,
                     annotated_form=form, chorus=chorus, bar=bar
                     ) if chord is not None and meter == "4/4" and chorus else None
        slices[bar].append((beat, event))
    if not slices:
        return []
    bars = []
    for bar in range(min(slices), max(slices) + 1):
        values = slices.get(bar, [])
        if [beat for beat, _ in values] != [1, 2, 3, 4] or not all(event for _, event in values):
            bars.append([])
        else:
            bars.append([event for _, event in values])
    return bars


def weimar_jazz(database, excluded_titles, excluded_fingerprints, counts):
    # Add only one window per underlying composition, even with multiple soloists.
    grouped = defaultdict(list)
    with sqlite3.connect(database.resolve().as_uri() + "?immutable=1", uri=True) as db:
        info = db.execute("SELECT name, major, minor, release FROM db_info").fetchone()
        if info != ("Weimar Jazz Database (WJazzD)", 2, 2, "2.1"):
            raise ValueError(f"Unexpected WJazzD source version {info}")
        works = db.execute(
            "SELECT s.melid, s.compid, c.title, s.title, s.performer, s.key, s.style, c.tonalitytype "
            "FROM solo_info s JOIN composition_info c ON s.compid=c.compid ORDER BY s.compid,s.melid"
        )
        all_beats = defaultdict(list)
        for melid, *beat in db.execute(
                "SELECT melid, bar, beat, signature, chord, form, chorus_id FROM beats ORDER BY melid,onset"):
            all_beats[melid].append(beat)
        for melid, compid, title, alias, artist, key, style, tonality in works:
            title_id = title_key(title)
            if not title_id or title_id in excluded_titles or title_key(alias) in excluded_titles:
                counts["jazz_solo_title_overlap_or_missing"] += 1
                continue
            grouped[title_id].append((melid, compid, title, artist, key, style, tonality))
    choices = []
    for title_id, solos in grouped.items():
        possible = []
        for melid, compid, title, artist, key, style, tonality in solos:
            for _, prefix, target in query_windows(jazz_bars(all_beats[melid], key), 3):
                fingerprint = window_fingerprint(prefix, target)
                if fingerprint and fingerprint in excluded_fingerprints:
                    continue
                cut = target[0]["bar"]
                possible.append((stable(SEED, title_id, melid, cut), melid, compid, title, artist,
                                 style, tonality, cut, prefix, target, fingerprint))
        if possible:
            choices.append(min(possible))
        else:
            counts["jazz_works_no_eligible_window"] += 1
    selected = []
    for _, melid, compid, title, artist, style, tonality, cut, prefix, target, fingerprint in sorted(choices):
        if fingerprint and fingerprint in excluded_fingerprints:
            counts["jazz_duplicate_window"] += 1
            continue
        if fingerprint:
            excluded_fingerprints.add(fingerprint)
        selected.append(dict(id=f"wjazzd:{compid}:{melid}:{cut}", group=f"wjazzd:{compid}", title=title,
                             artist=artist, source="WJazzD", source_style=style, source_tonality=tonality,
                             kind="independent_jazz_window", source_bar=cut, prefix=prefix, target=target))
    counts["jazz_eligible_works_before_limit"] = len(selected)
    return selected[:72]


def exclusions(args, rock_index):
    a2 = args.v9a / "sources/a2-candidate-v8-comparison.csv"
    with a2.open(encoding="utf-8-sig", newline="") as fp:
        titles = {title_key(row["title"]) for row in csv.DictReader(fp)}
    billboard = args.b5 / "sources/billboard_events.jsonl"
    mpb = args.b5 / "benchmark/cross_source.jsonl"
    previous = args.v9a / "confirmation/confirmation.jsonl"
    titles.update(title_key(r["index"]["title"]) for r in read_jsonl(billboard))
    titles.update(title_key(r["title"]) for r in read_jsonl(mpb))
    # Include A2 Rock reserves checked before the V9-A split.
    for line in rock_index.read_text(encoding="utf-8-sig").splitlines():
        fields = line.split("\t")
        if fields[0] in ("life_on_mars", "god_only_knows"):
            titles.add(title_key(fields[2]))
    fingerprints = set()
    for row in read_jsonl(previous):
        titles.add(title_key(row["title"]))
        fingerprint = window_fingerprint(row["prefix"], row["target"])
        if fingerprint:
            fingerprints.add(fingerprint)
    return titles, fingerprints, {p.name: sha256(p) for p in (a2, billboard, mpb, previous)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("v9a", "b5", "output"):
        parser.add_argument(f"--{option}", required=True, type=Path)
    parser.add_argument("--wjazzd", type=Path)
    args = parser.parse_args()
    index = args.v9a / "sources/rs200.txt"
    archive = args.v9a / "sources/rs200_harmony_exp.zip"
    titles, fingerprints, input_hashes = exclusions(args, index)
    counts = Counter()
    rock = list(rock_leftovers(archive, index, titles, fingerprints, counts))
    output = args.output
    output.mkdir(parents=True, exist_ok=True)
    write_jsonl(output / "rock.jsonl", rock)
    write_json(output / "rock-freeze.json", dict(seed=SEED, works=len(rock), exclusions=counts,
                                                  sha256=sha256(output / "rock.jsonl"),
                                                  inputs=input_hashes | {p.name: sha256(p) for p in (index, archive)},
                                                  license="CC BY 4.0, Trevor de Clercq and David Temperley"))
    if args.wjazzd:
        for line in index.read_text(encoding="utf-8-sig").splitlines():
            if line.strip():
                titles.add(title_key(line.split("\t")[2]))
        jazz_counts = Counter()
        jazz = weimar_jazz(args.wjazzd, titles, fingerprints, jazz_counts)
        write_jsonl(output / "jazz.jsonl", jazz)
        write_json(output / "jazz-freeze.json", dict(
            seed=SEED, works=len(jazz), exclusions=jazz_counts, sha256=sha256(output / "jazz.jsonl"),
            inputs=input_hashes | {p.name: sha256(p) for p in (index, archive, args.wjazzd)},
            license="ODbL 1.0, The Jazzomat Research Project, v2.1 (DB 2.2); evaluation only",
            attribution="https://jazzomat.hfm-weimar.de/download/download.html"))


if __name__ == "__main__":
    main()
