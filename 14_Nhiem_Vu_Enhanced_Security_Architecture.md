# 14 — NHIỆM VỤ (mở rộng) + Kiến trúc bảo mật thực tế: ai ký · ai giữ khóa · các node

> **Nguyên tắc biên tập (theo yêu cầu):** *chỉ THÊM và SỬA chỗ sai, KHÔNG xóa nội dung gốc.*
> - Nội dung Nhiệm vụ gốc của nhóm giữ nguyên ở **Phần A** (đánh dấu 🔧 nơi cần sửa, lý do kèm theo).
> - **Phần B → G** là phần **bổ sung** (ai ký / ai giữ khóa / từng node / cách các bên lớn làm thật / bổ sung bảo mật / nguồn).
> - Cơ sở: `05_Implement & Benchmark Lattice-based Schemes (Kyber, Dilithium).md`, `08_Work_Plan_from_NT2205_Checklist.md`, `09_Architecture_Nodes_Deployment.md`, và thực tiễn triển khai của Google / Cloudflare / AWS / Apple / Signal (Phần G).

---

# PHẦN A — NHIỆM VỤ GỐC (giữ nguyên + sửa chỗ sai)

## WP1. Dựng môi trường build (x86 + ARM)
- Reproducible build scripts (Bash/Make/CMake). `Dockerfile` cho x86_64 + `docker buildx` multiarch (hoặc cross-compile `aarch64-linux-gnu`). Ghi gcc/clang version, flags (`-O2/-O3 -march=native`), CPU governor.
- **Thư viện & tooling:** liboqs (Open Quantum Safe), PQClean, reference implementations.
  - 🔧 **SỬA:** ~~"OpenSSL OQS fork cho tích hợp TLS"~~ → **OpenSSL ≥ 3.5 native** (đã có ML-KEM/ML-DSA sẵn). *OQS-OpenSSL fork đã DEPRECATED*; nếu cần provider thì dùng **oqs-provider** cắm vào OpenSSL 3.x, KHÔNG dùng fork cũ. (Nguồn: OQS thông báo ngừng fork; OpenSSL 3.5 hỗ trợ native — xem Phần G.)
- **Platforms:** x86_64 Linux (server-class) + ARM SBC (Raspberry Pi 4 aarch64). *(Pi Zero/3, ODROID, Jetson = optional.)*
- **Đo tài nguyên:** Monsoon power meter / INA219 (năng lượng), `time`, `perf stat`, `wrk`/`wrk2`/`ab`, `openssl speed`, `getrusage`, `htop`.
  - 🔧 **SỬA:** ~~`wr k`~~ → `wrk` (lỗi gõ).
- pqm4 (nếu test MCU), Docker/buildx, INA sensors, python/pandas/matplotlib.

## WP2. Harness microbenchmark
- **Thuật toán:** ML-KEM (512/768/1024), ML-DSA (44/65/87), RSA (2048/3072 — và **15360** cho Cat 5), ECDSA/ECDH (P-256/P-384/**P-521**), Ed25519 (baseline chữ ký).
  - 🔧 **SỬA tên (bắt buộc trong báo cáo):** "Kyber" → **ML-KEM (FIPS 203)**, "Dilithium" → **ML-DSA (FIPS 204)**. Tên cũ chỉ để trong ngoặc khi nhắc paper gốc.
  - 🔧 **THÊM:** map mức bảo mật rõ — L1↔RSA-3072↔P-256, L3↔RSA-7680↔P-384, **L5↔RSA-15360↔P-521** (repo đang đo Cat 5).
- **Nền tảng lý thuyết:** Module-LWE/Ring-LWE (KEM + signature). ML-KEM = KEM dựa Module-LWE; ML-DSA = chữ ký Module-LWE + Module-SIS (Fiat–Shamir with aborts).
- **Workloads:** keygen, encaps/decaps, sign, verify; TLS 1.3 handshake; code size; peak memory; energy/op.
- **Đo:** N iter (1000–10000)/op; median + mean + std + **95% CI**; **K=5–10 batch × M iter → median-of-medians**; **warm-up loại khỏi số đo**; **paired comparison** cùng phần cứng; CI bootstrap.
  - 🔧 **SỬA:** ~~"đo CPU cycles bằng rdtsc/cntvct_el0"~~ → trên **ARM `cntvct_el0` KHÔNG phải core cycles** (là virtual timer tần số cố định); **x86 `rdtsc`** là invariant-TSC ≠ core cycles khi bật turbo. → Báo cáo **thời gian (ns)** là chính; muốn cycle thật phải đọc **PMU** (`perf`/PMCCNTR_EL0).
- **Memory/size:** `size`, `readelf -S`, peak RSS qua `/usr/bin/time -v`/pmap.
- **Repeatability:** pin tần số (tắt turbo), isolate core (`taskset`), nhiều batch, warm-up; cố định OS/kernel hoặc container hóa.
- **Đúng đắn:** 🔧 **THÊM** chạy **KAT/test vector** xác minh encaps/decaps khớp + sign/verify pass TRƯỚC khi tin số.
- **Trình bày:** biểu đồ latency vs param, throughput vs concurrency, bytes vs algorithm, energy vs algorithm + bảng trade-off.

## WP3. Tối ưu NEON & so sánh (lõi RQ1/RQ2)
- Build **reference (portable C)** vs **optimized (NEON/asimd)**; PQClean/liboqs chọn implementation.
- So **x86 (AVX2)** vs **ARM (NEON)**; ablation flags `-O2/-O3/-mcpu=cortex-a72`.
- (mở rộng) ASIC/FPGA, SVE.
- 🔧 **THÊM:** đo speedup NEON bằng **cycle qua PMU** (ARM không có `rdtsc`); ghi rõ bản nào reference / bản nào optimized.

## WP4. TLS qua mạng + (kiến trúc onion)
- **Bố trí:** Pi = server ↔ PC = client, **QUA MẠNG THẬT** (🔧 **không localhost** — đúng yêu cầu).
- Đo **5 metric** mỗi cấu hình (cổ điển / PQC / hybrid):
  1. **Handshake time** — median + mean + std + **p95/p99**.
  2. **Byte trên dây** — tách KEM (pubkey+ciphertext) và cert/chữ ký (chỗ PQC phình to).
  3. Số gói + RTT + **phân mảnh** (vượt MTU 1500? vượt initcwnd 10?).
  4. **Throughput** handshake/s dưới tải (concurrency 1–16), **server single + multi-thread**.
  5. 🔧 **THÊM (đã có ⭐):** quét **netem** trễ {0,20,60,100ms} × mất gói {0,1,3,5,10%}.
- Hybrid `X25519MLKEM768` → trả lời **RQ3**.
  - 🔧 **SỬA:** ~~"OpenSSL-OQS TLS handshake (ECDHE + Kyber)"~~ → **OpenSSL ≥3.5 `-groups X25519MLKEM768`**; cert ký **ML-DSA** (`genpkey -algorithm ML-DSA-65/87`). Công cụ: `s_server`/`s_client`, `s_time`, `tcpdump`/Wireshark, `tc netem`, `wrk`.

## WP5. Kiến trúc giải pháp (onion 3 lớp)
- **Đo trên server thật, bảo mật cao nhất như các trang web lớn** (🔧 **KHÔNG localhost**).
- Sơ đồ: `client → mạng → [Firewall → IDS/IPS] → AuthN → AuthZ → Crypto → Asset`.
- Ánh xạ PQC vào 3 lớp:
  - **Cryptography** → ML-KEM (+ hybrid) mã hóa kênh.
  - **Authentication** → chứng chỉ ML-DSA (mTLS).
  - **Authorization** → token/quyền **ký bằng ML-DSA** (RBAC) — xác nhận mức hiện thực với GV.
- ⭐ Dựng HTTPS server thật (nginx PQC từ **oqs-demos**), đo end-to-end; phân tích lớp bằng Wireshark.
- 👉 **Chi tiết "ai ký / ai giữ khóa / từng node": xem Phần B–E (bổ sung).**

## WP6. Tài nguyên, năng lượng & đối chiếu
- Kích thước khóa/ciphertext/chữ ký (bytes vs algorithm); peak RSS (`/usr/bin/time -v`); code size (`size`, `readelf -S`).
- Năng lượng: Pi thuê từ xa → ước lượng **CPU power model** (hoặc Pi local nếu mượn được); nếu không → ghi **Limitations**.
- **Validation:** đối chiếu số với NIST/literature (MDPI, bài Pi 4) để xác nhận hợp lý.

## WP7. Thống kê & phân tích
- pandas đọc CSV; **CI bootstrap**; phân tích **phân phối ML-DSA** (rejection sampling → lệch phải) bằng median + percentile; phân biệt timing-variability (thuật toán) vs timing-leak (rò khóa).
- Biểu đồ: latency vs param, bytes vs algo, throughput vs concurrency, latency vs % mất gói, energy vs algo.
- **Trả lời RQ1/RQ2/RQ3 bằng số của nhóm** + khuyến nghị; tách bạch **an toàn = lập luận**, **hiệu năng = đo**.

> 🔧 **THÊM lưu ý chung (từ §17 & §8.3 file 05):** giữ cấu trúc repo `build/ benchmarks/{micro,tls,energy}/ scripts/ docker/ docs/ tools/`; benchmark flow dùng `clock_gettime(CLOCK_MONOTONIC_RAW)`. **Bảo mật khi setup như Google + trang uy tín quốc tế, không triển khai trên localhost** → cụ thể hóa ở Phần E.

---

# PHẦN B — CÁC BÊN LIÊN QUAN (Stakeholders / Actors)

> Mỗi "bên" là một **chủ thể có định danh + khóa**. Đây là phần đồ án còn thiếu: nói rõ **ai là ai, giữ khóa gì, ký gì, verify gì**.

| # | Bên liên quan (Actor) | Giữ khóa gì (private) | KÝ gì | VERIFY gì |
|---|---|---|---|---|
| **R** | **Root CA** (offline, trong **HSM**) | Root private key (ML-DSA / hybrid) | Chỉ ký **cert của Intermediate CA** | — (tự ký chính nó) |
| **I** | **Intermediate CA** (online, trong HSM) | Intermediate private key | Ký **cert leaf** cho mọi node (server, client, service) | Chuỗi tới Root |
| **S** | **Server** (API Gateway, Backend) | Server TLS private key (ML-KEM decaps + ML-DSA sign) | Ký **TLS handshake** (CertificateVerify), ký response nếu cần | Client cert (mTLS), token |
| **C** | **Client / Edge device** (ARM Pi) | Device private key (ML-DSA) | Ký **client TLS handshake** (mTLS), ký telemetry | Server cert |
| **IdP** | **Identity Provider / IAM** (Keycloak…) | **Token-signing key** (ML-DSA / Ed25519) | Ký **JWT/OIDC token** (access/ID token) | Credential người dùng (WebAuthn/TOTP) |
| **U** | **Người dùng cuối** | WebAuthn/FIDO2 passkey (giữ trong thiết bị/TPM) | Ký **challenge đăng nhập** (WebAuthn assertion) | — |
| **PDP** | **Policy Decision Point** (OPA) | mTLS service key | — (ra **quyết định** allow/deny, không ký token) | Token + thuộc tính |
| **KMS** | **KMS / Vault** | **KEK** (key-encryption-key), bao bọc DEK | (HSM ký nếu là CA-backed) | Định danh client xin khóa |
| **CI** | **Pipeline build/CI** (ký phần mềm) | Khóa tạm (keyless qua Fulcio) | Ký **artifact/SBOM** (Sigstore cosign) | OIDC identity của job |
| **TL** | **Transparency Log** (CT / Rekor) | Log signing key | Ký **Signed Tree Head** (bằng chứng minh bạch) | Mọi cert/chữ ký được ghi nhận |

**Đọc ngang một dòng = trả lời "ai ký – ai giữ khóa".** Ví dụ: *IdP giữ token-signing key, ký JWT, người dùng đăng nhập bằng WebAuthn; Gateway verify JWT đó.*

---

# PHẦN C — AI KÝ · AI GIỮ KHÓA (chuỗi tin cậy chi tiết)

## C.1. PKI — chuỗi chứng chỉ (chain of trust)
Thực tế (DigiCert/Sectigo/Google) dùng **3 cấp**:

```
Root CA  (self-signed, private key trong HSM, OFFLINE)
   │  ký
   ▼
Intermediate CA  (online, HSM)  ◄── chịu trách nhiệm ký hằng ngày
   │  ký
   ▼
Leaf cert  (server / client / service)  ◄── xuất hiện trong TLS handshake
```

- **Root CA KHÔNG bao giờ ký trực tiếp leaf** → nếu Intermediate lộ chỉ revoke Intermediate, Root vẫn an toàn. *(Nguồn: Sectigo, Keyfactor, DigiCert — Phần G.)*
- Trong đồ án: **N6 (CA)** đóng cả Root+Intermediate (đơn giản hóa, ghi rõ giả định); cấp **leaf ML-DSA** (đối chứng RSA-15360/P-521) cho N1–N7.
- Verify: client đi ngược chuỗi leaf → Intermediate → Root (nằm trong **trust store**).

## C.2. mTLS — định danh dịch vụ (ai ký SVID)
Mô hình **SPIFFE/SPIRE** (chuẩn cloud-native):
- **SPIRE Server** = CA của một *trust domain*; nó **tự ký** rồi **ký SVID** (cert X.509) cho **mọi workload**.
- Mỗi service có một **SPIFFE ID** (vd `spiffe://domain/ns/app`) → nhúng trong SVID.
- Khi 2 service nói chuyện mTLS: cả hai **xuất SVID, verify chữ ký + hạn + SPIFFE ID** khớp policy.
- **Khóa ngắn hạn**, tự động **rotate** trước khi hết hạn. *(Nguồn: spiffe.io, Red Hat — Phần G.)*
- 👉 Map vào đồ án: thay vì hostname, định danh **theo identity** (giống Google ALTS) — đưa vào lớp **Authentication**.

## C.3. Token (ai ký JWT)
- **IdP** giữ **token-signing private key**; ký **JWT** (header `alg`, `kid`).
- **Gateway/Backend** verify chữ ký bằng **public key** lấy từ **JWKS** endpoint của IdP.
- 🔧 Bảo mật bắt buộc: **pin `alg`** (từ chối `alg=none`), kiểm `kid` (chống injection), TTL ngắn, refresh-rotation + reuse-detect, **PoP = DPoP / mTLS-bound** chống replay.
- Đồ án: ký JWT bằng **ML-DSA** (so Ed25519/RSA-PSS) → đo sign/verify rate + kích thước token.

## C.4. Quản trị khóa (ai giữ khóa — KMS/HSM)
- **Envelope encryption:** dữ liệu mã bằng **DEK**; DEK được bọc bởi **KEK**; **KEK nằm trong KMS/HSM, không bao giờ rời HSM**.
- Vòng đời: generate → **rotate** (≤10 phút trong đồ án) → revoke → versioning → audit.
- **Root/Intermediate CA private key + token-signing key** lý tưởng đặt trong **HSM**. *(Nguồn: AWS Private CA + KMS, Keyfactor — Phần G.)*

## C.5. Ký phần mềm / chuỗi cung ứng (ai ký artifact) — *bổ sung bảo mật*
Mô hình **Sigstore** (keyless):
- **CI job** xác thực qua **OIDC** (GitHub/Google/IdP) → nhận token ngắn hạn.
- **Fulcio** (CA) cấp **cert ngắn hạn** gắn identity ↔ khóa tạm.
- **cosign** ký **artifact/SBOM/SLSA provenance**; ghi sự kiện vào **Rekor** (transparency log).
- Verifier tin **"sự kiện ký đã được ghi log"** thay vì tin khóa dài hạn. *(Nguồn: sigstore — Phần G.)*
- 👉 Thêm vào đồ án như một **lớp bảo mật chuỗi cung ứng** cho chính các Docker image/binary benchmark.

---

# PHẦN D — TỪNG NODE (bổ sung cho `09_Architecture_Nodes_Deployment.md`)

> File 09 đã liệt kê 7 node (N1 Edge … N7 KMS). Phần này **bổ sung cột "ai ký / ai giữ khóa"** cho rõ.

| Node | Giữ khóa private gì | Node này KÝ gì | Node này VERIFY gì |
|---|---|---|---|
| **N1 Edge (ARM Pi)** | Device key ML-DSA + session key | client handshake (mTLS), telemetry | server cert (chuỗi tới Root) |
| **N2 Gateway (x86)** | Server TLS key (ML-KEM decaps + ML-DSA) | server handshake (CertificateVerify) | client cert + JWT (PoP) |
| **N3 IdP** | **Token-signing key (ML-DSA)** | **JWT/OIDC token** | credential người dùng (WebAuthn) |
| **N4 PDP (OPA)** | mTLS service key | — (chỉ ra quyết định allow/deny) | token + thuộc tính ABAC |
| **N5 Backend ●Asset** | DEK (dữ liệu) + mTLS key | (tùy) ký dữ liệu nghiệp vụ | mTLS từ Gateway, scope |
| **N6 CA/PKI** | **Root + Intermediate key (HSM)** | **cert leaf ML-DSA cho mọi node** | yêu cầu cấp cert (CSR) |
| **N7 KMS/Vault** | **KEK** (bọc DEK) + khóa ký | (HSM) | identity client xin khóa |

**Luồng "ai ký ai" trong 1 phiên:**
1. N6 (CA) **ký cert** cho N1, N2 (mTLS hai chiều).
2. U (người dùng) đăng nhập → N3 (IdP) **ký JWT**.
3. N1 mở **TLS hybrid X25519MLKEM768** tới N2; N2 dùng cert do N6 ký để chứng minh danh tính.
4. N2 **verify** JWT (public key từ N3) + client cert → hỏi N4 (PDP) **quyết định** quyền.
5. N5 lấy **DEK** (bọc bởi KEK ở N7) để giải mã ●Asset.

---

# PHẦN E — CÁCH CÁC BÊN LỚN LÀM THẬT (để biện minh thiết kế)

> Dùng phần này trong báo cáo để chứng minh thiết kế của nhóm **bám thực tiễn quốc tế**, không tự nghĩ.

| Bên | Họ làm gì với PQC / bảo mật | Nhóm học gì |
|---|---|---|
| **Google Chrome** | TLS 1.3 mặc định **hybrid X25519+ML-KEM768** (codepoint `0x11EC`); bỏ Kyber cũ vì ML-KEM final khác. | Dùng đúng **hybrid X25519MLKEM768** cho RQ3. |
| **Google nội bộ (ALTS)** | mTLS-like, **định danh theo entity** (không theo hostname); handshake **EC + quantum-safe**; trao cert ký. | Lớp AuthN định danh **theo identity** (SPIFFE-style). |
| **Google BeyondCorp** | **Zero-trust**: không tin mạng nội bộ; mọi request xác thực + cert thiết bị (CBA). | Onion = deny-by-default mọi zone. |
| **Cloudflare** | ~43% kết nối người dùng (09/2025) đã **hybrid PQC**; PQC tới cả **origin server** + IPsec. | PQC khả thi quy mô lớn → động lực đề tài. |
| **AWS** | **AWS-LC** + ML-KEM; **AWS Private CA + KMS** ký code **ML-DSA**; root PQC trong HSM. | CA + KMS giữ khóa; ký bằng ML-DSA. |
| **Apple iMessage PQ3** | PQC **cả initial key + ratchet liên tục** (Level 3); chống harvest-now-decrypt-later. | Lý do "tại sao PQC ngay bây giờ". |
| **Signal PQXDH** | App nhắn tin lớn đầu tiên có PQC ở **key establishment** (Level 2). | Hybrid KEM trong key agreement. |
| **Sigstore (Linux Fdn)** | Ký phần mềm **keyless** (Fulcio CA + Rekor log); short-lived cert. | Bảo mật chuỗi cung ứng cho image benchmark. |

**Thông điệp cốt lõi để báo cáo:** thiết kế của nhóm = *bản thu nhỏ của kiến trúc thật*: **hybrid PQC (Google/Cloudflare) + identity-based mTLS (Google ALTS/SPIFFE) + PKI 3 cấp có HSM (AWS/DigiCert) + zero-trust (BeyondCorp) + ký token/cert bằng ML-DSA (AWS ML-DSA code signing)*.

---

# PHẦN F — BỔ SUNG BẢO MẬT (thêm zô, miễn là security)

> Các hạng mục **nên thêm** để đồ án "đầy" về bảo mật (gắn với invariants NT2205 I1–I6):

1. **Crypto-agility / Hybrid:** luôn ghép cổ điển + PQC (X25519MLKEM768) → nếu 1 cái gãy vẫn an toàn; dễ chuyển thuật toán. *(Google/Cloudflare đang làm.)*
2. **Harvest-now-decrypt-later:** nêu rõ trong threat model → biện minh PQC ngay. *(Apple PQ3.)*
3. **Certificate Transparency (CT):** log công khai mọi cert phát hành → phát hiện cert giả mạo.
4. **Revocation:** OCSP stapling / CRL / short-lived cert (giảm phụ thuộc revocation).
5. **HSM** cho Root CA + token-signing key (không để khóa trần trên đĩa).
6. **Key rotation tự động** + versioning + blast-radius ≤ 24h.
7. **PoP (Proof-of-Possession):** DPoP hoặc mTLS-bound token → chống replay token bị trộm.
8. **Zero-trust / deny-by-default:** mọi zone không tin nhau; PEP@Gateway + PDP(OPA).
9. **mTLS east-west** giữa mọi service nội bộ (SPIFFE SVID).
10. **AEAD + nonce discipline:** AES-256-GCM / XChaCha20-Poly1305; misuse-resistant (GCM-SIV) khi phù hợp.
11. **Constant-time / side-channel:** xác nhận PQClean + OpenSSL constant-time; (mở rộng) `dudect`/timecop.
12. **Supply-chain (Sigstore/cosign):** ký Docker image + SBOM của chính benchmark.
13. **Defense in depth:** Firewall + IDS/IPS chỉ ở biên (DMZ), zone segmentation cho phần trong.
14. **Observability:** log có cấu trúc + lý do quyết định AuthZ (giải thích được — I5).

---

# PHẦN G — NGUỒN (đã research)

**PQC deployment thực tế:**
- Google Chrome → ML-KEM hybrid: blog.google/chromium "Advancing Our Amazing Bet on Asymmetric Cryptography"; The Hacker News "Google Chrome Switches to ML-KEM" (2024).
- Cloudflare: blog.cloudflare.com "State of the post-quantum Internet in 2025"; "Automatically Secure …6,000,000 domains"; developers.cloudflare.com/ssl/post-quantum-cryptography.
- AWS: aws.amazon.com/security/post-quantum-cryptography; AWS blog "Post-quantum (ML-DSA) code signing with AWS Private CA and AWS KMS".
- Apple iMessage PQ3: security.apple.com/blog/imessage-pq3. Signal PQXDH: signal.org (PQXDH).

**Định danh & PKI:**
- SPIFFE/SPIRE: spiffe.io/docs (SVID, trust domain, rotation); redhat.com "What are SPIFFE and SPIRE".
- Google ALTS: cloud.google.com/docs/security/encryption-in-transit/application-layer-transport-security. BeyondCorp/BeyondProd: cloud.google.com/docs/security/beyondprod.
- PKI hierarchy / chain of trust: Sectigo, Keyfactor, DigiCert (root vs intermediate).

**Chứng chỉ PQC & ký phần mềm:**
- Composite ML-DSA in X.509: lamps-wg draft-ietf-lamps-pq-composite-sigs; ML-DSA in X.509 = **RFC 9881**.
- Sigstore (Fulcio + Rekor + cosign), SLSA provenance: sigstore.dev.

**Chuẩn nền:** NIST FIPS 203 (ML-KEM), FIPS 204 (ML-DSA), FIPS 205 (SLH-DSA); IETF draft-ietf-tls-ecdhe-mlkem (X25519MLKEM768). *(Danh mục IEEE đầy đủ: `06_References_and_Technical_Roadmap.md` Phụ lục B.)*

> **Truy vết file 05/08:** Phần A = WP1–WP7 (08); lớp Crypto = §3/§7 file 05; onion 3 lớp = 08+NT2205. Phần B–F bổ sung "ai ký/ai giữ khóa/node" mà bản tổng hợp của nhóm còn thiếu.
