# 09 — Kiến trúc tổng quan → Từng node mạng → Kế hoạch triển khai

> **Cấu trúc thầy yêu cầu (Onion Model):** đi sâu **3 lớp trong cùng** — **Cryptography → Authorization → Authentication** (3 lớp ôm sát *Asset*) — trong **MỘT kiến trúc tổng quan** → **đi từng node mạng** → **kế hoạch triển khai**. Hai lớp ngoài (Firewall, IDS/IPS) chỉ áp ở node nào **cần**.
>
> **Cơ sở:** `05_Implement & Benchmark Lattice-based Schemes`. PQC (ML-KEM = FIPS 203, ML-DSA = FIPS 204) chính là **lớp Cryptography**; toàn bộ benchmark của 05 là phần **đo lường lớp này**. AuthN/AuthZ là ngữ cảnh hệ thống để 3 lớp có chỗ "sống".

---

## 0. Nguyên tắc áp dụng Onion Model (áp có chọn lọc)

```
   Onion layer            Áp dụng ở đâu?                       Nguồn
   ───────────────────────────────────────────────────────────────────
   (5) Firewall      →    chỉ tại biên zone (Gateway, zone seg)  cần
   (4) IDS/IPS       →    chỉ tại DMZ/Gateway                    cần
   (3) Authentication →   MỌI node có định danh                  3 lớp trong
   (2) Authorization →    node nào ra quyết định truy cập        3 lớp trong
   (1) Cryptography  →    MỌI node (TLS/AEAD/chữ ký) ◄ FILE 05   3 lớp trong
   ●   ASSET         →    dữ liệu nhạy cảm + khóa private
```

**Trọng tâm đồ án = 3 lớp trong (1)(2)(3).** Lớp (1) Cryptography là nơi PQC của file 05 nằm và là nơi **đo benchmark**.

---

## 1. KIẾN TRÚC TỔNG QUAN

**Hệ thống:** *Secure Edge-to-Cloud API* — thiết bị biên (ARM) trao đổi dữ liệu an toàn với dịch vụ trung tâm (x86) qua kênh **TLS 1.3 hybrid hậu lượng tử**, token/cert ký bằng **ML-DSA**. (Chọn topology này vì nó dùng **cả ML-KEM (trao khóa) lẫn ML-DSA (chữ ký)** và trải trên **x86 + ARM** — đúng 2 nền tảng file 05 §8.1.)

```
        ZONE 0 — FIELD / EDGE (mạng không tin cậy)
   ┌─────────────────────────────┐
   │  N1. EDGE NODE  (ARM Pi 4)  │   Lớp: Crypto + AuthN
   │  - PQC-TLS client           │
   │  - ML-DSA device identity   │
   │  - AEAD dữ liệu trước khi gửi│
   └──────────────┬──────────────┘
                  │  TLS 1.3 hybrid  X25519MLKEM768   ◄── LỚP CRYPTO (file 05: RQ3)
   ═══════════════╪═══════════ (5) FIREWALL ════════════════════════
                  ▼
        ZONE 1 — DMZ (public-facing)
   ┌─────────────────────────────┐
   │  N2. API GATEWAY  (x86)     │   Lớp: Crypto + AuthN + AuthZ  (+ Firewall + IDS/IPS)
   │  - TLS termination (hybrid) │
   │  - PEP: verify token, PoP   │
   │  - rate-limit / WAF         │
   └───┬───────────────────┬─────┘
       │ (token introspect)│ (authz query)        ── mTLS nội bộ ──
   ════╪═══════════════════╪═══════ (5) FIREWALL ══════════════════
       ▼                   ▼
        ZONE 2 — INTERNAL / APP (tin cậy)
   ┌──────────────┐  ┌──────────────┐  ┌────────────────────────┐
   │ N3. IdP/IAM  │  │ N4. PDP(OPA) │  │ N5. BACKEND APP        │
   │ - ML-DSA ký  │  │ - RBAC/ABAC  │  │ - xử lý nghiệp vụ      │
   │   token/JWT  │  │ - deny-by-   │  │ - AEAD at-rest (envelope)│
   │ - WebAuthn   │  │   default    │  │   ●●●  ASSET (DB)  ●●●  │
   └──────┬───────┘  └──────────────┘  └───────────┬────────────┘
          │ (lấy khóa ký)                          │ (lấy DEK)
   ═══════╪════════════════════════════════════════╪══ (5) FIREWALL ══
          ▼                                         ▼
        ZONE 3 — SECURE / CRYPTO (tin cậy nhất)
   ┌──────────────────┐         ┌──────────────────────┐
   │ N6. CA (PKI)     │         │ N7. KMS / Vault       │
   │ - cấp cert ML-DSA│         │ - KEK/DEK, khóa ký     │
   │   (hoặc hybrid)  │         │ - rotate / revoke      │
   └──────────────────┘         └──────────────────────┘
```

**Luồng dữ liệu chính:** `N1 Edge → (TLS hybrid PQC) → N2 Gateway → (mTLS) → N5 Backend → ●Asset`. Gateway hỏi `N3 IdP` (xác thực token) và `N4 PDP` (cấp quyền). `N3/N6` lấy khóa ký từ `N7 KMS`.

---

## 2. ĐI TỪNG NODE MẠNG (3 lớp trong + onion áp dụng + đo gì từ 05)

### N1 — EDGE NODE (ARM Raspberry Pi 4) — *Zone 0*
| Lớp | Áp dụng tại node |
|---|---|
| **Cryptography** ◄05 | Client TLS 1.3 **hybrid `X25519MLKEM768`** (ML-KEM trao khóa). Mã hóa dữ liệu trước khi gửi bằng **AEAD** (AES-256-GCM/XChaCha20). Ký message/telemetry bằng **ML-DSA** (device key). |
| **Authentication** | Định danh thiết bị bằng **cert ML-DSA** (mTLS client cert) do N6 cấp. |
| **Authorization** | *(không áp — node biên không ra quyết định cấp quyền)* |
| **Onion ngoài** | *(không có firewall/IDS riêng — nằm ngoài vành đai)* |
| **Benchmark file 05 đo tại đây** | keygen/encaps/sign trên **ARM**; handshake client-side; **năng lượng/op (INA219)** — RQ1, RQ2. |

### N2 — API GATEWAY / Reverse Proxy (x86) — *Zone 1 / DMZ*
| Lớp | Áp dụng tại node |
|---|---|
| **Cryptography** ◄05 | **Kết thúc TLS hybrid** (server-side ML-KEM decaps). Cert server ký **ML-DSA** (hoặc RSA/ECDSA để đối chứng). Re-encrypt nội bộ qua mTLS. |
| **Authentication** | Verify JWT (kiểm `kid`, pin `alg`), **PoP = DPoP / mTLS-bound** chống replay. |
| **Authorization** | **PEP** — gọi N4 (PDP) lấy quyết định; deny-by-default. |
| **Onion ngoài** | ✅ **Firewall** (biên DMZ) + ✅ **IDS/IPS/WAF** (đây là node *cần* áp 2 lớp ngoài). |
| **Benchmark file 05 đo tại đây** | **handshake latency hybrid vs classical, throughput đồng thời (`wrk`)** trên **x86** — RQ1, RQ3 (TLS macrobenchmark §7.6). |

### N3 — IdP / IAM (x86) — *Zone 2*
| Lớp | Áp dụng tại node |
|---|---|
| **Cryptography** ◄05 | **Ký token/JWT bằng ML-DSA** (so với Ed25519/RSA-PSS). Khóa ký lấy từ N7. |
| **Authentication** | **WebAuthn/FIDO2** cho người dùng; mTLS cho dịch vụ; TOTP fallback. |
| **Authorization** | Phát hành scope/claims (đầu vào cho ABAC ở N4). |
| **Onion ngoài** | *(không — nằm trong vùng tin cậy sau firewall zone)* |
| **Benchmark file 05 đo tại đây** | **sign/verify ML-DSA** (token signing rate) — RQ1; so kích thước token/chữ ký (§9 bytes vs algorithm). |

### N4 — PDP / OPA (x86) — *Zone 2*
| Lớp | Áp dụng tại node |
|---|---|
| **Cryptography** | mTLS với Gateway (kênh truyền quyết định). |
| **Authentication** | mTLS service identity. |
| **Authorization** ★ | **Lõi của lớp AuthZ**: RBAC → ABAC (Rego), deny-by-default, **log reason** mọi quyết định. |
| **Onion ngoài** | *(không)* |
| **Benchmark file 05 đo tại đây** | *(không phải PQC — nhưng đo policy pass-rate cho phần Evaluation hệ thống)* |

### N5 — BACKEND APP + ASSET DB (x86) — *Zone 2* ● chứa Asset
| Lớp | Áp dụng tại node |
|---|---|
| **Cryptography** ◄05 | **AEAD at-rest** + **envelope encryption**: DEK mã hóa dữ liệu, KEK ở N7 bọc DEK. (DEK có thể được trao bằng cơ chế dựa **ML-KEM** để minh họa KEM ngoài TLS.) |
| **Authentication** | Chỉ nhận kết nối mTLS từ Gateway. |
| **Authorization** | Tin quyết định đã được PEP@Gateway cưỡng chế; kiểm tra lại scope. |
| **Onion ngoài** | *(không — bảo vệ bằng zone segmentation)* |
| **Benchmark file 05 đo tại đây** | (tuỳ) **encaps/decaps ML-KEM** cho envelope; throughput giải mã dữ liệu. |

### N6 — CA / PKI — *Zone 3*
| Lớp | Áp dụng tại node |
|---|---|
| **Cryptography** ◄05 | **Cấp chứng chỉ ký bằng ML-DSA** (hoặc hybrid cert) cho mọi node. |
| **Authentication** | Chỉ thao tác qua kênh quản trị mTLS. |
| **Authorization** | Chính sách cấp/thu hồi cert. |
| **Benchmark file 05 đo tại đây** | keygen ML-DSA; thời gian cấp/verify chuỗi cert (so RSA-15360/P-521 — bytes & verify time). |

### N7 — KMS / Vault — *Zone 3*
| Lớp | Áp dụng tại node |
|---|---|
| **Cryptography** | Lưu **KEK/DEK + khóa ký ML-DSA**; sinh/rotate/revoke; audit. |
| **Authentication** | mTLS + token ngắn hạn cho mỗi client xin khóa. |
| **Authorization** | Policy ai được lấy khóa nào (least-privilege). |
| **Benchmark file 05 đo tại đây** | *(không phải PQC — nhưng đo rotation SLA cho Evaluation hệ thống)* |

---

## 3. BẢNG TỔNG HỢP node × lớp

| Node | Crypto (1) ◄05 | AuthZ (2) | AuthN (3) | Firewall (5) | IDS/IPS (4) |
|---|:---:|:---:|:---:|:---:|:---:|
| N1 Edge (ARM) | ✅ TLS hybrid + AEAD + ML-DSA | — | ✅ cert | — | — |
| N2 Gateway (x86) | ✅ TLS term + cert | ✅ PEP | ✅ token+PoP | ✅ | ✅ |
| N3 IdP (x86) | ✅ ML-DSA ký token | (cấp scope) | ✅ WebAuthn | (zone) | — |
| N4 PDP (x86) | ✅ mTLS | ✅★ Rego | ✅ mTLS | (zone) | — |
| N5 Backend (x86) ● | ✅ AEAD/envelope | (kiểm tra) | ✅ mTLS | (zone) | — |
| N6 CA | ✅ cấp cert ML-DSA | ✅ | ✅ | (zone) | — |
| N7 KMS | ✅ key lifecycle | ✅ | ✅ | (zone) | — |

★ = node lõi của lớp đó. **Cột Crypto (1) = nơi file 05 sống và đo.**

---

## 4. KẾ HOẠCH TRIỂN KHAI (Deployment Plan)

> **2 nền tảng (đúng file 05 §8.1):** **x86_64** host Zone 1–3 (container/VM); **ARM Raspberry Pi 4** đóng vai N1 Edge (Zone 0). Cùng MỘT kiến trúc, 2 phần cứng → vừa là hệ thống thật, vừa lấy số liệu **x86 vs ARM** của 05.

### Phase 0 — Provision hạ tầng *(file 05 §7.3, §8.1–8.2)*
- [ ] x86 server: cài Docker/k8s, dựng 4 network zone (firewall iptables/nftables giữa các zone).
- [ ] ARM Pi 4: cài OS aarch64, toolchain, governor=performance.
- [ ] Clone & build **liboqs + OpenSSL ≥ 3.5** (PQC native) trên **cả x86 và ARM** (cross-compile/buildx).

### Phase 1 — Bootstrap PKI & khóa *(lớp Crypto + N6, N7)*
- [ ] N6 (CA): sinh root + cấp **cert ML-DSA** cho mọi node (đối chứng: cert RSA-15360 / P-521).
- [ ] N7 (KMS/Vault): khởi tạo KEK; chính sách rotate ≤ 10 phút.

### Phase 2 — Dựng lõi server (Zone 1–3 trên x86)
- [ ] N2 Gateway: TLS 1.3 bật nhóm `X25519MLKEM768`; PEP; rate-limit; WAF/IDS.
- [ ] N3 IdP (Keycloak/tự dựng): ký token **ML-DSA**; WebAuthn.
- [ ] N4 PDP (OPA): nạp policy Rego; deny-by-default.
- [ ] N5 Backend: AEAD at-rest + envelope (DEK từ N7).

### Phase 3 — Triển khai Edge (ARM Pi 4)
- [ ] N1: client PQC-TLS tới N2; nạp cert ML-DSA thiết bị; AEAD + ký telemetry.
- [ ] (tuỳ) gắn INA219 đo năng lượng.

### Phase 4 — Thí nghiệm đo (toàn bộ Methodology file 05)
- [ ] **Micro (§7.4):** keygen/encaps/decaps/sign/verify tại N1(ARM) & N2/N3(x86), N iter + warm-up + median/95% CI.
- [ ] **Macro TLS (§7.6):** handshake N1→N2 — classical vs PQC vs **hybrid**; throughput đồng thời (`wrk`).
- [ ] **Optimized (§7.2):** reference C vs NEON trên ARM; ablation flags.
- [ ] **Size/memory/energy (§7.4):** `size`, `readelf`, `/usr/bin/time -v`, RAPL/INA219.

### Phase 5 — Tổng hợp & đánh giá *(file 05 §9, §11)*
- [ ] CSV → biểu đồ (latency/throughput/bytes/energy) → bảng trade-off → trả lời RQ1/RQ2/RQ3.
- [ ] Báo cáo + demo handshake hybrid + benchmark.

### Runbook tối thiểu mỗi node (mẫu)
```
Node ID & Zone:
Vai trò + lớp Onion áp dụng:
BOM (phần mềm + phiên bản):
Khóa/cert (thuật toán, nguồn):
Lệnh cài + cấu hình:
Health check + log:
Đo gì từ file 05 (nếu có):
```

---

## 5. Truy vết về file 05

| Phần kiến trúc này | Mục file 05 |
|---|---|
| Lớp Cryptography (ML-KEM/ML-DSA, hybrid) | §3 Background, §7.1, §7.2 |
| N1 Edge trên ARM + năng lượng | §8.1, RQ2 |
| Handshake hybrid tại N2 Gateway | §7.6, RQ3 |
| Thí nghiệm Phase 4 | §7.4, §7.5, §7.6 |
| 2 nền tảng x86 + ARM | §8.1 |
| Phase 5 tổng hợp | §9, §11 |

> **Tóm lại:** Kiến trúc này = *một* hệ thống thật (7 node, 4 zone) trong đó **lớp Cryptography chính là đề tài PQC của file 05**; AuthN/AuthZ chỉ áp ở node cần (theo lời thầy "phần nào cần thì mới áp"); Firewall/IDS chỉ ở Gateway. Benchmark của 05 được nhúng làm phần đo lường lớp Crypto trên đúng 2 nền tảng x86 + ARM.
