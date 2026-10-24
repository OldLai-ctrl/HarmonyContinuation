# B3 engineering evidence

Engineering was built and tested from repository HEAD d9d4539 with an isolated VS 2026 Release build at engineering/build. The final focused test passed after the B3 test changes. No installation, GUI session, commit, or push was performed.

The compiler accepted 170 candidates into the proposal database: 828 total entries including the 658-entry V5 baseline. Metadata grades were A/B/C = 152/3/15; 132 A sources were approved into the 790-entry experiment database and 38 remained unapproved. The original four pending sources remained pending, the original nine candidates without mode membership remained C and unadopted, and no unapproved IDs entered the experiment database.

The three negative contract cases were rerun against the frozen metadata and each exited with code 1 and its expected rejection:

- Adding B1_025 (grade B) to approved IDs: rejected as a pending candidate.
- Setting B1_025 to approved while it remained grade B: rejected for grade/adoption disagreement.
- Downgrading B3_01_001 from A to B without labelActions: rejected for the missing grade-change audit.

The focused test passed 1/1. Recall found every approved source across 517 legal-prefix cases and 1,584 source-recall transpositions. The 49 approved minor sources also passed fixed-C-minor recall. At the last non-full prefix, 114/132 approved sources qualified. The tested qualification scope, including targeted sources checked at every legal prefix, found qualification for 121/132; this is the observed tested scope, not a ceiling. Snapshot source search found a legal ranked prefix for all 132 sources and restored a playable preview after serialization.

Per-source qualification at the last non-full prefix, with the default Top-3 visibility separated from More:

| Batch | Approved | Qualified | Top-3 visible | Qualified beyond Top-3 | Not qualified at that prefix |
|---|---:|---:|---:|---:|---:|
| B1 | 45 | 39 | 38 | 1 | 6 |
| B3 | 87 | 75 | 75 | 0 | 12 |

The one qualified B1 source beyond Top-3 was B1_038 in Resolve at rank 7, available through More. Qualified source counts by Continue group were:

| Batch | Loop | Resolve | Develop | Color |
|---|---:|---:|---:|---:|
| B1 qualified / Top-3 | 24 / 24 | 12 / 11 | 1 / 1 | 2 / 2 |
| B3 qualified / Top-3 | 48 / 48 | 23 / 23 | 2 / 2 | 2 / 2 |

The four new representatives were checked over all 12 tonic transpositions using the longest C-key prefix at which the source appeared in the same legal-prefix search used by Snapshot. Candidate display IDs were excluded; ordered group, primary/supporting source sequences, and match/eligibility/ranking scores were compared (similarity tolerance 0.001; score tolerance 0.05). All four sources appeared at all 12 keys; repeated identical inputs were stable at 12/12 keys. Prefixes were B3_01_048=3, B3_01_056=7, B3_01_065=5, B3_01_071=1. Source sequences and scores matched at every key.

All 132 approved full routes passed pitch-mask and Standard MIDI File round-trip checks. Offline Preview audio rendered for all 132, covering 781 chord events. Snapshot/Preview source selection, serialization, restoration, and preview rebuild passed for each approved source. This verifies local APIs and rendered media, not GUI interaction or manual listening of every source.

The Enrich API ran once for each approved source and returned candidates for all 132: 528 candidates total (polish/rich/advanced = 141/306/81); no sources were no-ops. Diagnostics covered 781 protected chords, 623 insertion attempts, 317 failed insertions, 26 duplicates, and zero hard-filtered edits. Failed insertion attempts were explained by the default rules finding no safe unique edit; they were not API errors.

Wish mode pools were measured from the actual 790-entry Factory V6 experiment library at fixed tonic C. Non-strict / strict counts were: Ionian 487/238; Dorian 25/24; Phrygian 13/13; Lydian 20/20; Mixolydian 24/24; Aeolian 115/65; Locrian 0/0; harmonic minor 123/33; ascending melodic minor 6/6; classical melodic minor 0/0. Locrian and classical melodic minor had zero reviewed sources in their mode annotations. This tests the Wish API pool, not its GUI.

The real retrieval gate compared the same 152 grade-A query sources against capped and full scans:

| Pool | Entries / cap inputs | Full-only raw matches | Query-source misses | Full Top-20 retained | Default qualified / visible | Cap median / p95 ms | Full median / p95 ms | Gate |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| Baseline + all grade-A shadow | 810 / 800 | 0 | 0 | 3040/3040 | 128/152, 127/152 | 2.960 / 6.2614 | 2.4906 / 5.0059 | STOP |
| Active experiment | 790 / 790 | 0 | 0 | 3040/3040 | 114/152, 113/152 | 2.5698 / 5.6318 | 2.4662 / 5.4382 | OPEN |

The 810-entry shadow exceeds the 800-source shortlist cap, so the cap structurally truncates the candidate input. This run observed no full-only returned matches, no query-source misses, and no Top-20 loss. The gate therefore stops further affected admissions because truncation occurred, without claiming an observed recall failure. The active 790-entry experiment stays below the cap. The minimal correction is to preserve the 790 active sources and keep the 20 grade-A budget holds pending; do not change scoring, thresholds, weights, or retrieval behavior.

Synthetic 1000/1200 stress results are diagnostic only and do not substitute for real content. The real 810-entry shadow is the applicable over-cap evidence.

After the final metadata-only audit-field update, the compiler regenerated the proposal DB, experiment DB, and HCA in final-refresh. Their SHA-256 values exactly matched the DB/HCA consumed by the focused test:

- Proposal DB: FDFC079AEF87AA6621C1035CD5497130E0B8AEA7F6486168BE1266578FAE6E51
- Experiment DB: 57D970E9D56B3B0AE36C74148AAD1A1686285A542B10AD0D9B0B8C88A51053F0
- Experiment HCA: 7ADB958D9380FDE8B6004BBE738E967497C17ECED854379DEB2105A4DE790780

Primary evidence files: FactoryV6Focused-transposition-final.log, negative-contracts/results.json, final-refresh/factory-v6-compiler.log, final-refresh/factory-v6-summary.txt, and build/factory-v6-media/real-shortlist-gate.tsv. The engineering build preceded the B3 local commit. Final Git identity is recorded by the primary agent in the delivery; no push was performed.

## Final source, program, and evidence hashes

SHA-256 values for the source and programs used by the final build:

- tests/FactoryV6Tests.cpp: C9A0BC3ED671E76C6C575295E08B391C952170C2DA92AB0E9F6FD0DABBD10319
- tools/factory_v6_compiler.cpp: A12F8985402F0ED5E7764CFD4100004AB47479441AD76E20944926D8631BB462
- Release/FactoryV6Tests.exe: E213371FCF955DC0CAB5E80F990876DC433114B44E283858A438069194793E84
- Release/factory_v6_compiler.exe: 672C4E61314B60C60A5A71E932E4C15183445F2126C832B5665E09571B8F219C
- FactoryV6Focused-transposition-final.log: E9DAEAE9F5714BB9B38A86D36806BD33CA45AFEB291DC23038A92DE91D7C1B25
- negative-contracts/results.json: 06BAD87B1160839E7132BC27A7A2F05B25D4C320A7732AEC7633C925F4A2FBCA
- final-refresh/factory-v6-summary.txt: 4C812FB846C6A22370091ED6E649E5917E464B04DF333710CF99629C0C5B793F