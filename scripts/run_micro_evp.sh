#!/usr/bin/env bash
# Chạy harness OpenSSL-EVP (pqc_bench.cpp) -> số liệu BẢN TỐI ƯU (AVX2 native) cho mọi scheme.
# Bổ trợ cho 3 benchmark PQClean reference (so reference-vs-optimized = RQ1).
# Ghi: data/micro/x86/micro_evp_x86.csv  (cột: ...,median_ns,ci95lo,ci95hi,...,pk_bytes,...)
set -euo pipefail
cd "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
IMG="${IMG:-pqc:wp1-amd64}"; D=data/micro/x86; mkdir -p "$D"
podman run --rm -v "$PWD:/work:z" -w /work "$IMG" sh -lc '
  set -e; export LD_LIBRARY_PATH=/opt/openssl/lib64
  g++ -O2 -std=c++17 benchmarks/micro/pqc_bench.cpp -o /tmp/b \
      -I/opt/openssl/include -L/opt/openssl/lib64 -lssl -lcrypto -lpthread -ldl
  D=data/micro/x86
  /tmp/b --suite kem --level all --iters 100 --warmup 20 --batches 10 --maxms 8000 --csv $D/_evp_kem.csv   2>/dev/null
  /tmp/b --suite kex --level all --iters 100 --warmup 20 --batches 10 --maxms 8000 --csv $D/_evp_kex.csv   2>/dev/null
  /tmp/b --suite sig --only ML-DSA --iters 100 --warmup 20 --batches 10 --maxms 8000 --csv $D/_evp_mldsa.csv 2>/dev/null
  /tmp/b --suite sig --only ECDSA --iters 100 --warmup 20 --batches 10 --maxms 8000 --csv $D/_evp_ecdsa.csv 2>/dev/null
  /tmp/b --suite sig --only RSA --warmup 0 --batches 1 --iters 3 --maxms 120000 --csv $D/_evp_rsa.csv      2>/dev/null
  awk "FNR==1 && NR!=1{next}{print}" $D/_evp_kem.csv $D/_evp_kex.csv $D/_evp_mldsa.csv $D/_evp_ecdsa.csv $D/_evp_rsa.csv > $D/micro_evp_x86.csv
  rm -f $D/_evp_*.csv
  echo "DONE_EVP rows=$(wc -l < $D/micro_evp_x86.csv)"
'
