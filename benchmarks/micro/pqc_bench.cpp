// pqc_bench.cpp — Microbenchmark PQC vs cổ điển, dùng OpenSSL 3.6.x EVP (ML-KEM/ML-DSA NATIVE).
// ------------------------------------------------------------------------------------------------
// Thiết kế khoa học (bám file 05 §7.4-7.5 + SUPERCOP/eBACS + paper so sánh PQC-TLS):
//   * So sánh theo MỨC BẢO MẬT NIST khớp nhau (Category 1/3/5):
//       Cat1: ML-KEM-512 | ECDH/ECDSA P-256 | RSA-3072
//       Cat3: ML-KEM-768 | ML-DSA-65 | P-384 | RSA-7680
//       Cat5: ML-KEM-1024| ML-DSA-87 | P-521 | RSA-15360   (ML-DSA-44 = Cat2)
//   * Cấu hình tham số đầu vào qua CLI (suite/level/iters/warmup/batches/maxms/csv).
//   * Thống kê: B batch x M vòng -> median-of-medians, 95% CI (t), mean, stddev, p95, min.
//   * CỔNG ĐÚNG ĐẮN (tránh "failure in crypto"): roundtrip phải đúng trước khi tính giờ
//       (KEM: ss_encaps==ss_decaps; SIG: verify OK + chữ ký hỏng bị từ chối; KEX: 2 bên ra cùng ss).
//   * Đo kích thước byte (pk/sk/ct|sig) cho phần network-overhead (§9).
//   * Ngân sách thời gian/op (maxms) -> op chậm (RSA-15360 keygen) không treo.
// API tham chiếu tài liệu chính thống:
//   https://docs.openssl.org/3.5/man3/EVP_PKEY_Q_keygen/
//   https://docs.openssl.org/3.5/man3/EVP_PKEY_encapsulate/  (ML-KEM)
//   https://docs.openssl.org/3.5/man3/EVP_DigestSign/        (ML-DSA, md=NULL = pure)
//   https://docs.openssl.org/3.5/man3/EVP_PKEY_derive/       (ECDH)
// ------------------------------------------------------------------------------------------------
// Build (MinGW, OpenSSL 3.6.2 ở F:\cryptolibrary):
//   g++ -O2 -std=c++17 pqc_bench.cpp -o pqc_bench.exe \
//       -IF:\cryptolibrary\include -LF:\cryptolibrary\lib64 \
//       -l:libssl.a -l:libcrypto.a -lws2_32 -lcrypt32
// Build (Linux/Docker, OpenSSL 3.6.2 ở /opt/openssl):
//   g++ -O2 -std=c++17 pqc_bench.cpp -o pqc_bench \
//       -I/opt/openssl/include -L/opt/openssl/lib -lssl -lcrypto
// ------------------------------------------------------------------------------------------------
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <ctime>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <functional>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/core_names.h>
#include <openssl/x509.h>   // i2d_PUBKEY, i2d_PrivateKey

// ---------------- đồng hồ phân giải cao ----------------
#if defined(_WIN32)
  #include <windows.h>
  static uint64_t now_ns() {
      static LARGE_INTEGER f = [] { LARGE_INTEGER x; QueryPerformanceFrequency(&x); return x; }();
      LARGE_INTEGER c; QueryPerformanceCounter(&c);
      return (uint64_t)((long double)c.QuadPart * 1e9L / (long double)f.QuadPart);
  }
#else
  static uint64_t now_ns() {
      struct timespec ts; clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
      return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
  }
#endif

// ---------------- cấu hình (CLI) ----------------
struct Config {
    std::string suite = "all";   // kem|sig|kex|all
    std::string level = "all";   // 1|2|3|5|all
    long iters   = 1000;         // tổng số vòng đo / op
    long warmup  = 100;          // vòng warm-up (loại khỏi thống kê)
    int  batches = 10;           // số batch (cho median-of-medians + CI)
    double maxms = 3000.0;       // ngân sách ms / op (chống treo op chậm)
    std::string csv = "";        // file CSV (rỗng = stdout)
    std::string only = "";       // lọc tên thuật toán (substring), rỗng = tất cả
};

// ---------------- thống kê ----------------
static double t975(int df) { // t-value 95% 2 phía theo bậc tự do (xấp xỉ, fallback 1.96)
    static const double tb[] = {0,12.71,4.30,3.18,2.78,2.57,2.45,2.36,2.31,2.26,2.23,
                                2.20,2.18,2.16,2.14,2.13,2.12,2.11,2.10,2.09,2.09};
    if (df >= 1 && df <= 20) return tb[df];
    return 1.96;
}
struct Stat {
    double median, ci_lo, ci_hi, mean, stddev, p95;
    uint64_t mn; long count;
};
static Stat summarize(std::vector<uint64_t>& all, std::vector<double>& batch_medians) {
    std::sort(all.begin(), all.end());
    const size_t n = all.size();
    Stat s{}; s.count = (long)n;
    if (n == 0) return s;
    double mean = 0; for (uint64_t x : all) mean += (double)x; mean /= (double)n;
    double var = 0;  for (uint64_t x : all) { double d=(double)x-mean; var+=d*d; } var /= (double)n;
    s.mean = mean; s.stddev = std::sqrt(var);
    s.p95 = (double)all[(size_t)(0.95*(double)n)]; s.mn = all.front();
    // median-of-medians + 95% CI từ batch medians (t-distribution)
    std::sort(batch_medians.begin(), batch_medians.end());
    const int B = (int)batch_medians.size();
    if (B >= 2) {
        double mm = 0; for (double x : batch_medians) mm += x; mm /= B;
        double bv = 0; for (double x : batch_medians) { double d=x-mm; bv+=d*d; } bv /= (B-1);
        double sd = std::sqrt(bv), half = t975(B-1) * sd / std::sqrt((double)B);
        s.median = batch_medians[B/2];
        s.ci_lo = std::max(0.0, mm - half); s.ci_hi = mm + half;  // latency không âm
    } else {
        s.median = (double)all[n/2]; s.ci_lo = s.ci_hi = s.median;
    }
    return s;
}

// Đo 1 thao tác: op() trả true nếu thành công. B batch, mỗi batch tới khi đủ M vòng HOẶC hết maxms.
static Stat measure(const Config& c, const std::function<bool()>& op) {
    for (long i = 0; i < c.warmup; i++) if (!op()) return Stat{};
    const long M = std::max(1L, c.iters / std::max(1, c.batches));
    const double budget_ns = c.maxms * 1e6;
    std::vector<uint64_t> all; all.reserve(c.iters);
    std::vector<double> bmed;
    uint64_t t_start = now_ns();
    for (int b = 0; b < c.batches; b++) {
        std::vector<uint64_t> batch; batch.reserve(M);
        for (long i = 0; i < M; i++) {
            uint64_t t0 = now_ns();
            bool ok = op();
            uint64_t t1 = now_ns();
            if (!ok) return Stat{};
            batch.push_back(t1 - t0);
            if ((double)(now_ns() - t_start) > budget_ns) break;  // hết ngân sách
        }
        if (batch.empty()) break;
        std::sort(batch.begin(), batch.end());
        bmed.push_back((double)batch[batch.size()/2]);
        all.insert(all.end(), batch.begin(), batch.end());
        if ((double)(now_ns() - t_start) > budget_ns) break;
    }
    return summarize(all, bmed);
}

// ---------------- OpenSSL helpers ----------------
static EVP_PKEY* kg_named(const char* type) { return EVP_PKEY_Q_keygen(nullptr, nullptr, type); }
static EVP_PKEY* kg_rsa(size_t bits)        { return EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", bits); }
static EVP_PKEY* kg_ec(const char* curve)   { return EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", curve); }

// kích thước public key (byte): thử raw param (PQC) -> fallback DER SubjectPublicKeyInfo
static long pub_bytes(EVP_PKEY* k) {
    size_t l = 0;
    if (EVP_PKEY_get_octet_string_param(k, OSSL_PKEY_PARAM_PUB_KEY, nullptr, 0, &l) == 1 && l > 0)
        return (long)l;
    int d = i2d_PUBKEY(k, nullptr);
    return d > 0 ? (long)d : -1;
}
static long priv_bytes(EVP_PKEY* k) { int d = i2d_PrivateKey(k, nullptr); return d > 0 ? (long)d : -1; }

// ---------------- mô tả thuật toán ----------------
enum Kind { KEM, SIG, KEX };
struct Algo {
    std::string name; const char* cat; Kind kind;
    std::function<EVP_PKEY*()> keygen;
    const char* md;   // SIG: "SHA256" cho RSA/ECDSA, nullptr cho ML-DSA (pure)
};

// ---------------- in CSV ----------------
static FILE* g_out = stdout;
static void row(const std::string& cat, const std::string& algo, const char* suite, const char* op,
                const Stat& s, long pk, long sk, long extra) {
    fprintf(g_out, "%s,%s,%s,%s,%ld,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%llu,%ld,%ld,%ld\n",
            cat.c_str(), algo.c_str(), suite, op, s.count, s.median, s.ci_lo, s.ci_hi,
            s.mean, s.stddev, s.p95, (unsigned long long)s.mn, pk, sk, extra);
    fflush(g_out);
}

// ---------------- KEM (ML-KEM) ----------------
static bool run_kem(const Config& c, const Algo& a) {
    EVP_PKEY* key = a.keygen(); if (!key) { fprintf(stderr, "[skip] keygen fail %s\n", a.name.c_str()); return false; }
    // cổng đúng đắn: encaps -> decaps -> ss khớp
    EVP_PKEY_CTX* ec = EVP_PKEY_CTX_new_from_pkey(nullptr, key, nullptr);
    size_t ctlen=0, sslen=0;
    if (EVP_PKEY_encapsulate_init(ec, nullptr)!=1 || EVP_PKEY_encapsulate(ec,nullptr,&ctlen,nullptr,&sslen)!=1)
        { fprintf(stderr,"[skip] encaps init %s\n",a.name.c_str()); EVP_PKEY_CTX_free(ec); EVP_PKEY_free(key); return false; }
    std::vector<uint8_t> ct(ctlen), ss1(sslen), ss2(sslen);
    EVP_PKEY_encapsulate(ec, ct.data(), &ctlen, ss1.data(), &sslen);
    EVP_PKEY_CTX* dc = EVP_PKEY_CTX_new_from_pkey(nullptr, key, nullptr);
    size_t s2 = ss2.size();
    EVP_PKEY_decapsulate_init(dc, nullptr);
    EVP_PKEY_decapsulate(dc, ss2.data(), &s2, ct.data(), ctlen);
    if (s2 != sslen || memcmp(ss1.data(), ss2.data(), sslen) != 0) {
        fprintf(stderr, "[CORRECTNESS FAIL] %s shared secret mismatch\n", a.name.c_str());
        EVP_PKEY_CTX_free(ec); EVP_PKEY_CTX_free(dc); EVP_PKEY_free(key); return false;
    }
    long pk = pub_bytes(key), sk = priv_bytes(key);
    EVP_PKEY_CTX_free(ec); EVP_PKEY_CTX_free(dc); EVP_PKEY_free(key);

    Stat s;
    s = measure(c, [&]{ EVP_PKEY* k=a.keygen(); if(!k)return false; EVP_PKEY_free(k); return true; });
    row(a.cat, a.name, "kem", "keygen", s, pk, sk, (long)ctlen);
    {
        EVP_PKEY* k = a.keygen();
        EVP_PKEY_CTX* e = EVP_PKEY_CTX_new_from_pkey(nullptr,k,nullptr); EVP_PKEY_encapsulate_init(e,nullptr);
        std::vector<uint8_t> C(ctlen), S(sslen);
        s = measure(c, [&]{ size_t cl=ctlen, sl=sslen; return EVP_PKEY_encapsulate(e,C.data(),&cl,S.data(),&sl)==1; });
        row(a.cat, a.name, "kem", "encaps", s, pk, sk, (long)ctlen);
        // decaps cần 1 ct hợp lệ
        size_t cl=ctlen, sl=sslen; EVP_PKEY_encapsulate(e,C.data(),&cl,S.data(),&sl);
        EVP_PKEY_CTX* d = EVP_PKEY_CTX_new_from_pkey(nullptr,k,nullptr); EVP_PKEY_decapsulate_init(d,nullptr);
        std::vector<uint8_t> S2(sslen);
        s = measure(c, [&]{ size_t s2l=S2.size(); return EVP_PKEY_decapsulate(d,S2.data(),&s2l,C.data(),cl)==1; });
        row(a.cat, a.name, "kem", "decaps", s, pk, sk, (long)ctlen);
        EVP_PKEY_CTX_free(e); EVP_PKEY_CTX_free(d); EVP_PKEY_free(k);
    }
    return true;
}

// ---------------- SIG (ML-DSA, RSA, ECDSA) ----------------
static bool run_sig(const Config& c, const Algo& a) {
    EVP_PKEY* key = a.keygen(); if (!key) { fprintf(stderr,"[skip] keygen fail %s\n",a.name.c_str()); return false; }
    uint8_t msg[32]; RAND_bytes(msg, sizeof msg);
    // cổng đúng đắn: sign -> verify OK; chữ ký hỏng -> verify FAIL
    std::vector<uint8_t> sig(EVP_PKEY_get_size(key) + 64); size_t siglen = sig.size();
    EVP_MD_CTX* mc = EVP_MD_CTX_new();
    bool ok = EVP_DigestSignInit_ex(mc,nullptr,a.md,nullptr,nullptr,key,nullptr)==1
           && EVP_DigestSign(mc, sig.data(), &siglen, msg, sizeof msg)==1;
    EVP_MD_CTX_free(mc);
    if (!ok) { fprintf(stderr,"[skip] sign fail %s\n",a.name.c_str()); EVP_PKEY_free(key); return false; }
    mc = EVP_MD_CTX_new();
    int v = (EVP_DigestVerifyInit_ex(mc,nullptr,a.md,nullptr,nullptr,key,nullptr)==1)
            ? EVP_DigestVerify(mc, sig.data(), siglen, msg, sizeof msg) : -1;
    EVP_MD_CTX_free(mc);
    std::vector<uint8_t> bad = sig; bad[bad.size()/2] ^= 0xFF;  // làm hỏng chữ ký
    mc = EVP_MD_CTX_new();
    int vb = (EVP_DigestVerifyInit_ex(mc,nullptr,a.md,nullptr,nullptr,key,nullptr)==1)
            ? EVP_DigestVerify(mc, bad.data(), siglen, msg, sizeof msg) : -1;
    EVP_MD_CTX_free(mc);
    if (v != 1 || vb == 1) {
        fprintf(stderr, "[CORRECTNESS FAIL] %s verify=%d tampered=%d\n", a.name.c_str(), v, vb);
        EVP_PKEY_free(key); return false;
    }
    long pk = pub_bytes(key), sk = priv_bytes(key);
    EVP_PKEY_free(key);

    Stat s;
    s = measure(c, [&]{ EVP_PKEY* k=a.keygen(); if(!k)return false; EVP_PKEY_free(k); return true; });
    row(a.cat, a.name, "sig", "keygen", s, pk, sk, (long)siglen);
    {
        EVP_PKEY* k = a.keygen();
        std::vector<uint8_t> S(sig.size()); size_t sl=0;
        s = measure(c, [&]{
            EVP_MD_CTX* m=EVP_MD_CTX_new(); size_t l=S.size();
            bool r = EVP_DigestSignInit_ex(m,nullptr,a.md,nullptr,nullptr,k,nullptr)==1
                  && EVP_DigestSign(m,S.data(),&l,msg,sizeof msg)==1;
            if (r) sl=l; EVP_MD_CTX_free(m); return r;
        });
        row(a.cat, a.name, "sig", "sign", s, pk, sk, (long)sl);
        s = measure(c, [&]{
            EVP_MD_CTX* m=EVP_MD_CTX_new();
            bool r = EVP_DigestVerifyInit_ex(m,nullptr,a.md,nullptr,nullptr,k,nullptr)==1
                  && EVP_DigestVerify(m,S.data(),sl,msg,sizeof msg)==1;
            EVP_MD_CTX_free(m); return r;
        });
        row(a.cat, a.name, "sig", "verify", s, pk, sk, (long)sl);
        EVP_PKEY_free(k);
    }
    return true;
}

// ---------------- KEX (ECDH) ----------------
static bool run_kex(const Config& c, const Algo& a) {
    EVP_PKEY* A = a.keygen(); EVP_PKEY* B = a.keygen();
    if (!A || !B) { fprintf(stderr,"[skip] keygen fail %s\n",a.name.c_str()); return false; }
    auto derive = [](EVP_PKEY* me, EVP_PKEY* peer, std::vector<uint8_t>& out)->bool{
        EVP_PKEY_CTX* c2 = EVP_PKEY_CTX_new(me,nullptr); size_t l=0;
        bool r = EVP_PKEY_derive_init(c2)==1 && EVP_PKEY_derive_set_peer(c2,peer)==1
              && EVP_PKEY_derive(c2,nullptr,&l)==1;
        out.resize(l ? l : 32);
        r = r && EVP_PKEY_derive(c2,out.data(),&l)==1; out.resize(l);
        EVP_PKEY_CTX_free(c2); return r;
    };
    std::vector<uint8_t> s1, s2;
    if (!derive(A,B,s1) || !derive(B,A,s2) || s1!=s2) {
        fprintf(stderr,"[CORRECTNESS FAIL] %s ECDH mismatch\n", a.name.c_str());
        EVP_PKEY_free(A); EVP_PKEY_free(B); return false;
    }
    long pk = pub_bytes(A), sk = priv_bytes(A);
    Stat s;
    s = measure(c, [&]{ EVP_PKEY* k=a.keygen(); if(!k)return false; EVP_PKEY_free(k); return true; });
    row(a.cat, a.name, "kex", "keygen", s, pk, sk, (long)s1.size());
    s = measure(c, [&]{ std::vector<uint8_t> o; return derive(A,B,o); });
    row(a.cat, a.name, "kex", "derive", s, pk, sk, (long)s1.size());
    EVP_PKEY_free(A); EVP_PKEY_free(B);
    return true;
}

int main(int argc, char** argv) {
    Config c;
    for (int i = 1; i < argc; i++) {
        std::string k = argv[i];
        auto val = [&](const char* def)->std::string{ return (i+1<argc)? argv[++i] : def; };
        if      (k=="--suite")   c.suite  = val("all");
        else if (k=="--level")   c.level  = val("all");
        else if (k=="--iters")   c.iters  = atol(val("1000").c_str());
        else if (k=="--warmup")  c.warmup = atol(val("100").c_str());
        else if (k=="--batches") c.batches= atoi(val("10").c_str());
        else if (k=="--maxms")   c.maxms  = atof(val("3000").c_str());
        else if (k=="--csv")     c.csv    = val("");
        else if (k=="--only")    c.only   = val("");
        else if (k=="--help") {
            printf("Usage: pqc_bench [--suite kem|sig|kex|all] [--level 1|2|3|5|all]\n"
                   "  [--iters N] [--warmup W] [--batches B] [--maxms MS] [--only NAME] [--csv FILE]\n");
            return 0;
        }
    }
    if (!c.csv.empty()) { g_out = fopen(c.csv.c_str(), "w"); if (!g_out) { perror("csv"); return 1; } }

    // Bảng thuật toán theo mức bảo mật (Cat). md=nullptr cho ML-DSA (pure), "SHA256" cho RSA/ECDSA.
    std::vector<Algo> algos = {
        // KEM (ML-KEM) vs KEX (ECDH)
        {"ML-KEM-512", "1", KEM, []{return kg_named("ML-KEM-512");},  nullptr},
        {"ML-KEM-768", "3", KEM, []{return kg_named("ML-KEM-768");},  nullptr},
        {"ML-KEM-1024","5", KEM, []{return kg_named("ML-KEM-1024");}, nullptr},
        {"ECDH-P-256", "1", KEX, []{return kg_ec("P-256");}, nullptr},
        {"ECDH-P-384", "3", KEX, []{return kg_ec("P-384");}, nullptr},
        {"ECDH-P-521", "5", KEX, []{return kg_ec("P-521");}, nullptr},
        // SIG (ML-DSA) vs RSA/ECDSA
        {"ML-DSA-44",  "2", SIG, []{return kg_named("ML-DSA-44");}, nullptr},
        {"ML-DSA-65",  "3", SIG, []{return kg_named("ML-DSA-65");}, nullptr},
        {"ML-DSA-87",  "5", SIG, []{return kg_named("ML-DSA-87");}, nullptr},
        {"ECDSA-P-256","1", SIG, []{return kg_ec("P-256");}, "SHA256"},
        {"ECDSA-P-384","3", SIG, []{return kg_ec("P-384");}, "SHA256"},
        {"ECDSA-P-521","5", SIG, []{return kg_ec("P-521");}, "SHA256"},
        {"RSA-3072",   "1", SIG, []{return kg_rsa(3072);},  "SHA256"},
        {"RSA-7680",   "3", SIG, []{return kg_rsa(7680);},  "SHA256"},
        {"RSA-15360",  "5", SIG, []{return kg_rsa(15360);}, "SHA256"},
    };

    fprintf(g_out, "category,algo,suite,op,count,median_ns,ci95lo_ns,ci95hi_ns,mean_ns,stddev_ns,p95_ns,min_ns,pk_bytes,sk_bytes,ct_or_sig_bytes\n");
    for (const Algo& a : algos) {
        const char* suite = (a.kind==KEM?"kem":a.kind==SIG?"sig":"kex");
        if (c.suite!="all" && c.suite!=suite) continue;
        if (c.level!="all" && c.level!=a.cat) continue;
        if (!c.only.empty() && a.name.find(c.only)==std::string::npos) continue;
        fprintf(stderr, "== %s (Cat %s) ==\n", a.name.c_str(), a.cat);
        if      (a.kind==KEM) run_kem(c, a);
        else if (a.kind==SIG) run_sig(c, a);
        else                  run_kex(c, a);
    }
    if (g_out != stdout) fclose(g_out);
    return 0;
}
