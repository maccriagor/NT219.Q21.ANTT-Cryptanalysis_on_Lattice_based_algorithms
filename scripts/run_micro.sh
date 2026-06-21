#!/bin/bash
# =============================================================================
# run_micro.sh - Run the EVP microbenchmark across the full algorithm matrix.
#
# Output:
#   data/raw/<arch>/<algo>.raw.csv   raw samples (from BENCH_CSV)
#   data/raw/<arch>/<algo>.kv.txt    key-value summary (bench stdout)
#   data/summary_micro_<arch>.csv    long format: algo,metric,value
# =============================================================================

set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
# Activate the custom OpenSSL (with PQC) if available; harmless otherwise.
# shellcheck disable=SC1091
[ -f "$HERE/setenv.sh" ] && source "$HERE/setenv.sh" >/dev/null 2>&1 || true

BENCH="$ROOT/build/bench_evp"
ARCH="$(uname -m)"
RAWDIR="$ROOT/data/raw/$ARCH"
SUMMARY="$ROOT/data/summary_micro_${ARCH}.csv"
mkdir -p "$RAWDIR"

# Algorithm matrix: "family param". 
# This 15-algo set is the DEFAULT. 
# Limit the algorithms by setting the MICRO_ALGOS environment variable, e.g.  MICRO_ALGOS="mlkem 768;mldsa 65;rsa 3072" make bench
if [ -n "${MICRO_ALGOS:-}" ]; then
  IFS=';' read -r -a ALGOS <<< "$MICRO_ALGOS"
else
  ALGOS=(
    "rsa 3072" "rsa 7680" "rsa 15360"
    "ecdsa p256" "ecdsa p384" "ecdsa p521"
    "ecdh p256" "ecdh p384" "ecdh p521"
    "mlkem 512" "mlkem 768" "mlkem 1024"
    "mldsa 44" "mldsa 65" "mldsa 87"
  )
fi

[ -x "$BENCH" ] || { echo "Not found: $BENCH (run 'make' first)"; exit 1; }
echo "algo,metric,value" > "$SUMMARY"

for entry in "${ALGOS[@]}"; do
  set -- $entry
  fam="$1"; param="$2"
  tag="${fam}_${param}"
  raw="$RAWDIR/${tag}.raw.csv"
  kv="$RAWDIR/${tag}.kv.txt"
  if BENCH_CSV="$raw" "$BENCH" "$fam" "$param" > "$kv" 2>/dev/null; then
    algo="$(awk -F': ' '/^algo: /{print $2; exit}' "$kv")"
    [ -z "$algo" ] && algo="$tag"
    # Turn each "key: value" line into "algo,key,value" (skip openssl:/algo:).
    awk -F': ' -v a="$algo" '
      /^openssl: /||/^algo: /{next}
      /^[a-z][a-z0-9-]*: /{v=$2; gsub(/,/,"",v); print a "," $1 "," v}
    ' "$kv" >> "$SUMMARY"
    echo "==> $fam $param  (ok)"
  else
    echo "==> $fam $param  (skip: not available with this OpenSSL)"
  fi
done
echo "Raw    : $RAWDIR/"
echo "Summary: $SUMMARY"
