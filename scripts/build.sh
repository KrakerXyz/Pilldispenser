#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
export ARDUINO_DIRECTORIES_DATA="${ARDUINO_DIRECTORIES_DATA:-$HOME/.arduino15}"
export PATH="$ROOT/.local/python/bin:$PATH"
CORE_VERSION=2.0.17
CORE="$ARDUINO_DIRECTORIES_DATA/packages/esp32/hardware/esp32/$CORE_VERSION"
FQBN='esp32:esp32:esp32:FlashMode=dio,FlashFreq=80,FlashSize=4M,PartitionScheme=min_spiffs,PSRAM=disabled'
LABEL="${PILL_BUILD_LABEL:-v.1.9-grid}"
[[ "$LABEL" =~ ^[A-Za-z0-9._-]+$ ]] || { echo 'Invalid PILL_BUILD_LABEL' >&2; exit 1; }
test -f "$CORE/platform.txt" || { echo 'Run ./scripts/setup.sh first.' >&2; exit 1; }
test -x "$ROOT/.local/python/bin/python" || { echo 'Run ./scripts/setup.sh first.' >&2; exit 1; }
python3 -c 'import serial; assert serial.__version__ == "3.5"'
ACTIVE_CORE="$(arduino-cli board details --fqbn "$FQBN" --json | python3 -c 'import json,sys; print(json.load(sys.stdin)["version"])')"
[[ "$ACTIVE_CORE" == "$CORE_VERSION" ]] || { echo 'Active ESP32 core differs from the project pin; rerun setup.' >&2; exit 1; }
mkdir -p "$ROOT/.build" "$ROOT/build"
rm -f -- "$ROOT/build/manifest.json"
STAGE="$(mktemp -d "$ROOT/.build/source-XXXXXX")"
python3 - "$ROOT" "$STAGE" "$LABEL" <<'PY'
from pathlib import Path
import re, shutil, sys
root, stage, label = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
shutil.copytree(root / 'Arduino/PillDispenser', stage / 'PillDispenser')
p = stage / 'PillDispenser/PillDispenser.ino'
source, count = re.subn(r'String versionnumber = "[^"]+";', f'String versionnumber = "{label}";', p.read_text())
assert count == 1, 'Upstream firmware version declaration changed'
p.write_text(source)
PY
arduino-cli compile --fqbn "$FQBN" \
    --libraries "$ROOT/Arduino/libraries" \
    --build-path "$ROOT/.build/compile" --output-dir "$ROOT/build" \
    --jobs "${JOBS:-2}" "$STAGE/PillDispenser"
"$ARDUINO_DIRECTORIES_DATA/packages/esp32/tools/mkspiffs/0.2.3/mkspiffs" \
    -c "$ROOT/Arduino/PillDispenser/data" -b 4096 -p 256 -s 131072 \
    "$ROOT/build/PillDispenser.spiffs.bin"
python3 - "$ROOT" "$LABEL" "$FQBN" <<'PY'
from pathlib import Path
import hashlib, json, subprocess, sys
root, label, fqbn = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
inputs = hashlib.sha256()
for p in sorted((root/'Arduino').rglob('*')):
    if p.is_file():
        inputs.update(str(p.relative_to(root)).encode() + b'\0' + p.read_bytes() + b'\0')
manifest = {'source_commit': subprocess.check_output(['git','rev-parse','HEAD'], cwd=root, text=True).strip(),
            'source_dirty': bool(subprocess.check_output(['git','status','--porcelain','--','Arduino'], cwd=root, text=True).strip()),
            'arduino_inputs_sha256': inputs.hexdigest(),
            'arduino_cli': subprocess.check_output(['arduino-cli','version'], text=True).strip(),
            'firmware_version': label,
            'removed_settings': ['alertinterval', 'spkvolume', 'mutebutton',
                                 'bottoken', 'chatID', 'telegramalert'],
            'esp32_core': json.loads(subprocess.check_output(['arduino-cli','board','details','--fqbn',fqbn,'--json'], text=True))['version'],
            'fqbn': fqbn, 'artifacts': {}}
for name, limit in [('PillDispenser.ino.bin',1966080), ('PillDispenser.spiffs.bin',131072)]:
    data = (root/'build'/name).read_bytes()
    assert 0 < len(data) <= limit, f'{name} does not fit its partition'
    manifest['artifacts'][name] = {'bytes':len(data), 'sha256':hashlib.sha256(data).hexdigest(), 'md5':hashlib.md5(data).hexdigest()}
(root/'build/manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
print(json.dumps(manifest, indent=2))
PY
