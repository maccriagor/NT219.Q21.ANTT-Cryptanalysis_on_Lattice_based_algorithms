# CẨM NANG QUAY DEMO — NT219 PQC Capstone (Deliverable 5)

Chú ý ko commit nha:

═══════════════════ x86_64 ═══════════════════

1. Clone về: git clone https://github.com/maccriagor/NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms.git
2. Di chuyển vào repo: cd NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms
#Chú ý nếu out ra phiên terminal hiện tại thì chạy lại bước 2 và bước 4
3. Thêm quyền: chmod +x scripts/*
4. Thêm quyền: chmod +x tls13-scratch/*.sh

# Build môi trường microbenchmark (Default environment variable = 0)
5. [SKIP_TESTS=1|0] [FORCE=1|0] [JOBS=<n>] bash scripts/build_openssl.sh
SKIP_TESTS=1 : Bỏ make test (build nhanh hơn)
FORCE=1 : Cài lại OpenSSL (build đè bản cũ)
JOBS=<n> : Số luồng CPU biên dịch (Default JOBS = nproc = số lõi máy)
vd: SKIP_TESTS=1 JOBS=8 bash scripts/build_openssl.sh
output: ~/pqc/openssl/ + docs/openssl.commit

6. Kích hoạt OpenSSL (Mỗi terminal mới): source scripts/setenv.sh
vd: source scripts/setenv.sh   → in "Activated OpenSSL from: ~/pqc/openssl"

7. Kiểm tra môi trường (phải PASS): bash scripts/verify_env.sh
output: docs/env_report_x86_64.txt

[KHÔNG CẦN DEMO] [MICRO_ALGOS="..."] [BENCH_ITERS=<n>] [BENCH_KEYGEN_ITERS=<n>] [BENCH_WARMUP=<n>] make bench
MINCRO_ALOGOS : Thuật toán muốn đo trong 15 thuật toán (Default = 15 thuật toán)
  (
    "rsa 3072" "rsa 7680" "rsa 15360"
    "ecdsa p256" "ecdsa p384" "ecdsa p521"
    "ecdh p256" "ecdh p384" "ecdh p521"
    "mlkem 512" "mlkem 768" "mlkem 1024"
    "mldsa 44" "mldsa 65" "mldsa 87"
  )
BENCH_ITERS=<n>: Số vòng lặp đo các phép (encap/decap/sign/verify/derive). Mỗi op chạy n lần → lấy median/mean/p95/CI. (Default = 2000)
BENCH_KEYGEN_ITERS=<n> : Số vòng RIÊNG cho KEYGEN (tách ra vì keygen chậm hơn nhiều, nhất là RSA). (Default = 50)
BENCH_WARMUP=<n>       : Số vòng "làm nóng" chạy trước rồi BỎ kết quả (để CPU/cache ổn định, tránh lần đầu chậm bất thường). (Default = 20)
output: data/summary_micro_x86_64.csv + data/raw/x86_64/

# Build liboqs (ref = C thuần, opt = SIMD)
8. [FORCE=1|0] bash scripts/build_liboqs.sh ref
FORCE=1 : Build lại liboqs ref
vd: bash scripts/build_liboqs.sh ref
output: ~/pqc/liboqs-ref/

9. [FORCE=1|0] bash scripts/build_liboqs.sh opt
FORCE=1 : Build lại liboqs opt (AVX2)
vd: FORCE=1 bash scripts/build_liboqs.sh opt
output: ~/pqc/liboqs-neon/

[KHÔNG CẦN DEMO]. Build bench_oqs (2 bản): make bench_oqs
output: build/bench_oqs_ref + build/bench_oqs_opt

10. Lấy PQClean (cho code-size): bash scripts/fetch_pqclean.sh clean
output: ~/pqc/src/PQClean/ + docs/pqclean.commit

# Sinh cert TLS
11. [CERT_SET="<...>"] [FORCE=1|0] bash scripts/gen_tls_certs.sh
CERT_SET="..." : Danh sách cert sinh (Default = "rsa2048 ecp256 mldsa65")
FORCE=1 : Sinh lại cert đã có
vd: bash scripts/gen_tls_certs.sh
output: ~/pqc/tls/*.cert.pem + *.key.pem

12. Ghim xung CPU: sudo cpupower frequency-set -g performance

# ───────────── ĐO ─────────────

# Microbenchmark EVP 
13. [MICRO_ALGOS="fam param;..."] [BENCH_ITERS=<n>] [BENCH_KEYGEN_ITERS=<n>] [BENCH_WARMUP=<n>] make bench
MICRO_ALGOS="..." : Giới hạn thuật toán (Default = 15 algo đầy đủ)
BENCH_ITERS=<n> : Số vòng op nhanh (Default 2000)
BENCH_KEYGEN_ITERS=<n> : Số vòng keygen (Default 200)
BENCH_WARMUP=<n> : Số vòng làm nóng bỏ đi (Default 20)
vd: MICRO_ALGOS="mlkem 768;rsa 3072" BENCH_KEYGEN_ITERS=50 make bench
    → ==> mlkem 768 (ok) ... Summary: data/summary_micro_x86_64.csv
output: data/summary_micro_x86_64.csv + data/raw/x86_64/

# liboqs ref vs opt (WP3)
14. [OQS_ALGOS="..."] [BENCH_ITERS=<n>] [BENCH_KEYGEN_ITERS=<n>] make oqs
OQS_ALGOS="..." : Giới hạn thuật toán PQC (Default = 6 scheme)
vd: OQS_ALGOS="mlkem 768" make oqs   → ==> ref mlkem 768 (ok) / ==> opt mlkem 768 (ok)
output: data/bench_oqs_x86_64.csv

15. [KEMS="..."] [SIGS="..."] bash scripts/run_liboqs_speed.sh
KEMS="..." : KEM cho speed_kem (Default "ML-KEM-512 ML-KEM-768 ML-KEM-1024")
SIGS="..." : Sig cho speed_sig (Default "ML-DSA-44 ML-DSA-65 ML-DSA-87")
vd: KEMS="ML-KEM-768" SIGS="ML-DSA-65" bash scripts/run_liboqs_speed.sh
output: data/liboqs_speed_x86_64.csv

# Bộ nhớ + code size (WP5)
16. make codesize
output: data/codesize_x86_64.csv
17. make memory      (chậm: RSA-15360 ~vài phút)
output: data/memory_x86_64.csv

# TLS handshake (WP4)
18. [TLS_ITERS=<n>] [TLS_CONC=<n>] [TLS_DUR=<s>] [PORT=<n>] [CERTS="..."] [GROUP_LIST="..."] make tls
TLS_ITERS=<n> : Số handshake đo tuần tự (Default 50)
TLS_CONC=<n> : Số luồng đo throughput (Default 4)
TLS_DUR=<s> : Thời gian đo throughput, giây (Default 10)
PORT=<n> : Cổng server (Default 4433)
CERTS="..." : Cert dùng (Default "rsa2048 ecp256 mldsa65")
GROUP_LIST="..." : Nhóm key-exchange (Default "X25519 X25519MLKEM768 MLKEM768")
vd: TLS_ITERS=20 CERTS="mldsa65" GROUP_LIST="X25519MLKEM768" make tls
output: data/tls_handshake_x86_64.csv

19. [WORKERS_LIST="1 auto"] [LISTEN=<addr>] bash scripts/bench_tls_nginx.sh
WORKERS_LIST="..." : Số worker nginx thử (Default "1 auto")
LISTEN=<addr> : Địa chỉ bind (Default 127.0.0.1; LAN dùng 0.0.0.0)
vd: LISTEN=0.0.0.0 bash scripts/bench_tls_nginx.sh
output: data/tls_handshake_nginx-x86_64.csv

20. [DELAYS="..."] [ITERS=<n>] [LOSSES="..."] [GROUPS="..."] [CERT=<...>] sudo bash scripts/bench_rtt.sh {scratch|nginx}
{scratch|nginx} : Backend — TLS tự viết / nginx (BẮT BUỘC chọn 1)
DELAYS="..." : Độ trễ mỗi đầu, RTT≈2× (Default "0ms 2.5ms 15ms 39ms")
ITERS=<n> : Số handshake mỗi mức RTT (Default 200)
LOSSES="..." : % mất gói (Default "0")
vd: DELAYS="0ms 15ms" LOSSES="0 1" sudo bash scripts/bench_rtt.sh scratch
output: rtt_scratch/summary.csv (hoặc rtt_nginx/summary.csv)

21. (cần root) sudo make tlsnetem
output: data/netem_x86_64.csv

# ───────────── TỔNG HỢP + DEMO ─────────────

24. Gộp bảng + biểu đồ: make analyze
output: analysis_out/tables.md + analysis_out/*.png

═══════════════════ ARM (Raspberry Pi 4) ═══════════════════

1. Clone về: git clone https://github.com/maccriagor/NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms.git
2. Di chuyển vào repo: cd NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms
#Chú ý nếu out ra phiên terminal hiện tại thì chạy lại bước 2 và bước 6
3. Thêm quyền: chmod +x scripts/*
4. Thêm quyền: chmod +x tls13-scratch/*.sh

# Build OpenSSL — ⚠️ Pi chậm nên BẮT BUỘC bỏ test
5. SKIP_TESTS=1 [FORCE=1|0] [JOBS=<n>] bash scripts/build_openssl.sh
SKIP_TESTS=1 : Bỏ make test (Pi chạy full test rất lâu → luôn bật)
FORCE=1 : Cài lại OpenSSL
JOBS=<n> : Số luồng CPU (Default JOBS = nproc = 4 trên Pi 4)
vd: SKIP_TESTS=1 bash scripts/build_openssl.sh
output: ~/pqc/openssl/ + docs/openssl.commit

6. Kích hoạt OpenSSL (MỖI terminal mới): source scripts/setenv.sh
7. Kiểm tra môi trường: bash scripts/verify_env.sh
output: docs/env_report_aarch64.txt
8. Build benchmark: make
output: build/bench_evp

# Build liboqs — ⚠️ BẮT BUỘC CẢ 2 BẢN (NEON = thí nghiệm RQ2)
9. [USE_ARM_PMU=1|0] [FORCE=1|0] bash scripts/build_liboqs.sh ref
USE_ARM_PMU=1 : Bật bộ đếm chu kỳ PMU (cần module kernel)
FORCE=1 : Build lại
vd: bash scripts/build_liboqs.sh ref
output: ~/pqc/liboqs-ref/

10. [USE_ARM_PMU=1|0] [FORCE=1|0] bash scripts/build_liboqs.sh opt
(opt = NEON trên ARM)
vd: bash scripts/build_liboqs.sh opt
output: ~/pqc/liboqs-neon/

11. make bench_oqs
output: build/bench_oqs_ref + build/bench_oqs_opt

# PQClean — ⚠️ thêm bản NEON
12. bash scripts/fetch_pqclean.sh clean
13. bash scripts/fetch_pqclean.sh aarch64
output: ~/pqc/src/PQClean/

14. [CERT_SET="<...>"] [FORCE=1|0] bash scripts/gen_tls_certs.sh
CERT_SET="..." : Cert sinh (Default "rsa2048 ecp256 mldsa65")
output: ~/pqc/tls/*.pem

15. ⚠️ Ghim xung chống nóng: sudo cpupower frequency-set -g performance

# ───────────── ĐO (cùng núm env như x86, output đuôi _aarch64) ─────────────

16. [MICRO_ALGOS="..."] [BENCH_ITERS=<n>] [BENCH_KEYGEN_ITERS=<n>] [BENCH_WARMUP=<n>] make bench
(núm giống x86 — xem bước 15 phần x86)
vd: MICRO_ALGOS="mlkem 768;rsa 3072" BENCH_KEYGEN_ITERS=50 make bench
output: data/summary_micro_aarch64.csv

17. [OQS_ALGOS="..."] make oqs      (⚠️ cần đủ ref+opt)
output: data/bench_oqs_aarch64.csv

18. [KEMS="..."] [SIGS="..."] bash scripts/run_liboqs_speed.sh
output: data/liboqs_speed_aarch64.csv

19. make codesize
output: data/codesize_aarch64.csv
20. make memory      (⚠️ nghỉ nguội giữa batch ~80°C)
output: data/memory_aarch64.csv

21. [TLS_ITERS=<n>] [CERTS="..."] [GROUP_LIST="..."] make tls
vd: TLS_ITERS=20 CERTS="mldsa65" make tls
output: data/tls_handshake_aarch64.csv

22. [WORKERS_LIST="1 auto"] bash scripts/bench_tls_nginx.sh
output: data/tls_handshake_nginx-aarch64.csv

23. [DELAYS="..."] [ITERS=<n>] [LOSSES="..."] sudo bash scripts/bench_rtt.sh {scratch|nginx}
output: rtt_{scratch|nginx}/summary.csv

24. sudo make tlsnetem
output: data/netem_aarch64.csv

# ───────────── TỔNG HỢP + DEMO + LƯU ─────────────

25. make analyze
output: analysis_out/tables.md + *.png

26. [PORT=<n>] bash scripts/demo.sh
output: in màn hình

27. [BATCHES=<n>] bash run_arm.sh      (⚠️ ARM: run_arm.sh, nghỉ nguội 120s/batch)
BATCHES=<n> : Số batch K (Default 5)
output: data/raw/aarch64/summary_batch*.csv + data/summary_micro_aarch64.csv

28. ⚠️ Lưu kết quả TRƯỚC khi trả Pi:
git add data analysis_out docs && git commit -m "aarch64" && git push