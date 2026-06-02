# ROADMAP HOÀN THIỆN ĐỒ ÁN — NT219

**Đề tài:** Implement & Benchmark Lattice-based Schemes (Kyber/Dilithium) — so sánh hiệu năng với RSA/ECC trên môi trường thực (Linux x86 + ARM).

**Mục tiêu roadmap:** tối đa điểm (làm cả phần khó).
**Phạm vi đã chốt (gồm yêu cầu GV thêm):** đo **qua mạng thật** (client ↔ server) + **kiến trúc onion 3 lớp trong** (Authentication / Authorization / Cryptography).

> Cách dùng: tick `- [x]` khi xong. Việc gắn ⭐ = phần "kéo điểm" (làm để đạt mức tối đa). Việc gắn 🔴 = nằm trên **đường găng** (critical path) — chậm cái này là chậm cả đồ án.

---

## 0. TRẠNG THÁI HIỆN TẠI (mốc xuất phát)

- [x] Thuê Raspberry Pi 4 (Mythic Beasts): Cortex-A72, 4GB, arm64
- [x] SSH vào Pi thành công, OS = Raspberry Pi OS Bookworm **64-bit (arm64)**
- [x] Xác nhận `dpkg --print-architecture` → `arm64`, `uname -m` → `aarch64`
- [x] Xác nhận OpenSSL **3.0.11** có sẵn (oqs-provider sẽ cắm được)
- [x] Ghi `lscpu` (Cortex-A72, L2 1MiB, flag `asimd`=NEON) cho phần tái lập
- [x] Chốt phạm vi với GV: network + onion 3 lớp = bắt buộc

**➡ Bạn đang đứng ở đầu WP1 (build liboqs + OpenSSL/oqs-provider).** Việc kế tiếp ngay: build liboqs trên Pi.

---

## 1. BẢN ĐỒ TỔNG QUAN: TUẦN ↔ WP ↔ TRỌNG SỐ ĐIỂM

| Tuần | Gói việc (WP) | Nội dung chính | Trọng số điểm liên quan |
|---|---|---|---|
| 1–2 | WP0 | Lý thuyết, chọn tham số, map bảo mật, lit review, **threat model** | Literature 20% |
| 3–4 | WP1 | Build env x86 + ARM, liboqs + OpenSSL/oqs-provider, **Docker** | Tái lập 25% |
| 5–6 | WP2 | Harness microbench (sig + KEM), x86 + ARM, **KAT đúng đắn** | Thí nghiệm 30% + Tái lập 25% |
| 7–8 | WP3 | Tối ưu **NEON** vs reference, cờ build, đo cycle (PMU) | Thí nghiệm 30% (RQ1/RQ2) |
| 9 | WP4 + WP5 | **TLS qua mạng** (3 cấu hình + netem) + **kiến trúc onion 3 lớp** | Thí nghiệm 30% + Thiết kế |
| 10 | WP6 | Bộ nhớ/code size/kích thước byte, năng lượng (ước lượng), **validation** | Thí nghiệm 30% + Phân tích 15% |
| 11 | WP7a | Thống kê (bootstrap), biểu đồ, **trả lời RQ1/RQ2/RQ3** | Phân tích 15% |
| 12 | WP7b | Báo cáo, dọn repo, Docker, demo | Trình bày 10% + Tái lập 25% |

**Đường găng (critical path):** WP1 → WP2 → WP3 → WP4 → WP7. (WP5/onion phụ thuộc oqs-provider của WP1 + handshake của WP4.)
**Mốc nộp:** 🔴 **Giữa kỳ** = cuối Tuần 6 (sau WP2: design + microbench ban đầu) · **Cuối kỳ** = Tuần 12. *(§11 file 05)*

**5 MỤC TIÊU HỌC THUẬT (§2 file 05) ↔ WP đảm nhận — kiểm tra không sót:**

| Mục tiêu §2 | WP đảm nhận | Trạng thái |
|---|---|---|
| 1. Hiểu toán lattice + vai trò Kyber/Dilithium trong PQC | WP0 | ✅ |
| 2. Build + tích hợp thư viện PQC (liboqs/PQClean) vào mô hình thử nghiệm | WP1, WP2, WP4–WP5 | ✅ |
| 3. Đo chính xác: latency, throughput, memory, code size, energy | WP2, WP6, WP7a | ✅ (energy = ước lượng → Limitations) |
| 4. Tối ưu build (flags, assembly, NEON) + đánh giá tác động | WP3 | ✅ |
| 5. So sánh an toàn/chi phí/hiệu năng PQC vs RSA/ECC + kết luận + khuyến nghị | WP0, WP6, WP7 | ✅ |

---

## 2. LỘ TRÌNH CHI TIẾT THEO TUẦN

### 🗓 TUẦN 1–2 — WP0: Nền tảng & thiết kế (chuẩn bị "giấy tờ")

**Việc:**
- [ ] Viết phần lý thuyết: Module-LWE (Kyber), Module-LWE + Module-SIS + Fiat–Shamir-with-aborts (Dilithium)
- [ ] 🔴 **Bảng map mức bảo mật** (chống lỗi so sánh không công bằng):
  - L1 ↔ RSA-3072 ↔ P-256 ; L3 ↔ RSA-7680 ↔ P-384 ; L5 ↔ RSA-15360 ↔ P-521
  - [ ] Quyết định: so "iso-security" hay "thực tiễn triển khai (RSA tối đa 3072)" — **ghi rõ giả định**
- [ ] Chọn tham số: ML-KEM 512/768/1024, ML-DSA 44/65/87, RSA 2048/3072, ECDSA/ECDH P-256/P-384, Ed25519
- [ ] 🔴 **Phát biểu 3 RQ + GIẢ THUYẾT** ra giấy (vd: *"PQC có overhead băng thông/độ trễ cao hơn RSA/ECC, nhưng NEON + tối ưu phần mềm kéo về mức khả thi cho server & Pi"*) → sẽ kiểm chứng bằng số liệu ở WP7 *(§4 — file 05 BẮT BUỘC có giả thuyết)*
- [ ] Literature review ≥6 nguồn:
  - [ ] FIPS 203 (ML-KEM) + FIPS 204 (ML-DSA)
  - [ ] Bos et al. (CRYSTALS-Kyber, IEEE EuroS&P 2018)
  - [ ] Ducas et al. (CRYSTALS-Dilithium, TCHES 2018)
  - [ ] Paquin/Stebila/Tamvada (Benchmarking PQC in TLS, PQCrypto 2020)
  - [ ] Sikeridis et al. (Evaluating PQC TLS, IEEE ICC 2021)
  - [ ] Tasopoulos et al. (PQC TLS embedded, ISPEC 2022 + năng lượng, Computing Frontiers 2023)
  - [ ] pqm4 (Kannwischer et al.)
- [ ] ⭐ **Threat model / Security Risks**: kẻ tấn công, "harvest-now-decrypt-later", tài sản (Asset), mỗi lớp onion chống đe dọa gì
- [x] Chốt phạm vi với GV (network + onion)

**Đầu ra:** chương Background + Literature + Threat Model + bảng tham số/bảo mật.
**Checkpoint (DoD):** giải thích được *tại sao* RSA-3072 chỉ tương đương L1, và *tại sao* cần PQC ngay bây giờ.

---

### 🗓 TUẦN 3–4 — WP1: Dựng môi trường build (x86 + ARM) 🔴

**Trên Pi (ARM) — làm ngay:**
- [ ] Cài công cụ: `build-essential cmake ninja-build git python3 python3-pip pkg-config libssl-dev astyle`
- [ ] Khóa governor `performance`; kiểm tra `vcgencmd get_throttled` = `0x0`
- [ ] 🔴 Build **liboqs**: `cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DOQS_DIST_BUILD=ON ..` → `ninja`
  - *Ghi chú §7.2:* liboqs đã đóng gói **reference C (gốc từ PQClean) + bản tối ưu** → thỏa yêu cầu "PQClean + liboqs" của file 05; nhánh **reference** chính là bản đối chứng cho WP3
- [ ] Chạy thử: `./tests/speed_kem ML-KEM-768` và `./tests/speed_sig ML-DSA-65` ra số ✔ (**mốc lớn đầu tiên**)
- [ ] 🔴 Build **OpenSSL 3 + oqs-provider** (cho TLS); xác minh `openssl list -providers` thấy `oqsprovider`

**Trên x86 (PC Linux hoặc WSL Ubuntu):**
- [ ] Lặp lại đúng các bước trên (bỏ `vcgencmd`)
- [ ] Ghi `lscpu`, `openssl version`, gcc version cho x86

**Tái lập:**
- [ ] ⭐ Viết **Dockerfile** đóng băng build (liboqs + OpenSSL + oqs-provider)
- [ ] ⭐ `docker buildx` đa kiến trúc (x86_64 + arm64) HOẶC script cross-compile riêng
- [ ] Cập nhật tooling trong báo cáo: **oqs-provider** (bỏ "OpenSSL-OQS fork"), **ML-KEM/ML-DSA** (FIPS 203/204), bỏ chữ "RFC" → "FIPS + IETF draft"

**Đầu ra:** môi trường build chạy được trên cả 2 nền tảng + Dockerfile.
**Checkpoint (DoD):** chạy được 1 phép Kyber + 1 phép RSA trên *cả* x86 và ARM, từ *cùng* một build OpenSSL+oqs.

---

### 🗓 TUẦN 5–6 — WP2: Harness microbenchmark

**Việc:**
- [x] Harness chữ ký `sig_bench.cpp` (RSA/ECDSA/Ed25519/ML-DSA) — đã có
- [ ] 🔴 Viết **harness KEM** (ECDH + ML-KEM): `EVP_PKEY_encapsulate/decapsulate` + `EVP_PKEY_derive`
- [ ] Chạy ≥**1000 vòng**/phép (tới 10000), có **warm-up** (loại khỏi số đo), thu **median + mean + std + CI 95%** ra **CSV** *(§7.4)*
- [ ] 🔴 **K = 5–10 batch × M iteration**, báo cáo **median-of-medians**; timer **`clock_gettime(CLOCK_MONOTONIC_RAW)`** *(§7.5, §8.3 — đừng bỏ)*
- [ ] ⭐ **Paired comparison**: so từng cặp thuật toán trên **cùng phần cứng / cùng batch** để giảm phương sai *(§7.5)*
- [ ] 🔴 Quét **đủ 3 mức** mỗi họ (ML-KEM 512/768/1024 và ML-DSA 44/65/87) — không chỉ mức giữa *(§7.1)*
- [ ] Chạy trên **cả x86 và ARM**, cùng cấu hình
- [ ] Pin core (`taskset`), lặp nhiều batch
- [ ] Dùng `openssl speed` + `speed_kem`/`speed_sig` làm **đối chứng** (không phải nguồn chính)
- [ ] ⭐ Đo đúng trục: KEM (Kyber ↔ ECDH), Chữ ký (Dilithium ↔ RSA/ECDSA/Ed25519); (tùy) báo cáo **ops/sec** cấp primitive *(§9)*

**Kiểm thử tính đúng đắn (MỚI — đừng bỏ):**
- [ ] ⭐ Chạy **KAT / test vector** xác minh implementation đúng (encaps/decaps khớp, sign/verify pass)
- [ ] Ghi lại "correctness OK" trước khi tin số đo

**Đầu ra:** CSV microbenchmark x86 + ARM cho mọi thuật toán.
**🔴 MỐC GIỮA KỲ (cuối Tuần 6 — §11 bắt buộc):** nộp **Mid-term report/presentation** = thiết kế + bộ test + **microbenchmark ban đầu**.
**Checkpoint (DoD):** số harness khớp thứ tự độ lớn với `openssl speed`; KAT pass.

---

### 🗓 TUẦN 7–8 — WP3: Tối ưu NEON & so sánh (lõi RQ1/RQ2)

**Việc:**
- [ ] ⭐ Build **bản tối ưu NEON** (asimd) trong liboqs; so **reference (C) vs optimized**
- [ ] Thử cờ `-O2`, `-O3`; ghi gcc version + cấu hình CPU
- [ ] 🔴 Đo **cycle** bằng PMU: bật performance counter qua kernel module (ARM **không có** `rdtsc`)
- [ ] Lập bảng "% tăng tốc nhờ NEON" trên ARM
- [ ] ⭐ So x86 (AVX2) vs ARM (NEON) để thấy khác biệt nền tảng

**Đầu ra:** bảng reference-vs-optimized, dữ liệu cho RQ1 (yếu tố ảnh hưởng) + RQ2 (NEON có khả thi hóa PQC trên Pi).
**Checkpoint (DoD):** chỉ ra được NEON giúp Kyber/Dilithium nhanh hơn bao nhiêu lần trên Pi.

---

### 🗓 TUẦN 9 — WP4 (TLS qua mạng) + WP5 (kiến trúc onion) 🔴⭐

**Bố trí: Pi = server ↔ PC = client, QUA MẠNG THẬT (không localhost).**

**WP4 — Đo handshake (3 cấu hình: cổ điển / PQC / hybrid):**
- [ ] 🔴 Tạo chứng chỉ: 1 bản ECDSA, 1 bản **Dilithium (ML-DSA)**
- [ ] Đo **5 metric** mỗi cấu hình:
  - [ ] (1) **Thời gian handshake** — median + mean + std + **p95/p99** (không chỉ mean)
  - [ ] (2) ⭐ **Số byte trên dây** — tách KEM (pubkey+ciphertext) và chứng chỉ/chữ ký (chỗ PQC phình to)
  - [ ] (3) Số gói + RTT + **phân mảnh** (vượt MTU 1500? vượt initcwnd?)
  - [ ] (4) **Throughput** handshake/giây dưới tải (concurrency 1–16), **test cả server single-threaded VÀ multi-threaded** *(§7.4)*
  - [ ] (5) 🔴⭐ Dưới **netem**: độ trễ {0,20,60,100ms} × mất gói {0,1,3,5,10%}
- [ ] Hybrid: `X25519MLKEM768` → trả lời **RQ3**
- [ ] Công cụ: `s_time`, `s_server`/`s_client`, `tcpdump`/Wireshark, `tc netem`, `wrk`

**WP5 — Kiến trúc giải pháp + onion 3 lớp:**
- [ ] 🔴 **Vẽ sơ đồ kiến trúc**: client → mạng → [Firewall → IDS/IPS] → **Auth → Authz → Crypto** → Asset
- [ ] Ánh xạ PQC vào 3 lớp trong:
  - [ ] **Cryptography** → Kyber/ML-KEM (+ hybrid) mã hóa kênh
  - [ ] **Authentication** → chứng chỉ Dilithium (mTLS)
  - [ ] **Authorization** → token/quyền **ký bằng Dilithium** (RBAC) — *xác nhận mức độ hiện thực với GV*
- [ ] ⭐ Dựng HTTPS server thật (nginx PQC từ oqs-demos), đo end-to-end
- [ ] ⭐ Phân tích lớp bằng **Wireshark có bản vá bóc tách PQC**

**Đầu ra:** CSV handshake (5 metric × 3 cấu hình × điều kiện mạng) + sơ đồ kiến trúc onion + demo nginx PQC.
**Checkpoint (DoD):** bắt được gói handshake PQC qua mạng, thấy rõ byte PQC > cổ điển; sơ đồ onion gắn đúng Kyber/Dilithium vào 3 lớp.

---

### 🗓 TUẦN 10 — WP6: Tài nguyên, năng lượng & đối chiếu

**Việc:**
- [ ] Đo kích thước **khóa / ciphertext / chữ ký** (byte) — bảng "bytes vs algorithm"
- [ ] Đo **peak RSS** (`/usr/bin/time -v`) + **code size** (`size`, `readelf -S`)
- [ ] **Năng lượng**: Pi thuê từ xa → **không tiếp cận vật lý**:
  - [ ] ⭐ Phương án tối đa điểm: ước lượng qua **CPU power model**, HOẶC đo trên 1 Pi local nếu mượn được
  - [ ] Nếu không: ghi rõ vào **Limitations** (đã lường trước)
- [ ] ⭐ **Validation**: đối chiếu số của mình với **NIST/literature** (vd MDPI, bài Pi) để xác nhận hợp lý

**Đầu ra:** bảng kích thước/bộ nhớ + ghi chú validation.
**Checkpoint (DoD):** số liệu nằm trong khoảng hợp lý so với các bài đã công bố.

---

### 🗓 TUẦN 11 — WP7a: Thống kê & phân tích

**Việc:**
- [ ] Python/pandas đọc toàn bộ CSV
- [ ] 🔴 Tính CI **bootstrap** (chuẩn hơn xấp xỉ chuẩn khi phân phối lệch)
- [ ] ⭐ Phân tích **phân phối Dilithium** (rejection sampling → lệch phải) bằng median + percentile; **phân biệt** timing-variability (do thuật toán) vs timing-leak (rò rỉ khóa)
- [ ] Vẽ biểu đồ: latency vs tham số, bytes vs thuật toán, throughput vs concurrency, latency vs % mất gói, **energy vs thuật toán** (nếu có ước lượng) *(§9)*
- [ ] 🔴 **Trả lời rõ RQ1 / RQ2 / RQ3 bằng số liệu của mình** + khuyến nghị thực tiễn
- [ ] Tách bạch: **an toàn = lập luận** (không đo được) ; **hiệu năng = đo**

**Đầu ra:** figures + tables + phần trả lời RQ.
**Checkpoint (DoD):** mỗi RQ có câu trả lời dựa trên số liệu, kèm CI.

---

### 🗓 TUẦN 12 — WP7b: Báo cáo, repo, demo (đóng gói điểm)

**Việc:**
- [ ] Viết **báo cáo đầy đủ** theo bố cục: Tóm tắt → Background → Threat Model/Security Risks → Lit review → **Kiến trúc giải pháp (onion)** → Methodology → Results → Phân tích RQ → **Đạo đức (test env + responsible disclosure)** → Limitations → Kết luận *(Đạo đức: §14)*
- [ ] ⭐ Dọn **repo** theo cấu trúc mục 17 (benchmarks/micro, /tls, scripts, docker, docs, tools)
- [ ] ⭐ Hoàn thiện **Docker** + README "cách dựng lại" (đây là thứ vượt được bài MDPI vốn không có code)
- [ ] Xuất **CSV thô** + biểu đồ + binary sizes làm artifacts
- [ ] ⭐ Quay **demo** ngắn: chạy benchmark + handshake cổ điển vs PQC
- [ ] Slide thuyết trình

**Đầu ra:** Final report (PDF/MD) + code repo + Dockerfile + CSV + demo.
**Checkpoint (DoD):** người khác `docker build` ra đúng môi trường của bạn và chạy lại được thí nghiệm.

---

## 3. DANH SÁCH "TỐI ĐA ĐIỂM" (⭐ gom lại — đừng bỏ cái nào)

- [ ] Threat model / Security Risks rõ ràng
- [ ] Bản tối ưu **NEON** + so reference (RQ1/RQ2)
- [ ] So x86 (AVX2) vs ARM (NEON)
- [ ] TLS **qua mạng thật** + **netem** (đường cong mất gói)
- [ ] **Hybrid** handshake (RQ3)
- [ ] **Kiến trúc onion 3 lớp** + nginx PQC + Wireshark bóc tách PQC
- [ ] KAT (đúng đắn) + Validation (đối chiếu NIST/literature)
- [ ] Năng lượng (ước lượng power model) hoặc Limitations trung thực
- [ ] Bootstrap CI + phân tích phân phối Dilithium
- [ ] **Docker tái lập** + repo sạch + demo

---

## 4. ĐIỂM RỦI RO HAY GÃY (canh chừng tại các checkpoint)

| Rủi ro | Dấu hiệu | Xử lý |
|---|---|---|
| Build liboqs + oqs-provider | lỗi cmake/ninja, version lệch | clone đúng cặp version; đọc lỗi, dán hỏi |
| NEON không bật | số tối ưu = số reference | kiểm tra flag `asimd`, build đúng backend |
| Cycle trên ARM | không đọc được cycle | bật PMU qua kernel module; dùng ns làm chính |
| Throttling nhiệt Pi | số dao động, tụt xung | `vcgencmd get_throttled` = 0x0; loại lần bị bóp |
| Đo localhost (sai) | không thấy hiệu ứng mạng | bắt buộc Pi ↔ PC qua mạng thật |
| Chỉ báo mean | Dilithium/handshake lệch | median + p95/p99 + bootstrap |
| Năng lượng | không có phần cứng | power model hoặc Limitations |
| So sánh không công bằng | RSA-3072 vs L3 | map bảo mật + ghi giả định |

---

## 5. NÉN LỘ TRÌNH (nếu deadline gấp — bản tối thiểu vẫn có điểm)

Thứ tự ưu tiên nếu thiếu thời gian (giữ phần ăn điểm cao + khả thi nhất):
1. **WP1 + WP2 trên ARM** → có số microbench thật (Kyber/Dilithium + RSA/ECC). *(ăn 30%+25%)*
2. **Báo cáo** (lit review + methodology + bảng số + trả lời RQ + map bảo mật + limitations). *(ăn 20%+15%+10%)*
3. **Sơ đồ kiến trúc onion 3 lớp** (thiết kế, chưa cần đo đủ). *(đáp ứng GV)*
4. Còn giờ: thêm **TLS qua mạng** (ít nhất 1 lần đo cổ điển vs PQC) + **Docker**.
5. Cắt (ghi Limitations): năng lượng, ablation NEON đầy đủ, onion đo đủ 3 lớp.

> Nguyên tắc: **có số liệu thật + báo cáo chắc + thiết kế kiến trúc** > cố làm tất cả rồi dở dang.

---

## 6. VIỆC KẾ TIẾP NGAY BÂY GIỜ

➡ Bạn đang ở **đầu WP1**. Hành động kế tiếp: trên Pi, cài công cụ build → **build liboqs** → chạy `speed_kem`/`speed_sig` ra số (mốc lớn đầu tiên). Sau đó build OpenSSL+oqs-provider.

Khi `speed_kem`/`speed_sig` ra số → chuyển sang **harness KEM** (WP2) và bắt đầu thu CSV.