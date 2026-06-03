# docker/buildx-multiarch.sh
#!/usr/bin/env bash
set -euo pipefail
# 1) Nạp QEMU emulator (binfmt) — chỉ cần trên Linux standalone
docker run --privileged --rm tonistiigi/binfmt --install all
# 2) Tạo builder docker-container (driver mặc định KHÔNG build được multi-arch)
docker buildx create --name multiarch --driver docker-container --use --bootstrap
docker buildx inspect --bootstrap         # phải thấy linux/amd64, linux/arm64
# 3) Build + push (multi-arch BẮT BUỘC --push, không --load được)
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  -f docker/Dockerfile.x86_64 \
  -t <registry>/nt219-pqc:0.10.0 \
  --push .
# 4) Kiểm tra manifest list
docker buildx imagetools inspect <registry>/nt219-pqc:0.10.0