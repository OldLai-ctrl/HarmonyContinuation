# Preview verification

Build with CMake, then run `ctest --test-dir build-vst3 -C Debug --output-on-failure` and `build-vst3/preview_cli.exe --bench`.

`PreviewTests` checks: absolute-to-relative positions, exact closed durations, OPEN suggestion and median/default fallback, boundary, tempo conversion and clamps, 44.1/48/96 kHz sample timing, essential chord notes, slash bass, extensions, greedy motion bound, finite/clamped/deterministic audio, stop/replace/finish, a counted no-allocation render block, schema version and pin rematch/drop. `ProductizationTests` and all earlier tests remain in the same CTest run.

The benchmark renders 10, 32 and 64 one-QN chords at 44.1 and 48 kHz, reporting elapsed wall time, peak voices and peak amplitude. Timings are observations, not a machine-independent acceptance threshold. No assertion about subjective sound or Cubase output can be made from these tests.

The VST3 SDK validator and EditorHost must still run on the built plugin. Cubase drag, DPI, state restore and playhead tests are deferred until a host is available. The eight golden WAV files are generated locally and are deliberately ignored by Git through the build-directory ignore rule.
