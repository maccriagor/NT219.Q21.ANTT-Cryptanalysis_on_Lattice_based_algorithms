# tls13-scratch/server/Makefile -- track D server, y chang illustrated-tls13.
OSSL ?= $(shell bash -c '. ../../scripts/versions.env 2>/dev/null && echo $$OSSL_PREFIX')
ifeq ($(strip $(OSSL)),)
  OSSL := $(HOME)/pqc/openssl
endif

CFLAGS  += -I$(OSSL)/include -Wall
LDFLAGS += -L$(OSSL)/lib64 -L$(OSSL)/lib -lssl -lcrypto -lpthread

server: main.c
	$(CC) $(CFLAGS) -o server main.c -Wall $(LDFLAGS)

# quick self-signed cert so the server has something to load. Any signature algo
# works; drop in $HOME/pqc/tls/mldsa65.cert.pem (+ key) to test PQC authentication.
cert:
	$(OSSL)/bin/openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-384 \
	  -nodes -keyout server.key -out server.crt -days 365 -subj /CN=localhost

clean:
	rm -f server

.PHONY: cert clean
