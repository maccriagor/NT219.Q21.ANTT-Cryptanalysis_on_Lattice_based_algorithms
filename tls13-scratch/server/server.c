/* =============================================================================
 * server.c  --  TLS 1.3 server, hand-rolled protocol (RFC 8446).
 *
 * The mirror image of client.c. Where illustrated-tls13's server/main.c just
 * calls SSL_accept(), here every server handshake message and key is built by
 * hand. Interoperates with client.c AND with `openssl s_client`.
 *
 * Handshake from the server's side (RFC 8446 section 2, figure 1):
 *   [1] <- ClientHello                                   (sec 4.1.2)
 *   [2] -> ServerHello                                   (sec 4.1.3)
 *   [3]    ECDHE -> shared secret                        (sec 7.4.2)
 *   [4]    key schedule -> handshake keys                (sec 7.1)
 *   [5] -> {EncryptedExtensions}{Certificate}{CertificateVerify}{Finished}
 *          (CertificateVerify is signed with the server key, sec 4.4.3)
 *   [6] <- {Finished}; verify client Finished MAC        (sec 4.4.4)
 *   [7]    application data
 * ===========================================================================*/
#include "tls13.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <stdatomic.h>

#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#define MAXREC 17000

/* ----- parse ClientHello: extract client key_share + session_id (to echo) - *
 * RFC 8446 section 4.1.2 / 4.2.8                                              */
static void parse_client_hello(const uint8_t *m, size_t mlen,
                               uint8_t client_pub[32],
                               uint8_t client_ek[MLKEM768_EK_LEN], int *hybrid,
                               uint8_t session_id[32], size_t *sid_len)
{
    *hybrid = 0;
    if (m[0] != HS_CLIENT_HELLO) die("expected ClientHello");
    (void)mlen;                                        /* length tracked inline */
    const uint8_t *b = m + 4;
    size_t off = 2 + 32;                               /* legacy_version+random*/
    *sid_len = b[off]; off += 1;                       /* legacy_session_id    */
    memcpy(session_id, b + off, *sid_len); off += *sid_len;
    size_t cslen = (b[off] << 8) | b[off + 1]; off += 2 + cslen;  /* cipher_suites */
    off += 1 + b[off];                                 /* compression methods  */
    size_t extlen = (b[off] << 8) | b[off + 1]; off += 2;
    size_t end = off + extlen;
    int found = 0;
    while (off < end) {
        uint16_t et = (b[off] << 8) | b[off + 1];
        uint16_t el = (b[off + 2] << 8) | b[off + 3]; off += 4;
        if (et == EXT_KEY_SHARE) {
            /* client_shares<0..>: walk entries, take the X25519 one           */
            size_t li = off + 2, lend = off + el;       /* skip list length     */
            while (li + 4 <= lend) {
                uint16_t grp = (b[li] << 8) | b[li + 1];
                uint16_t klen = (b[li + 2] << 8) | b[li + 3];
                const uint8_t *key = b + li + 4;
                if (grp == NAMED_GROUP_X25519MLKEM768 &&
                    klen == MLKEM768_EK_LEN + 32) {    /* prefer hybrid: ek||X25519 */
                    memcpy(client_ek, key, MLKEM768_EK_LEN);
                    memcpy(client_pub, key + MLKEM768_EK_LEN, 32);
                    *hybrid = 1; found = 1;
                } else if (grp == NAMED_GROUP_X25519 && klen == 32 && !found) {
                    memcpy(client_pub, key, 32);
                    found = 1;
                }
                li += 4 + klen;
            }
        }
        off += el;
    }
    if (!found) die("ClientHello without X25519 key_share");
}

/* ----- ServerHello (RFC 8446 section 4.1.3) ------------------------------- */
static void build_server_hello(buf_t *out, const uint8_t server_pub[32],
                               const uint8_t *mlkem_ct /* NULL=X25519; else CT_LEN */,
                               const uint8_t session_id[32], size_t sid_len)
{
    buf_t b; buf_init(&b);
    buf_u16(&b, LEGACY_VERSION);                       /* legacy_version=0x0303*/
    uint8_t random[32];                                /* 32-byte Random       */
    if (RAND_bytes(random, 32) != 1) die("RAND_bytes");
    buf_add(&b, random, 32);
    buf_u8(&b, (uint8_t)sid_len);                      /* echo session_id      */
    buf_add(&b, session_id, sid_len);
    buf_u16(&b, TLS_AES_256_GCM_SHA384);               /* cipher_suite         */
    buf_u8(&b, 0);                                     /* compression: null    */

    size_t ext = buf_begin_vec(&b, 2);                 /* extensions<6..>      */
    /* supported_versions (sec 4.2.1): selected_version = 0x0304 (just 2 bytes)*/
    buf_u16(&b, EXT_SUPPORTED_VERSIONS);
    buf_u16(&b, 2); buf_u16(&b, TLS13_VERSION);
    /* key_share (sec 4.2.8): server_share = group || key_exchange<1..>        */
    buf_u16(&b, EXT_KEY_SHARE);
    if (mlkem_ct) {                                    /* hybrid: ct || X25519 */
        buf_u16(&b, 2 + 2 + MLKEM768_CT_LEN + 32);     /* extension_data length */
        buf_u16(&b, NAMED_GROUP_X25519MLKEM768);
        buf_u16(&b, MLKEM768_CT_LEN + 32);             /* key_exchange length   */
        buf_add(&b, mlkem_ct, MLKEM768_CT_LEN);        /* 1088: ML-KEM ct FIRST */
        buf_add(&b, server_pub, 32);                   /* 32:   then X25519     */
    } else {
        buf_u16(&b, 2 + 2 + 32);
        buf_u16(&b, NAMED_GROUP_X25519);
        buf_u16(&b, 32); buf_add(&b, server_pub, 32);
    }
    buf_end_vec(&b, ext, 2);

    buf_u8(out, HS_SERVER_HELLO);                      /* handshake header     */
    buf_u24(out, (uint32_t)b.len);
    buf_add(out, b.data, b.len);
    buf_free(&b);
}

/* ----- EncryptedExtensions (empty) -- RFC 8446 section 4.3.1 -------------- */
static void build_encrypted_extensions(buf_t *out)
{
    buf_u8(out, HS_ENCRYPTED_EXTENSIONS);
    buf_u24(out, 2);
    buf_u16(out, 0);                                   /* extensions<0..> = {} */
}

/* ----- Certificate -- RFC 8446 section 4.4.2 ------------------------------ */
static void build_certificate(buf_t *out, const uint8_t *der, size_t derlen)
{
    buf_t b; buf_init(&b);
    buf_u8(&b, 0);                                     /* request_context = "" */
    size_t list = buf_begin_vec(&b, 3);                /* certificate_list<0..>*/
    size_t entry = buf_begin_vec(&b, 3);               /* cert_data<1..>       */
    buf_add(&b, der, derlen);
    buf_end_vec(&b, entry, 3);
    buf_u16(&b, 0);                                    /* per-cert extensions={} */
    buf_end_vec(&b, list, 3);

    buf_u8(out, HS_CERTIFICATE);
    buf_u24(out, (uint32_t)b.len);
    buf_add(out, b.data, b.len);
    buf_free(&b);
}

/* ----- CertificateVerify -- RFC 8446 section 4.4.3 ------------------------ *
 * Sign 0x20 x64 || "TLS 1.3, server CertificateVerify" || 0x00 || TH(..Cert) *
 * with the server key (ecdsa_secp256r1_sha256 -> ECDSA over SHA-256).        */
static void build_certificate_verify(buf_t *out, EVP_PKEY *key,
                                     const uint8_t th_cert[HASH_LEN])
{
    uint8_t content[64 + 33 + 1 + HASH_LEN];
    memset(content, 0x20, 64);
    memcpy(content + 64, "TLS 1.3, server CertificateVerify", 33);
    content[64 + 33] = 0x00;
    memcpy(content + 64 + 34, th_cert, HASH_LEN);
    size_t clen = sizeof content;

    uint8_t sig[256]; size_t siglen = sizeof sig;
    EVP_MD_CTX *md = EVP_MD_CTX_new();
    if (EVP_DigestSignInit(md, NULL, EVP_sha256(), NULL, key) != 1 ||
        EVP_DigestSign(md, sig, &siglen, content, clen) != 1)
        die("CertificateVerify signing failed");
    EVP_MD_CTX_free(md);

    buf_t b; buf_init(&b);
    buf_u16(&b, SIG_ECDSA_SECP256R1_SHA256);           /* algorithm            */
    buf_u16(&b, (uint16_t)siglen); buf_add(&b, sig, siglen);  /* signature<0..> */

    buf_u8(out, HS_CERTIFICATE_VERIFY);
    buf_u24(out, (uint32_t)b.len);
    buf_add(out, b.data, b.len);
    buf_free(&b);
}

/* ----- Finished -- RFC 8446 section 4.4.4 --------------------------------- */
static void build_finished(buf_t *out, const uint8_t base_secret[HASH_LEN],
                           const uint8_t th[HASH_LEN])
{
    uint8_t fin_key[HASH_LEN], vd[HASH_LEN];
    hkdf_expand_label(base_secret, "finished", NULL, 0, fin_key, HASH_LEN);
    hmac_sha384(fin_key, HASH_LEN, th, HASH_LEN, vd);
    buf_u8(out, HS_FINISHED);
    buf_u24(out, HASH_LEN);
    buf_add(out, vd, HASH_LEN);
}

/* read one handshake message of an expected type from an encrypted record    */
static void read_handshake_msg(tls_conn *c, const uint8_t key[KEY_LEN],
                               const uint8_t iv[IV_LEN], uint64_t seq,
                               uint8_t want_type, uint8_t *body, size_t *blen)
{
    uint8_t type, *payload; size_t plen;
    for (;;) {
        if (record_read(c->fd, &type, &payload, &plen) < 0) die("read record");
        if (type == CT_CHANGE_CIPHER_SPEC) { free(payload); continue; }
        break;
    }
    uint8_t rh[5] = { type, (LEGACY_VERSION>>8)&0xFF, LEGACY_VERSION&0xFF,
                      (plen>>8)&0xFF, plen&0xFF };
    uint8_t pt[MAXREC], inner; size_t ptlen;
    if (record_decrypt(key, iv, seq, rh, payload, plen, &inner, pt, &ptlen) != 0)
        die("decrypt record");
    free(payload);
    if (inner != CT_HANDSHAKE || pt[0] != want_type) die("unexpected handshake msg");
    uint32_t ml = (pt[1] << 16) | (pt[2] << 8) | pt[3];
    memcpy(body, pt + 4, ml);
    *blen = ml;
}

static double now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);  /* RAW: no NTP, no kernel freq scaling */
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

/* ===========================================================================
 *  Serve ONE connection: the full server-side handshake + ping/pong.
 *  Returns the server-side handshake time in microseconds (ClientHello read
 *  .. client Finished verified) -- the analogue of timing SSL_accept.
 *  verbose != 0 prints the step-by-step trace (single-connection demo);
 *  in benchmark mode (many connections) it is silenced.
 * ===========================================================================*/
static double serve_one(int fd, const uint8_t *der, int derlen, EVP_PKEY *key, int verbose)
{
    tls_conn c; memset(&c, 0, sizeof c); c.fd = fd; buf_init(&c.transcript);
    double t0 = now_us();                              /* <-- handshake start  */

    /* --- [1] ClientHello -------------------------------------------------- */
    uint8_t type, *payload; size_t plen;
    do {
        if (record_read(fd, &type, &payload, &plen) < 0) die("read ClientHello");
        if (type == CT_CHANGE_CIPHER_SPEC) { free(payload); continue; }
        if (type == CT_ALERT) die("client sent alert");
    } while (type != CT_HANDSHAKE);
    buf_add(&c.transcript, payload, plen);             /* transcript starts CH */
    uint8_t client_pub[32], client_ek[MLKEM768_EK_LEN], sid[32]; size_t sid_len;
    int hybrid = 0;
    parse_client_hello(payload, plen, client_pub, client_ek, &hybrid, sid, &sid_len);
    free(payload);
    if (verbose) printf("[1] <- ClientHello (%s)\n",
                        hybrid ? "X25519MLKEM768" : "X25519");

    /* --- [2] ServerHello: X25519 keygen+derive (+ ML-KEM encaps if hybrid) - */
    uint8_t priv[32], pub[32];
    x25519_keygen(priv, pub);
    uint8_t ss_x[32];
    if (!x25519_derive(priv, client_pub, ss_x)) die("X25519 derive");
    uint8_t ecdhe[MLKEM768_SS_LEN + 32]; size_t ecdhe_len;
    uint8_t mlkem_ct[MLKEM768_CT_LEN];
    if (hybrid) {                                      /* ss = ML-KEM_ss || X25519_ss */
        uint8_t ss_m[MLKEM768_SS_LEN];
        if (!mlkem768_encaps(client_ek, mlkem_ct, ss_m)) die("ML-KEM-768 encaps");
        memcpy(ecdhe, ss_m, MLKEM768_SS_LEN);
        memcpy(ecdhe + MLKEM768_SS_LEN, ss_x, 32);
        ecdhe_len = MLKEM768_SS_LEN + 32;              /* 64 */
    } else {
        memcpy(ecdhe, ss_x, 32); ecdhe_len = 32;
    }
    buf_t sh; buf_init(&sh);
    build_server_hello(&sh, pub, hybrid ? mlkem_ct : NULL, sid, sid_len);
    buf_add(&c.transcript, sh.data, sh.len);
    record_send_plain(fd, CT_HANDSHAKE, sh.data, sh.len, LEGACY_VERSION);
    buf_free(&sh);
    if (verbose) printf("[2] -> ServerHello\n");

    /* --- [4] handshake keys ---------------------------------------------- */
    if (verbose) hexdump("[3] (EC)DHE shared secret", ecdhe, ecdhe_len);
    ks_derive_handshake(&c, ecdhe, ecdhe_len);
    if (verbose) printf("[4] derived handshake traffic keys\n");

    /* middlebox-compat CCS, then the encrypted server flight (sec 4.3-4.4) -- */
    record_send_plain(fd, CT_CHANGE_CIPHER_SPEC, (const uint8_t *)"\x01", 1, LEGACY_VERSION);

    uint64_t s_seq = 0;                                /* server handshake epoch*/

    /* [5a] EncryptedExtensions */
    buf_t ee; buf_init(&ee); build_encrypted_extensions(&ee);
    buf_add(&c.transcript, ee.data, ee.len);
    record_send_enc(fd, c.s_hs_key, c.s_hs_iv, s_seq++, CT_HANDSHAKE, ee.data, ee.len);
    buf_free(&ee); if (verbose) printf("    [5a] -> EncryptedExtensions\n");

    /* [5b] Certificate */
    buf_t ct; buf_init(&ct); build_certificate(&ct, der, derlen);
    buf_add(&c.transcript, ct.data, ct.len);
    record_send_enc(fd, c.s_hs_key, c.s_hs_iv, s_seq++, CT_HANDSHAKE, ct.data, ct.len);
    buf_free(&ct); if (verbose) printf("    [5b] -> Certificate\n");

    /* [5c] CertificateVerify -- sign TH(ClientHello..Certificate) */
    uint8_t th_cert[HASH_LEN];
    sha384(c.transcript.data, c.transcript.len, th_cert);
    buf_t cv; buf_init(&cv); build_certificate_verify(&cv, key, th_cert);
    buf_add(&c.transcript, cv.data, cv.len);
    record_send_enc(fd, c.s_hs_key, c.s_hs_iv, s_seq++, CT_HANDSHAKE, cv.data, cv.len);
    buf_free(&cv); if (verbose) printf("    [5c] -> CertificateVerify (signed)\n");

    /* [5d] Finished -- MAC over TH(ClientHello..CertificateVerify) */
    uint8_t th_beforefin[HASH_LEN];
    sha384(c.transcript.data, c.transcript.len, th_beforefin);
    buf_t fin; buf_init(&fin); build_finished(&fin, c.s_hs_secret, th_beforefin);
    buf_add(&c.transcript, fin.data, fin.len);
    record_send_enc(fd, c.s_hs_key, c.s_hs_iv, s_seq++, CT_HANDSHAKE, fin.data, fin.len);
    buf_free(&fin); if (verbose) printf("    [5d] -> Finished\n");

    /* --- application keys (TH = ClientHello..server Finished) ------------- */
    ks_derive_application(&c);

    /* --- [6] read + verify client Finished -------------------------------- */
    uint8_t th_sf[HASH_LEN];                            /* CH..server Finished  */
    sha384(c.transcript.data, c.transcript.len, th_sf);
    uint8_t cfin_body[HASH_LEN]; size_t cfin_len;
    read_handshake_msg(&c, c.c_hs_key, c.c_hs_iv, 0, HS_FINISHED, cfin_body, &cfin_len);
    uint8_t cfin_key[HASH_LEN], expect[HASH_LEN];
    hkdf_expand_label(c.c_hs_secret, "finished", NULL, 0, cfin_key, HASH_LEN);
    hmac_sha384(cfin_key, HASH_LEN, th_sf, HASH_LEN, expect);
    if (cfin_len != HASH_LEN || CRYPTO_memcmp(expect, cfin_body, HASH_LEN) != 0)
        die("client Finished MAC INVALID");
    double hs_us = now_us() - t0;                      /* <-- handshake done   */
    if (verbose) printf("[6] client Finished MAC OK\n");

    /* --- [7] application data: read "ping", reply "pong" ------------------ */
    uint8_t pt[MAXREC]; uint64_t r_seq = 0;
    for (int i = 0; i < 10; i++) {
        if (record_read(fd, &type, &payload, &plen) < 0) die("read app data");
        if (type == CT_CHANGE_CIPHER_SPEC) { free(payload); continue; }
        uint8_t rh[5] = { type, (LEGACY_VERSION>>8)&0xFF, LEGACY_VERSION&0xFF,
                          (plen>>8)&0xFF, plen&0xFF };
        uint8_t it; size_t ol;
        if (record_decrypt(c.c_ap_key, c.c_ap_iv, r_seq++, rh,
                           payload, plen, &it, pt, &ol) != 0) die("decrypt app");
        free(payload);
        if (it == CT_APPLICATION_DATA) {
            if (verbose) printf("[7] <- application data: \"%.*s\"\n", (int)ol, pt);
            break;
        }
    }
    const char *reply = "pong";
    record_send_enc(fd, c.s_ap_key, c.s_ap_iv, 0, CT_APPLICATION_DATA,
                    (const uint8_t *)reply, strlen(reply));
    if (verbose) printf("[8] -> application data: \"%s\"\n", reply);

    /* close_notify alert (RFC 8446 section 6.1) -> clean shutdown           */
    uint8_t close_notify[2] = { 1, 0 };   /* warning(1), close_notify(0)      */
    record_send_enc(fd, c.s_ap_key, c.s_ap_iv, 1, CT_ALERT, close_notify, 2);

    close(fd);
    buf_free(&c.transcript);
    return hs_us;
}

/* ---- worker pool for the multi-threaded benchmark server ---------------- *
 * N threads each loop accept()+serve_one() on the shared listening socket;
 * the kernel load-balances accept() across them. The cert/key are read-only
 * and shared (OpenSSL 3.0 EVP_PKEY signing is safe for concurrent use).      */
static int            g_listen;
static const uint8_t *g_der; static int g_derlen;
static EVP_PKEY      *g_key;
static int            g_count;
static atomic_int     g_served;
static pthread_mutex_t g_out_mx = PTHREAD_MUTEX_INITIALIZER;

static void *srv_worker(void *arg)
{
    (void)arg;
    for (;;) {
        int fd = accept(g_listen, NULL, NULL);
        if (fd < 0) break;                  /* listen socket closed -> stop   */
        double hs_us = serve_one(fd, g_der, g_derlen, g_key, 0);
        int n = atomic_fetch_add(&g_served, 1) + 1;
        /* per-connection CSV to stdout (like tls_mini_server): pipe to a file */
        pthread_mutex_lock(&g_out_mx);
        printf("%d,TLS_AES_256_GCM_SHA384,X25519,%.3f\n", n, hs_us);
        pthread_mutex_unlock(&g_out_mx);
        if (n >= g_count) {
            shutdown(g_listen, SHUT_RDWR);  /* wakes accept() in other workers */
            break;
        }
    }
    return NULL;
}

int main(int argc, char **argv)
{
    /* usage: ./server [port] [count] [--threads N]
     *   count==1            -> single-connection demo (verbose trace)
     *   count>1, threads==1 -> single-threaded benchmark server
     *   count>1, threads==N -> N worker threads (multi-threaded server)       */
    int port    = argc > 1 ? atoi(argv[1]) : 8400;
    int count   = argc > 2 ? atoi(argv[2]) : 1;
    int threads = 1;
    const char *e;                                     /* repo "parameter law" */
    if ((e = getenv("TLS_TOTAL")))   count   = atoi(e);
    if ((e = getenv("TLS_THREADS"))) threads = atoi(e);
    for (int i = 3; i < argc; i++)
        if (!strcmp(argv[i], "--threads") && i + 1 < argc) threads = atoi(argv[++i]);
    if (count < 1) count = 1;
    if (threads < 1) threads = 1;

    signal(SIGPIPE, SIG_IGN);   /* a client that closes early must not kill us */

    /* --- load server cert (DER) + private key (EC P-256) ----------------- */
    FILE *f = fopen("server.crt", "r");
    if (!f) die("open server.crt");
    X509 *x = PEM_read_X509(f, NULL, NULL, NULL); fclose(f);
    if (!x) die("parse server.crt");
    uint8_t *der = NULL; int derlen = i2d_X509(x, &der);
    X509_free(x);

    f = fopen("server.key", "r");
    if (!f) die("open server.key");
    EVP_PKEY *key = PEM_read_PrivateKey(f, NULL, NULL, NULL); fclose(f);
    if (!key) die("parse server.key");

    /* --- listen ---------------------------------------------------------- */
    int ls = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1; setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(port);
    if (bind(ls, (struct sockaddr *)&a, sizeof a) < 0) die("bind");
    if (listen(ls, 128) < 0) die("listen");

    if (count == 1 && threads == 1) {                  /* ---- demo ---------- */
        printf("[*] listening on port %d (1 connection)\n", port);
        int fd = accept(ls, NULL, NULL);
        if (fd < 0) die("accept");
        printf("[*] accepted connection\n");
        serve_one(fd, der, derlen, key, 1);
        close(ls); EVP_PKEY_free(key); OPENSSL_free(der);
        printf("[OK] hand-rolled TLS 1.3 server finished.\n");
        return 0;
    }

    /* ---- benchmark server (single- or multi-threaded) ------------------- *
     * banner -> stderr; CSV header + rows -> stdout (pipe stdout to a file). */
    fprintf(stderr, "[*] listening on port %d  threads=%d  serving %d handshakes\n",
            port, threads, count);
    printf("conn,suite,group,handshake_us\n");
    fflush(stdout);
    g_listen = ls; g_der = der; g_derlen = derlen; g_key = key;
    g_count = count; atomic_store(&g_served, 0);

    pthread_t *tid = malloc(threads * sizeof(pthread_t));
    for (int i = 0; i < threads; i++) pthread_create(&tid[i], NULL, srv_worker, NULL);
    for (int i = 0; i < threads; i++) pthread_join(tid[i], NULL);
    free(tid);
    close(ls);

    EVP_PKEY_free(key); OPENSSL_free(der);
    fprintf(stderr, "[OK] served %d handshakes with %d thread%s.\n",
            count, threads, threads == 1 ? "" : "s");
    return 0;
}
