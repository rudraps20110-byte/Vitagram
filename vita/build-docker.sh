#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

docker run --rm \
  -v "${SCRIPT_DIR}:/src" \
  -w /src \
  vitasdk/vitasdk:latest \
  bash -c 'mkdir -p build && cd build && cmake .. -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake && make -j$(nproc)'

echo "Built: ${SCRIPT_DIR}/build/VitaGram.vpk"

