# Factory Library V4 development

## Scope and compatibility

V4 is opt-in development content on `library/v4`, based on the reliable post-release
main checkout. Released v0.9.0, its tag, Setup and Factory V3 source files remain
unchanged. V4 has 629 inherited entries plus 28 Phase 1 A/B candidates: **657**.
The compiler rejects exact musical duplicates and also rejects new root/quality/bass
connection families repeated from V3 or other additions, ignoring rhythm, optional
major/minor sevenths, extension masks and semantic target annotations for that check.

Storage formats remain Factory 2, User 2, Session 5, Continuation Snapshot 3 and
Enrichment Snapshot 2. **Use V4 with the engine on this branch, identified as
0.9.0-factory-v4.1 or a subsequent compatible build.** The released v0.9.0 loader
can parse Schema 2 but lacks the explicit-bass matching and complete exact-mask
voicing changes. V4 is not approved as a drop-in replacement for its production DB.
Do not activate this development DB in the production ProgramData store.

Only builds configured with `HC_FACTORY_LIBRARY_VERSION=4` prefer their bundled V4
over an older installed Factory. Production defaults remain Library 3. No global
active pointer or installed package is changed. No second user-facing installer.

## Sources and build

- `data/factory/` and `data/factory-v3/`: immutable input to V4 compilation.
- `data/factory-v4/candidates.json`: reviewed, complete Schema 2 additions.
- `tools/generate_v4_catalog.py`: explicit original composition and typed-data
  authoring recipe; no random generation, rotations or transposition copies.
- `tools/factory_v4_compiler.cpp`: canonicalizes V3, applies bounded semantic
  maintenance to copies, checks old sounding identities, rejects musical duplicates,
  compiles Library 4 and validates exact typed-data and metadata readback.
- `data/review/factory-v4-capabilities.json`: all eight C proposals, excluded from
  compilation. No fixed voice tracking or segmented tonal engine is claimed.

Configure a separate build with `-DHC_FACTORY_LIBRARY_VERSION=4
-DHC_PRERELEASE=factory-v4.1`. Use the existing offline SDK for a plugin build and
disable `SMTG_RUN_VST_VALIDATOR`. Build only `factory_database`, `FactoryV4Tests`
and the consumers needed for development. The database is `<build>/factory.db`;
the compiler writes `<build>/factory-v4-maintenance.tsv`. Never send V4 output to
the release staging directory or to `LIBRARY_V3_SUMMARY.md`.

## Phase 1 fidelity and adjustments

The original A01-A14/B01-B14 definitions were recovered from the earlier Phase 1
answer in this conversation; the repository had no saved candidate catalog.
The authoring recipe preserves 27 routes. A14 deliberately uses Db/F (N6) instead
of root-position Db, to cover the explicitly requested Neapolitan sixth.
B08 uses the complete eight-chord ending already specified in Phase 1. A09
retains root-position Db, so N6 and ordinary flat II remain distinct. A08 uses a
minor tonic and major IV for Dorian color but does not establish a modal-key model.
GospelCandidate and NeoSoulCandidate remain editorial hypotheses, not listening
approval. B12 is same-quality chord movement, not guaranteed strict Planing.

Bass notation `@n` in the recipe means the root-relative `bassInterval` in 0..11;
it is lowered to typedData, never parsed as a secondary-target slash. Rich ninths
and thirteenths use explicit interval masks and display suffixes with parseable
seed qualities. Every new phrase retains all events in its skeleton. Purposeful
rhythms shorten passing diminished chords and Side-slipping excursions; no extra
rows are counted for rhythm-only variations.

The E7-A7-D7-G7-C chain targets actual following roots A-D-G-C. E7-A7-Dm-G7-C
retains its distinct D-minor destination. Tritone substitutions carry their actual
target and Substitution role instead of being mislabeled as secondary dominants.

## Local engine adaptations

- Match queries retain normalized bass and pitch sets. An explicitly authored
  template bass or mask contributes a local mismatch cost; legacy templates with
  unspecified realization retain their previous costs. V4 recommendations reject
  an aligned, explicitly contradictory bass. Unknown input bass remains allowed.
- Queries with specified non-root bass or exact masks conservatively retain their
  full connection path as the matching skeleton, including short diminished nodes.
  Ordinary unspecified input retains the existing harmonic-analysis skeleton.
- Analysis recognizes an observed fifth-related dominant-to-dominant chain.
  A still-open final dominant can match a template's subsequent secondary target
  without pretending that the target has already been observed in the input.
- Candidate diversity includes realization so slash bass and exact colors are not
  collapsed into ordinary same-root alternatives. Continuation realization already
  carries typed harmonicData into preview, snapshots and MIDI.
- Explicit pitch sets keep all authored chord tones in the shared preview/MIDI
  voicer. Ordinary unspecified chords retain their previous four-upper-note policy.
- Enrichment remains independent of Factory template recommendation. Its destructive
  replacement/inversion and insertion operations conservatively avoid explicit
  bass/mask connections. It may offer no rewrite for an already exact phrase;
  that is preferable to erasing the defining data. No new Factory-driven Enrichment
  generator is introduced.
- Candidate labels show actual slash bass and rich suffixes. Bilingual Why text
  states when a continuation preserves specified bass or exact chord tones.

## V4-only metadata maintenance

No requirement to fill every one of the 294 empty technique lists. Add only
structure-backed ModalMixture, DeceptiveResolution and OpenEnding, plus Backdoor,
TritoneSubstitution and SecondaryLeadingTone where the actual structure supports it.
The ten audited V3_SPARK #ii/#iv diminished approaches get correct next targets,
ChromaticDominant function and SecondaryLeadingTone roles; their Borrowed role is
removed. Existing aliases and historical technique strings remain available.
Thirteen omitted secondary-leading diminished events are restored to skeletons.
Pitches, bass, durations and existing IDs remain unchanged. Each affected ID and
change category is recorded in the compiler's maintenance TSV.

## Technique vocabulary

Use free technique strings without expanding the storage schema or fixed built-in
tag enum. Main families: ChromaticRootAsc/Desc, ChromaticBassAsc/Desc, SlashBass,
PedalPoint, DominantChain, SequentialTonicization, SecondaryDominant,
SecondaryLeadingTone, PassingDiminishedAsc/Desc, TritoneSubstitution, Backdoor,
ModalMixture, Neapolitan, NeapolitanSixth, SideSlipping, SameQualityMotion,
ChromaticMediant, OpenEnding, DeceptiveResolution and EvadedResolution.
CommonToneConnection describes available harmonic common tones, not a locked voice.
No new special-card UI is included.

## Deferred capabilities

C01/C02: same-inner-voice identity, register and line constraints.
C03/C04: sustained common tones in specified voices.
C05/C06: key segments, pivot dual function and established modulation confirmation.
C07: fixed voicing and exact parallel voice movement.
C08: phrase boundary and cross-phrase resolution association.
These remain eight deferred proposals, not effective additions.

## Verification results

The opt-in development build compiles Library 4 / Schema 2: 629 inherited + 28 new,
zero exact duplicates and zero new connection-family duplicates. All inherited
pitch, rhythm and bass identities pass preservation checks; typed data and metadata
round-trip through the compiled DB.

Maintenance affects **139 unique inherited records**: 138 technique lists change,
57 of the 294 empty lists get evidence-backed tags (237 remain empty), 10 target /
role / function corrections and 13 skeleton restorations. These overlapping counts
must not be added together. Per-ID evidence: `<build>/factory-v4-maintenance.tsv`.

Focused recommendation queries use a forced C-major key, the stated prefix and
the template's preferred intent, without a style filter. Ordinary limits mean
three candidates per group. Results below refer only to these particular inputs.

| Template | Input prefix length | Limit per group | Visible rank | Result |
| --- | ---: | ---: | ---: | --- |
| B01 | 2 | 3 | 1 | Remaining D7–G7–C intact |
| B02 | 3 | 3 | 3 | Remaining A7–Dm–G7–C intact |
| B08 | 2 | 3 | 1 | Remaining descending bass and cadence intact |
| B08 in D | 2 | 3 | 1 | Transposed bass and pitches intact |
| B09 | 4 | **100** | **5** | Matches and renders; absent from default top three |
| B10 | 3 | 3 | 2 | C pedal intact |
| B11 | 2 | 3 | 1 | Returns from Dbmaj9 to Cmaj9 before the cadence |
| B04 | 3 | 3 | 2 | Eb7 resolves to Dmaj7, followed by home-key cadence |

Every listed recommendation passes continuation-length/identity checks, Snapshot 3
round-trip, preview construction and non-silent synth rendering, VoiceLed MIDI
bass/exact-tone checks and SMF write/read pitch/timing checks. Wrong-bass and
wrong-color comparisons test discrimination; explicit contradictory V4 bass is
rejected. Enrichment protection is checked for B08/B11. No individual default-top-3
reachability claim is made for the other 21 additions. B09 is ranking-limited for
the tested prefix, not a proven unreachable candidate or a data compilation failure.

All 28 new skeletons retain every authored event, including N6 bass, diminished
connectors and secondary targets. This is conservative path preservation, not a
new voice-leading or modulation representation.

The Windows x64 development VST3 compiles with the local offline SDK and validator
disabled. Direct regressions pass: Matcher 40/40, Continuation 42/42, Preview 75,
MIDI export 578, and Enrichment's 30 existing cases (159 candidates with preview,
MIDI and snapshot checks). This is the directly affected test executable, not a
full enrichment benchmark run. Continuation initially failed its temporary-file
atomic replacement inside the sandbox's default TEMP; rerunning with TEMP/TMP
scoped to `<build>/test-tmp` passes without a production persistence change.

The focused test also verifies bilingual realization Why and isolated development
bundle selection while retaining a Library 3 active pointer and historical DB.
No actual DAW GUI interaction or human listening approval is claimed. Evidence is
`<build>/FactoryV4Tests.log`, the five named regression logs, `plugin-configure.log`
and `plugin-build.log`; generated evidence is not committed as source.
No full CTest, validator suite, DAW matrix or broad music/performance benchmark.
