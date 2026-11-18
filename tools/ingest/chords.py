"""Chord vocabulary for real-corpus ingest.

Two notations are supported:

* absolute symbols (Billboard SALAMI): ``C``, ``F#m7``, ``Bb:maj7``, ``C/E``
* roman numerals (rock corpus): ``I``, ``bVII``, ``V11``, ``I64``, ``V/V``

Everything is reduced to root pitch class, quality token, bass pitch class and
the sounding intervals; nothing above the thirteenth is kept, and no voicing
information survives.
"""

from __future__ import annotations

NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
MAJOR_STEPS = {"I": 0, "II": 2, "III": 4, "IV": 5, "V": 7, "VI": 9, "VII": 11}
DEGREES = ["VII", "VI", "IV", "V", "III", "II", "I"]

QUALITIES: dict[str, tuple[list[int], list[int]]] = {
    "": ([0, 4, 7], []),
    "m": ([0, 3, 7], []),
    "7": ([0, 4, 7, 10], []),
    "maj7": ([0, 4, 7, 11], []),
    "m7": ([0, 3, 7, 10], []),
    "dim": ([0, 3, 6], []),
    "dim7": ([0, 3, 6, 9], []),
    "hdim7": ([0, 3, 6, 10], []),
    "aug": ([0, 4, 8], []),
    "sus4": ([0, 5, 7], []),
    "sus2": ([0, 2, 7], []),
    "6": ([0, 4, 7], [9]),
    "m6": ([0, 3, 7], [9]),
    "9": ([0, 4, 7, 10], [2]),
    "maj9": ([0, 4, 7, 11], [2]),
    "m9": ([0, 3, 7, 10], [2]),
    "11": ([0, 4, 7, 10], [2, 5]),
    "m11": ([0, 3, 7, 10], [2, 5]),
    "13": ([0, 4, 7, 10], [2, 5, 9]),
    "add9": ([0, 4, 7], [2]),
    "madd9": ([0, 3, 7], [2]),
    "mMaj7": ([0, 3, 7, 11], []),
    "maj11": ([0, 4, 7, 11], [2, 5]),
    "maj13": ([0, 4, 7, 11], [2, 5, 9]),
    "m13": ([0, 3, 7, 10], [2, 5, 9]),
}

# Token used when a symbol is re-rendered relative to C (major) or A (minor).
SUFFIX = {"": "", "m": "m", "7": "7", "maj7": "maj7", "m7": "m7", "dim": "dim",
          "dim7": "dim7", "hdim7": "m7b5", "aug": "aug", "sus4": "sus4",
          "sus2": "sus2", "6": "6", "m6": "m6", "9": "9", "maj9": "maj9",
          "m9": "m9", "11": "11", "m11": "m11", "13": "13", "add9": "add9", "mMaj7": "mMaj7", "maj11": "maj11", "maj13": "maj13", "m13": "m13",
          "madd9": "madd9"}

TRIAD_INVERSION = {"6": 1, "64": 2}
SEVENTH_INVERSION = {"65": 1, "43": 2, "42": 3}


def pitch_class(token: str) -> int:
    base = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
    text = token.strip()
    if not text or text[0].upper() not in base:
        raise ValueError(f"bad pitch name: {token!r}")
    value = base[text[0].upper()]
    for accidental in text[1:]:
        if accidental in "#♯":
            value += 1
        elif accidental in "b♭":
            value -= 1
        elif accidental == "♮":
            pass
        else:
            raise ValueError(f"bad accidental in {token!r}")
    return value % 12


def parse_key(text: str) -> tuple[int, str]:
    """``F`` -> (5,'major'); ``Fm`` / ``f minor`` -> (5,'minor')."""
    body = text.strip()
    lowered = body.lower()
    if lowered.endswith(" minor"):
        return pitch_class(body[: -len(" minor")]), "minor"
    if lowered.endswith(" major"):
        return pitch_class(body[: -len(" major")]), "major"
    if body[-1:] == "m":
        return pitch_class(body[:-1]), "minor"
    if body[0].islower():
        return pitch_class(body[0].upper() + body[1:]), "minor"
    return pitch_class(body), "major"


def _split_degree(text: str) -> tuple[str, str]:
    for degree in DEGREES:
        if text.upper().startswith(degree):
            return text[: len(degree)], text[len(degree):]
    raise ValueError(f"bad roman degree in {text!r}")


def _roman_quality(degree: str, suffix: str) -> tuple[str, str] | None:
    lower = degree.islower()
    if suffix in ("", "6", "64"):
        return ("m" if lower else ""), suffix
    if suffix in SEVENTH_INVERSION:
        return ("m7" if lower else "7"), suffix
    if suffix == "7":
        return ("m7" if lower else "7"), ""
    if suffix in ("maj7", "M7", "Δ7"):
        return ("maj7" if not lower else "m7"), ""
    if suffix in ("9",):
        return ("m9" if lower else "9"), ""
    if suffix in ("maj9",):
        return "maj9", ""
    if suffix in ("11",):
        return ("m11" if lower else "11"), ""
    if suffix in ("13",):
        return ("m7" if lower else "13"), ""
    if suffix in ("s4", "sus4", "sus"):
        return "sus4", ""
    if suffix in ("°", "dim", "o"):
        return "dim", ""
    if suffix in ("°7", "dim7", "o7"):
        return "dim7", ""
    if suffix in ("ø7", "ø", "m7b5"):
        return "hdim7", ""
    if suffix in ("+", "aug", "#5"):
        return "aug", ""
    return None


def parse_roman(token: str, key: int) -> tuple[int, str, int, list[int], list[int]]:
    """Roman numeral -> (root, quality, bass, intervals, colours).

    Accidentals and degrees are read against the major scale, which is how the
    rock corpus writes minor-key music (``i``, ``bIII``, ``bVI``, ``bVII``).
    """
    text = token.strip()
    accidental = 0
    while text and text[0] in "b#♭♯":
        accidental += -1 if text[0] in "b♭" else 1
        text = text[1:]
    secondary = None
    if "/" in text:
        text, secondary = text.split("/", 1)
    degree, suffix = _split_degree(text)
    parsed = _roman_quality(degree, suffix)
    if parsed is None:
        raise ValueError(f"unsupported roman suffix in {token!r}")
    quality, inversion = parsed
    intervals, colours = QUALITIES[quality]
    root = (key + MAJOR_STEPS[degree.upper()] + accidental) % 12
    bass = root
    if inversion in TRIAD_INVERSION:
        bass = (root + intervals[TRIAD_INVERSION[inversion]]) % 12
    elif inversion in SEVENTH_INVERSION:
        index = SEVENTH_INVERSION[inversion]
        if index >= len(intervals):
            raise ValueError(f"inversion does not fit {token!r}")
        bass = (root + intervals[index]) % 12
    if secondary:
        target = parse_roman(secondary, key)[0]
        root = bass = target
        quality = "7"
        intervals, colours = QUALITIES["7"]
    return root, quality, bass, intervals, colours


def _split_symbol_quality(suffix: str) -> tuple[str, str] | None:
    """``m7`` -> ('m7',''); ``maj7`` -> ('maj7',''); ``64`` -> ('','64')."""
    if suffix in ("", "6", "64", "7", "65", "43", "42"):
        if suffix in TRIAD_INVERSION or suffix in SEVENTH_INVERSION:
            return ("7" if suffix in SEVENTH_INVERSION else ""), suffix
        return ("" if suffix == "" else "7"), suffix
    for quality in sorted(QUALITIES, key=len, reverse=True):
        if quality and suffix.startswith(quality):
            return quality, suffix[len(quality):]
    return None


def parse_symbol(token: str) -> tuple[int, str, int, list[int], list[int]]:
    """Absolute chord symbol -> (root, quality, bass, intervals, colours).

    Accepts ``C``, ``F#m7``, ``Bb:min7``, ``C/E``, ``A:maj7/3``. A slash bass
    may be a pitch name or a chord degree (``/3``, ``/b7``).
    """
    text = token.strip().replace(":", "")
    body, _, bass_token = text.partition("/")
    body = body.strip()
    if not body:
        raise ValueError(f"empty chord symbol: {token!r}")
    name = body[0].upper()
    rest = body[1:]
    while rest and rest[0] in "#♯b♭":
        name += rest[0]
        rest = rest[1:]
    if rest[:3].lower() == "min":
        rest = "m" + rest[3:]
    elif rest[:3].lower() == "maj" and rest[3:4] != "7":
        rest = "" + rest[3:]
    parsed = _split_symbol_quality(rest)
    if parsed is None:
        raise ValueError(f"unsupported quality in {token!r}")
    quality, inversion = parsed
    intervals, colours = QUALITIES[quality]
    root = pitch_class(name)
    bass = root
    if inversion in TRIAD_INVERSION:
        bass = (root + intervals[TRIAD_INVERSION[inversion]]) % 12
    elif inversion in SEVENTH_INVERSION:
        bass = (root + intervals[SEVENTH_INVERSION[inversion]]) % 12
    if bass_token.strip():
        value = bass_token.strip()
        if value[0] in "1234567b#":
            offset = MAJOR_STEPS[DEGREES[7 - int(value.lstrip("b#"))]] if value.lstrip("b#") in "1234567" else None
            if offset is None:
                raise ValueError(f"bad slash degree in {token!r}")
            if value[0] == "b":
                offset -= 1
            elif value[0] == "#":
                offset += 1
            bass = (root + offset) % 12
        else:
            bass = pitch_class(value)
    return root, quality, bass, intervals, colours


def render_symbol(root: int, quality: str, bass: int) -> str:
    text = NAMES[root % 12] + SUFFIX[quality]
    return text if bass % 12 == root % 12 else f"{text}/{NAMES[bass % 12]}"
