# Color Preference Reranker — dev.6 / dev.7

A read-only presentation stage for Continue, after original candidate generation.
Off is the default and directly returns the existing presentation order. No
candidate, score, group, chord, duration, bass, constraint or MIDI data is edited.
dev.7 extends the same presentation stage to Enrichment (see below). The original engine result remains authoritative.

## Inputs and preset matching

`ColorPreferenceReranker::rank` receives source timed pitch sets, selected and
possible keys, original-order candidates, complete `PathColor` trajectories and a
preference. It returns original indices and per-original-index cost/reason. It
uses the dev.5 analyzer without reinterpreting ambiguous directions.

- Continue source: needs at least three reliable timed source positions; compares
  temperature/tension trajectory extrapolation and mean Ts/signed Ws movement.
  Projected changes are capped at ±4 W and ±2 T, allowing a bounded continuation.
- Warmer / cooler: duration-weighted fit to a +4 / −4 W ramp over every position,
  including the source-last/candidate-first seam. Local fluctuations are allowed;
  neither the final chord nor a strict monotonicity rule determines the result.
- Tension rise and release: fits a three-unit interior tension arc, with tolerance
  for local deviations and penalties for insufficient accumulation/release.
- Tension then warm closure: requires actual interior accumulation and release
  of at least one T unit, final W≥1 and final W at least 0.5 above the weighted
  latter-half mean, with a latter-half fitted warming ≥0.5 and tension decline ≥0.5. The existing intent evaluator must report DominantTonic or
  StableTonic for a Resolve candidate; color cannot invent functional closure.

Time coordinates use event midpoints; durations weight every point. Static T/W,
all internal transitions and the seam come from the complete analyzed path.
Model temperature does not identify human emotion. Preset ramps and tolerances
are explicit product heuristics, not new claims about the author's model.

Errors use fixed scales: T/10, W differences/20, Ts/20, signed Ws differences/20
(or |Ws|/10). Non-auto goals reserve 10% for spatial movement magnitude. T, ΔT
and Ti are not added as duplicate independent rewards. No pool min-max,
randomness, machine learning or candidate search is used.

## Quality protection and deterministic order

Groups remain separate. Original quality gate ≥60 and hard melody satisfaction
are required to move; excluded candidates remain present in original positions.
Only adjacent original-order windows of at most three are compared: maximum
rank displacement is two. Every pair must share the original five-point quality
band, differ by at most three ranking points, have the same key, intent, styles
and constraints, and differ by at most 0.05 in melody fit, match similarity and
match/skeleton/style/intent/rhythm/cadence subscores. This preserves the existing
FULL/SKELETON evidence and quality distinctions. No engine weight or score is
modified. Quantized costs and stable sorting preserve original-order ties.

Unknown/Uncertain/missing timing, unsupported goals or functional mismatches have
no invented zero score. Such candidates form fixed anchors; no sorting window
crosses them. A short/unreliable source disables automatic continuation.

The UI first applies the unchanged eligibility/diversity/duplicate visibility
policy, then reorders that same visible set using the full-group index order.
The More list also uses the group order; identity-based selection/actions still
refer to original candidate indices. Hidden candidates are not promoted past
existing visibility rules. Conservative windows may produce no visible change.

## UI and state

A compact Color sort dropdown joins the existing recommendation toolbar, with
six choices (including Off), in Chinese and English. Display color hints remains
independent. Only enabled sorting adds its preset and brief reason to Why,
including specific fallback reasons. Changing preference invalidates only the
presentation cache; it sends no generation/state-change callback. Source,
analysis and original-result updates invalidate the cache. No work occurs in the
audio thread and no I/O, additional thread or runtime is introduced.

EditorUiState retains the preference within the plugin instance, including
editor recreation. It does not persist across project/plugin reload. No schema
changes are made. Original indices restore exactly when Off is selected.

## Validation history

The dev.6 one-time smoke tool was removed during repository cleanup. Historical verification is summarized in NOW.md.

## Enrichment Color Guidance — dev.7

`rankEnrichment` compares original and complete upgraded paths, with no artificial
Continuation seam. Shared `track`, `fit`, normalization and `sortWindows` retain
dev.6 goal matching and maximum two-position displacement. Off returns original
indices immediately. Both modes share one default-Off preference and dropdown.

Source and candidate are aligned by QN interval overlap, including inserted chords
that subdivide an original position. Unknown timing, unmatched coverage, Unknown
or Uncertain positions prevent scoring. Auto preserves the original T/W/S contour
with fixed weights 0.4/0.4/0.1 plus 0.05 each for Ts and signed Ws change; at least
three reliable source positions are required. Warmer/cooler splits the existing
90% trajectory component equally between complete-candidate and source-relative
W ramp matching, keeping the same 10% spatial component. Tension arc reuses the
full-path target. Warm closure also requires original analyzed tonic closure,
compatible requested intent and a final tonic of supported major/minor quality;
color does not independently invent a harmonic cadence.

`analyzeEnrichment` reuses HarmonyColorAnalyzer, prioritizing explicit bass or
bassNoteValue over names. A verified slash bass is accepted if typed bass is absent.
Missing bass produces Uncertain/MissingBass with no fabricated root-bass W/S.
Candidate timing, pitch masks and extensions are unchanged. Color describes
symbolic bass; it does not verify final MIDI voicing or perceived sound.

Groups, complexity level, five-point quality bands and maximum three-score-point
gaps remain protected. Pairwise skeleton/style/complexity/melody differences must
be ≤0.05; hard melody constraints must pass. Existing tendency profiles supply
operation/insertion/substitution budgets and skeleton minima: Conservative uses
1/2/3 operations and 0.88 skeleton minimum, Balanced 2/3/5 and 0.75, Bold 2/4/6
and 0.75. These are the engine's actual engineering meanings, not new emotional
labels. Existing full-path and skeleton preservation scores are read-only.
The current engine supports melody constraints, not separate per-chord locks.

Existing VoiceLeading measurement is reused unchanged: candidates need a reliable
measurement, at most 0.02 loss against source, and at most 0.02 pairwise difference.
Its label-based bass must agree with the explicit symbolic bass; disagreement is
protected in original position rather than bypassing voice quality with a color
score. No generator, substitution, extension, inversion, VoiceLeading or melody
algorithm is modified, and no edit budget is increased.

Cards, More, Why, Preview, MIDI payload, snapshot and comparison all resolve the
same presentation-index-to-original-index mapping or stable ID. Original containers
never reorder. Why retains existing explanations and adds one target-matching
sentence only when enabled; hints remain independently switchable. A symbolic-bass
note distinguishes model information from final sound. Preferences remain instance
local without database/session/snapshot format changes.

The dev.7 one-time smoke tool was removed during repository cleanup. Historical verification is summarized in NOW.md; production ranking behavior is unchanged.
