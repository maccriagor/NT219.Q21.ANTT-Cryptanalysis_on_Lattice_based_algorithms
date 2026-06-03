#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/versions.env

WORK="$(pwd)/.build/provider"
mkdir -p "$WORK"; cd "$WORK"
[[ -d oqs-provider/.git ]] || git clone https://github.com/open-quantum-safe/oqs-provider.git
cd oqs-provider
git fetch --tags
git checkout "$OQSPROVIDER_TAG"
git rev-parse HEAD > "$OLDPWD/../../docs/oqsprovider.commit"

liboqs_DIR="$LIBOQS_PREFIX_OPT" cmake \
  -DOPENSSL_ROOT_DIR="$OSSL_PREFIX" \
  -DCMAKE_PREFIX_PATH="$OSSL_PREFIX" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PROVIDER_PREFIX" \
  -S . -B _build
cmake --build _build -j"$JOBS"
ctest --test-dir _build            # self-test provider
cmake --install _build

# Đặt oqsprovider.so vào ossl-modules của OpenSSL tự build
MODDIR="$OSSL_PREFIX/lib/ossl-modules"; mkdir -p "$MODDIR"
cp _build/lib/oqsprovider.so "$MODDIR"/

# Kích hoạt provider trong openssl.cnf (theo oqs-demos)
CNF="$OSSL_PREFIX/ssl/openssl.cnf"
sed -i "s/default = default_sect/default = default_sect\noqsprovider = oqsprovider_sect/g" "$CNF"
sed -i "s/\[default_sect\]/\[default_sect\]\nactivate = 1\n\[oqsprovider_sect\]\nactivate = 1\n/g" "$CNF"

# Kiểm chứng
export OPENSSL_CONF="$CNF" OPENSSL_MODULES="$MODDIR"
"$OSSL_PREFIX/bin/openssl" list -providers -verbose -provider oqsprovider
"$OSSL_PREFIX/bin/openssl" list -kem-algorithms -provider oqsprovider | head