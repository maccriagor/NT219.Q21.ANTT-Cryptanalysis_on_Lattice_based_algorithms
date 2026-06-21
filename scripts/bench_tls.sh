#!/usr/bin/env bash
# =============================================================================
# bench_tls.sh - WP4 macrobenchmark: TLS 1.3 full-handshake latency and
# throughput under concurrency, on loopback (brief 7.4 / 9 / RQ3).
#
# Matrix = certificate sig alg (rsa2048, ecp256, mldsa65)
#        x TLS group: X25519 (classical), X25519MLKEM768 (hybrid, RQ3),
#                     MLKEM768 (pure PQC).
#   Group names per OpenSSL 3.5 docs (SSL_CTX_set1_groups_list): hybrids
#   X25519MLKEM768 / SecP256r1MLKEM768 / SecP384r1MLKEM1024; case-insensitive.
#
# Latency  : TLS_ITERS sequential full handshakes via s_client (warm-up first),
#            wall-clock per handshake -> median/mean/p95 (brief 7.5).
# Throughput: TLS_CONC parallel s_client workers for TLS_DUR seconds
#            -> completed handshakes/sec. (wrk is NOT used: distro wrk links
#            the system OpenSSL, which lacks ML-KEM groups.)
#
# Env knobs: TLS_ITERS=50 TLS_WARMUP=5 TLS_CONC=4 TLS_DUR=10 PORT=4433
# Output   : data/tls_handshake_<arch>.csv
# Requires : gen_tls_certs.sh done; OpenSSL >= 3.5 (ML-KEM groups).
# =============================================================================
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
source "$HERE/versions.env"
# shellcheck disable=SC1091
source "$HERE/setenv.sh" >/dev/null

openssl list -kem-algorithms | grep -qi "ML-KEM" \
  || { echo "ERROR: this OpenSSL lacks ML-KEM: run scripts/build_openssl.sh"; exit 1; }
TLSDIR="$HOME/pqc/tls"
[ -f "$TLSDIR/mldsa65.cert.pem" ] || { echo "ERROR: certs missing: run scripts/gen_tls_certs.sh"; exit 1; }

PORT="${PORT:-4433}"; ITERS="${TLS_ITERS:-50}"; WARM="${TLS_WARMUP:-5}"
CONC="${TLS_CONC:-4}"; DUR="${TLS_DUR:-10}"
CERTS="${CERTS:-rsa2048 ecp256 mldsa65}"
GROUP_LIST="${GROUP_LIST:-X25519 X25519MLKEM768 MLKEM768}"
SERVER_GROUPS="X25519:X25519MLKEM768:MLKEM768"
ARCH="$(uname -m)"
OUTCSV="$ROOT/data/tls_handshake_${ARCH}.csv"
mkdir -p "$ROOT/data"
echo "arch,cert,group,hs_median_ms,hs_mean_ms,hs_p95_ms,throughput_hs_s,concurrency,cert_bytes" > "$OUTCSV"

SRV_PID=""
cleanup() { [ -n "$SRV_PID" ] && kill "$SRV_PID" 2>/dev/null; wait 2>/dev/null; }
trap cleanup EXIT

one_handshake() { # $1=group $2=cafile -> exit 0 on success
  openssl s_client -connect "127.0.0.1:$PORT" -tls1_3 -groups "$1" \
      -CAfile "$2" -verify_return_error </dev/null >/dev/null 2>&1
}

for cert in $CERTS; do
  CRT="$TLSDIR/$cert.cert.pem"; KEY="$TLSDIR/$cert.key.pem"
  [ -f "$CRT" ] || { echo "skip cert $cert (missing)"; continue; }
  openssl s_server -accept "$PORT" -cert "$CRT" -key "$KEY" \
      -tls1_3 -groups "$SERVER_GROUPS" -quiet >/dev/null 2>&1 &
  SRV_PID=$!
  # Wait until the server actually answers a handshake.
  for _ in $(seq 1 50); do one_handshake "X25519" "$CRT" && break; sleep 0.1; done

  for grp in $GROUP_LIST; do
    if ! one_handshake "$grp" "$CRT"; then
      echo "==> $cert / $grp : FAILED (group not negotiable) - recorded as NA"
      echo "$ARCH,$cert,$grp,NA,NA,NA,NA,$CONC,$(stat -c%s "$CRT")" >> "$OUTCSV"
      continue
    fi
    for _ in $(seq 1 "$WARM"); do one_handshake "$grp" "$CRT"; done
    # ---- latency: ITERS sequential full handshakes ----
    TMP="$(mktemp)"
    for _ in $(seq 1 "$ITERS"); do
      t0=$(date +%s%N); one_handshake "$grp" "$CRT"; t1=$(date +%s%N)
      echo $(( (t1 - t0) / 1000 )) >> "$TMP"        # microseconds
    done
    read -r MED MEAN P95 <<< "$(sort -n "$TMP" | awk '
      { a[NR]=$1; s+=$1 }
      END { m=(NR%2)?a[(NR+1)/2]:(a[NR/2]+a[NR/2+1])/2;
            p=a[int(0.95*NR)>0?int(0.95*NR):1];
            printf "%.3f %.3f %.3f", m/1000, s/NR/1000, p/1000 }')"
    rm -f "$TMP"
    # ---- throughput: CONC parallel workers for DUR seconds ----
    CNTDIR="$(mktemp -d)"; WPIDS=""
    for w in $(seq 1 "$CONC"); do
      ( n=0; end=$(( $(date +%s) + DUR ))
        while [ "$(date +%s)" -lt "$end" ]; do one_handshake "$grp" "$CRT" && n=$((n+1)); done
        echo "$n" > "$CNTDIR/$w" ) &
      WPIDS="$WPIDS $!"
    done
    wait $WPIDS          # wait ONLY for the workers, not the background s_server (bare 'wait' would hang on it)
    TOTAL=$(cat "$CNTDIR"/* | awk '{s+=$1} END{print s}')
    rm -rf "$CNTDIR"
    TPS=$(awk -v t="$TOTAL" -v d="$DUR" 'BEGIN{printf "%.1f", t/d}')
    echo "$ARCH,$cert,$grp,$MED,$MEAN,$P95,$TPS,$CONC,$(stat -c%s "$CRT")" >> "$OUTCSV"
    echo "==> $cert / $grp : median=${MED}ms p95=${P95}ms throughput=${TPS} hs/s (conc=$CONC)"
  done
  kill "$SRV_PID" 2>/dev/null; wait 2>/dev/null; SRV_PID=""
done
echo "DONE. CSV: $OUTCSV"
