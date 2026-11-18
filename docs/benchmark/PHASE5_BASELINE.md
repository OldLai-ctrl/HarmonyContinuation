# Phase 5 continuation quality baseline

## Provenance

- Starting code: Phase 4C commit `20d1c0f` (the same harmony, matching, recommendation, and factory data as the committed baseline). Phase 5 adds presentation and measurement; no matcher or recommendation weights were changed for this first report.
- Fixed factory library: 161 templates. User library was excluded from the run so it is reproducible.
- Inputs: 42 hand-authored unfinished phrases under `data/benchmark/continuation/`, spanning 15 categories. Each case records its aim without prescribing one correct continuation.
- Machine report: [`reports/phase5_baseline.json`](../../reports/phase5_baseline.json). Brief generated table: [`reports/phase5_baseline.md`](../../reports/phase5_baseline.md).
- Reproduce from repository root: `build-core/continuation_benchmark_cli.exe --all --output reports/phase5_baseline.json --commit 20d1c0f`. The command writes both files and returns exit code 1 while the three documented identical Top 2 paths remain. This is a quality gate, not a crash.

## Machine observations

| Measure | Baseline |
| --- | ---: |
| Cases | 42 |
| Candidates | 214 |
| PreviewSequence / MIDI / snapshot roundtrips | 214 / 214 / 214 |
| Cases with offline render | 42 / 42 |
| Same-group identical Top 2 paths | 3 |
| Cross-group duplicate paths | 21 occurrences |
| Continuations of one chord | 82 |
| Continuations of 2–3 chords | 103 |
| Continuations of 4–6 chords | 29 |
| Continuations of 7+ chords | 0 |
| CompleteContinuationRatio (at least two events) | 132 / 214 = 61.7% |

| Group | Cases with candidates | Empty cases | Empty ratio |
| --- | ---: | ---: | ---: |
| Resolve | 42 | 0 | 0% |
| Develop | 27 | 15 | 35.7% |
| Loop | 30 | 12 | 28.6% |
| Color | 20 | 22 | 52.4% |

Category counts: major functional 4; minor functional 4; ii–V 4; secondary dominant 3; borrowed iv 3; bVII/bVI 3; pop loop 3; jazz functional 3; city pop/R&B 3; short 2; long 2; key ambiguity 2; anomalous chord 2; OPEN duration 2; near cadence 2.

## PRODUCT RISKS and listening order

1. **One-chord paths are common:** 82/214. Listen first to `bench_031`–`bench_034` and cadential cases. A one-chord result may be useful, but the product promise is a complete phrase.
2. **Color is often absent:** 22/42 cases. Check borrowed iv (`bench_016`–`bench_018`), bVII/bVI (`bench_019`–`bench_021`), and city pop/R&B (`bench_028`–`bench_030`) before deciding whether this is a coverage or intent-classification issue.
3. **Exact duplicate Top 2 Loop paths:** `bench_020`, `bench_024`, and `bench_035`. The benchmark flags them as structural quality failures; ranking remains unchanged until a human compares them.
4. **Cross-group repetition:** 21 occurrences. Listen to the reported pair in each case before judging whether one group label is misleading.
5. **Long input coverage:** the longer cases each yielded only one candidate. They pass the product path but have limited choice.
6. **Anomalous chords:** `bench_037` and `bench_038` need listening to distinguish sensible recovery from generic output.
7. **Sound versus harmony:** record preview timbre and voicing problems as `Sound` or `Voicing` in rating files. They are separate from harmonic usefulness.

The numeric report is a baseline, not a human quality verdict. Do not tune weights or add factory templates from these counts alone. Rate individual candidate fingerprints in Demo Benchmark mode, then use `benchmark_compare OLD.json NEW.json --output changes.json` for later changes. The comparison reports changed candidates, ranks, empty groups, OPEN holds, and key interpretation without declaring the new result better.
