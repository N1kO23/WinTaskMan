#!/bin/sh
# Clean release build. The program ends up in build/WinTaskMan.
set -eu
cd "$(dirname "$0")"

rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel "$(nproc)"
