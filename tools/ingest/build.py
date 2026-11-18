"""Build Factory candidates from an annotated corpus.

Usage:
    python tools/ingest/build.py rs200 --sources DIR --out DIR

Outputs ``phrases.tsv`` (one row per ingested phrase, human readable) and
``candidates.json`` (the format ``factory_v6_compiler`` reads).

Nothing here classifies a phrase musically. Boundaries come from the corpus,
from an arrival on the tonic, or from an 8-bar grid; the same bar line is never
stored as a label. The only per-entry judgement is the 13-category style
affinity, which the engine scores with.
"""

from __future__ import annotations

import argparse
import csv
import importlib.util
import json
import re
import sys
import zipfile
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/b5"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

import billboard as BB  # noqa: E402
import boundaries as BD  # noqa: E402
import chords as C  # noqa: E402
import naming  # noqa: E402

_spec = importlib.util.spec_from_file_location("v8_encoder", ROOT / "data/factory-v8/encode.py")
v8 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(v8)
v8.QUALITIES.update({
    "dim": (6, [0, 3, 6], []),
    "dim7": (7, [0, 3, 6, 9], []),
    "11": (3, [0, 4, 7, 10], [2, 5]),
    "13": (3, [0, 4, 7, 10], [2, 5, 9]),
    "sus2": (9, [0, 2, 7], []),
    "aug": (11, [0, 4, 8], []),
    "madd9": (2, [0, 3, 7], [2]),
    "maj11": (4, [0, 4, 7, 11], [2, 5]),
    "maj13": (4, [0, 4, 7, 11], [2, 5, 9]),
    "m13": (5, [0, 3, 7, 10], [2, 5, 9]),
})
FAMILIES = {1: "maj", 2: "min", 3: "dom", 4: "maj", 5: "min", 6: "dim",
            7: "dim", 8: "hdim", 9: "sus", 10: "sus", 11: "aug"}
SCALE = {"major": [0, 2, 4, 5, 7, 9, 11], "minor": [0, 2, 3, 5, 7, 8, 10]}
BAR_QN = {"4/4": 4.0, "3/4": 3.0, "2/4": 2.0, "6/8": 3.0, "12/8": 6.0,
          "5/4": 5.0, "7/4": 7.0}

MIN_CHORDS, MAX_CHORDS, GRID_BARS = 4, 20, 8

# A phrase that spends two thirds of its sounding time on the tonic and the
# dominant carries no progression to develop (see ``tonic_dominant_share``).
VAMP_SHARE = 0.66

# ``render_bars`` re-spells every phrase against C major / A minor, so labels
# and boundary evidence have to be read in that same key.
TONIC_OF = {"major": 0, "minor": 9}

# Phrase-unit splitting (I04). Tuned offline on a per-song split of the corpus:
# 4-bar hypermetric alignment + a hard 4-bar minimum keeps 90% of cuts on
# hypermetric lines and turns 3,124 whole sections into ~8,000 phrase units.
SPLIT_WEIGHTS = dict(BD.DEFAULT_WEIGHTS, cadence=0.75, repeat=0.30, restart=0.10)
# ``SPLIT_LOW`` is the floor in chord *changes*, not bars: the corpus carries no
# tempo, so a bar is not a comparable unit of musical length across songs.
# ``SPLIT_HIGH``/``SPLIT_AMOUNT_HIGH`` stay bar/chord ceilings -- an entry that
# spans half a section is not a phrase no matter how much it holds.
SPLIT_LAM, SPLIT_LOW, SPLIT_HIGH, SPLIT_TARGET, SPLIT_METRE_BONUS = 0.35, 4, 8, 6, 0.35
SPLIT_AMOUNT_HIGH = 10


# ---------------------------------------------------------------- corpus

def load_rs200(sources: Path) -> list[dict]:
    """de Clercq & Temperley rock corpus, ``_dt`` roman-numeral annotations."""
    archive = sources / "rs200_harmony_exp.zip"
    index = sources / "rs200.txt"
    meta = {}
    for line in index.read_text(encoding="utf-8", errors="replace").splitlines():
        parts = line.rstrip("\n").split("\t")
        if len(parts) >= 3:
            meta[parts[0]] = {"slug": parts[0], "title": parts[2].strip(),
                              "artist": parts[3].strip() if len(parts) > 3 else ""}
    songs = []
    with zipfile.ZipFile(archive) as bundle:
        for name in sorted(bundle.namelist()):
            if "MACOSX" in name or not name.endswith("_dt.txt"):
                continue
            slug = Path(name).name[: -len("_dt.txt")]
            text = bundle.read(name).decode("utf-8", errors="replace")
            match = re.search(r"\[([^\]]+)\]", text)
            if not match:
                continue
            key_pc, mode = C.parse_key(match.group(1))
            body = " ".join(line for line in text[match.end():].splitlines()
                            if not line.lstrip().lower().startswith("warning"))
            bars = [bar.split() for bar in body.split("|")]
            info = dict(meta.get(slug, {"slug": slug, "title": slug, "artist": ""}))
            info.update(key=key_pc, mode=mode, meter="4/4", bars=bars,
                        styles={"rock": 0.9})
            songs.append(info)
    return songs


def load_billboard(sources: Path) -> list[dict]:
    """McGill Billboard: SALAMI already marks the sections, so nothing is inferred.

    A section marker (``A, chorus,``) starts a new phrase; the rest of the file
    is read with the same bar semantics as the rock corpus.
    """
    root = sources / "bb" / "McGill-Billboard"
    skipped_meter = {}
    songs = []
    for path in sorted(root.glob("*/salami_chords.txt")):
        parsed = BB.parse_song(path)
        if parsed is None:
            continue
        key, mode = _billboard_key(parsed)
        # The library stores a single-digit numerator over 4 or 8, so 12/8 has
        # no bar length it can express; those songs are skipped, not rewritten.
        if not re.fullmatch(r"[1-9]/(4|8)", parsed["meter"]):
            skipped_meter[parsed["meter"]] = skipped_meter.get(parsed["meter"], 0) + 1
            continue
        meter = parsed["meter"]
        sections = []
        for label, bars in parsed["sections"]:
            rows = [BB.chord_tokens(bar) for bar in bars]
            if not rows or any(not row for row in rows):
                continue
            sections.append((label, rows))
        if not sections:
            continue
        songs.append(dict(slug=parsed["slug"], title=parsed["title"],
                          artist=parsed["artist"], key=key, mode=mode, meter=meter,
                          sections=sections, styles={"pop": 0.9}))
    if skipped_meter:
        print("skipped meters:", skipped_meter)
    return songs


MINOR_QUALITIES = {"m", "m7", "m9", "m11", "m6", "mMaj7", "dim", "dim7", "hdim7"}


def _billboard_chords(song: dict) -> list[tuple[int, str]]:
    chords = []
    for _, bars in song["sections"]:
        for bar in bars:
            for token in BB.chord_tokens(bar):
                if token in (".", "R"):
                    continue
                try:
                    root, quality, *_ = C.parse_symbol(token)
                except ValueError:
                    continue
                chords.append((root, quality))
    return chords


def _billboard_key(song: dict) -> tuple[int, str]:
    """Respect the SALAMI header and only overrule it when the harmony insists.

    The header's ``# tonic:`` is the annotator's own statement of the key, and
    measuring the corpus shows the named tonic really is a chord root in
    99.4% of songs.  Scoring every candidate root purely from chord counts
    therefore threw away better information than it added: on this corpus it
    disagreed with the header in 285 of 855 songs, and 202 of those sat exactly
    a fifth away -- the classic relative/dominant confusion.  So the header
    wins by default, and is only replaced when its own tonic barely appears as
    a chord *and* another root explains the song's harmony far better.
    """
    chords = _billboard_chords(song)
    header = C.pitch_class(song["tonic"])
    if not chords:
        return header, "major"
    counts: dict[int, int] = {}
    for root, _ in chords:
        counts[root] = counts.get(root, 0) + 1
    total = len(chords)
    last_root = chords[-1][0]

    def evidence(root: int) -> float:
        return (2.0 * counts.get(root, 0)
                + 1.5 * counts.get((root + 7) % 12, 0)
                + 1.0 * counts.get((root + 5) % 12, 0)
                + 0.5 * counts.get((root + 9) % 12, 0)
                + (2.0 if root == last_root else 0.0))

    best = max(counts, key=evidence)
    # Overrule the header only on overwhelming evidence: its tonic is almost
    # never used as a chord, and the challenger is at least three times better.
    header_share = counts.get(header, 0) / total
    if best != header and header_share < 0.05 and evidence(best) >= 3.0 * evidence(header):
        chosen = best
    else:
        chosen = header
    minor = sum(1 for root, quality in chords if root == chosen and quality in MINOR_QUALITIES)
    major = sum(1 for root, quality in chords if root == chosen and quality not in MINOR_QUALITIES)
    return chosen, ("minor" if minor > major else "major")


def section_events(bars: list[list[str]], meter: str) -> list[dict]:
    """Absolute chord symbols on a bar grid -> timed events (``R`` rests, ``.`` holds)."""
    span = BAR_QN[meter]
    events, onset = [], 0.0
    for bar in bars:
        tokens = [token for token in bar if token]
        if not tokens:
            onset += span
            continue
        unit = span / len(tokens)
        for token in tokens:
            if token == "R":
                onset += unit
                continue
            if token == ".":
                if events:
                    events[-1]["qn"] += unit
                onset += unit
                continue
            root, quality, bass, intervals, colours = C.parse_symbol(token)
            events.append(dict(root=root, quality=quality, bass=bass,
                               intervals=intervals, colours=colours,
                               qn=unit, onset=onset))
            onset += unit
    merged = []
    for event in events:
        previous = merged[-1] if merged else None
        joined = (previous is not None and previous["root"] == event["root"]
                  and previous["quality"] == event["quality"]
                  and abs(previous["onset"] + previous["qn"] - event["onset"]) < 1e-9
                  and int(previous["onset"] // span) == int(event["onset"] // span))
        if joined:
            previous["qn"] += event["qn"]
        else:
            merged.append(dict(event))
    return merged


def song_events(song: dict) -> list[dict]:
    """Flatten bars into timed chord events; ``.`` extends, ``R`` rests."""
    span = BAR_QN[song["meter"]]
    events, unparsed, total = [], 0, 0
    onset = 0.0
    for bar in song["bars"]:
        tokens = [token for token in bar if token]
        if not tokens:
            onset += span
            continue
        unit = span / len(tokens)
        for token in tokens:
            total += 1
            if token == "R":
                if events:
                    events[-1]["qn"] += unit
                onset += unit
                continue
            if token == ".":
                if not events:
                    onset += unit
                    continue
                events[-1]["qn"] += unit
                onset += unit
                continue
            try:
                root, quality, bass, intervals, colours = C.parse_roman(token, song["key"])
            except ValueError:
                unparsed += 1
                if events:
                    events[-1]["qn"] += unit
                onset += unit
                continue
            events.append(dict(root=root, quality=quality, bass=bass,
                               intervals=intervals, colours=colours,
                               qn=unit, onset=onset, tokens=1))
            onset += unit
    song["parsed"] = total - unparsed
    song["total_tokens"] = total
    merged = []
    for event in events:
        previous = merged[-1] if merged else None
        same_bar = previous is not None and int(previous["onset"] // span) == int(event["onset"] // span)
        if same_bar and previous["root"] == event["root"] and previous["quality"] == event["quality"] \
                and abs(previous["onset"] + previous["qn"] - event["onset"]) < 1e-9:
            previous["qn"] += event["qn"]
        else:
            merged.append(dict(event))
    return merged


def rephase(events: list[dict], span: float) -> list[dict]:
    """Align the bar grid with the chord grid.

    dT-style annotations often place every chord change half a bar after the
    bar line; a pickup then shifts the whole tune.  Keep the longest run of
    chords that share one phase, drop the pickup and rebase the timeline.
    """
    if not events:
        return events
    phases = [round(event["onset"] % span, 3) for event in events]
    phase = Counter(phases).most_common(1)[0][0]
    start = phases.index(phase)
    if any(abs(round(event["onset"] % span, 3) - phase) > 1e-3 for event in events[start:]):
        # mixed harmonic rhythm: keep the tune as is and cut only where a
        # chord change happens to land on a bar line.
        return [], events
    base = events[start]["onset"]
    pickup = events[:start]
    lead = sum(event["qn"] for event in pickup)
    for event in pickup:
        event["onset"] -= base
    for event in events[start:]:
        event["onset"] -= base
    return pickup, events[start:]


def segment(events: list[dict], key: int, bar_span: float = 4.0) -> list[list[dict]]:
    """Split on bar lines only, at tonic arrivals or on the 8-bar grid.

    A cut is never placed mid-bar, so every emitted phrase has complete bars.
    """
    phrases, current, total_qn = [], [], 0.0
    for event in events:
        current.append(event)
        total_qn += event["qn"]
        bars = round(total_qn / bar_span)
        on_bar = abs(total_qn - bars * bar_span) < 1e-3
        if not on_bar:
            continue
        long_enough = len(current) >= MIN_CHORDS
        if len(current) >= MAX_CHORDS or (long_enough and bars >= GRID_BARS):
            phrases.append(current)
            current, total_qn = [], 0.0
    if len(current) >= MIN_CHORDS and abs(total_qn - round(total_qn / bar_span) * bar_span) < 1e-3:
        phrases.append(current)
    return phrases


# ---------------------------------------------------------------- style

def degree_offset(root: int, key: int) -> int:
    return (root - key) % 12


def features(phrase: list[dict], key: int, mode: str) -> dict:
    scale = SCALE[mode]
    offsets = [degree_offset(event["root"], key) for event in phrase]
    diatonic = sum(1 for offset, event in zip(offsets, phrase)
                   if offset in scale and event["quality"] in ("", "m", "7", "maj7", "m7"))
    borrowed = sum(1 for offset in offsets if offset not in scale)
    sevenths = sum(1 for event in phrase if event["quality"] in ("7", "maj7", "m7", "m9", "maj9", "9", "m7b5", "dim7"))
    extended = sum(1 for event in phrase if event["quality"] in ("9", "maj9", "m9", "11", "m11", "13", "add9", "madd9"))
    dominants = sum(1 for event in phrase if event["quality"] in ("7", "9", "13"))
    inversions = sum(1 for event in phrase if event["bass"] != event["root"])
    total = len(phrase)
    # A dominant seventh that resolves down a fifth into a degree other than
    # the tonic is an applied dominant -- the clearest tonal marker the corpus
    # offers without looking at melody.
    applied = 0
    for index in range(total - 1):
        if phrase[index]["quality"] in ("7", "9", "13") \
                and offsets[index] != scale[4] \
                and (offsets[index] - offsets[index + 1]) % 12 == 5:
            applied += 1
    return {
        "n": total,
        "diatonic": diatonic / total,
        "borrowed": borrowed / total,
        "sevenths": sevenths / total,
        "extended": extended / total,
        "dominants": dominants / total,
        "inversions": inversions / total,
        "has_major_four": mode == "minor" and 5 in offsets,
        "has_flat_seven": mode == "major" and 10 in offsets,
        "has_flat_two": mode == "minor" and 1 in offsets,
        "applied": applied / total,
        "cadence_tonic": offsets[-1] == 0,
        "cadence_dominant": offsets[-1] == scale[4],
        "offsets": offsets,
    }


def tier(value: float, low: float, high: float) -> float:
    if value >= high:
        return 0.9
    if value >= (low + high) / 2:
        return 0.65
    if value >= low:
        return 0.35
    return 0.0


def style_affinity(fs: dict, song: dict) -> dict[str, float]:
    """Corpus gives the base category; harmony features add or withdraw weight."""
    base = song.get("styles", {})
    score = dict(base)
    def bump(style: str, value: float) -> None:
        score[style] = max(score.get(style, 0.0), value)

    if fs["extended"] >= 0.4 and fs["sevenths"] >= 0.7:
        bump("jazz", 0.9)
    if fs["sevenths"] >= 0.5 and fs["extended"] >= 0.2:
        bump("rnb", 0.65)
        bump("citypop", 0.65)
    if fs["extended"] >= 0.3 and fs["inversions"] >= 0.25:
        bump("citypop", 0.9)
    if fs["diatonic"] >= 0.95 and fs["sevenths"] <= 0.15:
        bump("folk", 0.65)
        bump("pop", 0.65)
    if fs["borrowed"] >= 0.3:
        bump("experimental", 0.65)
    if fs["has_major_four"] or fs["has_flat_seven"] or fs["has_flat_two"]:
        bump("cinematic", 0.65)
        bump("anime", 0.65)
    if fs["dominants"] >= 0.4 and fs["sevenths"] >= 0.6:
        bump("blues", 0.65)
    if fs["inversions"] >= 0.3:
        bump("jazz", 0.65)
    # Genres are not exclusive: the canon progression is pop and classical at
    # the same time, plain diatonic triads are folk and classical.  These two
    # therefore add graded evidence instead of gating on an all-or-nothing test
    # (the old classical gate demanded no sevenths, no borrowed notes and
    # inversions all at once, which recognised 35 entries of 2,854).
    classical = 0.0
    if fs["diatonic"] >= 0.9:
        classical += 0.35
        if fs["sevenths"] <= 0.15:
            classical += 0.15
        if fs["borrowed"] <= 0.1:
            classical += 0.10
        if fs["inversions"] >= 0.25:
            classical += 0.20
        if fs["cadence_tonic"]:
            classical += 0.10
    if classical:
        bump("classical", round(min(classical, 0.9), 2))
    functional = 0.0
    if fs["cadence_tonic"] or fs["cadence_dominant"]:
        functional += 0.45
        if fs["cadence_tonic"]:
            functional += 0.15
        if fs["sevenths"] <= 0.4:
            functional += 0.10
    if functional:
        bump("functional", round(min(functional, 0.9), 2))
    score = {style: weight for style, weight in score.items() if weight > 0}
    if not score:
        score["other"] = 0.6
    return score


# ---------------------------------------------------------------- encode

def degree_tokens(phrase: list[dict], key: int, mode: str) -> list[tuple]:
    """Functional roman spelling: exact degree first, then flat, then sharp.

    Pop analysis writes ``bVII``/``bIII``/``bVI``; matching the nearest scale
    step in scale order instead produced ``#III`` for a plain ``IV`` whenever a
    chromatic neighbour sat earlier in the scale.
    """
    scale = SCALE[mode]
    tokens = []
    for event in phrase:
        offset = degree_offset(event["root"], key)
        degree, alteration = 7, offset - scale[6]
        for want, alt in ((0, 0), (11, -1), (1, 1)):
            for index, step in enumerate(scale):
                if (offset - step) % 12 == want:
                    degree, alteration = index + 1, alt
                    break
            else:
                continue
            break
        if degree == 5 and alteration < 0:      # bV reads better as #IV
            degree, alteration = 4, 1
        tokens.append((degree, alteration, event["quality"]))
    return tokens


class RestInside(Exception):
    """A bar in the middle of the phrase carries silence; the phrase splits there."""


def _bar_tokens(phrase: list[dict], symbol, position: float, bar_span: float):
    tokens = []
    for event in phrase:
        left = max(event["onset"], position)
        right = min(event["onset"] + event["qn"], position + bar_span)
        if right - left <= 1e-9:
            continue
        tokens.append((symbol(event), right - left))
    return tokens


def _collapse(tokens):
    collapsed = []
    for name, duration in tokens:
        if collapsed and collapsed[-1][0] == name:
            collapsed[-1][1] += duration
        else:
            collapsed.append([name, duration])
    return collapsed


def render_bars(phrase: list[dict], key: int, mode: str, bar_span: float = 4.0,
                lead: float = 0.0) -> str:
    """Re-express the phrase relative to C major / A minor, bar by bar.

    A chord that crosses a bar line is written on both sides with the split
    durations.  Silence in the middle of a phrase is not invented away: the
    caller splits the phrase at that bar (``RestInside``).  A short *first* bar
    (``lead``, an anacrusis kept as written) and a short *last* bar stay short.
    """
    tonic = 0 if mode == "major" else 9
    def symbol(event):
        return C.render_symbol((tonic + degree_offset(event["root"], key)) % 12,
                               event["quality"],
                               (tonic + degree_offset(event["bass"], key)) % 12)
    start = phrase[0]["onset"]
    end = max(event["onset"] + event["qn"] for event in phrase)
    bars, position = [], start
    if lead > 1e-9:
        bars.append(_collapse(_bar_tokens(phrase, symbol, position, lead)))
        position += lead
    while position < end - 1e-9:
        tokens = _bar_tokens(phrase, symbol, position, bar_span)
        total = sum(duration for _, duration in tokens)
        if total < bar_span - 1e-6 and position + bar_span < end - 1e-6:
            raise RestInside(position)
        bars.append(_collapse(tokens))
        position += bar_span
    # Every chord carries its own duration.  Leaving the number off a bar that
    # happens to hold one chord made a whole-bar chord look identical to a
    # four-beat slice of a longer one, and the reader had no way to tell
    # ``| C |`` from ``| C@2 |`` after the fact.
    rendered = []
    for bar in bars:
        rendered.append(" ".join(f"{name}@{duration:g}" for name, duration in bar))
    return " | ".join(rendered)


def split_at_rests(phrase: list[dict], key: int, mode: str, bar_span: float,
                   lead: float = 0.0) -> list[tuple[list[dict], str]]:
    """Split a phrase wherever a bar carries silence; every piece keeps its notes.

    Silence is a phrase boundary, never a reason to drop the music around it.
    """
    try:
        return [(phrase, render_bars(phrase, key, mode, bar_span, lead))]
    except RestInside as rest:
        position = rest.args[0]
        left = [event for event in phrase if event["onset"] + event["qn"] <= position + 1e-9]
        right = [event for event in phrase if event["onset"] >= position + bar_span - 1e-9]
        pieces = []
        for part in (left, right):
            if len(part) >= MIN_CHORDS:
                pieces.extend(split_at_rests(part, key, mode, bar_span))
        return pieces


def performance_key(bars: str, meter: str) -> tuple:
    """Pitch-set + timing signature, spelled the way the compiler compares it.

    Two rows that differ only in chord spelling (``C6`` vs ``Am7/C``) collide
    here, exactly as they would in the compiled library.
    """
    key, onset = [], 0.0
    for index, (symbol, duration) in enumerate(symbols(bars, meter)):
        _, detail, performed = v8.parse_chord(symbol, duration, index, "C", "major")
        key.append((detail["rootPC"], performed["bass"],
                    tuple(sorted(detail["baseIntervals"] + detail["colorIntervals"])),
                    round(onset, 4), round(duration, 4)))
        onset += duration
    return tuple(key)


ROMAN = {1: "I", 2: "II", 3: "III", 4: "IV", 5: "V", 6: "VI", 7: "VII"}
_DEGREE = {value: key for key, value in ROMAN.items()}
_TOKEN = re.compile(r"^([#b]?)([ivIV]+)(.*)$")


def _parse_token(token):
    match = _TOKEN.match(token)
    if not match:
        return None
    accidental, body, suffix = match.groups()
    degree = _DEGREE.get(body.upper())
    if degree is None:
        return None
    alteration = 1 if accidental == "#" else -1 if accidental == "b" else 0
    return degree, alteration, body.islower(), suffix


def _periodic(tokens):
    count = len(tokens)
    for period in range(1, count // 2 + 1):
        if count % period == 0 and all(tokens[i] == tokens[i % period] for i in range(count)):
            return True
    return False


def classify_phrase(tokens, mode, bars=1):
    """Derive (intent, cadence) from what the phrase *does*, not from its spelling.

    The catalogue label is a prior for the engine's own evaluator, so it has to
    match how a player would use the phrase:

    * loop  -- a cell that repeats itself: an exact period, or a short phrase
               whose last chord runs back into the first one by a fourth or a
               fifth (``G-Em-C-D`` returns to ``G`` and is a vamp).  At least
               three chord changes and two bars, so ``I-V`` does not become a
               loop by arithmetic alone.
    * resolve -- settles on the tonic (V/IV/VII -> I) *or* on the dominant
               (half cadence).  "收束到属/主" is the user's definition.
    * color -- chromatic, borrowed or diminished writing that neither closes
               nor repeats.
    * develop -- everything that keeps moving: stops on a middle function,
               deceptive cadence, secondary-dominant chains.
    """
    parsed = [_parse_token(token) for token in tokens]
    if not parsed or any(item is None for item in parsed):
        return "develop", "none"
    first, last = parsed[0], parsed[-1]
    tonic_quality = last[2] if mode == "minor" else not last[2]
    if len(parsed) >= 4 and _periodic(tokens):
        return "loop", "loop_closure"
    if len(parsed) >= 2 and last[0] == 1 and last[1] == 0 and tonic_quality \
            and parsed[-2][1] == 0 and parsed[-2][0] in (4, 5, 7):
        previous = parsed[-2][0]
        cadence = "authentic" if previous == 5 else "plagal" if previous == 4 else "modal"
        return "resolve", cadence
    if last[0] == 5 and last[1] == 0:
        return "resolve", "half"
    if len(parsed) >= 3 and bars >= 2 and len(parsed) <= 8 \
            and (first[0] - last[0]) % 7 in (3, 4):
        return "loop", "loop_closure"
    if any(token[1] != 0 or "dim" in token[3] or "aug" in token[3] for token in parsed):
        return "color", "none"
    if len(parsed) >= 2 and parsed[-2][0] == 5 and last[0] == 6 and last[1] == 0:
        return "develop", "deceptive"
    return "develop", "none"


def encode(row: dict) -> dict:
    mode, meter = row["mode"], row["meter"]
    tonic = "C" if mode == "major" else "A"
    events, full, raw, plain = [], [], [], []
    onset = 0.0
    for index, (symbol, duration) in enumerate(symbols(row["bars"], meter)):
        match, detail, performed = v8.parse_chord(symbol, duration, index, tonic, mode)
        performed["startQN"] = onset
        onset += duration
        full.append(match)
        events.append(detail)
        raw.append(performed)
        plain.append(dict(root=detail["rootPC"], bass=performed["bass"], duration=duration,
                          intervals=sorted(detail["baseIntervals"] + detail["colorIntervals"]),
                          quality=FAMILIES[match["quality"]]))
    v8.validate_projection(row["id"], tonic, mode, full, events, raw)
    tokens = [v8.sequence_token(item) for item in full]
    intent, cadence = classify_phrase(tokens, mode, len(row["bars"].split("|")))
    tags = [f"ingest-{row['source_type']}"]
    built_in = [intent, f"common-{mode}"]
    encoded = dict(id=row["id"], name=row["name_zh"], nameZh=row["name_zh"], nameEn=row["name_en"],
                   aliases=row.get("aliases") or row["id"], builtInTags=",".join(built_in), techniques="",
                   complexityLevel="basic", description=row["name_en"], note=row.get("note", ""),
                   mode=mode, sequence=" ".join(v8.sequence_token(item) for item in full),
                   rhythm=" ".join(str(item["durationQN"]) for item in full), cadence=cadence,
                   intent=intent, loopable=intent == "loop", styles=row["styles"], tags=",".join(tags),
                   skeleton=",".join(map(str, range(len(full)))), meter=meter,
                   sourceType="factory", priorWeight=0.5, complexity=0.5, version=9,
                   phraseLength=len(full), secondaryIntents=0,
                   typedData=json.dumps(dict(events=raw, key=[v8.pitch(tonic), int(mode == "minor")],
                                             full=full, tags=tags, aliases=[row["id"]],
                                             builtInTags=built_in, techniques=[]),
                                        ensure_ascii=False, separators=(",", ":")))
    return encoded


def symbols(bars: str, meter: str):
    numerator, denominator = map(int, meter.split("/"))
    span = numerator * 4 / denominator
    parts = bars.split("|")
    for index, bar in enumerate(parts):
        tokens = bar.split()
        durations = []
        for token in tokens:
            symbol, _, duration = token.partition("@")
            durations.append((symbol, float(duration) if duration else span / len(tokens)))
        residual = span - sum(time for _, time in durations)
        if abs(residual) > 1e-4:
            # An anacrusis, a phrase that ends before its last bar does, or a
            # chord merged across several bars into one cell
            # (``G@15`` for ``G | G | G | G@3``).
            if index in (0, len(parts) - 1) or (
                    len(durations) == 1 and durations[0][1] > span):
                yield from durations
                continue
            raise ValueError(f"incomplete bar in {bars}")
        if durations:
            durations[-1] = (durations[-1][0], durations[-1][1] + residual)
        yield from durations


# ---------------------------------------------------------------- main

def build(corpus: str, sources: Path, out: Path, limit: int | None,
          split: bool = False) -> None:
    if corpus not in ("rs200", "billboard"):
        raise SystemExit(f"unknown corpus {corpus}")
    if corpus == "billboard":
        return build_billboard(sources, out, limit, split)
    songs = load_rs200(sources)
    if limit:
        songs = songs[:limit]
    rows, seen = [], set()
    seen = set()
    stats = Counter()
    for song in songs:
        events = song_events(song)
        span = BAR_QN[song["meter"]]
        pickup, events = rephase(events, span)
        if not any(abs(event["onset"] - round(event["onset"] / span) * span) < 1e-3
                   for event in events):
            stats["off_grid"] += 1
            continue
        stats["songs"] += 1
        stats["tokens"] += song["total_tokens"]
        stats["parsed"] += song["parsed"]
        lead = sum(event["qn"] for event in pickup)
        for index, phrase in enumerate(segment(events, song["key"], BAR_QN[song["meter"]])):
            if index == 0 and pickup:
                phrase = pickup + phrase
            tokens = degree_tokens(phrase, song["key"], song["mode"])
            if len(phrase) < MIN_CHORDS:
                continue
            fs = features(phrase, song["key"], song["mode"])
            styles = style_affinity(fs, song)
            if not styles:
                stats["no_style"] += 1
                continue
            pieces = split_at_rests(phrase, song["key"], song["mode"],
                                    BAR_QN[song["meter"]], lead if index == 0 else 0.0)
            if not pieces:
                stats["silent_bar"] += 1
            for piece_index, (piece, bars) in enumerate(pieces):
                piece_tokens = degree_tokens(piece, song["key"], song["mode"])
                piece_styles = (styles if piece is phrase
                                else style_affinity(features(piece, song["key"], song["mode"]), song))
                if not piece_styles:
                    continue
                bar_count = len(bars.split("|"))
                distinct = len({event["root"] for event in piece})
                if distinct < 3 or len(piece) / bar_count < 0.5:
                    stats["thin"] += 1
                    continue
                signature = performance_key(bars, song["meter"])
                if signature in seen:
                    stats["duplicate"] += 1
                    continue
                seen.add(signature)
                suffix = f"_{piece_index}" if len(pieces) > 1 else ""
                ident = (f"RS_{re.sub(r'[^0-9a-zA-Z]+', '_', song['slug']).upper()}"
                         f"_{index:02d}{suffix}")
                display_zh, display_en = naming.name_phrase(piece_tokens, song["mode"])
                rows.append(dict(
                    id=ident,
                    name_zh=display_zh,
                    name_en=display_en,
                    note=f"{song['title']} - {song['artist']} [{index + 1}{suffix}]",
                    mode=song["mode"], meter=song["meter"],
                    bars=bars,
                    styles=",".join(f"{style}:{weight:g}"
                                    for style, weight in sorted(piece_styles.items())),
                    source_type=corpus,
                ))
                stats["phrases"] += 1
    out.mkdir(parents=True, exist_ok=True)
    tsv = out / "phrases.tsv"
    with tsv.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, delimiter="\t",
                                fieldnames=["id", "name_zh", "name_en", "note", "mode", "meter", "bars", "styles", "source_type"])
        writer.writeheader()
        writer.writerows(rows)
    candidates = out / "candidates.json"
    encoded = [encode(row) for row in rows]
    candidates.write_text(json.dumps(encoded, ensure_ascii=False, separators=(",", ":")),
                          encoding="utf-8")
    coverage = stats["parsed"] / max(1, stats["tokens"])
    print(f"songs={stats['songs']} tokens={stats['tokens']} parse_coverage={coverage:.4f} "
          f"phrases={stats['phrases']} duplicate={stats['duplicate']} "
          f"no_style={stats['no_style']} silent_bar={stats['silent_bar']} "
          f"normalised={stats['normalised']} "
          f"off_grid={stats['off_grid']} thin={stats['thin']}")
    print(f"wrote {tsv} and {candidates}")


def chunk_bars(rendered: str, limit: int = 64) -> list[str]:
    """Keep a long annotated section whole but split it at bar lines for storage.

    The library stores at most 64 chords per entry. A longer section is halved
    on a bar line until every piece fits, so no piece is left with a single
    orphan chord at its edge.
    """
    bars = rendered.split("|")
    total = sum(len(bar.split()) for bar in bars)
    if total <= limit or len(bars) < 2:
        return [rendered]
    target, running, cut = total / 2, 0, 1
    for index, bar in enumerate(bars[:-1], start=1):
        running += len(bar.split())
        cut = index
        if running >= target:
            break
    pieces = ["|".join(bars[:cut]), "|".join(bars[cut:])]
    return [piece for part in pieces for piece in chunk_bars(part, limit)]


def bars_tokens(bars: str, key: int, mode: str, meter: str) -> list[tuple]:
    """Display-only roman labels for a rendered bar string."""
    tokens = []
    for symbol, _ in symbols(bars, meter):
        try:
            root, quality, bass, intervals, colours = C.parse_symbol(symbol)
        except ValueError:
            continue
        tokens.append((root, quality, bass, intervals, colours))
    events = [dict(root=item[0], quality=item[1], bass=item[2], intervals=item[3],
                   colours=item[4], qn=1.0, onset=float(index))
              for index, item in enumerate(tokens)]
    return degree_tokens(events, key, mode)

def bars_astokens(bars: str, key: int, mode: str, meter: str) -> list[list[str]]:
    """Roman label per bar, so boundaries can be scored on the same text the user sees."""
    out = []
    for cell in bars.split("|"):
        if not cell.strip():
            continue
        out.append([roman_token(token) for token in bars_tokens(cell, key, mode, meter)])
    return out


def normalize_rhythm(bars: str) -> str:
    """Collapse chords padded over identical bars before anything else looks at them."""
    cells = [cell for cell in bars.split("|") if cell.strip()]
    collapsed = BD.collapse_run_repeats(cells)
    if len(collapsed) == len(cells):
        return bars
    return "|".join(collapsed)


def tonic_dominant_share(bars: str, mode: str, meter: str) -> float:
    """Fraction of the phrase's sounding time on the tonic or the dominant.

    ``I V I V`` carries no progression the engine can develop; it is one chord
    alternating with its neighbour.  Counting sounding time (not chord count)
    catches the padded forms ``C@4 | G | C | G`` as well as the fast ones.
    """
    tonic = TONIC_OF[mode]
    dominant = (tonic + 7) % 12
    total = hit = 0.0
    for symbol, duration in symbols(bars, meter):
        try:
            root = C.parse_symbol(symbol)[0]
        except ValueError:
            continue
        total += duration
        if root in (tonic, dominant):
            hit += duration
    return hit / total if total else 0.0


def merge_held_bars(bars: str) -> str:
    """Fold a chord repeated bar after bar into one cell of the summed length.

    ``G | G | G | G@3`` is one chord held for four bars, not four chord changes;
    keeping it as four bars inflates the phrase length and makes the entry look
    like a progression where the harmony never moves.
    """
    cells = [cell for cell in bars.split("|") if cell.strip()]
    merged = []
    for cell in cells:
        tokens = cell.split()
        if len(tokens) != 1 or not merged or len(merged[-1].split()) != 1:
            merged.append(cell)
            continue
        previous = merged[-1].split()[0]
        if previous.partition("@")[0] != tokens[0].partition("@")[0]:
            merged.append(cell)
            continue
        held = float(previous.partition("@")[2] or 0) + float(tokens[0].partition("@")[2] or 0)
        merged[-1] = f"{previous.partition('@')[0]}@{held:g}"
    return "|".join(merged)


def chain_cells(bars: str, size: int = 2) -> list[str]:
    """Split an ``A B A C`` chain into its cells, or return nothing.

    A section whose two-bar blocks repeat but do not tile the whole phrase is
    built from cells, not from one long line: ``A B A C`` is two statements of
    ``A``, each answered differently.  Cutting at the block boundary keeps the
    shared half visible in both answers instead of burying it in a longer unit.
    """
    cells = [cell for cell in bars.split("|") if cell.strip()]
    # Two statements of A and two answers is the smallest thing that can be an
    # ``A B A C``. Fewer blocks is a repeat, not a chain.
    if len(cells) < size * 4 or len(cells) % size:
        return []
    blocks = [tuple(BD.cell_key(cell) for cell in cells[start:start + size])
              for start in range(0, len(cells), size)]
    distinct = len(set(blocks))
    if distinct < 2 or distinct >= len(blocks):
        return []
    if BD.period_of(cells):
        return []
    return cells


def unit_bars(cells: list[str], span: float) -> float:
    """Sounding length of a unit in quarter notes, merged chords included."""
    return sum(float(token.partition("@")[2] or span) for cell in cells for token in cell.split())

def attach_held_tails(units: list[tuple[str, str]], meter: str) -> list[tuple[str, str]]:
    """Fold a unit that is nothing but one held chord into the unit before it.

    ``... | D | G | G | G | G@3`` ends with one chord held over four bars.  After
    merging it is a single ``G@15`` cell, which is not a phrase on its own; it is
    the tail of the phrase that led to it.  Returns units unchanged when there is
    nothing to attach.
    """
    if not units:
        return units
    out = [list(units[0])]
    for bars, tag in units[1:]:
        cells = [cell for cell in bars.split("|") if cell.strip()]
        held = merge_held_bars(bars)
        tail = [cell for cell in held.split("|") if cell.strip()]
        prev = [cell for cell in out[-1][0].split("|") if cell.strip()]
        if len(cells) > 1 and len(tail) == 1 \
                and sum(1 for _ in symbols(held, meter)) < MIN_CHORDS \
                and len(prev) + 1 <= SPLIT_HIGH:
            out[-1][0] = out[-1][0].rstrip("| ") + " | " + held
            continue
        out.append([bars, tag])
    return [(bars, tag) for bars, tag in out]

def split_units(bars: str, mode: str, meter: str, stats: Counter) -> list[tuple[str, str]]:
    """Cut one stored section into phrase units. Returns [(rendered bars, tag)].

    The bars handed in are already re-spelled against C major / A minor
    (``render_bars``), so the labels must be read in that same key; passing the
    song's original key here transposed every label a second time.
    """
    # A chord held over several bars is one chord change, not several: merge it
    # before measuring anything, so the section's bar count is the harmonic
    # rhythm the listener hears.
    cells = [c for c in bars.split("|") if c.strip()]
    tonic = 9 if mode == "minor" else 0
    per_bar = bars_astokens(bars, tonic, mode, meter)
    flat = [token for bar in per_bar for token in bar]
    degrees = BD.bars_of(cells, flat)
    if not degrees:
        return [(bars, "")]
    tokens = [token for bar in degrees for token in bar]
    if len(set(tokens)) < 2 or len({tuple(bar) for bar in degrees}) < 2:
        # A static vamp is not a phrase: one chord (or one bar shape) repeated.
        stats["static"] += 1
        return []
    # The tonic/dominant test belongs to the unit, not the section: a long
    # verse that alternates I and V may still contain a four-bar answer worth
    # keeping, so the section is cut first and each unit is judged on its own.
    lead = BD.pickup_length(cells, span=BAR_QN.get(meter))
    if lead:
        stats["pickup"] += 1
    # A cell repeated over the whole section is a loop, not a phrase: fold it to
    # two cycles (four bars at the smallest cell) and store that once.  Emitting
    # the repetitions separately only fills the library with copies of itself.
    period = BD.period_of(cells[lead:])
    # Store the **smallest** usable cycle, not the largest that fits. One cycle is
    # the phrase; the repeat proves it is one, it is not more evidence to hand the
    # matcher. Only when a single cycle carries too few chord changes to stand as
    # a phrase do we store two, and never more than the bar ceiling allows.
    if period:
        one = sum(1 for cell in cells[lead:lead + period] for _ in cell.split())
        cycles = 1 if one >= SPLIT_LOW else 2
        while cycles > 1 and (lead + period * cycles > SPLIT_HIGH
                              or period * cycles > len(cells) - lead):
            cycles -= 1
    else:
        cycles = 0
    if period and period * cycles <= len(cells) - lead:
        keep = lead + period * cycles
        stats["folded"] += 1
        stats["folded_units"] += (len(cells) - lead) // period - cycles
        return [("|".join(cells[:keep]), " u1")]
    # ``A B A C``: two statements of A, answered differently.  Storing it as one
    # long line hides the shared half; storing the two answers keeps it.
    chain = chain_cells("|".join(cells[lead:]))
    if chain:
        stats["chains"] += 1
        # ``A B A C`` cut at the two-bar block: each unit is one statement of A
        # plus its answer, which is where the contrast lives.  A song whose cells
        # are a single chord each cannot be cut that fine and stay above the
        # library minimum, so those fall back to pairing two blocks.
        # The unit has to clear the minimum on its own chords.  A cell of
        # ``C@2 Am@2`` holds two chords, so cutting there would produce units the
        # library rejects; the block pair (``A+B`` / ``A+C``) is the smallest cut
        # that survives.
        halves = ["|".join(chain[start:start + 2]) for start in range(0, len(chain), 2)]
        size = 2 if all(sum(1 for _ in symbols(half, meter)) >= SPLIT_LOW
                        for half in halves) else 4
        return attach_held_tails(
            [("|".join(chain[start:start + size]), f" u{start // size + 1}")
             for start in range(0, len(chain), size)], meter)
    content = [len(bar) for bar in degrees]
    scores = BD.boundary_scores(degrees, SPLIT_WEIGHTS, metre=4,
                                metre_bonus=SPLIT_METRE_BONUS, lead=lead)
    cuts = BD.segment(degrees, scores, SPLIT_LAM, SPLIT_LOW, SPLIT_HIGH, SPLIT_TARGET,
                      0.5, True, lead, content, SPLIT_AMOUNT_HIGH)
    if not cuts:
        return [(bars, "")]
    units = BD.units(cells, cuts)
    return attach_held_tails([("|".join(unit), f" u{index + 1}")
                              for index, unit in enumerate(units)], meter)


def build_billboard(sources: Path, out: Path, limit: int | None, split: bool = False) -> None:
    songs = load_billboard(sources)
    if limit:
        songs = songs[:limit]
    rows, seen, stats = [], set(), Counter()
    for song in songs:
        span = BAR_QN[song["meter"]]
        stats["songs"] += 1
        for index, (label, bars) in enumerate(song["sections"]):
            events = section_events(bars, song["meter"])
            if len(events) < MIN_CHORDS:
                stats["short"] += 1
                continue
            stats["tokens"] += len(events)
            fs = features(events, song["key"], song["mode"])
            styles = style_affinity(fs, song)
            if not styles:
                stats["no_style"] += 1
                continue
            pieces = split_at_rests(events, song["key"], song["mode"], span)
            if not pieces:
                stats["silent_bar"] += 1
            for piece_index, (piece, rendered) in enumerate(pieces):
                if len(piece) < MIN_CHORDS:
                    stats["short"] += 1
                    continue
                normalised = normalize_rhythm(rendered)
                if normalised != rendered:
                    stats["normalised"] += 1
                rendered = normalised
                piece_styles = (styles if piece is events
                                else style_affinity(features(piece, song["key"], song["mode"]), song))
                if not piece_styles:
                    continue
                role = re.sub(r"[^a-z]", "", str(label or "section").lower()) or "section"
                base = f"BB_{song['slug']}_{index:02d}" + (f"_{piece_index}" if len(pieces) > 1 else "")
                if split:
                    units = split_units(rendered, song["mode"], song["meter"], stats)
                else:
                    units = [(rendered, "")]
                for unit_index, (unit_bars, unit_tag) in enumerate(units):
                    # Merge after cutting, so boundaries stay in real bars: a
                    # run of the same chord is one chord change held longer,
                    # not several bars the phrase can be split between.
                    # A chord held over several bars is one chord change: merge
                    # it before anything counts chords, so the entry shows the
                    # harmonic rhythm a player would actually read -- and is
                    # judged on that, not on how many bars it was padded over.
                    held = merge_held_bars(unit_bars)
                    if held != unit_bars:
                        stats["merged"] += 1
                        unit_bars = held
                    chord_count = sum(1 for _ in symbols(unit_bars, song["meter"]))
                    if tonic_dominant_share(unit_bars, song["mode"], song["meter"]) > VAMP_SHARE:
                        stats["vamp_unit"] += 1
                        continue
                    # No second rhythm collapse here: the section was normalised
                    # before it was cut, and folding a unit that is itself a
                    # repeated cell turns four bars of ``C Am`` into one bar.
                    if chord_count < MIN_CHORDS:
                        stats["short"] += 1
                        continue
                    signature = performance_key(unit_bars, song["meter"])
                    if signature in seen:
                        stats["duplicate"] += 1
                        continue
                    seen.add(signature)
                    for chunk_index, chunk in enumerate(chunk_bars(unit_bars)):
                        # The bars are already re-spelled against C major / A
                        # minor, so the label must be read in that key too.
                        tokens = bars_tokens(chunk, TONIC_OF[song["mode"]],
                                             song["mode"], song["meter"])
                        suffix = f"_{chunk_index}" if chunk_index else ""
                        display_zh, display_en = naming.name_phrase(tokens, song["mode"])
                        rows.append(dict(
                            id=base + (f"_u{unit_index}" if len(units) > 1 else "") + suffix,
                            name_zh=display_zh,
                            name_en=display_en,
                            note=f"{song['title']} - {song['artist']} "
                                 f"[{role} {index + 1}{unit_tag}{suffix}]",
                            mode=song["mode"], meter=song["meter"], bars=chunk,
                            styles=",".join(f"{style}:{weight:g}"
                                            for style, weight in sorted(piece_styles.items())),
                            source_type="billboard",
                        ))
                        stats["phrases"] += 1
    out.mkdir(parents=True, exist_ok=True)
    tsv = out / "phrases.tsv"
    fields = ["id", "name_zh", "name_en", "note", "mode", "meter", "bars", "styles", "source_type"]
    with tsv.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, delimiter="\t", fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)
    candidates = out / "candidates.json"
    candidates.write_text(json.dumps([encode(row) for row in rows],
                                     ensure_ascii=False, separators=(",", ":")),
                          encoding="utf-8")
    print(f"songs={stats['songs']} tokens={stats['tokens']} phrases={stats['phrases']} "
          f"normalised={stats['normalised']} "
          f"short={stats['short']} duplicate={stats['duplicate']} "
          f"no_style={stats['no_style']} silent_bar={stats['silent_bar']} "
          f"static={stats['static']} pickup={stats['pickup']} "
          f"folded={stats['folded']} folded_units={stats['folded_units']} "
          f"vamp={stats['vamp']} vamp_unit={stats['vamp_unit']} merged={stats['merged']} "
          f"chains={stats['chains']}")
    print(f"wrote {tsv} and {candidates}")

def roman_token(token: tuple) -> str:
    degree, alteration, quality = token
    text = ("b" if alteration < 0 else "#" if alteration > 0 else "") + ["", "I", "II", "III", "IV", "V", "VI", "VII"][degree]
    if quality in ("m", "m7", "m9", "m11"):
        text = text.lower()
    suffix = {"": "", "m": "", "7": "7", "m7": "7", "maj7": "maj7", "m9": "9", "maj9": "maj9",
              "9": "9", "11": "11", "m11": "11", "13": "13", "dim": "°", "dim7": "°7",
              "m7b5": "ø7", "hdim7": "ø7", "sus4": "sus4", "sus2": "sus2", "aug": "+",
              "6": "6", "m6": "6", "add9": "add9", "madd9": "add9", "mMaj7": "maj7"}.get(quality, quality)
    return text + suffix


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus", choices=("rs200", "billboard"))
    parser.add_argument("--sources", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--limit", type=int)
    parser.add_argument("--split-units", action="store_true",
                        help="store phrase units instead of whole corpus sections (I04)")
    args = parser.parse_args()
    build(args.corpus, args.sources, args.out, args.limit, args.split_units)


if __name__ == "__main__":
    main()
