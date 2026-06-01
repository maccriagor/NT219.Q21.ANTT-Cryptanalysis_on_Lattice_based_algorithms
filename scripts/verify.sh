#!/usr/bin/env bash
# verify.sh — Kiểm chứng ĐÚNG ĐẮN implementation PQC (chạy TRONG container, chỉ cần terminal).
# Theo chuẩn nghiên cứu: roundtrip (liboqs + OpenSSL) + tamper-reject + KAT vs NIST.
#   Dùng:  verify.sh           (kiểm nhanh)
#          verify.sh --kat      (thêm KAT vs NIST — lâu hơn)
set -uo pipefail
T=/opt/build/liboqs/build/tests
P=0; F=0
ok(){ echo "  [PASS] $1"; P=$((P+1)); }
no(){ echo "  [FAIL] $1"; F=$((F+1)); }

echo "=== Tầng 1: liboqs functional roundtrip (keygen/encaps/decaps · sign/verify) ==="
"$T/test_kem" ML-KEM-768 >/dev/null 2>&1 && ok "liboqs test_kem ML-KEM-768" || no "liboqs test_kem ML-KEM-768"
"$T/test_sig" ML-DSA-65  >/dev/null 2>&1 && ok "liboqs test_sig ML-DSA-65"  || no "liboqs test_sig ML-DSA-65"

echo "=== Tầng 2: OpenSSL 3.6.2 native roundtrip (CLI chính thống) ==="
D="$(mktemp -d)"; cd "$D"
# ML-KEM: encap rồi decap -> 2 shared secret PHẢI giống nhau
openssl genpkey -algorithm ML-KEM-768 -out sk.pem 2>/dev/null
openssl pkey -in sk.pem -pubout -out pk.pem 2>/dev/null
openssl pkeyutl -encap -inkey pk.pem -pubin -out ct.bin -secret ssa.bin 2>/dev/null
openssl pkeyutl -decap -inkey sk.pem -in ct.bin -secret ssb.bin 2>/dev/null
cmp -s ssa.bin ssb.bin && ok "OpenSSL ML-KEM-768: shared secret khớp (encap=decap)" \
                       || no "OpenSSL ML-KEM-768: shared secret KHÔNG khớp"
# ML-DSA: sign/verify + kiểm chữ ký hỏng bị từ chối
echo "hello-pqc" > m.txt
openssl genpkey -algorithm ML-DSA-65 -out dsa.pem 2>/dev/null
if openssl pkeyutl -sign -rawin -in m.txt -inkey dsa.pem -out m.sig 2>/dev/null \
   && openssl pkeyutl -verify -rawin -in m.txt -inkey dsa.pem -sigfile m.sig 2>/dev/null; then
  ok "OpenSSL ML-DSA-65: sign/verify hợp lệ"
  cp m.sig m.bad; truncate -s -1 m.bad     # làm hỏng chữ ký
  openssl pkeyutl -verify -rawin -in m.txt -inkey dsa.pem -sigfile m.bad 2>/dev/null \
    && no "ML-DSA: chữ ký HỎNG vẫn verify (sai!)" || ok "OpenSSL ML-DSA-65: từ chối chữ ký hỏng"
else
  no "OpenSSL ML-DSA-65 sign/verify (thử bỏ -rawin / xem 'openssl pkeyutl -help')"
fi
cd / ; rm -rf "$D"

echo "=== Tầng 3: KAT vs NIST submission vectors (tùy chọn) ==="
if [ "${1:-}" = "--kat" ]; then
  if ninja -C /opt/build/liboqs/build run_tests >/tmp/kat.log 2>&1; then
    ok "KAT / run_tests (so với vector NIST)"
  else
    no "KAT thất bại — xem /tmp/kat.log"
  fi
else
  echo "  [SKIP] thêm '--kat' để chạy. Lệnh đầy đủ: ninja -C /opt/build/liboqs/build run_tests"
fi

echo ""
echo "================ KẾT QUẢ: PASS=$P  FAIL=$F ================"
if [ "$F" -eq 0 ]; then
  echo ">>> ĐÚNG ✔  — ghi 'correctness OK' vào nhật ký trước khi tin số benchmark."
else
  echo ">>> CÓ LỖI — KHÔNG dùng số đo cho tới khi sửa."; exit 1
fi
