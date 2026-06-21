# tls13-scratch/Makefile -- track D (y chang illustrated-tls13): thin OpenSSL
# server + client, both on the standard SSL_* API (no hand-rolled protocol).
.PHONY: all server client cert clean
all: server client
server: ; $(MAKE) -C server
client: ; $(MAKE) -C client
cert:   ; $(MAKE) -C server cert
clean:  ; -$(MAKE) -C server clean; -$(MAKE) -C client clean
