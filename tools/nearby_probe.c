/* SPDX-License-Identifier: MIT */
/* Does a stored Rapport identity key produce the iPhone's NearbyInfo auth tag?
 *
 * The iPhone has no Bluetooth IRK on this Mac (docs/STATE.md), so BroadTek
 * cannot resolve its RPAs the way it resolves the Watch's. macOS does not need
 * one either: it recognises the phone by a 3-byte auth tag carried in the
 * NearbyInfo advertisement (Apple manufacturer sub-type 0x10), which it joins
 * to the phone's identity. BroadTek already sees RPA <-> tag on the air
 * (`broadtek-nearby`). If a key the Mac stores reproduces the tag from the
 * address, BroadTek can do the join itself, and the accept-list substitution
 * that is already proven on the Apple Watch reaches the phone too.
 *
 * So: given one measured (RPA, tag) pair for the phone, try every 16-byte
 * window of every same-account Rapport identity against a set of plausible
 * constructions, and say whether any of them lands on the tag.
 *
 *     make nearby-probe && sudo build/out/nearby_probe <rpa> <tag>
 *     sudo build/out/nearby_probe 4b:00:00:00:00:05 b834bc
 *
 * <rpa> is as the air and the logs spell it, most significant byte first.
 *
 * It prints only which construction matched, at which offset, and in which
 * identity. No key, and no byte of any stored item, is passed to any output
 * function anywhere in this file -- the same rule tools/keychain_probe.c
 * follows. A match is a finding about an algorithm, not a key to publish. */
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <CommonCrypto/CommonCrypto.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_BLOBS 32
#define MAX_BLOB  256

struct blob {
    uint8_t  bytes[MAX_BLOB];
    size_t   len;
    char     who[64];           /* the account, an opaque device UUID */
};

static struct blob g_blobs[MAX_BLOBS];
static int g_n_blobs;

/* ---- the constructions ------------------------------------------------ */

static void aes_ecb(const uint8_t key[16], const uint8_t in[16], uint8_t out[16])
{
    size_t moved = 0;
    CCCrypt(kCCEncrypt, kCCAlgorithmAES, kCCOptionECBMode,
            key, 16, NULL, in, 16, out, 16, &moved);
}

/* AES-CMAC (RFC 4493), the MAC the Bluetooth and Find My specs use. */
static void cmac(const uint8_t key[16], const uint8_t *msg, size_t len, uint8_t out[16])
{
    uint8_t zero[16] = {0}, l[16], k1[16], k2[16];
    aes_ecb(key, zero, l);

    for (int i = 0; i < 16; i++)
        k1[i] = (uint8_t)((l[i] << 1) | (i + 1 < 16 ? (l[i + 1] >> 7) : 0));
    if (l[0] & 0x80) k1[15] ^= 0x87;
    for (int i = 0; i < 16; i++)
        k2[i] = (uint8_t)((k1[i] << 1) | (i + 1 < 16 ? (k1[i + 1] >> 7) : 0));
    if (k1[0] & 0x80) k2[15] ^= 0x87;

    size_t blocks = (len + 15) / 16;
    if (blocks == 0) blocks = 1;
    uint8_t x[16] = {0}, y[16];

    for (size_t b = 0; b + 1 < blocks; b++) {
        for (int i = 0; i < 16; i++) y[i] = x[i] ^ msg[b * 16 + i];
        aes_ecb(key, y, x);
    }

    uint8_t last[16] = {0};
    size_t rest = len - (blocks - 1) * 16;
    if (rest == 16) {
        memcpy(last, msg + (blocks - 1) * 16, 16);
        for (int i = 0; i < 16; i++) last[i] ^= k1[i];
    } else {
        if (rest > 0) memcpy(last, msg + (blocks - 1) * 16, rest);
        last[rest] = 0x80;
        for (int i = 0; i < 16; i++) last[i] ^= k2[i];
    }
    for (int i = 0; i < 16; i++) y[i] = x[i] ^ last[i];
    aes_ecb(key, y, out);
}

static const char *kFnName[] = {
    "ah(): AES-ECB, input right-aligned in a zero block",
    "AES-ECB, input left-aligned in a zero block",
    "AES-CMAC",
    "HMAC-SHA256",
    "SHA256(key || input)",
    "SHA256(input || key)",
    "SHA1(key || input)",
    "HMAC-SHA1",
};
#define N_FN 8

/* out gets 3 bytes. */
static void derive(int fn, const uint8_t key[16], const uint8_t *in, size_t inlen,
                   uint8_t out[3])
{
    uint8_t block[16] = {0}, wide[CC_SHA256_DIGEST_LENGTH], msg[64];

    switch (fn) {
    case 0:
        if (inlen > 16) inlen = 16;
        memcpy(block + (16 - inlen), in, inlen);
        aes_ecb(key, block, wide);
        memcpy(out, wide, 3);
        break;
    case 1:
        if (inlen > 16) inlen = 16;
        memcpy(block, in, inlen);
        aes_ecb(key, block, wide);
        memcpy(out, wide, 3);
        break;
    case 2:
        cmac(key, in, inlen, wide);
        memcpy(out, wide, 3);
        break;
    case 3:
        CCHmac(kCCHmacAlgSHA256, key, 16, in, inlen, wide);
        memcpy(out, wide, 3);
        break;
    case 4:
        memcpy(msg, key, 16);
        memcpy(msg + 16, in, inlen);
        CC_SHA256(msg, (CC_LONG)(16 + inlen), wide);
        memcpy(out, wide, 3);
        memset(msg, 0, sizeof(msg));
        break;
    case 5:
        memcpy(msg, in, inlen);
        memcpy(msg + inlen, key, 16);
        CC_SHA256(msg, (CC_LONG)(16 + inlen), wide);
        memcpy(out, wide, 3);
        memset(msg, 0, sizeof(msg));
        break;
    case 6:
        memcpy(msg, key, 16);
        memcpy(msg + 16, in, inlen);
        CC_SHA1(msg, (CC_LONG)(16 + inlen), wide);
        memcpy(out, wide, 3);
        memset(msg, 0, sizeof(msg));
        break;
    default:
        CCHmac(kCCHmacAlgSHA1, key, 16, in, inlen, wide);
        memcpy(out, wide, 3);
        break;
    }
    memset(block, 0, sizeof(block));
    memset(wide, 0, sizeof(wide));
}

/* ---- the keychain ----------------------------------------------------- */

/* Every same-account identity, by exact service. The account is a device
 * UUID, not a credential, and the data is never rendered. */
static int loadIdentities(void)
{
    const void *k[] = { kSecClass, kSecAttrService, kSecAttrAccessGroup,
                        kSecMatchLimit, kSecReturnAttributes, kSecReturnData,
                        kSecAttrSynchronizable };
    const void *v[] = { kSecClassGenericPassword, CFSTR("RPIdentity-SameAccountDevice"),
                        CFSTR("com.apple.rapport"), kSecMatchLimitAll,
                        kCFBooleanTrue, kCFBooleanTrue, kSecAttrSynchronizableAny };
    CFDictionaryRef q = CFDictionaryCreate(NULL, k, v, 7, &kCFTypeDictionaryKeyCallBacks,
                                           &kCFTypeDictionaryValueCallBacks);
    CFTypeRef result = NULL;
    OSStatus st = SecItemCopyMatching(q, &result);
    CFRelease(q);

    if (st != errSecSuccess || result == NULL) {
        printf("  could not read the Rapport identities: status %d\n", (int)st);
        if (result != NULL) CFRelease(result);
        return 0;
    }
    if (CFGetTypeID(result) != CFArrayGetTypeID()) { CFRelease(result); return 0; }

    CFIndex n = CFArrayGetCount(result);
    for (CFIndex i = 0; i < n && g_n_blobs < MAX_BLOBS; i++) {
        CFTypeRef item = CFArrayGetValueAtIndex(result, i);
        if (CFGetTypeID(item) != CFDictionaryGetTypeID()) continue;
        CFDataRef d = CFDictionaryGetValue(item, kSecValueData);
        if (d == NULL || CFGetTypeID(d) != CFDataGetTypeID()) continue;

        CFIndex len = CFDataGetLength(d);
        if (len < 16) continue;
        if (len > MAX_BLOB) len = MAX_BLOB;

        struct blob *b = &g_blobs[g_n_blobs++];
        CFDataGetBytes(d, CFRangeMake(0, len), b->bytes);
        b->len = (size_t)len;
        snprintf(b->who, sizeof(b->who), "(no account)");
        CFTypeRef acct = CFDictionaryGetValue(item, kSecAttrAccount);
        if (acct != NULL && CFGetTypeID(acct) == CFStringGetTypeID())
            CFStringGetCString(acct, b->who, sizeof(b->who), kCFStringEncodingUTF8);
    }
    CFRelease(result);
    return g_n_blobs;
}

/* ---- the search ------------------------------------------------------- */

static int parseAddr(const char *s, uint8_t out[6])
{
    int n = 0;
    while (*s != '\0' && n < 6) {
        if (*s == ':' || *s == '-') { s++; continue; }
        char hex[3] = { s[0], s[1], 0 };
        if (s[1] == '\0') return 0;
        out[n++] = (uint8_t)strtoul(hex, NULL, 16);
        s += 2;
    }
    return n == 6;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("usage: sudo %s <rpa, msb first> <3-byte tag>\n", argv[0]);
        printf("   eg: sudo %s 4b:00:00:00:00:05 b834bc\n", argv[0]);
        return 2;
    }

    uint8_t rpa[6], tag[3];
    if (!parseAddr(argv[1], rpa)) { printf("bad address\n"); return 2; }
    if (!parseAddr(argv[2], tag) && strlen(argv[2]) != 6) { printf("bad tag\n"); return 2; }
    {
        char t[7] = {0};
        strncpy(t, argv[2], 6);
        for (int i = 0; i < 3; i++) {
            char hex[3] = { t[i * 2], t[i * 2 + 1], 0 };
            tag[i] = (uint8_t)strtoul(hex, NULL, 16);
        }
    }

    printf("BroadTek NearbyInfo tag probe\n");
    printf("  address %02x:%02x:%02x:%02x:%02x:%02x   tag %02x%02x%02x\n",
           rpa[0], rpa[1], rpa[2], rpa[3], rpa[4], rpa[5], tag[0], tag[1], tag[2]);
    printf("  euid %d%s\n\n", (int)geteuid(), geteuid() == 0 ? "" : "  (not root)");

    if (loadIdentities() == 0) { printf("no identities to test\n"); return 1; }
    printf("  %d same-account identit%s to test against\n\n",
           g_n_blobs, g_n_blobs == 1 ? "y" : "ies");

    /* The address, in the orders and slices a derivation might use. An RPA is
     * prand (top 3 bytes, with 01 in the top two bits) then hash (low 3). */
    uint8_t rev[6];
    for (int i = 0; i < 6; i++) rev[i] = rpa[5 - i];

    struct { const char *name; const uint8_t *p; size_t n; } inputs[] = {
        { "prand (top 3, msb first)",   rpa,      3 },
        { "prand (top 3, reversed)",    rev + 3,  3 },
        { "full address, msb first",    rpa,      6 },
        { "full address, reversed",     rev,      6 },
        { "hash (low 3, msb first)",    rpa + 3,  3 },
        { "hash (low 3, reversed)",     rev,      3 },
    };
    const int n_in = (int)(sizeof(inputs) / sizeof(inputs[0]));

    long tried = 0;
    int hits = 0;

    for (int b = 0; b < g_n_blobs; b++) {
        size_t windows = g_blobs[b].len >= 16 ? g_blobs[b].len - 16 + 1 : 0;
        for (size_t off = 0; off < windows; off++) {
            uint8_t key[16], keyrev[16];
            memcpy(key, g_blobs[b].bytes + off, 16);
            for (int i = 0; i < 16; i++) keyrev[i] = key[15 - i];

            for (int order = 0; order < 2; order++) {
                const uint8_t *k = order == 0 ? key : keyrev;
                for (int fn = 0; fn < N_FN; fn++) {
                    for (int in = 0; in < n_in; in++) {
                        uint8_t out[3];
                        derive(fn, k, inputs[in].p, inputs[in].n, out);
                        tried++;
                        if (memcmp(out, tag, 3) == 0) {
                            hits++;
                            printf("  MATCH  identity %s\n", g_blobs[b].who);
                            printf("         key: offset %zu, %s\n", off,
                                   order == 0 ? "as stored" : "byte-reversed");
                            printf("         %s over %s\n", kFnName[fn], inputs[in].name);
                        }
                    }
                }
            }
            memset(key, 0, sizeof(key));
            memset(keyrev, 0, sizeof(keyrev));
        }
    }

    printf("\n  %ld derivations tried, %d match%s\n", tried, hits, hits == 1 ? "" : "es");
    if (hits == 0)
        printf("  No construction in this set reproduces the tag. Either the key is not\n"
               "  in these identities, or the derivation takes something else as well\n"
               "  (a counter, a salt, the advert's flag bytes).\n");
    else
        printf("  Confirm with a second (address, tag) pair before building on it: a\n"
               "  3-byte target can be hit by chance about once in 16.8M tries.\n");

    for (int b = 0; b < g_n_blobs; b++) memset(g_blobs[b].bytes, 0, sizeof(g_blobs[b].bytes));
    return hits > 0 ? 0 : 1;
}
