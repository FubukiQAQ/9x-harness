/* Axiom NeuroBench 2003 - SHA1 / HMAC-SHA1 / Base64 */
#include "sm.h"

/* ---------------- SHA-1 ---------------- */
typedef struct {
    unsigned int h[5];
    unsigned long long len;
    unsigned char buf[64];
    unsigned int buflen;
} SHA1CTX;

static unsigned int rol(unsigned int v, int n) { return (v << n) | (v >> (32 - n)); }

static void sha1_block(SHA1CTX *c, const unsigned char *p)
{
    unsigned int w[80];
    unsigned int a, b, d, e, f, k, t, cc;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((unsigned int)p[i * 4] << 24) | ((unsigned int)p[i * 4 + 1] << 16) |
               ((unsigned int)p[i * 4 + 2] << 8) | (unsigned int)p[i * 4 + 3];
    for (i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    a = c->h[0]; b = c->h[1]; cc = c->h[2]; d = c->h[3]; e = c->h[4];
    for (i = 0; i < 80; i++) {
        if (i < 20)      { f = (b & cc) | ((~b) & d);        k = 0x5A827999u; }
        else if (i < 40) { f = b ^ cc ^ d;                   k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (b & cc) | (b & d) | (cc & d); k = 0x8F1BBCDCu; }
        else             { f = b ^ cc ^ d;                   k = 0xCA62C1D6u; }
        t = rol(a, 5) + f + e + k + w[i];
        e = d; d = cc; cc = rol(b, 30); b = a; a = t;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d; c->h[4] += e;
}

static void sha1_init(SHA1CTX *c)
{
    c->h[0] = 0x67452301u; c->h[1] = 0xEFCDAB89u; c->h[2] = 0x98BADCFEu;
    c->h[3] = 0x10325476u; c->h[4] = 0xC3D2E1F0u;
    c->len = 0; c->buflen = 0;
}

static void sha1_update(SHA1CTX *c, const void *data, size_t len)
{
    const unsigned char *p = (const unsigned char *)data;
    c->len += len;
    while (len) {
        unsigned int n = 64 - c->buflen;
        if (n > len) n = (unsigned int)len;
        memcpy(c->buf + c->buflen, p, n);
        c->buflen += n; p += n; len -= n;
        if (c->buflen == 64) { sha1_block(c, c->buf); c->buflen = 0; }
    }
}

static void sha1_final(SHA1CTX *c, unsigned char out[20])
{
    unsigned long long bitlen = c->len * 8;
    unsigned char pad = 0x80;
    unsigned char zero = 0;
    unsigned char lenbuf[8];
    int i;
    sha1_update(c, &pad, 1);
    while (c->buflen != 56) sha1_update(c, &zero, 1);
    for (i = 0; i < 8; i++) lenbuf[i] = (unsigned char)(bitlen >> ((7 - i) * 8));
    sha1_update(c, lenbuf, 8);
    for (i = 0; i < 5; i++) {
        out[i * 4]     = (unsigned char)(c->h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(c->h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(c->h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)(c->h[i]);
    }
}

void sm_sha1(const void *data, size_t len, unsigned char out[20])
{
    SHA1CTX c;
    sha1_init(&c);
    sha1_update(&c, data, len);
    sha1_final(&c, out);
}

void sm_hmac_sha1(const void *key, size_t keylen, const void *data, size_t len,
                  unsigned char out[20])
{
    unsigned char k[64], ipad[64], opad[64], inner[20];
    int i;
    memset(k, 0, sizeof(k));
    if (keylen > 64) sm_sha1(key, keylen, k);
    else memcpy(k, key, keylen);
    for (i = 0; i < 64; i++) { ipad[i] = k[i] ^ 0x36; opad[i] = k[i] ^ 0x5C; }
    {
        SHA1CTX c;
        sha1_init(&c);
        sha1_update(&c, ipad, 64);
        sha1_update(&c, data, len);
        sha1_final(&c, inner);
    }
    {
        SHA1CTX c;
        sha1_init(&c);
        sha1_update(&c, opad, 64);
        sha1_update(&c, inner, 20);
        sha1_final(&c, out);
    }
}

/* ---------------- Base64 ---------------- */
static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t sm_b64_encode(const unsigned char *in, size_t len, char *out, int wrap)
{
    size_t i = 0, o = 0;
    int col = 0;
    while (i + 2 < len) {
        unsigned int v = ((unsigned int)in[i] << 16) | ((unsigned int)in[i + 1] << 8) | in[i + 2];
        out[o++] = B64[(v >> 18) & 63];
        out[o++] = B64[(v >> 12) & 63];
        out[o++] = B64[(v >> 6) & 63];
        out[o++] = B64[v & 63];
        i += 3; col += 4;
        if (wrap > 0 && col >= wrap) { out[o++] = '\r'; out[o++] = '\n'; col = 0; }
    }
    if (i < len) {
        unsigned int v = (unsigned int)in[i] << 16;
        if (i + 1 < len) v |= (unsigned int)in[i + 1] << 8;
        out[o++] = B64[(v >> 18) & 63];
        out[o++] = B64[(v >> 12) & 63];
        out[o++] = (i + 1 < len) ? B64[(v >> 6) & 63] : '=';
        out[o++] = '=';
    }
    out[o] = 0;
    return o;
}

static int b64val(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

long sm_b64_decode(const char *in, size_t len, unsigned char *out)
{
    size_t i, o = 0;
    unsigned int acc = 0;
    int nbits = 0;
    if (len == (size_t)-1) len = strlen(in);
    for (i = 0; i < len; i++) {
        int v;
        char c = in[i];
        if (c == '=') break;
        v = b64val(c);
        if (v < 0) continue;
        acc = (acc << 6) | (unsigned int)v;
        nbits += 6;
        if (nbits >= 8) {
            nbits -= 8;
            out[o++] = (unsigned char)((acc >> nbits) & 0xFF);
        }
    }
    return (long)o;
}
