# Continuation benchmark

Phase 5 contains 42 hand-authored unfinished chord phrases, not single-answer unit tests. `tools/generate_phase5_cases.py` holds the curated source list and writes the checked-in `bench_001.json` through `bench_042.json` files. Do not regenerate them as random permutations.

Each case has `id`, `name`, `category`, `tempo`, `meter`, `chords`, `notes`, and one or more `expectedCharacteristics`. Optional `forcedKey` and `style` express intended context. An omitted final duration represents OPEN. Expectations describe useful behavior and do not name one mandatory answer.

The 15 categories cover major/minor function, ii–V, secondary dominants, borrowed iv, bVII/bVI, pop loops, jazz, city pop/R&B, short and longer phrases, key ambiguity, anomalous chords, unknown current duration, and near cadences. The CLI validates names, IDs, chord counts, and metadata before running the real core.
