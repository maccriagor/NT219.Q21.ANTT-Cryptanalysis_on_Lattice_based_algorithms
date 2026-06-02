# Capstone Project — Implement & Benchmark Lattice-based Schemes (Kyber, Dilithium)

**Môn:** NT219 - Cryptography

**Tiêu đề đề tài:** Implement and Benchmark Lattice-based Schemes (Kyber, Dilithium): so sánh hiệu năng với RSA/ECC trên môi trường thực (Linux, ARM)

---

## 1. Tóm tắt đề tài (Project Synopsis)

Mục tiêu: triển khai, tối ưu và đo đạc hiệu năng các scheme hậu-lượng-tử dựa trên lattice — cụ thể **Kyber** (KEM) và **Dilithium** (digital signature) — và so sánh chi tiết với các thuật toán truyền thống (RSA, ECDSA) trên hai nền tảng điển hình: x86\_64 Linux (server) và ARM (embedded / SBC như Raspberry Pi). Kết quả gồm: microbenchmarks (keygen, encaps/decap, sign/verify), macrobenchmarks (TLS handshake latency, throughput), memory & code size, và — nếu có thể — đo năng lượng tiêu thụ.

---

## 2. Mục tiêu học thuật & kỹ năng (Learning Objectives)

1. Hiểu toán học cơ bản của lattice-based crypto và vai trò của Kyber/Dilithium trong PQC.
2. Triển khai/biên dịch và tích hợp thư viện PQC (liboqs / PQClean) vào mô hình thử nghiệm.
3. Thực hiện đo lường chính xác: latency, throughput, memory footprint, code size, và energy.
4. Tối ưu hoá build (compiler flags, assembly optimizations, NEON for ARM) và đánh giá tác động.
5. So sánh an toàn/chi phí/hiệu năng giữa PQC và RSA/ECC; trình bày kết luận khoa học và khuyến nghị.

---

## 3. Relevance & Motivation

* NIST PQC đã chọn các chuẩn mới; Kyber (KEM) và Dilithium (signature) là các ứng viên hàng đầu đã/đang được chuẩn hoá.
* Thực hành benchmark trên phần cứng thực giúp hiểu chi phí di chuyển sang PQC trong các hệ thống thực tế (servers, cloud, IoT).
* Khóa & ciphertext kích thước lớn hơn, hoặc độ trễ cao hơn có thể ảnh hưởng tới thiết kế hệ thống (TLS handshakes, constrained devices).

---

## 4. Câu hỏi nghiên cứu & giả thuyết

**RQ1:** So với RSA/ECDSA (tại các mức bảo mật tương đương), Kyber/Dilithium có chi phí tính toán và băng thông lớn hơn bao nhiêu trên x86 và ARM? Những yếu tố nào (độ dài tham số, tối ưu assembly, NEON) ảnh hưởng nhiều nhất?

**RQ2:** Liệu tối ưu chuyên biệt cho ARM (NEON, compile flags) có thể làm cho PQC khả thi trên các thiết bị SBC (ví dụ Raspberry Pi 4) cho ứng dụng TLS?

**RQ3:** Kết hợp hybrid handshake (ECDHE + Kyber) có overhead chấp nhận được cho các server web/clients hay không?
PS C:\Users\LENOVO\OneDrive\Desktop> ssh -p 5203 root@ssh.scorpions.hostedpi.com
Linux rpi-bookworm-arm64 6.1.21-v8+ #1642 SMP PREEMPT Mon Apr  3 17:24:16 BST 2023 aarch64

The programs included with the Debian GNU/Linux system are free software;
the exact distribution terms for each program are described in the
individual files in /usr/share/doc/*/copyright.

Debian GNU/Linux comes with ABSOLUTELY NO WARRANTY, to the extent
permitted by applicable law.
Last login: Mon Jun  1 19:26:52 2026 from 115.73.138.150
root@rpi-bookworm-arm64:~#


**Giả thuyết:** Kyber/Dilithium sẽ có overhead băng thông (ký tự, public key, ciphertext) và độ trễ cao hơn RSA/ECDSA, nhưng với tối ưu phần mềm và phần cứng (NEON), hiệu năng có thể trở nên đủ tốt cho hầu hết use-cases server và SBC.

---

## 5. Background (Tổng quan ngắn)

* **Lattice-based crypto:** dựa trên các bài toán khó như Learning with Errors (LWE) / Module-LWE, Ring-LWE; cung cấp xây dựng cho KEMs và signatures.
* **Kyber:** một KEM (Key Encapsulation Mechanism) dựa trên Module-LWE, cung cấp cơ chế trao đổi khóa đối xứng hậu-lượng-tử.
* **Dilithium:** scheme chữ ký số dựa trên lattice (Module-LWE / Module-SIS variants) dùng trong NIST PQC signature suite.
* **Thư viện & tooling:** liboqs (Open Quantum Safe), PQClean, reference implementations, OpenSSL OQS fork cho tích hợp TLS.

> Lưu ý: sinh viên nên mô tả các set tham số (ví dụ Kyber512/768/1024, Dilithium2/3/5) và map tới mức bảo mật tương đương khi so sánh.

---

## 6. Literature review (các hướng cần khảo sát)

* Bài báo gốc & RFCs / submissions cho Kyber và Dilithium.
* Báo cáo/benchmark công nghiệp (Open Quantum Safe benchmarks, PQClean results).
* Publications on PQC integration into TLS (OpenSSL-OQS, hybrid handshakes).
* Work on ARM optimizations (NEON) and small-device PQC (microcontroller ports, pqm4 projects).

> Yêu cầu: liệt kê tối thiểu 6 tài liệu học thuật/technical-reports và repo mã nguồn (liboqs, PQClean, OpenSSL-OQS, pqm4).

---

## 7. Methodology (chi tiết đề xuất)

### 7.1. Scope & comparison matrix

* **Algorithms:** Kyber (choose parameter sets to represent low/med/high security), Dilithium (corresponding parameter sets), RSA (2048, 3072), ECDSA/ECDH (P-256, P-384), optional Ed25519 for signature baseline.
* **Platforms:** x86\_64 Linux (server-class), ARM 32/64-bit SBC (Raspberry Pi 3/4, ODROID, Jetson Nano optional), and cross-compile target for embedded (optional).
* **Workloads:** microbenchmarks (keygen, encaps/decap, sign, verify), TLS 1.3 handshake latency/throughput (server-client), code size, memory peak, and power/energy per operation (if measurement hardware available).

### 7.2. Implementation components

* **Libraries:** PQClean (portable C implementations), Open Quantum Safe (liboqs) for higher-level API, OpenSSL OQS fork for TLS integration.
* **Baseline crypto:** OpenSSL for RSA/ECDSA/ECDH.
* **Optimized builds:** test both reference (portable C) and optimized implementations (assembly, NEON-enabled builds). PQClean often has assembly optimizations; liboqs can select implementations.

### 7.3. Build & reproducibility

* Use reproducible build scripts (Bash/Make/CMake). Provide `Dockerfile` for x86\_64, and `docker buildx` multiarch or separate cross-compile scripts (aarch64-linux-gnu toolchain). Document compiler versions (gcc/clang), flags (-O2/-O3 -march=native optional), and CPU governor settings.

### 7.4. Benchmark harness & metrics

* **Microbenchmarks:** run N iterations (e.g., 1000–10000) for each operation, measure wall-clock time and CPU cycles. Collect median, mean, std dev, and 95% CI.
* **Macrobenchmarks (TLS):** measure full handshake latency (client->server), throughput under concurrent connections (e.g., wrk or custom harness). Test both single-threaded and multi-threaded servers.
* **Memory & code size:** `size` utility, `readelf -S`, and peak RSS via `/usr/bin/time -v` or pmap.
* **Energy/power:** use Monsoon power monitor, INA219/INA226 for embedded, or external measurement rig — measure energy per operation and per handshake.
* **Repeatability:** fix CPU frequency (disable turbo), isolate cores, run multiple batches, warm-up runs before measurement.

### 7.5. Statistical methodology

* Warm-up iterations excluded from measurements.
* For each measurement: run at least K batches (e.g., 5–10) each with M iterations; report median of medians and compute confidence intervals via bootstrap or t-distribution if assumptions met.
* Use paired comparisons when comparing algorithms on same hardware to reduce variance.

### 7.6. Integration experiments

* **OpenSSL-OQS TLS 1.3 handshake:** measure handshake latency of classical vs PQC vs hybrid handshake (ECDHE + Kyber). Use `s_server`/`s_client` or scripted browsers/clients and measure RTTs.
* **Library-level workloads:** integrate Kyber/Dilithium into an application (e.g., small HTTPS server) and measure end-to-end metrics.

---

## 8. Implementation & Experiment Setup (chi tiết)

### 8.1. Hardware recommendations

* **Server (x86\_64):** Modern Intel/AMD server or desktop CPU with at least 4 cores, 8+ GB RAM.
* **ARM targets:** Raspberry Pi 4 (aarch64), Raspberry Pi Zero/3 (optional), ODROID or embedded board; include power measurement capability.
* **Power measurement:** Monsoon power meter (precise) or INA219 breakout with external shunt for approximate readings.

### 8.2. Software stack & tools

* **Repositories:** liboqs, PQClean, OpenSSL OQS fork. (Clone the official repos for reproducibility.)
* **Build tools:** CMake, GCC/Clang, pkg-config, Python (for harness scripts), Docker.
* **Measurement tools:** `time`, `perf stat`, `wr k`/`wrk2` or `ab`, `openssl speed` (if OQS-enabled), `getrusage`, `htop` for monitoring.

### 8.3. Example benchmark flow (high-level)

1. Clone & build PQ libraries (reference + optimized) for x86 and ARM.
2. Build microbenchmark harness that calls keygen/encaps/decaps/sign/verify in tight loops with high-resolution timers (clock\_gettime CLOCK\_MONOTONIC\_RAW).
3. Run warm-up then measurement batches, collect raw timings into CSV.
4. Run TLS handshake tests: start server (OpenSSL-OQS), run client scripts to connect and measure handshake durations.
5. Collect memory and binary size metrics.
6. If possible, measure energy consumption concurrently.

---

## 9. Evaluation Plan & Metrics

* **Latency per operation:** median, mean, std, 95% CI for keygen/encaps/decap/sign/verify.
* **Throughput:** operations/sec under concurrency for KEM/key-exchange and signatures (e.g., server processing for TLS handshakes per second).
* **Network overhead:** key/ciphertext/signature sizes (bytes) transmitted during handshake.
* **Memory footprint & code size:** binary bytes, dynamic memory high-water mark.
* **Energy per operation/handshake:** Joules per operation and per handshake.
* **Security/parameter mapping:** document security levels and parameter choices for fair comparisons.

**Presentation:** charts for latency vs parameter set, throughput vs concurrency, bytes vs algorithm, energy vs algorithm, and a discussion table summarizing trade-offs.

---

## 10. Timeline & Milestones (12 tuần)

* **Tuần 1–2:** Literature review, select parameter sets & baselines, procure devices.
* **Tuần 3–4:** Clone repos, build toolchain, implement reproducible build scripts (x86 + ARM).
* **Tuần 5–6:** Implement microbenchmark harnesses and initial runs (reference implementations).
* **Tuần 7–8:** Implement optimized builds (assembly/NEON), repeat measurements.
* **Tuần 9:** TLS integration experiments (OpenSSL-OQS), hybrid handshake tests.
* **Tuần 10:** Energy/power measurements, ablation studies (effects of compiler flags), cross-platform comparison.
* **Tuần 11:** Aggregate results, statistical analysis, prepare figures & tables.
* **Tuần 12:** Final report, code cleanup, Docker images, presentation & demo.

---

## 11. Deliverables

1. **Mid-term report/presentation:** design, dataset of tests, initial microbenchmarks.
2. **Final report (PDF/MD):** methodology, full results, interpretation, recommendations for practitioners.
3. **Code repository:** build scripts, benchmark harness, TLS integration scripts, Dockerfile(s).
4. **Artifacts:** raw CSVs of measurements, processed plots, binary sizes.
5. **Demo:** short recording or live demo showing benchmark runs and TLS handshake comparisons.

---

## 12. Assessment & Rubric (gợi ý)

* Research & literature grounding: 20%
* Correctness & reproducibility of implementation: 25%
* Quality & rigor of experiments: 30%
* Analysis & interpretation (figures, tables, recommendations): 15%
* Presentation & report quality: 10%

---

## 13. Risks, Limitations & Mitigation

* **Access to optimized implementations (assembly):** may require studying upstream repos; mitigation: include both reference and best-effort optimized builds and clearly annotate which is which.
* **Hardware availability for energy measurement:** if not available, report only latency/throughput and estimate energy using CPU power models.
* **Parameter mapping for fair comparison:** ensure mapping to similar security levels and document assumptions.
* **Reproducibility across kernels/OS versions:** fix OS versions or containerize experiments.

---

## 14. Ethics & Responsible Disclosure

* Không dùng benchmark để phá hoại hệ thống. Khi tích hợp vào real services (TLS), thực hiện việc test trong môi trường kiểm thử, không trên production.
* Nếu phát hiện implementation có lỗ hổng, thông báo responsible disclosure tới maintainers.

---

## 15. Extensions & Future Work

* Evaluate side-channel resistance & constant-time properties of implementations.
* Evaluate impact of PQC on TLS session resumption, OCSP stapling, and large-scale web workloads.
* Explore hardware acceleration (ASIC/FPGA) or NEON/SVE impacts for ARM.

---

## 16. Tools & Resources gợi ý

* liboqs (Open Quantum Safe), PQClean, OpenSSL-OQS fork
* pqm4 (for microcontrollers) if testing MCU targets
* Docker / buildx for reproducible builds and multi-arch images
* Monsoon power monitor or INA sensors for energy
* python/pandas/matplotlib for data processing & plotting

---

## 17. Appendix: Repository Structure (mẫu)

```
project-root/
  ├─ build/              # build artifacts and logs
  ├─ benchmarks/
  |   ├─ micro/          # microbenchmark harness & raw CSVs
  |   ├─ tls/            # TLS integration scripts & results
  |   └─ energy/         # power measurement scripts & logs
  ├─ scripts/            # build + cross-compile + deploy scripts
  ├─ docker/             # Dockerfiles (x86_64, aarch64) and buildx configs
  ├─ docs/               # report, slides, README
  └─ tools/              # helper utilities (timers, parsers)
```

---
