# Makefile - build the handshake client against OUR OpenSSL 3.6.2 (native ML-KEM).
# OPENSSL_DIR from the environment (setenv.sh exports OSSL_PREFIX).
OPENSSL_DIR ?= $(OSSL_PREFIX)

client.o: client.c
	$(CC) -g -Wall -Wextra -I$(OPENSSL_DIR)/include $< \
	  -L$(OPENSSL_DIR)/lib64 -L$(OPENSSL_DIR)/lib -lssl -lcrypto -o $@

clean:
	rm -f client.o
