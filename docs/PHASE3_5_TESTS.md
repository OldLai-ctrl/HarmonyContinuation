# Phase 3.5 verification, 2026-09-25

Baseline: `phase3-core-stable` points to `6d0561c`. Work is isolated on `phase3.5/productization`. No `src/core` or `data/factory` file was changed.

- Full MSVC/VST3 build: PASS.
- CTest: 8/8 PASS. New `ProductizationTests` reports 54 checks, including Auto/Forced key, style and intent state, view-only invalidation, three-pin limit/unpin, state round trip and malformed state, version mismatch recomputation, visibility thresholds/diversity/empty group, ranking-only worker reuse, relative-time recommendation saving and user library CRUD, ambiguous key, and A–H full core pipelines plus save conversion.
- VST3 Validator: 47/47 PASS.
- SDK EditorHost: plugin process opened and remained responsive for four seconds. This does not establish Cubase behavior.
- HarmonyContinuationDemo: Windows process created a real VSTGUI child window and remained responsive with Case D. Each Case A–H launched as a separate hidden GUI process and remained alive/responsive. Headless case-switch automation was inconclusive, so clicking the dropdown still needs visual manual QA.
- Idle demo process: over a three-second paused interval, reported additional process CPU time was 0 seconds. This is a short smoke test, not a comprehensive performance profile.
- `factory.db` and bundled `Contents/Resources/factory.db` SHA-256 matched.
- `git diff --check`: clean after staging.

Pending, because no Cubase machine/use is available in this stage: install and open in Cubase, drag a Chord Track phrase, key/style/intent clicks, playback/highlight, UI save/edit/delete, project close/reopen state recovery, screen/DPI visual review, and any musical listening judgment. The Demo uses the same view/core and can be used for pre-host visual QA, but hidden-window capture returned black, so no visual screenshot claim is made. No Phase 4 audio, audition, MIDI, drag-back, or ranking retuning was attempted.
