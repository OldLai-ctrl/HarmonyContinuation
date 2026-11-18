# Offline audition

From the repository root after building:

```powershell
.\build-vst3\preview_cli.exe --case A --group Resolve --candidate 1 --output .\build-vst3\caseA_resolve1.wav
.\build-vst3\preview_cli.exe --scenario .\tests\fixtures\demo\case_a.json --group Resolve --candidate 1 --output .\build-vst3\preview.wav
.\build-vst3\preview_cli.exe --golden-dir .\build-vst3\preview-golden
.\build-vst3\preview_cli.exe --bench
```

`--case` / `--scenario` runs the actual recommendation engine against the unchanged 161-item factory library; groups are `Resolve`, `Develop`, `Loop`, `Color`, and candidate numbers start at 1. The console prints tempo, each chord's relative QN position/duration and MIDI voicing, then duration, peak, RMS, peak voices and output path. `--sample-rate 44100` (or 48000/96000) overrides the default 48000.

`--golden-dir` creates deterministic, explicit audition fixtures rather than asserting that the current recommender ranks these exact paths first: A C–Am–Dm–G–C; B C–Am–A7–Dm–G–C; C Dm7–G7–Cmaj7; D C–F–Fm–C; E C–G–Am–F–C; F Am–F–Dm–E7–Am; G C–Am–F#–Dm–G–C; H C–Am–F–G–C. Each chord has two QN at 120 BPM. The WAV files stay under the ignored build directory and are not committed.

In `HarmonyContinuationDemo.exe`, load Case A–H and click ▶ on a recommendation card to hear the full imported phrase followed by that suggestion. Click ■ to stop, or click another card to replace the playing phrase. The mini timeline shows the existing/recommended boundary and a playhead. The demo alone uses Win32 WAV playback and does not alter plugin audio buses.

Later listening review should score naturalness, intent fit, rhythm fit and distinctiveness, with comments. `AuditionRating` is a development-only struct; ratings are not written to `user.db`.
