import csv, sys
sys.path.insert(0, 'tools/ingest')
import build as B

rows = list(csv.DictReader(open(r'D:\VibeCoding\_ingest_bb_v7\phrases.tsv', encoding='utf-8'), delimiter='\t'))
n2 = n4 = 0
for r in rows:
    ch = B.chain_cells(r['bars'])
    if not ch:
        continue
    blocks = ["|".join(ch[i:i+2]) for i in range(0, len(ch), 2)]
    if all(sum(1 for _ in B.symbols(b, r['meter'])) >= 4 for b in blocks):
        n2 += 1
    else:
        n4 += 1
print("chain rows:", n2 + n4, "-> 2-bar units viable:", n2, " must fall back to 4-bar:", n4)
