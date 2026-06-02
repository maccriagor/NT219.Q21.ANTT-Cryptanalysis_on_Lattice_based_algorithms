# Runbook WP1 — Thiết lập môi trường build (Docker + OpenSSL 3.6.2)

**Phạm vi:** dựng môi trường build PQC trên ARM (Raspberry Pi) và x86, xác nhận chạy được. Chưa đo benchmark.

Môi trường đóng gói trong Docker (`docker/Dockerfile.pqc`), build native trên từng máy. OpenSSL 3.6.2 build từ nguồn cung cấp ML-KEM/ML-DSA native (không dùng oqs-provider); liboqs dùng cho microbenchmark primitive.

## Thông số máy cần khớp

| Mục | Giá trị | Cách kiểm |
|---|---|---|
| SSH (Pi) | `ssh.scorpions.hostedpi.com:5203`, user `root` (proxy IPv4) | trang Mythic Beasts |
| Kiến trúc | `aarch64` (Pi) / `x86_64` | `uname -m` |
| Docker | cài ở Phase 1 | `docker --version` |
| OpenSSL | `3.6.2` (trong image) | `docker run --rm pqc:wp1-arm64 openssl version` |
| File env | `env_linux-arm.txt` (Pi) / `env_linux-x86.txt` (x86) | sinh theo `uname -m` |

## Phase 0 — Kết nối (từ Windows)
```bash
ssh -p 5203 root@ssh.scorpions.hostedpi.com
```

## Phase 1 — Cài Docker
```bash
apt-get update && apt-get install -y curl
curl -fsSL https://get.docker.com | sh
docker run --rm hello-world
```

## Phase 2 — Lấy mã nguồn (Dockerfile + scripts)
```bash
git clone <URL-repo> ~/pqc && cd ~/pqc
```
> Build context phải chứa `docker/` và `scripts/`.

## Phase 3 — Build image
```bash
bash scripts/docker_build.sh        # Pi -> pqc:wp1-arm64 ; x86 -> pqc:wp1-amd64
```
> Lần đầu build OpenSSL từ nguồn mất ~20-40 phút; nên chạy trong `tmux`.

## Phase 4 — Kiểm tra môi trường
```bash
docker run --rm pqc:wp1-arm64 openssl version
docker run --rm pqc:wp1-arm64 sh -c 'openssl list -kem-algorithms | grep -i ML-KEM'
docker run --rm pqc:wp1-arm64 sh -c 'openssl list -signature-algorithms | grep -i ML-DSA'
docker run --rm pqc:wp1-arm64 /opt/build/liboqs/build/tests/speed_kem ML-KEM-768
```

## Phase 5 — Ghi cấu hình máy vào /data/env
```bash
mkdir -p ~/pqc/data
docker run --rm -v "$HOME/pqc/data:/data" pqc:wp1-arm64 capture_env.sh
cat ~/pqc/data/env/env_linux-arm.txt
```
> `/data` là volume mount nên file ghi ra được giữ lại trên host.

## Phase 6 — x86 (sau)
Trên WSL2 Ubuntu: chạy lại `scripts/docker_build.sh` → `pqc:wp1-amd64`; `capture_env.sh` sinh `env_linux-x86.txt`.

## Hoàn thành  khi
- `docker build` ra image (OpenSSL 3.6.2)
- `openssl list` hiển thị ML-KEM-512/768/1024 và ML-DSA-44/65/87 (native)
- `speed_kem` / `speed_sig` chạy ra số
- `data/env/env_linux-arm.txt` đã sinh

## Tài liệu tham khảo
- OpenSSL: <https://openssl-library.org/source/> · <https://docs.openssl.org/3.5/man7/EVP_PKEY-ML-KEM/>
- liboqs: <https://github.com/open-quantum-safe/liboqs> · <https://github.com/open-quantum-safe/liboqs/blob/main/CONFIGURE.md>
