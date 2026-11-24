#!/bin/bash
# Build, save the actual HW2 assets, then run both homework gameplay tests.
set -euo pipefail
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
PROJECT_FILE="$PROJECT_DIR/MechanicsHomework.uproject"
mkdir -p "$PROJECT_DIR/Saved/HW2"
exec > >(tee "$PROJECT_DIR/Saved/HW2/Prepare.log") 2>&1
trap 'result=$?; if [ "$result" -ne 0 ]; then echo "FAILED ($result). See Saved/HW2/Prepare.log"; fi; echo "Press Enter to close."; read -r _' EXIT

bash "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" \
  MechanicsHomeworkEditor Mac Development -Architecture=arm64 \
  -Project="$PROJECT_FILE" -MaxParallelActions=4

"$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJECT_FILE" \
  -run=HomeworkAISetup -unattended -nop4 -nosound -nullrhi \
  -abslog="$PROJECT_DIR/Saved/HW2/Setup.log" -stdout -FullStdOutLogOutput

for asset in BB_Guard.uasset BT_Guard.uasset PCG_Courtyard.uasset Lvl_PCGPatrol.umap; do
  test -s "$PROJECT_DIR/Content/Homework2/$asset"
done

"$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJECT_FILE" \
  -unattended -nop4 -nosound -nullrhi -nosplash \
  -ExecCmds="Automation RunTests Homework." \
  -TestExit="Automation Test Queue Empty" \
  -ReportExportPath="$PROJECT_DIR/Saved/HW2/Automation" \
  -abslog="$PROJECT_DIR/Saved/HW2/Tests.log" -stdout -FullStdOutLogOutput

python3 - "$PROJECT_DIR/Saved/HW2/Automation/index.json" <<'PY'
import json, sys
from pathlib import Path
path = Path(sys.argv[1])
if not path.is_file():
    raise SystemExit("FAILED: automation report was not created")
data = json.loads(path.read_text())
tests = data.get("tests", [])
expected = {"Homework.Gameplay.EndToEnd", "Homework.AI.PatrolChaseNoise"}
seen = {t.get("fullTestPath"): t.get("state") for t in tests}
for name in sorted(expected):
    print(name, seen.get(name, "MISSING"))
if any(seen.get(name) != "Success" for name in expected):
    raise SystemExit("FAILED: inspect the automation report")
print("SUCCESS: HW2 assets saved and gameplay tests passed.")
PY
