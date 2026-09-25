# Recommendation snapshot (Phase 4B Core)

`RecommendationSnapshot` is an explicit, portable JSON capture of one already computed recommendation. It is independent of `PluginSessionState` and `user.db`. The Demo saves it with a recommendation card's `SNAP` button and opens it with `Open Snapshot`. After opening, the saved candidate is used directly for audition and MIDI export; it need not be found again in the current factory library.

The `.hcrec.json` file contains `schemaVersion: 1`, tempo, fixed meter, selected key/style/intent, phrase-relative existing chords, the complete continuation and suggested OPEN duration, source template identifiers, rank-related values, and available match alignment data. It stores identifiers and trace information, not complete factory templates. Snapshot capture subtracts the imported phrase's first project position, so the portable phrase starts at QN 0.

The reader validates sizes, values, phrase timing, candidate data, and a rebuilt preview. Unknown schema versions return `UnsupportedVersion`. A valid snapshot can be passed to `midi_export_cli --snapshot-in file.hcrec.json --mode voice-led --scope full --output restored.mid`; normal CLI runs can write one using `--snapshot-out file.hcrec.json`.

This is a development/reproduction file, not an automatic project save. It does not update recommendation weights or user-library data. Rating JSON export is deferred; Phase 4B does not collect or learn from ratings.
