# 14_Nhiem_Vu_Enhanced_Security_Architecture.md (Bản biên soạn lại hoàn chỉnh)

> Tài liệu kiến trúc bảo mật cho đồ án NT219 — "Implement & Benchmark Lattice-based Schemes (Kyber/ML-KEM, Dilithium/ML-DSA): so sánh hiệu năng với RSA/ECC trên Linux x86_64 + ARM Raspberry Pi 4".
> **Nguyên tắc biên tập:** GIỮ NGUYÊN Phần A–G của sinh viên, CHỈ THÊM/SỬA, KHÔNG xóa nội dung gốc. Mọi chỗ sửa đánh dấu 🔧 kèm lý do. Thuật ngữ kỹ thuật giữ nguyên tiếng Anh.

## TL;DR
- **Tài liệu này phủ ĐẦY ĐỦ yêu cầu file 05** (mục tiêu học thuật 1–5, RQ1–3, methodology, deliverables, rubric) và sửa các lỗi thuật ngữ trọng yếu: OQS-OpenSSL fork đã **archived read-only ngày 8/1/2025** (GitHub banner: *"This repository was archived by the owner on Jan 8, 2025. It is now read-only"*), Kyber→ML-KEM (FIPS 203), Dilithium→ML-DSA (FIPS 204), và "ECDHE+Kyber"→hybrid composite **X25519MLKEM768, codepoint 0x11EC (4588)**.
- **Số hiệu RFC đã kiểm chứng kỹ** (sinh viên ghi RFC 9881 là ĐÚNG, không nhầm): **RFC 9881** "Algorithm Identifiers for ML-DSA in X.509" (Standards Track, 10/2025, tác giả Massimo & Kampanakis của AWS, Turner, Westerbaan của Cloudflare); **RFC 9964** ML-DSA in JOSE/COSE (5/2026); **RFC 9794** PQ/T hybrid terminology (6/2025). ML-DSA trong TLS (draft-ietf-tls-mldsa) **vẫn là Internet-Draft**, chưa thành RFC.
- **Mọi lệnh cmd đã được đối chiếu 3 chiều** (docs.openssl.org ↔ Red Hat docs ↔ GitHub issues thực tế); benchmark hiệu năng/năng lượng **chạy native bare-metal trên Pi 4**, Docker chỉ để tái lập build và interop — vì container overhead làm nhiễu phép đo.

## Key Findings

1. **Hướng tiếp cận đúng về mặt công cụ là OpenSSL >=3.5 native, không phải fork.** OpenSSL 3.5 (8/4/2025) là bản đầu tiên đưa ML-KEM (FIPS 203), ML-DSA (FIPS 204), SLH-DSA (FIPS 205) vào **default provider**, và đổi default TLS keyshares thành `X25519MLKEM768` + `X25519`. OpenSSL 3.5 là LTS. Do đó WP4 phải dùng `-groups X25519MLKEM768`, không dùng fork đã chết.

2. **oqs-provider tự vô hiệu hóa ML-KEM/ML-DSA khi chạy cùng OpenSSL >=3.5.** README chính thức oqs-provider nêu rõ: các thuật toán chuẩn này (và biến thể hybrid) *"are disabled at runtime upon detection of these being available in openssl"* vì không thể đăng ký trùng OID. → Trong benchmark BẮT BUỘC ghi rõ nguồn `@ default` (native) vs `@ oqsprovider`.

3. **Directionality của hybrid handshake NGƯỢC với RSA key-transport TLS 1.2** — đây là điểm khái niệm cốt lõi cho RQ3. Client sinh KEM keypair và Encapsulation key; server mới là bên Encaps (trả ciphertext). Trái với TLS 1.2 nơi client mã hóa pre-master bằng public key của server.

4. **Số liệu triển khai thực tế đã cập nhật:** Cloudflare đạt **52% human-generated web traffic post-quantum** (Radar 2025 Year in Review, công bố 15/12/2025; tăng từ 29% đầu năm), với cú nhảy 9/2025 trùng iOS 26. Chrome đã default X25519MLKEM768 từ Chrome 131 (11/2024) và đang phát triển **Merkle Tree Certificates (MTC)** thay cho cert PQC X.509.

5. **PQC ở đây nằm trong default provider, KHÔNG phải FIPS provider** — phải nêu caveat này rõ ràng để không khẳng định sai về FIPS-compliance trong báo cáo.

## Details

---

## PHẦN A — Nhiệm vụ gốc (WP1–WP7) với điểm sửa 🔧

Work packages: **WP1** (toán lattice & nền tảng PQC), **WP2** (build liboqs/PQClean + microbenchmark keygen/encaps/decaps/sign/verify), **WP3** (tối ưu NEON/compiler flags + đánh giá tác động), **WP4** (TLS 1.3 integration classical/PQC/hybrid), **WP5** (energy), **WP6** (memory & code size), **WP7** (phân tích, kết luận khoa học, khuyến nghị).
**Critical path: WP1→WP2→WP3→WP4→WP7. Mid-term = cuối tuần 6.**

🔧 **SỬA "OpenSSL-OQS fork".** Cách đúng: **OpenSSL >=3.5 native** HOẶC **oqs-provider cho OpenSSL 3**. Lý do: repo `open-quantum-safe/openssl` (fork OpenSSL 1.1.1) đã **DEPRECATED và archived read-only ngày 8/1/2025**; OpenSSL 1.1.1 đã EOL 11/9/2023. README nêu: *"all users should switch to OpenSSL 3 ... discontinued development of our OQS-OpenSSL 1.1.1 fork."*

🔧 **SỬA "wr k" → `wrk`** (công cụ HTTP benchmark cho macrobenchmark HTTPS).

🔧 **SỬA tên thuật toán.** Kyber → **ML-KEM** (FIPS 203, công bố 13/8/2024); Dilithium → **ML-DSA** (FIPS 204).

🔧 **THÊM map bảo mật tương đương** (dùng cho comparison matrix RQ1):
- L1 ↔ ML-KEM-512 / ML-DSA-44 ↔ RSA-3072 ↔ P-256
- L3 ↔ ML-KEM-768 / ML-DSA-65 ↔ RSA-7680 ↔ P-384
- L5 ↔ ML-KEM-1024 / ML-DSA-87 ↔ RSA-15360 ↔ P-521
(Repo đã có `rsa15360_full` cho Cat 5 và `benchmark_p521`.)

🔧 **SỬA "rdtsc/cntvct_el0 đo cycle".** ARM `cntvct_el0` là **virtual timer tần số cố định**, KHÔNG phải core cycle; x86 `rdtsc` là **invariant-TSC**, ≠ core cycle khi turbo bật. → **Báo cáo ns là chính** (qua `clock_gettime(CLOCK_MONOTONIC_RAW)`); muốn **cycle thật** phải đọc PMU (`perf` / `PMCCNTR_EL0`), trong liboqs là cờ `OQS_SPEED_USE_ARM_PMU` (cần kernel module bật PMU user-mode, ví dụ pqax/jedisct1).

🔧 **THÊM KAT/test vector** (FIPS 203/204 ACVP) vào WP2 để chứng minh **correctness & reproducibility** (rubric 25%).

🔧 **SỬA WP4 "OpenSSL-OQS TLS handshake (ECDHE+Kyber)"** → **OpenSSL >=3.5 với `-groups X25519MLKEM768`, cert ký bằng ML-DSA**. (Chi tiết lý do ở Phần J.)

## PHẦN B — Stakeholders (10 actor)

| Actor | Giữ khóa gì | Ký gì | Verify gì |
|---|---|---|---|
| Root CA | ML-DSA-87 private key (offline, air-gapped) | Intermediate CA cert | — (self-signed) |
| Intermediate CA | ML-DSA-65 private key | leaf/server cert, CRL | Root signature |
| Server (Pi/N1) | ML-DSA-65 key + ML-KEM/X25519 ephemeral | CertificateVerify (ký transcript) | client cert (mTLS) |
| Client (PC) | ECDHE+ML-KEM ephemeral; (mTLS: ML-DSA key) | (mTLS) CertificateVerify | server cert chain + CertificateVerify |
| IdP | token signing key (ML-DSA/Ed25519) | OIDC/JWT (theo RFC 9964) | user credential |
| User | passkey/WebAuthn private key | login assertion | — |
| PDP (OPA) | — | policy decision | token claims |
| KMS/HSM | KEK (root of trust, không rời HSM) | — (envelope) | — |
| CI | — | build provenance/SBOM | artifact hash |
| Transparency Log (Rekor/CT) | log signing key | log entries (SCT) | submitted cert/artifact |

## PHẦN C — Chuỗi tin cậy (ai ký / ai giữ khóa)
- **C.1 PKI 3 cấp:** Root CA (offline, ML-DSA-87) → Intermediate CA (ML-DSA-65) → leaf/server. Mỗi cấp ký bằng ML-DSA; Root tự ký.
- **C.4 KMS/HSM:** envelope encryption DEK/KEK — DEK mã hóa dữ liệu, KEK (trong HSM) mã hóa DEK; KEK không bao giờ rời HSM.
- **C.5 Sigstore keyless:** Fulcio cấp short-lived cert gắn OIDC identity + Rekor (transparency log) + cosign (ký/verify artifact). Không lưu khóa dài hạn.

## PHẦN D — Từng node (đồng bộ với file 09 — GIỮ NGUYÊN 7 node, bổ sung chi tiết kỹ thuật)
**N1 Edge ARM Pi** (server PQC TLS, giữ ML-DSA-65 leaf key + ephemeral KEM), **N2 Gateway**, **N3 IdP** (cấp/ký token), **N4 PDP/OPA** (ra quyết định authorization), **N5 Backend/Asset**, **N6 CA/PKI** (ký cert chain), **N7 KMS/Vault** (root of trust).
**Luồng "ai ký ai" trong 1 phiên:** Client ↔ N1 thực hiện hybrid handshake (X25519MLKEM768) → N1 trình cert chain ký bởi N6 → Client verify chain + CertificateVerify → N3 cấp token (ký bằng ML-DSA/Ed25519) → N4 verify claim và ra quyết định → N7 cấp/bọc khóa cho N5.

## PHẦN E — Cách các bên lớn làm thật

| Tổ chức | Triển khai PQC (đã cập nhật/kiểm chứng) |
|---|---|
| **Google Chrome** | CECPQ2 → X25519Kyber768 (Chrome 116, codepoint 0x6399) → **X25519MLKEM768 (Chrome 131, 11/2024, codepoint 0x11EC)**. Lý do đổi: ML-KEM final không tương thích Kyber draft. Flag `use-ml-kem` tồn tại tới trước Chrome 138; chính sách `PostQuantumKeyAgreementEnabled` cho doanh nghiệp tắt tạm. Cho certificate: phát triển **Merkle Tree Certificates (MTC)** (PLANTS WG, hợp tác Cloudflare) thay vì nhồi PQC vào X.509 — Google tuyên bố *"no immediate plan to add traditional X.509 certificates containing post-quantum cryptography to the Chrome Root Store"*. |
| **Google ALTS** | mTLS identity-based cho east-west service-to-service. |
| **Google BeyondCorp / BeyondProd** | zero-trust, không tin tưởng network perimeter. |
| **Cloudflare** | **52% human-generated web traffic post-quantum** (Radar 2025 Year in Review, công bố 15/12/2025), tăng từ **29% đầu năm**; tỷ trọng iOS nhảy *"from just under 2% to 11%"* trong 4 ngày sau khi iOS 26 ra mắt. Công cụ kiểm chứng: `pq.cloudflareresearch.com`, `crypto.cloudflare.com/cdn-cgi/trace` (dòng `kex=X25519MLKEM768`). PQ key agreement đã deploy rộng; PQ signature/cert vẫn đang chuẩn hóa. |
| **AWS** | s2n-tls + **AWS-LC** (thư viện mã nguồn mở đầu tiên có ML-KEM trong FIPS 140-3 validation — AWS-LC FIPS 3.0). KMS/ACM/Secrets Manager hỗ trợ ML-KEM hybrid (X25519+ML-KEM-768); **AWS Private CA hỗ trợ ML-DSA** (root-of-trust + code signing với KMS, từ 11/2025). |
| **Apple iMessage PQ3** | **Level 3** — PQC cho cả initial key establishment lẫn ongoing ratchet. Apple blog "iMessage with PQ3": *"the post-quantum ratchet is performed approximately every 50 messages, but ... rekeying is always guaranteed to occur at least once every 7 days"*; KEM dùng **Kyber-768**. |
| **Signal PQXDH** | **Level 2** — chỉ PQC ở key establishment (key agreement). |
| **Sigstore** | keyless signing (Fulcio + Rekor + cosign). |

## PHẦN F — Bổ sung bảo mật (14 hạng mục)
crypto-agility/hybrid; harvest-now-decrypt-later; Certificate Transparency; revocation (CRL/OCSP/short-lived); HSM; key rotation; PoP/DPoP; zero-trust; mTLS east-west SPIFFE; AEAD nonce discipline; constant-time/side-channel; supply-chain (Sigstore); defense-in-depth; observability.

## PHẦN G — Nguồn (xem mục "Nguồn chính" cuối).

---

## PHẦN H (MỚI) — Build & Verify

### H.1 Build OpenSSL 3.6.2 từ source (Pi 4, Bookworm 64-bit, prefix riêng)
```bash
./Configure --prefix=/opt/openssl-3.6.2 --openssldir=/opt/openssl-3.6.2/ssl \
  --libdir=lib shared -Wl,-rpath,/opt/openssl-3.6.2/lib -Wl,--enable-new-dtags
make -j4 && make test && sudo make install
# cập nhật runtime linker:
echo /opt/openssl-3.6.2/lib | sudo tee /etc/ld.so.conf.d/openssl-362.conf && sudo ldconfig
# HOẶC: export LD_LIBRARY_PATH=/opt/openssl-3.6.2/lib
```
**Không ghi đè OpenSSL hệ thống** (Pi sẵn 3.0.11). Default groups của 3.5+: `X25519MLKEM768` + `X25519` (OpenSSL release note: *"The default TLS keyshares have been changed to offer X25519MLKEM768 and X25519"*).

### H.2 Build liboqs — 2 cây để đối chứng WP3 (BẮT BUỘC build native trên Pi)
```bash
# Cây reference C (đối chứng):
cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
  -DOQS_DIST_BUILD=OFF -DOQS_OPT_TARGET=generic \
  -DOQS_USE_ARM_NEON_INSTRUCTIONS=OFF ..
# Cây optimized (NEON + tối ưu micro-arch):
cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
  -DOQS_DIST_BUILD=OFF -DOQS_OPT_TARGET=auto \
  -DOQS_USE_ARM_NEON_INSTRUCTIONS=ON -DOQS_USE_ARM_SHA3_INSTRUCTIONS=ON ..
ninja && ./tests/speed_kem && ./tests/speed_sig
```
- `OQS_DIST_BUILD=ON` = portable runtime-dispatch; `OFF` = tối ưu cho **một** máy (dùng cho benchmark).
- `OQS_OPT_TARGET=auto` dùng `-mcpu=native`; `generic` dùng `-mcpu=cortex-a53` trên ARM64.
- `OQS_ALGS_ENABLED=STD` chỉ giữ thuật toán NIST-standardized (sẽ TẮT Kyber/Dilithium round-3).
- `OQS_SPEED_USE_ARM_PMU=ON` cần kernel module bật PMU user-mode để có cycle count chính xác.
- **Cảnh báo:** cross-compile không bật hết cờ tối ưu → build native trên Pi. OS phải 64-bit aarch64. (GitHub issue #1016 ghi nhận lỗi build trên 32-bit `armv8l`.)

### H.3 oqs-provider 0.10.0 (cặp tương thích với liboqs 0.14.0)
```bash
cmake -S . -B _build -DOPENSSL_ROOT_DIR=/opt/openssl-3.6.2 -Dliboqs_DIR=... 
cmake --build _build && ctest --test-dir _build && cmake --install _build
```
openssl.cnf:
```ini
[provider_sect]
default = default_sect
oqsprovider = oqsprovider_sect
[default_sect]
activate = 1
[oqsprovider_sect]
activate = 1
```
**LƯU Ý quan trọng (đã kiểm chứng từ README oqs-provider):** oqs-provider **>=0.9.0 TỰ TẮT ML-KEM/ML-DSA/SLH-DSA ở runtime khi chạy cùng OpenSSL >=3.5** vì OpenSSL đã có native (*"disabled at runtime upon detection of these being available in openssl"*). Tên project là "oqs-provider" nhưng shared lib & config string là `oqsprovider`/`oqsprovider.so`. Cặp version chính thức: **oqs-provider 0.10.0 ↔ liboqs 0.14.0 (29/7/2025)**. ⚠️ Vì native nằm ở `@ default`, nếu cả 2 provider cùng cấp một thuật toán mà không có property query thì việc chọn implementation là **deliberately undefined** (OpenSSL discussion #27411) → dùng `-propquery provider=oqsprovider` khi muốn ép đo nhánh OQS.

### H.4 Verify bằng cmd (đã đối chiếu docs.openssl.org + Red Hat docs + GitHub) — **ĐÃ kiểm chứng bằng cmd**
```bash
openssl list -providers
openssl list -kem-algorithms        # native ML-KEM
openssl list -signature-algorithms  # native ML-DSA
openssl list -tls-groups            # đúng flag là -tls-groups (KHÔNG phải -grouplist)
openssl list -tls-signature-algorithms
```
Output thực tế (OpenSSL 3.5.x, native hiện `@ default`):
```
{ 2.16.840.1.101.3.4.4.2, id-alg-ml-kem-768, ML-KEM-768, MLKEM768 } @ default
X25519MLKEM768    @ default
{ 2.16.840.1.101.3.4.3.18, id-ml-dsa-65, ML-DSA-65, MLDSA65 } @ default
```
Khi cấp qua OQS (OpenSSL <3.5): hiện `X25519MLKEM768 @ oqsprovider`.

**Tạo CA ML-DSA + cert leaf** (ML-DSA là "pure" — **KHÔNG cần `-sigopt`/digest**; man page EVP_SIGNATURE-ML-DSA: *"the digest passed in mdname must be NULL"*):
```bash
openssl genpkey -algorithm ML-DSA-65 -out ml-dsa.key
# Root/CA self-signed (Red Hat RHEL 9/10 docs, Example 2.2):
openssl req -x509 -newkey mldsa65 -keyout ca.key -out ca.crt -days 30 -nodes -subj /CN=RootCA
# Leaf ký bởi CA ML-DSA (flow algorithm-agnostic):
openssl req -new -key leaf.key -out leaf.csr
openssl x509 -req -in leaf.csr -CA ca.crt -CAkey ca.key -CAcreateserial -days 365 -out leaf.crt
```
**Alias tên** (đều hợp lệ, tương đương): `ML-DSA-65`, `mldsa65`, `id-ml-dsa-65`, OID **2.16.840.1.101.3.4.3.18** (ML-DSA-44=.3.17, ML-DSA-87=.3.19). Lưu ý: hiện chưa public CA nào hỗ trợ PQC signature → chỉ dùng local CA/self-signed (Red Hat docs).

**Server qua MẠNG THẬT (Pi=server ↔ PC=client, KHÔNG localhost):**
```bash
# Trên Pi:
openssl s_server -cert server.crt -key server.key -groups X25519MLKEM768 -tls1_3 -accept 4433 -www
# Trên PC (thay <pi-ip>):
openssl s_client -connect <pi-ip>:4433 -groups X25519MLKEM768 -tls1_3 -CAfile root.crt
```
**Output kỳ vọng (verbatim — đã kiểm chứng Red Hat RHEL 9/10 docs + OpenSSL PR #25808):**
```
Negotiated TLS1.3 group: X25519MLKEM768
Peer signature type: mldsa65
New, TLSv1.3, Cipher is TLS_AES_256_GCM_SHA384
```
Dùng `-msg`/`-trace`/`-debug` để xem chi tiết handshake. **mTLS:** server thêm `-Verify 1`, client thêm `-cert client.crt -key client.key`.
⚠️ **Caveat tên hiển thị thay đổi theo build** (đối chiếu nguồn web ↔ cmd): bản **3.5.0-alpha** hiện `Peer signature type: id-ml-dsa-44` (GitHub issue #27115), bản **release** hiện `mldsa65`. Với hybrid group, một số build cũ **bỏ dòng `Server Temp Key:`** (GitHub issue #25888) → dựa vào dòng `Negotiated TLS1.3 group:` là tin cậy nhất.

### H.5 Wireshark/tshark
Filter: `tls.handshake.extensions_supported_group`, `tls.handshake.extensions_key_share_group`. Codepoint: **X25519MLKEM768 = 0x11EC (4588)**, X25519 = 0x001D (29), X25519Kyber768Draft00 = 0x6399, SecP256r1MLKEM768 = 0x11EB (4587). Wireshark cũ hiện "Unknown (0x11ec)". **Lưu ý:** `SSLKEYLOGFILE` với ML-KEM có thể chưa đầy đủ ở vài client.

### H.6 curl & Chrome (đối chiếu NGUỒN WEB vs CMD vs ỨNG DỤNG)
```bash
curl --curves X25519MLKEM768 https://crypto.cloudflare.com/cdn-cgi/trace   # kỳ vọng dòng kex=X25519MLKEM768
```
Image `openquantumsafe/curl` (x86_64 + arm64). Test interop: `test.openquantumsafe.org` (mỗi tổ hợp SIG/KEM một port riêng, dùng `--cacert oqs-testca.pem`). Chrome: DevTools → Security tab → *"Key exchange: X25519MLKEM768"*; `chrome://net-export` xuất NetLog.

### H.7 Docker trên Pi — phân biệt BẮT BUỘC vs TÙY CHỌN
- **BẮT BUỘC:** benchmark hiệu năng/năng lượng/memory chạy **NATIVE bare-metal** trên Pi (container overhead làm nhiễu).
- **TÙY CHỌN:** Docker để tái lập build, `buildx` multi-arch trên x86, interop test. Image dựng sẵn `openquantumsafe/curl` & `openquantumsafe/nginx` (x86_64 + arm64).
- Lỗi overlayfs trên Pi/VPS (`failed to mount overlay`) → fallback `fuse-overlayfs` hoặc storage-driver `vfs`.
- **oqs-demos** (Dockerfile nginx/curl/haproxy/httpd theo quy trình 3 tầng liboqs→OpenSSL3→oqs-provider, ARG `LIBOQS_TAG`/`OQSPROVIDER_TAG`/`SIG_ALG`) mang nhãn **"PARTIALLY SUPPORTED"**, một số demo **"Unmaintained"**.
- **pqm4** chỉ cho Cortex-M4 microcontroller — **KHÔNG cần cho Pi 4 Cortex-A72**.

## PHẦN I (MỚI) — Bảng "ai ký / ai verify / dùng khóa gì / thuật toán gì" cho TLS 1.3 hybrid

**Hybrid X25519MLKEM768 — directionality (đã kiểm chứng draft-ietf-tls-ecdhe-mlkem):**

| Bước | Bên thực hiện | Hành động | Kích thước |
|---|---|---|---|
| 1 | **Client** | Sinh KEM keypair (ML-KEM-768 encapsulation key) + X25519 ephemeral; gửi trong **ClientHello key_share** | **client share 1216 B** = 1184 (ML-KEM) + 32 (X25519) |
| 2 | **Server** | **Encaps** với encapsulation key của client → trả ciphertext + X25519 share trong **ServerHello key_share** | **server share 1120 B** = 1088 (ciphertext) + 32 (X25519) |
| 3 | **Client** | **Decaps** ciphertext | shared secret **64 B** (32+32, nối ML-KEM ‖ X25519) |

> **Nhấn mạnh:** directionality này **NGƯỢC với RSA key-transport TLS 1.2** — xưa client mã hóa pre-master bằng public key của server (key transport); nay client là bên sinh KEM key và server mới Encaps (KEM). (SecP256r1MLKEM768: client share 1249 B, server share 1153 B.)

**Authentication (ký/verify):**
- **CertificateVerify:** server **ký toàn bộ transcript** bằng ML-DSA private key ứng với cert; **client verify** bằng public key trong leaf cert (RFC 8446 §4.4.3: *"A signature over the entire handshake using the private key corresponding to the public key in the Certificate message"*). **mTLS:** client cũng gửi Certificate + CertificateVerify.
- **`signature_algorithms` vs `signature_algorithms_cert` (RFC 8446 §4.2.3):** `signature_algorithms` áp dụng cho chữ ký **CertificateVerify**; `signature_algorithms_cert` áp dụng cho chữ ký **trong chuỗi cert**. Nếu không có `_cert` thì `signature_algorithms` áp dụng cho cả hai.
- **Chuỗi cert (mỗi cấp ký bằng ML-DSA):** leaf ký bởi Intermediate CA (ML-DSA-65) → Intermediate ký bởi Root (ML-DSA-87) → Root self-signed.

| Vai trò | Khóa dùng | Thuật toán | Ký gì / Verify gì |
|---|---|---|---|
| Client | ephemeral X25519 + ML-KEM-768 | KEM + ECDHE | sinh enc key; verify cert chain + CertificateVerify của server |
| Server | leaf ML-DSA-65 key + ephemeral KEM/X25519 | ML-DSA + KEM | Encaps; ký CertificateVerify |
| Intermediate CA | ML-DSA-65 | ML-DSA | ký leaf cert |
| Root CA | ML-DSA-87 (offline) | ML-DSA | ký Intermediate, tự ký |

## PHẦN J (MỚI) — Tại sao PHẢI sửa "OpenSSL-OQS / ECDHE+Kyber"
1. **"OQS-OpenSSL fork"** (fork OpenSSL 1.1.1) đã **DEPRECATED, archived read-only 8/1/2025**; OpenSSL 1.1.1 EOL **11/9/2023**. Cách đúng: oqs-provider cho OpenSSL 3 HOẶC OpenSSL 3.5+/3.6+ native.
2. **"Kyber" → ML-KEM** (FIPS 203, 13/8/2024). ML-KEM final **KHÔNG tương thích** Kyber draft → Chrome đổi codepoint **0x6399 → 0x11EC** (Google: *"The changes to the final version of ML-KEM make it incompatible with the previously deployed version of Kyber"*).
3. **"ECDHE+Kyber" sai cả tên thuật toán lẫn cấu trúc.** Đúng là **hybrid composite X25519MLKEM768** (hoặc SecP256r1MLKEM768 / SecP384r1MLKEM1024) theo **draft-ietf-tls-ecdhe-mlkem**.
4. **Gọi "RFC" cho Kyber/Dilithium là SAI** — chúng là **FIPS standards + IETF Internet-Drafts** cho TLS. Số RFC đã kiểm chứng kỹ:
   - **RFC 9794** — PQ/T hybrid terminology (Informational, 6/2025).
   - **RFC 9881** — ML-DSA in X.509 & CRLs (Standards Track, 10/2025). **Sinh viên ghi RFC 9881 là ĐÚNG, không nhầm.**
   - **RFC 9964** — ML-DSA in JOSE/COSE (Standards Track, 5/2026).
   - **ML-DSA in TLS (draft-ietf-tls-mldsa)** vẫn là Internet-Draft (codepoint mldsa44/65/87) — **CHƯA** thành RFC.

## PHẦN K (MỚI) — OWASP Top 10:2025 A04 Cryptographic Failures
A04 (tụt từ #2 năm 2021 xuống **#4** năm 2025; vẫn là rủi ro nghiêm trọng). **CWE liên quan:** CWE-327 (Broken/Risky Crypto Algorithm), CWE-331 (Insufficient Entropy), CWE-338 (Weak PRNG), CWE-1241 (Predictable Algorithm in RNG). **"How to prevent" (trích OWASP):** mã hóa mọi dữ liệu in-transit bằng **TLS >=1.2 với forward secrecy**, **bỏ CBC ciphers**, **hỗ trợ quantum key exchange algorithms**, và *"You need to prepare now for post quantum cryptography (PQC) ... so that high risk systems are safe no later than the end of 2030"*; hash mật khẩu bằng Argon2/yescrypt/scrypt/PBKDF2-HMAC-SHA-512; HSTS; tránh FTP/STARTTLS không mã hóa.

**Bảng ánh xạ A04 → kiến trúc onion 3 lớp:**

| Yêu cầu A04 | Lớp onion | Biện pháp trong kiến trúc |
|---|---|---|
| Quantum key exchange | **Cryptography** | hybrid X25519MLKEM768 |
| Forward secrecy, bỏ CBC | **Cryptography** | TLS 1.3 ephemeral (EC)DHE, AEAD AES-GCM/ChaCha20 |
| Quản lý khóa / không lộ key | **Cryptography** | KMS/HSM envelope, key rotation |
| Xác thực mạnh | **Authentication** | mTLS, cert ML-DSA, WebAuthn/passkey |
| Phân quyền theo classification | **Authorization** | PDP/OPA, zero-trust, DPoP/PoP |

## PHẦN L (MỚI) — Threat model & CNSA 2.0
- **Harvest-now-decrypt-later (HNDL):** kẻ tấn công thu thập ciphertext hôm nay để giải mã sau khi có quantum computer → **KEM cấp bách hơn signature** (chữ ký bị phá trong tương lai không hồi tố làm lộ phiên cũ; nhưng confidentiality bị HNDL ngay bây giờ). Đây là cơ sở khoa học cho RQ3 và việc ưu tiên hybrid key exchange.
- **KEM vs Signature:** KEM = confidentiality; Signature = authentication / integrity / non-repudiation.
- **Forward secrecy:** TLS 1.3 ephemeral keyshare → phiên cũ an toàn dù long-term key lộ.
- **Downgrade protection:** handshake signature (CertificateVerify) ký **toàn transcript** gồm cả nhóm KEM đã chọn → chống tấn công hạ cấp nhóm.
- **Crypto-agility (hybrid):** an toàn nếu **một trong hai** thuật toán còn vững.
- **Side-channel/constant-time:** liboqs khuyến cáo implementation kháng side-channel.
- **Certificate lifecycle:** rotation, CRL/OCSP, short-lived cert, **Root CA offline**.
- **CNSA 2.0** (NSA, công bố 7/9/2022): mandate **ML-KEM-1024 + ML-DSA-87** cho National Security Systems. Timeline (web browser/server & cloud): *"Support and prefer CNSA 2.0 by 2025; exclusive use by 2033"*; software/firmware signing prefer 2025 / exclusive 2030; mục tiêu toàn bộ NSS quantum-resistant **2035** (NSM-10). Cổng mua sắm: từ **1/1/2027** mọi NSS mới phải hỗ trợ CNSA 2.0. **Lưu ý:** civilian default là ML-KEM-768, KHÔNG đạt CNSA 2.0 (vốn đòi 1024/87).

## PHẦN M (MỚI) — Bảng kích thước (FIPS 203/204 final, bytes)

| Scheme | Public key | Private key | CT / Sig | NIST level |
|---|---|---|---|---|
| ML-KEM-512 | 800 | 1632 (seed 64) | CT 768 | 1 |
| ML-KEM-768 | 1184 | 2400 (seed 64) | CT 1088 | 3 |
| ML-KEM-1024 | 1568 | 3168 (seed 64) | CT 1568 | 5 |
| ML-DSA-44 | 1312 | 2560 | Sig 2420 | 2 |
| ML-DSA-65 | 1952 | 4032 | Sig **3309** | 3 |
| ML-DSA-87 | 2592 | 4896 | Sig **4627** | 5 |
| RSA-3072 | ~387 | ~1.7K | Sig 384 | ~1 |
| ECDSA P-256 | 64 (33 nén) | 32 | Sig ~64–72 | ~1 |
| ECDSA P-384 | 96 | 48 | Sig ~96–104 | ~3 |
| Ed25519 | 32 | 32 | Sig 64 | ~1 |

Shared secret ML-KEM (mọi mức) = 32 B. **FIPS 204 ipd vs final** (dùng giá trị **final**): ML-DSA-65 signature 3293→**3309**; ML-DSA-87 4595→**4627**.
**Ý nghĩa cho RQ1:** ML-DSA-65 (pubkey 1952 + sig 3309 ≈ 5.3 KB) lớn gấp ~20–50 lần ECDSA P-256 → chính là lý do Google chọn MTC và prioritize KEM trước signature; chuỗi cert + SCT đầy đủ có thể đẩy overhead authentication lên >13–14 KB/handshake.

## PHẦN N (MỚI) — Ánh xạ phủ ĐẦY ĐỦ yêu cầu file 05

| Yêu cầu file 05 | Phủ ở đâu |
|---|---|
| **Mục tiêu 1** (toán lattice + vai trò Kyber/Dilithium) | WP1, Phần L, M |
| **Mục tiêu 2** (build + tích hợp liboqs/PQClean) | WP2, Phần H.2–H.3 |
| **Mục tiêu 3** (đo latency/throughput/memory/code size/energy) | WP2/WP5/WP6, methodology |
| **Mục tiêu 4** (tối ưu compiler flags/assembly/NEON + đánh giá) | WP3, Phần H.2 (2 cây đối chứng) |
| **Mục tiêu 5** (so sánh an toàn/chi phí/hiệu năng + kết luận + khuyến nghị) | WP7, Phần M, Recommendations |
| **RQ1** (overhead tính toán & băng thông, yếu tố nào ảnh hưởng) | microbenchmark + bảng M + Phần I (kích thước handshake) |
| **RQ2** (NEON + compile flags làm PQC khả thi trên Pi 4 cho TLS?) | H.1/H.2/WP3/WP4 |
| **RQ3** (hybrid ECDHE+Kyber overhead chấp nhận được?) | Phần I + WP4 macrobenchmark + Phần L (HNDL) |
| **Methodology** | clock_gettime(CLOCK_MONOTONIC_RAW); microbench N=1000–10000; macrobench TLS 1.3 latency/throughput; memory & code size (`size`, `readelf -S`, peak RSS qua `/usr/bin/time -v`, `pmap`); energy (Monsoon, INA219/INA226); thống kê: warm-up loại bỏ, K=5–10 batch × M iter, median-of-medians, 95% CI bootstrap, paired comparison; integration TLS 1.3 classical/PQC/hybrid (`s_server`/`s_client`, HTTPS server) |
| **Repeatability** | pin CPU freq (tắt turbo), isolate core, multiple batches, warm-up |
| **Deliverables** | mid-term/final report, code repo (build scripts, benchmark harness, TLS integration scripts, Dockerfile), artifacts (raw CSV, plots, binary sizes), demo |
| **Rubric** | Research & literature 20% (Phần E,J,nguồn); Correctness & reproducibility 25% (KAT/ACVP, H.4); Quality & rigor 30% (methodology, 2 cây đối chứng); Analysis 15% (M,L,Recs); Presentation 10% |
| **Repo structure** | build/, benchmarks/{micro,tls,energy}/, scripts/, docker/, docs/, tools/ |
| **Yêu cầu file 08** | mạng thật (Pi=server↔PC=client, KHÔNG localhost — H.4); onion 3 lớp (Auth/Authz/Crypto — K) + Firewall/IDS-IPS vòng ngoài (sơ đồ) |

## Sơ đồ kiến trúc (ASCII) — node, ký/verify, luồng dữ liệu, onion 3 lớp + vòng ngoài
```
┌──────────────────────── Firewall / IDS-IPS (vòng ngoài) ────────────────────────┐
│  ┌────────────── Lớp 1: AUTHENTICATION (mTLS, ML-DSA cert, WebAuthn) ──────────┐  │
│  │  ┌──────────── Lớp 2: AUTHORIZATION (PDP/OPA, zero-trust, DPoP) ─────────┐  │  │
│  │  │  ┌────────── Lớp 3: CRYPTOGRAPHY (X25519MLKEM768, AES-GCM, KMS) ────┐ │  │  │
│  │  │  │                                                                 │ │  │  │
│  │  │  │   PC (Client)                         Pi (N1 Server, arm64)     │ │  │  │
│  │  │  │   ──ClientHello key_share[1216B]────────────►  (Encaps ML-KEM)  │ │  │  │
│  │  │  │   ◄─ServerHello key_share[1120B] + Cert(ML-DSA-65) + CertVerify │ │  │  │
│  │  │  │     (Decaps) verify chain ◄── N6 CA/PKI ký leaf  ◄── Root(ML-DSA-87)│ │  │
│  │  │  │   ──[mTLS] Cert + CertVerify (ML-DSA)────────►  (-Verify 1)      │ │  │  │
│  │  │  └─────────────────────────────────────────────────────────────────┘ │  │  │
│  │  │      N3 IdP (ký token RFC9964) │ N4 PDP/OPA │ N5 Backend │ N7 KMS/HSM   │  │  │
│  │  └────────────────────────────────────────────────────────────────────────┘  │  │
│  └──────────────────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

## Recommendations (staged, có benchmark/threshold để đổi quyết định)

**Giai đoạn 1 — Nền tảng (tuần 1–3, đạt critical path WP1→WP2):**
1. Build OpenSSL 3.6.2 vào `/opt/openssl-3.6.2` (không ghi đè hệ thống); xác minh `openssl list -kem-algorithms` thấy `X25519MLKEM768 @ default`.
2. Build **2 cây liboqs** (reference vs NEON-optimized) native trên Pi; chạy KAT/ACVP (FIPS 203/204) trước khi đo — **nếu KAT fail thì DỪNG**, không đo tiếp (correctness là tiền đề của rubric 25%).
3. Microbenchmark `speed_kem`/`speed_sig`, N≥1000, báo cáo **ns** là chính.

**Giai đoạn 2 — Tối ưu & TLS (tuần 4–6, mid-term):**
4. Đo tác động NEON: nếu speedup NEON cho ML-KEM-768 keygen/encaps **< 1.15×** trên Cortex-A72 thì kiểm tra lại cờ (`OQS_OPT_TARGET=auto`, `OQS_USE_ARM_SHA3_INSTRUCTIONS`) — A72 không có SHA3 hardware nên NEON keccak là nguồn lợi chính.
5. Chạy TLS 1.3 qua **mạng thật** (H.4); xác nhận `Negotiated TLS1.3 group: X25519MLKEM768` + `Peer signature type: mldsa65`.
6. **Threshold RQ2/RQ3:** nếu hybrid handshake median latency tăng **< ~10–20%** so với classical (P-256+ECDSA) trên Pi qua mạng thật → kết luận PQC/hybrid **khả thi** cho SBC TLS. Nếu tăng > ~50% → khuyến nghị giới hạn ML-KEM-768 (không 1024) cho edge và phân tích nguyên nhân (param length vs assembly vs NEON).

**Giai đoạn 3 — Phân tích & báo cáo (tuần 7+, WP7):**
7. So sánh PQC vs RSA/ECC ở mức bảo mật tương đương (bảng M); tách rõ overhead **tính toán** vs **băng thông** (handshake bytes ở Phần I).
8. Khuyến nghị kiến trúc: dùng **hybrid X25519MLKEM768** (mitigate HNDL ngay), giữ classical cert ML-DSA cho private PKI; nêu MTC như hướng tương lai cho public web.

**Yếu tố sẽ thay đổi khuyến nghị:** (a) nếu repo cần Cat 5 → chuyển ML-KEM-1024/ML-DSA-87 (CNSA 2.0), chấp nhận handshake lớn hơn; (b) nếu cần FIPS-compliance thực → **không** dùng default provider PQC hiện tại (xem Caveats); (c) nếu đo energy bằng INA219 cho thấy ML-DSA-87 sign vượt ngân sách năng lượng edge → hạ xuống ML-DSA-65.

## Caveats
- **liboqs / oqs-demos là PROTOTYPE, KHÔNG production.** liboqs README: *"caution is advised when deploying quantum-safe algorithms"*; oqs-demos nhãn **"PARTIALLY SUPPORTED"**, vài demo **"Unmaintained"**; OQS-OpenSSL fork **archived read-only 8/1/2025**.
- **PQC trong default provider, KHÔNG trong FIPS provider** của OpenSSL 3.5 — không được khẳng định FIPS-compliance trong báo cáo. (OpenSSL note: JITTER seed source ngoài FIPS validation; PQC native ở default provider.)
- **draft vs RFC:** hybrid TLS (draft-ietf-tls-ecdhe-mlkem) và ML-DSA-in-TLS (draft-ietf-tls-mldsa) **vẫn là Internet-Draft**; chỉ X.509 (RFC 9881), JOSE/COSE (RFC 9964), terminology (RFC 9794) là RFC final.
- **FIPS 204 ipd vs final:** dùng giá trị **final** (ML-DSA-65 sig 3309, ML-DSA-87 sig 4627), không dùng giá trị ipd.
- **Tên hiển thị ML-DSA thay đổi theo build OpenSSL** (id-ml-dsa-44 ở alpha vs mldsa44/65 ở release) — đối chiếu đúng version build khi viết báo cáo.
- **Cycle count "thật"** chỉ có khi bật PMU (`OQS_SPEED_USE_ARM_PMU` + kernel module); mặc định liboqs trên ARM dùng `clock_gettime` (ns).
- **Số liệu Cloudflare 52%** là human-generated web traffic tới mạng Cloudflare (Radar 2025 Year in Review, kỳ 1/1–2/12/2025), không phải toàn Internet.
- Một số chi tiết AWS (tên group "SecP256r1MLKEM768" cụ thể trong tài liệu KMS/ACM) chỉ xác nhận được dạng "ML-KEM-768 + x25519 hybrid", không verbatim — nêu thận trọng.

## Nguồn chính (kèm tóm tắt paraphrase)
- **RFC 8446** (datatracker.ietf.org/doc/html/rfc8446) — TLS 1.3; §4.2.3 phân biệt signature_algorithms vs signature_algorithms_cert; §4.4.3 CertificateVerify ký toàn transcript.
- **RFC 9794** (rfc-editor.org/info/rfc9794) — thuật ngữ PQ/T hybrid, NCSC + Naval Postgraduate School, 6/2025.
- **RFC 9881** (rfc-editor.org/rfc/rfc9881.html) — ML-DSA in X.509 & CRL, Standards Track 10/2025; OID id-ml-dsa-44/65/87 = .3.17/.18/.19.
- **RFC 9964** (datatracker.ietf.org/doc/rfc9964) — ML-DSA in JOSE/COSE, 5/2026.
- **draft-ietf-tls-ecdhe-mlkem** (datatracker.ietf.org/doc/draft-ietf-tls-ecdhe-mlkem) — X25519MLKEM768=0x11EC; client share 1216 B, server share 1120 B, shared secret 64 B.
- **draft-ietf-tls-mlkem** — kích thước ML-KEM (enc key 800/1184/1568; CT 768/1088/1568).
- **FIPS 203 / FIPS 204** (nvlpubs.nist.gov) — chuẩn ML-KEM / ML-DSA final.
- **OWASP Top 10:2025 A04** (owasp.org/Top10/2025/A04_2025-Cryptographic_Failures) — chuẩn bị PQC trước cuối 2030, TLS≥1.2 FS, bỏ CBC, quantum key exchange; CWE-327/331/338/1241.
- **Cloudflare Radar 2025 Year in Review** (blog.cloudflare.com/radar-2025-year-in-review) — 52% post-quantum (từ 29%).
- **Apple "iMessage with PQ3"** (security.apple.com/blog/imessage-pq3) — Level 3, rekey ~50 messages/≤7 ngày, Kyber-768.
- **Chrome / Google Security Blog, The Hacker News, BleepingComputer** — codepoint 0x6399→0x11EC, Chrome 131; Merkle Tree Certificates (blog.google/security/cultivating-a-robust-and-efficient-quantum-safe-https).
- **docs.openssl.org 3.5/3.6** — man pages openssl-genpkey, openssl-list, EVP_PKEY-ML-KEM, EVP_SIGNATURE-ML-DSA (pure, mdname=NULL).
- **Red Hat RHEL 9/10 "Securing networks"** — Example tạo cert mldsa65 + output `Peer signature type: mldsa65`, `Negotiated TLS1.3 group: X25519MLKEM768`.
- **github.com/open-quantum-safe** (liboqs CONFIGURE.md, oqs-provider README/releases, openssl archived) — cờ build, oqs-provider tắt PQC native khi OpenSSL≥3.5, fork archived 8/1/2025.
- **NSA CNSA 2.0** (media.defense.gov) — ML-KEM-1024 + ML-DSA-87, timeline 2025/2030/2033, NSS 2035.
- **AWS Security Blog** — AWS-LC FIPS 3.0 (ML-KEM trong FIPS 140-3 validation), KMS/ACM/Secrets Manager ML-KEM hybrid, Private CA ML-DSA.