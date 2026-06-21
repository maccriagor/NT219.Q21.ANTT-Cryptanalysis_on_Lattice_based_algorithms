#!/usr/bin/env bash
# =============================================================================
# measure_memory.sh - WP5 peak memory (RSS). Wraps each run with GNU time -v
# and reads "Maximum resident set size" (KB). Output: data/memory_<arch>.csv
# Requires GNU time at /usr/bin/time (Ubuntu: sudo apt-get install time).
# =============================================================================
# DESIGN NOTES
#   - Metric = peak RSS ("Maximum resident set size", GNU time -v), the
#     exact tool the brief 7.4 names: "peak RSS via /usr/bin/time -v".
#   - /usr/bin/time is GNU time; the shell BUILTIN `time` cannot report RSS.
#   - Short runs on purpose: footprint is about the high-water mark, not
#     throughput - a few iterations already touch the peak.
#   - Same 15-algorithm matrix as run_micro.sh for table symmetry.
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

[ -x "$BENCH" ]     || { echo "Not found: $BENCH (run 'make' first)"; exit 1; }
[ -x "$TIME_BIN" ]  || { echo "Need GNU time at $TIME_BIN (sudo apt-get install time)"; exit 1; }

# 15-algo matrix, identical to run_micro.sh (brief 7.1) so the WP5 RSS table
# lines up 1:1 with the WP2 latency table (no rsa-2048; adds rsa-7680/15360 + p521).
ALGOS=( "rsa 3072" "rsa 7680" "rsa 15360"
        "ecdsa p256" "ecdsa p384" "ecdsa p521"
        "ecdh p256" "ecdh p384" "ecdh p521"
        "mlkem 512" "mlkem 768" "mlkem 1024"
        "mldsa 44" "mldsa 65" "mldsa 87" )

# Peak RSS is a high-water mark -> ONE keygen already touches the peak. Keep
# keygen iters at 1: RSA-15360 keygen is ~30 s-several min EACH, so anything more
# only lengthens the pass with no change to peak RSS.
export BENCH_ITERS=20 BENCH_KEYGEN_ITERS=1 BENCH_WARMUP=3
echo "algo,peak_rss_kb" > "$OUT"

for entry in "${ALGOS[@]}"; do
  set -- $entry
  fam="$1"; param="$2"
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