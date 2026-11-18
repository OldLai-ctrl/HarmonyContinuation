"""Billboard 2.0 (McGill Billboard) SALAMI chord annotations.

The corpus already carries its own section markers, so nothing here infers a
boundary: ``A, chorus,`` on a line starts a section, and every entry is exactly
the bars the annotator put between two markers.

Chord vocabulary in the corpus (``C:maj``, ``A:min7``, ``D:maj/5``, ``N`` for a
rest, ``.`` for "same as the previous chord") is mapped onto the engine's
quality set.  Two spellings have no exact equivalent and are folded, not
dropped: ``:5`` (power chord) and ``:1`` (single note) both become a major
triad, because the annotation deliberately omits the third.
"""

from __future__ import annotations

import re
from pathlib import Path

# SALAMI quality -> (chords.QUALITIES key, is this an exact match?)
QUALITY = {
    "maj": ("", True),
    "min": ("m", True),
    "7": ("7", True),
    "min7": ("m7", True),
    "maj7": ("maj7", True),
    "sus4": ("sus4", True),
    "sus2": ("sus2", True),
    "dim": ("dim", True),
    "dim7": ("dim7", True),
    "hdim7": ("hdim7", True),
    "aug": ("aug", True),
    "6": ("6", True),
    "maj6": ("6", True),
    "m6": ("m6", True),
    "min6": ("m6", True),
    "9": ("9", True),
    "maj9": ("maj9", True),
    "min9": ("m9", True),
    "11": ("11", True),
    "maj11": ("maj11", True),
    "min11": ("m11", True),
    "13": ("13", True),
    "maj13": ("maj9", False),
    "min13": ("m11", False),
    "minmaj7": ("mMaj7", True),
    "5": ("", False),
    "1": ("", False),
    # ``EXTENSION`` produces these canonical spellings, so they resolve here too;
    # without them ``X:maj/11`` was accepted and then dropped one step later.
    "m9": ("m9", True),
    "m11": ("m11", True),
    "m13": ("m13", True),
}

# quality + slash extension -> the quality that actually describes the sonority
EXTENSION = {
    ("maj", "9"): ("maj9", None), ("maj", "11"): ("maj11", None), ("maj", "13"): ("maj13", None),
    ("maj7", "9"): ("maj9", None), ("maj7", "11"): ("maj11", None), ("maj7", "13"): ("maj13", None),
    ("min", "9"): ("m9", None), ("min", "11"): ("m11", None), ("min", "13"): ("m13", None),
    ("min7", "9"): ("m9", None), ("min7", "11"): ("m11", None), ("min7", "13"): ("m13", None),
    ("7", "9"): ("9", None), ("7", "11"): ("11", None), ("7", "13"): ("13", None),
    ("sus2", "9"): ("sus2", None), ("sus4", "9"): ("sus4", None),
}

NOTE_NAMES = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]
BASE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
TRIAD = {"": [0, 4, 7], "m": [0, 3, 7], "7": [0, 4, 7, 10], "maj7": [0, 4, 7, 11],
         "m7": [0, 3, 7, 10], "dim": [0, 3, 6], "dim7": [0, 3, 6, 9],
         "hdim7": [0, 3, 6, 10], "aug": [0, 4, 8], "sus4": [0, 5, 7], "sus2": [0, 2, 7],
         "9": [0, 4, 7, 10], "maj9": [0, 4, 7, 11], "m9": [0, 3, 7, 10],
         "11": [0, 4, 7, 10], "maj11": [0, 4, 7, 11], "m11": [0, 3, 7, 10],
         "13": [0, 4, 7, 10], "maj13": [0, 4, 7, 11], "m13": [0, 3, 7, 10],
         "6": [0, 4, 7, 9], "m6": [0, 3, 7, 9], "mMaj7": [0, 3, 7, 11],
         "add9": [0, 4, 7], "madd9": [0, 3, 7]}


def _pitch(name: str) -> int:
    value = BASE[name[0].upper()]
    for accidental in name[1:]:
        if accidental in "#♯":
            value += 1
        elif accidental in "b♭":
            value -= 1
    return value % 12


def resolve_bass(root: str, quality: str, token: str) -> str | None:
    """Turn a SALAMI slash token into a concrete bass note.

    ``3``/``5``/``7``/``b7`` name a chord tone; ``9``/``11``/``13``/``b9`` are
    colour tones that the chord already carries, so the slash adds no bass and
    is dropped rather than invented.
    """
    token = token.strip()
    if not token:
        return None
    if token[0] in "ABCDEFG":
        return token
    value = token
    accidental = 0
    while value and value[0] in "b#":
        accidental += -1 if value[0] == "b" else 1
        value = value[1:]
    if not value.isdigit():
        return None
    degree = int(value)
    intervals = TRIAD.get(quality, [0, 4, 7])
    index = {3: 1, 5: 2, 7: 3}.get(degree)
    if index is None:
        return None
    if index >= len(intervals):
        return None
    offset = intervals[index]
    if degree == 7:
        offset += accidental  # ``b7``
    low = _pitch(root) + offset
    return NOTE_NAMES[low % 12]

HEADER = re.compile(r"^#\s*(\w+):\s*(.+?)\s*$")


def parse_header(text: str) -> dict:
    header = {}
    for line in text.splitlines():
        match = HEADER.match(line)
        if match:
            header[match.group(1).lower()] = match.group(2)
        elif not line.startswith("#"):
            break
    return header


def parse_lines(text: str):
    """Yield ``(section, bars)`` groups; ``section`` is None for the first bars."""
    section, bars, pending = None, [], []

    def flush():
        nonlocal bars
        if bars:
            yield_group = (section, bars)
            bars = []
            return [yield_group]
        return []

    groups = []
    for line in text.splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        _, _, body = line.partition("\t")
        body = body.strip()
        if not body or body in ("silence", "end"):
            continue
        marker = re.match(r"^([A-Z])(?:'*)\s*,\s*([A-Za-z0-9 -]+?)\s*,", body)
        if marker:
            groups.extend(flush())
            section = (marker.group(1) + marker.group(2).strip(), marker.group(2).strip())
            body = body[marker.end():]
        first, last = body.find("|"), body.rfind("|")
        if first < 0 or last <= first:
            continue
        for chunk in body[first + 1: last].split("|"):
            tokens = chunk.split()
            bars.append(tokens)
    groups.extend(flush())
    return section, groups


def parse_song(path: Path) -> dict | None:
    text = path.read_text(encoding="utf-8", errors="replace")
    header = parse_header(text)
    tonic = header.get("tonic", "").strip()
    meter = header.get("metre", header.get("meter", "4/4")).strip()
    if not tonic or not meter:
        return None
    _, groups = parse_lines(text)
    if not groups:
        return None
    bars = [bar for _, group in groups for bar in group]
    sections = []
    for name, group in groups:
        sections.append((name, group))
    return dict(slug=path.parent.name,
                title=header.get("title", path.parent.name),
                artist=header.get("artist", ""),
                tonic=tonic, meter=meter, sections=sections, bars=bars)


def chord_tokens(tokens: list[str]) -> list[str]:
    """SALAMI bar tokens -> tokens the ingest event reader understands.

    A bar is returned as ``[]`` only when nothing in it could be read.  One odd
    token must not erase the rest of the bar: the corpus writes qualifiers such
    as ``C:1(b3,b7)/b7``, ``&pause`` and ``*`` that carry no chord of their own,
    and dropping the whole bar with them was silently deleting music.
    """
    out = []
    for token in tokens:
        token = token.strip().rstrip(",")
        # Only a bare bracket standing on its own is noise.  ``Ab:7(#9)`` also
        # ends with ``)`` and used to be thrown away by that test.
        if not token or token in ("(", ")") or token.startswith("(") or token.startswith(")"):
            continue
        if token in ("&pause", "*"):
            # ``&pause`` is a written silence; ``*`` is an unlabelled bar.  Both
            # are kept as rests so the surrounding music still reaches the
            # library instead of losing the whole section to one of them.
            out.append("R")
            continue
        if token == "N":
            out.append("R")
            continue
        if token == ".":
            out.append(".")
            continue
        symbol = _symbol(token)
        if symbol is not None:
            out.append(symbol)
    return out


def _symbol(token: str) -> str | None:
    """One SALAMI token -> an engine chord symbol, or ``None`` if unreadable."""
    match = re.fullmatch(
        r"([A-G][#b]?):([A-Za-z0-9]+)(?:\(([A-Za-z0-9,#b]+)\))?(?:/([A-Za-z0-9#b]+))?", token)
    if not match:
        return None
    root, quality, parens, bass = match.groups()
    # ``Db:maj(9)/5`` writes the colour tone parenthesised instead of slashed;
    # it names the same sonority as ``Db:maj/9``, so it takes the same route.
    if parens in ("9", "11", "13") and (quality, parens) in EXTENSION:
        quality = EXTENSION[(quality, parens)][0]
    if bass in ("9", "11", "13"):
        # ``A:maj/9`` is "A major with a 9", not "A major over 9 in the bass"
        quality, bass = EXTENSION.get((quality, bass), (quality, None))
        if quality is None:
            return None
    mapped = QUALITY.get(quality)
    if mapped is None and parens:
        # ``A:5(b7)`` / ``A:1(b3,b7)``: a shell voicing of the plain triad.
        mapped = QUALITY.get(quality)
    if mapped is None:
        return None
    suffix, _ = mapped
    symbol = root + suffix
    resolved = resolve_bass(root, suffix, bass) if bass else None
    if resolved:
        symbol += "/" + resolved
    return symbol
