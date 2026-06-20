# tls13-scratch/Makefile -- build ca track D (server + client) + self-test.
# Goi de quy 3 Makefile con, giong cach repo cha goi cac thu muc con.
.PHONY: all server client repro cert clean
all: server client
server: ; $(MAKE) -C server
client: ; $(MAKE) -C client
repro:  ; $(MAKE) -C common repro
cert:   ; $(MAKE) -C server cert
clean:  ; -$(MAKE) -C server clean; -$(MAKE) -C client clean; -$(MAKE) -C common clean
