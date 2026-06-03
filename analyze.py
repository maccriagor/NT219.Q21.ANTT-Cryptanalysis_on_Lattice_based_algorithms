#!/usr/bin/env python3
"""
analyze.py - Phan tich benchmark PQC (NT219).

INPUT CHINH: cac file CSV "raw samples" do harness bench_evp sinh ra qua BENCH_CSV,
header: algo,op,iter,wall_ns,cycles  (moi dong = 1 lan do mot thao tac).
Tu mau tho nay tinh ĐAY ĐU:
    n, mean, median, std (ddof=1), p95, p99,
    95% CI cua MEAN (normal: mean +/- 1.96*std/sqrt(n)),
    95% CI cua MEDIAN (bootstrap),
    ops_per_sec
cho ca wall-clock (us) lan cycles.

FALLBACK: neu khong co CSV raw, doc file result_* dinh dang key-value cu cua
lelegard (-microsec/-count/-persec) -- moi op chi 1 mau nen std=0.

XUAT: summary.csv (day du) + RESULTS.txt (bang doc duoc, CO mean & std) + bieu do.

Sua so voi ban cu:
  * IN CA mean va std (ban cu tinh nhung bo sot khoi bang RESULTS.txt).
  * Them 95% CI cua MEAN (ban cu chi co bootstrap CI cua median).
  * Doc dung dinh dang harness moi (CSV wall_ns/cycles), khong con vo nghia
    khi gap khoa '-wall-median-ns' nhu parser cu.
  * Chuan hoa ten thuat toan khi map muc bao mat (vd 'ecdsa-P-256').
"""
import argparse
import glob
import os
import sys
import numpy as np
import pandas as pd

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    HAVE_MPL = True
except Exception:
    HAVE_MPL = False

RAW_COLS = {"algo", "op", "iter", "wall_ns", "cycles"}

# Nhom muc bao mat tuong duong (NIST). Khop sau khi chuan hoa (bo '-', lowercase).
SECURITY_LEVELS = {
    "L1": ["mlkem512", "mldsa44", "rsa3072", "ecdsap256", "ecdhp256"],
    "L3": ["mlkem768", "mldsa65", "rsa7680", "ecdsap384", "ecdhp384"],
    "L5": ["mlkem1024", "mldsa87", "rsa15360", "ecdsap521", "ecdhp521"],
}

# ---- dinh dang key-value cu (fallback) ----
SCALAR_KEYS = {"openssl", "algo", "key-size", "data-size", "output-size",
               "encrypted-size", "decrypted-size", "pss-digest-size",
               "signature-size", "wrapped-size", "unwrapped-size"}
METRIC_SUFFIXES = {"microsec", "count", "persec", "size", "bitrate"}


# ===========================================================================
# Thong ke tu mot mang mau
# ===========================================================================
def bootstrap_ci_median(x, n_boot=2000, ci=95, seed=12345):
    """Bootstrap khoang tin cay cho MEDIAN (vectorized)."""
    x = np.asarray(x, dtype=float)
    x = x[np.isfinite(x)]
    if x.size == 0:
        return (np.nan, np.nan)
    if x.size == 1:
        return (float(x[0]), float(x[0]))
    rng = np.random.default_rng(seed)
    idx = rng.integers(0, x.size, size=(n_boot, x.size))
    boots = np.median(x[idx], axis=1)
    lo = float(np.percentile(boots, (100 - ci) / 2))
    hi = float(np.percentile(boots, 100 - (100 - ci) / 2))
    return (lo, hi)


def stats_from_samples(x):
    """Tra ve dict thong ke day du tu mang mau x."""
    x = np.asarray(x, dtype=float)
    x = x[np.isfinite(x)]
    n = int(x.size)
    out = dict(n=n, mean=np.nan, median=np.nan, std=np.nan, p95=np.nan, p99=np.nan,
               ci_mean_lo=np.nan, ci_mean_hi=np.nan,
               ci_med_lo=np.nan, ci_med_hi=np.nan)
    if n == 0:
        return out
    out["mean"] = float(np.mean(x))
    out["median"] = float(np.median(x))
    out["p95"] = float(np.percentile(x, 95))
    out["p99"] = float(np.percentile(x, 99))
    if n > 1:
        out["std"] = float(np.std(x, ddof=1))               # sample std
        half = 1.96 * out["std"] / np.sqrt(n)               # CI 95% cua MEAN (normal)
        out["ci_mean_lo"] = out["mean"] - half
        out["ci_mean_hi"] = out["mean"] + half
    else:
        out["std"] = 0.0
        out["ci_mean_lo"] = out["ci_mean_hi"] = out["mean"]
    out["ci_med_lo"], out["ci_med_hi"] = bootstrap_ci_median(x)  # CI 95% cua MEDIAN
    return out


# ===========================================================================
# Doc CSV raw (input chinh)
# ===========================================================================
def load_raw_csv(input_dir):
    """Doc tat ca CSV co header algo,op,iter,wall_ns,cycles. Tra ve DataFrame gop."""
    frames = []
    for path in sorted(glob.glob(os.path.join(input_dir, "*.csv"))):
        try:
            df = pd.read_csv(path)
        except Exception as exc:
            print(f"[CANH BAO] Khong doc duoc {path}: {exc}", file=sys.stderr)
            continue
        if not RAW_COLS.issubset(df.columns):
            continue  # khong phai CSV raw cua harness
        frames.append(df[list(RAW_COLS)])
    if not frames:
        return None
    return pd.concat(frames, ignore_index=True)


def summarize_raw(raw):
    """Tu DataFrame raw -> bang thong ke moi (algo, op), cho wall (us) va cycles."""
    rows = []
    for (algo, op), g in raw.groupby(["algo", "op"]):
        wall_us = g["wall_ns"].to_numpy(dtype=float) / 1000.0   # ns -> us
        cyc = g["cycles"].to_numpy(dtype=float)
        sw = stats_from_samples(wall_us)
        sc = stats_from_samples(cyc)
        row = {"algo": algo, "op": op, "n": sw["n"]}
        for k, v in sw.items():
            if k != "n":
                row[f"wall_us_{k}"] = v
        for k, v in sc.items():
            if k != "n":
                row[f"cyc_{k}"] = v
        row["ops_per_sec"] = (1e6 / sw["median"]
                              if sw["median"] and sw["median"] > 0 else np.nan)
        rows.append(row)
    return pd.DataFrame(rows)


# ===========================================================================
# Fallback: doc dinh dang key-value cu (lelegard) -- moi op 1 mau
# ===========================================================================
def parse_keyvalue_file(path):
    meta = {"_file": os.path.basename(path)}
    rec = {}
    with open(path, "r", errors="replace") as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("#") or ":" not in line:
                continue
            key, _, val = line.partition(":")
            key, val = key.strip(), val.strip()
            if key in SCALAR_KEYS:
                meta[key] = val
                continue
            parts = key.rsplit("-", 1)
            if len(parts) != 2 or parts[1] not in METRIC_SUFFIXES:
                continue
            op, metric = parts
            try:
                rec.setdefault(op, {})[metric] = float(val)
            except ValueError:
                pass
    return meta, rec


def summarize_keyvalue(input_dir):
    rows = []
    files = [f for f in sorted(glob.glob(os.path.join(input_dir, "result_*")))]
    files += [f for f in sorted(glob.glob(os.path.join(input_dir, "*.txt")))]
    for path in files:
        meta, rec = parse_keyvalue_file(path)
        algo = meta.get("algo",
                        os.path.basename(path).replace("result_", "").lower())
        for op, m in rec.items():
            micro = m.get("microsec")
            count = m.get("count")
            persec = m.get("persec")
            lat_us = (micro / count) if (micro and count) else (
                     (1e6 / persec) if persec else np.nan)
            rows.append({"algo": algo, "op": op, "n": 1,
                         "wall_us_mean": lat_us, "wall_us_median": lat_us,
                         "wall_us_std": 0.0, "wall_us_p95": lat_us,
                         "wall_us_p99": lat_us, "wall_us_ci_mean_lo": lat_us,
                         "wall_us_ci_mean_hi": lat_us,
                         "ops_per_sec": (persec if persec else
                                         (1e6 / lat_us if lat_us else np.nan))})
    return pd.DataFrame(rows)


# ===========================================================================
# Muc bao mat + bieu do
# ===========================================================================
def normalize(s):
    return "".join(ch for ch in s.lower() if ch.isalnum())


def security_level_of(algo):
    a = normalize(algo)
    for lvl, members in SECURITY_LEVELS.items():
        for m in members:
            if normalize(m) in a:
                return lvl
    return "other"


def plot_comparisons(df, out_dir):
    if not HAVE_MPL:
        print("[CANH BAO] matplotlib khong san co, bo qua ve bieu do.", file=sys.stderr)
        return
    # 1) median wall_us co error bar = CI 95% cua median
    if "wall_us_median" in df.columns and not df["wall_us_median"].dropna().empty:
        ops = sorted(df["op"].unique())
        algos = sorted(df["algo"].unique())
        x = np.arange(len(ops))
        width = 0.8 / max(len(algos), 1)
        fig, ax = plt.subplots(figsize=(12, 6))
        for i, algo in enumerate(algos):
            sub = df[df["algo"] == algo].set_index("op")
            med, lo, hi = [], [], []
            for o in ops:
                if o in sub.index:
                    m = sub.loc[o, "wall_us_median"]
                    med.append(m)
                    lo.append(m - sub.loc[o, "wall_us_ci_med_lo"]
                              if "wall_us_ci_med_lo" in sub.columns else 0)
                    hi.append(sub.loc[o, "wall_us_ci_med_hi"] - m
                              if "wall_us_ci_med_hi" in sub.columns else 0)
                else:
                    med.append(np.nan); lo.append(0); hi.append(0)
            ax.bar(x + i * width, med, width, label=algo,
                   yerr=[np.abs(lo), np.abs(hi)], capsize=3)
        ax.set_xticks(x + width * len(algos) / 2)
        ax.set_xticklabels(ops, rotation=30, ha="right")
        ax.set_ylabel("Latency median (us) — thap = tot (error bar = CI95 median)")
        ax.set_title("PQC vs classical — latency theo thao tac")
        ax.legend(fontsize=8, ncol=2)
        fig.tight_layout()
        fig.savefig(os.path.join(out_dir, "latency_median_us.png"), dpi=120)
        plt.close(fig)

    # 2) ops_per_sec
    if "ops_per_sec" in df.columns and not df["ops_per_sec"].dropna().empty:
        ops = sorted(df["op"].unique())
        algos = sorted(df["algo"].unique())
        x = np.arange(len(ops))
        width = 0.8 / max(len(algos), 1)
        fig, ax = plt.subplots(figsize=(12, 6))
        for i, algo in enumerate(algos):
            sub = df[df["algo"] == algo].set_index("op")
            y = [sub.loc[o, "ops_per_sec"] if o in sub.index else np.nan for o in ops]
            ax.bar(x + i * width, y, width, label=algo)
        ax.set_xticks(x + width * len(algos) / 2)
        ax.set_xticklabels(ops, rotation=30, ha="right")
        ax.set_ylabel("Operations / second — cao = tot")
        ax.set_title("PQC vs classical — throughput")
        ax.legend(fontsize=8, ncol=2)
        fig.tight_layout()
        fig.savefig(os.path.join(out_dir, "ops_per_sec.png"), dpi=120)
        plt.close(fig)


# ===========================================================================
# main
# ===========================================================================
def main():
    ap = argparse.ArgumentParser(description="Phan tich benchmark PQC (NT219).")
    ap.add_argument("-i", "--input", default="./results",
                    help="Thu muc chua CSV raw (algo,op,iter,wall_ns,cycles) "
                         "hoac file result_* (mac dinh ./results)")
    ap.add_argument("-o", "--output", default="./analysis_out",
                    help="Thu muc xuat ket qua")
    args = ap.parse_args()

    os.makedirs(args.output, exist_ok=True)

    raw = load_raw_csv(args.input)
    if raw is not None and not raw.empty:
        print(f"[INFO] Doc {len(raw)} mau tho tu CSV trong {args.input}")
        df = summarize_raw(raw)
        src = "raw-csv"
    else:
        print(f"[INFO] Khong thay CSV raw -> fallback doc key-value result_*",
              file=sys.stderr)
        df = summarize_keyvalue(args.input)
        src = "key-value"

    if df.empty:
        print("Khong co du lieu hop le.", file=sys.stderr)
        sys.exit(1)

    df["level"] = df["algo"].map(security_level_of)
    df = df.sort_values(["level", "algo", "op"]).reset_index(drop=True)

    # summary.csv day du
    csv_path = os.path.join(args.output, "summary.csv")
    df.to_csv(csv_path, index=False)

    # RESULTS.txt: bang doc duoc, CO mean & std (sua loi cu)
    txt_path = os.path.join(args.output, "RESULTS.txt")
    cols = [c for c in ["level", "algo", "op", "n",
                        "wall_us_mean", "wall_us_median", "wall_us_std",
                        "wall_us_p95", "wall_us_p99",
                        "wall_us_ci_mean_lo", "wall_us_ci_mean_hi",
                        "ops_per_sec", "cyc_median"] if c in df.columns]
    with open(txt_path, "w") as fh:
        fh.write(f"== BANG TONG HOP BENCHMARK PQC (NT219) [nguon: {src}] ==\n")
        fh.write("Don vi wall_us = microsecond. ci_mean = 95% CI cua trung binh "
                 "(normal). cyc_median = cycle trung vi.\n\n")
        fh.write(df[cols].to_string(index=False,
                                    float_format=lambda v: f"{v:,.3f}"))
        fh.write("\n")

    plot_comparisons(df, args.output)
    print(f"Da ghi: {csv_path}, {txt_path}"
          + ("" if not HAVE_MPL else f" va bieu do PNG trong {args.output}"))


if __name__ == "__main__":
    main()
