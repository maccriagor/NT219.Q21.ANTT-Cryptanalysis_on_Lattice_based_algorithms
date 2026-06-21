#!/bin/bash
# =============================================================================
# build_liboqs.sh <ref|opt> - Build ONE liboqs tree.
# 
# Usage:
#   bash scripts/build_liboqs.sh ref    # portable C build  -> $LIBOQS_PREFIX_REF
#   bash scripts/build_liboqs.sh opt    # NEON/native build -> $LIBOQS_PREFIX_OPT
# 
# Environment variables:
#   FORCE=1 bash scripts/build_liboqs.sh ref|opt  # rebuild over  
#   USE_ARM_PMU=1 bash scripts/build_liboqs.sh opt   # + PMU cycle counter
# =============================================================================
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/.." && pwd)"
source "$HERE/versions.env"

VARIANT="${1:-}"
case "$VARIANT" in
  ref) PREFIX="$LIBOQS_PREFIX_REF"; OPT_TARGET="generic"; NEON="OFF" ;;
  opt) PREFIX="$LIBOQS_PREFIX_OPT"; OPT_TARGET="auto"; NEON="ON" ;;
  *) echo "Usage: $0 ref|opt"; exit 1 ;;
esac

# liboqs links against OUR OpenSSL: it must be built first (fail fast, clearly).
[ -x "$OSSL_PREFIX/bin/openssl" ] || { echo "OpenSSL missing at $OSSL_PREFIX: run scripts/build_openssl.sh first"; exit 1; }

if [ -e "$PREFIX/lib/liboqs.a" ] && [ "${FORCE:-0}" != "1" ]; then
  echo "liboqs($VARIANT) already installed at $PREFIX (FORCE=1 to rebuild). Skipping."
  exit 0
fi

# Download EXACTLY the pinned release. To build a different release later:
# change LIBOQS_TAG in versions.env, delete $SRC_ROOT/liboqs, re-run FORCE=1.
mkdir -p "$SRC_ROOT"
cd "$SRC_ROOT"
if [ ! -d liboqs ]; then
  git clone --depth 1 --branch "$LIBOQS_TAG" https://github.com/open-quantum-safe/liboqs.git
fi
cd liboqs
mkdir -p "$REPO_ROOT/docs"
git rev-parse HEAD > "$REPO_ROOT/docs/liboqs.commit"

# ARM-only flags: explicit NEON switch; optional PMU cycle counter.
EXTRA=""
if [ "$(uname -m)" = "aarch64" ]; then
  EXTRA="-DOQS_USE_ARM_NEON_INSTRUCTIONS=$NEON"
  if [ "${USE_ARM_PMU:-0}" = "1" ]; then
    EXTRA="$EXTRA -DOQS_SPEED_USE_ARM_PMU=ON"
  fi
fi

cmake -GNinja -S . -B "build-$VARIANT" \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DOQS_DIST_BUILD=OFF \
  -DOQS_OPT_TARGET="$OPT_TARGET" \
  -DOQS_USE_OPENSSL=ON \
  -DOPENSSL_ROOT_DIR="$OSSL_PREFIX" \
  $EXTRA
ninja -C "build-$VARIANT"
ninja -C "build-$VARIANT" install
echo "DONE: liboqs($VARIANT) -> $PREFIX   (speed tools in $SRC_ROOT/liboqs/build-$VARIANT/tests/)"