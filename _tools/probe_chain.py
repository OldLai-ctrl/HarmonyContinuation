import csv, sys
sys.path.insert(0, 'tools/ingest')
import build as B
import boundaries as BD

rows = list(csv.DictReader(open(r'D:\VibeCoding\_ingest_bb_v6\phrases.tsv', encoding='utf-8'), delimiter='\t'))
tot_cells = short_cells = 0
sections = 0
ok_all = 0
for r in rows:
    ch = B.chain_cells(r['bars'])
    if not ch:
        continue
    sections += 1
    bad = 0
    for i in range(0, len(ch), 2):
        unit = "|".join(ch[i:i+2])
        n = sum(1 for _ in B.symbols(unit, r['meter']))
        tot_cells += 1
        if n < 4:
            short_cells += 1
            bad += 1
    if bad == 0:
        ok_all += 1
print(f"chain sections={sections} cells={tot_cells} cells_under_4_chords={short_cells} "
      f"({short_cells/tot_cells*100:.1f}%) sections_where_all_cells_survive={ok_all}")
