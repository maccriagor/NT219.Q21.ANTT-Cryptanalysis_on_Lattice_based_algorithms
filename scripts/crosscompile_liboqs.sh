#!/usr/bin/env bash
# ============================================================================
# scripts/crosscompile_liboqs.sh
# CROSS-COMPILE liboqs cho arm64 từ PC Ubuntu x86_64.
# >>> CHỈ TẠO ARTIFACT <<<  KHÔNG dùng output này để đo hiệu năng.
# Số NEON/cycle phải lấy từ build NATIVE trên chính Pi 4 (scripts/build_liboqs.sh).
# Yêu cầu: apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu cmake ninja-build
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")/.."                 # về gốc repo (giống mọi script khác)
ROOT="$(pwd)"                            # đường tuyệt đối tới gốc repo
source scripts/versions.env             # lấy $LIBOQS_TAG

# 1) Lấy source liboqs đúng version đã ghim
WORK="$ROOT/.build/liboqs"
mkdir -p "$WORK"
if [[ ! -d "$WORK/src/.git" ]]; then
  git clone https://github.com/open-quantum-safe/liboqs.git "$WORK/src"
fi
git -C "$WORK/src" fetch --tags
git -C "$WORK/src" checkout "$LIBOQS_TAG"
git -C "$WORK/src" rev-parse HEAD > "$ROOT/docs/liboqs-cross.commit"   # ghi lại commit

# 2) Cross-compile bằng toolchain file (đường TUYỆT ĐỐI — CMake yêu cầu)
#    OQS_DIST_BUILD=ON: bắt buộc cho cross-compile (runtime dispatch, portable)
#    OQS_USE_OPENSSL=OFF: tránh phải có OpenSSL arm64 sysroot lúc cross-compile
cmake -GNinja -S "$WORK/src" -B "$WORK/build-cross" \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchain-aarch64.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DOQS_DIST_BUILD=ON \
  -DBUILD_SHARED_LIBS=ON \
  -DOQS_USE_OPENSSL=OFF

cmake --build "$WORK/build-cross" -j"$(nproc)"

# 3) Bằng chứng đây là binary arm64 (không phải x86)
echo ">> Kiểm chứng kiến trúc:"
file "$WORK/build-cross/lib/"*.so* 2>/dev/null | head -3 || true
echo ">> Xong. Artifact arm64 ở: $WORK/build-cross"
echo ">> NHẮC LẠI: artifact only — KHÔNG đo. Số đo lấy từ build native trên Pi."
