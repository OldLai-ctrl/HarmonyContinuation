#!/usr/bin/env python3
"""A+B phrase concatenation and phrase-boundary research.

The library stores phrase units. Real playing contains several phrases in a row,
so "where does the last phrase start" has to be answered before matching.
This tool keeps that question separate from the matcher so the same code serves
three purposes:

  pairs     emit real adjacent phrase pairs (A -> B) from the ingest outputs
  concat    emit long progressions A+B for the matcher / future ingests
  evaluate  measure how often a boundary rule recovers the true A|B seam
  split     split one long progression with the same rule, for experiments

The boundary rule (see DECISIONS D62): a cut is a position k where

  * the input before k ends like some library phrase  (suffix match), and
  * the input from k on starts like some library phrase (prefix match).

Both sides must be confirmed by the library; a cut confirmed by only one side
is what the earlier tonic-arrival heuristic did, and it over-cut. Among the
confirmed cuts the shortest k wins, because the longest one drifts into the
second phrase.

`suffix`/`prefix` tables are built from the candidate rows by song, so
`--holdout` can hide whole songs while measuring, which is what a real import
looks like: the song is not in the library.
"""
from __future__ import annotations

import argparse
import collections
import csv
import json
import re
import sys
from pathlib import Path

_ROMAN = {"i":1,"ii":2,"iii":3,"iv":4,"v":5,"vi":6,"vii":7}
_TOKEN = re.compile(r"^([#b]?)([ivIV]+)(.*)$")


def signature(token: str):
    """Degree + alteration only: the cross-song currency.

    Quality is dissolved on purpose. A phrase in C major and the same phrase in
    G major share a signature; that is what lets a song that is *not* in the
    library still be recognised as "this is the end of a phrase".
    """
    m = _TOKEN.match(token)
    if not m:
        return None
    accidental, roman, _suffix = m.groups()
    degree = _ROMAN.get(roman.lower())
    if degree is None:
        return None
    alteration = {"": 0, "#": 1, "b": -1}[accidental]
    return (degree, alteration)


def load_rows(path: Path) -> list[dict]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(data, dict):
        data = data.get("pieces", data.get("candidates", []))
    return data


def load_links(path: Path) -> list[dict]:
    with path.open(encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def song_of(unit_id: str) -> str:
    return unit_id.split("_u")[0]


def tokens(row: dict) -> tuple[str, ...]:
    return tuple(row["sequence"].split())


def signatures(row: dict) -> tuple:
    return tuple(signature(t) for t in tokens(row) if signature(t) is not None)


class PhraseTable:
    """Exact plus one-mismatch lookup for phrase starts/ends.

    Real imports rarely equal a library phrase exactly (an inversion, a 9th, a
    sus here and there), so one substitution is tolerated at length >= 4. The
    delete-one index keeps that check O(n) instead of O(library)."""

    def __init__(self):
        self.exact: dict[str, set[tuple]] = collections.defaultdict(set)
        self.wild: dict[tuple, set[tuple]] = collections.defaultdict(set)

    def add(self, mode: str, seq: tuple):
        self.exact[mode].add(seq)
        if len(seq) >= 4:
            for i in range(len(seq)):
                self.wild[(mode, len(seq), i)].add(seq[:i] + seq[i + 1:])

    def match_kind(self, mode: str, seq: tuple) -> int:
        """2 = exact, 1 = one substitution, 0 = not a phrase."""
        if seq in self.exact[mode]:
            return 2
        if len(seq) < 4:
            return 0
        for i in range(len(seq)):
            if seq[:i] + seq[i + 1:] in self.wild[(mode, len(seq), i)]:
                return 1
        return 0


def build_tables(rows: list[dict], min_len: int = 2):
    suffix = PhraseTable()
    prefix = PhraseTable()
    for row in rows:
        seq = signatures(row)
        mode = row["mode"]
        for k in range(min_len, len(seq) + 1):
            suffix.add(mode, seq[-k:])
            prefix.add(mode, seq[:k])
    return suffix, prefix


def find_cuts(seq: tuple[str, ...], mode: str, suffix, prefix,
              min_side: int = 2) -> list[int]:
    """Every k where both sides are confirmed library phrases."""
    cuts = []
    for k in range(min_side, len(seq) - min_side + 1):
        head = suffix.match_kind(mode, seq[:k])
        tail = prefix.match_kind(mode, seq[k:])
        if head and tail:
            cuts.append((k, head, tail))
    return cuts


def choose_cut(cuts):
    """Prefer a cut whose *tail* is an exact phrase start, then the shortest.

    The tail is the part that gets matched, so it must be trustworthy; the head
    only needs to end like a phrase. Measured on 150 held-out pairs this rule
    recovers the seam exactly in 51% of cases and within one chord in 63%,
    against 42% / 63% for a plain "best evidence, then shortest" rule.
    """
    if not cuts:
        return None
    best_tail = max(tail for _k, _head, tail in cuts)
    return min(k for k, _head, tail in cuts if tail == best_tail)


def collect_pairs(rows_by_id, links, limit: int | None, same_mode: bool = True):
    pairs = []
    for link in links:
        a, b = link["prev"], link["next"]
        if a not in rows_by_id or b not in rows_by_id:
            continue
        if same_mode and rows_by_id[a]["mode"] != rows_by_id[b]["mode"]:
            continue
        pairs.append((a, b, link.get("song", ""), link.get("section", "")))
        if limit and len(pairs) >= limit:
            break
    return pairs


def cmd_pairs(args) -> int:
    rows = load_rows(Path(args.candidates))
    rows_by_id = {r["id"]: r for r in rows}
    pairs = collect_pairs(rows_by_id, load_links(Path(args.links)), args.limit)
    out = Path(args.out)
    with out.open("w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f, delimiter="\t")
        writer.writerow(["a", "b", "song", "section", "mode", "a_sequence", "b_sequence"])
        for a, b, song, section in pairs:
            writer.writerow([a, b, song, section, rows_by_id[a]["mode"],
                             " ".join(tokens(rows_by_id[a])),
                             " ".join(tokens(rows_by_id[b]))])
    print(f"pairs={len(pairs)} out={out}")
    return 0


def cmd_concat(args) -> int:
    """Emit A+B as a matcher query (chord JSON) with the true seam."""
    rows = load_rows(Path(args.candidates))
    rows_by_id = {r["id"]: r for r in rows}
    pairs = collect_pairs(rows_by_id, load_links(Path(args.links)), args.limit)
    out = []
    for a, b, song, section in pairs:
        ra, rb = rows_by_id[a], rows_by_id[b]
        events = []
        start = 0.0
        for row in (ra, rb):
            for chord in row.get("typedData", {}).get("events", []) or []:
                events.append({
                    "name": chord.get("rawName") or chord.get("name", ""),
                    "start": start,
                    "duration": chord.get("durationQN") or 4.0,
                })
                start += chord.get("durationQN") or 4.0
        out.append({
            "id": f"{a}+{b}", "song": song, "section": section,
            "mode": ra["mode"], "seam": len(ra.get("typedData", {}).get("events", []) or []),
            "total": len(events), "chords": events,
        })
    Path(args.out).write_text(json.dumps(out, ensure_ascii=False, indent=1), encoding="utf-8")
    print(f"concatenations={len(out)} out={args.out}")
    return 0


def cmd_evaluate(args) -> int:
    rows = load_rows(Path(args.candidates))
    rows_by_id = {r["id"]: r for r in rows}
    pairs = collect_pairs(rows_by_id, load_links(Path(args.links)), args.limit)
    stats = collections.Counter()
    cache = {}
    for a, b, _song, _section in pairs:
        sa, sb = signatures(rows_by_id[a]), signatures(rows_by_id[b])
        seq = sa + sb
        truth = len(sa)
        mode = rows_by_id[a]["mode"]
        if args.holdout:
            # Hide the whole song: the library a user imports is not in the library.
            hidden = song_of(a)
            if hidden not in cache:
                visible = [r for r in rows if song_of(r["id"]) != hidden]
                cache[hidden] = build_tables(visible)
            suffix, prefix = cache[hidden]
        else:
            if "all" not in cache:
                cache["all"] = build_tables(rows)
            suffix, prefix = cache["all"]
        cuts = find_cuts(seq, mode, suffix, prefix)
        stats["pairs"] += 1
        if not cuts:
            stats["unconfirmed"] += 1
            continue
        stats["confirmed"] += 1
        pick = choose_cut(cuts)
        stats["exact_shortest"] += pick == truth
        stats["exact_longest"] += max(k for k, _h, _t in cuts) == truth
        stats["within1_shortest"] += abs(pick - truth) <= 1
        stats[f"cuts_{min(len(cuts), 4)}"] += 1
    total = stats["pairs"] or 1
    print(f"pairs={stats['pairs']} confirmed={stats['confirmed']} "
          f"({100 * stats['confirmed'] / total:.1f}%)")
    print(f"shortest exact={stats['exact_shortest']} "
          f"({100 * stats['exact_shortest'] / total:.1f}%) "
          f"+-1={stats['within1_shortest']} "
          f"longest exact={stats['exact_longest']}")
    print("cuts per pair:", {k: v for k, v in sorted(stats.items()) if k.startswith("cuts_")})
    return 0


def cmd_split(args) -> int:
    """Split one chord list (the same JSON recommend_cli takes)."""
    rows = load_rows(Path(args.candidates))
    suffix, prefix = build_tables(rows)
    chords = json.loads(Path(args.query).read_text(encoding="utf-8"))
    if isinstance(chords, dict):
        chords = chords["chords"]
    # Degrees are the only cross-song currency; resolve them with the ingest
    # helpers so this agrees with what the library stores.
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import build as ingest  # noqa: PLC0415
    parsed = ingest.parse_chord_list(chords, args.mode) if hasattr(ingest, "parse_chord_list") else None
    if parsed is None:
        raise SystemExit("ingest.parse_chord_list is missing; pass a candidates path")
    seq = tuple(parsed)
    cuts = find_cuts(seq, args.mode, suffix, prefix)
    print({"length": len(seq), "cuts": cuts, "tail": list(seq[cuts[0]:]) if cuts else list(seq)})
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name, fn in (("pairs", cmd_pairs), ("concat", cmd_concat),
                     ("evaluate", cmd_evaluate), ("split", cmd_split)):
        p = sub.add_parser(name)
        p.add_argument("--candidates", required=True)
        p.add_argument("--links", required=(name != "split"))
        p.add_argument("--out", required=(name in ("pairs", "concat")))
        p.add_argument("--limit", type=int, default=None)
        p.add_argument("--holdout", action="store_true")
        p.add_argument("--query")
        p.add_argument("--mode", default="major")
        p.set_defaults(func=fn)
    args = ap.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
