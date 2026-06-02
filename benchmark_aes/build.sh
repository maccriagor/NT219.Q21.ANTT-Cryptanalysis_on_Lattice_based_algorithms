#!/usr/bin/env bash
# AES-GCM (đa mức 128/192/256) builder — chỉ cần OpenSSL EVP.
set -euo pipefail
cd "$(dirname -- "$0")"
echo "🔨 Building benchmark_aes ..."
g++ -O2 -std=c++17 -Wall benchmark_aes.cpp -lssl -lcrypto -o benchmark_aes
echo "✅ BUILD COMPLETE!"; size benchmark_aes 2>/dev/null || true
echo -n "🔹 SHA-256: "; sha256sum benchmark_aes | cut -d' ' -f1
echo "▶️  Run đủ 3 mức × N lần: ./run.sh   ·   1-click: ./build_run.sh"
