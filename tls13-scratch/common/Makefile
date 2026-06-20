# tls13-scratch/common/Makefile -- engine dung chung + self-test fidelity (LINK TINH).
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
CFLAGS  += -Wall -Wextra -O2 -pthread
LDLIBS  := -lm

# self-test: tai lap illustrated-tls13/captures/keylog.txt dung tung byte
repro: repro_keylog.c tls13.c tls13.h
	$(CC) $(CFLAGS) -o repro_keylog repro_keylog.c tls13.c $(OSSL) $(LDLIBS)
	./repro_keylog

clean:
	rm -f repro_keylog
.PHONY: repro clean
