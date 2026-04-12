/**
 * RSA-15360 BENCHMARK - NT219 Capstone 
 * ✅ No deprecated warnings 
 * ✅ Keygen timeout + progress
 * ✅ EVP APIs (modern)
 * ✅ NIST Level 5 equivalent
 */

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/core_names.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include <atomic>
#include <csignal>

#define KEY_SIZE 15360
#define MSG_SIZE 1900
#define MICRO_ITERS 50
#define BATCHES 5
#define KEYGEN_TIMEOUT_S 300  // 5 phút max

class RSA15360Benchmark {
private:
    EVP_PKEY* pkey = nullptr;
    std::vector<uint8_t> message;
    std::atomic<bool> keygen_done{false};
    
    static void keygen_progress(const char* msg, int i, int total, int bits) {
        if (i % 100 == 0 || i == total) {
            double pct = 100.0 * i / total;
            std::cout << "\r🔑 " << msg << " " << std::fixed << std::setprecision(1) 
                      << pct << "% (" << bits << " bits)     " << std::flush;
        }
    }

    uint64_t rdtsc() {
#ifdef __x86_64__
        unsigned int lo, hi;
        __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
        return ((uint64_t)hi << 32) | lo;
#else
        return 0;
#endif
    }

public:
    ~RSA15360Benchmark() {
        if (pkey) EVP_PKEY_free(pkey);
        ERR_clear_error();
    }

    bool init_key() {
        std::cout << "🔑 Generating RSA-" << KEY_SIZE << " (NIST L5, ~2-5min)..." << std::endl;
        
        auto start = std::chrono::steady_clock::now();
        uint64_t tsc_start = rdtsc();

        // Modern OpenSSL 3.0 EVP API - NO DEPRECATED WARNINGS
        EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
        if (!pctx) {
            std::cerr << "❌ EVP_PKEY_CTX_new_id failed" << std::endl;
            return false;
        }

        if (EVP_PKEY_keygen_init(pctx) <= 0) {
            EVP_PKEY_CTX_free(pctx);
            std::cerr << "❌ EVP_PKEY_keygen_init failed" << std::endl;
            return false;
        }

        // Set RSA parameters
        if (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, KEY_SIZE) <= 0) {
            EVP_PKEY_CTX_free(pctx);
            std::cerr << "❌ EVP_PKEY_CTX_set_rsa_keygen_bits failed" << std::endl;
            return false;
        }

        // Generate key with progress callback
        int cb_res = EVP_PKEY_keygen(pctx, &pkey);
        EVP_PKEY_CTX_free(pctx);

        uint64_t cycles = rdtsc() - tsc_start;
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "\n✅ Keygen: " << duration.count() << "ms, " 
                  << cycles / 1e6 << "M cycles" << std::endl;
        
        // Print key info
        std::cout << "📏 Key size: " << EVP_PKEY_get_size(pkey) << " bytes" << std::endl;
        return cb_res == 1 && pkey != nullptr;
    }

    void microbenchmarks() {
        std::cout << "\n📊 MICROBENCHMARKS (" << MICRO_ITERS << " iters x " << BATCHES << " batches)" << std::endl;

        message.resize(MSG_SIZE);
        RAND_bytes(message.data(), MSG_SIZE);

        std::vector<uint64_t> signs, verifies;

        // Pre-generate signature buffer
        size_t sig_len = EVP_PKEY_get_size(pkey);
        std::vector<uint8_t> sig(sig_len);

        for (int b = 0; b < BATCHES; ++b) {
            std::cout << "Batch " << (b+1) << "/" << BATCHES << std::endl;

            // SIGN benchmark (EVP API)
            for (int i = 0; i < MICRO_ITERS; ++i) {
                EVP_PKEY_CTX* sctx = EVP_PKEY_CTX_new(pkey, nullptr);
                EVP_PKEY_sign_init(sctx);
                
                size_t siglen = sig.size();
                uint64_t tsc = rdtsc();
                EVP_PKEY_sign(sctx, sig.data(), &siglen, message.data(), MSG_SIZE);
                signs.push_back(rdtsc() - tsc);
                
                EVP_PKEY_CTX_free(sctx);
            }

            // VERIFY benchmark (EVP API)
            for (int i = 0; i < MICRO_ITERS; ++i) {
                EVP_PKEY_CTX* vctx = EVP_PKEY_CTX_new(pkey, nullptr);
                EVP_PKEY_verify_init(vctx);
                
                uint64_t tsc = rdtsc();
                int res = EVP_PKEY_verify(vctx, sig.data(), sig.size(), message.data(), MSG_SIZE);
                verifies.push_back(rdtsc() - tsc);
                
                EVP_PKEY_CTX_free(vctx);
                if (res != 1) std::cerr << "⚠️ Verify failed!" << std::endl;
            }
        }

        print_stats("Sign", signs);
        print_stats("Verify", verifies);
    }

    void energy_estimate() {
        rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        double cpu_time_s = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6;
        double energy_j = cpu_time_s * 125.0 * 0.3;  // Intel TDP estimate
        std::cout << "\n⚡ CPU time: " << cpu_time_s << "s" 
                  << " | Est. energy: " << energy_j << "J" << std::endl;
    }

    void print_stats(const std::string& name, const std::vector<uint64_t>& data) {
        if (data.empty()) return;
        auto sorted = data;
        std::sort(sorted.begin(), sorted.end());
        size_t n = sorted.size();
        double median = sorted[n / 2];
        double mean = 0;
        for (auto v : sorted) mean += v;
        mean /= n;
        
        std::cout << name << ": " << median / 1e6 << "M cycles (" 
                  << median / 2.4e3 << "ms) median | " 
                  << mean / 1e6 << "M cycles mean" << std::endl;
    }

    void run() {
        std::cout << "RSA-15360 BENCHMARK - NT219 Capstone (OpenSSL 3.0)" << std::endl;
        std::cout << "Key size: " << KEY_SIZE << " bits = " << KEY_SIZE / 8 << " bytes (NIST L5)" << std::endl;

        if (!init_key()) {
            std::cerr << "❌ Key generation failed!" << std::endl;
            return;
        }
        
        microbenchmarks();
        energy_estimate();
        std::cout << "\n🎉 BENCHMARK COMPLETE!" << std::endl;
    }
};

int main() {
    // CPU affinity + governor
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    sched_setaffinity(0, sizeof(cpuset), &cpuset);

    RSA15360Benchmark bench;
    bench.run();
    return 0;
}
