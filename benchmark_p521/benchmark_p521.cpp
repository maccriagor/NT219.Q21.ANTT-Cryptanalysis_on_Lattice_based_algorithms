/**
 * ECDSA secp521r1 MICROBENCHMARK - NT219 Capstone
 * Key size: 521-bit (NIST Level 5)
 */

#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <stdint.h>
#include <unistd.h>

#ifdef __linux__
#include <sched.h>
#endif

#define MSG_SIZE 1900
#define MICRO_ITERS 1000

// -----------------------------------------------------------
// Struct for benchmarking time and cpu cycles (PQ-Style)
// -----------------------------------------------------------
struct BenchResult {
    uint64_t cycles;
    long long ns;
};

static inline void serialize() {
    unsigned int a, b, c, d;
    __asm__ __volatile__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
}

static inline uint64_t get_cycles() {
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

template<typename Func>
BenchResult measure(Func f) {
    auto t_start = std::chrono::high_resolution_clock::now();

    serialize();
    uint64_t c_start = get_cycles();
    serialize();

    f(); 

    serialize();
    uint64_t c_end = get_cycles();
    serialize();

    auto t_end = std::chrono::high_resolution_clock::now();

    return { (c_end - c_start),
             std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count() };
}
// -----------------------------------------------------------

class ECDSAP521Benchmark {
private:
    EC_KEY* ec_key = nullptr;
    std::vector<uint8_t> message;
    unsigned char msg_hash[SHA512_DIGEST_LENGTH]; // SHA-512 cho NIST Level 5

    void compute_hash() {
        SHA512(message.data(), message.size(), msg_hash);
    }

public:
    ~ECDSAP521Benchmark() {
        if (ec_key) EC_KEY_free(ec_key);
        ERR_free_strings();
    }

    void init_key() {
        if (ec_key) {
            EC_KEY_free(ec_key);
        }
        ec_key = EC_KEY_new_by_curve_name(NID_secp521r1);
        EC_KEY_generate_key(ec_key);
    }

    void microbenchmarks() {
        std::cout << "\n📊 MICROBENCHMARKS (" << MICRO_ITERS << " iters averaged) " << std::endl;
        
        message.resize(MSG_SIZE);
        RAND_bytes(message.data(), MSG_SIZE);
        compute_hash(); 
        
        // -----------------------------------------------------------
        // 1. Đo hiệu năng Sinh khóa (Keygen)
        // -----------------------------------------------------------
        BenchResult k_data = measure([&]() {
            for (int i = 0; i < MICRO_ITERS; ++i) {
                init_key();
            }
        });
        k_data.cycles /= MICRO_ITERS;
        k_data.ns /= MICRO_ITERS;

        // -----------------------------------------------------------
        // 2. Đo hiệu năng Ký (Sign)
        // -----------------------------------------------------------
        int max_sig_len = ECDSA_size(ec_key);
        std::vector<uint8_t> sig(max_sig_len);
        unsigned int sig_len = 0;

        BenchResult s_data = measure([&]() {
            for (int i = 0; i < MICRO_ITERS; ++i) {
                ECDSA_sign(0, msg_hash, SHA512_DIGEST_LENGTH, sig.data(), &sig_len, ec_key);
            }
        });
        s_data.cycles /= MICRO_ITERS;
        s_data.ns /= MICRO_ITERS;

        // -----------------------------------------------------------
        // 3. Đo hiệu năng Xác minh (Verify)
        // -----------------------------------------------------------
        BenchResult v_data = measure([&]() {
            for (int i = 0; i < MICRO_ITERS; ++i) {
                ECDSA_verify(0, msg_hash, SHA512_DIGEST_LENGTH, sig.data(), sig_len, ec_key);
            }
        });
        v_data.cycles /= MICRO_ITERS;
        v_data.ns /= MICRO_ITERS;

        // In kết quả chi tiết
        std::cout << "   [DEBUG] Keygen : " << k_data.cycles << " cycles, " << k_data.ns << " ns\n";
        std::cout << "   [DEBUG] Sign   : " << s_data.cycles << " cycles, " << s_data.ns << " ns\n";
        std::cout << "   [DEBUG] Verify : " << v_data.cycles << " cycles, " << v_data.ns << " ns\n\n";

        // In theo đúng format CSV của đoạn code PQ để dễ xử lý log
        std::cout << "DATA:"
                  << k_data.cycles << "," << k_data.ns << ","
                  << s_data.cycles << "," << s_data.ns << ","
                  << v_data.cycles << "," << v_data.ns << std::endl;
    }

    void run_full() {
        std::cout << "=================================================" << std::endl;
        std::cout << " ECDSA secp521r1 MICROBENCHMARK - NT219 Capstone " << std::endl;
        std::cout << "=================================================" << std::endl;
        
        init_key();
        microbenchmarks();
        
        std::cout << "\n🎉 BENCHMARK COMPLETE!" << std::endl;
    }
};

int main() {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
        std::cerr << "Warning: Unable to set CPU affinity (requires Linux environment)." << std::endl;
    }
#endif

    ECDSAP521Benchmark bench;
    bench.run_full();
    return 0;
}
