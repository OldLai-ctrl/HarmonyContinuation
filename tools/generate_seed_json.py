"""Write the hand selected first factory seed catalogue as reviewable JSON files.

Each row is an independently chosen harmonic phrase; no transposition or Cartesian
combination is performed. Runtime and database compiler only consume the JSON.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "data" / "factory"
# sequence | intent | cadence | rhythm (blank means four QN per event)
SEEDS = {
    "common_major": ("major", "functional:1,pop:0.5", """
I IV V I|resolve|authentic|
I ii V I|resolve|authentic|
I vi ii V I|resolve|authentic|4 2 2 2 4
I vi V7/ii ii V I|resolve|authentic|4 2 1 2 2 4
I iii vi ii V I|resolve|authentic|
I IV ii V I|resolve|authentic|
I V IV I|resolve|plagal|
I IV I|resolve|plagal|
I V vi|develop|deceptive|
I IV V vi|develop|deceptive|
I ii V|develop|half|
I IV I V|develop|half|
I vi IV V I|resolve|authentic|
I iii IV V I|resolve|authentic|
I vi ii IV V I|resolve|authentic|
I V7/ii ii V I|resolve|authentic|4 2 2 4 4
I V7/V V I|resolve|authentic|4 2 2 4
I #Idim7 ii V I|resolve|authentic|4 1 3 4 4
I IV iv I|color|plagal|
I bVII IV I|color|modal|
I bVI bVII I|color|modal|
"""),
    "common_minor": ("minor", "functional:1,rock:0.4", """
i iv V i|resolve|authentic|
i iim7b5 V i|resolve|authentic|
i VI iv V i|resolve|authentic|
i III VI V i|resolve|authentic|
i VII VI V i|resolve|authentic|
i iv VII III|develop|modal|
i VI III VII|loop|loop_closure|
i VII VI VII|loop|loop_closure|
i V VI|develop|deceptive|
i iv V VI|develop|deceptive|
i iim7b5 V|develop|half|
i III iv V i|resolve|authentic|
i VI iim7b5 V i|resolve|authentic|
i VII III iv V i|resolve|authentic|
i VI VII i|color|modal|
i bII V i|color|authentic|
i iv bII V i|color|authentic|
i #Idim7 iim7b5 V i|color|authentic|4 1 3 4 4
i III VII iv V i|resolve|authentic|
i V7/iv iv V i|resolve|authentic|4 2 2 4 4
"""),
    "pop": ("major", "pop:1,rock:0.5", """
I V vi IV|loop|loop_closure|
I vi IV V|loop|loop_closure|
vi IV I V|loop|loop_closure|
IV I V vi|loop|loop_closure|
I IV vi V|loop|loop_closure|
I V IV V|loop|loop_closure|
I iii IV V|loop|loop_closure|
I V ii IV|loop|loop_closure|
vi V IV V|loop|loop_closure|
I bVII IV I|loop|modal|
I IV V vi IV V I|resolve|authentic|4 4 4 4 2 2 4
I vi IV ii V I|resolve|authentic|4 4 2 2 4 4
I V vi IV ii V I|resolve|authentic|4 4 4 4 2 2 4
I V vi iii IV I IV V|develop|half|
I iii vi IV|develop|none|
I vi ii V|develop|half|4 4 2 2
I IV iv I|color|plagal|4 4 2 4
I bVI IV V I|color|authentic|4 2 2 4 4
I bVII IV V I|color|authentic|4 2 2 4 4
I V7/vi vi IV V I|resolve|authentic|4 2 2 4 4 4
"""),
    "rock": ("major", "rock:1,pop:0.4", """
I bVII IV I|loop|modal|
I IV bVII IV|loop|loop_closure|
I V bVII IV|loop|loop_closure|
I bVI bVII I|resolve|modal|
I bVII I IV|loop|loop_closure|
I IV I bVII|loop|loop_closure|
I V IV I|resolve|plagal|4 2 2 4
I bVII IV V I|resolve|authentic|
I IV bVII V I|resolve|authentic|
I V vi bVII IV|develop|modal|
I IV V bVII|develop|modal|
I bVI bVII IV I|color|modal|
I iv bVII I|color|modal|
I bIII bVII IV I|color|modal|
I IV #IVdim V I|color|authentic|4 2 1 1 4
I V7/IV IV bVII I|color|modal|4 2 2 4 4
I vi bVII IV|develop|none|
I bVII IV bVI V I|resolve|authentic|
I IV bVI bVII I|resolve|modal|
I V vi IV bVII I|resolve|modal|
"""),
    "rnb_soul": ("major", "rnb:1,jazz:0.5,pop:0.3", """
Imaj7 vi7 ii7 V7 Imaj7|resolve|authentic|
Imaj7 iii7 vi7 ii7 V7 Imaj7|resolve|authentic|
Imaj7 IVmaj7 iv7 Imaj7|color|plagal|
Imaj7 vi7 IVmaj7 V7|loop|loop_closure|
Imaj7 V7/vi vi7 IVmaj7|develop|none|4 2 2 4
Imaj7 IVmaj7 iii7 vi7 ii7 V7|develop|half|
Imaj7 #Idim7 ii7 V7 Imaj7|color|authentic|4 1 3 4 4
Imaj7 bVImaj7 ii7 V7 Imaj7|color|authentic|
Imaj7 vi7 ii7 IVmaj7 iv7 Imaj7|color|plagal|
Imaj7 iii7 IVmaj7 iv7 Imaj7|color|plagal|
vi7 ii7 V7 Imaj7|resolve|authentic|
IVmaj7 iii7 vi7 ii7 V7 Imaj7|resolve|authentic|
Imaj7 V7/ii ii7 V7 Imaj7|resolve|authentic|4 2 2 4 4
Imaj7 IVmaj7 bVII7 Imaj7|color|modal|
Imaj7 vi7 bVImaj7 V7 Imaj7|color|authentic|
Imaj7 ii7 V7 iii7 vi7|develop|deceptive|
Imaj7 V7/vi vi7 ii7 V7 Imaj7|resolve|authentic|4 2 2 4 4 4
Imaj7 IVmaj7 V7 iii7 vi7|develop|deceptive|
Imaj7 iii7 vi7 IVmaj7|loop|loop_closure|
Imaj7 ii7 IVmaj7 V7 Imaj7|resolve|authentic|
"""),
    "jazz": ("major", "jazz:1,rnb:0.4", """
ii7 V7 Imaj7|resolve|authentic|
iii7 V7/ii ii7 V7 Imaj7|resolve|authentic|
Imaj7 V7/ii ii7 V7 Imaj7|resolve|authentic|
Imaj7 V7/ii ii7 V7 Imaj7|resolve|authentic|4 2 2 2 4
Imaj7 V7/V V7 Imaj7|resolve|authentic|4 2 2 4
Imaj7 viim7b5 iii7 vi7 ii7 V7 Imaj7|resolve|authentic|
ii7 V7 iii7 V7/ii ii7 V7 Imaj7|develop|authentic|
Imaj7 IVmaj7 viim7b5 iii7 V7/ii ii7 V7 Imaj7|develop|authentic|
Imaj7 #Idim7 ii7 V7 Imaj7|color|authentic|4 0.5 3.5 4 4
Imaj7 iv7 bVII7 Imaj7|color|modal|
Imaj7 bVImaj7 V7 Imaj7|color|authentic|
Imaj7 bII7 Imaj7|color|modal|
ii7 V7 Imaj7 V7/ii|loop|loop_closure|
Imaj7 vi7 ii7 V7|loop|loop_closure|4 2 2 4
iii7 V7/ii ii7 V7|loop|loop_closure|
Imaj7 V7/vi vi7 V7/ii ii7 V7 Imaj7|develop|authentic|4 2 2 2 2 2 4
ii7 V7 Imaj7 IVmaj7 viim7b5 iii7|develop|none|
Imaj7 IVmaj7 iv7 bVII7 Imaj7|color|modal|
Imaj7 V7/ii ii7 bII7 Imaj7|color|authentic|
ii7 V7 iii7 vi7 ii7 V7 Imaj7|develop|authentic|
"""),
    "citypop_jpop": ("major", "citypop:1,pop:0.5,jazz:0.4", """
IVmaj7 V7 iii7 vi7|loop|loop_closure|
IVmaj7 V7 iii7 vi7 ii7 V7 Imaj7|resolve|authentic|
Imaj7 V7/vi vi7 IVmaj7|loop|loop_closure|4 2 2 4
Imaj7 iii7 vi7 IVmaj7 V7|develop|half|
Imaj7 #Idim7 ii7 V7 iii7 vi7|develop|deceptive|4 1 3 4 4 4
Imaj7 IVmaj7 iv7 iii7 vi7|color|none|
Imaj7 bVImaj7 bVII7 Imaj7|color|modal|
Imaj7 IVmaj7 iii7 V7/ii ii7 V7 Imaj7|resolve|authentic|
IVmaj7 iv7 iii7 vi7 ii7 V7 Imaj7|color|authentic|
Imaj7 V7/IV IVmaj7 iv7 Imaj7|color|plagal|4 2 2 2 4
vi7 ii7 V7 Imaj7 IVmaj7|develop|none|
Imaj7 iii7 IVmaj7 iv7|color|none|
Imaj7 V7 iii7 vi7 IVmaj7 V7 Imaj7|resolve|authentic|
IVmaj7 iii7 vi7 ii7 V7 iii7|develop|deceptive|
Imaj7 IVmaj7 V7 vi7 iii7 IVmaj7 V7 Imaj7|develop|authentic|
Imaj7 bVII7 IVmaj7 iv7 Imaj7|color|plagal|
Imaj7 iii7 vi7 ii7 V7|develop|half|
Imaj7 IVmaj7 V7/vi vi7 ii7 V7 Imaj7|resolve|authentic|4 4 2 2 2 2 4
Imaj7 V7/ii ii7 iv7 Imaj7|color|plagal|
Imaj7 IVmaj7 iii7 vi7 bVImaj7 V7 Imaj7|color|authentic|
"""),
    "functional": ("major", "functional:1,jazz:0.3", """
I V7/ii ii V I|resolve|authentic|4 2 2 4 4
I V7/iii iii vi ii V I|develop|authentic|4 2 2 4 4 4 4
I V7/vi vi ii V I|resolve|authentic|4 2 2 4 4 4
I V7/V V7 I|resolve|authentic|4 2 2 4
I viidim7/ii ii V I|color|authentic|4 1 3 4 4
I #Idim7 ii V I|color|authentic|4 0.5 3.5 4 4
I IV iv V I|color|authentic|
I bVI V I|color|authentic|
I bVII V I|color|authentic|
I ii V vi|develop|deceptive|
I IV V vi ii V I|develop|authentic|
I iii vi ii V I|develop|authentic|4 2 2 2 2 4
I vi IV ii V I|resolve|authentic|4 2 2 2 2 4
I IV V I IV I|resolve|plagal|
I ii V I IV V I|resolve|authentic|
I V vi iii IV V I|develop|authentic|
I IV bVII IV I|color|modal|
I V7/IV IV V7/V V I|develop|authentic|4 2 2 2 2 4
I vi V7/ii ii V7/V V I|develop|authentic|4 4 2 2 2 2 4
I ii IV V I|resolve|authentic|4 2 2 4 4
"""),
}

for category, (mode, styles, body) in SEEDS.items():
    items = []
    for number, line in enumerate(body.strip().splitlines(), 1):
        sequence, intent, cadence, rhythm = line.strip().split("|")
        items.append(dict(
            id=f"{category.upper()}_{number:03}",
            name=f"{category.replace('_', ' ').title()} {number:02}",
            mode=mode, sequence=sequence, rhythm=rhythm,
            intent=intent, cadence=cadence,
            loopable=intent == "loop", meter="4/4", styles=styles,
            priorWeight=0.85 if category in {"common_major", "common_minor"} else 0.65,
            complexity=min(0.95, 0.25 + 0.08 * len(sequence.split())),
            sourceType="factory", tags=category.replace("_", ","), version=1,
            phraseLength=len(sequence.split()),
        ))
    ROOT.mkdir(parents=True, exist_ok=True)
    (ROOT / f"{category}.json").write_text(json.dumps(items, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(f"wrote {sum(len(v[2].strip().splitlines()) for v in SEEDS.values())} curated seeds")
