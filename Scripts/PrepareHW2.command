#!/bin/bash
# Build and test the checked-in maps on macOS.
set -euo pipefail
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
PROJECT_FILE="$PROJECT_DIR/MechanicsHomework.uproject"
mkdir -p "$PROJECT_DIR/Saved/HW2"
exec > >(tee "$PROJECT_DIR/Saved/HW2/Prepare.log") 2>&1
trap 'if [[ -t 0 ]]; then read -r -p "Press Enter to close." _ || true; fi' EXIT

bash "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" \
  MechanicsHomeworkEditor Mac Development -Architecture=arm64 \
  -Project="$PROJECT_FILE" -MaxParallelActions=4

assets=(BB_Guard.uasset BT_Guard.uasset PCG_Courtyard.uasset Lvl_PCGPatrol.umap)
for asset in "${assets[@]}"; do
  if [[ ! -s "$PROJECT_DIR/Content/Homework2/$asset" ]]; then
    "$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJECT_FILE" \
      -run=HomeworkAISetup -unattended -nop4 -nosound -nullrhi \
      -abslog="$PROJECT_DIR/Saved/HW2/Setup.log" -stdout -FullStdOutLogOutput
    break
  fi
done
for asset in "${assets[@]}"; do
  test -s "$PROJECT_DIR/Content/Homework2/$asset"
done

REPORT_DIR="$(mktemp -d "$PROJECT_DIR/Saved/HW2/Automation.XXXXXX")"
"$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJECT_FILE" \
  -unattended -nop4 -nosound -nullrhi -nosplash \
  -ExecCmds="Automation RunTests Homework." \
  -TestExit="Automation Test Queue Empty" \
  -ReportExportPath="$REPORT_DIR" \
  -abslog="$PROJECT_DIR/Saved/HW2/Tests.log" -stdout -FullStdOutLogOutput

python3 - "$REPORT_DIR/index.json" <<'PY'
import json, sys
from pathlib import Path
path = Path(sys.argv[1])
if not path.is_file():
    raise SystemExit("FAILED: automation report was not created")
data = json.loads(path.read_text(encoding="utf-8-sig"))
tests = data.get("tests", [])
expected = {"Homework.Gameplay.EndToEnd", "Homework.AI.PatrolChaseNoise"}
seen = {t.get("fullTestPath"): t.get("state") for t in tests}
for name in sorted(expected):
    print(name, seen.get(name, "MISSING"))
if any(seen.get(name) != "Success" for name in expected):
    raise SystemExit("FAILED: inspect the automation report")
print("SUCCESS: both gameplay tests passed.")
PY
