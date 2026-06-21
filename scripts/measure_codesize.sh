#!/bin/bash
# =============================================================================
# measure_codesize.sh - code size of the built crypto libraries, via `size`.
#
# Usage:
#   bash scripts/measure_codesize.sh                       # auto-find libcrypto + liboqs
#   CODESIZE_LIBS="a.so b.a" bash scripts/measure_codesize.sh   # measure given files
#   E.g: CODESIZE_LIBS="lib/libml-kem-768_clean.a ..." bash scripts/measure_codesize.sh
# 
# Output: data/codesize_<arch>.csv  (file,text,data,bss,total in bytes)
# =============================================================================
set -u
 
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
# shellcheck disable=SC1091
[ -f "$HERE/versions.env" ] && source "$HERE/versions.env" >/dev/null 2>&1 || true
 
ARCH="$(uname -m)"
OUT="$ROOT/data/codesize_${ARCH}.csv"
mkdir -p "$ROOT/data"
 
# Pick the libraries: CODESIZE_LIBS wins; else auto-find our OpenSSL + liboqs
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
  # Read the (TOTALS) line of `size -t` (last row). For a .so it equals the one
  # object row; for a .a (archive = many objects) it is the true SUM. Plain
  # `size | awk NR==2` took only the FIRST object of an archive (too small).
  read -r text data bss dec < <(size -t "$f" 2>/dev/null | tail -1 | awk '{print $1,$2,$3,$4}')
  if [ -n "${text:-}" ]; then
    # Label = <prefix-dir>/<file> so OPT and REF liboqs.so don't collide
    label="$(basename "$(dirname "$(dirname "$f")")")/$(basename "$f")"
    echo "${label},${text},${data},${bss},${dec}" >> "$OUT"
    echo "==> ${label}: text=${text} data=${data} bss=${bss} total=${dec} bytes"
  fi
done
 
echo "Code-size CSV: $OUT"