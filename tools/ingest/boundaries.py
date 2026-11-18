"""Bar-level phrase boundary evidence and dynamic-programming segmentation.

This module is the ingestion-side answer to I04 ("every library entry is a whole
section, so recommendations are hopelessly long"). A unit is cut only on a bar
line, and only where several independent signals agree: a cadence lands, the
next bar restarts the section's opening chord, a periodic pattern restarts, the
bar before is empty, or the rhythmic density jumps. The DP then picks the cut
set that maximises total evidence minus a per-cut price (``lam``) and a soft
length prior, so a phrase is never split just because it is long.

Nothing here touches product code; ``tools/ingest/build.py --split-units``
consumes it at ingest time.
"""

from __future__ import annotations

import random
import re

TOKEN_RE = re.compile(r"^([#b]?)([ivIV]+)")

# A dominant or predominant function immediately before a cut is the strongest
# evidence that the phrase ended there rather than merely paused.
CADENCE_PREVIOUS = {
    "V", "V7", "v", "v7", "IV", "IV7", "iv", "ii", "II", "ii7", "II7",
    "bVII", "bVII7", "bII", "VII", "vii", "vii7", "viiø7",
}
TONIC = {"I", "i", "Imaj7", "imaj7"}
DECEPTIVE_TARGET = {"vi", "VI", "iii", "III"}

DEFAULT_WEIGHTS = {
    "repeat": 0.45,
    "cadence": 0.35,
    "restart": 0.15,
    "rest": 0.30,
    "density": 0.10,
}

# A section can begin before the first downbeat of its phrase.  The bar is real
# music and stays in the unit, but it does not weigh like a structural bar:
# evidence inside it is damped, and the hypermetric grid is anchored after it
# instead of at the raw bar index.
PICKUP_WEIGHT = 0.6
# A cell this long is the section, not a phrase in it: fold it once, never store
# the whole tiling.
MAX_PERIOD = 8


def degree_of(token: str) -> str:
    """Strip the quality suffix, keep the accidental and the roman numeral."""
    match = TOKEN_RE.match(token)
    if not match:
        return token
    return match.group(1) + match.group(2)


def bars_of(cells, degrees):
    """Split a flat degree list into per-bar lists using the original bar cells."""
    flat = [degree_of(token) for token in degrees]
    bars, index = [], 0
    for cell in cells:
        width = len(cell.split())
        bars.append(flat[index:index + width])
        index += width
    if index != len(flat):
        return None
    return bars or None


def cadence_score(previous_bar, next_bar, mode="major"):
    if not previous_bar or not next_bar:
        return 0.0
    before, after = previous_bar[-1], next_bar[0]
    if after in TONIC and before in CADENCE_PREVIOUS:
        return 1.0
    if before in {"V", "V7", "v", "v7"} and after in DECEPTIVE_TARGET:
        return 0.55
    return 0.0


def similarity(left, right):
    if not left or not right or len(left) != len(right):
        return 0.0
    total = sum(len(bar) for bar in left)
    if not total:
        return 0.0
    same = sum(
        1
        for bar_left, bar_right in zip(left, right)
        for a, b in zip(bar_left, bar_right)
        if a == b
    )
    return same / total


def repeat_score(bars, index, max_period=8, threshold=0.75):
    best = 0.0
    for period in range(1, max_period + 1):
        if index - period < 0 or index + period > len(bars):
            break
        score = similarity(bars[index - period:index], bars[index:index + period])
        if score >= threshold:
            best = max(best, score)
    return best


def density_score(bars, index):
    before, after = len(bars[index - 1]), len(bars[index])
    return 0.4 if abs(before - after) >= 2 else 0.0


def rest_score(bars, index):
    return 0.8 if (not bars[index - 1] or not bars[index]) else 0.0


def restart_score(bars, index):
    opening = bars[0][0] if bars[0] else None
    previous = bars[index - 1][-1] if bars[index - 1] else None
    current = bars[index][0] if bars[index] else None
    if current and opening and current == opening and previous and previous != current:
        return 0.7
    return 0.0


def boundary_scores(bars, weights=None, metre=4, metre_bonus=0.20, lead=0):
    """Per-bar-line boundary evidence; ``metre_bonus`` rewards hypermetric alignment.

    Cutting on a bar line that also closes a 4-bar hypermetric group is the
    normal case in this repertoire. Without the bonus the DP happily ends a unit
    one bar into the next loop, which yields units that begin like a phrase and
    stop mid-loop.

    ``lead`` is the number of bars stripped as a pickup before the grid starts.
    The pickup stays in the unit (it is real music), but the grid is anchored
    after it and no cut is allowed immediately behind it.
    """
    weights = weights or DEFAULT_WEIGHTS
    scores = [0.0] * len(bars)
    for index in range(1, len(bars)):
        if index <= lead:
            continue
        evidence = (
            weights["repeat"] * repeat_score(bars, index)
            + weights["cadence"] * cadence_score(bars[index - 1], bars[index])
            + weights["restart"] * restart_score(bars, index)
            + weights["rest"] * rest_score(bars, index)
            + weights["density"] * density_score(bars, index)
        )
        if metre and (index - lead) % metre == 0:
            evidence += metre_bonus
        scores[index] = evidence
    return scores


def length_cost(length, low, high, target, weight):
    """Soft prior: pay for being outside [low, high].

    No lean towards the long end any more. The library wants the *smallest*
    usable phrase: a longer entry is not more evidence, it is a phrase plus
    whatever followed it, and matching a short fragment against it dilutes the
    score (``BB_0698_02`` and its 14-chord neighbours were the warning).
    """
    cost = 0.0
    if length < low:
        cost += weight * (low - length) / max(1, low)
    elif length > high:
        cost += weight * (length - high) / max(1, high)
    return cost


def segment(bars, scores, lam=0.6, low=3, high=10, target=6.0, length_weight=0.5,
            hard_low=True, lead=0, content=None, amount_high=None):
    """Return the cut positions (bar indices) that maximise total evidence - cost.

    Two different ceilings, because two different things are being limited:

    * ``high`` is a **bar** ceiling. An entry covering half a section is not a
      phrase however many chord changes it holds, and the library should never
      store one.
    * ``low`` is a **chord-change** floor when ``content`` is given, and a bar
      floor otherwise. Bar count is not what makes a phrase: a tempo-60 tune can
      spend one bar on a whole phrase while a tempo-170 tune needs four, and the
      corpus carries no tempo to tell them apart. Chord changes are the thing the
      library actually matches on, so that is what the floor counts.

    ``amount_high`` caps the chord changes in one unit (``length_cost`` uses it);
    it defaults to ``high``.

    With ``hard_low`` a unit below the floor is only allowed when no other split
    exists (the whole section is itself below it). Without it the DP happily
    leaves 1-2 bar residues at a section tail, which are pure noise.

    ``lead`` bars at the front are a pickup. They stay inside the first unit and
    buy back none of their own length: an anacrusis bar is not a structural bar
    of the phrase that follows it.
    """
    count = len(bars)
    by_content = content is not None
    content = content if by_content else [1] * count
    amount_high = amount_high if amount_high is not None else high
    best = [-1e9] * (count + 1)
    back = [0] * (count + 1)
    best[0] = 0.0
    for end in range(1, count + 1):
        for start in range(0, end):
            length = end - start
            if length > high:
                continue
            amount = sum(content[start:end])
            measure = amount if by_content else length
            if hard_low and measure < low and not (start == 0 and end == count):
                continue
            value = (
                best[start]
                + (scores[end] if end < count else 0.0)
                - lam
                - length_cost(measure, low, amount_high, target, length_weight)
            )
            if value > best[end]:
                best[end] = value
                back[end] = start
    cuts, cursor = [], count
    while cursor > 0:
        cursor = back[cursor]
        if cursor > 0:
            cuts.append(cursor)
    return sorted(cuts)


def units(bars, cuts):
    edges = [0] + list(cuts) + [len(bars)]
    return [bars[edges[i]:edges[i + 1]] for i in range(len(edges) - 1)]


def stability(bars, cuts, lam, low, high, target, length_weight, rounds=3, noise=0.08, seed=7,
              hard_low=True):
    """Share of units whose boundaries survive small score noise."""
    if not bars:
        return 1.0
    rng = random.Random(seed)
    base = set(cuts)
    kept = total = 0
    for _ in range(rounds):
        base_scores = boundary_scores(bars)
        jittered = [
            value + rng.uniform(-noise, noise) if index else 0.0
            for index, value in enumerate(base_scores)
        ]
        noisy = set(segment(bars, jittered, lam, low, high, target, length_weight, hard_low))
        for cut in base:
            total += 1
            kept += 1 if cut in noisy else 0
    return kept / total if total else 1.0

def cell_key(cell):
    """Bar signature with durations ignored: ``C@2 G@2`` and ``C G`` are the same."""
    return tuple(token.split("@")[0] for token in cell.split())


def period_of(cells, max_period=MAX_PERIOD, coverage=0.95):
    """Smallest period that explains the cells, or 0 when none does.

    A cell repeated at least twice is a loop, not a phrase.  Requiring the
    period to tile the whole run (not just start it) keeps a section that opens
    with a one-off bar from being folded as if that bar belonged to the cycle.
    """
    keys = [cell_key(cell) for cell in cells]
    count = len(keys)
    for period in range(1, max_period + 1):
        if 2 * period > count:
            break
        same = sum(1 for index in range(period, count) if keys[index] == keys[index - period])
        if count > period and same / (count - period) >= coverage:
            return period
    return 0


def bar_total(cell, span=None):
    """Sounding length of one bar; ``@`` durations win, otherwise one per token."""
    total = 0.0
    for token in cell.split():
        _, _, duration = token.partition("@")
        total += float(duration) if duration else (1.0 if span is None else span)
    return total


def pickup_length(cells, max_lead=2, span=None):
    """How many opening bars sit outside the section's periodic structure.

    A section that repeats ``| C G | F G |`` eight times has a two-bar cell and
    no pickup.  The same section written ``| F/C | C G | F G | ... |`` starts on
    a bar that never comes back -- the anacrusis the annotator heard.  A bar
    that is merely part of the loop (``| C G |`` as the first bar of a two-bar
    cell) is never mistaken for a pickup.

    A first bar that does not sound for the whole bar is the other kind of
    anacrusis: the section opens with empty beats and the first chord enters
    late (I05).  ``span`` is the bar length in quarter notes; without it the
    gap test is skipped.
    """
    if span and cells and bar_total(cells[0]) < span - 1e-3:
        return 1
    if len(cells) < 5:
        return 0
    keys = [cell_key(cell) for cell in cells]
    for lead in range(0, max_lead + 1):
        if lead == 0:
            if period_of(cells):
                return 0
            continue
        body = cells[lead:]
        if len(body) < 4:
            break
        period = period_of(body)
        if period and period * 2 <= len(body):
            return lead
    # No periodic reading: a lone opener that never returns is still a pickup,
    # but only in a section long enough for that claim to mean anything.
    if keys[0] not in keys[1:] and len(cells) >= 6:
        return 1
    return 0


def collapse_run_repeats(cells):
    """Normalise harmonic rhythm that pads every chord over several bars.

    ``C C G G Am Am B B`` is the same progression as ``C G Am B`` played twice as
    slow; keeping the padded form inflates the bar count, defeats deduplication
    and makes the entry look like a long phrase. Runs of identical bar cells are
    divided by their common factor, so every remaining bar carries one chord
    unless the phrase genuinely mixes harmonic rhythm.
    """
    from math import gcd

    if not cells:
        return list(cells)
    runs = []
    for cell in cells:
        key = cell_key(cell)
        if runs and runs[-1][0] == key:
            runs[-1][1] += 1
            runs[-1][2].append(cell)
        else:
            runs.append([key, 1, [cell]])
    if len(runs) == len(cells):
        return list(cells)
    factor = 0
    for _key, length, _cells in runs:
        factor = gcd(factor, length)
    if factor < 2:
        return list(cells)
    out = []
    for _key, length, members in runs:
        # Keep the written cell, durations included: the point of the collapse
        # is the bar count, not to erase how long each chord sounds.
        for _ in range(length // factor):
            out.append(members[0])
    return out
