# Phase 2 Top 3 实测回放

这些结果由 Debug 构建的 progression_match_cli 对 tests/fixtures/progressions/queries 中的八组输入逐一运行得到。分数是相似度，不是概率；每个候选的完整对应关系与原因标记保留在下方。

运行命令：`build-vst3\progression_match_cli.exe <query.json> tests\fixtures\progressions\templates.json`

## A_exact_prefix

```text
QUERY
0  C
1  Am
2  Dm
3  G
KEY INTERPRETATIONS
C Major  prior=0.267
A Minor  prior=0.213
G Major  prior=0.126
TOP MATCHES
M05  Tonic vi predominant cadence  similarity=0.982  skeleton=1.000  full=1.000  rhythm=1.000  key=C Major  matchStart=0  matchEnd=3  continuationStart=4
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Am <-> vi  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Dm <-> ii  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
M22  I vi ii V  similarity=0.982  skeleton=1.000  full=1.000  rhythm=1.000  key=C Major  matchStart=0  matchEnd=3  continuationStart=4
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Am <-> vi  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Dm <-> ii  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
M17  V of ii approach  similarity=0.927  skeleton=0.957  full=0.916  rhythm=0.948  key=C Major  matchStart=0  matchEnd=4  continuationStart=5
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Am <-> vi  cost=0.000  SameDegree|SameFunction|SameRole
  TEMPLATE_DELETION  GAP <-> V7/ii  cost=0.189  EmbellishingDeletion|LowStructuralPenalty
  SUBSTITUTE  Dm <-> ii  cost=0.075  SameDegree|SameFunction|SameRole|RhythmMismatch
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
PERF prefilterMs=0.002 alignmentMs=3.966 totalMs=3.969 shortlist=50
```

## B_secondary_dominant

```text
QUERY
0  C
1  Am
2  A7
3  Dm
4  G
KEY INTERPRETATIONS
C Major  prior=0.208
D Minor  prior=0.181
A Minor  prior=0.174
TOP MATCHES
M17  V of ii approach  similarity=0.980  skeleton=1.000  full=1.000  rhythm=1.000  key=C Major  matchStart=0  matchEnd=4  continuationStart=5
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Am <-> vi  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  A7 <-> V7/ii  cost=0.000  SameDegree|SameFunction|SameRole|SecondaryTargetMatch
  MATCH  Dm <-> ii  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
M05  Tonic vi predominant cadence  similarity=0.931  skeleton=0.957  full=0.934  rhythm=0.948  key=C Major  matchStart=0  matchEnd=3  continuationStart=4
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Am <-> vi  cost=0.000  SameDegree|SameFunction|SameRole
  QUERY_INSERTION  A7 <-> GAP  cost=0.180  EmbellishingInsertion|LowStructuralPenalty|ResolvesToMatchedTarget
  SUBSTITUTE  Dm <-> ii  cost=0.075  SameDegree|SameFunction|SameRole|RhythmMismatch
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
M22  I vi ii V  similarity=0.931  skeleton=0.957  full=0.934  rhythm=0.948  key=C Major  matchStart=0  matchEnd=3  continuationStart=4
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Am <-> vi  cost=0.000  SameDegree|SameFunction|SameRole
  QUERY_INSERTION  A7 <-> GAP  cost=0.180  EmbellishingInsertion|LowStructuralPenalty|ResolvesToMatchedTarget
  SUBSTITUTE  Dm <-> ii  cost=0.075  SameDegree|SameFunction|SameRole|RhythmMismatch
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
PERF prefilterMs=0.003 alignmentMs=3.229 totalMs=3.231 shortlist=50
```

## C_missing_vi

```text
QUERY
0  C
1  Dm
2  G
KEY INTERPRETATIONS
C Major  prior=0.310
A Minor  prior=0.170
G Major  prior=0.114
TOP MATCHES
M08  Tonic ii dominant  similarity=0.983  skeleton=1.000  full=1.000  rhythm=1.000  key=C Major  matchStart=0  matchEnd=2  continuationStart=3
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Dm <-> ii  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
M34  I ii V vi  similarity=0.983  skeleton=1.000  full=1.000  rhythm=1.000  key=C Major  matchStart=0  matchEnd=2  continuationStart=3
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Dm <-> ii  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
M19  Diminished approach  similarity=0.937  skeleton=0.971  full=0.915  rhythm=0.968  key=C Major  matchStart=0  matchEnd=3  continuationStart=4
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  TEMPLATE_DELETION  GAP <-> viidim7/ii  cost=0.166  EmbellishingDeletion|LowStructuralPenalty
  SUBSTITUTE  Dm <-> ii  cost=0.035  SameDegree|SameFunction|SameRole
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
PERF prefilterMs=0.003 alignmentMs=3.504 totalMs=3.507 shortlist=50
```

## D_diminished_passing

```text
QUERY
0  C
1  C#dim7
2  Dm
3  G
KEY INTERPRETATIONS
C Major  prior=0.264
A Minor  prior=0.168
F Major  prior=0.078
TOP MATCHES
M19  Diminished approach  similarity=0.975  skeleton=1.000  full=0.980  rhythm=1.000  key=C Major  matchStart=0  matchEnd=3  continuationStart=4
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  SUBSTITUTE  C#dim7 <-> viidim7/ii  cost=0.060  SameFunction|SameRole|SecondaryTargetMatch|EnharmonicDegree
  MATCH  Dm <-> ii  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
M08  Tonic ii dominant  similarity=0.942  skeleton=0.971  full=0.933  rhythm=0.968  key=C Major  matchStart=0  matchEnd=2  continuationStart=3
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  QUERY_INSERTION  C#dim7 <-> GAP  cost=0.174  EmbellishingInsertion|LowStructuralPenalty|ResolvesToMatchedTarget
  SUBSTITUTE  Dm <-> ii  cost=0.035  SameDegree|SameFunction|SameRole
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
M34  I ii V vi  similarity=0.942  skeleton=0.971  full=0.933  rhythm=0.968  key=C Major  matchStart=0  matchEnd=2  continuationStart=3
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  QUERY_INSERTION  C#dim7 <-> GAP  cost=0.174  EmbellishingInsertion|LowStructuralPenalty|ResolvesToMatchedTarget
  SUBSTITUTE  Dm <-> ii  cost=0.035  SameDegree|SameFunction|SameRole
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
PERF prefilterMs=0.003 alignmentMs=4.128 totalMs=4.130 shortlist=50
```

## E_borrowed_iv

```text
QUERY
0  C
1  Fm
2  G
3  C
KEY INTERPRETATIONS
C Major  prior=0.433
F Minor  prior=0.244
A Minor  prior=0.049
TOP MATCHES
M16  Borrowed iv cadence  similarity=0.986  skeleton=1.000  full=1.000  rhythm=1.000  key=C Major  matchStart=0  matchEnd=3  continuationStart=4
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Fm <-> iv  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
M01  Simple authentic  similarity=0.875  skeleton=0.886  full=0.886  rhythm=1.000  key=C Major  matchStart=0  matchEnd=3  continuationStart=4
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  SUBSTITUTE  Fm <-> IV  cost=0.362  SameDegree|QualityVariant|BorrowedVariant
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
M07  Plagal predominant cadence  similarity=0.733  skeleton=0.740  full=0.740  rhythm=0.625  key=C Major  matchStart=0  matchEnd=3  continuationStart=4
  SUBSTITUTE  C <-> I  cost=0.255  SameDegree|SameFunction|SameRole|RhythmMismatch
  SUBSTITUTE  Fm <-> IV  cost=0.436  SameDegree|QualityVariant|BorrowedVariant|RhythmMismatch
  SUBSTITUTE  G <-> V  cost=0.105  SameDegree|SameFunction|SameRole|RhythmMismatch
  SUBSTITUTE  C <-> I  cost=0.105  SameDegree|SameFunction|SameRole|RhythmMismatch
PERF prefilterMs=0.003 alignmentMs=3.283 totalMs=3.286 shortlist=50
```

## F_scaled_rhythm

```text
QUERY
0  C
1  Am
2  Dm
3  G
KEY INTERPRETATIONS
C Major  prior=0.267
A Minor  prior=0.213
G Major  prior=0.126
TOP MATCHES
M05  Tonic vi predominant cadence  similarity=0.982  skeleton=1.000  full=1.000  rhythm=1.000  key=C Major  matchStart=0  matchEnd=3  continuationStart=4
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Am <-> vi  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Dm <-> ii  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
M22  I vi ii V  similarity=0.982  skeleton=1.000  full=1.000  rhythm=1.000  key=C Major  matchStart=0  matchEnd=3  continuationStart=4
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Am <-> vi  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Dm <-> ii  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
M17  V of ii approach  similarity=0.927  skeleton=0.957  full=0.916  rhythm=0.948  key=C Major  matchStart=0  matchEnd=4  continuationStart=5
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Am <-> vi  cost=0.000  SameDegree|SameFunction|SameRole
  TEMPLATE_DELETION  GAP <-> V7/ii  cost=0.189  EmbellishingDeletion|LowStructuralPenalty
  SUBSTITUTE  Dm <-> ii  cost=0.075  SameDegree|SameFunction|SameRole|RhythmMismatch
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
PERF prefilterMs=0.003 alignmentMs=4.206 totalMs=4.208 shortlist=50
```

## G_wrong_chord

```text
QUERY
0  C
1  Am
2  Eb
3  G
KEY INTERPRETATIONS
G Major  prior=0.293
C Major  prior=0.286
F Major  prior=0.084
TOP MATCHES
M03  Fifties turnaround  similarity=0.639  skeleton=0.648  full=0.648  rhythm=1.000  key=C Major  matchStart=0  matchEnd=3  continuationStart=4
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Am <-> vi  cost=0.000  SameDegree|SameFunction|SameRole
  SUBSTITUTE  Eb <-> IV  cost=1.300
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
M05  Tonic vi predominant cadence  similarity=0.639  skeleton=0.648  full=0.648  rhythm=1.000  key=C Major  matchStart=0  matchEnd=3  continuationStart=4
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Am <-> vi  cost=0.000  SameDegree|SameFunction|SameRole
  SUBSTITUTE  Eb <-> ii  cost=1.300  QualityVariant
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
M22  I vi ii V  similarity=0.639  skeleton=0.648  full=0.648  rhythm=1.000  key=C Major  matchStart=0  matchEnd=3  continuationStart=4
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  Am <-> vi  cost=0.000  SameDegree|SameFunction|SameRole
  SUBSTITUTE  Eb <-> ii  cost=1.300  QualityVariant
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
PERF prefilterMs=0.002 alignmentMs=5.338 totalMs=5.340 shortlist=50
```

## H_ambiguous_key

```text
QUERY
0  Am
1  F
2  C
3  G
KEY INTERPRETATIONS
A Minor  prior=0.252
C Major  prior=0.201
G Major  prior=0.196
TOP MATCHES
M13  vi IV I V  similarity=0.980  skeleton=1.000  full=1.000  rhythm=1.000  key=C Major  matchStart=0  matchEnd=3  continuationStart=4
  MATCH  Am <-> vi  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  F <-> IV  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
m03  Minor VI VII i  similarity=0.813  skeleton=0.828  full=0.828  rhythm=1.000  key=A Minor  matchStart=0  matchEnd=2  continuationStart=3
  MATCH  Am <-> i  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  F <-> VI  cost=0.000  SameDegree|SameFunction|SameRole
  QUERY_INSERTION  C <-> GAP  cost=0.567  StructuralInsertion
  MATCH  G <-> VII  cost=0.000  SameDegree|SameFunction|SameRole
M09  I IV I V  similarity=0.793  skeleton=0.808  full=0.808  rhythm=1.000  key=C Major  matchStart=1  matchEnd=3  continuationStart=4
  QUERY_INSERTION  Am <-> GAP  cost=0.589  StructuralInsertion
  MATCH  F <-> IV  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  C <-> I  cost=0.000  SameDegree|SameFunction|SameRole
  MATCH  G <-> V  cost=0.000  SameDegree|SameFunction|SameRole
PERF prefilterMs=0.003 alignmentMs=4.965 totalMs=4.967 shortlist=50
```
