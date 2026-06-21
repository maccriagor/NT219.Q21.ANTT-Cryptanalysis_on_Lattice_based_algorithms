#!/bin/bash
set -e

# --- Paths & Tooling ---
export LIBOQS_DIR="/home/maccriagor/Documents/liboqs/build-opt"
OPENSSL_BIN="$(pwd)/openssl-build/bin/openssl"
export OPENSSL_CONF="$(pwd)/openssl-build/ssl/openssl.cnf"
export LD_LIBRARY_PATH="$(pwd)/openssl-build/lib64:$(pwd)/openssl-build/lib:$LIBOQS_DIR/lib:$LD_LIBRARY_PATH"

CERT_DIR="$(pwd)/certs"
mkdir -p "$CERT_DIR"
cd "$CERT_DIR"

# Clean up size analysis and old profiles to start fresh
rm -f size_analysis.csv

generate_missing_suite() {
    local name=$1
    local sig_alg=$2
    local ecc_curve=$3

    echo "Generating certificates for profile: $name"
    
    if [[ "$name" == rsa* ]]; then
        bits=$(echo $name | grep -o '[0-9]\+')
        "$OPENSSL_BIN" genrsa -out ca_${name}.key $bits
        "$OPENSSL_BIN" req -x509 -new -nodes -key ca_${name}.key -subj "/CN=${name}CA" -days 365 -out ca_${name}.crt
        "$OPENSSL_BIN" genrsa -out server_${name}.key $bits
        "$OPENSSL_BIN" req -new -key server_${name}.key -subj "/CN=localhost" -out server_${name}.csr
        "$OPENSSL_BIN" x509 -req -in server_${name}.csr -CA ca_${name}.crt -CAkey ca_${name}.key -CAcreateserial -out server_${name}.crt -days 365
    
    elif [[ "$name" == ecdsa* ]]; then
        "$OPENSSL_BIN" ecparam -name "$ecc_curve" -genkey -out ca_${name}.key
        "$OPENSSL_BIN" req -x509 -new -nodes -key ca_${name}.key -subj "/CN=${name}CA" -days 365 -out ca_${name}.crt
        "$OPENSSL_BIN" ecparam -name "$ecc_curve" -genkey -out server_${name}.key
        "$OPENSSL_BIN" req -new -key server_${name}.key -subj "/CN=localhost" -out server_${name}.csr
        "$OPENSSL_BIN" x509 -req -in server_${name}.csr -CA ca_${name}.crt -CAkey ca_${name}.key -CAcreateserial -out server_${name}.crt -days 365
    
    else
        "$OPENSSL_BIN" genpkey -algorithm "$sig_alg" -out ca_${name}.key
        "$OPENSSL_BIN" req -x509 -new -nodes -key ca_${name}.key -subj "/CN=${name}CA" -days 365 -out ca_${name}.crt
        "$OPENSSL_BIN" genpkey -algorithm "$sig_alg" -out server_${name}.key
        "$OPENSSL_BIN" req -new -key server_${name}.key -subj "/CN=localhost" -out server_${name}.csr
        "$OPENSSL_BIN" x509 -req -in server_${name}.csr -CA ca_${name}.crt -CAkey ca_${name}.key -CAcreateserial -out server_${name}.crt -days 365
    fi

    pub_size=$(wc -c < server_${name}.key)
    cert_size=$(wc -c < server_${name}.crt)
    echo "$name,$pub_size,$cert_size" >> size_analysis.csv
}

# --- 1. Traditional RSA Profiles ---
generate_missing_suite "rsa2048" "rsa" ""
generate_missing_suite "rsa3072" "rsa" ""
generate_missing_suite "rsa4096" "rsa" ""
generate_missing_suite "rsa7680" "rsa" ""

# --- 2. Traditional ECDSA Profiles ---
generate_missing_suite "ecdsa"    "ec" "prime256v1" # Standard fallback (P-256)
generate_missing_suite "ecdsa384" "ec" "secp384r1"
generate_missing_suite "ecdsa521" "ec" "secp521r1"

# --- 3. Pure Post-Quantum ML-DSA Profiles ---
generate_missing_suite "mldsa44" "mldsa44" ""
generate_missing_suite "mldsa65" "mldsa65" ""
generate_missing_suite "mldsa87" "mldsa87" ""

# --- 4. Hybrid (Classic + PQ) Profiles ---
generate_missing_suite "p256_mldsa44" "p256_mldsa44" ""
generate_missing_suite "p384_mldsa65" "p384_mldsa65" ""
generate_missing_suite "p521_mldsa87" "p521_mldsa87" ""

# --- 5. Generate Standalone .pem Variations ---
# Creating duplicates/links to match the explicit *.cert.pem and *.key.pem files in your list
echo "Creating explicit alias PEM mappings..."
cp server_ecdsa.key      ecp256.key.pem
cp server_ecdsa.crt      ecp256.cert.pem
cp server_mldsa65.key    mldsa65.key.pem
cp server_mldsa65.crt    mldsa65.cert.pem
cp server_rsa2048.key    rsa2048.key.pem
cp server_rsa2048.crt    rsa2048.cert.pem

echo "=== Generation Complete ==="
cat size_analysis.csv
