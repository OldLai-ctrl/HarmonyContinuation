"""Write the hand-curated Phase 5 quality review inputs. Run only when editing the case set."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "data" / "benchmark" / "continuation"
cases = []


def add(category, name, phrase, notes, expected, *, tempo=120, meter=(4, 4),
        style=None, forced_key=None):
    chords = []
    start = 0.0
    for token in phrase.split():
        label, length = token.rsplit("@", 1)
        chord = {"name": label, "start": start}
        if length != "?":
            chord["duration"] = float(length)
            start += float(length)
        chords.append(chord)
    item = {
        "id": f"bench_{len(cases) + 1:03}", "name": name, "category": category,
        "tempo": tempo, "meter": list(meter), "chords": chords,
        "notes": notes, "expectedCharacteristics": expected,
    }
    if style:
        item["style"] = style
    if forced_key:
        item["forcedKey"] = forced_key
    cases.append(item)


add("major_functional", "I IV V in C", "C@4 F@4 G@?", "Plain pre-cadential major phrase.", ["A complete resolution path should be available."])
add("major_functional", "I vi ii V", "C@4 Am@4 Dm@2 G@?", "Classic turnaround with a short predominant.", ["Resolve and develop should be meaningfully distinguishable."])
add("major_functional", "G major predominant", "G@4 C@4 D@?", "Subdominant to dominant in G.", ["A tonic-directed option should be present."], forced_key="G:major")
add("major_functional", "F major cadence", "F@4 Bb@2 C@?", "Short F-major phrase with compressed predominant.", ["OPEN hold should remain positive."], forced_key="F:major")

add("minor_functional", "A minor dominant", "Am@4 Dm@4 E7@?", "Raised leading tone before A-minor cadence.", ["Minor-key interpretation should remain plausible."], forced_key="A:minor")
add("minor_functional", "A minor diminished approach", "Am@4 F@4 Bdim@2 E7@?", "Diminished chord prepares dominant.", ["The diminished approach should not collapse the match."], forced_key="A:minor")
add("minor_functional", "E minor dominant", "Em@4 Am@4 B7@?", "Compact E-minor cadence setup.", ["A multi-chord continuation is preferable to a lone tonic."], forced_key="E:minor")
add("minor_functional", "D minor dominant", "Dm@4 Gm@4 A7@?", "Subdominant and dominant in D minor.", ["Resolve intent should remain musically useful."], forced_key="D:minor")

add("ii_v", "D minor seven to G seven", "Dm7@2 G7@?", "Two-chord C-major ii-V.", ["The output should offer more than the next chord alone."], style="jazz")
add("ii_v", "F sharp half diminished", "F#m7b5@2 B7@?", "Minor ii-V aiming at E minor.", ["Half-diminished input should remain analyzable."], style="jazz", forced_key="E:minor")
add("ii_v", "A minor seven to D seven", "Am7@2 D7@2 Gmaj7@?", "G-major ii-V with tonic already supplied.", ["Continuation may develop beyond the existing tonic."], style="jazz", forced_key="G:major")
add("ii_v", "G minor seven to C seven", "Gm7@2 C7@2 Fmaj7@?", "F-major ii-V with an open tonic.", ["OPEN chord hold should be explainable."], style="jazz", forced_key="F:major")

add("secondary_dominant", "A seven into D minor", "C@4 Am@4 A7@2 Dm@?", "Secondary dominant V/ii in C.", ["Secondary dominant should not break matching."])
add("secondary_dominant", "E seven into A minor", "C@4 E7@2 Am@4 Dm@?", "Applied dominant lands on vi.", ["Different intents should not return identical paths only."])
add("secondary_dominant", "B seven into E minor", "G@4 B7@2 Em@4 Am@?", "Applied dominant in G-major surroundings.", ["A full continuation should follow the applied resolution."], forced_key="G:major")

add("borrowed_iv", "C major borrowed minor iv", "C@4 F@4 Fm@?", "Parallel-minor iv invites a color cadence.", ["Color option should be inspected for modal borrowing."])
add("borrowed_iv", "G major borrowed minor iv", "G@4 C@4 Cm@?", "Borrowed iv in G major.", ["Borrowed chord should not be treated as a parsing error."], forced_key="G:major")
add("borrowed_iv", "F major borrowed minor iv", "F@4 Bb@4 Bbm@?", "Borrowed iv in F major.", ["Color and resolve paths should be compared."], forced_key="F:major")

add("flat_vii_vi", "C major flat seven", "C@4 Bb@4 F@?", "Rock-like bVII-IV motion.", ["Loop intent should be assessed by ear."], style="rock")
add("flat_vii_vi", "A minor descending colors", "Am@4 G@4 F@?", "Descending bVII-bVI in A minor.", ["Minor and relative-major interpretations may compete."], style="rock")
add("flat_vii_vi", "G major flat six", "G@4 Eb@4 F@?", "Chromatic bVI-bVII ascent.", ["Color option should avoid generic tonic-only output."], style="rock", forced_key="G:major")

add("pop_loop", "Four chord C pop loop", "C@4 G@4 Am@4 F@?", "Familiar I-V-vi-IV loop with unfinished final bar.", ["Loop should reconnect naturally to the opening."], style="pop")
add("pop_loop", "Four chord G pop loop", "G@4 D@4 Em@4 C@?", "I-V-vi-IV transposed to G.", ["Compare against the C loop for transposition consistency."], style="pop", forced_key="G:major")
add("pop_loop", "Minor-first pop loop", "Am@4 F@4 C@4 G@?", "vi-IV-I-V viewed from a minor opening.", ["Key ambiguity should remain visible."], style="pop")

add("jazz_functional", "C jazz turnaround", "Cmaj7@4 A7@2 Dm7@2 G7@?", "I-V/ii-ii-V with seventh chords.", ["Complete cadence and more expansive development should both be reviewed."], style="jazz")
add("jazz_functional", "F jazz turnaround", "Fmaj7@4 D7@2 Gm7@2 C7@?", "F-major turnaround.", ["Applied dominant should retain its function."], style="jazz", forced_key="F:major")
add("jazz_functional", "B flat jazz turnaround", "Bbmaj7@4 G7@2 Cm7@2 F7@?", "B-flat turnaround with compact ii-V.", ["Candidate path length should be checked."], style="jazz", forced_key="Bb:major")

add("citypop_rnb", "City pop chromatic turnaround", "Cmaj7@4 E7@2 Am7@2 D7@?", "Chromatic dominant chain in a city-pop setting.", ["Style support should not force a single template."], style="citypop", tempo=104)
add("citypop_rnb", "R and B seventh color", "Fmaj7@4 Em7@2 Am7@2 Dm7@?", "Smooth seventh-chord descent.", ["Rhythm and usability need separate listening scores."], style="rnb", tempo=88)
add("citypop_rnb", "A flat color progression", "Abmaj7@4 Gm7@2 C7@2 Fm7@?", "Jazz-color phrase in A-flat surroundings.", ["Check whether key interpretation stays coherent."], style="citypop", tempo=100, forced_key="Ab:major")

add("short", "Two chord C to G", "C@4 G@?", "Very little context; exposes overconfidence.", ["Several interpretations may be reasonable."])
add("short", "Two chord minor dominant", "Am@4 E7@?", "Short minor phrase with strong dominant.", ["A useful continuation should include a phrase arc."], forced_key="A:minor")

add("long", "Six chord major phrase", "C@4 Em@2 Am@2 F@4 Dm@2 G@?", "Longer phrase reaches a clear dominant after varied durations.", ["Matcher should still yield distinct full paths."], style="pop")
add("long", "Five chord minor phrase", "Am@4 G@2 C@2 Dm@4 E7@?", "Longer A-minor phrase crosses the relative major before its dominant.", ["Avoid a one-chord recommendation after a long input."], forced_key="A:minor")

add("key_ambiguity", "Relative major or minor", "Am@4 F@4 C@4 G@?", "C-major and A-minor readings both plausible.", ["Report multiple key candidates without treating one as certain."])
add("key_ambiguity", "Sparse C and A minor", "C@4 Am@4 F@?", "Short context cannot fully settle tonal center.", ["Different intents may diverge despite key uncertainty."])

add("anomalous", "Chromatic F sharp insertion", "C@4 F@4 F#@2 G@?", "Unexpected chromatic chord before dominant.", ["Flag bland normalization during listening; do not patch templates."])
add("anomalous", "A minor flat six interruption", "Am@4 Dm@4 Ab@2 E7@?", "Out-of-pattern Ab tests robustness.", ["No crash or nonfinite score is acceptable."], forced_key="A:minor")

add("open_duration", "Open A minor after C and G", "C@4 G@2 Am@?", "Final hold is unspecified and must be proposed.", ["Every displayed OPEN hold must be positive."], tempo=92)
add("open_duration", "Open G dominant after D minor", "Dm7@2 G7@?", "Short ii-V with unknown current hold.", ["Candidate duration changes should be visible in report."], style="jazz")

add("near_cadence", "Dominant already present", "C@4 F@4 Dm@2 G@?", "Input is one step from a familiar cadence.", ["Resolve should not obscure the existing phrase boundary."])
add("near_cadence", "Minor dominant already present", "Am@4 Dm@4 E7@?", "Minor-key resolution may need more than a tonic event.", ["Compare complete-path ratio against major cadences."], forced_key="A:minor", tempo=96)

assert len(cases) == 42, len(cases)
ROOT.mkdir(parents=True, exist_ok=True)
for item in cases:
    (ROOT / f"{item['id']}.json").write_text(json.dumps(item, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(f"wrote {len(cases)} benchmark cases")
