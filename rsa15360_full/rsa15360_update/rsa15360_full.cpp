/**
 * RSA-15360 BENCHMARK - NT219 Capstone
 * ✅ Microbenchmarks (keygen/sign/verify)
 * ✅ Energy estimation
 * Key size: 15360-bit (NIST Level 5)
 */

#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>
#include <vector>

#define KEY_SIZE 15360
#define MSG_SIZE 1900
#define MICRO_ITERS 50
#define BATCHES 5

class RSA15360Benchmark {
private:
    RSA* rsa_key = nullptr;
    std::vector<uint8_t> message;

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
        if (rsa_key) RSA_free(rsa_key);
        ERR_free_strings();
    }

    bool init_key() {
        std::cout << "🔑 Generating RSA-15360 keypair (30-60s)..." << std::endl;
        auto start = std::chrono::steady_clock::now();
        uint64_t tsc_start = rdtsc();

        BIGNUM* bne = BN_new();
        if (!bne) return false;

        if (BN_set_word(bne, RSA_F4) != 1) {
            BN_free(bne);
            return false;
        }

        if (rsa_key) {
            RSA_free(rsa_key);
            rsa_key = nullptr;
        }
        rsa_key = RSA_new();
        bool ok = rsa_key && RSA_generate_key_ex(rsa_key, KEY_SIZE, bne, nullptr) == 1;
        BN_free(bne);

        uint64_t cycles = rdtsc() - tsc_start;
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "✅ Keygen: " << duration.count() << "ms, " << cycles / 1e6 << "M cycles" << std::endl;
        return ok;
    }

    void microbenchmarks() {
        std::cout << "\n📊 MICROBENCHMARKS (" << MICRO_ITERS << " iters x " << BATCHES << " batches)" << std::endl;

        message.resize(MSG_SIZE);
        RAND_bytes(message.data(), MSG_SIZE);

        std::vector<uint64_t> keygens, signs, verifies;

        for (int b = 0; b < BATCHES; ++b) {
            for (int i = 0; i < MICRO_ITERS; ++i) {
                uint64_t tsc = rdtsc();
                init_key();
                keygens.push_back(rdtsc() - tsc);
            }

            std::vector<uint8_t> sig(KEY_SIZE / 8);
            unsigned int sig_len = 0;
            for (int i = 0; i < MICRO_ITERS; ++i) {
                uint64_t tsc = rdtsc();
                RSA_sign(NID_sha512, message.data(), MSG_SIZE, sig.data(), &sig_len, rsa_key);
                signs.push_back(rdtsc() - tsc);
            }

            for (int i = 0; i < MICRO_ITERS; ++i) {
                uint64_t tsc = rdtsc();
                RSA_verify(NID_sha512, message.data(), MSG_SIZE, sig.data(), sig_len, rsa_key);
                verifies.push_back(rdtsc() - tsc);
            }
        }

        print_stats("Keygen", keygens);
        print_stats("Sign", signs);
        print_stats("Verify", verifies);
    }

    void energy_estimate() {
        std::cout << "\n⚡ ENERGY ESTIMATE (Intel TDP model)" << std::endl;
        rusage usage;
        getrusage(RUSAGE_SELF, &usage);

        double cpu_time_s = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6;
        double energy_j = cpu_time_s * 125.0 * 0.3;
        std::cout << "CPU time: " << cpu_time_s << "s" << std::endl;
        std::cout << "Est. energy: " << energy_j << "J (total benchmark)" << std::endl;
    }

    void print_stats(const std::string& name, const std::vector<uint64_t>& data) {
        if (data.empty()) return;
        auto sorted = data;
        std::sort(sorted.begin(), sorted.end());
        double median = sorted[sorted.size() / 2];
        std::cout << name << ": " << median / 1e6 << "M cycles (" << median / 2.4e3 << "ms) median" << std::endl;
    }

    void run() {
        std::cout << "RSA-15360 BENCHMARK - NT219 Capstone" << std::endl;
        std::cout << "Key size: " << KEY_SIZE << " bits = " << KEY_SIZE / 8 << " bytes" << std::endl;

        if (!init_key()) return;
        microbenchmarks();
        energy_estimate();

        std::cout << "\n🎉 BENCHMARK COMPLETE!" << std::endl;
    }
};

int main() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    sched_setaffinity(0, sizeof(cpuset), &cpuset);

    RSA15360Benchmark bench;
    bench.run();
    return 0;
}
