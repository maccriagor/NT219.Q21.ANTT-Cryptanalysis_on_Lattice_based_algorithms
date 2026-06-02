/**
 * ML-KEM (FIPS 203) MICROBENCHMARK ĐA MỨC NIST — NT219 Capstone
 * ---------------------------------------------------------------------------
 * Quét đủ 3 mức bảo mật NIST (file 05 §7.1 "không chỉ mức giữa"):
 *     ML-KEM-512 (Cat 1) · ML-KEM-768 (Cat 3) · ML-KEM-1024 (Cat 5)
 * Đo 3 thao tác KEM: Keygen (K), Encapsulation (E), Decapsulation (D).
 * Thư viện: PQClean *_clean (reference C — bản đối chứng cho WP3 NEON/AVX2).
 *
 * Dùng:   ./benchmark_mlkem <512|768|1024>     (runner.py lặp đủ 3 mức × 100 lần)
 * Đo:     single-shot/lần (rdtsc + chrono ns) — runner gom 100 mẫu/mức để lấy
 *         median/p95 ở WP7. Cổng đúng đắn: ss_encaps == ss_decaps.
 * Output: "DATA:K_cyc,K_ns,E_cyc,E_ns,D_cyc,D_ns"  (lỗi correctness → stderr, exit!=0).
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
#include "mlkem512/api.h"
#include "mlkem768/api.h"
#include "mlkem1024/api.h"
}

// ---------------- khung đo cycles + thời gian (x86 rdtsc / ARM cntvct) ----------------
struct BenchResult { uint64_t cycles; long long ns; };

static inline void serialize() {
#if defined(__x86_64__) || defined(_M_X64)
    unsigned int a, b, c, d;
    __asm__ __volatile__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
#elif defined(__aarch64__)
    __asm__ __volatile__("dmb sy" ::: "memory");
#endif
}
static inline uint64_t get_cycles() {
#if defined(__x86_64__) || defined(_M_X64)
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
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

// ---------------- bảng tham số (API cùng chữ ký giữa các mức) ----------------
typedef int (*kp_t)(uint8_t*, uint8_t*);
typedef int (*enc_t)(uint8_t*, uint8_t*, const uint8_t*);
typedef int (*dec_t)(uint8_t*, const uint8_t*, const uint8_t*);
struct Param { const char* lvl; const char* cat; int pk, sk, ct, ss; kp_t kp; enc_t enc; dec_t dec; };

static const Param PARAMS[] = {
  {"512","1",
   PQCLEAN_MLKEM512_CLEAN_CRYPTO_PUBLICKEYBYTES, PQCLEAN_MLKEM512_CLEAN_CRYPTO_SECRETKEYBYTES,
   PQCLEAN_MLKEM512_CLEAN_CRYPTO_CIPHERTEXTBYTES, PQCLEAN_MLKEM512_CLEAN_CRYPTO_BYTES,
   PQCLEAN_MLKEM512_CLEAN_crypto_kem_keypair, PQCLEAN_MLKEM512_CLEAN_crypto_kem_enc, PQCLEAN_MLKEM512_CLEAN_crypto_kem_dec},
  {"768","3",
   PQCLEAN_MLKEM768_CLEAN_CRYPTO_PUBLICKEYBYTES, PQCLEAN_MLKEM768_CLEAN_CRYPTO_SECRETKEYBYTES,
   PQCLEAN_MLKEM768_CLEAN_CRYPTO_CIPHERTEXTBYTES, PQCLEAN_MLKEM768_CLEAN_CRYPTO_BYTES,
   PQCLEAN_MLKEM768_CLEAN_crypto_kem_keypair, PQCLEAN_MLKEM768_CLEAN_crypto_kem_enc, PQCLEAN_MLKEM768_CLEAN_crypto_kem_dec},
  {"1024","5",
   PQCLEAN_MLKEM1024_CLEAN_CRYPTO_PUBLICKEYBYTES, PQCLEAN_MLKEM1024_CLEAN_CRYPTO_SECRETKEYBYTES,
   PQCLEAN_MLKEM1024_CLEAN_CRYPTO_CIPHERTEXTBYTES, PQCLEAN_MLKEM1024_CLEAN_CRYPTO_BYTES,
   PQCLEAN_MLKEM1024_CLEAN_crypto_kem_keypair, PQCLEAN_MLKEM1024_CLEAN_crypto_kem_enc, PQCLEAN_MLKEM1024_CLEAN_crypto_kem_dec},
};

int main(int argc, char** argv) {
#ifdef __linux__
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(0, &cs);
    if (sched_setaffinity(0, sizeof(cs), &cs) != 0) std::cerr << "[warn] CPU affinity\n";
#endif
    std::string want = (argc > 1) ? argv[1] : "1024";
    const Param* p = nullptr;
    for (const auto& q : PARAMS) if (want == q.lvl) p = &q;
    if (!p) { std::cerr << "usage: benchmark_mlkem <512|768|1024>\n"; return 2; }

    std::vector<uint8_t> pk(p->pk), sk(p->sk), ct(p->ct), ssE(p->ss), ssD(p->ss);
    BenchResult k = measure([&]{ p->kp(pk.data(), sk.data()); });
    BenchResult e = measure([&]{ p->enc(ct.data(), ssE.data(), pk.data()); });
    BenchResult d = measure([&]{ p->dec(ssD.data(), ct.data(), sk.data()); });

    if (std::memcmp(ssE.data(), ssD.data(), p->ss) != 0) {
        std::cerr << "[CORRECTNESS FAIL] ML-KEM-" << p->lvl << " shared secret mismatch\n";
        return 1;
    }
    std::cout << "DATA:" << k.cycles << "," << k.ns << "," << e.cycles << "," << e.ns
              << "," << d.cycles << "," << d.ns << std::endl;
    return 0;
}
