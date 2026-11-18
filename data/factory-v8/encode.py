"""Encode the reviewed G8 phrases without importing or writing Factory V7."""

from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path


OUT_DIR = Path(__file__).resolve().parent
ROOT = OUT_DIR.parents[1]
V7_METADATA = ROOT / "data" / "factory-v7" / "metadata.json"
G7_BASELINE = "58dfce0fb78c712def4902d81b8103747b746ea2"

PHRASES = [
    ("PlateauRise", "C", "major", "suspended", "Fmaj7 G6 Am7 C/E Fadd9 Am7/E Dm9 Gsus4"),
    ("NeighbourPlateau", "C", "major", "loop_return", "Fmaj9 Am9 G6 C/E Fmaj9 Dm9 Em7 Gsus4"),
    ("AbsentTonic", "C", "major", "open_unresolved", "Fadd9 G/F Em7 Am9 Fmaj7 Am7/E Dm9 Em7"),
    ("DorianPedal", "A", "minor", "modal_development", "Am9 D/A G/A D/A Am7 Cmaj7/E D9/F# Bm7"),
    ("MinorLine", "A", "minor", "open_unresolved", "Am9 AmMaj7/G# Am7/G D9/F# Fmaj7 Em7 Dm9 G6"),
    ("PhrygianPedal", "A", "minor", "modal_development", "Am7 Bb/A C/A Bb/A Am9 Gm9 Bbmaj7 Gm9"),
    ("LydianExpansion", "C", "major", "modal_development", "Cmaj9 D/C Em7/D D/C Cmaj7/E D/F# Em9 F#m7b5"),
    ("SubdominantPlane", "C", "major", "open_unresolved", "Fmaj9 G/F C/E Dm9 Bbmaj7 C/Bb Gm9 Am7"),
    ("ChromaticMediantBridge", "C", "major", "center_transition", "Am9 Fmaj7 Abmaj7 Bb6 Cm9 Ebmaj7 Fm9 Abmaj7"),
    ("AscendingBass", "C", "major", "open_unresolved", "Dm9 Em7 Fmaj9 G6 Am9 Bm7b5 Cmaj9/E Fmaj9/A"),
    ("SuspendedSequence", "C", "major", "suspended", "Dm11 Gsus4 Em11 Asus4 Fmaj9 Bbadd9 C/G Dm9/A"),
    ("BorrowedPlagal", "A", "minor", "center_transition", "Am9 Cmaj7/G Fmaj9 Dm9 Fm6 Abmaj7 Gm9 Bbadd9"),
    ("ChangingPedal", "C", "major", "suspended", "Cmaj9/G D/G Em7/G Fmaj7/G Gsus4 F/G Em7/G Dm9/G"),
    ("AeolianLift", "A", "minor", "loop_return", "Am9 C/G Fmaj9 G6 Am/E Dm9 Fmaj7 Gsus4"),
    ("MinorMediantMotion", "A", "minor", "interim_pause", "Am9 Fmaj7 Dm9 Bbmaj7 Gm9 Ebmaj7 Cm9 Fm9"),
    ("CommonToneSideStep", "C", "major", "center_transition", "Cmaj9 Em7 Abmaj9 Gm9 Fmaj9 Dm9 Dbmaj7 Ebadd9"),
]
DISPLAY_NAMES_ZH = {
    "PlateauRise": "01悬置抬升",
    "NeighbourPlateau": "02邻接平台回环",
    "AbsentTonic": "03隐主音展开",
    "DorianPedal": "04多利亚持续低音",
    "MinorLine": "05小调下行低音线",
    "PhrygianPedal": "06弗里吉亚持续低音",
    "LydianExpansion": "07利底亚展开",
    "SubdominantPlane": "08下属和声流动",
    "ChromaticMediantBridge": "09半音中介跨段",
    "AscendingBass": "10上行低音展开",
    "SuspendedSequence": "11悬置和声递进",
    "BorrowedPlagal": "12借用下属转接",
    "ChangingPedal": "13持续音逐层变化",
    "AeolianLift": "14自然小调抬升回环",
    "MinorMediantMotion": "15小调中介阶段停顿",
    "CommonToneSideStep": "16共同音侧向转接",
}
FAMILY_MAP = {
    "PlateauRise": "PlateauLoop",
    "NeighbourPlateau": "PlateauLoop",
    "DorianPedal": "ModalPedal",
    "PhrygianPedal": "ModalPedal",
    "CommonToneSideStep": "ChromaticMediantBridge",
}

# This is the subset used by the supplied source chords. Unknown qualities fail
# loudly; no chord is simplified or silently downgraded.
QUALITIES = {
    "": (1, [0, 4, 7], []),
    "m": (2, [0, 3, 7], []),
    "7": (3, [0, 4, 7, 10], []),
    "maj7": (4, [0, 4, 7, 11], []),
    "m7": (5, [0, 3, 7, 10], []),
    "9": (3, [0, 4, 7, 10], [2]),
    "maj9": (4, [0, 4, 7, 11], [2]),
    "m9": (5, [0, 3, 7, 10], [2]),
    "m11": (5, [0, 3, 7, 10], [2, 5]),
    "6": (1, [0, 4, 7], [9]),
    "m6": (2, [0, 3, 7], [9]),
    "add9": (1, [0, 4, 7], [2]),
    "m7b5": (8, [0, 3, 6, 10], []),
    "mMaj7": (2, [0, 3, 7, 11], []),
    "sus4": (10, [0, 5, 7], []),
}
PITCH_CLASS = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
PITCH_CLASS_NAMES = ["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
MAJOR_DEGREES = [
    (1, 0), (2, -1), (2, 0), (3, -1), (3, 0), (4, 0),
    (4, 1), (5, 0), (6, -1), (6, 0), (7, -1), (7, 0),
]
# Degrees are relative to the stored A-minor tonic, not C-major-relative labels.
A_MINOR_DEGREES = [
    (3, 0), (3, 1), (4, 0), (4, 1), (5, 0), (6, 0),
    (6, 1), (7, 0), (7, 1), (1, 0), (2, -1), (2, 0),
]
SCALE_INTERVALS = {
    "major": [0, 2, 4, 5, 7, 9, 11],
    "minor": [0, 2, 3, 5, 7, 8, 10],
}
ROMAN = ["", "I", "II", "III", "IV", "V", "VI", "VII"]
ROOT_RE = re.compile(r"([A-G][b#]?)(.*)")

SOURCES = {
    "duinker-2019": {
        "title": "Plateau Loops and Hybrid Tonics in Recent Pop Music",
        "url": "https://www.mtosmt.org/issues/mto.19.25.4/mto.19.25.4.duinker.html",
        "scope": "Plateau-loop, hybrid-tonic, and absent-tonic concepts only; not a source for any complete G8 phrase.",
    },
    "hudson-wang-2025": {
        "title": "Chord Tone or Harmonic-Bass Divorce? An Enactive Approach to Hearing Pedals in Popular Music",
        "url": "https://www.mtosmt.org/issues/mto.25.31.4/mto.25.31.4.hudson_wang.html",
        "scope": "Pedal-sonority mechanisms only; not a source for any complete G8 phrase.",
    },
    "li-2026": {
        "title": "Modulation and Chromatic Mediants in Japanese Anime Songs",
        "url": "https://mtosmt.org/issues/mto.26.32.1/mto.26.32.1.li.php",
        "scope": "Anime-song chromatic-mediant and sectional-transition mechanisms only; not a source for any complete G8 phrase.",
    },
}
SOURCE_FOR_FAMILY = {
    "PlateauLoop": "duinker-2019",
    "AbsentTonic": "duinker-2019",
    "ModalPedal": "hudson-wang-2025",
    "ChangingPedal": "hudson-wang-2025",
    "ChromaticMediantBridge": "li-2026",
}
LOOPABLE_NAMES = {"NeighbourPlateau", "AeolianLift"}
ALLOWED_ENDINGS = {
    "open_unresolved", "suspended", "loop_return", "modal_development",
    "center_transition", "interim_pause",
}


def pitch(note: str) -> int:
    match = ROOT_RE.fullmatch(note)
    if not match or match[2]:
        raise ValueError(f"invalid note spelling: {note}")
    letter = match[1][0]
    accidental = match[1][1:]
    return (PITCH_CLASS[letter] + accidental.count("#") - accidental.count("b")) % 12


def mask(intervals: list[int]) -> int:
    return sum(1 << interval for interval in intervals)


def parse_chord(symbol: str, duration: float, index: int, tonic: str, mode: str) -> tuple[dict, dict, dict]:
    parts = symbol.split("/")
    if len(parts) > 2:
        raise ValueError(f"multiple slash separators are unsupported: {symbol}")
    chord_body = parts[0]
    match = ROOT_RE.fullmatch(chord_body)
    if not match:
        raise ValueError(f"invalid chord symbol: {symbol}")
    root_note, suffix = match.groups()
    if suffix not in QUALITIES:
        raise ValueError(f"unsupported chord quality {suffix!r} in {symbol}; no fallback applied")
    quality, base_intervals, color_intervals = QUALITIES[suffix]
    root_pc = pitch(root_note)
    bass_note = parts[1] if len(parts) == 2 else root_note
    bass_pc = pitch(bass_note)
    tonic_pc = pitch(tonic)
    if tonic_pc == 0 and mode == "major":
        degrees = MAJOR_DEGREES
    elif tonic_pc == 9 and mode == "minor":
        degrees = A_MINOR_DEGREES
    else:
        raise ValueError(f"unsupported G8 reference key: {tonic} {mode}")
    sounding_pcs = sorted({(root_pc + interval) % 12 for interval in base_intervals + color_intervals} | {bass_pc})

    runtime_event = {
        "degree": list(degrees[root_pc]),
        "target": None,
        "quality": quality,
        "function": 1 if root_pc == tonic_pc and quality in (1, 2, 4, 5) else 0,
        "roles": 1 | (256 if len(parts) == 2 else 0) | (512 if color_intervals else 0),
        "weight": 0.8,
        "durationQN": duration,
        "sourceIndex": index,
        "bassInterval": (bass_pc - root_pc) % 12 if len(parts) == 2 else None,
        "intervalMask": mask(base_intervals),
        "colorMask": mask(color_intervals),
        "suffix": suffix,
    }
    editorial_event = {
        "symbol": symbol,
        "rootNote": root_note,
        "rootPC": root_pc,
        "bassNote": bass_note,
        "bassPC": bass_pc,
        "pitchClasses": sounding_pcs,
        "notes": [{"pitchClass": pc, "canonicalName": PITCH_CLASS_NAMES[pc]} for pc in sounding_pcs],
        "baseIntervals": base_intervals,
        "colorIntervals": color_intervals,
        "durationQN": duration,
    }
    chord_event = {
        "name": symbol,
        "startQN": index * duration,
        "durationQN": duration,
        "open": False,
        "quality": quality,
        "source": 0,
        "root": root_pc,
        "bass": bass_pc,
        "keyNote": None,
        "bassNote": None,
        "rawId": None,
        "rawName": symbol,
        "rawKeyNote": None,
        "rawBassNote": None,
        "rawProjectTime": None,
        "rawTimeDomain": None,
        "mask": f"0x{mask(base_intervals + color_intervals):x}",
        "pitches": None,
        "type": None,
        "color": None,
    }
    return runtime_event, editorial_event, chord_event


def sequence_token(event: dict) -> str:
    degree, alteration = event["degree"]
    token = ("b" if alteration < 0 else "#" if alteration > 0 else "") + ROMAN[degree]
    if event["quality"] in (2, 5):
        token = token.lower()
    token += {
        3: "7", 4: "maj7", 5: "7", 6: "dim", 7: "dim7",
        8: "m7b5", 9: "sus2", 10: "sus4", 11: "aug",
    }.get(event["quality"], "")
    return token


def musical_signature(events: list[dict], meter: str) -> tuple:
    return (
        meter,
        tuple(
            (
                event["rootPC"], event["bassPC"], tuple(event["pitchClasses"]),
                tuple(event["baseIntervals"]), tuple(event["colorIntervals"]),
                float(event["durationQN"]),
            )
            for event in events
        ),
    )


def fingerprint(signature: tuple) -> str:
    canonical = json.dumps(signature, ensure_ascii=False, separators=(",", ":"))
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def validate_projection(
    ident: str,
    tonic: str,
    mode: str,
    runtime_events: list[dict],
    chord_events: list[dict],
    raw_chord_events: list[dict],
) -> None:
    tonic_pc = pitch(tonic)
    scale = SCALE_INTERVALS[mode]
    for index, (projected, chord, raw) in enumerate(zip(runtime_events, chord_events, raw_chord_events)):
        degree, alteration = projected["degree"]
        rebuilt_root = (tonic_pc + scale[degree - 1] + alteration) % 12
        if rebuilt_root != chord["rootPC"]:
            raise ValueError(
                f"{ident} event {index}: degree reconstructs root {rebuilt_root}, raw root is {chord['rootPC']}"
            )
        bass_interval = projected["bassInterval"] or 0
        rebuilt_bass = (rebuilt_root + bass_interval) % 12
        if rebuilt_bass != chord["bassPC"]:
            raise ValueError(
                f"{ident} event {index}: bass interval reconstructs {rebuilt_bass}, raw bass is {chord['bassPC']}"
            )
        projected_mask = projected["intervalMask"] | projected["colorMask"]
        raw_mask = int(raw["mask"], 16)
        if projected_mask != raw_mask:
            raise ValueError(
                f"{ident} event {index}: projection mask {projected_mask:#x}, raw pitch mask {raw_mask:#x}"
            )


def source_references(family: str) -> list[dict]:
    source_id = SOURCE_FOR_FAMILY.get(family)
    if source_id is None:
        return []
    source = SOURCES[source_id]
    return [{
        "sourceId": source_id,
        "url": source["url"],
        "locator": "Family or harmonic mechanism only; no whole-phrase transcription.",
        "support": "family-mechanism",
        "wholePhraseDirectSupport": False,
    }]


def encode() -> tuple[list[dict], dict]:
    candidates = []
    metadata_records = []
    seen_ids = set()
    seen_signatures = {}

    for index, (name, tonic, mode, ending, raw_chords) in enumerate(PHRASES, 1):
        family = FAMILY_MAP.get(name, name)
        name_zh = DISPLAY_NAMES_ZH[name]
        ident = f"V8_01_{index:03d}"
        if ending not in ALLOWED_ENDINGS:
            raise ValueError(f"{ident}: unsupported G8 ending label {ending}")
        if ident in seen_ids:
            raise ValueError(f"duplicate candidate ID: {ident}")
        seen_ids.add(ident)
        symbols = raw_chords.split()
        if len(symbols) != 8:
            raise ValueError(f"{ident}: expected 8 source chords, found {len(symbols)}")
        durations = [4.0] * len(symbols)
        runtime_events = []
        chord_events = []
        raw_chord_events = []
        for event_index, (symbol, duration) in enumerate(zip(symbols, durations)):
            runtime_event, editorial_event, raw_chord_event = parse_chord(
                symbol, duration, event_index, tonic, mode
            )
            runtime_events.append(runtime_event)
            chord_events.append(editorial_event)
            raw_chord_events.append(raw_chord_event)
        validate_projection(ident, tonic, mode, runtime_events, chord_events, raw_chord_events)

        # Match the V7 encoder's explicit secondary-dominant annotations.
        tonic_pc = pitch(tonic)
        for event_index, event in enumerate(runtime_events[:-1]):
            if event["quality"] == 3 and (chord_events[event_index + 1]["rootPC"] - chord_events[event_index]["rootPC"]) % 12 == 5:
                next_root = chord_events[event_index + 1]["rootPC"]
                event["function"] = 4 if next_root == tonic_pc else 6
                if next_root != tonic_pc:
                    event["target"] = runtime_events[event_index + 1]["degree"]
                    event["roles"] |= 16

        signature = musical_signature(chord_events, "4/4")
        previous = seen_signatures.get(signature)
        if previous:
            raise ValueError(f"duplicate G8 musical identity: {previous} and {ident}")
        seen_signatures[signature] = ident

        loopable = name in LOOPABLE_NAMES
        if (ending == "loop_return") != loopable:
            raise ValueError(f"{ident}: loop return label and loopability disagree")
        intent = "loop" if loopable else "develop"
        tags = [family, f"G8-ending-{ending}", "G8-editorial", "G8-complete"]
        built_in_tags = [intent, "common-minor" if mode == "minor" else "common-major"]
        techniques = []
        if any("/" in symbol for symbol in symbols):
            techniques.append("SlashBass")
        if any(event["colorMask"] for event in runtime_events):
            techniques.append("ExtendedColor")

        candidate = {
            "id": ident,
            "name": name_zh,
            "nameZh": name_zh,
            "nameEn": f"{ident} {name}",
            "aliases": ident,
            "builtInTags": ",".join(built_in_tags),
            "techniques": ",".join(techniques),
            "complexityLevel": "rich",
            "description": f"Original editorial complete phrase: {name} (family {family}).",
            "note": "No melody, voicing, recording, or user audition is included; index availability is untested.",
            "mode": mode,
            "sequence": " ".join(sequence_token(event) for event in runtime_events),
            "rhythm": " ".join("4" for _ in durations),
            "cadence": "none",
            "intent": intent,
            "loopable": loopable,
            # Required legacy runtime field, inherited from the V7 encoder.
            # It is not evidence of genre prevalence for these phrases.
            "styles": "pop:0.55,rock:0.35",
            "tags": ",".join(tags),
            "skeleton": ",".join(str(i) for i in range(len(runtime_events))),
            "meter": "4/4",
            "sourceType": "factory",
            "priorWeight": 0.55,
            "complexity": 0.65,
            "version": 8,
            "phraseLength": len(runtime_events),
            "secondaryIntents": 0,
            "typedData": json.dumps(
                {
                    "events": raw_chord_events,
                    "key": [pitch(tonic), 0 if mode == "major" else 1],
                    "full": runtime_events,
                    "tags": tags, "aliases": [ident],
                    "builtInTags": built_in_tags, "techniques": techniques,
                },
                ensure_ascii=False, separators=(",", ":"),
            ),
        }
        candidates.append(candidate)

        reference_list = source_references(family)
        support = (
            "Referenced source supports only the named family or harmonic mechanism; it does not support this complete phrase."
            if reference_list else
            "No external source claim; this complete phrase is original editorial composition."
        )
        if name == "NeighbourPlateau":
            boundary_basis = "Designed return from Gsus4 to the opening harmony through shared chord tones; loopability is editorial and index availability is untested."
        elif name == "AeolianLift":
            boundary_basis = "Designed return from Gsus4 to the opening Am9 through shared chord tones; loopability is editorial and index availability is untested."
        elif name == "MinorMediantMotion":
            boundary_basis = "Cm9 to Fm9 has local minor v-i arrival in F minor; classify as an interim pause, not an open ending. Later phrase availability is unconfirmed."
        else:
            boundary_basis = f"Primary editorial ending label is {ending}; the supplied final chord {symbols[-1]} has no final tonic-cadential closure. No automatic wraparound."
        identity_hash = fingerprint(signature)
        metadata = {
            "id": ident,
            "musicalIdentity": name,
            "nameZh": name_zh,
            "family": family,
            "submechanism": name,
            "rawChords": raw_chords,
            "chords": " ".join(symbols),
            "referenceTonic": tonic,
            "sourceCoordinateMode": mode,
            "meter": "4/4",
            "durationQN": durations,
            "chordEvents": chord_events,
            "formUse": ["complete-phrase"],
            "purpose": (
                "Complete phrase with a local F-minor v-i arrival and an interim pause; later continuation/use is unconfirmed."
                if name == "MinorMediantMotion" else
                "Complete eight-chord phrase; further index availability and recommendation use have not been tested."
            ),
            "ending": ending,
            "boundary": {"loop": loopable, "basis": boundary_basis},
            "openIncrementEligible": ending != "interim_pause",
            "rcrEligibilityStatus": "excluded-interim-pause" if ending == "interim_pause" else "not-assessed",
            "memberships": [],
            "modeAnnotationStatus": "not-reviewed; no new HCA membership emitted",
            "structure": f"Original eight-chord {name} phrase in the {family} family; all source event durations are 4 QN.",
            "sourceType": "original-editorial-design",
            "source": "Original editorial composition from the supplied raw chord sequence; not transcribed from a commercial work, score, recording, or MIDI.",
            "support": support,
            "sourceReferences": reference_list,
            "applicationCases": [],
            "styleEvidence": [{
                "style": "pop/rock runtime defaults",
                "level": "legacy-schema-default",
                "basis": "Reuses V7 runtime style weights; not a genre-frequency or source-work claim.",
            }],
            "reviewed": True,
            "editorialReviewStatus": "complete-phrase-structure-reviewed",
            "grade": "A",
            "status": "editorial-reviewed",
            "adoptionStatus": "approved",
            "auditionStatus": "pending-human",
            "indexAvailabilityStatus": "not-tested",
            "reviewRecord": [{
                "reviewer": "primary-agent",
                "decision": "Reviewed as an original complete phrase for editorial structure; this is not user listening approval or index-availability validation.",
            }],
            "uncertainties": "No melody, voicing, performance, user audition, or index test supplied.",
            "identityDefinition": "chordEvents + meter + durationQN; pitch classes in fixed C coordinate, referenceTonic names contextual center; no voicing octave is supplied.",
            "musicalIdentityFingerprint": identity_hash,
            "knownRelationships": [],
        }
        metadata_records.append(metadata)

    if len(candidates) != 16 or len(metadata_records) != 16:
        raise ValueError("expected exactly 16 G8 records")
    if len({item["family"] for item in metadata_records}) != 13:
        raise ValueError("expected 13 conservatively merged G8 families")
    open_records = [item for item in metadata_records if item["openIncrementEligible"]]
    if len(open_records) != 15 or len({item["family"] for item in open_records}) != 12:
        raise ValueError("expected 15 open/unresolved records across 12 families")
    if sum(sum(item["durationQN"]) for item in metadata_records) != 512.0:
        raise ValueError("expected sixteen phrases of 32 QN each")
    for candidate in candidates:
        typed = json.loads(candidate["typedData"])
        if len(typed["events"]) != 8 or len(typed["events"]) != len(typed["full"]):
            raise ValueError(f"{candidate['id']}: raw ChordEvent/full event count mismatch")
        expected_key = [0, 0] if candidate["mode"] == "major" else [9, 1]
        if typed["key"] != expected_key:
            raise ValueError(f"{candidate['id']}: raw chord key does not match its reference tonic")
    if any(item["memberships"] for item in metadata_records):
        raise ValueError("G8 phrase review must not emit unreviewed HCA memberships")

    if not V7_METADATA.exists():
        raise FileNotFoundError(f"cannot compare against V7 candidate metadata: {V7_METADATA}")
    v7 = json.loads(V7_METADATA.read_text(encoding="utf-8-sig"))
    v7_signatures = {
        musical_signature(item["chordEvents"], item["meter"]): item["id"]
        for item in v7["candidates"]
    }
    collisions = []
    for item in metadata_records:
        signature = musical_signature(item["chordEvents"], item["meter"])
        if signature in v7_signatures:
            collisions.append((item["id"], v7_signatures[signature]))
    if collisions:
        raise ValueError(f"G8 exact-identity collision with V7 increment: {collisions}")

    root_metadata = {
        "version": 8,
        "baseline": G7_BASELINE,
        "factoryStorageSchemaVersion": 2,
        "sources": SOURCES,
        "runtimeEncodingNotes": [
            "candidates.json contains only the 16 G8 additions; it does not duplicate V7.",
            "Runtime sourceType=factory is required by the existing Factory JSON parser; editorial sourceType is original-editorial-design in metadata.json.",
            "cadence is none for all phrases; editorial ending and boundary.loop are separate fields.",
            "The required legacy pop/rock style weights are inherited from V7 and make no source-work or genre-frequency claim.",
        ],
        "candidates": metadata_records,
    }
    return candidates, root_metadata


def main() -> None:
    candidates, metadata = encode()
    approved_ids = [item["id"] for item in metadata["candidates"] if item["reviewed"]]
    (OUT_DIR / "candidates.json").write_text(
        json.dumps(candidates, ensure_ascii=False, indent=2, allow_nan=False) + "\n",
        encoding="utf-8",
    )
    (OUT_DIR / "metadata.json").write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2, allow_nan=False) + "\n",
        encoding="utf-8",
    )
    (OUT_DIR / "approved-ids.txt").write_text("\n".join(approved_ids) + "\n", encoding="utf-8")

    family_counts = {}
    for item in metadata["candidates"]:
        family_counts[item["family"]] = family_counts.get(item["family"], 0) + 1
    print(f"records={len(candidates)} approved={len(approved_ids)} unique_identities={len({item['musicalIdentityFingerprint'] for item in metadata['candidates']})}")
    open_counts = {}
    for item in metadata["candidates"]:
        if item["openIncrementEligible"]:
            open_counts[item["family"]] = open_counts.get(item["family"], 0) + 1
    print(f"families={len(family_counts)} family_counts={json.dumps(family_counts, ensure_ascii=False, sort_keys=True)}")
    print(f"open_unresolved={sum(open_counts.values())} open_families={len(open_counts)} ending_interim_pause={sum(item['ending'] == 'interim_pause' for item in metadata['candidates'])}")
    print("events=128 duration_per_phrase_qn=32 total_duration_qn=512 hca_memberships=0 v7_increment_identity_collisions=0")
    print("fingerprints:")
    for item in metadata["candidates"]:
        print(f"{item['id']}\t{item['family']}\t{item['musicalIdentityFingerprint']}")


if __name__ == "__main__":
    main()
