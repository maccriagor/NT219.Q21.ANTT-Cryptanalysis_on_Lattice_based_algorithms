#!/usr/bin/env bash
# =============================================================================
# run_liboqs_speed.sh - WP3 runner: drive liboqs speed_kem / speed_sig over
# BOTH trees (ref, opt) and the 6-scheme matrix, saving raw output AND a tidy
# CSV (brief 11 "Artifacts: raw CSVs"; 7.5 paired comparison ref<->opt on the
# same machine = the NEON/AVX2 experiment).
#
# Self-adapting: a tree that is not built is skipped with a notice, so on x86
# with only 'ref' it still produces ref rows; on ARM run build_liboqs.sh for
# BOTH trees first (that is the RQ2 experiment).
#
# Output:
#   data/raw/<arch>/liboqs_<variant>_<algo>.txt   (full tool output, evidence)
#   data/liboqs_speed_<arch>.csv                  (arch,variant,algo,op,...)
# Parser verified against a real speed_kem run (table columns:
#   op | iterations | total s | mean us | stdev | cycles mean | cycles stdev).
# =============================================================================
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
source "$HERE/versions.env"

ARCH="$(uname -m)"
RAW="$ROOT/data/raw/$ARCH"
CSV="$ROOT/data/liboqs_speed_${ARCH}.csv"
mkdir -p "$RAW"
KEMS="${KEMS:-ML-KEM-512 ML-KEM-768 ML-KEM-1024}"
SIGS="${SIGS:-ML-DSA-44 ML-DSA-65 ML-DSA-87}"

echo "arch,variant,algo,op,iterations,mean_us,stdev_us,cycles_mean" > "$CSV"

parse_into_csv() { # $1=variant $2=algo $3=rawfile  (append rows to CSV)
  awk -F'|' -v A="$ARCH" -v V="$1" -v G="$2" '
    /\|/ {
      line=$0; gsub(/ /,"",line); n=split(line,f,"|")
      if (n>=7 && f[1]!="Operation" && f[2]+0>0)
        printf "%s,%s,%s,%s,%s,%s,%s,%s\n",A,V,G,f[1],f[2],f[4],f[5],f[6]
    }' "$3" >> "$CSV"
}

for variant in ref opt; do
  TESTS="$SRC_ROOT/liboqs/build-$variant/tests"
  if [ ! -x "$TESTS/speed_kem" ]; then
    echo "skip variant '$variant' (not built: run scripts/build_liboqs.sh $variant)"; continue
  fi
  for alg in $KEMS; do
    out="$RAW/liboqs_${variant}_${alg}.txt"
    "$TESTS/speed_kem" "$alg" | tee "$out" >/dev/null \
      && parse_into_csv "$variant" "$alg" "$out" \
      && echo "==> $variant $alg (speed_kem) saved"
  done
  for alg in $SIGS; do
    out="$RAW/liboqs_${variant}_${alg}.txt"
    "$TESTS/speed_sig" "$alg" | tee "$out" >/dev/null \
      && parse_into_csv "$variant" "$alg" "$out" \
      && echo "==> $variant $alg (speed_sig) saved"
  done
done
echo "DONE. CSV: $CSV  (raw evidence in $RAW)"
