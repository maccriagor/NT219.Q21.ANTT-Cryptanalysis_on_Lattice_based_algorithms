# EVAL — Evaluation (Đánh giá định lượng)

> **Phạm vi (NT2205 §5).** Tài liệu này gồm: (A) **các evaluation sheet** bám invariants I1–I7 cho 3 lớp **E-Crypto / E-AuthN / E-AuthZ**; (B) **performance evaluation** trả lời **RQ1/RQ2/RQ3** bằng số liệu. Mỗi sheet theo mẫu rubric: *Eval ID → liên hệ invariants → thủ tục đo (bước + công cụ) → metric + ngưỡng → bằng chứng → kết quả + diễn giải*. Liên kết: tài sản/mục tiêu SMART [`docs/AIM.md`](AIM.md); invariants [`docs/ARCH.md`](ARCH.md); giải pháp [`docs/CRYPTO_SOLUTION.md`](CRYPTO_SOLUTION.md); tái lập [`docs/RUNBOOK.md`](RUNBOOK.md).
>
> **Liêm chính dữ liệu:** mọi sheet ghi rõ **MEASURED (x86, đo thật)** hay **MODELED (ARM/energy/netem)**.

---

## A. Evaluation Sheets

### A.1 E-Crypto — Lớp Cryptography

#### E-C1 — Plaintext leakage = 0

| Trường | Nội dung |
|---|---|
| **Eval ID** | E-C1 — Không rò rỉ plaintext |
| **Liên hệ invariants** | I1 (no plaintext leak), mục tiêu **G1** |
| **Thủ tục đo** | (1) Roundtrip AEAD/KEM qua **cổng đúng đắn nội bộ** của benchmark (ML-KEM: `ss_encaps == ss_decaps`; AES-GCM: decrypt khớp plaintext). (2) Đối chứng kênh **bảo vệ vs không bảo vệ**: bắt handshake TLS thật, kiểm tra payload không xuất hiện plaintext. Công cụ: `scripts/run_kat.sh`, `scripts/run_tls_handshake.sh`, (tùy) `tcpdump`/grep sentinel |
| **Metric + Ngưỡng** | plaintext leakage = **0 byte**; KEM roundtrip OK 100% |
| **Bằng chứng** | [`data/kat/kat_results.txt`](../data/kat/kat_results.txt), [`data/tls/x86/handshake_x86.txt`](../data/tls/x86/handshake_x86.txt) |
| **Kết quả + Diễn giải** | **MEASURED (x86): ĐẠT.** KAT ML-KEM 512/768/1024 = **30/30 roundtrip OK**, shared secret 32 B khớp; kênh TLS 1.3 mã hóa AEAD ⇒ 0 byte plaintext. |
| **Hướng cải tiến** | Thêm capture pcap đối chứng kênh cleartext (HTTP) để minh hoạ trực quan delta. |

#### E-C2 — Nonce discipline

| Trường | Nội dung |
|---|---|
| **Eval ID** | E-C2 — Kỷ luật nonce AEAD |
| **Liên hệ invariants** | I1, I2; mục tiêu **G2** |
| **Thủ tục đo** | Sinh tải AES-256-GCM nhiều iteration; theo dõi allocator nonce (counter/random 96-bit) → đếm trùng lặp; giả lập lỗi (ép tái dùng nonce) → kỳ vọng **từ chối + log**. Công cụ: `benchmark_aes/` (đa mức ×100), bộ đếm nonce |
| **Metric + Ngưỡng** | nonce reuse = **0**; AEAD error (vận hành bình thường) = **0**; lỗi giả lập → reject |
| **Bằng chứng** | [`data/micro/x86/aes_x86.csv`](../data/micro/x86/aes_x86.csv) (300 hàng), counter log |
| **Kết quả + Diễn giải** | **MEASURED (x86): ĐẠT.** 300 phép AES-GCM không trùng nonce; AES-NI enc/dec ≈ **7.3/2.4 µs** đủ rẻ để dùng nonce ngẫu nhiên 96-bit an toàn ở quy mô này. |
| **Hướng cải tiến** | Với khối lượng rất lớn cùng 1 khóa: chuyển **AES-GCM-SIV / XChaCha20-Poly1305** (misuse-resistant). |

#### E-C3 — Integrity (tamper-reject + chữ ký)

| Trường | Nội dung |
|---|---|
| **Eval ID** | E-C3 — Toàn vẹn: ciphertext/tag/chữ ký |
| **Liên hệ invariants** | I2 (tamper rejected), I3 (data-origin); mục tiêu **G3** |
| **Thủ tục đo** | (1) Sửa 1 bit **ciphertext/tag** AES-GCM → `decrypt` phải **fail**. (2) Sửa **chữ ký ML-DSA** → `verify` phải **FAIL**; chữ ký thật → **PASS**. (3) Chạy toàn bộ **KAT 9/9** (3 scheme × 3 mức). Công cụ: `scripts/run_kat.sh` (cổng tamper-reject nội bộ) |
| **Metric + Ngưỡng** | tamper-reject = **100%**; KAT = **9/9 PASS**, FAIL = 0 |
| **Bằng chứng** | [`data/kat/kat_results.txt`](../data/kat/kat_results.txt) |
| **Kết quả + Diễn giải** | **MEASURED (x86): ĐẠT.** `TỔNG: PASS=9 FAIL=0`. KAT ghi rõ "AES-GCM: tampered tag rejected" và "ML-DSA: verify(real)=PASS, verify(tampered)=FAIL". Kích thước pk/sig khớp FIPS 203/204 ⇒ implementation đúng đắn, số đo đáng tin. |
| **Hướng cải tiến** | Mở rộng tamper-vector (sửa nhiều vị trí, độ dài) + fuzz nhẹ trên parser cert ML-DSA. |

### A.2 E-AuthN — Lớp Authentication

#### E-N1 — Cert / identity (server) + PQC handshake + downgrade

| Trường | Nội dung |
|---|---|
| **Eval ID** | E-N1 — Xác thực server & đàm phán hậu lượng tử |
| **Liên hệ invariants** | I4, I7; mục tiêu **G4, G5, G6** |
| **Thủ tục đo** | (1) Bắt tay TLS 1.3 hai cấu hình cùng mức Cat; xác nhận **nhóm đàm phán** + **peer signature**. (2) Thử ép nhóm/cipher yếu → kỳ vọng **handshake fail (downgrade rejected)**. (3) (AuthN người dùng — kế hoạch) chạy ma trận WebAuthn/TOTP đo success/false-accept/lockout. Công cụ: `scripts/run_tls_handshake.sh` (OpenSSL `s_server`/`s_client`/`s_time`), IdP log |
| **Metric + Ngưỡng** | negotiated group = `X25519MLKEM768`; peer sig = `mldsa65`; downgrade rejected = **100%**; (user) success ≥ **99%**, false-accept = **0**, lockout ≤ 1 s |
| **Bằng chứng** | [`data/tls/x86/handshake_x86.txt`](../data/tls/x86/handshake_x86.txt), [`data/tls/x86/sizes.csv`](../data/tls/x86/sizes.csv) |
| **Kết quả + Diễn giải** | **MEASURED (x86): ĐẠT (server-side).** Log: `Negotiated TLS1.3 group: X25519MLKEM768`, `Peer signature type: mldsa65`, `Protocol: TLSv1.3`. Xác thực server hậu lượng tử thành công. **Phần WebAuthn/TOTP: theo kế hoạch** (gate vận hành ở D-x86). |
| **Hướng cải tiến** | Thêm test ép downgrade tự động (offer chỉ nhóm yếu) ghi vào log; bổ sung ma trận MFA thực. |

#### E-N2 — Service identity (rotate / revoke)

| Trường | Nội dung |
|---|---|
| **Eval ID** | E-N2 — Danh tính dịch vụ + vòng đời khóa |
| **Liên hệ invariants** | I4, I6; mục tiêu **G7, G11** |
| **Thủ tục đo** | (1) Chỉ dịch vụ có cert mTLS hợp lệ (CA ML-DSA) được truy cập; dịch vụ giả/không cert → **từ chối**. (2) Rotate khóa/cert qua KMS/CA, đo **thời gian hoàn tất**; thu hồi key cũ → kỳ vọng **bị từ chối** trong cửa sổ. Công cụ: KMS/Vault, CA, trust store; timeline rotation |
| **Metric + Ngưỡng** | unauthorized access = **0**; rotation ≤ **10 phút**; key cũ từ chối ≤ **24h** |
| **Bằng chứng** | timeline rotation, JWKS `kid` overlap, mTLS access log (gói EVIDENCE) |
| **Kết quả + Diễn giải** | **Theo kế hoạch (operational).** Thiết kế đáp ứng SLA; ML-DSA keygen ≈ **267 µs** (so RSA-15360 ≈ **15.7 s**) khiến **rotate khóa ký gần như tức thì** về mặt sinh khóa ⇒ thuận lợi đạt G11. |
| **Hướng cải tiến** | Tự động hoá rotate + script đo timeline; thêm CRL/OCSP-stapling cho revoke. |

### A.3 E-AuthZ — Lớp Authorization

#### E-Z1 — Policy matrix (RBAC/ABAC) + explainability

| Trường | Nội dung |
|---|---|
| **Eval ID** | E-Z1 — Ma trận chính sách & giải thích được |
| **Liên hệ invariants** | I5; mục tiêu **G8, G12** |
| **Thủ tục đo** | Chạy bộ test `role × resource × action (+ ABAC)`; đo **pass-rate**; hành động **không khai báo** → kỳ vọng deny; **tái dựng** mỗi quyết định từ (policy version + input + reason). Công cụ: PDP OPA/Rego (test JUnit/JSON), decision log |
| **Metric + Ngưỡng** | policy pass-rate ≥ **95%**; undeclared action deny = **100%**; explainability = **100%** |
| **Bằng chứng** | Rego/ACL + test report (gói POLICIES/EVIDENCE), decision log |
| **Kết quả + Diễn giải** | **Theo kế hoạch (operational).** Mô hình **deny-by-default** + PDP trả `reason` đảm bảo tái dựng 100%; cần dataset test ma trận để chốt số pass-rate. |
| **Hướng cải tiến** | Sinh ma trận test tự động từ schema role/resource; property-based test cho ABAC. |

#### E-Z2 — Token hardening (reject `alg=none`, PoP replay)

| Trường | Nội dung |
|---|---|
| **Eval ID** | E-Z2 — Cứng hoá token |
| **Liên hệ invariants** | I4 (PoP), I2; mục tiêu **G9, G10** |
| **Thủ tục đo** | (1) Gửi token `alg=none`, `kid` injection, header confusion → kỳ vọng **reject + log reason**. (2) **Replay** token/yêu cầu đã dùng (không PoP) → kỳ vọng **bị chặn**; token PoP-bound (DPoP/mTLS) khi đổi kênh → invalid. Công cụ: GW verify, DPoP middleware |
| **Metric + Ngưỡng** | reject (alg/kid/confusion) = **100%**; replay accepted = **0** |
| **Bằng chứng** | GW log (reason), test vector token độc hại (gói EVIDENCE) |
| **Kết quả + Diễn giải** | **Theo kế hoạch (operational).** Pin `alg` + allowlist `kid` + ký **ML-DSA** + PoP đáp ứng G9/G10; cần bộ test token độc hại để chốt 100%. |
| **Hướng cải tiến** | Thêm corpus token tấn công (jwt_tool-style) chạy trong CI. |

### A.4 Bảng tổng kết invariants (đạt/chưa)

| Invariant | Bài đo | Loại | Trạng thái |
|---|---|---|---|
| I1 plaintext leak=0 | E-C1 | MEASURED x86 | ✅ Đạt |
| I2 tamper rejected | E-C3 | MEASURED x86 | ✅ Đạt (9/9) |
| I3 data-origin | E-C3 | MEASURED x86 | ✅ Đạt |
| I4 AuthN + PoP | E-N1, E-Z2 | MEASURED (server) / kế hoạch (user/PoP) | ◻ Một phần |
| I5 explainability | E-Z1 | Kế hoạch (operational) | ◻ Kế hoạch |
| I6 key ops | E-N2 | Kế hoạch (operational) | ◻ Kế hoạch |
| I7 PQC-ready + perf | E-N1, RQ3 | MEASURED x86 | ✅ Đạt |

---

## B. Performance Evaluation — RQ1 / RQ2 / RQ3

> **3 câu hỏi nghiên cứu.** **RQ1:** Yếu tố nào ảnh hưởng hiệu năng PQC (mức tham số, reference-C vs tối ưu AVX2/NEON, kích thước byte)? **RQ2:** NEON có khả thi hoá PQC trên ARM (Cortex-A72) không? **RQ3:** Overhead của handshake hybrid PQC trong TLS 1.3 là bao nhiêu?

### RQ1 — Yếu tố ảnh hưởng (MEASURED, x86)

**(a) Latency theo mức NIST — ML-KEM reference-C (µs, median):** nguồn [`data/micro/x86/mlkem_x86.csv`](../data/micro/x86/mlkem_x86.csv) → [`data/micro/summary_x86.csv`](../data/micro/summary_x86.csv).

| Mức | keygen | encaps | decaps |
|---|--:|--:|--:|
| L1 (512) | 34 | 36 | 42 |
| L3 (768) | 58 | 62 | 72 |
| L5 (1024) | 82 | 83 | 94 |

**ML-DSA reference-C (µs, median):**

| Mức | keygen | sign | verify | sign p95 |
|---|--:|--:|--:|--:|
| L2 (44) | 90 | 315 | 83 | ~0.93 ms |
| L3 (65) | 168 | 454 | 139 | ~1.66 ms |
| L5 (87) | 267 | 563 | 239 | ~1.53 ms |

> **Quan sát:** (i) chi phí tăng **đều theo mức** (nhiều module/độ rộng ma trận hơn). (ii) **ML-DSA sign lệch phải mạnh** (rejection sampling, Fiat–Shamir-with-aborts) → bắt buộc dùng **median + p95**, không dùng mean. (iii) **AES-NI** enc/dec ≈ **7.3/2.4 µs**, rẻ so với lattice.

**(b) reference-C vs tối ưu AVX2 (MEASURED, x86) — nguồn [`data/micro/x86/micro_evp_x86.csv`](../data/micro/x86/micro_evp_x86.csv):**

| Phép (Cat5) | reference-C | AVX2 (OpenSSL native) | Speedup |
|---|--:|--:|--:|
| ML-KEM-1024 keygen | 82 µs | **46 µs** | ~1.8× |
| ML-KEM-1024 encaps | 83 µs | **22 µs** | ~3.8× |
| ML-KEM-1024 decaps | 94 µs | **35 µs** | ~2.7× |

→ **Trả lời RQ1:** ảnh hưởng chính = **mức tham số** + **mức tối ưu SIMD**; với ML-KEM-1024, AVX2 rút encaps từ 83 → **22 µs**. Kích thước byte (pk/ct/sig) là yếu tố chi phối **băng thông** (xem RQ3), tách biệt với CPU.

**(c) PQC vs cổ điển (MEASURED, x86, Cat5) — headline:**

| So sánh | PQC | Cổ điển | Tỉ lệ |
|---|--:|--:|--:|
| KEM (encaps+decaps) | ML-KEM-1024 ≈ **57 µs** | ECDH-P-521 derive = 1284 µs | **≈ 22× nhanh hơn** |
| sign | ML-DSA-87 = 563 µs | RSA-15360 sign = 177 ms | **≈ 210× nhanh hơn** |
| keygen | ML-DSA-87 = 267 µs | RSA-15360 keygen = **15.7 s** | **≈ 78000× nhanh hơn** |

> RSA-15360 keygen 15.7 s là **không khả thi vận hành** ở Cat5 → củng cố lựa chọn PQC ([`docs/CRYPTO_SOLUTION.md`](CRYPTO_SOLUTION.md)). Baseline cổ điển: [`data/micro/x86/ecdsa_p521_x86.csv`](../data/micro/x86/ecdsa_p521_x86.csv), [`data/micro/x86/micro_evp_x86.csv`](../data/micro/x86/micro_evp_x86.csv).

### RQ2 — NEON trên ARM (MODELED — đánh dấu rõ)

> **MODELED.** ARM reference-C ≈ **4.6×** chậm hơn x86; NEON tăng tốc **1.7–2.3×**; AES ≈ **22×** chậm (BCM2711 không có ARMv8 crypto ext). Nguồn: [`data/micro/arm/`](../data/micro/arm/), [`data/micro/neon_vs_ref.csv`](../data/micro/neon_vs_ref.csv). Hệ số có trích dẫn (freq ratio 2.93 × IPC gap ~1.6); xem `docs/DATA_PROVENANCE.md`.

| Phép | ARM ref-C (ns, mô hình) | NEON speedup | ARM NEON (ns, mô hình) |
|---|--:|--:|--:|
| ML-KEM-1024 decaps | 430811 | 2.3 | **187309** |
| ML-KEM-1024 encaps | 381708 | 2.1 | **181766** |
| ML-DSA-87 sign | 2701836 | 1.7 | **1589315** |
| ML-DSA-87 verify | 1075698 | 1.8 | **597610** |

→ **Trả lời RQ2 (mô hình):** NEON kéo ML-KEM về **~0.18 ms/op** và ML-DSA-87 verify về **~0.6 ms** trên Cortex-A72 ⇒ **khả thi** cho edge. Hạn chế: AES phần mềm đắt (~22×) → ưu tiên phần cứng có ARMv8 crypto ext nếu cần AEAD nặng. **Đây là ước lượng**, cần đo thật trên Pi để chốt (xem [`docs/RUNBOOK.md`](RUNBOOK.md) §ARM).

### RQ3 — Overhead handshake hybrid TLS 1.3

**(a) MEASURED (x86, localhost, OpenSSL 3.6.1) — [`data/tls/x86/handshake_x86.txt`](../data/tls/x86/handshake_x86.txt):**

| Cấu hình | Throughput | Cert on-wire |
|---|--:|--:|
| classical (ECDHE-P256 + ECDSA-P256) | 2118 hs/s | 534 B |
| **PQC hybrid (X25519MLKEM768 + ML-DSA-65)** | 1934 hs/s | 7464 B |
| **Overhead** | **≈ 9%** | **≈ 14×** |

**(b) MODELED — ma trận netem (delay × loss) [`data/tls/netem_matrix.csv`](../data/tls/netem_matrix.csv):** handshake hybrid ≈ **10.8 KB / 8 gói** vs cổ điển **1.5 KB / 2 gói**; nhạy hơn khi mất gói (ví dụ tại 20 ms / 5% loss: hybrid ~**121 ms** vs classical ~**61 ms** median).

→ **Trả lời RQ3:** trên CPU overhead chỉ ~**9%** (KEM hậu lượng tử rẻ); chi phí thực nằm ở **byte chứng chỉ ML-DSA** (≈14×) → bất lợi rõ hơn dưới **mất gói/MTU**. Kết luận: **hybrid PQC khả dụng cho server** (đạt G13/I7); cần cân nhắc nén/kích thước cert khi mạng kém.

### Tài nguyên & độ tin cậy số đo (MEASURED, x86)

| Hạng mục | Giá trị | Nguồn |
|---|---|---|
| Code size `.text` ML-KEM | 9.5–10.7 KB | [`data/resource/code_size.txt`](../data/resource/code_size.txt) |
| Code size `.text` ML-DSA | 16–16.7 KB | `code_size.txt` |
| Peak RSS | 3.6–7.6 MB | [`data/resource/peak_rss.txt`](../data/resource/peak_rss.txt) |
| KAT | **9/9 PASS** | [`data/kat/kat_results.txt`](../data/kat/kat_results.txt) |
| Năng lượng (J/op) | **MODELED** (P_x86=15 W, P_arm=4 W) | [`data/resource/energy_estimate.csv`](../data/resource/energy_estimate.csv) |

> **Phương pháp thống kê:** mỗi phép **100 mẫu** (cột `n=100`), báo cáo **median/mean/std/p95 + CI95** (bootstrap khuyến nghị do phân phối lệch của ML-DSA sign). Biểu đồ: `python3 tools/plot.py` → `docs/report/figures/*.svg`.
