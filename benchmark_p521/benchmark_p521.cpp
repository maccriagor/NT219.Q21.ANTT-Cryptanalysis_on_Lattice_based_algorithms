/**
 * ECDSA secp521r1 COMPLETE BENCHMARK - NT219 Capstone
 * ✅ Microbenchmarks (keygen/sign/verify)
 * ✅ Memory & Accurate Binary Code size
 * ✅ Energy estimation
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
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sched.h>

#define MSG_SIZE 1900
#define MICRO_ITERS 1000    // P-521 
#define BATCHES 5

class ECDSAP521Benchmark {
private:
    EC_KEY* ec_key = nullptr;
    std::vector<uint8_t> message;
    unsigned char msg_hash[SHA512_DIGEST_LENGTH]; // SHA-512 cho NIST Level 5

    uint64_t rdtsc() {
        #if defined(__x86_64__) || defined(__i386__)
        unsigned int lo, hi;
        __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
        return ((uint64_t)hi << 32) | lo;
        #else
        return 0;
        #endif
    }

    // ECDSA chuẩn yêu cầu ký lên mã băm của thông điệp
    void compute_hash() {
        SHA512(message.data(), message.size(), msg_hash);
    }

public:
    ~ECDSAP521Benchmark() {
        if (ec_key) EC_KEY_free(ec_key);
        ERR_free_strings();
    }

    bool init_key() {
        if (ec_key) {
            EC_KEY_free(ec_key);
        }
        
        // Khởi tạo đối tượng khóa với đường cong P-521
        ec_key = EC_KEY_new_by_curve_name(NID_secp521r1);
        if (!ec_key) return false;

        // Sinh khóa Public/Private ngẫu nhiên
        bool ok = EC_KEY_generate_key(ec_key) == 1;
        return ok;
    }

    void microbenchmarks() {
        std::cout << "\n📊 MICROBENCHMARKS (" << MICRO_ITERS << " iters x " << BATCHES << " batches) " << std::endl;
        
        // Chuẩn bị dữ liệu và mã băm
        message.resize(MSG_SIZE);
        RAND_bytes(message.data(), MSG_SIZE);
        compute_hash(); 
        
        std::vector<uint64_t> keygens, signs, verifies;
        
        for (int b = 0; b < BATCHES; ++b) {
            // ---------------------------------------------------------
            // 1. Đo hiệu năng Sinh khóa (Keygen)
            // ---------------------------------------------------------
            for (int i = 0; i < MICRO_ITERS; ++i) {
                uint64_t tsc_start = rdtsc();
                init_key();  // Sinh khóa mới
                keygens.push_back(rdtsc() - tsc_start);
            }
            
            // ---------------------------------------------------------
            // 2. Đo hiệu năng Ký (Sign) sử dụng API ECDSA_sign
            // ---------------------------------------------------------
            // Kích thước chữ ký tối đa của P-521 (tính bằng byte)
            int max_sig_len = ECDSA_size(ec_key);
            std::vector<uint8_t> sig(max_sig_len);
            unsigned int sig_len = 0;

            for (int i = 0; i < MICRO_ITERS; ++i) {
                uint64_t tsc = rdtsc();
                // Sử dụng OpenSSL API chuẩn: ECDSA_sign
                ECDSA_sign(0, msg_hash, SHA512_DIGEST_LENGTH, sig.data(), &sig_len, ec_key);
                signs.push_back(rdtsc() - tsc);
            }
            
            // ---------------------------------------------------------
            // 3. Đo hiệu năng Xác minh (Verify) sử dụng API ECDSA_verify
            // ---------------------------------------------------------
            for (int i = 0; i < MICRO_ITERS; ++i) {
                uint64_t tsc = rdtsc();
                // Sử dụng OpenSSL API chuẩn: ECDSA_verify
                ECDSA_verify(0, msg_hash, SHA512_DIGEST_LENGTH, sig.data(), sig_len, ec_key);
                verifies.push_back(rdtsc() - tsc);
            }
        }
        
        print_stats("Keygen", keygens);
        print_stats("Sign  ", signs);
        print_stats("Verify", verifies);
    }

    void memory_codesize() {
        std::cout << "\n💾 MEMORY & CODE SIZE" << std::endl;
        
        // 1. Peak RSS (Bộ nhớ RAM thực tế cấp phát tối đa)
        rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        std::cout << "Peak RSS: " << usage.ru_maxrss / 1024 << " MB" << std::endl;
        
        // 2. Kích thước file thực thi (Binary Code Size) chính xác
        struct stat st;
        // Đọc thông tin file đang thực thi thông qua symlink của Linux
        if (stat("/proc/self/exe", &st) == 0) {
            std::cout << "Binary Size: " << st.st_size << " bytes (" << st.st_size / 1024 << " KB)" << std::endl;
        } else {
            std::cout << "Binary Size: Không thể đọc được." << std::endl;
        }
        
        // 3. Kích thước Khóa và Chữ ký
        if (ec_key) {
            int priv_len = i2d_ECPrivateKey(ec_key, nullptr);
            int pub_len  = i2o_ECPublicKey(ec_key, nullptr);
            int sig_max_len = ECDSA_size(ec_key);
            std::cout << "Public Key size: " << pub_len << " bytes" << std::endl;
            std::cout << "Private Key size: " << priv_len << " bytes" << std::endl;
            std::cout << "Signature max size: " << sig_max_len << " bytes" << std::endl;
        }
    }

    void energy_estimate() {
        std::cout << "\n⚡ ENERGY ESTIMATE (Intel TDP model)" << std::endl;
        rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        
        // Tính toán dựa trên System + User CPU time
        double cpu_time_s = usage.ru_utime.tv_sec + (usage.ru_utime.tv_usec / 1e6) +
                            usage.ru_stime.tv_sec + (usage.ru_stime.tv_usec / 1e6);
        // Giả định vi xử lý chạy TDP 125W ở 30% load
        double energy_j = cpu_time_s * 125.0 * 0.3;  
        
        std::cout << "CPU time used: " << std::fixed << std::setprecision(4) << cpu_time_s << " s" << std::endl;
        std::cout << "Estimated energy: " << energy_j << " Joules" << std::endl;
    }

    // Hàm in kết quả Benchmark
    void print_stats(const std::string& name, std::vector<uint64_t>& data) {
        if (data.empty()) return;
        // Sắp xếp để lấy Median (trung vị), giúp loại bỏ nhiễu của hệ điều hành
        std::sort(data.begin(), data.end());
        double median_cycles = data[data.size() / 2];
        
        double median_ms = median_cycles / 2.4e6; 
        
        std::cout << "- " << name << ": " << std::fixed << std::setprecision(3) 
                  << (median_cycles / 1e6) << " M cycles (" 
                  << median_ms << " ms) median" << std::endl;
    }

    void run_full() {
        std::cout << "=================================================" << std::endl;
        std::cout << " ECDSA secp521r1 FULL BENCHMARK - NT219 Capstone " << std::endl;
        std::cout << "=================================================" << std::endl;
        std::cout << "Curve: NIST P-521 (NIST Level 5 Security)" << std::endl;
        
        std::cout << "🔑 Generating initial P-521 keypair..." << std::endl;
        if (!init_key()) {
            std::cout << "❌ Keygen failed!" << std::endl;
            return;
        }
        std::cout << "✅ Keygen successful." << std::endl;
        
        microbenchmarks();
        memory_codesize();
        energy_estimate();
        
        std::cout << "\n🎉 FULL BENCHMARK COMPLETE!" << std::endl;
    }
};

int main() {
    // KỸ THUẬT CPU PINNING: Ghim tiến trình vào CPU Core 0
    // Đảm bảo cache L1/L2 không bị trượt do OS chuyển tiến trình sang core khác
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
        std::cerr << "Warning: Unable to set CPU affinity (requires Linux environment)." << std::endl;
    }
    ECDSAP521Benchmark bench;
    bench.run_full();
    return 0;
}