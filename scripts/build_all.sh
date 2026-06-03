#!/usr/bin/env bash
# ============================================================================
# scripts/build_all.sh
# "Người điều phối" — dựng TOÀN BỘ stack PQC bằng MỘT lệnh, đúng thứ tự phụ thuộc:
#
#     build_openssl.sh  →  build_liboqs.sh  →  build_provider.sh
#        (nền tảng)          (cần OpenSSL)        (cần OpenSSL + liboqs)
#
# Thứ tự KHÔNG đảo được: liboqs cần OPENSSL_ROOT_DIR; oqs-provider cần cả
# OPENSSL_ROOT_DIR lẫn liboqs_DIR. Chạy: ./scripts/build_all.sh
# ============================================================================
set -euo pipefail

# Về thư mục scripts/ (để gọi các script anh em bằng đường dẫn chắc chắn)
cd "$(dirname "$0")"
ROOT="$(cd .. && pwd)"           # gốc repo (để in thông báo)
source ./versions.env            # nạp version + prefix dùng chung

log() { printf '\n\033[1;36m==> %s\033[0m\n' "$*"; }

START=$(date +%s)

log "1/3  Build OpenSSL $OPENSSL_TAG  →  $OSSL_PREFIX"
./build_openssl.sh

log "2/3  Build liboqs $LIBOQS_TAG  (2 cây: reference C vs NEON)"
./build_liboqs.sh

log "3/3  Build oqs-provider $OQSPROVIDER_TAG  + activate openssl.cnf"
./build_provider.sh

# --- Kiểm chứng cuối: PQC đã sẵn sàng chưa ---
log "Kiểm chứng: liệt kê thuật toán PQC native"
"$OSSL_PREFIX/bin/openssl" list -kem-algorithms        2>/dev/null | grep -i mlkem || true
"$OSSL_PREFIX/bin/openssl" list -signature-algorithms  2>/dev/null | grep -i mldsa || true

END=$(date +%s)
log "HOÀN TẤT toàn bộ stack trong $((END-START))s."
echo ">> OpenSSL:     $OSSL_PREFIX"
echo ">> liboqs NEON: $LIBOQS_PREFIX_OPT   (dùng để ĐO)"
echo ">> liboqs ref:  $LIBOQS_PREFIX_REF   (đối chứng WP3)"
echo ">> provider:    $PROVIDER_PREFIX"
echo
echo ">> Bước tiếp: build harness  ->  make OSSLROOT=$OSSL_PREFIX"
echo ">>            chuẩn hóa CPU  ->  sudo ./scripts/setup_governor.sh"
echo ">>            ghi môi trường ->  ./scripts/collect_env.sh"