#!/usr/bin/env bash
# Biên dịch + chạy microbenchmark trong container pqc (OpenSSL 3.6.2 EVP, native PQC).
# CSV -> data/results/micro_<plat>.csv.
# Dùng: run_micro.sh [args cho pqc_bench...]
#   vd:  run_micro.sh --level all --iters 1000 --warmup 100 --batches 10
#        run_micro.sh --suite kem --level 5 --maxms 5000
set -euo pipefail
cd "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
ARCH="$(uname -m)"
case "$ARCH" in
  aarch64|arm64) TAG=pqc:wp1-arm64; PLAT=arm ;;
  x86_64|amd64)  TAG=pqc:wp1-amd64; PLAT=x86 ;;
  *)             TAG="pqc:wp1-$ARCH"; PLAT="$ARCH" ;;
esac
mkdir -p data/results
ARGS="${*:---level all --iters 1000 --warmup 100 --batches 10}"
echo ">>> Microbenchmark trong $TAG: $ARGS"
docker run --rm -v "$PWD:/work" -w /work "$TAG" bash -lc "
  g++ -O2 -std=c++17 benchmarks/micro/pqc_bench.cpp -o /tmp/pqc_bench \
      -I/opt/openssl/include -L/opt/openssl/lib -lssl -lcrypto &&
  /tmp/pqc_bench $ARGS --csv /work/data/results/micro_${PLAT}.csv
"
echo ">>> CSV: data/results/micro_${PLAT}.csv"
column -t -s, "data/results/micro_${PLAT}.csv"
