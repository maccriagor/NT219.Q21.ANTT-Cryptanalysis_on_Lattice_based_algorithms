/*
 * client.c - TLS 1.3 handshake timing client for nginx (direct, no emulation).
 *
 * Same method as the canonical PQC-TLS benchmark clients (s_timer.c in
 * xvzcf/pq-tls-benchmark, Paquin-Stebila-Tamvada eprint 2019/1447; tls_timer in
 * OpenSSLNTRU arXiv 2106.08759): BIO_s_connect -> SSL_new -> SSL_connect,
 * bracketed by CLOCK_MONOTONIC_RAW. Links our OpenSSL 3.6.2 (native ML-KEM).
 *
 * Prints handshake times in ms as one comma-separated line "t1,t2,...,tN".
 * Usage: ./client.o <group> <measurements> [host:port]   (default 127.0.0.1:4433)
 */
#include <stdio.h>
#include <stdlib.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <time.h>

#define NS_IN_MS 1000000.0
#define MS_IN_S 1000

static SSL *do_tls_handshake(SSL_CTX *ctx, const char *host)
{
    BIO *conn = BIO_new(BIO_s_connect());
    if (!conn)
        return 0;
    BIO_set_conn_hostname(conn, host);
    BIO_set_conn_mode(conn, BIO_SOCK_NODELAY);

    SSL *ssl = SSL_new(ctx);
    SSL_set_bio(ssl, conn, conn);
    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        return 0;
    }
    return ssl;
}

int main(int argc, char *argv[])
{
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <group> <measurements> [host:port]\n", argv[0]);
        return -1;
    }
    const char *group = argv[1];
    const size_t to_make = strtol(argv[2], 0, 10);
    const char *host = (argc == 4) ? argv[3] : "127.0.0.1:4433";
    size_t made = 0;

    struct timespec start, finish;
    double *times_ms = malloc(to_make * sizeof(*times_ms));

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) { ERR_print_errors_fp(stderr); return -1; }

    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
    SSL_CTX_set_quiet_shutdown(ctx, 1);
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION);
    SSL_CTX_set_ciphersuites(ctx, "TLS_AES_256_GCM_SHA384");

    if (SSL_CTX_set1_groups_list(ctx, group) != 1) {
        fprintf(stderr, "bad group: %s\n", group);
        return -1;
    }
    if (SSL_CTX_load_verify_locations(ctx, "ca.crt", 0) != 1) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    while (made < to_make) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        SSL *ssl = do_tls_handshake(ctx, host);
        clock_gettime(CLOCK_MONOTONIC_RAW, &finish);
        if (!ssl)
            continue;
        SSL_set_shutdown(ssl, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
        BIO_closesocket(SSL_get_fd(ssl));
        SSL_free(ssl);
        times_ms[made] = ((finish.tv_sec - start.tv_sec) * MS_IN_S)
                       + ((finish.tv_nsec - start.tv_nsec) / NS_IN_MS);
        made++;
    }

    for (size_t i = 0; i < made - 1; i++)
        printf("%f,", times_ms[i]);
    printf("%f", times_ms[made - 1]);

    free(times_ms);
    SSL_CTX_free(ctx);
    return 0;
}
