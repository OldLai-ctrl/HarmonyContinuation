"""Capture the eight development recommendation cases from a built CLI."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: py tools/build_phase3_case_report.py build-core/recommend_cli.exe")
cli = Path(sys.argv[1]).resolve()
if not cli.exists():
    raise SystemExit(f"missing CLI: {cli}")
titles = {
    "a": "I → vi → ii (C, Am, Dm OPEN)",
    "b": "I → vi → V/ii → ii (C, Am, A7, Dm OPEN)",
    "c": "ii → V (Dm7, G7 OPEN)",
    "d": "I → IV → iv (C, F, Fm OPEN)",
    "e": "I → V → vi → IV (C, G, Am, F OPEN)",
    "f": "A minor (Am, F, Dm, E7 OPEN)",
    "g": "Unexplained chord (C, Am, F#, Dm OPEN)",
    "h": "Relative key ambiguity (C, Am, F OPEN)",
}
out = ["# Phase 3 recommendation case capture", "",
       "Captured from the standalone CLI and the packaged factory database. Scores are ordinal, not probabilities. Cubase host behavior is not covered by these cases.", ""]
for letter, title in titles.items():
    args = [str(cli), str(ROOT / "tests" / "fixtures" / "recommendations" / f"case_{letter}.json")]
    args.extend(["--user", str(cli.parent / "phase3-case-empty-user.db")])
    if letter != "h":
        args.extend(["--key", "A:minor" if letter == "f" else "C:major"])
    result = subprocess.run(args, check=True, capture_output=True, text=True, encoding="utf-8")
    captured = "\n".join(line.rstrip() for line in result.stdout.rstrip().splitlines())
    out.extend([f"## Case {letter.upper()}: {title}", "", "```text", captured, "```", ""])
out.extend([
    "## Manual music review flags", "",
    "- Case C's Develop group can end on a tonic even though the template is tagged Develop; this classification may be too broad.",
    "- Case D's Resolve and Loop alternatives have noticeably weaker match evidence than the direct iv → I Color path. They are retained above the current threshold and should be judged in Cubase.",
    "- Case G still offers a plausible G → C after the unexplained F# chord. The explicit insertion penalty lowers the score by about ten points versus Case B, but the output may sound unmotivated.",
    "- Case F has only a Resolve path above threshold. The engine deliberately leaves the other groups empty instead of filling them with weak candidates.",
    "- Some jazz and City Pop catalogue entries use uniform four beat durations; a human rhythm pass is needed before treating those grooves as polished examples.",
    "",
])
path = ROOT / "docs" / "PHASE3_CASES.md"
path.write_text("\n".join(out), encoding="utf-8")
print(path)
