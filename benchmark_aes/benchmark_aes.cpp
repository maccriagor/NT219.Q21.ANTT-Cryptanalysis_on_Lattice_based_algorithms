/**
 * AES-GCM (AEAD) MICROBENCHMARK ĐA MỨC NIST — NT219 Capstone
 * ---------------------------------------------------------------------------
 * Lớp mã hoá dữ liệu đối xứng của giao thức hybrid. Quét 3 mức khoá khớp Category:
 *     AES-128-GCM (Cat 1) · AES-192-GCM (Cat 3) · AES-256-GCM (Cat 5)
 * (Grover làm giảm 1/2 độ an toàn đối xứng → AES-256 ≈ 128-bit hậu lượng tử = Cat 5,
 *  khớp ML-KEM-1024 / ML-DSA-87.)
 * Đo: Encrypt (E) + Decrypt-có-xác-thực-tag (D) trên payload MSG_SIZE byte.
 * Thư viện: OpenSSL 3.x EVP. Cổng đúng đắn: roundtrip khớp + tag hỏng bị từ chối.
 *
 * Dùng:   ./benchmark_aes <128|192|256>
 * Output: "DATA:E_cyc,E_ns,D_cyc,D_ns"
 */

#include <iostream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <string>
#include <openssl/evp.h>
#include <openssl/rand.h>
#ifdef __linux__
#include <sched.h>
#endif

#define MSG_SIZE 1900

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

static bool gcm_encrypt(const EVP_CIPHER* cph, const unsigned char* key, const unsigned char* pt, int pt_len,
                        unsigned char* ct, unsigned char* iv, unsigned char* tag) {
    EVP_CIPHER_CTX* c = EVP_CIPHER_CTX_new(); if (!c) return false;
    int len;
    bool ok = RAND_bytes(iv, 12) == 1
           && EVP_EncryptInit_ex(c, cph, NULL, NULL, NULL) == 1
           && EVP_CIPHER_CTX_ctrl(c, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) == 1
           && EVP_EncryptInit_ex(c, NULL, NULL, key, iv) == 1
           && EVP_EncryptUpdate(c, ct, &len, pt, pt_len) == 1
           && EVP_EncryptFinal_ex(c, ct + len, &len) == 1
           && EVP_CIPHER_CTX_ctrl(c, EVP_CTRL_GCM_GET_TAG, 16, tag) == 1;
    EVP_CIPHER_CTX_free(c); return ok;
}
static bool gcm_decrypt(const EVP_CIPHER* cph, const unsigned char* key, const unsigned char* ct, int ct_len,
                        const unsigned char* iv, const unsigned char* tag, unsigned char* pt) {
    EVP_CIPHER_CTX* c = EVP_CIPHER_CTX_new(); if (!c) return false;
    int len, ret = -1;
    bool init = EVP_DecryptInit_ex(c, cph, NULL, NULL, NULL) == 1
             && EVP_CIPHER_CTX_ctrl(c, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) == 1
             && EVP_DecryptInit_ex(c, NULL, NULL, key, iv) == 1
             && EVP_DecryptUpdate(c, pt, &len, ct, ct_len) == 1
             && EVP_CIPHER_CTX_ctrl(c, EVP_CTRL_GCM_SET_TAG, 16, (void*)tag) == 1;
    if (init) ret = EVP_DecryptFinal_ex(c, pt + len, &len);
    EVP_CIPHER_CTX_free(c); return init && ret > 0;
}

int main(int argc, char** argv) {
#ifdef __linux__
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(0, &cs);
    if (sched_setaffinity(0, sizeof(cs), &cs) != 0) std::cerr << "[warn] CPU affinity\n";
#endif
    std::string want = (argc > 1) ? argv[1] : "256";
    int keybits; const EVP_CIPHER* cph;
    if      (want == "128") { keybits = 128; cph = EVP_aes_128_gcm(); }
    else if (want == "192") { keybits = 192; cph = EVP_aes_192_gcm(); }
    else if (want == "256") { keybits = 256; cph = EVP_aes_256_gcm(); }
    else { std::cerr << "usage: benchmark_aes <128|192|256>\n"; return 2; }

    std::vector<unsigned char> key(keybits / 8);
    RAND_bytes(key.data(), key.size());
    std::vector<unsigned char> pt(MSG_SIZE), ct(MSG_SIZE), dec(MSG_SIZE);
    RAND_bytes(pt.data(), MSG_SIZE);
    unsigned char iv[12], tag[16];

    bool enc_ok = false, dec_ok = false;
    BenchResult e = measure([&]{ enc_ok = gcm_encrypt(cph, key.data(), pt.data(), MSG_SIZE, ct.data(), iv, tag); });
    BenchResult d = measure([&]{ dec_ok = gcm_decrypt(cph, key.data(), ct.data(), MSG_SIZE, iv, tag, dec.data()); });

    unsigned char bad[16]; std::memcpy(bad, tag, 16); bad[0] ^= 0xFF;
    bool tamper_rejected = !gcm_decrypt(cph, key.data(), ct.data(), MSG_SIZE, iv, bad, dec.data());
    if (!enc_ok || !dec_ok || std::memcmp(pt.data(), dec.data(), MSG_SIZE) != 0 || !tamper_rejected) {
        std::cerr << "[CORRECTNESS FAIL] AES-" << keybits << "-GCM enc=" << enc_ok
                  << " dec=" << dec_ok << " tamper_rejected=" << tamper_rejected << "\n";
        return 1;
    }
    std::cout << "DATA:" << e.cycles << "," << e.ns << "," << d.cycles << "," << d.ns << std::endl;
    return 0;
}
