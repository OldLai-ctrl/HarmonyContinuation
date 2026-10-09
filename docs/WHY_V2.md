# Why? V2 and Color UI — dev.8

Presentation integration only. Analyzer mathematics, reranking goals/windows,
original engines/scores, groups, FULL/SKELETON, tendency budgets, MIDI, libraries,
host adapters and all schemas remain unchanged. Previous algorithm evidence from
dev.5–dev.7 is reused; this document supersedes their card/Why presentation policy.

## Cards and color meaning

Existing path, group/purpose and action geometry remain. Muted gold/cyan/gray
stripes accompany text. The primary tag prefers complete-path warming/cooling or
warm/cool turn, then overall warmth/balance; at most one tension-release/rise or
connection-contrast tag follows. Colors still represent whole-path mean W, so a
cool path can have a warming trend without being colored warm overall.

All labels read existing analyzer flags/metrics, with no new thresholds or chord
name mapping. Ending release is explained as an ending change, not a claim that
the whole path monotonically relaxes. W and Ws are never combined into one warmth
judgment: ordinary text uses W, while advanced details list Ws separately. Unknown
and Uncertain use neutral Insufficient data text with the existing reason, including
missing symbolic bass. No unreliable value becomes zero.

Continuation purpose text is separate from its color hint. Enrichment keeps group
purpose and the full path; detailed techniques are in Why. If the short hint is
too wide, its secondary tag is omitted using actual font width. Backgrounds,
actions, card dimensions, scaling and index mappings stay unchanged.

## Why structure

The complete candidate path precedes one to three explanation sentences:

1. Existing functional completion evidence, or up to two actual Enrichment
   operation techniques; no new harmonic inference.
2. One combined complete-path warmth/trend/tension sentence when hints are on.
3. One preference reason when sorting is active. Only a candidate actually moved
   forward is described as promoted; otherwise the wording says its full path was
   compared. An insufficient-color fallback replaces duplicate color prose.

The same formatter serves both modes and both locales. Font-measured paragraphs
wrap on whitespace or UTF-8 boundaries. Existing advanced Details holds original
scores, per-operation explanations, sources, VoiceLeading and optional color
T/W/S/ΔT/ΔW/Ts/Ws values (first eight positions, explicitly marked if truncated).
The model/emotion distinction and symbolic-bass/final-voicing note also live there.
No subjective emotion is inferred from the model.

## Independent controls and persistence

The shared Color Preference dropdown retains six choices and also exposes the
independent checked Color Hints toggle; the existing More toggle remains linked
to the same state. Hints default on, sorting default Off. Hiding hints removes
card/Why color summaries and color details without disabling sorting. Active
sorting still explains its reason, so its behavior remains visible. Neither
visualization change nor preference selection sends a generation callback.
Existing candidate caches and stable indices remain authoritative.

Controller's existing EditorUiState is instance-owned; no independent lightweight
cross-project preference store was found in the inspected controller/session path.
Cross-project persistence is therefore not added: host/session serialization and
schemas remain frozen. No new settings I/O, dependency or thread is introduced.

## Minimal verification

One Release build of HarmonyContinuation and WhyV2Smoke; run the latter once for
one Continuation and one Enrichment UI scenario. Each combines card/Why rendering,
Chinese/English text, advanced details, independent toggles, neutral fallbacks and
Preview/MIDI callback identity. Off restores the original indices; preference
switches do not generate candidates. Optional continuation/enrichment argument
allows only the failing scenario to be retried. No historical suite or UI matrix.
Offscreen rendering is not real-DAW acceptance; real DAW remains Pending. Actual
execution and artifact source commit are recorded in NOW.md.
