#!/usr/bin/env bash
# =============================================================================
# build_oqsprovider.sh [ref|opt] - Build the OQS Provider for OpenSSL 3.
#
# The brief lists "OpenSSL OQS fork" for TLS integration. That 1.1.1 fork is
# OFFICIALLY DEPRECATED/archived; its designated successor is this provider
# ("The OQS Provider for OpenSSL 3 provides full support for post-quantum key
# exchange and authentication in TLS 1.3, X.509, S/MIME" - deprecation notice,
# github.com/open-quantum-safe/openssl). Build steps follow the oqs-provider
# README (cmake -DOPENSSL_ROOT_DIR -Dliboqs_DIR; build; install).
# Requires: build_openssl.sh and build_liboqs.sh <variant> done first.
# =============================================================================
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/.." && pwd)"
source "$HERE/versions.env"

VARIANT="${1:-opt}"
case "$VARIANT" in
  ref) LIBOQS_PREFIX="$LIBOQS_PREFIX_REF" ;;
  opt) LIBOQS_PREFIX="$LIBOQS_PREFIX_OPT" ;;
  *) echo "Usage: $0 ref|opt"; exit 1 ;;
esac
[ -e "$LIBOQS_PREFIX/lib/liboqs.so" ] || { echo "liboqs($VARIANT) missing: run build_liboqs.sh $VARIANT"; exit 1; }
[ -x "$OSSL_PREFIX/bin/openssl" ]     || { echo "OpenSSL missing: run build_openssl.sh"; exit 1; }

# Download EXACTLY the pinned release. To build a different release later:
# change OQSPROVIDER_TAG in versions.env, delete $SRC_ROOT/oqs-provider, re-run.
mkdir -p "$SRC_ROOT"; cd "$SRC_ROOT"
if [ ! -d oqs-provider ]; then
  git clone --depth 1 --branch "$OQSPROVIDER_TAG" https://github.com/open-quantum-safe/oqs-provider.git
fi
cd oqs-provider
mkdir -p "$REPO_ROOT/docs"
git rev-parse HEAD > "$REPO_ROOT/docs/oqsprovider.commit"

cmake -S . -B _build \
  -DOPENSSL_ROOT_DIR="$OSSL_PREFIX" \
  -Dliboqs_DIR="$LIBOQS_PREFIX/lib/cmake/liboqs" \
  -DCMAKE_INSTALL_PREFIX="$OQSPROVIDER_PREFIX"
cmake --build _build -j "$JOBS"
cmake --install _build

# Verify the provider actually loads into OUR OpenSSL.
export LD_LIBRARY_PATH="$OSSL_PREFIX/lib:$OSSL_PREFIX/lib64:$LIBOQS_PREFIX/lib:${LD_LIBRARY_PATH:-}"
"$OSSL_PREFIX/bin/openssl" list -providers \
    -provider-path "$OQSPROVIDER_PREFIX/lib" -provider oqsprovider \
  | grep -qi oqsprovider || { echo "ERROR: oqsprovider failed to load"; exit 1; }
echo "DONE. Use with: -provider-path $OQSPROVIDER_PREFIX/lib -provider oqsprovider -provider default"
