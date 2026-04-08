/**
 * RSA-15360 COMPLETE BENCHMARK - NT219 Capstone
 * ✅ Microbenchmarks (keygen/sign/verify)
 * ✅ Macrobenchmarks (TLS simulation)  
 * ✅ Memory/Code size
 * ✅ Energy estimation
 * Key size: 15360-bit (NIST Level 5)
 */

#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <sys/resource.h>
#include <unistd.h>
#include <sched.h>
#include <fstream>

#define KEY_SIZE 15360
#define MSG_SIZE 1900
#define MICRO_ITERS 50      // RSA-15360 siêu chậm
#define MACRO_ITERS 10      // TLS handshakes
#define BATCHES 5

class RSA15360FullBenchmark {
private:
    RSA* rsa_key = nullptr;
    SSL_CTX* ssl_ctx = nullptr;
    std::vector<uint8_t> message;

    uint64_t rdtsc() {
        #ifdef __x86_64__
        unsigned int lo, hi;
        __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
        return ((uint64_t)hi << 32) | lo;
        #else
        return 0;
        #endif
    }

public:
    ~RSA15360FullBenchmark() {
        if (rsa_key) RSA_free(rsa_key);
        if (ssl_ctx) SSL_CTX_free(ssl_ctx);
        ERR_free_strings();
    }

    bool init_key() {
        std::cout << "🔑 Generating RSA-15360 keypair (30-60s)..." << std::endl;
        auto start = std::chrono::steady_clock::now();
        uint64_t tsc_start = rdtsc();

        BIGNUM* bne = BN_new();
        BN_set_word(bne, RSA_F4);
        rsa_key = RSA_new();
        
        bool ok = RSA_generate_key_ex(rsa_key, KEY_SIZE, bne, nullptr) == 1;
        BN_free(bne);

        uint64_t cycles = rdtsc() - tsc_start;
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "✅ Keygen: " << duration.count() << "ms, " 
                  << cycles/1e6 << "M cycles" << std::endl;
        return ok;
    }

    void microbenchmarks() {
        std::cout << "\n📊 MICROBENCHMARKS (" << MICRO_ITERS << " iters x " << BATCHES << " batches) " << std::endl;
        
        message.resize(MSG_SIZE);
        RAND_bytes(message.data(), MSG_SIZE);
        
        std::vector<uint64_t> keygens, signs, verifies;
        
        for (int b = 0; b < BATCHES; ++b) {
            // Keygen batch
            for (int i = 0; i < MICRO_ITERS; ++i) {
                uint64_t cycles;
                init_key();  // Regenerate mỗi lần
                keygens.push_back(cycles);
            }
            
            // Sign batch
            std::vector<uint8_t> sig(KEY_SIZE/8);
            unsigned int sig_len;
            for (int i = 0; i < MICRO_ITERS; ++i) {
                uint64_t tsc = rdtsc();
                RSA_sign(EVP_sha512(), message.data(), MSG_SIZE, sig.data(), &sig_len, rsa_key);
                signs.push_back(rdtsc() - tsc);
            }
            
            // Verify batch
            for (int i = 0; i < MICRO_ITERS; ++i) {
                uint64_t tsc = rdtsc();
                RSA_verify(EVP_sha512(), message.data(), MSG_SIZE, sig.data(), sig_len, rsa_key);
                verifies.push_back(rdtsc() - tsc);
            }
        }
        
        print_stats("Keygen", keygens);
        print_stats("Sign", signs);
        print_stats("Verify", verifies);
    }

    void macrobenchmarks_tls() {
        std::cout << "\n🤝 MACROBENCHMARKS - TLS Handshake Simulation (" 
                  << MACRO_ITERS << " handshakes)" << std::endl;
        
        std::vector<double> handshake_times;
        for (int i = 0; i < MACRO_ITERS; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            
            // Simulate TLS handshake với RSA cert
            uint64_t dummy_cycles = rdtsc();
            std::vector<uint8_t> sig;
            sign_message(message, sig);  // RSA signature trong handshake
            verify_message(message, sig); // Client verify
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            handshake_times.push_back(duration.count());
        }
        
        print_stats_ms("TLS Handshake Latency", handshake_times);
        std::cout << "Throughput: " << (1000.0 / median_ms(handshake_times)) << " HPS" << std::endl;
    }

    void memory_codesize() {
        std::cout << "\n💾 MEMORY & CODE SIZE" << std::endl;
        
        // Peak RSS
        rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        std::cout << "Peak RSS: " << usage.ru_maxrss/1024 << " MB" << std::endl;
        
        // Binary size
        FILE* pipe = popen("size rsa_15360 2>/dev/null || echo 'N/A'", "r");
        char buffer[128];
        std::cout << "Binary size:\n";
        while (fgets(buffer, 128, pipe) != NULL) {
            std::cout << buffer;
        }
        pclose(pipe);
        
        // Key sizes
        int pub_len = i2d_RSAPublicKey(rsa_key, nullptr);
        int priv_len = i2d_RSAPrivateKey(rsa_key, nullptr);
        std::cout << "Pubkey: " << pub_len << "B, Privkey: " << priv_len << "B" << std::endl;
    }

    void energy_estimate() {
        std::cout << "\n⚡ ENERGY ESTIMATE (Intel TDP model)" << std::endl;
        rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        
        // Giả sử 125W TDP, 3GHz
        double cpu_time_s = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6;
        double energy_j = cpu_time_s * 125.0 * 0.3;  // 30% utilization
        std::cout << "CPU time: " << cpu_time_s << "s" << std::endl;
        std::cout << "Est. energy: " << energy_j << "J (Keygen)" << std::endl;
    }

    // Helpers
    void print_stats(const std::string& name, const std::vector<uint64_t>& data) {
        if (data.empty()) return;
        auto sorted = data;
        std::sort(sorted.begin(), sorted.end());
        double median = sorted[sorted.size()/2];
        std::cout << name << ": " << median/1e6 << "M cycles (" 
                  << median/2.4e3 << "ms) median" << std::endl;
    }
    
    void print_stats_ms(const std::string& name, const std::vector<double>& data) {
        if (data.empty()) return;
        auto sorted = data;
        std::sort(sorted.begin(), sorted.end());
        double median = sorted[sorted.size()/2];
        std::cout << name << ": " << median << "ms median (95th: " 
                  << sorted[(int)(sorted.size()*0.95)] << "ms)" << std::endl;
    }
    
    double median_ms(const std::vector<double>& data) {
        auto sorted = data;
        std::sort(sorted.begin(), sorted.end());
        return sorted[sorted.size()/2];
    }
    
    void sign_message(const std::vector<uint8_t>& msg, std::vector<uint8_t>& sig) {
        unsigned int sig_len;
        sig.resize(KEY_SIZE/8);
        RSA_sign(EVP_sha512(), msg.data(), msg.size(), sig.data(), &sig_len, rsa_key);
    }
    
    bool verify_message(const std::vector<uint8_t>& msg, const std::vector<uint8_t>& sig) {
        unsigned int sig_len = sig.size();
        return RSA_verify(EVP_sha512(), msg.data(), msg.size(), sig.data(), sig_len, rsa_key) == 1;
    }

    void run_full() {
        std::cout << "RSA-15360 FULL BENCHMARK - NT219 Capstone" << std::endl;
        std::cout << "Key size: " << KEY_SIZE << " bits = " << KEY_SIZE/8 << " bytes" << std::endl;
        
        if (!init_key()) return;
        
        microbenchmarks();
        macrobenchmarks_tls();
        memory_codesize();
        energy_estimate();
        
        std::cout << "\n🎉 FULL BENCHMARK COMPLETE!" << std::endl;
    }
};

int main() {
    // CPU pinning
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    sched_setaffinity(0, sizeof(cpuset), &cpuset);
    
    RSA15360FullBenchmark bench;
    bench.run_full();
    return 0;
}