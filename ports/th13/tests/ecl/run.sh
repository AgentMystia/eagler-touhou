#!/usr/bin/env bash
# ECL 虚拟机单测构建与运行（容器内 g++，无外部依赖）。
# 自动发现 tests/ecl/ecl_*_test.cpp 套件；用 --debug 切 -O0 -g。
# 用法：tests/ecl/run.sh [--debug]
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
port_dir="$(cd "$script_dir/../.." && pwd)"
repo_root="$(cd "$port_dir/../../.." && pwd)"
cd "$repo_root"

flags="-O2 -Wall -Wextra"
[[ "${1:-}" == "--debug" ]] && flags="-O0 -g"

out_dir="$port_dir/tests/ecl/build"
mkdir -p "$out_dir"

sources=(
  eagler-touhou/ports/th13/web/cpp/game/Interpolation.cpp
  eagler-touhou/ports/th13/web/cpp/game/EclResource.cpp
  eagler-touhou/ports/th13/web/cpp/game/EclContext.cpp
  eagler-touhou/ports/th13/web/cpp/game/EclInterpreter.cpp
  eagler-touhou/ports/th13/web/cpp/game/EclThreads.cpp
  eagler-touhou/ports/th13/web/cpp/game/EclFile.cpp
)

status=0
for suite in "$script_dir"/ecl_*_test.cpp; do
  name="$(basename "$suite" .cpp)"
  g++ -std=c++17 $flags -fno-exceptions -fno-rtti -ffp-contract=off \
    -Ieagler-touhou/ports/th13/web/cpp \
    "${sources[@]}" "$suite" \
    -o "$out_dir/$name" || status=1
done
(( status )) && exit 1

for suite in "$script_dir"/ecl_*_test.cpp; do
  name="$(basename "$suite" .cpp)"
  "$out_dir/$name" || status=1
done
exit $status
