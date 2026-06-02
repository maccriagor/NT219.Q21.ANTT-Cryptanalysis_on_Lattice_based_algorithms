#!/usr/bin/env bash
# Sinh chứng chỉ TLS tự ký, KÝ BẰNG ML-DSA-65 (OpenSSL 3.6.2 native).
# Tài liệu: https://docs.openssl.org/3.5/man1/openssl-req/ , EVP_PKEY-ML-DSA
set -euo pipefail
OUT="${1:-/opt/httpd/conf}"
openssl req -x509 -new -newkey mldsa65 -nodes \
  -keyout "$OUT/server.key" -out "$OUT/server.crt" \
  -subj "/CN=localhost/O=NT219-PQC-Demo" -days 365
echo ">>> Đã sinh chứng chỉ ML-DSA-65:"
openssl x509 -in "$OUT/server.crt" -noout -subject -issuer | sed 's/^/    /'
openssl x509 -in "$OUT/server.crt" -noout -text | grep -i "Signature Algorithm" | head -1 | sed 's/^/    /'
