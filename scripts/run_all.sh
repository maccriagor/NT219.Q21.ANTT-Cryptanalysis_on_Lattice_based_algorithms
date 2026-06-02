#!/usr/bin/env bash
# run_all.sh — Chạy toàn bộ pipeline. Bạn CHỈ CẦN chạy script này.
#   1) Build image PQC (OpenSSL 3.6.2 + liboqs + PQClean)   [tự prefetch vendor/ nếu thiếu]
#   2) Ghi cấu hình máy -> data/env/
#   3) Verify đúng đắn (PQC native + liboqs + PQClean)
#   4) Microbenchmark -> data/results/micro_<plat>.csv
#   5) Demo TLS 1.3 PQC + Apache -> data/results/
# Biến: N (số vòng đo, mặc định 1000), WARM (warm-up, 100).
set -euo pipefail
cd "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
ARCH="$(uname -m)"; case "$ARCH" in aarch64|arm64) TAG=pqc:wp1-arm64 ;; *) TAG=pqc:wp1-amd64 ;; esac

echo "############ [1/5] Build image PQC ############"
bash scripts/docker_build.sh

echo "############ [2/5] Ghi cấu hình máy ############"
mkdir -p data
docker run --rm -v "$PWD/data:/data" "$TAG" capture_env.sh

echo "############ [3/5] Verify PQC (native + liboqs + PQClean) ############"
docker run --rm "$TAG" sh -c '
  echo "- openssl: $(openssl version)";
  echo "- ML-KEM native:"; openssl list -kem-algorithms | grep -i ML-KEM | sed "s/^/    /";
  echo "- ML-DSA native:"; openssl list -signature-algorithms | grep -i ML-DSA | sed "s/^/    /";
  echo "- liboqs speed_kem ML-KEM-768 (smoke):"; /opt/build/liboqs/build/tests/speed_kem ML-KEM-768 2>/dev/null | head -4 | sed "s/^/    /";
  echo "- PQClean dirs:"; ls /opt/pqclean/crypto_kem | sed "s/^/    /"
'

echo "############ [4/5] Microbenchmark ############"
bash scripts/run_micro.sh "${N:-1000}" "${WARM:-100}"

echo "############ [5/5] Demo TLS 1.3 PQC + Apache ############"
bash scripts/run_apache_demo.sh

echo
echo "############ HOÀN TẤT — kết quả trong data/ ############"
find data -type f | sed 's/^/  /'
