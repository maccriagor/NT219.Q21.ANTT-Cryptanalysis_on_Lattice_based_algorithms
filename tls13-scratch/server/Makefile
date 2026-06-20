# tls13-scratch/server/Makefile -- TLS 1.3 server tu cuon protocol (track D, NT219).
# LINK TINH: nhet libcrypto.a cua OpenSSL repo NT219 vao binary -> KHONG can .so /
# LD_LIBRARY_PATH / setenv.sh luc chay. Bo cuc theo illustrated-tls13/server/Makefile;
# OpenSSL tim qua scripts/versions.env (fallback ~/pqc/openssl; roi he thong dong).
OSSLROOT ?= $(shell bash -c '. ../../scripts/versions.env 2>/dev/null && echo $$OSSL_PREFIX')
ifeq ($(strip $(OSSLROOT)),)
  OSSLROOT := $(HOME)/pqc/openssl
endif
ifneq ($(wildcard $(OSSLROOT)/lib64/libcrypto.a),)
  OSSLLIB := $(OSSLROOT)/lib64
else ifneq ($(wildcard $(OSSLROOT)/lib/libcrypto.a),)
  OSSLLIB := $(OSSLROOT)/lib
endif
ifdef OSSLLIB
  CFLAGS += -I$(OSSLROOT)/include
  OSSL   := $(OSSLLIB)/libcrypto.a -ldl   # LINK TINH (track D chi dung libcrypto, KHONG -lssl)
else
  OSSL   := -lcrypto                      # fallback: OpenSSL he thong (dong) khi chay standalone
endif
COMMON  := ../common
CFLAGS  += -I$(COMMON) -Wall -Wextra -O2 -pthread
LDLIBS  := -lm

server: server.c $(COMMON)/tls13.c $(COMMON)/tls13.h
	$(CC) $(CFLAGS) -o server server.c $(COMMON)/tls13.c $(OSSL) $(LDLIBS)

# cert EC P-256 tu ky (ecdsa_secp256r1_sha256) -- chay 1 lan
cert server.crt server.key:
	openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 \
	    -keyout server.key -out server.crt -days 365 -nodes -subj "/CN=localhost"

clean:
	rm -f server
.PHONY: cert clean
