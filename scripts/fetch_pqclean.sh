#!/usr/bin/env bash
# =============================================================================
# fetch_pqclean.sh [clean|avx2|aarch64] - Fetch PQClean and build per-scheme
# reference libraries (brief 7.2: "PQClean (portable C implementations)").
#
# PQClean ships THREE implementations per scheme: clean (portable C reference),
# avx2 (x86 optimized), aarch64 (ARM/NEON optimized). Each builds into its own
# static library lib<scheme>_<variant>.a -> perfect per-algorithm code-size
# measurement for WP5 (size lib*.a), and an upstream source liboqs imports.
#
# NOTE (checked Jun 2026): PQClean is deprecated upstream and will be archived
# read-only in July 2026; successor: https://github.com/pq-code-package.
# The pinned commit (docs/pqclean.commit) keeps this build reproducible.
# =============================================================================
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/.." && pwd)"
source "$HERE/versions.env"

VARIANT="${1:-clean}"
case "$VARIANT" in clean|avx2|aarch64) ;; *) echo "Usage: $0 clean|avx2|aarch64"; exit 1;; esac
# Optimized variants contain arch-specific assembly: fail fast on the wrong CPU.
case "$VARIANT:$(uname -m)" in
  aarch64:x86_64|avx2:aarch64)
    echo "ERROR: variant '$VARIANT' cannot build on this machine ($(uname -m))."; exit 1;;
esac

mkdir -p "$SRC_ROOT"; cd "$SRC_ROOT"
if [ ! -d PQClean/.git ]; then
  git clone --depth 1 https://github.com/PQClean/PQClean.git
fi
cd PQClean
mkdir -p "$REPO_ROOT/docs"
git rev-parse HEAD > "$REPO_ROOT/docs/pqclean.commit"

# Build the comparison-matrix schemes (brief 7.1) for the chosen variant.
SCHEMES="crypto_kem/ml-kem-512 crypto_kem/ml-kem-768 crypto_kem/ml-kem-1024 \
         crypto_sign/ml-dsa-44 crypto_sign/ml-dsa-65 crypto_sign/ml-dsa-87"
for s in $SCHEMES; do
  if [ -d "$s/$VARIANT" ]; then
    make -C "$s/$VARIANT" >/dev/null
    echo "built: $s/$VARIANT -> $(ls "$s/$VARIANT"/lib*.a)"
  else
    echo "skip : $s ($VARIANT not provided upstream)"
  fi
done
echo "DONE. Per-scheme libraries under $SRC_ROOT/PQClean (use 'size' on lib*.a for WP5)."
