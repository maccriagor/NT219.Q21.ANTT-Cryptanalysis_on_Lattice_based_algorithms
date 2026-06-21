#!/usr/bin/env python3
# =============================================================================
# td_stats.py - Summarize Track-D handshake stats (tls13-scratch bench mode).
#
# Each data/td_<label>_<arch>.csv is ONE line "t1,t2,...,tN" (ms, raw). Drops
# the warm-up (first W handshakes), then prints median/mean/std/p95/p99/min/max
# + a bootstrap 95% CI of the median, for both the full and the steady
# (post-warm-up) set per label. With an 'x25519' label present, also prints each
# other group's % overhead vs it (by median, steady set) with a bootstrap CI.
#
# Usage:
#   python3 scripts/td_stats.py                       # read every data/td_*.csv
#   python3 scripts/td_stats.py data/td_hybrid_x86_64.csv ...
#
# Environment variables:
#   WARMUP=20   number of leading handshakes dropped as warm-up
#   BOOT=5000   number of bootstrap resamples
# =============================================================================
import glob
import os
import random
import statistics as st
import sys
from pathlib import Path

WARMUP = int(os.environ.get("WARMUP", "20"))   # leading handshakes treated as warm-up
BOOT = int(os.environ.get("BOOT", "5000"))     # number of bootstrap resamples
random.seed(12345)                             # reproducible


def pct(xs, p):
    """Percentile p (0..100), linear interpolation (numpy default style)."""
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
    """95% CI of the median via bootstrap (percentile method)."""
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
        print("No data/td_*.csv found (run from the repo root directory).")
        return

    groups = {}  # label -> {"full": [...], "steady": [...]}
    for f in files:
        xs = load(f)
        lab = label_of(f)
        if not xs:
            print(f"[{lab:14s}] {f}: EMPTY (0 numbers) - skipped")
            continue
        steady = xs[WARMUP:] if len(xs) > WARMUP else xs
        groups[lab] = {"full": xs, "steady": steady}

    if not groups:
        print("No file has any data.")
        return

    hdr = (f"{'group':16s}{'set':8s}{'n':>5s}{'median':>9s}{'mean':>9s}"
           f"{'std':>8s}{'p95':>9s}{'p99':>9s}{'min':>8s}{'max':>9s}"
           f"   CI95(median)")
    print(f"\n=== Track-D handshake latency (ms)  |  warm-up: drop first {WARMUP} handshakes ===")
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

    # Overhead relative to x25519 (if present), computed on the steady set, by median.
    base = None
    for cand in ("x25519", "X25519"):
        if cand in groups:
            base = cand
            break
    if base:
        bx = groups[base]["steady"]
        bmed = st.median(bx)
        nb = len(bx)
        print(f"=== Handshake overhead relative to {base} (median, steady set) ===")
        for lab, g in groups.items():
            if lab == base:
                continue
            gx = g["steady"]
            gmed = st.median(gx)
            over = (gmed / bmed - 1.0) * 100.0
            # bootstrap CI for the median ratio (2 independent groups -> resample separately)
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
