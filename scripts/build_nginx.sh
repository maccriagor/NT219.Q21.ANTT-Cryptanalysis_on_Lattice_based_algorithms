#!/usr/bin/env bash
# =============================================================================
# build_nginx.sh - Build nginx (pinned tag) DYNAMICALLY linked against OUR
# OpenSSL 3.6.2 (~/pqc/openssl), so TLS gets ML-KEM groups + ML-DSA certs.
#
# Why build from source: distro nginx links the SYSTEM OpenSSL 3.0.x, which
# has no ML-KEM -> PQC measurement impossible. nginx takes all TLS from the
# OpenSSL it is linked with; ssl_ecdh_curve passes the group list straight to
# SSL_CTX_set1_curves_list (verified live: PQC list accepted, fake rejected).
#
# Modules rewrite & gzip are DISABLED on purpose: they pull PCRE/zlib dev
# packages we do not need for a benchmark server (consequence: no 'return'
# directive -> the bench config serves a tiny static file instead).
#
# Runtime: like bench_evp, NO rpath is baked in; start nginx from scripts
# that source setenv.sh (LD_LIBRARY_PATH), as bench_tls_nginx.sh does.
#
# Output: $NGINX_PREFIX (sbin/nginx, conf/, html/, logs/), docs/nginx.commit
# Requires: build_openssl.sh done. Re-run: idempotent; FORCE=1 to rebuild.
# =============================================================================
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/.." && pwd)"
source "$HERE/versions.env"

[ -x "$OSSL_PREFIX/bin/openssl" ] || { echo "OpenSSL missing: run scripts/build_openssl.sh first"; exit 1; }
if [ -x "$NGINX_PREFIX/sbin/nginx" ] && [ "${FORCE:-0}" != "1" ]; then
  echo "nginx already installed at $NGINX_PREFIX. Skipping (FORCE=1 to rebuild)."; exit 0
fi
# OpenSSL installs libs under lib/ or lib64/ depending on platform.
OSSL_LIBDIR="$OSSL_PREFIX/lib"; [ -d "$OSSL_PREFIX/lib64" ] && OSSL_LIBDIR="$OSSL_PREFIX/lib64"

# Download EXACTLY the pinned release. To change: edit NGINX_TAG in
# versions.env, delete $SRC_ROOT/nginx, re-run with FORCE=1.
mkdir -p "$SRC_ROOT"; cd "$SRC_ROOT"
[ -d nginx ] || git clone --depth 1 --branch "$NGINX_TAG" https://github.com/nginx/nginx.git
cd nginx
mkdir -p "$REPO_ROOT/docs"
git rev-parse HEAD > "$REPO_ROOT/docs/nginx.commit"

auto/configure --prefix="$NGINX_PREFIX" --with-http_ssl_module \
  --without-http_rewrite_module --without-http_gzip_module \
  --with-cc-opt="-I$OSSL_PREFIX/include" --with-ld-opt="-L$OSSL_LIBDIR"
make -j "$JOBS"
make install

# Verify: the binary must report OUR OpenSSL, and actually start with it.
export LD_LIBRARY_PATH="$OSSL_LIBDIR:${LD_LIBRARY_PATH:-}"
"$NGINX_PREFIX/sbin/nginx" -V 2>&1 | grep -q "built with OpenSSL 3.6" \
  || { echo "ERROR: nginx not built with our OpenSSL"; exit 1; }
"$NGINX_PREFIX/sbin/nginx" -V 2>&1 | grep -E "nginx version|built with"
echo "DONE. Next: scripts/gen_tls_certs.sh then scripts/bench_tls_nginx.sh"
