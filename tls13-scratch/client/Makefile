# =============================================================================
# tls13-scratch/client/Makefile - TLS 1.3 client 
# Standard OpenSSL SSL_* API (not hand-rolled). Statically links the project
# OpenSSL 3.6.2 (native ML-KEM) so classical/hybrid/PQC groups are available.
# Usage:
#   make            # build ./client
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

client: main.c
	$(CC) $(CFLAGS) -o client main.c $(OSSL_A_SSL) -lpthread -ldl

clean:
	rm -f client

.PHONY: clean
