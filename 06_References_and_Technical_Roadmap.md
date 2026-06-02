# 06 — References & Technical Roadmap

**Phụ lục học thuật + kỹ thuật cho đề tài:** *Implement and Benchmark Lattice-based Schemes (ML-KEM / ML-DSA): so sánh hiệu năng với RSA/ECC trên x86_64 và ARM.*

**Mục đích file này:** cung cấp (a) danh mục tài liệu/chuẩn quốc tế trích dẫn đúng học thuật để dán vào *Literature Review* và *References* của báo cáo, và (b) lộ trình kỹ thuật bám sát code hiện có (`main.cpp`, `runner.py`, `rsa15360_full/`, `benchmark_p521/`) để "ra là làm được".

> ⚠️ **Quy ước tên thuật toán (bắt buộc trong báo cáo):** kể từ 13/08/2024 NIST chuẩn hoá **Kyber → ML-KEM (FIPS 203)** và **Dilithium → ML-DSA (FIPS 204)**. Bản FIPS-final khác *nhẹ* so với bản round-3 (domain separation, xử lý hash). Repo dùng PQClean `MLKEM*`/`MLDSA*` = **bản FIPS-final** → phải gọi đúng là ML-KEM/ML-DSA, ghi tên Kyber/Dilithium trong ngoặc khi nhắc paper gốc.

---

## PHẦN I — Literature Review (cho mục 6 của đề cương)

### 1.1. Tiêu chuẩn quốc tế (Standards)

| Mã chuẩn | Tên | Vai trò trong đề tài |
|---|---|---|
| **NIST FIPS 203** | Module-Lattice-Based Key-Encapsulation Mechanism (ML-KEM) | Chuẩn cho thuật toán KEM benchmark chính |
| **NIST FIPS 204** | Module-Lattice-Based Digital Signature (ML-DSA) | Chuẩn cho thuật toán chữ ký benchmark chính |
| **NIST FIPS 205** | Stateless Hash-Based Digital Signature (SLH-DSA) | Đối chứng họ hash-based (mở rộng) |
| **NIST FIPS 186-5** + **SP 800-186** | Digital Signature Standard; đường cong P-256/384/521 | Cơ sở chuẩn cho baseline RSA/ECDSA/ECDH |
| **NIST SP 1800-38** / **SP 800-227 (draft)** | Migration to PQC; Recommendations for KEM | Bối cảnh "Motivation/Relevance" |
| **ISO/IEC 18033-2** (+ amendment) | Asymmetric ciphers (đang bổ sung ML-KEM, FrodoKEM, Classic McEliece) | Chuẩn quốc tế ngoài NIST cho KEM |
| **ISO/IEC 14888-3** | Digital signatures with appendix (đường đưa ML-DSA/SLH-DSA vào ISO) | Chuẩn quốc tế ngoài NIST cho chữ ký |
| **IETF draft-ietf-tls-hybrid-design** | Hybrid key exchange in TLS 1.3 | Khung lý thuyết cho RQ3 (hybrid handshake) |
| **IETF draft-ietf-tls-ecdhe-mlkem** | X25519MLKEM768, SecP256r1MLKEM768, SecP384r1MLKEM1024 | Định nghĩa nhóm hybrid cụ thể đo trong RQ3 |
| **IETF RFC 9370** | Multiple Key Exchanges in IKEv2 | Bối cảnh hybrid ngoài TLS |

### 1.2. Bài báo nền tảng thuật toán
- **[Kyber]** CRYSTALS-Kyber — KEM dựa trên Module-LWE, IND-CCA2 qua biến đổi Fujisaki-Okamoto.
- **[Dilithium]** CRYSTALS-Dilithium — chữ ký "Fiat-Shamir with Aborts" dựa trên Module-LWE + Module-SIS.
- **[Regev05]** LWE — bài toán khó nền tảng; **[LPR10]** Ring-LWE; **[LS15]** Module lattices (Module-LWE/SIS).

### 1.3. Tối ưu phần cứng (lõi cho RQ1/RQ2 — ARM/NEON)
- **[NeonNTT]** Becker et al., TCHES 2022 — NTT dùng NEON trên **Cortex-A72 (đúng CPU Raspberry Pi 4)** và Apple M1. **Đây là tài liệu kỹ thuật chính cho phần NEON.**
- **[pqm4]** Kannwischer et al. — framework benchmark de-facto trên Cortex-M4.
- **[KyberM4]** Botros-Kannwischer-Schwabe — Kyber tiết kiệm bộ nhớ trên Cortex-M4.

### 1.4. Phương pháp benchmark (cho mục 7.4 / 7.5)
- **[SUPERCOP]** Bernstein-Lange, eBACS — chuẩn vàng đo cycle: *median-of-many measurements*, tắt frequency scaling. Dùng đối chiếu số đo của ta.

### 1.5. Công trình so sánh PQC vs RSA/ECC (giống đề tài nhất — đọc để định khung)
- **[Perf25]** Performance analysis ML-KEM/ML-DSA vs RSA & ECDH/ECDSA trên x86 / Raspberry Pi 4 / macOS (arXiv 2505.02239).
- **[RP2040]** Benchmark ML-KEM/ML-DSA trên Cortex-M0+ — gồm cả **energy** (arXiv 2603.19340).
- **[PQTLS]** Faster Post-Quantum TLS 1.3 Based on ML-KEM (arXiv 2404.13544) — cho phần macrobenchmark TLS.

### 1.6. Repo / dự án tham chiếu
| Repo | Vai trò |
|---|---|
| **PQClean** | Reference C đã FIPS-final (đang dùng) — "reference build". |
| **liboqs + oqs-provider** (Open Quantum Safe) | API bậc cao + tích hợp TLS; build NEON-optimized. |
| **OpenSSL ≥ 3.5.0** | Baseline RSA/ECC **và** native ML-KEM/ML-DSA cho TLS. |
| **pqm4** (mupq) | Khung benchmark Cortex-M (nếu mở rộng MCU). |
| **SUPERCOP** | Ground-truth cycle counts để đối chiếu. |

---

## PHẦN II — Bảng tham số chuẩn (fair-comparison ở Category 5)

So sánh phải cùng mức an toàn. Lựa chọn của repo là **NIST Category 5 (≈ AES-256 / ~256-bit)** — đúng và công bằng. Số liệu đã đối chiếu khớp `#define` trong `main.cpp`.

| Scheme | Std | Cat | Public key | Secret key | CT / Signature |
|---|---|---|---|---|---|
| ML-KEM-1024 | FIPS 203 | 5 | 1568 B | 3168 B | ct **1568 B**, ss 32 B |
| ML-DSA-87 | FIPS 204 | 5 | 2592 B | 4896 B | sig **4627 B** |
| RSA-15360 | (cổ điển) | ~5 | ~1920 B | ~7.7 KB | sig **1920 B** |
| ECDSA/ECDH P-521 | FIPS 186-5 | ~5 | 133 B (point) | 66 B | sig ~132 B |

**Bảng đối chiếu mức an toàn (để justify lựa chọn tham số):**

| NIST Cat | ≈ Đối xứng | RSA | ECC |
|---|---|---|---|
| 1 | AES-128 | RSA-3072 | P-256 |
| 3 | AES-192 | RSA-7680 | P-384 |
| 5 | AES-256 | **RSA-15360** | **P-521** |

---

## PHẦN III — Đối chiếu hiện trạng repo với chuẩn

| Thành phần repo | Tình trạng | Ghi chú |
|---|---|---|
| `main.cpp` — ML-KEM-1024 + ML-DSA-87 + AES-256-GCM | ✅ Chạy được, đúng FIPS-final | Là "authenticated KEM" mini (Bob ký pk_kem, Amy verify rồi encapsulate) |
| `rsa15360_full/`, `benchmark_p521/` | ✅ Có baseline Cat-5 | Tách keygen RSA-15360 khỏi biểu đồ steady-state (quá chậm) |
| `Dockerfile` (OpenSSL 3.6.1 + PQClean, multi-stage) | ✅ Tốt, reproducible | Thêm `buildx --platform linux/arm64` cho ARM |
| Đo "cycles" qua `rdtsc`/`cntvct_el0` | ⚠️ Sai khái niệm | Xem F1 — không phải core cycles |
| `runner.py` fork-per-iteration | ⚠️ Méo số liệu | Xem F1.2 — chuyển sang in-process loop |
| Chỉ đo 1 mức (Cat 5) | ⚠️ Thiếu cho RQ1 | Xem F2 — cần cả Cat 1/3/5 |
| TLS macrobenchmark (RQ3) | ❌ Chưa có | Xem F4 |

---

## PHẦN IV — Lộ trình kỹ thuật (NHỮNG VIỆC CẦN LÀM)

### F1. Sửa phương pháp ĐO (ưu tiên #1 — chiếm 30% rubric "rigor")
1. **`cntvct_el0` (ARM) KHÔNG phải CPU cycles** — đó là *virtual timer* tần số cố định (~19.2/54 MHz). Trên ARM: hoặc chỉ báo cáo **thời gian (ns)**, hoặc đọc `PMCCNTR_EL0` qua `perf`/kernel module để có cycle thật. **`rdtsc` (x86)** là *invariant TSC* (tần số tham chiếu), ≠ core cycles khi bật turbo/DVFS → muốn cycle thật phải pin tần số hoặc dùng `perf stat`.
2. **Bỏ fork-per-iteration** trong `runner.py`. Chuyển vòng lặp đo vào **trong C++**: `K` warm-up (vd 100) + `N` đo (1000–10000) cho mỗi op, gom mảng, in **median / mean / std / 95% CI** (bootstrap). Đây đúng cách SUPERCOP làm.
3. **Cố định môi trường đo** (script `bench_env.sh`):
   ```bash
   sudo cpupower frequency-set -g performance     # pin governor
   echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost   # tắt turbo (x86)
   taskset -c 2 ./app                              # pin core
   echo 0 | sudo tee /proc/sys/kernel/randomize_va_space     # tắt ASLR khi đo cycle
   ```

### F2. Hoàn thiện ma trận so sánh 3 mức (cho RQ1)
PQClean có sẵn cả 3 mức → chỉ đổi macro tên hàm + `#define` size (xem Phụ lục A). Đo: ML-KEM-512/768/1024, ML-DSA-44/65/87, RSA-3072/7680/15360, P-256/384/521. Tách **RSA keygen** ra biểu đồ riêng.

### F3. Hai nền tảng + tối ưu (RQ1/RQ2)
- Build **x86_64** và **ARM aarch64 (Pi 4)**: `docker buildx build --platform linux/arm64` hoặc cross-compile `aarch64-linux-gnu-g++`.
- Đo **2 biến thể**: PQClean `clean` (reference C) **vs** liboqs build NEON-optimized (`OQS_USE_CPU_EXTENSIONS=ON`). Chênh lệch này = nội dung RQ2.
- Ablation flags: `-O2` vs `-O3` vs `-O3 -mcpu=cortex-a72`; ghi rõ gcc/clang version.

### F4. Macrobenchmark TLS 1.3 hybrid (RQ3) — dùng OpenSSL ≥ 3.5 native (KHÔNG dùng OQS fork đã deprecated)
```bash
# Server: chứng chỉ ký bằng ML-DSA-87 / RSA-15360 / ECDSA P-521 (3 lần đo riêng)
openssl s_server -cert cert.pem -key key.pem -groups X25519MLKEM768 -www -accept 4433
# Client đo handshake:
openssl s_client -connect localhost:4433 -groups X25519MLKEM768   # hybrid PQC
openssl s_client -connect localhost:4433 -groups x25519           # cổ điển (baseline)
```
Đo: handshake latency, bytes truyền (full ClientHello→Finished), throughput dưới tải đồng thời (`wrk`).

### F5. Energy (optional, ăn điểm)
- x86: `perf stat -e power/energy-pkg/ ./app` (Intel/AMD RAPL).
- ARM Pi: không có RAPL → INA219/INA226 đo dòng qua shunt, hoặc ước lượng `cycles × TDP/freq` (ghi rõ "ước lượng").

### F6. Side-channel / constant-time (extension — nâng tầm học thuật)
- Nêu rõ PQClean `clean` + OpenSSL RSA/ECC đều constant-time. Chạy `dudect` hoặc `valgrind --tool=memcheck` (kỹ thuật ct-verif/timecop) để kiểm chứng không rò rỉ timing.

---

## PHẦN V — Lỗi/điểm cần sửa cụ thể

1. **Đổi tên** "Kyber → ML-KEM (FIPS 203)", "Dilithium → ML-DSA (FIPS 204)" trong toàn bộ doc 05.
2. **Doc 05 mục 7.2 & 16:** thay *"OpenSSL-OQS fork"* → *"OpenSSL ≥ 3.5 native, fallback oqs-provider"* (fork đã deprecated).
3. **`main.cpp` (hàm `get_cycles`):** không gọi giá trị `cntvct_el0`/`rdtsc` là "cycles" trong báo cáo (xem F1.1).
4. **Scope:** repo tên *"Cryptanalysis on Lattice-based algorithms"* nhưng nội dung là *Implement & Benchmark* — làm rõ trong báo cáo (đây là benchmark, không phải lattice reduction/cryptanalysis).
5. **`runner.py`:** thay kiến trúc fork-per-iteration (F1.2).

---

## Phụ lục A — Mapping PQClean cho 3 mức bảo mật (phục vụ F2)

| Mức | Macro hàm (KEM) | PK / SK / CT |
|---|---|---|
| Cat 1 | `PQCLEAN_MLKEM512_CLEAN_crypto_kem_{keypair,enc,dec}` | 800 / 1632 / 768 |
| Cat 3 | `PQCLEAN_MLKEM768_CLEAN_crypto_kem_{keypair,enc,dec}` | 1184 / 2400 / 1088 |
| Cat 5 | `PQCLEAN_MLKEM1024_CLEAN_crypto_kem_{keypair,enc,dec}` | 1568 / 3168 / 1568 |

| Mức | Macro hàm (Signature) | PK / SK / SIG |
|---|---|---|
| Cat 2 | `PQCLEAN_MLDSA44_CLEAN_crypto_sign_{keypair,signature,verify}` | 1312 / 2560 / 2420 |
| Cat 3 | `PQCLEAN_MLDSA65_CLEAN_crypto_sign_{keypair,signature,verify}` | 1952 / 4032 / 3309 |
| Cat 5 | `PQCLEAN_MLDSA87_CLEAN_crypto_sign_{keypair,signature,verify}` | 2592 / 4896 / 4627 |

ss (shared secret) ML-KEM mọi mức = 32 B.

---

## Phụ lục B — Danh mục tài liệu tham khảo (IEEE-style, dán thẳng vào References)

[1] National Institute of Standards and Technology, *Module-Lattice-Based Key-Encapsulation Mechanism Standard*, FIPS PUB 203, Aug. 2024. doi:10.6028/NIST.FIPS.203.

[2] National Institute of Standards and Technology, *Module-Lattice-Based Digital Signature Standard*, FIPS PUB 204, Aug. 2024. doi:10.6028/NIST.FIPS.204.

[3] National Institute of Standards and Technology, *Stateless Hash-Based Digital Signature Standard*, FIPS PUB 205, Aug. 2024. doi:10.6028/NIST.FIPS.205.

[4] National Institute of Standards and Technology, *Digital Signature Standard (DSS)*, FIPS PUB 186-5, Feb. 2023.

[5] J. Bos, L. Ducas, E. Kiltz, T. Lepoint, V. Lyubashevsky, J. M. Schanck, P. Schwabe, G. Seiler, and D. Stehlé, "CRYSTALS – Kyber: A CCA-Secure Module-Lattice-Based KEM," in *Proc. IEEE European Symp. Security and Privacy (EuroS&P)*, 2018, pp. 353–367. (IACR ePrint 2017/634)

[6] L. Ducas, E. Kiltz, T. Lepoint, V. Lyubashevsky, P. Schwabe, G. Seiler, and D. Stehlé, "CRYSTALS-Dilithium: A Lattice-Based Digital Signature Scheme," *IACR Trans. Cryptographic Hardware and Embedded Systems (TCHES)*, vol. 2018, no. 1, pp. 238–268, 2018. (IACR ePrint 2017/633)

[7] O. Regev, "On lattices, learning with errors, random linear codes, and cryptography," in *Proc. 37th ACM Symp. Theory of Computing (STOC)*, 2005, pp. 84–93.

[8] V. Lyubashevsky, C. Peikert, and O. Regev, "On Ideal Lattices and Learning with Errors over Rings," in *Advances in Cryptology – EUROCRYPT 2010*, pp. 1–23.

[9] A. Langlois and D. Stehlé, "Worst-case to average-case reductions for module lattices," *Designs, Codes and Cryptography*, vol. 75, no. 3, pp. 565–599, 2015.

[10] H. Becker, V. Hwang, M. J. Kannwischer, B.-Y. Yang, and S.-Y. Yang, "Neon NTT: Faster Dilithium, Kyber, and Saber on Cortex-A72 and Apple M1," *IACR TCHES*, vol. 2022, no. 1, pp. 221–244, 2022.

[11] M. J. Kannwischer, R. Petri, J. Rijneveld, P. Schwabe, and K. Stoffelen, "pqm4: Testing and Benchmarking NIST PQC on ARM Cortex-M4," *2nd NIST PQC Standardization Conf.*, 2019. (repo: github.com/mupq/pqm4)

[12] L. Botros, M. J. Kannwischer, and P. Schwabe, "Memory-Efficient High-Speed Implementation of Kyber on Cortex-M4," in *AFRICACRYPT 2019*, pp. 209–228.

[13] D. J. Bernstein and T. Lange (eds.), "eBACS: ECRYPT Benchmarking of Cryptographic Systems." [Online]. Available: https://bench.cr.yp.to/

[14] D. Stebila, S. Fluhrer, and S. Gueron, "Hybrid key exchange in TLS 1.3," IETF Internet-Draft draft-ietf-tls-hybrid-design (work in progress).

[15] K. Kwiatkowski, P. Kampanakis, B. E. Westerbaan, and D. Stebila, "Post-quantum hybrid ECDHE-MLKEM Key Agreement for TLSv1.3," IETF Internet-Draft draft-ietf-tls-ecdhe-mlkem (work in progress).

[16] ISO/IEC 18033-2:2006, *Information technology — Security techniques — Encryption algorithms — Part 2: Asymmetric ciphers* (amendment adding PQC KEMs, in progress).

[17] D. Stebila and M. Mosca, "Post-Quantum Key Exchange for the Internet and the Open Quantum Safe Project," in *Selected Areas in Cryptography (SAC) 2016*, pp. 14–37. (liboqs / oqs-provider)

[18] PQClean contributors, "PQClean: clean, portable, tested implementations of post-quantum cryptography." [Online]. Available: https://github.com/PQClean/PQClean

---

*File này bổ sung cho `05_Implement & Benchmark Lattice-based Schemes (Kyber, Dilithium).md`. Cập nhật tên thuật toán và tooling theo tình trạng chuẩn hoá tính đến 2025–2026.*
