#!/usr/bin/env bash
# docker_build.sh — Build image PQC NATIVE trên host hiện tại (CÙNG 1 Dockerfile).
#   • Chạy trên Pi (Mythic Beasts, arm64)  -> image pqc:wp1-arm64
#   • Chạy trên Linux/WSL x86              -> image pqc:wp1-amd64
# Build NATIVE (không giả lập) -> số liệu mới hợp lệ cho benchmark.
set -euo pipefail

# Về thư mục gốc repo (script nằm trong scripts/), nơi có docker/ và scripts/
cd "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

ARCH="$(uname -m)"
case "$ARCH" in
  aarch64|arm64) TAG="pqc:wp1-arm64" ;;
  x86_64|amd64)  TAG="pqc:wp1-amd64" ;;
  *)             TAG="pqc:wp1-$ARCH" ;;
esac

echo ">>> Build $TAG (native, arch=$ARCH). OpenSSL 3.6.2 từ nguồn ~20-40' lần đầu..."
docker build -t "$TAG" -f docker/Dockerfile.pqc .

echo ""
echo ">>> Verify môi trường:"
docker run --rm "$TAG" openssl version
docker run --rm "$TAG" sh -c 'openssl list -kem-algorithms | grep -i ML-KEM | head -3'
docker run --rm "$TAG" /opt/build/liboqs/build/tests/speed_kem ML-KEM-768 | head -4 || true

echo ""
echo ">>> Xong. Image: $TAG"
echo ">>> Ghi cấu hình máy:  docker run --rm -v \"\$PWD/data:/data\" $TAG capture_env.sh"

# ---------------------------------------------------------------------------
# (TÙY CHỌN) Cross-build arm64 TỪ x86 bằng buildx + QEMU — CHỈ để đóng gói/đẩy
# registry, KHÔNG dùng để đo (giả lập != Pi thật). Bỏ comment nếu cần:
#   docker buildx create --use --name pqcbuilder 2>/dev/null || true
#   docker buildx build --platform linux/arm64,linux/amd64 \
#     -t pqc:wp1 -f docker/Dockerfile.pqc --load .
# ---------------------------------------------------------------------------
