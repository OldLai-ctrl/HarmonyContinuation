"""Drop entries the compiler reports as duplicate performances until it compiles."""
import json
import re
import subprocess
import sys
from pathlib import Path

root = Path(r"D:\VibeCoding\HarmonyContinuation")
candidates_path = Path(r"D:\VibeCoding\_ingest_rs200\candidates.json")
out = Path(r"D:\VibeCoding\factory-rs200.db")
exe = root / "build/Release/factory_v6_compiler.exe"

rows = json.loads(candidates_path.read_text(encoding="utf-8"))
dropped = []
for attempt in range(400):
    if out.exists():
        out.unlink()
    proc = subprocess.run(
        [str(exe), "--v9-a-incremental", str(root / "build/factory.db"),
         str(candidates_path), str(out)],
        capture_output=True, text=True)
    text = (proc.stdout or "") + (proc.stderr or "")
    if "PASS" in text:
        print(text.strip().splitlines()[-1])
        break
    match = re.search(r"duplicate V9 ID or performed music: (\S+)", text)
    if not match:
        print("UNEXPECTED:", text.strip()[-500:])
        sys.exit(1)
    bad = match.group(1)
    dropped.append(bad)
    rows = [row for row in rows if row["id"] != bad]
    candidates_path.write_text(json.dumps(rows, ensure_ascii=False), encoding="utf-8")
else:
    print("gave up")
    sys.exit(1)

print(f"kept={len(rows)} dropped={len(dropped)}")
Path(r"D:\VibeCoding\_tools\dropped_rs200_faithful.txt").write_text(
    "\n".join(dropped), encoding="utf-8")
print("size", out.stat().st_size if out.exists() else 0)
