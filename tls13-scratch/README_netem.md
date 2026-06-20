# Track-D: đo handshake qua mạng giả lập (netem / RTT)

Bổ sung cho `tls13-scratch/` (Track-D) khả năng đo TLS 1.3 handshake **qua RTT/mất gói
giả lập** thay vì chỉ localhost — mô phỏng phương pháp của **xvzcf/pq-tls-benchmark**
(`emulation-exp`): 2 network namespace nối bằng veth, `tc netem` thêm độ trễ + loss,
nửa RTT mỗi đầu (RTT ≈ 2× delay).

## Có gì trong gói này
- `setup_ns.sh` / `teardown_ns.sh` — dựng/gỡ 2 namespace `srv_ns`/`cli_ns` + veth
  (10.0.0.1 / 10.0.0.2), static ARP, tắt offload, gắn qdisc `netem` rỗng.
- `bench_netem.sh` — script điều phối: quét (RTT × loss), chạy server + client `--bench`,
  gom dòng `SUMMARY`, ghi CSV vào **đúng layout `data/` của repo** (để `make analyze` thấy).

## client.c đã MERGE sẵn (KHÔNG chép đè)
Phần đo netem là **3 chỗ THUẦN BỔ SUNG** đã được merge thẳng vào `client/client.c`
(giữ nguyên handshake `run_session()` **và** phần lai ML-KEM `X25519MLKEM768`):
1. đọc env `NETEM_RTT` / `NETEM_LOSS` làm nhãn (mặc định `"na"`);
2. thêm 2 cột đầu `rtt_ms,loss_pct` vào header CSV và mỗi dòng số liệu;
3. in một dòng `SUMMARY rtt_ms=.. loss_pct=.. ok=.. total_median_us=.. total_p95_us=..
   total_mean_us=.. total_ci95_us=..` để script gom kết quả (grep `^SUMMARY`).

> Lưu ý: KHÔNG `cp` file `client.c.patched` cũ đè lên — bản đó dựa trên client cũ
> (chỉ X25519+ECDSA) nên sẽ XOÁ phần ML-KEM. Việc merge đã làm rồi.

## Cách chạy đo
Cần **root** (`ip netns` + `tc`) và kernel có module `sch_netem`.
```bash
# từ thư mục repo (tiện nhất):
sudo make tlsnetem

# hoặc trực tiếp, tuỳ biến tham số:
cd tls13-scratch
sudo ITERS=1000 WARMUP=100 ./bench_netem.sh
sudo DELAYS="2.5ms 15ms 50ms" LOSSES="0 1 3" ./bench_netem.sh
```
Script tự: build (client+server), sinh `server.crt/server.key` nếu thiếu, dựng/teardown
namespace+veth, áp netem (nửa RTT mỗi đầu), chạy server (phục vụ đúng `WARMUP+ITERS`
handshake) trong `srv_ns` và client `--bench` trong `cli_ns`, đọc lại RTT thật bằng `ping`.

## Kết quả (đúng layout repo)
- `data/netem_<arch>.csv` — tổng hợp: `rtt_ms,loss_pct,ok,median_us,p95_us,mean_us,ci95_us`
  (RTT để **dạng số** như `5.04`, không phải `5p04`, để `analyze.py` đọc được).
- `data/raw/<arch>/netem_rtt<R>ms_loss<L>.csv` — số liệu thô từng handshake mỗi điểm
  (có cột `rtt_ms,loss_pct` + breakdown per-phase).
- `make analyze` → bảng *"TLS 1.3 handshake over netem RTT/loss"* + chart
  `analysis_out/netem_<arch>.png` (median-vs-RTT, mỗi series một mức loss).

## Vì sao phải đo qua RTT (cốt lõi RQ3)
Trên localhost RTT≈0 nên chi phí TRUYỀN cert/chữ ký = 0 → không thấy điểm yếu PQC. Qua
RTT thật, gói server (ServerHello+Certificate+CertificateVerify) nếu vượt **initial
congestion window** (RFC 6928: initcwnd=10 ≈ 14600 byte) sẽ tốn **thêm 1 round-trip**.
Với chữ ký lớn như **ML-DSA**, hiệu ứng này mới lộ ra — đúng thứ cần đo cho "PQC trong
TLS trên thiết bị biên". (Track-D dùng X25519+ECDSA mặc định; đặt `TLS_GROUP=X25519MLKEM768`
để bật nhóm lai khi muốn phơi bày chênh lệch.)

## Hai cách triển khai
- **Một máy + namespace** (script này): tiện, tái lập tốt, nhưng KHÔNG phản ánh CPU thật.
- **Pi4 (client) ↔ laptop (server) thật**: bỏ phần namespace, đặt
  `tc qdisc add dev eth0 root netem delay <nửa RTT>ms` trên `eth0` MỖI máy, rồi chạy
  `server`/`client` trực tiếp. Dùng cách này lấy số liệu RQ3 chính thức (CPU Cortex-A72 thật).

## Caveat
- `tc`/netem cần module kernel `sch_netem`; môi trường nào thiếu (vd sandbox CI) sẽ báo
  *"qdisc kind is unknown"* ở `tc qdisc ... netem`. Raspberry Pi OS có thể phải cài
  `linux-modules-extra` / nạp module trước.
- **loss rất cao** (≥20%) có thể làm TCP reset → `run_session` gọi `die()` và client thoát.
  Quét loss vừa phải (≤3%) thì handshake chỉ chậm hơn, không lỗi.
- Server phục vụ đúng `WARMUP+ITERS`; nếu một handshake hỏng giữa chừng (loss>0) số đếm có
  thể lệch. Với `loss=0` thì không vấn đề.

## Nguồn tham khảo (phương pháp)
- xvzcf/pq-tls-benchmark — `emulation-exp/` (netns + veth + `tc netem`, client `s_timer`):
  https://github.com/xvzcf/pq-tls-benchmark
- Paquin, Stebila, Tamvada, *Benchmarking Post-Quantum Cryptography in TLS*, PQCrypto 2020.
- Schwabe, Stebila, Wiggers (KEMTLS): "add x/2 ms latency on client and server interfaces".
- RFC 6928 (initcwnd=10) — lý do gói lớn PQC tốn thêm round-trip dưới RTT.
- man tc-netem (cú pháp delay/loss/rate/limit).
