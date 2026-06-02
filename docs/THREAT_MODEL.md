# THREAT MODEL — Phân tích mối đe dọa & rủi ro mật mã (Crypto Risk Analysis)

> **Đề tài (NT219 — Cryptography & Applications):** Benchmark & triển khai mật mã hậu lượng tử (Post-Quantum Cryptography) — **ML-KEM (FIPS 203)** cho trao đổi khóa + **ML-DSA (FIPS 204)** cho chữ ký số, đối chứng với RSA/ECC cổ điển, ở các mức an toàn NIST **Category 1 / 3 / 5**. Triển khai trong **TLS 1.3** (hybrid group `X25519MLKEM768` + chứng chỉ ML-DSA) trên **x86 + ARM (Raspberry Pi 4)**.
>
> **Kiến trúc "onion" — 3 lớp trong cùng:** **Cryptography → Authentication (AuthN) → Authorization (AuthZ)** ôm sát *Asset*. Tài liệu này bám sát rubric NT2205 (asset-centric) và các *invariants* **I1–I7** của đồ án.
>
> **Bằng chứng đo lường:** mọi số liệu định lượng trong tài liệu trích từ repo này — `data/micro/` (micro-benchmark x86/ARM), `data/tls/x86/handshake_x86.txt` (đo handshake & kích thước cert), `data/resource/` (RSS, code size, năng lượng mô hình). Số ARM NEON và năng lượng là *modeled* (đã ghi rõ ở header CSV).

---

## 1. Mục tiêu & phạm vi (Scope)

### 1.1 Mục tiêu của threat model

Tài liệu xác định **chúng ta đang bảo vệ tài sản gì, chống lại đối thủ nào, bằng cơ chế nào**, và quan trọng nhất là chứng minh **vì sao phải migrate sang PQC ngay bây giờ** dù máy tính lượng tử đủ mạnh (CRQC) chưa tồn tại. Cụ thể:

1. Liệt kê **attacker model** (từ kẻ nghe lén thụ động đến đối thủ CRQC tương lai).
2. Mô tả **mối đe dọa lượng tử** lên mật mã cổ điển (Shor, Grover) và lý do tin rằng bài toán lattice kháng lượng tử.
3. Lập **danh mục tài sản A1–A5** và gắn mỗi tài sản với lớp onion bảo vệ nó.
4. Liệt kê **đe dọa theo từng lớp onion** (Crypto / AuthN / AuthZ) dưới dạng bảng STRIDE-ish → tài sản → mitigation.
5. Phân tích **rủi ro đặc thù của PQC** (kích thước lớn, độ trưởng thành thấp, rejection-sampling timing, downgrade).
6. Đặt **mục tiêu bảo vệ SMART** (định lượng) và **map mitigation vào thiết kế** qua invariants **I1–I7**.
7. Nêu trung thực **rủi ro tồn dư & giới hạn**.

### 1.2 Phạm vi (in-scope) và ngoài phạm vi (out-of-scope)

| In-scope | Out-of-scope |
|---|---|
| Mật mã đối xứng/bất đối xứng, KEM, chữ ký số, AEAD | Bảo mật vật lý cơ sở (datacenter, BMS) |
| Kênh TLS 1.3 (handshake hybrid, AEAD record) | DFIR / điều tra số sau sự cố (theo yêu cầu rubric: *không DFIR*) |
| AuthN bằng cert/token ML-DSA; AuthZ deny-by-default | Tấn công social-engineering ngoài phishing kỹ thuật |
| Side-channel **timing** ở mức thuật toán/triển khai | Fault-injection / glitching phần cứng đầy đủ (chỉ nêu nhận thức) |
| Mối đe dọa lượng tử (Shor/Grover) và HNDL | Supply-chain compromise toàn diện (chỉ nêu giới hạn) |

**Hệ thống tham chiếu** (theo `docs/09_Architecture_Nodes_Deployment.md`): *Secure Edge-to-Cloud API* — thiết bị biên ARM (Pi 4, node N1) trao đổi dữ liệu với dịch vụ trung tâm x86 qua **TLS 1.3 hybrid PQC**; token/cert ký bằng **ML-DSA**; KMS/Vault (N7) + CA (N6) ở zone tin cậy nhất.

---

## 2. Attacker model (Mô hình kẻ tấn công)

Ta mô hình hóa năng lực kẻ tấn công theo hai trục: **(a) vị trí trên kênh** và **(b) năng lực tính toán** (cổ điển vs lượng tử). Đối thủ mạnh nhất là **CRQC** — nhưng đối thủ *nguy hiểm nhất ngay hôm nay* lại là **HNDL** (lưu trữ trước, giải mã sau).

| Mã | Kẻ tấn công | Năng lực | Mục tiêu | Vi phạm thuộc tính |
|---|---|---|---|---|
| **AT-1** | Passive eavesdropper | Đọc toàn bộ lưu lượng (tap/SPAN, ISP, Wi-Fi) | Khôi phục plaintext, metadata | Confidentiality |
| **AT-2** | Active MITM / tamper | Chèn/sửa/xóa gói; đứng giữa kết nối | Đọc & sửa nội dung, mạo danh endpoint | Confidentiality, Integrity, Authenticity |
| **AT-3** | Replay attacker | Ghi & phát lại bản tin/handshake/token hợp lệ | Lặp giao dịch, vượt xác thực | Freshness, Authenticity |
| **AT-4** | Downgrade attacker | Ép thương lượng hạ cấp thuật toán/version | Buộc dùng nhóm/cipher yếu hơn (classical-only) | Confidentiality (gián tiếp) |
| **AT-5** | **HNDL** (harvest-now-decrypt-later) | AT-1 + **lưu trữ dài hạn**; chờ CRQC tương lai | Giải mã *hồi tố* lưu lượng hôm nay sau 10–20 năm | Long-term Confidentiality |
| **AT-6** | **CRQC adversary** | Sở hữu máy tính lượng tử đủ lớn, chạy Shor/Grover | Phá RSA/ECC theo thời gian đa thức; làm yếu AES/hash | Tất cả thuộc tính dựa trên crypto cổ điển |

### 2.1 Vì sao HNDL buộc phải migrate sang PQC *ngay bây giờ* — lập luận theo timeline (Mosca)

CRQC chưa tồn tại, nhưng **bí mật bị thu thập hôm nay vẫn còn nhạy cảm trong nhiều năm tới** (hồ sơ y tế, khóa ký dài hạn, dữ liệu pháp lý). Kẻ AT-5 chỉ cần **lưu ciphertext hôm nay** và **giải mã khi CRQC xuất hiện**. Theo **định lý Mosca**, ta gặp rủi ro nếu:

> **X + Y > Z**
> trong đó: **X** = số năm dữ liệu cần được bảo mật (shelf-life), **Y** = số năm cần để migrate toàn hệ thống sang PQC, **Z** = số năm cho đến khi CRQC khả dụng.

Với hệ thống Edge-to-Cloud có dữ liệu nhạy cảm cần bảo mật **X ≈ 10–15 năm**, thời gian migrate thực tế **Y ≈ 2–5 năm** (cập nhật thư viện, cert, giao thức, thiết bị ARM ngoài hiện trường), và ước lượng dè dặt **Z ≈ 10–20 năm**: tổng **X + Y** rất dễ vượt **Z**. **Kết luận:** dù CRQC còn xa, lưu lượng *hôm nay* đã nằm trong cửa sổ rủi ro HNDL → **phải triển khai PQC-hybrid ngay**, không chờ CRQC. Đây chính là động lực cốt lõi của đồ án (invariant **I7**).

---

## 3. Mối đe dọa lượng tử lên mật mã cổ điển

### 3.1 Shor's algorithm — phá RSA và ECC

Thuật toán Shor giải **factoring** (cơ sở của RSA) và **discrete logarithm** (cơ sở của ECDH/ECDSA, DH cổ điển) trong **thời gian đa thức** trên CRQC. Hệ quả: các hệ cổ điển này **mất gần như toàn bộ an toàn** trước đối thủ lượng tử — coi như **0-bit security vs CRQC**.

| Hệ cổ điển | Bài toán khó (cổ điển) | Trước CRQC (Shor) |
|---|---|---|
| RSA (factoring) | Phân tích số nguyên lớn | **Bị phá — 0-bit** |
| ECDH / ECDSA (ECC) | Elliptic-curve discrete log | **Bị phá — 0-bit** |
| Diffie-Hellman cổ điển | Discrete log (mod p) | **Bị phá — 0-bit** |

> Đây là lý do trong handshake hybrid, **thành phần X25519 (ECDH) chỉ còn vai trò chống đối thủ *cổ điển*** — phần kháng lượng tử do **ML-KEM** đảm nhiệm.

### 3.2 Grover's algorithm — speedup bậc hai trên mật mã đối xứng/hash

Grover cho **tăng tốc bậc hai** (quadratic) trong tìm kiếm khóa brute-force: khóa **n-bit** chỉ còn cường độ hiệu dụng **≈ n/2 bit** trước CRQC. Khác Shor, đây **không** là sụp đổ hoàn toàn — chỉ là *giảm nửa*. Cách phòng vệ: **dùng khóa gấp đôi**.

| Primitive | An toàn cổ điển | An toàn hậu lượng tử (Grover) | Khuyến nghị |
|---|---|---|---|
| AES-128 | 128-bit | **≈ 64-bit** | Không đủ cho dữ liệu dài hạn |
| **AES-256** | 256-bit | **≈ 128-bit** | **Dùng cho Cat 5** ✅ |
| SHA-256 (preimage) | 256-bit | ≈ 128-bit | Đủ cho hầu hết mục đích |

→ Đồ án chọn **AES-256-GCM** cho lớp dữ liệu (AEAD) để giữ **≈128-bit post-quantum**, đồng bộ với mức **Category 5** của ML-KEM-1024 / ML-DSA-87. (Đã benchmark AES-128/192/256 ở `data/micro/x86/aes_x86.csv`; chênh lệch chi phí giữa AES-256 và AES-128 nhỏ — median encrypt 7304 ns vs 6498 ns — nên việc "lên 256-bit" gần như *miễn phí* về hiệu năng.)

### 3.3 Vì sao bài toán lattice được tin là kháng lượng tử

ML-KEM và ML-DSA dựa trên các bài toán lattice **structured** trong vành module:

- **ML-KEM (FIPS 203)** ← **Module-LWE** (Module Learning With Errors): khôi phục bí mật từ các phương trình tuyến tính *bị nhiễu* trên lattice module. Tìm vector ngắn / giải LWE là **NP-hard ở trường hợp xấu nhất** và **chưa có thuật toán lượng tử nào** cho speedup đa thức tương tự Shor.
- **ML-DSA (FIPS 204)** ← **Module-LWE + Module-SIS** (Short Integer Solution), theo khuôn mẫu **Fiat–Shamir with Aborts** (CRYSTALS-Dilithium). An toàn dựa trên độ khó tìm vector ngắn (SIS) và phân biệt LWE.

| Scheme | Bài toán nền | Vì sao kháng lượng tử |
|---|---|---|
| ML-KEM | Module-LWE | Không tồn tại thuật toán lượng tử đa thức cho LWE/SVP; chỉ có speedup *đa thức nhỏ*, không *exponential* như Shor |
| ML-DSA | Module-LWE + Module-SIS | SIS/LWE khó cả với máy lượng tử; bảo mật giảm về độ khó lattice ở trường hợp xấu nhất |

**Ý chính:** Shor khai thác **cấu trúc nhóm abel** (factoring/DLP) — lattice **không có cấu trúc đó**, nên Shor không áp dụng được; chỉ còn Grover (speedup bậc hai), đã được tính vào việc chọn tham số Cat 1/3/5.

---

## 4. Asset catalog (A1–A5)

Theo rubric NT2205. Cột **Lớp onion** chỉ ra lớp *chịu trách nhiệm chính* bảo vệ tài sản; cột **Invariant** liên kết tới khẳng định cần kiểm chứng.

| Mã | Tài sản | Ví dụ cụ thể trong hệ thống | Trạng thái | Lớp onion bảo vệ | Invariant |
|---|---|---|---|---|---|
| **A1** | **Dữ liệu** | Telemetry từ N1 Edge; payload API; bản ghi DB (N5) | at-rest / in-transit / in-process | Cryptography | I1, I3 |
| **A2** | **Bí mật & Khóa** | **KEK/DEK** (envelope), **khóa ký ML-DSA**, **khóa private KEM (ML-KEM decaps key)**, **seed TOTP** | at-rest (Vault) / in-process | Cryptography (+ KMS) | I1, I6 |
| **A3** | **Danh tính** | User, service account, **device identity (cert ML-DSA)** của N1 | in-transit / at-rest | Authentication | I4 |
| **A4** | **Trạng thái & Chính sách** | Session cookie, **JWT claims (ký ML-DSA)**, **RBAC/ABAC policy** (OPA), ACL | in-process / in-transit | Authorization | I2, I5 |
| **A5** | **Hạ tầng tin cậy** | **CA/PKI (N6)**, **KMS/Vault (N7)**, JWKS, trust store | at-rest / in-process | Cryptography + AuthN + AuthZ (cross-cutting) | I6 |

**Tài sản giá trị cao nhất (crown jewels):** **A2** (đặc biệt khóa private KEM và khóa ký ML-DSA) và **A5** (CA + KMS). Lộ A2/A5 phá vỡ mọi đảm bảo của các lớp trên — do đó chúng được đặt ở **Zone 3 (Secure/Crypto)** tin cậy nhất và là trọng tâm của **I6** (key ops quan sát được).

---

## 5. Đe dọa theo từng lớp onion

Cột **STRIDE-ish**: S=Spoofing, T=Tampering, R=Repudiation, I=Information disclosure, D=Denial of service, E=Elevation of privilege.

### 5.1 Lớp Cryptography (lớp trong cùng — nơi PQC "sống")

| # | Đe dọa | STRIDE-ish | Tài sản | Mitigation |
|---|---|---|---|---|
| C-1 | Nghe lén lưu lượng TLS (AT-1) | I | A1 | TLS 1.3 + AEAD `AES-256-GCM`; key-exchange hybrid `X25519MLKEM768` |
| C-2 | **HNDL** — thu thập ciphertext, giải mã sau bằng CRQC (AT-5) | I | A1, A2 | **ML-KEM** trong hybrid → forward secrecy *kháng lượng tử*; AES-256 (≈128-bit PQ) |
| C-3 | Lộ/đánh cắp khóa private (KEM decaps key, signing key) | I, E | A2 | Khóa lưu trong **KMS/Vault (N7)**; rotation ≤ 10 phút; blast-radius giới hạn (I6) |
| C-4 | Sửa đổi ciphertext/record (AT-2) | T | A1 | AEAD tag (GMAC) → giải mã thất bại + log |
| C-5 | Nonce reuse trong GCM | I, T | A1 | Nonce-discipline; có thể dùng AEAD misuse-resistant; giám sát trùng nonce |
| C-6 | Side-channel timing trên triển khai PQC | I | A2 | Triển khai **constant-time**; xem §6c |

### 5.2 Lớp Authentication (AuthN)

| # | Đe dọa | STRIDE-ish | Tài sản | Mitigation |
|---|---|---|---|---|
| N-1 | Mạo danh server (server impersonation) (AT-2) | S | A3, A5 | Xác thực **cert ML-DSA** của server; verify chuỗi tới CA (N6); pin TLSv1.3 |
| N-2 | Giả mạo chứng chỉ (cert forgery) | S, T | A3, A5 | Chữ ký ML-DSA *kháng lượng tử* trên cert; CA private key trong KMS |
| N-3 | Phishing / đánh cắp credential | S | A3 | **WebAuthn/FIDO2** (phishing-resistant) ưu tiên; TOTP fallback có rate-limit (I4) |
| N-4 | Replay handshake/credential (AT-3) | S, R | A3, A4 | **PoP** (DPoP / mTLS-bound token); freshness của TLS 1.3; TTL ngắn (I4) |
| N-5 | Mạo danh device biên N1 | S | A3 | mTLS **client cert ML-DSA** cấp bởi N6; trust store tập trung |

### 5.3 Lớp Authorization (AuthZ)

| # | Đe dọa | STRIDE-ish | Tài sản | Mitigation |
|---|---|---|---|---|
| Z-1 | Leo thang đặc quyền (privilege escalation) | E | A4 | **Deny-by-default**; least-privilege; RBAC→ABAC; PDP (OPA/Rego) (I5) |
| Z-2 | Giả mạo token (token forgery) | S, T | A4 | JWT ký **ML-DSA**; kiểm `kid`; verify chữ ký bắt buộc (I2) |
| Z-3 | **`alg=none` / alg-confusion** trên JWT | T, E | A4 | **Pin `alg`** (chỉ chấp nhận ML-DSA); từ chối `none`/HS-vs-RS confusion |
| Z-4 | Refresh-token reuse | S, R | A4 | Refresh-rotation + reuse-detection; thu hồi phiên |
| Z-5 | Quyết định AuthZ không giải thích được (repudiation) | R | A4 | **Log reason** mọi quyết định PDP (I5) |

---

## 6. Rủi ro đặc thù của PQC

PQC mạnh hơn về kháng lượng tử nhưng **mang rủi ro vận hành mới**. Bốn nhóm chính:

### (a) Kích thước key / cert / signature lớn → fragmentation, amplification, DoS

PQC có khóa và chữ ký **lớn hơn cổ điển nhiều lần**. Đo lường trong repo:

| Đối tượng | PQC | Cổ điển | Tỉ lệ |
|---|---|---|---|
| **Chữ ký** ML-DSA-87 vs ECDSA P-521 | **4627 B** (`summary_x86.csv`) | 139 B | **≈ 33×** |
| **Cert** ML-DSA-65 vs ECDSA | **7464 B** (`data/tls/x86/sizes.csv`) | 534 B (đo) / ~0.5 KB | **≈ 14×** |
| **Public key** ML-KEM-1024 | 1568 B | (ECDH ~32–65 B) | nhiều lần |
| **Ciphertext/KEM** ML-KEM-1024 | 1568 B | — | — |

**Hệ quả an ninh:**
- **Fragmentation:** ClientHello/cert chain vượt MTU → phân mảnh IP/TCP, dễ rớt gói, tăng RTT.
- **Amplification / DoS:** cert chain & chữ ký lớn khiến **một handshake tiêu tốn nhiều băng thông & CPU hơn** → bề mặt **DoS khuếch đại** rộng hơn; kẻ tấn công gửi nhiều ClientHello giả buộc server tính KEM/verify đắt.
- **Tác động hiệu năng đã đo:** handshake hybrid `X25519MLKEM768 + ML-DSA-65` đạt **1934 handshakes/s** so với classical `ECDHE-P256 + ECDSA` **2118 handshakes/s** (`handshake_x86.txt`) — chậm **≈ 8.7%**. Mức suy giảm *chấp nhận được* trên x86, nhưng cần theo dõi trên ARM.

> **Mitigation:** rate-limit ClientHello tại gateway (N2); cân nhắc cert chain tối giản; giám sát kích thước record; đảm bảo path-MTU.

### (b) Độ trưởng thành thấp / triển khai mới → side-channel & bug

ML-KEM/ML-DSA mới chuẩn hóa (FIPS 203/204, 2024). Thư viện (liboqs, PQClean, OpenSSL 3.6) **chưa được kiểm thử thực chiến lâu năm** như OpenSSL RSA/ECC. Rủi ro: lỗi cài đặt (memory, parsing), **side-channel** chưa được làm cứng, và **interoperability** giữa các bản nháp giao thức. Build dùng trong đồ án là **reference-C PQClean** (không tối ưu side-channel sâu) — xem §9.

### (c) ML-DSA rejection sampling → thời gian ký *phụ thuộc dữ liệu* (right-skewed)

ML-DSA dùng **Fiat–Shamir with Aborts**: ký lặp lại, **loại bỏ (reject)** các mẫu không thỏa ràng buộc chuẩn (norm bound) và **thử lại** đến khi đạt. Số vòng lặp **biến thiên** theo khóa/thông điệp/randomness → **thời gian ký không hằng định, lệch phải (right-skewed)**.

**Bằng chứng đo (`data/micro/x86/mldsa_x86.csv`, `summary_x86.csv`, reference-C, n=100):**

| Param | median Sign | p95 Sign | mean Sign | Nhận xét |
|---|---|---|---|---|
| ML-DSA-44 (Cat 2) | 0.315 ms (314792 ns) | 0.929 ms (928792 ns) | 0.381 ms | mean ≫ median → đuôi phải dài |
| ML-DSA-65 (Cat 3) | 0.454 ms (453553 ns) | 1.661 ms (1660820 ns) | 0.591 ms | p95 ≈ **3.7×** median |
| **ML-DSA-87 (Cat 5)** | **0.563 ms** (562882 ns) | **≈ 1.5 ms** (1533649 ns) | 0.702 ms | **median 0.56 ms, p95 ~1.5 ms** |

So với Keygen/Verify (gần như hằng định: ML-DSA-87 Verify median 239044 ns, p95 376806 ns — tỉ lệ ~1.6×), **Sign** có đuôi rõ rệt.

**Phân biệt quan trọng — timing side-channel vs algorithmic variability:**
- Đây là **biến thiên thuật toán** (số vòng reject), **không nhất thiết** rò rỉ khóa nếu mỗi vòng được cài **constant-time** và việc reject **không phụ thuộc khóa bí mật theo cách quan sát được**.
- **Tuy nhiên**, nếu cài đặt để **thời gian/số vòng tương quan với bí mật**, kẻ AT-1 đo timing có thể suy luận thông tin. Vì vậy cần **constant-time implementation** (so sánh, rejection, sampling đều phải data-independent ở mức secret) và, khi cần, **deterministic signing** (hedged) để giảm phụ thuộc randomness.
- **Vận hành:** vì p95 ≈ 1.5 ms (ML-DSA-87), phải **đặt timeout/SLA theo p95–p99**, không theo median, để tránh nghẽn dưới tải.

### (d) Downgrade attack ép về classical-only

Kẻ AT-4 cố gỡ phần PQC, ép thương lượng nhóm **chỉ-cổ-điển** (P-256/X25519 đơn lẻ) → mở lại cửa cho HNDL.

**Mitigation — hybrid + config pinning:**
- **Hybrid `X25519MLKEM768`:** khóa phiên dẫn xuất từ **cả hai** thành phần; **an toàn nếu *bất kỳ* một thành phần còn vững** (X25519 vững vs cổ điển; ML-KEM-768 vững vs lượng tử). Kẻ phá được một bên vẫn không có khóa.
- **TLS config pinning:** chỉ cho phép **TLSv1.3** và **chỉ các nhóm PQ-hybrid** (loại bỏ nhóm classical-only và TLS ≤ 1.2). Khi đó downgrade về classical-only sẽ **handshake failure** thay vì rớt âm thầm.
- Bằng chứng cấu hình: `handshake_x86.txt` xác nhận case hybrid thương lượng đúng `Negotiated TLS1.3 group: X25519MLKEM768`, `Peer signature type: mldsa65`.

---

## 7. Mục tiêu bảo vệ SMART (định lượng)

| # | Mục tiêu SMART | Ngưỡng đo | Lớp / Invariant |
|---|---|---|---|
| G-1 | **0 byte plaintext rò rỉ** trên kênh bảo vệ | 0 byte giải mã được từ tap (đối chứng kênh không bảo vệ) | Crypto / I1 |
| G-2 | **Lưu lượng harvest-now vẫn bảo mật vs CRQC tương lai** | 100% phiên dùng KEM kháng lượng tử (hybrid); 0 phiên classical-only | Crypto / I7 |
| G-3 | **0 chữ ký giả/tampered được chấp nhận** | Sửa 1 bit cert/token/record → verify FAIL 100% + log | Crypto/AuthZ / I2, I3 |
| G-4 | **Downgrade về classical-only bị từ chối 100%** | Mọi attempt ép TLS≤1.2 hoặc nhóm classical-only → handshake fail | Crypto / I7 |
| G-5 | **AuthN chống phishing** | WebAuthn success cao; false-accept = 0; replay (no-PoP) bị chặn 100% | AuthN / I4 |
| G-6 | **AuthZ deny-by-default** | Request không có quyền → deny 100%, có *reason* trong log | AuthZ / I5 |
| G-7 | **Key rotation ≤ 10 phút**; blast-radius giới hạn | Thời gian rotate khóa ≤ 10 min; key cũ revoke đúng hạn | KMS / I6 |
| G-8 | **Overhead handshake chấp nhận được** | Suy giảm throughput hybrid vs classical ≤ 10% (đo: **≈8.7%** trên x86) | Crypto / — |

---

## 8. Mitigation map vào thiết kế (Invariants I1–I7)

| Invariant | Khẳng định | Cơ chế thiết kế (mapping) | Bằng chứng / Node |
|---|---|---|---|
| **I1** | Không rò rỉ plaintext trên kênh bảo vệ | TLS 1.3 + **AES-256-GCM** AEAD; hybrid KEM `X25519MLKEM768` | `handshake_x86.txt`; N1↔N2 |
| **I2** | Tampering (ciphertext/token) bị từ chối + log | AEAD tag; **JWT ký ML-DSA** + pin `alg`; verify bắt buộc | AuthZ PDP (N4), GW (N2) |
| **I3** | Toàn vẹn dữ liệu (data integrity / origin) | AEAD integrity; chữ ký **ML-DSA** trên telemetry & cert | N1 device key, N6 CA |
| **I4** | AuthN chống phishing + **PoP** chống replay | **WebAuthn/FIDO2**; DPoP/mTLS-bound token; cert ML-DSA mTLS | N3 IdP, N2 GW |
| **I5** | Quyết định AuthZ giải thích được | **Deny-by-default**; PDP OPA/Rego; **log reason** | N4 PDP |
| **I6** | Key ops quan sát được (rotate nhanh, blast-radius nhỏ) | **KMS/Vault**: gen/rotate/revoke/versioning/audit; rotation ≤10 phút | N7 KMS |
| **I7** | **Migration PQC / kháng HNDL** | **ML-KEM** hybrid (forward secrecy kháng lượng tử); pin TLSv1.3 + chỉ nhóm PQ-hybrid; chống downgrade | Toàn kênh; §2.1, §6d |

**Tóm tắt 3 trụ thiết kế:**
1. **Hybrid KEM `X25519MLKEM768`** → bảo mật kênh, kháng HNDL, an toàn nếu *một* thành phần còn vững.
2. **Chứng chỉ & token ML-DSA** → AuthN/AuthZ kháng lượng tử; chống cert/token forgery; chống `alg=none`.
3. **AES-256-GCM AEAD** cho dữ liệu (≈128-bit post-quantum) + **deny-by-default AuthZ** với token ký ML-DSA.

---

## 9. Rủi ro tồn dư & Giới hạn (trung thực)

| Rủi ro tồn dư | Mô tả | Mức | Giảm thiểu / Ghi chú |
|---|---|---|---|
| **Độ tin cậy tham số PQC** | ML-KEM/ML-DSA mới (2024); phân tích thám mã lattice còn tiến triển; có thể cần điều chỉnh tham số tương lai | Trung bình | Dùng **hybrid** (không đặt cược 100% vào PQC); theo dõi NIST IR & cập nhật |
| **Side-channel trên build reference (non-NEON)** | Build đo là **PQClean reference-C** — **không** tối ưu constant-time sâu; rejection-sampling ML-DSA right-skew (§6c) có thể lộ qua timing nếu cài sai | Trung bình–Cao | Production cần thư viện **constant-time, hardened**; ARM nên dùng backend NEON; audit timing |
| **Overhead năng lượng/kích thước trên ARM ràng buộc** | Trên Pi 4, thời gian PQC cao hơn nhiều x86. Ví dụ (modeled, `energy_estimate.csv` + `neon_vs_ref.csv`): ML-DSA-87 **Sign ≈ 2.70 ms ref / ≈1.59 ms NEON**, năng lượng **≈10.8 mJ/op**; ML-KEM-1024 Decaps ≈0.43 ms ref. Code size/RSS nhỏ (mldsa text ~16.5 KB; peak RSS ~3.7 MB) | Trung bình | NEON cho speedup **1.7–2.3×**; tách Sign khỏi đường nóng; batch; chọn Cat phù hợp thiết bị |
| **Số ARM & năng lượng là *modeled*** | `neon_vs_ref.csv`, `energy_estimate.csv` ghi rõ **MODELED** (không phải đo phần cứng thật) | — | Cần đo thực trên Pi 4 để xác nhận |
| **Phụ thuộc thư viện / supply-chain** | liboqs/PQClean/OpenSSL 3.6 còn non-trẻ; lỗi parsing cert lớn có thể thành DoS | Trung bình | Pin version; theo dõi CVE; fuzz parser cert ML-DSA |
| **DoS qua handshake đắt** | Cert/sig lớn + verify ML-DSA → amplification (§6a) | Trung bình | Rate-limit ClientHello; cookie/anti-flood; giám sát p95 |

> **Nguyên tắc xuyên suốt:** đồ án **không** thay thế cổ điển bằng PQC một cách "tất tay" — mà dùng **hybrid**, để nếu *một* trong hai họ thuật toán bị phá (lượng tử *hoặc* lỗi tham số PQC), kênh vẫn an toàn. Đây là phòng vệ chống chính *sự non trẻ* của PQC.

---

## 10. References

1. **NIST FIPS 203** — *Module-Lattice-Based Key-Encapsulation Mechanism Standard* (ML-KEM), 2024.
2. **NIST FIPS 204** — *Module-Lattice-Based Digital Signature Standard* (ML-DSA), 2024.
3. **NIST FIPS 205** — *Stateless Hash-Based Digital Signature Standard* (SLH-DSA), 2024.
4. **NIST IR 8413** — *Status Report on the Third Round of the NIST Post-Quantum Cryptography Standardization Process*, 2022.
5. **IETF draft-ietf-tls-hybrid-design** — *Hybrid key exchange in TLS 1.3*.
6. **RFC 8446** — *The Transport Layer Security (TLS) Protocol Version 1.3*.
7. **CRYSTALS-Kyber** — Bos et al., *CRYSTALS-Kyber: a CCA-secure module-lattice-based KEM* (cơ sở của ML-KEM).
8. **CRYSTALS-Dilithium** — Ducas et al., *CRYSTALS-Dilithium: Digital Signatures from Module Lattices* (cơ sở của ML-DSA; Fiat–Shamir with Aborts).
9. **Mosca, M.** — *Cybersecurity in an era with quantum computers: will we be ready?* (định lý X + Y > Z về timeline HNDL).
10. **Shor, P.W.** — *Polynomial-Time Algorithms for Prime Factorization and Discrete Logarithms on a Quantum Computer*.
11. **Grover, L.K.** — *A fast quantum mechanical algorithm for database search*.

---

### Phụ lục — Nguồn số liệu trong repo

| Số liệu dùng trong tài liệu | File nguồn |
|---|---|
| Micro-benchmark x86 (median/mean/p95, kích thước pk/sk/sig) | `data/micro/summary_x86.csv`, `data/micro/x86/*.csv` |
| ARM NEON speedup (modeled) | `data/micro/neon_vs_ref.csv` |
| Handshake/s & kích thước cert TLS | `data/tls/x86/handshake_x86.txt`, `data/tls/x86/sizes.csv` |
| RSS, code size, năng lượng (modeled) | `data/resource/peak_rss.txt`, `data/resource/code_size.txt`, `data/resource/energy_estimate.csv` |
| Kiến trúc onion, node N1–N7, invariants | `docs/09_Architecture_Nodes_Deployment.md`, `NT2205_Checklist_Corrected_v2.md` |

> *Lưu ý: các số gắn nhãn **MODELED** (ARM NEON, năng lượng) là ước lượng mô hình, không phải đo phần cứng thật — đã nêu ở §9.*
