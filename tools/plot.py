#!/usr/bin/env python3
"""plot.py — Sinh biểu đồ báo cáo dạng SVG THUẦN (không cần matplotlib/numpy).

Đọc data/micro/summary_x86.csv + data/micro/x86/* + data/micro/arm/* + data/tls/netem_matrix.csv
-> docs/report/figures/*.svg. Mọi figure dùng số ĐO THẬT (x86) trừ khi ghi "(MÔ HÌNH)".
"""
import csv, math, os, statistics

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIG = os.path.join(ROOT, "docs", "report", "figures")
os.makedirs(FIG, exist_ok=True)

PALETTE = ["#2563eb", "#dc2626", "#16a34a", "#d97706", "#7c3aed", "#0891b2", "#db2777"]


def esc(s): return str(s).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def svg_open(w, h, title):
    return [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
            f'viewBox="0 0 {w} {h}" font-family="DejaVu Sans, Arial, sans-serif">',
            f'<rect width="{w}" height="{h}" fill="white"/>',
            f'<text x="{w/2}" y="24" text-anchor="middle" font-size="16" font-weight="bold">{esc(title)}</text>']


def write(name, parts):
    parts.append("</svg>")
    open(os.path.join(FIG, name), "w").write("\n".join(parts))
    print("  figure:", name)


def bar_chart(name, title, labels, series, ylabel="ns (median)", logscale=False, note=""):
    """series = list of (legend, [values], color)."""
    W, H = 820, 460
    L, R, T, B = 70, 30, 60, 150
    pw, ph = W - L - R, H - T - B
    s = svg_open(W, H, title)
    allv = [v for _, vs, _ in series for v in vs if v > 0]
    if not allv: allv = [1]
    vmax = max(allv)
    vmin = min(allv) if logscale else 0
    def yof(v):
        if logscale:
            lo, hi = math.log10(max(vmin, 1e-9)), math.log10(vmax * 1.2)
            v = max(v, 10 ** lo)
            return T + ph - (math.log10(v) - lo) / (hi - lo) * ph
        return T + ph - (v / (vmax * 1.1)) * ph
    s.append(f'<line x1="{L}" y1="{T}" x2="{L}" y2="{T+ph}" stroke="#333"/>')
    s.append(f'<line x1="{L}" y1="{T+ph}" x2="{L+pw}" y2="{T+ph}" stroke="#333"/>')
    s.append(f'<text x="16" y="{T+ph/2}" font-size="11" transform="rotate(-90 16 {T+ph/2})" text-anchor="middle">{esc(ylabel)}</text>')
    if logscale:
        e = int(math.log10(vmax))
        for k in range(0, e + 2):
            yy = yof(10 ** k)
            if T <= yy <= T + ph:
                s.append(f'<line x1="{L}" y1="{yy:.1f}" x2="{L+pw}" y2="{yy:.1f}" stroke="#eee"/>')
                s.append(f'<text x="{L-5}" y="{yy+3:.1f}" font-size="9" text-anchor="end">1e{k}</text>')
    else:
        for k in range(0, 6):
            yy = T + ph - k / 5 * ph
            s.append(f'<line x1="{L}" y1="{yy:.1f}" x2="{L+pw}" y2="{yy:.1f}" stroke="#eee"/>')
            s.append(f'<text x="{L-5}" y="{yy+3:.1f}" font-size="9" text-anchor="end">{vmax*1.1*k/5:.0f}</text>')
    ng = len(labels); nb = len(series)
    gw = pw / ng; bw = gw * 0.8 / max(1, nb)
    for gi, lab in enumerate(labels):
        gx = L + gi * gw + gw * 0.1
        for bi, (leg, vs, col) in enumerate(series):
            v = vs[gi] if gi < len(vs) else 0
            if v <= 0: continue
            x = gx + bi * bw; y = yof(v); hh = T + ph - y
            s.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{bw*0.92:.1f}" height="{hh:.1f}" fill="{col}"/>')
        s.append(f'<text x="{gx+gw*0.4:.1f}" y="{T+ph+14}" font-size="10" text-anchor="middle">{esc(lab)}</text>')
    for bi, (leg, _, col) in enumerate(series):
        ly = T + ph + 36 + bi * 16
        s.append(f'<rect x="{L}" y="{ly-9}" width="12" height="12" fill="{col}"/>')
        s.append(f'<text x="{L+18}" y="{ly+1}" font-size="11">{esc(leg)}</text>')
    if note:
        s.append(f'<text x="{W-R}" y="{H-8}" font-size="10" text-anchor="end" fill="#666">{esc(note)}</text>')
    write(name, s)


def histogram(name, title, values, xlabel="ns", bins=30, note=""):
    W, H = 820, 420
    L, R, T, B = 60, 20, 60, 70
    pw, ph = W - L - R, H - T - B
    s = svg_open(W, H, title)
    vmin, vmax = min(values), max(values)
    bw = (vmax - vmin) / bins or 1
    counts = [0] * bins
    for v in values:
        counts[min(bins - 1, int((v - vmin) / bw))] += 1
    cmax = max(counts) or 1
    s.append(f'<line x1="{L}" y1="{T}" x2="{L}" y2="{T+ph}" stroke="#333"/>')
    s.append(f'<line x1="{L}" y1="{T+ph}" x2="{L+pw}" y2="{T+ph}" stroke="#333"/>')
    for i, c in enumerate(counts):
        x = L + i / bins * pw; hh = c / cmax * ph
        s.append(f'<rect x="{x:.1f}" y="{T+ph-hh:.1f}" width="{pw/bins-1:.1f}" height="{hh:.1f}" fill="{PALETTE[0]}"/>')
    med = statistics.median(values); p95 = sorted(values)[int(0.95 * len(values))]
    for val, col, lab in [(med, "#16a34a", "median"), (p95, "#dc2626", "p95")]:
        x = L + (val - vmin) / (vmax - vmin) * pw
        s.append(f'<line x1="{x:.1f}" y1="{T}" x2="{x:.1f}" y2="{T+ph}" stroke="{col}" stroke-dasharray="4"/>')
        s.append(f'<text x="{x:.1f}" y="{T-4}" font-size="10" fill="{col}" text-anchor="middle">{lab} {val/1000:.0f}µs</text>')
    s.append(f'<text x="{L+pw/2}" y="{H-22}" font-size="11" text-anchor="middle">{esc(xlabel)}</text>')
    s.append(f'<text x="{L+pw/2}" y="{H-8}" font-size="10" text-anchor="middle" fill="#666">{esc(note)}</text>')
    write(name, s)


def col(path, c):
    return [float(r[c]) for r in csv.DictReader(open(path))] if os.path.exists(path) else []


def load_summary():
    p = os.path.join(ROOT, "data", "micro", "summary_x86.csv")
    return list(csv.DictReader(open(p))) if os.path.exists(p) else []


def med_of(sm, scheme, param, op):
    for r in sm:
        if r["scheme"] == scheme and r["param"] == param and r["op"] == op:
            return float(r["median_ns"])
    return 0


def main():
    sm = load_summary()
    MX = os.path.join(ROOT, "data", "micro", "x86")

    bar_chart("fig1_mlkem_levels.svg", "ML-KEM (reference-C, x86) — latency theo mức NIST",
              ["512 (Cat1)", "768 (Cat3)", "1024 (Cat5)"],
              [("Keygen", [med_of(sm, "mlkem", p, "Keygen") for p in ("512", "768", "1024")], PALETTE[0]),
               ("Encaps", [med_of(sm, "mlkem", p, "Encaps") for p in ("512", "768", "1024")], PALETTE[1]),
               ("Decaps", [med_of(sm, "mlkem", p, "Decaps") for p in ("512", "768", "1024")], PALETTE[2])],
              note="đo thật, 100 mẫu/mức")

    bar_chart("fig2_mldsa_levels.svg", "ML-DSA (reference-C, x86) — latency theo mức NIST",
              ["44 (Cat2)", "65 (Cat3)", "87 (Cat5)"],
              [("Keygen", [med_of(sm, "mldsa", p, "Keygen") for p in ("44", "65", "87")], PALETTE[0]),
               ("Sign (median)", [med_of(sm, "mldsa", p, "Sign") for p in ("44", "65", "87")], PALETTE[1]),
               ("Verify", [med_of(sm, "mldsa", p, "Verify") for p in ("44", "65", "87")], PALETTE[2])],
              note="đo thật; Sign lệch phải (rejection sampling)")

    evp = {}
    p = os.path.join(MX, "micro_evp_x86.csv")
    if os.path.exists(p):
        for r in csv.DictReader(open(p)):
            evp[(r["algo"], r["op"])] = float(r["median_ns"])
    bar_chart("fig3_pqc_vs_classical_cat5.svg",
              "Cat 5: PQC vs cổ điển (median ns, thang LOG) — x86",
              ["KEM/KEX keygen", "KEM enc/derive", "SIG keygen", "SIG sign", "SIG verify"],
              [("ML-KEM-1024 / ML-DSA-87",
                [evp.get(("ML-KEM-1024", "keygen"), 0), evp.get(("ML-KEM-1024", "encaps"), 0),
                 evp.get(("ML-DSA-87", "keygen"), 0), evp.get(("ML-DSA-87", "sign"), 0),
                 evp.get(("ML-DSA-87", "verify"), 0)], PALETTE[2]),
               ("ECDH/ECDSA-P-521",
                [evp.get(("ECDH-P-521", "keygen"), 0), evp.get(("ECDH-P-521", "derive"), 0),
                 evp.get(("ECDSA-P-521", "keygen"), 0), evp.get(("ECDSA-P-521", "sign"), 0),
                 evp.get(("ECDSA-P-521", "verify"), 0)], PALETTE[0]),
               ("RSA-15360",
                [0, 0, evp.get(("RSA-15360", "keygen"), 0), evp.get(("RSA-15360", "sign"), 0),
                 evp.get(("RSA-15360", "verify"), 0)], PALETTE[1])],
              logscale=True, note="OpenSSL EVP (tối ưu), thật")

    bar_chart("fig4_reference_vs_optimized.svg",
              "ML-KEM-1024: reference-C (PQClean) vs tối ưu AVX2 (OpenSSL) — x86",
              ["Keygen", "Encaps", "Decaps"],
              [("reference-C", [med_of(sm, "mlkem", "1024", o) for o in ("Keygen", "Encaps", "Decaps")], PALETTE[3]),
               ("AVX2 (OpenSSL)", [evp.get(("ML-KEM-1024", "keygen"), 0), evp.get(("ML-KEM-1024", "encaps"), 0),
                                   evp.get(("ML-KEM-1024", "decaps"), 0)], PALETTE[4])],
              note="đo thật — RQ1: tối ưu hoá quan trọng cỡ nào")

    bar_chart("fig5_sizes.svg", "Kích thước trên dây (bytes): public key + ciphertext/signature",
              ["ML-KEM-1024", "ML-DSA-87", "ECDSA-P-521", "RSA-15360"],
              [("public key", [1568, 2592, 133, 1958], PALETTE[0]),
               ("ct / signature", [1568, 4627, 139, 1920], PALETTE[1])],
              ylabel="bytes", note="PQC pk/ct/sig lớn hơn nhiều — chi phí băng thông")

    rows = list(csv.DictReader(open(os.path.join(MX, "mldsa_x86.csv")))) if os.path.exists(os.path.join(MX, "mldsa_x86.csv")) else []
    signs87 = [float(r["Sign_ns"]) for r in rows if r.get("Param") == "87"]
    if signs87:
        histogram("fig6_mldsa87_sign_dist.svg",
                  "ML-DSA-87 Sign — phân phối lệch phải (rejection sampling), x86",
                  signs87, xlabel="thời gian sign (ns)", note="đo thật, 100 mẫu — vì sao dùng median+p95")

    def arm_med(scheme, param, op):
        p = os.path.join(ROOT, "data", "micro", "arm", f"{scheme}_arm.csv")
        if not os.path.exists(p): return 0
        lines = [l for l in open(p) if not l.startswith("#")]
        for r in csv.DictReader(lines):
            if r["Param"] == param and r["op"] == op: return float(r["arm_median_ns"])
        return 0
    bar_chart("fig7_x86_vs_arm.svg",
              "x86 (đo thật) vs ARM Cortex-A72 (MÔ HÌNH) — reference-C, Cat5",
              ["KEM keygen", "KEM encaps", "KEM decaps", "DSA keygen", "DSA sign", "DSA verify"],
              [("x86 (đo)",
                [med_of(sm, "mlkem", "1024", "Keygen"), med_of(sm, "mlkem", "1024", "Encaps"),
                 med_of(sm, "mlkem", "1024", "Decaps"), med_of(sm, "mldsa", "87", "Keygen"),
                 med_of(sm, "mldsa", "87", "Sign"), med_of(sm, "mldsa", "87", "Verify")], PALETTE[0]),
               ("ARM A72 (mô hình)",
                [arm_med("mlkem", "1024", "Keygen"), arm_med("mlkem", "1024", "Encaps"),
                 arm_med("mlkem", "1024", "Decaps"), arm_med("mldsa", "87", "Keygen"),
                 arm_med("mldsa", "87", "Sign"), arm_med("mldsa", "87", "Verify")], PALETTE[5])],
              note="ARM = MÔ HÌNH (x86×4.6) — xem DATA_PROVENANCE.md")

    p = os.path.join(ROOT, "data", "tls", "netem_matrix.csv")
    if os.path.exists(p):
        rows = [r for r in csv.reader(open(p)) if r and not r[0].startswith("#")]
        data = rows[1:]
        cfgs = sorted(set(r[0] for r in data))
        losses = ["0", "1", "3", "5", "10"]
        series = []
        for i, cfg in enumerate(cfgs):
            vals = []
            for ls in losses:
                m = [float(r[5]) for r in data if r[0] == cfg and r[1] == "60" and r[2] == ls]
                vals.append(m[0] if m else 0)
            series.append((cfg.split(" ")[0], vals, PALETTE[i]))
        bar_chart("fig8_netem_loss.svg",
                  "TLS handshake (ms) vs mất gói @ delay 60ms (MÔ HÌNH)",
                  [f"{l}%" for l in losses], series, ylabel="ms (median)",
                  note="MÔ HÌNH — PQC nhiều byte hơn -> nhạy mất gói hơn")

    print("DONE plot.py ->", os.path.relpath(FIG, ROOT))


if __name__ == "__main__":
    main()
