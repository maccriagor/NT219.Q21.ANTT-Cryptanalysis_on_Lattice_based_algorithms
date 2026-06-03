#!/usr/bin/env bash
# buildx_multiarch.sh — build image Crypto++ đa kiến trúc (amd64 + arm64) bằng docker buildx.
# ⚠ arm64 build qua QEMU = GIẢ LẬP → CHỈ để đóng gói/CI, KHÔNG dùng để ĐO benchmark (đo trên ARM thật).
set -euo pipefail
cd "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"   # repo root (chứa cryptopp_build/Dockerfile.cryptopp)
DF=cryptopp_build/Dockerfile.cryptopp

# 1) Bật giả lập đa kiến trúc + tạo builder
docker run --privileged --rm tonistiigi/binfmt --install arm64,amd64 >/dev/null 2>&1 || true
docker buildx create --name xbuilder --use 2>/dev/null || docker buildx use xbuilder

# 2a) Đẩy registry (ảnh đa kiến trúc trong 1 tag):
#   docker buildx build --platform linux/amd64,linux/arm64 -t <registry>/cryptopp:multi -f "$DF" --push .

# 2b) Lấy về local TỪNG kiến trúc (--load chỉ nhận 1 platform/lần):
docker buildx build --platform linux/amd64 -t cryptopp:amd64 -f "$DF" --load .
docker buildx build --platform linux/arm64 -t cryptopp:arm64 -f "$DF" --load .

echo ">>> cryptopp:amd64 (native) · cryptopp:arm64 (QEMU — chỉ đóng gói)"
