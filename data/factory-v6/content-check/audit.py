#!/usr/bin/env python3
"""Deterministic read-only checks for the V6 content candidate bundle.

Reads only ../content/candidates.json and ../content/metadata.json.
Each candidate's major/minor mode selects the coordinate scale used to decode
its scale degrees; this is separate from metadata modal memberships.
Pitch masks are interpreted relative to each chord root, as specified by the
source project. The script makes no edits to either input file.
"""

from __future__ import annotations

import json
import re
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CANDIDATE_PATH = ROOT / "content" / "candidates.json"
METADATA_PATH = ROOT / "content" / "metadata.json"
PC = {
    "C": 0, "B#": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3,
    "Eb": 3, "E": 4, "Fb": 4, "E#": 5, "F": 5, "F#": 6,
    "Gb": 6, "G": 7, "G#": 8, "Ab": 8, "A": 9, "A#": 10,
    "Bb": 10, "B": 11, "Cb": 11,
}
PC_NAMES = ("C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B")
MAJOR_SCALE = (0, 2, 4, 5, 7, 9, 11)
MINOR_SCALE = (0, 2, 3, 5, 7, 8, 10)
COORDINATE_SCALES = {"major": MAJOR_SCALE, "minor": MINOR_SCALE}
MODES = {
    "ionian": (0, 2, 4, 5, 7, 9, 11),
    "dorian": (0, 2, 3, 5, 7, 9, 10),
    "phrygian": (0, 1, 3, 5, 7, 8, 10),
    "lydian": (0, 2, 4, 6, 7, 9, 11),
    "mixolydian": (0, 2, 4, 5, 7, 9, 10),
    "aeolian": (0, 2, 3, 5, 7, 8, 10),
    "harmonic_minor": (0, 2, 3, 5, 7, 8, 11),
    "melodic_minor_ascending": (0, 2, 3, 5, 7, 9, 11),
    "classical_melodic_minor": (0, 2, 3, 5, 7, 9, 11),
    "locrian": (0, 1, 3, 5, 6, 8, 10),
}


def parse_chord(label: str) -> tuple[int, set[int], int]:
    """Parse the supported chord labels into root, root-relative tones, bass."""
    head, slash, bass_label = label.partition("/")
    match = re.fullmatch(r"([A-G](?:#|b)?)(.*)", head)
    if not match or match.group(1) not in PC:
        raise ValueError(f"unsupported root in {label!r}")
    root = PC[match.group(1)]
    suffix = match.group(2)
    low = suffix.lower()

    if slash:
        bass_match = re.fullmatch(r"([A-G](?:#|b)?)", bass_label)
        if not bass_match or bass_match.group(1) not in PC:
            raise ValueError(f"unsupported slash bass in {label!r}")
        bass = PC[bass_match.group(1)]
    else:
        bass = root

    if low == "5":
        tones = {0, 7}
    elif low.startswith("mmaj"):
        tones = {0, 3, 7, 11}
        tail = low[4:]
    elif low.startswith("m7b5"):
        tones = {0, 3, 6, 10}
        tail = low[4:]
    elif low.startswith("dim7"):
        tones = {0, 3, 6, 9}
        tail = low[4:]
    elif low.startswith("dim"):
        tones = {0, 3, 6}
        tail = low[3:]
    elif low.startswith("aug"):
        tones = {0, 4, 8}
        tail = low[3:]
    elif "sus2" in low:
        tones = {0, 2, 7}
        tail = low.replace("sus2", "")
    elif "sus4" in low:
        tones = {0, 5, 7}
        tail = low.replace("sus4", "")
    elif low.startswith("maj"):
        tones = {0, 4, 7}
        tail = low[3:]
    elif low.startswith("m"):
        tones = {0, 3, 7}
        tail = low[1:]
    elif low.startswith("add9"):
        tones = {0, 2, 4, 7}
        tail = low[5:]
    else:
        tones = {0, 4, 7}
        tail = low

    if low != "5":
        family_minor_major = low.startswith("mmaj")
        family_minor = low.startswith("m") and not family_minor_major and not low.startswith("maj")
        family_sus = "sus2" in low or "sus4" in low
        family_major7 = low.startswith("maj") or family_minor_major
        is_add9 = low.startswith("add9")

        has_7 = "7" in tail or any(x in tail for x in ("9", "11", "13"))
        if has_7 and not is_add9:
            if family_major7:
                tones.add(11)
            elif family_minor_major:
                tones.add(11)
            else:
                tones.add(10)

        if re.search(r"(?<![#b])13", tail):
            tones.update((2, 9))
        elif re.search(r"(?<![#b])11", tail):
            tones.update((2, 5))
        elif re.search(r"(?<![#b])9", tail):
            tones.add(2)
        if "6" in tail:
            tones.add(9)

        if "b9" in tail:
            tones.add(1)
        if "#9" in tail:
            tones.add(3)
        if "#11" in tail:
            tones.add(6)
        if "b13" in tail:
            tones.add(8)

    return root, tones, bass


def pc_set(mask: int) -> set[int]:
    return {i for i in range(12) if mask & (1 << i)}


def source_event(event: dict, coordinate_mode: str) -> tuple[int, set[int], int]:
    degree = event.get("degree")
    if not isinstance(degree, list) or len(degree) != 2:
        raise ValueError(f"invalid degree {degree!r}")
    scale_degree, alteration = degree
    scale = COORDINATE_SCALES.get(coordinate_mode)
    if scale is None:
        raise ValueError(f"unsupported candidate mode {coordinate_mode!r}")
    if not isinstance(scale_degree, int) or not 1 <= scale_degree <= 7:
        raise ValueError(f"invalid scale degree {scale_degree!r}")
    root = (scale[scale_degree - 1] + alteration) % 12
    mask = int(event.get("intervalMask", 0)) | int(event.get("colorMask", 0))
    bass_interval = event.get("bassInterval")
    bass = root if bass_interval is None else (root + int(bass_interval)) % 12
    return root, pc_set(mask), bass


def event_signature(events: list[dict], include_duration: bool, coordinate_mode: str) -> tuple:
    result = []
    for event in events:
        root, tones, bass = source_event(event, coordinate_mode)
        item = (root, tuple(sorted(tones)), bass)
        if include_duration:
            item += (float(event["durationQN"]),)
        result.append(item)
    return tuple(result)


def main() -> None:
    candidates = json.loads(CANDIDATE_PATH.read_text(encoding="utf-8"))
    metadata = json.loads(METADATA_PATH.read_text(encoding="utf-8"))
    rows = metadata["candidates"]
    by_meta = {row["id"]: row for row in rows}

    problems: list[str] = []
    pitch_mismatches: list[str] = []
    duration_mismatches: list[str] = []
    events_by_id: dict[str, list[dict]] = {}
    modal_notes: list[str] = []
    duplicate_ids: list[str] = []

    ids = [row["id"] for row in candidates]
    if len(ids) != 60:
        problems.append(f"expected 60 candidate rows; found {len(ids)}")
    if len(set(ids)) != len(ids):
        duplicate_ids = sorted(i for i, n in Counter(ids).items() if n > 1)
        problems.append(f"duplicate IDs: {duplicate_ids}")
    if ids != [row["id"] for row in rows]:
        problems.append("metadata IDs/order do not match candidates.json")

    for candidate in candidates:
        cid = candidate["id"]
        coordinate_mode = candidate.get("mode", "")
        if coordinate_mode not in COORDINATE_SCALES:
            problems.append(f"{cid}: unsupported candidate mode {coordinate_mode!r}")
        meta = by_meta.get(cid)
        if meta is None:
            continue
        try:
            typed = json.loads(candidate["typedData"])
        except Exception as exc:
            problems.append(f"{cid}: invalid typedData: {exc}")
            continue
        events = typed.get("full", [])
        events_by_id[cid] = events
        labels = meta.get("chords", "").split()
        typed_durations = [float(event.get("durationQN", -1)) for event in events]
        metadata_durations = [float(value) for value in meta.get("durationQN", [])]
        rhythm_durations = [float(value) for value in candidate.get("rhythm", "").split()]
        if not (len(events) == len(labels) == len(metadata_durations) == len(rhythm_durations)
                == int(candidate.get("phraseLength", -1))):
            duration_mismatches.append(
                f"{cid}: event/chord/duration/rhythm/phraseLength counts "
                f"{len(events)}/{len(labels)}/{len(metadata_durations)}/"
                f"{len(rhythm_durations)}/{candidate.get('phraseLength')}"
            )
        if typed_durations != metadata_durations or typed_durations != rhythm_durations:
            duration_mismatches.append(
                f"{cid}: typedData={typed_durations}, metadata={metadata_durations}, "
                f"rhythm={rhythm_durations}"
            )
        if candidate.get("meter") != meta.get("meter"):
            duration_mismatches.append(
                f"{cid}: meter differs: candidate={candidate.get('meter')}, metadata={meta.get('meter')}"
            )

        for index, (event, label) in enumerate(zip(events, labels), start=1):
            try:
                expected_root, expected_tones, expected_bass = parse_chord(label)
                actual_root, actual_tones, actual_bass = source_event(event, coordinate_mode)
            except Exception as exc:
                pitch_mismatches.append(f"{cid} chord {index} {label}: parse error: {exc}")
                continue
            mismatch = []
            if actual_root != expected_root:
                mismatch.append(f"root {PC_NAMES[actual_root]} != {PC_NAMES[expected_root]}")
            if actual_bass != expected_bass:
                mismatch.append(f"bass {PC_NAMES[actual_bass]} != {PC_NAMES[expected_bass]}")
            if actual_tones != expected_tones:
                actual = ",".join(PC_NAMES[x] for x in sorted(actual_tones))
                expected = ",".join(PC_NAMES[x] for x in sorted(expected_tones))
                mismatch.append(f"tones {{{actual}}} != notation {{{expected}}}")
            if mismatch:
                pitch_mismatches.append(f"{cid} chord {index} {label}: " + "; ".join(mismatch))

        for membership in meta.get("memberships", []):
            mode = membership.get("mode", "")
            if mode not in MODES:
                modal_notes.append(f"{cid}: unrecognized mode label {mode!r}")
                continue
            allowed = set(MODES[mode])
            all_tones = set().union(*(
                {(root + interval) % 12 for interval in tones} | {root}
                for root, tones, _bass in (source_event(event, coordinate_mode) for event in events)
            ))
            outside = sorted(all_tones - allowed)
            if outside:
                modal_notes.append(
                    f"{cid} {mode}: strict-set extras " + ",".join(PC_NAMES[n] for n in outside)
                )

    duplicates = []
    time_only = []
    related = []
    for i, left in enumerate(candidates):
        left_events = events_by_id.get(left["id"], [])
        for right in candidates[i + 1:]:
            right_events = events_by_id.get(right["id"], [])
            if not left_events or not right_events:
                continue
            left_mode = left.get("mode", "")
            right_mode = right.get("mode", "")
            if event_signature(left_events, True, left_mode) == event_signature(right_events, True, right_mode):
                duplicates.append((left["id"], right["id"]))
            elif event_signature(left_events, False, left_mode) == event_signature(right_events, False, right_mode):
                time_only.append((left["id"], right["id"]))
            if len(left_events) == len(right_events) and len(left_events) >= 4:
                left_bass = [source_event(e, left_mode)[2] for e in left_events]
                right_bass = [source_event(e, right_mode)[2] for e in right_events]
                left_roots = [source_event(e, left_mode)[0] for e in left_events]
                right_roots = [source_event(e, right_mode)[0] for e in right_events]
                same_bass = left_bass == right_bass
                root_matches = sum(a == b for a, b in zip(left_roots, right_roots))
                if same_bass and root_matches >= len(left_events) - 1 and (left["id"], right["id"]) not in duplicates:
                    related.append((left["id"], right["id"], root_matches, len(left_events)))

    # Prefix-related bass contours catch a phrase that adds a closing tonic event.
    bass_prefix = []
    for i, left in enumerate(candidates):
        le = events_by_id.get(left["id"], [])
        if not le:
            continue
        for right in candidates[i + 1:]:
            revents = events_by_id.get(right["id"], [])
            if not revents or len(le) == len(revents):
                continue
            left_bass = [source_event(e, left.get("mode", ""))[2] for e in le]
            right_bass = [source_event(e, right.get("mode", ""))[2] for e in revents]
            short_bass, long_bass = (left_bass, right_bass) if len(le) < len(revents) else (right_bass, left_bass)
            if short_bass == long_bass[:len(short_bass)] and len(short_bass) >= 5:
                bass_prefix.append((left["id"], right["id"], len(short_bass), len(long_bass)))

    print(f"candidate_count={len(candidates)} metadata_count={len(rows)} id_order_aligned={ids == [row['id'] for row in rows]}")
    print(f"candidate_coordinate_mode_counts={dict(Counter(row.get('mode') for row in candidates))}")
    print(f"pitch_bass_notation_mismatches={len(pitch_mismatches)}")
    for problem in pitch_mismatches:
        print("PITCH: " + problem)
    print(f"duration_rhythm_mismatches={len(duration_mismatches)}")
    for problem in duration_mismatches:
        print("RHYTHM: " + problem)
    print(f"strict_mode_pitch_set_exceptions={len(modal_notes)}")
    for note in modal_notes:
        print("MODE: " + note)
    print(f"exact_duplicates={duplicates}")
    print(f"same_harmony_different_rhythm={time_only}")
    print(f"same_length_same_bass_near_root_outline={related}")
    print(f"bass_prefix_related_phrases={bass_prefix}")
    for problem in problems:
        print("STRUCTURE: " + problem)


if __name__ == "__main__":
    main()
