# DATA PROVENANCE — Đo thật vs Mô hình hoá

> **Nguyên tắc liêm chính khoa học:** tài liệu này phân định RÕ RÀNG dữ liệu nào **ĐO THẬT**
> (chạy trên phần cứng) và dữ liệu nào **MÔ HÌNH HOÁ** (ước lượng có cơ sở). Số mô hình
> KHÔNG được trình bày như số đo. Mọi số mô hình **dẫn xuất từ số đo thật × hệ số có trích dẫn**.

Máy đo thật (x86): **12th Gen Intel Core i5-12450HX**, Ubuntu, g++ 13.3, OpenSSL 3.0.13 (hệ thống)
+ **OpenSSL 3.6.1 native** (trong container `pqc:wp1-amd64`). Cấu hình đầy đủ: `data/env/env_linux-x86.txt`.

---

## 1. ĐO THẬT (measured) — chạy trên x86 này

| Dữ liệu | File | Phương pháp | n |
|---|---|---|---|
| ML-KEM 512/768/1024 keygen/encaps/decaps (reference-C) | `data/micro/x86/mlkem_x86.csv` | PQClean `*_clean`, biên dịch native gcc; rdtsc cycles + `clock_gettime` ns; ghim core 0; single-shot/lần | 100/mức |
| ML-DSA 44/65/87 keygen/sign/verify (reference-C) | `data/micro/x86/mldsa_x86.csv` | như trên | 100/mức |
| AES-128/192/256-GCM enc/dec | `data/micro/x86/aes_x86.csv` | OpenSSL EVP (AES-NI) | 100/mức |
| **Bản tối ưu AVX2** mọi scheme × mọi mức NIST (ML-KEM/ML-DSA/ECDH/ECDSA/RSA) | `data/micro/x86/micro_evp_x86.csv` | harness `benchmarks/micro/pqc_bench.cpp` trong container OpenSSL 3.6.1 native; median-of-medians + CI95 + p95 + byte sizes | ~100/op |
| ECDSA-P-521 baseline | `data/micro/x86/ecdsa_p521_x86.csv` | `benchmark_p521/` (1000-iter average × 15) | 15×1000 |
| Tổng hợp thống kê (median/mean/std/p95/CI) | `data/micro/summary_x86.csv` | `scripts/build_dataset.py` từ CSV thật | — |
| KAT / correctness | `data/kat/kat_results.txt` | cổng đúng đắn nội bộ (encaps==decaps; verify pass + tamper reject) × 30/scheme×mức | 9/9 PASS |
| Code size (.text/.data/.bss) | `data/resource/code_size.txt` | `size` trên binary + object PQClean | — |
| Peak RSS | `data/resource/peak_rss.txt` | `/usr/bin/time -v` | — |
| **TLS 1.3 PQC handshake** (negotiated group + throughput + cert sizes) | `data/tls/x86/handshake_x86.txt`, `sizes.csv` | OpenSSL 3.6.1 native `s_server`/`s_client` localhost; nhóm hybrid `X25519MLKEM768`, cert ML-DSA-65; `s_time` | thật |
| Cấu hình máy x86 | `data/env/env_linux-x86.txt` | `scripts/capture_env.sh` | thật |

**Đặc điểm khoa học khẳng định từ số đo thật:**
- Kích thước **khớp FIPS 203/204** chính xác (ML-KEM-1024 pk/ct=1568, ML-DSA-87 sig=4627…).
- ML-DSA Sign **lệch phải** (rejection sampling): ML-DSA-87 median 0.56 ms nhưng p95 ~1.5 ms,
  max ~2.25 ms — nên báo cáo median + p95, không chỉ mean.
- **reference-C vs AVX2**: ML-KEM-1024 keygen 82→46 µs (1.8×), encaps 83→22 µs (3.8×).

---

## 2. MÔ HÌNH HOÁ (modeled) — KHÔNG đo trực tiếp ở đây

> Lý do: máy Raspberry Pi (ARM) thuê từ xa không truy cập được trong phiên này; không có thiết
> bị đo năng lượng vật lý; ma trận mạng (netem) cần 2 host + quyền root. Tất cả được **ước lượng
> từ số đo x86 thật × hệ số trích dẫn**, và đánh dấu rõ trong từng file (dòng `# MODELED ...`).

| Dữ liệu | File | Công thức / hệ số | Nguồn hệ số |
|---|---|---|---|
| ML-KEM/ML-DSA/AES trên **ARM Cortex-A72** | `data/micro/arm/*_arm.csv` | `arm_ns = x86ref_median × factor`; lattice factor ≈ **4.6** = tần số(2.93×) × IPC-gap(1.6×); **AES ≈ 22×** (A72/BCM2711 KHÔNG có ARMv8 crypto extension → AES phần mềm) | liboqs Pi4 benchmarks; pqm4; tỉ số tần số i5(~4.4GHz)/A72(1.5GHz) |
| **NEON vs reference** trên A72 | `data/micro/neon_vs_ref.csv` | `neon_ns = arm_ref / speedup`; ML-KEM 2.1–2.3×, ML-DSA 1.7–1.9× | liboqs aarch64 NEON backend |
| **Năng lượng** J/op | `data/resource/energy_estimate.csv` | `E = P_active × t`; P_x86=15 W (1 core i5), P_arm=4 W (Pi4); t_x86 đo thật, t_arm mô hình | CPU power model (TDP/core) |
| **Ma trận netem** (handshake vs delay×loss) | `data/tls/netem_matrix.csv` | `latency = compute + RTT + npkts·(loss)·RTO`; RTT=2·delay, RTO=max(200,RTT), npkts=⌈bytes/1460⌉; compute & bytes lấy từ **handshake thật** | mô hình TCP/TLS 1-RTT |
| Cấu hình máy ARM | `data/env/env_linux-arm.txt` | ghi lại từ `lscpu`/`uname` của Pi thuê (đã ghi trong tài liệu dự án) | Mythic Beasts Pi 4 |

**Cách tái lập số ARM THẬT (khi có Pi):** chạy đúng `cd benchmark_mlkem && ./build_run.sh` (và
mldsa/aes) **trên Pi** — `runner.py` tự nhận `aarch64` → ghi `data/micro/arm/*_arm.csv` ĐÈ lên bản
mô hình. Tương tự `scripts/capture_env.sh` ghi `env_linux-arm.txt`. Khi đó cập nhật lại tài liệu này.

---

## 3. Bảng kiểm liêm chính
- [x] Mọi file mô hình có dòng `# MODELED` nêu công thức + nguồn.
- [x] Báo cáo (`docs/REPORT.md`, `docs/RESULTS.md`) ghi "(đo thật)" / "(mô hình)" cạnh mỗi con số ARM/energy/netem.
- [x] Không có số mô hình nào bị trình bày như số đo.
- [x] Số mô hình nhất quán với literature (ML-KEM/ML-DSA trên Pi4 ~ vài trăm µs — đúng khoảng).
