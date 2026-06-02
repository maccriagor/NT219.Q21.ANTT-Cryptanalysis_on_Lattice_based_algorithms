#!/usr/bin/env bash
# run_tls_handshake.sh — WP4: bắt tay TLS 1.3 THẬT (localhost) trong container OpenSSL 3.6 native.
# So 3 cấu hình ở cùng mức Cat: (1) cổ điển ECDHE-P256 + cert ECDSA-P256,
# (2) PQC hybrid X25519MLKEM768 + cert ML-DSA-65, (3) đo bytes-on-wire + nhóm thương lượng.
# Kết quả -> data/tls/x86/  (handshake_x86.txt + sizes.csv). Đây là ANCHOR thật; ma trận netem
# (độ trễ × mất gói) là MÔ HÌNH dẫn xuất (scripts/build_dataset.py).
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"; cd "$ROOT"
IMG="${IMG:-pqc:wp1-amd64}"; mkdir -p data/tls/x86
podman run --rm -v "$PWD:/work:z" -w /work "$IMG" sh -lc '
  set -e; export PATH=/opt/openssl/bin:$PATH LD_LIBRARY_PATH=/opt/openssl/lib64
  cd /tmp
  OUT=/work/data/tls/x86; mkdir -p "$OUT"
  # Image OpenSSL 3.6 không có openssl.cnf mặc định -> cấp config tối thiểu cho `req`.
  printf "[req]\ndistinguished_name=dn\nprompt=no\n[dn]\nCN=pqc-demo\n" > min.cnf
  CFG="-config min.cnf"
  echo "=== OpenSSL: $(openssl version) ===" | tee $OUT/handshake_x86.txt

  # --- Chứng chỉ cổ điển (ECDSA P-256) ---
  openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 -keyout ec.key -out ec.crt \
    -days 30 -nodes -subj "/CN=pqc-demo-classical" $CFG >/dev/null 2>&1
  # --- Chứng chỉ hậu lượng tử (ML-DSA-65) ---
  openssl req -x509 -newkey ml-dsa-65 -keyout mldsa.key -out mldsa.crt \
    -days 30 -nodes -subj "/CN=pqc-demo-mldsa" $CFG >/dev/null 2>&1
  echo "cert,pub_alg,cert_bytes" > $OUT/sizes.csv
  echo "classical-ECDSA-P256,$(openssl x509 -in ec.crt -noout -text | grep -m1 "Public Key Algorithm" | sed "s/.*: //"),$(wc -c <ec.crt)" >> $OUT/sizes.csv
  echo "pqc-ML-DSA-65,$(openssl x509 -in mldsa.crt -noout -text | grep -m1 "Public Key Algorithm" | sed "s/.*: //"),$(wc -c <mldsa.crt)" >> $OUT/sizes.csv

  run_case() { # $1=label $2=cert $3=key $4=groups
    local L="$1" C="$2" K="$3" G="$4"
    openssl s_server -accept 4433 -cert "$C" -key "$K" -tls1_3 -groups "$G" -www -quiet >/dev/null 2>&1 &
    local SP=$!; sleep 0.6
    echo "" | tee -a $OUT/handshake_x86.txt
    echo "===== CASE: $L  (groups=$G) =====" | tee -a $OUT/handshake_x86.txt
    echo "Q" | openssl s_client -connect localhost:4433 -tls1_3 -groups "$G" 2>/dev/null \
      | grep -iE "Negotiated|Protocol|Cipher|Peer signature|Server Temp Key|Verify return" \
      | tee -a $OUT/handshake_x86.txt || true
    # đo throughput handshake/s (s_time, 3s)
    echo -n "handshakes/sec: " | tee -a $OUT/handshake_x86.txt
    openssl s_time -connect localhost:4433 -new -time 3 2>/dev/null | grep -oE "[0-9.]+ connections/user sec" \
      | tee -a $OUT/handshake_x86.txt || echo "n/a" | tee -a $OUT/handshake_x86.txt
    kill $SP 2>/dev/null || true; wait $SP 2>/dev/null || true
  }
  run_case "classical (ECDHE-P256 + ECDSA cert)" ec.crt ec.key "P-256"
  run_case "PQC-hybrid (X25519MLKEM768 + ML-DSA-65 cert)" mldsa.crt mldsa.key "X25519MLKEM768"
  echo "" | tee -a $OUT/handshake_x86.txt
  echo "=== cert sizes ===" | tee -a $OUT/handshake_x86.txt
  cat $OUT/sizes.csv | tee -a $OUT/handshake_x86.txt
  echo "DONE_TLS"
'
