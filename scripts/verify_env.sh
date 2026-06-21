#!/bin/bash
# =============================================================================
# verify_env.sh - check the custom OpenSSL really has ML-KEM/ML-DSA, then dump the
# build environment (compilers, flags, CPU) to docs/env_report_<arch>.txt for the writeup.
# Usage:
#   bash scripts/verify_env.sh
# =============================================================================

set -eu

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/.." && pwd)"
source "$HERE/versions.env"

ARCH="$(uname -m)"
REPORT="$REPO_ROOT/docs/env_report_${ARCH}.txt"
mkdir -p "$REPO_ROOT/docs"

# stop here if we'd otherwise benchmark the system openssl without PQC
if [ ! -x "$OSSL_PREFIX/bin/openssl" ]; then
  echo "FAIL: no openssl at $OSSL_PREFIX. Run scripts/build_openssl.sh"
  exit 1
fi
export LD_LIBRARY_PATH="$OSSL_PREFIX/lib:$OSSL_PREFIX/lib64:${LD_LIBRARY_PATH:-}"

OSSL_VER="$("$OSSL_PREFIX/bin/openssl" version)"
KEMS=$("$OSSL_PREFIX/bin/openssl" list -kem-algorithms | grep -ci ML-KEM || true)
SIGS=$("$OSSL_PREFIX/bin/openssl" list -signature-algorithms | grep -ci ML-DSA || true)

if [ "$KEMS" -lt 1 ]; then
  echo "FAIL: ML-KEM not available in $OSSL_VER"
  exit 1
fi
if [ "$SIGS" -lt 1 ]; then
  echo "FAIL: ML-DSA not available in $OSSL_VER"
  exit 1
fi

# none of the facts below are fatal - if a tool is missing we just note it
GOV_FILE="/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
if [ -r "$GOV_FILE" ]; then
  GOVERNOR="$(cat "$GOV_FILE")"
else
  GOVERNOR="unavailable (container/VM)"
fi

# we build with gcc; clang is recorded too since the brief asks for gcc/clang
if command -v clang >/dev/null; then
  CLANG_VER="$(clang --version | head -1)"
else
  CLANG_VER="not installed"
fi

CPU_MODEL="$(lscpu | grep -i 'model name' | head -1 | cut -d: -f2- | sed 's/^ *//')"

if [ -f "$REPO_ROOT/Makefile" ]; then
  BUILD_FLAGS="$(grep -E '^RELEASE' "$REPO_ROOT/Makefile")"
else
  BUILD_FLAGS="Makefile not found"
fi

{
  echo "environment report, $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "uname: $(uname -a)"
  echo "cpu: $CPU_MODEL ($(nproc) cores)"
  echo "cpu governor: $GOVERNOR"
  echo "gcc: $(gcc --version | head -1)"
  echo "g++: $(g++ --version | head -1)"
  echo "clang: $CLANG_VER"
  echo "cmake: $(cmake --version | head -1)"
  echo "perl: $(perl -e 'print $^V')"
  echo "openssl: $OSSL_VER (prefix $OSSL_PREFIX)"
  echo "ML-KEM algorithms: $KEMS, ML-DSA algorithms: $SIGS"
  echo "openssl commit: $(cat "$REPO_ROOT/docs/openssl.commit" 2>/dev/null || echo n/a)"
  echo "liboqs commit: $(cat "$REPO_ROOT/docs/liboqs.commit" 2>/dev/null || echo 'n/a')"
  echo "build flags: $BUILD_FLAGS"
} | tee "$REPORT"

echo "PASS. Report written to: $REPORT"