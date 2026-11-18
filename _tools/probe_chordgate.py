import sys, pathlib, collections
sys.path.insert(0,'tools/ingest')
import build as B, boundaries as BD

songs = B.load_billboard(pathlib.Path('D:/VibeCoding/_hc_evidence/sources'))

# 1) how much musical content sits in one bar? (chord changes per bar)
per_bar = collections.Counter()
for s in songs:
    for bs in [b for _, b in s['sections']]:
        for b in bs:
            if b: per_bar[len(b)] += 1
tot = sum(per_bar.values())
print("bars by chord changes:", dict(sorted(per_bar.items())))
print(f"bars with >=4 chord changes: {sum(v for k,v in per_bar.items() if k>=4)} = {sum(v for k,v in per_bar.items() if k>=4)/tot*100:.1f}%")

# 2) 2-bar blocks by chord changes (the chain cell)
blk = collections.Counter()
for s in songs:
    for bs in [b for _, b in s['sections']]:
        cells = [b for b in bs if b]
        for i in range(0, len(cells)-1, 2):
            blk[len(cells[i])+len(cells[i+1])] += 1
tb = sum(blk.values())
print("\n2-bar blocks by chord changes:", dict(sorted(blk.items())))
print(f"  >=4: {sum(v for k,v in blk.items() if k>=4)} = {sum(v for k,v in blk.items() if k>=4)/tb*100:.1f}%")
print(f"  >=3: {sum(v for k,v in blk.items() if k>=3)} = {sum(v for k,v in blk.items() if k>=3)/tb*100:.1f}%")

# 3) sounding duration of bars: QN per bar by meter
q = collections.Counter()
for s in songs:
    q[B.BAR_QN[s['meter']]] += 1
print("\nsongs by bar span (QN):", dict(sorted(q.items())))
