# Phase 6 development checkpoint (0.6.0-dev.1)

This branch starts at the Phase 5 baseline `580c67c7986aee740dd487e86cdb6973ab8351b0`. It is a development build, not a validated Cubase release.

## Implemented

- One product version source generates the VST3 class/module versions and the About/CLI version strings. The current development version is `0.6.0-dev.1`.
- `zh-CN` is the default interface language, with `en-US` selectable in More. Session state persists locale and Continue/Enrich mode. Session schema is 4 because Phase 5 already used schema 3; versions 1–3 remain readable.
- All 161 factory entries have Chinese/English display names, aliases, stable built-in tag IDs, technique IDs and complexity metadata. Factory library version is 2. The SQLite schema remains 1; legacy user-library rows are read and updated in place without dropping free tags or notes.
- The independent enrichment engine generates Polish, Rich and Advanced variants with operation explanations, identity scoring and edit limits. The Demo can preview, export MIDI and save enrichment snapshots. The VST3 UI shows enrichment results; audio preview inside Cubase remains out of scope for Phase 6.
- Continuation snapshots now use schema 2 with a product version field and still read schema 1. Enrichment snapshots use a separate schema 1.

## Automated checks

- Full CTest: 13/13 passed. Enrichment suite: 30 cases with preview, MIDI and snapshot round trips.
- VST3 SDK Validator: 47/47 passed.
- Phase 5 continuation comparison: 42 cases, 214 candidates and **zero changed results**. The existing three structural benchmark errors remain; the benchmark CLI therefore exits 1, while the baseline comparison exits 0.
- EditorHost and Demo process smoke tests: both remained responsive six seconds after launch. This does not establish Cubase stability or visual correctness.

## Remaining release gates

- Review the 161 display names and inspect the Chinese interface visually at compact and expanded window sizes.
- Manually test the new build in Cubase: loading, chord-track drag, both modes, playback, resize, user-library editing and session restoration. Record any host crash or hang before considering an RC.
- Pin/compare in Enrich mode and additional borrowed/substitution transformations are still incomplete. Keep the version at `dev.1` until these and the product review are resolved.
