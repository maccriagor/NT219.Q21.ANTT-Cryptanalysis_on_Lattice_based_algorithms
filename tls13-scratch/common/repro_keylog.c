/* =============================================================================
 * repro_keylog.c  --  fidelity self-test against illustrated-tls13.
 *
 * Proves this code reproduces the repo's published values BYTE-FOR-BYTE, using
 * the deterministic inputs the repo bakes in via openssl/instruments.patch:
 *
 *   client ephemeral X25519 private key = 0x20 0x21 ... 0x3f   (the patch)
 *   server ephemeral X25519 public  key = site/files/server-ephemeral-public.key
 *   hello_hash = SHA384(ClientHello || ServerHello)  (published on the site)
 *
 * It then runs the exact RFC 8446 section 7.1 key schedule and checks the
 * handshake traffic secrets against illustrated-tls13/captures/keylog.txt.
 *
 * Build:  make repro      Run: ./repro_keylog
 * ===========================================================================*/
#include "tls13.h"
#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>

/* ---- reference values straight from the repo ---------------------------- */
static const char *REPO_CLIENT_PUB =
    "358072d6365880d1aeea329adf9121383851ed21a28e3b75e965d0d2cd166254";
static const char *REPO_SERVER_PUB =
    "9fd7ad6dcff4298dd3f96d5b1b2af910a0535b1488d7f8fabb349a982880b615";
static const char *REPO_SHARED =        /* X25519(client_priv, server_pub)   */
    "df4a291baa1eb7cfa6934b29b474baad2697e29f1f920dcc77c8a0a088447624";
static const char *REPO_HELLO_HASH =    /* SHA384(ClientHello || ServerHello) */
    "e05f64fcd082bdb0dce473adf669c2769f257a1c75a51b7887468b5e0e7a7de4"
    "f4d34555112077f16e079019d5a845bd";
static const char *KEYLOG_CLIENT_HS =   /* CLIENT_HANDSHAKE_TRAFFIC_SECRET     */
    "db89d2d6df0e84fed74a2288f8fd4d0959f790ff23946cdf4c26d85e51bebd42"
    "ae184501972f8d30c4a3e4a3693d0ef0";
static const char *KEYLOG_SERVER_HS =   /* SERVER_HANDSHAKE_TRAFFIC_SECRET     */
    "23323da031634b241dd37d61032b62a4f450584d1f7f47983ba2f7cc0cdcc39a"
    "68f481f2b019f9403a3051908a5d1622";

/* ---- tiny hex helpers --------------------------------------------------- */
static void unhex(const char *h, uint8_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) sscanf(h + 2 * i, "%2hhx", &out[i]);
}
static int check(const char *name, const uint8_t *got, const char *expect_hex, size_t n)
{
    uint8_t exp[64]; unhex(expect_hex, exp, n);
    int ok = memcmp(got, exp, n) == 0;
    printf("  %-28s %s\n", name, ok ? "MATCH" : "*** MISMATCH ***");
    if (!ok) {
        printf("    got   : "); for (size_t i=0;i<n;i++) printf("%02x", got[i]); printf("\n");
        printf("    expect: %s\n", expect_hex);
    }
    return ok;
}

int main(void)
{
    int all = 1;
    printf("illustrated-tls13 fidelity check (TLS_AES_256_GCM_SHA384):\n");

    /* (1) X25519 public key from the repo's fixed client private key.        */
    uint8_t client_priv[32];
    for (int i = 0; i < 32; i++) client_priv[i] = 0x20 + i;   /* 0x20..0x3f   */
    uint8_t client_pub[32]; size_t l = 32;
    EVP_PKEY *sk = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, NULL, client_priv, 32);
    EVP_PKEY_get_raw_public_key(sk, client_pub, &l);
    EVP_PKEY_free(sk);
    all &= check("X25519 client public key", client_pub, REPO_CLIENT_PUB, 32);

    /* (2) ECDHE shared secret = X25519(client_priv, repo server_pub).        */
    uint8_t server_pub[32]; unhex(REPO_SERVER_PUB, server_pub, 32);
    uint8_t shared[32];
    if (!x25519_derive(client_priv, server_pub, shared)) { printf("derive failed\n"); return 1; }
    all &= check("X25519 ECDHE shared secret", shared, REPO_SHARED, 32);

    /* (3) RFC 8446 7.1 key schedule, fed the repo's hello_hash.              */
    uint8_t zeros[HASH_LEN] = {0};
    uint8_t empty_hash[HASH_LEN]; sha384((const uint8_t *)"", 0, empty_hash);
    uint8_t early[HASH_LEN];   hkdf_extract(zeros, HASH_LEN, zeros, HASH_LEN, early);
    uint8_t derived[HASH_LEN]; derive_secret(early, "derived", empty_hash, derived);
    uint8_t hs[HASH_LEN];      hkdf_extract(derived, HASH_LEN, shared, 32, hs);

    uint8_t hello[HASH_LEN]; unhex(REPO_HELLO_HASH, hello, HASH_LEN);
    uint8_t c_hs[HASH_LEN], s_hs[HASH_LEN];
    derive_secret(hs, "c hs traffic", hello, c_hs);
    derive_secret(hs, "s hs traffic", hello, s_hs);
    all &= check("CLIENT_HANDSHAKE_SECRET", c_hs, KEYLOG_CLIENT_HS, HASH_LEN);
    all &= check("SERVER_HANDSHAKE_SECRET", s_hs, KEYLOG_SERVER_HS, HASH_LEN);

    printf("%s\n", all ? "ALL MATCH -> byte-identical to illustrated-tls13 + RFC 8446"
                       : "FAILED");
    return all ? 0 : 1;
}
