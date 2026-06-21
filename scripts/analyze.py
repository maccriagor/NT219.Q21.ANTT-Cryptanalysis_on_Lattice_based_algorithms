#!/usr/bin/env python3
# =============================================================================
# analyze.py - Aggregate benchmark CSVs into tables + charts (brief section 9
# "Presentation": latency vs parameter set, throughput, bytes vs algorithm;
# week-11 "statistical analysis, prepare figures & tables").
#
# Inputs (any subset may exist; produced by the measurement scripts):
#   data/summary_micro_<arch>.csv   long format: algo,metric,value
#   data/memory_<arch>.csv          algo,peak_rss_kb
#   data/codesize_<arch>.csv        file,text_bytes,data_bytes,bss_bytes,total_bytes
#   data/bench_oqs_<arch>.csv       arch,variant,algo,op,wall_median_ns,...  (liboqs ref vs opt)
#   data/nginx_handshake_<arch>.csv arch,cert,group,hs_median_ms,hs_mean_ms,hs_p95_ms,iters,cert_bytes
#
# Output: analysis_out/tables.md (+ *.png charts when matplotlib is available)
# Usage : python3 scripts/analyze.py        (from repo root or anywhere)
# =============================================================================
import csv
import statistics
import sys
from pathlib import Path
from collections import defaultdict

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "data"
OUT = ROOT / "analysis_out"
OUT.mkdir(exist_ok=True)

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    HAVE_MPL = True
except ImportError:
    HAVE_MPL = False
    print("NOTE: matplotlib not found -> tables only "
          "(pip install matplotlib --break-system-packages)")


def read_csv(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def fnum(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


def fmt_val(x):
    """Plain decimal for the summary_agg CSV: full precision for whole numbers,
    NO scientific notation. Replaces f"{x:.6g}" which both turned big counts into
    1.20804e+06 AND truncated to 6 sig-figs (e.g. true median 1208040.5 -> 1208040,
    losing the .5). Whole -> int; |x|>=1 -> trimmed decimals; tiny -> 4 sig-figs."""
    x = float(x)
    if x == int(x):
        return str(int(x))
    if abs(x) >= 1:
        return f"{x:.3f}".rstrip("0").rstrip(".")
    return f"{x:.4g}"


def md_table(headers, rows):
    out = ["| " + " | ".join(headers) + " |",
           "|" + "|".join(["---"] * len(headers)) + "|"]
    out += ["| " + " | ".join(str(c) for c in r) + " |" for r in rows]
    return "\n".join(out) + "\n"


def bar_chart(labels, series, title, ylabel, fname, log=False):
    """series: list of (name, values) drawn side by side."""
    if not HAVE_MPL or not labels:
        return
    import numpy as np
    x = np.arange(len(labels))
    width = 0.8 / max(len(series), 1)
    fig, ax = plt.subplots(figsize=(max(6, len(labels) * 0.9), 4))
    for i, (name, vals) in enumerate(series):
        ax.bar(x + i * width, vals, width, label=name)
    ax.set_xticks(x + width * (len(series) - 1) / 2)
    ax.set_xticklabels(labels, rotation=30, ha="right", fontsize=8)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    if log:
        ax.set_yscale("log")
    if len(series) > 1:
        ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(OUT / fname, dpi=140)
    plt.close(fig)
    print(f"  chart: analysis_out/{fname}")


def load_micro(arch):
    """Source for one arch. Prefer the K independent batches
    (data/raw/<arch>/summary_batch*.csv) and report the MEDIAN-OF-MEDIANS, also
    writing summary_agg_<arch>.csv (+ _spread.csv: n,median,mean,min,max,cv_pct;
    cv = batch-to-batch noise, high = unstable e.g. RSA keygen). Fall back to the
    single latest summary_micro_<arch>.csv when no batches were archived.
    Returns algo -> metric -> value. This is the merge step folded INTO analyze:
    no separate script and no 'cp summary_agg summary_micro' overwrite needed."""
    table = defaultdict(dict)
    batches = sorted((DATA / "raw" / arch).glob("summary_batch*.csv"))
    if batches:
        samples = defaultdict(list)          # (algo, metric) -> [v_batch1, ...]
        for b in batches:
            for r in read_csv(b):
                v = fnum(r.get("value"))
                if v is not None:
                    samples[(r["algo"], r["metric"])].append(v)
        agg = DATA / f"summary_agg_{arch}.csv"
        spread = DATA / f"summary_agg_{arch}_spread.csv"
        with open(agg, "w", newline="") as fa, open(spread, "w", newline="") as fs:
            wa = csv.writer(fa, lineterminator="\n")
            ws = csv.writer(fs, lineterminator="\n")
            wa.writerow(["algo", "metric", "value"])
            ws.writerow(["algo", "metric", "n", "median", "mean",
                         "min", "max", "cv_pct"])
            for (algo, metric), vals in sorted(samples.items()):
                med = statistics.median(vals)
                mean = statistics.fmean(vals)
                sd = statistics.stdev(vals) if len(vals) > 1 else 0.0
                cv = (100.0 * sd / mean) if mean else 0.0
                wa.writerow([algo, metric, fmt_val(med)])
                ws.writerow([algo, metric, len(vals), fmt_val(med), fmt_val(mean),
                             fmt_val(min(vals)), fmt_val(max(vals)), f"{cv:.2f}"])
                table[algo][metric] = med
        print(f"  {arch}: median-of-medians over {len(batches)} batch(es) "
              f"-> data/{agg.name} (+ _spread.csv)")
    elif (DATA / f"summary_micro_{arch}.csv").exists():
        for r in read_csv(DATA / f"summary_micro_{arch}.csv"):
            v = fnum(r.get("value"))
            if v is not None:
                table[r["algo"]][r["metric"]] = v
        print(f"  {arch}: single batch (summary_micro_{arch}.csv)")
    return table


report = ["# Benchmark analysis\n"]
# An arch counts if it has a single summary OR archived K-batches.
_batch_arches = {d.name for d in (DATA / "raw").glob("*")
                 if d.is_dir() and any(d.glob("summary_batch*.csv"))}
arches = sorted({p.stem.removeprefix("summary_micro_")
                 for p in DATA.glob("summary_micro_*.csv")} | _batch_arches)

# ---- 1) Microbenchmark latency: median-of-medians over K batches ------------
micro = {arch: load_micro(arch) for arch in arches}  # arch -> algo -> metric -> value

for arch, table in micro.items():
    medians = sorted({m for a in table.values() for m in a if "median" in m})
    if not medians:
        continue
    report.append(f"\n## Microbenchmark medians ({arch})\n")
    algos = sorted(table)
    rows = [[a] + [table[a].get(m, "") for m in medians] for a in algos]
    report.append(md_table(["algo"] + medians, rows))
    for m in medians:
        labels = [a for a in algos if m in table[a]]
        bar_chart(labels, [(arch, [table[a][m] for a in labels])],
                  f"{m} ({arch})", m, f"micro_{m}_{arch}.png", log=True)

# Cross-arch ratio (valid bridge: same algo, ratio across machines)
if len(micro) >= 2:
    a1, a2 = sorted(micro)[:2]
    common = sorted(set(micro[a1]) & set(micro[a2]))
    mets = sorted({m for a in common for m in micro[a1][a] if "median" in m})
    rows = []
    for a in common:
        for m in mets:
            v1, v2 = micro[a1][a].get(m), micro[a2][a].get(m)
            if v1 and v2:
                rows.append([a, m, v1, v2, f"{v2 / v1:.2f}x"])
    if rows:
        report.append(f"\n## Cross-platform ratio ({a2} / {a1})\n")
        report.append(md_table(["algo", "metric", a1, a2, "ratio"], rows))

# ---- 2) Peak RSS ------------------------------------------------------------
for p in sorted(DATA.glob("memory_*.csv")):
    arch = p.stem.removeprefix("memory_")
    rows = read_csv(p)
    rows = [r for r in rows if fnum(r.get("peak_rss_kb")) is not None]
    if not rows:
        continue
    report.append(f"\n## Peak RSS ({arch})\n")
    report.append(md_table(["algo", "peak_rss_kb"],
                           [[r["algo"], r["peak_rss_kb"]] for r in rows]))
    bar_chart([r["algo"] for r in rows],
              [(arch, [fnum(r["peak_rss_kb"]) for r in rows])],
              f"Peak RSS ({arch})", "KB", f"memory_{arch}.png")

# ---- 3) Code size -----------------------------------------------------------
for p in sorted(DATA.glob("codesize_*.csv")):
    arch = p.stem.removeprefix("codesize_")
    rows = read_csv(p)
    rows = [r for r in rows if fnum(r.get("total_bytes")) is not None]
    if not rows:
        continue
    rows.sort(key=lambda r: -fnum(r["total_bytes"]))
    report.append(f"\n## Code size ({arch})\n")
    report.append(md_table(["file", "text", "data", "bss", "total"],
                           [[r["file"], r["text_bytes"], r["data_bytes"],
                             r["bss_bytes"], r["total_bytes"]] for r in rows]))
    bar_chart([r["file"] for r in rows],
              [(arch, [fnum(r["total_bytes"]) / 1024 for r in rows])],
              f"Code size ({arch})", "KiB", f"codesize_{arch}.png", log=True)

# ---- 3b) PQClean per-scheme code size (the trustworthy PER-ALGORITHM number)
# The codesize_*.csv above is the MIXED libcrypto/liboqs; the real per-scheme PQC
# size comes from `size -t lib<scheme>_<impl>.a` saved to data/raw/<arch>/
# pqclean_*_size.txt (each line: "<lib> text data bss dec hex (TOTALS)"; the §4.3/
# §5.4 commands write basename on x86 and full path on ARM -> Path().name handles both).
for d in sorted((DATA / "raw").glob("*")):
    if not d.is_dir():
        continue
    arch = d.name
    seen = {}  # (scheme, impl) -> (text, data, bss, total)  (dedup across files)
    for f in sorted(d.glob("pqclean_*_size.txt")):
        for line in f.read_text().splitlines():
            parts = line.split()
            if len(parts) < 5:
                continue
            try:
                text, data, bss, total = (int(parts[1]), int(parts[2]),
                                          int(parts[3]), int(parts[4]))
            except ValueError:
                continue                       # header / malformed / non-TOTALS line
            base = Path(parts[0]).name          # basename (x86) or from full path (ARM)
            if base.startswith("lib"):
                base = base[3:]
            if base.endswith(".a"):
                base = base[:-2]
            scheme, _, impl = base.rpartition("_")   # ml-kem-768_clean -> ml-kem-768, clean
            seen[(scheme or base, impl or "-")] = (text, data, bss, total)
    if seen:
        srows = [[s, i, t, da, b, tot] for (s, i), (t, da, b, tot) in sorted(seen.items())]
        report.append(f"\n## PQClean per-scheme code size ({arch})\n")
        report.append(md_table(["scheme", "impl", "text", "data", "bss", "total"], srows))
        bar_chart([f"{r[0]}.{r[1]}" for r in srows],
                  [("total bytes", [r[5] for r in srows])],
                  f"PQClean per-scheme code size ({arch})", "bytes",
                  f"pqclean_codesize_{arch}.png", log=True)

# ---- 4) TLS 1.3 handshake via nginx -----------------------------------------
# data/nginx_handshake_<arch>.csv (from nginx-bench/run.sh): one row per
# (cert, KEX group) with handshake median/mean/p95 over ITERS connections.
for p in sorted(DATA.glob("nginx_handshake_*.csv")):
    arch = p.stem.removeprefix("nginx_handshake_")
    rows = read_csv(p)
    if not rows:
        continue
    report.append(f"\n## TLS 1.3 handshake via nginx ({arch})\n")
    report.append(md_table(
        ["cert", "group", "median_ms", "mean_ms", "p95_ms", "iters", "cert_bytes"],
        [[r["cert"], r["group"], r["hs_median_ms"], r["hs_mean_ms"],
          r["hs_p95_ms"], r["iters"], r["cert_bytes"]] for r in rows]))
    ok = [r for r in rows if fnum(r.get("hs_median_ms")) is not None]
    groups = sorted({r["group"] for r in ok})
    certs = sorted({r["cert"] for r in ok})
    if groups and certs:
        series = []
        for c in certs:
            by = {r["group"]: fnum(r["hs_median_ms"]) for r in ok if r["cert"] == c}
            series.append((c, [by.get(g, 0) for g in groups]))
        bar_chart(groups, series, f"TLS handshake median via nginx ({arch})",
                  "ms", f"nginx_handshake_{arch}.png")

# ---- 5b) EVP vs liboqs ref/opt cross-check (bench_oqs) ----------------------
# data/bench_oqs_<arch>.csv (from scripts/run_oqs.sh) carries the liboqs-DIRECT
# medians; join them with the EVP medians (same algo/op names, e.g.
# "encap-wall-median-ns") to show EVP vs liboqs-ref vs liboqs-opt side by side:
# a measurement-soundness cross-check AND the SIMD speedup in one table.
for p in sorted(DATA.glob("bench_oqs_*.csv")):
    arch = p.stem.removeprefix("bench_oqs_")
    oqs = defaultdict(dict)  # (algo, op) -> variant -> median_ns
    for r in read_csv(p):
        v = fnum(r.get("wall_median_ns"))
        if v is not None:
            oqs[(r["algo"], r["op"])][r["variant"]] = v
    rows = []
    for (algo, op), d in sorted(oqs.items()):
        ref, opt = d.get("ref"), d.get("opt")
        evp = micro.get(arch, {}).get(algo, {}).get(f"{op}-wall-median-ns")
        rows.append([
            algo, op,
            f"{evp:.0f}" if evp else "-",
            f"{ref:.0f}" if ref else "-",
            f"{opt:.0f}" if opt else "-",
            f"x{ref / opt:.2f}" if (ref and opt) else "-",   # ref/opt SIMD speedup
            f"{evp / opt:.2f}" if (evp and opt) else "-",     # EVP overhead vs liboqs-opt
        ])
    if rows:
        report.append(f"\n## EVP vs liboqs ref/opt - median ns ({arch})\n")
        report.append(md_table(
            ["algo", "op", "EVP_ns", "liboqs_ref_ns", "liboqs_opt_ns",
             "ref/opt", "EVP/opt"], rows))
        sp = [(a, o, d["ref"] / d["opt"]) for (a, o), d in sorted(oqs.items())
              if d.get("ref") and d.get("opt")]
        if sp:
            bar_chart([f"{a}.{o}" for a, o, _ in sp],
                      [("ref/opt", [x for *_, x in sp])],
                      f"bench_oqs SIMD speedup ref->opt ({arch})", "x times",
                      f"bench_oqs_speedup_{arch}.png")

(OUT / "tables.md").write_text("\n".join(report))
print(f"DONE. Tables: analysis_out/tables.md"
      + ("" if HAVE_MPL else "  (charts skipped: no matplotlib)"))
if not any(DATA.glob("*.csv")):
    print("WARNING: data/ has no CSVs yet - run 'make bench' / 'make memory' / "
          "'make codesize' first")
    sys.exit(1)
