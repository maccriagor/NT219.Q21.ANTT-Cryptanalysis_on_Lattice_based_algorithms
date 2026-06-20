#!/usr/bin/env bash
# =============================================================================
# gen_tls_certs.sh - Generate self-signed TLS server certificates for WP4.
#
# Certificates carry the SIGNATURE algorithm only; the KEM (ML-KEM) is an
# ephemeral key exchange and never appears in certificates (see openssl-users,
# V. Dukhovni). So the WP4 matrix is: cert sig alg x TLS group.
#
# Algorithms (names per OpenSSL docs, "added in OpenSSL 3.5" for ML-DSA):
#   rsa2048  - classical baseline           (RSA-2048)
#   ecp256   - classical elliptic baseline  (ECDSA P-256)
#   mldsa65  - post-quantum, FIPS 204       (ML-DSA-65)
# Optional extras via: CERT_SET="mldsa44 mldsa87 rsa3072" bash gen_tls_certs.sh
#
# Source: https://docs.openssl.org/3.5/man7/EVP_PKEY-ML-DSA/ (algorithm names)
# Output: $HOME/pqc/tls/<name>.key.pem / <name>.cert.pem  (keys NEVER in repo)
# Requires: build_openssl.sh done (OpenSSL >= 3.5 with ML-DSA).
# =============================================================================
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/versions.env"
# shellcheck disable=SC1091
source "$HERE/setenv.sh" >/dev/null

openssl list -signature-algorithms | grep -qi "ML-DSA" \
  || { echo "ERROR: this OpenSSL lacks ML-DSA: run scripts/build_openssl.sh first"; exit 1; }

OUT="$HOME/pqc/tls"; mkdir -p "$OUT"
DEFAULT_SET="rsa2048 ecp256 mldsa65"
SET="${CERT_SET:-$DEFAULT_SET}"

genkey() { # name -> genpkey args
  case "$1" in
    rsa2048) echo "-algorithm RSA -pkeyopt rsa_keygen_bits:2048";;
    rsa3072) echo "-algorithm RSA -pkeyopt rsa_keygen_bits:3072";;
    ecp256)  echo "-algorithm EC  -pkeyopt ec_paramgen_curve:P-256";;
    mldsa44) echo "-algorithm ML-DSA-44";;
    mldsa65) echo "-algorithm ML-DSA-65";;
    mldsa87) echo "-algorithm ML-DSA-87";;
    *) echo ""; return 1;;
  esac
}

for name in $SET; do
  KEY="$OUT/$name.key.pem"; CRT="$OUT/$name.cert.pem"
  if [ -f "$CRT" ] && [ "${FORCE:-0}" != "1" ]; then
    echo "skip : $name (exists; FORCE=1 to regenerate)"; continue
  fi
  args="$(genkey "$name")" || { echo "skip : $name (unknown)"; continue; }
  # shellcheck disable=SC2086
  openssl genpkey $args -out "$KEY"
  openssl req -x509 -key "$KEY" -out "$CRT" -days 365 -subj "/CN=localhost"
  echo "built: $name  cert=$(stat -c%s "$CRT") bytes  key=$(stat -c%s "$KEY") bytes"
done
echo "DONE. Certificates in $OUT (next: scripts/bench_tls.sh)"
