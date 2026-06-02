# Báo cáo triển khai — PQC (ML-KEM/ML-DSA) trên môi trường thực

**Đề tài:** Implement & Benchmark lattice-based schemes (ML-KEM, ML-DSA) + so sánh RSA/ECC; demo TLS 1.3 hậu lượng tử bằng Apache trên x86 và ARM.

---

## 1. Project goals (Mục tiêu)

1. Triển khai và **đo hiệu năng** ML-KEM (FIPS 203) và ML-DSA (FIPS 204) so với RSA/ECDSA/ECDH trên **x86_64** và **ARM (Cortex-A72)**.
2. Tích hợp PQC vào **dịch vụ thực tế (Apache httpd + TLS 1.3)** dùng nhóm hybrid **X25519MLKEM768** và chứng chỉ **ML-DSA**.
3. Đảm bảo **tái lập** (Docker + nguồn vendored, pin phiên bản) và **đúng đắn** (verify trước khi đo).
4. Bám **chuẩn quốc tế** và đường **production thật** (OpenSSL 3.6.2 native), tránh thư viện đã deprecated.

## 2. Security risks (Rủi ro bảo mật & Threat model)

| Rủi ro | Mô tả | Giảm thiểu |
|---|---|---|
| **Harvest-now, decrypt-later** | Kẻ địch thu ciphertext hôm nay, giải mã khi có máy lượng tử | KEX hybrid **X25519MLKEM768** (bảo mật ngay cả khi 1 thành phần bị phá) |
| **Shor** phá RSA/ECC | Máy lượng tử phá nhân tử/logarit rời rạc | Chữ ký **ML-DSA**, KEM **ML-KEM** (dựa Module-LWE/SIS) |
| **Grover** giảm 1/2 đối xứng | √ tốc độ tìm khóa đối xứng | Mức Category 5 = AES-256; chọn tham số tương ứng |
| **Downgrade / cấu hình sai** | Ép về nhóm/cipher yếu | `SSLProtocol -all +TLSv1.3`; chỉ bật nhóm hybrid + x25519 |
| **Dùng lib lỗi thời** | OQS-OpenSSL fork đã deprecated/archived | Dùng **OpenSSL 3.6.2 native** (ML-KEM/ML-DSA built-in) |

## 3. Solution architecture (Kiến trúc giải pháp — Onion 3 lớp trong)

```
   Client  ──TLS1.3 hybrid (X25519MLKEM768)──►  Apache httpd (mod_ssl + OpenSSL 3.6.2)  ──►  Asset
                                                  └─ Cert ký ML-DSA-65
   Lớp Cryptography : ML-KEM (KEX hybrid) + AES-GCM (TLS1.3) + ML-DSA (chữ ký/cert)
   Lớp Authentication: chứng chỉ máy chủ ML-DSA (xác thực server)
   Lớp Authorization : (mở rộng) ràng buộc truy cập tài nguyên trong httpd
```

- **Lớp Cryptography (lõi):** OpenSSL 3.6.2 cung cấp ML-KEM/ML-DSA **native**; nhóm TLS hybrid `X25519MLKEM768` (IETF draft-ietf-tls-ecdhe-mlkem); chữ ký chứng chỉ **ML-DSA-65** (FIPS 204).
- **Đo lường lớp Crypto:** hàm đo tự viết `benchmarks/micro/pqc_bench.cpp` (liboqs + OpenSSL EVP) + đối chứng `PQClean` (reference) và liboqs (NEON).
- **Hai nền tảng:** cùng một image Docker build native trên x86 và ARM.

## 4. Deployment plan (Phương án triển khai)

| Thành phần | Công cụ | Pin version |
|---|---|---|
| Môi trường build | Docker (`docker/Dockerfile.pqc`), nguồn vendored | OpenSSL 3.6.2 · liboqs 0.12.0 · PQClean (ml-kem/ml-dsa) |
| Đo vi mô | `benchmarks/micro/pqc_bench.cpp` chạy trong container | N=1000, warm-up=100, median/p95 |
| Demo TLS | Apache httpd link OpenSSL 3.6.2 (`docker/Dockerfile.apache`) | httpd 2.4.62 |
| Orchestration | `scripts/run_all.sh` (1 lệnh) | — |

**Xử lý môi trường mạng hạn chế (Pi IPv6-only, container chặn HTTPS):** nguồn upstream được **prefetch trên host** (`scripts/fetch_sources.sh`) vào `vendor/`, Dockerfile **COPY** thay vì tải trong container → build không phụ thuộc HTTPS.

## 5. Demo results (Kết quả demo — đã thực thi)

**WP1-ARM (verified):** image `pqc:wp1-arm64` (604MB) — OpenSSL **3.6.2**, ML-KEM-512/768/1024 + ML-DSA-44/65/87 **native**, liboqs 0.12.0, PQClean (ml-kem/ml-dsa).

**WP4-ARM — TLS 1.3 handshake hậu lượng tử trên Raspberry Pi 4 (THẬT):**
```
Negotiated TLS1.3 group: X25519MLKEM768      <- KEX hybrid PQC
Peer signature type: mldsa65                 <- cert ký ML-DSA-65
Protocol: TLSv1.3   Cipher: TLS_AES_256_GCM_SHA384
```
→ Trao khóa hybrid hậu lượng tử + xác thực server bằng chữ ký hậu lượng tử, trên phần cứng ARM thực. (`data/results/pqc_handshake_arm.txt`)

**WP2 — Microbenchmark:** `data/results/micro_x86.csv` (Windows MinGW) + `micro_arm.csv` (Pi).
Cột: `category,algo,suite,op,count,median_ns,ci95lo_ns,ci95hi_ns,mean_ns,stddev_ns,p95_ns,min_ns,pk_bytes,sk_bytes,ct_or_sig_bytes`.
- Kích thước **khớp FIPS 203/204** (validate): ML-KEM 512/768/1024 = pk 800/1184/1568, ct 768/1088/1568; ML-DSA-65 = pk 1952, sig 3309.
- Phát hiện: ML-DSA sign biến thiên cao (rejection sampling — paper Dilithium); RSA keygen mức cao cực chậm (RSA-7680 ~18s).

**Biểu đồ (WP7a):** `python3 tools/plot.py` → `docs/figs/` (latency, sizes, x86 vs ARM) + bảng overhead theo Category.

## 6. Standards compliance (Chuẩn quốc tế)

- **NIST FIPS 203** (ML-KEM), **FIPS 204** (ML-DSA).
- **IETF** draft-ietf-tls-ecdhe-mlkem (X25519MLKEM768), draft-ietf-tls-hybrid-design, RFC 8446 (TLS 1.3).
- **OpenSSL 3.6.2** (native ML-KEM/ML-DSA) — bản mới nhất, đã vá CVE.
- Đã **bỏ** OQS-OpenSSL fork (archived/deprecated) — đúng khuyến nghị upstream.

## 7. How to run (Cách chạy — bạn chỉ chạy script)

```bash
# Trên Pi (ARM) qua SSH, hoặc trên WSL2 (x86) — trong thư mục repo:
bash scripts/run_all.sh
# Tùy chọn số vòng đo: N=5000 WARM=200 bash scripts/run_all.sh
```
Pipeline: build image → ghi env → verify → microbenchmark → demo TLS/Apache. Kết quả trong `data/`.

> Lần đầu trên host mạng hạn chế: nếu `vendor/` trống, `run_all.sh` tự gọi `fetch_sources.sh` (cần host có mạng).
