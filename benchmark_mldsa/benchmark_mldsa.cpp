/**
 * ML-DSA (FIPS 204) MICROBENCHMARK ĐA MỨC NIST — NT219 Capstone
 * ---------------------------------------------------------------------------
 * Quét đủ 3 mức: ML-DSA-44 (Cat 2) · ML-DSA-65 (Cat 3) · ML-DSA-87 (Cat 5).
 * Đo: Keygen (K), Sign (S), Verify (V). Thư viện: PQClean *_clean (reference C).
 *
 * Ghi chú khoa học: Sign dùng rejection sampling (Fiat–Shamir w/ aborts) → phân phối
 * thời gian LỆCH PHẢI; đo single-shot × 100 lần để giữ phân phối (median/p95 ở WP7).
 * Cổng đúng đắn: Verify(thật)=PASS và Verify(hỏng)=FAIL.
 *
 * Dùng:   ./benchmark_mldsa <44|65|87>
 * Output: "DATA:K_cyc,K_ns,S_cyc,S_ns,V_cyc,V_ns"
 */

#include <iostream>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <string>
#include <vector>
#ifdef __linux__
#include <sched.h>
#endif

extern "C" {
#include "mldsa44/api.h"
#include "mldsa65/api.h"
#include "mldsa87/api.h"
}

struct BenchResult { uint64_t cycles; long long ns; };
static inline void serialize() {
#if defined(__x86_64__) || defined(_M_X64)
    unsigned int a, b, c, d; __asm__ __volatile__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
#elif defined(__aarch64__)
    __asm__ __volatile__("dmb sy" ::: "memory");
#endif
}
static inline uint64_t get_cycles() {
#if defined(__x86_64__) || defined(_M_X64)
    unsigned int lo, hi; __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi)); return ((uint64_t)hi << 32) | lo;
#elif defined(__aarch64__)
    uint64_t v; __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(v)); return v;
#else
    return 0;
#endif
}
template <typename Func>
BenchResult measure(Func f) {
    auto t0 = std::chrono::high_resolution_clock::now();
    serialize(); uint64_t c0 = get_cycles(); serialize();
    f();
    serialize(); uint64_t c1 = get_cycles(); serialize();
    auto t1 = std::chrono::high_resolution_clock::now();
    return { c1 - c0, std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() };
}

typedef int (*kp_t)(uint8_t*, uint8_t*);
typedef int (*sign_t)(uint8_t*, size_t*, const uint8_t*, size_t, const uint8_t*);
typedef int (*vrfy_t)(const uint8_t*, size_t, const uint8_t*, size_t, const uint8_t*);
struct Param { const char* lvl; const char* cat; int pk, sk, sig; kp_t kp; sign_t sign; vrfy_t vrfy; };

static const Param PARAMS[] = {
  {"44","2",
   PQCLEAN_MLDSA44_CLEAN_CRYPTO_PUBLICKEYBYTES, PQCLEAN_MLDSA44_CLEAN_CRYPTO_SECRETKEYBYTES, PQCLEAN_MLDSA44_CLEAN_CRYPTO_BYTES,
   PQCLEAN_MLDSA44_CLEAN_crypto_sign_keypair, PQCLEAN_MLDSA44_CLEAN_crypto_sign_signature, PQCLEAN_MLDSA44_CLEAN_crypto_sign_verify},
  {"65","3",
   PQCLEAN_MLDSA65_CLEAN_CRYPTO_PUBLICKEYBYTES, PQCLEAN_MLDSA65_CLEAN_CRYPTO_SECRETKEYBYTES, PQCLEAN_MLDSA65_CLEAN_CRYPTO_BYTES,
   PQCLEAN_MLDSA65_CLEAN_crypto_sign_keypair, PQCLEAN_MLDSA65_CLEAN_crypto_sign_signature, PQCLEAN_MLDSA65_CLEAN_crypto_sign_verify},
  {"87","5",
   PQCLEAN_MLDSA87_CLEAN_CRYPTO_PUBLICKEYBYTES, PQCLEAN_MLDSA87_CLEAN_CRYPTO_SECRETKEYBYTES, PQCLEAN_MLDSA87_CLEAN_CRYPTO_BYTES,
   PQCLEAN_MLDSA87_CLEAN_crypto_sign_keypair, PQCLEAN_MLDSA87_CLEAN_crypto_sign_signature, PQCLEAN_MLDSA87_CLEAN_crypto_sign_verify},
};

int main(int argc, char** argv) {
#ifdef __linux__
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(0, &cs);
    if (sched_setaffinity(0, sizeof(cs), &cs) != 0) std::cerr << "[warn] CPU affinity\n";
#endif
    std::string want = (argc > 1) ? argv[1] : "87";
    const Param* p = nullptr;
    for (const auto& q : PARAMS) if (want == q.lvl) p = &q;
    if (!p) { std::cerr << "usage: benchmark_mldsa <44|65|87>\n"; return 2; }

    std::vector<uint8_t> pk(p->pk), sk(p->sk), sig(p->sig);
    size_t siglen = 0;
    unsigned char msg[32];
    for (int i = 0; i < 32; i++) msg[i] = (unsigned char)(i * 7 + 1);

    BenchResult k = measure([&]{ p->kp(pk.data(), sk.data()); });
    BenchResult s = measure([&]{ p->sign(sig.data(), &siglen, msg, 32, sk.data()); });
    int vres = 2;
    BenchResult v = measure([&]{ vres = p->vrfy(sig.data(), siglen, msg, 32, pk.data()); });

    std::vector<uint8_t> bad(sig.begin(), sig.begin() + siglen);
    bad[siglen / 2] ^= 0xFF;
    int vbad = p->vrfy(bad.data(), siglen, msg, 32, pk.data());
    if (vres != 0 || vbad == 0) {
        std::cerr << "[CORRECTNESS FAIL] ML-DSA-" << p->lvl << " verify=" << vres
                  << " tampered_accepted=" << (vbad == 0) << "\n";
        return 1;
    }
    std::cout << "DATA:" << k.cycles << "," << k.ns << "," << s.cycles << "," << s.ns
              << "," << v.cycles << "," << v.ns << std::endl;
    return 0;
}
