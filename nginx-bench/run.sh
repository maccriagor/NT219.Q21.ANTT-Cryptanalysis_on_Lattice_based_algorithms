#!/bin/bash
# run.sh - benchmark TLS 1.3 handshakes against a LOCAL nginx, direct (no emulation).
#
# Method: start our PQC nginx on loopback, then a client-side handshake timer
# (client.c) measures SSL_connect wall-time per group. Same shape as the OQS
# nginx demo and Paquin-Stebila-Tamvada, but WITHOUT network namespaces or tc
# netem: this isolates the pure crypto + stack cost of the handshake (no
# synthetic RTT/loss). Sweep = cert (server signature) x group (KEM).
#
# Usage:
#   bash nginx-bench/run.sh
#   ITERS=2000 CERTS="ecp384 mldsa65" KEX_GROUPS="X25519MLKEM768 MLKEM768" bash nginx-bench/run.sh
#
# Requires: scripts/build_nginx.sh + scripts/gen_tls_certs.sh done first.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
source "$ROOT/scripts/setenv.sh" >/dev/null   # OSSL_PREFIX, NGINX_PREFIX, LD_LIBRARY_PATH

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-4433}"
ITERS="${ITERS:-1000}"
CERTS="${CERTS:-rsa7680 ecp384 mldsa65}"            # NIST Cat 3 signatures
KEX_GROUPS="${KEX_GROUPS:-X25519 X25519MLKEM768 MLKEM768}"  # classical / hybrid / pure-PQC KEM
CERTDIR="${TLSDIR:-$HOME/pqc/tls}"
ARCH="$(uname -m)"

NGX="$NGINX_PREFIX/sbin/nginx"
[ -x "$NGX" ] || { echo "ERROR: nginx missing -> run scripts/build_nginx.sh"; exit 1; }

make -C "$HERE" OPENSSL_DIR="$OSSL_PREFIX" client.o >/dev/null

RUNDIR="$ROOT/data/nginx-run"
mkdir -p "$RUNDIR/logs"

write_conf() {   # $1 = host:port already in vars
  cat > "$RUNDIR/nginx.conf" << CONF
worker_processes 1;
pid logs/nginx.pid;
error_log logs/error.log;
events { worker_connections 1024; }
http {
    access_log off;
    server {
        listen $HOST:$PORT ssl;
        ssl_certificate     server.crt;
        ssl_certificate_key server.key;
        ssl_protocols TLSv1.3;
        ssl_ecdh_curve X25519:X25519MLKEM768:MLKEM768;
        ssl_session_cache   off;
        ssl_session_tickets off;
        location / { return 200 "ok\n"; }
    }
}
CONF
}

start_nginx() {   # $1 = cert name
  cp "$CERTDIR/$1.cert.pem" "$RUNDIR/server.crt"
  cp "$CERTDIR/$1.key.pem"  "$RUNDIR/server.key"
  cp "$CERTDIR/$1.cert.pem" "$HERE/ca.crt"        # client trusts this self-signed cert
  write_conf
  "$NGX" -p "$RUNDIR" -c "$RUNDIR/nginx.conf"
  sleep 1
}

stop_nginx() {
  "$NGX" -p "$RUNDIR" -c "$RUNDIR/nginx.conf" -s stop 2>/dev/null || true
  sleep 1
}

# median / mean / p95 from comma-separated ms on stdin
stats() {
  tr ',' '\n' | sort -n | awk '
    { v[NR]=$1; sum+=$1 }
    END {
      n=NR; if (n==0) { printf "NA NA NA\n"; exit }
      med = (n%2) ? v[int((n+1)/2)] : (v[n/2]+v[n/2+1])/2
      p=int(0.95*n); if (p<1) p=1
      printf "%.3f %.3f %.3f\n", med, sum/n, v[p]
    }'
}

OUT="$ROOT/data/nginx_handshake_${ARCH}.csv"
mkdir -p "$ROOT/data"
echo "arch,cert,group,hs_median_ms,hs_mean_ms,hs_p95_ms,iters,cert_bytes" > "$OUT"

trap stop_nginx EXIT
for cert in $CERTS; do
  [ -f "$CERTDIR/$cert.cert.pem" ] || { echo "skip $cert (missing)"; continue; }
  start_nginx "$cert"
  CB="$(stat -c%s "$CERTDIR/$cert.cert.pem")"
  for grp in $KEX_GROUPS; do
    raw="$("$HERE/client.o" "$grp" "$ITERS" "$HOST:$PORT" 2>/dev/null || true)"
    if [ -z "$raw" ]; then
      echo "$ARCH,$cert,$grp,NA,NA,NA,$ITERS,$CB" >> "$OUT"
      echo "==> $cert/$grp : FAILED (NA)"
    else
      read -r med mean p95 < <(printf '%s' "$raw" | stats)
      echo "$ARCH,$cert,$grp,$med,$mean,$p95,$ITERS,$CB" >> "$OUT"
      echo "==> $cert/$grp : median=${med}ms mean=${mean}ms p95=${p95}ms"
    fi
  done
  stop_nginx
done

echo "done -> $OUT"
