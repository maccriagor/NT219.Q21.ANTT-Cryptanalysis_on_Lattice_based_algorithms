# CRYPTO_SOLUTION — Giải pháp mật mã 3 lớp (Crypto / AuthN / AuthZ)

> **Phạm vi (NT2205 §3).** Tài liệu này đặc tả giải pháp mật mã theo **3 lớp** của mô hình onion: **3.1 Cryptography** (bảo vệ dữ liệu), **3.2 Authentication** (xác thực), **3.3 Authorization** (cấp quyền). Mỗi quyết định thuật toán được **biện minh bằng trade-off ĐO ĐƯỢC** (x86 thật) và **không cố định thuật toán lỗi thời**. Tài sản: [`docs/AIM.md`](AIM.md); vị trí thi hành: [`docs/ARCH.md`](ARCH.md); kiểm chứng: [`docs/EVAL.md`](EVAL.md).

---

## 0) Bảng tham số & ánh xạ mức bảo mật (so sánh công bằng)

> Nguyên tắc so "iso-security": đối chứng PQC ↔ RSA/ECC **cùng NIST Category**, không so RSA-3072 với Cat5.

| NIST Cat | ML-KEM | ML-DSA | RSA | ECDH/ECDSA | AES |
|:--:|:--:|:--:|:--:|:--:|:--:|
| **1** | ML-KEM-512 | ML-DSA-44 (Cat2) | RSA-3072 | P-256 | AES-128 |
| **3** | ML-KEM-768 | ML-DSA-65 | RSA-7680 | P-384 | AES-192 |
| **5** | ML-KEM-1024 | ML-DSA-87 | RSA-15360 | P-521 | AES-256 |

**Kích thước (byte) — đối chứng FIPS 203/204, đã KAT 9/9 PASS:**

| Thuật toán | pk | sk | ct / sig |
|---|--:|--:|--:|
| ML-KEM-1024 | 1568 | 3168 | **1568** (ct) |
| ML-DSA-87 | 2592 | 4896 | **4627** (sig) |
| ECDSA-P-521 | 133 | 223 | **139** (sig) |
| RSA-15360 | — | — | **1920** (sig) |

---

## 3.1 Lớp Cryptography — Bảo vệ dữ liệu

### (a) Truyền (in-transit): TLS 1.3 + KEX hybrid + AEAD

| Hạng mục | Lựa chọn | Biện minh |
|---|---|---|
| Giao thức | **TLS 1.3** (RFC 8446); **tắt 0-RTT** cho giao dịch nhạy | Loại bỏ cipher cũ, giảm bề mặt downgrade; 0-RTT có nguy cơ replay |
| KEX | **`X25519MLKEM768`** (hybrid: ML-KEM-768 ⊕ X25519) | Chống **HNDL**: an toàn nếu *một* nhánh còn an toàn; là nhóm IETF `draft-ietf-tls-ecdhe-mlkem` |
| AEAD record | **AES-256-GCM** (`TLS_AES_256_GCM_SHA384`) | Category 5 chống Grover; AES-NI nhanh (enc/dec ≈ **7.3/2.4 µs** đo thật) |
| Ciphersuite | Thu gọn; chỉ TLS 1.3 suites; pinning tùy chọn | Giảm cấu hình sai / downgrade (R8) |

**Bằng chứng đàm phán (đo THẬT, localhost, OpenSSL 3.6.1 — [`data/tls/x86/handshake_x86.txt`](../data/tls/x86/handshake_x86.txt)):**
```
Negotiated TLS1.3 group: X25519MLKEM768     ← KEX hybrid PQC
Peer signature type: mldsa65                ← cert ký ML-DSA-65
Protocol: TLSv1.3   Cipher: TLS_AES_256_GCM_SHA384
```

**Trade-off đo được (vì sao chọn hybrid):**

| Cấu hình | Throughput | Cert on-wire | Ghi chú |
|---|--:|--:|---|
| Classical (ECDHE-P256 + ECDSA-P256) | **2118 hs/s** | 534 B | baseline |
| **PQC hybrid (X25519MLKEM768 + ML-DSA-65)** | **1934 hs/s** | 7464 B | overhead ≈ **9%**; cert ≈ **14×** |

→ KEM hậu lượng tử rất rẻ về CPU; chi phí thực tế nằm ở **byte chứng chỉ/chữ ký** (ML-DSA), nhạy hơn khi mất gói (xem ma trận netem, RQ3 ở [`docs/EVAL.md`](EVAL.md)). Overhead ~9% chấp nhận được → đáp ứng mục tiêu **G13/I7**.

### (b) Lưu trữ (at-rest): Envelope encryption + quản trị nonce

- **Envelope:** mỗi đối tượng dùng **DEK** (AES-256-GCM) được **bọc bởi KEK** ở KMS/Vault; KEK không rời KMS. DEK per-object giới hạn blast-radius.
- **Misuse-resistance:** ưu tiên **AES-256-GCM với quản trị nonce chuẩn** (counter/random 96-bit không tái dùng dưới cùng khóa); nơi rủi ro tái dùng nonce cao có thể chọn **AES-GCM-SIV / XChaCha20-Poly1305** (misuse-resistant). Mục tiêu **G2**: nonce reuse = 0.
- **Vùng cấm:** plaintext & shared secret (32 B) chỉ tồn tại in-process, **zeroize sau dùng**, không ghi log (A1.3).

### (c) Chữ ký / Xác thực dữ liệu: ML-DSA

- **Chứng chỉ & artifact** ký bằng **ML-DSA** (FIPS 204); hàm băm/label/tham số (44/65/87) **tài liệu hóa**. Verify pass ⇒ **toàn vẹn nguồn gốc (I3)**; verify fail ⇒ **tamper reject (I2)** — đã chứng minh bằng cổng "verify(tampered)=FAIL" trong KAT.

**Vì sao ML-DSA thay RSA/ECDSA cho ký (trade-off đo thật, Cat5):**

| Phép | ML-DSA-87 | RSA-15360 | ECDSA-P-521 | Headline |
|---|--:|--:|--:|---|
| keygen | **267 µs** | **15.7 s** (!) | 1294 µs | ML-DSA ≈ **78000×** nhanh hơn RSA keygen |
| sign | **563 µs** | 177 ms | 1358 µs | ML-DSA ≈ **210×** nhanh hơn RSA sign |
| verify | **239 µs** | 0.77 ms | 1103 µs | ML-DSA nhanh hơn ECDSA-P-521 ~4.6× |
| sig size | 4627 B | 1920 B | 139 B | đánh đổi: chữ ký lớn hơn |

> **Lưu ý phân phối:** ML-DSA **sign lệch phải** (rejection sampling, Fiat–Shamir-with-aborts) — p95 tới ~**1.5 ms**. Báo cáo dùng **median + p95**, không chỉ mean (xem [`data/micro/summary_x86.csv`](../data/micro/summary_x86.csv)).

### (d) Quản trị khóa (key management/rotation)

- **Vòng đời** (A2.*, A5.2) ở KMS/Vault: `gen → version → rotate → revoke → audit`.
- **SLA (G11/I6):** rotate ≤ **10 phút**; key cũ bị từ chối ≤ **24h** (overlap JWKS `kid`).
- **Khóa ML-KEM** ephemeral mỗi handshake (không lưu lâu); **khóa ký ML-DSA** rotate qua CA/JWKS.

---

## 3.2 Lớp Authentication (AuthN) — Xác thực

### (a) Danh tính server & dịch vụ: ML-DSA / mTLS

- **Server:** chứng chỉ ký **ML-DSA-65** (đã đàm phán thật, `peer sig = mldsa65`). Xác thực server hậu lượng tử (I4, I7).
- **Dịch vụ (east-west):** **mTLS** (hoặc **SPIFFE/SPIRE**) với cert do **CA ML-DSA** ký; trust store quản trị tập trung (A5.4). Chỉ dịch vụ có cert hợp lệ truy cập (G7).
- **Rotate/revoke:** cùng vòng đời §3.1(d); revoke có hiệu lực ≤ 24h (G11).

### (b) Người dùng: WebAuthn ưu tiên, TOTP dự phòng

| Cơ chế | Vai trò | Tính chất |
|---|---|---|
| **WebAuthn/FIDO2** | Chính | **Chống phishing** (origin-bound), khóa private ở authenticator (A2.6) |
| **TOTP** | Dự phòng | Seed TOTP bảo vệ; **không bypass recovery**; rate-limit + lockout |
| Account hardening | — | Khóa tài khoản đúng ngưỡng (G6: success ≥99%, false-accept=0, lockout ≤1s) |

### (c) Quản lý phiên (session)

- Cookie **`Secure + HttpOnly + SameSite`**; **chống session fixation** (rotate session ID sau đăng nhập); TTL hợp lý + đồng bộ thời gian (chrony, A5.5).

---

## 3.3 Lớp Authorization (AuthZ) — Cấp quyền

### (a) Mô hình & thi hành

- **deny-by-default**, **least-privilege**; bắt đầu **RBAC** rồi mở rộng **ABAC** (thuộc tính người dùng/thiết bị/ngữ cảnh).
- **Thi hành:** **PEP** tại Gateway/service gọi **PDP** (OPA/Rego hoặc ACL kiểu Zanzibar); **log reason** mọi quyết định ⇒ **giải thích được (I5/G12)**.
- **Mục tiêu (G8):** ma trận `role × resource × action (+ABAC)` pass-rate ≥ **95%**; hành động không khai báo **deny 100%**.

### (b) Token: pin `alg`, kiểm soát `kid`, ký ML-DSA

| Biện pháp | Mục đích | Mục tiêu |
|---|---|---|
| **Pin `alg`** (allowlist; từ chối `alg=none`) | Chống algorithm confusion | G9 (reject 100%) |
| **Allowlist `kid`** (chống `kid` injection/header confusion) | Chống chọn khóa độc hại | G9 |
| **TTL ngắn** + **refresh-rotation** + **reuse-detect** | Giảm blast-radius, chống reuse | G11 |
| **Ký token bằng ML-DSA** (qua JWKS) | Token hậu lượng tử, toàn vẹn (I2) | I2, I4 |
| **PoP: DPoP hoặc mTLS-bound (`cnf`)** | **Chống replay** (token ăn cắp vô dụng) | G10 (replay=0) |

> **Lưu ý chọn thuật toán token:** mặc định **ML-DSA** cho chữ ký token (đồng bộ chuỗi tin cậy hậu lượng tử). Nơi cần chữ ký nhỏ (cookie/header chật) có thể dùng **Ed25519** (cổ điển, không lỗi thời) như fallback có biện minh — **tuyệt đối không** dùng HMAC `none`, RSA-PKCS#1v1.5 cũ, hay `alg` không pin.

---

## 4) Tổng kết quyết định thuật toán (không pin thuật toán lỗi thời)

| Lớp | Chọn dùng (production) | Đối chứng / Loại bỏ | Biện minh chính (đo được) |
|---|---|---|---|
| Cryptography (KEX) | **X25519MLKEM768** (hybrid) | thuần RSA-KEX/ECDH | Chống HNDL; KEM rẻ CPU; overhead handshake ~9% |
| Cryptography (AEAD) | **AES-256-GCM** | RC4/CBC cũ | AES-NI 7.3/2.4 µs; Cat5 chống Grover |
| Cryptography (sig data) | **ML-DSA** | RSA-PKCS#1v1.5 lỗi thời | sign 210× / keygen 78000× nhanh hơn RSA-15360 |
| AuthN | **ML-DSA cert + mTLS + WebAuthn** | mật khẩu đơn, OQS-fork archived | Chống phishing; xác thực hậu lượng tử |
| AuthZ | **JWT pin `alg` + ML-DSA + DPoP/mTLS-bound** | `alg=none`, JWT không PoP | Chống confusion + replay; giải thích được |

> Mọi số liệu nền là **ĐO THẬT trên x86**; phần ARM/năng lượng/netem là **MÔ HÌNH** (đánh dấu rõ ở [`docs/EVAL.md`](EVAL.md)). Lý do "vì sao PQC ngay bây giờ" (HNDL, Shor) ở [`docs/THREAT_MODEL.md`](THREAT_MODEL.md).
