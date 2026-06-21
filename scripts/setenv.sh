#!/bin/bash
# =============================================================================
# Run: 
#   source scripts/setenv.sh
# =============================================================================

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$here/versions.env"

export PATH="$OSSL_PREFIX/bin:$PATH"
export LD_LIBRARY_PATH="$OSSL_PREFIX/lib:$OSSL_PREFIX/lib64:${LD_LIBRARY_PATH:-}"

echo "Activated OpenSSL from: $OSSL_PREFIX"
"$OSSL_PREFIX/bin/openssl" version \
  || echo "(openssl not built yet at $OSSL_PREFIX - run scripts/build_openssl.sh)"