#!/bin/bash
# =============================================================================
# run_oqs.sh - cross-check runner. Drives build/bench_oqs_{ref,opt} over
# the PQC matrix and parses their EVP-style key-value output into ONE tidy CSV.
# analyze.py turns it into an "EVP vs liboqs ref/opt" comparison table:
#   - EVP path  = bench_evp (summary)             -> real application path
#   - liboqs ref = bench_oqs_ref (SIMD off)       -> portable C
#   - liboqs opt = bench_oqs_opt (NEON/AVX2 on)   -> the optimization
#
# Output:  data/bench_oqs_<arch>.csv
#          (arch,variant,algo,op,wall_median_ns,wall_p95_ns,cyc_median)
#   NOTE: column 'cyc_median' carries bench_oqs's '<op>-ctr-median' value,
#         i.e. the TSC counter ("raw HW counter, not retired core cycles").
# Requires: make bench_oqs   (build/bench_oqs_ref + build/bench_oqs_opt).
# Env (same names as bench_evp; override freely):
#   BENCH_ITERS (2000) BENCH_KEYGEN_ITERS (200) BENCH_WARMUP (20)
#   OQS_ALGOS ("mlkem 512;...;mldsa 87")   restrict the matrix
# =============================================================================
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
# bench_oqs links liboqs.a + libcrypto.a statically; setenv is sourced only for env parity.
# shellcheck disable=SC1091
[ -f "$HERE/setenv.sh" ] && source "$HERE/setenv.sh" >/dev/null 2>&1 || true
ARCH="$(uname -m)"
OUT="$ROOT/data/bench_oqs_${ARCH}.csv"
mkdir -p "$ROOT/data"
ALGOS="${OQS_ALGOS:-mlkem 512;mlkem 768;mlkem 1024;mldsa 44;mldsa 65;mldsa 87}"
ITERS="${BENCH_ITERS:-2000}"; KITERS="${BENCH_KEYGEN_ITERS:-200}"; WARM="${BENCH_WARMUP:-20}"
# Parse one bench_oqs run (stdin) -> rows "arch,variant,algo,op,median,p95,cyc".
# The counter line bench_oqs prints is '<op>-ctr-median' (TSC), NOT '-cyc-median':
# that line is what populates ops[], so getting its name wrong yields ZERO rows.
parse() {  # $1 = variant
  awk -F': ' -v A="$ARCH" -v V="$1" '
    /^algo: /                 { algo=$2 }
    /-wall-median-ns: /       { split($1,f,"-"); med[f[1]]=$2 }
    /-wall-p95-ns: /          { split($1,f,"-"); p95[f[1]]=$2 }
    /-ctr-median: /           { split($1,f,"-"); cyc[f[1]]=$2; ops[f[1]]=1 }
    END { for (o in ops) printf "%s,%s,%s,%s,%s,%s,%s\n",
                  A, V, algo, o, med[o], p95[o], cyc[o] }
  '
}
echo "arch,variant,algo,op,wall_median_ns,wall_p95_ns,cyc_median" > "$OUT"
IFS=';' read -r -a LIST <<< "$ALGOS"
for variant in ref opt; do
  BIN="$ROOT/build/bench_oqs_$variant"
  if [ ! -x "$BIN" ]; then
    echo "skip variant '$variant': $BIN missing (run 'make bench_oqs')"; continue
  fi
  for entry in "${LIST[@]}"; do
    # shellcheck disable=SC2086
    set -- $entry
    # Run bench_oqs, funnel stdout through parse(), and COUNT the rows produced.
    # Do NOT trust the pipeline's exit status: awk exits 0 even on empty input,
    # so the old 'if ... (ok)' reported success while writing nothing. Counting
    # the parsed rows is the only honest signal that this algo actually landed.
    rows="$(BENCH_ITERS="$ITERS" BENCH_KEYGEN_ITERS="$KITERS" BENCH_WARMUP="$WARM" \
              "$BIN" "$1" "$2" 2>/dev/null | parse "$variant")"
    n="$(printf '%s\n' "$rows" | grep -c . || true)"
    if [ "$n" -gt 0 ]; then
      printf '%s\n' "$rows" >> "$OUT"
      echo "==> $variant $1 $2 (ok: $n rows)"
    else
      echo "==> $variant $1 $2 (SKIP: 0 rows - bench_oqs printed nothing or format drifted; run it directly to see)"
    fi
  done
done
echo "DONE. CSV: $OUT  (next: make analyze -> 'EVP vs liboqs ref/opt' table)"
