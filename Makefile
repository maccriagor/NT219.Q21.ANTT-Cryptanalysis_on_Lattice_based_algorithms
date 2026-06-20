# =============================================================================
# bench_evp from src/*.cpp, statically linked against a custom OpenSSL (ML-KEM/ML-DSA).
# OpenSSL path comes from scripts/versions.env; the scripts' openssl CLI needs setenv.sh.
# Run it:
#   make             build bench_evp
#   make bench       run all algos -> data/summary_micro_<arch>.csv
#   make memory      peak RSS per algo -> data/memory_<arch>.csv
#   make codesize    crypto lib sizes -> data/codesize_<arch>.csv
#   make tls         TLS 1.3 handshakes -> data/tls_handshake_<arch>.csv
#   make bench_oqs   build liboqs-direct micro-bench -> build/bench_oqs_{ref,opt}
#   make oqs         bench_oqs + run ref-vs-opt sweep -> data/bench_oqs_<arch>.csv
#   make tlsnetem    TLS 1.3 over netem RTT/loss (needs root + sch_netem)
#   make analyze     aggregate everything into analysis_out/
#   make help        list these targets
#   make DEBUG=1     debug build (-O0)
#   make OSSLROOT=/path/to/openssl  pick a specific OpenSSL
# =============================================================================

SRCDIR := src
BINDIR := build
EXEC := $(BINDIR)/bench_evp
# bench_oqs.cpp links liboqs and has its OWN main() -> EXCLUDE here; build it with
# the separate `bench_oqs` target below (else: 2 main() + missing <oqs/oqs.h>).
SOURCES := $(filter-out $(SRCDIR)/bench_oqs.cpp,$(wildcard $(SRCDIR)/*.cpp))
OBJECTS := $(patsubst $(SRCDIR)/%.cpp,$(BINDIR)/%.o,$(SOURCES))

CXX ?= g++
WARNINGS := -Wall
RELEASE := -O3 -g2 -DNDEBUG
DEBUGOPT := -g2 -O0

CXXFLAGS += $(WARNINGS) -pthread
LDFLAGS += -pthread
LDLIBS += -lm -ldl

ifdef DEBUG
  CXXFLAGS += $(DEBUGOPT)
  LDFLAGS += -g
else
  CXXFLAGS += $(RELEASE)
endif

ROOT_DIR := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
OSSLROOT ?= $(shell bash -c '. "$(ROOT_DIR)scripts/versions.env" 2>/dev/null && echo "$$OSSL_PREFIX"')

ifeq ($(strip $(OSSLROOT)),)
  $(error Cannot determine OSSLROOT. Run make from the repo root, or pass OSSLROOT=/path/to/openssl)
endif

ifneq ($(wildcard $(OSSLROOT)/lib64),)
  OSSLLIBDIR := $(OSSLROOT)/lib64
else
  OSSLLIBDIR := $(OSSLROOT)/lib
endif

CPPFLAGS += -I$(OSSLROOT)/include
LDFLAGS += -L$(OSSLLIBDIR)

OSSL_A := $(OSSLLIBDIR)/libcrypto.a
OSSL_A_SSL := $(OSSLLIBDIR)/libssl.a $(OSSLLIBDIR)/libcrypto.a

.PHONY: all clean distclean
.DEFAULT_GOAL := all

all: $(EXEC)

$(EXEC): $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ $(OSSL_A) $(LDLIBS) -o $@

$(BINDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BINDIR)

distclean: clean
	rm -rf analysis_out *.log core

.PHONY: bench memory codesize
bench: $(EXEC)
	scripts/run_micro.sh

memory: $(EXEC)
	scripts/measure_memory.sh

codesize:
	scripts/measure_codesize.sh

.PHONY: tls analyze tlsnetem
tls:
	scripts/bench_tls.sh

tlsnetem:
	cd tls13-scratch && ./bench_netem.sh

analyze:
	python3 scripts/analyze.py

.PHONY: tlsmini tlsclient
tlsmini: $(BINDIR)/tls_mini_server
tlsclient: $(BINDIR)/tls_timer_client

$(BINDIR)/tls_mini_server: $(SRCDIR)/tls_mini_server.c
	@mkdir -p $(BINDIR)
	$(CC) $(CPPFLAGS) -Wall -O2 -o $@ $< $(LDFLAGS) $(OSSL_A_SSL) $(LDLIBS)

$(BINDIR)/tls_timer_client: $(SRCDIR)/tls_timer_client.c
	@mkdir -p $(BINDIR)
	$(CC) $(CPPFLAGS) -Wall -O2 -o $@ $< $(LDFLAGS) $(OSSL_A_SSL) $(LDLIBS)

# liboqs-direct micro-bench: the SAME bench_oqs.cpp built against BOTH trees
# (ref = portable C, opt = SIMD) -> build/bench_oqs_{ref,opt}. Prefixes from
# versions.env. Drive both with `make oqs` (or scripts/run_oqs.sh).
LIBOQS_REF := $(shell bash -c '. "$(ROOT_DIR)scripts/versions.env" 2>/dev/null && echo "$$LIBOQS_PREFIX_REF"')
LIBOQS_OPT := $(shell bash -c '. "$(ROOT_DIR)scripts/versions.env" 2>/dev/null && echo "$$LIBOQS_PREFIX_OPT"')
OQSFLAGS := -O2 -Wall -pthread

.PHONY: bench_oqs oqs
bench_oqs: $(BINDIR)/bench_oqs_ref $(BINDIR)/bench_oqs_opt

# build BOTH binaries, then run the ref-vs-opt sweep -> data/bench_oqs_<arch>.csv
oqs: bench_oqs
	scripts/run_oqs.sh

$(BINDIR)/bench_oqs_ref: $(SRCDIR)/bench_oqs.cpp
	@mkdir -p $(BINDIR)
	@[ -d "$(LIBOQS_REF)/include" ] || { echo "liboqs ref missing at $(LIBOQS_REF) (run scripts/build_liboqs.sh ref)"; exit 1; }
	$(CXX) $(OQSFLAGS) $< -I$(LIBOQS_REF)/include -L$(LIBOQS_REF)/lib -loqs -Wl,-rpath,$(LIBOQS_REF)/lib -lm -o $@

$(BINDIR)/bench_oqs_opt: $(SRCDIR)/bench_oqs.cpp
	@mkdir -p $(BINDIR)
	@[ -d "$(LIBOQS_OPT)/include" ] || { echo "liboqs opt missing at $(LIBOQS_OPT) (run scripts/build_liboqs.sh opt)"; exit 1; }
	$(CXX) $(OQSFLAGS) $< -I$(LIBOQS_OPT)/include -L$(LIBOQS_OPT)/lib -loqs -Wl,-rpath,$(LIBOQS_OPT)/lib -lm -o $@

.PHONY: help
help:
	@grep -E '^#   make ' $(firstword $(MAKEFILE_LIST)) | sed 's/^#   /  /'