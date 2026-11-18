import csv, sys
sys.path.insert(0, 'tools/ingest')
import build as B
import boundaries as BD
import chords as C

rows = list(csv.DictReader(open(r'D:\VibeCoding\_ingest_bb_v6\phrases.tsv', encoding='utf-8'), delimiter='\t'))

def share_tonic_dom(bars, mode, meter):
    tonic = 9 if mode == 'minor' else 0
    dom = (tonic + 7) % 12
    total = hit = 0.0
    for sym, dur in B.symbols(bars, meter):
        try:
            root = C.parse_symbol(sym)[0]
        except Exception:
            continue
        total += dur
        if root in (tonic, dom):
            hit += dur
    return hit / total if total else 0.0

vamp = merged = chains = 0
vamp_any = 0
example = []
for r in rows:
    s = share_tonic_dom(r['bars'], r['mode'], r['meter'])
    if s > 0.66:
        vamp += 1
        if len(example) < 5: example.append((r['id'], round(s,2), r['name_zh'][:40], r['bars'][:50]))
    cells = [c for c in r['bars'].split('|') if c.strip()]
    i = 0; run = False
    while i < len(cells):
        j = i
        while j + 1 < len(cells) and len(cells[j].split()) == 1 and len(cells[j+1].split()) == 1 \
              and BD.cell_key(cells[j]) == BD.cell_key(cells[j+1]):
            j += 1
        if j > i: run = True
        i = j + 1
    if run: merged += 1
    if len(cells) >= 8 and len(cells) % 2 == 0 and not BD.period_of(cells):
        blocks = [tuple(BD.cell_key(c) for c in cells[k:k+2]) for k in range(0, len(cells), 2)]
        if len(set(blocks)) < len(blocks): chains += 1

n = len(rows)
print("rows", n)
print(f"vamp (I/V duration share > 0.66): {vamp} = {vamp/n*100:.1f}%")
print(f"rows with bar-level single-chord run: {merged} = {merged/n*100:.1f}%")
print(f"2-bar chain candidates (>=8 bars, non-periodic): {chains} = {chains/n*100:.1f}%")
print()
for e in example: print("  ", e)
