#!/usr/bin/env bash
# =============================================================================
# bench_rtt.sh -- ONE netem-RTT harness for BOTH TLS backends.
#
#     sudo ./scripts/bench_rtt.sh scratch     # TLS 1.3 tu viet (tls13-scratch)
#     sudo ./scripts/bench_rtt.sh nginx       # nginx + OpenSSL (hybrid PQC groups)
#
# Hai lan chay DUNG CHUNG: mang (veth pair qua 2 netns), netem delay moi phia
# (RTT ~= 2x; gia tri THAT doc lai bang ping), va cach do handshake
# (CLOCK_MONOTONIC_RAW quanh SSL_connect / quanh cu handshake cua ban tu viet).
# Chi khac DUY NHAT cap (server, client) -> hai backend so sanh truc tiep duoc.
#
# Phuong phap (KHONG di xa cac nguon nay -- xem docs/MAPPING_RTT_netem.md):
#   - netem RTT/loss tren veth pair  : xvzcf/pq-tls-benchmark
#     https://github.com/xvzcf/pq-tls-benchmark
#   - do handshake in-process (s_timer.c) : Paquin-Stebila-Tamvada, eprint 2019/1447
#     https://eprint.iacr.org/2019/1447
#   - tc-netem(8)  https://man7.org/linux/man-pages/man8/tc-netem.8.html
#   - ip-netns(8)  https://man7.org/linux/man-pages/man8/ip-netns.8.html
#   - TLS 1.3 RFC 8446 ; hybrid draft-ietf-tls-hybrid-design ;
#     X25519MLKEM768 (codepoint 0x11EC) draft-kwiatkowski-tls-ecdhe-mlkem
#
# Yeu cau may THAT (sandbox khong co): kernel co sch_netem, iproute2, ethtool,
# (backend nginx:) scripts/build_nginx.sh + gen_tls_certs.sh + make tlsclient,
# OpenSSL >= 3.5 (ML-KEM native) cho cac group hybrid.
# Env knobs: DELAYS ITERS WARMUP PORT GROUPS CERT RES   (mac dinh ben duoi)
# =============================================================================
set -u

BACKEND="${1:-scratch}"
case "$BACKEND" in
  scratch|nginx) ;;
  *) echo "usage: $0 {scratch|nginx}"; exit 2 ;;
esac

# ---- knobs (env override; cung 'parameter law' nhu phan con lai cua repo) ----
DELAYS="${DELAYS:-0ms 2.5ms 15ms 39ms}"     # per-side; RTT ~= 2x (ping = su that)
ITERS="${ITERS:-200}"
WARMUP="${WARMUP:-20}"
PORT="${PORT:-4433}"
GROUPS="${GROUPS:-X25519 X25519MLKEM768}"    # truc nginx; scratch ep ve X25519
CERT="${CERT:-ecp256}"                       # nginx cert: rsa2048 | ecp256 | mldsa65

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
RES="${RES:-$PWD/rtt_${BACKEND}}"; mkdir -p "$RES"
OUT="$RES/summary.csv"

# namespaces / addresses (giong het setup_ns.sh de nhat quan)
SRV_NS=srv_ns; SRV_VE=srv_ve; SRV_MAC=00:00:00:00:00:01; SRV_IP=10.0.0.1
CLI_NS=cli_ns; CLI_VE=cli_ve; CLI_MAC=00:00:00:00:00:02; CLI_IP=10.0.0.2
COUNT=$((WARMUP + ITERS))

[ "$(id -u)" = 0 ] || { echo "run as root (ip netns + tc can root)"; exit 1; }

# ----------------------------- mang (dung chung) -----------------------------
net_down() { ip netns del $SRV_NS 2>/dev/null || true; ip netns del $CLI_NS 2>/dev/null || true; }
net_up() {
  net_down
  ip netns add $SRV_NS; ip netns add $CLI_NS
  ip link add name $SRV_VE address $SRV_MAC netns $SRV_NS type veth \
          peer name $CLI_VE address $CLI_MAC netns $CLI_NS
  ip netns exec $SRV_NS ip addr add $SRV_IP/24 dev $SRV_VE
  ip netns exec $SRV_NS ip link set $SRV_VE up; ip netns exec $SRV_NS ip link set lo up
  ip netns exec $CLI_NS ip addr add $CLI_IP/24 dev $CLI_VE
  ip netns exec $CLI_NS ip link set $CLI_VE up; ip netns exec $CLI_NS ip link set lo up
  # static ARP -> handshake timing khong bi neighbour resolution lam nhieu
  ip netns exec $SRV_NS ip neigh add $CLI_IP lladdr $CLI_MAC dev $SRV_VE
  ip netns exec $CLI_NS ip neigh add $SRV_IP lladdr $SRV_MAC dev $CLI_VE
  # tat offload (gso/gro/tso) -> netem moi phan anh dung
  ip netns exec $SRV_NS ethtool -K $SRV_VE gso off gro off tso off 2>/dev/null || true
  ip netns exec $CLI_NS ethtool -K $CLI_VE gso off gro off tso off 2>/dev/null || true
  # gan netem rong; set_delay() dien delay moi lan chay
  ip netns exec $SRV_NS tc qdisc add dev $SRV_VE root netem
  ip netns exec $CLI_NS tc qdisc add dev $CLI_VE root netem
}
set_delay() {  # $1 = per-side delay
  ip netns exec $SRV_NS tc qdisc change dev $SRV_VE root netem limit 1000 delay "$1" rate 1000mbit
  ip netns exec $CLI_NS tc qdisc change dev $CLI_VE root netem limit 1000 delay "$1" rate 1000mbit
}
real_rtt() {   # avg ms doc tu ping (su that, khong phai 2x ly thuyet)
  ip netns exec $CLI_NS ping $SRV_IP -c 20 -i 0.2 2>/dev/null | tail -1 | cut -d'/' -f5
}

# median + p95 tu mot cot CSV (col 1-based). Dung chung cho ca 2 backend.
stats() {  # $1=file $2=col  -> "median p95 n"
  [ -f "$1" ] || { echo "NA NA 0"; return; }
  awk -F, -v c="$2" 'NR>1 && $c ~ /^[0-9.]+$/ {print $c}' "$1" | sort -n | awk '
    { a[NR]=$1 }
    END { n=NR; if (n==0) { print "NA NA 0"; exit }
          med=(n%2)?a[(n+1)/2]:(a[int(n/2)]+a[int(n/2)+1])/2;
          pi=int(0.95*n+0.5); if (pi<1) pi=1; if (pi>n) pi=n;
          printf "%.2f %.2f %d\n", med, a[pi], n }'
}

# ----------------------------- chuan bi backend ------------------------------
NGX=""; TIMER=""; CRT=""; NGXRUN=""
if [ "$BACKEND" = scratch ]; then
  ( cd "$ROOT/tls13-scratch" && make >/dev/null )
  [ -f "$ROOT/tls13-scratch/server/server.crt" ] || \
     ( cd "$ROOT/tls13-scratch/server" && make cert >/dev/null )
  GROUPS="X25519"                       # ban tu viet la classical-only
else
  # shellcheck disable=SC1091
  source "$HERE/versions.env"; source "$HERE/setenv.sh" >/dev/null   # LD_LIBRARY_PATH -> OpenSSL 3.5+
  NGX="$NGINX_PREFIX/sbin/nginx"; TLSDIR="$HOME/pqc/tls"
  [ -x "$NGX" ] || { echo "ERROR: nginx thieu -> scripts/build_nginx.sh"; exit 1; }
  ( cd "$ROOT" && make tlsclient >/dev/null 2>&1 ) || true
  TIMER="$ROOT/build/tls_timer_client"
  [ -x "$TIMER" ] || { echo "ERROR: tls_timer_client thieu -> make tlsclient"; exit 1; }
  CRT="$TLSDIR/$CERT.cert.pem"; KEY="$TLSDIR/$CERT.key.pem"
  [ -f "$CRT" ] || { echo "ERROR: cert $CERT thieu -> scripts/gen_tls_certs.sh"; exit 1; }
  NGXRUN="$(mktemp -d)"; mkdir -p "$NGXRUN/logs" "$NGXRUN/html"; echo ok > "$NGXRUN/html/index.html"
  sed -e "s|__WORKERS__|1|" -e "s|__PORT__|$PORT|" -e "s|__LISTEN__|$SRV_IP|" \
      -e "s|__CRT__|$CRT|" -e "s|__KEY__|$KEY|" \
      -e "s|__GROUPS__|X25519:X25519MLKEM768:MLKEM768|" \
      "$HERE/nginx_bench.conf" > "$NGXRUN/bench.conf"
fi

# ----------------------------- sweep -----------------------------------------
net_up
trap 'net_down; [ -n "$NGXRUN" ] && rm -rf "$NGXRUN"' EXIT
echo "backend,group,rtt_ms,ok,median_us,p95_us" > "$OUT"
echo "[*] backend=$BACKEND  delays(per-side)=$DELAYS  groups=$GROUPS  iters=$ITERS"

for d in $DELAYS; do
  set_delay "$d"
  rtt="$(real_rtt)"; tag="$(echo "$rtt" | tr '.' p)"
  for g in $GROUPS; do
    raw="$RES/raw_${g}_${tag}ms.csv"
    if [ "$BACKEND" = scratch ]; then
      ( cd "$ROOT/tls13-scratch/server" && ip netns exec $SRV_NS ./server "$PORT" "$COUNT" >/dev/null 2>&1 ) &
      srv=$!; sleep 0.5
      ip netns exec $CLI_NS env BENCH_WARMUP="$WARMUP" NETEM_RTT="$rtt" NETEM_LOSS=0 \
        "$ROOT/tls13-scratch/client/client" "$SRV_IP" "$PORT" --bench "$ITERS" --csv "$raw" >/dev/null 2>&1 || true
      wait "$srv" 2>/dev/null || true
      # client.c cua repo nay da MERGE netem: header = rtt_ms,loss_pct,iter,total_us,... -> total_us = cot 4
      read -r med p95 n < <(stats "$raw" 4)
    else
      ip netns exec $SRV_NS "$NGX" -p "$NGXRUN" -c bench.conf >/dev/null 2>&1 &
      srv=$!; sleep 0.5
      ip netns exec $CLI_NS "$TIMER" "$SRV_IP" "$PORT" "$g" "$WARMUP" "$CRT" >/dev/null 2>&1 || true   # warmup
      ip netns exec $CLI_NS "$TIMER" "$SRV_IP" "$PORT" "$g" "$ITERS"  "$CRT" > "$raw" 2>/dev/null || true
      kill "$srv" 2>/dev/null; wait "$srv" 2>/dev/null || true
      read -r med p95 n < <(stats "$raw" 3)      # connect_us = cot 3
    fi
    echo "$BACKEND,$g,$rtt,${n:-0},${med:-NA},${p95:-NA}" | tee -a "$OUT"
  done
done
echo "[OK] DONE -> $OUT"
