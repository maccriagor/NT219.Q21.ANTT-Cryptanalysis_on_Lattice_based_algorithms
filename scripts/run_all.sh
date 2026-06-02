#!/usr/bin/env bash
# run_all.sh — Chạy TOÀN BỘ pipeline đồ án bằng 1 lệnh (đo thật x86 + sinh số mô hình + figures).
# Trên Raspberry Pi (aarch64): các benchmark tự ghi vào data/micro/arm/ (runner tự nhận arch).
#
#   1) build libpqc.a (PQClean reference, đủ 6 param-set)
#   2) 3 benchmark PQC đa mức NIST  -> data/micro/<arch>/{mlkem,mldsa,aes}_<arch>.csv  (100/mức)
#   3) KAT / correctness            -> data/kat/kat_results.txt
#   4) (x86) bản tối ưu AVX2        -> data/micro/x86/micro_evp_x86.csv          [cần podman]
#   5) (x86) TLS 1.3 PQC handshake  -> data/tls/x86/handshake_x86.txt            [cần podman]
#   6) capture_env                  -> data/env/env_linux-<arch>.txt
#   7) tổng hợp + sinh số mô hình   -> summary + arm/ + neon + energy + netem
#   8) figures SVG                  -> docs/report/figures/*.svg
set -euo pipefail
cd "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
ARCH="$(uname -m)"; HAVE_PODMAN=0; command -v podman >/dev/null 2>&1 && HAVE_PODMAN=1
command -v docker >/dev/null 2>&1 && HAVE_PODMAN=1

echo "############ [1/8] build libpqc.a ############"
bash scripts/build_pqc_lib.sh

echo "############ [2/8] microbenchmark PQC đa mức NIST ############"
( cd benchmark_mlkem && bash build_run.sh )
( cd benchmark_mldsa && bash build_run.sh )
( cd benchmark_aes   && bash build_run.sh )

echo "############ [3/8] KAT / correctness ############"
bash scripts/run_kat.sh || echo "⚠️ KAT có lỗi — xem data/kat/kat_results.txt"

if [ "$ARCH" = "x86_64" ] && [ "$HAVE_PODMAN" = 1 ]; then
  echo "############ [4/8] bản tối ưu AVX2 (OpenSSL EVP, container) ############"
  bash scripts/run_micro_evp.sh || echo "⚠️ EVP harness lỗi (bỏ qua)"
  echo "############ [5/8] TLS 1.3 PQC handshake (container) ############"
  bash scripts/run_tls_handshake.sh || echo "⚠️ TLS handshake lỗi (bỏ qua)"
else
  echo "############ [4-5/8] BỎ QUA bản tối ưu + TLS (cần x86 + podman) ############"
fi

echo "############ [6/8] capture_env ############"
bash scripts/capture_env.sh data/env || true

echo "############ [7/8] tổng hợp + sinh số mô hình ############"
python3 scripts/build_dataset.py

echo "############ [8/8] figures SVG ############"
python3 tools/plot.py

echo
echo "############ HOÀN TẤT — kết quả trong data/ + docs/report/figures/ ############"
echo "Đo thật vs mô hình: docs/DATA_PROVENANCE.md · Kết quả + RQ: docs/RESULTS.md"
find data -name '*.csv' -o -name '*.txt' | grep -v .gitkeep | sed 's/^/  /' | sort
