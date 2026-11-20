/* SPDX-License-Identifier: MIT */
/* Can anything on this machine read bluetoothd's keychain items?
 *
 * Every other route to a peer's IRK is closed (docs/STATE.md §3): macOS will
 * not hand one over through HCI, the devices are iCloud-paired so no bonding
 * exchange ever happens here to capture, and the LE device database holds no
 * keys. The keychain, access group com.apple.bluetooth, is the last source.
 *
 * Reading it needs the keychain-access-groups entitlement naming that group,
 * which is Apple-private. This machine boots with AMFI entitlement
 * enforcement off (amfi_get_out_of_my_way=0x1, amfi=0x80), so a locally
 * signed binary claiming it may be allowed. This program finds out, and
 * nothing more.
 *
 *     make probe && sudo build/out/keychain_probe
 *
 * It answers one question: are the items reachable, and is there anything
 * of the right shape to be an IRK. It therefore prints counts, classes,
 * attribute names and byte *lengths*.
 *
 * It never prints key material. Every CFData is reported as its length and
 * its contents are not passed to any output function anywhere in this file --
 * not for attributes, not for the item's data. An IRK that got printed to a
 * terminal, a scrollback buffer or a log would be a worse outcome than not
 * having read it at all. The same rule governs the kext, which keeps captured
 * keys out of the IO registry and the kernel log. */
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <stdio.h>
#include <unistd.h>

static const char *errName(OSStatus s)
{
    switch (s) {
    case errSecSuccess:              return "success";
    case errSecItemNotFound:         return "no such item";
    case errSecMissingEntitlement:   return "MISSING ENTITLEMENT";
    case errSecInteractionNotAllowed: return "interaction not allowed";
    case errSecAuthFailed:           return "authentication failed";
    case errSecParam:                return "bad parameter";
    case errSecNotAvailable:         return "keychain not available";
    default:                         return "?";
    }
}

/* What *kind* of blob this is, which is needed to know how to parse out an
 * IRK later. Only the container format is reported: a format is not a secret,
 * and nothing from inside the blob is printed. */
static const char *blobFormat(CFDataRef d)
{
    CFIndex n = CFDataGetLength(d);
    if (n < 2) return "";
    uint8_t head[8] = {0};
    CFRange r = CFRangeMake(0, n < 8 ? n : 8);
    CFDataGetBytes(d, r, head);

    if (n >= 8 && memcmp(head, "bplist00", 8) == 0) return "  binary plist";
    if (head[0] == '<' || head[0] == '{')           return "  text";
    if (head[0] == 0x30)                            return "  DER/ASN.1";
    return "  opaque";
}

/* The shape of a bond blob: which fields it has and how big each is.
 *
 * Key names and sizes only. No value of any kind is printed -- not data, not
 * even strings, because a key could perfectly well be stored hex-encoded in
 * one. A field called IRK with 16 bytes in it is a complete answer to "which
 * field is the key" and requires seeing none of it. */
static void describeNode(CFTypeRef node, int depth);

static void describeEntry(const void *key, const void *value, void *ctx)
{
    int depth = *(int *)ctx;
    char name[128] = "?";
    if (CFGetTypeID(key) == CFStringGetTypeID())
        CFStringGetCString(key, name, sizeof(name), kCFStringEncodingUTF8);
    printf("%*s%s:", 8 + depth * 2, "", name);
    describeNode(value, depth + 1);
}

static void describeNode(CFTypeRef node, int depth)
{
    if (node == NULL) { printf(" (null)\n"); return; }
    CFTypeID t = CFGetTypeID(node);

    if (t == CFDictionaryGetTypeID()) {
        printf(" dict, %ld entr%s\n", (long)CFDictionaryGetCount(node),
               CFDictionaryGetCount(node) == 1 ? "y" : "ies");
        int d = depth;
        CFDictionaryApplyFunction(node, describeEntry, &d);
    } else if (t == CFArrayGetTypeID()) {
        CFIndex n = CFArrayGetCount(node);
        printf(" array, %ld item%s\n", (long)n, n == 1 ? "" : "s");
        for (CFIndex i = 0; i < n; i++) {
            printf("%*s[%ld]:", 8 + depth * 2, "", (long)i);
            describeNode(CFArrayGetValueAtIndex(node, i), depth + 1);
        }
    } else if (t == CFDataGetTypeID()) {
        CFIndex n = CFDataGetLength(node);
        printf(" data, %ld bytes%s\n", (long)n,
               n == 16 ? "   <-- the size of an IRK" : "");
    } else if (t == CFStringGetTypeID()) {
        printf(" string, %ld chars\n", (long)CFStringGetLength(node));
    } else if (t == CFBooleanGetTypeID()) {
        printf(" bool\n");
    } else if (t == CFNumberGetTypeID()) {
        printf(" number\n");
    } else if (t == CFDateGetTypeID()) {
        printf(" date\n");
    } else {
        printf(" (other type)\n");
    }
}

/* Parse a bond blob and describe it, or say why it could not be parsed. */
static void describeBond(CFDataRef data)
{
    CFErrorRef err = NULL;
    CFPropertyListRef pl = CFPropertyListCreateWithData(NULL, data,
                                                        kCFPropertyListImmutable,
                                                        NULL, &err);
    if (pl == NULL) {
        printf("        not a property list");
        if (err != NULL) {
            CFIndex code = CFErrorGetCode(err);
            printf(" (error %ld)", (long)code);
            CFRelease(err);
        }
        printf("; container format is%s\n", blobFormat(data));
        return;
    }
    printf("       ");
    describeNode(pl, 0);
    CFRelease(pl);
}

/* Print one attribute. A CFData becomes its length: never its bytes. */
static void showValue(const void *key, const void *value, void *ctx)
{
    (void)ctx;
    char name[128] = "?";
    if (CFGetTypeID(key) == CFStringGetTypeID())
        CFStringGetCString(key, name, sizeof(name), kCFStringEncodingUTF8);

    CFTypeID t = CFGetTypeID(value);
    if (t == CFDataGetTypeID()) {
        /* The one case that matters, and the one that must stay opaque. */
        printf("      %-24s <%ld bytes>%s\n", name, (long)CFDataGetLength(value),
               blobFormat(value));
    } else if (t == CFStringGetTypeID()) {
        char buf[256] = "";
        CFStringGetCString(value, buf, sizeof(buf), kCFStringEncodingUTF8);
        printf("      %-24s %s\n", name, buf);
    } else if (t == CFNumberGetTypeID()) {
        long long n = 0;
        CFNumberGetValue(value, kCFNumberLongLongType, &n);
        printf("      %-24s %lld\n", name, n);
    } else if (t == CFBooleanGetTypeID()) {
        printf("      %-24s %s\n", name, CFBooleanGetValue(value) ? "true" : "false");
    } else if (t == CFDateGetTypeID()) {
        printf("      %-24s (date)\n", name);
    } else {
        printf("      %-24s (other type)\n", name);
    }
}

/* One query: a class, in or out of the data-protection keychain. */
static int probe(CFStringRef cls, const char *clsName, int dataProtection,
                 CFStringRef group, const char *groupName)
{
    const void *keys[8];
    const void *vals[8];
    CFIndex n = 0;

    keys[n] = kSecClass;              vals[n++] = cls;
    keys[n] = kSecAttrAccessGroup;    vals[n++] = group;
    keys[n] = kSecMatchLimit;         vals[n++] = kSecMatchLimitAll;
    keys[n] = kSecReturnAttributes;   vals[n++] = kCFBooleanTrue;
    /* Asked for so the item's size can be reported. Never printed. */
    keys[n] = kSecReturnData;         vals[n++] = kCFBooleanTrue;
    /* The bonds are iCloud-synced, so synchronizable items must be included
     * or a populated keychain would look empty. */
    keys[n] = kSecAttrSynchronizable; vals[n++] = kSecAttrSynchronizableAny;
    if (dataProtection) {
        keys[n] = kSecUseDataProtectionKeychain; vals[n++] = kCFBooleanTrue;
    }

    CFDictionaryRef q = CFDictionaryCreate(NULL, keys, vals, n,
                                           &kCFTypeDictionaryKeyCallBacks,
                                           &kCFTypeDictionaryValueCallBacks);
    CFTypeRef result = NULL;
    OSStatus st = SecItemCopyMatching(q, &result);
    CFRelease(q);

    printf("  %-20s %-18s %-16s %s (%d)\n", groupName, clsName,
           dataProtection ? "data-protection" : "file keychain",
           errName(st), (int)st);

    int found = 0;
    if (st == errSecSuccess && result != NULL) {
        if (CFGetTypeID(result) == CFArrayGetTypeID()) {
            CFIndex count = CFArrayGetCount(result);
            found = (int)count;
            printf("    %ld item(s)\n", (long)count);
            for (CFIndex i = 0; i < count; i++) {
                CFTypeRef item = CFArrayGetValueAtIndex(result, i);
                printf("    item %ld:\n", (long)i);
                if (CFGetTypeID(item) == CFDictionaryGetTypeID())
                    CFDictionaryApplyFunction(item, showValue, NULL);
            }
        } else {
            found = 1;
            printf("    1 item (not an array)\n");
        }
    }
    if (result != NULL) CFRelease(result);
    return found;
}

/* One exact service, in one group. Used to ask where a device's bond is
 * without listing a group's contents: the iPhone's LE bond is not in
 * com.apple.bluetooth, and finding it must not mean enumerating "apple". */
static int probeService(const char *svc, CFStringRef group, const char *groupName)
{
    CFStringRef svcRef = CFStringCreateWithCString(NULL, svc, kCFStringEncodingUTF8);
    const void *keys[7];
    const void *vals[7];
    CFIndex n = 0;

    keys[n] = kSecClass;              vals[n++] = kSecClassGenericPassword;
    keys[n] = kSecAttrService;        vals[n++] = svcRef;
    keys[n] = kSecAttrAccessGroup;    vals[n++] = group;
    keys[n] = kSecMatchLimit;         vals[n++] = kSecMatchLimitAll;
    keys[n] = kSecReturnAttributes;   vals[n++] = kCFBooleanTrue;
    keys[n] = kSecReturnData;         vals[n++] = kCFBooleanTrue;
    keys[n] = kSecAttrSynchronizable; vals[n++] = kSecAttrSynchronizableAny;

    CFDictionaryRef q = CFDictionaryCreate(NULL, keys, vals, n,
                                           &kCFTypeDictionaryKeyCallBacks,
                                           &kCFTypeDictionaryValueCallBacks);
    CFTypeRef result = NULL;
    OSStatus st = SecItemCopyMatching(q, &result);
    CFRelease(q);
    CFRelease(svcRef);

    int count = 0;
    if (st == errSecSuccess && result != NULL && CFGetTypeID(result) == CFArrayGetTypeID())
        count = (int)CFArrayGetCount(result);

    printf("  %-22s in %-22s %s (%d), %d item(s)\n", svc, groupName,
           errName(st), (int)st, count);

    for (int i = 0; i < count; i++) {
        CFTypeRef item = CFArrayGetValueAtIndex(result, i);
        if (CFGetTypeID(item) != CFDictionaryGetTypeID()) continue;
        CFTypeRef acct = CFDictionaryGetValue(item, kSecAttrAccount);
        CFTypeRef data = CFDictionaryGetValue(item, kSecValueData);
        char who[128] = "(no account)";
        if (acct != NULL && CFGetTypeID(acct) == CFStringGetTypeID())
            CFStringGetCString(acct, who, sizeof(who), kCFStringEncodingUTF8);
        printf("      %-34s %ld bytes%s\n", who,
               data != NULL ? (long)CFDataGetLength(data) : 0L,
               data != NULL ? blobFormat(data) : "");
        if (data != NULL && CFGetTypeID(data) == CFDataGetTypeID())
            describeBond(data);
    }
    if (result != NULL) CFRelease(result);
    return count;
}

/* One device, by its exact account name, in one group.
 *
 * This is how to look outside com.apple.bluetooth without listing anybody's
 * credential store: the account is a single Bluetooth address, so the query
 * can match only that device's bond and nothing else. The Watch's key is in
 * com.apple.bluetooth under BluetoothLESync; the iPhone's is not, and whether
 * it exists anywhere locally decides whether Instant Hotspot is reachable. */
static int probeAccount(const char *acct, CFStringRef group, const char *groupName)
{
    CFStringRef acctRef = CFStringCreateWithCString(NULL, acct, kCFStringEncodingUTF8);
    const void *keys[] = { kSecClass, kSecAttrAccount, kSecAttrAccessGroup,
                           kSecMatchLimit, kSecReturnAttributes, kSecReturnData,
                           kSecAttrSynchronizable };
    const void *vals[] = { kSecClassGenericPassword, acctRef, group,
                           kSecMatchLimitAll, kCFBooleanTrue, kCFBooleanTrue,
                           kSecAttrSynchronizableAny };

    CFDictionaryRef q = CFDictionaryCreate(NULL, keys, vals, 7,
                                           &kCFTypeDictionaryKeyCallBacks,
                                           &kCFTypeDictionaryValueCallBacks);
    CFTypeRef result = NULL;
    OSStatus st = SecItemCopyMatching(q, &result);
    CFRelease(q);
    CFRelease(acctRef);

    int count = 0;
    if (st == errSecSuccess && result != NULL && CFGetTypeID(result) == CFArrayGetTypeID())
        count = (int)CFArrayGetCount(result);

    printf("  %-26s in %-22s %s (%d), %d item(s)\n", acct, groupName,
           errName(st), (int)st, count);

    for (int i = 0; i < count; i++) {
        CFTypeRef item = CFArrayGetValueAtIndex(result, i);
        if (CFGetTypeID(item) != CFDictionaryGetTypeID()) continue;
        CFTypeRef svc = CFDictionaryGetValue(item, kSecAttrService);
        char svcName[128] = "(no service)";
        if (svc != NULL && CFGetTypeID(svc) == CFStringGetTypeID())
            CFStringGetCString(svc, svcName, sizeof(svcName), kCFStringEncodingUTF8);
        CFTypeRef data = CFDictionaryGetValue(item, kSecValueData);
        printf("      service %-24s %ld bytes\n", svcName,
               data != NULL ? (long)CFDataGetLength(data) : 0L);
        if (data != NULL && CFGetTypeID(data) == CFDataGetTypeID()) describeBond(data);
    }
    if (result != NULL) CFRelease(result);
    return count;
}

/* Fetch one item's data by exact service and account. Caller releases. */
static CFDataRef fetchBlob(const char *svc, const char *acct, CFStringRef group)
{
    CFStringRef svcRef = CFStringCreateWithCString(NULL, svc, kCFStringEncodingUTF8);
    CFStringRef acctRef = CFStringCreateWithCString(NULL, acct, kCFStringEncodingUTF8);
    const void *k[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecAttrAccessGroup,
                        kSecMatchLimit, kSecReturnData, kSecAttrSynchronizable };
    const void *v[] = { kSecClassGenericPassword, svcRef, acctRef, group,
                        kSecMatchLimitOne, kCFBooleanTrue, kSecAttrSynchronizableAny };
    CFDictionaryRef q = CFDictionaryCreate(NULL, k, v, 7,
                                           &kCFTypeDictionaryKeyCallBacks,
                                           &kCFTypeDictionaryValueCallBacks);
    CFTypeRef out = NULL;
    OSStatus st = SecItemCopyMatching(q, &out);
    CFRelease(q); CFRelease(svcRef); CFRelease(acctRef);
    if (st != errSecSuccess || out == NULL) return NULL;
    if (CFGetTypeID(out) != CFDataGetTypeID()) { CFRelease(out); return NULL; }
    return (CFDataRef)out;
}

/* Does Rapport's 61-byte device identity contain a Bluetooth IRK?
 *
 * The Watch answers it. Its IRK is known -- it is in BluetoothLESync and it
 * demonstrably resolves that device's advertisements -- and it also has a
 * Rapport identity. So: does the known key appear anywhere inside the Rapport
 * blob, forwards or byte-reversed?
 *
 * If it does, the iPhone's Rapport blob is a route to its key and Instant
 * Hotspot is reachable. If it does not, Rapport keeps something else and the
 * phone has no IRK stored on this Mac.
 *
 * Nothing is printed but a yes or no and an offset. Both inputs are key
 * material and neither is rendered. */
static void rapportCarriesIRK(void)
{
    printf("\n  -- does Rapport's identity contain the Bluetooth IRK? --\n");

    CFDataRef bond = fetchBlob("BluetoothLESync", "Public 8C:00:00:00:00:02",
                               CFSTR("com.apple.bluetooth"));
    CFDataRef rp = fetchBlob("RPIdentity-SameAccountDevice",
                             "00000000-0000-0000-0000-000000000001",
                             CFSTR("com.apple.rapport"));
    if (bond == NULL || rp == NULL) {
        printf("      could not read both of the Watch's items\n");
        if (bond != NULL) CFRelease(bond);
        if (rp != NULL) CFRelease(rp);
        return;
    }

    /* The IRK out of the bond plist. */
    uint8_t irk[16];
    int haveIrk = 0;
    CFPropertyListRef pl = CFPropertyListCreateWithData(NULL, bond,
                                                        kCFPropertyListImmutable, NULL, NULL);
    if (pl != NULL && CFGetTypeID(pl) == CFDictionaryGetTypeID()) {
        CFDataRef d = CFDictionaryGetValue(pl, CFSTR("Remote IRK"));
        if (d != NULL && CFGetTypeID(d) == CFDataGetTypeID() && CFDataGetLength(d) == 16) {
            CFDataGetBytes(d, CFRangeMake(0, 16), irk);
            haveIrk = 1;
        }
    }
    if (pl != NULL) CFRelease(pl);

    if (!haveIrk) {
        printf("      no 16-byte Remote IRK in the Watch's bond\n");
        CFRelease(bond); CFRelease(rp);
        return;
    }

    CFIndex n = CFDataGetLength(rp);
    uint8_t blob[128];
    if (n > (CFIndex)sizeof(blob)) n = sizeof(blob);
    CFDataGetBytes(rp, CFRangeMake(0, n), blob);

    uint8_t rev[16];
    for (int i = 0; i < 16; i++) rev[i] = irk[15 - i];

    int at = -1, atRev = -1;
    for (CFIndex off = 0; off + 16 <= n; off++) {
        if (memcmp(blob + off, irk, 16) == 0 && at < 0) at = (int)off;
        if (memcmp(blob + off, rev, 16) == 0 && atRev < 0) atRev = (int)off;
    }

    printf("      Watch IRK inside its %ld-byte Rapport identity: ", (long)n);
    if (at >= 0)         printf("YES at offset %d\n", at);
    else if (atRev >= 0) printf("YES byte-reversed at offset %d\n", atRev);
    else                 printf("no\n");

    memset(irk, 0, sizeof(irk));
    memset(rev, 0, sizeof(rev));
    memset(blob, 0, sizeof(blob));
    CFRelease(bond); CFRelease(rp);
}

int main(void)
{
    printf("BroadTek keychain probe\n");
    printf("  euid %d%s\n", (int)geteuid(),
           geteuid() == 0 ? "" : "  (not root: expect to see nothing)");
    printf("\n");

    struct { CFStringRef cls; const char *name; } classes[] = {
        { kSecClassGenericPassword,  "generic password" },
        { kSecClassInternetPassword, "internet password" },
        { kSecClassKey,              "key" },
        { kSecClassCertificate,      "certificate" },
        { kSecClassIdentity,         "identity" },
    };

    /* com.apple.bluetooth and nothing else.
     *
     * The entitlement also claims "apple", and querying it once returned 119
     * items: Apple ID authentication tokens and other credentials with no
     * connection to Bluetooth whatsoever. A tool for finding one device's
     * identity key has no business enumerating a person's credential store,
     * so that group is deliberately not queried. If an LE bond ever has to be
     * looked for outside com.apple.bluetooth, it should be by an exact
     * service name, not by listing a group. */
    struct { CFStringRef group; const char *name; } groups[] = {
        { CFSTR("com.apple.bluetooth"), "com.apple.bluetooth" },
    };

    int total = 0;
    for (size_t g = 0; g < sizeof(groups) / sizeof(groups[0]); g++)
        for (int dp = 0; dp <= 1; dp++)
            for (size_t i = 0; i < sizeof(classes) / sizeof(classes[0]); i++)
                total += probe(classes[i].cls, classes[i].name, dp,
                               groups[g].group, groups[g].name);

    /* Where is the iPhone's bond? Asked by exact service, never by listing. */
    printf("\n  -- LE bonds by exact service name --\n");
    probeService("BluetoothLESync", CFSTR("com.apple.bluetooth"), "com.apple.bluetooth");
    probeService("BluetoothLESync", CFSTR("apple"), "apple");
    probeService("BluetoothGlobal", CFSTR("com.apple.bluetooth"), "com.apple.bluetooth");

    /* Hunting the iPhone's bond: every query below names one exact account,
     * so each can match only that device. No group is ever listed.
     *
     * The address is tried in the form the Watch's entry uses, and then the
     * phone's other identifiers, because a store that is not Bluetooth's may
     * key on a device UUID rather than a BD address. */
    struct { CFStringRef g; const char *name; } hunt[] = {
        { CFSTR("com.apple.bluetooth"),        "com.apple.bluetooth" },
        { CFSTR("com.apple.rapport"),          "com.apple.rapport" },
        { CFSTR("com.apple.sharing"),          "com.apple.sharing" },
        { CFSTR("com.apple.identityservices"), "com.apple.identityservices" },
        { CFSTR("com.apple.coreduetd"),        "com.apple.coreduetd" },
        { CFSTR("com.apple.wifi"),             "com.apple.wifi" },
        { CFSTR("apple"),                      "apple" },
    };
    const char *accounts[] = {
        "Public 64:00:00:00:00:01",                 /* as the Watch's is stored */
        "64:00:00:00:00:01",
        "Public 64:00:00:00:00:01",
        "00000000-0000-0000-0000-000000000002",     /* the phone's CBDevice UUID */
        "00000000-0000-0000-0000-000000000003",     /* its IDS identifier */
    };

    printf("\n  -- the iPhone, by exact account, across groups --\n");
    int phone = 0;
    for (size_t g = 0; g < sizeof(hunt) / sizeof(hunt[0]); g++)
        for (size_t a = 0; a < sizeof(accounts) / sizeof(accounts[0]); a++)
            phone += probeAccount(accounts[a], hunt[g].g, hunt[g].name);
    printf("  -> %d item(s) for the iPhone\n", phone);

    /* What else Rapport keeps for same-account devices. One exact service in
     * one group: device identities, not anybody's credentials. */
    printf("\n  -- Rapport's same-account device identities --\n");
    probeService("RPIdentity-SameAccountDevice", CFSTR("com.apple.rapport"), "com.apple.rapport");
    probeService("RPIdentity", CFSTR("com.apple.rapport"), "com.apple.rapport");
    probeService("BluetoothLESync", CFSTR("com.apple.rapport"), "com.apple.rapport");

    rapportCarriesIRK();

    printf("\n  -- the Watch, as a control --\n");
    probeAccount("Public 8C:00:00:00:00:02", CFSTR("com.apple.bluetooth"), "com.apple.bluetooth");

    printf("\n%d item(s) readable in all.\n", total);
    if (total == 0)
        printf("Nothing reachable: either the entitlement was refused or the\n"
               "group holds nothing. Check for MISSING ENTITLEMENT above --\n"
               "if that is the reason, the keychain route is closed too.\n");
    else
        printf("Reachable. Look for a 16-byte item: that is an IRK's size.\n"
               "No key bytes were printed and none will be.\n");
    return total > 0 ? 0 : 1;
}
