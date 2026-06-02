# AIM — Asset-centric Context (Bối cảnh hướng tài sản)

> **Phạm vi (NT2205 §1).** Tài liệu này xác lập **danh mục tài sản (A1–A5)**, **ngữ cảnh & ràng buộc triển khai**, và **phân tích rủi ro → mục tiêu bảo vệ định lượng (SMART)** cho hệ thống triển khai **mật mã hậu lượng tử (PQC)**: trao khóa **ML-KEM (FIPS 203)** + chữ ký **ML-DSA (FIPS 204)**, đối chứng RSA/ECC ở các mức bảo mật **NIST Category 1/3/5**, trong một dịch vụ **TLS 1.3** (nhóm hybrid `X25519MLKEM768`, chứng chỉ ký ML-DSA) trên hai nền tảng **x86_64** và **ARM (Cortex-A72)**.
>
> **Mô hình kiến trúc:** "onion" với 3 lớp lõi bên trong là **Cryptography / Authentication / Authorization** (xem [`docs/ARCH.md`](ARCH.md)). Phân tích kẻ tấn công và ánh xạ "lớp nào chống đe dọa gì" nằm ở [`docs/THREAT_MODEL.md`](THREAT_MODEL.md); giải pháp 3 lớp ở [`docs/CRYPTO_SOLUTION.md`](CRYPTO_SOLUTION.md); kiểm chứng định lượng ở [`docs/EVAL.md`](EVAL.md).
>
> *Không nhắc/động tới DFIR (theo rubric).*

---

## 1.1 Danh mục tài sản (Asset Catalog A1–A5)

Mỗi tài sản được gán: trạng thái/vị trí, độ nhạy (C/I/A = Confidentiality/Integrity/Availability), nơi cưỡng chế (lớp onion), và **invariant** liên quan ở [`docs/ARCH.md`](ARCH.md).

### A1 — Dữ liệu (Data)

| ID | Tài sản | Trạng thái | C | I | A | Bảo vệ chính | Lớp onion | Invariant |
|---|---|---|:--:|:--:|:--:|---|---|---|
| A1.1 | Payload ứng dụng (API request/response) | in-transit | Cao | Cao | TB | TLS 1.3 AEAD (AES-256-GCM) qua kênh hybrid `X25519MLKEM768` | Cryptography | I1, I2 |
| A1.2 | Dữ liệu nhạy nghỉ (at-rest: DB, file mật) | at-rest | Cao | Cao | TB | Envelope encryption (DEK bọc bởi KEK ở KMS), AEAD | Cryptography | I1, I2 |
| A1.3 | Dữ liệu đang xử lý (in-process: buffer plaintext, shared secret 32 B) | in-process | Rất cao | Cao | Thấp | Vòng đời ngắn, zeroize sau dùng, không log | Cryptography | I1 |
| A1.4 | Bản ghi đo/telemetry (CSV benchmark, log handshake) | at-rest | Thấp | Cao | TB | Tính toàn vẹn (hash/chữ ký artifact), không chứa bí mật | Cryptography | I2 |

### A2 — Bí mật & Khóa (Secrets & Keys)

| ID | Tài sản | Loại | C | I | Vòng đời / Rotation | Lớp onion | Invariant |
|---|---|---|:--:|:--:|---|---|---|
| A2.1 | **Khóa bí mật ML-KEM** (sk, 1632/2400/**3168** B cho L1/L3/L5) | KEM decaps key | Rất cao | Cao | Ephemeral mỗi handshake (không lưu lâu) | Cryptography | I1, I6 |
| A2.2 | **Khóa ký ML-DSA** (sk, 2560/4032/**4896** B cho L2/L3/L5) — ký chứng chỉ & token | Signing key | Rất cao | Cao | Rotate ≤ 10 phút; key cũ revoke ≤ 24h | AuthN, AuthZ | I6 |
| A2.3 | KEK (Key-Encryption-Key) ở KMS/Vault | Wrapping key | Rất cao | Cao | Rotate có versioning; không rời HSM/KMS | Cryptography | I6 |
| A2.4 | DEK (Data-Encryption-Key) cho A1.2 | Data key | Cao | Cao | Per-object, bọc bởi KEK | Cryptography | I1, I6 |
| A2.5 | Token-signing key (JWT/PASETO) — ưu tiên ML-DSA | Signing key | Rất cao | Cao | Rotate qua JWKS `kid`, overlap ≤ 24h | AuthZ | I4, I6 |
| A2.6 | Seed TOTP / credential WebAuthn (resident key phía authenticator) | MFA secret | Rất cao | Cao | Bind 1 user; recovery không bypass MFA | AuthN | I4 |
| A2.7 | Khóa riêng TLS server + private key mTLS dịch vụ | Identity key | Rất cao | Cao | Rotate cùng cert (A5.1) | AuthN | I4, I6 |

> **Lưu ý kích thước (đối chứng FIPS, đã KAT 9/9 PASS — [`data/kat/kat_results.txt`](../data/kat/kat_results.txt)):** ML-KEM-1024 pk = ct = **1568 B**; ML-DSA-87 pk = **2592 B**, sig = **4627 B**. So với cổ điển: ECDSA-P-521 sig = **139 B**, RSA-15360 sig = **1920 B**. Khóa/chữ ký PQC **lớn hơn 1–2 bậc** → đây là ràng buộc băng thông/bộ nhớ chính (xem §1.2 và RQ3 ở [`docs/EVAL.md`](EVAL.md)).

### A3 — Danh tính (Identities)

| ID | Tài sản | Thuộc tính liên quan | Lớp onion | Invariant |
|---|---|---|---|---|
| A3.1 | Người dùng cuối | user_id, roles, MFA-bound (WebAuthn/TOTP), device posture | AuthN → AuthZ | I3, I4, I5 |
| A3.2 | Dịch vụ (service / workload) | SPIFFE ID hoặc CN trong cert mTLS, được ký bởi ML-DSA CA | AuthN | I4, I6 |
| A3.3 | Thiết bị (device) | device_id, attestation (nếu có), TLS client identity | AuthN | I4 |

### A4 — Trạng thái & Chính sách (State & Policy)

| ID | Tài sản | Mô tả | I | Lớp onion | Invariant |
|---|---|---|:--:|---|---|
| A4.1 | Session | Cookie `Secure+HttpOnly+SameSite`, chống session fixation | Cao | AuthN | I4 |
| A4.2 | JWT claims | `sub/aud/exp/kid/cnf`; **pin `alg`**; PoP-bound (`cnf`) | Rất cao | AuthZ | I4, I5 |
| A4.3 | Chính sách RBAC→ABAC (OPA/Rego hoặc ACL) | Ma trận role × resource × action + thuộc tính ngữ cảnh | Rất cao | AuthZ | I5 |
| A4.4 | Quyết định AuthZ (decision log có lý do) | reason, policy version, input hash | Cao | AuthZ | I5 |

### A5 — Hạ tầng tin cậy (Trust Infrastructure)

| ID | Tài sản | Mô tả | Lớp onion | Invariant |
|---|---|---|---|---|
| A5.1 | CA / chuỗi tin cậy | CA ký **ML-DSA**; phát chứng chỉ server & mTLS dịch vụ | AuthN | I4, I6 |
| A5.2 | KMS / Vault | Vòng đời khóa (gen/rotate/revoke/version/audit) cho A2.* | Cryptography | I6 |
| A5.3 | JWKS endpoint | Công bố public key ký token theo `kid` | AuthZ | I4, I5 |
| A5.4 | Trust store dịch vụ | Root/intermediate cho mTLS east-west | AuthN | I4 |
| A5.5 | Đồng bộ thời gian (chrony/NTP) | Cần cho TTL token, TOTP, hiệu lực chứng chỉ | (cross-cutting) | I4, I5 |

---

## 1.2 Ngữ cảnh & Ràng buộc (Context & Constraints)

### Kiến trúc triển khai & phân vùng mạng

- **Mô hình:** hybrid on-prem/cloud. Frontend là **API Gateway / reverse-proxy (Apache httpd link OpenSSL 3.6.x native, hoặc Envoy/NGINX)**; backend là dịch vụ ứng dụng + asset store. East-west giữa dịch vụ dùng **mTLS**.
- **Phân vùng:** `public` (Internet ↔ GW) · `dmz` (GW ↔ IdP/PDP) · `private` (dịch vụ ↔ asset, KMS) · `mgmt` (vận hành, observability).
- **Trust boundary chính:**
  1. **Internet ↔ Gateway** — nơi chấm dứt TLS 1.3 hybrid (lớp Cryptography) và xác thực client.
  2. **Gateway ↔ Dịch vụ** — mTLS (lớp AuthN), token PoP-bound (lớp AuthZ).
  3. **Dịch vụ ↔ KMS/CA** — đường cấp/rotate khóa & chứng chỉ (A5).

### Hai nền tảng triển khai (≥2 deployments — yêu cầu rubric §4)

| | **D-x86 (đo THẬT)** | **D-ARM / Edge (đo MÔ HÌNH)** |
|---|---|---|
| Phần cứng | Intel **i5-12450HX**, AVX2 256-bit, ~4.4 GHz | **Cortex-A72** (BCM2711, Raspberry Pi 4), NEON 128-bit, 1.5 GHz |
| OS / Toolchain | Ubuntu 24.04, gcc 13.3, OpenSSL 3.6.1 (TLS), PQClean ref-C | Raspberry Pi OS Bookworm arm64, gcc 12, OpenSSL 3.6.x (container) |
| Vai trò | Server hiệu năng cao / cloud | Edge/IoT, ràng buộc CPU–năng lượng |
| Nguồn số liệu | `data/micro/x86/`, `data/tls/x86/`, `data/kat/` | `data/micro/arm/`, `data/tls/netem_matrix.csv` (dẫn xuất) |
| Env | [`data/env/env_linux-x86.txt`](../data/env/env_linux-x86.txt) | [`data/env/env_linux-arm.txt`](../data/env/env_linux-arm.txt) |

> **Tính liêm chính dữ liệu (đọc kỹ).** Số x86 là **ĐO THẬT**. Số ARM, NEON, năng lượng, netem là **MÔ HÌNH** dẫn xuất từ số x86 thật nhân hệ số có trích dẫn (mỗi file có dòng `# MODELED ...`). Mọi bảng trong gói tài liệu này **luôn phân biệt rõ measured (x86) vs modeled (ARM/energy/netem)**.

### Ràng buộc hiệu năng (đặc biệt trên ARM)

- **CPU-bound:** reference-C trên Cortex-A72 ≈ **4.6× chậm hơn** x86 (mô hình); **NEON** kéo lại **1.7–2.3×**. AES không có ARMv8 crypto extension trên BCM2711 → AES phần mềm ≈ **22× chậm hơn** (mô hình). Nguồn: [`data/micro/arm/`](../data/micro/arm/), [`data/micro/neon_vs_ref.csv`](../data/micro/neon_vs_ref.csv).
- **Băng thông/độ trễ mạng:** handshake hybrid ≈ **10.8 KB on-wire** (8 gói) so với cổ điển ≈ **1.5 KB** (2 gói) → nhạy với mất gói (xem ma trận netem ở [`docs/EVAL.md`](EVAL.md)).
- **Bộ nhớ/code size:** peak RSS **3.6–7.6 MB**; `.text` ML-KEM **9.5–10.7 KB**, ML-DSA **16–16.7 KB** ([`data/resource/`](../data/resource/)) — phù hợp cả lớp thiết bị nhỏ.

### Ràng buộc pháp lý/chuẩn

- **NIST FIPS 203 (ML-KEM)**, **FIPS 204 (ML-DSA)**; **IETF** `draft-ietf-tls-ecdhe-mlkem` (X25519MLKEM768), RFC 8446 (TLS 1.3).
- **Không cố định thuật toán lỗi thời.** Dùng OpenSSL native ML-KEM/ML-DSA; **bỏ** OQS-OpenSSL fork (archived). RSA chỉ giữ làm **đối chứng**, không làm primitive sản xuất.

---

## 1.3 Phân tích rủi ro & Mục tiêu bảo vệ (SMART)

### Rủi ro chính → Giảm thiểu (tóm tắt; chi tiết ở [`docs/THREAT_MODEL.md`](THREAT_MODEL.md))

| ID | Rủi ro | Tài sản bị đe dọa | Giảm thiểu | Lớp onion |
|---|---|---|---|---|
| R1 | **Nghe lén** kênh truyền | A1.1, A1.3 | TLS 1.3 + AEAD; kênh **hybrid** PQC | Cryptography |
| R2 | **Harvest-now, decrypt-later** (HNDL) | A1.1, A2.1 | KEX **hybrid X25519MLKEM768**: an toàn ngay cả khi 1 nhánh bị phá | Cryptography |
| R3 | **Shor** phá RSA/ECC trong tương lai | A2.2, A5.1 | Chữ ký **ML-DSA**, KEM **ML-KEM** (Module-LWE/SIS) | Cryptography, AuthN |
| R4 | **Grover** giảm ½ đối xứng | A1.* | Category 5 ⇒ **AES-256-GCM** | Cryptography |
| R5 | **Chỉnh sửa / tamper** ciphertext, token, dữ liệu | A1.1, A4.2 | AEAD tag; ML-DSA verify; tamper → từ chối + log | Cryptography, AuthZ |
| R6 | **Replay** token/yêu cầu | A4.1, A4.2 | **PoP** (DPoP hoặc mTLS-bound), nonce, TTL ngắn | AuthZ |
| R7 | **Giả mạo / phishing** danh tính | A3.1, A3.2 | **WebAuthn** (chống phishing), mTLS dịch vụ | AuthN |
| R8 | **Downgrade / cấu hình sai** | kênh TLS | Chỉ bật TLS 1.3 + nhóm hybrid/x25519; pin `alg` | Cryptography |
| R9 | **Leo quyền** (privilege escalation) | A4.3 | **deny-by-default**, least-privilege, RBAC→ABAC | AuthZ |
| R10 | **Lộ/thu hồi khóa chậm** | A2.* | KMS rotate ≤ 10 phút, revoke ≤ 24h | Cryptography (cross) |
| R11 | **Token confusion** (`alg=none`, `kid` injection) | A4.2 | Pin `alg`, allowlist `kid`, JWKS có kiểm soát | AuthZ |

### Mục tiêu bảo vệ định lượng (SMART)

> S = cụ thể, M = đo được, A = khả thi, R = liên quan tài sản/đe dọa, T = có mốc thời gian. Mỗi mục tiêu trỏ tới bài đo ở [`docs/EVAL.md`](EVAL.md).

| ID | Mục tiêu SMART | Metric & Ngưỡng | Tài sản | Rủi ro | Bài đo |
|---|---|---|---|---|---|
| G1 | **0 byte plaintext rò rỉ** trên kênh bảo vệ (đối chứng kênh không bảo vệ) | plaintext leakage = **0 B** | A1.1, A1.3 | R1, R5 | E-C1 |
| G2 | **Không trùng nonce** AEAD dưới tải; lỗi giả lập → từ chối + log | nonce reuse = **0**; AEAD error (bình thường) = **0** | A1.1 | R5 | E-C2 |
| G3 | **Toàn vẹn tuyệt đối**: sửa ciphertext/tag bị chặn; chữ ký sai verify fail | tamper-reject = **100%**; KAT = **9/9 PASS** | A1, A2.2, A4.2 | R5 | E-C3 |
| G4 | **Downgrade bị từ chối 100%**: chỉ TLS 1.3 + nhóm hybrid/x25519 đàm phán thành công | downgrade rejected = **100%**; nhóm = `X25519MLKEM768` | kênh TLS | R8 | E-N1 |
| G5 | **Hiệu lực hậu lượng tử của handshake**: nhóm hybrid PQC được đàm phán + cert ML-DSA xác thực | negotiated group = `X25519MLKEM768`; peer sig = `mldsa65` | A5.1 | R2, R3 | E-N1 |
| G6 | **MFA mạnh**: đăng nhập thành công cao, không chấp nhận sai | success ≥ **99%**; **false-accept = 0**; lockout ≤ 1 s | A3.1, A2.6 | R7 | E-N1 |
| G7 | **Danh tính dịch vụ ràng buộc**: chỉ dịch vụ hợp lệ truy cập; revoke/rotate có hiệu lực | unauthorized access = **0**; revoke hiệu lực ≤ 24h | A3.2, A5.1 | R7, R10 | E-N2 |
| G8 | **Cấp quyền chặt**: ma trận policy đạt pass-rate cao; hành động không khai báo bị từ chối | policy pass-rate ≥ **95%**; undeclared action deny = **100%** | A4.3 | R9 | E-Z1 |
| G9 | **Token cứng**: từ chối `alg=none`, `kid` injection, header confusion | reject = **100%**; log reason đầy đủ | A4.2, A2.5 | R11 | E-Z2 |
| G10 | **Chống replay (PoP)**: phát lại yêu cầu/token bị chặn | replay accepted = **0** | A4.2 | R6 | E-Z2 |
| G11 | **Xoay khóa nhanh, blast-radius giới hạn** | rotation ≤ **10 phút**; key cũ từ chối ≤ **24h** | A2.*, A5.2 | R10 | E-N2 (cross) |
| G12 | **Giải thích được**: tái dựng quyết định AuthZ từ log/policy | explainability = **100%** ca kiểm thử | A4.4 | R9 | E-Z1 |
| G13 | **Khả thi hiệu năng PQC**: overhead handshake hybrid chấp nhận được trên server | overhead throughput ≤ **~10%** vs cổ điển (đo thật: **~9%**) | A1.1 | R2 | RQ3 |

> **Tham chiếu chéo:** mô hình kẻ tấn công đầy đủ, cây tấn công và "mỗi lớp onion chống đe dọa gì" được phát triển trong [`docs/THREAT_MODEL.md`](THREAT_MODEL.md). Invariants I1–I7 và cách kiểm chứng nằm ở [`docs/ARCH.md`](ARCH.md). Số liệu hiệu năng nền cho G13 (ví dụ ML-KEM-1024 KEM ≈ **22×** nhanh hơn ECDH-P-521; ML-DSA-87 sign ≈ **210×** nhanh hơn RSA-15360 sign) được trình bày ở §"Performance evaluation" của [`docs/EVAL.md`](EVAL.md).
