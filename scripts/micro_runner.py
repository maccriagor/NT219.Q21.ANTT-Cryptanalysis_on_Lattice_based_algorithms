#!/usr/bin/env python3
"""micro_runner.py — Engine chạy benchmark binary, gom dòng "DATA:" -> CSV trong repo.

Hỗ trợ ĐA MỨC NIST: với --levels "512:1,768:3,1024:5" thì lặp từng mức, chạy
`bin <param>` N lần/mức, thêm cột Param + Category. Không có --levels thì chạy 1 lần
không tham số.

Tự nhận kiến trúc: x86_64->x86, aarch64->arm (cùng runner chạy trên PC lẫn Pi).
Ghim core 0 bằng taskset nếu có (không cần sudo). CSV -> data/micro/<arch>/<name>_<arch>.csv.
"""
import argparse, csv, os, platform, shutil, subprocess, sys


def detect_arch():
    m = platform.machine().lower()
    if m in ("aarch64", "arm64"): return "arm"
    if m in ("x86_64", "amd64"):  return "x86"
    return m


def read_iters(path, default=100):
    if path and os.path.exists(path):
        try: return int(open(path).readline().strip())
        except Exception: pass
    return default


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--name", required=True)
    ap.add_argument("--bin", required=True)
    ap.add_argument("--cols", required=True, help="tên cột đo (sau Iter[,Param,Category])")
    ap.add_argument("--levels", default="", help='vd "512:1,768:3,1024:5" (param:category); rỗng = 1 mức')
    ap.add_argument("--iters", type=int, default=None)
    ap.add_argument("--input", default="input.txt")
    ap.add_argument("--outdir", default=None)
    args = ap.parse_args()

    n = args.iters if args.iters is not None else read_iters(args.input)
    if not os.path.exists(args.bin):
        print(f"❌ Không thấy binary {args.bin}. Chạy ./build.sh trước.", file=sys.stderr); sys.exit(1)

    arch = detect_arch()
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    outdir = args.outdir or os.path.join(repo, "data", "micro", arch)
    os.makedirs(outdir, exist_ok=True)
    out = os.path.join(outdir, f"{args.name}_{arch}.csv")

    levels = []
    for tok in args.levels.split(","):
        tok = tok.strip()
        if not tok: continue
        param, cat = (tok.split(":") + [""])[:2]
        levels.append((param, cat))
    multilevel = len(levels) > 0
    if not multilevel:
        levels = [(None, None)]

    has_taskset = shutil.which("taskset") is not None
    cols = args.cols.split(",")
    header = (["Iter", "Param", "Category"] if multilevel else ["Iter"]) + cols

    try:
        size = os.path.getsize(args.bin)
        sha = subprocess.check_output(["sha256sum", args.bin]).decode().split()[0]
        print(f"📊 {args.name}: binary {size} B · sha256 {sha[:16]}…  (arch={arch}, N={n}/mức, "
              f"{'mức=' + ','.join(p for p, _ in levels) if multilevel else '1 mức'})")
    except Exception:
        pass

    tot_ok = tot_bad = 0
    with open(out, "w", newline="") as f:
        w = csv.writer(f); w.writerow(header)
        for param, cat in levels:
            ok = bad = 0
            for i in range(n):
                base = ([args.bin] + ([param] if param else []))
                cmd = (["taskset", "-c", "0"] + base) if has_taskset else base
                p = subprocess.run(cmd, capture_output=True, text=True)
                line = next((l for l in (p.stdout + "\n" + p.stderr).splitlines()
                             if l.startswith("DATA:")), None)
                if line:
                    vals = line.replace("DATA:", "").replace(" ", "").strip().split(",")
                    if len(vals) == len(cols):
                        w.writerow(([i + 1, param, cat] if multilevel else [i + 1]) + vals)
                        ok += 1; continue
                bad += 1
                if bad <= 2 and p.stderr.strip():
                    print(f"⚠️ {param or ''} iter {i+1}: {p.stderr.strip().splitlines()[0]}", file=sys.stderr)
            tot_ok += ok; tot_bad += bad
            tag = f"[{param}/Cat{cat}] " if multilevel else ""
            print(f"  {tag}{ok}/{n} mẫu" + (f" (⚠️{bad} fail)" if bad else ""))
    print(f"✅ {args.name}: {tot_ok} mẫu → {os.path.relpath(out, repo)}"
          + (f"  (⚠️{tot_bad} fail)" if tot_bad else ""))
    if tot_ok == 0: sys.exit(2)


if __name__ == "__main__":
    main()
