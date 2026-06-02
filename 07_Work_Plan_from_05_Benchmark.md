# 07 — Work Plan (Greenfield) cho `05_Implement & Benchmark Lattice-based Schemes`

> **Phạm vi:** Bắt đầu từ con số 0, không dùng lại code cũ trong repo. Mọi việc đều **có mục tham chiếu** trong file 05.
> **Mục tiêu:** Trả lời 3 câu hỏi nghiên cứu của file 05 (§4):
> - **RQ1:** PQC đắt hơn RSA/ECDSA bao nhiêu trên x86 và ARM? Yếu tố nào ảnh hưởng nhất?
> - **RQ2:** Tối ưu ARM (NEON, flags) có làm PQC khả thi trên SBC (Pi 4) cho TLS không?
> - **RQ3:** Hybrid handshake (ECDHE + ML-KEM) có overhead chấp nhận được không?

---

## ⚠️ Mục tôi THÊM (không có trong file 05 — đề xuất áp để giữ tính chính xác học thuật)

- [ ] **Đổi tên thuật toán** trong toàn bộ báo cáo: "Kyber" → **ML-KEM (FIPS 203)**; "Dilithium" → **ML-DSA (FIPS 204)**. Ghi tên gốc trong ngoặc khi nhắc paper.
  - *Lý do:* NIST chuẩn hoá 13/08/2024; bản FIPS-final khác *nhẹ* round-3.
- [ ] **Bỏ "OpenSSL-OQS fork"** trong §7.2 & §16 → dùng **OpenSSL ≥ 3.5 native** (hỗ trợ ML-KEM/ML-DSA + nhóm hybrid), fallback `oqs-provider`.
  - *Lý do:* OQS fork đã deprecated (4/2025).
- [ ] Không gọi giá trị **`rdtsc` / `cntvct_el0`** là "CPU cycles" trong báo cáo.
  - *Lý do:* `rdtsc` = invariant-TSC (tần số tham chiếu), `cntvct_el0` = virtual timer ARM (~19.2/54 MHz cố định) → đều không phải core cycles khi DVFS/turbo bật. Muốn cycle thật → `perf stat` hoặc đọc `PMCCNTR_EL0` qua PMU; nếu không → chỉ báo thời gian (ns).

---

## WP0 — Nền tảng học thuật *(Tuần 1–2)*

**Tham chiếu file 05:** §2 (Learning Objectives), §5 (Background), §6 (Literature review), §10 (Tuần 1–2).

- [ ] Viết `docs/background.md`: nền toán **LWE → Ring-LWE → Module-LWE / Module-SIS** → giải thích vì sao ML-KEM dựa Module-LWE, ML-DSA dựa Module-LWE + Module-SIS. *(§5)*
- [ ] Viết `docs/literature.md`: **≥ 6 nguồn học thuật** + repo. Gợi ý: *(§6 yêu cầu "tối thiểu 6")*
  - [ ] 2 paper gốc: Kyber (IACR ePrint 2017/634), Dilithium (IACR ePrint 2017/633)
  - [ ] 3 chuẩn: FIPS 203, 204, 205
  - [ ] Neon-NTT trên Cortex-A72 + Apple M1 (TCHES 2022) — **lõi cho RQ2**
  - [ ] pqm4 (Kannwischer et al.)
  - [ ] SUPERCOP / eBACS (Bernstein–Lange)
  - [ ] Repo: liboqs, PQClean, OpenSSL 3.5+, pqm4
- [ ] Lập **bảng map mức an toàn** (Cat 1/3/5 ↔ RSA ↔ ECC) để biện minh "so sánh công bằng". *(§5 + §9 "Security/parameter mapping")*

**Sản phẩm:** `docs/background.md`, `docs/literature.md`.

---

## WP1 — Scope & Comparison Matrix *(Tuần 1–2)*

**Tham chiếu file 05:** §7.1, §10.

- [ ] Chốt ma trận thuật toán × tham số × nền tảng × workload theo §7.1:

| Loại | Thuật toán (FIPS) | Tham số | NIST Cat | Baseline cổ điển |
|---|---|---|---|---|
| KEM/KEX | ML-KEM | 512 / 768 / 1024 | 1 / 3 / 5 | ECDH P-256/P-384; RSA-2048/3072 |
| Signature | ML-DSA | 44 / 65 / 87 | 2 / 3 / 5 | ECDSA P-256/P-384; RSA-2048/3072; Ed25519 |

- [ ] **Platforms:** x86_64 (server) + ARM aarch64 (Pi 4); tuỳ chọn Pi 3/Zero, Jetson. *(§7.1)*
- [ ] **Workloads:** micro (keygen, encaps, decaps, sign, verify); macro (TLS 1.3 handshake latency + throughput); **code size**; **peak memory**; **energy/op** (nếu có thiết bị). *(§7.1)*

**Sản phẩm:** `docs/comparison_matrix.md`.

---

## WP2 — Môi trường & build tái lập *(Tuần 3–4)*

**Tham chiếu file 05:** §7.2, §7.3, §8.1, §8.2, §10.

- [ ] Chuẩn bị phần cứng: x86 ≥ 4 nhân / 8GB RAM; **Raspberry Pi 4 (aarch64)**; (tuỳ) INA219/Monsoon. *(§8.1)*
- [ ] Clone & build: **liboqs**, **PQClean**, **OpenSSL ≥ 3.5** (cho baseline RSA/ECC + native PQC). *(§7.2, §8.2)*
- [ ] Viết build script tái lập: **CMake/Make/Bash + Dockerfile (x86_64) + `docker buildx --platform linux/arm64`** (hoặc cross-compile `aarch64-linux-gnu`). *(§7.3 nguyên văn)*
- [ ] Ghi rõ phiên bản gcc/clang, flags (`-O2`/`-O3`/`-march=native`/`-mcpu=cortex-a72`), CPU governor = **performance**, tắt turbo. *(§7.3)*
- [ ] Pin core (`taskset`), tắt ASLR khi đo cycle, đồng bộ thời gian.

**Sản phẩm:** `scripts/`, `docker/`, `docs/build_env.md`.

---

## WP3 — Microbenchmark harness *(Tuần 5–6)*

**Tham chiếu file 05:** §7.4, §8.3, §10.

- [ ] Harness C/C++ gọi `keygen`/`encaps`/`decaps`/`sign`/`verify` trong **tight loop**. *(§8.3 bước 2)*
- [ ] Timer phân giải cao: **`clock_gettime(CLOCK_MONOTONIC_RAW)`** cho ns. *(§8.3 nguyên văn)*
- [ ] Cycles qua `perf` / PMU (không chỉ `rdtsc`/`cntvct_el0`). *(bổ sung — xem mục "Tôi thêm")*
- [ ] Chạy **N = 1000–10000 iterations/op**, xuất raw CSV. *(§7.4 + §8.3)*

**Sản phẩm:** `benchmarks/micro/` + CSV thô.

---

## WP4 — Phương pháp thống kê *(Tuần 5–6)*

**Tham chiếu file 05:** §7.4, §7.5.

- [ ] **Warm-up iterations** loại khỏi số đo. *(§7.4 + §7.5)*
- [ ] **K batches × M iterations** (K = 5–10). *(§7.5)*
- [ ] Báo cáo **median-of-medians, mean, std, 95% CI** (bootstrap hoặc t-distribution). *(§7.5)*
- [ ] **Paired comparison** trên cùng phần cứng để giảm phương sai. *(§7.5)*

**Sản phẩm:** `tools/stats.py` (pandas).

---

## WP5 — Build tối ưu & ablation *(Tuần 7–8)*

**Tham chiếu file 05:** §7.2, §10 (Tuần 7–8 và "ablation studies").

- [ ] So sánh **2 biến thể**: reference C (PQClean `clean`) vs **NEON-optimized** (liboqs `OQS_USE_CPU_EXTENSIONS=ON`). *(§7.2 nguyên văn)*
- [ ] **Ablation compiler flags:** `-O2` vs `-O3` vs `-O3 -mcpu=cortex-a72`. *(§10 "effects of compiler flags")*
- [ ] So sánh kết quả x86 vs ARM cùng biến thể để trả lời **RQ1/RQ2**.

**Sản phẩm:** bảng "reference vs optimized" cho x86 & ARM, biểu đồ tốc độ.

---

## WP6 — TLS integration / Macrobenchmark *(Tuần 9)*

**Tham chiếu file 05:** §7.4 (throughput concurrency), §7.6, §10.

- [ ] Dựng **OpenSSL ≥ 3.5** `s_server` / `s_client`. *(§7.6 nguyên văn)*
- [ ] Đo handshake **3 chế độ**: classical (`x25519`) vs PQC (`mlkem768/1024`) vs **hybrid (`X25519MLKEM768`)**. *(§7.6 "classical vs PQC vs hybrid")*
- [ ] Chứng chỉ ký bằng **ML-DSA vs RSA vs ECDSA** — đo full handshake bytes + latency.
- [ ] **Throughput** dưới tải đồng thời (`wrk`/`wrk2`), single- & multi-thread. *(§7.4)*
- [ ] Tích hợp Library-level: small HTTPS server end-to-end. *(§7.6)*

**Sản phẩm:** `benchmarks/tls/` + CSV.

---

## WP7 — Memory, code size, energy *(Tuần 10)*

**Tham chiếu file 05:** §7.4, §10.

- [ ] **Code/binary size:** `size`, `readelf -S`. *(§7.4 nguyên văn)*
- [ ] **Peak RSS:** `/usr/bin/time -v`, `pmap`. *(§7.4 nguyên văn)*
- [ ] **Energy/op:**
  - x86: `perf stat -e power/energy-pkg/` (Intel/AMD RAPL).
  - ARM Pi: **INA219/INA226** hoặc **Monsoon**; hoặc ước lượng `cycles × TDP/freq` (ghi rõ "ước lượng"). *(§7.4 nguyên văn)*
- [ ] Cross-platform comparison. *(§10 "cross-platform comparison")*

**Sản phẩm:** `benchmarks/energy/` + CSV.

---

## WP8 — Phân tích, biểu đồ, báo cáo *(Tuần 11–12)*

**Tham chiếu file 05:** §9 (Evaluation Plan), §11 (Deliverables), §10.

- [ ] Tổng hợp CSV → **biểu đồ (matplotlib)**: *(§9 nguyên văn)*
  - [ ] latency vs parameter set
  - [ ] throughput vs concurrency
  - [ ] bytes vs algorithm
  - [ ] energy vs algorithm
- [ ] **Bảng trade-off** + trả lời rõ RQ1/RQ2/RQ3. *(§9)*
- [ ] Statistical analysis + figures & tables. *(§10 Tuần 11)*
- [ ] **Mid-term report** (đầu kỳ T6). *(§11)*
- [ ] **Final report (PDF/MD)** — methodology + full results + interpretation + recommendations. *(§11)*
- [ ] **Demo** — recording hoặc live demo benchmark + TLS handshake. *(§11)*

**Sản phẩm:** `docs/RESULTS.md`, `docs/final_report.pdf`, biểu đồ, recording demo.

---

## Cấu trúc repo *(§17)*

```
project-root/
  ├─ build/              # build artifacts + logs
  ├─ benchmarks/
  │   ├─ micro/          # microbenchmark harness + raw CSV
  │   ├─ tls/            # TLS integration scripts + results
  │   └─ energy/         # power measurement scripts + logs
  ├─ scripts/            # build + cross-compile + deploy
  ├─ docker/             # Dockerfile x86_64, aarch64, buildx
  ├─ docs/               # background, literature, results, report, slides
  └─ tools/              # timers, parsers, stats
```

---

## Checklist deliverables cuối kỳ *(§11)*

- [ ] **Mid-term report** (T6)
- [ ] **Final report** (PDF + MD)
- [ ] **Code repository** (build scripts + harness + TLS scripts + Dockerfile)
- [ ] **Artifacts** (raw CSVs + processed plots + binary sizes)
- [ ] **Demo** (recording / live)

---

## Việc nền xuyên suốt *(§13 Risks, §14 Ethics)*

- [ ] Cố định OS/kernel version → containerize để tái lập. *(§13)*
- [ ] Thiếu thiết bị đo điện → chỉ báo latency/throughput + ước lượng energy (ghi rõ giả định). *(§13)*
- [ ] Document parameter mapping giả định cho fair comparison. *(§13)*
- [ ] Chỉ test môi trường kiểm thử, **không production**. *(§14)*
- [ ] Phát hiện lỗ hổng → **responsible disclosure** tới maintainers. *(§14)*

---

## Timeline 12 tuần *(§10 nguyên văn)*

| Tuần | Việc | WP |
|---|---|---|
| 1–2 | Literature review, select parameter sets & baselines, procure devices | WP0, WP1 |
| 3–4 | Clone repos, build toolchain, reproducible build scripts (x86 + ARM) | WP2 |
| 5–6 | Microbenchmark harnesses + initial runs (reference) | WP3, WP4 |
| 7–8 | Optimized builds (assembly/NEON), repeat measurements | WP5 |
| 9 | TLS integration experiments, hybrid handshake tests | WP6 |
| 10 | Energy/power measurements, ablation, cross-platform | WP7 |
| 11 | Aggregate results, statistical analysis, figures & tables | WP8 |
| 12 | Final report, code cleanup, Docker images, presentation & demo | WP8 |

---

## Rubric chấm *(§12)*

| Tiêu chí | Trọng số | WP liên quan |
|---|---|---|
| Research & literature grounding | 20% | WP0 |
| Correctness & reproducibility of implementation | 25% | WP2, WP3 |
| **Quality & rigor of experiments** | **30%** | **WP3, WP4, WP5, WP6, WP7** |
| Analysis & interpretation (figures/tables/recommendations) | 15% | WP8 |
| Presentation & report quality | 10% | WP8 |
