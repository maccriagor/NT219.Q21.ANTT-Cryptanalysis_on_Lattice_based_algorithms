#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/versions.env

WORK="$(pwd)/.build/openssl"
mkdir -p "$WORK"; cd "$WORK"
[[ -d openssl/.git ]] || git clone https://github.com/openssl/openssl.git
cd openssl
git fetch --tags
git checkout "$OPENSSL_TAG"
git rev-parse HEAD > "$OLDPWD/../../docs/openssl.commit"   # GHI LẠI commit hash

# Cấu hình: prefix riêng, shared, rpath + new-dtags. KHÔNG ghi đè hệ thống.
./Configure \
  --prefix="$OSSL_PREFIX" \
  --openssldir="$OSSL_PREFIX/ssl" \
  --libdir=lib \
  shared \
  -Wl,-rpath,"$OSSL_PREFIX/lib" -Wl,--enable-new-dtags

make -j"$JOBS"
make test           # BẮT BUỘC chạy self-test (bằng chứng build đúng)
sudo make install

# (Tùy chọn) cho ld.so tìm thấy mà không cần LD_LIBRARY_PATH:
echo "$OSSL_PREFIX/lib" | sudo tee /etc/ld.so.conf.d/openssl-3.6.2.conf
sudo ldconfig
"$OSSL_PREFIX/bin/openssl" version -a