#!/usr/bin/env python3
"""td_stats.py - tóm tắt thống kê handshake Track-D (tls13-scratch bench mode).

Mỗi file data/td_<label>_<arch>.csv là MỘT dòng "t1,t2,...,tN" (ms, raw).
Script bỏ warm-up (W handshake đầu) rồi in median/mean/std/p95/p99/min/max
+ khoảng tin cậy 95% của median (bootstrap). Nếu có nhãn 'x25519' làm mốc,
in thêm % overhead của các nhóm khác so với nó (theo median) kèm CI bootstrap.

Dùng:
  python3 scripts/td_stats.py                      # đọc mọi data/td_*.csv
  python3 scripts/td_stats.py data/td_hybrid_x86_64.csv ...
  WARMUP=20 python3 scripts/td_stats.py            # đổi số handshake bỏ đầu
Chỉ dùng thư viện chuẩn (không cần numpy/scipy).
"""
import glob
import os
import random
import statistics as st
import sys
from pathlib import Path

WARMUP = int(os.environ.get("WARMUP", "20"))   # số handshake đầu coi là warm-up
BOOT = int(os.environ.get("BOOT", "5000"))     # số lần resample bootstrap
random.seed(12345)                              # tái lập được


def pct(xs, p):
    """Bách phân vị p (0..100), nội suy tuyến tính (kiểu numpy mặc định)."""
    if not xs:
        return float("nan")
    s = sorted(xs)
    if len(s) == 1:
        return s[0]
    k = (len(s) - 1) * (p / 100.0)
    lo = int(k)
    hi = min(lo + 1, len(s) - 1)
    return s[lo] + (s[hi] - s[lo]) * (k - lo)


def boot_median_ci(xs, b=BOOT):
    """CI95 của median bằng bootstrap (percentile method)."""
    n = len(xs)
    if n < 2:
        return (float("nan"), float("nan"))
    meds = []
    for _ in range(b):
        sample = [xs[random.randrange(n)] for _ in range(n)]
        meds.append(st.median(sample))
    return (pct(meds, 2.5), pct(meds, 97.5))


def load(path):
    txt = Path(path).read_text().strip()
    if not txt:
        return []
    out = []
    for tok in txt.replace("\n", ",").split(","):
        tok = tok.strip()
        if tok:
            out.append(float(tok))
    return out


def label_of(path):
    # td_<label>_<arch>.csv -> <label>
    stem = Path(path).stem
    parts = stem.split("_")
    return parts[1] if len(parts) >= 2 and parts[0] == "td" else stem


def summarize(xs):
    return {
        "n": len(xs),
        "median": st.median(xs),
        "mean": st.fmean(xs),
        "std": st.pstdev(xs) if len(xs) > 1 else 0.0,
        "min": min(xs),
        "p95": pct(xs, 95),
        "p99": pct(xs, 99),
        "max": max(xs),
    }


def main():
    files = sys.argv[1:] or sorted(glob.glob("data/td_*.csv"))
    if not files:
        print("Khong tim thay data/td_*.csv (chay tu thu muc goc repo).")
        return

    groups = {}  # label -> {"full": [...], "steady": [...]}
    for f in files:
        xs = load(f)
        lab = label_of(f)
        if not xs:
            print(f"[{lab:14s}] {f}: RONG (0 so) - bo qua")
            continue
        steady = xs[WARMUP:] if len(xs) > WARMUP else xs
        groups[lab] = {"full": xs, "steady": steady, "path": f}

    if not groups:
        print("Khong co file nao co du lieu.")
        return

    hdr = (f"{'group':16s}{'set':8s}{'n':>5s}{'median':>9s}{'mean':>9s}"
           f"{'std':>8s}{'p95':>9s}{'p99':>9s}{'min':>8s}{'max':>9s}"
           f"   CI95(median)")
    print(f"\n=== Track-D handshake latency (ms)  |  warm-up bo {WARMUP} handshake dau ===")
    print(hdr)
    print("-" * len(hdr))
    for lab, g in groups.items():
        for which in ("full", "steady"):
            xs = g[which]
            s = summarize(xs)
            if which == "steady":
                lo, hi = boot_median_ci(xs)
                ci = f"[{lo:.3f}, {hi:.3f}]"
            else:
                ci = ""
            print(f"{lab:16s}{which:8s}{s['n']:>5d}{s['median']:>9.3f}"
                  f"{s['mean']:>9.3f}{s['std']:>8.3f}{s['p95']:>9.3f}"
                  f"{s['p99']:>9.3f}{s['min']:>8.3f}{s['max']:>9.3f}   {ci}")
        print()

    # Overhead so voi x25519 (neu co), tinh tren tap steady, theo median.
    base = None
    for cand in ("x25519", "X25519"):
        if cand in groups:
            base = cand
            break
    if base:
        bx = groups[base]["steady"]
        bmed = st.median(bx)
        nb = len(bx)
        print(f"=== Overhead handshake so voi {base} (median, tap steady) ===")
        for lab, g in groups.items():
            if lab == base:
                continue
            gx = g["steady"]
            gmed = st.median(gx)
            over = (gmed / bmed - 1.0) * 100.0
            # CI bootstrap cho ti so median (2 nhom doc lap -> resample rieng)
            ng = len(gx)
            ratios = []
            for _ in range(BOOT):
                bs = st.median([bx[random.randrange(nb)] for _ in range(nb)])
                gs = st.median([gx[random.randrange(ng)] for _ in range(ng)])
                ratios.append((gs / bs - 1.0) * 100.0)
            clo, chi = pct(ratios, 2.5), pct(ratios, 97.5)
            print(f"  {lab:16s} median {gmed:.3f} ms  vs {bmed:.3f} ms"
                  f"  ->  {over:+.1f}%   CI95 [{clo:+.1f}%, {chi:+.1f}%]")
        print()


if __name__ == "__main__":
    main()
