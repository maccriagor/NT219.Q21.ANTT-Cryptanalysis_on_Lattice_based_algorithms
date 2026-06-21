// client/main.c - TLS 1.3 client
//
// Usage: ./client [host] [port] [count] [group]
//   defaults: 127.0.0.1 8400 1 (OpenSSL default group list)
//   count==1 -> illustrated mode : keylog on, one handshake, ping/pong, 1 ms
//   count>1  -> bench mode       : silent, N handshakes, prints "t1,...,tN" (ms)
//   group    -> optional, e.g. X25519 | X25519MLKEM768 | MLKEM768
//               (standard SSL_CTX_set1_groups_list; to sweep classical/hybrid/PQC)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <netdb.h>
#include <time.h>
#include <signal.h>

#define NS_IN_MS 1000000.0
#define MS_IN_S 1000

void die(const char *str)
{
	fprintf(stderr, "%s: %s\n", str, strerror(errno));
	exit(1);
}

void init_openssl()
{
	SSL_load_error_strings();
	OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl()
{
	EVP_cleanup();
}

SSL_CTX *create_context()
{
	SSL_CTX *ctx = SSL_CTX_new(SSLv23_client_method());
	if (!ctx)
		die("Unable to create SSL context");
	SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
	return ctx;
}

void keylog_callback(const SSL *ssl, const char *line)
{
	printf("%s\n", line);
}

void configure_context(SSL_CTX *ctx, int with_keylog)
{
	SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
	SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
	if (with_keylog)
		SSL_CTX_set_keylog_callback(ctx, keylog_callback);
}

size_t resolve_hostname(const char *host, const char *port, struct sockaddr_storage *addr)
{
	struct addrinfo *res = 0;
	if (getaddrinfo(host, port, 0, &res) != 0)
		die("Unable to transform address");
	size_t len = res->ai_addrlen;
	memcpy(addr, res->ai_addr, len);
	freeaddrinfo(res);
	return len;
}

// One full connection. Times SSL_do_handshake and returns the time in ms.
// If pingpong != 0 it also writes "ping" / reads the reply (illustrated mode).
double one_connection(SSL_CTX *ctx, struct sockaddr_storage *addr, size_t len, int pingpong)
{
	int sock = socket(addr->ss_family, SOCK_STREAM, 0);
	if (sock < 0)
		die("Unable to create socket");
	if (connect(sock, (struct sockaddr *)addr, len) < 0)
		die("Unable to connect");

	SSL *ssl = SSL_new(ctx);
	SSL_set_fd(ssl, sock);
	SSL_set_tlsext_host_name(ssl, "example.ulfheim.net");
	SSL_set_connect_state(ssl);

	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
	if (SSL_do_handshake(ssl) <= 0) {
		ERR_print_errors_fp(stderr);
		die("Unable to do handshake");
	}
	clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
	double ms = ((t1.tv_sec - t0.tv_sec) * MS_IN_S)
	          + ((t1.tv_nsec - t0.tv_nsec) / NS_IN_MS);

	if (pingpong) {
		char wbuf[] = "ping";
		if (SSL_write(ssl, wbuf, strlen(wbuf)) <= 0) {
			ERR_print_errors_fp(stderr);
			die("Unable to write to server");
		}
		printf("Wrote [%s]\n", wbuf);
		char rbuf[128];
		int ret = SSL_read(ssl, rbuf, sizeof(rbuf) - 1);
		if (ret <= 0) {
			ERR_print_errors_fp(stderr);
			die("Unable to read from server");
		}
		rbuf[ret] = '\0';
		printf("Read [%s]\n", rbuf);
	}

	SSL_free(ssl);
	close(sock);
	return ms;
}

int main(int argc, char **argv)
{
	const char *host  = (argc > 1) ? argv[1] : "127.0.0.1";
	const char *port  = (argc > 2) ? argv[2] : "8400";
	long count        = (argc > 3) ? strtol(argv[3], 0, 10) : 1;
	const char *group = (argc > 4) ? argv[4] : 0;
	if (count < 1)
		count = 1;

	setenv("SERVER", "0", 1);   // no-op unless linked against instrumented OpenSSL
	signal(SIGPIPE, SIG_IGN);
	init_openssl();
	SSL_CTX *ctx = create_context();
	configure_context(ctx, /*with_keylog=*/count == 1);

	if (group && SSL_CTX_set1_groups_list(ctx, group) != 1) {
		fprintf(stderr, "bad group: %s\n", group);
		return 1;
	}

	struct sockaddr_storage addr;
	size_t len = resolve_hostname(host, port, &addr);

	double *ms = malloc(count * sizeof(*ms));
	for (long i = 0; i < count; i++)
		ms[i] = one_connection(ctx, &addr, len, /*pingpong=*/count == 1);

	for (long i = 0; i < count; i++)
		printf("%f%s", ms[i], (i + 1 < count) ? "," : "\n");

	free(ms);
	SSL_CTX_free(ctx);
	cleanup_openssl();
	return 0;
}
