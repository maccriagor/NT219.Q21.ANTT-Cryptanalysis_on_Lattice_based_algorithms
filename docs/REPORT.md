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

## 5. Kết quả (đo thật x86) — chi tiết: [`RESULTS.md`](RESULTS.md)

**WP2 — Microbenchmark đủ 3 mức NIST (ĐO THẬT trên x86, 100 mẫu/mức):**
- Nguồn: 3 file tách theo thuật toán `benchmark_mlkem/` (512/768/1024), `benchmark_mldsa/`
  (44/65/87), `benchmark_aes/` (128/192/256) — PQClean reference-C, biên dịch native (rdtsc+ns,
  ghim core 0). CSV: `data/micro/x86/{mlkem,mldsa,aes}_x86.csv` (300 dòng/file). Bản tối ưu AVX2:
  `data/micro/x86/micro_evp_x86.csv`. Tổng hợp: `data/micro/summary_x86.csv`.
- **Kích thước khớp FIPS 203/204** (KAT 9/9 PASS — `data/kat/kat_results.txt`).
- Số liệu chính (Cat 5, median): ML-KEM-1024 K/E/D = 82/83/94 µs (reference) → 46/22/35 µs (AVX2);
  ML-DSA-87 K/S/V = 267/**563**/239 µs (sign p95 ≈ 1.53 ms — **rejection sampling**, `fig6`).
- **So sánh:** ML-KEM-1024 KEM nhanh hơn ECDH-P-521 **~22×**; ML-DSA-87 sign nhanh hơn RSA-15360
  **~210×** (RSA-15360 keygen = **15.7 s**!); nhưng chữ ký ML-DSA-87 lớn hơn ECDSA **33×**.

**WP4 — TLS 1.3 hậu lượng tử (ĐO THẬT, OpenSSL 3.6.1 native, localhost):**
```
Negotiated TLS1.3 group: X25519MLKEM768      <- KEX hybrid PQC
Peer signature type: mldsa65                 <- cert ký ML-DSA-65
Protocol: TLSv1.3   Cipher: TLS_AES_256_GCM_SHA384
```
→ 1934 handshake/s (hybrid) vs 2118 (cổ điển) = **~8.7% overhead**; cert ML-DSA-65 7464 B vs ECDSA
534 B (**14×**). Bằng chứng: `data/tls/x86/handshake_x86.txt`.

**WP2-ARM / WP3 NEON / WP6 năng lượng / WP4 netem — MÔ HÌNH** (Pi/đo-năng-lượng không truy cập được
trong phiên này): `data/micro/arm/*`, `data/micro/neon_vs_ref.csv`, `data/resource/energy_estimate.csv`,
`data/tls/netem_matrix.csv`. Phân định đo-thật-vs-mô-hình: [`DATA_PROVENANCE.md`](DATA_PROVENANCE.md).

**Biểu đồ (WP7a):** `python3 tools/plot.py` → `docs/report/figures/*.svg` (8 hình SVG: latency theo
mức, PQC vs cổ điển log-scale, reference vs AVX2, sizes, phân phối ML-DSA sign, x86 vs ARM, netem).

**Trả lời RQ1/RQ2/RQ3 + đối chiếu invariants:** xem [`RESULTS.md`](RESULTS.md) §8–9 và
[`EVAL.md`](EVAL.md). **Threat model / crypto risk:** [`THREAT_MODEL.md`](THREAT_MODEL.md).

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
