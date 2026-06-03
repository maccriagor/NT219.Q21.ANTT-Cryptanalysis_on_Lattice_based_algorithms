#!/usr/bin/env bash
# build_native.sh — build Crypto++ native (GNUmakefile) trên host hiện tại.
#   bash cryptopp_build/build_native.sh [CRYPTOPP_TAG] [CXXFLAGS]
set -euo pipefail
TAG="${1:-CRYPTOPP_8_9_0}"
CXXFLAGS="${2:--DNDEBUG -g2 -O2 -fPIC}"   # -O2 ổn định; '-O3 -march=native' nếu tối ưu cho chính máy này

[ -d cryptopp ] || git clone --depth 1 -b "$TAG" https://github.com/weidai11/cryptopp.git
cd cryptopp
make clean || true
make -j"$(nproc)" CXXFLAGS="$CXXFLAGS" static dynamic cryptest.exe
make install PREFIX="$PWD/_install"

echo "== build info =="; g++ --version | head -1; echo "TAG=$TAG  CXXFLAGS=$CXXFLAGS"
echo "== correctness =="; ./cryptest.exe v | tail -3       # validate
echo "== benchmark   =="; ./cryptest.exe b 3               # 3 giây/thuật toán -> bảng MB/s, cpb
