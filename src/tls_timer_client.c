/* ============================================================================
 * tls_timer_client.c - Self-implemented TLS 1.3 timing client (WP4).
 *
 * The in-process counterpart to tls_mini_server.c: instead of spawning
 * `openssl s_client` once per handshake (fork/exec overhead on every probe),
 * this opens N handshakes inside ONE process and times each SSL_connect with
 * CLOCK_MONOTONIC_RAW - the same method as bench_evp and as s_timer.c in
 * Paquin-Stebila-Tamvada (eprint 2019/1447), whose do_tls_handshake() this
 * mirrors (BIO_s_connect -> SSL_new -> SSL_set_bio -> SSL_connect).
 *
 * Why it exists: removes the per-handshake process-spawn cost, giving cleaner
 * ABSOLUTE latency for the report (the s_client loop is fine for DIFFERENCES
 * between groups on one machine, but carries a constant spawn tax).
 *
 * Prints one CSV line per handshake: iter,group,connect_us  + a final median
 * to stderr. Pair it with any of the three WP4 servers (s_server / nginx /
 * tls_mini_server); for nginx point --port at the running nginx.
 *
 * Build: make tlsclient
 * Run  : source scripts/setenv.sh
 *        ./build/tls_timer_client 127.0.0.1 8443 X25519MLKEM768 50 ca.pem
 * ========================================================================== */
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int cmp_long(const void *a, const void *b) {
    long x = *(const long *)a, y = *(const long *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr,
            "Usage: %s host port group iters [ca.pem]\n"
            "  e.g. %s 127.0.0.1 8443 X25519MLKEM768 50 ca.pem\n",
            argv[0], argv[0]);
        return 1;
    }
    const char *host = argv[1], *port = argv[2], *group = argv[3];
    long iters = atol(argv[4]);
    const char *ca = (argc > 5) ? argv[5] : NULL;
    char target[256];
    snprintf(target, sizeof target, "%s:%s", host, port);

    /* ---- TLS 1.3-only client context, force ONE group (the variable) ---- */
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == NULL
        || SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION) != 1
        || SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION) != 1) {
        ERR_print_errors_fp(stderr);
        return 1;
    }
    if (SSL_CTX_set1_groups_list(ctx, group) != 1) {
        fprintf(stderr, "FATAL: this OpenSSL rejected group \"%s\"\n", group);
        ERR_print_errors_fp(stderr);
        return 1;
    }
    if (ca != NULL && SSL_CTX_load_verify_locations(ctx, ca, NULL) != 1) {
        fprintf(stderr, "FATAL: cannot load CA file %s\n", ca);
        return 1;
    }

    long *samples = calloc(iters, sizeof(long));
    long done = 0;
    printf("iter,group,connect_us\n");
    for (long i = 0; i < iters; i++) {
        /* Mirror s_timer.c's do_tls_handshake: BIO connect -> SSL_connect. */
        BIO *conn = BIO_new(BIO_s_connect());
        BIO_set_conn_hostname(conn, target);
        BIO_set_nbio(conn, 0);
        SSL *ssl = SSL_new(ctx);
        SSL_set_bio(ssl, conn, conn);

        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
        int ok = SSL_connect(ssl);                 /* full TLS 1.3 handshake */
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

        if (ok == 1) {
            long us = (t1.tv_sec - t0.tv_sec) * 1000000L
                    + (t1.tv_nsec - t0.tv_nsec) / 1000L;
            const char *neg = SSL_group_to_name(ssl, SSL_get_negotiated_group(ssl));
            printf("%ld,%s,%ld\n", i + 1, neg ? neg : group, us);
            samples[done++] = us;
        } else {
            ERR_print_errors_fp(stderr);
        }
        SSL_shutdown(ssl);
        SSL_free(ssl);                             /* frees the BIO too */
    }
    fflush(stdout);

    if (done > 0) {
        qsort(samples, done, sizeof(long), cmp_long);
        long med = samples[done / 2];
        fprintf(stderr, "median_connect_us=%ld over %ld handshakes (group=%s)\n",
                med, done, group);
    }
    free(samples);
    SSL_CTX_free(ctx);
    return done == iters ? 0 : 2;
}
