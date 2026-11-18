"""Compile ingested candidates into a Factory library, dropping duplicate performances.

The compiler parses at most 4096 templates per candidate file (`tools/TemplateJson.cpp:222`),
which is a parse-time cap rather than a library cap: compiled libraries with ~5k templates
load fine.  So candidates are compiled in batches, each batch starting from the previous
batch's output.
"""

import json
import subprocess
import sys
from pathlib import Path

COMPILER = Path(r"D:\VibeCoding\HarmonyContinuation\build\Release\factory_v6_compiler.exe")
BASE = Path(sys.argv[1])          # baseline db, e.g. build/factory.db
CANDIDATES = Path(sys.argv[2])    # candidates.json
OUTPUT = Path(sys.argv[3])        # new db path (must not exist)
DROPPED = Path(sys.argv[4]) if len(sys.argv) > 4 else Path("dropped.txt")
BATCH = 16384

candidates = json.loads(CANDIDATES.read_text(encoding="utf-8"))
dropped = []
base = BASE
batches = [candidates[i:i + BATCH] for i in range(0, len(candidates), BATCH)]
print(f"candidates={len(candidates)} batches={[len(b) for b in batches]}")

for number, batch in enumerate(batches):
    last = number == len(batches) - 1
    target = OUTPUT if last else OUTPUT.with_suffix(f".{number}.db")
    for attempt in range(600):
        temp = CANDIDATES.with_suffix(f".{number}.{attempt}.json")
        temp.write_text(json.dumps(batch, ensure_ascii=False, separators=(",", ":")),
                        encoding="utf-8")
        if target.exists():
            target.unlink()
        result = subprocess.run([str(COMPILER), "--v9-a-incremental", str(base), str(temp), str(target)],
                                capture_output=True, text=True, encoding="utf-8", errors="replace")
        temp.unlink()
        out = (result.stdout or "") + (result.stderr or "")
        if "PASS" in out:
            print(f"batch {number}: {out.strip().splitlines()[-1]}")
            break
        marker = "duplicate V9 ID or performed music: "
        if marker in out:
            victim = out.split(marker, 1)[1].split()[0].strip()
            dropped.append(victim)
            before = len(batch)
            batch = [item for item in batch if item["id"] != victim]
            print(f"drop {victim} ({before} -> {len(batch)})")
            continue
        print("FAILED")
        print(out[-2000:])
        sys.exit(1)
    else:
        print("gave up")
        sys.exit(1)
    base = target

DROPPED.write_text("\n".join(dropped), encoding="utf-8")
print(f"dropped={len(dropped)} size={OUTPUT.stat().st_size}")
