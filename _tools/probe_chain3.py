import sys
sys.path.insert(0, 'tools/ingest')
import build as B
from pathlib import Path
from collections import Counter

songs = B.load_billboard(Path('D:/VibeCoding/_hc_evidence/sources'))
stat = Counter()
for s in songs:
    span = B.BAR_QN[s['meter']]
    for lab, bars in s['sections']:
        ev = B.section_events(bars, s['meter'])
        if len(ev) < B.MIN_CHORDS:
            continue
        pieces = B.split_at_rests(ev, s['key'], s['mode'], span)
        for piece, rendered in pieces:
            rendered = B.normalize_rhythm(rendered)
            cells = [c for c in rendered.split('|') if c.strip()]
            ch = B.chain_cells("|".join(cells))
            if not ch:
                continue
            stat['chain_pieces'] += 1
            for start in range(0, len(ch), 2):
                half = "|".join(ch[start:start+2])
                n = sum(1 for _ in B.symbols(half, s['meter']))
                stat['halves'] += 1
                if n < B.MIN_CHORDS:
                    stat['half_short'] += 1
                if n >= 8:
                    stat['half_dense'] += 1
                stat[f'half_chords_{min(n,10)}'] += 1
print("chain pieces:", stat['chain_pieces'], "halves:", stat['halves'])
print("halves under 4 chords:", stat['half_short'], f"{stat['half_short']/stat['halves']*100:.1f}%")
print("halves with 8+ chords:", stat['half_dense'])
print({k: v for k, v in sorted(stat.items()) if k.startswith('half_chords_')})
