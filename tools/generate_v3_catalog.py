"""Write the explicitly authored V3 phrases, without transposition or cross products.

The resulting JSON is the production source; this script records its initial
composition. Existing V2 rows are read only to omit identical phrases.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# Each line is a complete phrase, not a list of interchangeable building blocks.
FAMILIES = [
    ("HOME", "明亮归途", "Bright Homecoming", "major", "resolve", "pop:0.8,functional:0.9", "common-major", """
I ii IV V I
I IV vi ii V I
I vi IV ii V I
I iii IV ii V I
I ii vi IV V I
I IV ii V7 I
I vi ii V7 I
I iii vi IV V I
I V vi ii V I
I V IV ii V I
I vi iii ii V I
I iii ii V I
I IV iii vi ii V I
I ii iii IV V I
I vi IV V7 I
I V ii V I
I IV V vi ii V I
I ii V vi IV V I
I vi V IV ii V I
I IV I ii V I
I ii I IV V I
I iii vi ii V7 I
I vi IV I ii V I
I V vi IV ii V I
IV I vi ii V I
IV ii V I
IV iii vi ii V I
IV I ii V I
IV vi ii V I
ii IV V I
ii vi IV V I
ii V vi ii V I
vi IV ii V I
vi I IV ii V I
vi iii IV V I
vi ii IV V I
vi V IV ii V I
iii IV ii V I
iii vi IV ii V I
iii ii V I
iii IV V I
I IV V I IV I
I vi IV V I IV I
I ii V I IV I
I V vi IV I
I iii vi IV I
I ii IV I
I vi ii IV I
I V IV ii IV I
I IV vi IV I
I iii IV I
I IV ii IV I
vi ii V I IV I
IV V I ii V I
ii V I vi ii V I
"""),
    ("OPEN", "远行篇章", "Open Horizons", "major", "develop", "pop:0.8,functional:0.7", "common-major", """
I IV vi ii V
I vi IV ii V
I iii IV ii V
I V vi ii V
I ii iii vi IV V
I IV iii vi ii V
I vi iii IV ii V
I V IV ii V
I ii I vi IV V
I IV I vi ii V
I vi V IV ii V
I iii vi IV ii V
I IV vi I ii V
I ii vi I IV V
I V vi I IV V
I vi ii IV V
I IV ii vi V
I iii IV vi ii V
I V IV I ii V
I ii IV I vi V
IV I vi ii V
IV iii vi ii V
IV vi I ii V
ii I vi IV V
ii IV I vi V
vi I IV ii V
vi iii IV ii V
iii IV I ii V
iii vi IV ii V
IV ii iii vi V
I IV V vi iii
I ii V vi IV
I iii vi ii IV
I vi IV ii iii
I IV I iii vi
I ii IV vi iii
I V vi iii ii
I vi V IV ii
I iii IV vi ii
I V IV vi ii
IV I iii vi ii
ii V vi IV iii
vi IV I ii iii
iii vi IV I ii
IV V iii vi ii
I IV vi V iii
I iii vi V IV
I ii vi V IV
I V ii IV vi
I IV iii ii vi
I vi ii iii IV
I iii IV ii vi
I V vi ii IV
I ii IV iii vi
I IV V ii vi
"""),
    ("CYCLE", "晴日循环", "Daylight Cycles", "major", "loop", "pop:1,rock:0.6", "pop", """
I ii vi IV
I iii ii IV
I IV vi V
I V ii IV
I vi iii V
I ii IV vi
I iii V IV
I IV iii ii
I V IV vi
I vi V ii
I IV ii vi
I ii iii vi
I vi ii V
I iii vi V
I V iii IV
I IV V ii
I ii V IV
I vi IV ii
I iii IV vi
I V vi ii
vi I ii IV
vi iii I IV
vi IV ii V
vi V I IV
vi ii I V
vi IV iii V
IV I ii vi
IV iii I V
IV vi ii I
IV V ii I
ii IV vi I
ii vi I V
ii I iii IV
iii IV vi I
iii vi ii I
I IV I vi IV V
I vi I ii IV V
I ii IV I vi V
I V vi I ii IV
I iii IV I vi V
I vi ii I IV V
I IV vi ii I V
I V IV I vi ii
I ii vi IV I V
I iii vi IV ii V
vi IV I iii ii V
IV I vi ii I V
ii IV I vi iii V
I IV iii vi IV V
I vi IV I iii V
I ii IV vi ii V
I V iii vi IV V
I iii IV vi I V
I IV vi I iii V
I vi iii I ii V
"""),
    ("DUSK", "暮色归途", "Dusk Homecoming", "minor", "resolve", "functional:0.9,pop:0.7", "common-minor", """
i iv iidim V i
i VI iv iidim V i
i III VI iv V i
i VII VI iv V i
i iv VI iidim V i
i iidim iv V i
i VI iidim V7 i
i iv V7 i
i III iv V i
i VII iv V i
i VI III iv V i
i iv III VI V i
i V VI iv V i
i iv V VI iidim V i
i iidim V VI iv V i
i III VI iidim V i
i VII III VI iidim V i
i VI VII iv V i
i iv VII VI V i
i VI iv i iidim V i
i iv i VI V i
i iidim i iv V i
i III i iv V i
i VII i iv V i
i V i iv V i
iv VI iidim V i
iv III VI V i
iv i iidim V i
iv VII VI V i
VI iv iidim V i
VI III iv V i
VI VII iv V i
VI i iv V i
III VI iv V i
III iv iidim V i
III VII VI V i
VII VI iv V i
VII III iv V i
VII i iidim V i
iidim iv V i
i VI iv i
i III iv i
i VII iv i
i VI III iv i
i iv VI iv i
i V VI iv i
i iidim V i iv i
i VI iidim V i iv i
i III VI V i iv i
i VII VI V i iv i
VI VII i iv V i
iv V i VI iidim V i
i III VII VI iv i
i VI VII III iv V i
i iv VI III iidim V i
"""),
    ("NIGHT", "夜行循环", "Night Cycles", "minor", "loop", "pop:0.8,rock:0.8", "common-minor", """
i iv VII VI
i VI iv VII
i III iv VII
i VII iv VI
i VI III VII
i iv III VII
i VII III iv
i III VII iv
i VI VII iv
i iv VI VII
i III VI iv
i VII VI III
i v VI iv
i iv v VI
i VI v iv
i III v VII
i VII v VI
i v iv VII
i iv VII III
i III iv VI
VI i iv VII
VI III i iv
VI VII iv i
VI iv i VII
VI v i iv
III i iv VII
III VI i iv
III VII iv i
III iv VI i
VII i iv VI
VII VI i iv
VII III i iv
iv i VI VII
iv VI i VII
iv III VII i
i VI i iv VII VI
i iv i VI VII III
i III VI i iv VII
i VII VI i III iv
i VI iv i III VII
i iv VII i VI III
i III iv i VI VII
i VII iv i III VI
i VI III i VII iv
i iv VI i III VII
i v VI i iv VII
i VI VII i iv III
i III VII i VI iv
i VII III i iv VI
i iv III i VII VI
i VI iv VII III VII
i III VI iv VII VI
i VII VI iv III VII
i iv VI III VII VI
i VI VII III iv VII
"""),
    ("VELVET", "柔光七和弦", "Velvet Sevenths", "major", "resolve", "jazz:0.9,rnb:0.8,citypop:0.7", "jazz", """
Imaj7 ii7 IVmaj7 V7 Imaj7
Imaj7 IVmaj7 vi7 ii7 V7 Imaj7
Imaj7 vi7 IVmaj7 ii7 V7 Imaj7
Imaj7 iii7 IVmaj7 ii7 V7 Imaj7
Imaj7 ii7 vi7 IVmaj7 V7 Imaj7
Imaj7 IVmaj7 ii7 V7 I
Imaj7 vi7 ii7 V7 I
Imaj7 iii7 vi7 IVmaj7 V7 Imaj7
Imaj7 V7 vi7 ii7 V7 Imaj7
Imaj7 vi7 iii7 ii7 V7 Imaj7
Imaj7 iii7 ii7 V7 Imaj7
Imaj7 IVmaj7 iii7 vi7 ii7 V7 Imaj7
Imaj7 ii7 iii7 IVmaj7 V7 Imaj7
Imaj7 vi7 IVmaj7 V7 Imaj7
Imaj7 IVmaj7 Imaj7 ii7 V7 Imaj7
Imaj7 ii7 Imaj7 IVmaj7 V7 Imaj7
Imaj7 vi7 IVmaj7 Imaj7 ii7 V7 Imaj7
IVmaj7 Imaj7 vi7 ii7 V7 Imaj7
IVmaj7 ii7 V7 Imaj7
IVmaj7 iii7 vi7 ii7 V7 Imaj7
IVmaj7 Imaj7 ii7 V7 Imaj7
IVmaj7 vi7 ii7 V7 Imaj7
ii7 IVmaj7 V7 Imaj7
ii7 vi7 IVmaj7 V7 Imaj7
ii7 V7 vi7 ii7 V7 Imaj7
vi7 IVmaj7 ii7 V7 Imaj7
vi7 Imaj7 IVmaj7 ii7 V7 Imaj7
vi7 iii7 IVmaj7 V7 Imaj7
vi7 ii7 IVmaj7 V7 Imaj7
iii7 IVmaj7 ii7 V7 Imaj7
iii7 vi7 IVmaj7 ii7 V7 Imaj7
iii7 ii7 V7 Imaj7
iii7 IVmaj7 V7 Imaj7
Imaj7 IVmaj7 V7 Imaj7 IVmaj7 Imaj7
Imaj7 vi7 IVmaj7 V7 Imaj7 IVmaj7 Imaj7
Imaj7 ii7 V7 Imaj7 IVmaj7 Imaj7
Imaj7 V7 vi7 IVmaj7 Imaj7
Imaj7 iii7 vi7 IVmaj7 Imaj7
Imaj7 ii7 IVmaj7 Imaj7
Imaj7 vi7 ii7 IVmaj7 Imaj7
Imaj7 IVmaj7 vi7 IVmaj7 Imaj7
Imaj7 iii7 IVmaj7 Imaj7
vi7 ii7 V7 Imaj7 IVmaj7 Imaj7
IVmaj7 V7 Imaj7 ii7 V7 Imaj7
ii7 V7 Imaj7 vi7 ii7 V7 Imaj7
Imaj7 V7/vi vi7 ii7 V7 Imaj7
Imaj7 V7/iii iii7 vi7 ii7 V7 Imaj7
Imaj7 V7/ii ii7 IVmaj7 V7 Imaj7
IVmaj7 V7/ii ii7 V7 Imaj7
vi7 V7/ii ii7 V7 Imaj7
iii7 V7/vi vi7 ii7 V7 Imaj7
Imaj7 vi7 V7/V V7 Imaj7
Imaj7 IVmaj7 V7/V V7 Imaj7
ii7 V7/V V7 Imaj7
Imaj7 viidim7 V7 Imaj7
"""),
    ("BLUE", "深蓝夜色", "Blue Evening", "minor", "resolve", "jazz:0.9,rnb:0.8", "common-minor", """
i7 iv7 iim7b5 V7 i7
i7 VImaj7 iv7 iim7b5 V7 i7
i7 IIImaj7 VImaj7 iv7 V7 i7
i7 VII7 VImaj7 iv7 V7 i7
i7 iv7 VImaj7 iim7b5 V7 i7
i7 iim7b5 iv7 V7 i7
i7 VImaj7 iim7b5 V7 i7
i7 iv7 V7 i
i7 IIImaj7 iv7 V7 i7
i7 VII7 iv7 V7 i7
i7 VImaj7 IIImaj7 iv7 V7 i7
i7 iv7 IIImaj7 VImaj7 V7 i7
i7 V7 VImaj7 iv7 V7 i7
i7 iv7 V7 VImaj7 iim7b5 V7 i7
i7 iim7b5 V7 VImaj7 iv7 V7 i7
i7 IIImaj7 VImaj7 iim7b5 V7 i7
i7 VII7 IIImaj7 VImaj7 iim7b5 V7 i7
i7 VImaj7 VII7 iv7 V7 i7
i7 iv7 VII7 VImaj7 V7 i7
i7 VImaj7 iv7 i7 iim7b5 V7 i7
i7 iv7 i7 VImaj7 V7 i7
i7 iim7b5 i7 iv7 V7 i7
i7 IIImaj7 i7 iv7 V7 i7
i7 VII7 i7 iv7 V7 i7
iv7 VImaj7 iim7b5 V7 i7
iv7 IIImaj7 VImaj7 V7 i7
iv7 i7 iim7b5 V7 i7
iv7 VII7 VImaj7 V7 i7
VImaj7 iv7 iim7b5 V7 i7
VImaj7 IIImaj7 iv7 V7 i7
VImaj7 VII7 iv7 V7 i7
VImaj7 i7 iv7 V7 i7
IIImaj7 VImaj7 iv7 V7 i7
IIImaj7 iv7 iim7b5 V7 i7
IIImaj7 VII7 VImaj7 V7 i7
VII7 VImaj7 iv7 V7 i7
VII7 IIImaj7 iv7 V7 i7
VII7 i7 iim7b5 V7 i7
iim7b5 iv7 V7 i7
i7 VImaj7 iv7 i7
i7 IIImaj7 iv7 i7
i7 VII7 iv7 i7
i7 VImaj7 IIImaj7 iv7 i7
i7 iv7 VImaj7 iv7 i7
i7 V7 VImaj7 iv7 i7
i7 iim7b5 V7 i7 iv7 i7
i7 VImaj7 iim7b5 V7 i7 iv7 i7
i7 IIImaj7 VImaj7 V7 i7 iv7 i7
i7 V7/iv iv7 iim7b5 V7 i7
i7 VImaj7 V7/iv iv7 V7 i7
i7 V7/III IIImaj7 VImaj7 V7 i7
i7 V7/VI VImaj7 iv7 V7 i7
i7 iv7 V7/V V7 i7
i7 VImaj7 V7/V V7 i7
iv7 V7/V V7 i7
"""),
    ("SHADE", "借用色彩", "Borrowed Shades", "major", "color", "citypop:0.8,rock:0.7,pop:0.7", "common-major", """
I vi IV iv I
I iii IV iv I
I ii IV iv I
I V vi IV iv I
I IV vi iv I
I vi ii iv I
I IV I iv I
I iii vi iv I
I ii V IV iv I
IV iii vi iv I
vi IV iv I
ii IV iv I
I bVI IV iv I
I bIII IV iv I
I bVII IV iv I
I IV bVI V I
I vi bVI V I
I ii bVI V I
I iii bVI V I
I bIII bVI V I
I bVII bVI V I
I IV iv V I
I vi iv V I
I iii iv V I
I ii iv V I
I bVI iv V I
I bIII iv V I
I bVII iv V I
I IV bVII IV I
I vi bVII IV I
I iii bVII IV I
I ii bVII IV I
I bVI bVII IV I
I bIII bVII IV I
I V bVII IV I
I bVII IV ii V I
I bVI IV ii V I
I bIII IV ii V I
I IV bVII ii V I
I vi bVII ii V I
I bVI ii V I
I bIII ii V I
I bVII ii V I
Imaj7 IVmaj7 iv7 Imaj7
Imaj7 vi7 IVmaj7 iv7 Imaj7
Imaj7 iii7 IVmaj7 iv7 Imaj7
Imaj7 ii7 IVmaj7 iv7 Imaj7
Imaj7 bVImaj7 V7 Imaj7
Imaj7 bIIImaj7 IVmaj7 Imaj7
Imaj7 bVII7 IVmaj7 Imaj7
Imaj7 IVmaj7 bVII7 Imaj7
Imaj7 vi7 bVII7 Imaj7
Imaj7 ii7 bVII7 Imaj7
Imaj7 IVmaj7 iv7 bVII7 Imaj7
Imaj7 bVImaj7 bVII7 Imaj7
"""),
    ("SPARK", "转折引路", "Chromatic Pathways", "major", "resolve", "functional:1,jazz:0.6,citypop:0.5", "functional", """
I V7/ii ii IV V I
I vi V7/ii ii V I
I IV V7/ii ii V I
I iii V7/ii ii V I
I V7/vi vi IV V I
I IV V7/vi vi ii V I
I ii V7/vi vi IV V I
I iii V7/vi vi ii V I
I V7/iii iii IV V I
I vi V7/iii iii ii V I
I IV V7/iii iii vi ii V I
I V7/IV IV ii V I
I vi V7/IV IV V I
I iii V7/IV IV V I
I ii V7/IV IV V I
I IV ii V7/V V I
I vi ii V7/V V I
I iii IV V7/V V I
I V7/ii ii V7/V V I
I V7/vi vi V7/ii ii V I
I V7/iii iii V7/vi vi ii V I
I V7/IV IV V7/ii ii V I
I vi V7/ii ii V7/V V I
IV V7/ii ii V I
vi V7/ii ii IV V I
iii V7/vi vi ii V I
ii V7/V V I
IV V7/V V I
vi V7/V V I
I #Idim7 ii IV V I
I vi #Idim7 ii V I
I IV #Idim7 ii V I
I iii #Idim7 ii V I
I ii #IIdim7 iii IV V I
I #IIdim7 iii vi ii V I
I IV #IVdim7 V I
I ii #IVdim7 V I
I vi #IVdim7 V I
I iii IV #IVdim7 V I
I V7/vi vi #Idim7 ii V I
I #Idim7 ii V7/V V I
I vi V7/ii ii #IVdim7 V I
I IV V7/ii ii #IVdim7 V I
I V7/IV IV #IVdim7 V I
I #IIdim7 iii V7/vi vi ii V I
I V7/iii iii #Idim7 ii V I
I IV viidim7 I
I vi viidim7 I
I ii viidim7 I
I iii IV viidim7 I
I IV ii viidim7 I
I V vi ii viidim7 I
I vi IV viidim7 I
IV ii viidim7 I
vi ii viidim7 I
"""),
]


def key(row):
    tokens = row["sequence"].split()
    # im7 and i7 encode the same Minor7. The runtime compiler verifies its own
    # realized phrase key too; this textual key only avoids obvious seed repeats.
    tokens = [t.replace("m7", "7") if t.endswith("m7") else t for t in tokens]
    durations = tuple(float(x) for x in row.get("rhythm", "").split()) or (4.0,) * len(tokens)
    return row["mode"], tuple(tokens), durations


def main():
    seen = {key(row) for path in (ROOT / "data/factory").glob("*.json")
            for row in json.loads(path.read_text(encoding="utf-8"))}
    output = ROOT / "data/factory-v3"
    output.mkdir(exist_ok=True)
    accepted = skipped = 0
    for code, zh, en, mode, intent, styles, tag, phrases in FAMILIES:
        rows = []
        for number, sequence in enumerate(phrases.strip().splitlines(), 1):
            draft = {"mode": mode, "sequence": sequence}
            signature = key(draft)
            if signature in seen:
                skipped += 1
                continue
            seen.add(signature)
            tokens = sequence.split()
            dominant = tokens[-2] in ("V", "V7")
            plagal = tokens[-2] in ("IV", "iv", "IVmaj7", "iv7")
            cadence = ("loop_closure" if intent == "loop" else
                       "authentic" if intent == "resolve" and dominant else
                       "plagal" if plagal and tokens[-1] in ("I", "i", "Imaj7", "i7") else
                       "half" if tokens[-1] in ("V", "V7") else "none")
            sevenths = any("7" in t for t in tokens)
            secondary = any("/" in t for t in tokens)
            diminished = any("dim7" in t for t in tokens)
            borrowed = mode == "major" and any(t.startswith("b") or t in ("iv", "iv7") for t in tokens)
            tags = [tag, intent]
            techniques = []
            for present, value, technique in [
                (sevenths, "seventh_chord", "SeventhColor"),
                (secondary, "secondary_dominant", "SecondaryDominant"),
                (diminished, "passing_diminished", "PassingDiminished"),
                (borrowed, "borrowed_chord", "BorrowedChord"),
            ]:
                if present:
                    tags.append(value)
                    techniques.append(technique)
            level = "advanced" if secondary or diminished else "rich" if sevenths or borrowed else "basic"
            name_zh, name_en = f"{zh} · {number:02}", f"{en} · {number:02}"
            row = dict(id=f"V3_{code}_{number:03}", name=name_en, nameZh=name_zh,
                       nameEn=name_en, aliases=f"{zh},{en}", builtInTags=",".join(tags),
                       techniques=",".join(techniques), complexityLevel=level,
                       description=f"{zh}；完整乐句，按所示和弦时值展开。",
                       mode=mode, sequence=sequence, rhythm=" ".join(["4"] * len(tokens)),
                       intent=intent, cadence=cadence, loopable=intent == "loop", meter="4/4",
                       styles=styles, tags=tag, sourceType="factory", priorWeight=0.5,
                       complexity=0.65 if level == "advanced" else 0.45 if level == "rich" else 0.25,
                       version=3, phraseLength=len(tokens))
            rows.append(row)
        (output / f"{code.lower()}.json").write_text(
            json.dumps(rows, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        accepted += len(rows)
        print(f"{code}: {len(rows)} new phrases")
    print(f"Authored additions={accepted}; identical source phrases omitted={skipped}; canonical total={155+accepted}")


if __name__ == "__main__":
    main()
