# 13 — HANDOFF: tiếp tục trên Ubuntu

Tài liệu bàn giao để **tiếp tục dự án trên Ubuntu** (WSL2 hoặc máy Ubuntu thật). Ghi rõ: đã làm gì, môi trường cần gì, lệnh tiếp theo cho từng WP.

---

## 0. Tóm tắt trạng thái (cập nhật khi bàn giao)

| WP | Trạng thái | Ghi chú |
|---|---|---|
| WP0 lý thuyết/lit review/threat | 🟡 tài liệu xong | `06_References...md`, `11_WP0_Study_Dossier.md` |
| WP1 môi trường build | ✅ **ARM xong** · ❌ x86 Docker chưa | image `pqc:wp1-arm64` (OpenSSL 3.6.2 + liboqs + PQClean) trên Pi |
| WP2 microbenchmark | 🟡 harness xong + test x86 (MinGW) · ARM đang chạy | `benchmarks/micro/pqc_bench.cpp` (OpenSSL EVP) |
| WP3 NEON vs reference + cycle | ❌ chưa | dùng liboqs (reference vs NEON) + perf PMU |
| WP4 TLS qua mạng + Apache | 🟡 script xong · handshake ARM đã thử | `scripts/run_apache_demo.sh`, `docker/Dockerfile.apache` |
| WP5 kiến trúc onion | 🟡 thiết kế xong | `09_Architecture...md`, `docs/REPORT.md` |
| WP6 RSS/size/energy/validation | ❌ mới có byte sizes | |
| WP7 thống kê/biểu đồ/báo cáo | ❌ chưa | cần data đủ |

---

## 1. Môi trường Ubuntu cần chuẩn bị

```bash
# Docker
curl -fsSL https://get.docker.com | sh
# Công cụ phụ trợ
sudo apt-get update && sudo apt-get install -y git python3 python3-pandas python3-matplotlib tmux
```
> Nếu Ubuntu là **WSL2 trên Windows**: bật Docker Desktop + WSL integration, hoặc cài Docker Engine trong WSL.

Lấy mã nguồn:
```bash
git clone -b DevOS <URL-repo> ~/pqc && cd ~/pqc
# HOẶC copy thư mục dự án sang (gồm benchmarks/, scripts/, docker/, apache/, docs/)
```

---

## 2. Kiến trúc giải pháp đã chốt (quan trọng — đừng làm lại sai)

- **OpenSSL 3.6.2** (build từ nguồn, native ML-KEM/ML-DSA) — **KHÔNG dùng oqs-provider/OQS-fork** (đã deprecated).
- **Harness chỉ dùng OpenSSL EVP** (`EVP_PKEY_Q_keygen`, `EVP_PKEY_encapsulate`, `EVP_DigestSign` md=NULL cho ML-DSA, `EVP_PKEY_derive`). → test được bằng MinGW lẫn Docker.
- **liboqs + PQClean** trong image chỉ để **WP3** (reference vs NEON).
- **Mạng hạn chế (Pi IPv6-only):** nguồn vendored vào `vendor/` bằng `scripts/fetch_sources.sh` (chạy trên HOST), Dockerfile COPY thay vì tải. **Ubuntu mạng bình thường vẫn chạy `fetch_sources.sh` được.**
- So sánh theo **mức bảo mật khớp (Cat 1/3/5)** — cột `category` trong CSV.

---

## 3. Build môi trường trên Ubuntu (WP1-x86)

```bash
cd ~/pqc
bash scripts/docker_build.sh          # tự fetch_sources.sh nếu vendor/ trống -> image pqc:wp1-amd64
docker run --rm -v "$PWD/data:/data" pqc:wp1-amd64 capture_env.sh   # ghi data/env/env_linux-x86.txt
```

---

## 4. Lệnh tiếp theo cho từng WP

### WP2 — Microbenchmark (chạy đủ, lấy số x86 + ARM)
```bash
# x86 (Ubuntu, trong Docker):
bash scripts/run_micro.sh --level all --iters 5000 --warmup 200 --batches 10
#   -> data/results/micro_x86.csv
# ARM (Pi): xem mục 6 (chạy trên Pi)
```
Tham số: `--suite kem|sig|kex|all  --level 1|2|3|5|all  --iters N  --warmup W  --batches B  --maxms MS  --only NAME`.
> RSA-15360 keygen rất chậm (phút) — dùng `--maxms` chặn, hoặc `--only ML` để bỏ RSA chậm khi cần nhanh.

### WP3 — NEON vs reference + cycle (chưa làm)
```bash
# Bản reference (tắt NEON) để so với bản NEON của liboqs:
docker run --rm pqc:wp1-amd64 bash -lc '/opt/build/liboqs/build/tests/speed_kem ML-KEM-768'
# Trên ARM: build liboqs lần 2 với -DOQS_USE_ARM_NEON_INSTRUCTIONS=OFF -> so speed_kem.
# Cycle: perf stat -e cycles (x86) hoặc PMCCNTR_EL0/perf (ARM, cần --cap-add=SYS_ADMIN khi docker run).
```

### WP4 — TLS qua mạng + Apache (script có rồi)
```bash
bash scripts/run_apache_demo.sh       # (A) handshake PQC s_server<->s_client (chắc chạy)
                                      # (B) build Apache httpd link OpenSSL 3.6.2 + handshake
# netem (mô phỏng mạng): tc qdisc add dev <if> root netem delay 60ms loss 3%   (cần quyền root + 2 host)
```

### WP6 — Tài nguyên + validation
```bash
# code size:
docker run --rm pqc:wp1-amd64 sh -c 'size /opt/build/liboqs/build/tests/speed_kem'
# RSS: /usr/bin/time -v <chạy harness>
# byte sizes: đã có trong CSV (cột pk_bytes,sk_bytes,ct_or_sig_bytes)
# validation: đối chiếu CSV với FIPS 203/204 + paper (đã đúng pk/ct/sig khi test)
```

### WP7 — Thống kê + biểu đồ + báo cáo
```bash
# Đọc CSV bằng pandas, vẽ biểu đồ (latency vs param, bytes vs algo, x86 vs ARM):
python3 tools/plot.py   # (cần viết — đọc data/results/*.csv -> docs/figs/*.png)
```

---

## 5. Truy cập Raspberry Pi (ARM, Mythic Beasts)

```bash
ssh -p 5203 root@ssh.scorpions.hostedpi.com      # key-based (mạng nhà IPv4-only -> dùng proxy 5203)
```
- Image `pqc:wp1-arm64` đã có sẵn. `vendor/` đã prefetch. Repo ở `/root/pqc`.
- ⚠️ Docker trên Pi: storage-driver = **fuse-overlayfs**, **dns 8.8.8.8** trong `/etc/docker/daemon.json` (đã set). Container **không HTTPS** được (IPv4-443 chặn) → mọi nguồn phải vendored trên host.

---

## 6. Chạy WP2 trên Pi (ARM)

```bash
ssh -p 5203 root@ssh.scorpions.hostedpi.com
cd /root/pqc
# chạy nền (RSA chậm):
tmux new-session -d -s bench "docker run --rm -v /root/pqc:/work -w /work pqc:wp1-arm64 bash -lc \
  'g++ -O2 -std=c++17 benchmarks/micro/pqc_bench.cpp -o /tmp/b -I/opt/openssl/include -L/opt/openssl/lib -lssl -lcrypto && \
   /tmp/b --level all --iters 1000 --maxms 4000 --csv /work/data/results/micro_arm.csv' > bench_arm.log 2>&1"
tail -f bench_arm.log     # theo dõi; xong -> data/results/micro_arm.csv
```

---

## 7. Bản đồ file

| File | Vai trò |
|---|---|
| `benchmarks/micro/pqc_bench.cpp` | **Hàm đo** (OpenSSL EVP, cấu hình, đa mức, CI, cổng đúng đắn, byte sizes) |
| `scripts/run_micro.sh` / `run_micro_win.ps1` | chạy đo (Docker / Windows-MinGW) |
| `scripts/fetch_sources.sh` | prefetch nguồn -> vendor/ (host) |
| `scripts/docker_build.sh` | build image pqc:wp1-<arch> |
| `scripts/run_apache_demo.sh` | demo TLS PQC + Apache |
| `scripts/run_all.sh` | chạy tất cả (build→verify→micro→demo) |
| `docker/Dockerfile.pqc` | image môi trường (vendored) |
| `docker/Dockerfile.apache` | Apache link OpenSSL 3.6.2 |
| `apache/httpd-pqc.conf`, `gen-cert.sh` | cấu hình TLS PQC + cert ML-DSA |
| `docs/REPORT.md` | báo cáo (goals/risks/kiến trúc/triển khai/demo) |
| `06`,`07`,`08`,`09`,`10`,`11` | tài liệu WP0 + roadmap + đặc tả |

---

## 8. Việc còn lại (ưu tiên)
1. WP1-x86 build (`docker_build.sh` trên Ubuntu) + capture_env.
2. WP2 full: `run_micro.sh` x86 + thu `micro_arm.csv` từ Pi.
3. WP4 chạy thật `run_apache_demo.sh` + (tùy) netem.
4. WP6 RSS/size/validation.
5. WP3 NEON vs reference (nếu còn thời gian).
6. WP7 viết `tools/plot.py` → biểu đồ → trả lời RQ1/RQ2/RQ3 → hoàn thiện `docs/REPORT.md`.
