#!/usr/bin/env bash
# =============================================================================
# bench_tls_nginx.sh - WP4 macrobenchmark against a REAL web server (nginx),
# same measurement method as bench_tls.sh for comparability, plus the axis
# the brief asks for in 7.4: "single-threaded and multi-threaded servers"
# (worker_processes 1 vs auto).
#
# Matrix = workers (1, auto) x cert (rsa2048, ecp256, mldsa65)
#                            x group (X25519, X25519MLKEM768, MLKEM768)
# Latency  : TLS_ITERS sequential FULL handshakes (resumption disabled in the
#            nginx config) -> median/mean/p95.
# Throughput: TLS_CONC parallel s_client workers for TLS_DUR seconds.
#
# Output: data/tls_handshake_nginx-<arch>.csv  (same columns as bench_tls.sh
#         + server,workers; analyze.py picks it up automatically).
# Requires: build_nginx.sh + gen_tls_certs.sh done.
# Env knobs: TLS_ITERS=50 TLS_WARMUP=5 TLS_CONC=4 TLS_DUR=10 PORT=8443
# =============================================================================
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
source "$HERE/versions.env"
# shellcheck disable=SC1091
source "$HERE/setenv.sh" >/dev/null   # exports LD_LIBRARY_PATH -> nginx finds our libssl

NGX="$NGINX_PREFIX/sbin/nginx"
[ -x "$NGX" ] || { echo "ERROR: nginx missing: run scripts/build_nginx.sh"; exit 1; }
TLSDIR="$HOME/pqc/tls"
[ -f "$TLSDIR/mldsa65.cert.pem" ] || { echo "ERROR: certs missing: run scripts/gen_tls_certs.sh"; exit 1; }

PORT="${PORT:-8443}"; ITERS="${TLS_ITERS:-50}"; WARM="${TLS_WARMUP:-5}"
CONC="${TLS_CONC:-4}"; DUR="${TLS_DUR:-10}"
CERTS="${CERTS:-rsa2048 ecp256 mldsa65}"
GROUP_LIST="${GROUP_LIST:-X25519 X25519MLKEM768 MLKEM768}"
WORKERS_LIST="${WORKERS_LIST:-1 auto}"
LISTEN="${LISTEN:-127.0.0.1}"           # bind address; set LISTEN=0.0.0.0 for LAN (see KICH_BAN §5.9)
SERVER_GROUPS="X25519:X25519MLKEM768:MLKEM768"
ARCH="$(uname -m)"
OUTCSV="$ROOT/data/tls_handshake_nginx-${ARCH}.csv"
mkdir -p "$ROOT/data"
echo "arch,cert,group,hs_median_ms,hs_mean_ms,hs_p95_ms,throughput_hs_s,concurrency,cert_bytes,server,workers" > "$OUTCSV"

RUN="$(mktemp -d)"; mkdir -p "$RUN/logs" "$RUN/html"; echo ok > "$RUN/html/index.html"
NPID=""
cleanup() { [ -n "$NPID" ] && kill "$NPID" 2>/dev/null; wait 2>/dev/null; rm -rf "$RUN"; }
trap cleanup EXIT

one_handshake() { # $1=group $2=cafile
  openssl s_client -connect "127.0.0.1:$PORT" -tls1_3 -groups "$1" \
      -CAfile "$2" -verify_return_error </dev/null >/dev/null 2>&1
}

for workers in $WORKERS_LIST; do
  for cert in $CERTS; do
    CRT="$TLSDIR/$cert.cert.pem"; KEY="$TLSDIR/$cert.key.pem"
    [ -f "$CRT" ] || { echo "skip cert $cert (missing)"; continue; }
    sed -e "s|__WORKERS__|$workers|" -e "s|__PORT__|$PORT|" \
        -e "s|__LISTEN__|$LISTEN|" \
        -e "s|__CRT__|$CRT|" -e "s|__KEY__|$KEY|" \
        -e "s|__GROUPS__|$SERVER_GROUPS|" \
        "$HERE/nginx_bench.conf" > "$RUN/bench.conf"
    "$NGX" -p "$RUN" -c bench.conf >/dev/null 2>&1 &
    NPID=$!
    for _ in $(seq 1 50); do one_handshake "X25519" "$CRT" && break; sleep 0.1; done

    for grp in $GROUP_LIST; do
      if ! one_handshake "$grp" "$CRT"; then
        echo "==> w=$workers $cert/$grp : FAILED - recorded NA"
        echo "$ARCH,$cert,$grp,NA,NA,NA,NA,$CONC,$(stat -c%s "$CRT"),nginx,$workers" >> "$OUTCSV"
        continue
      fi
      for _ in $(seq 1 "$WARM"); do one_handshake "$grp" "$CRT"; done
      TMP="$(mktemp)"
      for _ in $(seq 1 "$ITERS"); do
        t0=$(date +%s%N); one_handshake "$grp" "$CRT"; t1=$(date +%s%N)
        echo $(( (t1 - t0) / 1000 )) >> "$TMP"
      done
      read -r MED MEAN P95 <<< "$(sort -n "$TMP" | awk '
        { a[NR]=$1; s+=$1 }
        END { m=(NR%2)?a[(NR+1)/2]:(a[NR/2]+a[NR/2+1])/2;
              p=a[int(0.95*NR)>0?int(0.95*NR):1];
              printf "%.3f %.3f %.3f", m/1000, s/NR/1000, p/1000 }')"
      rm -f "$TMP"
      CNTDIR="$(mktemp -d)"; WPIDS=""
      for w in $(seq 1 "$CONC"); do
        ( n=0; end=$(( $(date +%s) + DUR ))
          while [ "$(date +%s)" -lt "$end" ]; do one_handshake "$grp" "$CRT" && n=$((n+1)); done
          echo "$n" > "$CNTDIR/$w" ) &
        WPIDS="$WPIDS $!"
      done
      wait $WPIDS          # wait ONLY for the workers, not the background nginx (bare 'wait' would hang on it)
      TOTAL=$(cat "$CNTDIR"/* | awk '{s+=$1} END{print s}')
      rm -rf "$CNTDIR"
      TPS=$(awk -v t="$TOTAL" -v d="$DUR" 'BEGIN{printf "%.1f", t/d}')
      echo "$ARCH,$cert,$grp,$MED,$MEAN,$P95,$TPS,$CONC,$(stat -c%s "$CRT"),nginx,$workers" >> "$OUTCSV"
      echo "==> w=$workers $cert/$grp : median=${MED}ms p95=${P95}ms tput=${TPS} hs/s"
    done
    kill "$NPID" 2>/dev/null; wait 2>/dev/null; NPID=""
  done
done
echo "DONE. CSV: $OUTCSV"
