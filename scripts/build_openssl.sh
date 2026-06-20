#!/bin/bash
# =============================================================================
# Usage:
#   bash scripts/build_openssl.sh
#   FORCE=1       rebuild over an existing install
#   SKIP_TESTS=1  skip `make test` (CI / slow ARM)
# =============================================================================

set -e

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/versions.env"

# Already built (FORCE=1 to redo)
if [ -x "$OSSL_PREFIX/bin/openssl" ] && [ "${FORCE:-0}" != 1 ]; then
  echo "OpenSSL already at $OSSL_PREFIX (FORCE=1 to rebuild)."
  exit 0
fi

# Fetch the pinned release
mkdir -p "$SRC_ROOT"
cd "$SRC_ROOT"
if [ ! -d openssl ]; then
  git clone --depth 1 --branch "$OPENSSL_TAG" https://github.com/openssl/openssl.git
fi
cd openssl
mkdir -p "$HERE/../docs"
git rev-parse HEAD > "$HERE/../docs/openssl.commit"   # reproducibility

# Configure + build + install
./config --prefix="$OSSL_PREFIX" --openssldir="$OSSL_PREFIX/ssl"
make -j"$JOBS"
if [ "${SKIP_TESTS:-0}" = 1 ]; then
  echo "SKIP_TESTS=1, skipping make test"
else
  make test
fi
make install

# Fail loud if PQC isn't actually there.
# x86_64 installs to lib64, ARM to lib.
export LD_LIBRARY_PATH="$OSSL_PREFIX/lib:$OSSL_PREFIX/lib64:${LD_LIBRARY_PATH:-}"

"$OSSL_PREFIX/bin/openssl" version

if ! "$OSSL_PREFIX/bin/openssl" list -kem-algorithms | grep -qi ML-KEM; then
  echo "no ML-KEM -> not a PQC build"
  exit 1
fi

if ! "$OSSL_PREFIX/bin/openssl" list -signature-algorithms | grep -qi ML-DSA; then
  echo "no ML-DSA -> not a PQC build"
  exit 1
fi

echo "Built OpenSSL $OPENSSL_TAG at $OSSL_PREFIX. Next: source scripts/setenv.sh"