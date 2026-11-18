"""Re-derive style weights for entries that still carry the template default.

The V7 editorial catalogue was shipped with ``pop:0.55,rock:0.35`` on every
entry that had no hand-written style.  Once styles became a graded, non
exclusive score (D49) those rows are the only ones left with a placeholder,
so this pass parses the stored ``sequence`` back into chords and runs the same
``style_affinity`` the ingest pipeline uses.

The library version has to move with the content: plugins that already have
the previous library installed compare version *and* content hash, so a fresh
database under an old version number is rejected as "same version, different
content".
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sqlite3
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "ingest"))

import build as B  # noqa: E402

_TOKEN = re.compile(r"^([#b]?)([ivIV]+)(.*)$")
QUALITIES = {
    "": "",
    "m": "m",
    "7": "7",
    "maj7": "maj7",
    "m7": "m7",
    "m7b5": "m7b5",
    "dim": "dim",
    "dim7": "dim7",
    "m9": "m9",
    "maj9": "maj9",
    "9": "9",
    "sus4": "sus4",
    "sus2": "sus2",
    "aug": "aug",
    "6": "6",
    "add9": "add9",
}


def _degree_of(roman: str) -> int | None:
    return B._DEGREE.get(roman.upper())


def _bass_offset(spec: str, scale: list[int]) -> int | None:
    """``/3`` is a scale-degree bass, ``/vi`` names the chord standing there."""
    if spec.isdigit():
        index = int(spec)
        if 1 <= index <= 7:
            return scale[index - 1]
        return None
    degree = _degree_of(spec.lstrip("#b"))
    if degree is None:
        return None
    return scale[degree - 1]


def events_from_sequence(sequence: str, mode: str) -> list[dict] | None:
    scale = B.SCALE[mode]
    events = []
    for token in sequence.split():
        match = _TOKEN.match(token)
        if not match:
            return None
        accidental, body, suffix = match.groups()
        degree = _degree_of(body)
        if degree is None:
            return None
        root = (scale[degree - 1] + (1 if accidental == "#" else -1 if accidental == "b" else 0)) % 12
        quality = suffix
        bass = root
        if "/" in suffix:
            quality, spec = suffix.split("/", 1)
            bass = _bass_offset(spec, scale)
            if bass is None:
                return None
        quality = QUALITIES.get(quality)
        if quality is None:
            return None
        events.append({"root": root, "quality": quality, "bass": bass})
    return events or None


def format_styles(styles: dict[str, float]) -> str:
    return ",".join(f"{name}:{value:g}" for name, value in sorted(styles.items()))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="compiled factory database to read")
    parser.add_argument("output", type=Path, help="database to write")
    parser.add_argument("--library-version", type=int, required=True)
    parser.add_argument("--default-styles", default="pop:0.55,rock:0.35")
    parser.add_argument("--base-styles", default="pop:0.55",
                        help="style floor kept while evidence-derived genres are added")
    parser.add_argument("--skip-prefix", default="BB_")
    args = parser.parse_args()

    shutil.copyfile(args.source, args.output)
    db = sqlite3.connect(args.output)
    rows = list(db.execute("SELECT id, payload FROM progressions"))
    relabelled, kept, unparsed = [], 0, []
    for row_id, payload in rows:
        data = json.loads(payload)
        if row_id.startswith(args.skip_prefix) or data.get("styles") != args.default_styles:
            kept += 1
            continue
        events = events_from_sequence(data.get("sequence", ""), data.get("mode", "major"))
        if events is None:
            unparsed.append(row_id)
            continue
        base = {}
        for item in args.base_styles.split(","):
            if item:
                name, value = item.split(":")
                base[name] = float(value)
        styles = B.style_affinity(B.features(events, 0, data.get("mode", "major")), {"styles": base})
        data["styles"] = format_styles(styles)
        db.execute("UPDATE progressions SET payload = ? WHERE id = ?", (json.dumps(data, ensure_ascii=False), row_id))
        relabelled.append((row_id, data["styles"]))

    db.execute("UPDATE metadata SET value = ? WHERE key = 'library_version'", (str(args.library_version),))
    db.commit()
    size = args.output.stat().st_size
    print(f"relabelled={len(relabelled)} kept={kept} unparsed={len(unparsed)} library_version={args.library_version} size={size}")
    for row_id, styles in relabelled[:8]:
        print(f"  {row_id}: {styles}")
    if unparsed:
        print("unparsed ids:", " ".join(unparsed[:20]))
    db.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
