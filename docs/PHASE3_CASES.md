# Phase 3 recommendation case capture

Captured from the standalone CLI and the packaged factory database. Scores are ordinal, not probabilities. Cubase host behavior is not covered by these cases.

## Case A: I → vi → ii (C, Am, Dm OPEN)

```text
QUERY
  C
  Am
  Dm
KEY
  C Major
RESOLVE
#1  score=80.7  key=C Major  match=0.981  style=Pop,Functional  cadence=authentic  support=2
  Current OPEN hold: 2.000 QN
  Continuation: G(2.000 QN) -> C(4.000 QN)
  Sources: COMMON_MAJOR_003 COMMON_MAJOR_004
#2  score=74.3  key=C Major  match=0.906  style=Pop,Functional  cadence=authentic  support=1
  Current OPEN hold: 4.000 QN
  Continuation: F(4.000 QN) -> G(4.000 QN) -> C(4.000 QN)
  Sources: COMMON_MAJOR_015
DEVELOP
#1  score=66.8  key=C Major  match=0.906  style=Pop,Rock  cadence=half  support=1
  Current OPEN hold: 2.000 QN
  Continuation: G(2.000 QN)
  Sources: POP_016
#2  score=62.6  key=C Major  match=0.767  style=Jazz,Functional  cadence=authentic  support=1
  Current OPEN hold: 2.000 QN
  Continuation: G(2.000 QN) -> C(4.000 QN)
  Sources: FUNCTIONAL_012
LOOP
#1  score=73.9  key=C Major  match=0.894  style=R&B,Jazz  cadence=loop closure  support=1
  Current OPEN hold: 2.000 QN
  Continuation: G7(4.000 QN) -> Cmaj7(4.000 QN)
  Sources: JAZZ_014
COLOR
#1  score=65.5  key=C Major  match=0.826  style=Pop,R&B,Jazz  cadence=plagal  support=1
  Current OPEN hold: 4.000 QN
  Continuation: Fmaj7(4.000 QN) -> Fm7(4.000 QN) -> Cmaj7(4.000 QN)
  Sources: RNB_SOUL_009
PERF totalMatchMs=6.492 shortlist=161
```

## Case B: I → vi → V/ii → ii (C, Am, A7, Dm OPEN)

```text
QUERY
  C
  Am
  A7
  Dm
KEY
  C Major
RESOLVE
#1  score=80.7  key=C Major  match=0.980  style=Pop,Functional  cadence=authentic  support=2
  Current OPEN hold: 2.000 QN
  Continuation: G(2.000 QN) -> C(4.000 QN)
  Sources: COMMON_MAJOR_004 COMMON_MAJOR_003
#2  score=75.3  key=C Major  match=0.923  style=Pop,Functional  cadence=authentic  support=1
  Current OPEN hold: 4.000 QN
  Continuation: F(4.000 QN) -> G(4.000 QN) -> C(4.000 QN)
  Sources: COMMON_MAJOR_015
DEVELOP
#1  score=67.8  key=C Major  match=0.923  style=Pop,Rock  cadence=half  support=1
  Current OPEN hold: 2.000 QN
  Continuation: G(2.000 QN)
  Sources: POP_016
#2  score=65.4  key=C Major  match=0.827  style=Jazz,Functional  cadence=authentic  support=1
  Current OPEN hold: 2.000 QN
  Continuation: D7(2.000 QN) -> G(2.000 QN) -> C(4.000 QN)
  Sources: FUNCTIONAL_019
#3  score=61.4  key=C Major  match=0.742  style=Jazz,Functional  cadence=authentic  support=1
  Current OPEN hold: 2.000 QN
  Continuation: G(2.000 QN) -> C(4.000 QN)
  Sources: FUNCTIONAL_012
LOOP
#1  score=72.1  key=C Major  match=0.855  style=R&B,Jazz  cadence=loop closure  support=1
  Current OPEN hold: 2.000 QN
  Continuation: G7(4.000 QN) -> Cmaj7(4.000 QN)
  Sources: JAZZ_014
COLOR
#1  score=66.7  key=C Major  match=0.847  style=Pop,R&B,Jazz  cadence=plagal  support=1
  Current OPEN hold: 4.000 QN
  Continuation: Fmaj7(4.000 QN) -> Fm7(4.000 QN) -> Cmaj7(4.000 QN)
  Sources: RNB_SOUL_009
PERF totalMatchMs=6.698 shortlist=161
```

## Case C: ii → V (Dm7, G7 OPEN)

```text
QUERY
  Dm7
  G7
KEY
  C Major
RESOLVE
#1  score=79.7  key=C Major  match=0.984  style=R&B,Jazz  cadence=authentic  support=12
  Current OPEN hold: 4.000 QN
  Continuation: Cmaj7(4.000 QN)
  Sources: JAZZ_001 RNB_SOUL_011 JAZZ_002 JAZZ_003 RNB_SOUL_001 RNB_SOUL_013 RNB_SOUL_002 RNB_SOUL_012 RNB_SOUL_017 CITYPOP_JPOP_002 CITYPOP_JPOP_008 JAZZ_006
#2  score=73.1  key=C Major  match=0.868  style=Jazz,Functional  cadence=authentic  support=1
  Current OPEN hold: 4.000 QN
  Continuation: C(4.000 QN) -> F(4.000 QN) -> G(4.000 QN) -> C(4.000 QN)
  Sources: FUNCTIONAL_015
DEVELOP
#1  score=76.0  key=C Major  match=0.984  style=R&B,Jazz  cadence=authentic  support=1
  Current OPEN hold: 4.000 QN
  Continuation: Em7(4.000 QN) -> A7(4.000 QN) -> Dm7(4.000 QN) -> G7(4.000 QN) -> Cmaj7(4.000 QN)
  Sources: JAZZ_007
#2  score=74.8  key=C Major  match=0.984  style=R&B,Jazz  cadence=none  support=1
  Current OPEN hold: 4.000 QN
  Continuation: Cmaj7(4.000 QN) -> Fmaj7(4.000 QN) -> Bm7b5(4.000 QN) -> Em7(4.000 QN)
  Sources: JAZZ_017
#3  score=67.0  key=C Major  match=0.891  style=Pop,Jazz,City Pop  cadence=deceptive  support=1
  Current OPEN hold: 4.000 QN
  Continuation: Em7(4.000 QN)
  Sources: CITYPOP_JPOP_014
LOOP
#1  score=77.1  key=C Major  match=0.984  style=R&B,Jazz  cadence=loop closure  support=1
  Current OPEN hold: 4.000 QN
  Continuation: Cmaj7(4.000 QN) -> A7(4.000 QN) -> Dm7(4.000 QN)
  Sources: JAZZ_013
#2  score=73.1  key=C Major  match=0.921  style=R&B,Jazz  cadence=loop closure  support=1
  Current OPEN hold: 4.000 QN
  Continuation: Em7(4.000 QN) -> A7(4.000 QN)
  Sources: JAZZ_015
#3  score=60.1  key=C Major  match=0.921  style=R&B,Jazz  cadence=loop closure  support=1
  Current OPEN hold: 4.000 QN
  Continuation: Cmaj7(4.000 QN) -> Am7(2.000 QN)
  Sources: JAZZ_014
COLOR
#1  score=73.2  key=C Major  match=0.942  style=R&B,Jazz  cadence=authentic  support=4
  Current OPEN hold: 4.000 QN
  Continuation: Cmaj7(4.000 QN)
  Sources: JAZZ_009 RNB_SOUL_007 RNB_SOUL_008 CITYPOP_JPOP_009
PERF totalMatchMs=5.157 shortlist=161
```

## Case D: I → IV → iv (C, F, Fm OPEN)

```text
QUERY
  C
  F
  Fm
KEY
  C Major
RESOLVE
#1  score=65.8  key=C Major  match=0.714  style=Pop,Rock  cadence=authentic  support=1
  Current OPEN hold: 4.000 QN
  Continuation: G(4.000 QN) -> C(4.000 QN)
  Sources: ROCK_009
#2  score=61.6  key=C Major  match=0.714  style=Pop,Rock  cadence=modal  support=1
  Current OPEN hold: 4.000 QN
  Continuation: Bb(4.000 QN) -> C(4.000 QN)
  Sources: ROCK_019
DEVELOP
  (none)
LOOP
#1  score=65.8  key=C Major  match=0.714  style=Pop,Rock  cadence=loop closure  support=1
  Current OPEN hold: 4.000 QN
  Continuation: F(4.000 QN) -> C(4.000 QN)
  Sources: ROCK_002
COLOR
#1  score=74.7  key=C Major  match=0.980  style=Pop,Functional  cadence=plagal  support=1
  Current OPEN hold: 4.000 QN
  Continuation: C(4.000 QN)
  Sources: COMMON_MAJOR_019
#2  score=74.3  key=C Major  match=0.980  style=Jazz,Functional  cadence=authentic  support=1
  Current OPEN hold: 4.000 QN
  Continuation: G(4.000 QN) -> C(4.000 QN)
  Sources: FUNCTIONAL_007
#3  score=70.4  key=C Major  match=0.893  style=Pop,Jazz,City Pop  cadence=none  support=1
  Current OPEN hold: 4.000 QN
  Continuation: Em7(4.000 QN) -> Am7(4.000 QN)
  Sources: CITYPOP_JPOP_006
PERF totalMatchMs=5.989 shortlist=161
```

## Case E: I → V → vi → IV (C, G, Am, F OPEN)

```text
QUERY
  C
  G
  Am
  F
KEY
  C Major
RESOLVE
#1  score=80.1  key=C Major  match=0.982  style=Pop,Rock  cadence=authentic  support=1
  Current OPEN hold: 4.000 QN
  Continuation: Dm(2.000 QN) -> G(2.000 QN) -> C(4.000 QN)
  Sources: POP_013
#2  score=74.7  key=C Major  match=0.982  style=Pop,Rock  cadence=modal  support=1
  Current OPEN hold: 4.000 QN
  Continuation: Bb(4.000 QN) -> C(4.000 QN)
  Sources: ROCK_020
#3  score=66.1  key=C Major  match=0.744  style=Pop,Jazz,City Pop  cadence=authentic  support=1
  Current OPEN hold: 4.000 QN
  Continuation: G7(4.000 QN) -> Cmaj7(4.000 QN)
  Sources: CITYPOP_JPOP_013
DEVELOP
#1  score=67.1  key=C Major  match=0.816  style=Pop,Rock  cadence=half  support=1
  Current OPEN hold: 4.000 QN
  Continuation: C(4.000 QN) -> F(4.000 QN) -> G(4.000 QN)
  Sources: POP_014
#2  score=65.9  key=C Major  match=0.816  style=Jazz,Functional  cadence=authentic  support=1
  Current OPEN hold: 4.000 QN
  Continuation: G(4.000 QN) -> C(4.000 QN)
  Sources: FUNCTIONAL_016
LOOP
#1  score=78.9  key=C Major  match=0.982  style=Pop,Rock  cadence=loop closure  support=2
  Current OPEN hold: 4.000 QN
  Continuation: C(4.000 QN)
  Sources: POP_001 ROCK_003
#2  score=63.5  key=C Major  match=0.814  style=Pop,Rock  cadence=loop closure  support=2
  Current OPEN hold: 4.000 QN
  Continuation: G(4.000 QN) -> C(4.000 QN)
  Sources: POP_006 POP_002
COLOR
  (none)
PERF totalMatchMs=6.806 shortlist=161
```

## Case F: A minor (Am, F, Dm, E7 OPEN)

```text
QUERY
  Am
  F
  Dm
  E7
KEY
  A Minor
RESOLVE
#1  score=83.4  key=A Minor  match=0.962  style=Rock,Functional  cadence=authentic  support=8
  Current OPEN hold: 4.000 QN
  Continuation: Am(4.000 QN)
  Sources: COMMON_MINOR_003 COMMON_MINOR_012 COMMON_MINOR_001 COMMON_MINOR_013 COMMON_MINOR_020 COMMON_MINOR_014 COMMON_MINOR_002 COMMON_MINOR_004
DEVELOP
  (none)
LOOP
  (none)
COLOR
  (none)
PERF totalMatchMs=1.965 shortlist=161
```

## Case G: Unexplained chord (C, Am, F#, Dm OPEN)

```text
QUERY
  C
  Am
  F#
  Dm
KEY
  C Major
RESOLVE
#1  score=70.5  key=C Major  match=0.930  style=Pop,Functional  cadence=authentic  support=2
  Current OPEN hold: 2.000 QN
  Continuation: G(2.000 QN) -> C(4.000 QN)
  Sources: COMMON_MAJOR_004 COMMON_MAJOR_003
#2  score=67.3  key=C Major  match=0.922  style=Pop,Functional  cadence=authentic  support=1
  Current OPEN hold: 4.000 QN
  Continuation: F(4.000 QN) -> G(4.000 QN) -> C(4.000 QN)
  Sources: COMMON_MAJOR_015
DEVELOP
  (none)
LOOP
#1  score=64.1  key=C Major  match=0.854  style=R&B,Jazz  cadence=loop closure  support=1
  Current OPEN hold: 2.000 QN
  Continuation: G7(4.000 QN) -> Cmaj7(4.000 QN)
  Sources: JAZZ_014
COLOR
  (none)
PERF totalMatchMs=7.551 shortlist=161
```

## Case H: Relative key ambiguity (C, Am, F OPEN)

```text
QUERY
  C
  Am
  F
KEY
  C Major
  F Major
  A Minor
RESOLVE
#1  score=83.2  key=C Major  match=0.981  style=Pop,Functional  cadence=authentic  support=4
  Current OPEN hold: 4.000 QN
  Continuation: G(4.000 QN) -> C(4.000 QN)
  Sources: COMMON_MAJOR_013 COMMON_MAJOR_014 POP_020 COMMON_MAJOR_001
#2  score=79.1  key=C Major  match=0.981  style=Pop,Rock  cadence=authentic  support=1
  Current OPEN hold: 2.000 QN
  Continuation: Dm(2.000 QN) -> G(4.000 QN) -> C(4.000 QN)
  Sources: POP_012
DEVELOP
#1  score=61.1  key=C Major  match=0.715  style=Pop,Rock  cadence=half  support=1
  Current OPEN hold: 2.000 QN
  Continuation: G(2.000 QN)
  Sources: POP_016
LOOP
#1  score=79.1  key=C Major  match=0.981  style=Pop,Rock  cadence=loop closure  support=2
  Current OPEN hold: 4.000 QN
  Continuation: G(4.000 QN) -> C(4.000 QN)
  Sources: POP_002 POP_007
#2  score=63.1  key=C Major  match=0.699  style=Pop,R&B,Jazz  cadence=loop closure  support=2
  Current OPEN hold: 4.000 QN
  Continuation: Cmaj7(4.000 QN)
  Sources: RNB_SOUL_019 CITYPOP_JPOP_003
COLOR
#1  score=64.1  key=C Major  match=0.728  style=Pop,R&B,Jazz  cadence=plagal  support=2
  Current OPEN hold: 4.000 QN
  Continuation: Fm7(4.000 QN) -> Cmaj7(4.000 QN)
  Sources: RNB_SOUL_010 RNB_SOUL_003
#2  score=61.7  key=C Major  match=0.728  style=Pop,Jazz,City Pop  cadence=none  support=1
  Current OPEN hold: 4.000 QN
  Continuation: Fm7(4.000 QN)
  Sources: CITYPOP_JPOP_012
#3  score=61.6  key=C Major  match=0.672  style=Pop,R&B,Jazz  cadence=plagal  support=1
  Current OPEN hold: 4.000 QN
  Continuation: Fmaj7(4.000 QN) -> Fm7(4.000 QN) -> Cmaj7(4.000 QN)
  Sources: RNB_SOUL_009
PERF totalMatchMs=11.890 shortlist=161
```

## Manual music review flags

- Case C's Develop group can end on a tonic even though the template is tagged Develop; this classification may be too broad.
- Case D's Resolve and Loop alternatives have noticeably weaker match evidence than the direct iv → I Color path. They are retained above the current threshold and should be judged in Cubase.
- Case G still offers a plausible G → C after the unexplained F# chord. The explicit insertion penalty lowers the score by about ten points versus Case B, but the output may sound unmotivated.
- Case F has only a Resolve path above threshold. The engine deliberately leaves the other groups empty instead of filling them with weak candidates.
- Some jazz and City Pop catalogue entries use uniform four beat durations; a human rhythm pass is needed before treating those grooves as polished examples.
