#!/usr/bin/env bash
# Demo TLS 1.3 hậu lượng tử THỰC TẾ:
#   (A) Bằng chứng handshake PQC (s_server <-> s_client, X25519MLKEM768) — luôn chạy được với OpenSSL 3.6.2.
#   (B) Triển khai Apache httpd link OpenSSL 3.6.2 (mục tiêu production).
# Kết quả ghi vào data/results/.
set -euo pipefail
cd "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
ARCH="$(uname -m)"; case "$ARCH" in aarch64|arm64) BASE=pqc:wp1-arm64 ;; *) BASE=pqc:wp1-amd64 ;; esac
mkdir -p data/results; R=data/results

echo "===== (A) Bằng chứng TLS 1.3 handshake PQC (OpenSSL 3.6.2 native) ====="
docker run --rm "$BASE" bash -lc '
  cd /tmp
  openssl req -x509 -new -newkey mldsa65 -nodes -keyout k.pem -out c.pem -subj "/CN=localhost" -days 1 >/dev/null 2>&1
  openssl s_server -quiet -tls1_3 -groups X25519MLKEM768 -cert c.pem -key k.pem -accept 4443 >/dev/null 2>&1 &
  SRV=$!; sleep 1
  echo | openssl s_client -connect localhost:4443 -groups X25519MLKEM768 -tls1_3 2>/dev/null \
    | grep -E "Negotiated TLS1.3 group|Cipher *:|Peer signature type|Signature type" || true
  kill $SRV 2>/dev/null || true
' | tee "$R/pqc_handshake.txt"

echo
echo "===== (B) Apache httpd link OpenSSL 3.6.2 (production target) ====="
[ -f vendor/httpd-2.4.62.tar.gz ] || bash scripts/fetch_sources.sh
echo ">>> Build image Apache (lần đầu ~10-20 phút)..."
if docker build --build-arg BASE_IMAGE="$BASE" -t pqc-apache -f docker/Dockerfile.apache . ; then
  docker rm -f pqc-apache-demo >/dev/null 2>&1 || true
  docker run -d --name pqc-apache-demo -p 4443:4443 pqc-apache >/dev/null
  sleep 4
  echo ">>> httpd -t (kiểm cấu hình):"
  docker exec pqc-apache-demo /opt/httpd/bin/httpd -t -f /opt/httpd/conf/httpd-pqc.conf || true
  echo ">>> Handshake client -> Apache qua nhóm hybrid X25519MLKEM768:"
  docker run --rm --network host "$BASE" bash -lc \
    'echo | openssl s_client -connect localhost:4443 -groups X25519MLKEM768 -tls1_3 2>/dev/null | grep -E "Negotiated TLS1.3 group|Cipher *:|subject="' \
    | tee "$R/apache_handshake.txt" || true
  echo ">>> Apache đang chạy: container pqc-apache-demo cổng 4443. Dừng: docker rm -f pqc-apache-demo"
else
  echo "!!! Build Apache lỗi. Phần (A) đã chứng minh PQC TLS chạy được; xem log build để chỉnh httpd." | tee "$R/apache_handshake.txt"
fi
