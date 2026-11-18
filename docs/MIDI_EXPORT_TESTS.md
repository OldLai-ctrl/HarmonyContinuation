# Phase 4B MIDI export checks

Run `MidiExportTests` after building `build-vst3`, then `ctest --test-dir build-vst3 --output-on-failure` from a Visual Studio developer shell. The test executable includes a minimal test-only SMF reader: it parses the generated tracks and asserts actual metadata and MIDI events rather than relying on file size.

The checks cover Format 1 and 480 PPQ; Golden A (14 QN, 6720 ticks); tempo, meter, major/minor key signature, intent, chord labels, and boundary marker; unchanged existing durations and OPEN fallback; fractional QN rounding; same-tick note-off before note-on; MIDI pitch range and invalid-input rejection; block-chord pitch classes including diminished, half-diminished, and slash-bass examples; voice-led pitches against the preview voicer/scheduler; all three scopes; phrase-relative starts; deterministic bytes; snapshot file and JSON roundtrip, frozen candidate, and unsupported versions; all 161 factory entries and a user-library entry through the shared exporter; and A–H in both arrangements.

CLI regression generation:

```powershell
& .\build-vst3\midi_export_cli.exe --case A --group Resolve --candidate 1 --mode block --scope full --output build-vst3\caseA-block.mid
& .\build-vst3\midi_export_cli.exe --case A --group Resolve --candidate 1 --mode voice-led --scope full --output build-vst3\caseA-voice.mid
```

The A–H fixture batch writes 8 Block Chords and 8 Voice Led files. Demo UI smoke checks click the recommendation MIDI and SNAP buttons and reopen a snapshot in a hidden Demo window. These checks establish local generation and UI responsiveness, but cannot prove Cubase will show metadata, accept a future drag payload, or restore it in a project. Those remain host tests.
