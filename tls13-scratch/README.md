# TLS 1.3 tự triển khai (RFC 8446) — track đo handshake riêng

Client + server TLS 1.3 viết tay bằng C, trong đó **bản thân giao thức được tự cuộn**
(record layer, key schedule, từng message handshake), khớp **byte‑for‑byte** với
**syncsynchalt/illustrated-tls13** và ánh xạ **RFC 8446**.

Đây là **track riêng, song song** trong đồ án NT219 (thầy yêu cầu thêm một bản TLS 1.3
*tự triển khai*). Nó đứng **cạnh** — không nằm trong — ba server RQ3 của repo, vốn đều
"thuê" trọn giao thức từ libssl:

| Server RQ3 (repo) | Ai viết giao thức TLS |
|-------------------|------------------------|
| A `s_server`            | OpenSSL libssl |
| B `nginx`               | OpenSSL libssl |
| C `tls_mini_server.c`   | OpenSSL libssl (mình chỉ sở hữu TCP + điểm đo) |
| **D — track này**       | **mình** (chỉ primitive lấy từ libcrypto) |

Track D là bản tự triển khai **sâu nhất**: không gì trong handshake bị giấu trong thư
viện, nên đo được **từng pha** và (về sau) swap classical↔PQC ngay mức giao thức. Ranh
giới **không** vượt: không tự chế thuật toán mã hoá — X25519 / AES‑256‑GCM / HKDF /
ECDSA lấy từ libcrypto, đúng "định luật người thuê nhà" và đúng cách
illustrated-tls13 / picotls / rustls làm.

- **cipher suite**: TLS_AES_256_GCM_SHA384 (suite illustrated-tls13 dùng)
- **key exchange**: X25519 (RFC 7748)
- **signature**: ecdsa_secp256r1_sha256 (hash chữ ký là SHA‑256, **độc lập** với SHA‑384 của suite)
- **phạm vi**: chỉ xác thực server (không client cert / PSK / 0‑RTT / HRR)
- **hiện classical**; lối mở rộng là hybrid X25519MLKEM768 + ML‑DSA‑65

## Tự viết gì vs mượn gì

`client/main.c` và `server/main.c` của chính repo illustrated-tls13 là **chương trình
OpenSSL mỏng** (`SSL_do_handshake` / `SSL_accept`) — libssl làm giao thức. Phần minh hoạ
byte‑level của repo đến từ OpenSSL vá khoá tất định + thư mục `captures/` + các demo
primitive trong `site/files/`.

Track này đi ngược lại: **tự cuộn giao thức** (record layer, key schedule, từng message)
và chỉ mượn **primitive** từ libcrypto — đúng các phép mà repo minh hoạ trong
`site/files/hkdf-384.sh`, `aes_256_gcm_*.c`, `curve25519-mult.c`.

## File (bố cục như illustrated-tls13: `server/` + `client/` riêng)

| đường dẫn | vai trò |
|-----------|---------|
| `common/tls13.h` `tls13.c` | engine dùng chung: primitive (HKDF/AEAD/X25519 qua libcrypto), record layer, key schedule |
| `common/repro_keylog.c` + `common/Makefile` | self‑test fidelity: tái lập `captures/keylog.txt` đúng từng byte (`make repro`) |
| `server/server.c` + `server/Makefile` | handshake server (ký CertificateVerify); phục vụ N kết nối; CSV per‑connection; `make` + `make cert` |
| `client/client.c` + `client/Makefile` | handshake client + `--bench` (latency) + `--load` (throughput); `make` |
| `Makefile` (gốc) | gọi đệ quy 3 Makefile con (`make` / `make repro` / `make cert` / `make clean`) |
| `MAPPING.md` | bảng ánh xạ code ↔ file illustrated-tls13 ↔ mục RFC 8446 |

Mỗi `server/Makefile` và `client/Makefile` theo đúng kiểu
`syncsynchalt/illustrated-tls13/server/Makefile`, chỉ khác là trỏ vào **OpenSSL của
repo NT219** (tìm qua `scripts/versions.env`, fallback `~/pqc/openssl`, rồi OpenSSL hệ
thống) và bake đường dẫn bằng `-Wl,-rpath` để **chạy đúng OpenSSL** không cần
`LD_LIBRARY_PATH`. Track D tự cuộn protocol nên chỉ link `-lcrypto` (KHÔNG `-lssl`).

## Build & chạy

```sh
# o thu muc goc tls13-scratch/:
make            # build server/server + client/client (goi 2 Makefile con)
make cert       # tao server/server.crt + server/server.key (lan dau)

# 1 handshake, in trace tung buoc (2 cua so):
( cd server && ./server 8400 )      &     # cua so server
  cd client && ./client 127.0.0.1 8400     # cua so client
```
Hoặc build/chạy từng phần: `make -C server`, `make -C client`, `make -C common repro`.

## Kiểm chứng đối chiếu illustrated-tls13 + RFC (ĐÃ double‑check)

Bốn lớp bằng chứng cho câu hỏi "handshake có đúng bản chất repo + RFC":

### 1. Lõi crypto — BYTE‑HỆT repo (`make repro`)
```sh
make repro      # ALL MATCH -> byte-identical to illustrated-tls13 + RFC 8446
```
Nạp đúng khoá tất định mà repo bỏ vào `openssl/instruments.patch` (client X25519 priv
`0x20..0x3f`, server pub đã công bố, `hello_hash` của site), key schedule §7.1 của track
tái lập **đúng từng byte**: X25519 client pub, ECDHE shared, và **cả hai** handshake
traffic secret trong `captures/keylog.txt`. → Key exchange + key schedule khớp bit‑for‑bit.

### 2. ServerHello — định dạng BYTE‑HỆT `captures/caps/serverhello`
ServerHello track phát ra dài **đúng 127 bytes** như repo, **mọi field byte‑hệt** trừ ba
giá trị bắt buộc khác mỗi phiên (server random, session_id echo, pub ephemeral):

| field | repo | track D |
|-------|------|---------|
| record header | `16 03 03 00 7a` | `16 03 03 00 7a` ✓ |
| handshake header | `02 00 00 76` | `02 00 00 76` ✓ |
| legacy_version | `03 03` | `03 03` ✓ |
| session_id len | `20` | `20` ✓ |
| cipher_suite | `13 02` | `13 02` ✓ |
| compression | `00` | `00` ✓ |
| extensions len | `00 2e` | `00 2e` ✓ |
| supported_versions | `00 2b 00 02 03 04` | `00 2b 00 02 03 04` ✓ |
| key_share header | `00 33 00 24 00 1d 00 20` | `00 33 00 24 00 1d 00 20` ✓ |
| random / sid / pub | (tất định) | (khác mỗi phiên — đúng RFC) |

### 3. ClientHello — bản minimal hợp lệ RFC (cùng lõi, ít extension hơn OpenSSL)
ClientHello track phát ra cùng **lõi** với repo: record `16 03 01 …`, legacy_version
`03 03`, random 32B, session_id 32B, compression null; và **encoding extension trùng
byte** ở `supported_versions` (`00 2b 00 03 02 03 04`) và `key_share`
(`00 33 00 26 00 24 00 1d 00 20 …`). Khác: track gửi **1 cipher suite** + **4 extension**
(supported_versions, supported_groups=x25519, signature_algorithms=ecdsa_secp256r1_sha256,
key_share), còn repo (OpenSSL) gửi 4 suite + 11 extension (SNI, ec_point_formats,
session_ticket, …). Cả hai đều **hợp lệ RFC 8446** — đây là khác biệt minimal‑vs‑full
bình thường giữa hai bản TLS đúng chuẩn, không phải lỗi.

### 4. Interop — đúng RFC (OpenSSL thật bắt tay được)
```sh
( cd server && ./server 8400 ) &          # server o thu muc server/
echo ping | openssl s_client -connect 127.0.0.1:8400 -tls1_3 \
  -ciphersuites TLS_AES_256_GCM_SHA384 -groups X25519 -CAfile server/server.crt
```
→ `New, TLSv1.3, Cipher is TLS_AES_256_GCM_SHA384` · `Server Temp Key: X25519` ·
`Peer signature type: ECDSA` · **`Verify return code: 0 (ok)`** + nhận "pong". Một bản
RFC chuẩn (OpenSSL) hoàn tất trọn handshake với server tự viết = đúng chuẩn, không chỉ
đúng repo.

**Kết luận:** cơ chế handshake (key exchange, key schedule, dẫn xuất secret, bảo vệ
record, trình tự + định dạng message) **byte‑hệt repo / đúng RFC**. Khác biệt duy nhất
trên dây là (a) ngẫu nhiên theo phiên (bắt buộc phải khác) và (b) ClientHello của track
là bản minimal so với bản đầy đủ của OpenSSL.

## Macrobenchmark (số liệu đo handshake)

### 1. Latency handshake (client→server)
```sh
( cd server && ./server 8400 1100 ) &
cd client && BENCH_ITERS=1000 ./client 127.0.0.1 8400 --bench --warmup 100
#   tham so: --bench N   --warmup W   --csv FILE   (hoac env BENCH_ITERS/BENCH_WARMUP)
```
Báo (microsecond, min/median/mean/p95/max/±CI95) qua N handshake thành công + CSV từng
vòng: **TOTAL** + tách pha **client keygen** (X25519 §4.2.8), **client ECDHE** (§7.4.2),
**key schedule** (HKDF‑SHA384 §7.1), **sig verify** (ECDSA §4.4.3). Bốn pha này đúng là
phần đổi khi swap PQC.

### 2. Throughput dưới đồng thời, server đơn vs đa luồng
```sh
mkdir -p ../data/raw/x86_64        # tu trong repo NT219: ../../data/raw/<arch>
# don luong:
( cd server && ./server 8400 20000 --threads 1 > ../../data/raw/x86_64/tlsmini_d_st.csv 2> run.log ) &
cd client && ./client 127.0.0.1 8400 --load --threads 16 --total 20000 --csv ../../data/raw/x86_64/load_d_st.csv
# da luong (doi --threads cua server):
( cd server && ./server 8400 20000 --threads 4   > ../../data/raw/x86_64/tlsmini_d_mt.csv 2> run.log ) &
cd client && ./client 127.0.0.1 8400 --load --threads 16 --total 20000 --csv ../../data/raw/x86_64/load_d_mt.csv
```
Client mở C luồng (`--threads C`) liên tục mở handshake mới đến khi đủ `--total M`; báo
**handshakes/sec** + latency **p50/p95/p99**. Server `--threads N` chọn đơn (N=1) vs đa
luồng (N>1). Server cũng in CSV per‑connection `conn,suite,group,handshake_us` (banner ra
stderr) như `tls_mini_server` — pipe stdout vào file. **Lưu ý**: server `count` phải
**bằng** client `--total` (lệch nhau → server chờ kết nối không tới và treo).

> Throughput chỉ scale trên máy nhiều core (`nproc > 1`). Trên 1 core, server đa luồng
> được kiểm **đúng** (mọi handshake verify chữ ký) nhưng không nhanh hơn. Không dùng
> `wrk` (nó chạy HTTP; endpoint của ta là TLS ping/pong tối giản) → `--load` là harness tự viết.

## Kiểm đúng số đo

1. **Chỉ đếm handshake verify thành công** (chữ ký + Finished MAC); `make repro` chứng
   minh crypto byte‑exact.
2. **Cross-check pha** với `openssl speed ecdhx25519` / `ecdsap256`: cùng cỡ, đo của ta
   cao hơn chút do tạo/huỷ object EVP mỗi handshake (chi phí thực per‑connection).
3. **Thống kê**: median (chịu nhiễu scheduler) + CI95 qua ≥1000 vòng; máy rảnh; ghim core `taskset`.
4. **Sanity tổng**: `openssl s_time -connect host:port -new -tls1_3` làm mốc cho TOTAL.

Đồng hồ là `CLOCK_MONOTONIC_RAW` (§8.3). Tham số theo env (`BENCH_ITERS`, `BENCH_WARMUP`,
`TLS_CONC`, `TLS_TOTAL`, `TLS_THREADS`, `BENCH_CSV`), cờ dòng lệnh ghi đè.

## Vị trí trong repo NT219

Đây là **track D — server tự triển khai** (thầy yêu cầu thêm), để **thư mục riêng** kẻo
lẫn với server A/B/C thuê libssl:

```
NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms/
├─ Makefile  src/  scripts/  data/  analysis_out/   # benchmark primitive + server A/B/C cua repo
│                                                    # (scripts/versions.env -> OpenSSL custom)
└─ tls13-scratch/                          # <-- track D nay (bo cuc nhu illustrated-tls13)
   ├─ common/   tls13.h  tls13.c  repro_keylog.c  Makefile
   ├─ server/   server.c  Makefile  server.crt  server.key
   ├─ client/   client.c  Makefile
   ├─ Makefile  README.md  MAPPING.md
   └─ (CSV ghi sang ../data/raw/<arch>/ ; tu trong server/-client/ la ../../data/raw/<arch>/)
```

Hai `server/Makefile` + `client/Makefile` đọc `../../scripts/versions.env` của repo nên
**dùng đúng OpenSSL** mà `bench_evp`/`nginx` dùng — đó là cách track D "khớp" repo NT219.

Cùng thước đo macro với A/B/C (latency handshake + throughput, đơn/đa luồng) nhưng toàn
bộ giao thức nằm trong tay mình. Benchmark primitive (RQ1/RQ2) đo **một phép crypto**;
track D đo **handshake TLS 1.3 hoàn chỉnh** mà các phép đó hợp thành. Chi tiết ánh xạ
xem `MAPPING.md` §7–§8.

**Hiện classical** (X25519+ECDSA); nhánh hybrid X25519MLKEM768 + ML‑DSA‑65 (primitive lấy
từ OpenSSL 3.5+ hoặc liboqs, không tự chế) là việc biến track D thành so sánh classical‑vs‑PQC.
