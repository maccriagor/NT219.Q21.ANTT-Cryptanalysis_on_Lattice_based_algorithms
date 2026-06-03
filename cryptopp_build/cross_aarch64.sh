#!/usr/bin/env bash
# cross_aarch64.sh — cross-compile Crypto++ cho ARM64 bằng toolchain aarch64-linux-gnu.
# Dùng GNUmakefile-cross của Crypto++ (binary ELF aarch64 -> copy sang Pi để chạy/benchmark).
#   bash cryptopp_build/cross_aarch64.sh [CRYPTOPP_TAG]
set -euo pipefail
TAG="${1:-CRYPTOPP_8_9_0}"

# Toolchain chéo
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  g++-aarch64-linux-gnu gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu git make file

[ -d cryptopp ] || git clone --depth 1 -b "$TAG" https://github.com/weidai11/cryptopp.git
cd cryptopp
make -f GNUmakefile-cross clean || true

# -march=armv8-a : NEON sẵn có; dùng 'armv8-a+crypto' nếu CPU đích có ARMv8 AES (Pi4/BCM2711 KHÔNG có)
make -f GNUmakefile-cross -j"$(nproc)" \
  CXX=aarch64-linux-gnu-g++ \
  AR=aarch64-linux-gnu-ar \
  RANLIB=aarch64-linux-gnu-ranlib \
  CXXFLAGS="-DNDEBUG -g2 -O2 -fPIC -march=armv8-a" \
  static dynamic cryptest.exe

echo "== xác nhận kiến trúc =="
file libcryptopp.a cryptest.exe | sed 's/^/  /'   # phải thấy 'ARM aarch64'
echo ">>> Copy cryptest.exe + libcryptopp.* sang Pi (scp) rồi chạy: ./cryptest.exe b 3"
