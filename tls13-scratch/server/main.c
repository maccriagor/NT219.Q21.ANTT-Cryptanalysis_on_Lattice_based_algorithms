// Track D server -- y chang syncsynchalt/illustrated-tls13 (server/main.c).
// Base file from https://wiki.openssl.org/index.php/Simple_TLS_Server
// licensed via OpenSSL License https://www.openssl.org/source/license.html
//
// Standard OpenSSL TLS 1.3 server (the SSL_* API), NOT a hand-rolled protocol.
// Links the project OpenSSL 3.6.2, so its OpenSSL >=3.5 default group list
// already accepts X25519MLKEM768 / MLKEM768 -- the client decides which group.
// Drop in an ML-DSA cert (e.g. $HOME/pqc/tls/mldsa65.cert.pem) to test PQC auth.
//
// Usage: ./server [port] [cert] [key]   (defaults: 8400 server.crt server.key)

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
#include <signal.h>

void die(const char *str)
{
	fprintf(stderr, "%s: %s\n", str, strerror(errno));
	exit(1);
}

int create_listen(int port)
{
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		die("Unable to create socket");
	int enable = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
		die("SO_REUSEADDR failed");
	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		die("Unable to bind to port");
	if (listen(s, 1) < 0)
		die("Unable to listen on port");
	printf("Listening on port %d\n", port);
	return s;
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
	SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
	if (!ctx)
		die("Unable to create SSL context");
	SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
	return ctx;
}

void keylog_callback(const SSL *ssl, const char *line)
{
	printf("%s\n", line);
}

void configure_context(SSL_CTX *ctx, const char *cert, const char *key)
{
	if (!SSL_CTX_set_ecdh_auto(ctx, 1))
		die("Unable to set ECDH curves");
	/* Offer every group we benchmark, incl. PURE ML-KEM (NOT in OpenSSL's
	   default list) so a client proposing only MLKEM768 can negotiate.
	   The file header comment is wrong for pure ML-KEM: the default list
	   carries hybrid X25519MLKEM768 but not standalone MLKEM*. */
	if (!SSL_CTX_set1_groups_list(ctx,
		"X25519MLKEM768:SecP256r1MLKEM768:SecP384r1MLKEM1024:"
		"MLKEM512:MLKEM768:MLKEM1024:"
		"x25519:x448:secp256r1:secp384r1:secp521r1"))
		die("Unable to set TLS groups");
	if (!SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM))
		die("Unable to load cert");
	if (!SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM))
		die("Unable to load key");
	SSL_CTX_set_keylog_callback(ctx, keylog_callback);
}

int main(int argc, char **argv)
{
	int port        = (argc > 1) ? atoi(argv[1]) : 8400;
	const char *crt = (argc > 2) ? argv[2] : "server.crt";
	const char *key = (argc > 3) ? argv[3] : "server.key";

	setenv("SERVER", "1", 1);   // no-op unless linked against instrumented OpenSSL
	// A latency client closes right after the handshake; TLS 1.3 then makes the
	// server write a NewSessionTicket into a closed socket -> EPIPE. Ignore
	// SIGPIPE so that returns an error instead of killing the server.
	signal(SIGPIPE, SIG_IGN);
	init_openssl();
	SSL_CTX *ctx = create_context();
	configure_context(ctx, crt, key);

	int sock = create_listen(port);

	// serve forever so the latency client can loop many handshakes
	for (;;) {
		struct sockaddr_in addr;
		socklen_t len = sizeof(addr);
		int client = accept(sock, (struct sockaddr *)&addr, &len);
		if (client < 0)
			continue;

		SSL *ssl = SSL_new(ctx);
		SSL_set_fd(ssl, client);
		if (SSL_accept(ssl) <= 0) {
			ERR_print_errors_fp(stderr);
		} else {
			char rbuf[128];
			int ret = SSL_read(ssl, rbuf, sizeof(rbuf) - 1);
			if (ret > 0) {
				rbuf[ret] = '\0';
				const char reply[] = "pong";
				SSL_write(ssl, reply, strlen(reply));
			}
		}
		SSL_free(ssl);
		close(client);
	}

	close(sock);
	SSL_CTX_free(ctx);
	cleanup_openssl();
	return 0;
}