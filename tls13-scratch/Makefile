# =============================================================================
# tls13-scratch/Makefile
# server + client on the standard SSL_* API (no hand-rolled protocol).
# Usage:
#   make            # build server + client
#   make cert       # self-signed server cert
#   make clean
# =============================================================================
.PHONY: all server client cert clean
all: server client
server: ; $(MAKE) -C server
client: ; $(MAKE) -C client
cert:   ; $(MAKE) -C server cert
clean:  ; -$(MAKE) -C server clean; -$(MAKE) -C client clean
