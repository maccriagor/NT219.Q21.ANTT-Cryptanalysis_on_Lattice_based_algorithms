#!/usr/bin/env bash
# =============================================================================
# tls13-scratch/bench_netem.sh  --  Track-D: do TLS 1.3 handshake qua RTT/loss
# gia lap (mo phong xvzcf/pq-tls-benchmark "emulation-exp").
#
# Goi tu Makefile cha :   make tlsnetem        # == cd tls13-scratch && ./bench_netem.sh
# Hoac truc tiep      :   sudo ./bench_netem.sh
#                         sudo ITERS=1000 WARMUP=100 ./bench_netem.sh
#                         sudo DELAYS="2.5ms 15ms 50ms" LOSSES="0 1 3" ./bench_netem.sh
#                         sudo TLS_GROUP=X25519MLKEM768 ./bench_netem.sh   # bat nhom lai
#
# Cach lam:
#   - 2 network namespace srv_ns / cli_ns noi bang veth (setup_ns.sh): 10.0.0.1 / 10.0.0.2
#   - tc netem them delay (+loss) o CA HAI dau  =>  RTT ~= 2 x delay (nua RTT moi phia)
#   - doc lai RTT THAT bang ping (khong ghi con so ly thuyet)
#   - server phuc vu dung WARMUP+ITERS handshake roi thoat; client --bench do tung phase
#   - gom dong "^SUMMARY" cua client  ->  data/netem_<arch>.csv  (cho `make analyze`)
#
# Yeu cau: chay bang ROOT (ip netns + tc); kernel co module sch_netem; iproute2,
#          ethtool, ping; OpenSSL cua repo da build (server/client link libcrypto.a).
# Chi tiet phuong phap + caveat: xem README_netem.md (cung thu muc).
# Nguon: xvzcf/pq-tls-benchmark ; Paquin-Stebila-Tamvada (eprint 2019/1447).
# =============================================================================
set -u

# ---- vi tri & hang so (PHAI trung ten dat trong setup_ns.sh) ----------------
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
ARCH="$(uname -m)"

SRV_NS=srv_ns ; SRV_VE=srv_ve ; SRV_IP=10.0.0.1
CLI_NS=cli_ns ; CLI_VE=cli_ve

# ---- tham so (env ghi de duoc -- "parameter law" cua repo) ------------------
PORT="${PORT:-8400}"                         # cong server (client mac dinh 8400)
ITERS="${ITERS:-1000}"                        # so handshake DO moi diem
WARMUP="${WARMUP:-100}"                        # so handshake lam nong (bi loai)
DELAYS="${DELAYS:-0ms 2.5ms 15ms 39ms}"       # delay NUA RTT moi phia -> RTT ~ 0/5/30/78 ms
LOSSES="${LOSSES:-0}"                          # % mat goi; >=20% co the lam TCP reset (xem caveat)
PING_N="${PING_N:-15}"                         # so goi ping de do RTT that
TLS_GROUP_ENV="${TLS_GROUP:-}"                # dat "X25519MLKEM768" de bat nhom lai (mac dinh X25519)

SRVDIR="$HERE/server" ;  CLIDIR="$HERE/client"
DATA="$ROOT/data" ;  RAW="$ROOT/data/raw/$ARCH"
SUMCSV="$DATA/netem_${ARCH}.csv"

die(){ echo "[bench_netem] LOI: $*" >&2; exit 1; }

case "${1:-}" in -h|--help)
  sed -n '2,30p' "${BASH_SOURCE[0]}"; exit 0 ;;
esac

# ---- kiem tra dieu kien -----------------------------------------------------
[ "$(id -u)" = 0 ] || die "can chay bang ROOT (ip netns + tc). Dung: sudo make tlsnetem"
command -v tc   >/dev/null 2>&1 || die "thieu 'tc' (apt install iproute2)"
command -v ping >/dev/null 2>&1 || die "thieu 'ping' (apt install iputils-ping)"
command -v ethtool >/dev/null 2>&1 || die "thieu 'ethtool' (apt install ethtool)"
modprobe sch_netem 2>/dev/null || true
mkdir -p "$DATA" "$RAW"

# ---- build server + client + cert (idempotent) ------------------------------
echo "[bench_netem] build server + client ..."
make -C "$SRVDIR" >/dev/null || die "build server that bai -- da build OpenSSL cua repo chua? (scripts/build_openssl.sh)"
make -C "$CLIDIR" >/dev/null || die "build client that bai"
if [ ! -s "$SRVDIR/server.crt" ] || [ ! -s "$SRVDIR/server.key" ]; then
  echo "[bench_netem] sinh server.crt / server.key (EC P-256) ..."
  make -C "$SRVDIR" cert >/dev/null || die "sinh cert that bai"
fi

# ---- dung / go namespace ----------------------------------------------------
cleanup(){ bash "$HERE/teardown_ns.sh" >/dev/null 2>&1 || true; }
trap cleanup EXIT
cleanup                                        # don sach lan chay truoc (neu con sot)
echo "[bench_netem] dung 2 namespace srv_ns/cli_ns + veth ..."
bash "$HERE/setup_ns.sh" >/dev/null 2>&1 || die "setup_ns.sh that bai (ip netns / veth?)"

# kiem tra netem co thuc su gan duoc (mot so kernel/sandbox thieu sch_netem)
ip netns exec "$SRV_NS" tc qdisc change dev "$SRV_VE" root netem delay 0ms 2>/dev/null \
  || die "kernel khong co sch_netem ('qdisc kind is unknown'). Cai linux-modules-extra hoac nap module."

TOTAL=$(( WARMUP + ITERS ))
echo "[bench_netem] arch=$ARCH port=$PORT iters=$ITERS warmup=$WARMUP total=$TOTAL group=${TLS_GROUP_ENV:-X25519}"
echo "rtt_ms,loss_pct,ok,median_us,p95_us,mean_us,ci95_us" > "$SUMCSV"

# ---- quet (delay x loss) ----------------------------------------------------
for D in $DELAYS; do
  dnum="${D%ms}"
  nomrtt="$(awk "BEGIN{printf \"%g\", 2*$dnum}")"        # RTT danh nghia = 2 x delay (cho ten file + fallback)
  for L in $LOSSES; do
    if [ "$L" = "0" ]; then loss_arg=""; else loss_arg="loss ${L}%"; fi
    echo "[bench_netem] --- delay/phia=$D  (RTT~${nomrtt}ms)  loss=${L}% ---"

    # ap netem o CA HAI dau (nua RTT moi phia)
    # shellcheck disable=SC2086
    ip netns exec "$SRV_NS" tc qdisc change dev "$SRV_VE" root netem delay "$D" $loss_arg
    # shellcheck disable=SC2086
    ip netns exec "$CLI_NS" tc qdisc change dev "$CLI_VE" root netem delay "$D" $loss_arg

    # doc RTT THAT bang ping (cung lam nong duong truyen)
    rtt="$(ip netns exec "$CLI_NS" ping -c "$PING_N" -i 0.2 -W 1 -q "$SRV_IP" 2>/dev/null \
           | sed -n 's#.*= [0-9.]*/\([0-9.]*\)/.*#\1#p')"
    [ -n "$rtt" ] || rtt="$nomrtt"             # ping fail (loss cao) -> dung danh nghia

    # server phuc vu dung TOTAL handshake roi tu thoat (chay nen trong srv_ns)
    srvlog="$(mktemp)"
    ip netns exec "$SRV_NS" env TLS_TOTAL="$TOTAL" \
        bash -c "cd '$SRVDIR' && exec ./server $PORT $TOTAL" >/dev/null 2>"$srvlog" &
    srvpid=$!
    for _ in $(seq 1 100); do grep -q listening "$srvlog" 2>/dev/null && break; sleep 0.05; done

    # client --bench trong cli_ns; gan nhan netem; raw CSV rieng moi diem
    rawcsv="$RAW/netem_rtt${nomrtt}ms_loss${L}.csv"
    clientout="$(
      ip netns exec "$CLI_NS" env \
        NETEM_RTT="$rtt" NETEM_LOSS="$L" \
        BENCH_ITERS="$ITERS" BENCH_WARMUP="$WARMUP" BENCH_CSV="$rawcsv" \
        ${TLS_GROUP_ENV:+TLS_GROUP="$TLS_GROUP_ENV"} \
        bash -c "cd '$CLIDIR' && exec ./client $SRV_IP $PORT --bench" 2>/dev/null )"

    wait "$srvpid" 2>/dev/null || true
    rm -f "$srvlog"

    # gom dong SUMMARY -> mot dong CSV tong hop
    sline="$(printf '%s\n' "$clientout" | grep -m1 '^SUMMARY')"
    if [ -n "$sline" ]; then
      printf '%s\n' "$sline" | sed -E \
        's/^SUMMARY rtt_ms=([^ ]*) loss_pct=([^ ]*) ok=([^ ]*) total_median_us=([^ ]*) total_p95_us=([^ ]*) total_mean_us=([^ ]*) total_ci95_us=([^ ]*).*/\1,\2,\3,\4,\5,\6,\7/' \
        >> "$SUMCSV"
      echo "[bench_netem]   $sline"
    else
      echo "[bench_netem]   CANH BAO: khong co dong SUMMARY (handshake loi het? loss qua cao?)" >&2
      echo "${rtt},${L},0,,,," >> "$SUMCSV"
    fi
  done
done

echo "[bench_netem] XONG."
echo "  tong hop : $SUMCSV"
echo "             (cot: rtt_ms,loss_pct,ok,median_us,p95_us,mean_us,ci95_us)"
echo "  raw/diem : $RAW/netem_rtt*ms_loss*.csv"
echo "  -> chay 'make analyze' de ra bang + chart analysis_out/netem_${ARCH}.png"
