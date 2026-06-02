# 10 — Đặc tả yêu cầu (bóc từ file 05) — Chỉ dẫn KHÔNG bỏ sót

> **Mục đích:** Liệt kê **mọi yêu cầu** ẩn/hiện trong `05_Implement & Benchmark Lattice-based Schemes`. Chốt xong **PHẦN LÕI** (mục A–K) rồi mới sang **viết repo** (mục L).
> **Quy ước:** `[MUST]` = bắt buộc · `[OPT]` = tùy chọn (file 05 ghi "optional / if possible") · `(§x)` = mục nguồn trong file 05.

---

## A. PHẠM VI & ĐỐI TƯỢNG SO SÁNH *(comparison matrix — §1, §7.1)*

- **A1 [MUST]** Triển khai + tối ưu + **đo** 2 scheme: **ML-KEM** (KEM) và **ML-DSA** (chữ ký). *(§1)*
- **A2 [MUST]** Mỗi scheme phải có **≥3 mức tham số** đại diện thấp/trung/cao: ML-KEM-512/768/1024, ML-DSA-44/65/87. *(§7.1, §5)*
- **A3 [MUST]** Baseline cổ điển: **RSA (2048, 3072)** + **ECDSA/ECDH (P-256, P-384)**. *(§7.1)*
- **A4 [OPT]** Ed25519 làm baseline chữ ký. *(§7.1)*
- **A5 [MUST]** Chạy trên **2 nền tảng**: **x86_64 Linux** (server) + **ARM SBC** (Raspberry Pi 3/4). *(§1, §7.1, §8.1)*
- **A6 [OPT]** ODROID / Jetson Nano / Pi Zero / cross-compile target nhúng. *(§7.1, §8.1)*
- **A7 [MUST]** Workloads phải gồm đủ 4 nhóm: **(a) microbenchmark** (keygen, encaps, decaps, sign, verify); **(b) macrobenchmark TLS** (handshake latency + throughput); **(c) code size + memory peak**; **(d) energy/op** `[OPT nếu không có thiết bị]`. *(§1, §7.1)*

---

## B. CÂU HỎI NGHIÊN CỨU PHẢI TRẢ LỜI *(§4)*

- **B1 [MUST]** **RQ1:** PQC đắt hơn RSA/ECDSA bao nhiêu (tính toán + băng thông) ở mức an toàn tương đương, trên **x86 và ARM**? Yếu tố nào (độ dài tham số, assembly, NEON) ảnh hưởng **nhiều nhất**? *(§4)*
- **B2 [MUST]** **RQ2:** Tối ưu ARM (NEON, compile flags) có làm PQC **khả thi trên SBC (Pi 4)** cho TLS không? *(§4)*
- **B3 [MUST]** **RQ3:** Hybrid handshake (ECDHE + ML-KEM) có **overhead chấp nhận được** cho web server/client không? *(§4)*
- **B4 [MUST]** Phát biểu **giả thuyết** rõ ràng và **kiểm chứng** bằng số liệu. *(§4)*

> ⚠️ Mọi thí nghiệm phải phục vụ trả lời B1–B3. Số liệu nào không gắn vào 1 RQ thì cân nhắc bỏ.

---

## C. NỀN TẢNG LÝ THUYẾT PHẢI TRÌNH BÀY *(§2, §5)*

- **C1 [MUST]** Giải thích toán nền: **LWE / Module-LWE / Ring-LWE / Module-SIS**; vì sao ML-KEM dựa Module-LWE, ML-DSA dựa Module-LWE + Module-SIS. *(§5)*
- **C2 [MUST]** Vai trò ML-KEM/ML-DSA trong **NIST PQC**. *(§2.1, §3)*
- **C3 [MUST]** Mô tả **các set tham số** + **map tới mức an toàn tương đương** (để so sánh công bằng). *(§5 lưu ý, §9)*
- **C4 [MUST]** **Literature review ≥ 6 tài liệu** học thuật/technical-report **+ repo** (liboqs, PQClean, OpenSSL-OQS, pqm4). *(§6 — yêu cầu định lượng "tối thiểu 6")*
- **C5 [MUST]** Khảo sát 4 hướng: paper gốc & RFC/submission; benchmark công nghiệp (OQS, PQClean); PQC trong TLS; tối ưu ARM + small-device (pqm4). *(§6)*

---

## D. THÀNH PHẦN TRIỂN KHAI *(§7.2, §8.2, §16)*

- **D1 [MUST]** Thư viện PQC: **PQClean** (portable C) + **liboqs** (API bậc cao). *(§7.2)*
- **D2 [MUST]** Tích hợp TLS qua **OpenSSL** *(file 05 ghi "OQS fork" — cập nhật: OpenSSL ≥ 3.5 native hoặc oqs-provider)*. *(§7.2, §16)*
- **D3 [MUST]** Baseline RSA/ECDSA/ECDH dùng **OpenSSL**. *(§7.2)*
- **D4 [MUST]** Phải build **CẢ HAI biến thể**: **reference (portable C)** VÀ **optimized (assembly/NEON)**; ghi rõ cái nào là cái nào. *(§7.2, §13)*
- **D5 [OPT]** pqm4 nếu test microcontroller. *(§16)*

---

## E. BUILD & TÁI LẬP *(§7.3, §8.1, §8.2)*

- **E1 [MUST]** Script build **tái lập** (Bash/Make/CMake). *(§7.3)*
- **E2 [MUST]** **Dockerfile cho x86_64**. *(§7.3)*
- **E3 [MUST]** **`docker buildx` multiarch HOẶC cross-compile** `aarch64-linux-gnu` cho ARM. *(§7.3)*
- **E4 [MUST]** Tài liệu hóa: **phiên bản compiler** (gcc/clang), **flags** (-O2/-O3/-march=native), **CPU governor**. *(§7.3)*
- **E5 [MUST]** Phần cứng đo: x86 ≥4 nhân/8GB; **Pi 4 aarch64**; (đo điện) Monsoon hoặc INA219. *(§8.1)*
- **E6 [MUST]** Công cụ đo: `time`, `perf stat`, `wrk`/`wrk2`/`ab`, `openssl speed`, `getrusage`, `htop`. *(§8.2)*

---

## F. CHỈ SỐ PHẢI ĐO *(§7.4, §9)*

- **F1 [MUST]** **Latency/op**: median, mean, std, **95% CI** cho keygen/encaps/decaps/sign/verify. *(§7.4, §9)*
- **F2 [MUST]** **Throughput**: ops/sec dưới đồng thời cho KEM/key-exchange và signature. *(§9)*
- **F3 [MUST]** **Network overhead**: kích thước key/ciphertext/signature (bytes) truyền trong handshake. *(§9)*
- **F4 [MUST]** **Memory + code size**: binary bytes (`size`, `readelf -S`), peak RSS (`/usr/bin/time -v`, `pmap`). *(§7.4, §9)*
- **F5 [MUST]** Đo **cả wall-clock (ns) lẫn CPU cycles**. *(§7.4)*
- **F6 [OPT]** **Energy/op + energy/handshake** (Joules). Nếu không có thiết bị → ước lượng qua CPU power model. *(§7.4, §9, §13)*
- **F7 [MUST]** **Security/parameter mapping**: tài liệu hóa mức an toàn + lựa chọn tham số cho so sánh công bằng. *(§9)*

---

## G. PHƯƠNG PHÁP ĐO (RIGOR) *(§7.4, §7.5)* — chiếm 30% điểm

- **G1 [MUST]** **N = 1000–10000 iterations** mỗi op. *(§7.4)*
- **G2 [MUST]** **Warm-up** runs, loại khỏi số đo. *(§7.4, §7.5)*
- **G3 [MUST]** **K batch (5–10) × M iteration**; báo cáo **median-of-medians**. *(§7.5)*
- **G4 [MUST]** **95% CI** qua bootstrap hoặc t-distribution. *(§7.5)*
- **G5 [MUST]** **Paired comparison** trên cùng phần cứng để giảm phương sai. *(§7.5)*
- **G6 [MUST]** **Cố định tần số CPU** (tắt turbo), **isolate cores**, nhiều batch. *(§7.4)*
- **G7 [MUST]** Timer phân giải cao: **`clock_gettime(CLOCK_MONOTONIC_RAW)`**. *(§8.3)*

---

## H. TÍCH HỢP TLS *(§7.6)*

- **H1 [MUST]** Đo handshake **3 chế độ**: **classical vs PQC vs hybrid (ECDHE + ML-KEM)**. *(§7.6)*
- **H2 [MUST]** Dùng **`s_server`/`s_client`** (hoặc client script) đo RTT/handshake duration. *(§7.6)*
- **H3 [MUST]** Throughput dưới **đồng thời**, test **single- và multi-threaded** server. *(§7.4)*
- **H4 [MUST]** **Tích hợp vào ứng dụng** (vd. **HTTPS server nhỏ**) đo end-to-end. *(§7.6)*

---

## I. PHÂN TÍCH & TRÌNH BÀY *(§9)*

- **I1 [MUST]** Biểu đồ: **latency vs parameter set**. *(§9)*
- **I2 [MUST]** Biểu đồ: **throughput vs concurrency**. *(§9)*
- **I3 [MUST]** Biểu đồ: **bytes vs algorithm**. *(§9)*
- **I4 [MUST]** Biểu đồ: **energy vs algorithm** `[OPT nếu không đo điện]`. *(§9)*
- **I5 [MUST]** **Bảng trade-off** tổng kết + trả lời rõ RQ1/RQ2/RQ3 + **khuyến nghị cho practitioner**. *(§9, §11)*

---

## J. SẢN PHẨM BẮT BUỘC NỘP *(§11)*

- **J1 [MUST]** **Mid-term report**: design + dataset test + microbenchmark ban đầu. *(§11)*
- **J2 [MUST]** **Final report (PDF/MD)**: methodology + full results + interpretation + recommendations. *(§11)*
- **J3 [MUST]** **Code repository**: build scripts + benchmark harness + TLS scripts + Dockerfile. *(§11)*
- **J4 [MUST]** **Artifacts**: raw CSV + processed plots + binary sizes. *(§11)*
- **J5 [MUST]** **Demo**: recording/live chạy benchmark + so sánh TLS handshake. *(§11)*

---

## K. RÀNG BUỘC / RỦI RO / ĐẠO ĐỨC *(§13, §14)*

- **K1 [MUST]** Nếu thiếu thiết bị đo điện → chỉ báo latency/throughput + **ước lượng energy** (ghi rõ giả định). *(§13)*
- **K2 [MUST]** **Cố định OS/kernel** hoặc containerize để tái lập. *(§13)*
- **K3 [MUST]** Document giả định **parameter mapping** cho fair comparison. *(§13)*
- **K4 [MUST]** Chỉ test **môi trường kiểm thử**, không production. *(§14)*
- **K5 [MUST]** Phát hiện lỗ hổng → **responsible disclosure**. *(§14)*

---

## ⭐ PHẦN LÕI CẦN CHỐT TRƯỚC KHI VIẾT REPO

> Đây là 8 quyết định "lõi" — chốt xong mới code, để không phải làm lại:

1. **[Chốt A1–A7]** Bảng comparison matrix cuối cùng (thuật toán × tham số × nền tảng × workload).
2. **[Chốt B1–B4]** Viết 3 RQ + giả thuyết ra giấy.
3. **[Chốt C3, F7]** Bảng map mức an toàn (Cat 1/3/5 ↔ RSA ↔ ECC).
4. **[Chốt C4]** Thu đủ **≥6 tài liệu** + đọc.
5. **[Chốt F1–F7]** Danh sách chỉ số đo cuối cùng (đo gì, đơn vị gì).
6. **[Chốt G1–G7]** Tham số phương pháp: N, K, M, warm-up, cách tính CI.
7. **[Chốt D1–D4, E4]** Phiên bản thư viện (liboqs/PQClean/OpenSSL) + biến thể reference vs optimized.
8. **[Chốt E5]** Phần cứng đã sẵn sàng (x86 + Pi 4 + (tùy) INA219).

✅ **8 cái này = "phần lõi".** Xong hết → mới sang mục L.

---

## L. VIẾT REPO (sau khi chốt lõi) *(§17)*

```
project-root/
  ├─ build/              # artifacts + logs
  ├─ benchmarks/
  │   ├─ micro/          # harness keygen/encaps/decaps/sign/verify + raw CSV  ← F1,F5,G
  │   ├─ tls/            # script handshake classical/PQC/hybrid + results      ← H
  │   └─ energy/         # script đo điện + logs                               ← F6
  ├─ scripts/            # build + cross-compile + deploy                       ← E1,E3
  ├─ docker/             # Dockerfile x86_64, aarch64, buildx                   ← E2,E3
  ├─ docs/               # background, literature, results, report, slides      ← C,I,J
  └─ tools/              # timers, parsers, stats.py (median/CI)                ← G3,G4
```

| Thư mục repo | Đáp ứng yêu cầu |
|---|---|
| `benchmarks/micro/` | A1–A4, F1, F4, F5, G1–G7 |
| `benchmarks/tls/` | H1–H4, F2, F3 |
| `benchmarks/energy/` | F6, K1 |
| `scripts/` + `docker/` | E1–E4, K2 |
| `tools/stats.py` | G3, G4, I1–I4 |
| `docs/` | C, I5, J1, J2 |

---

## Bảng đếm nhanh

| Nhóm | MUST | OPT |
|---|---|---|
| A Phạm vi | 5 | 2 |
| B Câu hỏi NC | 4 | 0 |
| C Lý thuyết | 5 | 0 |
| D Triển khai | 4 | 1 |
| E Build | 6 | 0 |
| F Chỉ số | 6 | 1 |
| G Phương pháp | 7 | 0 |
| H TLS | 4 | 0 |
| I Phân tích | 5 | 0 |
| J Sản phẩm | 5 | 0 |
| K Ràng buộc | 5 | 0 |
| **Tổng** | **56 MUST** | **5 OPT** |

> Cập nhật học thuật (không có trong 05, nên áp): tên **ML-KEM/ML-DSA (FIPS 203/204)**; **OpenSSL ≥ 3.5 native** thay OQS fork; **cycle thật** qua `perf`/PMU thay vì coi `rdtsc`/`cntvct_el0` là cycles.
