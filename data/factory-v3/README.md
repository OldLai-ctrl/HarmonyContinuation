# Factory Library 3 additions

These nine JSON files are the production source for 474 newly authored,
key-independent phrases. Together with the 155 canonical V2 phrases, the V3
catalogue contains 629 entries. The original `data/factory/` files remain the
161-row Library 2 source; their six legacy duplicates are redirected only when
compiling V3.

| File | Phrase family | New entries |
| --- | --- | ---: |
| home.json | Major resolution and plagal endings | 53 |
| open.json | Major development and open endings | 55 |
| cycle.json | Major loops | 51 |
| dusk.json | Minor resolution and plagal endings | 54 |
| night.json | Minor loops | 53 |
| velvet.json | Major seventh harmony | 53 |
| blue.json | Minor seventh harmony | 55 |
| shade.json | Borrowed major-key colors | 45 |
| spark.json | Secondary dominants and diminished approaches | 55 |

Each source row spells a complete phrase. No key transpositions, cyclic
rotations, random combinations, style-only duplicates, or rhythm-only copies
were used to reach the target. Phrase durations are explicit quarter-note
values; this first expansion uses four QN per chord in 4/4. Style and prior
weights are editorial hints, not measured frequencies. Names remain short,
bilingual, and numbered within a musical family; the chord sequence is displayed
separately. This is authored content, not a claim of human listening approval.

`tools/generate_v3_catalog.py` records the initial explicit composition list.
Its output JSON is authoritative for later editorial changes; do not rerun the
initial generator over edited rows. IDs have gaps where identical seed phrases
were omitted, and must not be renumbered. The compiler uses one canonical musical
fingerprint across the entire V3 catalogue. Identical music is retained once,
with styles, intents, techniques, tags and aliases merged; names and editorial
metadata do not affect identity. Counts are in [the short summary](../../LIBRARY_V3_SUMMARY.md).

Factory Schema 2 and snapshot formats remain frozen. Six unconfirmed candidates
are recorded in `data/review/factory-v3-questionable.tsv`, outside the production
input directories. Unsupported source notation must not be guessed, or promoted
by changing the schema. Only the optional summary counts that review file;
its candidates are never compiled into the production database.

Build from the normal CMake `factory_database` target, or call:

```text
db_compiler data/factory output/factory.db 3 data/factory-v3 LIBRARY_V3_SUMMARY.md
```

For Library 2, omit the additions directory and pass version 2. Current build
and verification evidence is maintained only in `NOW.md` at the project root.
