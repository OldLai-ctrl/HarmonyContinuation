import sys, pathlib, collections, importlib
sys.path.insert(0,'tools/ingest')
import build as B, boundaries as BD

songs = B.load_billboard(pathlib.Path('D:/VibeCoding/_hc_evidence/sources'))

def run(split_low, min_unit_bars, min_chords):
    B.SPLIT_LOW = split_low
    B.MIN_CHORDS = min_chords
    BD.MIN_UNIT_BARS = min_unit_bars
    seen=set(); units=0; bars=collections.Counter(); secs=sect_units=0
    for s in songs:
        span = B.BAR_QN[s['meter']]
        for i,(lab,bs) in enumerate(s['sections']):
            ev = B.section_events(bs, s['meter'])
            if len(ev) < min_chords: continue
            if not B.style_affinity(B.features(ev, s['key'], s['mode']), s): continue
            secs += 1
            n = 0
            for piece, rendered in B.split_at_rests(ev, s['key'], s['mode'], span):
                if len(piece) < min_chords: continue
                rendered = B.normalize_rhythm(rendered)
                st = collections.Counter()
                for ub, tag in B.split_units(rendered, s['mode'], s['meter'], st):
                    held = B.merge_held_bars(ub)
                    if B.tonic_dominant_share(held, s['mode'], s['meter']) > B.VAMP_SHARE: continue
                    if sum(1 for _ in B.symbols(held, s['meter'])) < min_chords: continue
                    sig = B.performance_key(held, s['meter'])
                    if sig in seen: continue
                    seen.add(sig); n += 1
                    bars[len([c for c in held.split('|') if c.strip()])] += 1
            units += n
            sect_units += (n > 1)
    return units, bars, sect_units, secs

for low, mub, mc in [(4,4,4),(2,2,4),(2,1,4),(3,2,4),(2,2,3)]:
    u, bh, su, secs = run(low, mub, mc)
    print(f"SPLIT_LOW={low} MIN_UNIT_BARS={mub} MIN_CHORDS={mc}: units={u} sections_with_multi={su}/{secs}")
    print("   bars:", dict(sorted(bh.items())))
