/* ============================================================================
 * tls_mini_server.c - Self-implemented HTTPS server: OUR TCP + OUR HTTP,
 * TLS 1.3 (enforced) configured through OpenSSL libssl.
 *
 * Role in the project (brief 7.6 "integrate Kyber/Dilithium into an
 * application (e.g., small HTTPS server) and measure end-to-end"):
 *   - third integration tier: library (bench_evp) -> THIS -> nginx;
 *   - server-side handshake timing: CLOCK_MONOTONIC_RAW around SSL_accept
 *     (same clock as bench_evp and as s_timer.c in Paquin-Stebila-Tamvada,
 *     eprint 2019/1447), printed per connection as CSV-ish lines;
 *   - prints the NEGOTIATED group per connection -> hard evidence that a
 *     given run really used MLKEM768 / X25519MLKEM768.
 *
 * Pattern follows the official OpenSSL guide demo (demos/guide/
 * tls-server-block.c) and demos/sslecho. TLS 1.3 only via
 * SSL_CTX_set_min_proto_version. PQC groups: by default the linked
 * OpenSSL's own list is used (>= 3.5 already prefers X25519MLKEM768);
 * set TLS_GROUPS=... to force, e.g. TLS_GROUPS=MLKEM768 (fails loudly on
 * an OpenSSL that lacks the group - same behaviour nginx showed).
 *
 * Build: make tlsmini          (links OUR OpenSSL via the same -I/-L flags)
 * Run  : source scripts/setenv.sh
 *        ./build/tls_mini_server ~/pqc/tls/mldsa65.cert.pem \
 *                                ~/pqc/tls/mldsa65.key.pem 9443
 * Test : curl -k https://127.0.0.1:9443/
 * ========================================================================== */
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stop;
static void on_signal(int sig) { (void)sig; stop = 1; }

static long us_since(const struct timespec *a, const struct timespec *b) {
    return (b->tv_sec - a->tv_sec) * 1000000L + (b->tv_nsec - a->tv_nsec) / 1000L;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s cert.pem key.pem [port=9443]\n", argv[0]);
        return 1;
    }
    int port = (argc > 3) ? atoi(argv[3]) : 9443;
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);

    /* ---- TLS context: cert/key, TLS 1.3 ONLY, optional forced groups ---- */
    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if (ctx == NULL
        || SSL_CTX_use_certificate_chain_file(ctx, argv[1]) <= 0
        || SSL_CTX_use_PrivateKey_file(ctx, argv[2], SSL_FILETYPE_PEM) <= 0
        || SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION) != 1) {
        ERR_print_errors_fp(stderr);
        return 1;
    }
    const char *groups = getenv("TLS_GROUPS");
    if (groups != NULL && SSL_CTX_set1_groups_list(ctx, groups) != 1) {
        fprintf(stderr, "FATAL: this OpenSSL rejected TLS_GROUPS=\"%s\"\n", groups);
        ERR_print_errors_fp(stderr);
        return 1;
    }

    /* ---- OUR TCP layer ---------------------------------------------------- */
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int on = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    /* Bind address: loopback by default (safe); set BIND_ALL=1 to listen on
     * 0.0.0.0 for LAN benchmarking from another host. */
    addr.sin_addr.s_addr = (getenv("BIND_ALL") && getenv("BIND_ALL")[0] == '1')
                         ? htonl(INADDR_ANY) : htonl(INADDR_LOOPBACK);
    if (bind(srv, (struct sockaddr *)&addr, sizeof addr) < 0 || listen(srv, 64) < 0) {
        perror("bind/listen");
        return 1;
    }
    fprintf(stderr, "tls_mini_server: https://127.0.0.1:%d (TLS 1.3 only%s%s)\n",
            port, groups ? ", groups=" : "", groups ? groups : "");
    printf("conn,tls,group,handshake_us\n");
    fflush(stdout);

    /* ---- Serve: one full handshake per connection (Connection: close) ---- */
    static const char body[] = "ok\n";
    char resp[160];
    int n = snprintf(resp, sizeof resp,
                     "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n"
                     "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
                     sizeof body - 1, body);
    long conn = 0;
    while (!stop) {
        int c = accept(srv, NULL, NULL);
        if (c < 0)
            break;                              /* EINTR on SIGINT/SIGTERM */
        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, c);

        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
        int ok = SSL_accept(ssl);               /* the TLS 1.3 handshake   */
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

        if (ok == 1) {
            printf("%ld,%s,%s,%ld\n", ++conn, SSL_get_version(ssl),
                   SSL_group_to_name(ssl, SSL_get_negotiated_group(ssl)),
                   us_since(&t0, &t1));
            fflush(stdout);
            /* Minimal HTTP: read request head, answer, close. */
            char buf[2048];
            int r, got = 0;
            while ((r = SSL_read(ssl, buf + got, (int)sizeof buf - got - 1)) > 0) {
                got += r;
                buf[got] = '\0';
                if (strstr(buf, "\r\n\r\n") != NULL || got >= (int)sizeof buf - 1)
                    break;
            }
            SSL_write(ssl, resp, n);
            SSL_shutdown(ssl);
        }
        SSL_free(ssl);
        close(c);
    }
    close(srv);
    SSL_CTX_free(ctx);
    fprintf(stderr, "tls_mini_server: stopped after %ld connections\n", conn);
    return 0;
}
