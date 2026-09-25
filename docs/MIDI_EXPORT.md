# Portable MIDI export (Phase 4B Core)

## Architecture

`preview::buildSequence` is the single source of chord order, phrase-relative quarter-note (QN) positions, existing durations, OPEN-chord fallback, and recommendation timing. `midi::buildClip` turns that sequence into a host-independent `ExportSequence` of notes and markers. `midi::writeToMemory` serializes the sequence; `writeToFile` saves those bytes. `MidiClipPayload` keeps the bytes, suggested filename, note/chord counts, and optional boundary tick together for future consumers. The core has no VST3, VSTGUI, Win32, or audio-thread dependency.

Library items use `previewFromTemplate` and then the same clip builder and writer. The UI only invokes these APIs. No MIDI synthesis parameters or preview envelope are encoded.

## Arrangements and scopes

- **Block Chords:** one bass note (MIDI 36–48) plus each pitch class of the chord in MIDI 60–71. All begin at the chord start and last for its full duration. Upper velocity is 80; bass velocity is 90.
- **Voice Led:** calls the existing `preview::ChordVoicer` for every chord, including preceding chords in a suffix-only export. This preserves the voicing path of the full preview. Notes have the same nominal chord durations as the preview schedule.
- **Full Phrase** (default): current phrase and selected continuation.
- **Current Only:** current phrase. The Demo and CLI build its preview without a candidate so an OPEN ending uses the established fallback duration.
- **Continuation Only:** recommendation suffix with its first chord shifted to QN 0. The full phrase is still voiced before selecting the suffix.

All clips start at QN 0, regardless of the Cubase project position. QN is rounded to ticks using `round(QN × PPQ)`. There is no random timing or velocity.

## SMF layout

The writer produces Standard MIDI File **Format 1**, with **480 PPQ** by default:

| Track | Contents |
| --- | --- |
| 0 | Tempo (`FF 51`), time signature (`FF 58`), optional major/minor key signature (`FF 59`), intent text, chord labels as text events, recommended-start marker (`FF 06`), end of track |
| 1 | Channel 1 note-on/note-off events and end of track |

The recommended-start marker text is `HarmonyContinuation: Recommended Start`; it appears at the first suffix chord (QN 0 for suffix-only export). Chord labels are stored at each chord start. DAW display of those labels is host-dependent. At a shared tick, note-off is written before note-on, so repeated pitches form separate notes without a stuck note. Notes, tempo, meter, pitch range, channel, event length, and total duration are validated before bytes are returned. Output for identical inputs is byte-identical.

Key-signature metadata supports major and minor using the selected tonic; enharmonic notation follows the current pitch-class mapping. It does not alter chord pitches.

## Entry points

The standalone Demo offers a recommendation-card `MIDI` button, a top-level `Current MIDI` button, and `Export MIDI` in the library inspector. Files are saved through the Windows Save dialog. `midi_export_cli --case A --group Resolve --candidate 1 --mode voice-led --scope full --output caseA.mid` is the command-line equivalent. `--scenario` accepts a scenario JSON path; `--debug` prints notes. The CLI also accepts `--snapshot-in` and `--snapshot-out` for frozen recommendations.

The VST3 plugin does not expose a host file dialog or MIDI drag in this phase. A future drag source can consume `MidiClipPayload.smfBytes` and `suggestedFilename` without reimplementing MIDI serialization. Cubase import, drag-and-drop, Chord Track conversion, and host restoration still require a real Cubase session to verify.
