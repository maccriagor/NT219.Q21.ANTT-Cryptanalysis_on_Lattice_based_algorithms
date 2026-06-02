# 11 — WP0 Study Dossier (NIST · RFC/IETF · IEEE + Nền toán học)

> **Dùng cho WP0 (Tuần 1–2):** nền lý thuyết + literature review (≥6) + threat model + bảng map bảo mật.
> Mỗi nguồn: **trích dẫn chuẩn → nội dung → RÚT GÌ cho đồ án → link**. Cột "WP0" cho biết phục vụ phần nào.
> Ký hiệu: 📕 đọc kỹ · 📖 đọc chính phần liên quan · 📄 đọc lướt (context).

---

## ⭐ THỨ TỰ ĐỌC ĐỀ XUẤT (đọc theo mạch này cho dễ hiểu)

1. **NIST Security Categories** (định nghĩa L1/L3/L5) → để hiểu "so sánh công bằng" nghĩa là gì.
2. **Nền toán:** Regev (LWE) → LPR (Ring-LWE) → Langlois–Stehlé (Module-LWE/SIS) → Lyubashevsky (Fiat-Shamir-with-aborts).
3. **FIPS 203 + FIPS 204** → thuật toán chính thức (Kyber→ML-KEM, Dilithium→ML-DSA).
4. **IEEE/hội nghị benchmark** (Sikeridis, Paquin, Neon-NTT) → cách đo + kết quả tham chiếu.
5. **RFC 8446 + hybrid drafts** → giao thức TLS để đo (WP4).
6. **Threat model** (Shor, Grover, Mosca, HNDL) → "tại sao PQC ngay bây giờ".

---

## A. NIST — Chuẩn nền (đọc TRƯỚC) 

| # | Trích dẫn | Rút gì cho đồ án | WP0 |
|---|---|---|---|
| A1 📕 | **NIST, FIPS 203: Module-Lattice-Based Key-Encapsulation Mechanism (ML-KEM)**, Aug 2024. doi:10.6028/NIST.FIPS.203 | Định nghĩa chính thức **ML-KEM** (Kyber): K-PKE + biến đổi Fujisaki–Okamoto; 3 mức 512/768/1024; **kích thước ek/dk/ct** chuẩn. → Chương Background (KEM) + bảng tham số. | Background, Tham số |
| A2 📕 | **NIST, FIPS 204: Module-Lattice-Based Digital Signature (ML-DSA)**, Aug 2024. doi:10.6028/NIST.FIPS.204 | Định nghĩa **ML-DSA** (Dilithium): Fiat-Shamir-with-aborts trên Module-LWE+SIS; 3 mức 44/65/87; sizes chữ ký. → Background (signature). | Background |
| A3 📄 | **NIST, FIPS 205: Stateless Hash-Based Digital Signature (SLH-DSA)**, Aug 2024 | Đối chứng họ hash-based (SPHINCS+). Đọc lướt để so sánh "vì sao lattice nhanh hơn hash-based". | Lit review |
| A4 📕 | **NIST IR 8413: Status Report on the Third Round of the NIST PQC Standardization Process**, 2022 | **Lý do CHỌN** Kyber/Dilithium làm chuẩn chính (so với Falcon/SPHINCS+): cân bằng hiệu năng/kích thước/độ tin cậy. → Motivation + biện minh chọn thuật toán. | Motivation, Lit |
| A5 📕 | **NIST, Call for Proposals (Dec 2016) — mục Security Strength Categories** | **Định nghĩa Category 1–5**: L1/3/5 neo theo brute-force AES-128/192/256; L2/4 neo theo va chạm SHA. → **Bảng map bảo mật** (cốt lõi để so công bằng). | 🔴 Bảng bảo mật |
| A6 📖 | **NIST, FIPS 186-5: Digital Signature Standard** + **SP 800-186** (đường cong) | Chuẩn cho **baseline RSA/ECDSA/EdDSA** + P-256/384/521. → Biện minh baseline cổ điển. | Baseline |
| A7 📄 | **NIST, SP 800-208: Stateful Hash-Based Signatures (XMSS/LMS)**, 2020 | Context: chữ ký hậu-lượng-tử khác (stateful). Đọc lướt. | Lit (context) |

**Links:** [FIPS 203](https://csrc.nist.gov/pubs/fips/203/final) · [FIPS 204](https://csrc.nist.gov/pubs/fips/204/final) · [FIPS 205](https://csrc.nist.gov/pubs/fips/205/final) · [NIST IR 8413](https://csrc.nist.gov/pubs/ir/8413/upd1/final) · [Call for Proposals (security categories)](https://csrc.nist.gov/CSRC/media/Projects/Post-Quantum-Cryptography/documents/call-for-proposals-final-dec-2016.pdf) · [FIPS 186-5](https://csrc.nist.gov/pubs/fips/186-5/final) · [SP 800-208](https://csrc.nist.gov/pubs/sp/800/208/final)

---

## B. RFC / IETF — Chuẩn giao thức (cho WP4/WP5 + threat)

| # | Trích dẫn | Rút gì cho đồ án | WP0 |
|---|---|---|---|
| B1 📕 | **RFC 8446 — The TLS Protocol Version 1.3** (2018) | Hiểu **cấu trúc handshake** (ClientHello→Finished), `key_share`, `supported_groups`, chứng chỉ — để biết **đo cái gì** ở WP4. | Kiến trúc, WP4 |
| B2 📖 | **RFC 7748 — Elliptic Curves for Security (X25519)** | Thành phần **cổ điển** trong hybrid `X25519MLKEM768`. | Hybrid, WP4 |
| B3 📖 | **RFC 8032 — EdDSA (Ed25519)** | Baseline chữ ký hiện đại (đối chứng ML-DSA). | Baseline |
| B4 📄 | **RFC 8017 — PKCS #1 v2.2 (RSA)** | Baseline RSA (RSASSA-PSS/PKCS1). | Baseline |
| B5 📕 | **draft-ietf-tls-hybrid-design — Hybrid key exchange in TLS 1.3** | **Khung lý thuyết** ghép cổ điển+PQC (concatenation KEM). → cốt lõi **RQ3**. | 🔴 RQ3, WP4 |
| B6 📕 | **draft-ietf-tls-ecdhe-mlkem — Post-quantum hybrid ECDHE-MLKEM** | Định nghĩa nhóm cụ thể: **X25519MLKEM768**, SecP256r1MLKEM768, SecP384r1MLKEM1024. → nhóm bạn sẽ đo. | 🔴 WP4 |
| B7 📄 | **RFC 8391 (XMSS)** + **RFC 8554 (LMS/HSS)** | Hash-based đã chuẩn hóa (context, đối chứng). | Lit (context) |

> ⚠️ **Lưu ý học thuật:** Hybrid PQC TLS **vẫn là Internet-Draft, CHƯA thành RFC**. Trong báo cáo ghi "IETF draft" — đừng gọi là RFC (đây là lỗi roadmap đã sửa).

**Links:** [RFC 8446](https://www.rfc-editor.org/rfc/rfc8446) · [RFC 7748](https://www.rfc-editor.org/rfc/rfc7748) · [RFC 8032](https://www.rfc-editor.org/rfc/rfc8032) · [RFC 8017](https://www.rfc-editor.org/rfc/rfc8017) · [draft-ietf-tls-hybrid-design](https://datatracker.ietf.org/doc/draft-ietf-tls-hybrid-design/) · [draft-ietf-tls-ecdhe-mlkem](https://datatracker.ietf.org/doc/draft-ietf-tls-ecdhe-mlkem/) · [RFC 8391](https://www.rfc-editor.org/rfc/rfc8391) · [RFC 8554](https://www.rfc-editor.org/rfc/rfc8554)

---

## C. IEEE & hội nghị top — Nghiên cứu khoa học (lõi literature review + methodology)

| # | Trích dẫn | Rút gì cho đồ án | WP0 |
|---|---|---|---|
| C1 📕 | **J. Bos et al., "CRYSTALS-Kyber: A CCA-Secure Module-Lattice-Based KEM," *IEEE EuroS&P 2018*, pp. 353–367.** (ePrint 2017/634) | **Paper gốc Kyber** (đăng ở IEEE). Thiết kế, tham số, lập luận an toàn. | 📕 Background, Lit |
| C2 📕 | **L. Ducas et al., "CRYSTALS-Dilithium: A Lattice-Based Digital Signature Scheme," *IACR TCHES* 2018(1):238–268.** (ePrint 2017/633) | **Paper gốc Dilithium**. Fiat-Shamir-with-aborts, rejection sampling (→ giải thích phân phối lệch ở WP7a). | 📕 Background, Lit |
| C3 📕 | **D. Sikeridis, P. Kampanakis, M. Devetsikiotis, "Post-Quantum Authentication in TLS 1.3: A Performance Study," *NDSS 2020*.** (ePrint 2020/071) | Đo **handshake latency thực** khi dùng chữ ký PQC; chứng minh **kích thước key/cert tác động lớn** đến handshake. → methodology WP4 + RQ3. | 🔴 Methodology, RQ3 |
| C4 📖 | **D. Sikeridis, P. Kampanakis, M. Devetsikiotis, "Assessing the Overhead of PQC in TLS 1.3 and SSH," *ACM CoNEXT 2020*.** | Overhead handshake **1–300% (TLS)** tùy thuật toán. → số liệu tham chiếu để validate. | Validation, RQ3 |
| C5 📕 | **C. Paquin, D. Stebila, G. Tamvada, "Benchmarking Post-Quantum Cryptography in TLS," *PQCrypto 2020*, LNCS 12100:72–91.** (ePrint 2019/1447) | Đo PQC-TLS dưới **độ trễ mạng giả lập (emulated latency)** → chính là lý do bạn dùng **netem** ở WP4. Phương pháp chuẩn. | 🔴 Methodology (netem) |
| C6 📕 | **H. Becker, V. Hwang, M. Kannwischer, B.-Y. Yang, S.-Y. Yang, "Neon NTT: Faster Dilithium, Kyber, and Saber on Cortex-A72 and Apple M1," *IACR TCHES* 2022(1):221–244.** | Tối ưu **NEON trên Cortex-A72 = đúng CPU Pi 4 của bạn**. → cốt lõi WP3/RQ2 (NEON tăng tốc bao nhiêu). | 🔴 WP3, RQ2 |
| C7 📖 | **M. Kannwischer et al., "pqm4: Testing and Benchmarking NIST PQC on ARM Cortex-M4."** (repo mupq/pqm4) | Khung benchmark chuẩn cho ARM; cách đo cycle + memory công bằng. → methodology. | Methodology |
| C8 📖 | **G. Tasopoulos et al., "Energy Consumption Evaluation of PQ TLS 1.3 on Resource-Constrained Embedded Devices," ISPEC 2022 / Computing Frontiers 2023.** | Đo **năng lượng** PQC-TLS trên thiết bị nhúng. → tham chiếu cho WP6 (energy) + validation. | WP6, Validation |
| C9 📖 | **(IEEE Access / MDPI 2023–2024) Benchmark PQC trên Raspberry Pi 4** — vd "Evaluation of Performance, Energy, and Computation Costs of PQC" (IEEE Access); "Constrained Device Performance Benchmarking with PQC" (MDPI Cryptography 2024). | **Số liệu Pi 4 mới nhất** để **đối chiếu (validation)** kết quả của bạn nằm trong khoảng hợp lý. | 🔴 Validation (WP6) |

**Links:** [Kyber (ePrint)](https://eprint.iacr.org/2017/634.pdf) · [Dilithium (ePrint)](https://eprint.iacr.org/2017/633.pdf) · [Sikeridis NDSS 2020](https://www.ndss-symposium.org/wp-content/uploads/2020/02/24203.pdf) · [Sikeridis CoNEXT 2020](https://dl.acm.org/doi/10.1145/3386367.3431305) · [Paquin PQCrypto 2020](https://eprint.iacr.org/2019/1447) · [Neon NTT TCHES 2022](https://kannwischer.eu/papers/2021_neonntt_preprint20210726.pdf) · [pqm4](https://github.com/mupq/pqm4) · [IEEE Access PQC embedded](https://eprints.soton.ac.uk/486704/1/PQC_IEEE_Access_.pdf) · [MDPI constrained-device PQC](https://www.mdpi.com/2410-387X/8/2/21)

---

## D. Nền toán học — papers gốc (cho Mục tiêu 1, chương Background)

| # | Trích dẫn | Rút gì cho đồ án | WP0 |
|---|---|---|---|
| D1 📖 | **O. Regev, "On Lattices, Learning with Errors, Random Linear Codes, and Cryptography," *STOC 2005* / *JACM* 56(6), 2009.** | Bài toán **LWE** — nền của mọi lattice crypto. Hiểu: cho `b = A·s + e`, tìm `s` là khó. | 📕 Background §toán |
| D2 📄 | **O. Regev, "The Learning with Errors Problem" (survey/invited).** | Bản **dễ đọc** giải thích LWE — đọc cái này trước D1 cho đỡ nặng. | Background |
| D3 📖 | **V. Lyubashevsky, C. Peikert, O. Regev, "On Ideal Lattices and Learning with Errors over Rings," *EUROCRYPT 2010*.** | **Ring-LWE** — đưa cấu trúc đa thức vào để nhanh/gọn hơn. | Background |
| D4 📖 | **A. Langlois, D. Stehlé, "Worst-Case to Average-Case Reductions for Module Lattices," *Designs, Codes and Cryptography* 75(3):565–599, 2015.** (ePrint 2012/090) | **Module-LWE / Module-SIS** — nền trực tiếp của Kyber/Dilithium (cân bằng giữa LWE và Ring-LWE). | 📕 Background (cốt lõi) |
| D5 📖 | **V. Lyubashevsky, "Fiat-Shamir with Aborts: Applications to Lattice and Factoring-Based Signatures," *ASIACRYPT 2009*.** | Kỹ thuật **rejection sampling** biến identification scheme → chữ ký; nền của Dilithium. → giải thích vì sao thời gian sign **lệch phải** (WP7a). | 📕 Background (Dilithium) |
| D6 📄 | **C. Peikert, "A Decade of Lattice Cryptography," 2016 (survey).** | Survey toàn cảnh — tra cứu khi cần chiều sâu. | Tham khảo |

**Links:** [Regev LWE (STOC/JACM)](https://cims.nyu.edu/~regev/papers/qcrypto.pdf) · [Regev LWE survey](https://cims.nyu.edu/~regev/papers/lwesurvey.pdf) · [LPR Ring-LWE](https://eprint.iacr.org/2012/230) · [Langlois–Stehlé Module](https://eprint.iacr.org/2012/090) · [Lyubashevsky FS-with-aborts](https://www.iacr.org/archive/asiacrypt2009/59120596/59120596.pdf)

---

## E. THREAT MODEL — vật liệu xây dựng (Mục "Security Risks" của WP0)

| # | Khái niệm | Trích dẫn / nguồn | Dùng để |
|---|---|---|---|
| E1 📕 | **Thuật toán Shor** — phá RSA/ECC trong thời gian đa thức trên máy tính lượng tử | P. Shor, "Polynomial-Time Algorithms for Prime Factorization and Discrete Logarithms on a Quantum Computer," *SIAM J. Computing* 26(5), 1997 | Vì sao RSA/ECDSA **sụp đổ** trước lượng tử |
| E2 📖 | **Thuật toán Grover** — giảm một nửa độ an toàn đối xứng (√) | L. Grover, STOC 1996 | Vì sao cần **AES-256** (Cat 5), không phải AES-128 |
| E3 📕 | **Mosca's inequality (X + Y > Z)** — nếu (thời gian dữ liệu cần bí mật X) + (thời gian migrate Y) > (thời gian có máy lượng tử Z) thì **đã trễ** | M. Mosca, "Cybersecurity in an era with quantum computers," *IEEE S&P* 16(5), 2018 | Lập luận **"migrate ngay bây giờ"** |
| E4 📕 | **Harvest-Now, Decrypt-Later (HNDL)** — kẻ địch thu thập ciphertext hôm nay, giải mã khi có máy lượng tử | (đề cập trong NIST IR 8413, Mosca) | Threat chính cho **dữ liệu cần bí mật lâu dài** |

> **Ghép vào onion (WP5):** mỗi lớp chống đe dọa gì — Crypto (HNDL, nghe lén) · Auth (giả mạo danh tính) · Authz (leo quyền). Threat model là "tại sao" của kiến trúc.

**Links:** [Shor (SIAM)](https://doi.org/10.1137/S0097539795293172) · [Mosca IEEE S&P 2018](https://doi.org/10.1109/MSP.2018.3761723)

---

## F. ĐỐI CHIẾU: nguồn nào phục vụ deliverable nào của WP0

| Deliverable WP0 | Nguồn chính |
|---|---|
| **Chương Background (toán)** | D1–D5 + A1, A2, C1, C2 |
| **Literature review ≥6** (đủ chỉ tiêu §6) | C1, C2, C3, C5, C6, C7 (+ A4) → **đã ≥6** ✔ |
| **Bảng map bảo mật L1/L3/L5** | A5 (định nghĩa) + A6 (RSA/ECC) |
| **Threat model / Security Risks** | E1–E4 + A4 |
| **Chọn tham số** | A1, A2 (sizes) + A5 (mức) |

---

## G. BẢNG MAP BẢO MẬT (chốt sẵn — copy vào báo cáo)

| NIST Category | Neo theo (brute-force) | ML-KEM | ML-DSA | RSA | ECC |
|---|---|---|---|---|---|
| **1** | AES-128 key search | ML-KEM-512 | — | RSA-3072 | P-256 |
| **2** | SHA-256 collision | — | ML-DSA-44 | — | — |
| **3** | AES-192 key search | ML-KEM-768 | ML-DSA-65 | RSA-7680 | P-384 |
| **4** | SHA-384 collision | — | — | — | — |
| **5** | AES-256 key search | ML-KEM-1024 | ML-DSA-87 | RSA-15360 | P-521 |

> **Quyết định cần ghi rõ (WP0):** so **"iso-security"** (RSA-15360 ↔ Cat 5 — công bằng lý thuyết nhưng RSA-15360 không thực tế) **hay** so **"thực tiễn triển khai"** (RSA tối đa 2048/3072 — phổ biến nhưng chỉ ≈ Cat 1). → **Ghi giả định** để tránh lỗi "so sánh không công bằng".

---

*File bổ trợ cho WP0 trong `08` (ROADMAP). Cập nhật tên ML-KEM/ML-DSA (FIPS 203/204) và lưu ý "hybrid TLS = draft, chưa RFC".*
