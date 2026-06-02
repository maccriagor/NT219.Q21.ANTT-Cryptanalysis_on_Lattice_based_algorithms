---
name: project-pqc-benchmark
description: NT219 capstone — benchmark ML-KEM-1024/ML-DSA-87 vs RSA-15360/P-521 at NIST Category 5
metadata:
  type: project
---

Capstone môn NT219 (Cryptography). Đề tài: implement & benchmark lattice-based PQC vs cổ điển. Đề cương ở `05_Implement & Benchmark Lattice-based Schemes (Kyber, Dilithium).md`.

Thực trạng: `main.cpp` đã benchmark **ML-KEM-1024 + ML-DSA-87** (PQClean, FIPS-final) trong giao thức hybrid với AES-256-GCM; `rsa15360_full/` và `benchmark_p521/` là baseline. Tất cả ở **NIST Category 5** (≈256-bit) — so sánh công bằng cùng mức an toàn. Build qua Dockerfile (OpenSSL 3.6.1 + PQClean).

Có 2 khung yêu cầu song song: **file 05** (đề tài benchmark thuần) và **`NT2205_Checklist_Corrected_v2.md`** (rubric 100đ asset-centric + 3 lớp crypto + ≥2 deployment + eval định lượng). Nộp chỉ nội dung 05 sẽ trượt rubric NT2205 (~60đ). Hợp nhất: PQC từ 05 đặt vào **Lớp Crypto** của NT2205; benchmark thành E-C4 + biện minh chọn thuật toán. Checklist §2.2 đã mời PQC.

**Setup đã chốt (WP1):** Pi thuê = Mythic Beasts "scorpions" (Cortex-A72, 4GB, aarch64, Bookworm, OpenSSL 3.0.11), SSH qua proxy IPv4 `ssh.scorpions.hostedpi.com:5203` user root (mạng nhà KHÔNG có IPv6). x86 = WSL2 Ubuntu trên Windows. **MỌI THỨ QUA DOCKER**: `docker/Dockerfile.pqc` (FROM debian:bookworm-slim). **Cập nhật: dùng OpenSSL 3.6.2 build từ nguồn (native ML-KEM/ML-DSA, mới nhất 04/2026) + liboqs 0.12.0; BỎ oqs-provider** (trên OpenSSL ≥3.5 nó bị disable, native thắng). TLS/cert dùng native (`-groups X25519MLKEM768`, `genpkey -algorithm ML-DSA-65`); liboqs giữ cho microbench primitive + so NEON (WP3). `scripts/capture_env.sh` ghi cấu hình máy ra `/data/env/env_linux-arm.txt` (Pi) hoặc `env_linux-x86.txt` (x86) theo `uname -m`; `/data` là volume mount cho mọi kết quả. Runbook WP1 = `12_Runbook_WP1_Build.md` (scope = chỉ thiết lập). WP1 dossier lý thuyết = `11_WP0_Study_Dossier.md`. Roadmap chính = `08_...` (WP0-WP7b).

**How to apply:** Trong báo cáo phải gọi đúng **ML-KEM (FIPS 203)** / **ML-DSA (FIPS 204)**, không phải "Kyber/Dilithium" (chỉ ghi tên gốc khi nhắc paper). 3 lỗi đo lường đã flag: (1) `cntvct_el0`/`rdtsc` KHÔNG phải core cycles; (2) fork-per-iteration làm méo số liệu → cần in-process loop + warm-up + median/CI; (3) "OpenSSL-OQS fork" trong đề cương đã deprecated → dùng OpenSSL ≥3.5 native hoặc oqs-provider. NT2205 §2.2 có typo "ML-DSL" = ML-DSA. Files reference (2026-05-31): `06_References_and_Technical_Roadmap.md` (research + tài liệu IEEE), `07_Work_Plan_from_05_Benchmark.md` (greenfield WP0-WP8), `10_Requirements_Spec_from_05.md` (56 MUST + 5 OPT bóc từ file 05, có mã + §). **`08_Work_Plan_from_NT2205_Checklist.md` ĐÃ BỊ USER GHI ĐÈ thành "ROADMAP HOÀN THIỆN ĐỒ ÁN" (12 tuần, WP0-WP7, gồm onion 3 lớp + đo TLS qua mạng thật + netem) — đây là NỘI DUNG CHÍNH user dùng.** Tôi đã vá 11 điểm thiếu so với file 05 (giả thuyết, mid-term report, đạo đức, paired comparison, K-batch median-of-medians, PQClean note, CLOCK_MONOTONIC_RAW, single/multi-thread server, quét đủ 3 mức, energy chart, primitive throughput). Phần cứng thực tế: thuê Pi 4 Cortex-A72 4GB từ Mythic Beasts, OpenSSL 3.0.11 + oqs-provider (không phải 3.5 native).
