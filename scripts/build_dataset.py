#!/usr/bin/env python3
"""build_dataset.py — Tổng hợp số đo THẬT (x86) + sinh số MÔ HÌNH (ARM/NEON/energy/netem).

Đọc:  data/micro/x86/{mlkem,mldsa,aes}_x86.csv  (reference-C, đo thật, đủ mức NIST)
      data/micro/x86/micro_evp_x86.csv          (OpenSSL AVX2, đo thật, đủ mức)
      data/micro/x86/ecdsa_p521_x86.csv          (baseline, đo thật)
Xuất: data/micro/summary_x86.csv                (median/mean/p95/CI mỗi scheme×mức×op)
      data/micro/arm/{mlkem,mldsa,aes}_arm.csv   (MÔ HÌNH: x86_ref × hệ số nền tảng)
      data/micro/neon_vs_ref.csv                 (MÔ HÌNH: reference-C vs NEON trên Cortex-A72)
      data/resource/energy_estimate.csv          (MÔ HÌNH: CPU power model J/op)
      data/tls/netem_matrix.csv                  (MÔ HÌNH: handshake vs delay×loss × cấu hình)

LIÊM CHÍNH: mọi file MÔ HÌNH có dòng "# MODELED ..." nêu công thức + nguồn hệ số. KHÔNG bịa số:
mỗi số dẫn xuất từ số đo thật x86 nhân hệ số có trích dẫn (xem docs/DATA_PROVENANCE.md).
"""
import csv, math, os, statistics

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MX = os.path.join(ROOT, "data", "micro", "x86")
os.makedirs(os.path.join(ROOT, "data", "micro", "arm"), exist_ok=True)
os.makedirs(os.path.join(ROOT, "data", "resource"), exist_ok=True)
os.makedirs(os.path.join(ROOT, "data", "tls"), exist_ok=True)

# ----- kích thước byte theo FIPS 203/204 (pk, sk, ct|sig) -----
SIZES = {
    ("mlkem", "512"): (800, 1632, 768), ("mlkem", "768"): (1184, 2400, 1088),
    ("mlkem", "1024"): (1568, 3168, 1568),
    ("mldsa", "44"): (1312, 2560, 2420), ("mldsa", "65"): (1952, 4032, 3309),
    ("mldsa", "87"): (2592, 4896, 4627),
    ("aes", "128"): (16, 0, 16), ("aes", "192"): (24, 0, 16), ("aes", "256"): (32, 0, 16),
}
CAT = {"512": "1", "768": "3", "1024": "5", "44": "2", "65": "3", "87": "5",
       "128": "1", "192": "3", "256": "5"}

# ----- HỆ SỐ MÔ HÌNH (có trích dẫn trong DATA_PROVENANCE.md) -----
# ARM Cortex-A72 @1.5GHz vs i5-12450HX: tần số ~2.93x + IPC gap reference-C ~1.6x (liboqs Pi4).
ARM_FACTOR = {"Keygen": 4.6, "Encaps": 4.6, "Decaps": 4.6, "Sign": 4.8, "Verify": 4.5}
ARM_FACTOR_AES = 22.0   # BCM2711 Cortex-A72 KHÔNG có ARMv8 crypto ext -> AES phần mềm.
# NEON (liboqs aarch64) tăng tốc so reference-C trên A72:
NEON_SPEEDUP = {("mlkem", "Keygen"): 2.2, ("mlkem", "Encaps"): 2.1, ("mlkem", "Decaps"): 2.3,
                ("mldsa", "Keygen"): 1.9, ("mldsa", "Sign"): 1.7, ("mldsa", "Verify"): 1.8}
P_X86_W, P_ARM_W = 15.0, 4.0   # công suất 1 core active (x86 i5 / Pi4) — ước lượng power model.


def load(name, opcols):
    """Đọc CSV (Iter,Param,Category,<cyc,ns>*) -> dict[(param,op)] = list of ns."""
    path = os.path.join(MX, name)
    rows = list(csv.DictReader(open(path))) if os.path.exists(path) else []
    out = {}
    for r in rows:
        param = r.get("Param", "")
        for op, ns_col in opcols:
            out.setdefault((param, op), []).append(float(r[ns_col]))
    return out


def stats(vals):
    vals = sorted(vals); n = len(vals)
    mean = statistics.fmean(vals); sd = statistics.pstdev(vals)
    med = statistics.median(vals); p95 = vals[min(n - 1, int(0.95 * n))]
    t = 1.984 if n > 100 else 2.0   # ~t_0.975
    half = t * sd / math.sqrt(n) if n else 0
    return dict(n=n, median=med, mean=mean, std=sd, p95=p95,
                ci_lo=max(0, mean - half), ci_hi=mean + half, mn=vals[0] if vals else 0)


# ====== 1) SUMMARY (đo thật, reference-C) ======
SCHEMES = {
    "mlkem": ("mlkem_x86.csv", [("Keygen", "Keygen_ns"), ("Encaps", "Encaps_ns"), ("Decaps", "Decaps_ns")]),
    "mldsa": ("mldsa_x86.csv", [("Keygen", "Keygen_ns"), ("Sign", "Sign_ns"), ("Verify", "Verify_ns")]),
    "aes":   ("aes_x86.csv",   [("Encrypt", "Encrypt_ns"), ("Decrypt", "Decrypt_ns")]),
}
summary = []
for sch, (fname, opcols) in SCHEMES.items():
    data = load(fname, opcols)
    for (param, op), vals in sorted(data.items()):
        s = stats(vals)
        pk, sk, cs = SIZES.get((sch, param), (0, 0, 0))
        summary.append([sch, param, CAT.get(param, "?"), op, "reference-C", s["n"],
                        f"{s['median']:.0f}", f"{s['mean']:.0f}", f"{s['std']:.0f}",
                        f"{s['p95']:.0f}", f"{s['ci_lo']:.0f}", f"{s['ci_hi']:.0f}", pk, sk, cs])

with open(os.path.join(ROOT, "data", "micro", "summary_x86.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["scheme", "param", "category", "op", "build", "n", "median_ns", "mean_ns",
                "std_ns", "p95_ns", "ci95lo_ns", "ci95hi_ns", "pk_bytes", "sk_bytes", "ct_or_sig_bytes"])
    w.writerows(summary)
print(f"summary_x86.csv: {len(summary)} rows")

# ====== 2) ARM micro (MÔ HÌNH) ======
for sch, (fname, opcols) in SCHEMES.items():
    data = load(fname, opcols)
    rows = []
    for (param, op), vals in sorted(data.items()):
        med = statistics.median(vals)
        fac = ARM_FACTOR_AES if sch == "aes" else ARM_FACTOR[op]
        rows.append([param, CAT.get(param, "?"), op, f"{med * fac:.0f}", f"{fac:g}", f"{med:.0f}"])
    out = os.path.join(ROOT, "data", "micro", "arm", f"{sch}_arm.csv")
    with open(out, "w", newline="") as f:
        f.write("# MODELED — Cortex-A72 reference-C = x86_reference_median_ns * factor.\n")
        f.write("# factor = freq_ratio(2.93) * IPC_gap(~1.6) [lattice]; AES=22 (no ARMv8 crypto ext on BCM2711).\n")
        w = csv.writer(f)
        w.writerow(["Param", "Category", "op", "arm_median_ns", "factor", "x86ref_median_ns"])
        w.writerows(rows)
print("arm/{mlkem,mldsa,aes}_arm.csv: modeled")

# ====== 3) NEON vs reference (MÔ HÌNH, ARM) ======
with open(os.path.join(ROOT, "data", "micro", "neon_vs_ref.csv"), "w", newline="") as f:
    f.write("# MODELED — liboqs aarch64 NEON backend speedup vs PQClean reference-C trên Cortex-A72.\n")
    w = csv.writer(f)
    w.writerow(["scheme", "param", "op", "arm_ref_ns", "neon_speedup", "arm_neon_ns"])
    for sch in ("mlkem", "mldsa"):
        data = load(SCHEMES[sch][0], SCHEMES[sch][1])
        for (param, op), vals in sorted(data.items()):
            sp = NEON_SPEEDUP.get((sch, op))
            if not sp: continue
            fac = ARM_FACTOR[op]; arm_ref = statistics.median(vals) * fac
            w.writerow([sch, param, op, f"{arm_ref:.0f}", f"{sp:g}", f"{arm_ref / sp:.0f}"])
print("neon_vs_ref.csv: modeled")

# ====== 4) Energy (MÔ HÌNH power model) ======
with open(os.path.join(ROOT, "data", "resource", "energy_estimate.csv"), "w", newline="") as f:
    f.write(f"# MODELED — E = P_active * t. P_x86={P_X86_W}W (i5 1 core), P_arm={P_ARM_W}W (Pi4). "
            f"t_x86 = đo thật; t_arm = mô hình (ARM factor).\n")
    w = csv.writer(f)
    w.writerow(["scheme", "param", "op", "x86_ns", "x86_uJ", "arm_ns_model", "arm_uJ_model"])
    for sch, (fname, opcols) in SCHEMES.items():
        data = load(fname, opcols)
        for (param, op), vals in sorted(data.items()):
            med = statistics.median(vals)
            fac = ARM_FACTOR_AES if sch == "aes" else ARM_FACTOR[op]
            arm_ns = med * fac
            w.writerow([sch, param, op, f"{med:.0f}", f"{P_X86_W * med * 1e-3:.2f}",
                        f"{arm_ns:.0f}", f"{P_ARM_W * arm_ns * 1e-3:.2f}"])
print("energy_estimate.csv: modeled")

# ====== 5) netem TLS matrix (MÔ HÌNH) ======
# Anchor: TLS 1.3 = 1-RTT. compute_ms từ chữ ký+KEM thật. bytes-on-wire -> số gói -> rủi ro mất gói.
CONFIGS = {  # compute_ms (verify+KEX), handshake_bytes (ClientHello..Finished)
    "classical (ECDHE-P256/ECDSA)": dict(compute=1.4, hs_bytes=1500),
    "pqc-hybrid (X25519MLKEM768/ML-DSA-65)": dict(compute=1.1, hs_bytes=10800),
}
DELAYS = [0, 20, 60, 100]      # ms một chiều
LOSSES = [0, 1, 3, 5, 10]      # %
with open(os.path.join(ROOT, "data", "tls", "netem_matrix.csv"), "w", newline="") as f:
    f.write("# MODELED — latency = compute + n_rtt*RTT + retransmit. RTT=2*delay; "
            "RTO=max(200,RTT); npkts=ceil(hs_bytes/1460); retransmit=npkts*(loss/100)*RTO.\n")
    f.write("# Anchor compute_ms & hs_bytes từ bắt tay THẬT localhost (data/tls/x86/). p95≈1.3*median.\n")
    w = csv.writer(f)
    w.writerow(["config", "delay_ms", "loss_pct", "hs_bytes", "n_packets",
                "median_ms", "p95_ms"])
    for cfg, c in CONFIGS.items():
        npkts = math.ceil(c["hs_bytes"] / 1460)
        for delay in DELAYS:
            rtt = 2 * delay
            rto = max(200, rtt)
            for loss in LOSSES:
                retrans = npkts * (loss / 100.0) * rto
                med = c["compute"] + rtt + retrans
                w.writerow([cfg, delay, loss, c["hs_bytes"], npkts,
                            f"{med:.1f}", f"{med * 1.3:.1f}"])
print("netem_matrix.csv: modeled")
print("DONE build_dataset.py")
