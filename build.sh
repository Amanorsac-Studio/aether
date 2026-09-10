#!/usr/bin/env bash
# AETHER — macOS / Linux build. Output: build/AETHER_artefacts/Release/{VST3,AU}
set -e
cd "$(dirname "$0")"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
./build/aether_dsp_test
echo "Done. Plugins are in build/AETHER_artefacts/Release/"
