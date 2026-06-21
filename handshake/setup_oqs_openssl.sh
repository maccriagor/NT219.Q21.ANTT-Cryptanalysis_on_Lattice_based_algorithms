#!/bin/bash
set -e

# --- Environment Configuration ---
export LIBOQS_DIR="/home/maccriagor/Documents/liboqs/build-opt"
export BASE_DIR=$(pwd)
export OPENSSL_DIR="$BASE_DIR/openssl-build"
export OQS_PROVIDER_DIR="$BASE_DIR/oqs-provider"

echo "=== Installing Essential Build Tools ==="
sudo apt-get update && sudo apt-get install build-essential cmake libtool automake autoconf ninja-build bc -y

echo "=== Compiling Isolated OpenSSL 3.3 ==="
if [ ! -d "openssl" ]; then
    git clone --depth 1 -b openssl-3.3 https://github.com/openssl/openssl.git
fi
cd openssl
# Modified: Appended /ssl to --openssldir to align with your OPENSSL_CONF target path
./config --prefix="$OPENSSL_DIR" --openssldir="$OPENSSL_DIR/ssl" shared
make -j$(nproc)
# Modified: Added install_ssldirs to guarantee openssl.cnf gets generated
make install_sw install_ssldirs
cd ..

echo "=== Compiling oqs-provider ==="
if [ ! -d "oqs-provider" ]; then
    git clone --depth 1 https://github.com/open-quantum-safe/oqs-provider.git
fi
cd oqs-provider
# Tell CMake exactly where your liboqs and local OpenSSL live
liboqs_DIR="$LIBOQS_DIR" cmake -S . -B _build \
    -DOPENSSL_ROOT_DIR="$OPENSSL_DIR" \
    -DCMAKE_INSTALL_PREFIX="$OPENSSL_DIR"
cmake --build _build --parallel $(nproc)
cmake --install _build
cd ..

echo "=== Injecting OQS Configuration ==="
export OPENSSL_CONF="$OPENSSL_DIR/ssl/openssl.cnf"
if ! grep -q "oqs_sect" "$OPENSSL_CONF"; then
    cat <<EOF >> "$OPENSSL_CONF"

[provider_sect]
default = default_sect
oqsprovider = oqs_sect

[default_sect]
activate = 1

[oqs_sect]
activate = 1
EOF
fi

echo "=== Verifying Algorithm Registration ==="
export LD_LIBRARY_PATH="$OPENSSL_DIR/lib64:$OPENSSL_DIR/lib:$LIBOQS_DIR/lib:$LD_LIBRARY_PATH"

echo "Available Signatures:"
"$OPENSSL_DIR/bin/openssl" list -signature-algorithms | grep -E "dilithium" || true
echo "Available KEMs:"
"$OPENSSL_DIR/bin/openssl" list -kem-algorithms | grep -E "kyber" || true

echo "Setup completed successfully! OpenSSL binary: $OPENSSL_DIR/bin/openssl"
