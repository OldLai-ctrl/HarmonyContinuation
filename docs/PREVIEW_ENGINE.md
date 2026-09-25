# Preview core (Phase 4A)

`harmony_preview` depends only on `harmony_core`. It has no VST3, VSTGUI, Win32, database, recommendation-ranking or host-process dependency. The VST3 processor bus and category are unchanged.

## Sequence

`buildSequence(imported, candidate, tempo)` keeps the exact durations and offsets of all closed imported chords, subtracts the first imported absolute QN from each start, and appends the candidate after the current chord. The final OPEN chord uses the candidate's finite positive suggested total hold, else the median of known closed chords marked structural by the existing harmony analysis. If no structural duration is available it uses all known closed durations, then 4 QN. Invalid closed durations are rejected. Tempo is clamped to 20–400 BPM; nonfinite or nonpositive tempo becomes 120 BPM. A missing candidate produces an existing-phrase-only preview. `recommendationBoundary` is the first recommended event index.

QN conversion uses `QN * 60 / BPM`; sample positions are rounded with the supplied sample rate. The scheduler is sample based, with event start and end samples precomputed on `start`. No Cubase absolute position is passed into rendering.

## Voicing

`ChordVoicer` parses concrete labels and uses the existing chord normalizer for interval/quality interpretation. A slash label overrides the bass pitch class. Bass sits at MIDI 36–48; three or four upper notes are chosen between MIDI 48–84. Identity notes (third, seventh, diminished fifth/seventh) are selected before extensions, fifth and doubled root. Small inversion/octave options are scored greedily against the prior chord by movement, large leaps, bass motion and spread. No random choices are made.

The current accepted label set includes major/minor, 6, 7, maj7, m7, 9, maj9, m9, 7b9, 7#9, 13, sus2, sus4, aug, dim, dim7 and m7b5. A realized secondary or borrowed chord is treated just like any other concrete label.

## Sound and scheduler

`PreviewSynth` uses 85% sine and 15% triangle with a 10 ms attack, 100 ms release, 16 fixed voices and quietest-voice stealing. Its output uses gain 0.22 divided by the square root of active voice count, with a final 0.95 safety clamp. It exposes `prepare`, `start`, `stop`, `replace`, `process`, `reset`, sample/QN position, finished state and peak voice count. Start/replace allocate and construct the schedule; `process` uses fixed voices and precomputed event/end cursors and performs no heap allocation. Stop prevents future note-ons and releases active voices. The complete existing plus recommended phrase plays once.

`renderOffline` runs the same engine in 512-frame blocks, returns stereo float samples and peak/RMS/voice metrics, then `writeWav16` writes stereo PCM WAV. Offline allocations are allowed and output is limited to ten minutes. It is a development tool, not a production plugin dependency.

The demo uses the independent preview library plus Win32 `PlaySound` in `DemoMain.cpp`; Windows types do not enter the preview library. The later VST3 audio stage can call this same `prepare/start/process/stop` API from its own adapter, after Cubase testing is available. Preview playback state is never serialized into the plugin session.
