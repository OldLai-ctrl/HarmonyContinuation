# Phase 3 verification record

## Scope and status

Phase 2 Cubase smoke: **NOT TESTED in this run**. The user was away from the machine. The prior Phase 2 core commit is `3cefa0a`; no commit named `phase2-stable-matcher-validated` is created, because the requested Cubase smoke prerequisite is unmet. Phase 3 Cubase RECOMMEND view: **NOT TESTED in Cubase**. Standalone editor launch is a narrower check.

## Automated checks

- Factory compiler: 161 sources in eight JSON files, schema field checks, duplicate checks, and `factory.db` readback PASS. Similar skeleton variants are reported for review rather than automatically removed.
- `ContinuationEngineTests`: 42/42 assertions PASS. This includes exact and middle continuation, secondary insertion, OPEN duration, rhythm scaling, no suffix, one loop wrap, intent groups, concrete chord spelling, dedup/support, style and intent preference, ambiguous and forced key interpretation, user database CRUD, factory/user isolation, version/type rejection, malformed JSON metadata, stale worker suppression, and an unexplained-chord score comparison.
- Full plugin CTest: 7/7 PASS after Phase 3 integration.
- SDK VST3 Validator: 47/47 PASS after Phase 3 integration.
- Standalone SDK EditorHost: plugin editor process remained responsive four seconds after launch. This checks editor opening, not drag/playback behavior in Cubase.
- Bundle integrity: generated `factory.db` and the copy under `Contents/Resources` have equal SHA-256. The updated install verifier returned MATCH when both files matched and exit code 2 when the installed resource was deliberately absent. The privileged install script was not run against Program Files in this round.
- CLI user save smoke: a phrase starting at project QN 32 persisted as relative durations and skeleton indices, had no absolute position field in SQLite, and appeared as a supporting user template in a later recommendation.

## Full pipeline benchmark

Debug MSVC x64, query C → Am → A7 → Dm OPEN. Timings include query analysis, in memory index construction, Phase 2 prefilter and alignment, continuation extraction, grouping, deduplication, and ranking. They are observations, not absolute performance gates.

| Templates | Total | Match | Shortlist | Visible candidates |
|---:|---:|---:|---:|---:|
| 161 factory | 11.3 ms | 8.2 ms | 161 | 7 |
| 1,000 synthetic | 56.4 ms | 46.3 ms | 800 | 7 |
| 10,000 synthetic | 176.1 ms | 90.6 ms | 800 | 7 |

The 1k and 10k corpora repeat and shift the seed catalogue for load testing. They are **not** additional curated library entries. No audio thread processing is part of these timings.

## Music cases

Eight CLI runs with all available Resolve, Develop, Loop, and Color candidates, scores, concrete durations, OPEN holds, key interpretation, style, cadence, support counts, and source template IDs are in [PHASE3_CASES.md](PHASE3_CASES.md). The final section there lists results requiring human music review. Empty groups are intentional when no candidate clears the threshold.

## Remaining host gate

When the user returns, install and hash check a chosen build, then repeat the Phase 2 drag/playback smoke and Phase 3 RECOMMEND interaction in Cubase. Validator and EditorHost cannot establish Cubase host behavior or sound musical quality.
