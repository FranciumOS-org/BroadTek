/* SPDX-License-Identifier: MIT */
/* Read a peer's IRK out of the keychain and hand it to the kext.
 *
 * macOS never gives this controller an IRK over HCI: it decides what the
 * controller may do from its chipset classification, not from the HCI
 * features, and a THIRD_PARTY_DONGLE is offered neither the resolving list
 * nor Apple's IRK vendor command (docs/STATE.md §3). The key does exist
 * locally though, because the bond was synced from iCloud, and it is readable
 * here -- the entitlement is honoured on this machine, which boots with AMFI
 * enforcement off.
 *
 *     make irk-install && sudo build/out/irk_install
 *
 * The key is read, converted and written to the kext. It is never printed,
 * never logged, and never put anywhere a later reader could find it, which is
 * the same rule the kext follows for the registry and the kernel log. Only
 * the identity address it belongs to is reported, and that is not a secret.
 *
 * On byte order, see --reverse-irk below: it is the one thing about this that
 * could not be settled by reading. */
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <IOKit/IOKitLib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BLOB_LEN (1 + 6 + 16)

/* "Public 8C:00:00:00:00:02" -> type 0, and the address least significant
 * byte first, which is the order HCI and src/core use. */
static int parseAccount(CFStringRef acct, uint8_t *type, uint8_t addr[6])
{
    char buf[128] = "";
    if (acct == NULL || CFGetTypeID(acct) != CFStringGetTypeID()) return 0;
    if (!CFStringGetCString(acct, buf, sizeof(buf), kCFStringEncodingUTF8)) return 0;

    const char *p = buf;
    if (strncmp(p, "Public", 6) == 0)      { *type = 0; p += 6; }
    else if (strncmp(p, "Random", 6) == 0) { *type = 1; p += 6; }
    else                                   { *type = 0; }
    while (*p == ' ') p++;

    unsigned v[6];
    if (sscanf(p, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6)
        return 0;
    for (int i = 0; i < 6; i++) {
        if (v[i] > 0xff) return 0;
        addr[5 - i] = (uint8_t)v[i];        /* displayed MSB first, stored LSB first */
    }
    return 1;
}

/* Hand one blob to the kext through BroadTekConfig's setProperties. */
static int install(const uint8_t *blob)
{
    io_service_t svc = IOServiceGetMatchingService(kIOMainPortDefault,
                                                   IOServiceMatching("BroadTekConfig"));
    if (svc == IO_OBJECT_NULL) {
        fprintf(stderr, "BroadTekConfig not found: is the kext loaded?\n");
        return 0;
    }

    CFDataRef data = CFDataCreate(NULL, blob, BLOB_LEN);
    CFStringRef key = CFSTR("broadtek-install-irk");
    CFDictionaryRef props = CFDictionaryCreate(NULL, (const void **)&key,
                                               (const void **)&data, 1,
                                               &kCFTypeDictionaryKeyCallBacks,
                                               &kCFTypeDictionaryValueCallBacks);
    kern_return_t kr = IORegistryEntrySetCFProperties(svc, (CFTypeRef)props);

    CFRelease(props);
    CFRelease(data);
    IOObjectRelease(svc);

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "  the kext refused it: 0x%x%s\n", kr,
                kr == kIOReturnNotPrivileged ? " (not privileged: run with sudo)" : "");
        return 0;
    }
    return 1;
}

/* How many resolving-list entries the kext says are live, from the counter it
 * publishes on the platform entry; -1 if it cannot be read. */
static int liveEntries(void)
{
    io_registry_entry_t plat = IOServiceGetMatchingService(kIOMainPortDefault,
                                    IOServiceMatching("IOPlatformExpert"));
    if (plat == IO_OBJECT_NULL) return -1;
    CFTypeRef v = IORegistryEntryCreateCFProperty(plat, CFSTR("broadtek-resolving-list"),
                                                  NULL, 0);
    IOObjectRelease(plat);
    if (v == NULL) return -1;
    long long n = -1;
    if (CFGetTypeID(v) == CFNumberGetTypeID())
        CFNumberGetValue(v, kCFNumberLongLongType, &n);
    CFRelease(v);
    return (int)n;
}

/* One pass: read every bond and install it. Returns how many went in. */
static int runOnce(int reverse, int quiet)
{

    /* The bonds are iCloud-synced, so synchronizable items must be included
     * or the query comes back empty. */
    const void *qk[] = { kSecClass, kSecAttrService, kSecAttrAccessGroup,
                         kSecMatchLimit, kSecReturnAttributes, kSecReturnData,
                         kSecAttrSynchronizable };
    const void *qv[] = { kSecClassGenericPassword, CFSTR("BluetoothLESync"),
                         CFSTR("com.apple.bluetooth"), kSecMatchLimitAll,
                         kCFBooleanTrue, kCFBooleanTrue, kSecAttrSynchronizableAny };
    CFDictionaryRef q = CFDictionaryCreate(NULL, qk, qv, 7,
                                           &kCFTypeDictionaryKeyCallBacks,
                                           &kCFTypeDictionaryValueCallBacks);
    CFTypeRef found = NULL;
    OSStatus st = SecItemCopyMatching(q, &found);
    CFRelease(q);

    if (st != errSecSuccess || found == NULL) {
        fprintf(stderr, "no LE bond readable (error %d)%s\n", (int)st,
                st == errSecMissingEntitlement ? ": entitlement refused" :
                st == errSecParam ? ": not in a login session (run through launchctl asuser)" : "");
        return 0;       /* nothing installed: the caller must not count this */
    }
    if (CFGetTypeID(found) != CFArrayGetTypeID()) { CFRelease(found); return 0; }

    CFIndex n = CFArrayGetCount(found);
    printf("%ld bond(s) in the keychain%s\n", (long)n,
           reverse ? ", installing the IRK byte-reversed" : "");

    int done = 0;
    for (CFIndex i = 0; i < n; i++) {
        CFDictionaryRef item = CFArrayGetValueAtIndex(found, i);
        if (CFGetTypeID(item) != CFDictionaryGetTypeID()) continue;

        uint8_t blob[BLOB_LEN] = {0};
        if (!parseAccount(CFDictionaryGetValue(item, kSecAttrAccount),
                          &blob[0], &blob[1])) {
            fprintf(stderr, "  item %ld: could not read its address\n", (long)i);
            continue;
        }

        CFDataRef body = CFDictionaryGetValue(item, kSecValueData);
        if (body == NULL || CFGetTypeID(body) != CFDataGetTypeID()) continue;

        CFPropertyListRef pl = CFPropertyListCreateWithData(NULL, body,
                                                            kCFPropertyListImmutable,
                                                            NULL, NULL);
        if (pl == NULL) { fprintf(stderr, "  item %ld: not a property list\n", (long)i); continue; }

        CFDataRef irk = NULL;
        if (CFGetTypeID(pl) == CFDictionaryGetTypeID())
            irk = CFDictionaryGetValue(pl, CFSTR("Remote IRK"));

        if (irk == NULL || CFGetTypeID(irk) != CFDataGetTypeID() ||
            CFDataGetLength(irk) != 16) {
            fprintf(stderr, "  item %ld: no 16-byte Remote IRK\n", (long)i);
            CFRelease(pl);
            continue;
        }

        uint8_t key[16];
        CFDataGetBytes(irk, CFRangeMake(0, 16), key);
        for (int k = 0; k < 16; k++) blob[7 + k] = reverse ? key[15 - k] : key[k];
        memset(key, 0, sizeof(key));
        CFRelease(pl);

        printf("  %02x:%02x:%02x:%02x:%02x:%02x (%s): ",
               blob[6], blob[5], blob[4], blob[3], blob[2], blob[1],
               blob[0] ? "random" : "public");
        fflush(stdout);
        if (install(blob)) { printf("installed\n"); done++; }

        memset(blob, 0, sizeof(blob));
    }
    CFRelease(found);

    if (!quiet) printf("\n%d key(s) installed.\n", done);
    return done;
}

int main(int argc, char **argv)
{
    int reverse = 0, wait = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--reverse-irk") == 0) reverse = 1;
        else if (strcmp(argv[i], "--wait") == 0) wait = 1;
    }

    if (!wait) {
        int done = runOnce(reverse, 0);
        if (done > 0)
            printf("Check broadtek-resolving-list in ioreg. If advertising reports\n"
                   "never resolve, try --reverse-irk: whether the keychain stores the\n"
                   "key in the same byte order HCI uses was not determined by reading,\n"
                   "and the wrong order simply never matches.\n");
        return done > 0 ? 0 : 1;
    }

    /* --wait is for the LaunchDaemon. Two things can be missing this early:
     * the keychain may not be readable yet, and the kext's config service may
     * not have registered. Both resolve on their own within a minute or so of
     * boot, so retry rather than fail -- but bounded, because a daemon that
     * retries forever is a daemon nobody notices is broken. */
    const int attempts = 60, gap = 5;       /* five minutes */
    for (int i = 1; i <= attempts; i++) {
        int done = runOnce(reverse, 1);
        if (done > 0 && liveEntries() >= done) {
            printf("installed %d key(s) on attempt %d\n", done, i);
            /* Early in boot a key the kext accepted has been found missing a
             * minute later (2026-11-20, cause not pinned down: the kernel log
             * had wrapped). Keep handing it down for the first two minutes.
             * Re-installing the same key is harmless: the kext keeps its
             * state, and the count it publishes is refreshed each time. */
            int lost = 0;
            for (int w = 0; w < 24; w++) {
                sleep(gap);
                int before = liveEntries();
                runOnce(reverse, 1);
                if (before < done) lost++;
            }
            printf("kept the key in place for two minutes; %d pass(es) found it missing\n",
                   lost);
            return 0;
        }
        if (done > 0)
            fprintf(stderr, "kext accepted %d key(s) but lists %d live (attempt %d)\n",
                    done, liveEntries(), i);
        if (i == 1 || i % 6 == 0)
            fprintf(stderr, "nothing installed yet (attempt %d of %d); retrying\n",
                    i, attempts);
        sleep(gap);
    }
    fprintf(stderr, "gave up after %d attempts: no key installed\n", attempts);
    return 1;
}
