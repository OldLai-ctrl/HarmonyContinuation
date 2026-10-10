# Color Analysis V1 — dev.5

`HarmonyColorAnalyzer` is an independent, read-only sidecar. It never supplies
weights, scores or ordering to Matcher, ContinuationEngine or EnrichmentEngine.
The original drop → automatic recommendations → audition → MIDI workflow is
unchanged. Visible-card/Why analysis is lazy and cached in MainView, after results
have been published. It performs no file/database access, harmonic re-analysis,
audio-thread work or additional scheduling. Source pitch sets are prepared once
per imported revision; candidate harmonicData is used when present.

## Mathematical basis and supported subset

Primary reference: Tian Jianian, 《和弦的表现色彩量化模型》, supplied PDF,
pp. 4–6, 8, 10, 12–16 (including the December 2025 update note on p.6).
The PDF remains an external research source and is not bundled. The purchased
workbook, its database, templates, stored descriptors and converted copies are
not included. Structural mathematical rules are implemented independently.
[Whfkl/ColorChord](https://github.com/Whfkl/ColorChord), MIT, was consulted for the
older maximum-gap unwrapping construction; no Python runtime/source is included.

- Twelve-tone equal-tempered pitch classes; duplicates/octaves count once.
  Explicit bass/inversion is retained; a sounding non-chord bass joins the set.
  Pitch sets come from the existing chord materializer, including extension masks.
  Its root-position default for unslashed abstract labels is also the color model's
  assumed bass; the result is not a prediction of unspecified performance voicing.
- Three to six distinct pitches with a unique maximum fifth-circle gap and span
  at most six, where PDF p.5 specifies a consonance grade; also the power fifth
  explicitly assigned r=10 in p.8. No unmatched grade receives a default score.
- Fifth-circle positions: A=15°, D=45°, G=75°, C=105°, …, E=345°.
  Cut at the largest gap, unwrap that occupied arc, then take the arithmetic mean
  of the expanded angles. This is not an ordinary circular mean. Internal and
  public angles are radians, normalized to [0, 2π).
- Multiple equal largest gaps produce `Uncertain`, with diagnostic candidate
  directions and no scalar r/θ/W/S or transition score. No contextual tie-break is
  invented. Unsupported pitch sets/grades and unique wide spans >6 produce
  `Unknown`. The newer wide-span algorithm was revised but not fully specified;
  V1 does not silently substitute the older algorithm. Single notes, unsupported
  dyads and sets exceeding six pitches also remain Unknown.

For supported chords: T=10−r, W=r sin(θB−θ), S=r cos(θB−θ).
S means concentration toward the bass, not functional tonal stability.
For two reliable adjacent chords: ΔT=T2−T1, ΔW=W2−W1,
Ts=√(r1²+r2²−2r1r2 cos(θ2−θ1)), Ws=(r1r2/10) sin(θ1−θ2),
Ti=|ΔT|+Ts. Signed Ws and static ΔW remain separate. The first chord has no
dynamic value. PDF p.16's printed arithmetic example is inconsistent; formulas
are authoritative, and its erroneous total is not used as a test oracle.

## Complete-path read interface

`prepare`, `analyzePath` and `analyzeContinuation` return original-order positions,
start/duration QN, static metrics and optional predecessor links. The continuation
boundary is explicit; its first chord links to the final original chord. OPEN
duration uses the candidate's existing suggested duration if supplied, otherwise
remains missing. No new fallback duration is invented.

Whole-path temperature/tension are duration-weighted means. Trends use weighted
regression over event midpoint times and report the fitted first-to-last change.
Ending ΔW/ΔT and maximum Ts are separate fields. Thus a cool path can gradually
warm without being labeled warm overall. Any missing/ambiguous chord or unknown
timing suppresses whole-path summaries, while valid individual metrics/links
remain accessible. Enrichment analyzes its complete replacement progression.
These structs are available to dev.6; V1 implements no candidate sorting.

## Presentation policy

The thin inline stripe and text use existing card space; card backgrounds and
geometry remain unchanged. Gold/cyan/gray follow the whole-path mean W, not the
ending alone. ±1 is the warm/cool threshold. Warming/cooling needs at least three
positions, fitted change ≥1 in magnitude, at least two-thirds of transitions in
that direction and no opposing transition beyond 0.05. Warm/cool turns require
values on both sides of ±1. Tension rise needs fitted change ≥1 and more rising
than falling transitions; release requires final ΔT≤−1. Color contrast uses
max Ts≥8. These are documented UI heuristics, not author or psychological norms.

Existing functional Why remains; brief color sentences are appended separately
in Simplified Chinese and English. They distinguish whole-path tendency, trend
and ending; no happiness/sadness or other emotion inference is made.

Advanced menu → Show color hints, default on. Off means no new stripe, labels or
explanations and no color calculation on that rendering path. Existing
EditorUiState preserves the flag across editor recreation in the same plugin
instance. It is not persisted across plugin/project reload; no existing DB,
Session or Snapshot format is extended. Factory V3 and all schemas stay frozen.

## Minimal validation

One Release build of the plugin and `HarmonyColorSmoke`; run that executable once
for three calculation scenarios and one offscreen UI/Why smoke (both locales,
toggle, recommendation identity and editor-state preservation). No CTest,
Validator, benchmarks, MIDI suite, host matrix, installer or performance regression.
Actual executed results belong in NOW.md. Stop after completion; dev.6 is separate.
