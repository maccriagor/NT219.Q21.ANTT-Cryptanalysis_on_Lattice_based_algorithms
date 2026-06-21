# Implement-Benchmark-Lattice-based-Schemes

Benchmark ML-KEM / ML-DSA (Kyber, Dilithium — tên FIPS 203/204) so với RSA/ECC trên Linux **x86_64** và **ARM (Raspberry Pi 4)**. NT219 Capstone.

Người mới: làm theo đúng MỘT mục Quickstart tùy máy. Cùng một nhiệm vụ có thể có nhiều phương án công nghệ — xem "Bảng chọn phương án".

## Yêu cầu
- Ubuntu 22.04/24.04 (x86_64 hoặc ARM64; máy ARM thuê chọn image **Ubuntu**).
- sudo (chỉ bước cài gói). Mọi thứ khác cài vào `~/pqc`, không đụng OpenSSL hệ thống.

## Quickstart — PC x86_64
```bash
git clone <repo> && cd <repo> && chmod +x scripts/*.sh
bash scripts/install_prereqs.sh          # toolchain + công cụ đo (tshark -> Yes)
bash scripts/build_openssl.sh            # OpenSSL 3.6.2 PQC-native (chạy đủ test 1 lần)
source scripts/setenv.sh                 # MỖI terminal mới
bash scripts/verify_env.sh               # phải PASS -> docs/env_report_x86_64.txt
make                                     # bench_evp
bash scripts/build_liboqs.sh ref         # liboqs C thuần (opt: tùy chọn, xem bảng)
bash scripts/fetch_pqclean.sh clean
bash scripts/gen_tls_certs.sh            # cert RSA/ECDSA/ML-DSA cho WP4
# --- Đo (chi tiết K-batch + thứ tự: docs/KICH_BAN_TRIEN_KHAI.md) ---
sudo cpupower frequency-set -g performance
make bench && make memory && make codesize
make oqs                                 # liboqs ref vs opt -> data/bench_oqs
bash scripts/build_nginx.sh && bash nginx-bench/run.sh   # WP4 TLS handshake -> data/nginx_handshake
make analyze                             # bảng + biểu đồ -> analysis_out/
```

## Quickstart — ARM Raspberry Pi 4
```bash
git clone <repo> && cd <repo> && chmod +x scripts/*.sh
bash scripts/install_prereqs.sh
SKIP_TESTS=1 bash scripts/build_openssl.sh    # Pi build chậm; full test qua đêm 1 lần
source scripts/setenv.sh && bash scripts/verify_env.sh
make                                     # bench_evp
bash scripts/build_liboqs.sh ref              # BẮT BUỘC cả hai cây:
bash scripts/build_liboqs.sh opt              #   ref vs opt = thí nghiệm NEON (RQ2)
bash scripts/fetch_pqclean.sh clean && bash scripts/fetch_pqclean.sh aarch64
bash scripts/gen_tls_certs.sh
sudo cpupower frequency-set -g performance
make bench && make memory && make codesize    # nghỉ nguội giữa các batch (Pi throttle ~80°C)
make oqs                                      # liboqs ref vs opt (cần đủ ref+opt)
bash scripts/build_nginx.sh && bash nginx-bench/run.sh && make analyze
git add data analysis_out docs && git commit -m "aarch64" && git push   # TRƯỚC khi trả máy
```

## Bảng chọn phương án (cùng nhiệm vụ — nhiều công nghệ)

| Nhiệm vụ | Phương án | Lệnh | Chọn khi |
|---|---|---|---|
| **WP4: đo TLS 1.3 handshake** | A. **nginx** (build từ nguồn, link OpenSSL của ta) | `bash scripts/build_nginx.sh` rồi `bash nginx-bench/run.sh` | mặc định — server thật, trục cert × group → `data/nginx_handshake_<arch>.csv` |
| | B. **Track D** (illustrated-tls13, thin OpenSSL) | `make -C tls13-scratch` rồi chạy client | đo handshake latency phía client, ép từng group (X25519/X25519MLKEM768/MLKEM768) |
| **§7.6: TLS end-to-end "tự làm"** | **Track D** — client+server libssl ta tự build (mẫu illustrated-tls13 / OpenSSL guide) | `make -C tls13-scratch`; ép thuần PQC: `./tls13-scratch/client/client 127.0.0.1 8400 100 MLKEM768` | mức "tự triển khai" sâu nhất |
| **Môi trường ARM** | A. native trên máy ARM | Quickstart ARM | **nguồn số liệu duy nhất** |
| | B. cross-compile trên PC | `sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu && bash scripts/cross_build_liboqs.sh` | bằng chứng §7.3; KHÔNG đo |
| | C. Docker buildx (QEMU) | `bash scripts/docker_buildx.sh` | đóng hộp đa kiến trúc; KHÔNG đo trong QEMU |
| **WP2: microbenchmark** | A. `bench_evp` (EVP, tự viết) | `make bench` | số liệu chính (median/p95/CI, K-batch) |
| | B. `openssl speed` | `openssl speed rsa2048 ecdsap256` | đối chứng chéo công cụ |
| **WP3: liboqs ref/opt** | A. runner tự động ra CSV | `make oqs` | mặc định (ra `data/bench_oqs_<arch>.csv` cho analyze) |
| | B. chạy tay một thuật toán | `~/pqc/src/liboqs/build-ref/tests/speed_kem ML-KEM-768` | kiểm nhanh / debug |
| **Throughput đồng thời** | A. nhiều handshake nối tiếp qua nginx-bench | `ITERS=2000 bash nginx-bench/run.sh` | mọi group, kể cả PQC |
| | B. `wrk` | `wrk -t4 -c16 https://127.0.0.1:8443/` | CHỈ hàng X25519 (wrk link OpenSSL hệ thống, không có ML-KEM) — dùng đối chứng công cụ |
| **Build OpenSSL** | đủ test / bỏ test | mặc định / `SKIP_TESTS=1 bash scripts/build_openssl.sh` | đủ test ≥1 lần lấy bằng chứng |

## Kiểm tra kết quả (mỗi sản phẩm — một lệnh nghiệm thu)

| Sản phẩm | Lệnh kiểm |
|---|---|
| Môi trường | `bash scripts/verify_env.sh` → `PASS`; `openssl version` → 3.6.2 |
| Bằng chứng tái lập | `ls docs/*.commit docs/env_report_*` (5 commit + 2 report khi xong 2 máy) |
| Số WP2/WP5 | `head -3 data/summary_micro_$(uname -m).csv data/memory_*.csv data/codesize_*.csv` |
| Số WP3 | `head -5 data/bench_oqs_$(uname -m).csv` (ARM phải có cả ref lẫn opt) |
| Số WP4 | `head -5 data/nginx_handshake_*.csv`; nginx: `LD_LIBRARY_PATH=... ~/pqc/nginx/sbin/nginx -V` → "built with OpenSSL 3.6.2" |
| Bảng/biểu đồ | mở `analysis_out/tables.md` + các `.png` |
| Binary cross | `file ~/pqc/liboqs-aarch64-cross/lib/liboqs.so*` → "ARM aarch64" |
| Demo | làm theo `docs/DEMO.md` (quay màn hình, Deliverable 5) |

## Cấu trúc repo ↔ mẫu §17 của đề

| Trong repo | Vai trò (ánh xạ §17) |
|---|---|
| `scripts/` | build + cross-compile + deploy + đo (gộp "scripts/" và harness điều phối của "benchmarks/") |
| `src/` + `Makefile` | harness micro (`bench_evp`) + liboqs micro-bench (`bench_oqs`) — ruột của "benchmarks/micro"; server TLS ở `tls13-scratch/` + `nginx-bench/` |
| `data/`, `data/raw/<arch>/` | raw CSV + bằng chứng thô ("benchmarks/*/raw CSVs", deliverable §11.4) |
| `analysis_out/` | plots + bảng đã xử lý ("tools/parsers" → `scripts/analyze.py`) |
| `docker/`, `cmake/` | Dockerfile x86_64 + toolchain aarch64 ("docker/") |
| `docs/` (+ `docs/report/`) | tài liệu, kịch bản đo, báo cáo LaTeX ("docs/") |
| — energy | ngoài phạm vi (đề §13: thiếu Monsoon/INA → khai trong Limitations) |

## Tài liệu chi tiết
`docs/KICH_BAN_TRIEN_KHAI.md` (kịch bản đo đầy đủ 2 nền tảng, K-batch, checklist) · `docs/WP1_bang_tra_cuu.md` (tra nhanh 12 file môi trường) · `docs/WP1_chi_tiet_x86_vs_ARM.md` (vì sao hai máy) · `docs/WP1_huong_dan.pdf` (bản in) · `docs/report/main.tex` (khung báo cáo, XeLaTeX ×2)

## Lưu ý
- `setenv.sh` phải `source`, không chạy `./` (biến môi trường mất theo tiến trình con).
- Chạy lại script build thấy `Skipping` là đúng thiết kế; build lại thật: `FORCE=1`.
- Đổi phiên bản: sửa tag trong `scripts/versions.env`, xóa nguồn trong `~/pqc/src/`, `FORCE=1`.
- File `.sh` từ Windows lỗi `^M`: `sed -i 's/\r$//' scripts/*.sh`.
- Binary theo máy — không copy `~/pqc` chéo máy. Không commit khóa (`.gitignore` chặn).

## Biến môi trường — tự chỉnh mọi vòng lặp, không phụ thuộc mặc định

Mọi tham số đo đều ghi đè được lúc chạy (mặc định trong code chỉ là *fallback*). Cả hai cú pháp đều tới đích (đã test sống): `BENCH_ITERS=5000 make bench` **hoặc** `make bench BENCH_ITERS=5000`. Nhiều biến đặt cùng lúc: `BENCH_ITERS=5000 BENCH_WARMUP=50 make bench`.

| Nhóm | Biến (mặc định) | Ý nghĩa |
|---|---|---|
| **WP2** `make bench` | `BENCH_ITERS` (2000) | vòng đo cho ops nhanh (sign/verify/encaps/decaps) |
| | `BENCH_KEYGEN_ITERS` (50 nhánh RSA/EC · 200 nhánh PQC) | vòng keygen — RSA keygen ~10ms/vòng nên mặc định thấp hơn; override áp cho cả hai nhánh |
| | `BENCH_WARMUP` (20) | vòng nóng máy bị loại trước khi đo (§7.5) |
| | `BENCH_CSV=đường/dẫn` (tắt) | xuất mẫu thô từng vòng (bootstrap CI tuần 11) |
| **WP4 nginx** `bash nginx-bench/run.sh` | `ITERS` (1000) · `CERTS` ("rsa7680 ecp384 mldsa65") | handshake median/mean/p95 mỗi cert |
| | `KEX_GROUPS` ("X25519 X25519MLKEM768 MLKEM768") | trục group KEM |
| | `HOST` (127.0.0.1) · `PORT` (4433) | địa chỉ bind / đổi cổng |
| **Cert** `gen_tls_certs.sh` | `CERT_SET` ("rsa2048 ecp256 mldsa65") · `FORCE=1` | thêm mldsa44/87, rsa3072 |
| **WP3** `make oqs` | `OQS_ALGOS` (6 scheme) · `BENCH_ITERS` (2000) · `BENCH_KEYGEN_ITERS` (200) | thu hẹp khi debug |
| **WP5** `measure_codesize.sh` | `CODESIZE_LIBS="path1 path2"` | đo file tùy chọn (vd .a per-algo của PQClean) |
| **Track D** `make -C tls13-scratch` | CLI: `./client <host> <port> <count> <group>` | `count>1` = chế độ đo latency; ép group qua tham số thứ 4 |
| **Build** | `FORCE=1` (openssl/liboqs/nginx/certs) · `SKIP_TESTS=1` (openssl) · `USE_ARM_PMU=1` (liboqs opt, ARM) · `JOBS` (versions.env) | build lại / bỏ test suite / bật PMU sau khi nạp pqax |

Ví dụ tổ hợp hay dùng:

```bash
# Số "lấy thật" cho báo cáo (x86): vòng cao + mẫu thô
BENCH_WARMUP=50 BENCH_ITERS=5000 BENCH_KEYGEN_ITERS=300 make bench

# Pi 4: vòng vừa phải để batch ngắn, tránh chạm throttle
BENCH_ITERS=1500 make bench && bash nginx-bench/run.sh

# TLS tải nặng + ma trận mở rộng
CERT_SET="rsa2048 ecp256 mldsa44 mldsa65 mldsa87" FORCE=1 bash scripts/gen_tls_certs.sh
ITERS=2000 CERTS="rsa2048 mldsa44 mldsa65 mldsa87" bash nginx-bench/run.sh

# Ép Track D chạy thuần PQC, đo latency phía client
make -C tls13-scratch && ( cd tls13-scratch/server && ./server & )
./tls13-scratch/client/client 127.0.0.1 8400 100 MLKEM768
```

Cố ý **không** tham số hóa: danh sách 12 thuật toán trong `run_micro.sh`/`measure_memory.sh` — đó là *ma trận thí nghiệm* §7.1 (đổi nó là đổi thiết kế nghiên cứu, không phải chỉnh tham số chạy); cần đo lẻ một thuật toán thì gọi thẳng `./build/bench_evp mlkem 768`.
