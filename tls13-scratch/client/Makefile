# tls13-scratch/client/Makefile -- TLS 1.3 client tu cuon protocol (track D, NT219).
# LINK TINH libcrypto.a (xem ghi chu server/Makefile). Cung style illustrated-tls13.
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
  OSSL   := $(OSSLLIB)/libcrypto.a -ldl
else
  OSSL   := -lcrypto
endif
COMMON  := ../common
CFLAGS  += -I$(COMMON) -Wall -Wextra -O2 -pthread
LDLIBS  := -lm

client: client.c $(COMMON)/tls13.c $(COMMON)/tls13.h
	$(CC) $(CFLAGS) -o client client.c $(COMMON)/tls13.c $(OSSL) $(LDLIBS)

clean:
	rm -f client
.PHONY: clean
