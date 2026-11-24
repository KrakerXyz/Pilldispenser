#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
export ARDUINO_DIRECTORIES_DATA="${ARDUINO_DIRECTORIES_DATA:-$HOME/.arduino15}"
CORE_VERSION=2.0.17
INDEX_URL=https://espressif.github.io/arduino-esp32/package_esp32_index.json
command -v arduino-cli >/dev/null || { echo 'Install Arduino CLI in the dev-env image first.' >&2; exit 1; }
arduino-cli version
mkdir -p "$ROOT/.local"
if [[ ! -x "$ROOT/.local/python/bin/python" ]]; then
    /usr/bin/python3 -m venv "$ROOT/.local/python"
fi
if ! "$ROOT/.local/python/bin/python" -c 'import serial; assert serial.__version__ == "3.5"' 2>/dev/null; then
    "$ROOT/.local/python/bin/python" -m pip install --disable-pip-version-check 'pyserial==3.5'
fi
ACTIVE_CORE="$(arduino-cli board details --fqbn esp32:esp32:esp32 --json 2>/dev/null | "$ROOT/.local/python/bin/python" -c 'import json,sys; print(json.load(sys.stdin).get("version", ""))' 2>/dev/null || true)"
if [[ "$ACTIVE_CORE" != "$CORE_VERSION" ]]; then
    arduino-cli core update-index --additional-urls "$INDEX_URL"
    arduino-cli core install "esp32:esp32@$CORE_VERSION" --additional-urls "$INDEX_URL"
fi
test -f "$ARDUINO_DIRECTORIES_DATA/packages/esp32/hardware/esp32/$CORE_VERSION/platform.txt"
arduino-cli board details --fqbn esp32:esp32:esp32 --json | "$ROOT/.local/python/bin/python" -c 'import json,sys; assert json.load(sys.stdin)["version"] == "2.0.17", "Unexpected active ESP32 core"'
test -x "$ARDUINO_DIRECTORIES_DATA/packages/esp32/tools/mkspiffs/0.2.3/mkspiffs"
arduino-cli core list
printf '\nSetup complete. Build with ./scripts/build.sh\n'
