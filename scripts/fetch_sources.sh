#!/usr/bin/env bash
# fetch_sources.sh — Tải nguồn upstream vào vendor/ (chạy trên HOST, nơi HTTPS chạy được).
# Cần thiết vì một số host (vd Raspberry Pi thuê, IPv6-only) chặn HTTPS trong container Docker.
# Sau đó Dockerfile COPY từ vendor/ nên build không cần network (trừ apt qua port 80).
set -euo pipefail
OPENSSL_VER="${OPENSSL_VER:-3.6.2}"
LIBOQS_TAG="${LIBOQS_TAG:-0.12.0}"
HTTPD_VER="${HTTPD_VER:-2.4.62}"
APR_VER="${APR_VER:-1.7.5}"
APRUTIL_VER="${APRUTIL_VER:-1.6.3}"

cd "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
mkdir -p vendor && cd vendor

echo "[1] OpenSSL ${OPENSSL_VER}"
wget -q --tries=3 --timeout=90 -O "openssl-${OPENSSL_VER}.tar.gz" \
  "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VER}/openssl-${OPENSSL_VER}.tar.gz"
wget -q --tries=3 --timeout=90 -O "openssl-${OPENSSL_VER}.tar.gz.sha256" \
  "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VER}/openssl-${OPENSSL_VER}.tar.gz.sha256"

echo "[2] liboqs ${LIBOQS_TAG}"
rm -rf liboqs
git clone --depth 1 --branch "${LIBOQS_TAG}" https://github.com/open-quantum-safe/liboqs.git liboqs
rm -rf liboqs/.git

echo "[3] PQClean (sparse: ml-kem, ml-dsa, common)"
rm -rf pqclean
git clone --depth 1 --filter=blob:none --sparse https://github.com/PQClean/PQClean.git pqclean
( cd pqclean && git sparse-checkout set \
    crypto_kem/ml-kem-512 crypto_kem/ml-kem-768 crypto_kem/ml-kem-1024 \
    crypto_sign/ml-dsa-44 crypto_sign/ml-dsa-65 crypto_sign/ml-dsa-87 common )
rm -rf pqclean/.git

echo "[4] Apache httpd ${HTTPD_VER} + apr ${APR_VER} + apr-util ${APRUTIL_VER} (cho demo PQC TLS)"
wget -q --tries=3 --timeout=90 -O "httpd-${HTTPD_VER}.tar.gz"     "https://archive.apache.org/dist/httpd/httpd-${HTTPD_VER}.tar.gz"
wget -q --tries=3 --timeout=90 -O "apr-${APR_VER}.tar.gz"         "https://archive.apache.org/dist/apr/apr-${APR_VER}.tar.gz"
wget -q --tries=3 --timeout=90 -O "apr-util-${APRUTIL_VER}.tar.gz" "https://archive.apache.org/dist/apr/apr-util-${APRUTIL_VER}.tar.gz"

echo "Done. vendor/ sẵn sàng:"
du -sh "openssl-${OPENSSL_VER}.tar.gz" liboqs pqclean "httpd-${HTTPD_VER}.tar.gz" 2>/dev/null || true
