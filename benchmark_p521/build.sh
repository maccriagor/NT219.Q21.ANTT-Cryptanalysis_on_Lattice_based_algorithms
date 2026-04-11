#!/bin/bash
# ECDSA P-521 Full Benchmark Builder
set -euo pipefail

echo "🔨 Installing dependencies..."
sudo apt update -qq
sudo apt install -y -qq g++ libssl-dev build-essential

echo "🔨 Building ECDSA P-521 Benchmark..."
g++ -O3 -std=c++17 -Wall -flto -march=native -mtune=native \
    benchmark_p521.cpp -lssl -lcrypto -o benchmark_p521 -pthread

echo "✅ BUILD COMPLETE!"
echo "📏 Binary info:"
du -h benchmark_p521 | cut -f1
sha256sum benchmark_p521
ls -la benchmark_p521
echo ""
echo "▶️  Run: ./run.sh"
echo "📊 1-click: ./build_run.sh"