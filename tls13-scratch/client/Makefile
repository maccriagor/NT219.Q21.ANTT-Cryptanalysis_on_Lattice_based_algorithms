# tls13-scratch/client/Makefile -- track D client, y chang illustrated-tls13.
# Standard OpenSSL TLS 1.3 client (NOT hand-rolled). Links the project OpenSSL
# 3.6.2 (native ML-KEM) so classical/hybrid/PQC groups are available.
OSSL ?= $(shell bash -c '. ../../scripts/versions.env 2>/dev/null && echo $$OSSL_PREFIX')
ifeq ($(strip $(OSSL)),)
  OSSL := $(HOME)/pqc/openssl
endif

CFLAGS  += -I$(OSSL)/include -Wall
LDFLAGS += -L$(OSSL)/lib64 -L$(OSSL)/lib -lssl -lcrypto -lpthread

client: main.c
	$(CC) $(CFLAGS) -o client main.c -Wall $(LDFLAGS)

clean:
	rm -f client

.PHONY: clean
