# NT219.Q21.ANTT — Cryptanalysis on Lattice-based Algorithms

**Implement & Benchmark Lattice-based Schemes (ML-KEM / ML-DSA): so sánh hiệu năng với RSA/ECC trên Linux x86_64 + Raspberry Pi 4**

> Đồ án môn NT219 (Cryptography). Triển khai, tối ưu và đo hiệu năng các scheme hậu lượng tử dựa trên lattice — **ML-KEM** (FIPS 203, tên cũ Kyber) và **ML-DSA** (FIPS 204, tên cũ Dilithium) — rồi so sánh chi tiết với **RSA / ECDSA / ECDH / Ed25519** trên hai nền tảng: **x86_64 Linux** (server) và **ARM Raspberry Pi 4** (Cortex-A72, arm64, SBC). Đo qua **mạng thật** (Pi = server ↔ PC = client) trong kiến trúc **onion 3 lớp** (Authentication / Authorization / Cryptography).

---

## 1. Mục tiêu & câu hỏi nghiên cứu

**Mục tiêu:** đo 3 tầng (primitive · TLS · tài nguyên), so PQC vs cổ điển ở cùng mức bảo mật, đánh giá tác động tối ưu NEON trên ARM, và rút ra khuyến nghị triển khai.

| RQ | Nội dung | Trả lời bằng |
|----|----------|--------------|
| **RQ1** | Overhead tính toán & băng thông của ML-KEM/ML-DSA vs RSA/ECDSA ở mức bảo mật tương đương; yếu tố nào ảnh hưởng nhất (param length / assembly / NEON)? | primitive cycles·ns + kích thước key/sig/ct + ablation NEON |
| **RQ2** | Tối ưu NEON + compiler flags có làm PQC khả thi trên Pi 4 cho TLS không? | reference-C vs NEON trên Cortex-A72 + TLS handshake trên Pi |
| **RQ3** | Hybrid handshake (X25519MLKEM768) có overhead chấp nhận được cho web server/client? | TLS handshake hybrid vs classical + bytes-on-wire + netem |

> **Thuật ngữ (đã chuẩn hóa):** dùng tên chuẩn **ML-KEM / ML-DSA**, không dùng "Kyber/Dilithium" (trừ khi nhắc paper gốc). Nhóm hybrid TLS là **X25519MLKEM768** (codepoint `0x11EC`), KHÔNG gọi "ECDHE+Kyber".

---

## 2. Phiên bản đã ghim (pinned versions)

Stack ràng buộc version chặt — nguồn duy nhất ở [`scripts/versions.env`](scripts/versions.env):

| Thành phần | Version | Ghi chú |
|-----------|---------|---------|
| OpenSSL | **3.6.2** | build từ source vào prefix riêng; PQC native (ML-KEM/ML-DSA/SLH-DSA) từ 3.5 |
| liboqs | **0.14.0** (hoặc 0.15.0) | microbenchmark `speed_kem`/`speed_sig`; build 2 cây (ref vs NEON) |
| oqs-provider | **0.10.0** | cặp tương thích với liboqs 0.14.0 |

> ⚠️ **Lưu ý tương thích:** OpenSSL ≥ 3.5 đã có PQC native ở **default provider**, nên oqs-provider ≥ 0.9.0 **tự tắt** ML-KEM/ML-DSA khi chạy cùng OpenSSL ≥ 3.5. Khi đo phải ghi rõ nguồn `@ default` (native) hay `@ oqsprovider`. PQC ở đây nằm trong **default provider, KHÔNG phải FIPS provider** — không khẳng định FIPS-compliance.

---

## 3. Cấu trúc repo

```
NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms/
├── README.md                     # file này
├── Makefile                      # build harness bench (kiểu rsabench: OSSLROOT, build/run/clean)
├── scripts/
│   ├── versions.env              # GHIM version + commit hash (single source of truth)
│   ├── build_openssl.sh          # build OpenSSL 3.6.2 vào prefix riêng
│   ├── build_liboqs.sh           # build 2 cây: reference C vs NEON-optimized (WP3)
│   ├── build_provider.sh         # build + install oqs-provider, vá openssl.cnf
│   ├── build_all.sh              # gọi tuần tự 3 script trên (1 lệnh tái lập)
│   ├── crosscompile_liboqs.sh    # cross-compile arm64 (CHỈ artifact — không đo)
│   ├── collect_env.sh            # ghi toolchain → docs/env-<host>.txt
│   └── setup_governor.sh         # governor=performance + tắt turbo + check throttle
├── cmake/
│   └── toolchain-aarch64.cmake   # toolchain cross-compile aarch64 (chỉ artifact)
├── apps/
│   └── bench.cpp                 # harness C++ đo primitive qua OpenSSL EVP
├── benchmarks/
│   ├── micro/                    # microbenchmark harness & raw CSV
│   ├── tls/                      # TLS integration scripts & results (mạng thật + netem)
│   └── energy/                   # power measurement scripts & logs
├── docker/
│   ├── Dockerfile.x86_64         # multi-stage, pin digest + ARG version
│   └── buildx-multiarch.sh       # docker buildx amd64+arm64 (artifact, KHÔNG benchmark)
├── data/
│   ├── raw/{x86_64,arm64}/       # CSV thô theo nền tảng
│   └── processed/                # CSV đã tổng hợp (median-of-medians, CI bootstrap)
├── analysis/                     # pandas/bootstrap → figures
├── tools/                        # helper (timers, parsers)
├── docs/                         # report, slides, env dumps, reproducibility checklist
└── .gitignore                    # bỏ build/, .build/, *.key, *.pem, .claude/, memory/
```

> `build/`, `.build/`, file khóa `*.key`/`*.pem`, và thư mục công cụ (`.claude/`, `memory/`) **không commit** — đưa vào `.gitignore`. liboqs/OpenSSL là source bên thứ ba, chỉ commit **script biết cách lấy về**, không commit source của chúng.

---

## 4. Build (tái lập từ đầu)

### 4.1 Yêu cầu hệ thống

```bash
# Ubuntu / Debian / Raspberry Pi OS Bookworm 64-bit
sudo apt update
sudo apt install -y build-essential cmake ninja-build git wget ca-certificates perl libssl-dev
# (Tùy chọn) cross-compile arm64 trên PC x86:
sudo apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

### 4.2 Build toàn bộ stack bằng MỘT lệnh

```bash
git clone https://github.com/maccriagor/NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms.git && cd NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms
./scripts/build_all.sh          # OpenSSL 3.6.2 → liboqs (2 cây) → oqs-provider
```

`build_all.sh` gọi tuần tự: `build_openssl.sh` → `build_liboqs.sh` → `build_provider.sh`. Mỗi script `set -euo pipefail` (fail-fast), cài vào **prefix riêng** (`/opt/...`, KHÔNG đụng `/usr`), và chạy `make test`/`ctest` làm bằng chứng build đúng.

### 4.3 Build harness benchmark

```bash
make OSSLROOT=/opt/openssl-3.6.2          # build apps/bench.cpp → build/bench
./build/bench > data/raw/$(uname -m).csv  # đổ kết quả ra CSV
```

### 4.4 Kiểm chứng PQC sẵn sàng

```bash
/opt/openssl-3.6.2/bin/openssl list -kem-algorithms | grep -i mlkem
/opt/openssl-3.6.2/bin/openssl list -signature-algorithms | grep -i mldsa
# kỳ vọng thấy X25519MLKEM768, ML-KEM-768, ML-DSA-65 @ default
```

> **OpenSSL build vào prefix riêng, KHÔNG ghi đè 3.0.11 hệ thống.** Kiểm chứng tách biệt: `ldd /opt/openssl-3.6.2/bin/openssl` phải trỏ tới `/opt/openssl-3.6.2/lib/libcrypto.so.3`. Cờ `-Wl,-rpath,... --enable-new-dtags` giúp binary tự tìm thư viện, không cần `LD_LIBRARY_PATH`.

---

## 5. Cross-compile arm64 (tùy chọn — chỉ artifact)

Trên PC Ubuntu, dùng toolchain `aarch64-linux-gnu` để tạo binary arm64:

```bash
./scripts/crosscompile_liboqs.sh        # output: .build/liboqs/build-cross (ELF aarch64)
```

> ⚠️ **Binary cross-compile CHỈ là artifact — KHÔNG dùng để đo.** Nó không chạy được trên PC (ARM ≠ x86) và có thể không bật hết cờ tối ưu CPU. **Số NEON/cycle thật phải build NATIVE trên chính Pi 4** (`build_liboqs.sh` với `OQS_DIST_BUILD=OFF -DOQS_OPT_TARGET=cortex-a72`). Tương tự, **không lấy số đo từ Docker/QEMU emulation**.

---

## 6. Benchmark — đo gì

### 6.1 Primitive (qua liboqs `speed_kem`/`speed_sig` + harness EVP)

| Thuật toán | Thao tác | Đơn vị |
|-----------|----------|--------|
| RSA-2048/3072/15360 | keygen · sign · verify (PSS) · encrypt/decrypt (OAEP) | ns, cycles, ops/s, bytes |
| ECDH P-256/384/521 | keygen · derive | ns, cycles, ops/s, bytes |
| ECDSA P-256/384/521 | keygen · sign · verify | ns, cycles, ops/s, bytes |
| Ed25519 | keygen · sign · verify | ns, cycles, ops/s, bytes |
| ML-KEM-512/768/1024 | keygen · encaps · decaps | ns, cycles, ops/s, bytes |
| ML-DSA-44/65/87 | keygen · sign · verify | ns, cycles, ops/s, bytes |

> ⚠️ `openssl speed` đo được **ML-KEM** nhưng **KHÔNG đo được ML-DSA** trong 3.5/3.6 → ML-DSA lấy từ **liboqs `speed_sig`** hoặc **harness EVP custom** (`apps/bench.cpp`). ML-DSA sign dùng rejection sampling → phân phối lệch phải → báo **median + p95 + p99**, không chỉ mean.

### 6.2 TLS macro (mạng thật: Pi=server ↔ PC=client, KHÔNG localhost)

Handshake latency (median/p95/p99) · throughput (concurrency 1–16) · bytes-on-wire (tách KEM vs cert/sig) · packet/RTT/fragmentation · hành vi dưới **netem** (latency {0,20,60,100ms} × loss {0,1,3,5,10%}). Ba cấu hình: classical · PQC thuần · hybrid X25519MLKEM768.

### 6.3 Tài nguyên

peak RSS (`/usr/bin/time -v`, liboqs `test_*_mem`) · code size (`size`, `readelf -S`) · stack · energy (INA219/INA226 hoặc CPU power model).

### 6.4 Ma trận iso-security (NIST SP 800-57)

| Level | ML-KEM | ML-DSA | RSA | ECDSA/ECDH |
|-------|--------|--------|-----|-----------|
| L1 (~128-bit) | 512 | 44 (Cat 2) | 3072 | P-256 |
| L3 (~192-bit) | 768 | 65 | 7680 | P-384 |
| L5 (~256-bit) | 1024 | 87 | 15360 | P-521 |

---

## 7. Chuẩn hóa môi trường đo (BẮT BUỘC để số đo lặp lại)

```bash
sudo ./scripts/setup_governor.sh    # governor=performance, tắt turbo, check throttle
./scripts/collect_env.sh            # ghi toolchain → docs/env-<host>.txt (chạy trên CẢ PC và Pi)
```

**Raspberry Pi 4** — ghim tần số trong `/boot/firmware/config.txt`:
```ini
arm_freq=1500       # tần số chuẩn (1.5GHz) — mục tiêu là ổn định, không phải nhanh nhất
force_turbo=1       # giữ tần số cố định, bỏ qua governor động
```
Cần **tản nhiệt** (heatsink + fan; Pi 4 throttle ở 80–85°C). Trước mỗi lần đo: `vcgencmd get_throttled` phải `= 0x0`.

**Thống kê:** mỗi op N=1000–10000, K=5–10 batch, `clock_gettime(CLOCK_MONOTONIC_RAW)`, loại warm-up, **median-of-medians + 95% CI bootstrap**; thêm **p95/p99** cho ML-DSA sign và mọi handshake. Pin core: `taskset -c <core> chrt -f 80 ./build/bench`.

---

## 8. Bốn cảnh báo quyết định tính đúng của số đo

1. **Cross-compile & QEMU emulation TUYỆT ĐỐI không dùng để đo** — chỉ tạo artifact. (Emulation chậm 5–12×.)
2. **`-march=native` / `-mcpu=native` không portable** (gây `Illegal instruction` trên CPU khác) — chỉ dùng khi đo trên đúng máy đó, không cho artifact phân phối.
3. **Đo NEON/cycle chính xác phải BUILD NATIVE trên Pi** (`OQS_DIST_BUILD=OFF`) — CPU feature detection không chạy lúc cross-compile.
4. **Governor=performance + tắt turbo + chống thermal throttle** là điều kiện cần để giảm variance.

---

## 9. Checklist reproducibility (mục tiêu: người chấm `git clone` → 1 lệnh → tái lập)

**Bắt buộc (đóng góp trực tiếp 25% rubric):**
- [ ] `scripts/versions.env` ghim **tag + commit hash** OpenSSL/liboqs/oqs-provider (không `main`/`latest`).
- [ ] Build script `set -euo pipefail`, cài vào prefix riêng, chạy `make test`/`ctest`.
- [ ] `build_all.sh` dựng toàn bộ stack bằng **1 lệnh**.
- [ ] `docker/Dockerfile.x86_64` multi-stage, pin base image theo digest, `ARG ..._TAG`.
- [ ] `collect_env.sh` đã chạy trên **cả PC và Pi**, commit vào `docs/`.
- [ ] Báo cáo ghi rõ **flags** mỗi cây build (`-O2`/`-O3`, `-mcpu=cortex-a72`, `OQS_DIST_BUILD`, `OQS_OPT_TARGET`).
- [ ] `setup_governor.sh` chạy; Pi có `arm_freq` cố định + `vcgencmd get_throttled == 0x0` trước khi đo.

**Tùy chọn (điểm cộng):**
- [ ] `cmake/toolchain-aarch64.cmake` + cross-compile script (kèm cảnh báo "artifact only").
- [ ] `docker/buildx-multiarch.sh` tạo manifest list amd64+arm64.
- [ ] Build 2 cây liboqs (reference C vs NEON) đối chứng WP3.
- [ ] `OQS_SPEED_USE_ARM_PMU=ON` + kernel module `mupq/pqax` trên Pi để có cycle-count chính xác.

---

## 10. Lệnh tái lập đầy đủ (tóm tắt)

```bash
git clone https://github.com/maccriagor/NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms.git && cd NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms
./scripts/build_all.sh                      # build OpenSSL + liboqs + oqs-provider
sudo ./scripts/setup_governor.sh            # chuẩn hóa CPU
./scripts/collect_env.sh                    # ghi môi trường
make OSSLROOT=/opt/openssl-3.6.2            # build harness
./build/bench > data/raw/$(uname -m).csv    # đo primitive
# TLS macro: chạy benchmarks/tls/ với Pi=server ↔ PC=client (mạng thật)
```

---

## 11. Tài liệu

Báo cáo, slide, env dumps và checklist nằm trong [`docs/`](docs/). Các file kế hoạch đồ án: `05_*` (đề bài), `08_*` (work plan), `09_*` (architecture nodes), `14_*` (enhanced security architecture).

> **Đo được vs lập luận:** mọi metric hiệu năng (time, cycles, ops/s, bytes, RSS, handshake latency, throughput) là **đo được** → báo bằng số + CI. Mức bảo mật (L1/L3/L5, IND-CCA2, EUF-CMA, kháng lượng tử) **không benchmark được** → chỉ lập luận qua spec (FIPS 203/204). Không trộn hai loại.

---

## 12. Nguồn chính

NIST FIPS 203 (ML-KEM) · FIPS 204 (ML-DSA) · RFC 8446 (TLS 1.3) · draft-ietf-tls-ecdhe-mlkem (X25519MLKEM768) · RFC 9794 (PQ/T terminology) · RFC 9881 (ML-DSA in X.509) · OWASP Top 10:2025 A04 · liboqs / oqs-provider / oqs-demos (github.com/open-quantum-safe) · OpenSSL INSTALL.md · reproducible-builds.org · lelegard/pqcbench (Makefile pattern).