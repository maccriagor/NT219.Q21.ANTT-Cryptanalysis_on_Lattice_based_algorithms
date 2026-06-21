#!/bin/bash
# =============================================================================
# gen_tls_certs.sh - generate self-signed TLS server certs for the matrix,
# grouped by NIST security category (like-for-like comparison).
#
# Usage:
#   bash scripts/gen_tls_certs.sh                 # default: Cat 3
#   CAT=2   bash scripts/gen_tls_certs.sh         # Cat 2 set
#   CAT=5   bash scripts/gen_tls_certs.sh         # Cat 5 set
#
# Environment variables:
#   CERT_SET="rsa7680 mldsa65"  bash scripts/gen_tls_certs.sh   # custom set
#   FORCE=1 bash scripts/gen_tls_certs.sh         # overwrite existing
#
# Sets per NIST security category (approx AES strength):
#   Cat 2 (~AES-128):  rsa3072   ecp256  mldsa44   (*ML-DSA-44 claims Cat 2)
#   Cat 3 (~AES-192):  rsa7680   ecp384  mldsa65
#   Cat 5 (~AES-256):  rsa15360  ecp521  mldsa87
# Output: $HOME/pqc/tls/<name>.key.pem + <name>.cert.pem   (keys NEVER in repo)
# =============================================================================

set -eu

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/versions.env"
# shellcheck disable=SC1091
source "$HERE/setenv.sh" >/dev/null

# Need an OpenSSL that actually has ML-DSA
if ! openssl list -signature-algorithms | grep -qi "ML-DSA"; then
  echo "ERROR: this OpenSSL lacks ML-DSA: run scripts/build_openssl.sh first"
  exit 1
fi

OUT="$HOME/pqc/tls"
mkdir -p "$OUT"

# Pick the set: CERT_SET wins; else CAT={1,3,5}; else Cat 3
case "${CAT:-3}" in
  2) CAT_SET="rsa3072 ecp256 mldsa44" ;;
  3) CAT_SET="rsa7680 ecp384 mldsa65" ;;
  5) CAT_SET="rsa15360 ecp521 mldsa87" ;;
  *) echo "ERROR: CAT must be 2, 3, or 5"; exit 1 ;;
esac
SET="${CERT_SET:-$CAT_SET}"

# Map a cert name to its openssl genpkey args
genkey() {
  case "$1" in
    rsa3072)  echo "-algorithm RSA -pkeyopt rsa_keygen_bits:3072" ;;
    rsa7680)  echo "-algorithm RSA -pkeyopt rsa_keygen_bits:7680" ;;
    rsa15360) echo "-algorithm RSA -pkeyopt rsa_keygen_bits:15360" ;;
    ecp256)   echo "-algorithm EC  -pkeyopt ec_paramgen_curve:P-256" ;;
    ecp384)   echo "-algorithm EC  -pkeyopt ec_paramgen_curve:P-384" ;;
    ecp521)   echo "-algorithm EC  -pkeyopt ec_paramgen_curve:P-521" ;;
    mldsa44)  echo "-algorithm ML-DSA-44" ;;
    mldsa65)  echo "-algorithm ML-DSA-65" ;;
    mldsa87)  echo "-algorithm ML-DSA-87" ;;
    *) return 1 ;;
  esac
}

for name in $SET; do
  KEY="$OUT/$name.key.pem"
  CRT="$OUT/$name.cert.pem"

  if [ -f "$CRT" ] && [ "${FORCE:-0}" != "1" ]; then
    echo "skip : $name (exists; FORCE=1 to regenerate)"
    continue
  fi

  args="$(genkey "$name")" || { echo "skip : $name (unknown)"; continue; }

  # shellcheck disable=SC2086
  openssl genpkey $args -out "$KEY"
  openssl req -x509 -key "$KEY" -out "$CRT" -days 365 -subj "/CN=localhost"

  echo "built: $name  cert=$(stat -c%s "$CRT") bytes  key=$(stat -c%s "$KEY") bytes"
done

echo "DONE. Certificates in $OUT (next: bash scripts/build_nginx.sh then bash nginx-bench/run.sh)"