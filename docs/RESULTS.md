# RESULTS — Số liệu & Trả lời câu hỏi nghiên cứu (NT219)

> Tất cả số **x86 là ĐO THẬT** (i5-12450HX). Số **ARM/energy/netem là MÔ HÌNH** (ghi rõ).
> Chi tiết đo-thật-vs-mô-hình: [`DATA_PROVENANCE.md`](DATA_PROVENANCE.md). Biểu đồ: `report/figures/*.svg`.
> Phương pháp: single-shot/lần × 100 mẫu/mức (rdtsc cycles + `CLOCK_MONOTONIC_RAW` ns), ghim core 0,
> cổng đúng đắn (KAT 9/9 PASS) trước khi tin số đo. So sánh theo **mức bảo mật NIST khớp nhau**.

## 1. Microbenchmark — PQC reference-C (PQClean, x86, median ns/µs)

| Scheme | Mức (Cat) | Keygen | Encaps/Sign | Decaps/Verify | pk (B) | ct/sig (B) |
|---|---|--:|--:|--:|--:|--:|
| ML-KEM-512 | 512 (Cat1) | 34.2 µs | 35.9 µs | 42.1 µs | 800 | 768 |
| ML-KEM-768 | 768 (Cat3) | 58.3 µs | 61.8 µs | 72.0 µs | 1184 | 1088 |
| ML-KEM-1024 | 1024 (Cat5) | 82.4 µs | 83.0 µs | 93.7 µs | 1568 | 1568 |
| ML-DSA-44 | 44 (Cat2) | 89.9 µs | **314.8 µs** (sign) | 82.6 µs | 1312 | 2420 |
| ML-DSA-65 | 65 (Cat3) | 167.5 µs | **453.6 µs** (sign) | 138.6 µs | 1952 | 3309 |
| ML-DSA-87 | 87 (Cat5) | 267.2 µs | **562.9 µs** (sign) | 239.0 µs | 2592 | 4627 |
| AES-256-GCM | 256 (Cat5) | — | 7.3 µs (enc) | 2.4 µs (dec) | key 32 | tag 16 |

> ML-DSA **Sign lệch phải** (rejection sampling): ML-DSA-87 median 562.9 µs nhưng **p95 ≈ 1.53 ms**,
> max ≈ 2.25 ms (xem `fig6_mldsa87_sign_dist.svg`). → dùng median + p95, KHÔNG chỉ mean.

## 2. Bản tối ưu AVX2 (OpenSSL 3.6.1 native, x86) vs reference-C — RQ1

| Op (ML-KEM-1024) | reference-C | AVX2 | Tăng tốc |
|---|--:|--:|--:|
| Keygen | 82.4 µs | 46.3 µs | **1.78×** |
| Encaps | 83.0 µs | 22.3 µs | **3.72×** |
| Decaps | 93.7 µs | 34.9 µs | **2.68×** |

→ Tối ưu vector hoá (AVX2 trên x86; NEON trên ARM) là **yếu tố ảnh hưởng lớn nhất** đến hiệu năng
PQC. Bản reference-C chính là "đáy" cần tối ưu (xem `fig4`).

## 3. Cổ điển (baseline, x86 tối ưu) — so ở Cat 5

| Thuật toán | Keygen | Sign/Derive | Verify | pk (B) | sig/ct (B) |
|---|--:|--:|--:|--:|--:|
| ECDH-P-521 | 1.30 ms | 1.28 ms (derive) | — | 133 | 66 |
| ECDSA-P-521 | 1.29 ms | 1.36 ms | 1.10 ms | 133 | 139 |
| RSA-15360 | **15.7 s** (!) | **177 ms** | 0.77 ms | 1958 | 1920 |
| RSA-3072 (Cat1) | 70 ms | 1.55 ms | 36 µs | 422 | 384 |

## 4. So sánh đầu cuối PQC vs cổ điển (Cat 5) — điểm nhấn

| Trục | PQC | Cổ điển | Kết luận |
|---|--:|--:|---|
| **KEM** (encaps+decaps) | ML-KEM-1024 **57 µs** | ECDH-P-521 1284 µs | PQC **~22× NHANH hơn** |
| **Sign** | ML-DSA-87 841 µs | RSA-15360 177 ms | PQC **~210× nhanh hơn** RSA |
| **Sign** | ML-DSA-87 841 µs | ECDSA-P-521 1358 µs | PQC ~1.6× nhanh hơn ECDSA |
| **Keygen (sig)** | ML-DSA-87 200 µs | RSA-15360 15.7 s | PQC **~78 000× nhanh hơn** |
| **Verify** | ML-DSA-87 215 µs | ECDSA-P-521 1103 µs | PQC ~5× nhanh hơn |
| **Byte chữ ký** | ML-DSA-87 **4627 B** | ECDSA-P-521 139 B | PQC **33× LỚN hơn** |
| **Byte KEM ct** | ML-KEM-1024 **1568 B** | ECDH-P-521 66 B | PQC **24× lớn hơn** |

→ **Trade-off cốt lõi:** PQC ở Cat 5 **rẻ hơn nhiều về tính toán** (đặc biệt so RSA-15360 vốn bất khả
thi) nhưng **đắt hơn nhiều về băng thông** (khóa/chữ ký lớn). Đây là chi phí thật của migration.

## 5. Tài nguyên (x86, đo thật) + Năng lượng (mô hình)

- **Code size (.text):** ML-KEM ~9.5–10.7 KB, ML-DSA ~16–16.7 KB (PQClean reference). `data/resource/code_size.txt`.
- **Peak RSS:** ML-KEM 3.66 MB · ML-DSA 3.74 MB · AES 7.56 MB. `data/resource/peak_rss.txt`.
- **Năng lượng (MÔ HÌNH, power model):** `data/resource/energy_estimate.csv` — vd ML-KEM-1024 keygen
  ≈ 1.2 µJ (x86, 15 W) / ≈ 1.5 µJ (ARM mô hình, 4 W). KHÔNG đo bằng đồng hồ vật lý (Limitations).

## 6. ARM Cortex-A72 (MÔ HÌNH) + NEON — RQ2

> MÔ HÌNH: `arm_ns = x86ref × 4.6` (lattice) / `×22` (AES). NEON tăng tốc từ `neon_vs_ref.csv`.

| Op (Cat5, ARM mô hình) | reference-C | + NEON | Ghi chú |
|---|--:|--:|---|
| ML-KEM-1024 Keygen | ~379 µs | ~172 µs | NEON 2.2× |
| ML-KEM-1024 Encaps | ~382 µs | ~182 µs | NEON 2.1× |
| ML-DSA-87 Sign | ~2.70 ms | ~1.59 ms | NEON 1.7× |
| ML-DSA-87 Verify | ~1.08 ms | ~0.60 ms | NEON 1.8× |

→ **RQ2:** ngay cả reference-C trên Pi 4 vẫn ở mức **vài trăm µs–vài ms** mỗi thao tác; **NEON kéo
về ~½**, đủ khả thi cho TLS trên SBC (hàng trăm handshake/giây). AES không có crypto-extension trên
BCM2711 nên chậm (~22×) — nên ưu tiên ChaCha20 hoặc bật AES phần mềm tối ưu trên Pi.

## 7. TLS 1.3 PQC qua OpenSSL 3.6.1 native (x86, ĐO THẬT) — RQ3

```
PQC-hybrid : Negotiated TLS1.3 group: X25519MLKEM768 · Peer signature: mldsa65 · TLS_AES_256_GCM_SHA384
classical  : groups P-256 · Peer signature: ecdsa_secp256r1_sha256
```
| Cấu hình | handshakes/giây | cert (B) |
|---|--:|--:|
| Cổ điển (ECDHE-P256 + ECDSA cert) | 2118 | 534 |
| **PQC-hybrid (X25519MLKEM768 + ML-DSA-65)** | 1934 | 7464 (**14×**) |

→ **RQ3:** overhead throughput của hybrid handshake chỉ **~8.7%** trên cùng máy — **chấp nhận được**
cho server. Chi phí chính là **kích thước** (cert PQC 14× lớn) → ảnh hưởng khi mạng kém/MTU nhỏ. Ma
trận netem (MÔ HÌNH, `netem_matrix.csv`, `fig8`): dưới mất gói cao, handshake PQC nhạy hơn do nhiều
gói hơn.

## 8. Trả lời câu hỏi nghiên cứu (tóm tắt)

- **RQ1 — Chi phí PQC vs RSA/ECC & yếu tố ảnh hưởng:** Về *tính toán*, PQC ở Cat 5 **rẻ hơn** cổ điển
  (KEM 22× nhanh hơn ECDH-P521; sign 210× nhanh hơn RSA-15360). Về *băng thông*, PQC **đắt hơn**
  (sig 33×, ct 24×). Yếu tố ảnh hưởng lớn nhất: **mức tối ưu hoá** (AVX2/NEON cho 1.8–3.8×) > tham số.
- **RQ2 — NEON khả thi hoá PQC trên Pi?** Có. Reference-C trên A72 ~ vài trăm µs–ms; NEON kéo ~½ →
  đủ cho TLS server-class trên SBC. (Số ARM là mô hình; cần chạy lại trên Pi để chốt.)
- **RQ3 — Hybrid handshake overhead?** ~8.7% throughput trên server — chấp nhận được; chi phí thật
  nằm ở **kích thước** chứng chỉ/khóa, đáng lưu ý cho mạng hạn chế.

## 9. Đối chiếu invariants (tóm tắt — chi tiết ở `EVAL.md`)
- **I1/I3 (bí mật + toàn vẹn dữ liệu):** AES-256-GCM roundtrip OK + **tag hỏng bị từ chối** (KAT PASS).
- **I2 (chống giả mạo):** ML-DSA verify(chữ ký hỏng) = FAIL (KAT PASS) → E-C3 đạt.
- **I4 (AuthN):** cert ML-DSA-65 thương lượng thật trong TLS 1.3 (handshake_x86.txt).
- **I7 (kháng lượng tử / HNDL):** KEX hybrid X25519MLKEM768 — an toàn nếu MỘT trong hai thành phần còn vững.
