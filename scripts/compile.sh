#!/usr/bin/env bash
# compile.sh [--run] <source.c|.cpp>
# Biên dịch một file harness trong container PQC (liboqs + OpenSSL 3.6.2).
# Tự đổi '\' -> '/' để nhận đường dẫn truyền từ Windows (VS Code ${relativeFile}).
set -euo pipefail

RUN=0
if [ "${1:-}" = "--run" ]; then RUN=1; shift; fi
SRC="${1:?Cách dùng: compile.sh [--run] <source>}"
SRC="${SRC//\\//}"          # \  ->  /
OUT="${SRC%.*}"             # bỏ phần mở rộng -> tên binary

case "$SRC" in
  *.c) CC=gcc; STD=-std=c11 ;;
  *)   CC=g++; STD=-std=c++17 ;;
esac

"$CC" -O3 "$STD" -Wall "$SRC" -o "$OUT" \
  -I/opt/openssl/include -I/usr/local/include \
  -L/opt/openssl/lib -L/usr/local/lib \
  -loqs -lssl -lcrypto -lpthread

echo "Built: $OUT"
if [ "$RUN" -eq 1 ]; then echo "----- run -----"; "./$OUT"; fi
