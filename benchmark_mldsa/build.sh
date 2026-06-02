#!/usr/bin/env bash
# ML-DSA (đa mức 44/65/87) builder — link với libpqc.a (PQClean reference, gcc).
set -euo pipefail
HERE="$(cd -- "$(dirname -- "$0")" && pwd)"; ROOT="$(cd -- "$HERE/.." && pwd)"; cd "$HERE"
bash "$ROOT/scripts/build_pqc_lib.sh"
echo "🔨 Building benchmark_mldsa ..."
g++ -O2 -std=c++17 -Wall benchmark_mldsa.cpp -I"$ROOT/vendor/pqclean" "$ROOT/build/libpqc.a" -o benchmark_mldsa
echo "✅ BUILD COMPLETE!"; size benchmark_mldsa 2>/dev/null || true
echo -n "🔹 SHA-256: "; sha256sum benchmark_mldsa | cut -d' ' -f1
echo "▶️  Run đủ 3 mức × N lần: ./run.sh   ·   1-click: ./build_run.sh"
