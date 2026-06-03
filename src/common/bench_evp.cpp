// ============================================================================
// bench_evp.cpp  —  Microbenchmark mat ma bang OpenSSL 3.x EVP (NT219)
//
//   ./build/bench_evp <family> <param>
//   family: rsa | ecdsa | ecdh | mlkem | mldsa
//   vd: ./build/bench_evp rsa 2048      ./build/bench_evp ecdsa p256
//       ./build/bench_evp mlkem 768     ./build/bench_evp mldsa 65   (can OpenSSL >= 3.5)
//
// Phuong phap (microbenchmark): voi MOI thao tac, chay N iterations (lay tu env),
// do CA wall-clock (ns, clock_gettime CLOCK_MONOTONIC_RAW) LAN cycles (rdtsc/cntvct),
// roi tinh median / mean / std / p95 / p99 / 95% CI. So lan lap = BIEN MOI TRUONG:
//   BENCH_ITERS        (mac dinh 2000)  -- so vong cho op nhanh
//   BENCH_KEYGEN_ITERS (mac dinh 50)    -- keygen cham nen it vong hon
//   BENCH_WARMUP       (mac dinh 20)    -- so vong warm-up bi loai (cache/lazy-init)
//   BENCH_CSV=path                       -- neu set, ghi mau tho (raw samples) ra CSV
// Doi so dong lenh KHONG can bien dich lai -> tai lap tot, ghi env vao ket qua.
//
// Build (KHUYEN NGHI dung Makefile, KHONG go g++ tay):
//   make                              # OpenSSL he thong
//   make OSSLROOT=/opt/openssl-3.6.2  # OpenSSL tu build (PQC native)
//
// LUU Y: chi dung OpenSSL (link -lcrypto). KHONG #include <cryptopp/...>,
// KHONG link -lcryptopp. Sinh key co the dung tool Crypto++ cua thay (file PEM
// lien thong); harness NAY chi DO bang OpenSSL EVP de so sanh cong bang.
// ============================================================================

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/core_names.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

#if defined(__x86_64__) || defined(__i386__)
#  include <x86intrin.h>          // __rdtscp
#endif

// ---------------------------------------------------------------------------
// Dong ho: wall-clock (ns) va cycle counter.
// ---------------------------------------------------------------------------
static inline uint64_t now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

// Tra ve "cycles". x86: TSC (rdtscp). aarch64: CNTVCT_EL0 — la VIRTUAL TIMER
// tan so co dinh, KHONG phai core cycle that; muon cycle chinh xac tren Pi can
// PMU (kernel module, xem CONFIGURE liboqs). Bao cao wall-ns la chinh, cycles phu.
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

// ---------------------------------------------------------------------------
// Bao loi OpenSSL roi thoat.
// ---------------------------------------------------------------------------
[[noreturn]] static void fatal(const char* msg) {
    fprintf(stderr, "LOI: %s\n", msg);
    ERR_print_errors_fp(stderr);
    exit(1);
}

// ---------------------------------------------------------------------------
// Thong ke tu mot vector mau.
// ---------------------------------------------------------------------------
struct Stats {
    size_t n = 0;
    double median = 0, mean = 0, sd = 0, p95 = 0, p99 = 0, ci_lo = 0, ci_hi = 0;
};

static double percentile(const std::vector<double>& sorted, double p) {
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
        // 95% CI cua MEAN (xap xi normal): mean +/- 1.96 * sd/sqrt(n)
        double half = 1.96 * s.sd / std::sqrt((double)v.size());
        s.ci_lo = s.mean - half;
        s.ci_hi = s.mean + half;
    } else {
        s.ci_lo = s.ci_hi = s.mean;
    }
    return s;
}

// ---------------------------------------------------------------------------
// Lay so vong tu env (mac dinh fallback).
// ---------------------------------------------------------------------------
static size_t env_size(const char* name, size_t dflt) {
    const char* s = getenv(name);
    if (!s || !*s) return dflt;
    long long v = atoll(s);
    return (v > 0) ? (size_t)v : dflt;
}

static const char* g_csv_path = nullptr;
static FILE*        g_csv = nullptr;
static std::string  g_algo;

// ---------------------------------------------------------------------------
// Runner: do mot op qua N iterations + warmup, in key-value, ghi CSV neu can.
//   fn() thuc hien DUNG MOT thao tac (fatal neu loi).
// ---------------------------------------------------------------------------
static void run_op(const char* op, size_t iters, size_t warmup,
                   const std::function<void()>& fn) {
    for (size_t i = 0; i < warmup; ++i) fn();

    std::vector<double> wall(iters), cyc(iters);
    for (size_t i = 0; i < iters; ++i) {
        uint64_t t0 = now_ns();
        uint64_t c0 = read_cycles();
        fn();
        uint64_t c1 = read_cycles();
        uint64_t t1 = now_ns();
        wall[i] = (double)(t1 - t0);
        cyc[i]  = (double)(c1 - c0);
        if (g_csv) fprintf(g_csv, "%s,%s,%zu,%.0f,%.0f\n",
                           g_algo.c_str(), op, i, wall[i], cyc[i]);
    }

    Stats w = compute_stats(wall);
    Stats c = compute_stats(cyc);

    // --- in key-value (separator ": ", tuong thich analyze.py) ---
    printf("%s-iters: %zu\n", op, iters);
    printf("%s-wall-median-ns: %.1f\n", op, w.median);
    printf("%s-wall-mean-ns: %.1f\n",   op, w.mean);
    printf("%s-wall-std-ns: %.1f\n",    op, w.sd);
    printf("%s-wall-p95-ns: %.1f\n",    op, w.p95);
    printf("%s-wall-p99-ns: %.1f\n",    op, w.p99);
    printf("%s-wall-ci95lo-ns: %.1f\n", op, w.ci_lo);
    printf("%s-wall-ci95hi-ns: %.1f\n", op, w.ci_hi);
    printf("%s-cyc-median: %.1f\n",     op, c.median);
    printf("%s-cyc-mean: %.1f\n",       op, c.mean);
    if (w.median > 0) printf("%s-persec: %.1f\n", op, 1e9 / w.median);
    fflush(stdout);
}

// ===========================================================================
// RSA
// ===========================================================================
static void bench_rsa(int bits) {
    size_t iters   = env_size("BENCH_ITERS", 2000);
    size_t kiters  = env_size("BENCH_KEYGEN_ITERS", 50);
    size_t warmup  = env_size("BENCH_WARMUP", 20);

    g_algo = "rsa-" + std::to_string(bits);
    printf("algo: %s\n", g_algo.c_str());
    printf("key-size: %d\n", bits);

    // ---- keygen (tai su dung gctx; moi vong sinh + free 1 key) ----
    {
        EVP_PKEY_CTX* gctx = EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr);
        if (!gctx) fatal("RSA CTX_new_from_name");
        if (EVP_PKEY_keygen_init(gctx) <= 0) fatal("RSA keygen_init");
        if (EVP_PKEY_CTX_set_rsa_keygen_bits(gctx, bits) <= 0) fatal("set_rsa_keygen_bits");
        run_op("keygen", kiters, (warmup < kiters ? warmup : 0), [&]() {
            EVP_PKEY* k = nullptr;
            if (EVP_PKEY_generate(gctx, &k) <= 0) fatal("EVP_PKEY_generate");
            EVP_PKEY_free(k);
        });
        EVP_PKEY_CTX_free(gctx);
    }

    // Sinh 1 key co dinh dung cho cac op con lai
    EVP_PKEY* pkey = nullptr;
    {
        EVP_PKEY_CTX* gctx = EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr);
        EVP_PKEY_keygen_init(gctx);
        EVP_PKEY_CTX_set_rsa_keygen_bits(gctx, bits);
        if (EVP_PKEY_generate(gctx, &pkey) <= 0) fatal("RSA gen fixed key");
        EVP_PKEY_CTX_free(gctx);
    }

    std::vector<unsigned char> tbs(32, 0x5A);   // gia lap digest SHA-256
    std::vector<unsigned char> msg(32, 0xA5);   // ban ro ngan cho OAEP

    // ---- PSS sign ----
    std::vector<unsigned char> sig;
    size_t siglen = 0;
    {
        EVP_PKEY_CTX* sctx = EVP_PKEY_CTX_new_from_pkey(nullptr, pkey, nullptr);
        if (!sctx || EVP_PKEY_sign_init(sctx) <= 0
            || EVP_PKEY_CTX_set_rsa_padding(sctx, RSA_PKCS1_PSS_PADDING) <= 0
            || EVP_PKEY_CTX_set_signature_md(sctx, EVP_sha256()) <= 0)
            fatal("PSS sign init");
        // siglen la size_t* (in/out): goi sig=NULL de lay do dai
        if (EVP_PKEY_sign(sctx, nullptr, &siglen, tbs.data(), tbs.size()) <= 0)
            fatal("PSS sign len");
        sig.resize(siglen);
        run_op("pss-sign", iters, warmup, [&]() {
            size_t outl = sig.size();              // in/out moi vong
            if (EVP_PKEY_sign(sctx, sig.data(), &outl, tbs.data(), tbs.size()) <= 0)
                fatal("PSS sign");
        });
        EVP_PKEY_CTX_free(sctx);
    }

    // ---- PSS verify ----  (luu y: siglen la GIA TRI, khong dung &)
    {
        EVP_PKEY_CTX* vctx = EVP_PKEY_CTX_new_from_pkey(nullptr, pkey, nullptr);
        if (!vctx || EVP_PKEY_verify_init(vctx) <= 0
            || EVP_PKEY_CTX_set_rsa_padding(vctx, RSA_PKCS1_PSS_PADDING) <= 0
            || EVP_PKEY_CTX_set_signature_md(vctx, EVP_sha256()) <= 0)
            fatal("PSS verify init");
        run_op("pss-verify", iters, warmup, [&]() {
            int rc = EVP_PKEY_verify(vctx, sig.data(), siglen, tbs.data(), tbs.size());
            if (rc < 0) fatal("PSS verify error");
            if (rc == 0) fatal("PSS chu ky khong hop le");
        });
        EVP_PKEY_CTX_free(vctx);
    }

    // ---- OAEP encrypt ----
    std::vector<unsigned char> ct;
    size_t ctlen = 0;
    {
        EVP_PKEY_CTX* ectx = EVP_PKEY_CTX_new_from_pkey(nullptr, pkey, nullptr);
        if (!ectx || EVP_PKEY_encrypt_init(ectx) <= 0
            || EVP_PKEY_CTX_set_rsa_padding(ectx, RSA_PKCS1_OAEP_PADDING) <= 0
            || EVP_PKEY_CTX_set_rsa_oaep_md(ectx, EVP_sha256()) <= 0)
            fatal("OAEP enc init");
        if (EVP_PKEY_encrypt(ectx, nullptr, &ctlen, msg.data(), msg.size()) <= 0)
            fatal("OAEP enc len");
        ct.resize(ctlen);
        run_op("oaep-encrypt", iters, warmup, [&]() {
            size_t outl = ct.size();
            if (EVP_PKEY_encrypt(ectx, ct.data(), &outl, msg.data(), msg.size()) <= 0)
                fatal("OAEP encrypt");
        });
        EVP_PKEY_CTX_free(ectx);
    }

    // ---- OAEP decrypt ----
    {
        std::vector<unsigned char> pt(EVP_PKEY_get_size(pkey));
        EVP_PKEY_CTX* dctx = EVP_PKEY_CTX_new_from_pkey(nullptr, pkey, nullptr);
        if (!dctx || EVP_PKEY_decrypt_init(dctx) <= 0
            || EVP_PKEY_CTX_set_rsa_padding(dctx, RSA_PKCS1_OAEP_PADDING) <= 0
            || EVP_PKEY_CTX_set_rsa_oaep_md(dctx, EVP_sha256()) <= 0)
            fatal("OAEP dec init");
        run_op("oaep-decrypt", iters, warmup, [&]() {
            size_t outl = pt.size();
            if (EVP_PKEY_decrypt(dctx, pt.data(), &outl, ct.data(), ctlen) <= 0)
                fatal("OAEP decrypt");
        });
        EVP_PKEY_CTX_free(dctx);
    }
    EVP_PKEY_free(pkey);
}

// ===========================================================================
// ECDSA / ECDH  (curve: "P-256" | "P-384" | "P-521")
// ===========================================================================
static EVP_PKEY* ec_gen(const char* curve) {
    EVP_PKEY* k = EVP_EC_gen(curve);   // tien ich 3.0+: sinh khoa EC theo ten duong cong
    if (!k) fatal("EVP_EC_gen");
    return k;
}

static void bench_ecdsa(const char* curve) {
    size_t iters  = env_size("BENCH_ITERS", 2000);
    size_t kiters = env_size("BENCH_KEYGEN_ITERS", 50);
    size_t warmup = env_size("BENCH_WARMUP", 20);

    g_algo = std::string("ecdsa-") + curve;
    printf("algo: %s\n", g_algo.c_str());

    run_op("keygen", kiters, (warmup < kiters ? warmup : 0), [&]() {
        EVP_PKEY* k = ec_gen(curve);
        EVP_PKEY_free(k);
    });

    EVP_PKEY* pkey = ec_gen(curve);
    std::vector<unsigned char> tbs(32, 0x5A);
    std::vector<unsigned char> sig;
    size_t siglen = 0;
    {
        EVP_PKEY_CTX* sctx = EVP_PKEY_CTX_new_from_pkey(nullptr, pkey, nullptr);
        if (!sctx || EVP_PKEY_sign_init(sctx) <= 0
            || EVP_PKEY_CTX_set_signature_md(sctx, EVP_sha256()) <= 0)
            fatal("ECDSA sign init");
        if (EVP_PKEY_sign(sctx, nullptr, &siglen, tbs.data(), tbs.size()) <= 0)
            fatal("ECDSA sign len");
        sig.resize(siglen);
        run_op("sign", iters, warmup, [&]() {
            size_t outl = sig.size();
            if (EVP_PKEY_sign(sctx, sig.data(), &outl, tbs.data(), tbs.size()) <= 0)
                fatal("ECDSA sign");
            siglen = outl;   // do dai chu ky ECDSA thay doi (DER) -> cap nhat cho verify
        });
        EVP_PKEY_CTX_free(sctx);
    }
    {
        EVP_PKEY_CTX* vctx = EVP_PKEY_CTX_new_from_pkey(nullptr, pkey, nullptr);
        if (!vctx || EVP_PKEY_verify_init(vctx) <= 0
            || EVP_PKEY_CTX_set_signature_md(vctx, EVP_sha256()) <= 0)
            fatal("ECDSA verify init");
        run_op("verify", iters, warmup, [&]() {
            int rc = EVP_PKEY_verify(vctx, sig.data(), siglen, tbs.data(), tbs.size());
            if (rc < 0) fatal("ECDSA verify error");
        });
        EVP_PKEY_CTX_free(vctx);
    }
    EVP_PKEY_free(pkey);
}

static void bench_ecdh(const char* curve) {
    size_t iters  = env_size("BENCH_ITERS", 2000);
    size_t kiters = env_size("BENCH_KEYGEN_ITERS", 50);
    size_t warmup = env_size("BENCH_WARMUP", 20);

    g_algo = std::string("ecdh-") + curve;
    printf("algo: %s\n", g_algo.c_str());

    run_op("keygen", kiters, (warmup < kiters ? warmup : 0), [&]() {
        EVP_PKEY* k = ec_gen(curve);
        EVP_PKEY_free(k);
    });

    EVP_PKEY* self = ec_gen(curve);
    EVP_PKEY* peer = ec_gen(curve);
    {
        EVP_PKEY_CTX* dctx = EVP_PKEY_CTX_new_from_pkey(nullptr, self, nullptr);
        if (!dctx || EVP_PKEY_derive_init(dctx) <= 0
            || EVP_PKEY_derive_set_peer(dctx, peer) <= 0)
            fatal("ECDH derive init");
        size_t slen = 0;
        if (EVP_PKEY_derive(dctx, nullptr, &slen) <= 0) fatal("ECDH derive len");
        std::vector<unsigned char> secret(slen);
        run_op("derive", iters, warmup, [&]() {
            size_t outl = secret.size();
            if (EVP_PKEY_derive(dctx, secret.data(), &outl) <= 0) fatal("ECDH derive");
        });
        EVP_PKEY_CTX_free(dctx);
    }
    EVP_PKEY_free(self);
    EVP_PKEY_free(peer);
}

// ===========================================================================
// ML-KEM / ML-DSA  — CAN OpenSSL >= 3.5 (compile-in khi du version)
// ===========================================================================
#if OPENSSL_VERSION_NUMBER >= 0x30500000L
static EVP_PKEY* pqc_gen(const char* name) {
    EVP_PKEY_CTX* gctx = EVP_PKEY_CTX_new_from_name(nullptr, name, nullptr);
    if (!gctx) fatal("PQC CTX_new_from_name");
    if (EVP_PKEY_keygen_init(gctx) <= 0) fatal("PQC keygen_init");
    EVP_PKEY* k = nullptr;
    if (EVP_PKEY_generate(gctx, &k) <= 0) fatal("PQC generate");
    EVP_PKEY_CTX_free(gctx);
    return k;
}

static void bench_mlkem(const char* name) {   // name vd "ML-KEM-768"
    size_t iters  = env_size("BENCH_ITERS", 2000);
    size_t kiters = env_size("BENCH_KEYGEN_ITERS", 200);
    size_t warmup = env_size("BENCH_WARMUP", 20);

    g_algo = name;
    printf("algo: %s\n", g_algo.c_str());

    run_op("keygen", kiters, warmup, [&]() {
        EVP_PKEY* k = pqc_gen(name);
        EVP_PKEY_free(k);
    });

    EVP_PKEY* kp = pqc_gen(name);

    // encaps: lay kich thuoc truoc (NULL), roi loop
    EVP_PKEY_CTX* ectx = EVP_PKEY_CTX_new_from_pkey(nullptr, kp, nullptr);
    if (!ectx || EVP_PKEY_encapsulate_init(ectx, nullptr) <= 0) fatal("encaps init");
    size_t wlen = 0, klen = 0;
    if (EVP_PKEY_encapsulate(ectx, nullptr, &wlen, nullptr, &klen) <= 0)
        fatal("encaps len");
    std::vector<unsigned char> wrapped(wlen), shared(klen);
    run_op("encap", iters, warmup, [&]() {
        size_t wl = wrapped.size(), kl = shared.size();
        if (EVP_PKEY_encapsulate(ectx, wrapped.data(), &wl, shared.data(), &kl) <= 0)
            fatal("encaps");
    });

    // decaps
    EVP_PKEY_CTX* dctx = EVP_PKEY_CTX_new_from_pkey(nullptr, kp, nullptr);
    if (!dctx || EVP_PKEY_decapsulate_init(dctx, nullptr) <= 0) fatal("decaps init");
    std::vector<unsigned char> recovered(klen);
    run_op("decap", iters, warmup, [&]() {
        size_t rl = recovered.size();
        if (EVP_PKEY_decapsulate(dctx, recovered.data(), &rl,
                                 wrapped.data(), wrapped.size()) <= 0)
            fatal("decaps");
    });

    EVP_PKEY_CTX_free(ectx);
    EVP_PKEY_CTX_free(dctx);
    EVP_PKEY_free(kp);
}

static void bench_mldsa(const char* name) {    // name vd "ML-DSA-65"
    size_t iters  = env_size("BENCH_ITERS", 2000);
    size_t kiters = env_size("BENCH_KEYGEN_ITERS", 200);
    size_t warmup = env_size("BENCH_WARMUP", 20);

    g_algo = name;
    printf("algo: %s\n", g_algo.c_str());

    run_op("keygen", kiters, warmup, [&]() {
        EVP_PKEY* k = pqc_gen(name);
        EVP_PKEY_free(k);
    });

    EVP_PKEY* kp = pqc_gen(name);
    EVP_SIGNATURE* sig_alg = EVP_SIGNATURE_fetch(nullptr, name, nullptr);
    if (!sig_alg) fatal("EVP_SIGNATURE_fetch ML-DSA");

    std::vector<unsigned char> msg(64, 0x5A);
    std::vector<unsigned char> sig;
    size_t siglen = 0;
    // ML-DSA la "pure" signature: sign_message_init (md ngam dinh = NULL).
    {
        EVP_PKEY_CTX* sctx = EVP_PKEY_CTX_new_from_pkey(nullptr, kp, nullptr);
        if (!sctx || EVP_PKEY_sign_message_init(sctx, sig_alg, nullptr) <= 0)
            fatal("ML-DSA sign_message_init");
        if (EVP_PKEY_sign(sctx, nullptr, &siglen, msg.data(), msg.size()) <= 0)
            fatal("ML-DSA sign len");
        sig.resize(siglen);
        run_op("sign", iters, warmup, [&]() {
            // rejection sampling -> phai re-init moi vong (theo mau OpenSSL)
            size_t outl = sig.size();
            if (EVP_PKEY_sign_message_init(sctx, sig_alg, nullptr) <= 0
                || EVP_PKEY_sign(sctx, sig.data(), &outl, msg.data(), msg.size()) <= 0)
                fatal("ML-DSA sign");
            siglen = outl;
        });
        EVP_PKEY_CTX_free(sctx);
    }
    {
        EVP_PKEY_CTX* vctx = EVP_PKEY_CTX_new_from_pkey(nullptr, kp, nullptr);
        if (!vctx) fatal("ML-DSA verify ctx");
        run_op("verify", iters, warmup, [&]() {
            if (EVP_PKEY_verify_message_init(vctx, sig_alg, nullptr) <= 0)
                fatal("ML-DSA verify_message_init");
            int rc = EVP_PKEY_verify(vctx, sig.data(), siglen, msg.data(), msg.size());
            if (rc < 0) fatal("ML-DSA verify error");
        });
        EVP_PKEY_CTX_free(vctx);
    }
    EVP_SIGNATURE_free(sig_alg);
    EVP_PKEY_free(kp);
}
#endif // OpenSSL >= 3.5

// ===========================================================================
// main
// ===========================================================================
static void usage(const char* p) {
    fprintf(stderr,
        "Cach dung: %s <family> <param>\n"
        "  rsa   2048|3072|4096|15360\n"
        "  ecdsa p256|p384|p521\n"
        "  ecdh  p256|p384|p521\n"
        "  mlkem 512|768|1024     (can OpenSSL >= 3.5)\n"
        "  mldsa 44|65|87         (can OpenSSL >= 3.5)\n"
        "Bien moi truong: BENCH_ITERS, BENCH_KEYGEN_ITERS, BENCH_WARMUP, BENCH_CSV\n", p);
}

static std::string curve_of(const std::string& p) {
    if (p == "p256" || p == "P-256") return "P-256";
    if (p == "p384" || p == "P-384") return "P-384";
    if (p == "p521" || p == "P-521") return "P-521";
    return "";
}

int main(int argc, char** argv) {
    if (argc < 3) { usage(argv[0]); return 1; }
    std::string fam = argv[1], param = argv[2];

    printf("openssl: %s\n", OpenSSL_version(OPENSSL_VERSION));

    g_csv_path = getenv("BENCH_CSV");
    if (g_csv_path && *g_csv_path) {
        g_csv = fopen(g_csv_path, "w");
        if (!g_csv) fatal("khong mo duoc BENCH_CSV");
        fprintf(g_csv, "algo,op,iter,wall_ns,cycles\n");
    }

    if (fam == "rsa") {
        bench_rsa(atoi(param.c_str()));
    } else if (fam == "ecdsa") {
        std::string c = curve_of(param);
        if (c.empty()) { usage(argv[0]); return 1; }
        bench_ecdsa(c.c_str());
    } else if (fam == "ecdh") {
        std::string c = curve_of(param);
        if (c.empty()) { usage(argv[0]); return 1; }
        bench_ecdh(c.c_str());
    } else if (fam == "mlkem" || fam == "mldsa") {
#if OPENSSL_VERSION_NUMBER >= 0x30500000L
        std::string name = (fam == "mlkem" ? "ML-KEM-" : "ML-DSA-") + param;
        if (fam == "mlkem") bench_mlkem(name.c_str());
        else                bench_mldsa(name.c_str());
#else
        fprintf(stderr,
            "ML-KEM/ML-DSA can OpenSSL >= 3.5. Build lai voi OpenSSL 3.6.2:\n"
            "  make OSSLROOT=/opt/openssl-3.6.2\n");
        if (g_csv) fclose(g_csv);
        return 2;
#endif
    } else {
        usage(argv[0]);
        if (g_csv) fclose(g_csv);
        return 1;
    }

    if (g_csv) fclose(g_csv);
    return 0;
}
