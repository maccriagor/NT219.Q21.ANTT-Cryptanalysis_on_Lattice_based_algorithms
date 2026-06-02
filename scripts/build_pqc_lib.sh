#!/usr/bin/env bash
# build_pqc_lib.sh — Build static lib build/libpqc.a từ PQClean đã vendor, ĐỦ MỌI MỨC NIST.
# ---------------------------------------------------------------------------
# Param-set: ml-kem 512/768/1024 (Cat1/3/5) + ml-dsa 44/65/87 (Cat2/3/5) + common.
#
# QUAN TRỌNG:
#  (1) Nguồn PQClean là C → biên dịch bằng gcc (-c), rồi link vào C++ bằng g++.
#  (2) Mỗi param-set có params.h/poly.h RIÊNG nhưng TRÙNG TÊN FILE giữa các mức.
#      => mỗi .c phải biên dịch với "-I<thư-mục-của-chính-nó>" ĐỨNG TRƯỚC để thấy đúng
#      params.h của mức đó. Symbol đã được PQClean namespace theo mức (PQCLEAN_MLKEM512_…)
#      nên gộp chung 1 lib không đụng nhau.
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$ROOT"

VEN="vendor/pqclean"
LIB="build/libpqc.a"
CC="${CC:-gcc}"
PARAM_DIRS="mlkem512 mlkem768 mlkem1024 mldsa44 mldsa65 mldsa87"

for d in $PARAM_DIRS common; do
  [ -d "$VEN/$d" ] || { echo "❌ Thiếu $VEN/$d. Chạy scripts/fetch_sources.sh (vendor PQClean)."; exit 1; }
done

if [ "${1:-}" != "--force" ] && [ -f "$LIB" ]; then
  echo "✓ $LIB đã có (dùng '--force' để build lại)."; exit 0
fi

echo "🔨 Biên dịch PQClean (6 param-set) bằng $CC -> objects ..."
mkdir -p build/pqc_obj
rm -f build/pqc_obj/*.o
n=0
for src in $(find $(for d in $PARAM_DIRS common; do echo "$VEN/$d"; done) -name '*.c' \
              ! -path '*/keccak2x/*' ! -path '*/keccak4x/*'); do
  dir="$(dirname "$src")"
  obj="build/pqc_obj/$(echo "$src" | sed "s#$VEN/##; s#/#_#g; s#\.c\$#.o#")"
  "$CC" -O2 -c "$src" -I"$dir" -I"$VEN/common" -o "$obj"
  n=$((n + 1))
done
ar rcs "$LIB" build/pqc_obj/*.o
echo "✅ Đã tạo $LIB từ $n object ($(du -h "$LIB" | cut -f1))."
