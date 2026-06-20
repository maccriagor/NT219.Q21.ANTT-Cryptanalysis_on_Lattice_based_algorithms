#!/usr/bin/env bash
# =============================================================================
# demo.sh - 2-minute guided demo for the Deliverable-5 recording:
# "short recording or live demo showing benchmark runs and TLS handshake
# comparisons" (brief section 11).
#
# Shows: (1) environment proof, (2) one live microbenchmark (ML-KEM-768 vs
# RSA-2048 sign-family), (3) TLS 1.3 handshake classical vs hybrid vs pure-PQC
# with an ML-DSA certificate. Requires: WP1 env + gen_tls_certs.sh done.
# =============================================================================
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
source "$HERE/versions.env"
# shellcheck disable=SC1091
source "$HERE/setenv.sh" >/dev/null
PORT="${PORT:-4434}"
step() { printf "\n\033[1;34m== %s ==\033[0m\n" "$*"; }

step "1/3 Environment (PQC-native OpenSSL)"
openssl version
openssl list -kem-algorithms | grep -m1 "ML-KEM-768"
openssl list -signature-algorithms | grep -m1 "ML-DSA-65"

step "2/3 Microbenchmark sample (median latency)"
BENCH="$ROOT/build/bench_evp"
[ -x "$BENCH" ] || { echo "build/bench_evp missing: run 'make' first"; exit 1; }
echo "--- ML-KEM-768 ---";  BENCH_ITERS=300 "$BENCH" mlkem 768 2>/dev/null | grep -E "median|mean" | head -4
echo "--- RSA-2048 ----";   BENCH_ITERS=100 "$BENCH" rsa 2048  2>/dev/null | grep -E "median|mean" | head -4

step "3/3 TLS 1.3 handshake: classical vs hybrid vs pure PQC (cert: ML-DSA-65)"
CRT="$HOME/pqc/tls/mldsa65.cert.pem"; KEY="$HOME/pqc/tls/mldsa65.key.pem"
[ -f "$CRT" ] || { echo "certs missing: run scripts/gen_tls_certs.sh"; exit 1; }
openssl s_server -accept "$PORT" -cert "$CRT" -key "$KEY" -tls1_3 \
    -groups "X25519:X25519MLKEM768:MLKEM768" -quiet >/dev/null 2>&1 &
SRV=$!; trap 'kill $SRV 2>/dev/null' EXIT; sleep 0.5
for grp in X25519 X25519MLKEM768 MLKEM768; do
  best=999999
  for _ in 1 2 3; do
    t0=$(date +%s%N)
    openssl s_client -connect "127.0.0.1:$PORT" -tls1_3 -groups "$grp" \
        -CAfile "$CRT" </dev/null >/dev/null 2>&1
    t1=$(date +%s%N); ms=$(( (t1-t0)/1000000 )); [ "$ms" -lt "$best" ] && best=$ms
  done
  printf "  %-16s best-of-3: %s ms\n" "$grp" "$best"
done
echo ""
echo "Demo done. Full matrix + statistics: 'make tls' then 'make analyze'."
