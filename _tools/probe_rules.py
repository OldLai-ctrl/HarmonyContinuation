import csv, re, sys
sys.path.insert(0, 'tools/ingest')
import build as B

rows = list(csv.DictReader(open(r'D:\VibeCoding\_ingest_bb_v6\phrases.tsv', encoding='utf-8'), delimiter='\t'))
print("rows", len(rows))

def roman_root(tok):
    m = re.match(r'^(b|#)?(VII|VI|V|IV|III|II|I)', tok)
    if not m: return None
    alt = {'b':-1,'#':1}.get(m.group(1), 0)
    return {'I':1,'II':2,'III':3,'IV':4,'V':5,'VI':6,'VII':7}[m.group(2)], alt

stat = []
for r in rows:
    toks = r['name_zh'].split()
    try:
        durs = [d for _, d in B.symbols(r['bars'], r['meter'])]
    except Exception:
        durs = [1.0] * len(toks)
    n = min(len(toks), len(durs))
    toks2, durs2 = toks[:n], durs[:n]
    roots = [roman_root(t) for t in toks2]
    if not all(roots): continue
    total = sum(durs2) or 1.0
    td = sum(d for rt, d in zip(roots, durs2) if rt in ((1,0),(5,0)))
    stat.append((len(toks2), len(set(roots)), td/total, r['id'], r['name_zh'], r['bars']))

print("analysed", len(stat))
print("distinct roots <= 2:", sum(1 for n,d,*_ in stat if d <= 2))
print("distinct roots <= 3:", sum(1 for n,d,*_ in stat if d <= 3))
print("chords <= 4:", sum(1 for n,d,*_ in stat if n <= 4))
for thr in (0.5, 0.66, 0.75, 0.85):
    print(f"I+V duration share > {thr}: {sum(1 for n,d,s,*_ in stat if s > thr)}")
print()
print("--- <=2 distinct roots ---")
for n,d,s,_id,name,bars in stat:
    if d <= 2:
        print(f"{_id:22s} {n:2d}ch {s:4.2f} {name[:50]:50s} | {bars[:64]}")
