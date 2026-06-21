#!/bin/bash
# =============================================================================
# measure_memory.sh - peak memory (RSS) per algorithm, via GNU `time -v`.
#
# Usage:
#   bash scripts/measure_memory.sh
#
# Output: data/memory_<arch>.csv  (algo,peak_rss_kb)
# =============================================================================

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
# shellcheck disable=SC1091
[ -f "$HERE/setenv.sh" ] && source "$HERE/setenv.sh" >/dev/null 2>&1 || true

BENCH="$ROOT/build/bench_evp"
TIME_BIN="/usr/bin/time"           # GNU time, NOT the shell builtin
ARCH="$(uname -m)"
OUT="$ROOT/data/memory_${ARCH}.csv"
mkdir -p "$ROOT/data"

[ -x "$BENCH" ]    || { echo "Not found: $BENCH (run 'make' first)"; exit 1; }
[ -x "$TIME_BIN" ] || { echo "Need GNU time at $TIME_BIN (sudo apt-get install time)"; exit 1; }

# 15-algorithm matrix (same as the latency table): rsa 3072/7680/15360,
# ecdsa/ecdh p256/384/521, mlkem 512/768/1024, mldsa 44/65/87.
ALGOS=( "rsa 3072" "rsa 7680" "rsa 15360"
        "ecdsa p256" "ecdsa p384" "ecdsa p521"
        "ecdh p256" "ecdh p384" "ecdh p521"
        "mlkem 512" "mlkem 768" "mlkem 1024"
        "mldsa 44" "mldsa 65" "mldsa 87" )

# One keygen already touches the peak. Keep keygen iters at 1: RSA-15360 keygen
# is tens of seconds to minutes each, and more would not change peak RSS.
export BENCH_ITERS=20 BENCH_KEYGEN_ITERS=1 BENCH_WARMUP=3

echo "algo,peak_rss_kb" > "$OUT"

for entry in "${ALGOS[@]}"; do
  set -- $entry
  fam="$1"
  param="$2"
  log="$(mktemp)"
  if "$TIME_BIN" -v "$BENCH" "$fam" "$param" >/dev/null 2>"$log"; then
    rss="$(awk -F': ' '/Maximum resident set size/{print $2}' "$log")"
    echo "${fam}-${param},${rss}" >> "$OUT"
    echo "==> ${fam} ${param}: ${rss} KB"
  else
    echo "==> ${fam} ${param}  (skip)"
  fi
  rm -f "$log"
done

echo "Memory CSV: $OUT"