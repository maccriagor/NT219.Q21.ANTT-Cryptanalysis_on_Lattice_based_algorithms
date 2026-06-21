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
- **🚧** marks a command that is part of the *target* workflow but is **not yet
  committed to the repository**. It is kept here because it documents the
  intended interface; until the script/target lands, use the alternative noted
  under [Appendix A](#appendix-a--tls-handshake-with-whats-in-the-repo-today).
  See also the README's *Project status*.
- **Do not commit** while working, **except** the final save step, which commits
  only `data/ analysis_out/ docs/`.
- After every build that installs into `~/pqc`, re-running the script and seeing
  `Skipping` / `already at …` is by design — pass `FORCE=1` to rebuild for real.

---

# ═══════════════════ x86_64 ═══════════════════

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
> **step 6** (`source scripts/setenv.sh`).

**3. Make the scripts executable**
```bash
chmod +x scripts/*
```

**4. Make the Track-D scripts executable**
```bash
chmod +x tls13-scratch/*.sh
```

### Build the microbenchmark environment
*(default value of every `[FLAG]` is 0 / off)*

**5. Build OpenSSL (native ML-KEM / ML-DSA)**
```bash
[SKIP_TESTS=1|0] [FORCE=1|0] [JOBS=<n>] bash scripts/build_openssl.sh
```
- `SKIP_TESTS=1` — skip `make test` (faster build)
- `FORCE=1` — reinstall OpenSSL (build over the existing install)
- `JOBS=<n>` — compile threads (default `JOBS = nproc` = your core count)
- e.g. `SKIP_TESTS=1 JOBS=8 bash scripts/build_openssl.sh`
- output: `~/pqc/openssl/` + `docs/openssl.commit`

**6. Activate that OpenSSL — in EVERY new terminal**
```bash
source scripts/setenv.sh
```
- e.g. prints `Activated OpenSSL from: ~/pqc/openssl`

**7. Verify the environment (must PASS)**
```bash
bash scripts/verify_env.sh
```
- output: `docs/env_report_x86_64.txt`

**8. Build the benchmark**
```bash
make
```
- output: `build/bench_evp`

### Build liboqs (ref = portable C, opt = SIMD)

**9. liboqs ref**
```bash
[FORCE=1|0] bash scripts/build_liboqs.sh ref
```
- `FORCE=1` — rebuild liboqs ref
- output: `~/pqc/liboqs-ref/`

**10. liboqs opt (AVX2 on x86)**
```bash
[FORCE=1|0] bash scripts/build_liboqs.sh opt
```
- `FORCE=1` — rebuild liboqs opt
- e.g. `FORCE=1 bash scripts/build_liboqs.sh opt`
- output: `~/pqc/liboqs-neon/`

**11. Build `bench_oqs` (both trees)**
```bash
make bench_oqs
```
- output: `build/bench_oqs_ref` + `build/bench_oqs_opt`

**12. Fetch PQClean (for code-size)**
```bash
bash scripts/fetch_pqclean.sh clean
```
- output: `~/pqc/src/PQClean/` + `docs/pqclean.commit`

### Generate TLS certificates

**13. Generate certs**
```bash
[CERT_SET="<...>"] [FORCE=1|0] bash scripts/gen_tls_certs.sh
```
- `CERT_SET="..."` — certs to generate (default `"rsa2048 ecp256 mldsa65"`)
- `FORCE=1` — regenerate certs that already exist
- 💡 keep `CERT_SET` in sync with the `CERTS` you measure under TLS below
- e.g. `bash scripts/gen_tls_certs.sh`
- output: `~/pqc/tls/*.cert.pem` + `*.key.pem`

**14. (Optional) Pin CPU frequency for stable numbers**
```bash
sudo cpupower frequency-set -g performance
```

---

### ───────────── MEASURE ─────────────

**15. Microbenchmark — EVP (WP2)**
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
  → `==> mlkem 768 (ok) ... Summary: data/summary_micro_x86_64.csv`
- output: `data/summary_micro_x86_64.csv` + `data/raw/x86_64/`

**16. liboqs ref vs opt (WP3)**
```bash
[OQS_ALGOS="..."] [BENCH_ITERS=<n>] [BENCH_KEYGEN_ITERS=<n>] make oqs
```
- `OQS_ALGOS="..."` — restrict the PQC set (default = 6 schemes)
- e.g. `OQS_ALGOS="mlkem 768" make oqs` → `==> ref mlkem 768 (ok) / ==> opt mlkem 768 (ok)`
- output: `data/bench_oqs_x86_64.csv`

**17. 🚧 liboqs `speed_kem` / `speed_sig` cross-check**
```bash
[KEMS="..."] [SIGS="..."] bash scripts/run_liboqs_speed.sh
```
- `KEMS="..."` — KEMs for `speed_kem` (default `"ML-KEM-512 ML-KEM-768 ML-KEM-1024"`)
- `SIGS="..."` — signatures for `speed_sig` (default `"ML-DSA-44 ML-DSA-65 ML-DSA-87"`)
- e.g. `KEMS="ML-KEM-768" SIGS="ML-DSA-65" bash scripts/run_liboqs_speed.sh`
- output: `data/liboqs_speed_x86_64.csv`

**18. Code size (WP5)**
```bash
make codesize
```
- output: `data/codesize_x86_64.csv`

**19. Peak memory (WP5)** — slow: RSA-15360 takes a few minutes
```bash
make memory
```
- output: `data/memory_x86_64.csv`

**20. 🚧 TLS 1.3 handshake (WP4)**
```bash
[TLS_ITERS=<n>] [TLS_CONC=<n>] [TLS_DUR=<s>] [PORT=<n>] [CERTS="..."] [GROUP_LIST="..."] make tls
```
- `TLS_ITERS=<n>` — sequential handshakes measured (default 50)
- `TLS_CONC=<n>` — threads for throughput (default 4)
- `TLS_DUR=<s>` — throughput window, seconds (default 10)
- `PORT=<n>` — server port (default 4433)
- `CERTS="..."` — certificates to use (default `"rsa2048 ecp256 mldsa65"`)
- `GROUP_LIST="..."` — key-exchange groups (default `"X25519 X25519MLKEM768 MLKEM768"` = classical / hybrid / pure-PQC)
- e.g. `TLS_ITERS=20 CERTS="mldsa65" GROUP_LIST="X25519MLKEM768" make tls`
- output: `data/tls_handshake_x86_64.csv`

**21. 🚧 TLS handshake via nginx**
```bash
[WORKERS_LIST="1 auto"] [LISTEN=<addr>] bash scripts/bench_tls_nginx.sh
```
- `WORKERS_LIST="..."` — nginx worker counts to try (default `"1 auto"`)
- `LISTEN=<addr>` — bind address (default `127.0.0.1`; for LAN use `0.0.0.0`)
- e.g. `LISTEN=0.0.0.0 bash scripts/bench_tls_nginx.sh`
- output: `data/tls_handshake_nginx-x86_64.csv`

**22. 🚧 Handshake under emulated RTT / loss**
```bash
[DELAYS="..."] [ITERS=<n>] [LOSSES="..."] [GROUPS="..."] [CERT=<...>] sudo bash scripts/bench_rtt.sh {scratch|nginx}
```
- `{scratch|nginx}` — backend: self-built TLS (Track D) or nginx (**required**, pick one)
- `DELAYS="..."` — delay per side, RTT ≈ 2× (default `"0ms 2.5ms 15ms 39ms"`)
- `ITERS=<n>` — handshakes per RTT level (default 200)
- `LOSSES="..."` — packet-loss percentages (default `"0"`)
- e.g. `DELAYS="0ms 15ms" LOSSES="0 1" sudo bash scripts/bench_rtt.sh scratch`
- output: `rtt_scratch/summary.csv` (or `rtt_nginx/summary.csv`)

**23. 🚧 Handshake under `netem` (needs root)**
```bash
sudo make tlsnetem
```
- output: `data/netem_x86_64.csv`

---

### ───────────── AGGREGATE + DEMO ─────────────

**24. Aggregate tables + charts**
```bash
make analyze
```
- output: `analysis_out/tables.md` + `analysis_out/*.png`

**25. 🚧 One-shot demo (prints to screen, writes nothing)**
```bash
[PORT=<n>] bash scripts/demo.sh
```
- `PORT=<n>` — demo port (default 4434)
- e.g. `bash scripts/demo.sh`
- output: printed to screen (environment + microbench + TLS); **creates no files**

**26. 🚧 Full K-batch run (the report numbers)**
```bash
[BATCHES=<n>] [BENCH_ITERS=<n>] bash run.sh
```
- `BATCHES=<n>` — number of repeat batches K (default 5)
- e.g. `BATCHES=3 bash run.sh`
- output: `data/raw/x86_64/summary_batch*.csv` + `data/summary_micro_x86_64.csv`

---

# ═══════════════════ ARM (Raspberry Pi 4) ═══════════════════

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
> New terminal? Re-run **step 2** and **step 6** (`source scripts/setenv.sh`).

**3. Make the scripts executable**
```bash
chmod +x scripts/*
```

**4. Make the Track-D scripts executable**
```bash
chmod +x tls13-scratch/*.sh
```

### Build the environment

**5. ⚠️ Build OpenSSL — the Pi is slow, so always skip the test suite**
```bash
SKIP_TESTS=1 [FORCE=1|0] [JOBS=<n>] bash scripts/build_openssl.sh
```
- `SKIP_TESTS=1` — the full OpenSSL test suite takes very long on a Pi → keep it on
- `FORCE=1` — reinstall OpenSSL
- `JOBS=<n>` — compile threads (default `JOBS = nproc = 4` on a Pi 4)
- e.g. `SKIP_TESTS=1 bash scripts/build_openssl.sh`
- output: `~/pqc/openssl/` + `docs/openssl.commit`

**6. Activate OpenSSL — in EVERY new terminal**
```bash
source scripts/setenv.sh
```

**7. Verify the environment**
```bash
bash scripts/verify_env.sh
```
- output: `docs/env_report_aarch64.txt`

**8. Build the benchmark**
```bash
make
```
- output: `build/bench_evp`

### Build liboqs — ⚠️ BOTH trees required (NEON is the RQ2 experiment)

**9. liboqs ref**
```bash
[USE_ARM_PMU=1|0] [FORCE=1|0] bash scripts/build_liboqs.sh ref
```
- `USE_ARM_PMU=1` — enable the PMU cycle counter (needs the kernel module)
- `FORCE=1` — rebuild
- output: `~/pqc/liboqs-ref/`

**10. liboqs opt (NEON on ARM)**
```bash
[USE_ARM_PMU=1|0] [FORCE=1|0] bash scripts/build_liboqs.sh opt
```
- e.g. `bash scripts/build_liboqs.sh opt`
- output: `~/pqc/liboqs-neon/`

**11. Build `bench_oqs` (both)**
```bash
make bench_oqs
```
- output: `build/bench_oqs_ref` + `build/bench_oqs_opt`

### PQClean — ⚠️ add the aarch64 build too

**12. Portable C**
```bash
bash scripts/fetch_pqclean.sh clean
```

**13. aarch64-optimized**
```bash
bash scripts/fetch_pqclean.sh aarch64
```
- output: `~/pqc/src/PQClean/`

**14. Generate TLS certificates**
```bash
[CERT_SET="<...>"] [FORCE=1|0] bash scripts/gen_tls_certs.sh
```
- `CERT_SET="..."` — certs to generate (default `"rsa2048 ecp256 mldsa65"`)
- output: `~/pqc/tls/*.pem`

**15. ⚠️ Pin CPU frequency to fight thermal throttling**
```bash
sudo cpupower frequency-set -g performance
```

---

### ───────────── MEASURE (same knobs as x86; outputs end in `_aarch64`) ─────────────

**16. Microbenchmark — EVP (WP2)**
```bash
[MICRO_ALGOS="..."] [BENCH_ITERS=<n>] [BENCH_KEYGEN_ITERS=<n>] [BENCH_WARMUP=<n>] make bench
```
- Same knobs as x86 **step 15** (full 15-algo set by default).
- e.g. `MICRO_ALGOS="mlkem 768;rsa 3072" BENCH_KEYGEN_ITERS=50 make bench`
- output: `data/summary_micro_aarch64.csv`

**17. liboqs ref vs opt (WP3)** — ⚠️ needs both `ref` + `opt`
```bash
[OQS_ALGOS="..."] make oqs
```
- output: `data/bench_oqs_aarch64.csv`

**18. 🚧 liboqs `speed_kem` / `speed_sig` cross-check**
```bash
[KEMS="..."] [SIGS="..."] bash scripts/run_liboqs_speed.sh
```
- output: `data/liboqs_speed_aarch64.csv`

**19. Code size (WP5)**
```bash
make codesize
```
- output: `data/codesize_aarch64.csv`

**20. Peak memory (WP5)** — ⚠️ let the Pi cool between batches (~80 °C)
```bash
make memory
```
- output: `data/memory_aarch64.csv`

**21. 🚧 TLS 1.3 handshake (WP4)**
```bash
[TLS_ITERS=<n>] [CERTS="..."] [GROUP_LIST="..."] make tls
```
- e.g. `TLS_ITERS=20 CERTS="mldsa65" make tls`
- output: `data/tls_handshake_aarch64.csv`

**22. 🚧 TLS handshake via nginx**
```bash
[WORKERS_LIST="1 auto"] bash scripts/bench_tls_nginx.sh
```
- output: `data/tls_handshake_nginx-aarch64.csv`

**23. 🚧 Handshake under emulated RTT / loss**
```bash
[DELAYS="..."] [ITERS=<n>] [LOSSES="..."] sudo bash scripts/bench_rtt.sh {scratch|nginx}
```
- output: `rtt_{scratch|nginx}/summary.csv`

**24. 🚧 Handshake under `netem`**
```bash
sudo make tlsnetem
```
- output: `data/netem_aarch64.csv`

---

### ───────────── AGGREGATE + DEMO + SAVE ─────────────

**25. Aggregate tables + charts**
```bash
make analyze
```
- output: `analysis_out/tables.md` + `*.png`

**26. 🚧 One-shot demo**
```bash
[PORT=<n>] bash scripts/demo.sh
```
- output: printed to screen

**27. 🚧 Full K-batch run** — ⚠️ ARM uses `run_arm.sh` (cools 120 s between batches)
```bash
[BATCHES=<n>] bash run_arm.sh
```
- `BATCHES=<n>` — number of batches K (default 5)
- output: `data/raw/aarch64/summary_batch*.csv` + `data/summary_micro_aarch64.csv`

**28. ⚠️ Save results BEFORE returning the Pi**
```bash
git add data analysis_out docs && git commit -m "aarch64" && git push
```
> This is the **only** commit in the workflow, and it saves only
> `data/ analysis_out/ docs/`. Never commit `~/pqc`, keys, or build trees
> (`.gitignore` already blocks keys and source trees).

---

## Appendix A — TLS handshake with what's in the repo *today*

The 🚧 TLS steps above describe the target interface. Until those scripts/targets
land, you can run the TLS handshake measurement with the harnesses already in the
tree:

**Track D — self-built TLS 1.3 client & server** (from `docs/DEMO.md`):
```bash
make -C tls13-scratch                    # build client + server
make -C tls13-scratch/server cert        # self-signed cert (or copy an ML-DSA cert in to test PQC auth)
( cd tls13-scratch/server && ./server ) &   # server on port 8400
# 100 handshakes each: classical / hybrid / pure-PQC
./tls13-scratch/client/client 127.0.0.1 8400 100 X25519
./tls13-scratch/client/client 127.0.0.1 8400 100 X25519MLKEM768
./tls13-scratch/client/client 127.0.0.1 8400 100 MLKEM768
# prints one line "t1,t2,...,t100" (ms); summarize with scripts/td_stats.py
```

On the result branches, the nginx macrobenchmark is driven by
`nginx-bench/run.sh` (after `bash scripts/build_nginx.sh`), producing
`data/nginx_handshake_<arch>.csv`. A reworked OQS-provider harness is in
`handshake/` but is not yet portable (machine-specific paths).

## Appendix B — Common gotchas

- **`setenv.sh` must be `source`d**, never `./`-executed — otherwise the
  `PATH` / `LD_LIBRARY_PATH` exports vanish with the subshell.
- **Re-running a build shows `Skipping`?** That is intended. Force a real
  rebuild with `FORCE=1` (works for openssl / liboqs / nginx / certs).
- **Changing a version:** edit the tag in `scripts/versions.env`, delete the
  source under `~/pqc/src/`, then rebuild with `FORCE=1`.
- **Windows line endings (`^M`)** on a `.sh`: `sed -i 's/\r$//' scripts/*.sh`.
- **Per-machine binaries:** never copy `~/pqc` between machines; rebuild on each.
