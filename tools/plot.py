#!/usr/bin/env python3
# plot.py — Đọc data/results/micro_*.csv -> biểu đồ + bảng overhead PQC vs cổ điển (WP7a).
# Dùng: python3 tools/plot.py        (cần: pip/apt pandas matplotlib)
import sys, os, glob
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

RES, FIG = "data/results", "docs/figs"
os.makedirs(FIG, exist_ok=True)

frames = []
for f in sorted(glob.glob(f"{RES}/micro_*.csv")):
    plat = os.path.basename(f)[len("micro_"):-len(".csv")]
    try:
        d = pd.read_csv(f); d["platform"] = plat; frames.append(d)
        print(f"[load] {f}  ({len(d)} rows, platform={plat})")
    except Exception as e:
        print(f"[skip] {f}: {e}")
if not frames:
    print("Khong co data/results/micro_*.csv — chay scripts/run_micro.sh truoc."); sys.exit(1)

df = pd.concat(frames, ignore_index=True)
df["median_us"] = df["median_ns"] / 1000.0

# 1) Latency theo op cho từng platform/suite
for suite in df["suite"].unique():
    for plat in df["platform"].unique():
        s = df[(df.suite == suite) & (df.platform == plat)]
        if s.empty: continue
        piv = s.pivot_table(index="algo", columns="op", values="median_us")
        ax = piv.plot(kind="bar", figsize=(10, 5), logy=True)
        ax.set_ylabel("median latency (µs, log)"); ax.set_title(f"{suite.upper()} — {plat}")
        plt.tight_layout(); plt.savefig(f"{FIG}/latency_{suite}_{plat}.png"); plt.close()

# 2) Kích thước byte (pk + ct/sig) theo thuật toán
b = df.drop_duplicates("algo").set_index("algo")[["pk_bytes", "ct_or_sig_bytes"]]
ax = b.plot(kind="bar", figsize=(12, 5)); ax.set_ylabel("bytes")
ax.set_title("Public key & ciphertext/signature sizes (FIPS 203/204 vs cổ điển)")
plt.tight_layout(); plt.savefig(f"{FIG}/sizes.png"); plt.close()

# 3) x86 vs ARM (nếu có cả hai)
if df["platform"].nunique() >= 2:
    core = df[df.op.isin(["encaps", "sign", "derive"])]
    piv = core.pivot_table(index="algo", columns="platform", values="median_us")
    ax = piv.plot(kind="bar", figsize=(12, 5), logy=True)
    ax.set_ylabel("median (µs, log)"); ax.set_title("x86 vs ARM — core op latency")
    plt.tight_layout(); plt.savefig(f"{FIG}/x86_vs_arm.png"); plt.close()

# 4) Bảng tổng hợp (median µs) theo Category
print("\n=== Median latency (µs) theo Category / thuật toán / op ===")
tbl = df.pivot_table(index=["category", "algo", "suite", "platform"], columns="op", values="median_us")
print(tbl.round(1).to_string())
print(f"\n>>> Biểu đồ đã lưu: {FIG}/")
