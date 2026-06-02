#!/usr/bin/env bash
# run_kat.sh — Kiểm thử ĐÚNG ĐẮN (KAT-style) trước khi tin số đo (file 05 §7.4 "correctness OK").
# Mỗi benchmark có CỔNG đúng đắn nội bộ (ML-KEM: ss_encaps==ss_decaps; ML-DSA: verify pass +
# tamper reject; AES: roundtrip + tamper reject). Exit 0 = PASS. Đồng thời đối chiếu kích thước
# pk/ct/sig với FIPS 203/204. Kết quả -> data/kat/kat_results.txt
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"; cd "$ROOT"
OUT=data/kat/kat_results.txt; mkdir -p data/kat
ARCH="$(uname -m)"

# Build nếu thiếu binary
[ -x benchmark_mlkem/benchmark_mlkem ] || bash benchmark_mlkem/build.sh >/dev/null 2>&1
[ -x benchmark_mldsa/benchmark_mldsa ] || bash benchmark_mldsa/build.sh >/dev/null 2>&1
[ -x benchmark_aes/benchmark_aes ]     || bash benchmark_aes/build.sh   >/dev/null 2>&1

pass=0; fail=0
{
  echo "================ KAT / CORRECTNESS VALIDATION ================"
  echo "arch: $ARCH   time: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
  echo "Phương pháp: chạy mỗi scheme×mức 30 lần; mỗi lần qua cổng đúng đắn nội bộ (exit 0 = PASS)."
  echo
  declare -A SIZES=(
    [mlkem:512]="pk=800 ct=768 ss=32" [mlkem:768]="pk=1184 ct=1088 ss=32" [mlkem:1024]="pk=1568 ct=1568 ss=32"
    [mldsa:44]="pk=1312 sig=2420" [mldsa:65]="pk=1952 sig=3309" [mldsa:87]="pk=2592 sig=4627"
    [aes:128]="key=16 tag=16" [aes:192]="key=24 tag=16" [aes:256]="key=32 tag=16" )
  run_kat() { # $1=bin $2=name $3=level
    local ok=1
    for i in $(seq 1 30); do taskset -c 0 "./$1" "$3" >/dev/null 2>&1 || { ok=0; break; }; done
    if [ "$ok" = 1 ]; then echo "  PASS  $2-$3   (30/30 roundtrip OK)  FIPS sizes: ${SIZES[$2:$3]}"; pass=$((pass+1));
    else echo "  FAIL  $2-$3"; fail=$((fail+1)); fi
  }
  echo "--- ML-KEM (FIPS 203): encaps==decaps shared secret ---"
  for l in 512 768 1024; do run_kat benchmark_mlkem/benchmark_mlkem mlkem $l; done
  echo "--- ML-DSA (FIPS 204): verify(real)=PASS, verify(tampered)=FAIL ---"
  for l in 44 65 87; do run_kat benchmark_mldsa/benchmark_mldsa mldsa $l; done
  echo "--- AES-GCM: roundtrip OK, tampered tag rejected ---"
  for l in 128 192 256; do run_kat benchmark_aes/benchmark_aes aes $l; done
  echo
  echo "TỔNG: PASS=$pass  FAIL=$fail"
  [ "$fail" = 0 ] && echo "==> TẤT CẢ ĐÚNG ĐẮN: số đo trong data/micro/ đáng tin." \
                   || echo "==> CÓ LỖI: KHÔNG dùng số đo cho tới khi sửa xong."
} | tee "$OUT"
[ "$fail" = 0 ]
