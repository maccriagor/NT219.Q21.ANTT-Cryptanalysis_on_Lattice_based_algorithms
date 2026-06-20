#!/usr/bin/env bash
# =============================================================================
# measure_codesize.sh - WP5 code size of the built crypto libraries, via `size`
# (text/data/bss/total bytes). Output: data/codesize_<arch>.csv
# Defaults to the custom OpenSSL libcrypto and liboqs (if present); override:
#   CODESIZE_LIBS="/path/libcrypto.so /path/liboqs.so" scripts/measure_codesize.sh
# =============================================================================
# DESIGN NOTES
#   - Tool = binutils `size` (brief 7.4: "size utility, readelf -S"):
#     text = machine code, data = initialized globals (tables), bss =
#     zero-filled; total = their sum. Storage-side metric (vs RSS = runtime).
#   - libcrypto.so/liboqs.so aggregate MANY algorithms - per-algorithm
#     numbers are only possible via PQClean's per-scheme .a files:
#     CODESIZE_LIBS="path/libml-kem-768_clean.a ..." scripts/measure_codesize.sh
#   - clean-vs-avx2 (.a) quantifies the size cost of SIMD speed (Analysis).
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
# shellcheck disable=SC1091
[ -f "$HERE/versions.env" ] && source "$HERE/versions.env" >/dev/null 2>&1 || true

ARCH="$(uname -m)"
OUT="$ROOT/data/codesize_${ARCH}.csv"
mkdir -p "$ROOT/data"

LIBS="${CODESIZE_LIBS:-}"
if [ -z "$LIBS" ]; then
  for d in "${OSSL_PREFIX:-}/lib64" "${OSSL_PREFIX:-}/lib" \
           "${LIBOQS_PREFIX_OPT:-}/lib" "${LIBOQS_PREFIX_REF:-}/lib"; do
    for f in "$d"/libcrypto.so "$d"/libcrypto.a "$d"/liboqs.so "$d"/liboqs.a; do
      [ -f "$f" ] && LIBS="$LIBS $f"
    done
  done
fi
[ -z "$LIBS" ] && LIBS="$ROOT/build/bench_evp"     # fallback: the bench binary

echo "file,text_bytes,data_bytes,bss_bytes,total_bytes" > "$OUT"
for f in $LIBS; do
  [ -f "$f" ] || continue
  # Use `size -t` and read the (TOTALS) line (last row): for a .so it equals the
  # single object row, but for a .a (archive = MANY object rows) it is the true
  # SUM. Plain `size | awk NR==2` took only the FIRST object of an archive
  # (so libcrypto.a / per-scheme .a came out far too small). text data bss dec.
  read -r text data bss dec < <(size -t "$f" 2>/dev/null | tail -1 | awk '{print $1,$2,$3,$4}')
  if [ -n "${text:-}" ]; then
    echo "$(basename "$f"),${text},${data},${bss},${dec}" >> "$OUT"
    echo "==> $(basename "$f"): text=${text} data=${data} bss=${bss} total=${dec} bytes"
  fi
done
echo "Code-size CSV: $OUT"