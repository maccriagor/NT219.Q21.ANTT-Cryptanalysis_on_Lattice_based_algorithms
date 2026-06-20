# CẨM NANG QUAY DEMO — NT219 PQC Capstone (Deliverable 5)

Hướng dẫn quay video demo làm **bằng chứng chạy thật** cho đồ án benchmark mật mã
hậu lượng tử (ML-KEM / ML-DSA vs RSA / ECC). Tách theo hai máy **x86_64** và **ARM
(Raspberry Pi 4)**. Mỗi task ghi rõ bốn thứ:

> **Đo gì / là gì** · **Tại sao** (cho người mới) · **Lệnh quay** · **Kiểm đúng bằng tool có sẵn**

Cuối mỗi task có dòng **“Nói trên video”** = một câu thuyết minh ngắn khi quay.

---

## §0. Demo này để làm gì

Đề bài liệt kê 5 sản phẩm phải nộp; **Deliverable 5 = video demo** (quay màn hình
hoặc chạy trực tiếp). Vai trò của nó:

- **Bằng chứng pipeline chạy thật trên cả hai máy.** Theo *định luật bằng chứng*
  của đồ án: số chưa chạy / chưa push coi như chưa tồn tại. Video chứng minh không
  phải “chép số” mà thật sự build–đo–phân tích được.
- **Cho người chấm thấy bạn hiểu mình làm gì.** Vì vậy mỗi cảnh phải nói rõ *đo
  gì, tại sao, kiểm đúng thế nào* — không chỉ gõ lệnh cho chạy.

Bốn “định luật” xuyên suốt (nhắc khi quay):
1. **Người thuê nhà** — mọi phần mềm TLS (nginx, s_server, server tự viết) *thuê*
   mật mã từ thư viện; muốn PQC thì thư viện phải OpenSSL 3.5+.
2. **Bốn con số** — chỉ so trực tiếp hai số trên *cùng một máy*; giữa hai máy thì
   so **tỉ lệ** (opt/ref máy này vs opt/ref máy kia).
3. **Tham số** — mặc định trong code chỉ là fallback; mọi vòng lặp/cổng chỉnh qua
   biến môi trường.
4. **Bằng chứng** — push trước khi rời máy thuê.

---

## §1. Các thứ cần đo là gì — và tại sao (giải thích cho người mới)

### 1.1 Ba câu hỏi nghiên cứu (RQ) — sợi chỉ xuyên suốt
- **RQ1**: PQC đắt hơn RSA/ECDSA bao nhiêu về **tính toán + băng thông**, trên x86
  và ARM, yếu tố nào nặng nhất? → **microbenchmark + đo kích thước**.
- **RQ2**: tối ưu riêng cho ARM (**NEON**, cờ biên dịch) có kéo PQC về **mức khả
  thi trên SBC** không? → **thí nghiệm ghép cặp ref/opt trên liboqs**.
- **RQ3**: handshake **hybrid** (ECDHE + ML-KEM) có **overhead chấp nhận được**
  không? → **đo TLS 1.3 handshake thật** (latency + throughput), gồm cả **track D
  (TLS tự viết)** ở §4.8/§5.6.

Mọi phép đo tồn tại để trả lời ba câu này. Khi quay, mỗi task nên gắn vào RQ nào.

**Giả thuyết của đề** (kiểm bằng bảng `analysis_out/tables.md`): *PQC tốn băng
thông/khoá lớn hơn, nhưng tối ưu phần mềm (NEON/AVX2) kéo hiệu năng về đủ dùng.*
Nếu đúng: cột **kích thước** thua rõ, cột **tốc độ sau tối ưu** ngang/hơn RSA.

### 1.2 Bốn họ chỉ số (đề §9) — đo cái gì nghĩa là gì
- **Latency (độ trễ)**: một phép toán / một handshake mất **bao lâu**. Báo
  **median / mean / p95 / CI** chứ không chỉ trung bình. *Tại sao median?* Máy tính
  có nhiễu (OS xen ngang); median = “người đứng giữa hàng” nên miễn nhiễm vài vòng
  đột biến, còn mean bị một outlier kéo lệch.
- **Throughput (thông lượng)**: mỗi giây làm được **bao nhiêu việc** dưới tải song
  song. *Tại sao?* Người dùng quan tâm latency (vào web nhanh không), người vận
  hành quan tâm throughput (phục vụ nổi bao nhiêu người).
- **Memory (bộ nhớ lúc chạy)**: đỉnh RAM tiến trình chiếm (**peak RSS**).
- **Code size (kích thước mã lúc nằm im)**: thư viện nặng bao nhiêu byte trên đĩa
  (**text/data/bss**). *Hay nhầm:* memory đo lúc **chạy**, code size đo lúc **nằm im**.

### 1.3 Vì sao phải đo mức an toàn tương đương
So ML-KEM-768 với RSA-2048 rồi nói “PQC nhanh hơn” là **so lê với táo**. Quy đổi
(FIPS 203/204 + SP 800-57): mức 1 ↔ ML-KEM-512/ML-DSA-44 ↔ RSA-3072 ↔ P-256;
mức 3 ↔ ML-KEM-768/ML-DSA-65 ↔ RSA-7680 ↔ P-384; mức 5 ↔ ML-KEM-1024/ML-DSA-87 ↔
RSA-15360 ↔ P-521. (RSA-2048 vẫn đo làm baseline phổ biến nhưng phải chú thích nó
dưới mức 1.)

### 1.4 Warm-up & K-batch — vì sao không “đo một phát ăn ngay”
Vòng đo đầu tiên luôn chậm bất thường (cache lạnh, thư viện nạp lười). Vì vậy:
**bỏ vài vòng warm-up đầu**, và chạy **K=5 batch độc lập** rồi lấy
**median-of-medians** — một batch xui (nhiệt, tiến trình nền) không phá được kết quả.

---

## §2. Chuẩn bị quay (công cụ)

Chọn một trong hai cách quay:

- **Quay terminal (gọn, nhẹ)** — `asciinema`:
  ```sh
  sudo apt install asciinema
  asciinema rec demo_x86.cast          # bat dau quay; go 'exit' de dung
  asciinema play demo_x86.cast         # xem lai
  ```
  Hoặc đơn giản hơn, `script` (có sẵn): `script -t demo.timing demo.log`.
- **Quay màn hình (thấy cả biểu đồ PNG)** — OBS Studio / SimpleScreenRecorder:
  ```sh
  sudo apt install simplescreenrecorder   # hoac obs-studio
  ```

Mẹo quay:
- Phóng to font terminal cho dễ đọc.
- Phần TLS cần **2 cửa sổ** cạnh nhau (server | client) — thấy cả hai phía.
- Bật sẵn `source scripts/setenv.sh` ở **mọi** terminal mới (nếu không, lệnh dùng
  OpenSSL hệ thống → sai).
- Quay từng task thành đoạn ngắn rồi ghép, dễ hơn quay một mạch dài.

---

## §3. Bố cục video (storyboard)

```
[1] Intro 30s        : ten, MSSV, de tai, 3 RQ, ban do repo
[2] MAY x86_64       : build -> WP2 -> WP5 -> WP3 -> WP4(A/B/C) -> Track D -> analyze
[3] MAY ARM (Pi 4)   : build -> WP2(nhiet) -> WP3(NEON) -> WP5 -> WP4 -> Track D -> analyze -> cross-compile
[4] Tong hop         : bang cross-platform ratio, ket luan RQ1/RQ2/RQ3
```

Thời lượng gợi ý: 8–12 phút. Không cần quay phần build OpenSSL ~30 phút — quay
đoạn bắt đầu rồi cắt sang `openssl version` đã xong.

> Repo **đã có sẵn** `scripts/demo.sh` quay 3 màn ~2 phút (Deliverable 5). Cẩm nang
> này là bản **đầy đủ/giải thích** để quay kỹ hơn; bạn có thể chiếu `demo.sh` cho
> bản ngắn rồi dùng các §dưới để nói sâu từng phần.

### 3.1 — Bộ tham số DEMO-FAST (đã chạy thật, ráp lại để quay nhanh)

Toàn bộ kịch bản dưới có giá trị mặc định “lấy số thật” (chậm). Để **quay demo**,
ghi đè bằng các biến môi trường sau — tất cả đã được chạy kiểm trên x86_64, thời
gian thực đo kèm theo:

| Phần | Lệnh DEMO-FAST | ~Thời gian |
|------|----------------|-----------|
| WP2 micro (K-batch) | `for i in 1 2; do MICRO_ALGOS="mlkem 768;mldsa 65;rsa 3072" BENCH_WARMUP=0 BENCH_ITERS=1 BENCH_KEYGEN_ITERS=1 make bench; cp data/summary_micro_$(uname -m).csv data/raw/$(uname -m)/summary_batch$i.csv; done` | **~2 s** |
| WP5 memory/codesize | `make memory && make codesize` | ~15 s |
| WP3 liboqs | `bash scripts/run_liboqs_speed.sh` (sau khi đã build ref+opt) | ~40 s |
| WP4-A s_server | `TLS_ITERS=3 TLS_WARMUP=1 TLS_CONC=2 TLS_DUR=1 make tls` | **~9 s** |
| WP4-B nginx | `WORKERS_LIST=1 TLS_ITERS=3 TLS_WARMUP=1 TLS_CONC=2 TLS_DUR=1 bash scripts/bench_tls_nginx.sh` | **~9 s** |
| WP4-C tls_mini | client `... 9443 X25519MLKEM768 5 ...` (5 handshake) | ~1 s |
| Track D latency | server `count=25`, client `BENCH_ITERS=20 ... --warmup 5` | ~1 s |
| Track D throughput | server `count=200`, client `--total 200 --threads 8` | ~3 s |

> **Vì sao WP2 mặc-định-nhanh vẫn lâu nếu không dùng `MICRO_ALGOS`:** ma trận 15
> thuật toán gồm **RSA-15360**, mà **một** lần keygen RSA-15360 đã ngốn ~30 s–vài
> phút (chạy kiểm: 2 batch × 15 algo, warmup=0/iter=keygen=1 = **7 phút 38 giây**,
> gần như toàn bộ là RSA-15360/7680 keygen). `MICRO_ALGOS` (mới thêm, mặc định =
> đủ 15) cho phép quay nhanh **không sửa file** — vẫn giữ ma trận đầy đủ cho số thật.

> **Đã kiểm chứng & sửa (lần chạy thử toàn bản này):** §4.5 `make tls` và §4.6
> `bench_tls_nginx.sh` trước đây **treo vô hạn** do `wait` chờ luôn cả tiến trình
> server nền — **đã sửa** thành `wait` đúng các worker; §4.6 `nginx_bench.conf`
> thiếu thay `__LISTEN__` khiến nginx **không khởi động** — **đã sửa** trong
> `bench_tls_nginx.sh`. Lệnh kiểm `nginx -T` ở §4.6 và đoạn gộp tay ở §4.9 cũng đã
> chỉnh cho khớp thực tế (xem từng mục).

---

## §4. KỊCH BẢN MÁY x86_64 (Ubuntu)

### 4.0 — Lấy repo + cài gói + bật môi trường
**Là gì:** tải mã, cài toolchain/đo/phân tích, trỏ PATH về OpenSSL của ta.
**Tại sao:** ba đợt gói ánh xạ đề §8.2 (toolchain), §7.4 (`/usr/bin/time`, `perf`),
§16 (`matplotlib`). **Cố ý không cài `libssl-dev`** — trộn header hệ thống với bản
tự build là lớp bug nguy hiểm nhất.
**Lệnh quay:**
```sh
git clone <URL-repo> && cd <ten-repo> && chmod +x scripts/*.sh
bash scripts/install_prereqs.sh
source scripts/setenv.sh
which openssl            # phai tra ve ~/pqc/openssl/bin/openssl
```
**Kiểm đúng (tool có sẵn):** `which openssl` trỏ vào `~/pqc/...` (không phải
`/usr/bin/openssl`); nếu sai → chưa `source setenv.sh`.
**Nói trên video:** “Mọi phần mềm TLS sẽ thuê OpenSSL ở đây, nên phải bật môi
trường trước.”

### 4.1 — Build OpenSSL 3.6.2 (nền PQC-native) + cổng kiểm
**Đo/là gì:** dựng OpenSSL 3.6.2 vào `~/pqc/openssl` (Ubuntu chỉ có 3.0, **không**
có ML-KEM/ML-DSA).
**Tại sao:** không có thư viện 3.5+ thì **mọi phép đo PQC bất khả**. `verify_env.sh`
là *cổng chặn* chống lỗi “fail im lặng” (âm thầm đo trên OpenSSL 3.0).
**Lệnh quay:**
```sh
SKIP_TESTS=1 bash scripts/build_openssl.sh   # neu da build truoc; lan dau bo SKIP de chay test
bash scripts/verify_env.sh                   # phai in PASS
```
**Kiểm đúng (tool có sẵn):**
```sh
openssl version                              # OpenSSL 3.6.2
openssl list -kem-algorithms | grep ML-KEM   # thay ML-KEM-512/768/1024
cat docs/env_report_x86_64.txt               # bien ban: compiler, co, CPU, governor
```
**Nói trên video:** “verify_env PASS nghĩa là OpenSSL đang dùng thật sự có ML-KEM.”

### 4.2 — WP2: microbenchmark K-batch (số liệu xương sống → RQ1)
**Đo gì:** latency từng phép (keygen/encaps/decaps/sign/verify) cho 15 thuật toán,
qua **EVP API** chung, đồng hồ **CLOCK_MONOTONIC_RAW**.
**Tại sao:** đo qua cùng một “mặt tiền” EVP → chênh lệch là **của thuật toán**,
không phải của cách gọi. RAW = không bị NTP/kernel nắn giờ. Ghim xung CPU vì
governor tiết kiệm điện làm hai lần đo ra hai số khác nhau.
**Lệnh quay (DEMO-FAST — 2 batch, warmup=0, iter=keygen=1, ~2 giây):**
```sh
sudo cpupower frequency-set -g performance   # VM/container khong co cpufreq -> bo qua (env_report ghi 'unavailable')
ARCH=$(uname -m)
for i in 1 2; do                                                 # demo: K=2 batch
  MICRO_ALGOS="mlkem 768;mldsa 65;rsa 3072" \
    BENCH_WARMUP=0 BENCH_ITERS=1 BENCH_KEYGEN_ITERS=1 make bench  # subset 3 algo -> nhanh
  cp data/summary_micro_${ARCH}.csv data/raw/${ARCH}/summary_batch$i.csv
  # GIU raw+kv tung thuat toan THEO batch (neu khong, batch sau GHI DE batch truoc):
  for f in data/raw/${ARCH}/*.raw.csv; do
    [ -e "$f" ] || continue
    a=$(basename "$f" .raw.csv); mkdir -p "data/raw/${ARCH}/$a"
    mv "$f" "data/raw/${ARCH}/$a/batch$i.raw.csv"
    [ -e "data/raw/${ARCH}/$a.kv.txt" ] && mv "data/raw/${ARCH}/$a.kv.txt" "data/raw/${ARCH}/$a/batch$i.kv.txt"
  done
done
```
**Lệnh “lấy số thật” (báo cáo — K=5 batch, đủ 15 thuật toán, BỎ `MICRO_ALGOS`):**
```sh
for i in 1 2 3 4 5; do
  BENCH_WARMUP=50 BENCH_ITERS=5000 BENCH_KEYGEN_ITERS=50 make bench
  cp data/summary_micro_$(uname -m).csv data/raw/$(uname -m)/summary_batch$i.csv
  # ... (vong archive raw+kv y het tren) ...
done
```
> **CẢNH BÁO RSA-15360 (mức 5, baseline) RẤT chậm:** **một** lần keygen ~30 s–vài
> phút (chạy kiểm: 2 batch × đủ 15 algo ở iter=keygen=1 = **7 phút 38 giây**, gần
> như toàn bộ là RSA-15360/7680 keygen). Vì vậy demo **nên** dùng `MICRO_ALGOS`
> (bỏ RSA-15360) như trên; muốn số thật cho mức 5 thì để keygen iters thấp (vd 5)
> hoặc đo RSA-15360 thành 1 batch riêng. `MICRO_ALGOS` mặc định = đủ 15 (không đặt
> = chạy ma trận đầy đủ, không đổi thiết kế thí nghiệm).
**Kiểm đúng (tool có sẵn):**
```sh
./build/bench_evp mlkem 768                   # chay thu 1 thuat toan
~/pqc/openssl/bin/openssl speed rsa2048 ecdsap256   # doi chung: cung co thi tin
```
bench_evp còn đọc bộ đếm chu kỳ (rdtsc) để **kiểm chéo**: tỉ lệ thời gian giữa hai
thuật toán phải khớp tỉ lệ chu kỳ.
**Nói trên video:** “Đây là số gốc cho RQ1 — PQC đắt hơn cổ điển bao nhiêu.”

### 4.3 — WP5: bộ nhớ (peak RSS) + kích thước mã
**Đo gì:** đỉnh RAM mỗi thuật toán; text/data/bss của thư viện.
**Tại sao:** `/usr/bin/time -v` (GNU time, **không** phải `time` builtin của shell)
mới biết RSS. `libcrypto.so` trộn mọi thuật toán nên **per-algorithm chỉ tin được
từ PQClean** (mỗi scheme một file `.a`).
**Lệnh quay:**
```sh
make memory      # -> data/memory_x86_64.csv (algo, peak_rss_kb) -- 15 algo (KHOP run_micro.sh; da sua tu 12)
make codesize    # -> data/codesize_x86_64.csv (file,text,data,bss,total)
bash scripts/fetch_pqclean.sh clean
# per-scheme code size = TONG cac object trong .a -> them '-t' de lay thang dong (TOTALS):
for a in ~/pqc/src/PQClean/crypto_*/ml-*/clean/lib*.a; do
  printf "%-30s " "$(basename "$a")"; size -t "$a" | tail -1
done | tee data/raw/x86_64/pqclean_clean_size.txt
```
**Kiểm đúng (tool có sẵn):**
- **Code size — per-algorithm (số đáng tin):** `size` trên `.a` in TỪNG object nên phải
  cộng lại; thêm `-t` để lấy thẳng dòng tổng:
  `size -t ~/pqc/src/PQClean/crypto_kem/ml-kem-768/clean/libml-kem-768_clean.a | tail -1`
  (dòng `(TOTALS)` = tổng text/data/bss của cả scheme). File `pqclean_*_size.txt` (lệnh
  `tee` ở trên) giờ **được `analyze.py` đọc** thành bảng `PQClean per-scheme code size`
  (clean vs avx2/aarch64) trong `tables.md` — xem §4.9.
- **Code size — thư viện trộn (chỉ tham khảo):** `size ~/pqc/openssl/lib*/libcrypto.so`
  (≈6.5 MB, đúng). ✅ Cột `libcrypto.a` trong `codesize_*.csv` **giờ đã đúng tổng**: đã
  sửa `measure_codesize.sh` từ `awk 'NR==2'` (chỉ lấy object ĐẦU → ra 2653 B) sang
  `size -t … | tail -1` (dòng `(TOTALS)` = tổng thật, chạy kiểm: **5,562,256 B ≈ 5.56 MB**).
  Lưu ý: `libcrypto.a/.so` vẫn là **thư viện trộn** mọi thuật toán, nên số **per-algorithm
  chỉ tin từ PQClean** (`size -t` trên `.a` từng scheme ở trên).
- **Memory (peak RSS):** chạy **tay** đúng lệnh mà `make memory` bọc, rồi so dòng
  "Maximum resident set size" với cột `peak_rss_kb`:
  ```sh
  /usr/bin/time -v ./build/bench_evp mldsa 65 2>&1 | grep "Maximum resident set size"
  ```
  (`time` builtin của shell **không** in dòng này — phải đúng `/usr/bin/time`.)
  ⚠ `/usr/bin/time` đến từ gói `time` (Ubuntu: `sudo apt install time`). Gói này nằm
  ở **đợt cài “không bắt buộc”** của `install_prereqs.sh` nên có thể vắng — thiếu nó
  `make memory` báo `Need GNU time at /usr/bin/time` và **không tạo CSV**. Cài lại nếu cần.
> **Lưu ý peak RSS:** cả 15 thuật toán đều ra **~8–9 MB** (chạy kiểm x86_64: rsa-3072
> 8440 KB, rsa-7680 8500 KB…; con số tuyệt đối tuỳ máy/bản build) vì RSS bị baseline
> `libcrypto` (~6.5 MB) lấn át; chênh vài trăm KB giữa các dòng là **nhiễu**, KHÔNG phải
> bộ nhớ riêng từng thuật toán. Tín hiệu bộ nhớ thật của PQC nằm ở **code size + kích
> thước khoá/CT**.
**Nói trên video:** “Băng thông/khoá lớn của PQC lộ ra ở đây — phần code size.”

### 4.4 — WP3: liboqs ref/opt (thí nghiệm SIMD → RQ2)
**Đo gì:** build liboqs **hai cây** chỉ khác đúng một biến (SIMD tắt/bật), rồi đo
`speed_kem`/`speed_sig`.
**Tại sao:** đây là **paired comparison** — cùng phiên bản, cùng máy, cùng compiler;
biến độc lập duy nhất là tối ưu vi kiến trúc. Trên x86 “opt” = **AVX2**.
**Lệnh quay:**
```sh
bash scripts/build_liboqs.sh ref      # SIMD TAT (OQS_OPT_TARGET=generic)
bash scripts/build_liboqs.sh opt      # -march=native, AVX2 BAT
bash scripts/run_liboqs_speed.sh      # -> data/liboqs_speed_x86_64.csv
```
**Kiểm đúng (tool có sẵn):**
```sh
# Dong "CPU exts compile-time:" la bang chung SIMD (chay kiem ra dung):
~/pqc/src/liboqs/build-ref/tests/speed_kem ML-KEM-768 | grep "CPU exts"   # -> SSE SSE2
~/pqc/src/liboqs/build-opt/tests/speed_kem ML-KEM-768 | grep "CPU exts"   # -> ... AVX AVX2 AVX512 ...
#   ^ hai banner khac nhau = bang chung hai cay khac dung cho (neu ten thu muc khac, thu: ls ~/pqc/src/liboqs/build-*)
head -5 data/liboqs_speed_x86_64.csv
```
**Nói trên video:** “Cùng máy, chỉ khác AVX2 — chênh lệch chính là giá trị của tối ưu.”
> Speedup ref→opt **phụ thuộc CPU**; chạy kiểm trên x86_64 ra **×3.2–×5.9** (KEM/keygen
> ~×3.2–3.7, sign ML-DSA tới ~×5.9). Đừng kỳ vọng đúng một con số cố định.

### 4.5 — WP4-A: TLS handshake, server = `s_server` (cô lập mật mã → RQ3)
**Đo gì:** latency (lặp handshake tuần tự) + throughput (thả nhiều client song song),
trên ma trận **[chứng thư × nhóm trao đổi khoá]**.
**Tại sao:** `s_server` là server **mỏng nhất** (gần như chỉ libssl) → số của nó
**cô lập chi phí mật mã** khỏi kiến trúc server. Tắt session resumption để mỗi kết
nối là **full handshake** (nếu không, từ kết nối 2 sẽ bỏ truyền cert — mất đúng chỗ
PQC khác biệt nhất).
**Lệnh quay:**
```sh
bash scripts/gen_tls_certs.sh         # 3 cert: rsa2048, ecp256, mldsa65
TLS_ITERS=3 TLS_WARMUP=1 TLS_CONC=2 TLS_DUR=1 make tls   # DEMO-FAST ~9s -> data/tls_handshake_x86_64.csv
# (lay so that: 'make tls' tran -> TLS_ITERS=50 TLS_CONC=4 TLS_DUR=10)
```
> **Đã sửa (lần chạy thử này):** trước đây `make tls` **treo vô hạn** ở pha throughput
> vì lệnh `wait` (không tham số) chờ luôn cả `s_server` chạy nền. Đã sửa thành chờ
> đúng các worker (`wait $WPIDS`) — giờ chạy xong ~9 s, in đủ 9 dòng (3 cert × 3 group).
**Kiểm đúng (tool có sẵn):** `make tls` đã dùng `-verify_return_error` nên mọi
handshake **đếm được** đều verify thành công. Muốn kiểm **độc lập** (không phụ thuộc
script), tự dựng s_server rồi nối bằng s_client — vì `make tls` *không* để server
chạy sẵn:
```sh
~/pqc/openssl/bin/openssl s_server -accept 4433 -www -tls1_3 \
  -cert ~/pqc/tls/mldsa65.cert.pem -key ~/pqc/tls/mldsa65.key.pem \
  -groups X25519MLKEM768 &                      # cua so server
echo Q | ~/pqc/openssl/bin/openssl s_client -connect 127.0.0.1:4433 -tls1_3 \
  -groups X25519MLKEM768 -CAfile ~/pqc/tls/mldsa65.cert.pem 2>&1 \
  | grep -E "Cipher|group|Temp Key|signature|Verify"
kill %1                                          # tat s_server
```
→ thấy (chạy kiểm, OpenSSL 3.6.2): `New, TLSv1.3, Cipher is TLS_AES_256_GCM_SHA384`,
`Negotiated TLS1.3 group: X25519MLKEM768`, `Peer signature type: mldsa65` (OpenSSL in
**chữ thường**, không phải `ML-DSA-65`), `Verify return code: 0 (ok)`. (Grep ở trên có
`Temp Key` chỉ để phòng hờ — bản 3.6.2 in `Negotiated TLS1.3 group` nên dòng đó có thể
trống, không sao.)
**Nói trên video:** “Đây là handshake PQC thật — nhóm hybrid + cert ML-DSA.”

### 4.6 — WP4-B: TLS handshake, `nginx` **HTTPS** (server sản xuất + trục đơn/đa luồng → RQ3)
**Đo gì:** ma trận **18 ô** = workers(1, auto) × cert(3) × group(3), trên cổng **HTTPS
8443** (nginx bật TLS).
**Tại sao:** đo là **handshake TLS** nên nginx **bắt buộc HTTPS** — config phải có
`listen 8443 ssl;` + `ssl_protocols TLSv1.3;` + `ssl_certificate`/`ssl_certificate_key`
+ nhóm khoá (`ssl_ecdh_curve`/`ssl_conf_command Groups`). HTTP trần (không `ssl`) **không
có handshake để đo** → sai đề. `worker_processes 1` vs `auto` chính là trục “single‑ vs
multi‑threaded server” §7.4 đòi. nginx “thuê” đúng OpenSSL ta build (link động) nên có PQC.
**Lệnh quay:**
```sh
bash scripts/build_nginx.sh
~/pqc/nginx/sbin/nginx -V             # phai in: built with OpenSSL 3.6.2 (nginx KHONG trong PATH)
WORKERS_LIST=1 TLS_ITERS=3 TLS_WARMUP=1 TLS_CONC=2 TLS_DUR=1 \
  bash scripts/bench_tls_nginx.sh     # DEMO-FAST ~9s -> data/tls_handshake_nginx-x86_64.csv
# (du truc don/da luong + so that: bo override -> WORKERS_LIST="1 auto", ITERS=50, DUR=10)
```
> **Đã sửa 2 lỗi (lần chạy thử này):** (1) `nginx_bench.conf` có placeholder
> `__LISTEN__` mà `bench_tls_nginx.sh` **quên thay** → config thành `listen
> __LISTEN__:8443 ssl;` → nginx chết ngay (`host not found in "__LISTEN__:8443"`) →
> **mọi ô đo fail**. Đã thêm biến `LISTEN` (mặc định `127.0.0.1`, đặt `LISTEN=0.0.0.0`
> cho LAN §5.9) + thay `__LISTEN__`. (2) cùng lỗi `wait` treo như §4.5 — đã sửa
> `wait $WPIDS`. Giờ chạy xong ~9 s, in đủ 9 dòng (1 worker × 3 cert × 3 group).
**Kiểm đúng (tool có sẵn) — gồm xác nhận ĐÚNG LÀ HTTPS, không phải HTTP:**
> ⚠ **Quan trọng (đã sửa so bản cũ):** `nginx -T`/`nginx -t` **trần** (không `-c`)
> đọc **config MẶC ĐỊNH** `~/pqc/nginx/conf/nginx.conf` (`listen 80;`, KHÔNG ssl) —
> KHÔNG phải config benchmark. `bench_tls_nginx.sh` lại render config vào thư mục tạm
> rồi **xoá** khi xong, nên muốn kiểm phải **tự render + tự chạy nginx**. Khối dưới làm
> đúng vậy (đã chạy kiểm: `nginx -t` PASS, `s_client` Verify=0, `curl` https 200 / http 400):
```sh
~/pqc/nginx/sbin/nginx -V 2>&1 | grep -i "built with OpenSSL"   # OpenSSL 3.6.x  <- thue dung nha
ldd ~/pqc/nginx/sbin/nginx | grep -i ssl                        # link .so phai tro ~/pqc/openssl
# 1) Render config benchmark THẬT (giong het bench_tls_nginx.sh, da co __LISTEN__):
RUN=/tmp/ngx_check; rm -rf "$RUN"; mkdir -p "$RUN/logs" "$RUN/html"; echo ok > "$RUN/html/index.html"
sed -e "s|__WORKERS__|1|" -e "s|__PORT__|8443|" -e "s|__LISTEN__|127.0.0.1|" \
    -e "s|__CRT__|$HOME/pqc/tls/mldsa65.cert.pem|" -e "s|__KEY__|$HOME/pqc/tls/mldsa65.key.pem|" \
    -e "s|__GROUPS__|X25519:X25519MLKEM768:MLKEM768|" \
    scripts/nginx_bench.conf > "$RUN/bench.conf"
# 2) Kiem CHINH config benchmark (phai thay 'listen 127.0.0.1:8443 ssl' + TLSv1.3):
~/pqc/nginx/sbin/nginx -p "$RUN" -c bench.conf -T 2>&1 | grep -E "listen|ssl_protocols|ssl_certificate"
~/pqc/nginx/sbin/nginx -p "$RUN" -c bench.conf -t          # syntax ok; group bia -> SSL_CTX_set1_curves_list failed
# 3) Chay nginx (daemon off -> phai co '&') roi bat tay TLS that:
~/pqc/nginx/sbin/nginx -p "$RUN" -c bench.conf &           # NGINX phai DANG CHAY cho buoc nay
for _ in $(seq 1 20); do curl -k -s https://127.0.0.1:8443/ >/dev/null 2>&1 && break; sleep 0.3; done
echo | ~/pqc/openssl/bin/openssl s_client -connect 127.0.0.1:8443 -tls1_3 \
  -groups X25519MLKEM768 -CAfile ~/pqc/tls/mldsa65.cert.pem 2>&1 \
  | grep -E "Protocol|Cipher|Negotiated|Verify return"     # TLSv1.3 + Verify return code: 0
curl -k https://127.0.0.1:8443/ -o /dev/null -s -w "https: %{http_code}\n"   # 200
curl     http://127.0.0.1:8443/ -o /dev/null -s -w "http:  %{http_code}\n"   # 400 (nginx tu choi HTTP tran tren cong TLS)
~/pqc/nginx/sbin/nginx -p "$RUN" -c bench.conf -s stop 2>/dev/null           # tat nginx
```
→ `nginx -c bench.conf -T` cho thấy **`listen 127.0.0.1:8443 ssl;` + `ssl_protocols
TLSv1.3;`** (config thật); `s_client` ra `TLSv1.3` + `Verify return code: 0` = nginx
phục vụ **HTTPS**; `curl http://…:8443` ra **400** vì cổng chỉ nói TLS. Đó là bằng
chứng cứng nginx là HTTPS chứ không phải HTTP.
**Nói trên video:** “nginx chạy **HTTPS/TLS 1.3** trên 8443 (cert ML‑DSA, group hybrid);
so với phương án A, hiệu số = giá của một server thật + đa lõi.”

### 4.7 — WP4-C: cặp `tls_mini_server` + `tls_timer_client` (self-implemented của repo → RQ3)
**Đo gì:** server tự viết (TCP + **HTTPS**: TLS qua `SSL_accept` rồi trả HTTP tối giản
+ điểm đo) in mỗi kết nối một dòng `conn,tls,group,handshake_us`; client tự viết lặp
`SSL_connect` **trong tiến trình**, bấm giờ từng cái.
**Tại sao:** `s_client` spawn một tiến trình mỗi handshake (cộng vài ms fork/exec) —
đo trong-tiến-trình cho số **sạch cỡ paper**. Hai phía đo hai nửa: client =
handshake + round-trip; server = đúng đoạn `SSL_accept`; hiệu = chi phí mạng.
**Lệnh quay (2 cửa sổ):**
```sh
make tlsmini tlsclient
# cua so 1 (server):
./build/tls_mini_server ~/pqc/tls/mldsa65.cert.pem ~/pqc/tls/mldsa65.key.pem 9443 \
  | tee data/raw/x86_64/tlsmini_handshakes.csv
# cua so 2 (client):
./build/tls_timer_client 127.0.0.1 9443 X25519MLKEM768 50 ~/pqc/tls/mldsa65.cert.pem
```
> **Dừng server:** cửa sổ khác chạy `pkill -f tls_mini_server`. **Ctrl-C KHÔNG dừng được**
> (handler `signal()` + glibc SA_RESTART làm `accept()` khởi động lại). 50 dòng CSV đã được
> `tee` ghi (flush từng dòng) nên dừng kiểu nào cũng không mất số.

**Kiểm đúng (tool có sẵn):** cột `group` trong CSV = `X25519MLKEM768` (qua
`SSL_get_negotiated_group`) → **bằng chứng cứng** phiên đo thật sự chạy PQC; ép TLS
1.2 phải bị từ chối bằng alert.
**Nói trên video:** “Đây là server tự viết theo yêu cầu §7.6 — vẫn thuê libssl làm crypto.”

### 4.8 — Track D: TLS 1.3 **tự cuộn protocol** (thầy yêu cầu thêm — vẫn là đo handshake → RQ3)
**Là gì:** khác A/B/C (thuê *toàn bộ* protocol từ libssl), track D **tự viết
protocol** (record layer + key schedule + từng message handshake theo RFC 8446 +
illustrated-tls13); chỉ **primitive** (X25519/AES-GCM/HKDF/ECDSA) mượn libcrypto.
Đây là **TLS thuần**: phần đo là **handshake TLS 1.3**, còn app‑layer chỉ là ping/pong
tối giản (không phải HTTP) — handshake giống hệt dù app‑layer là gì, nên không ảnh
hưởng số đo. (Track *handshake HTTPS* là nginx ở §4.6; track D đo thẳng handshake.)
**Tại sao:** đây là bản tự triển khai **sâu nhất** — không gì trong handshake giấu
trong thư viện, nên đo được **từng pha** và sau này swap classical↔PQC ngay trong
code. Ranh giới: **không tự chế crypto** (đúng định luật người thuê nhà).
**Đo gì:** (giống A/B/C, cùng thước đo)
- **Latency phía client**: TOTAL + tách pha keygen/ECDHE/key-sched/sig-verify.
- **Latency phía server**: CSV `conn,suite,group,handshake_us` (đoạn ClientHello→client
  Finished verified = tương đương bấm giờ `SSL_accept`).
- **Throughput** dưới C client đồng thời; **server đơn vs đa luồng** qua `--threads N`.
**Lệnh quay (thư mục `tls13-scratch/`):**
Demo-fast dưới đã chạy kiểm (~5 s tổng). Số trong ngoặc là bản “lấy số thật”.
```sh
make                                         # build server/server + client/client
make repro                                   # bang chung byte-exact vs RFC + repo
make cert                                    # cert trong server/ (lan dau)
mkdir -p ../data/raw/x86_64                   # de redirect CSV khong loi (neu chua co)
# latency (client): server count = warmup + iters = 5 + 20 = 25  (that: 100+1000=1100)
( cd server && ./server 8400 25 ) &
cd client && BENCH_ITERS=20 ./client 127.0.0.1 8400 --bench --warmup 5 \
  --csv ../../data/raw/x86_64/tlsmini_d_latency.csv; cd ..   # --csv -> so vao data/, KHONG tao handshake_bench.csv trong source
# throughput + server-side CSV, don luong: server count = client --total = 200 (that: 20000)
( cd server && ./server 8400 200 --threads 1 > ../../data/raw/x86_64/tlsmini_d_st.csv 2> run.log ) &
( cd client && ./client 127.0.0.1 8400 --load --threads 8 --total 200 --csv ../../data/raw/x86_64/load_d_st.csv )
# da luong (so sanh):
( cd server && ./server 8400 200 --threads $(nproc) > ../../data/raw/x86_64/tlsmini_d_mt.csv 2> run.log ) &
( cd client && ./client 127.0.0.1 8400 --load --threads 8 --total 200 --csv ../../data/raw/x86_64/load_d_mt.csv )
```
> Lưu ý: server `count` phải **bằng** client `--total` (hoặc `--warmup + iters` ở chế
> độ `--bench`); server phục vụ đúng ngần ấy kết nối rồi thoát; lệch nhau → server chờ
> kết nối không tới và treo. (Chạy kiểm: 25 và 200 khớp đúng, không treo.)
**Kiểm đúng (tool có sẵn):**
```sh
make repro          # in: ALL MATCH -> byte-identical to illustrated-tls13 + RFC 8446
# interop voi OpenSSL that lam client (chung minh handshake dung chuan):
( cd server && ./server 8400 ) &
echo ping | ~/pqc/openssl/bin/openssl s_client -connect 127.0.0.1:8400 -tls1_3 \
  -ciphersuites TLS_AES_256_GCM_SHA384 -groups X25519 -CAfile server/server.crt -quiet
# doi chung pha crypto:
~/pqc/openssl/bin/openssl speed ecdhx25519 ecdsap256
```
`make repro` tái lập đúng từng byte `keylog.txt` của illustrated-tls13 (chạy kiểm in
`ALL MATCH -> byte-identical to illustrated-tls13 + RFC 8446`) → chứng minh key
schedule khớp RFC; `s_client` (có `-quiet`) in `depth=0 ... verify return:1` rồi nhận
**“pong”** → handshake tự viết đúng chuẩn. (Lưu ý: `-quiet` **giấu** dòng tổng `Verify
return code: 0 (ok)`; bằng chứng ở đây là chữ “pong” + `verify return:1`. Bỏ `-quiet`
nếu muốn thấy dòng `Verify return code: 0`.) Số pha (ECDHE ~100µs, chạy kiểm) cùng cỡ
`openssl speed` (X25519 ~27k op/s) — chênh do tạo/huỷ object EVP mỗi handshake.
**Nói trên video:** “Track D tự viết protocol; `make repro` chứng minh đúng từng
byte so RFC, rồi đo handshake cùng cách A/B/C.”

### 4.9 — analyze: từ CSV ra bảng + biểu đồ (gộp 5 batch NGAY trong analyze)
**Lệnh quay:** `make analyze`  → `analysis_out/tables.md` + `*.png`.

**Gộp K-batch (median-of-medians) — KHÔNG script riêng, KHÔNG `cp` đè file canonical:**
`scripts/analyze.py` tự dò `data/raw/<arch>/summary_batch*.csv`. Có ≥1 batch → nó
**tự gộp**: lấy *median qua các batch* cho từng `(algo, metric)`, ghi ra rồi VẼ luôn từ bản gộp:
- `data/summary_agg_<arch>.csv` — `algo,metric,value` (value = median-of-medians = **số chính thức**)
- `data/summary_agg_<arch>_spread.csv` — `n,median,mean,min,max,`**`cv_pct`** (cv cao = nhiễu, vd RSA keygen)

Không có batch nào → tự fallback `summary_micro_<arch>.csv` (1 lần đo). Vì gộp nằm
**trong** analyze nên **mỗi file một vai trò**, **bỏ hẳn** bước cũ `cp summary_agg →
summary_micro` (đè file canonical). `summary_micro` vẫn là "batch mới nhất", `summary_agg` là "bản gộp".

**Gộp tay trên terminal (đối chứng — phải BYTE-IDENTICAL với `make analyze`):**
> ⚠ **Sửa quan trọng:** bản cũ format `{:.6g}` nên **KHÔNG** khớp `analyze.py` (analyze
> dùng `fmt_val`: số nguyên giữ nguyên, không ký hiệu khoa học). Chạy kiểm cho thấy
> bản `.6g` lệch **227 dòng** (vd `3220048` → `3.22005e+06`, `234592.5` → `234592`).
> Bản cũ còn **ghi đè** thẳng `data/summary_agg_*.csv` (file chính). Bản dưới dùng đúng
> `fmt_val`, ghi ra `/tmp` rồi `diff` — chạy kiểm: **IDENTICAL** cả `agg` lẫn `_spread`.
```sh
python3 - "$(uname -m)" <<'PY'
import csv, statistics, glob, sys
from collections import defaultdict
def fmt_val(x):                       # GIONG HET analyze.py (khong dung .6g)
    x = float(x)
    if x == int(x): return str(int(x))
    if abs(x) >= 1: return f"{x:.3f}".rstrip("0").rstrip(".")
    return f"{x:.4g}"
arch = sys.argv[1]; s = defaultdict(list)
for f in sorted(glob.glob(f"data/raw/{arch}/summary_batch*.csv")):
    for r in csv.DictReader(open(f)):
        try: s[(r["algo"], r["metric"])].append(float(r["value"]))
        except ValueError: pass
with open("/tmp/agg_chk.csv","w",newline="") as agg, open("/tmp/spread_chk.csv","w",newline="") as spr:
    agg.write("algo,metric,value\n"); spr.write("algo,metric,n,median,mean,min,max,cv_pct\n")
    for (a, m), v in sorted(s.items()):
        md = statistics.median(v); mn = statistics.fmean(v)
        sd = statistics.stdev(v) if len(v) > 1 else 0.0; cv = (100*sd/mn) if mn else 0.0
        agg.write(f"{a},{m},{fmt_val(md)}\n")
        spr.write(f"{a},{m},{len(v)},{fmt_val(md)},{fmt_val(mn)},{fmt_val(min(v))},{fmt_val(max(v))},{cv:.2f}\n")
print(f"wrote /tmp/agg_chk.csv (+ /tmp/spread_chk.csv) tu {len(glob.glob(f'data/raw/{arch}/summary_batch*.csv'))} batch")
PY
# doi chung: phai khong co dong nao lech
diff data/summary_agg_$(uname -m).csv /tmp/agg_chk.csv && echo "AGG IDENTICAL voi analyze"
diff data/summary_agg_$(uname -m)_spread.csv /tmp/spread_chk.csv && echo "SPREAD IDENTICAL voi analyze"
```

**Analyze gom HẾT mọi cách đo:** `make analyze` quét **cả** `data/*.csv` LẪN
`data/raw/<arch>/*.csv` (kể cả `pqclean_*_size.txt`) → **9 nhóm bảng**: WP2 micro
(median-of-medians), WP5 peak-RSS + code-size lib trộn + **PQClean per-scheme code size**
(`## PQClean per-scheme code size` đọc từ `pqclean_*_size.txt` — số code-size
**per-algorithm** đáng tin, có cột `impl` clean/avx2/aarch64), WP3 liboqs ref/opt, và
**WP4 cả 4 cách** — A `s_server` (`TLS 1.3 handshake (<arch>)`), B nginx (`...
(nginx-<arch>)`), **C `tls_mini` + D track-D** (`Self-implemented TLS handshake -
methods C/D` đọc từ `tlsmini_handshakes.csv` / `tlsmini_d_*` / `load_d_*`), cùng **D
phân pha** (`Track D handshake phases` từ `tlsmini_d_latency.csv`).

**Kiểm đúng:** mở `analysis_out/tables.md`; bảng “liboqs ref vs opt” có speedup >×1
(chạy kiểm x86_64: ×3.2–×5.9) nghĩa là chuỗi WP3 thông suốt từ binary tới biểu đồ.
Demo 2 batch → analyze in `median-of-medians over 2 batch(es)` và sinh
`data/summary_agg_x86_64.csv` (+ `_spread.csv`); bản “lấy số thật” 5 batch in `over 5
batch(es)`. Chạy kiểm: analyze sinh **9 nhóm bảng + ~30 PNG** (gồm bảng PQClean
per-scheme; cần `matplotlib`, thiếu thì chỉ có bảng — `pip install matplotlib` hoặc
gói `python3-matplotlib`).
**Nói trên video:** “Toàn bộ số gom thành bảng đối chiếu giả thuyết; 5 batch được gộp
median-of-medians **ngay trong analyze** — không file thừa, không cp đè.”

### 4.10 — (Tùy chọn) đo mở rộng + Docker tái lập + nghiệm thu x86
```sh
# bytes that tren day (cho thay cert PQC to hon) - can mot s_server dang chay (xem 4.5):
sudo tshark -i lo -f "tcp port 4433" -a duration:5 -q -z conv,tcp
# wrk: CHI dung doi chung tren hang X25519 (wrk cua distro link OpenSSL he thong, khong co ML-KEM):
wrk -t4 -c16 -d10s https://127.0.0.1:8443/
# tai nang hon:
TLS_CONC=8 TLS_DUR=15 make tls
# tai lap: docker/Dockerfile.x86_64 GIO DA CO san trong repo (build duoc; chi la bang
#          chung dong goi, KHONG do trong container). scripts/docker_buildx.sh tro dung file nay.
# docker build -f docker/Dockerfile.x86_64 -t nt219-pqc .   # (can Docker; mac dinh moi truong khong co)
git add data analysis_out docs scripts && git commit -m "x86 measurements" && git push
```
> **Định luật:** không bao giờ đo trong Docker/QEMU — số của trình giả lập. `wrk`
> chỉ là **đối chứng công cụ** trên hàng cổ điển, **không** dùng cho số PQC.

---

## §5. KỊCH BẢN MÁY ARM (Raspberry Pi 4, aarch64)

### 5.0 — Vì sao phải có máy ARM THẬT (đọc trước khi quay)
- **Hiệu năng là thuộc tính phần cứng** (cache, pipeline, SIMD). QEMU cho binary
  chạy được nhưng **số là của trình giả lập**; cross-compile cho binary nhưng
  **không cho số nào**. → Số ARM **chỉ** đến từ máy ARM thật.
- **NEON = “AVX2 của ARM”** (vector 128-bit, hẹp hơn AVX2 256-bit). Cặp ref/opt
  trên Pi = thí nghiệm NEON = **trọng tâm RQ2**.
- **Cortex-A72 không có Crypto Extensions** → cặp ref/opt **sạch**, chênh lệch toàn
  bộ là NEON, không lẫn AES phần cứng.
- **Nhiệt**: Pi throttle (tự hạ xung) ~80°C → phải **nghỉ nguội** giữa batch.
- **Mạng IPv6-only/NAT64**: Pi thuê thường chỉ có IPv6; clone treo → kiểm
  `ping6 github.com`.

### 5.1 — Lấy repo + cài gói + build OpenSSL + cổng kiểm
**Tại sao khác x86:** Pi build chậm → dùng `SKIP_TESTS=1` hằng ngày, **một lần qua
đêm** chạy đủ test làm bằng chứng trên chính kiến trúc này.
```sh
git clone <URL-repo> && cd <ten-repo> && chmod +x scripts/*.sh
bash scripts/install_prereqs.sh
SKIP_TESTS=1 bash scripts/build_openssl.sh
source scripts/setenv.sh
bash scripts/verify_env.sh        # PASS -> docs/env_report_aarch64.txt
make && make tlsmini tlsclient
```
**Kiểm đúng:** `openssl version` → 3.6.2; `cat docs/env_report_aarch64.txt` (biên
bản **riêng** của máy này — đề §7.3 đòi tài liệu hoá từng nền tảng).

### 5.2 — WP2 với vòng NGHỈ-NGUỘI (giải thích từng dòng)
```sh
sudo cpupower frequency-set -g performance        # (1) ghim xung
for i in 1 2 3 4 5; do                            # (2) K=5 batch
  BENCH_ITERS=1500 make bench                      # (3) vong THAP hon x86 (1500 vs 5000): nhiet leo it
  cp data/summary_micro_aarch64.csv data/raw/aarch64/summary_batch$i.csv   # (4)
  # (4b) GIU raw+kv tung thuat toan THEO batch (NHU x86 §4.2; thieu thi batch sau GHI DE batch truoc):
  for f in data/raw/aarch64/*.raw.csv; do
    [ -e "$f" ] || continue
    a=$(basename "$f" .raw.csv); mkdir -p "data/raw/aarch64/$a"
    mv "$f" "data/raw/aarch64/$a/batch$i.raw.csv"
    [ -e "data/raw/aarch64/$a.kv.txt" ] && mv "data/raw/aarch64/$a.kv.txt" "data/raw/aarch64/$a/batch$i.kv.txt"
  done
  cat /sys/class/thermal/thermal_zone0/temp        # (5) doc nhiet (70000 = 70.0C)
  sleep 120                                         # (6) nghi 2 phut cho ha nhiet
done
```
> **DEMO-FAST trên Pi:** muốn quay nhanh, dùng đúng bộ như §4.2 — `for i in 1 2; do
> MICRO_ALGOS="mlkem 768;mldsa 65;rsa 3072" BENCH_WARMUP=0 BENCH_ITERS=1
> BENCH_KEYGEN_ITERS=1 make bench; ...; sleep 5; done` (bỏ RSA-15360 nên không cần
> nghỉ nguội lâu). Bản “lấy số thật” mới dùng `BENCH_ITERS=1500` + `sleep 120` như trên.
**Tại sao:** vòng thấp + nghỉ nguội giữ Pi **dưới ngưỡng throttle** để batch sau
không chậm hơn batch trước **vì nhiệt** (chứ không phải vì thuật toán). Vòng `(4b)`
**y hệt §4.2 (x86)**: gom raw+kv từng thuật toán theo batch vào `data/raw/aarch64/<algo>/`
— thiếu nó thì batch sau ghi đè raw batch trước (chỉ còn batch 5).
**Kiểm đúng:** bench_evp tự nhận ARM lúc compile — bộ đếm chu kỳ chuyển từ `rdtsc`
(x86) sang `cntvct` (ARM); cùng một mã nguồn, hai kiến trúc.

### 5.3 — WP3: liboqs ref/opt = thí nghiệm NEON (LÝ DO TỒN TẠI của máy này → RQ2)
```sh
bash scripts/build_liboqs.sh ref       # NEON TAT tuong minh
bash scripts/build_liboqs.sh opt       # NEON BAT (-mcpu=native)
bash scripts/run_liboqs_speed.sh       # can DU ref+opt
```
**Kiểm đúng:** banner cây opt phải in `NEON`; thiếu một cây → bảng “ref vs opt
(aarch64)” sẽ vắng trong `tables.md`.
**Nói trên video:** “Trên x86 opt là ‘nên có’; trên ARM nó **là** câu trả lời RQ2.”

### 5.4 — WP5 + chứng thư (sinh LẠI tại máy)
```sh
make memory && make codesize
bash scripts/fetch_pqclean.sh clean
bash scripts/fetch_pqclean.sh aarch64   # truc NEON thu hai (assembly NTT - CHI build duoc tren ARM)
# per-scheme total (clean vs aarch64/NEON) qua 'size -t' (dong TOTALS):
for a in ~/pqc/src/PQClean/crypto_kem/ml-kem-768/{clean,aarch64}/lib*.a; do
  printf "%-40s " "$a"; size -t "$a" | tail -1
done | tee data/raw/aarch64/pqclean_neon_size.txt
bash scripts/gen_tls_certs.sh           # khoa la tai san theo may, khong copy cheo
```

### 5.5 — WP4 A/B/C (lệnh y hệt x86)
```sh
TLS_ITERS=3 TLS_WARMUP=1 TLS_CONC=2 TLS_DUR=1 make tls   # A: s_server (DEMO-FAST)
bash scripts/build_nginx.sh             # Pi ~5-10 phut (KHONG rebuild OpenSSL - link dong)
WORKERS_LIST=1 TLS_ITERS=3 TLS_WARMUP=1 TLS_CONC=2 TLS_DUR=1 \
  bash scripts/bench_tls_nginx.sh       # B: nginx (DEMO-FAST)
# C: tls_mini_server + tls_timer_client (2 cua so, nhu 4.7)
```
> **Áp dụng cả hai bản sửa §4.5/§4.6** (đã vá trong script, dùng chung cho ARM): `make
> tls` và `bench_tls_nginx.sh` không còn treo ở `wait`; nginx có `__LISTEN__` đúng. Lệnh
> kiểm nginx HTTPS: render + chạy như §4.6 (bench config, không phải `nginx -T` trần).

### 5.6 — Track D trên ARM (TLS tự viết — đo handshake)
**Lệnh quay (trong `tls13-scratch/`):**
```sh
make && make repro                      # repro byte-exact tren ARM (in ALL MATCH)
make cert
mkdir -p ../data/raw/aarch64
# DEMO-FAST (count = warmup+iters = 25; total = 200) -- nhu §4.8; ban that: 1100 / 20000
( cd server && ./server 8400 25 ) &
( cd client && BENCH_ITERS=20 ./client 127.0.0.1 8400 --bench --warmup 5 \
    --csv ../../data/raw/aarch64/tlsmini_d_latency.csv )   # --csv -> data/, tranh handshake_bench.csv rac
( cd server && ./server 8400 200 --threads $(nproc) > ../../data/raw/aarch64/tlsmini_d_mt.csv 2> run.log ) &
( cd client && ./client 127.0.0.1 8400 --load --threads 8 --total 200 --csv ../../data/raw/aarch64/load_d_mt.csv )
```
**Kiểm đúng:** `make repro` ALL MATCH (cùng giá trị vàng trên ARM vì là số học, không
phụ thuộc kiến trúc); interop `openssl s_client` như §4.8.
**Nói trên video:** “Track D chạy y hệt trên ARM — đo cùng handshake, khác phần cứng.”

### 5.7 — analyze + bảng cross-platform + push
```sh
make analyze    # tu gop summary_batch* aarch64 -> summary_agg_aarch64 (median-of-medians, nhu §4.9)
git add data analysis_out docs && git commit -m "aarch64 measurements" && git push
```
**Kiểm đúng:** bảng **Cross-platform ratio** chỉ xuất hiện khi `data/` có CSV của
**cả hai máy** (kéo về một chỗ qua git).

### 5.8 — Cross-compile (bằng chứng năng lực, KHÔNG phải nguồn số) — chạy trên PC x86
```sh
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
bash scripts/cross_build_liboqs.sh
file ~/pqc/liboqs-aarch64-cross/lib/liboqs.so*   # phai in: ARM aarch64
```
**Tại sao:** đề §7.3 đòi script cross-compile; binary sinh ra là **generic ARM**,
**không bao giờ** là nguồn số đo.
**Nói trên video:** “Đây chỉ chứng minh biết đóng gói cho ARM — số thật vẫn từ Pi.”

### 5.9 — Phần III: gộp 2 máy + báo cáo + demo.sh + (tùy chọn) đo LAN
Sau khi **cả hai máy đã push** số lên git:
```sh
git pull && make analyze        # gop CSV 2 may -> bang Cross-platform ratio MOI xuat hien
cd docs/report && xelatex main.tex && xelatex main.tex   # bao cao (chay 2 lan cho muc luc/hinh)
bash scripts/demo.sh            # repo CO SAN script quay 3 man ~2 phut (Deliverable 5)
```
**Đo qua LAN (mở rộng RQ3 — chênh với loopback = chi phí mạng thật):**
```sh
# may SERVER:
sudo ufw allow 8443/tcp
#   nginx: dat __LISTEN__=0.0.0.0   |   server tu viet repo: BIND_ALL=1
# may CLIENT: chep cert cong khai sang roi do:
scp user@<IP-server>:~/pqc/tls/mldsa65.cert.pem .
./build/tls_timer_client <IP-server> 8443 X25519MLKEM768 50 mldsa65.cert.pem
```
**Kiểm đúng:** `make repro`/`s_client` như trên; bảng Cross-platform ratio xuất hiện
đúng = `data/` đã có CSV **cả hai máy**. Qua LAN, `ClientHello ~1KB` của ML-KEM có
thể lộ chuyện phân mảnh gói — đó là dữ liệu cho RQ1/RQ3.
**Nói trên video:** “Bảng tỉ lệ cross-platform + demo.sh là phần tổng kết; kết luận
RQ1/RQ2/RQ3 đọc từ `analysis_out/tables.md`.”

> **Lưu ý quan trọng về cách so 2 máy** (định luật bốn-con-số): **không** kết luận
> “ARM chậm hơn x86 N lần là do PQC” — đó là do hai CPU. Câu trả lời RQ2 nằm ở
> **so tỉ lệ speedup ref→opt** của hai máy (NEON kéo được hệ số ≈ AVX2 thì PQC khả
> thi trên SBC).

---

## §6. Bảng KIỂM ĐÚNG bằng tool có sẵn (tổng hợp)

| Task | Tool có sẵn | Kỳ vọng PASS |
|------|-------------|--------------|
| Môi trường | `which openssl`, `openssl version`, `verify_env.sh`, `ldd` | trỏ `~/pqc/openssl`; in `3.6.2`; PASS; binary link đúng OpenSSL ta |
| OpenSSL có PQC | `openssl list -kem-algorithms \| grep ML-KEM` | thấy ML-KEM-512/768/1024 |
| WP2 micro | `openssl speed`, cycles ratio, KAT | cùng cỡ với speed; tỉ lệ thời gian ≈ tỉ lệ chu kỳ |
| WP3 ref/opt | dòng `CPU exts compile-time:` của `speed_kem` | ref: `SSE SSE2`; opt: `… AVX AVX2 …`(x86)/`NEON`(ARM); speedup ref/opt phụ thuộc CPU (x86 chạy kiểm ×3.2–×5.9) |
| WP5 | `size -t` (TOTALS), `readelf -S`, `/usr/bin/time -v` | per-scheme: `size -t … \| tail -1` = tổng; cột `libcrypto.a` trong CSV **đã đúng tổng** (sửa `size -t`, ~5.56 MB) — nhưng vẫn là lib trộn, per-algorithm chỉ tin PQClean; "Maximum resident set size" khớp `peak_rss_kb` (15 algo, mọi algo ~8–9 MB = baseline) |
| WP4 A/B/C | `openssl s_client -CAfile` (cần server **đang chạy**, xem 4.5/4.6), `nginx -V` + `nginx -p RUN -c bench.conf -T/-t` (KHÔNG `-T` trần — nó đọc config mặc định `listen 80`), `curl http://…:8443`→400, `tshark -z conv,tcp` | `Verify return code: 0`; `nginx -c bench.conf -T` thấy `listen …:8443 ssl`+TLSv1.3; built with OpenSSL 3.6; group bịa → fail-loudly; bytes cert PQC lớn hơn (mldsa65 7517B ≫ rsa2048 1115B) |
| group thật | cột `group` trong CSV (`SSL_get_negotiated_group`) | `X25519MLKEM768` |
| **Track D** | `make repro`, `openssl s_client`, `openssl speed` | **ALL MATCH** (byte-exact); `Verify return code: 0` + “pong”; pha cùng cỡ speed |
| Cross-compile | `file <lib>` | `ARM aarch64` |
| Tái lập | `docs/openssl.commit`, Docker build | commit hash tồn tại; image tự-fail nếu hỏng |

*Ý tưởng chung:* mỗi phép đo có **một tool độc lập** xác nhận “số này đáng tin”.
Riêng phần TLS, công cụ vàng là `openssl s_client` (verify=0 nghĩa là client thật
chấp nhận handshake của ta), và với track D thêm `make repro` (đúng từng byte vs RFC).

---

## §7. Checklist quay (tick đủ mới nộp)

**Máy x86_64:**
- [ ] `verify_env` PASS + `openssl version` = 3.6.2
- [ ] file batch trong `data/raw/x86_64/` (demo: `summary_batch1..2`; số thật: `1..5`)
- [ ] `memory` / `codesize` / `liboqs_speed` / `tls_handshake*` CSV có số
- [ ] WP4: `make tls` & nginx **chạy xong** (không treo) + `s_client` Verify=0 trên cả 3 cert; `nginx -V` OpenSSL 3.6
- [ ] **Track D**: `make repro` ALL MATCH; latency + throughput (đơn & đa luồng)
- [ ] `analysis_out/tables.md` + PNG (cần `matplotlib`)
- [ ] đã `git push`

**Máy ARM:**
- [ ] 5 batch + **log nhiệt**; `liboqs_speed_aarch64.csv` đủ **cả ref+opt**
- [ ] PQClean `clean` + `aarch64` size
- [ ] WP4 + **Track D** trên ARM
- [ ] `env_report_aarch64.txt`; đã `git push`

**Tổng hợp:**
- [ ] `git pull && make analyze` → **bảng Cross-platform ratio** xuất hiện
- [ ] Nói rõ kết luận **RQ1 / RQ2 / RQ3** trên video

---

## §8. Lỗi hay gặp khi quay (đừng hoảng)

1. **Kết nối #1 chậm bất thường** (vd 7996µs vs ~1500µs các kết nối sau) → đó là
   **warm-up**, giải thích trên video, đừng cắt. Đây là lý do dùng K-batch + bỏ warm-up.
2. **Quên `source setenv.sh`** → `ldd nginx` văng `OPENSSL_3.2.0 not found` hoặc đo
   nhầm OpenSSL 3.0. Bật lại môi trường.
3. **Track D treo** khi server `count` ≠ client `--total` → đặt **bằng nhau**.
4. **Pi nóng** giữa batch → tăng `sleep`, đợi nhiệt `< 70000`.
5. **Group bịa** (gõ sai tên nhóm) → fail-loudly với `SSL_CTX_set1_curves_list
   failed`; đây là **tính năng** (chống “thuê nhầm nhà”), không phải lỗi của bạn.
6. **Đo trong QEMU/Docker** → cấm; chỉ dùng làm bằng chứng đóng gói.
7. **File nào bị đè, file nào giữ:** chạy lại **cùng một lệnh** đè file *canonical* của
   chính nó (`summary_micro_<arch>.csv`, `tls_handshake_<arch>.csv`,
   `tls_handshake_nginx-<arch>.csv`, `tlsmini_handshakes.csv`, `tlsmini_d_*.csv`, raw
   `<algo>.raw.csv`) — **cố ý** (analyze đọc bản mới nhất). **A/B/C/D KHÔNG đè nhau**
   (tên khác + tách `<arch>`). Chỉ lịch sử K-batch cần giữ tay: `summary_batch$i.csv`
   (cp) + `<algo>/batch$i.*` (archive loop §4.2 cho x86 **và** §5.2 cho ARM).
8. **Gộp 5 batch: KHÔNG script riêng, KHÔNG `cp` đè.** `make analyze` **tự** dò
   `data/raw/<arch>/summary_batch*.csv` → median-of-medians → `summary_agg_<arch>.csv`
   (+ `_spread.csv`; `cv_pct` cao = nhiễu, vd RSA keygen) → vẽ luôn từ bản gộp. Không
   có batch → fallback `summary_micro`. (Bản gộp tay trên terminal: xem §4.9.)
9. **File rác đừng commit** (đã chặn trong `.gitignore`): binary track-D
   `tls13-scratch/{server/server, client/client, common/repro_keylog}`, `server/server.crt`,
   và `handshake_bench.csv` (latency track-D khi quên `--csv`). Track-D latency **luôn**
   kèm `--csv ../../data/raw/<arch>/tlsmini_d_latency.csv` → số rơi vào `data/`, không vào source.
10. **Hai điểm script ĐÃ SỬA tận gốc (trước đây chỉ "đừng tin nhầm số"):** (a)
    `measure_codesize.sh` từng đọc `awk 'NR==2'` → cột `libcrypto.a` chỉ là object ĐẦU
    (2653 B). **Đã sửa** sang `size -t … | tail -1` (dòng `(TOTALS)`) → libcrypto.a +
    per-scheme `.a` ra **đúng tổng** (chạy kiểm libcrypto.a = 5.56 MB). Vẫn lưu ý:
    `libcrypto.a/.so` là lib trộn, per-algorithm chỉ tin PQClean; CSV có **2 dòng cùng tên
    `liboqs.so`** (ref ~5 MB vs opt/neon ~11 MB) — phân biệt bằng kích thước. (b)
    `measure_memory.sh` từng chạy 12 algo (có rsa-2048, thiếu rsa-7680/15360 + p521).
    **Đã sửa** thành **15 algo y hệt `run_micro.sh`** (bảng RSS khớp 1:1 bảng WP2); keygen
    iters hạ xuống 2 để RSA-15360 không kéo dài pass.

> **11–13: ba lỗi đã PHÁT HIỆN & SỬA khi chạy thử toàn bản (đừng để tái phát):**

11. **`make tls` & `bench_tls_nginx.sh` từng TREO VÔ HẠN** (không ra dòng nào, CSV chỉ có
    header). Nguyên nhân: pha throughput gọi `wait` (không tham số) → chờ luôn cả tiến
    trình **server nền** (s_server/nginx chạy mãi). **Đã sửa** → `wait $WPIDS` (chỉ chờ
    worker). Nếu thấy treo lại: kiểm `wait` trong hai script có còn tham số PID không.
12. **`bench_tls_nginx.sh` nginx không khởi động** vì `nginx_bench.conf` có `__LISTEN__`
    mà script **quên thay** → `listen __LISTEN__:8443 ssl;` → `host not found`. **Đã sửa**:
    thêm biến `LISTEN` (mặc định `127.0.0.1`; `LISTEN=0.0.0.0` cho LAN) + thay trong `sed`.
    Lệnh kiểm `nginx -T`/`-t` **trần** vô dụng (đọc config mặc định `listen 80`) — phải
    `nginx -p RUN -c bench.conf -T` trên config đã render (§4.6).
13. **Đối chứng gộp K-batch tay (§4.9) từng KHÔNG khớp analyze** vì format `.6g` (lệch
    227 dòng: vd `3220048`→`3.22005e+06`). **Đã sửa**: dùng `fmt_val` y hệt analyze, ghi
    `/tmp` rồi `diff` (chạy kiểm: IDENTICAL). Đừng để bản `.6g` ghi đè `summary_agg_*`.

---

*Cẩm nang này là kịch bản quay Deliverable 5. Track D (TLS tự viết) ở §4.8/§5.6 là
một phần của phép đo handshake (RQ3), song song ba server thuê libssl A/B/C.*
