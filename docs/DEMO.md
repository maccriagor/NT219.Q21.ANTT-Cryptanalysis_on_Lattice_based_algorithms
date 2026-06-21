# CẨM NANG QUAY DEMO — NT219 PQC Capstone (Deliverable 5)

Chú ý ko commit nha (chỉ commit `data/ analysis_out/ docs/` ở bước cuối).

═══════════════════ x86_64 ═══════════════════

1. Clone về: git clone https://github.com/maccriagor/NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms.git
2. Di chuyển vào repo: cd NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms
#Chú ý nếu out ra phiên terminal hiện tại thì chạy lại bước 2 và bước 5 (source setenv.sh)
3. Thêm quyền: chmod +x scripts/*

# Build môi trường microbenchmark (Default environment variable = 0)
4. [SKIP_TESTS=1|0] [FORCE=1|0] [JOBS=<n>] bash scripts/build_openssl.sh
SKIP_TESTS=1 : Bỏ make test (build nhanh hơn)
FORCE=1 : Cài lại OpenSSL (build đè bản cũ)
JOBS=<n> : Số luồng CPU biên dịch (Default JOBS = nproc = số lõi máy)
vd: SKIP_TESTS=1 JOBS=8 bash scripts/build_openssl.sh
output: ~/pqc/openssl/ + docs/openssl.commit

5. Kích hoạt OpenSSL (Mỗi terminal mới): source scripts/setenv.sh
vd: source scripts/setenv.sh   → in "Activated OpenSSL from: ~/pqc/openssl"

6. Kiểm tra môi trường (phải PASS): bash scripts/verify_env.sh
output: docs/env_report_x86_64.txt

# Build liboqs (ref = C thuần, opt = SIMD/AVX2)
7. [FORCE=1|0] bash scripts/build_liboqs.sh ref
output: ~/pqc/liboqs-ref/
8. [FORCE=1|0] bash scripts/build_liboqs.sh opt
output: ~/pqc/liboqs-neon/

9. Lấy PQClean (cho code-size): bash scripts/fetch_pqclean.sh clean
output: ~/pqc/src/PQClean/ + docs/pqclean.commit

# Sinh cert TLS (KHỚP với CERTS của nginx-bench bên dưới)
10. [CERT_SET="<...>"] [FORCE=1|0] bash scripts/gen_tls_certs.sh
CERT_SET="..." : Danh sách cert sinh (Default = "rsa2048 ecp256 mldsa65")
vd (Cat-3, khớp nginx-bench): CERT_SET="rsa7680 ecp384 mldsa65" bash scripts/gen_tls_certs.sh
output: ~/pqc/tls/*.cert.pem + *.key.pem

# Build nginx (link OpenSSL 3.6.2 của ta) — cho đo TLS handshake
11. [FORCE=1|0] bash scripts/build_nginx.sh
output: ~/pqc/nginx/ + docs/nginx.commit

12. Ghim xung CPU: sudo cpupower frequency-set -g performance

# ───────────── ĐO ─────────────

# Microbenchmark EVP (WP2 — 15 thuật toán)
13. [MICRO_ALGOS="fam param;..."] [BENCH_ITERS=<n>] [BENCH_KEYGEN_ITERS=<n>] [BENCH_WARMUP=<n>] make bench
MICRO_ALGOS="..." : Giới hạn thuật toán (Default = 15 algo:
  rsa 3072/7680/15360, ecdsa p256/p384/p521, ecdh p256/p384/p521,
  mlkem 512/768/1024, mldsa 44/65/87)
BENCH_ITERS=<n> : Số vòng op nhanh encap/decap/sign/verify/derive (Default 2000)
BENCH_KEYGEN_ITERS=<n> : Số vòng RIÊNG cho keygen (Default 200; RSA chậm nên tách)
BENCH_WARMUP=<n> : Số vòng làm nóng bỏ đi (Default 20)
vd: MICRO_ALGOS="mlkem 768;rsa 3072" BENCH_KEYGEN_ITERS=50 make bench
output: data/summary_micro_x86_64.csv + data/raw/x86_64/

# liboqs ref vs opt (WP3 — hiệu ứng SIMD/NEON)
14. [OQS_ALGOS="..."] [BENCH_ITERS=<n>] [BENCH_KEYGEN_ITERS=<n>] make oqs
OQS_ALGOS="..." : Giới hạn thuật toán PQC (Default = 6 scheme)
vd: OQS_ALGOS="mlkem 768" make oqs   → ==> ref mlkem 768 (ok) / ==> opt mlkem 768 (ok)
output: data/bench_oqs_x86_64.csv

# Bộ nhớ + code size (WP5)
15. make codesize
output: data/codesize_x86_64.csv
16. make memory      (chậm: RSA-15360 ~vài phút)
output: data/memory_x86_64.csv

# TLS 1.3 handshake qua nginx (WP4)
17. [ITERS=<n>] [CERTS="..."] [KEX_GROUPS="..."] [HOST=<addr>] [PORT=<n>] bash nginx-bench/run.sh
ITERS=<n> : Số handshake mỗi (cert × group) (Default 1000)
CERTS="..." : Cert dùng — phải đã sinh ở bước 10 (Default "rsa7680 ecp384 mldsa65")
KEX_GROUPS="..." : Nhóm key-exchange (Default "X25519 X25519MLKEM768 MLKEM768" = cổ điển/hybrid/PQC thuần)
HOST/PORT : bind nginx (Default 127.0.0.1:4433)
vd: ITERS=2000 CERTS="mldsa65" KEX_GROUPS="X25519MLKEM768" bash nginx-bench/run.sh
output: data/nginx_handshake_x86_64.csv

# TLS 1.3 handshake "tự làm" — Track D (illustrated-tls13, thin OpenSSL)
18. Build + chạy:
    make -C tls13-scratch                          # build client + server
    make -C tls13-scratch/server cert              # cert self-signed (hoặc cp cert mldsa65 vào để test PQC auth)
    ( cd tls13-scratch/server && ./server ) &      # server cổng 8400
    ./tls13-scratch/client/client 127.0.0.1 8400 100 X25519            # 100 handshake cổ điển
    ./tls13-scratch/client/client 127.0.0.1 8400 100 X25519MLKEM768    # 100 handshake hybrid
    ./tls13-scratch/client/client 127.0.0.1 8400 100 MLKEM768          # 100 handshake PQC thuần
output: in màn hình "t1,t2,...,t100" (ms) — tự lấy median/mean/p95

# ───────────── TỔNG HỢP ─────────────

19. Gộp bảng + biểu đồ: make analyze
(đọc summary_micro / memory / codesize / bench_oqs / nginx_handshake)
output: analysis_out/tables.md + analysis_out/*.png

═══════════════════ ARM (Raspberry Pi 4) ═══════════════════

1. Clone về: git clone https://github.com/maccriagor/NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms.git
2. Di chuyển vào repo: cd NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms
#Chú ý nếu out ra phiên terminal hiện tại thì chạy lại bước 2 và bước 5 (source setenv.sh)
3. Thêm quyền: chmod +x scripts/*

# Build OpenSSL — ⚠️ Pi chậm nên BẮT BUỘC bỏ test
4. SKIP_TESTS=1 [FORCE=1|0] [JOBS=<n>] bash scripts/build_openssl.sh
vd: SKIP_TESTS=1 bash scripts/build_openssl.sh
output: ~/pqc/openssl/ + docs/openssl.commit

5. Kích hoạt OpenSSL (MỖI terminal mới): source scripts/setenv.sh
6. Kiểm tra môi trường: bash scripts/verify_env.sh
output: docs/env_report_aarch64.txt

# Build liboqs — ⚠️ BẮT BUỘC CẢ 2 BẢN (NEON = thí nghiệm RQ2)
7. [USE_ARM_PMU=1|0] [FORCE=1|0] bash scripts/build_liboqs.sh ref
USE_ARM_PMU=1 : Bật bộ đếm chu kỳ PMU (cần module kernel pqax)
output: ~/pqc/liboqs-ref/
8. [USE_ARM_PMU=1|0] [FORCE=1|0] bash scripts/build_liboqs.sh opt   (opt = NEON trên ARM)
output: ~/pqc/liboqs-neon/

# PQClean — ⚠️ thêm bản aarch64
9. bash scripts/fetch_pqclean.sh clean
10. bash scripts/fetch_pqclean.sh aarch64
output: ~/pqc/src/PQClean/

11. [CERT_SET="<...>"] [FORCE=1|0] bash scripts/gen_tls_certs.sh
vd: CERT_SET="rsa7680 ecp384 mldsa65" bash scripts/gen_tls_certs.sh
output: ~/pqc/tls/*.pem

12. [FORCE=1|0] bash scripts/build_nginx.sh
output: ~/pqc/nginx/ + docs/nginx.commit

13. ⚠️ Ghim xung chống nóng: sudo cpupower frequency-set -g performance

# ───────────── ĐO (cùng núm env như x86, output đuôi _aarch64) ─────────────

14. [MICRO_ALGOS="..."] [BENCH_ITERS=<n>] [BENCH_KEYGEN_ITERS=<n>] [BENCH_WARMUP=<n>] make bench
vd: MICRO_ALGOS="mlkem 768;rsa 3072" BENCH_KEYGEN_ITERS=50 make bench
output: data/summary_micro_aarch64.csv

15. [OQS_ALGOS="..."] make oqs      (⚠️ cần đủ ref+opt)
output: data/bench_oqs_aarch64.csv

16. make codesize
output: data/codesize_aarch64.csv
17. make memory      (⚠️ nghỉ nguội giữa batch ~80°C)
output: data/memory_aarch64.csv

18. [ITERS=<n>] [CERTS="..."] [KEX_GROUPS="..."] bash nginx-bench/run.sh
output: data/nginx_handshake_aarch64.csv

19. Track D (như x86 bước 18):
    make -C tls13-scratch && make -C tls13-scratch/server cert
    ( cd tls13-scratch/server && ./server ) &
    ./tls13-scratch/client/client 127.0.0.1 8400 100 X25519MLKEM768
output: in màn hình (ms)

# ───────────── TỔNG HỢP + LƯU ─────────────

20. make analyze
output: analysis_out/tables.md + *.png

21. [BATCHES=<n>] bash run_arm.sh      (⚠️ ARM: run_arm.sh, nghỉ nguội 120s/batch)
BATCHES=<n> : Số batch K (Default 5)
output: data/raw/aarch64/summary_batch*.csv + data/summary_micro_aarch64.csv

22. ⚠️ Lưu kết quả TRƯỚC khi trả Pi:
git add data analysis_out docs && git commit -m "aarch64" && git push
