"""Explicit, original Phase 1 A/B phrases. Authoring helper; never edits V3.

@n is a root-relative bassInterval, NOT a secondary-function slash.
Rich suffixes are lowered to existing Schema 2 typedData plus seed qualities.
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# id | mode | sequence | styles | intent | cadence | techniques | musical purpose
PHRASES = """
A01|major|I bII ii bIII iii IV V I|pop,functional|resolve|authentic|ChromaticRootAsc,ModalMixture|Two chromatic root approaches followed by a clear tonic return.
A02|major|I VII bVII VI bVI V I|rock,pop|resolve|authentic|ChromaticRootDesc,SameQualityMotion|Descending major-chord roots; no fixed parallel voicing is promised.
A03|major|I iii biiidim7 ii V7 I|jazz,rnb|resolve|authentic|PassingDiminishedDesc,CommonToneConnection|Eb diminished connects E minor down to D minor; shared notes are available, not voice-locked.
A04|major|I vi bvidim7 V7 I|jazz,functional|resolve|authentic|PassingDiminishedDesc|Ab diminished shares B D F with G7 while its root descends.
A05|major|I IV #ivdim7 V vi ii V7 I|pop,functional|resolve|authentic|SecondaryLeadingTone,DeceptiveResolution|Approach V, evade to vi, then recover through ii-V-I.
A06|major|I #idim7 ii #iidim7 iii V7 I|jazz,functional|resolve|authentic|SecondaryLeadingTone,PassingDiminishedAsc,ChromaticRootAsc|Two separate ascending diminished approaches resolve to ii and iii.
A07|major|I bIII IV iv I V7 I|pop,citypop|resolve|authentic|ModalMixture,BorrowedChord|Borrowed mediant and minor IV resolve plagally before a final dominant cadence.
A08|minor|i IV i VII IV i|rock,rnb|loop|loop_closure|ModalMixture,DorianColor|Major IV supplies natural six against a minor tonic; no modal-key engine is claimed.
A09|minor|i bII VI V7 i|functional,rock|resolve|authentic|Neapolitan,BorrowedChord|Flat II and VI form a chromatic predominant region before V-i.
A10|major|I IV V7 bVI bVII I|pop,rock|resolve|modal|DeceptiveResolution,ModalMixture|V7 evades tonic through borrowed bVI-bVII-I.
A11|major|I ii V7 iv I|pop,citypop|resolve|plagal|EvadedResolution,BorrowedChord|Minor IV changes the expected dominant-to-tonic route.
A12|major|I IV V7 vi ii V7|pop,functional|develop|half|DeceptiveResolution,OpenEnding|Deceptive motion is followed by a new open dominant ending.
A13|major|I bIII bVI III V I|citypop,jazz|color|authentic|ChromaticMediant,ModalMixture|Multiple same-major third relations return through V; no cinematic style claim.
A14|major|I iv bVI bII@4 V7 I|functional,pop|resolve|authentic|NeapolitanSixth,ModalMixture,SlashBass|First-inversion flat II distinguishes N6 from root-position flat II; adjusted from Phase 1.
B01|major|III7 VI7 II7 V7 I|jazz,functional|resolve|authentic|DominantChain,SecondaryDominant|E7 targets A7, A7 targets D7, D7 targets G7; targets are actual next roots.
B02|major|I V7/iii iii V7/ii ii V7 I|jazz,citypop|resolve|authentic|SequentialTonicization,SecondaryDominant|B7-Em and A7-Dm are separate tonicizations before G7-C.
B03|major|I III7 VI7 ii V7 I|jazz,rnb|resolve|authentic|DominantChain,SecondaryDominant|E7 targets A7, which resolves to D minor rather than D7.
B04|major|Imaj7 iii7 bIII7 IImaj7 ii7 V7 Imaj7|jazz,citypop|resolve|authentic|TritoneSubstitution,SequentialTonicization|Eb7 substitutes for A7 into D major, then D minor returns to the home ii-V.
B05|major|Imaj7 vi7 bVI7 Vmaj7 V7 Imaj7|jazz|resolve|authentic|TritoneSubstitution|Ab7 substitutes for D7 into G major; Gmaj7 changes to G7 to return home.
B06|major|Imaj7 iii7 V7/ii ii7 iv7 bVII7 Imaj7|rnb,jazz|resolve|plagal|Backdoor,SecondaryDominant|A functional approach to ii turns into the minor-IV backdoor.
B07|major|Imaj7 v7 I7 IVmaj7 iv7 bVII7 Imaj7|jazz,rnb|resolve|plagal|Backdoor,SequentialTonicization|Gm7-C7 tonicizes F, then F minor leads back through Bb7.
B08|major|I Imaj7@11 I7@10 IV@4 iv@3 I@7 V7 I|pop,citypop|resolve|authentic|ChromaticBassDesc,SlashBass,BorrowedChord|Bass C B Bb A Ab G defines the phrase; complete G7-C ending.
B09|major|I@4 IV #ivdim7 V vi V@4 I|pop,functional|resolve|authentic|ChromaticBassAsc,SlashBass,SecondaryLeadingTone|Bass E F F# G A B C; only its first section is chromatic.
B10|major|I IV@7 iv@7 I V7@5 I|pop,rnb|color|plagal|PedalPoint,SlashBass,BorrowedChord|C bass remains fixed under changing upper harmony including G7/C.
B11|major|Imaj9 bIImaj9 Imaj9 vi9 ii9 V13 Imaj9|jazz,rnb|resolve|authentic|SideSlipping,SeventhColor|Cmaj9 moves to Dbmaj9 and returns exactly before a functional cadence.
B12|major|Imaj7 bIIImaj7 bIImaj7 Imaj7 IVmaj7 V7 Imaj7|jazz,citypop|color|authentic|SameQualityMotion,ChromaticMediant|Nonfunctional same-quality movement returns to tonic; strict planing is not claimed.
B13|major|I I7@4 IV iv@3 I@7 V7 I|rnb,functional|resolve|authentic|SlashBass,ModalMixture,GospelCandidate|Inverted tonic dominant pushes to IV; minor-IV bass and tonic over G shape the return.
B14|major|Imaj9 III7@4 vi9 bVI13 V13 Imaj9|rnb,jazz|resolve|authentic|ChromaticBassDesc,SecondaryDominant,TritoneSubstitution,NeoSoulCandidate|G#-A then Ab-G bass supports secondary resolution and dominant-region tritone substitution; style needs listening review.
""".strip()
ROMANS = ['I', 'II', 'III', 'IV', 'V', 'VI', 'VII']
NAMES = {
    'A01': ('半音根音上行与回归', 'Chromatic Root Ascent and Return'),
    'A02': ('大和弦根音半音下行', 'Descending Major-Chord Roots'),
    'A03': ('下降减七连接至 ii', 'Descending Diminished Approach to ii'),
    'A04': ('下降减七连接至属七', 'Descending Diminished Approach to V7'),
    'A05': ('副导接属后欺骗回收', 'Leading-Tone Approach and Deceptive Recovery'),
    'A06': ('两个目标的上行减七', 'Ascending Diminished Approaches to ii and iii'),
    'A07': ('借用三度与小下属回归', 'Borrowed Mediant and Minor-Subdominant Return'),
    'A08': ('小主和弦与 Dorian 大 IV', 'Minor Tonic with Dorian Major IV'),
    'A09': ('小调降二级属前区', 'Minor Flat-II Predominant Region'),
    'A10': ('属七经借用降六级回归', 'Borrowed Flat-VI Deceptive Return'),
    'A11': ('属七转小下属终止', 'Dominant to Minor-Subdominant Cadence'),
    'A12': ('欺骗后再次开放属结尾', 'Deceptive Recovery to an Open Dominant'),
    'A13': ('色彩三度链与属回归', 'Chromatic Mediant Chain and Dominant Return'),
    'A14': ('混合属前与 Neapolitan 六和弦', 'Mixed Predominants with Neapolitan Sixth'),
    'B01': ('四级连续属七链', 'Four-Stage Dominant Chain'),
    'B02': ('iii 与 ii 的连续主音化', 'Sequential Tonicization of iii and ii'),
    'B03': ('连续属链转入小 ii', 'Dominant Chain into Minor ii'),
    'B04': ('三全音替代至大 II', 'Tritone Substitution to Major II'),
    'B05': ('三全音替代至大 V', 'Tritone Substitution to Major V'),
    'B06': ('副属路线转 Backdoor', 'Secondary-Dominant Route into Backdoor'),
    'B07': ('IV 主音化与 Backdoor', 'IV Tonicization and Backdoor'),
    'B08': ('半音下降低音完整乐句', 'Complete Chromatic Descending Bass Phrase'),
    'B09': ('上行低音与副导经过', 'Ascending Bass with Leading-Tone Approach'),
    'B10': ('主音持续低音与小下属', 'Tonic Pedal with Minor Subdominant'),
    'B11': ('半音 Side-slipping 移出与返回', 'Semitone Side-Slipping and Return'),
    'B12': ('同性质色彩移动后回归', 'Same-Quality Color Motion and Return'),
    'B13': ('转位属前与变格回归', 'Inverted Predominant and Plagal Return'),
    'B14': ('扩展属与半音低音回收', 'Extended Dominants and Chromatic Bass Recovery'),
}
QUALITIES = {'': (1, [0,4,7]), 'm': (2,[0,3,7]), '7': (3,[0,4,7,10]),
             'maj7': (4,[0,4,7,11]), 'm7': (5,[0,3,7,10]),
             'dim7': (7,[0,3,6,9]), 'maj9': (4,[0,2,4,7,11]),
             'm9': (5,[0,2,3,7,10]), '13': (3,[0,2,4,7,9,10])}

def degree(text):
    m = re.fullmatch(r'([b#]?)([ivIV]+)', text)
    return [ROMANS.index(m[2].upper()) + 1, {'':0,'b':-1,'#':1}[m[1]]]

def author(line):
    code, mode, notation, styles, intent, cadence, techniques, purpose = line.split('|')
    scale = [0,2,4,5,7,9,11] if mode == 'major' else [0,2,3,5,7,8,10]
    events, seeds = [], []
    for token in notation.split():
        body, *bass = token.split('@')
        body, *target_text = body.split('/')
        m = re.fullmatch(r'([b#]?[ivIV]+)(.*)', body)
        d, suffix = degree(m[1]), m[2]
        lower = m[1][-1].islower()
        quality_suffix = ('m' if not suffix else 'm'+suffix) if lower and suffix in ('','7','9') else suffix
        q, pitches = QUALITIES[quality_suffix]
        target = degree(target_text[0]) if target_text else None
        if target:
            root = (scale[target[0]-1] + target[1] + 7) % 12
            index, alter = min(enumerate((root-v+18)%12-6 for v in scale), key=lambda z:abs(z[1]))
            d = [index+1, alter]
        function = {1:1,2:3,3:2,4:3,5:4,6:2,7:4}[d[0]]
        diatonic = ({1,4},{2,5},{2,5},{1,4},{1,3},{2,5},{6,8}) if mode == 'major' else (
            {2,5},{6,8},{1,4},{2,5},{2,5,1,3},{1,4},{1,3})
        if q not in diatonic[d[0]-1]: function = 0
        roles = 1
        if target:
            roles |= 16; function = 6
        elif d[1] or mode == 'major' and d[0] == 4 and lower:
            roles |= 64; function = 5 if d[0] == 4 else 0
        if code == 'B11' and d == [2,-1]:
            roles = 1; function = 0  # nonfunctional excursion, not a predominant
        if code == 'A08' and d == [4,0]: roles |= 64; function = 5
        if code in ('A09','A14') and d == [2,-1]: function = 5
        if bass: roles |= 256
        seed_suffix = {1:'',2:'',3:'7',4:'maj7',5:'7',7:'dim7'}[q]
        # Sequence is parseable legacy notation; typedData is the exact authority.
        seed_degree = ('b' if d[1]<0 else '#' if d[1]>0 else '') + ROMANS[d[0]-1]
        if q in (2,5,7): seed_degree = seed_degree.lower()
        seeds.append(seed_degree + seed_suffix)
        events.append(dict(degree=d,target=target,quality=q,function=function,roles=roles,
            weight=.85,durationQN=4,sourceIndex=len(events),bassInterval=int(bass[0]) if bass else None,
            intervalMask=sum(1<<n for n in pitches) if suffix in ('maj9','9','13') else 0,
            colorMask=0,suffix=quality_suffix if suffix in ('maj9','9','13') else ''))
    def root(e): return (scale[e['degree'][0]-1]+e['degree'][1])%12
    for i, e in enumerate(events[:-1]):
        nxt = events[i+1]; distance = (root(nxt)-root(e))%12
        if e['quality'] == 7:
            e['roles'] = (e['roles'] & ~64) | 8 | 4
            if distance == 1:
                e['function'] = 6 if nxt['degree'] != [1,0] else 4
                if nxt['degree'] != [1,0]: e['target']=nxt['degree']; e['roles'] |= 32
        if code in ('B01','B03') and e['quality']==3 and distance==5 and e['degree'] != [5,0]:
            e['target']=nxt['degree']; e['roles'] |= 16; e['function']=6
        if code in ('B04','B05','B14') and e['quality']==3 and distance==11:
            e['target']=nxt['degree']; e['roles']=(e['roles'] & ~64)|128; e['function']=6
        if code=='B07' and i==2:
            e['target']=[4,0];e['roles']|=16;e['function']=6
        if code in ('B08','B13','B14') and e['quality']==3 and distance==5 and e['degree'] != [5,0]:
            e['target']=nxt['degree'];e['roles']|=16;e['function']=6
    # Deliberate functional rhythms, not extra rows for rhythm-only variants.
    durations = [4]*len(events)
    if code in ('A03','A04','A05','A06','B09'):
        for i,e in enumerate(events):
            if e['quality']==7: durations[i]=1; durations[i+1]=3
    if code=='B01': durations=[2,2,2,2,8]
    if code=='B11': durations=[3,1,4,4,4,4,8]
    if code=='B08': durations=[2,2,2,2,2,2,4,8]
    for e,d in zip(events,durations):e['durationQN']=d
    title_zh, title_en = NAMES[code]
    item = dict(id='V4_'+code,name=title_en,nameZh=title_zh,nameEn=title_en,
        aliases=code+','+techniques,builtInTags='common-'+mode+','+intent,techniques=techniques,
        complexityLevel='rich',description=purpose,mode=mode,sequence=' '.join(seeds),
        rhythm=' '.join(map(str,durations)),intent=intent,cadence=cadence,loopable=intent=='loop',
        skeleton=','.join(map(str,range(len(events)))),meter='4/4',styles=','.join(s+':0.8' for s in styles.split(',')),
        tags=techniques,sourceType='factory',priorWeight=.55,complexity=.55,version=4,phraseLength=len(events))
    typed=dict(events=[],key=[],full=events,tags=techniques.split(','),aliases=item['aliases'].split(','),
        builtInTags=item['builtInTags'].split(','),techniques=techniques.split(','))
    item['typedData']=json.dumps(typed,ensure_ascii=False,separators=(',',':'))
    return item

if __name__ == '__main__':
    records = [author(line) for line in PHRASES.splitlines()]
    assert len(records)==28
    dest=ROOT/'data/factory-v4';dest.mkdir(exist_ok=True)
    (dest/'candidates.json').write_text(json.dumps(records,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Authored 28 Phase 1 A/B candidates; V3 untouched.')
