#!/usr/bin/env bash
set -euo pipefail
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_dir=${1:-"$repo_root/Arduino/PillDispenser"}
build_dir="$repo_root/.local/day-status-test"
mkdir -p "$build_dir"
# Isolate the actual production function, not a rewritten testing copy. The rest
# of Check_Alerts.ino depends on the physical display and scheduled LVGL events.
python3 - "$source_dir/Check_Alerts.ino" "$build_dir/DayStatusActual.h" <<'PY'
from pathlib import Path
import hashlib, json, sys
source = Path(sys.argv[1]).resolve()
text = source.read_text()
marker = '\nvoid reconcileCurrentDay('
assert text.startswith('bool reconcileDayStatus(') and marker in text, 'Production function boundary changed; update test extraction'
function = text.split(marker, 1)[0]
assert function.count('{') == function.count('}'), 'Unbalanced extracted function'
Path(sys.argv[2]).write_text('#line 1 ' + json.dumps(str(source)) + '\n' + function)
print('Actual reconciliation source SHA256:', hashlib.sha256(function.encode()).hexdigest())
PY
"${CXX:-g++}" -std=c++17 -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$build_dir" "$repo_root/tests/day_status_test.cpp" -o "$build_dir/day_status_test"
# Each case uses a fresh process so the production static cached day starts as
# it would after boot. Repeated calls within each case exercise the cache.
cases=(migration manual-reset invalid-clock repeat same-day-boot reset-boundary \
       late-boot year-boundary leap-day busy-reset busy-migration reboot-pending queued-command)
for case_name in "${cases[@]}"; do "$build_dir/day_status_test" "$case_name"; done
printf '%s day-status tests passed (actual source; no hardware)\n' "${#cases[@]}"
