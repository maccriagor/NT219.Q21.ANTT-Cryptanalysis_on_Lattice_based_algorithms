#!/bin/bash
# ECDSA P-521 Full Benchmark Builder
set -euo pipefail

echo "🔨 Installing dependencies..."
sudo apt update -qq
sudo apt install -y -qq g++ libssl-dev build-essential

echo "🔨 Building ECDSA P-521 Benchmark..."
g++ -O3 -std=c++17 -Wall -Wno-deprecated-declarations -flto -march=native -mtune=native \
    benchmark_p521.cpp -lssl -lcrypto -o benchmark_p521 -pthread

echo "✅ BUILD COMPLETE!"
echo "------------------------------------------------------------------------"
echo "📊 STATIC BINARY PROFILING (Post-Compilation)"
echo "------------------------------------------------------------------------"

# 1. Hiển thị dung lượng (Disk Usage)
echo -n "🔹 Storage Allocation: "
du -h benchmark_p521 | cut -f1

# 2. Hiển thị mã băm (Cryptographic Integrity)
echo -n "🔹 SHA-256 Checksum  : "
sha256sum benchmark_p521 | cut -d' ' -f1

# 3. Hiển thị chi tiết file (Binary Metadata)
echo "🔹 Metadata Details  :"
ls -la benchmark_p521 | awk '{print "   - Permissions: " $1 "\n   - Size (Bytes): " $5 "\n   - Timestamp  : " $6 " " $7 " " $8}'

echo "------------------------------------------------------------------------"
echo ""
echo "▶️  Run: ./run.sh"
echo "📊 1-click: ./build_run.sh"#!/bin/bash
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
