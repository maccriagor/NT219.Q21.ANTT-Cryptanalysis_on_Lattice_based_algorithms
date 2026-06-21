# Cryptanalysis & Benchmarking of Lattice-based Algorithms

> **NT219 (Cryptography) capstone.** A reproducible benchmark of the NIST
> post-quantum standards **ML-KEM** and **ML-DSA** (Kyber / Dilithium —
> FIPS&nbsp;203/204) against classical **RSA** and **ECC**, measured at three
> levels — raw primitive, memory & code size, and TLS&nbsp;1.3 handshake — on
> both **x86_64** and **ARM (Raspberry&nbsp;Pi&nbsp;4)**.

The goal is an apples-to-apples answer to: *what does migrating to post-quantum
cryptography actually cost?* — in CPU time, memory, binary size, and TLS
connection latency, on a server-class x86 CPU and a constrained ARM device.

- **New here? Start with the [RUNBOOK](RUNBOOK.md)** — a step-by-step, copy-paste
  procedure for each platform.
- **Results** live on the per-architecture branches:
  [`x86_64`](https://github.com/maccriagor/NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms/tree/x86_64)
  and
  [`arm`](https://github.com/maccriagor/NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms/tree/arm)
  (raw CSVs under `data/`, processed tables and charts under `analysis_out/`).

---

## Contents

- [What is measured](#what-is-measured)
- [Algorithm matrix](#algorithm-matrix)
- [Quickstart](#quickstart)
- [Repository layout](#repository-layout)
- [Methodology](#methodology)
- [Toolchain & versions](#toolchain--versions)
- [Requirements](#requirements)
- [Reproducibility](#reproducibility)
- [Project status](#project-status)
- [License](#license)

---

## What is measured

| Track | What it answers | How |
|---|---|---|
| **Primitive microbenchmark** (WP2) | Cost of one keygen / encap / decap / sign / verify / derive | `bench_evp` (OpenSSL 3.x EVP API), wall-clock **ns** + CPU **cycles**, median / p95 / p99 over K independent batches |
| **liboqs ref vs opt** (WP3) | How much SIMD (AVX2 / NEON) buys over portable C | The same `bench_oqs` built against two liboqs trees (portable-C `ref` vs native `opt`) → per-op speedup |
| **TLS 1.3 handshake** (WP4) | End-to-end connection cost of PQC vs classical | Handshake latency + throughput across a **certificate (signature) × group (KEM)** matrix; classical, hybrid, and pure-PQC |
| **Memory & code size** (WP5) | RAM and binary footprint per algorithm | Peak RSS via `/usr/bin/time -v`; static-library text/data/bss via `size`; per-scheme sizes from PQClean |
| **Self-built TLS** (Track D) | A from-scratch TLS 1.3 client/server on libssl | `tls13-scratch/` — handshake latency timed client-side, forcing each group |

All tracks emit raw CSVs into `data/` (and `data/raw/<arch>/`), which
`scripts/analyze.py` aggregates into `analysis_out/tables.md` plus PNG charts.

## Algorithm matrix

Grouped by NIST security category so comparisons are like-for-like.

| Category | Signatures | Key exchange / KEM |
|---|---|---|
| **Level 1** (~AES-128) | RSA-2048/3072, ECDSA P-256, ML-DSA-44 | X25519, ECDH P-256, ML-KEM-512, P-256+ML-KEM-512 |
| **Level 3** (~AES-192) | RSA-7680, ECDSA P-384, ML-DSA-65 | ECDH P-384, ML-KEM-768, **X25519+ML-KEM-768** (hybrid), P-384+ML-KEM-768 |
| **Level 5** (~AES-256) | RSA-15360, ECDSA P-521, ML-DSA-87 | ECDH P-521, ML-KEM-1024, P-521+ML-KEM-1024 |

The microbenchmark sweeps 15 primitives (RSA 3072/7680/15360, ECDSA & ECDH
P-256/384/521, ML-KEM 512/768/1024, ML-DSA 44/65/87). The TLS tracks add the
hybrid groups, which are the realistic migration target.

## Quickstart

Full, platform-specific steps are in the **[RUNBOOK](RUNBOOK.md)**. The short
version for x86_64:

```bash
git clone https://github.com/maccriagor/NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms.git
cd NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms
chmod +x scripts/*

bash scripts/install_prereqs.sh          # toolchain + measurement tools (sudo, once)
bash scripts/build_openssl.sh            # OpenSSL 3.6.2 with native ML-KEM / ML-DSA -> ~/pqc/openssl
source scripts/setenv.sh                 # activate that OpenSSL — RE-RUN in every new terminal
bash scripts/verify_env.sh               # must PASS

make                                     # build bench_evp
bash scripts/build_liboqs.sh ref         # portable-C liboqs (add `opt` for the SIMD comparison)
bash scripts/fetch_pqclean.sh clean

make bench && make memory && make codesize
make oqs                                  # liboqs ref vs opt
make analyze                              # tables + charts -> analysis_out/
```

> `setenv.sh` must be **sourced**, not executed — it exports `PATH` /
> `LD_LIBRARY_PATH` into your shell. Re-source it in every new terminal.

## Repository layout

| Path | Role |
|---|---|
| `scripts/` | Build, cross-compile, environment, and measurement drivers (`build_*.sh`, `run_*.sh`, `measure_*.sh`, `analyze.py`, `versions.env`) |
| `src/` + `Makefile` | Microbenchmark harnesses: `bench_evp.cpp` (EVP) and `bench_oqs.cpp` (liboqs-direct) |
| `tls13-scratch/` | Track D — self-built TLS 1.3 client & server on libssl |
| `handshake/` | Work-in-progress OQS-provider TLS handshake harness (see [Project status](#project-status)) |
| `data/`, `data/raw/<arch>/` | Raw benchmark CSVs (deliverable evidence) |
| `analysis_out/` | Aggregated tables (`tables.md`) and charts (`*.png`) from `analyze.py` |
| `docker/`, `cmake/` | Reproducible x86_64 image + aarch64 cross-toolchain |
| `docs/` | `DEMO.md` (demo script), `report/main.tex` (LaTeX report), environment reports, commit pins |

## Methodology

- **Warm-up discarded.** Each microbenchmark drops the first `BENCH_WARMUP`
  iterations before timing, to exclude cache/lazy-init effects.
- **K independent batches.** `run.sh` (x86) / `run_arm.sh` (ARM) repeat the full
  sweep K times and report the **median-of-medians**, with batch-to-batch spread
  (cv%) to expose unstable measurements (e.g. RSA keygen).
- **Bootstrap confidence intervals.** Raw per-iteration samples (`BENCH_CSV`)
  feed a percentile bootstrap for 95% CIs on medians and on PQC-vs-classical
  overhead ratios.
- **ref vs opt is paired.** The same source built two ways on the same machine
  isolates the SIMD contribution; cross-machine results are bridged by the
  opt/ref ratio rather than compared in absolute terms.
- **Pinned everything.** Toolchain versions are fixed in `scripts/versions.env`
  and recorded as commit hashes in `docs/*.commit`.

Every measurement parameter is overridable at run time (see the RUNBOOK);
defaults in code are only fallbacks.

## Toolchain & versions

Pinned in [`scripts/versions.env`](scripts/versions.env); everything installs
under `~/pqc` (no system OpenSSL is touched, no `sudo` except the one-time
prerequisite install).

| Component | Version |
|---|---|
| OpenSSL | `openssl-3.6.2` (native ML-KEM / ML-DSA) |
| liboqs | `0.14.0` (built twice: portable-C `ref` and SIMD `opt`) |
| oqs-provider | `0.10.0` |
| nginx | `release-1.30.2` (linked against our OpenSSL, for the TLS macrobenchmark) |
| PQClean | pinned by commit (no upstream release tags) |

## Requirements

- Ubuntu 22.04 / 24.04 (x86_64 or ARM64; for a rented ARM box choose an Ubuntu image).
- `sudo` for the prerequisite install only — everything else lives in `~/pqc`.
- A few GB of disk and time: a full OpenSSL build with tests, and two liboqs
  trees, are not instant (and are markedly slower on a Pi).

## Reproducibility

- `docs/openssl.commit`, `docs/liboqs.commit`, `docs/pqclean.commit` — exact
  upstream commits used.
- `docs/env_report_<arch>.txt` — compiler, flags, and CPU for each machine,
  produced by `scripts/verify_env.sh`.
- `docker/Dockerfile.x86_64` and `cmake/aarch64-toolchain.cmake` — containerized
  build and ARM cross-compilation evidence (cross builds prove portability; all
  *measurements* come from native hardware).

## Project status

The committed measurement pipeline (microbench, liboqs ref/opt, memory, code
size, analysis) is stable on `main` and on the result branches. The **TLS
handshake harness is being reworked** — the `handshake/` directory is a
work-in-progress OQS-provider client/server experiment with machine-specific
paths and is **not yet portable**. Some commands in the RUNBOOK (e.g.
`make tls`, `make tlsnetem`, `scripts/run_liboqs_speed.sh`,
`scripts/bench_tls_nginx.sh`, `scripts/bench_rtt.sh`, `scripts/demo.sh`)
describe the **target** workflow and are not all present in the repository yet;
the RUNBOOK marks them accordingly.

## License

Apache License 2.0 — see [LICENSE](LICENSE).
