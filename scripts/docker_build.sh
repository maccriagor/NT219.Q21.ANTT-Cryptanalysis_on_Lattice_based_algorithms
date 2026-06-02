#!/usr/bin/env bash
# Build image PQC native trên host hiện tại (cùng một Dockerfile cho cả hai kiến trúc).
#   Pi (arm64)    -> pqc:wp1-arm64
#   Linux/WSL x86 -> pqc:wp1-amd64
set -euo pipefail
cd "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

ARCH="$(uname -m)"
case "$ARCH" in
  aarch64|arm64) TAG="pqc:wp1-arm64" ;;
  x86_64|amd64)  TAG="pqc:wp1-amd64" ;;
  *)             TAG="pqc:wp1-$ARCH" ;;
esac

echo ">>> Build $TAG (arch=$ARCH). Lần đầu build OpenSSL từ nguồn mất ~20-40 phút."
docker build -t "$TAG" -f docker/Dockerfile.pqc .

echo
echo ">>> Kiem tra image:"
docker run --rm "$TAG" openssl version
docker run --rm "$TAG" sh -c 'openssl list -kem-algorithms | grep -i ML-KEM | head -3'

echo
echo ">>> Image: $TAG"
echo ">>> Ghi cau hinh may: docker run --rm -v \"\$PWD/data:/data\" $TAG capture_env.sh"

# Tùy chọn — cross-build đa kiến trúc bằng buildx (chỉ để đóng gói, không dùng để đo):
#   docker buildx build --platform linux/arm64,linux/amd64 -t pqc:wp1 -f docker/Dockerfile.pqc --load .
