// ============================================================================
// bench_oqs.cpp  —  Microbenchmark ML-KEM / ML-DSA via the liboqs API (NT219)
//
//  Usage:
//      ./bench_oqs <family> <param>     family: mlkem | mldsa
//      e.g. ./bench_oqs mlkem 768       ./bench_oqs mldsa 65
//      (a full liboqs name also works:  ./bench_oqs ML-KEM-1024)
//  Enviroment variables:
//      BENCH_ITERS        (default 2000) iterations for fast ops
//      BENCH_KEYGEN_ITERS (default 200)  keygen is slower -> fewer iterations
//      BENCH_WARMUP       (default 20)   warm-up iterations discarded
//      BENCH_CSV=path                     raw per-iteration CSV (algo,op,iter,...)
// ============================================================================

#include <oqs/oqs.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#if defined(__x86_64__) || defined(__i386__)
#  include <x86intrin.h>          // __rdtscp
#endif

// ---------------------------------------------------------------------------
// FIPS reference constants (for the runtime conformance self-check).
//   ML-KEM: FIPS 203 (ek, dk, ct, ss) + NIST category + IND-CCA.
//   ML-DSA: FIPS 204 Table 2 (pk, sk, sig) + NIST category.
// ---------------------------------------------------------------------------
struct KemRef { const char *name; size_t pk, sk, ct, ss; int level; bool ind_cca; };
static const KemRef KEM_REF[] = {
    {"ML-KEM-512",   800, 1632,  768, 32, 1, true},
    {"ML-KEM-768",  1184, 2400, 1088, 32, 3, true},
    {"ML-KEM-1024", 1568, 3168, 1568, 32, 5, true},
};
struct SigRef { const char *name; size_t pk, sk, sig; int level; };
static const SigRef SIG_REF[] = {
    {"ML-DSA-44", 1312, 2560, 2420, 2},
    {"ML-DSA-65", 1952, 4032, 3309, 3},
    {"ML-DSA-87", 2592, 4896, 4627, 5},
};

// ---------------------------------------------------------------------------
// Clock: wall-clock (ns) and cycle counter. (Same as bench_evp.cpp.)
// ---------------------------------------------------------------------------
static inline uint64_t now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

// x86: TSC (rdtscp). aarch64: CNTVCT_EL0 — virtual timer, NOT true cycles.
static inline uint64_t read_cycles() {
#if defined(__x86_64__) || defined(__i386__)
    unsigned int aux;
    uint64_t t = __rdtscp(&aux);
    (void)aux;
    return t;
#elif defined(__aarch64__)
    uint64_t v;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v));
    return v;
#else
    return 0;
#endif
}

// Name of the value read_cycles() returns, for HONEST labels:
//   x86 rdtscp     = TSC (constant-rate, NOT retired core cycles)
//   aarch64 cntvct = virtual timer (~fixed freq, NOT cycles)
#if defined(__x86_64__) || defined(__i386__)
static const char *CTR_NAME = "tsc";
#elif defined(__aarch64__)
static const char *CTR_NAME = "vtimer";
#else
static const char *CTR_NAME = "ctr";
#endif

[[noreturn]] static void fatal(const char *msg) {
    fprintf(stderr, "ERROR: %s\n", msg);
    exit(1);
}

// ---------------------------------------------------------------------------
// Statistics from a sample vector. (Same as bench_evp.cpp.)
// ---------------------------------------------------------------------------
struct Stats {
    size_t n = 0;
    double median = 0, mean = 0, sd = 0, p95 = 0, p99 = 0, ci_lo = 0, ci_hi = 0;
};

static double percentile(const std::vector<double> &sorted, double p) {
    if (sorted.empty()) return 0.0;
    double idx = (p / 100.0) * (double)(sorted.size() - 1);
    size_t i = (size_t)std::lround(idx);
    if (i >= sorted.size()) i = sorted.size() - 1;
    return sorted[i];
}

static Stats compute_stats(std::vector<double> v) {
    Stats s;
    s.n = v.size();
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    s.median = percentile(v, 50.0);
    s.p95    = percentile(v, 95.0);
    s.p99    = percentile(v, 99.0);
    double sum = 0.0;
    for (double x : v) sum += x;
    s.mean = sum / (double)v.size();
    if (v.size() > 1) {
        double acc = 0.0;
        for (double x : v) acc += (x - s.mean) * (x - s.mean);
        s.sd = std::sqrt(acc / (double)(v.size() - 1));   // sample std (ddof=1)
        double half = 1.96 * s.sd / std::sqrt((double)v.size());
        s.ci_lo = s.mean - half;
        s.ci_hi = s.mean + half;
    } else {
        s.ci_lo = s.ci_hi = s.mean;
    }
    return s;
}

static size_t env_size(const char *name, size_t dflt) {
    const char *s = getenv(name);
    if (!s || !*s) return dflt;
    long long v = atoll(s);
    return (v > 0) ? (size_t)v : dflt;
}

static FILE *g_csv = nullptr;
static std::string g_algo;

// ---------------------------------------------------------------------------
// Runner: measure one op over N iterations + warm-up.
//   Same shape as bench_evp.cpp's run_op: fn() performs EXACTLY ONE operation;
//   the timed for-loop brackets each call with the wall clock and cycle counter.
// ---------------------------------------------------------------------------
static void run_op(const char *op, size_t iters, size_t warmup,
                   const std::function<void()> &fn) {
    for (size_t i = 0; i < warmup; ++i) fn();          // warm-up (discarded)

    std::vector<double> wall(iters), cyc(iters);
    for (size_t i = 0; i < iters; ++i) {
        uint64_t t0 = now_ns();
        uint64_t c0 = read_cycles();
        fn();                                          // one operation
        uint64_t c1 = read_cycles();
        uint64_t t1 = now_ns();
        wall[i] = (double)(t1 - t0);
        cyc[i]  = (double)(c1 - c0);
        if (g_csv) fprintf(g_csv, "%s,%s,%zu,%.0f,%.0f\n",
                           g_algo.c_str(), op, i, wall[i], cyc[i]);
    }

    Stats w = compute_stats(wall);
    Stats c = compute_stats(cyc);

    printf("%s-iters: %zu\n", op, iters);
    printf("%s-wall-median-ns: %.1f\n", op, w.median);
    printf("%s-wall-mean-ns: %.1f\n",   op, w.mean);
    printf("%s-wall-std-ns: %.1f\n",    op, w.sd);
    printf("%s-wall-p95-ns: %.1f\n",    op, w.p95);
    printf("%s-wall-p99-ns: %.1f\n",    op, w.p99);
    printf("%s-wall-ci95lo-ns: %.1f\n", op, w.ci_lo);
    printf("%s-wall-ci95hi-ns: %.1f\n", op, w.ci_hi);
    printf("%s-ctr-median: %.1f\n",     op, c.median);
    printf("%s-ctr-mean: %.1f\n",       op, c.mean);
    if (w.median > 0) printf("%s-persec: %.1f\n", op, 1e9 / w.median);
    fflush(stdout);
}

// ---------------------------------------------------------------------------
// Conformance self-check: compare liboqs-reported sizes / NIST level against
// the FIPS 203 / FIPS 204 reference table. Prints machine-readable lines plus a
// human WARNING to stderr on mismatch. Does NOT abort (numbers still produced),
// but a mismatch means a non-standard or outdated liboqs.
// ---------------------------------------------------------------------------
// RAII for liboqs handles (stateless deleter -> zero size overhead, unlike a
// function-pointer deleter). Frees on every return/exit path automatically.
struct OqsKemDeleter { void operator()(OQS_KEM *k) const { OQS_KEM_free(k); } };
struct OqsSigDeleter { void operator()(OQS_SIG *s) const { OQS_SIG_free(s); } };
using OqsKemPtr = std::unique_ptr<OQS_KEM, OqsKemDeleter>;
using OqsSigPtr = std::unique_ptr<OQS_SIG, OqsSigDeleter>;

static void check_kem_conformance(const OQS_KEM *kem) {
    const KemRef *r = nullptr;
    for (const auto &e : KEM_REF) if (g_algo == e.name) { r = &e; break; }
    if (!r) { printf("fips203-conformance: N/A (no reference for %s)\n", g_algo.c_str()); return; }

    bool ok = (kem->length_public_key   == r->pk) &&
              (kem->length_secret_key   == r->sk) &&
              (kem->length_ciphertext   == r->ct) &&
              (kem->length_shared_secret== r->ss) &&
              ((int)kem->claimed_nist_level == r->level) &&
              (kem->ind_cca == r->ind_cca);
    printf("fips203-expected-pk-sk-ct-ss: %zu,%zu,%zu,%zu\n", r->pk, r->sk, r->ct, r->ss);
    printf("fips203-expected-nist-level: %d\n", r->level);
    printf("nist-level: %d\n", (int)kem->claimed_nist_level);
    printf("ind-cca: %s\n", kem->ind_cca ? "Y" : "N");
    printf("fips203-conformance: %s\n", ok ? "PASS" : "WARN");
    if (!ok) fprintf(stderr,
        "WARNING: size/level of '%s' does NOT match FIPS 203 "
        "(liboqs pk=%zu sk=%zu ct=%zu ss=%zu level=%d ind_cca=%d). "
        "May be an old or non-standard liboqs.\n",
        g_algo.c_str(), kem->length_public_key, kem->length_secret_key,
        kem->length_ciphertext, kem->length_shared_secret,
        (int)kem->claimed_nist_level, (int)kem->ind_cca);
}

static void check_sig_conformance(const OQS_SIG *sig) {
    const SigRef *r = nullptr;
    for (const auto &e : SIG_REF) if (g_algo == e.name) { r = &e; break; }
    if (!r) { printf("fips204-conformance: N/A (no reference for %s)\n", g_algo.c_str()); return; }

    bool ok = (sig->length_public_key == r->pk) &&
              (sig->length_secret_key == r->sk) &&
              (sig->length_signature  == r->sig) &&     // ML-DSA sig length is fixed
              ((int)sig->claimed_nist_level == r->level);
    printf("fips204-expected-pk-sk-sig: %zu,%zu,%zu\n", r->pk, r->sk, r->sig);
    printf("fips204-expected-nist-level: %d\n", r->level);
    printf("nist-level: %d\n", (int)sig->claimed_nist_level);
    printf("euf-cma: %s\n", sig->euf_cma ? "Y" : "N");
    printf("fips204-conformance: %s\n", ok ? "PASS" : "WARN");
    if (!ok) fprintf(stderr,
        "WARNING: size/level of '%s' does NOT match FIPS 204 "
        "(liboqs pk=%zu sk=%zu sig=%zu level=%d). May be an old or non-standard liboqs.\n",
        g_algo.c_str(), sig->length_public_key, sig->length_secret_key,
        sig->length_signature, (int)sig->claimed_nist_level);
}

// ===========================================================================
// ML-KEM  (FIPS 203)  via OQS_KEM_*
//   keygen = Alg.19 ; encap = Alg.20 (ek -> c,K, NO message) ; decap = Alg.21.
//   Buffers allocated ONCE; each op timed in its own loop; decap reuses the
//   SAME fixed ciphertext.
// ===========================================================================
static void bench_kem(const char *name) {
    size_t iters  = env_size("BENCH_ITERS", 2000);
    size_t kiters = env_size("BENCH_KEYGEN_ITERS", 200);
    size_t warmup = env_size("BENCH_WARMUP", 20);

    OqsKemPtr kem(OQS_KEM_new(name));
    if (!kem) {
        fprintf(stderr, "ERROR: KEM '%s' not built into this liboqs\n", name);
        exit(2);
    }
    g_algo = name;
    printf("algo: %s\n", g_algo.c_str());

    // FIPS 203 conformance (sizes + NIST level + IND-CCA) BEFORE timing.
    check_kem_conformance(kem.get());

    std::vector<uint8_t> pk(kem->length_public_key);
    std::vector<uint8_t> sk(kem->length_secret_key);
    std::vector<uint8_t> ct(kem->length_ciphertext);
    std::vector<uint8_t> ss_e(kem->length_shared_secret);
    std::vector<uint8_t> ss_d(kem->length_shared_secret);

    // keygen [Alg.19]: overwrite pk/sk each iteration; after the loop pk/sk hold
    // a valid keypair.
    run_op("keygen", kiters, warmup, [&]() {
        if (OQS_KEM_keypair(kem.get(), pk.data(), sk.data()) != OQS_SUCCESS)
            fatal("OQS_KEM_keypair");
    });

    // Correctness [Alg.20 then Alg.21]: encaps -> decaps -> shared secrets MUST
    // match (FIPS 203 / RFC 9936). This is the dcommey/pqc_evaluation check.
    if (OQS_KEM_encaps(kem.get(), ct.data(), ss_e.data(), pk.data()) != OQS_SUCCESS)
        fatal("OQS_KEM_encaps (setup)");
    if (OQS_KEM_decaps(kem.get(), ss_d.data(), ct.data(), sk.data()) != OQS_SUCCESS)
        fatal("OQS_KEM_decaps (setup)");
    if (memcmp(ss_e.data(), ss_d.data(), kem->length_shared_secret) != 0)
        fatal("ML-KEM round-trip: shared secret MISMATCH (encap != decap)");
    printf("kem-roundtrip: OK\n");

    // encap [Alg.20]: fixed public key; ct/ss_e refreshed each iteration.
    run_op("encap", iters, warmup, [&]() {
        if (OQS_KEM_encaps(kem.get(), ct.data(), ss_e.data(), pk.data()) != OQS_SUCCESS)
            fatal("OQS_KEM_encaps");
    });

    // decap [Alg.21]: decapsulate the SAME fixed ciphertext repeatedly.
    run_op("decap", iters, warmup, [&]() {
        if (OQS_KEM_decaps(kem.get(), ss_d.data(), ct.data(), sk.data()) != OQS_SUCCESS)
            fatal("OQS_KEM_decaps");
    });

    printf("size-pubkey-bytes: %zu\n",        kem->length_public_key);
    printf("size-privkey-bytes: %zu\n",       kem->length_secret_key);
    printf("size-ciphertext-bytes: %zu\n",    kem->length_ciphertext);
    printf("size-shared-secret-bytes: %zu\n", kem->length_shared_secret);
    fflush(stdout);
}

// ===========================================================================
// ML-DSA  (FIPS 204)  via OQS_SIG_*
//   keygen / sign / verify. Sign is randomized ("hedged"); rejection sampling
//   happens INSIDE each OQS_SIG_sign call (no re-init at the API level, unlike
//   the OpenSSL EVP ML-DSA path). Fixed message (OQS_randombytes), each op
//   timed in its own loop, verify re-checks the SAME fixed (message, signature)
//   pair.
// ===========================================================================
static void bench_sig(const char *name) {
    size_t iters  = env_size("BENCH_ITERS", 2000);
    size_t kiters = env_size("BENCH_KEYGEN_ITERS", 200);
    size_t warmup = env_size("BENCH_WARMUP", 20);

    OqsSigPtr sig(OQS_SIG_new(name));
    if (!sig) {
        fprintf(stderr, "ERROR: SIG '%s' not built into this liboqs\n", name);
        exit(2);
    }
    g_algo = name;
    printf("algo: %s\n", g_algo.c_str());

    // FIPS 204 conformance (sizes + NIST level) BEFORE timing.
    check_sig_conformance(sig.get());

    std::vector<uint8_t> pk(sig->length_public_key);
    std::vector<uint8_t> sk(sig->length_secret_key);
    std::vector<uint8_t> signature(sig->length_signature);
    const size_t msg_len = 50;                 // fixed test message length
    std::vector<uint8_t> msg(msg_len);
    OQS_randombytes(msg.data(), msg_len);      // fixed random message (reused)
    size_t sig_len = 0;

    // keygen: after the loop pk/sk hold a valid keypair.
    run_op("keygen", kiters, warmup, [&]() {
        if (OQS_SIG_keypair(sig.get(), pk.data(), sk.data()) != OQS_SUCCESS)
            fatal("OQS_SIG_keypair");
    });

    // Correctness: sign then verify the fixed message (FIPS 204 EUF-CMA round-trip).
    if (OQS_SIG_sign(sig.get(), signature.data(), &sig_len, msg.data(), msg_len, sk.data())
            != OQS_SUCCESS)
        fatal("OQS_SIG_sign (setup)");
    if (OQS_SIG_verify(sig.get(), msg.data(), msg_len, signature.data(), sig_len, pk.data())
            != OQS_SUCCESS)
        fatal("ML-DSA: verify of just-signed message FAILED");
    printf("sig-roundtrip: OK\n");

    // sign: signature/sig_len refreshed each iteration (same message, randomized).
    run_op("sign", iters, warmup, [&]() {
        size_t outl = signature.size();
        if (OQS_SIG_sign(sig.get(), signature.data(), &outl, msg.data(), msg_len, sk.data())
                != OQS_SUCCESS)
            fatal("OQS_SIG_sign");
        sig_len = outl;   // ML-DSA fixed; captured in case of variable-length schemes
    });

    // verify: re-verify the SAME fixed (message, signature) pair repeatedly.
    run_op("verify", iters, warmup, [&]() {
        if (OQS_SIG_verify(sig.get(), msg.data(), msg_len, signature.data(), sig_len,
                           pk.data()) != OQS_SUCCESS)
            fatal("OQS_SIG_verify: signature invalid");
    });

    printf("size-pubkey-bytes: %zu\n",           sig->length_public_key);
    printf("size-privkey-bytes: %zu\n",          sig->length_secret_key);
    printf("size-signature-bytes: %zu\n",        sig->length_signature);   // max
    printf("size-signature-actual-bytes: %zu\n", sig_len);
    fflush(stdout);
}

// ===========================================================================
// main
// ===========================================================================
static void usage(const char *p) {
    fprintf(stderr,
        "Usage: %s <family> <param>\n"
        "  mlkem 512|768|1024\n"
        "  mldsa 44|65|87\n"
        "Or pass the full liboqs name: %s ML-KEM-768\n"
        "Env vars: BENCH_ITERS, BENCH_KEYGEN_ITERS, BENCH_WARMUP, BENCH_CSV\n",
        p, p);
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 1; }

    OQS_init();
    printf("lib: liboqs %s\n", OQS_version());
    printf("counter: %s (raw HW counter, not retired core cycles)\n", CTR_NAME);

    // Resolve the liboqs algorithm name.
    std::string name;
    std::string a1 = argv[1];
    if (a1.rfind("ML-KEM-", 0) == 0 || a1.rfind("ML-DSA-", 0) == 0) {
        name = a1;                                   // full name passed directly
    } else if (argc >= 3) {
        std::string fam = a1, param = argv[2];
        if (fam == "mlkem")      name = "ML-KEM-" + param;
        else if (fam == "mldsa") name = "ML-DSA-" + param;
        else { usage(argv[0]); OQS_destroy(); return 1; }
    } else {
        usage(argv[0]); OQS_destroy(); return 1;
    }

    const char *csv = getenv("BENCH_CSV");
    if (csv && *csv) {
        g_csv = fopen(csv, "w");
        if (!g_csv) fatal("cannot open BENCH_CSV");
        fprintf(g_csv, "algo,op,iter,wall_ns,counter\n");
    }

    if (name.rfind("ML-KEM-", 0) == 0)      bench_kem(name.c_str());
    else if (name.rfind("ML-DSA-", 0) == 0) bench_sig(name.c_str());
    else { usage(argv[0]); if (g_csv) fclose(g_csv); OQS_destroy(); return 1; }

    if (g_csv) fclose(g_csv);
    OQS_destroy();
    return 0;
}