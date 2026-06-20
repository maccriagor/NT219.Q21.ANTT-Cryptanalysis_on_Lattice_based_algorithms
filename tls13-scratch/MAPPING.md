# MAPPING — hand-rolled TLS 1.3 (C) ↔ RFC 8446 ↔ illustrated-tls13 repo

Three source files:

| file        | role                                                              |
|-------------|-------------------------------------------------------------------|
| `tls13.h`   | shared declarations, protocol constants, the `buf_t` byte builder |
| `tls13.c`   | shared engine: HKDF, key schedule, AEAD record layer, X25519, I/O |
| `client.c`  | client handshake state machine + `main()`                         |
| `server.c`  | server handshake state machine + `main()`                         |

The illustrated-tls13 repo proves the handshake by driving **OpenSSL** through
`SSL_do_handshake()` / `SSL_accept()` (its `client/main.c` and `server/main.c`
are thin wrappers) and documenting the resulting bytes. We instead **roll the
protocol by hand** and borrow only the **primitives** from libcrypto — using the
exact OpenSSL calls the repo demonstrates in its per-primitive files.

---

## 1. Cryptographic primitives (borrowed from libcrypto)

| our code (`tls13.c`)                | RFC                     | repo reference |
|-------------------------------------|-------------------------|----------------|
| `sha384()`                          | FIPS 180-4              | (used throughout the site as "SHA384") |
| `hmac_sha384()`                     | RFC 2104                | `site/files/hkdf-384.sh` (the `openssl dgst -mac HMAC`) |
| `hkdf_extract()`                    | RFC 5869 §2.2           | `hkdf-384.sh` op `extract` (`PRK = HMAC(salt, IKM)`) |
| `hkdf_expand()` (static)            | RFC 5869 §2.3           | `hkdf-384.sh` op `expand` (`T(i)=HMAC(PRK, T(i-1)|info|i)`) |
| `hkdf_expand_label()`               | **RFC 8446 §7.1**       | `hkdf-384.sh` op `expandlabel` (HkdfLabel: `uint16 len`‖`"tls13 "+label`‖`context`) |
| `derive_secret()`                   | **RFC 8446 §7.1**       | `hkdf-384.sh` (Derive-Secret = expandlabel with transcript hash) |
| `x25519_keygen()` / `x25519_derive()` | RFC 7748 (§7.4.2 use) | `site/files/curve25519-mult.c` (DJB donna on the site; `EVP_PKEY` X25519 here) |
| `build_nonce()`                     | **RFC 8446 §5.3**       | `site/files/aes_256_gcm_encrypt.c` → `build_iv()` (nonce = iv XOR seq) |
| `aead_seal()`                       | RFC 5116 (§5.2 use)     | `site/files/aes_256_gcm_encrypt.c` (`EVP_aes_256_gcm`, set ivlen, AAD, GET_TAG) |
| `aead_open()`                       | RFC 5116 (§5.2 use)     | `site/files/aes_256_gcm_decrypt.c` (`SET_TAG` then `DecryptFinal` verifies) |

Principle ("from scratch" the way rustls/picotls/illustrated-tls13 mean it):
roll the **protocol**, never roll the **primitives**.

---

## 2. Record layer — RFC 8446 §5

| our code (`tls13.c`)        | what it does                                            | RFC      |
|-----------------------------|---------------------------------------------------------|----------|
| `record_send_plain()`       | `TLSPlaintext`: type‖legacy_version‖length‖fragment     | §5.1     |
| `record_read()`             | read 5-byte header + body off the socket                | §5.1     |
| `record_send_enc()`         | inner = `content‖type`; AAD = `0x17‖0x0303‖len`; AEAD-seal | §5.2  |
| `record_decrypt()`          | AEAD-open, strip zero padding, recover real content type| §5.2     |

Repo bytes to compare against: `captures/caps/serverenc*` (ciphertext records)
and the corresponding `*plain`/decrypted views on `site/index.html`.

---

## 3. Key schedule — RFC 8446 §7.1

| our code (`tls13.c`)         | secret produced                                  | RFC   |
|------------------------------|--------------------------------------------------|-------|
| `ks_derive_handshake()`      | Early Secret → derived → **Handshake Secret**; then `c hs traffic` / `s hs traffic` + their key/iv | §7.1, §7.3 |
| `ks_derive_application()`    | derived → **Master Secret**; then `c ap traffic` / `s ap traffic` + their key/iv | §7.1, §7.3 |
| `traffic_keys()` (static)    | `write_key`=Expand-Label(.,"key",.,16); `write_iv`=Expand-Label(.,"iv",.,12) | §7.3 |

The transcript hash fed into each `Derive-Secret` is `sha384(conn.transcript)`,
i.e. the concatenation of handshake messages (RFC §4.4.1). Compare the secrets
against `captures/keylog.txt` in the repo (SSLKEYLOGFILE format).

---

## 4. Handshake messages — RFC 8446 §4

| message               | our code                                            | RFC       | repo capture |
|-----------------------|-----------------------------------------------------|-----------|--------------|
| ClientHello           | `build_client_hello()` (client.c) / `parse_client_hello()` (server.c) | §4.1.2 | `captures/caps/clienthello` |
| ServerHello           | `build_server_hello()` (server.c) / `parse_server_hello()` (client.c) | §4.1.3 | `captures/caps/serverhello` |
| └ supported_versions  | client: `list<2..254>`; server: `selected_version`  | §4.2.1    | |
| └ supported_groups    | x25519 (`0x001D`)                                   | §4.2.7    | |
| └ signature_algorithms| ecdsa_secp256r1_sha256 (`0x0403`)                   | §4.2.3    | |
| └ key_share           | client: `client_shares<0..>`; server: one `KeyShareEntry` | §4.2.8 | |
| EncryptedExtensions   | `build_encrypted_extensions()` (empty)              | §4.3.1    | `serverencextensions` |
| Certificate           | `build_certificate()` / parsed inline in client.c   | §4.4.2    | `serverenccert` |
| CertificateVerify     | `build_certificate_verify()` (sign) / `verify_cert_verify()` (verify) | §4.4.3 | `serverenccertverify` |
| Finished              | `build_finished()` / verified inline both sides     | §4.4.4    | `serverencfinished`, `clientencfinished` |
| change_cipher_spec    | sent for middlebox compatibility, ignored on receipt| §5 / D.4  | `serverccs`, `clientccs` |

### CertificateVerify detail (§4.4.3)
Signed/verified content = `0x20 × 64` ‖ `"TLS 1.3, server CertificateVerify"`
‖ `0x00` ‖ `Transcript-Hash(ClientHello..Certificate)`.
- server signs it with the EC key via `EVP_DigestSign` + `EVP_sha256` →
  `ecdsa_secp256r1_sha256` (`build_certificate_verify`).
- client reconstructs the same bytes and checks it with `EVP_DigestVerify`
  against the public key pulled out of the leaf Certificate (`verify_cert_verify`).

### Finished detail (§4.4.4)
`finished_key = HKDF-Expand-Label(BaseKey, "finished", "", 32)`;
`verify_data  = HMAC(finished_key, Transcript-Hash(..))`.
BaseKey = `*_handshake_traffic_secret`. Server Finished covers `..CertificateVerify`;
client Finished covers `..server Finished`.

---

## 5. Overall flow — RFC 8446 §2, figure 1

`illustrated-tls13/client/main.c` and `server/main.c` do this whole dance inside
one `SSL_do_handshake()` / `SSL_accept()` call. Our `main()` in `client.c` /
`server.c` walks the same numbered steps explicitly:

```
[1] ClientHello                         §4.1.2
[2] ServerHello                         §4.1.3
[3] ECDHE shared secret                 §7.4.2
[4] key schedule -> handshake keys      §7.1
[5] {EncryptedExtensions}{Certificate}{CertificateVerify}{Finished}   §4.3-4.4
[6] verify peer (sig + Finished MAC)    §4.4.3 / §4.4.4
[7] {Finished}; application data        §4.4.4 / §2
```

---

## 6. Verification performed

| test                              | how                         | result |
|-----------------------------------|-----------------------------|--------|
| byte-fidelity vs repo + RFC       | `make repro` (`repro_keylog.c`) | X25519 public, ECDHE shared, and **both** handshake-traffic secrets reproduce `captures/keylog.txt` exactly — ALL MATCH |
| client.c ↔ server.c               | `./server 8400 & ./client`  | ping/pong; both Finished MACs + the ECDSA CertificateVerify all verify |
| OpenSSL client ↔ server.c         | `openssl s_client -tls1_3 -ciphersuites TLS_AES_256_GCM_SHA384 -groups X25519` | s_client accepts our ECDSA CertificateVerify, completes the handshake, gets `pong` |

`repro_keylog.c` is the strong one: feeding the deterministic inputs from the
repo's `openssl/instruments.patch` (client X25519 private key `0x20..0x3f`, the
published server public key, the `hello_hash` from the site) and running our RFC
8446 §7.1 key schedule reproduces the repo's `keylog.txt` secrets byte-for-byte.

---

## 7. Macrobenchmark layer (added for the NT219 capstone)

The handshake above is the *device under test*; these pieces are what the
capstone actually measures. They are **not** from the repo or the RFC — they are
the measurement harness wrapped around the hand-rolled handshake, kept minimal
and only as needed.

| piece                         | where           | what it measures |
|-------------------------------|-----------------|------------------|
| `run_session(..., t)`         | client.c        | one handshake, `CLOCK_MONOTONIC`-timed; fills a per-phase timing record |
| `--bench N`                   | client.c        | **full handshake latency**: TOTAL + per-phase (keygen / ECDHE / key-sched / sig-verify), min/median/mean/p95/max + 95% CI, CSV |
| `load_worker` + `--load --threads C --total M` | client.c | **throughput under concurrency**: C threads hammer the server; reports handshakes/sec + latency p50/p95/p99, CSV |
| `srv_worker` + `--threads N`  | server.c        | **single- vs multi-threaded server**: N worker threads each `accept()`+serve on the shared socket |

Notes that keep it honest:
- The four timed phases (keygen, ECDHE, key schedule, sig verify) are exactly the
  ones that change when X25519→ML-KEM and ECDSA→ML-DSA, so the same harness reports
  a PQC swap with no new measurement code.
- Throughput/latency *under load* is meaningful only on a multi-core host. Run the
  single- vs multi-threaded comparison where `nproc > 1`; on one core the worker
  pool is verified correct (every handshake's signature is checked) but cannot
  speed up.
- We do not use `wrk`: `wrk` drives HTTP, our endpoint is a minimal TLS ping/pong,
  so the concurrent harness is hand-written (`--load`).

How to know the numbers are right: (1) only handshakes that fully verify are
counted; `make repro` proves the crypto is byte-exact. (2) cross-check the phase
costs against `openssl speed ecdhx25519` / `ecdsap256` (same order; ours is a bit
higher because each handshake builds+frees the EVP objects once, a real
per-connection cost). (3) report median + CI over ≥1000 iters, idle box, pin a
core with `taskset`. (4) sanity-check TOTAL against `openssl s_time -tls1_3`.

---

## 8. Where this goes in the NT219 repo

This is **track D — the self-implemented server** (the professor's added
requirement), beside the repo's libssl-based RQ3 servers A (`s_server`), B
(`nginx`), C (`tls_mini_server.c`). A/B/C rent the whole TLS protocol from libssl;
track D implements the protocol itself (only the crypto primitives are libcrypto).

```
NT219.Q21.ANTT-Cryptanalysis_on_Lattice_based_algorithms/
├─ Makefile  src/  scripts/  data/  analysis_out/   # repo's primitive benchmarks + RQ3 servers
└─ tls13-scratch/                          # <-- this track D (layout like illustrated-tls13)
   ├─ common/   tls13.h  tls13.c  repro_keylog.c  Makefile   # shared engine + fidelity test
   ├─ server/   server.c  Makefile  server.crt  server.key   # make / make cert
   ├─ client/   client.c  Makefile                            # make
   ├─ Makefile  README.md  MAPPING.md                         # top-level recurses into the 3
   └─ (CSVs -> ../data/raw/<arch>/ ; from server//client/ that is ../../data/raw/<arch>/)
```
Each `server/Makefile` + `client/Makefile` mirrors `illustrated-tls13/server/Makefile`,
pointed at the NT219 repo's OpenSSL via `../../scripts/versions.env` (with `-Wl,-rpath`),
so track D links the same OpenSSL as `bench_evp` / `nginx`.

Same macro metric as A/B/C (handshake latency + throughput, single/multi-threaded)
but with the whole protocol under our control, so the classical↔PQC swap happens
in our code. The primitive microbenchmarks (RQ1/RQ2) time one crypto op; track D
times the full TLS 1.3 handshake those ops compose into.

---

## 9. Scope (RFC 8446 §9.1 baseline) and omissions

Implemented: TLS_AES_256_GCM_SHA384, X25519, server authentication with a real
signed+verified CertificateVerify, full key schedule, AEAD record layer.

Deliberately omitted (each a clean extension point): PSK / session resumption
(§2.2), 0-RTT (§2.3), HelloRetryRequest (§4.1.4), client authentication (§4.3.2),
KeyUpdate (§4.6.3), multiple cipher suites / groups, certificate **chain** trust
(we verify the CertificateVerify signature but do not walk a CA chain), record
padding.

To extend toward post-quantum (NT219): give ClientHello/ServerHello a hybrid
`key_share` (X25519 public ‖ ML-KEM public; server replies X25519 public ‖
ML-KEM ciphertext), concatenate the two shared secrets into the single
`hkdf_extract` in `ks_derive_handshake`, and borrow the ML-KEM primitive from
liboqs. The message framing and key schedule are unchanged.
