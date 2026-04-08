#!/bin/bash
# RSA-15360 Benchmark Builder
set -euo pipefail

echo "🔨 Installing dependencies..."
sudo apt update -qq
sudo apt install -y -qq g++ libssl-dev build-essential

echo "🔨 Building RSA-15360 Benchmark..."
g++ -O3 -std=c++17 -Wall -flto -march=native -mtune=native \
    rsa15360_full.cpp -lssl -lcrypto -o rsa15360_full -pthread

echo "✅ BUILD COMPLETE!"
echo "▶️  Run: ./run.sh"
echo "📊 1-click: ./build_run.sh"
