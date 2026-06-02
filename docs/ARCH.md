# ARCH — System Architecture (Kiến trúc hệ thống)

> **Phạm vi (NT2205 §2).** Tài liệu này trình bày **sơ đồ kiến trúc** (boundary, thành phần, luồng dữ liệu, nơi cưỡng chế chính sách, nơi thi hành mật mã), **thành phần lõi & quyết định kỹ thuật**, và **bộ invariants I1–I7** kèm cách kiểm chứng. Tài sản tham chiếu [`docs/AIM.md`](AIM.md); giải pháp 3 lớp [`docs/CRYPTO_SOLUTION.md`](CRYPTO_SOLUTION.md); kiểm chứng định lượng [`docs/EVAL.md`](EVAL.md).
>
> **Mô hình "onion":** mọi yêu cầu đi qua các vành ngoài (mạng → Firewall/IDS) rồi tới **3 lớp lõi theo thứ tự AuthN → AuthZ → Cryptography → Asset**. PQC được ánh xạ: **ML-KEM → lớp Cryptography** (bảo mật kênh, hybrid); **ML-DSA → lớp AuthN** (chứng chỉ server/dịch vụ) **+ lớp AuthZ** (token được ký).

---

## 2.1 Sơ đồ kiến trúc (ASCII)

```
                                INTERNET (untrusted)
                                       │
   ┌───────────────────────────────────┼──────────────────────────────────────────┐
   │  CLIENT                            │                                           │
   │  ┌───────────────┐                 │   A1.1 payload (in-transit)               │
   │  │ Browser / App │  TLS 1.3 ClientHello                                        │
   │  │  - WebAuthn   │  groups = X25519MLKEM768  ───────────────►                  │
   │  │  - DPoP key   │  (hybrid PQC KEX)                                           │
   │  └───────────────┘                                                            │
   └───────────────────────────────────┬──────────────────────────────────────────┘
                                       │  ╔═══════════ TRUST BOUNDARY #1 (Internet ↔ Edge) ═══════════╗
            ┌──────────────────────────▼───────────────────────────┐
            │  VÀNH NGOÀI (perimeter) — zone: public/dmz            │
            │   ┌──────────────┐      ┌──────────────────────┐      │
            │   │  Firewall /  │ ───► │   IDS / IPS / WAF     │      │
            │   │  rate-limit  │      │  (giám sát, không     │      │
            │   └──────────────┘      │   giải mã nội dung)   │      │
            │                         └──────────────────────┘      │
            └──────────────────────────┬───────────────────────────┘
                                       │
            ╔══════════════════════════▼═══════════════════════════════════════════╗
            ║   API GATEWAY / Reverse-proxy (Apache httpd + OpenSSL 3.6.x native    ║
            ║                              hoặc Envoy/NGINX) = PEP                  ║
            ║                                                                      ║
            ║   ▼ LỚP 1 ── AUTHENTICATION (AuthN) ─────────────────────────────┐  ║
            ║   │  • Chấm dứt TLS 1.3; xác thực SERVER bằng cert ký ML-DSA-65   │  ║
            ║   │  • mTLS / SPIFFE cho danh tính DỊCH VỤ (cert do CA ML-DSA ký) │  ║
            ║   │  • Người dùng: WebAuthn (chống phishing) / TOTP qua IdP       │  ║
            ║   │  • Session cookie Secure+HttpOnly+SameSite                    │  ║
            ║   └───────────────────────────────┬──────────────────────────────┘  ║
            ║                                    ▼                                 ║
            ║   ▼ LỚP 2 ── AUTHORIZATION (AuthZ) ──────────────────────────────┐  ║
            ║   │  • PEP@GW gọi PDP (OPA/Rego hoặc ACL); deny-by-default        │  ║
            ║   │  • JWT pin `alg`, allowlist `kid`, TTL ngắn, ký ML-DSA        │  ║
            ║   │  • PoP: DPoP hoặc mTLS-bound (cnf) chống replay               │  ║
            ║   │  • Decision log có reason (giải thích được)                   │  ║
            ║   └───────────────────────────────┬──────────────────────────────┘  ║
            ║                                    ▼                                 ║
            ║   ▼ LỚP 3 ── CRYPTOGRAPHY (lõi) ─────────────────────────────────┐  ║
            ║   │  • KEX hybrid X25519MLKEM768 (ML-KEM-768 ⊕ X25519)           │  ║
            ║   │  • AEAD record: AES-256-GCM (TLS_AES_256_GCM_SHA384)         │  ║
            ║   │  • Envelope encryption at-rest: DEK (AEAD) ⟵ KEK (KMS)       │  ║
            ║   │  • Quản trị nonce GCM; zeroize shared secret 32 B            │  ║
            ║   └───────────────────────────────┬──────────────────────────────┘  ║
            ╚════════════════════════════════════┼═════════════════════════════════╝
                                       │  ╔═══ TRUST BOUNDARY #2 (GW ↔ dịch vụ: mTLS east-west) ═══╗
            ┌──────────────────────────▼───────────────────────────┐
            │  BACKEND — zone: private                              │
            │   ┌──────────────┐   ┌──────────────┐                 │
            │   │ App Service  │──►│   ASSET      │  A1.2 at-rest   │
            │   │ (PEP cục bộ) │   │  store / DB  │  (envelope enc) │
            │   └──────┬───────┘   └──────────────┘                 │
            └──────────┼──────────────────────────────────────────-─┘
                       │  ╔═══ TRUST BOUNDARY #3 (dịch vụ ↔ hạ tầng tin cậy) ═══╗
   ┌───────────────────▼──────────────────────────────────────────────────────────┐
   │  HẠ TẦNG TIN CẬY (control plane) — zone: private/mgmt                         │
   │  ┌──────────┐  ┌────────────┐  ┌────────────┐  ┌──────────┐  ┌─────────────┐  │
   │  │ IdP/IAM  │  │ PDP (OPA / │  │ KMS/Vault  │  │ CA (ký   │  │ JWKS + chrony│ │
   │  │ OIDC,    │  │  Rego)     │  │ KEK,rotate │  │ ML-DSA)  │  │ time-sync    │ │
   │  │ WebAuthn │  │            │  │ /revoke    │  │          │  │              │ │
   │  └──────────┘  └────────────┘  └────────────┘  └──────────┘  └─────────────┘  │
   └──────────────────────────────────────────────────────────────────────────────┘

   OBSERVABILITY (cross-cutting): structured logs · metrics · traces · time-sync (chrony)
```

**Luồng dữ liệu (happy path):** Client mở **TLS 1.3 hybrid** → vành ngoài (firewall/IDS) → Gateway **chấm dứt TLS** & xác thực (AuthN) → kiểm tra quyền (AuthZ, deny-by-default) → áp mật mã dữ liệu (Cryptography) → mTLS tới dịch vụ → đọc/ghi asset (envelope encryption). Mọi quyết định AuthZ ghi **decision log có reason**.

**Hai nền tảng triển khai (rubric §4):** cùng kiến trúc trên **D-x86** (server, đo THẬT) và **D-ARM/Edge** (Cortex-A72, đo MÔ HÌNH). Khác biệt nằm ở chi phí lớp Cryptography (xem [`docs/EVAL.md`](EVAL.md) RQ1/RQ2), không ở kiến trúc logic.

---

## 2.2 Thành phần lõi & Quyết định kỹ thuật

| Thành phần | Vai trò | Quyết định kỹ thuật | PQC enforced tại đây? |
|---|---|---|---|
| **API Gateway / reverse-proxy** (Apache httpd + OpenSSL 3.6.x, hoặc Envoy/NGINX) | PEP: chấm dứt TLS, rate-limit, verify token, PoP | `SSLProtocol -all +TLSv1.3`; chỉ bật nhóm `X25519MLKEM768` (+ x25519); envelope tại rìa | **Có** — KEX hybrid (Crypto) + verify cert ML-DSA (AuthN) |
| **IdP / IAM** | Xác thực người dùng | OIDC/OAuth 2.1; **WebAuthn/FIDO2** ưu tiên; TOTP dự phòng (không bypass recovery) | Gián tiếp (token ký ML-DSA) |
| **PDP** (OPA/Rego hoặc ACL) | Ra quyết định cấp quyền | deny-by-default; RBAC→ABAC; log reason | Không (chính sách); token mang chữ ký ML-DSA |
| **KMS / Vault** | Vòng đời khóa | gen/rotate/revoke/versioning/audit cho KEK/DEK & khóa ký | **Có** — quản trị khóa ML-DSA & KEK |
| **CA** | Phát chứng chỉ | CA ký **ML-DSA**; cert server + mTLS dịch vụ | **Có** — chuỗi tin cậy hậu lượng tử (AuthN) |
| **JWKS** | Công bố khóa công khai ký token | `kid` allowlist; rotate overlap ≤ 24h | **Có** — public key ML-DSA |
| **Observability** | Quan sát được | structured logs, metrics, traces; chrony đồng bộ thời gian | Hỗ trợ I5, I6 |

**Nguyên tắc crypto (rationale tóm tắt — chi tiết [`docs/CRYPTO_SOLUTION.md`](CRYPTO_SOLUTION.md)):**
- **AEAD** cho cả truyền (TLS 1.3) và lưu (envelope) — **AES-256-GCM** với **quản trị nonce chuẩn** (đo thật AES-NI enc/dec ≈ **7.3/2.4 µs**).
- **KEX hybrid `X25519MLKEM768`** chống **harvest-now-decrypt-later**: an toàn nếu *một trong hai* nhánh còn an toàn.
- **Chữ ký ML-DSA** (FIPS 204) thay RSA/ECDSA cho cert & token — biện minh bằng trade-off đo được: ML-DSA-87 sign ≈ **210×** nhanh hơn RSA-15360 sign, keygen ≈ **78000×** nhanh hơn; đánh đổi là kích thước (sig 4627 B vs 139 B ECDSA-P-521). **Không pin thuật toán lỗi thời**; RSA chỉ làm đối chứng.

---

## 2.3 Invariants (I1–I7) — khẳng định cần được kiểm chứng

> Mỗi invariant liên kết tài sản ([`docs/AIM.md`](AIM.md)), lớp onion, **cách kiểm chứng** và bài đo tương ứng ở [`docs/EVAL.md`](EVAL.md). "Đạt/chưa" được tổng kết trong cột Kết quả.

| ID | Phát biểu invariant | Lớp onion | Cách kiểm chứng (công cụ + evidence) | Bài đo | Trạng thái |
|---|---|---|---|---|---|
| **I1** | **Không rò rỉ plaintext** trên kênh bảo vệ (in-transit & in-process). | Cryptography | So sánh capture kênh bảo vệ (TLS) vs kênh không bảo vệ; grep plaintext sentinel = 0 byte; KAT roundtrip. Evidence: [`data/kat/kat_results.txt`](../data/kat/kat_results.txt), [`data/tls/x86/handshake_x86.txt`](../data/tls/x86/handshake_x86.txt) | E-C1 | ✅ Đạt |
| **I2** | **Tampering bị từ chối** (ciphertext/tag/chữ ký/token) **+ có log giải thích**. | Cryptography, AuthZ | Sửa 1 bit ciphertext/tag → AEAD `decrypt` fail; sửa chữ ký → ML-DSA `verify` fail; cổng tamper-reject trong benchmark. Evidence: KAT 9/9 PASS (gồm "tampered tag rejected", "verify(tampered)=FAIL") | E-C3 | ✅ Đạt (9/9) |
| **I3** | **Toàn vẹn nguồn gốc dữ liệu** (data-origin authentication): dữ liệu nhận đúng như bên gửi tạo, không bị chỉnh sửa. | Cryptography (AuthN-data) | Chữ ký ML-DSA / AEAD-tag trên payload & artifact; verify pass = nguyên gốc. Evidence: KAT verify(real)=PASS; kích thước sig khớp FIPS | E-C3 | ✅ Đạt |
| **I4** | **AuthN chống phishing**; **token có ràng buộc sở hữu (PoP)** chống replay. | AuthN, AuthZ | WebAuthn (origin-bound) cho người dùng; mTLS cho dịch vụ; DPoP/mTLS-bound (`cnf`) cho token; thử replay = 0 chấp nhận. Evidence: cấu hình IdP + log PoP | E-N1, E-Z2 | ◻ Theo kế hoạch (design + gate) |
| **I5** | **Quyết định AuthZ giải thích được** từ log/chính sách (tái dựng 100%). | AuthZ | deny-by-default; PDP trả `reason`; tái dựng quyết định từ (policy version + input hash + decision log). Evidence: decision log + Rego/ACL | E-Z1 | ◻ Theo kế hoạch |
| **I6** | **Vận hành khóa quan sát được**: rotate nhanh; blast-radius giới hạn. | Cryptography (cross) | KMS rotate ≤ 10 phút; key cũ bị từ chối ≤ 24h; versioning + audit log. Evidence: timeline rotation + JWKS `kid` overlap | E-N2 (cross) | ◻ Theo kế hoạch |
| **I7** | **PQC-readiness & khả thi hiệu năng**: kênh đàm phán đúng nhóm hậu lượng tử (`X25519MLKEM768`) với cert ML-DSA, **và overhead nằm trong ngân sách** (throughput hybrid ≥ ~90% cổ điển). | Cryptography, AuthN | Bắt handshake: `Negotiated group = X25519MLKEM768`, `peer sig = mldsa65`; đo throughput hybrid vs cổ điản. Evidence: [`data/tls/x86/handshake_x86.txt`](../data/tls/x86/handshake_x86.txt) (1934 vs 2118 hs/s ≈ 9% overhead) | E-N1, RQ3 | ✅ Đạt (đo thật) |

> **Ghi chú liêm chính.** Các invariant gắn lớp Cryptography (I1, I2, I3, I7) được kiểm chứng bằng **số đo thật trên x86** (KAT, AES tamper-reject, ML-DSA tamper-reject, handshake localhost OpenSSL 3.6.1). Các invariant thuộc lớp AuthN/AuthZ ở mức **vận hành (operational)** kèm **cổng kiểm thử (gate) trong [`docs/EVAL.md`](EVAL.md)**; phần ARM/netem là **MÔ HÌNH** và được đánh dấu rõ.

### Ánh xạ PQC → lớp onion (tóm tắt)

| Primitive PQC | Lớp onion | Vai trò trong invariant |
|---|---|---|
| **ML-KEM** (FIPS 203, qua `X25519MLKEM768`) | **Cryptography** | I1 (bảo mật kênh), I7 (PQC-readiness), chống HNDL |
| **ML-DSA** (FIPS 204) — chứng chỉ | **AuthN** | I3, I4 (xác thực server/dịch vụ), I7 |
| **ML-DSA** (FIPS 204) — token được ký | **AuthZ** | I2, I4, I5 (token toàn vẹn, PoP, giải thích được) |
| **AES-256-GCM** (AEAD) | **Cryptography** | I1, I2 (bảo mật + toàn vẹn record/at-rest) |
