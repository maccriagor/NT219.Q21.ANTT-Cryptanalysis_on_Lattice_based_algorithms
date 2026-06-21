# =============================================================================
# tls13-scratch/server/Makefile - Track D TLS 1.3 server.
#
# Standard OpenSSL SSL_* API; statically links the project OpenSSL 3.6.2.
# Usage:
#   make            # build ./server
#   make cert       # self-signed P-384 cert (or drop in an ML-DSA cert)
# =============================================================================
OSSL ?= $(shell bash -c '. ../../scripts/versions.env 2>/dev/null && echo $$OSSL_PREFIX')
ifeq ($(strip $(OSSL)),)
  OSSL := $(HOME)/pqc/openssl
endif

# x86_64 installs to lib64, ARM to lib.
ifneq ($(wildcard $(OSSL)/lib64),)
  OSSLLIBDIR := $(OSSL)/lib64
else
  OSSLLIBDIR := $(OSSL)/lib
endif

# Static archives (libssl + libcrypto); no -lssl/-lcrypto, no rpath.
OSSL_A_SSL := $(OSSLLIBDIR)/libssl.a $(OSSLLIBDIR)/libcrypto.a

CFLAGS += -I$(OSSL)/include -Wall

server: main.c
	$(CC) $(CFLAGS) -o server main.c $(OSSL_A_SSL) -lpthread -ldl

# quick self-signed cert so the server has something to load. Any signature algo
# works; drop in $HOME/pqc/tls/mldsa65.cert.pem (+ key) to test PQC authentication.
cert:
	$(OSSL)/bin/openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-384 \
	  -nodes -keyout server.key -out server.crt -days 365 -subj /CN=localhost

clean:
	rm -f server

.PHONY: cert clean
