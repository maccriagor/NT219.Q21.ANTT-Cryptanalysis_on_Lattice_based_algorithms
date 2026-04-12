#!/bin/bash
# RSA-15360 Benchmark Builder (OpenSSL 3.0+)
set -euo pipefail

echo "🔨 Installing dependencies..."
sudo apt update -qq
sudo apt install -y -qq g++ libssl-dev build-essential cpufrequtils

echo "🔨 Building RSA-15360 Benchmark (OpenSSL 3.0 EVP APIs)..."
g++ -O3 -std=c++17 -Wall -Wextra -flto -march=native -mtune=native \
    -Wno-deprecated-declarations \
    rsa15360_full.cpp -lssl -lcrypto -o rsa15360_full -pthread

echo "✅ BUILD COMPLETE! (No warnings)"
echo "▶️  Run: ./run.sh"
echo "📊 1-click: ./build_run.sh"
