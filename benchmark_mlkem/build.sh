#!/usr/bin/env bash
# ML-KEM (đa mức 512/768/1024) builder — link với libpqc.a (PQClean reference, gcc).
set -euo pipefail
HERE="$(cd -- "$(dirname -- "$0")" && pwd)"; ROOT="$(cd -- "$HERE/.." && pwd)"; cd "$HERE"
bash "$ROOT/scripts/build_pqc_lib.sh"
echo "🔨 Building benchmark_mlkem ..."
g++ -O2 -std=c++17 -Wall benchmark_mlkem.cpp -I"$ROOT/vendor/pqclean" "$ROOT/build/libpqc.a" -o benchmark_mlkem
echo "✅ BUILD COMPLETE!"; size benchmark_mlkem 2>/dev/null || true
echo -n "🔹 SHA-256: "; sha256sum benchmark_mlkem | cut -d' ' -f1
echo "▶️  Run đủ 3 mức × N lần: ./run.sh   ·   1-click: ./build_run.sh"
