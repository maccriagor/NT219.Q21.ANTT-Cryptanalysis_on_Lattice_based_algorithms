# RUNBOOK — Tái lập từ máy sạch (Reproduce from a clean machine)

> **Phạm vi (NT2205 §9).** Hướng dẫn dựng lại toàn bộ kết quả từ một máy sạch: prerequisites → build thư viện PQC → chạy 3 microbench đa mức NIST → KAT → harness tối ưu (AVX2) → handshake TLS 1.3 → tổng hợp dataset + sinh số mô hình → biểu đồ. Có **đường ARM/Raspberry Pi** riêng. Mỗi đầu ra ghi rõ **MEASURED (x86, đo thật)** hay **MODELED (ARM/energy/netem, dẫn xuất)**. Liên kết: kiến trúc [`docs/ARCH.md`](ARCH.md), đánh giá [`docs/EVAL.md`](EVAL.md).
>
> **Base directory:** `/home/chung-tra274/Desktop/NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms/` (gọi tắt là `$REPO`). Mọi lệnh chạy từ `$REPO`.

---

## 0) Đường tắt — chạy TẤT CẢ bằng 1 lệnh

```bash
cd "$REPO"
bash scripts/run_all.sh
# tùy chọn số vòng đo: N=5000 WARM=200 bash scripts/run_all.sh
```

Pipeline `run_all.sh` (8 bước, tự nhận kiến trúc `x86_64`/`aarch64`):

| Bước | Hành động | Đầu ra | Loại |
|---|---|---|---|
| 1 | build `libpqc.a` (PQClean ref, 6 param-set) | `build/libpqc.a` | — |
| 2 | 3 microbench đa mức NIST (×100/mức) | `data/micro/<arch>/{mlkem,mldsa,aes}_<arch>.csv` | **MEASURED** |
| 3 | KAT / correctness | `data/kat/kat_results.txt` | **MEASURED** |
| 4 | (x86) bản tối ưu AVX2 (OpenSSL EVP, container) | `data/micro/x86/micro_evp_x86.csv` | **MEASURED** |
| 5 | (x86) TLS 1.3 PQC handshake (container) | `data/tls/x86/handshake_x86.txt`, `sizes.csv` | **MEASURED** |
| 6 | capture_env | `data/env/env_linux-<arch>.txt` | **MEASURED** |
| 7 | tổng hợp + sinh số mô hình | `summary_x86.csv` + `arm/` + `neon_vs_ref` + `energy` + `netem` | summary=MEASURED; arm/energy/netem=**MODELED** |
| 8 | figures SVG | `docs/report/figures/*.svg` | hỗn hợp (figure ghi rõ) |

> Bước 4–5 **chỉ chạy trên x86 + có `podman`/`docker`** (cần image `pqc:wp1-amd64`, OpenSSL 3.6 native). Trên ARM hoặc thiếu container → tự bỏ qua. Các mục dưới đây giải thích từng bước để chạy/độc lập hoá khi cần.

---

## 1) Prerequisites (máy sạch)

**Bắt buộc (microbench + KAT, mọi nền tảng):**

```bash
sudo apt-get update
sudo apt-get install -y build-essential g++ gcc make python3 libssl-dev
```

| Công cụ | Vai trò | Ghi chú phiên bản (đã kiểm thử) |
|---|---|---|
| `g++` / `gcc` | Biên dịch benchmark C++ + PQClean C | gcc 13.3 (x86) / gcc 12 (ARM) |
| `libssl-dev` (OpenSSL) | AES-GCM (`benchmark_aes`), harness | OpenSSL 3.0.13 (x86 hệ thống) / 3.6.1 (TLS) |
| `python3` | runner đa mức + tổng hợp dataset + plot | 3.10+ |
| `taskset` (util-linux) | Ghim core 0 khi đo (không cần sudo) | có sẵn |

**Tùy chọn (bước tối ưu AVX2 + TLS handshake — cần OpenSSL 3.6 native ML-KEM/ML-DSA):**

```bash
sudo apt-get install -y podman    # hoặc docker
# image build env: pqc:wp1-amd64 (OpenSSL 3.6.x + liboqs + PQClean), xem docker/Dockerfile.* + Dockerfile
```

> `tools/plot.py` (bước 8) sinh **SVG thuần**, **không cần** matplotlib/numpy. Nếu chỉ cần số liệu thô, có thể bỏ qua bước plot.

**Lấy nguồn PQClean (nếu `vendor/pqclean/` trống):**

```bash
bash scripts/fetch_sources.sh    # prefetch nguồn upstream vào vendor/ (cần host có mạng)
```

---

## 2) Build thư viện PQC (PQClean reference, đủ mọi mức NIST)

```bash
bash scripts/build_pqc_lib.sh            # tạo build/libpqc.a
# build lại từ đầu: bash scripts/build_pqc_lib.sh --force
```

- Biên dịch **6 param-set**: `mlkem512/768/1024` + `mldsa44/65/87` + `common` bằng `gcc -O2 -c`, gộp `ar` thành `build/libpqc.a`.
- Mỗi `.c` build với `-I<thư-mục-của-chính-nó>` đứng trước để thấy đúng `params.h` của mức đó (symbol đã namespace `PQCLEAN_MLKEM512_…` nên gộp 1 lib không đụng nhau).

---

## 3) Chạy 3 microbench đa mức NIST (×100/mức) — **MEASURED (x86)**

Mỗi thư mục có `build_run.sh` = `build.sh` (link `libpqc.a`) + `runner.py` (quét đủ 3 mức × N lần từ `input.txt`):

```bash
cd "$REPO/benchmark_mlkem" && ./build_run.sh    # ML-KEM 512/768/1024  (Cat 1/3/5)
cd "$REPO/benchmark_mldsa" && ./build_run.sh    # ML-DSA 44/65/87      (Cat 2/3/5)
cd "$REPO/benchmark_aes"   && ./build_run.sh    # AES-GCM 128/192/256  (Cat 1/3/5)
```

**Đầu ra (300 dòng/file = 3 mức × 100):**

| File | Cột chính |
|---|---|
| [`data/micro/x86/mlkem_x86.csv`](../data/micro/x86/mlkem_x86.csv) | `Iter,Param,Category,Keygen_Cyc,Keygen_ns,Encaps_…,Decaps_…` |
| [`data/micro/x86/mldsa_x86.csv`](../data/micro/x86/mldsa_x86.csv) | `…,Keygen_ns,Sign_ns,Verify_ns` |
| [`data/micro/x86/aes_x86.csv`](../data/micro/x86/aes_x86.csv) | `…,Encrypt_ns,Decrypt_ns` |

> Runner tự nhận kiến trúc (`x86_64`→`x86`, `aarch64`→`arm`), ghim core 0 bằng `taskset`, timer cycle+ns. **Kỳ vọng (đo thật x86):** ML-KEM-1024 K/E/D ≈ 82/83/94 µs; ML-DSA-87 K/S/V ≈ 267/563/239 µs (sign p95 ~1.5 ms — rejection sampling).

---

## 4) KAT / Correctness — **MEASURED (x86)** — *chạy TRƯỚC khi tin số đo*

```bash
bash scripts/run_kat.sh        # -> data/kat/kat_results.txt ; exit 0 = tất cả PASS
```

- Mỗi scheme×mức chạy **30 lần** qua **cổng đúng đắn nội bộ**: ML-KEM `ss_encaps==ss_decaps`; ML-DSA `verify(real)=PASS & verify(tampered)=FAIL`; AES-GCM roundtrip + **tampered tag rejected**. Đối chiếu pk/ct/sig với FIPS 203/204.
- **Kỳ vọng:** `TỔNG: PASS=9 FAIL=0` (xem [`data/kat/kat_results.txt`](../data/kat/kat_results.txt)). Đây là cổng cho **E-C3** / invariants I2, I3.

---

## 5) Harness tối ưu AVX2 — **MEASURED (x86)** — *cần podman + OpenSSL 3.6*

```bash
bash scripts/run_micro_evp.sh   # -> data/micro/x86/micro_evp_x86.csv
# chỉ định image khác: IMG=pqc:wp1-amd64 bash scripts/run_micro_evp.sh
```

- Chạy `benchmarks/micro/pqc_bench.cpp` (OpenSSL EVP native) trong container: KEM (ML-KEM), KEX (ECDH), SIG (ML-DSA/ECDSA/RSA) — bản **AVX2 tối ưu** + **baseline cổ điển** (ECDH/ECDSA P-256/384/521, RSA-3072/7680/15360).
- **Kỳ vọng:** ML-KEM-1024 AVX2 K/E/D ≈ 46/22/35 µs; baseline ECDH-P-521 derive ≈ 1284 µs, RSA-15360 keygen ≈ 15.7 s. Dùng cho **RQ1** (reference vs optimized) và headline PQC-vs-cổ-điển ([`docs/EVAL.md`](EVAL.md) §RQ1).

---

## 6) TLS 1.3 handshake (hybrid vs cổ điển) — **MEASURED (x86)** — *cần podman + OpenSSL 3.6*

```bash
bash scripts/run_tls_handshake.sh    # -> data/tls/x86/handshake_x86.txt + sizes.csv
```

- Sinh 2 cert (ECDSA-P256 + **ML-DSA-65**), chạy `s_server`/`s_client`/`s_time` cho 2 cấu hình cùng mức: cổ điển `P-256` và **hybrid `X25519MLKEM768`**.
- **Kỳ vọng (đo thật):** `Negotiated TLS1.3 group: X25519MLKEM768`, `Peer signature type: mldsa65`; throughput **1934 hs/s** (hybrid) vs **2118 hs/s** (cổ điển) ≈ **9% overhead**; cert **7464 B** (ML-DSA-65) vs **534 B** (ECDSA-P-256) ≈ **14×**. Đây là **anchor thật** cho **RQ3 / E-N1 / invariant I7**.

---

## 7) Tổng hợp dataset + sinh số MÔ HÌNH

```bash
python3 scripts/build_dataset.py
```

| Đầu ra | Loại | Mô tả |
|---|---|---|
| [`data/micro/summary_x86.csv`](../data/micro/summary_x86.csv) | **MEASURED** | median/mean/std/p95/CI95 mỗi scheme×mức×op |
| [`data/micro/arm/{mlkem,mldsa,aes}_arm.csv`](../data/micro/arm/) | **MODELED** | x86_ref × hệ số nền tảng (~4.6×; AES 22×) |
| [`data/micro/neon_vs_ref.csv`](../data/micro/neon_vs_ref.csv) | **MODELED** | reference-C vs NEON trên Cortex-A72 (1.7–2.3×) |
| [`data/resource/energy_estimate.csv`](../data/resource/energy_estimate.csv) | **MODELED** | E = P·t (P_x86=15 W, P_arm=4 W) |
| [`data/tls/netem_matrix.csv`](../data/tls/netem_matrix.csv) | **MODELED** | handshake vs delay×loss × cấu hình |

> **Liêm chính:** mọi file MÔ HÌNH có dòng đầu `# MODELED …` nêu công thức + nguồn hệ số (không bịa số; mỗi số dẫn xuất từ số đo thật x86 × hệ số có trích dẫn). Phân định đầy đủ: `docs/DATA_PROVENANCE.md`.

---

## 8) Biểu đồ (figures)

```bash
python3 tools/plot.py           # -> docs/report/figures/*.svg  (SVG thuần, không cần matplotlib)
```

Sinh các SVG: latency theo mức (ML-KEM/ML-DSA), PQC vs cổ điển (log-scale, Cat5), reference vs AVX2, sizes, phân phối ML-DSA sign (lệch phải), x86 vs ARM, netem. Mỗi figure ghi rõ **"đo thật"** hoặc **"(MÔ HÌNH)"**.

---

## 9) Đường ARM / Raspberry Pi 4 (Cortex-A72)

> **Mục tiêu:** thay số ARM **MÔ HÌNH** bằng **số ĐO THẬT** trên Pi. Cùng runner, tự nhận `aarch64`→`arm`, ghi vào `data/micro/arm/`.

```bash
# Trên Pi (SSH), trong $REPO:
sudo apt-get install -y build-essential g++ gcc python3 libssl-dev
# (khuyến nghị) khóa governor & kiểm tra không bị bóp xung:
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
vcgencmd get_throttled            # kỳ vọng 0x0

# build lib + chạy 3 microbench (giống x86, runner tự nhận arch):
bash scripts/build_pqc_lib.sh
cd "$REPO/benchmark_mlkem" && ./build_run.sh
cd "$REPO/benchmark_mldsa" && ./build_run.sh
cd "$REPO/benchmark_aes"   && ./build_run.sh    # -> data/micro/arm/*_arm.csv (ĐO THẬT)
bash scripts/run_kat.sh                          # KAT trên ARM
bash scripts/capture_env.sh data/env             # -> data/env/env_linux-arm.txt
```

- **Khi đo thật trên Pi:** `data/micro/arm/{mlkem,mldsa,aes}_arm.csv` trở thành **MEASURED** (ghi đè bản MÔ HÌNH); `capture_env.sh` ghi đè `env_linux-arm.txt` bằng cấu hình Pi thật (`uname -m = aarch64`).
- **TLS PQC trên Pi (tùy):** dùng container `pqc:wp1-arm64` (OpenSSL 3.6 native) + `scripts/run_tls_handshake.sh` → `data/tls/arm/` (đo thật ARM, đã chứng minh `X25519MLKEM768` + `mldsa65` chạy trên phần cứng ARM).
- **Còn lại MÔ HÌNH cho tới khi đo:** NEON speedup (cần build liboqs NEON + PMU), năng lượng (cần đo công suất vật lý), netem (cần `tc netem` thật giữa Pi↔PC). Ghi rõ ở Limitations / `docs/DATA_PROVENANCE.md`.

---

## 10) Bảng measured vs modeled (tóm tắt nhanh)

| Hạng mục | x86 | ARM |
|---|---|---|
| Microbench reference-C (3 mức) | **MEASURED** `data/micro/x86/*` | **MODELED** `data/micro/arm/*` → MEASURED khi chạy trên Pi |
| AVX2 optimized | **MEASURED** `micro_evp_x86.csv` | — (đối chứng NEON là MODELED) |
| KAT 9/9 | **MEASURED** | MEASURED khi chạy trên Pi |
| TLS handshake | **MEASURED** `handshake_x86.txt` | MEASURED khi chạy `pqc:wp1-arm64` |
| NEON speedup | — | **MODELED** `neon_vs_ref.csv` |
| Năng lượng | **MODELED** (P·t, t_x86 thật) | **MODELED** |
| netem (delay×loss) | **MODELED** `netem_matrix.csv` | **MODELED** |
| Code size / Peak RSS | **MEASURED** `data/resource/*` | MEASURED khi chạy trên Pi |

---

## 11) Lỗi dự kiến & xử lý

| Triệu chứng | Nguyên nhân | Xử lý |
|---|---|---|
| `❌ Thiếu vendor/pqclean/...` | chưa prefetch nguồn | `bash scripts/fetch_sources.sh` (host có mạng) |
| Bước 4–5 bị bỏ qua | không phải x86 hoặc thiếu podman | cài `podman`/`docker` + image `pqc:wp1-amd64`; hoặc chạy trên x86 |
| `openssl: unknown option ml-dsa-65` | OpenSSL hệ thống < 3.5 | dùng container OpenSSL 3.6 (handshake/EVP đã đóng gói) |
| Số ML-DSA sign dao động mạnh | rejection sampling (đúng bản chất) | báo cáo **median + p95**, không dùng mean |
| Số ARM = số x86 | đang đọc bản MÔ HÌNH, chưa chạy Pi | chạy mục §9 trên Pi để có MEASURED |
| Throttling trên Pi | `vcgencmd get_throttled` ≠ 0x0 | khóa governor `performance`, làm mát, loại lần bị bóp |
