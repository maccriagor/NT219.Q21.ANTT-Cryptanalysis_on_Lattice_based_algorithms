/* =============================================================================
 * tls13.h  --  Minimal TLS 1.3 (RFC 8446), built from scratch in C.
 *
 * Shared declarations for client.c and server.c.
 *
 * SCOPE (matches the handshake illustrated-tls13 walks through byte-by-byte):
 *   - cipher suite : TLS_AES_256_GCM_SHA384   (SHA-384 + AES-256-GCM)
 *   - key exchange : X25519 (RFC 7748)
 *   - signature    : ecdsa_secp256r1_sha256
 *                    (NB: the signature scheme's hash is SHA-256, independent
 *                     of the cipher suite's SHA-384 used in the key schedule)
 *   - server authentication only (no client cert, no PSK/0-RTT/HRR/KeyUpdate)
 *
 * PHILOSOPHY ("from scratch" the way rustls / picotls / illustrated-tls13 mean):
 *   - The PROTOCOL (message framing, key schedule, record layer) is hand-rolled
 *     here, byte for byte, mapped to RFC 8446 sections.
 *   - The PRIMITIVES (X25519, AES-256-GCM, SHA-384, HMAC, ECDSA) are borrowed
 *     from OpenSSL libcrypto -- never roll your own crypto. The exact OpenSSL
 *     usage mirrors the reference snippets in illustrated-tls13/site/files/.
 *
 * SOURCE MAP (see MAPPING.md for the full table):
 *   illustrated-tls13/site/files/hkdf-384.sh          -> hkdf_*()  below
 *   illustrated-tls13/site/files/aes_256_gcm_*.c       -> aead_*()  below
 *   illustrated-tls13/site/files/curve25519-mult.c     -> x25519_*() below
 *   illustrated-tls13/captures/caps (raw bytes)                  -> on-wire message layout
 * ===========================================================================*/
#ifndef TLS13_H
#define TLS13_H

#include <stdint.h>
#include <stddef.h>

/* ---- Record ContentType (RFC 8446 section 5.1) -------------------------- */
enum {
    CT_CHANGE_CIPHER_SPEC = 20,
    CT_ALERT              = 21,
    CT_HANDSHAKE          = 22,
    CT_APPLICATION_DATA   = 23,
};

/* ---- HandshakeType (RFC 8446 section 4, appendix B.3) ------------------- */
enum {
    HS_CLIENT_HELLO         = 1,
    HS_SERVER_HELLO         = 2,
    HS_NEW_SESSION_TICKET   = 4,
    HS_ENCRYPTED_EXTENSIONS = 8,
    HS_CERTIFICATE          = 11,
    HS_CERTIFICATE_VERIFY   = 15,
    HS_FINISHED             = 20,
};

/* ---- ExtensionType (RFC 8446 section 4.2) ------------------------------- */
enum {
    EXT_SERVER_NAME          = 0,
    EXT_SUPPORTED_GROUPS     = 10,
    EXT_SIGNATURE_ALGORITHMS = 13,
    EXT_SUPPORTED_VERSIONS   = 43,
    EXT_KEY_SHARE            = 51,
};

/* ---- Assorted code points ---------------------------------------------- */
#define NAMED_GROUP_X25519        0x001D  /* RFC 8446 section 4.2.7          */
#define TLS_AES_256_GCM_SHA384    0x1302  /* RFC 8446 appendix B.4           */
#define TLS13_VERSION             0x0304  /* RFC 8446 section 4.2.1          */
#define LEGACY_VERSION            0x0303  /* "TLS 1.2" on the wire           */
#define SIG_ECDSA_SECP256R1_SHA256 0x0403 /* RFC 8446 section 4.2.3          */
/* Hybrid PQC key exchange (draft-ietf-tls-ecdhe-mlkem; OpenSSL 3.5 native):
 * X25519MLKEM768 = ML-KEM-768 share FIRST, then X25519. shared = mlkem_ss||x25519_ss. */
#define NAMED_GROUP_X25519MLKEM768 0x11EC
#define MLKEM768_EK_LEN 1184              /* FIPS 203 encapsulation key (public) */
#define MLKEM768_CT_LEN 1088              /* FIPS 203 ciphertext                 */
#define MLKEM768_SS_LEN   32              /* FIPS 203 shared secret              */

#define HASH_LEN  48   /* SHA-384 output                                     */
#define KEY_LEN   32   /* AES-256 key                                        */
#define IV_LEN    12   /* AEAD nonce                                         */
#define TAG_LEN   16   /* AES-GCM tag                                        */

/* ============================ growable byte buffer ======================= */
/* Tiny helper used both to build outgoing messages and to accumulate the
 * handshake transcript (RFC 8446 section 4.4.1).                            */
typedef struct {
    uint8_t *data;
    size_t   len;
    size_t   cap;
} buf_t;

void buf_init(buf_t *b);
void buf_free(buf_t *b);
void buf_add(buf_t *b, const void *p, size_t n);
void buf_u8(buf_t *b, uint8_t v);
void buf_u16(buf_t *b, uint16_t v);
void buf_u24(buf_t *b, uint32_t v);
/* Reserve a length prefix now, fill it in once the body is appended.        */
size_t buf_begin_vec(buf_t *b, int prefix_bytes); /* returns patch position  */
void   buf_end_vec(buf_t *b, size_t mark, int prefix_bytes);

/* ============================== primitives =============================== */
/* (thin wrappers over OpenSSL libcrypto; see tls13.c)                       */
void sha384(const uint8_t *in, size_t len, uint8_t out[HASH_LEN]);
void hmac_sha384(const uint8_t *key, size_t klen,
                 const uint8_t *msg, size_t mlen, uint8_t out[HASH_LEN]);

/* HKDF, RFC 5869 + RFC 8446 section 7.1  (mirrors site/files/hkdf-384.sh)   */
void hkdf_extract(const uint8_t *salt, size_t slen,
                  const uint8_t *ikm, size_t ilen, uint8_t out[HASH_LEN]);
void hkdf_expand_label(const uint8_t secret[HASH_LEN],
                       const char *label,
                       const uint8_t *ctx, size_t ctxlen,
                       uint8_t *out, size_t outlen);
void derive_secret(const uint8_t secret[HASH_LEN], const char *label,
                   const uint8_t *transcript_hash, uint8_t out[HASH_LEN]);

/* X25519, RFC 7748  (mirrors site/files/curve25519-mult.c)                  */
void x25519_keygen(uint8_t priv[32], uint8_t pub[32]);
int  x25519_derive(const uint8_t priv[32], const uint8_t peer_pub[32],
                   uint8_t shared[32]);

/* ML-KEM-768, NIST FIPS 203, via OpenSSL EVP (EVP_PKEY_encapsulate/decapsulate).
 * Used by the hybrid group X25519MLKEM768 (NAMED_GROUP_X25519MLKEM768).
 *   ek = encapsulation key (public, sent on the wire); ct = ciphertext; ss = 32-byte
 *   shared secret. The decapsulation key stays opaque inside mlkem_key (an EVP_PKEY). */
typedef struct mlkem_key mlkem_key;
mlkem_key *mlkem768_keygen(uint8_t ek_out[MLKEM768_EK_LEN]);          /* client side  */
int  mlkem768_encaps(const uint8_t ek[MLKEM768_EK_LEN],
                     uint8_t ct_out[MLKEM768_CT_LEN],
                     uint8_t ss_out[MLKEM768_SS_LEN]);                /* server side  */
int  mlkem768_decaps(mlkem_key *k, const uint8_t ct[MLKEM768_CT_LEN],
                     uint8_t ss_out[MLKEM768_SS_LEN]);                /* client side  */
void mlkem_free(mlkem_key *k);

/* AES-256-GCM, RFC 5116  (mirrors site/files/aes_256_gcm_{en,de}crypt.c)    */
int aead_seal(const uint8_t key[KEY_LEN], const uint8_t base_iv[IV_LEN],
              uint64_t seq, const uint8_t *aad, size_t aadlen,
              const uint8_t *pt, size_t ptlen, uint8_t *out /*ptlen+TAG*/);
int aead_open(const uint8_t key[KEY_LEN], const uint8_t base_iv[IV_LEN],
              uint64_t seq, const uint8_t *aad, size_t aadlen,
              const uint8_t *ct, size_t ctlen, uint8_t *out /*ctlen-TAG*/);

/* ============================ connection state =========================== */
typedef struct {
    int fd;                       /* connected TCP socket                    */

    /* traffic secrets + derived key/iv for each direction & epoch           */
    uint8_t c_hs_secret[HASH_LEN], s_hs_secret[HASH_LEN];
    uint8_t c_ap_secret[HASH_LEN], s_ap_secret[HASH_LEN];
    uint8_t handshake_secret[HASH_LEN];

    uint8_t c_hs_key[KEY_LEN], c_hs_iv[IV_LEN];
    uint8_t s_hs_key[KEY_LEN], s_hs_iv[IV_LEN];
    uint8_t c_ap_key[KEY_LEN], c_ap_iv[IV_LEN];
    uint8_t s_ap_key[KEY_LEN], s_ap_iv[IV_LEN];

    /* AEAD record sequence numbers are tracked by the caller per epoch
     * (each direction resets to 0 on key change, RFC 8446 section 5.3).      */

    buf_t transcript;             /* all handshake msgs, headers included     */
} tls_conn;

/* ---- record layer helpers (RFC 8446 section 5) -------------------------- */
/* Send one plaintext record (handshake/CCS before keys are active).         */
int  record_send_plain(int fd, uint8_t type, const uint8_t *payload, size_t n,
                        uint16_t version);
/* Read one record off the wire: returns type, fills *payload (caller frees). */
int  record_read(int fd, uint8_t *type, uint8_t **payload, size_t *plen);

/* Send one *encrypted* record: AEAD-seals (inner = msg || content_type).
 * Key/iv/seq are explicit so the caller controls the epoch (sec 5.3).        */
int  record_send_enc(int fd, const uint8_t key[KEY_LEN], const uint8_t iv[IV_LEN],
                     uint64_t seq, uint8_t inner_type, const uint8_t *msg, size_t n);
/* Decrypt one already-read application_data record into (inner_type, body).   */
int  record_decrypt(const uint8_t key[KEY_LEN], const uint8_t iv[IV_LEN],
                    uint64_t seq, const uint8_t *rec_hdr,
                    const uint8_t *ct, size_t ctlen,
                    uint8_t *inner_type, uint8_t *out, size_t *outlen);

/* ---- key schedule (RFC 8446 section 7.1) -------------------------------- */
/* After ECDHE: derive Handshake Secret and both handshake traffic keys.     */
void ks_derive_handshake(tls_conn *c, const uint8_t *ecdhe, size_t ecdhe_len);
/* After server Finished: derive Master Secret and both app traffic keys.    */
void ks_derive_application(tls_conn *c);

/* ---- misc --------------------------------------------------------------- */
void die(const char *msg);
void hexdump(const char *label, const uint8_t *p, size_t n);

#endif /* TLS13_H */
