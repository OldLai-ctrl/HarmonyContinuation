import sys, csv, collections
sys.path.insert(0, 'tools/ingest')
import build as B

old = list(csv.DictReader(open(r'D:\VibeCoding\_ingest_bb_v6\phrases.tsv', encoding='utf-8'), delimiter='\t'))
kept = {r['id'] for r in csv.DictReader(open(r'D:\VibeCoding\_ingest_bb_v7\phrases.tsv', encoding='utf-8'), delimiter='\t')}
drop = [r for r in old if r['id'] not in kept]
vamped = [r for r in old if B.tonic_dominant_share(r['bars'], r['mode'], r['meter']) > B.VAMP_SHARE]
print(f"v6 rows={len(old)} v7 rows={len(kept)} removed={len(drop)}")
print(f"v6 rows that are tonic/dominant vamps={len(vamped)} ({len(vamped)/len(old)*100:.1f}%)")
print()
print("sample of removed vamp rows:")
for r in vamped[:10]:
    print(f"  {r['id']:20s} {B.tonic_dominant_share(r['bars'], r['mode'], r['meter']):.2f} "
          f"{r['name_zh'][:38]:38s} | {r['bars'][:48]}")
print()
shapes = collections.Counter(r['name_zh'] for r in vamped)
print("most common vamp labels:")
for label, n in shapes.most_common(12):
    print(f"  {n:4d}  {label[:60]}")
