# RUNBOOK — Building & Running the PQC Benchmarks

A step-by-step procedure for reproducing every measurement on a fresh machine,
for **x86_64** and for **ARM (Raspberry Pi 4)**. Written for someone who has
just cloned the repo and wants results, in order, with no guessing.

> The committed results live on the per-architecture branches:
> [`x86_64`](https://github.com/maccriagor/NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms/tree/x86_64)
> and [`arm`](https://github.com/maccriagor/NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms/tree/arm).

## How to read this runbook

- **Environment variables in `[BRACKETS]` are optional.** Put them before the
  command. Both `VAR=x make target` and `make target VAR=x` work, and you can
  set several at once: `BENCH_ITERS=5000 BENCH_WARMUP=50 make bench`.
- **`output:`** lists what a step produces.
- **`e.g.`** is a ready-to-run example.
- **⚠️** marks a platform caveat you should not skip.
- After a build that installs into `~/pqc`, re-running it and seeing `Skipping` /
  `already at …` is by design — pass `FORCE=1` to rebuild for real.
- **Do not commit** while working, except the final ARM save step, which commits
  only `data/ analysis_out/ docs/`.

---

## x86_64

### Setup

**1. Clone**

```bash
git clone https://github.com/maccriagor/NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms.git
```

**2. Enter the repo**

```bash
cd NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms
```

> If you open a new terminal (or your session drops), re-run **step 2** and
> **step 5** (`source scripts/setenv.sh`).

**3. Make the scripts executable**

```bash
chmod +x scripts/*
```

### Build the environment
*(default value of every `[FLAG]` is 0 / off)*

**4. Build OpenSSL (native ML-KEM / ML-DSA)**

```bash
[SKIP_TESTS=1|0] [FORCE=1|0] [JOBS=<n>] bash scripts/build_openssl.sh
```

- `SKIP_TESTS=1` — skip `make test` (faster build)
- `FORCE=1` — reinstall OpenSSL (build over the existing install)
- `JOBS=<n>` — compile threads (default `JOBS = nproc` = your core count)
- e.g. `SKIP_TESTS=1 JOBS=8 bash scripts/build_openssl.sh`
- output: `~/pqc/openssl/` + `docs/openssl.commit`

**5. Activate that OpenSSL — in EVERY new terminal**

```bash
source scripts/setenv.sh
```

- e.g. prints `Activated OpenSSL from: ~/pqc/openssl`

**6. Verify the environment (must PASS)**

```bash
bash scripts/verify_env.sh
```

- output: `docs/env_report_x86_64.txt`

**7. Build the benchmark**

```bash
make
```

- output: `build/bench_evp`

**8. Build liboqs — ref (portable C)**

```bash
[FORCE=1|0] bash scripts/build_liboqs.sh ref
```

- `FORCE=1` — rebuild liboqs ref
- output: `~/pqc/liboqs-ref/`

**9. Build liboqs — opt (AVX2 on x86)**

```bash
[FORCE=1|0] bash scripts/build_liboqs.sh opt
```

- e.g. `FORCE=1 bash scripts/build_liboqs.sh opt`
- output: `~/pqc/liboqs-neon/`

**10. Build `bench_oqs` (both trees)**

```bash
make bench_oqs
```

- output: `build/bench_oqs_ref` + `build/bench_oqs_opt`

**11. Fetch PQClean (for code-size)**

```bash
bash scripts/fetch_pqclean.sh clean
```

- output: `~/pqc/src/PQClean/` + `docs/pqclean.commit`

**12. Generate TLS certificates**

```bash
[CERT_SET="<...>"] [FORCE=1|0] bash scripts/gen_tls_certs.sh
```

- `CERT_SET="..."` — certs to generate (default `"rsa2048 ecp256 mldsa65"`)
- `FORCE=1` — regenerate certs that already exist
- output: `~/pqc/tls/*.cert.pem` + `*.key.pem`

**13. (Optional) Pin CPU frequency for stable numbers**

```bash
sudo cpupower frequency-set -g performance
```

### Measure

**14. Microbenchmark — EVP**

```bash
[MICRO_ALGOS="fam param;..."] [BENCH_ITERS=<n>] [BENCH_KEYGEN_ITERS=<n>] [BENCH_WARMUP=<n>] make bench
```

- `MICRO_ALGOS="..."` — restrict the algorithm set. Default is the full 15:
  `rsa 3072/7680/15360`, `ecdsa p256/p384/p521`, `ecdh p256/p384/p521`,
  `mlkem 512/768/1024`, `mldsa 44/65/87`
- `BENCH_ITERS=<n>` — iterations for fast ops (encap/decap/sign/verify/derive) (default 2000)
- `BENCH_KEYGEN_ITERS=<n>` — iterations for keygen (default 200; RSA keygen is slow, so it is separate)
- `BENCH_WARMUP=<n>` — warm-up iterations discarded before timing (default 20)
- e.g. `MICRO_ALGOS="mlkem 768;rsa 3072" BENCH_KEYGEN_ITERS=50 make bench`
- output: `data/summary_micro_x86_64.csv` + `data/raw/x86_64/`

**15. liboqs ref vs opt**

```bash
[OQS_ALGOS="..."] [BENCH_ITERS=<n>] [BENCH_KEYGEN_ITERS=<n>] make oqs
```

- `OQS_ALGOS="..."` — restrict the PQC set (default = 6 schemes)
- e.g. `OQS_ALGOS="mlkem 768" make oqs` → `==> ref mlkem 768 (ok) / ==> opt mlkem 768 (ok)`
- output: `data/bench_oqs_x86_64.csv`

**16. Code size**

```bash
make codesize
```

- output: `data/codesize_x86_64.csv`

**17. Peak memory** — slow: RSA-15360 takes a few minutes

```bash
make memory
```

- output: `data/memory_x86_64.csv`

**18. TLS 1.3 handshake — Track D (self-built client + server)**

```bash
make -C tls13-scratch                       # build client + server
make -C tls13-scratch/server cert           # self-signed cert (or drop in an ML-DSA cert for PQC auth)
( cd tls13-scratch/server && ./server ) &   # server on port 8400
# 100 handshakes per group -> one CSV line each, classical / hybrid / pure-PQC:
./tls13-scratch/client/client 127.0.0.1 8400 100 X25519         > data/td_x25519_$(uname -m).csv
./tls13-scratch/client/client 127.0.0.1 8400 100 X25519MLKEM768 > data/td_hybrid_$(uname -m).csv
./tls13-scratch/client/client 127.0.0.1 8400 100 MLKEM768       > data/td_mlkem768_$(uname -m).csv
python3 scripts/td_stats.py                 # median / p95 / CI + overhead vs x25519
```

- output: `data/td_*_x86_64.csv` + the printed statistics table

### Aggregate

**19. Aggregate tables + charts**

```bash
make analyze
```

- output: `analysis_out/tables.md` + `analysis_out/*.png`

---

## ARM (Raspberry Pi 4)

Same pipeline; outputs end in `_aarch64`. The Pi is slow and thermally limited,
so a few steps are **mandatory** (⚠️) rather than optional.

### Setup

**1. Clone**

```bash
git clone https://github.com/maccriagor/NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms.git
```

**2. Enter the repo**

```bash
cd NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms
```

> New terminal? Re-run **step 2** and **step 5** (`source scripts/setenv.sh`).

**3. Make the scripts executable**

```bash
chmod +x scripts/*
```

### Build the environment

**4. ⚠️ Build OpenSSL — the Pi is slow, so always skip the test suite**

```bash
SKIP_TESTS=1 [FORCE=1|0] [JOBS=<n>] bash scripts/build_openssl.sh
```

- `SKIP_TESTS=1` — the full OpenSSL test suite takes very long on a Pi → keep it on
- `JOBS=<n>` — compile threads (default `JOBS = nproc = 4` on a Pi 4)
- e.g. `SKIP_TESTS=1 bash scripts/build_openssl.sh`
- output: `~/pqc/openssl/` + `docs/openssl.commit`

**5. Activate OpenSSL — in EVERY new terminal**

```bash
source scripts/setenv.sh
```

**6. Verify the environment**

```bash
bash scripts/verify_env.sh
```

- output: `docs/env_report_aarch64.txt`

**7. Build the benchmark**

```bash
make
```

- output: `build/bench_evp`

**8. ⚠️ Build liboqs — ref (BOTH trees required; NEON is the RQ2 experiment)**

```bash
[USE_ARM_PMU=1|0] [FORCE=1|0] bash scripts/build_liboqs.sh ref
```

- `USE_ARM_PMU=1` — enable the PMU cycle counter (needs the kernel module)
- output: `~/pqc/liboqs-ref/`

**9. Build liboqs — opt (NEON on ARM)**

```bash
[USE_ARM_PMU=1|0] [FORCE=1|0] bash scripts/build_liboqs.sh opt
```

- output: `~/pqc/liboqs-neon/`

**10. Build `bench_oqs` (both)**

```bash
make bench_oqs
```

- output: `build/bench_oqs_ref` + `build/bench_oqs_opt`

**11. Fetch PQClean — portable C**

```bash
bash scripts/fetch_pqclean.sh clean
```

**12. Fetch PQClean — aarch64-optimized**

```bash
bash scripts/fetch_pqclean.sh aarch64
```

- output: `~/pqc/src/PQClean/`

**13. Generate TLS certificates**

```bash
[CERT_SET="<...>"] [FORCE=1|0] bash scripts/gen_tls_certs.sh
```

- output: `~/pqc/tls/*.pem`

**14. ⚠️ Pin CPU frequency to fight thermal throttling**

```bash
sudo cpupower frequency-set -g performance
```

### Measure
*(same knobs as x86; outputs end in `_aarch64`)*

**15. Microbenchmark — EVP**

```bash
[MICRO_ALGOS="..."] [BENCH_ITERS=<n>] [BENCH_KEYGEN_ITERS=<n>] [BENCH_WARMUP=<n>] make bench
```

- Same knobs as x86 **step 14** (full 15-algo set by default).
- e.g. `MICRO_ALGOS="mlkem 768;rsa 3072" BENCH_KEYGEN_ITERS=50 make bench`
- output: `data/summary_micro_aarch64.csv`

**16. liboqs ref vs opt** — ⚠️ needs both `ref` + `opt`

```bash
[OQS_ALGOS="..."] make oqs
```

- output: `data/bench_oqs_aarch64.csv`

**17. Code size**

```bash
make codesize
```

- output: `data/codesize_aarch64.csv`

**18. Peak memory** — ⚠️ let the Pi cool between batches (~80 °C)

```bash
make memory
```

- output: `data/memory_aarch64.csv`

**19. TLS 1.3 handshake — Track D**

```bash
make -C tls13-scratch && make -C tls13-scratch/server cert
( cd tls13-scratch/server && ./server ) &
./tls13-scratch/client/client 127.0.0.1 8400 100 X25519         > data/td_x25519_$(uname -m).csv
./tls13-scratch/client/client 127.0.0.1 8400 100 X25519MLKEM768 > data/td_hybrid_$(uname -m).csv
./tls13-scratch/client/client 127.0.0.1 8400 100 MLKEM768       > data/td_mlkem768_$(uname -m).csv
python3 scripts/td_stats.py
```

- output: `data/td_*_aarch64.csv` + the printed statistics table

### Aggregate & save

**20. Aggregate tables + charts**

```bash
make analyze
```

- output: `analysis_out/tables.md` + `*.png`

**21. ⚠️ Save results BEFORE returning the Pi**

```bash
git add data analysis_out docs && git commit -m "aarch64" && git push
```

> This is the **only** commit in the workflow, and it saves only
> `data/ analysis_out/ docs/`. Never commit `~/pqc`, keys, or build trees
> (`.gitignore` already blocks keys and source trees).

---

## TLS handshake suite — OQS-provider harness (`handshake/`)

A second, self-contained TLS 1.3 handshake benchmark, independent of the main
`~/pqc` toolchain and of Track D. It builds its **own** OpenSSL 3.3 + oqs-provider
inside `handshake/openssl-build/`, then measures handshake latency
(min / mean / p50 / p95 / p99 / max) and throughput across **13 cipher suites**
spanning NIST levels 1, 3, and 5 (classical, pure-PQC, and hybrid certs × KEMs).
It is architecture-independent (runs on x86_64 or ARM).

> ⚠️ **Work in progress — not yet portable.** Before running, edit the
> hard-coded paths in `handshake/*.sh`:
> - `LIBOQS_DIR=...` (all four scripts) → point at your built liboqs, e.g.
>   `~/pqc/liboqs-neon`.
> - `CERT_DIR=...` in `server_runner.sh` → point at `handshake/certs` (where
>   `gen_certs.sh` writes them); otherwise the server cannot find its cert.

**1. One-time setup — build isolated OpenSSL 3.3 + oqs-provider, then generate certs**

```bash
cd handshake
bash setup_oqs_openssl.sh     # OpenSSL 3.3 + oqs-provider -> handshake/openssl-build/
bash gen_certs.sh             # 13 cert profiles -> handshake/certs/ (+ certs/size_analysis.csv)
```

- output: `handshake/openssl-build/`, `handshake/certs/*.crt` + `*.key`, `handshake/certs/size_analysis.csv`
- `make` runs both scripts plus an `openssl.cnf` provider-injection step, but its
  `config_openssl` recipe has a syntax bug — prefer running the two scripts directly.

**2. Start the server for one suite (terminal 1)**

```bash
bash server_runner.sh <suite#>     # listens on port 44333; serves NUM_HANDSHAKES (500) then exits
```

**3. Benchmark from the client (terminal 2, SAME suite#)**

```bash
bash client_bench.sh <suite#>      # 5 warm-up + 500 timed handshakes + a 3 s throughput flood
```

- output: appends one row to `handshake/results/handshake_benchmarks2.csv`
  (`Suite,Min,Mean,P50,P95,P99,Max,Throughput_Conn_Sec` — times in ms)

Both scripts take the suite number as an argument (or prompt interactively if
omitted). The server and client **must use the same number**:

| # | Suite | NIST level |
|---|---|---|
| 1 | RSA2048_X25519 | 1 |
| 2 | ECDSA_P256 | 1 |
| 3 | MLDSA44_MLKEM512 | 1 |
| 4 | Hybrid_P256_MLKEM512 | 1 |
| 5 | RSA3072_X25519 | 3 |
| 6 | ECDSA_P384 | 3 |
| 7 | MLDSA65_MLKEM768 | 3 |
| 8 | Hybrid_X25519_MLKEM768 | 3 |
| 9 | Hybrid_P384_MLKEM768 | 3 |
| 10 | RSA7680_X25519 | 5 |
| 11 | ECDSA_P521 | 5 |
| 12 | MLDSA87_MLKEM1024 | 5 |
| 13 | Hybrid_P521_MLKEM1024 | 5 |

---

## Common gotchas

- **`setenv.sh` must be `source`d**, never `./`-executed — otherwise the
  `PATH` / `LD_LIBRARY_PATH` exports vanish with the subshell.
- **Re-running a build shows `Skipping`?** That is intended. Force a real
  rebuild with `FORCE=1` (works for openssl / liboqs / certs).
- **Changing a version:** edit the tag in `scripts/versions.env`, delete the
  source under `~/pqc/src/`, then rebuild with `FORCE=1`.
- **Windows line endings (`^M`)** on a `.sh`: `sed -i 's/\r$//' scripts/*.sh`.
- **Per-machine binaries:** never copy `~/pqc` between machines; rebuild on each.
