#!/bin/bash
# =============================================================================
# fetch_pqclean.sh - clone PQClean and build per-scheme static libraries.
#
# Usage:
#   bash scripts/fetch_pqclean.sh            # default: clean (portable C)
#   bash scripts/fetch_pqclean.sh avx2       # x86_64 optimized
#   bash scripts/fetch_pqclean.sh aarch64    # ARM/NEON optimized
# =============================================================================

set -eu

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/.." && pwd)"
source "$HERE/versions.env"

# Variant (default clean)
VARIANT="${1:-clean}"
case "$VARIANT" in
  clean|avx2|aarch64) ;;
  *) echo "Usage: $0 clean|avx2|aarch64"; exit 1 ;;
esac

# Optimized variants carry arch-specific asm; reject the wrong CPU
case "$VARIANT:$(uname -m)" in
  aarch64:x86_64|avx2:aarch64)
    echo "ERROR: variant '$VARIANT' cannot build on this machine ($(uname -m))."
    exit 1 ;;
esac

# Fetch the source
mkdir -p "$SRC_ROOT"
cd "$SRC_ROOT"
if [ ! -d PQClean/.git ]; then
  git clone --depth 1 https://github.com/PQClean/PQClean.git
fi

cd PQClean

# Record the build commit (reproducibility)
mkdir -p "$REPO_ROOT/docs"
git rev-parse HEAD > "$REPO_ROOT/docs/pqclean.commit"

# Comparison-matrix schemes
SCHEMES="crypto_kem/ml-kem-512 crypto_kem/ml-kem-768 crypto_kem/ml-kem-1024 \
         crypto_sign/ml-dsa-44 crypto_sign/ml-dsa-65 crypto_sign/ml-dsa-87"

# Build each scheme for the chosen variant
for s in $SCHEMES; do
  if [ -d "$s/$VARIANT" ]; then
    make -C "$s/$VARIANT" >/dev/null
    echo "built: $s/$VARIANT -> $(ls "$s/$VARIANT"/lib*.a)"
  else
    echo "skip : $s ($VARIANT not provided upstream)"
  fi
done

echo "DONE. Per-scheme libraries under $SRC_ROOT/PQClean (run 'size' on lib*.a)."