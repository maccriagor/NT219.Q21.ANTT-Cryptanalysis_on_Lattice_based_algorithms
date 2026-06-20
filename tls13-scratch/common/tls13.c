/* =============================================================================
 * tls13.c  --  shared TLS 1.3 mechanics for client.c / server.c
 * Protocol hand-rolled; primitives via OpenSSL libcrypto (see tls13.h header).
 * ===========================================================================*/
#include "tls13.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>

/* ============================== misc ===================================== */
void die(const char *msg)
{
    fprintf(stderr, "fatal: %s: %s\n", msg, errno ? strerror(errno) : "");
    exit(1);
}

void hexdump(const char *label, const uint8_t *p, size_t n)
{
    printf("    %s (%zu bytes): ", label, n);
    size_t show = n < 16 ? n : 16;
    for (size_t i = 0; i < show; i++) printf("%02x", p[i]);
    if (n > show) printf("...");
    printf("\n");
}

/* ========================= growable byte buffer ========================== */
void buf_init(buf_t *b) { b->data = NULL; b->len = 0; b->cap = 0; }
void buf_free(buf_t *b) { free(b->data); buf_init(b); }

void buf_add(buf_t *b, const void *p, size_t n)
{
    if (b->len + n > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 256;
        while (nc < b->len + n) nc *= 2;
        b->data = realloc(b->data, nc);
        if (!b->data) die("realloc");
        b->cap = nc;
    }
    memcpy(b->data + b->len, p, n);
    b->len += n;
}
void buf_u8 (buf_t *b, uint8_t v)  { buf_add(b, &v, 1); }
void buf_u16(buf_t *b, uint16_t v) { uint8_t t[2] = {v>>8, v}; buf_add(b, t, 2); }
void buf_u24(buf_t *b, uint32_t v) { uint8_t t[3] = {v>>16, v>>8, v}; buf_add(b, t, 3); }

/* Length-prefixed vector: reserve prefix bytes, append body, then back-patch. */
size_t buf_begin_vec(buf_t *b, int prefix_bytes)
{
    size_t mark = b->len;
    for (int i = 0; i < prefix_bytes; i++) buf_u8(b, 0);
    return mark;
}
void buf_end_vec(buf_t *b, size_t mark, int prefix_bytes)
{
    size_t body = b->len - mark - prefix_bytes;
    for (int i = 0; i < prefix_bytes; i++)
        b->data[mark + i] = (body >> (8 * (prefix_bytes - 1 - i))) & 0xFF;
}

/* ============================== primitives =============================== */
void sha384(const uint8_t *in, size_t len, uint8_t out[HASH_LEN])
{
    SHA384(in, len, out);
}

void hmac_sha384(const uint8_t *key, size_t klen,
                 const uint8_t *msg, size_t mlen, uint8_t out[HASH_LEN])
{
    unsigned int outl = HASH_LEN;
    if (!HMAC(EVP_sha384(), key, (int)klen, msg, mlen, out, &outl))
        die("HMAC");
}

/* ---- HKDF (RFC 5869) + Expand-Label (RFC 8446 7.1) ---------------------- *
 * Mirrors illustrated-tls13/site/files/hkdf-384.sh exactly: SHA-384, as used
 * by TLS_AES_256_GCM_SHA384.                                                 */
void hkdf_extract(const uint8_t *salt, size_t slen,
                  const uint8_t *ikm, size_t ilen, uint8_t out[HASH_LEN])
{
    /* PRK = HMAC-Hash(salt, IKM); empty salt -> HashLen zeros (RFC 5869 2.2) */
    uint8_t zeros[HASH_LEN] = {0};
    if (slen == 0) { salt = zeros; slen = HASH_LEN; }
    hmac_sha384(salt, slen, ikm, ilen, out);
}

/* HKDF-Expand (RFC 5869 2.3): T(i)=HMAC(PRK, T(i-1)|info|i).                  */
static void hkdf_expand(const uint8_t prk[HASH_LEN],
                        const uint8_t *info, size_t infolen,
                        uint8_t *out, size_t outlen)
{
    uint8_t t[HASH_LEN];
    size_t  tlen = 0, done = 0;
    uint8_t counter = 0;
    while (done < outlen) {
        counter++;
        /* msg = T(i-1) || info || counter */
        uint8_t msg[HASH_LEN + 512 + 1];
        size_t  mlen = 0;
        memcpy(msg, t, tlen); mlen += tlen;
        memcpy(msg + mlen, info, infolen); mlen += infolen;
        msg[mlen++] = counter;
        hmac_sha384(prk, HASH_LEN, msg, mlen, t);
        tlen = HASH_LEN;
        size_t take = (outlen - done < HASH_LEN) ? outlen - done : HASH_LEN;
        memcpy(out + done, t, take);
        done += take;
    }
}

void hkdf_expand_label(const uint8_t secret[HASH_LEN], const char *label,
                       const uint8_t *ctx, size_t ctxlen,
                       uint8_t *out, size_t outlen)
{
    /* HkdfLabel = uint16 length || opaque("tls13 "+label) || opaque(context) */
    char full[256];
    int  flen = snprintf(full, sizeof full, "tls13 %s", label);
    buf_t hl; buf_init(&hl);
    buf_u16(&hl, (uint16_t)outlen);
    buf_u8(&hl, (uint8_t)flen);   buf_add(&hl, full, flen);
    buf_u8(&hl, (uint8_t)ctxlen); buf_add(&hl, ctx, ctxlen);
    hkdf_expand(secret, hl.data, hl.len, out, outlen);
    buf_free(&hl);
}

void derive_secret(const uint8_t secret[HASH_LEN], const char *label,
                   const uint8_t *transcript_hash, uint8_t out[HASH_LEN])
{
    /* Derive-Secret(Secret, Label, Messages) = Expand-Label(., ., TH, Hash.len) */
    hkdf_expand_label(secret, label, transcript_hash, HASH_LEN, out, HASH_LEN);
}

/* ---- X25519 (RFC 7748) -- mirrors site/files/curve25519-mult.c ---------- *
 * (donna on the site; OpenSSL EVP here -- both compute the same scalar mult) */
void x25519_keygen(uint8_t priv[32], uint8_t pub[32])
{
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
    EVP_PKEY *pkey = NULL;
    if (!pctx || EVP_PKEY_keygen_init(pctx) <= 0 ||
        EVP_PKEY_keygen(pctx, &pkey) <= 0) die("X25519 keygen");
    size_t l = 32;
    if (EVP_PKEY_get_raw_private_key(pkey, priv, &l) <= 0) die("get priv");
    l = 32;
    if (EVP_PKEY_get_raw_public_key(pkey, pub, &l) <= 0) die("get pub");
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pctx);
}

int x25519_derive(const uint8_t priv[32], const uint8_t peer_pub[32],
                  uint8_t shared[32])
{
    EVP_PKEY *sk = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, NULL, priv, 32);
    EVP_PKEY *pk = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL, peer_pub, 32);
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(sk, NULL);
    size_t l = 32; int ok = 0;
    if (sk && pk && ctx && EVP_PKEY_derive_init(ctx) > 0 &&
        EVP_PKEY_derive_set_peer(ctx, pk) > 0 &&
        EVP_PKEY_derive(ctx, shared, &l) > 0)
        ok = 1;
    EVP_PKEY_CTX_free(ctx); EVP_PKEY_free(sk); EVP_PKEY_free(pk);
    return ok;
}

/* ---- ML-KEM-768 (NIST FIPS 203) via OpenSSL EVP ------------------------- *
 * Borrowed primitive (same "rent crypto from libcrypto" rule as X25519/AES).
 * keygen [FIPS 203 Alg.19] keeps the decapsulation key inside mlkem_key and
 * returns the encapsulation key (public) for the wire; encaps [Alg.20] and
 * decaps [Alg.21] are EVP_PKEY_encapsulate/decapsulate (verified roundtrip).  */
struct mlkem_key { EVP_PKEY *pkey; };

mlkem_key *mlkem768_keygen(uint8_t ek_out[MLKEM768_EK_LEN])
{
    EVP_PKEY *dk = EVP_PKEY_Q_keygen(NULL, NULL, "ML-KEM-768");
    if (!dk) return NULL;
    size_t eklen = MLKEM768_EK_LEN;
    if (EVP_PKEY_get_octet_string_param(dk, OSSL_PKEY_PARAM_PUB_KEY,
                                        ek_out, MLKEM768_EK_LEN, &eklen) != 1
        || eklen != MLKEM768_EK_LEN) { EVP_PKEY_free(dk); return NULL; }
    mlkem_key *k = malloc(sizeof *k);
    if (!k) { EVP_PKEY_free(dk); return NULL; }
    k->pkey = dk;
    return k;
}

int mlkem768_encaps(const uint8_t ek[MLKEM768_EK_LEN],
                    uint8_t ct_out[MLKEM768_CT_LEN], uint8_t ss_out[MLKEM768_SS_LEN])
{
    EVP_PKEY *peer = EVP_PKEY_new_raw_public_key_ex(NULL, "ML-KEM-768", NULL,
                                                    ek, MLKEM768_EK_LEN);
    if (!peer) return 0;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_pkey(NULL, peer, NULL);
    size_t ctlen = MLKEM768_CT_LEN, sslen = MLKEM768_SS_LEN; int ok = 0;
    if (ctx && EVP_PKEY_encapsulate_init(ctx, NULL) > 0
        && EVP_PKEY_encapsulate(ctx, ct_out, &ctlen, ss_out, &sslen) > 0
        && ctlen == MLKEM768_CT_LEN && sslen == MLKEM768_SS_LEN) ok = 1;
    EVP_PKEY_CTX_free(ctx); EVP_PKEY_free(peer);
    return ok;
}

int mlkem768_decaps(mlkem_key *k, const uint8_t ct[MLKEM768_CT_LEN],
                    uint8_t ss_out[MLKEM768_SS_LEN])
{
    if (!k || !k->pkey) return 0;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_pkey(NULL, k->pkey, NULL);
    size_t sslen = MLKEM768_SS_LEN; int ok = 0;
    if (ctx && EVP_PKEY_decapsulate_init(ctx, NULL) > 0
        && EVP_PKEY_decapsulate(ctx, ss_out, &sslen, ct, MLKEM768_CT_LEN) > 0
        && sslen == MLKEM768_SS_LEN) ok = 1;
    EVP_PKEY_CTX_free(ctx);
    return ok;
}

void mlkem_free(mlkem_key *k) { if (k) { EVP_PKEY_free(k->pkey); free(k); } }

/* ---- AES-256-GCM (RFC 5116) -- mirrors site/files/aes_256_gcm_*.c -------- */
static void build_nonce(const uint8_t base_iv[IV_LEN], uint64_t seq,
                        uint8_t nonce[IV_LEN])
{
    /* nonce = base_iv XOR (seq as 8-byte big-endian, right-aligned). sec 5.3 */
    memcpy(nonce, base_iv, IV_LEN);
    for (int i = 0; i < 8; i++)
        nonce[IV_LEN - 1 - i] ^= (uint8_t)(seq >> (8 * i));
}

int aead_seal(const uint8_t key[KEY_LEN], const uint8_t base_iv[IV_LEN],
              uint64_t seq, const uint8_t *aad, size_t aadlen,
              const uint8_t *pt, size_t ptlen, uint8_t *out)
{
    uint8_t nonce[IV_LEN]; build_nonce(base_iv, seq, nonce);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len, ok = 0;
    if (ctx &&
        EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_LEN, NULL) &&
        EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) &&
        EVP_EncryptUpdate(ctx, NULL, &len, aad, (int)aadlen) &&
        EVP_EncryptUpdate(ctx, out, &len, pt, (int)ptlen)) {
        int total = len;
        if (EVP_EncryptFinal_ex(ctx, out + total, &len)) {
            total += len;
            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN,
                                    out + total)) ok = 1;
        }
    }
    EVP_CIPHER_CTX_free(ctx);
    return ok ? 0 : -1;
}

int aead_open(const uint8_t key[KEY_LEN], const uint8_t base_iv[IV_LEN],
              uint64_t seq, const uint8_t *aad, size_t aadlen,
              const uint8_t *ct, size_t ctlen, uint8_t *out)
{
    if (ctlen < TAG_LEN) return -1;
    size_t bodylen = ctlen - TAG_LEN;
    uint8_t nonce[IV_LEN]; build_nonce(base_iv, seq, nonce);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len, ok = 0;
    if (ctx &&
        EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_LEN, NULL) &&
        EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) &&
        EVP_DecryptUpdate(ctx, NULL, &len, aad, (int)aadlen) &&
        EVP_DecryptUpdate(ctx, out, &len, ct, (int)bodylen) &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN,
                            (void *)(ct + bodylen)) &&
        EVP_DecryptFinal_ex(ctx, out + len, &len))   /* verifies the tag */
        ok = 1;
    EVP_CIPHER_CTX_free(ctx);
    return ok ? 0 : -1;
}

/* ============================== record layer ============================= *
 * TLSPlaintext / TLSCiphertext, RFC 8446 section 5.                         */
static int read_n(int fd, uint8_t *buf, size_t n)
{
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, buf + got, n - got);
        if (r <= 0) return -1;
        got += (size_t)r;
    }
    return 0;
}

int record_send_plain(int fd, uint8_t type, const uint8_t *payload, size_t n,
                       uint16_t version)
{
    uint8_t hdr[5] = { type, version >> 8, version, n >> 8, n };
    if (write(fd, hdr, 5) != 5) return -1;
    if (n && write(fd, payload, n) != (ssize_t)n) return -1;
    return 0;
}

int record_read(int fd, uint8_t *type, uint8_t **payload, size_t *plen)
{
    uint8_t hdr[5];
    if (read_n(fd, hdr, 5) < 0) return -1;
    *type = hdr[0];
    size_t len = (hdr[3] << 8) | hdr[4];
    uint8_t *p = malloc(len ? len : 1);
    if (!p) die("malloc");
    if (len && read_n(fd, p, len) < 0) { free(p); return -1; }
    *payload = p;
    *plen = len;
    return 0;
}

int record_send_enc(int fd, const uint8_t key[KEY_LEN], const uint8_t iv[IV_LEN],
                    uint64_t seq, uint8_t inner_type, const uint8_t *msg, size_t n)
{
    /* inner plaintext = content || content_type (no padding here). sec 5.2   */
    uint8_t *inner = malloc(n + 1);
    memcpy(inner, msg, n);
    inner[n] = inner_type;
    size_t ctlen = n + 1 + TAG_LEN;        /* ciphertext includes the tag     */
    /* additional_data = opaque_type(23) || legacy_version || length. sec 5.2 */
    uint8_t aad[5] = { CT_APPLICATION_DATA, (LEGACY_VERSION>>8)&0xFF, LEGACY_VERSION&0xFF,
                       (ctlen>>8)&0xFF, ctlen&0xFF };
    uint8_t *ct = malloc(ctlen);
    int rc = aead_seal(key, iv, seq, aad, 5, inner, n + 1, ct);
    free(inner);
    if (rc != 0) { free(ct); return -1; }
    int sent = (write(fd, aad, 5) == 5 &&
                write(fd, ct, ctlen) == (ssize_t)ctlen) ? 0 : -1;
    free(ct);
    return sent;
}

int record_decrypt(const uint8_t key[KEY_LEN], const uint8_t iv[IV_LEN],
                   uint64_t seq, const uint8_t *rec_hdr,
                   const uint8_t *ct, size_t ctlen,
                   uint8_t *inner_type, uint8_t *out, size_t *outlen)
{
    if (aead_open(key, iv, seq, rec_hdr, 5, ct, ctlen, out) != 0) return -1;
    size_t inner = ctlen - TAG_LEN;        /* plaintext = content||type||pad   */
    while (inner > 0 && out[inner - 1] == 0) inner--;   /* strip zero padding  */
    if (inner == 0) return -1;
    *inner_type = out[inner - 1];
    *outlen = inner - 1;
    return 0;
}

/* ============================== key schedule ============================= *
 * RFC 8446 section 7.1 ("Key Schedule" diagram).                            */
static void traffic_keys(const uint8_t secret[HASH_LEN],
                         uint8_t key[KEY_LEN], uint8_t iv[IV_LEN])
{
    hkdf_expand_label(secret, "key", NULL, 0, key, KEY_LEN); /* sec 7.3 */
    hkdf_expand_label(secret, "iv",  NULL, 0, iv,  IV_LEN);  /* sec 7.3 */
}

void ks_derive_handshake(tls_conn *c, const uint8_t *ecdhe, size_t ecdhe_len)
{
    uint8_t zeros[HASH_LEN] = {0};
    uint8_t empty_hash[HASH_LEN]; sha384((const uint8_t *)"", 0, empty_hash);

    /* Early Secret = HKDF-Extract(0, 0)  (no PSK)                            */
    uint8_t early[HASH_LEN];
    hkdf_extract(zeros, HASH_LEN, zeros, HASH_LEN, early);

    /* derived = Derive-Secret(Early, "derived", "")                          */
    uint8_t derived[HASH_LEN];
    derive_secret(early, "derived", empty_hash, derived);

    /* Handshake Secret = HKDF-Extract(derived, (EC)DHE).
     * IKM = the (EC)DHE shared secret: 32 bytes for X25519, or 64 bytes
     * (ML-KEM ss || X25519 ss) for the hybrid X25519MLKEM768.                 */
    hkdf_extract(derived, HASH_LEN, ecdhe, ecdhe_len, c->handshake_secret);

    /* Transcript hash over ClientHello..ServerHello (caller filled it)       */
    uint8_t th[HASH_LEN]; sha384(c->transcript.data, c->transcript.len, th);

    derive_secret(c->handshake_secret, "c hs traffic", th, c->c_hs_secret);
    derive_secret(c->handshake_secret, "s hs traffic", th, c->s_hs_secret);
    traffic_keys(c->c_hs_secret, c->c_hs_key, c->c_hs_iv);
    traffic_keys(c->s_hs_secret, c->s_hs_key, c->s_hs_iv);
}

void ks_derive_application(tls_conn *c)
{
    uint8_t zeros[HASH_LEN] = {0};
    uint8_t empty_hash[HASH_LEN]; sha384((const uint8_t *)"", 0, empty_hash);

    /* derived = Derive-Secret(Handshake, "derived", "")                      */
    uint8_t derived[HASH_LEN];
    derive_secret(c->handshake_secret, "derived", empty_hash, derived);

    /* Master Secret = HKDF-Extract(derived, 0)                               */
    uint8_t master[HASH_LEN];
    hkdf_extract(derived, HASH_LEN, zeros, HASH_LEN, master);

    /* Transcript hash over ClientHello..server Finished (caller filled it)   */
    uint8_t th[HASH_LEN]; sha384(c->transcript.data, c->transcript.len, th);

    derive_secret(master, "c ap traffic", th, c->c_ap_secret);
    derive_secret(master, "s ap traffic", th, c->s_ap_secret);
    traffic_keys(c->c_ap_secret, c->c_ap_key, c->c_ap_iv);
    traffic_keys(c->s_ap_secret, c->s_ap_key, c->s_ap_iv);
}
