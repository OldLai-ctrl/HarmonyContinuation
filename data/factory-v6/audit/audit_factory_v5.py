"""Read-only, reproducible Factory V5 / G2 v2 audit for V6 B1.

The SQLite source is opened with mode=ro and PRAGMA query_only. This script
writes only the requested audit directory.
"""
from __future__ import annotations

import argparse
import collections
import csv
import hashlib
import json
import math
import pathlib
import re
import shlex
import sqlite3
from typing import Any

ROOT = pathlib.Path(r"D:\VibeCoding")
DEFAULT_DB = ROOT / "HarmonyContinuation-g04r-delivery-20261024/stage-5/official-factory/factory-v5.db"
DEFAULT_ANN = ROOT / "HarmonyContinuation-release/data/mode-annotations/v2/factory-v5.hca"
DEFAULT_CANDIDATES = ROOT / "HarmonyContinuation-v6-evidence/content/candidates.json"
DEFAULT_METADATA = ROOT / "HarmonyContinuation-v6-evidence/content/metadata.json"
EXPECTED_DB_SHA256 = "F989A83EA9EF92BA7C272BF0E5E3C7073C225899DC81D60DD46C10EA6B48CB3C"
EXPECTED_BASELINE = "2421c6f"

# ChordQuality enum and ChordNormalizer.cpp essential() pitch masks.
QUALITY_MASK = {
    0: 0x000, 1: 0x091, 2: 0x089, 3: 0x491, 4: 0x891, 5: 0x489,
    6: 0x049, 7: 0x249, 8: 0x449, 9: 0x085, 10: 0x0A1, 11: 0x111,
}
QUALITY_NAME = {
    0: "unknown", 1: "major", 2: "minor", 3: "dominant7", 4: "major7",
    5: "minor7", 6: "diminished", 7: "diminished7", 8: "half-diminished7",
    9: "sus2", 10: "sus4", 11: "augmented",
}
SOURCE_SCALES = {
    "major": (0, 2, 4, 5, 7, 9, 11),
    "minor": (0, 2, 3, 5, 7, 8, 10),
}
MODE_SCALES = {
    "ionian": (0, 2, 4, 5, 7, 9, 11),
    "dorian": (0, 2, 3, 5, 7, 9, 10),
    "phrygian": (0, 1, 3, 5, 7, 8, 10),
    "lydian": (0, 2, 4, 6, 7, 9, 11),
    "mixolydian": (0, 2, 4, 5, 7, 9, 10),
    "aeolian": (0, 2, 3, 5, 7, 8, 10),
    "locrian": (0, 1, 3, 5, 6, 8, 10),
    "harmonic_minor": (0, 2, 3, 5, 7, 8, 11),
    "melodic_minor_ascending": (0, 2, 3, 5, 7, 9, 11),
    "classical_melodic_minor": None,
}
TONIC_PC = {
    "C": 0, "B#": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3,
    "E": 4, "Fb": 4, "E#": 5, "F": 5, "F#": 6, "Gb": 6, "G": 7,
    "G#": 8, "Ab": 8, "A": 9, "A#": 10, "Bb": 10, "B": 11, "Cb": 11,
}
REVIEW_TEXT_MARKERS = re.compile(
    r"borrow|mixture|secondary|chromatic|borrowed|借用|混合|副属|副导|拿坡里|半音|变音",
    re.IGNORECASE,
)


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def read_json(path: pathlib.Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def typed_data(row: dict[str, Any]) -> dict[str, Any]:
    value = row.get("typedData", {})
    return json.loads(value) if isinstance(value, str) else value


def cpp_hex(value: float) -> str:
    """Match std::hexfloat formatting used by the source fingerprint."""
    mantissa, exponent = float(value).hex().split("p")
    whole, fraction = mantissa.split(".")
    return whole + "." + (fraction + "0" * 13)[:13] + "p" + exponent


def classify_mask(mask: int) -> int:
    bit = lambda n: bool(mask & (1 << n))
    if bit(0) and bit(4) and bit(11):
        return 4
    if bit(0) and bit(4) and bit(10):
        return 3
    if not bit(6) and bit(0) and bit(3) and bit(10):
        return 5
    if bit(0) and bit(3) and bit(6):
        return 7 if bit(9) else 8 if bit(10) else 6
    if bit(0) and bit(4) and bit(8):
        return 11
    if bit(0) and bit(4) and bit(7):
        return 4 if bit(11) else 3 if bit(10) else 1
    if bit(0) and bit(3) and bit(7):
        return 5 if bit(10) else 2
    if bit(0) and bit(2) and bit(7):
        return 9
    if bit(0) and bit(5) and bit(7):
        return 10
    return 0


def named_color(suffix: str) -> int:
    lower = suffix.lower()
    if "6/9" in lower:
        return (1 << 9) | (1 << 2)
    if "b9" in lower:
        return 1 << 1
    if "#9" in lower:
        return 1 << 3
    if "13" in lower or lower == "6":
        return 1 << 9
    if "9" in lower:
        return 1 << 2
    return 0


def actual_sequence(row: dict[str, Any], tonic: int = 0) -> list[dict[str, Any]]:
    """Mirror template realization and ChordPitchSet normalization for Factory data."""
    mode = str(row.get("mode", "")).lower()
    scale = SOURCE_SCALES.get(mode)
    if scale is None:
        raise ValueError(f"unsupported source mode {mode!r} for {row.get('id')}")
    result = []
    cursor = 0.0
    for index, event in enumerate(typed_data(row).get("full", [])):
        degree = event.get("degree")
        if not degree or not 1 <= int(degree[0]) <= 7:
            raise ValueError(f"missing/invalid degree: {row.get('id')}[{index}]")
        root = (tonic + scale[int(degree[0]) - 1] + int(degree[1])) % 12
        quality = int(event.get("quality", 0))
        interval_mask = int(event.get("intervalMask") or 0)
        color_field = int(event.get("colorMask") or 0)
        suffix = str(event.get("suffix") or "")
        if interval_mask or color_field:
            raw = (interval_mask if interval_mask else QUALITY_MASK.get(quality, 0)) | color_field
            actual_quality = classify_mask(raw) or quality
            intervals = raw
            color = raw & ~QUALITY_MASK.get(actual_quality, 0)
            exact = True
        else:
            actual_quality = quality
            intervals = QUALITY_MASK.get(actual_quality, 0) | named_color(suffix)
            color = intervals & ~QUALITY_MASK.get(actual_quality, 0)
            exact = False
        bass_interval = int(event.get("bassInterval") or 0)
        duration = float(event["durationQN"]) if event.get("durationQN") is not None else 4.0
        if not math.isfinite(duration) or duration <= 0:
            raise ValueError(f"invalid duration: {row.get('id')}[{index}]")
        pitches = tuple(
            sorted({root} | {((root + bit) % 12) for bit in range(12) if (intervals | color) & (1 << bit)})
        )
        result.append({
            "index": index,
            "root": root,
            "bass": (root + bass_interval) % 12,
            "quality": actual_quality,
            "intervals": intervals,
            "color": color,
            "exact": exact,
            "start": cursor,
            "duration": duration,
            "pitchClasses": pitches,
            "sourceRoles": int(event.get("roles") or 0),
            "degree": degree,
            "suffix": suffix,
        })
        cursor += duration
    return result


def canonical_projection(row: dict[str, Any]) -> str:
    mode_value = 0 if str(row.get("mode")).lower() == "major" else 1
    numerator, denominator = (int(v) for v in str(row.get("meter", "4/4")).split("/"))
    out = f"{mode_value}:{numerator}/{denominator}"
    for event in typed_data(row).get("full", []):
        degree = event.get("degree")
        target = event.get("target")
        degree_text = f"{degree[0]},{degree[1]}" if degree else "-"
        target_text = f"{target[0]},{target[1]}" if target else "-"
        quality = int(event.get("quality", 0))
        interval_mask = int(event.get("intervalMask") or 0)
        base = interval_mask if interval_mask else QUALITY_MASK.get(quality, 0)
        color = int(event.get("colorMask") or 0)
        borrowed = 64 if int(event.get("roles") or 0) & 64 else 0
        bass_interval = int(event.get("bassInterval") or 0)
        duration = event.get("durationQN")
        duration_text = "open" if duration is None else cpp_hex(float(duration))
        out += (
            f"|{degree_text}:{quality}:{target_text}:{borrowed}:{bass_interval}:"
            f"{base | color}:{color}:{duration_text}"
        )
    return out


def runtime_fingerprint(row: dict[str, Any], tonic: int = 0) -> str:
    numerator, denominator = (int(v) for v in str(row.get("meter", "4/4")).split("/"))
    events = actual_sequence(row, tonic)
    if not events:
        return ""
    pieces = [f"performance-v1:{numerator}/{denominator}"]
    for event in events:
        pieces.append(
            f"|{event['root']}:{event['bass']}:{event['quality']}:"
            f"{event['intervals']}:{event['color']}:{int(event['exact'])}"
            f"@{cpp_hex(event['start'])}+{cpp_hex(event['duration'])}^-1"
        )
    return "".join(pieces)


def source_fingerprint(row: dict[str, Any]) -> str:
    projection = canonical_projection(row)
    performance = runtime_fingerprint(row, 0)
    return f"source-v1:{len(projection)}:{projection}:{performance}"


def actual_pitch_interval_mask(event: dict[str, Any]) -> int:
    """Normalize the realized pitch-class set relative to its chord root."""
    mask = 0
    for pitch in event["pitchClasses"]:
        mask |= 1 << ((pitch - event["root"]) % 12)
    return mask


def signature(row: dict[str, Any], *, kind: str, tonic: int = 0) -> str:
    events = actual_sequence(row, tonic)
    if not events:
        return ""
    anchor = events[0]["root"]
    parts: list[Any] = []
    if kind == "full":
        parts.append(str(row.get("meter", "4/4")))
    for event in events:
        token: list[Any] = [
            (event["root"] - anchor) % 12,
            (event["bass"] - anchor) % 12,
            actual_pitch_interval_mask(event),
        ]
        if kind == "full":
            token.extend([cpp_hex(event["start"]), cpp_hex(event["duration"])])
        elif kind == "runtime":
            token.extend([
                event["quality"], event["intervals"], event["color"], int(event["exact"]),
                cpp_hex(event["start"]), cpp_hex(event["duration"]),
            ])
        elif kind == "core":
            quality = event["quality"]
            if quality == 4:
                quality = 1
            elif quality == 5:
                quality = 2
            token = [token[0], token[1], quality]
        parts.append(tuple(token))
    return repr(parts)


def family_signature(row: dict[str, Any], tonic: int = 0) -> str:
    # Exact WishSampler::familyIdentity: relative root, relative bass, with
    # major7/major and minor7/minor collapsed. It is a diversity bucket only.
    return signature(row, kind="core", tonic=tonic)


def mode_mask(mode: str, tonic: int) -> set[int] | None:
    intervals = MODE_SCALES.get(mode)
    if intervals is None:
        return None
    return {(tonic + interval) % 12 for interval in intervals}


def membership_pitch_audit(row: dict[str, Any], membership: dict[str, Any]) -> dict[str, Any]:
    center = int(membership["tonicOffset"]) % 12
    scale = mode_mask(str(membership["mode"]), center)
    events = actual_sequence(row, 0)
    borrowed_indices = [e["index"] for e in events if e["sourceRoles"] & 64]
    altered_indices = [
        e["index"] for e in events if e["degree"] and int(e["degree"][1]) != 0
    ]
    outside: set[int] = set()
    outside_by_event: list[dict[str, Any]] = []
    if scale is not None:
        for event in events:
            notes = set(event["pitchClasses"])
            if event["root"] not in scale:
                notes.add(event["root"])
            if event["bass"] not in scale:
                notes.add(event["bass"])
            extra = sorted(notes - scale)
            if extra:
                outside.update(extra)
                outside_by_event.append({"index": event["index"], "pitchClasses": extra})
    strict = scale is not None and not outside and bool(events)
    evidence = str(membership.get("musicalEvidence", ""))
    explicit_wording = bool(REVIEW_TEXT_MARKERS.search(evidence))
    review = str(membership.get("review", ""))
    if review != "reviewed":
        evidence_class = "unconfirmed"
    elif strict:
        evidence_class = "reviewed_strictly_compatible"
    elif borrowed_indices or altered_indices or explicit_wording:
        evidence_class = "reviewed_nonstrict_with_borrow_or_chromatic_marker"
    else:
        evidence_class = "reviewed_nonstrict_without_explicit_marker"
    roots = [event["root"] for event in events]
    center_pc = center
    scale_degree_by_interval = (
        {interval: index + 1 for index, interval in enumerate(MODE_SCALES[str(membership["mode"])])}
        if scale is not None else {}
    )
    scale_degree_root_events: dict[str, list[int]] = {str(degree): [] for degree in range(1, 8)}
    non_diatonic_root_events = []
    degree5_dominant_seventh_events = []
    root_degrees = []
    for event in events:
        degree = scale_degree_by_interval.get((event["root"] - center_pc) % 12)
        root_degrees.append(degree)
        if degree is None:
            non_diatonic_root_events.append(event["index"])
        else:
            scale_degree_root_events[str(degree)].append(event["index"])
        if degree == 5 and event["quality"] == 3:
            degree5_dominant_seventh_events.append(event["index"])
    return {
        "centerPitchClassAtReferenceC": center_pc,
        "rootPitchClassesAtReferenceC": roots,
        "rootSemitonesFromCenter": [(root - center_pc) % 12 for root in roots],
        "tonicRootEventIndices": [i for i, root in enumerate(roots) if root == center_pc],
        "startsOrEndsOnTonicRoot": bool(roots) and (roots[0] == center_pc or roots[-1] == center_pc),
        "derivedMechanismEvidence": {
            "basis": "Actual realized chord roots, tones and bass; descriptive only, independent of stored techniques and not an assignment.",
            "rootScaleDegreeByEvent": root_degrees,
            "rootEventIndicesByScaleDegree": scale_degree_root_events,
            "nonDiatonicRootEventIndices": non_diatonic_root_events,
            "degree5DominantSeventhEventIndices": degree5_dominant_seventh_events,
            "bassSemitonesFromChordRootByEvent": [
                (event["bass"] - event["root"]) % 12 for event in events
            ],
            "bassPitchClassesAtReferenceC": [event["bass"] for event in events],
            "pitchIntervalsFromRootByEvent": [actual_pitch_interval_mask(event) for event in events],
            "chordQualityByEvent": [QUALITY_NAME.get(event["quality"], str(event["quality"])) for event in events],
        },
        "strictActualPitchCompatible": strict,
        "outsideScalePitchClassesAtReferenceC": sorted(outside),
        "outsideScaleEvents": outside_by_event,
        "borrowedRoleEventIndices": borrowed_indices,
        "alteredDegreeEventIndices": altered_indices,
        "evidenceTextHasBorrowOrChromaticMarker": explicit_wording,
        "evidenceClass": evidence_class,
    }


def read_annotations(path: pathlib.Path) -> dict[str, list[dict[str, Any]]]:
    lines = path.read_text(encoding="utf-8-sig").splitlines()
    if not lines or lines[0].strip() != "HC_MODE_ANNOTATIONS 1":
        raise ValueError("unsupported annotation header")
    result: dict[str, list[dict[str, Any]]] = collections.defaultdict(list)
    for line_no, line in enumerate(lines[1:], start=2):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        fields = shlex.split(line, posix=True)
        if len(fields) != 10:
            raise ValueError(f"annotation row {line_no} has {len(fields)} fields")
        row = {
            "tonicOffset": int(fields[1]),
            "sourceFingerprint": fields[2],
            "mode": fields[3],
            "usage": fields[4],
            "usageBasis": fields[5],
            "musicalEvidence": fields[6],
            "review": fields[7],
            "reviewer": fields[8],
            "melodicContext": fields[9],
            "line": line_no,
        }
        if not 0 <= row["tonicOffset"] <= 11:
            raise ValueError(f"annotation row {line_no} tonic offset outside 0..11")
        result[fields[0]].append(row)
    return dict(result)


def rhythm_audit(row: dict[str, Any]) -> dict[str, Any]:
    declared_text = str(row.get("rhythm", "")).strip()
    try:
        declared = [float(token) for token in declared_text.split()]
        actual = [
            float(event["durationQN"])
            for event in typed_data(row).get("full", [])
            if event.get("durationQN") is not None
        ]
        open_count = sum(event.get("durationQN") is None for event in typed_data(row).get("full", []))
        matches = (
            open_count == 0 and len(declared) == len(actual) and
            all(math.isclose(a, b, rel_tol=0, abs_tol=1e-9) for a, b in zip(declared, actual))
        )
    except (ValueError, TypeError):
        declared, actual, open_count, matches = [], [], 0, False
    return {
        "declaredRhythm": declared_text,
        "actualDurationsQN": actual,
        "openDurationCount": open_count,
        "rhythmMatchesFullEvents": matches,
    }


def parse_styles(value: Any) -> tuple[list[dict[str, Any]], list[str]]:
    text = str(value or "")
    tokens: list[dict[str, Any]] = []
    errors: list[str] = []
    for entry in text.split(",") if text else []:
        try:
            name, weight_text = entry.split(":", 1)
            weight = float(weight_text)
            if not name or not math.isfinite(weight) or not 0 <= weight <= 1:
                errors.append(entry)
            tokens.append({"name": name, "weight": weight})
        except (ValueError, TypeError):
            errors.append(entry)
    if not tokens:
        errors.append("empty")
    if len({t["name"] for t in tokens}) != len(tokens):
        errors.append("duplicate-style")
    return tokens, errors


def group_signatures(rows: dict[str, dict[str, Any]], kind: str) -> dict[str, list[str]]:
    groups: dict[str, list[str]] = collections.defaultdict(list)
    for row_id, row in rows.items():
        groups[signature(row, kind=kind)].append(row_id)
    return {key: sorted(value) for key, value in groups.items()}


def duplicate_groups(groups: dict[str, list[str]]) -> list[list[str]]:
    return sorted(
        (ids for ids in groups.values() if len(ids) > 1),
        key=lambda ids: (ids[0], len(ids), ids),
    )


def core_groups(rows: dict[str, dict[str, Any]]) -> dict[str, list[str]]:
    groups: dict[str, list[str]] = collections.defaultdict(list)
    for row_id, row in rows.items():
        groups[family_signature(row)].append(row_id)
    return {key: sorted(value) for key, value in groups.items()}


def hamming_edit_distance(left: list[tuple[int, int, int]], right: list[tuple[int, int, int]]) -> int:
    previous = list(range(len(right) + 1))
    for i, a in enumerate(left, 1):
        current = [i]
        for j, b in enumerate(right, 1):
            current.append(min(current[-1] + 1, previous[j] + 1, previous[j - 1] + (a != b)))
        previous = current
    return previous[-1]


def harmonic_tokens(row: dict[str, Any], tonic: int) -> list[tuple[int, int, int]]:
    return [
        (event["root"], event["bass"], actual_pitch_interval_mask(event))
        for event in actual_sequence(row, tonic)
    ]


def transposition_min_edit(left: list[tuple[int, int, int]], right: list[tuple[int, int, int]]) -> int:
    best = max(len(left), len(right))
    for shift in range(12):
        shifted = [((root + shift) % 12, (bass + shift) % 12, tones) for root, bass, tones in right]
        best = min(best, hamming_edit_distance(left, shifted))
        if best == 0:
            break
    return best


def candidate_comparison(
    candidates: list[dict[str, Any]],
    metadata: dict[str, Any],
    v5: dict[str, dict[str, Any]],
) -> dict[str, Any]:
    metadata_by_id = {item["id"]: item for item in metadata.get("candidates", [])}
    if {c["id"] for c in candidates} != set(metadata_by_id):
        raise ValueError("candidate JSON and metadata IDs differ")
    v5_full = group_signatures(v5, "full")
    v5_rhythmless = group_signatures(v5, "rhythmless")
    v5_core = core_groups(v5)
    v5_full_lookup = {member: ids for ids in v5_full.values() for member in ids}
    v5_rhythm_lookup = {member: ids for ids in v5_rhythmless.values() for member in ids}
    v5_core_lookup = {member: ids for ids in v5_core.values() for member in ids}
    v5_tokens = {row_id: harmonic_tokens(row, 0) for row_id, row in v5.items()}
    candidate_rows = {row["id"]: row for row in candidates}
    cand_full = group_signatures(candidate_rows, "full")
    cand_rhythmless = group_signatures(candidate_rows, "rhythmless")
    cand_core = core_groups(candidate_rows)
    cand_full_lookup = {member: ids for ids in cand_full.values() for member in ids}
    cand_rhythm_lookup = {member: ids for ids in cand_rhythmless.values() for member in ids}
    cand_core_lookup = {member: ids for ids in cand_core.values() for member in ids}
    records = []
    for candidate in sorted(candidates, key=lambda c: c["id"]):
        row_id = candidate["id"]
        item_meta = metadata_by_id[row_id]
        ref_name = str(item_meta.get("referenceTonic", "C"))
        if ref_name not in TONIC_PC:
            raise ValueError(f"unknown candidate reference tonic {ref_name!r} for {row_id}")
        ref_pc = TONIC_PC[ref_name]
        full_sig = signature(candidate, kind="full", tonic=ref_pc)
        rhythm_sig = signature(candidate, kind="rhythmless", tonic=ref_pc)
        core_sig = family_signature(candidate, tonic=ref_pc)
        exact_full = sorted(v5_full.get(full_sig, []))
        exact_rhythmless = sorted(v5_rhythmless.get(rhythm_sig, []))
        core_matches = sorted(v5_core.get(core_sig, []))
        cand_harmonic = harmonic_tokens(candidate, ref_pc)
        distances = [
            (transposition_min_edit(cand_harmonic, tokens), source_id)
            for source_id, tokens in v5_tokens.items()
        ]
        nearest = min((d for d, _ in distances), default=None)
        one_edit = sorted(source_id for d, source_id in distances if d == 1)
        memberships = []
        for membership in item_meta.get("memberships", []):
            copy = dict(membership)
            copy["strictPitchAudit"] = membership_pitch_audit(
                {**candidate, "mode": candidate.get("mode", "major")}, copy
            )
            memberships.append(copy)
        records.append({
            "id": row_id,
            "referenceTonic": ref_name,
            "status": item_meta.get("status", ""),
            "auditionStatus": item_meta.get("auditionStatus", ""),
            "candidateFileIntent": candidate.get("intent", ""),
            "candidateFileStyles": candidate.get("styles", ""),
            "candidateFileTechniques": candidate.get("techniques", ""),
            "candidateFileRhythm": candidate.get("rhythm", ""),
            "memberships": memberships,
            "strictReviewedMemberships": sum(
                m.get("review") == "reviewed" and m["strictPitchAudit"]["strictActualPitchCompatible"]
                for m in memberships
            ),
            "reviewedMemberships": sum(m.get("review") == "reviewed" for m in memberships),
            "completeSoundMatchesV5": exact_full,
            "rhythmlessSoundMatchesV5": exact_rhythmless,
            "coreConnectionFamilyMatchesV5": core_matches,
            "minimumHarmonicEventEditDistanceToV5": nearest,
            "oneChordEditNeighborsV5": one_edit,
            "otherCompleteSoundMatchesInV6": sorted(
                set(cand_full_lookup.get(row_id, [])) - {row_id}
            ),
            "otherRhythmlessMatchesInV6": sorted(
                set(cand_rhythm_lookup.get(row_id, [])) - {row_id}
            ),
            "otherCoreFamilyMembersInV6": sorted(
                set(cand_core_lookup.get(row_id, [])) - {row_id}
            ),
        })
    return {
        "candidateCount": len(candidates),
        "candidateJsonSha256": "",
        "candidateMetadataSha256": "",
        "candidateExactCompleteDuplicateGroups": duplicate_groups(cand_full),
        "candidateRhythmlessDuplicateGroups": duplicate_groups(cand_rhythmless),
        "candidateCoreFamilyGroups": duplicate_groups(cand_core),
        "records": records,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--db", type=pathlib.Path, default=DEFAULT_DB)
    parser.add_argument("--annotations", type=pathlib.Path, default=DEFAULT_ANN)
    parser.add_argument("--candidates", type=pathlib.Path, default=DEFAULT_CANDIDATES)
    parser.add_argument("--metadata", type=pathlib.Path, default=DEFAULT_METADATA)
    parser.add_argument("--baseline", default=EXPECTED_BASELINE)
    args = parser.parse_args()
    out_dir = pathlib.Path(__file__).resolve().parent

    db_hash = sha256(args.db)
    if db_hash != EXPECTED_DB_SHA256:
        raise SystemExit(f"DB SHA mismatch: expected {EXPECTED_DB_SHA256}, got {db_hash}")
    annotation_hash = sha256(args.annotations)
    candidate_hash = sha256(args.candidates)
    metadata_hash = sha256(args.metadata)

    uri = "file:" + args.db.resolve().as_posix() + "?mode=ro"
    connection = sqlite3.connect(uri, uri=True)
    connection.execute("PRAGMA query_only=ON")
    database_meta = dict(connection.execute("SELECT key,value FROM metadata"))
    db_rows: dict[str, dict[str, Any]] = {}
    for db_id, payload in connection.execute("SELECT id,payload FROM progressions ORDER BY id"):
        row = json.loads(payload)
        if db_id != row.get("id"):
            raise ValueError(f"SQL ID and payload ID differ: {db_id}")
        if db_id in db_rows:
            raise ValueError(f"duplicate factory ID: {db_id}")
        db_rows[db_id] = row
    connection.close()
    annotations = read_annotations(args.annotations)
    candidates = read_json(args.candidates)
    candidate_metadata = read_json(args.metadata)
    if not isinstance(candidates, list) or not isinstance(candidate_metadata, dict):
        raise ValueError("candidate source format changed")

    # Fingerprint and annotation integrity.
    ann_fingerprints: dict[str, set[str]] = {
        row_id: {m["sourceFingerprint"] for m in memberships}
        for row_id, memberships in annotations.items()
    }
    fp_computed = {row_id: source_fingerprint(row) for row_id, row in db_rows.items()}
    fp_match = {
        row_id: len(ann_fingerprints.get(row_id, set())) == 1 and
                fp_computed[row_id] in ann_fingerprints[row_id]
        for row_id in db_rows
    }
    if sum(fp_match.values()) != len(db_rows):
        # Keep the report reproducible and explicit; mismatches are not silently hidden.
        pass

    # Source metadata and music checks.
    metadata_records = {}
    style_names: collections.Counter[str] = collections.Counter()
    style_count = collections.Counter()
    technique_names: collections.Counter[str] = collections.Counter()
    intents: collections.Counter[str] = collections.Counter()
    cadences: collections.Counter[str] = collections.Counter()
    rhythms: collections.Counter[str] = collections.Counter()
    meters: collections.Counter[str] = collections.Counter()
    rhythm_mismatches = []
    missing_techniques = []
    style_errors = []
    technique_mismatches = []
    intent_cadence_checks = []
    for row_id, row in db_rows.items():
        typed = typed_data(row)
        styles, style_issues = parse_styles(row.get("styles"))
        style_count[len(styles)] += 1
        for item in styles:
            style_names[item["name"]] += 1
        if style_issues:
            style_errors.append({"id": row_id, "issues": style_issues, "value": row.get("styles", "")})
        techniques = [value.strip() for value in str(row.get("techniques", "")).split(",") if value.strip()]
        typed_techniques = [str(value).strip() for value in typed.get("techniques", []) if str(value).strip()]
        if techniques != typed_techniques:
            technique_mismatches.append({
                "id": row_id, "db": techniques, "typedData": typed_techniques,
            })
        for value in techniques:
            technique_names[value] += 1
        if not techniques:
            missing_techniques.append(row_id)
        intent = str(row.get("intent", ""))
        intents[intent] += 1
        cadence = str(row.get("cadence", ""))
        cadences[cadence] += 1
        rhythms[str(row.get("rhythm", ""))] += 1
        meters[str(row.get("meter", ""))] += 1
        rhythm_result = rhythm_audit(row)
        if not rhythm_result["rhythmMatchesFullEvents"]:
            rhythm_mismatches.append({"id": row_id, **rhythm_result})
        if (intent == "loop") != (cadence == "loop_closure"):
            intent_cadence_checks.append({
                "id": row_id, "intent": intent, "cadence": cadence,
                "loopable": row.get("loopable"),
            })
        metadata_records[row_id] = {
            "styles": styles,
            "techniques": techniques,
            "typedTechniques": typed_techniques,
            "rhythm": rhythm_result,
        }

    # Resolve G2 source-bound memberships and actual-pitch compatibility separately.
    source_status_counts: collections.Counter[str] = collections.Counter()
    membership_status_counts: collections.Counter[str] = collections.Counter()
    membership_mode_counts: collections.Counter[str] = collections.Counter()
    membership_mode_review_counts: collections.Counter[str] = collections.Counter()
    evidence_class_counts: collections.Counter[str] = collections.Counter()
    mechanism_evidence_counts: collections.Counter[str] = collections.Counter()
    memberships_by_old_mode: dict[str, collections.Counter[str]] = {
        "major": collections.Counter(), "minor": collections.Counter(),
    }
    old_equivalent_status: dict[str, collections.Counter[str]] = {
        "major": collections.Counter(), "minor": collections.Counter(),
    }
    tonic_offset_counts: collections.Counter[int] = collections.Counter()
    per_id: list[dict[str, Any]] = []
    reviewed_pool: dict[str, set[str]] = {mode: set() for mode in MODE_SCALES}
    reviewed_pool_c: dict[str, set[str]] = {mode: set() for mode in MODE_SCALES}
    reviewed_runtime_pool_c: dict[str, set[str]] = {mode: set() for mode in MODE_SCALES}
    all_pool: set[str] = set()
    all_pool_c: set[str] = set()
    all_runtime_pool_c: set[str] = set()
    eligible_sources: collections.Counter[str] = collections.Counter()
    weak_review_ids: dict[str, list[str]] = {"only_unconfirmed": [], "mixed": []}
    structural_review_clues = {
        "no_reviewed_tonic_at_first_or_last": [],
        "no_reviewed_tonic_root_anywhere": [],
        "reviewed_nonstrict_without_marker": [],
    }

    for row_id, row in db_rows.items():
        ms = annotations.get(row_id, [])
        if not ms:
            status = "unannotated"
        elif any(m["review"] != "reviewed" for m in ms):
            status = "mixed" if any(m["review"] == "reviewed" for m in ms) else "unconfirmed_only"
        else:
            status = "reviewed_only"
        source_status_counts[status] += 1
        for membership in ms:
            membership_status_counts[membership["review"]] += 1
            membership_mode_counts[membership["mode"]] += 1
            membership_mode_review_counts[f"{membership['mode']}:{membership['review']}"] += 1
            tonic_offset_counts[membership["tonicOffset"]] += 1
            memberships_by_old_mode[row["mode"]][membership["mode"] + ":" + membership["review"]] += 1
        old_expected = "ionian" if row["mode"] == "major" else "aeolian"
        old_matches = [
            m for m in ms if m["tonicOffset"] == 0 and m["mode"] == old_expected
        ]
        old_eq_statuses = {m["review"] for m in old_matches}
        old_equivalent = (
            "mixed" if old_eq_statuses == {"reviewed", "unconfirmed"} else
            "reviewed" if "reviewed" in old_eq_statuses else
            "unconfirmed" if old_matches else "missing"
        )
        old_equivalent_status[row["mode"]][old_equivalent] += 1
        if status == "unconfirmed_only":
            weak_review_ids["only_unconfirmed"].append(row_id)
        elif status == "mixed":
            weak_review_ids["mixed"].append(row_id)
        enriched = []
        for membership in ms:
            pitch_audit = membership_pitch_audit(row, membership)
            evidence_class_counts[pitch_audit["evidenceClass"]] += 1
            if membership["review"] == "reviewed":
                mechanism_evidence_counts["reviewed_with_borrowed_role"] += bool(pitch_audit["borrowedRoleEventIndices"])
                mechanism_evidence_counts["reviewed_with_altered_degree"] += bool(pitch_audit["alteredDegreeEventIndices"])
                mechanism_evidence_counts["reviewed_with_borrow_or_chromatic_text_marker"] += bool(
                    pitch_audit["evidenceTextHasBorrowOrChromaticMarker"]
                )
                mechanism_evidence_counts["reviewed_strict_pitch_compatible"] += bool(
                    pitch_audit["strictActualPitchCompatible"]
                )
                mechanism_evidence_counts["reviewed_nonstrict_without_these_clues"] += (
                    not pitch_audit["strictActualPitchCompatible"] and
                    not pitch_audit["borrowedRoleEventIndices"] and
                    not pitch_audit["alteredDegreeEventIndices"] and
                    not pitch_audit["evidenceTextHasBorrowOrChromaticMarker"]
                )
            else:
                mechanism_evidence_counts["unconfirmed_membership"] += 1
            enriched.append({**membership, "pitchAudit": pitch_audit})
        if any(m["pitchAudit"]["evidenceClass"] == "reviewed_nonstrict_without_explicit_marker" for m in enriched):
            structural_review_clues["reviewed_nonstrict_without_marker"].append(row_id)
        reviewed = [m for m in enriched if m["review"] == "reviewed"]
        if reviewed and not any(m["pitchAudit"]["startsOrEndsOnTonicRoot"] for m in reviewed):
            structural_review_clues["no_reviewed_tonic_at_first_or_last"].append(row_id)
        if reviewed and not any(m["pitchAudit"]["tonicRootEventIndices"] for m in reviewed):
            structural_review_clues["no_reviewed_tonic_root_anywhere"].append(row_id)

        # Runtime pool identity follows WishSampler::performanceFingerprint:
        # exact status, chord properties, timing, meter, and concrete key are retained.
        for key in range(12):
            all_pool.add(runtime_fingerprint(row, key))
        all_pool_c.add(signature(row, kind="full", tonic=0))
        all_runtime_pool_c.add(runtime_fingerprint(row, 0))
        source_modes = set()
        for membership in reviewed:
            mode = membership["mode"]
            if fp_match[row_id] and membership["pitchAudit"]["strictActualPitchCompatible"]:
                source_modes.add(mode)
                reviewed_pool_c[mode].add(signature(row, kind="full", tonic=0))
                reviewed_runtime_pool_c[mode].add(runtime_fingerprint(row, 0))
                for key in range(12):
                    reviewed_pool[mode].add(runtime_fingerprint(row, key))
        for mode in source_modes:
            eligible_sources[mode] += 1
        per_id.append({
            "id": row_id,
            "legacyMode": row.get("mode", ""),
            "legacyIntent": row.get("intent", ""),
            "cadence": row.get("cadence", ""),
            "loopable": row.get("loopable", False),
            "sequenceText": row.get("sequence", ""),
            "styles": row.get("styles", ""),
            "styleWeightsParsed": metadata_records[row_id]["styles"],
            "techniques": row.get("techniques", ""),
            "techniqueList": metadata_records[row_id]["techniques"],
            "techniqueParityWithTypedData": row_id not in {m["id"] for m in technique_mismatches},
            "rhythmDeclared": metadata_records[row_id]["rhythm"]["declaredRhythm"],
            "rhythmActualDurationsQN": metadata_records[row_id]["rhythm"]["actualDurationsQN"],
            "rhythmMatchesFullEvents": metadata_records[row_id]["rhythm"]["rhythmMatchesFullEvents"],
            "meter": row.get("meter", ""),
            "chordCount": len(typed_data(row).get("full", [])),
            "rootPitchClassesAtReferenceC": [event["root"] for event in actual_sequence(row, 0)],
            "sourceStatus": status,
            "annotationCount": len(ms),
            "sourceFingerprintExpected": next(iter(ann_fingerprints.get(row_id, set())), ""),
            "sourceFingerprintComputed": fp_computed[row_id],
            "sourceFingerprintMatches": fp_match[row_id],
            "legacyEquivalentMembershipStatus": old_equivalent,
            "memberships": enriched,
            "strictEligibleModes": sorted(source_modes),
            "completeSoundDuplicateGroup": "",
            "rhythmlessSoundDuplicateGroup": "",
            "coreConnectionFamily": "",
            "auditFlags": [],
        })

    # Duplication strata. Full sound signatures preserve exact pitch sets and rhythm.
    v5_full_groups = group_signatures(db_rows, "full")
    v5_rhythm_groups = group_signatures(db_rows, "rhythmless")
    v5_core_groups = core_groups(db_rows)
    full_duplicates = duplicate_groups(v5_full_groups)
    rhythm_duplicates = duplicate_groups(v5_rhythm_groups)
    core_duplicates = duplicate_groups(v5_core_groups)
    full_lookup = {member: ids for ids in full_duplicates for member in ids}
    rhythm_lookup = {member: ids for ids in rhythm_duplicates for member in ids}
    core_lookup = {member: ids for ids in core_duplicates for member in ids}
    per_id_by_id = {item["id"]: item for item in per_id}
    for group_index, ids in enumerate(full_duplicates, 1):
        group_id = f"FULL-{group_index:03d}"
        for row_id in ids:
            per_id_by_id[row_id]["completeSoundDuplicateGroup"] = group_id
            per_id_by_id[row_id]["auditFlags"].append("exact_complete_sound_duplicate")
    for group_index, ids in enumerate(rhythm_duplicates, 1):
        group_id = f"RHYTHMLESS-{group_index:03d}"
        for row_id in ids:
            per_id_by_id[row_id]["rhythmlessSoundDuplicateGroup"] = group_id
            per_id_by_id[row_id]["auditFlags"].append("same_harmony_ignoring_rhythm")
    for group_index, ids in enumerate(core_duplicates, 1):
        group_id = f"CORE-{group_index:03d}"
        for row_id in ids:
            per_id_by_id[row_id]["coreConnectionFamily"] = group_id
            per_id_by_id[row_id]["auditFlags"].append("core_family_only_low_confidence")
    for item in per_id:
        if item["sourceStatus"] in {"mixed", "unconfirmed_only"}:
            item["auditFlags"].append("G2_has_unconfirmed_membership")
        if not item["techniques"]:
            item["auditFlags"].append("technique_metadata_missing")
        if not item["rhythmMatchesFullEvents"]:
            item["auditFlags"].append("rhythm_text_differs_from_actual_events")
        if not item["sourceFingerprintMatches"]:
            item["auditFlags"].append("source_fingerprint_mismatch")

    # Candidate status/strict checks and cross-catalogue duplicate / near-family audit.
    candidate_result = candidate_comparison(candidates, candidate_metadata, db_rows)
    candidate_result["candidateJsonSha256"] = candidate_hash
    candidate_result["candidateMetadataSha256"] = metadata_hash
    candidate_status_counts = collections.Counter(
        str(item.get("status", "")) for item in candidate_metadata.get("candidates", [])
    )
    candidate_mode_counts = collections.Counter(str(item.get("mode", "")) for item in candidates)
    candidate_audition_counts = collections.Counter(
        str(item.get("auditionStatus", "")) for item in candidate_metadata.get("candidates", [])
    )
    candidate_exact_hits = [
        item for item in candidate_result["records"] if item["completeSoundMatchesV5"]
    ]
    candidate_rhythmless_hits = [
        item for item in candidate_result["records"] if item["rhythmlessSoundMatchesV5"]
    ]
    candidate_core_hits = [
        item for item in candidate_result["records"] if item["coreConnectionFamilyMatchesV5"]
    ]
    candidate_one_edit = [
        item for item in candidate_result["records"]
        if item["minimumHarmonicEventEditDistanceToV5"] == 1
    ]
    candidate_strict_pass = sum(
        item["strictReviewedMemberships"] > 0 for item in candidate_result["records"]
    )

    missing_technique_rows = [item for item in per_id if not item["techniques"]]
    missing_technique_impact = {
        "sourceStatusCounts": dict(collections.Counter(item["sourceStatus"] for item in missing_technique_rows)),
        "withAnyReviewedMembership": sum(
            any(m["review"] == "reviewed" for m in item["memberships"])
            for item in missing_technique_rows
        ),
        "withAnyUnconfirmedMembership": sum(
            any(m["review"] != "reviewed" for m in item["memberships"])
            for item in missing_technique_rows
        ),
        "withAnyStrictReviewedModeCompatibility": sum(
            bool(item["strictEligibleModes"]) for item in missing_technique_rows
        ),
        "withAnyReviewedNonstrictMembership": sum(
            any(m["review"] == "reviewed" and not m["pitchAudit"]["strictActualPitchCompatible"]
                for m in item["memberships"])
            for item in missing_technique_rows
        ),
        "withActualBorrowedRoleEvents": sum(
            any(m["pitchAudit"]["borrowedRoleEventIndices"] for m in item["memberships"])
            for item in missing_technique_rows
        ),
        "withAlteredDegreeEvents": sum(
            any(m["pitchAudit"]["alteredDegreeEventIndices"] for m in item["memberships"])
            for item in missing_technique_rows
        ),
        "withExplicitBorrowOrChromaticEvidenceText": sum(
            any(m["pitchAudit"]["evidenceTextHasBorrowOrChromaticMarker"] for m in item["memberships"])
            for item in missing_technique_rows
        ),
        "withFullSoundDuplicatePeer": sum(bool(item["completeSoundDuplicateGroup"]) for item in missing_technique_rows),
        "sourceIds": [item["id"] for item in missing_technique_rows],
    }
    top_voice_event_count = sum(
        event.get("topVoice") is not None
        for row in db_rows.values() for event in typed_data(row).get("full", [])
    )

    # Old mode fields are only a legacy reference. No membership is inferred from them.
    by_legacy_mode = {}
    for legacy_mode in ("major", "minor"):
        ids = [item for item in per_id if item["legacyMode"] == legacy_mode]
        by_legacy_mode[legacy_mode] = {
            "sourceCount": len(ids),
            "reviewedOnlySources": sum(item["sourceStatus"] == "reviewed_only" for item in ids),
            "mixedSources": sum(item["sourceStatus"] == "mixed" for item in ids),
            "unconfirmedOnlySources": sum(item["sourceStatus"] == "unconfirmed_only" for item in ids),
            "legacyEquivalentModeAtOffset0": "ionian" if legacy_mode == "major" else "aeolian",
            "legacyEquivalentStatusCounts": dict(old_equivalent_status[legacy_mode]),
            "membershipModeReviewCounts": dict(memberships_by_old_mode[legacy_mode]),
            "rowsWithoutAnyReviewedMembership": [
                item["id"] for item in ids if item["sourceStatus"] == "unconfirmed_only"
            ],
            "rowsWithNoReviewedTonicChordAtPhraseEdge": [
                item["id"] for item in ids
                if item["id"] in structural_review_clues["no_reviewed_tonic_at_first_or_last"]
            ],
        }

    nonstrict_no_marker_ids = set(structural_review_clues["reviewed_nonstrict_without_marker"])
    nonstrict_prefix_counts: collections.Counter[str] = collections.Counter()
    for row_id in nonstrict_no_marker_ids:
        parts = row_id.split("_")
        family = "_".join(parts[:2]) if row_id.startswith("V3_") else parts[0]
        nonstrict_prefix_counts[family] += 1
    nonstrict_missing_technique_ids = sorted(
        row_id for row_id in nonstrict_no_marker_ids
        if not per_id_by_id[row_id]["techniques"]
    )

    summary = {
        "audit": {
            "title": "Factory V5 658-row read-only audit for V6 B1",
            "baseline": args.baseline,
            "databasePath": str(args.db),
            "databaseSha256": db_hash,
            "databaseMetadata": database_meta,
            "annotationPath": str(args.annotations),
            "annotationSha256": annotation_hash,
            "candidateJsonPath": str(args.candidates),
            "candidateJsonSha256": candidate_hash,
            "candidateMetadataPath": str(args.metadata),
            "candidateMetadataSha256": metadata_hash,
            "readMode": "SQLite URI mode=ro plus PRAGMA query_only=ON; no user.db path opened",
            "generatedAtLocalDate": "2026-10-24",
            "humanListening": "Not performed by this audit; G2 notes say user audition pending.",
        },
        "identityAndCoverage": {
            "dbRows": len(db_rows),
            "uniqueIds": len(db_rows),
            "annotationIds": len(annotations),
            "missingAnnotationIds": sorted(set(db_rows) - set(annotations)),
            "extraAnnotationIds": sorted(set(annotations) - set(db_rows)),
            "annotationsWithOneConsistentFingerprint": sum(
                len(ann_fingerprints.get(row_id, set())) == 1 for row_id in db_rows
            ),
            "sourceFingerprintsMatching": sum(fp_match.values()),
            "sourceFingerprintMismatches": [
                {"id": row_id, "computed": fp_computed[row_id],
                 "annotation": sorted(ann_fingerprints.get(row_id, set()))}
                for row_id in db_rows if not fp_match[row_id]
            ],
            "annotationMemberships": sum(len(ms) for ms in annotations.values()),
            "sourceStatusCounts": dict(source_status_counts),
            "membershipReviewStatusCounts": dict(membership_status_counts),
            "membershipModeCounts": dict(membership_mode_counts),
            "membershipModeReviewCounts": dict(membership_mode_review_counts),
            "tonicOffsetCounts": dict(sorted(tonic_offset_counts.items())),
            "unconfirmedOnlyIds": sorted(weak_review_ids["only_unconfirmed"]),
            "mixedReviewIds": sorted(weak_review_ids["mixed"]),
            "reviewedMembershipPitchClasses": dict(evidence_class_counts),
            "independentMechanismEvidenceCounts": dict(mechanism_evidence_counts),
        },
        "legacyModeComparison": by_legacy_mode,
        "metadataChecks": {
            "legacyModeCounts": dict(collections.Counter(row.get("mode", "") for row in db_rows.values())),
            "intentCounts": dict(intents),
            "cadenceCounts": dict(cadences),
            "meterCounts": dict(meters),
            "topVoiceEventCount": top_voice_event_count,
            "styleAssignments": dict(style_names),
            "styleAssignmentCountPerRow": dict(style_count),
            "styleParseOrRangeErrors": style_errors,
            "techniqueTags": dict(technique_names),
            "techniquePopulatedRows": len(db_rows) - len(missing_techniques),
            "techniqueMissingRows": len(missing_techniques),
            "techniqueMissingIds": sorted(missing_techniques),
            "techniqueMissingImpact": missing_technique_impact,
            "techniqueParityMismatches": technique_mismatches,
            "rhythmUniquePatterns": len(rhythms),
            "rhythmPatternCounts": dict(rhythms),
            "rhythmMismatchCount": len(rhythm_mismatches),
            "rhythmMismatches": rhythm_mismatches,
            "intentCadenceLoopConsistencyClues": intent_cadence_checks,
        },
        "strictActualMusicPools": {
            "definition": (
                "Only reviewed G2 memberships admit a source; actual realized root, bass and every chord pitch "
                "are checked against the selected mode mask. C-reference independent-content counts deduplicate "
                "the complete sounding pitch/rhythm signature modulo global transposition; all-12 concrete "
                "performance totals use WishSampler performanceFingerprint."
            ),
            "unrestrictedUniqueMusicCountAtTonicC": len(all_pool_c),
            "unrestrictedRuntimeFingerprintsAtSourceC": len(all_runtime_pool_c),
            "all12KeysConcreteUniqueMusicCount": len(all_pool),
            "modeUniqueMusicCountsAtTonicC": {mode: len(pool) for mode, pool in reviewed_pool_c.items()},
            "modeRuntimeFingerprintsAtSourceC": {mode: len(pool) for mode, pool in reviewed_runtime_pool_c.items()},
            "modeAll12KeysConcreteUniqueMusicCounts": {mode: len(pool) for mode, pool in reviewed_pool.items()},
            "modeSourceCountsWithAtLeastOneStrictReviewedMembership": dict(eligible_sources),
            "unconfirmedMembershipsExcludedFromModePools": membership_status_counts["unconfirmed"],
        },
        "duplicateStrata": {
            "fullSoundDefinition": "Transposition-normalized chord root, bass, complete actual pitch-class set, meter, onset and duration.",
            "fullSoundConfidence": "High for identical encoded pitch-class performance and rhythm; does not claim identical voicing, audio or arrangement.",
            "fullSoundGroups": full_duplicates,
            "rhythmlessDefinition": "Same transposition-normalized chord roots, basses and pitch-class sets; timing and meter omitted.",
            "rhythmlessConfidence": "Medium; same harmonic sequence and chord contents, timing arrangement may differ.",
            "rhythmlessGroups": rhythm_duplicates,
            "coreFamilyDefinition": "Project WishSampler familyIdentity: relative root/bass and chord quality with major7/major and minor7/minor collapsed.",
            "coreFamilyConfidence": "Low similarity bucket only; never a duplicate verdict or deletion instruction.",
            "coreFamilyGroups": core_duplicates,
        },
        "v6DraftComparison": {
            "candidateCount": len(candidates),
            "candidateStatusCounts": dict(candidate_status_counts),
            "candidateFileModeCounts": dict(candidate_mode_counts),
            "candidateAuditionStatusCounts": dict(candidate_audition_counts),
            "completeSoundMatchesV5": [item["id"] for item in candidate_exact_hits],
            "rhythmlessMatchesV5": [item["id"] for item in candidate_rhythmless_hits],
            "coreFamilyMatchesV5": [item["id"] for item in candidate_core_hits],
            "minimumOneChordEditNearV5": [item["id"] for item in candidate_one_edit],
            "candidateStrictReviewedMembershipsPresent": candidate_strict_pass,
            "candidateCompleteSoundDuplicateGroups": candidate_result["candidateExactCompleteDuplicateGroups"],
            "candidateRhythmlessDuplicateGroups": candidate_result["candidateRhythmlessDuplicateGroups"],
            "candidateCoreFamilyGroups": candidate_result["candidateCoreFamilyGroups"],
        },
        "manualReviewClues": {
            "noReviewedTonicRootAtFirstOrLastPhraseEvent": sorted(
                structural_review_clues["no_reviewed_tonic_at_first_or_last"]
            ),
            "noReviewedTonicRootAnywhereInPhrase": sorted(
                structural_review_clues["no_reviewed_tonic_root_anywhere"]
            ),
            "reviewedNonStrictWithoutBorrowOrChromaticMarker": sorted(
                structural_review_clues["reviewed_nonstrict_without_marker"]
            ),
            "reviewedNonStrictWithoutMarkerIdFamilyCounts": dict(nonstrict_prefix_counts),
            "reviewedNonStrictWithoutMarkerWithMissingStoredTechniqueCount": len(nonstrict_missing_technique_ids),
            "reviewedNonStrictWithoutMarkerWithMissingStoredTechniqueIds": nonstrict_missing_technique_ids,
            "unconfirmedAndMixedEntriesAreNotMarkedAsReviewedByThisAudit": True,
        },
    }

    # Per-ID TSV fields are kept compact; the JSON contains full evidence and membership fields.
    tsv_fields = [
        "id", "legacyMode", "sequenceText", "styles", "legacyIntent", "cadence",
        "techniques", "rhythmDeclared", "rhythmActualDurationsQN", "rhythmMatchesFullEvents",
        "meter", "chordCount", "rootPitchClassesAtReferenceC", "sourceStatus",
        "legacyEquivalentMembershipStatus", "sourceFingerprintMatches",
        "sourceFingerprintExpected", "sourceFingerprintComputed", "annotationCount",
        "loopable", "styleWeightsParsed", "techniqueList", "techniqueParityWithTypedData",
        "completeSoundDuplicateGroup", "rhythmlessSoundDuplicateGroup",
        "coreConnectionFamily", "strictEligibleModes", "memberships", "auditFlags",
    ]
    with (out_dir / "per_id_audit.tsv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=tsv_fields, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        for item in per_id:
            writer.writerow({
                **item,
                "styleWeightsParsed": json.dumps(item["styleWeightsParsed"], ensure_ascii=False, separators=(",", ":")),
                "techniqueList": json.dumps(item["techniqueList"], ensure_ascii=False),
                "rhythmActualDurationsQN": json.dumps(item["rhythmActualDurationsQN"], ensure_ascii=False),
                "rootPitchClassesAtReferenceC": json.dumps(item["rootPitchClassesAtReferenceC"]),
                "strictEligibleModes": json.dumps(item["strictEligibleModes"], ensure_ascii=False),
                "memberships": json.dumps(item["memberships"], ensure_ascii=False, separators=(",", ":")),
                "auditFlags": json.dumps(item["auditFlags"], ensure_ascii=False),
            })
    (out_dir / "per_id_audit.json").write_text(
        json.dumps(per_id, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    (out_dir / "v6_candidate_comparison.json").write_text(
        json.dumps(candidate_result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    (out_dir / "summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    with (out_dir / "v6_candidate_comparison.tsv").open("w", encoding="utf-8", newline="") as stream:
        fields = [
            "id", "referenceTonic", "status", "auditionStatus", "candidateFileStyles",
            "candidateFileIntent", "candidateFileTechniques", "candidateFileRhythm",
            "strictReviewedMemberships", "reviewedMemberships", "memberships",
            "completeSoundMatchesV5", "rhythmlessSoundMatchesV5",
            "coreConnectionFamilyMatchesV5", "minimumHarmonicEventEditDistanceToV5",
            "oneChordEditNeighborsV5", "otherCompleteSoundMatchesInV6",
            "otherRhythmlessMatchesInV6", "otherCoreFamilyMembersInV6",
        ]
        writer = csv.DictWriter(stream, fieldnames=fields, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        for item in candidate_result["records"]:
            writer.writerow({
                **item,
                "memberships": json.dumps(item["memberships"], ensure_ascii=False, separators=(",", ":")),
                "completeSoundMatchesV5": json.dumps(item["completeSoundMatchesV5"]),
                "rhythmlessSoundMatchesV5": json.dumps(item["rhythmlessSoundMatchesV5"]),
                "coreConnectionFamilyMatchesV5": json.dumps(item["coreConnectionFamilyMatchesV5"]),
                "oneChordEditNeighborsV5": json.dumps(item["oneChordEditNeighborsV5"]),
                "otherCompleteSoundMatchesInV6": json.dumps(item["otherCompleteSoundMatchesInV6"]),
                "otherRhythmlessMatchesInV6": json.dumps(item["otherRhythmlessMatchesInV6"]),
                "otherCoreFamilyMembersInV6": json.dumps(item["otherCoreFamilyMembersInV6"]),
            })

    report = []
    report += [
        "# Factory V5 / G2 v2 只读审计",
        "",
        "本审计以 Factory V5 构建数据库及 G2 v2 来源绑定目录为对象，按实际音高与演奏时值复算结构覆盖。全库人工听评未执行；G2 目录的审校语义为结构编辑审校，用户试听仍待完成。",
        "",
        "## 对象与边界",
        "",
        f"- 源基线：{args.baseline}。数据库：{args.db}；SHA-256：{db_hash}；SQLite 只读模式，行数 {len(db_rows)}。",
        f"- G2 v2：{args.annotations}；SHA-256：{annotation_hash}；精确 ID 与源指纹匹配 {sum(fp_match.values())}/{len(db_rows)}。",
        f"- B1 草案：60 条候选源 SHA-256 {candidate_hash}；元数据 SHA-256 {metadata_hash}。比较不修改草案文件。",
        "- 此流程只打开明确列出的 Factory 数据库，不打开 user.db。",
        "",
        "## 覆盖矩阵",
        "",
        "| 检查项 | 当前结果 | 解释 |",
        "| --- | ---: | --- |",
        f"| Factory V5 行 / 唯一 ID | {len(db_rows)} / {len(set(db_rows))} | 数据库元数据：{json.dumps(database_meta, ensure_ascii=False)} |",
        f"| G2 v2 ID / 归属行 | {len(annotations)} / {sum(len(v) for v in annotations.values())} | 缺 ID {len(set(db_rows)-set(annotations))}；多余 ID {len(set(annotations)-set(db_rows))} |",
        f"| 来源指纹 | {sum(fp_match.values())}/{len(db_rows)} exact match | Python 复刻项目 C++ 指纹；数据库与 HCA SHA 锁定，脚本可重算 |",
        f"| G2 状态 | {source_status_counts['reviewed_only']} reviewed-only；{source_status_counts['mixed']} mixed；{source_status_counts['unconfirmed_only']} unconfirmed-only | 任一 unconfirmed 不计作全已审校 |",
        f"| 样式 / 主 intent | {len(db_rows)-sum(not r.get('styles') for r in db_rows.values())}/{len(db_rows)}；{len(db_rows)-sum(not r.get('intent') for r in db_rows.values())}/{len(db_rows)} | 另有逐 ID 字段和值、格式检查 |",
        f"| techniques | {len(db_rows)-len(missing_techniques)}/{len(db_rows)} populated；{len(missing_techniques)} 空 | 空标签是实际覆盖缺口；不可从和弦外观自动补写 |",
        f"| 节奏文本 | {len(db_rows)-len(rhythm_mismatches)}/{len(db_rows)} 与 full durationQN 一致 | {len(rhythms)} 种节奏文本；空时值数按全库实际字段核验 |",
        f"| 完整严格实际音乐池 | 固定 C 独立内容 {len(all_pool_c)}；全 12 调具体演奏 {len(all_pool)}（÷12={len(all_pool)//12}） | 主数字按完整实响序列跨移调去重；调式限定数量见下表 |",
        "",
        "## 旧 mode 与 G2 v2 分开看",
        "",
        "| Factory 旧 mode | 行数 | 仅 reviewed | 混合 | 仅 unconfirmed | 对应旧中心标签在 offset 0：reviewed / mixed / unconfirmed / missing |",
        "| --- | ---: | ---: | ---: | ---: | --- |",
    ]
    for legacy_mode in ("major", "minor"):
        data = by_legacy_mode[legacy_mode]
        counts = data["legacyEquivalentStatusCounts"]
        report.append(
            f"| {legacy_mode} | {data['sourceCount']} | {data['reviewedOnlySources']} | {data['mixedSources']} | "
            f"{data['unconfirmedOnlySources']} | {counts.get('reviewed',0)} / {counts.get('mixed',0)} / "
            f"{counts.get('unconfirmed',0)} / {counts.get('missing',0)} |"
        )
    report += [
        "",
        "旧 major/minor 是原库参考框架。脚本没有把 major 映射成已审校 Ionian，也没有把 minor 映射成已审校 Aeolian；“对应旧中心”只用于指出显式 G2 记录是否存在。",
        "",
        "## G2 中心、兼容与借用分开统计",
        "",
        "| 调式 | reviewed membership | unconfirmed membership | 固定 C 严格唯一音乐数 | 全 12 调具体演奏数 |",
        "| --- | ---: | ---: | ---: | ---: |",
    ]
    for mode in MODE_SCALES:
        report.append(
            f"| {mode} | {membership_mode_review_counts[mode + ':reviewed']} | {membership_mode_review_counts[mode + ':unconfirmed']} | "
            f"{len(reviewed_pool_c[mode])} | {len(reviewed_pool[mode])} |"
        )
    report += [
        "",
        f"全库 membership 级实际音集结果：{evidence_class_counts.get('reviewed_strictly_compatible',0)} reviewed 且全音集符合调式；"
        f"{evidence_class_counts.get('reviewed_nonstrict_with_borrow_or_chromatic_marker',0)} reviewed、实际超出静态音阶且有 Borrowed/变音/借用文字标记；"
        f"{evidence_class_counts.get('reviewed_nonstrict_without_explicit_marker',0)} reviewed、实际超出静态音阶但无 Borrowed role、变音 degree 或 evidence 文字线索；"
        f"{membership_status_counts['unconfirmed']} unconfirmed 独列。此标签核对的是实响音集，不反推调式归属。",
        f"独立机制线索（与 stored techniques 分开，且只作描述）：Borrowed role {mechanism_evidence_counts['reviewed_with_borrowed_role']} 条 reviewed membership；"
        f"变音 degree {mechanism_evidence_counts['reviewed_with_altered_degree']}；显式 Borrowed/变音文字 {mechanism_evidence_counts['reviewed_with_borrow_or_chromatic_text_marker']}；"
        f"严格兼容 {mechanism_evidence_counts['reviewed_strict_pitch_compatible']}；上述线索皆缺但音集超出 {mechanism_evidence_counts['reviewed_nonstrict_without_these_clues']}；unconfirmed {mechanism_evidence_counts['unconfirmed_membership']}。",
        "",
        "每条 membership 的中心 offset、mode、usage、usageBasis、status、审校人、原始音乐证据、实响根音与实际音集差异均在逐 ID JSON/TSV。与 stored techniques 分开的 derivedMechanismEvidence 给出相对音阶级数、属五级属七事件、低音相对根音、和弦音程集合；这些只描述实际音符，不推断 membership。",
        "",
        "## 风格、意图、技法、节奏字段",
        "",
        f"- Style：{len(db_rows)-sum(not r.get('styles') for r in db_rows.values())}/658 非空；格式/权重异常 {len(style_errors)} 条。权重仅按 0..1 解析有效性核验。",
        f"- Intent：{json.dumps(dict(intents), ensure_ascii=False)}；Cadence：{json.dumps(dict(cadences), ensure_ascii=False)}。",
        f"- Technique：{len(db_rows)-len(missing_techniques)} 条非空、{len(missing_techniques)} 条缺失；payload 与 typedData 不一致 {len(technique_mismatches)} 条。",
        f"- 空 technique 的具体影响：{missing_technique_impact['sourceStatusCounts']}；其中 reviewed 且实响 strict {missing_technique_impact['withAnyStrictReviewedModeCompatibility']} 条、reviewed 但实响 nonstrict {missing_technique_impact['withAnyReviewedNonstrictMembership']} 条、含实际 Borrowed role {missing_technique_impact['withActualBorrowedRoleEvents']} 条、degree 变音 {missing_technique_impact['withAlteredDegreeEvents']} 条、完整实响重复组成员 {missing_technique_impact['withFullSoundDuplicatePeer']} 条。空标签不表示没有音乐技法，明细按源 ID 见 summary.json 和 per-ID 台账。",
        f"- Rhythm：{len(rhythms)} 种 profile；与逐 chord 时值不一致 {len(rhythm_mismatches)} 条。全库以 actual durationQN 为准，明细保留声明文本。",
        "",
        "Style 为实际标签赋值数（每条可多标签）：",
        "",
        "| Style | 赋值数 |",
        "| --- | ---: |",
    ]
    for name, count in sorted(style_names.items(), key=lambda pair: (-pair[1], pair[0])):
        report.append(f"| {name} | {count} |")
    report += [
        "",
        "Technique 为存储标签出现次数，不代表音乐机制穷举；下表列前 12 类，完整频次见 summary.json：",
        "",
        "| Technique | 次数 |",
        "| --- | ---: |",
    ]
    for name, count in technique_names.most_common(12):
        report.append(f"| {name} | {count} |")
    report += [
        "",
        "拍号分布：",
        "",
        "| 拍号 | 条数 |",
        "| --- | ---: |",
    ]
    for name, count in sorted(meters.items(), key=lambda pair: (-pair[1], pair[0])):
        report.append(f"| {name} | {count} |")
    report += [
        "",
        "节奏 profile 分布（文本值与 actual durationQN 已逐行核对）：",
        "",
        "| Rhythm | 条数 |",
        "| --- | ---: |",
    ]
    for name, count in sorted(rhythms.items(), key=lambda pair: (-pair[1], pair[0])):
        report.append(f"| `{name}` | {count} |")
    report += [
        "",
        "## 重复与近似层级",
        "",
        f"- 完整实响重复：{len(full_duplicates)} 组，涉及 {sum(map(len,full_duplicates))} 条，冗余 {sum(len(g)-1 for g in full_duplicates)} 条；包含实际和弦音集、低音、拍号、起点与时值，跨移调归一。此类可作为高可信重复候选。",
        f"- 去节奏重复：{len(rhythm_duplicates)} 组，涉及 {sum(map(len,rhythm_duplicates))} 条，冗余 {sum(len(g)-1 for g in rhythm_duplicates)} 条；忽略拍号和时值，保留完整和弦音集/低音/顺序，属于相同和声内容、编排可能不同。",
        f"- 核心连接家族：{len(core_duplicates)} 组，涉及 {sum(map(len,core_duplicates))} 条；复用 WishSampler 的低置信度多样性家族规则，只含相对根音、低音与简化和弦质量，不能视为重复或删除依据。",
        "",
        "V5 完整实响重复组：",
        "",
    ]
    if full_duplicates:
        report.extend(f"- {', '.join(ids)}" for ids in full_duplicates)
    else:
        report.append("- 无")
    report += [
        "",
        "## V6 B1 草案与 V5 对照",
        "",
        f"- 候选 {len(candidates)} 条；候选文件 mode：{json.dumps(dict(candidate_mode_counts), ensure_ascii=False)}；元数据 status：{json.dumps(dict(candidate_status_counts), ensure_ascii=False)}；auditionStatus：{json.dumps(dict(candidate_audition_counts), ensure_ascii=False)}。",
        f"- 与 V5 完整实响完全重复 {len(candidate_exact_hits)} 条；去节奏相同 {len(candidate_rhythmless_hits)} 条；低置信度核心连接家族相同 {len(candidate_core_hits)} 条；最近相差一个和弦事件 {len(candidate_one_edit)} 条。",
        f"- 候选内部完整重复组：{json.dumps(candidate_result['candidateExactCompleteDuplicateGroups'], ensure_ascii=False)}。",
        "- 低置信度核心家族命中：V6_B1_010 ↔ V3_SHADE_032；V6_B1_028 ↔ JAZZ_018。均不是完整重复。V6_B1_028 的已审校 Ionian membership 实响超出 pitch classes [3, 8, 10]，其 Backdoor/ExtendedHarmony 标签可能解释材料，但应由主代理针对 membership evidence 复核。",
        "",
        "命中完整重复的 V6 条目及 V5 来源：",
        "",
    ]
    if candidate_exact_hits:
        for item in candidate_exact_hits:
            report.append(f"- {item['id']} = {', '.join(item['completeSoundMatchesV5'])}")
    else:
        report.append("- 无")
    report += [
        "",
        "候选 mode membership 与严格音阶兼容独立列出；严格通过只表示本次完整实响可被该静态音阶解释，不创建归属。借用/半音材料允许存在，但需以具体 membership 的 evidence/status 审查。",
        "",
        "## 待人工复核线索与批次建议",
        "",
        f"- Unconfirmed-only {len(weak_review_ids['only_unconfirmed'])} 条：{', '.join(sorted(weak_review_ids['only_unconfirmed']))}",
        f"- Mixed {len(weak_review_ids['mixed'])} 条：{', '.join(sorted(weak_review_ids['mixed']))}",
        f"- Reviewed membership 中首末都未落在其标注主音根音的源条目：{len(structural_review_clues['no_reviewed_tonic_at_first_or_last'])} 条：{', '.join(sorted(structural_review_clues['no_reviewed_tonic_at_first_or_last']))}。该边界 heuristic 不足以否定 Ionian：主代理按和弦序列复核这 12 条后确认序列内部含 I，边界由功能进行/回接解释；其中 CITYPOP_JPOP_011、JAZZ_013、JAZZ_017 的功能序列各有多种解释。现有 HCA 保持不变；主音中心并非唯一解释，未据此追加 Reviewed。",
        f"- 另有实际序列在 reviewed membership 下全程没有标注主音根音的源条目：{len(structural_review_clues['no_reviewed_tonic_root_anywhere'])} 条，名单见 summary.json；这是比“首末无 I”更直接的中心证据缺口。",
        f"- Reviewed 实际超出静态音阶且无 Borrowed role、变音 degree 或 evidence 文字标记：{evidence_class_counts.get('reviewed_nonstrict_without_explicit_marker',0)} 个 membership / {len(nonstrict_no_marker_ids)} 个源条目。ID 家族计数为 {json.dumps(dict(nonstrict_prefix_counts), ensure_ascii=False)}；其中 {len(nonstrict_missing_technique_ids)} 条 techniques 为空。V3_BLUE 只是源 ID 家族线索，可能是有意 blue-note / 蓝调材料或技法元数据缺口；不得作为归属错误自动处理。逐条外音 pitch class 和 evidence 在 per-ID 文件。",
        "- 全库 reviewed 仍只表示结构编辑审校；未完成人工听评。40 条含 unconfirmed 记录须继续保持待审，禁止从旧 Major/Minor 或全音阶兼容自动升为 Reviewed。",
        "",
        "建议只针对实际缺口复核：先审 25 条 unconfirmed-only 和 15 条 mixed 中具体 unconfirmed membership；再审 81 条非严格且无显式解释线索的记录（其中 50 条属 V3_BLUE ID 家族，需判断有意蓝调音还是技法元数据缺失）。237 条 techniques 空项应先关联实际角色/变音与功能影响再决定是否补标签，不做全库重读或自动补写。12 条边界主音 heuristic 已由主代理按功能/回接复核，现有 HCA 保持，不作负面判定。",
        "",
        "## 方法与复算",
        "",
        "audit_factory_v5.py 从上述只读 SQLite 数据库载入 658 个 payload；按 src/wish/MusicIdentity.cpp、src/wish/ModeAnnotations.cpp、src/wish/WishSampler.cpp、src/library/LegacyFactoryIdResolver.cpp 的身份、归一与过滤规则实现可复算投影。HCA 解析遵循 10 字段格式并按 exact Factory ID + source-v1 指纹匹配。严格池统计在 source match + reviewed membership 限定后，逐调式检查 root、bass 与每个 chord pitch；V5 无 topVoice 数据。固定 C 独立音乐数以完整实响音高/低音/拍号/节奏签名跨移调去重，全 12 调的具体演奏数另按 WishSampler performanceFingerprint 去重；音阶兼容从不产生 membership。",
        "",
        "重复定义及置信度见 summary.json duplicateStrata；核心连接家族完全复用当前 WishSampler 逻辑。v6_candidate_comparison.json/tsv 给出草案每条与 V5 的精确、去节奏、核心家族和一和弦编辑近邻。",
        "",
        "限制：固定 C 去重的完整实响签名忽略 C++ performanceFingerprint 的质量编码/精确标志差异，只比较实际根音、低音、pitch-class 集、拍号和时值；因此这是音乐内容重复口径，运行时身份数另保留于 summary.json。代码不推断键盘配器、声部进行、听感、风格正确性或全曲编曲同一性。结构中心标记和 evidence 供主代理针对具体条目复核；未进行 658 首听评。",
        "",
    ]
    (out_dir / "REPORT.md").write_text("\n".join(report), encoding="utf-8")
    print(json.dumps({
        "databaseRows": len(db_rows),
        "sourceFingerprintMatches": sum(fp_match.values()),
        "sourceStatusCounts": dict(source_status_counts),
        "techniqueMissing": len(missing_techniques),
        "rhythmMismatches": len(rhythm_mismatches),
        "unrestrictedUniqueMusicAtTonicC": len(all_pool_c),
        "all12KeysConcreteUniqueMusicCount": len(all_pool),
        "strictModeUniqueMusicAtTonicC": {mode: len(pool) for mode, pool in reviewed_pool_c.items()},
        "fullDuplicateGroups": full_duplicates,
        "rhythmlessDuplicateGroupCount": len(rhythm_duplicates),
        "coreFamilyGroupCount": len(core_duplicates),
        "v6CompleteSoundHits": [
            {"id": item["id"], "v5": item["completeSoundMatchesV5"]} for item in candidate_exact_hits
        ],
        "v6OneEditCount": len(candidate_one_edit),
        "outputs": [str(path) for path in sorted(out_dir.iterdir()) if path.is_file()],
    }, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
