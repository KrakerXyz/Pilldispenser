#!/usr/bin/env bash
set -euo pipefail
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_dir=${1:-"$repo_root/Arduino/PillDispenser"}
build_dir="$repo_root/.local/controller-test"
mkdir -p "$build_dir"
test -f "$source_dir/AppState.h"
test -f "$source_dir/Dispense_Rooutine.ino"
test -f "$source_dir/Restart_Control.ino"
"${CXX:-g++}" -std=c++17 -O1 -g -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$source_dir" "$repo_root/tests/controller_test.cpp" -o "$build_dir/controller_test"
"$build_dir/controller_test"
