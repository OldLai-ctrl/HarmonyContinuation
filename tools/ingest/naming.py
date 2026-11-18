"""Name a progression the way a player would describe it.

Order of precedence:

1. A well-known progression (王道進行, カノン, ツーファイブ, ...) gets its
   proper name, with no degrees in it -- the name alone should say what the
   phrase sounds like.
2. A phrase that is one chord away from such a progression is its 变式.
3. Everything else gets a descriptive name built from where it starts, how it
   moves and where it ends.  Only Chinese, English and Arabic numerals appear;
   roman numerals are reserved for the library's subtitle (the old name_zh).

Matching is done on digits (``4 5 3 6``) plus a coarse quality shape, because
``IV V iii vi`` and ``IVmaj7 V7 iii7 vi7`` are the same degrees but different
idioms (王道進行 vs 丸之内進行).
"""

from __future__ import annotations

SEVENTH = {"7", "maj7", "m7", "m9", "maj9", "9", "11", "13", "m11", "6", "m6",
           "add9", "madd9", "dim7", "m7b5", "hdim7", "mMaj7"}
MINOR_Q = {"m", "m7", "m9", "m11", "m6", "madd9", "mMaj7"}

# digits, shape ("triad"/"seventh"/None), Chinese name, English name
MAJOR_IDIOMS = [
    (("4", "5", "3", "6"), "triad", "王道进行", "Royal Road"),
    (("4", "5", "3", "6"), "seventh", "丸之内进行", "Marunouchi (Marusa)"),
    (("6", "4", "5", "1"), None, "小室进行", "Komuro"),
    (("1", "5", "6", "3", "4", "1", "4", "5"), None, "卡农进行", "Canon"),
    (("1", "5", "6", "4"), None, "1564 进行", "1564 (Let It Be)"),
    (("1", "6", "4", "5"), None, "145 情歌进行", "Stand By Me (50s changes)"),
    (("6", "4", "1", "5"), None, "6415 进行", "6415"),
    (("4", "5", "3", "6"), None, "4536 进行", "4536"),
    (("1", "6", "2", "5"), None, "1625 循环", "1625 turnaround"),
    (("2", "5", "1"), None, "二五一终止", "ii-V-I cadence"),
    (("1", "6", "4", "5", "4", "1"), None, "五度下行回环", "Descending fifths loop"),
    (("4", "3", "6", "2", "5"), "seventh", "Just the Two of Us 进行",
     "Just the Two of Us"),
    (("1", "5", "6", "3", "4"), None, "卡农起句", "Canon opening"),
    (("5", "6", "4", "1"), None, "属起回环", "Dominant-start loop"),
    (("1", "4", "5", "4"), None, "主起摇摆循环", "I-IV-V-IV shuffle"),
]
MINOR_IDIOMS = [
    (("1", "b7", "b6", "5"), None, "安达卢西亚进行", "Andalusian cadence"),
    (("1", "b7", "b6", "b7"), None, "弗里吉亚回环", "Phrygian loop"),
    (("6", "4", "5", "1"), None, "小调小室进行", "Minor Komuro"),
    (("1", "b7", "4", "5"), None, "小调摇滚回环", "Minor rock loop"),
    (("1", "4", "5"), None, "小调终止", "Minor cadence"),
]

SHORT_NAMES = {
    "4536": "4536 进行",
    "6415": "6415 进行",
    "1564": "1564 进行",
    "1625": "1625 循环",
}


def _digit(degree: int, alteration: int) -> str:
    return ("b" if alteration < 0 else "#" if alteration > 0 else "") + str(degree)


def _shape_of(qualities) -> str:
    sevenths = sum(1 for quality in qualities if quality in SEVENTH)
    return "seventh" if sevenths * 2 >= len(qualities) else "triad"


def _levenshtein(left, right) -> int:
    previous = list(range(len(right) + 1))
    for i, a in enumerate(left, 1):
        current = [i]
        for j, b in enumerate(right, 1):
            current.append(min(previous[j] + 1, current[j - 1] + 1,
                               previous[j - 1] + (a != b)))
        previous = current
    return previous[-1]


def _match_idiom(digits, qualities, mode):
    table = MINOR_IDIOMS if mode == "minor" else MAJOR_IDIOMS
    shape = _shape_of(qualities)
    best = None
    for idiom_digits, idiom_shape, zh, en in table:
        if idiom_digits != digits:
            continue
        if idiom_shape is not None and idiom_shape != shape:
            continue
        # A shape-specific entry beats a generic one and wins over it.
        rank = 2 if idiom_shape == shape and idiom_shape is not None else 1
        if best is None or rank > best[0]:
            best = (rank, zh, en)
    if best is not None:
        return best[1], best[2]
    return None


def _match_variant(digits, mode):
    """Find the nearest idiom and call this phrase its 变式.

    A variant has to start the same way and be one or two chords away; without
    the shared opening, ``5-3-1-5`` would be called a variant of ``6-4-1-5``,
    which is a different loop altogether.
    """
    table = MINOR_IDIOMS if mode == "minor" else MAJOR_IDIOMS
    best = None
    for idiom_digits, _shape, zh, en in table:
        if len(idiom_digits) < 5 or abs(len(idiom_digits) - len(digits)) > 2:
            continue
        if idiom_digits[:3] != digits[:3]:
            continue
        distance = _levenshtein(digits, idiom_digits)
        if distance <= 2 and (best is None or distance < best[0]):
            best = (distance, zh, en)
    if best is not None:
        return f"{best[1]}变式", f"{best[2]} variant"
    return None


def _motion(parsed) -> str | None:
    if len(parsed) < 3:
        return None
    steps = [(parsed[i][0] - parsed[i + 1][0]) % 7 for i in range(len(parsed) - 1)]
    fifths = sum(1 for step in steps if step == 4)
    down = sum(1 for step in steps if step == 1)
    up = sum(1 for step in steps if step == 6)
    if fifths >= len(steps) - 1 and fifths >= 2:
        return "五度循环"
    if down >= len(steps) - 1:
        return "级进下行"
    if up >= len(steps) - 1:
        return "级进上行"
    if any(item[2] in ("7", "9", "13") and item[0] != 5 and item[1] == 0
           for item in parsed[:-1]):
        return "副属推进"
    if any(item[1] != 0 for item in parsed):
        return "借用半音"
    return None


def describe(parsed, mode) -> tuple[str, str]:
    """Descriptive fallback name: start, motion, ending, plus one colour word."""
    first, last = parsed[0], parsed[-1]
    tonic_end = last[0] == 1 and last[1] == 0
    dominant_end = last[0] == 5 and last[1] == 0
    sevenths = sum(1 for item in parsed if item[2] in SEVENTH)

    zh_parts, en_parts = [], []
    if first[0] == 1 and first[1] == 0:
        zh_parts.append("主起")
        en_parts.append("home start")
    else:
        zh_parts.append(f"{_digit(first[0], first[1])}级起")
        en_parts.append(f"from {_digit(first[0], first[1])}")

    motion = _motion(parsed)
    if motion:
        zh_parts.append(motion)
        en_parts.append({"五度循环": "fifths cycle", "级进下行": "stepwise down",
                         "级进上行": "stepwise up", "副属推进": "secondary dominants",
                         "借用半音": "borrowed tones"}[motion])

    if tonic_end:
        zh_parts.append("回主")
        en_parts.append("home")
    elif dominant_end:
        zh_parts.append("停在属音")
        en_parts.append("on the dominant")
    else:
        zh_parts.append(f"悬在{_digit(last[0], last[1])}级")
        en_parts.append(f"hangs on {_digit(last[0], last[1])}")

    if sevenths * 2 >= len(parsed):
        zh_parts.append("七和弦")
        en_parts.append("sevenths")
    return " · ".join(zh_parts), " · ".join(en_parts)


def name_phrase(tokens, mode: str) -> tuple[str, str]:
    """tokens: [(degree, alteration, quality), ...] as degree_tokens() returns."""
    parsed = [(degree, alteration, quality) for degree, alteration, quality in tokens]
    digits = tuple(_digit(degree, alteration) for degree, alteration, _q in parsed)
    qualities = [quality for _d, _a, quality in parsed]

    idiom = _match_idiom(digits, qualities, mode)
    if idiom:
        return idiom
    joined = "".join(digits)
    if joined in SHORT_NAMES and len(parsed) <= 4:
        return SHORT_NAMES[joined], joined
    variant = _match_variant(digits, mode)
    if variant:
        return variant
    return describe(parsed, mode)
