"""Encode manually authored, fixed complete phrases; no music generation."""
import csv, json, pathlib, re, sqlite3, collections
P=pathlib.Path(__file__).parent
qualities={
'':(1,[0,4,7],[]),'m':(2,[0,3,7],[]),'7':(3,[0,4,7,10],[]),'maj7':(4,[0,4,7,11],[]),'m7':(5,[0,3,7,10],[]),
'9':(3,[0,4,7,10],[2]),'13':(3,[0,4,7,10],[2,9]),'maj9':(4,[0,4,7,11],[2]),'m9':(5,[0,3,7,10],[2]),'m11':(5,[0,3,7,10],[2,5]),
'6':(1,[0,4,7],[9]),'m6':(2,[0,3,7],[9]),'add9':(1,[0,4,7],[2]),'dim':(6,[0,3,6],[]),'dim7':(7,[0,3,6,9],[]),'m7b5':(8,[0,3,6,10],[]),
'7b9':(3,[0,4,7,10],[1]),'7#9b13':(3,[0,4,7,10],[3,8]),'7#9':(3,[0,4,7,10],[3]),'7b13':(3,[0,4,7,10],[8]),'maj7#11':(4,[0,4,7,11],[6]),
'mMaj9':(2,[0,3,7,11],[2]),'mMaj7':(2,[0,3,7,11],[]),'aug':(11,[0,4,8],[]),'7sus4':(10,[0,5,7,10],[]),'sus4':(10,[0,5,7],[]),'sus2':(9,[0,2,7],[]),'5':(1,[0,7],[])}
pcs={'C':0,'D':2,'E':4,'F':5,'G':7,'A':9,'B':11}
major=[(1,0),(2,-1),(2,0),(3,-1),(3,0),(4,0),(4,1),(5,0),(6,-1),(6,0),(7,-1),(7,0)]
minor=[(1,0),(2,-1),(2,0),(3,0),(3,1),(4,0),(4,1),(5,0),(6,0),(6,1),(7,0),(7,1)]
rom=['','I','II','III','IV','V','VI','VII']
def pitch(s): return (pcs[s[0]]+s.count('#')-s.count('b'))%12
def mask(v): return sum(1<<n for n in v)
refs={
'Ramage':('https://academic.oup.com/mts/article/45/2/238/7224663','abstract, archetype and tonal flexibility','family-model'),
'Spicer':('https://mtosmt.org/issues/mto.17.23.2/mto.17.23.2.spicer.html','sections 2-14, 26-27; fragile/emergent/absent distinctions','concept-only'),
'Duinker':('https://www.mtosmt.org/issues/mto.19.25.4/mto.19.25.4.duinker.html','sections 4.7 and 5.3-5.5; hybrid tonic requires melody/metric context','concept-only'),
'LiPR':('https://www.mtosmt.org/issues/mto.26.32.1/mto.26.32.1.li.php','3.1-3.5, Video Examples 2-9','case-mechanism'),
'LiRP':('https://www.mtosmt.org/issues/mto.26.32.1/mto.26.32.1.li.php','4.1-4.4, Video Examples 10-14','case-mechanism'),
'LiReturn':('https://www.mtosmt.org/issues/mto.26.32.1/mto.26.32.1.li.php','5.1-5.5, Video Examples 15-18','case-mechanism'),
'LiDouble':('https://www.mtosmt.org/issues/mto.26.32.1/mto.26.32.1.li.php','6.1-6.7, Video Examples 19-21','case-mechanism')}
refs['Uchiyama']=('https://sakkyoku.info/beginner/popular-chord-progression-pattern/','sections 2, 5-10; author instructional composition examples','composition-application')
refs['Atkinson']=('https://www.mtosmt.org/issues/mto.19.25.2/mto.19.25.2.atkinson.html','sections 17-18,25, Example11; Lydian component, not full flight topic','case-mechanism')
refs['Nazare']=('https://www.tannazare.com/research','The Japanese Sixth; author account of GSIM2023 research','technique-only')
refs['FlaTools']=('https://flatools.com/pages/theory/pops/chords','Marunouchi/Just the Two of Us 4361; author instructional family examples','teaching-family')
items=[];metas=[]
decisions={}
for decisionpath in (P/'review-decisions.json',P/'primary-decisions.json'):
    if decisionpath.exists():
        raw=json.loads(decisionpath.read_text(encoding='utf-8-sig'))
        entries=raw.get('decisions',[]) if isinstance(raw,dict) else raw
        for d in entries: decisions[d['id']]=d
for path in sorted(P.glob('batch[0-9][0-9].psv')):
    batch=path.stem[-2:]
    for n,r in enumerate(csv.DictReader(path.open(encoding='utf-8-sig'),delimiter='|'),1):
        ident=f'V7_{batch}_{n:03d}'
        mode=r['mode']; frame='minor' if mode in ('aeolian','dorian','phrygian','harmonic_minor','melodic_minor_ascending') else 'major'
        degs=minor if frame=='minor' else major
        notes=[];events=[];symbols=[];durations=[]
        for i,raw in enumerate(r['events'].split()):
            symbol,d=raw.rsplit('@',1); d=float(d); assert d>0
            rootq,*bass=symbol.split('/'); m=re.fullmatch(r'([A-G][b#]?)(.*)',rootq); assert m,(ident,symbol)
            root=pitch(m[1]); quality,base,color=qualities[m[2]]; b=pitch(bass[0]) if bass else root
            roles=1|(256 if bass else 0)|(512 if color else 0)
            e=dict(degree=list(degs[root]),target=None,quality=quality,function=1 if root==0 and quality in (1,2,4,5) else 0,roles=roles,weight=.8,durationQN=d,sourceIndex=i,bassInterval=(b-root)%12 if bass else None,intervalMask=mask(base),colorMask=mask(color),suffix=m[2])
            events.append(e);notes.append(dict(symbol=symbol,rootPC=root,bassPC=b,pitchClasses=sorted({(root+k)%12 for k in base+color}|{b}),baseIntervals=base,colorIntervals=color,durationQN=d));symbols.append(symbol);durations.append(d)
        for i,e in enumerate(events[:-1]):
            if e['quality']==3 and (notes[i+1]['rootPC']-notes[i]['rootPC'])%12==5:
                e['function']=4 if notes[i+1]['rootPC']==0 else 6
                if notes[i+1]['rootPC']!=0:e['target']=events[i+1]['degree'];e['roles']|=16
        sequence=[]
        for e in events:
            degree,alt=e['degree']; token=('b' if alt<0 else '#' if alt>0 else '')+rom[degree]
            if e['quality'] in (2,5):token=token.lower()
            token+={3:'7',4:'maj7',5:'7',6:'dim',7:'dim7',8:'m7b5',9:'sus2',10:'sus4',11:'aug'}.get(e['quality'],'')
            sequence.append(token)
        source=r['source']
        if batch=='01' and r['family'] in ('KomuroCross','CanonCross','MaruCross'): source='Uchiyama'
        if batch=='04' and n in (49,50): source='Atkinson';r['case']='Final Fantasy IV Lunar Whale, Atkinson Example11; raised-fourth component only'
        if batch=='02' and mode=='lydian' and source=='Ramage': source='Atkinson';r['case']='Final Fantasy IV Lunar Whale, Atkinson Example11; raised-fourth component only'
        url,locator,support=refs[source]
        boundary=r['boundary']=='yes'; ending=r['ending']; intent=r['intent']
        cadence='authentic' if intent=='resolve' else 'loop_closure' if boundary else 'none'
        techniques=[]
        if any('/' in s for s in symbols):techniques.append('SlashBass')
        if any(e['colorMask'] for e in events):techniques.append('ExtendedColor')
        if any(e['roles']&16 for e in events):techniques.append('SecondaryDominant')
        if source.startswith('Li'):techniques.append('SectionalModulation')
        if source=='Nazare':techniques.append('JapaneseSixth')
        meter=r.get('meter') or ('3/4' if batch=='01' and n==21 else '6/8' if batch=='01' and n in (28,60) else '4/4')
        if r['family']=='MaruCross': r['family']='MinorFifthChain'
        if r['family']=='GameSongBridge': r['family']='SectionalColorBridge'
        name=r['title'];tags=[r['family'],'V7-editorial'];built=[intent,'common-minor' if frame=='minor' else 'common-major']
        item=dict(id=ident,name=name,nameZh=name,nameEn=f'{ident} {name}',aliases=ident,builtInTags=','.join(built),techniques=','.join(techniques),complexityLevel='rich',description=r['basis'],note='Original editorial complete phrase; citations support family or mechanism, not this full performance.',mode=frame,sequence=' '.join(sequence),rhythm=' '.join(format(d,'g') for d in durations),cadence=cadence,intent=intent,loopable=boundary,styles='pop:0.55,rock:0.35',tags=','.join(tags),skeleton=','.join(str(i) for i in range(len(events))),meter=meter,sourceType='factory',priorWeight=.55,complexity=.65,version=7,phraseLength=len(events),secondaryIntents=0)
        item['typedData']=json.dumps(dict(events=[],key=[],full=events,tags=tags,aliases=[ident],builtInTags=built,techniques=techniques),ensure_ascii=False,separators=(',',':'))
        basis=r['basis'];membership=dict(tonicOffset=0,mode=mode,usage='unrated',usageBasis='Original design; repertoire frequency unmeasured.',musicalEvidence=basis,review='reviewed',reviewer='V7 primary editorial review',melodicContext='',typicality='contextual')
        tonic=r.get('tonic') or 'C'; membership['tonicOffset']=pitch(tonic)
        meta=dict(id=ident,musicalIdentity=name,family=r['family'],chords=' '.join(symbols),referenceTonic=tonic,sourceCoordinateMode=frame,meter=meter,durationQN=durations,chordEvents=notes,formUse=[r['section']],memberships=[membership],structure=basis,sourceType='original-editorial-design',source=f'Manually authored complete phrase, batch{batch}.psv line {n+1}. No commercial melody/MIDI/score copied.',support=support+'; complete phrase original, not source-song transcription',sourceReferences=[dict(url=url,locator=r['case'] if r['case'] not in ('family-only','concept-only') else locator,support=support,wholePhraseDirectSupport=False)],applicationCases=[r['case']] if 'VE' in r['case'] or source.startswith('Li') or source=='Atkinson' else [],styleEvidence=[dict(style='J-Pop/ACG application' if source.startswith('Li') or source in ('Ramage','Nazare') else 'game harmonic-component application' if source=='Atkinson' else 'pop/rock compositional application',level=support,basis='Source supports stated mechanism or vocabulary; exact phrase editorial. No genre frequency claim.')],ending=ending,boundary=dict(loop=boundary,basis=basis if boundary else 'Designed sectional phrase; no automatic wraparound.'),tonalInterpretation=f'{tonic} source-centre contextual reading; local tonicizations/modulations as described in structure. Not a claim of one unchanging scale.',typicality='contextual',grade='A',status='editorial-reviewed',adoptionStatus='approved',auditionStatus='pending-human',reviewRecord=[dict(reviewer='primary-agent',decision='hand-authored structural review; independent identity audit pending')],uncertainties='No melody, groove, lyric or orchestration supplied; hybrid tonic and stylistic typicality remain context dependent. Source does not directly support entire original route.')
        songs={2:'Emiri Kato: Terminal Terminal (2017)',3:'Hatsune Miku/ryo: ODDS&ENDS (2012)',4:'TRUE: Sincerely (2018)',5:'ICHIKO: I Say Yes (2007)',6:'Folder5: Believe (2000)',7:'Konomi Suzuki: This game (2014)',8:'AKINO: Extra Magic Hour (2014)',9:'ClariS: Connect (2011)',10:'Masatoshi Ono: departure! (2011)',11:'Kalafina: to the beginning (2012)',12:'CHEMISTRY: Period (2010)',13:'Nana Mizuki: No Limit (2015)',14:'Nana Mizuki: Fearless Hero (2012)',15:'ICHIKO: I Say Yes, postchorus (2007)',16:'Nana Mizuki: Fearless Hero, postchorus (2012)',19:'Yuki Kajiura: Swordland (2012)',20:'Natsumi Kiyoura: Tabi no Tochu (2008)',21:'Hiroshi Kitadani: We Are! (1999)'}
        ve=re.search(r'VE\s*(\d+)',r['case'])
        if ve and int(ve[1]) in songs:
            meta['applicationCases']=[dict(work=songs[int(ve[1])],section=r['section'],analysisLocator='Joy Li 2026 Video Example '+ve[1],relationship='analysed mechanism only; this original complete route is not the song excerpt')]
        if source=='Atkinson':
            meta['applicationCases']=[dict(work='Nobuo Uematsu: Lunar Whale, Final Fantasy IV (1991)',section='flight cue; raised-fourth component',analysisLocator='Atkinson 2019 section25, Example11',relationship='Lydian component only; melody, orchestration and complete flight topic are not recreated')]
        modepitches={'ionian':[0,2,4,5,7,9,11],'aeolian':[0,2,3,5,7,8,10],'dorian':[0,2,3,5,7,9,10],'phrygian':[0,1,3,5,7,8,10],'lydian':[0,2,4,6,7,9,11],'mixolydian':[0,2,4,5,7,9,10],'harmonic_minor':[0,2,3,5,7,8,11],'melodic_minor_ascending':[0,2,3,5,7,9,11]}
        allowed={(pitch(tonic)+k)%12 for k in modepitches.get(mode,[])}; observed={k for e in notes for k in e['pitchClasses']}
        meta['scaleCompatibility']=dict(outsidePitchClasses=sorted(observed-allowed),strictStaticCompatible=not bool(observed-allowed),scope='Pitch-set check only; does not establish tonic or mode membership. Contextual borrowing/modulation described separately in structure.')
        meta['identityDefinition']='chordEvents + meter + durationQN; fixed C coordinate pitch classes, referenceTonic names contextual centre. Existing renderer chooses voicing octaves.'
        if ident not in decisions:
            decisions[ident]=dict(id=ident,grade='B',reason='No explicit primary editorial admission decision; quarantined.')
        if ident in decisions:
            d=decisions[ident]; meta['grade']=d['grade'];meta['adoptionStatus']='approved' if d['grade']=='A' else 'pending';meta['status']='editorial-reviewed' if d['grade']=='A' else 'pending'
            meta['reviewRecord']=[dict(reviewer='primary-agent',decision=d['reason'])]
            meta['knownRelationships']=d.get('relations',[])
            if d['grade']!='A':membership['review']='pending';meta['uncertainties']+=' '+d['reason']
        if mode=='undetermined':
            meta['memberships']=[];meta['scaleCompatibility']=dict(strictStaticCompatible=None,scope='Tonic C riff but third absent; no reviewed modal membership inferred.')
        items.append(item);metas.append(meta)
(P/'candidates.json').write_text(json.dumps(items,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(P/'metadata.json').write_text(json.dumps(dict(version=7,baseline='24bdc239',candidates=metas),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(P/'approved-ids.txt').write_text('\n'.join(x['id'] for x in metas if x['grade']=='A')+'\n',encoding='utf-8')
print('Encoded explicit phrases',len(items),'families',dict(collections.Counter(x['family'] for x in metas)))
