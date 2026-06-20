/* =============================================================================
 * client.c  --  TLS 1.3 client, hand-rolled protocol (RFC 8446).
 *
 * Mirrors the *flow* that illustrated-tls13 drives through OpenSSL in
 * client/main.c (SSL_do_handshake), but here every handshake message and key
 * is built by hand. Run against server.c, or against `openssl s_server`.
 *
 * Handshake (RFC 8446 section 2, figure 1):
 *   [1] -> ClientHello                                   (sec 4.1.2)
 *   [2] <- ServerHello                                   (sec 4.1.3)
 *   [3]    ECDHE -> shared secret                        (sec 7.4.2)
 *   [4]    key schedule -> handshake keys                (sec 7.1)
 *   [5] <- {EncryptedExtensions}{Certificate}{CertificateVerify}{Finished}
 *   [6]    verify server signature + Finished MAC        (sec 4.4.3 / 4.4.4)
 *   [7] -> {Finished}; then application data             (sec 4.4.4)
 * ===========================================================================*/
#include "tls13.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <netdb.h>

#include <openssl/x509.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#define MAXREC 17000

/* ----- build ClientHello body+header (RFC 8446 section 4.1.2) ------------ *
 * mlkem_ek != NULL -> offer the hybrid group X25519MLKEM768 with key_share =
 * ML-KEM-768 encap key (1184) || X25519 pubkey (32); otherwise plain X25519.  */
static void build_client_hello(buf_t *out, const uint8_t pubkey[32],
                               const uint8_t *mlkem_ek)
{
    int hybrid = (mlkem_ek != NULL);
    buf_t b; buf_init(&b);
    buf_u16(&b, LEGACY_VERSION);                       /* legacy_version       */
    uint8_t random[32];                                /* 32-byte Random       */
    if (RAND_bytes(random, 32) != 1) die("RAND_bytes");
    buf_add(&b, random, 32);
    uint8_t sid[32];                                   /* legacy_session_id    */
    RAND_bytes(sid, 32);                               /* 32B: middlebox compat*/
    buf_u8(&b, 32); buf_add(&b, sid, 32);
    /* cipher_suites<2..> = just TLS_AES_256_GCM_SHA384                        */
    buf_u16(&b, 2); buf_u16(&b, TLS_AES_256_GCM_SHA384);
    buf_u8(&b, 1); buf_u8(&b, 0);                      /* compression: null    */

    /* extensions<8..> ----------------------------------------------------- */
    size_t ext = buf_begin_vec(&b, 2);
    /* supported_versions (sec 4.2.1): list<2..254> -> 0x0304                 */
    buf_u16(&b, EXT_SUPPORTED_VERSIONS);
    buf_u16(&b, 3); buf_u8(&b, 2); buf_u16(&b, TLS13_VERSION);
    /* supported_groups (sec 4.2.7): the single group we carry a key_share for */
    buf_u16(&b, EXT_SUPPORTED_GROUPS);
    buf_u16(&b, 4); buf_u16(&b, 2);
    buf_u16(&b, hybrid ? NAMED_GROUP_X25519MLKEM768 : NAMED_GROUP_X25519);
    /* signature_algorithms (sec 4.2.3): ecdsa_secp256r1_sha256               */
    buf_u16(&b, EXT_SIGNATURE_ALGORITHMS);
    buf_u16(&b, 4); buf_u16(&b, 2); buf_u16(&b, SIG_ECDSA_SECP256R1_SHA256);
    /* key_share (sec 4.2.8): one entry for the offered group                 */
    buf_u16(&b, EXT_KEY_SHARE);
    size_t ks = buf_begin_vec(&b, 2);
    size_t shares = buf_begin_vec(&b, 2);              /* client_shares<0..>   */
    if (hybrid) {                                      /* ML-KEM ek || X25519  */
        buf_u16(&b, NAMED_GROUP_X25519MLKEM768);
        size_t kx = buf_begin_vec(&b, 2);
        buf_add(&b, mlkem_ek, MLKEM768_EK_LEN);        /* 1184: ML-KEM FIRST   */
        buf_add(&b, pubkey, 32);                       /* 32:   then X25519    */
        buf_end_vec(&b, kx, 2);
    } else {
        buf_u16(&b, NAMED_GROUP_X25519);
        buf_u16(&b, 32); buf_add(&b, pubkey, 32);      /* key_exchange<1..>    */
    }
    buf_end_vec(&b, shares, 2);
    buf_end_vec(&b, ks, 2);
    buf_end_vec(&b, ext, 2);

    /* handshake header: msg_type(1) || length(3)  (sec 4)                    */
    buf_u8(out, HS_CLIENT_HELLO);
    buf_u24(out, (uint32_t)b.len);
    buf_add(out, b.data, b.len);
    buf_free(&b);
}

/* ----- parse ServerHello, return server key_share (RFC 8446 4.1.3/4.2.8) - */
static void parse_server_hello(const uint8_t *m, size_t mlen, uint8_t server_pub[32],
                               uint8_t *server_ct /* NULL=X25519; else MLKEM768_CT_LEN */)
{
    if (m[0] != HS_SERVER_HELLO) die("expected ServerHello");
    (void)mlen;                                        /* length tracked inline */
    const uint8_t *b = m + 4;
    size_t off = 2 + 32;                               /* legacy_version+random*/
    off += 1 + b[off];                                 /* legacy_session_id    */
    uint16_t cs = (b[off] << 8) | b[off + 1]; off += 2;/* cipher_suite         */
    if (cs != TLS_AES_256_GCM_SHA384) die("server chose unexpected cipher");
    off += 1;                                          /* compression method   */
    size_t extlen = (b[off] << 8) | b[off + 1]; off += 2;
    size_t end = off + extlen;
    int found = 0;
    while (off < end) {
        uint16_t et = (b[off] << 8) | b[off + 1];
        uint16_t el = (b[off + 2] << 8) | b[off + 3]; off += 4;
        if (et == EXT_KEY_SHARE) {                     /* group||keylen||key   */
            uint16_t grp  = (b[off] << 8) | b[off + 1];
            uint16_t klen = (b[off + 2] << 8) | b[off + 3];
            const uint8_t *key = b + off + 4;
            if (server_ct) {                           /* hybrid: ct || X25519 */
                if (grp != NAMED_GROUP_X25519MLKEM768 ||
                    klen != MLKEM768_CT_LEN + 32) die("server hybrid key_share bad");
                memcpy(server_ct, key, MLKEM768_CT_LEN);
                memcpy(server_pub, key + MLKEM768_CT_LEN, 32);
            } else {
                if (grp != NAMED_GROUP_X25519 || klen != 32)
                    die("server key_share not 32-byte X25519");
                memcpy(server_pub, key, 32);
            }
            found = 1;
        }
        off += el;
    }
    if (!found) die("ServerHello without key_share (HelloRetryRequest?)");
}

/* ----- verify CertificateVerify signature (RFC 8446 section 4.4.3) -------- */
static int verify_cert_verify(EVP_PKEY *server_key, const uint8_t th_cert[HASH_LEN],
                              const uint8_t *sig, size_t siglen)
{
    /* signed content = 0x20 x64 || "TLS 1.3, server CertificateVerify" || 0x00
     *                  || Transcript-Hash(ClientHello..Certificate)          */
    uint8_t content[64 + 33 + 1 + HASH_LEN];
    memset(content, 0x20, 64);
    const char *ctx = "TLS 1.3, server CertificateVerify";   /* 33 bytes      */
    memcpy(content + 64, ctx, 33);
    content[64 + 33] = 0x00;
    memcpy(content + 64 + 34, th_cert, HASH_LEN);
    size_t clen = 64 + 33 + 1 + HASH_LEN;

    EVP_MD_CTX *md = EVP_MD_CTX_new();
    int ok = (EVP_DigestVerifyInit(md, NULL, EVP_sha256(), NULL, server_key) == 1
              && EVP_DigestVerify(md, sig, siglen, content, clen) == 1);
    EVP_MD_CTX_free(md);
    return ok;
}

/* ===========================================================================
 *  One full TLS 1.3 session, optionally timed.
 *
 *  verbose != 0 : print the step-by-step trace (the demo).
 *  t != NULL    : record per-phase timings (the benchmark). All printf is
 *                 suppressed when verbose == 0 so it cannot pollute timings.
 *
 *  The timed interval is the handshake only: from just before the client's
 *  X25519 keygen up to the moment the client Finished is written. The "ping"
 *  exchange afterwards is NOT part of the measured handshake.
 * ===========================================================================*/
struct hs_timing {            /* all fields in microseconds                   */
    double total;             /* whole handshake (keygen .. Finished sent)    */
    double keygen;            /* client X25519 ephemeral keygen   (sec 4.2.8) */
    double ecdhe;             /* client X25519 shared secret      (sec 7.4.2) */
    double key_sched;         /* handshake key schedule           (sec 7.1)   */
    double sig_verify;        /* verify server CertificateVerify  (sec 4.4.3) */
};

/* Key-exchange group: 0 = X25519 (default, byte-exact vs illustrated-tls13),
 * 1 = hybrid X25519MLKEM768. Set from env TLS_GROUP in main().                */
static int g_hybrid = 0;

static double now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);  /* RAW: no NTP, no kernel freq scaling */
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

static int run_session(const char *host, const char *port, int verbose,
                       struct hs_timing *t)
{
    /* --- TCP connect ----------------------------------------------------- */
    struct addrinfo *res = NULL;
    if (getaddrinfo(host, port, NULL, &res) != 0) die("getaddrinfo");
    int fd = socket(res->ai_family, SOCK_STREAM, 0);
    if (fd < 0 || connect(fd, res->ai_addr, res->ai_addrlen) < 0)
        die("connect");
    freeaddrinfo(res);

    tls_conn c; memset(&c, 0, sizeof c); c.fd = fd; buf_init(&c.transcript);

    double t0 = now_us();                              /* <-- handshake start  */

    /* --- [1] ClientHello ------------------------------------------------- */
    uint8_t priv[32], pub[32];
    x25519_keygen(priv, pub);
    uint8_t mlkem_ek[MLKEM768_EK_LEN];
    mlkem_key *mk = NULL;
    if (g_hybrid) { mk = mlkem768_keygen(mlkem_ek); if (!mk) die("ML-KEM-768 keygen"); }
    double t_kg = now_us();                            /* after client keygen(s) */
    buf_t ch; buf_init(&ch);
    build_client_hello(&ch, pub, g_hybrid ? mlkem_ek : NULL);
    buf_add(&c.transcript, ch.data, ch.len);           /* transcript starts CH */
    record_send_plain(fd, CT_HANDSHAKE, ch.data, ch.len, 0x0301);
    if (verbose) printf("[1] -> ClientHello (%zu bytes)\n", ch.len);
    buf_free(&ch);

    /* --- [2] ServerHello (plaintext) ------------------------------------- */
    uint8_t type, *payload; size_t plen;
    do {
        if (record_read(fd, &type, &payload, &plen) < 0) die("read ServerHello");
        if (type == CT_CHANGE_CIPHER_SPEC) { free(payload); continue; }
        if (type == CT_ALERT) die("server sent alert");
    } while (type != CT_HANDSHAKE);
    buf_add(&c.transcript, payload, plen);
    uint8_t server_pub[32], server_ct[MLKEM768_CT_LEN];
    parse_server_hello(payload, plen, server_pub, g_hybrid ? server_ct : NULL);
    free(payload);
    double t_sh = now_us();                            /* after ServerHello    */
    if (verbose) printf("[2] <- ServerHello\n");

    /* --- [3] (EC)DHE  (+ ML-KEM decaps for the hybrid group) ------------- */
    uint8_t ecdhe[MLKEM768_SS_LEN + 32]; size_t ecdhe_len; uint8_t ss_x[32];
    if (!x25519_derive(priv, server_pub, ss_x)) die("X25519 derive");
    if (g_hybrid) {                                    /* ss = ML-KEM_ss || X25519_ss */
        uint8_t ss_m[MLKEM768_SS_LEN];
        if (!mlkem768_decaps(mk, server_ct, ss_m)) die("ML-KEM-768 decaps");
        memcpy(ecdhe, ss_m, MLKEM768_SS_LEN);
        memcpy(ecdhe + MLKEM768_SS_LEN, ss_x, 32);
        ecdhe_len = MLKEM768_SS_LEN + 32;              /* 64 */
    } else {
        memcpy(ecdhe, ss_x, 32); ecdhe_len = 32;
    }
    if (mk) mlkem_free(mk);
    double t_ec = now_us();                            /* after (EC)DHE+decaps  */
    if (verbose) hexdump("[3] (EC)DHE shared secret", ecdhe, ecdhe_len);

    /* --- [4] handshake keys --------------------------------------------- */
    ks_derive_handshake(&c, ecdhe, ecdhe_len);
    double t_ks = now_us();                            /* after key schedule   */
    if (verbose) printf("[4] derived handshake traffic keys\n");

    /* --- [5] read+decrypt server flight --------------------------------- */
    uint64_t s_seq = 0;
    buf_t hs; buf_init(&hs);                            /* reassembly buffer    */
    int got_fin = 0;
    EVP_PKEY *server_key = NULL;
    uint8_t th_cert[HASH_LEN]; int have_th_cert = 0;
    uint8_t *cv_sig = NULL; size_t cv_siglen = 0;
    uint8_t th_beforefin[HASH_LEN]; uint8_t server_fin[HASH_LEN]; int have_fin = 0;
    uint8_t pt[MAXREC];

    while (!got_fin) {
        if (record_read(fd, &type, &payload, &plen) < 0) die("read flight");
        if (type == CT_CHANGE_CIPHER_SPEC) { free(payload); continue; }
        uint8_t rec_hdr[5] = { type, (LEGACY_VERSION>>8)&0xFF, LEGACY_VERSION&0xFF, (plen>>8)&0xFF, plen&0xFF };
        uint8_t inner_type; size_t ptlen;
        if (record_decrypt(c.s_hs_key, c.s_hs_iv, s_seq++, rec_hdr,
                           payload, plen, &inner_type, pt, &ptlen) != 0)
            die("decrypt server flight");
        free(payload);
        if (inner_type != CT_HANDSHAKE) continue;
        buf_add(&hs, pt, ptlen);

        size_t off = 0;
        while (off + 4 <= hs.len) {
            uint8_t mt = hs.data[off];
            uint32_t ml = (hs.data[off+1] << 16) | (hs.data[off+2] << 8) | hs.data[off+3];
            if (off + 4 + ml > hs.len) break;          /* message spans records*/
            uint8_t *full = hs.data + off; size_t flen = 4 + ml;

            if (mt == HS_FINISHED) {
                /* transcript hash through CertificateVerify (before Finished) */
                sha384(c.transcript.data, c.transcript.len, th_beforefin);
                memcpy(server_fin, full + 4, HASH_LEN); have_fin = 1;
                buf_add(&c.transcript, full, flen); got_fin = 1;
                if (verbose) printf("    [5d] <- Finished\n");
            } else {
                if (mt == HS_ENCRYPTED_EXTENSIONS) {
                    if (verbose) printf("    [5a] <- EncryptedExtensions\n");
                } else if (mt == HS_CERTIFICATE) {
                    /* extract first cert's DER -> public key (sec 4.4.2)      */
                    uint8_t *body = full + 4;
                    size_t ctxlen = body[0];
                    uint8_t *cl = body + 1 + ctxlen;   /* certificate_list     */
                    uint8_t *e = cl + 3;               /* first CertificateEntry*/
                    size_t cert_dlen = (e[0] << 16) | (e[1] << 8) | e[2];
                    const uint8_t *der = e + 3;
                    X509 *x = d2i_X509(NULL, &der, cert_dlen);
                    if (!x) die("parse server certificate");
                    server_key = X509_get_pubkey(x);
                    X509_free(x);
                    if (verbose) printf("    [5b] <- Certificate\n");
                } else if (mt == HS_CERTIFICATE_VERIFY) {
                    /* algorithm(2) || signature<0..> ; snapshot TH first      */
                    sha384(c.transcript.data, c.transcript.len, th_cert);
                    have_th_cert = 1;
                    cv_siglen = (full[6] << 8) | full[7];
                    cv_sig = malloc(cv_siglen);
                    memcpy(cv_sig, full + 8, cv_siglen);
                    if (verbose) printf("    [5c] <- CertificateVerify\n");
                }
                buf_add(&c.transcript, full, flen);    /* ALL msgs into transcript */
            }
            off += flen;
        }
        memmove(hs.data, hs.data + off, hs.len - off); /* keep leftover        */
        hs.len -= off;
    }
    buf_free(&hs);
    double t_fl = now_us();                            /* after full flight    */

    /* --- [6] verify signature + Finished MAC ----------------------------- */
    if (!server_key || !have_th_cert || !cv_sig) die("missing cert/verify");
    if (!verify_cert_verify(server_key, th_cert, cv_sig, cv_siglen))
        die("CertificateVerify signature INVALID");
    double t_sv = now_us();                            /* after signature ver. */
    if (verbose) printf("[6] CertificateVerify signature OK\n");

    uint8_t fin_key[HASH_LEN], expect[HASH_LEN];
    hkdf_expand_label(c.s_hs_secret, "finished", NULL, 0, fin_key, HASH_LEN);
    hmac_sha384(fin_key, HASH_LEN, th_beforefin, HASH_LEN, expect);
    if (!have_fin || CRYPTO_memcmp(expect, server_fin, HASH_LEN) != 0)
        die("server Finished MAC INVALID");
    if (verbose) printf("[6] server Finished MAC OK\n");
    EVP_PKEY_free(server_key); free(cv_sig);

    /* --- [7] application keys + client Finished -------------------------- */
    ks_derive_application(&c);
    uint8_t th_sf[HASH_LEN];                            /* CH..server Finished  */
    sha384(c.transcript.data, c.transcript.len, th_sf);
    uint8_t cfin_key[HASH_LEN], cfin_vd[HASH_LEN];
    hkdf_expand_label(c.c_hs_secret, "finished", NULL, 0, cfin_key, HASH_LEN);
    hmac_sha384(cfin_key, HASH_LEN, th_sf, HASH_LEN, cfin_vd);
    uint8_t cfin[4 + HASH_LEN];
    cfin[0] = HS_FINISHED; cfin[1] = 0; cfin[2] = 0; cfin[3] = HASH_LEN;
    memcpy(cfin + 4, cfin_vd, HASH_LEN);
    record_send_plain(fd, CT_CHANGE_CIPHER_SPEC, (const uint8_t *)"\x01", 1, 0x0303);
    record_send_enc(fd, c.c_hs_key, c.c_hs_iv, 0, CT_HANDSHAKE, cfin, sizeof cfin);
    double t_done = now_us();                          /* <-- handshake done   */
    if (verbose) printf("[7] -> Finished  (handshake complete)\n");

    if (t) {                                           /* fill timing record   */
        t->total      = t_done - t0;
        t->keygen     = t_kg   - t0;
        t->ecdhe      = t_ec   - t_sh;
        t->key_sched  = t_ks   - t_ec;
        t->sig_verify = t_sv   - t_fl;
    }

    /* --- application data (not part of the measured handshake) ----------- */
    const char *msg = "ping";
    record_send_enc(fd, c.c_ap_key, c.c_ap_iv, 0, CT_APPLICATION_DATA,
                    (const uint8_t *)msg, strlen(msg));
    if (verbose) printf("[8] -> application data: \"%s\"\n", msg);

    uint64_t r_seq = 0;
    for (int i = 0; i < 20; i++) {
        if (record_read(fd, &type, &payload, &plen) < 0) break;
        if (type == CT_CHANGE_CIPHER_SPEC) { free(payload); continue; }
        uint8_t rh[5] = { type, (LEGACY_VERSION>>8)&0xFF, LEGACY_VERSION&0xFF, (plen>>8)&0xFF, plen&0xFF };
        uint8_t it; size_t ol;
        if (record_decrypt(c.s_ap_key, c.s_ap_iv, r_seq++, rh,
                           payload, plen, &it, pt, &ol) != 0) { free(payload); break; }
        free(payload);
        if (it == CT_HANDSHAKE) { if (verbose) printf("    <- NewSessionTicket (ignored)\n"); continue; }
        if (it == CT_ALERT) { if (verbose) printf("    <- alert\n"); break; }
        if (it == CT_APPLICATION_DATA) {
            if (verbose) printf("[9] <- application data: \"%.*s\"\n", (int)ol, pt);
            break;
        }
    }

    /* close_notify alert (RFC 8446 section 6.1) -> clean shutdown           */
    uint8_t close_notify[2] = { 1, 0 };   /* warning(1), close_notify(0)      */
    record_send_enc(fd, c.c_ap_key, c.c_ap_iv, 1, CT_ALERT, close_notify, 2);

    close(fd);
    buf_free(&c.transcript);
    if (verbose) printf("[OK] hand-rolled TLS 1.3 client finished.\n");
    return 0;
}

/* ============================ benchmark stats =========================== */
struct stats { double min, median, mean, p95, max, ci95; };

static int cmp_dbl(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static struct stats summarize(double *v, int n)   /* sorts v in place         */
{
    qsort(v, n, sizeof(double), cmp_dbl);
    struct stats s;
    s.min = v[0]; s.max = v[n - 1];
    s.median = (n & 1) ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
    s.p95 = v[(int)(0.95 * (n - 1) + 0.5)];
    double sum = 0; for (int i = 0; i < n; i++) sum += v[i];
    s.mean = sum / n;
    double var = 0; for (int i = 0; i < n; i++) { double d = v[i] - s.mean; var += d * d; }
    var /= (n > 1 ? n - 1 : 1);
    s.ci95 = 1.96 * sqrt(var) / sqrt((double)n);       /* 95% CI of the mean   */
    return s;
}

static void print_row(const char *name, double *v, int n)
{
    struct stats s = summarize(v, n);
    printf("  %-22s %9.2f %9.2f %9.2f %9.2f %9.2f  +/-%.2f\n",
           name, s.min, s.median, s.mean, s.p95, s.max, s.ci95);
}

/* ============================ load generator ============================ *
 * Throughput + latency under concurrency: `conc` threads each keep starting
 * fresh handshakes until `total` have completed; report handshakes/sec and the
 * latency distribution. Custom concurrent harness (wrk-style, but wrk speaks
 * HTTP and our endpoint is a minimal TLS ping/pong, so we roll our own).      */
static const char *g_host, *g_port;
static int         g_total;
static atomic_int  g_issued;
static double     *g_lat;          /* per-handshake total latency (us)         */

static void *load_worker(void *arg)
{
    (void)arg;
    for (;;) {
        int i = atomic_fetch_add(&g_issued, 1);
        if (i >= g_total) break;
        struct hs_timing t;
        run_session(g_host, g_port, 0, &t);
        g_lat[i] = t.total;
    }
    return NULL;
}

static double pctile(const double *sorted, int n, double p)
{
    int idx = (int)(p * (n - 1) + 0.5);
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    return sorted[idx];
}

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);   /* peer may close first; ignore broken-pipe   */
    const char *host = argc > 1 ? argv[1] : "127.0.0.1";
    const char *port = argc > 2 ? argv[2] : "8400";    /* repo uses 8400       */

    int bench = 0, iters = 0, warmup = -1;
    int load = 0, conc = 1, total = 0;
    const char *csv = "handshake_bench.csv";
    /* netem condition labels (set by bench_netem.sh via env); only tag the output */
    const char *netem_rtt  = getenv("NETEM_RTT")  ? getenv("NETEM_RTT")  : "na";
    const char *netem_loss = getenv("NETEM_LOSS") ? getenv("NETEM_LOSS") : "na";

    /* repo "parameter law": env vars give defaults, flags override (Appendix B) */
    const char *e;
    if ((e = getenv("TLS_ITERS")))   iters  = atoi(e);
    if ((e = getenv("BENCH_ITERS"))) iters  = atoi(e);
    if ((e = getenv("BENCH_WARMUP")))warmup = atoi(e);
    if ((e = getenv("TLS_CONC")))    conc   = atoi(e);
    if ((e = getenv("TLS_TOTAL")))   total  = atoi(e);
    if ((e = getenv("BENCH_CSV")))   csv    = e;
    if ((e = getenv("TLS_GROUP")) && strstr(e, "MLKEM")) g_hybrid = 1;  /* hybrid */

    for (int i = 3; i < argc; i++) {
        if      (!strcmp(argv[i], "--bench"))   { bench = 1; if (i+1<argc && isdigit((unsigned char)argv[i+1][0])) iters = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--load"))    { load  = 1; }
        else if (!strcmp(argv[i], "--threads") && i+1<argc) conc   = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--total")   && i+1<argc) total  = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--warmup")  && i+1<argc) warmup = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--csv")     && i+1<argc) csv    = argv[++i];
    }

    /* ---- load mode: throughput + latency under concurrency -------------- */
    if (load) {
        if (total < 1) total = 20000;
        if (conc  < 1) conc  = 1;
        g_host = host; g_port = port; g_total = total;
        atomic_store(&g_issued, 0);
        g_lat = malloc((size_t)total * sizeof(double));
        printf("Self-implemented TLS 1.3 load test  ->  %s:%s\n", host, port);
        printf("suite=TLS_AES_256_GCM_SHA384  group=X25519  sig=ecdsa_secp256r1_sha256\n");
        printf("concurrency=%d  total=%d handshakes  clock=CLOCK_MONOTONIC_RAW\n\n", conc, total);

        pthread_t *tid = malloc(conc * sizeof(pthread_t));
        double t0 = now_us();
        for (int i = 0; i < conc; i++) pthread_create(&tid[i], NULL, load_worker, NULL);
        for (int i = 0; i < conc; i++) pthread_join(tid[i], NULL);
        double sec = (now_us() - t0) / 1e6;
        free(tid);

        double *lat = malloc((size_t)total * sizeof(double));
        memcpy(lat, g_lat, (size_t)total * sizeof(double));
        qsort(lat, total, sizeof(double), cmp_dbl);
        double sum = 0; for (int i = 0; i < total; i++) sum += lat[i];

        printf("throughput : %.0f handshakes/sec  (%d in %.3f s)\n", total / sec, total, sec);
        printf("latency_us : mean %.1f  p50 %.1f  p95 %.1f  p99 %.1f  max %.1f\n",
               sum / total, pctile(lat, total, 0.50), pctile(lat, total, 0.95),
               pctile(lat, total, 0.99), lat[total - 1]);

        FILE *f = fopen(csv, "w");
        if (f) {
            fprintf(f, "handshake_us\n");
            for (int i = 0; i < total; i++) fprintf(f, "%.3f\n", g_lat[i]);
            fclose(f);
            printf("\nper-handshake CSV: %s\n", csv);
        }
        free(lat); free(g_lat);
        return 0;
    }

    if (!bench)
        return run_session(host, port, 1, NULL);       /* demo: verbose trace  */

    /* ---- benchmark mode ------------------------------------------------- */
    if (iters < 1) iters = 1000;
    if (warmup < 0) warmup = iters / 10 > 0 ? iters / 10 : 1;
    printf("Self-implemented TLS 1.3 handshake benchmark  ->  %s:%s\n", host, port);
    printf("suite=TLS_AES_256_GCM_SHA384  group=X25519  sig=ecdsa_secp256r1_sha256\n");
    printf("iterations=%d  warmup=%d  clock=CLOCK_MONOTONIC_RAW\n\n", iters, warmup);

    for (int i = 0; i < warmup; i++) run_session(host, port, 0, NULL);

    double *tot = malloc(iters * sizeof(double)), *kg = malloc(iters * sizeof(double));
    double *ec  = malloc(iters * sizeof(double)), *ks = malloc(iters * sizeof(double));
    double *sv  = malloc(iters * sizeof(double));
    FILE *f = fopen(csv, "w");
    if (f) fprintf(f, "rtt_ms,loss_pct,iter,total_us,keygen_us,ecdhe_us,key_sched_us,sig_verify_us\n");

    int ok = 0;
    for (int i = 0; i < iters; i++) {
        struct hs_timing t;
        if (run_session(host, port, 0, &t) != 0) { fprintf(stderr, "handshake %d failed\n", i); continue; }
        tot[ok] = t.total; kg[ok] = t.keygen; ec[ok] = t.ecdhe;
        ks[ok] = t.key_sched; sv[ok] = t.sig_verify;
        if (f) fprintf(f, "%s,%s,%d,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                       netem_rtt, netem_loss, i, t.total, t.keygen, t.ecdhe, t.key_sched, t.sig_verify);
        ok++;
    }
    if (f) fclose(f);

    printf("microseconds over %d successful handshakes:\n", ok);
    printf("  %-22s %9s %9s %9s %9s %9s\n", "phase", "min", "median", "mean", "p95", "max");
    print_row("TOTAL handshake",   tot, ok);
    print_row("client keygen",     kg,  ok);
    print_row("client ECDHE",      ec,  ok);
    print_row("key schedule",      ks,  ok);
    print_row("sig verify (ECDSA)",sv,  ok);
    printf("\nper-iteration CSV: %s\n", csv);

    /* machine-readable summary line: bench_netem.sh greps "^SUMMARY" to collect results */
    struct stats st = summarize(tot, ok);   /* tot already sorted by print_row; summarize is idempotent */
    printf("SUMMARY rtt_ms=%s loss_pct=%s ok=%d total_median_us=%.3f total_p95_us=%.3f "
           "total_mean_us=%.3f total_ci95_us=%.3f\n",
           netem_rtt, netem_loss, ok, st.median, st.p95, st.mean, st.ci95);

    free(tot); free(kg); free(ec); free(ks); free(sv);
    return ok == iters ? 0 : 1;
}
