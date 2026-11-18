import csv, re, sys, statistics
sys.path.insert(0, 'tools/ingest')
import build as B

rows = list(csv.DictReader(open(r'D:\VibeCoding\_ingest_bb_v6\phrases.tsv', encoding='utf-8'), delimiter='\t'))

def roots_of(name):
    out = []
    for t in name.split():
        m = re.match(r'^(b|#)?(VII|VI|V|IV|III|II|I)', t)
        if m:
            out.append(({'I':1,'II':2,'III':3,'IV':4,'V':5,'VI':6,'VII':7}[m.group(2)],
                        {'b':-1,'#':1}.get(m.group(1), 0)))
    return out

def tonic_dom(rt, mode):
    return rt in ((1,0),(5,0))

n_roots = 0; n_iv = 0; both = 0; keep = 0; total = 0
merged_rows = 0; merged_events = 0
for r in rows:
    rt = roots_of(r['name_zh'])
    if not rt:
        continue
    total += 1
    try:
        durs = [d for _, d in B.symbols(r['bars'], r['meter'])]
    except Exception:
        continue
    n = min(len(rt), len(durs))
    rt2, durs2 = rt[:n], durs[:n]
    share = sum(d for a, d in zip(rt2, durs2) if tonic_dom(a, r['mode'])) / (sum(durs2) or 1)
    r2 = len(set(rt2)) <= 2
    r3 = share > 0.66
    if r2: n_roots += 1
    if r3: n_iv += 1
    if r2 and r3: both += 1
    if r2 or r3: keep += 1
    # merging: adjacent identical roman labels
    labs = r['name_zh'].split()
    m = sum(1 for i in range(1, len(labs)) if labs[i] == labs[i-1])
    if m: merged_rows += 1; merged_events += m

print("rows with labels:", total)
print("rule A (<=2 distinct roots):", n_roots)
print("rule B (I/V duration share >0.66):", n_iv)
print("both:", both, " either(removed):", keep)
print()
print("rows containing adjacent identical labels:", merged_rows, "/", total, "=", round(merged_rows/total*100,1), "%")
print("total adjacent-identical pairs:", merged_events)
