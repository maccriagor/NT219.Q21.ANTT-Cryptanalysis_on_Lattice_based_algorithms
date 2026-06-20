#!/usr/bin/env bash
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"
# Kich hoat OpenSSL PQC cho phien (run_micro.sh cung tu source; lam day cho chac).
# shellcheck disable=SC1091
[ -f scripts/setenv.sh ] && source scripts/setenv.sh >/dev/null 2>&1 || true

# Ghim xung CPU (neu co). Khong co sudo/cpupower -> bo qua, vo hai.
sudo cpupower frequency-set -g performance 2>/dev/null || true

ARCH="$(uname -m)"
BATCHES="${BATCHES:-5}"
FAST="rsa 3072;ecdsa p256;ecdsa p384;ecdsa p521;ecdh p256;ecdh p384;ecdh p521;mlkem 512;mlkem 768;mlkem 1024;mldsa 44;mldsa 65;mldsa 87"
SLOW="rsa 7680;rsa 15360"

mkdir -p "data/raw/${ARCH}"
for i in $(seq 1 "$BATCHES"); do
  echo "===== BATCH $i / $BATCHES  ($ARCH) ====="

  # --- Luot 1: 13 thuat toan con lai -- 2000 vong ---
  MICRO_ALGOS="$FAST" BENCH_WARMUP=50 BENCH_ITERS=2000 BENCH_KEYGEN_ITERS=1000 make bench
  cp "data/summary_micro_${ARCH}.csv" "data/raw/${ARCH}/summary_batch$i.csv"

  # --- Luot 2: rsa 7680 + rsa 15360 -- 100 vong ops, keygen=3 ---
  MICRO_ALGOS="$SLOW" BENCH_WARMUP=10 BENCH_ITERS=100 BENCH_KEYGEN_ITERS=3 make bench
  tail -n +2 "data/summary_micro_${ARCH}.csv" >> "data/raw/${ARCH}/summary_batch$i.csv"
  cp "data/raw/${ARCH}/summary_batch$i.csv" "data/summary_micro_${ARCH}.csv"

  # --- Archive raw+kv tung thuat toan (du 15 o top-level sau 2 luot) ---
  for f in data/raw/${ARCH}/*.raw.csv; do
    [ -e "$f" ] || continue
    a=$(basename "$f" .raw.csv); mkdir -p "data/raw/${ARCH}/$a"
    mv "$f" "data/raw/${ARCH}/$a/batch$i.raw.csv"
    [ -e "data/raw/${ARCH}/$a.kv.txt" ] && mv "data/raw/${ARCH}/$a.kv.txt" "data/raw/${ARCH}/$a/batch$i.kv.txt"
  done

  # --- ARM (Pi 4): nghi nguoi chong throttle nhiet; x86 bo qua ---
  if [ "$ARCH" = "aarch64" ]; then
    cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null   # 70000 = 70.0C
    [ "$i" -lt "$BATCHES" ] && sleep 120
  fi
done
echo "DONE. $BATCHES batch -> data/raw/${ARCH}/summary_batch*.csv (+ per-algo). Tiep theo: make analyze"