/* SPDX-License-Identifier: MIT */

/* bluetoothd's external-dongle test, reproduced exactly.
 *
 * bluetoothd decides whether this machine has a third-party Bluetooth dongle
 * -- and therefore whether the LE advertising stack is allowed to start at all
 * -- in a probe at 0x1000e996f (docs/LE_ADVERTISING.md). The probe is plain
 * IOKit, and the whole verdict is a bit test on one byte:
 *
 *     IOServiceMatching("IOUSBHostDevice"), IOServiceGetMatchingServices
 *     read idVendor, idProduct, bDeviceClass, locationID; skip the node if
 *         any of the four is missing or is not a number
 *     accept if (bDeviceClass - 0xe0) is in { 0, 15, 31 }
 *         i.e. bDeviceClass in { 0xE0, 0xEF, 0xFF }
 *     except the two id pairs 05AC:110A and 0451:3410, which are skipped
 *
 * Nothing in those four tests checks that the device is Bluetooth; the product
 * name is only logged. So more than one node can qualify, and the order
 * IOServiceGetMatchingServices returns them matters. This tool walks that same
 * iterator in that same order and lists the candidates.
 *
 * It deliberately stops there. After the four tests above, bluetoothd calls
 * one more per-node check (0x1000ed0c0) before it sets its "found" flag, and
 * that one is not reproduced here: it reads the registry entry name and then
 * opens the node through an IOCFPlugIn interface, so it plausibly rejects
 * anything that is not really a Bluetooth device. A node listed as a candidate
 * below may therefore still be rejected. The authority on which node won is
 * bluetoothd's own line at boot:
 *
 *     [bm3_usb][GetProductAndVendorID] -- Found external Bluetooth dongle --
 *         vendorID = 0x%04x, produceID = 0x%04x
 *
 * What this tool is for is the opposite direction, where it is conclusive: a
 * node that fails here cannot be chosen. Getting every candidate to fail is
 * what makes bluetoothd say "dongle not found".
 *
 * Read-only, and needs no privileges. */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <stdbool.h>
#include <stdio.h>

/* The bits bluetoothd tests, as the immediate 0x80008001 in the binary. */
#define kClassMask   0x80008001u
#define kClassBase   0xe0

static bool read_u32(io_service_t svc, const char *key, uint32_t *out)
{
    CFStringRef name = CFStringCreateWithCString(kCFAllocatorDefault, key,
                                                 kCFStringEncodingUTF8);
    if (name == NULL) return false;

    CFTypeRef v = IORegistryEntryCreateCFProperty(svc, name, kCFAllocatorDefault, 0);
    CFRelease(name);
    if (v == NULL) return false;

    bool ok = false;
    if (CFGetTypeID(v) == CFNumberGetTypeID()) {
        /* bluetoothd asks for kCFNumberSInt32Type (type 9), so a wider value
         * is truncated the same way here. */
        int32_t n = 0;
        ok = CFNumberGetValue((CFNumberRef)v, kCFNumberSInt32Type, &n);
        if (ok) *out = (uint32_t)n;
    }
    CFRelease(v);
    return ok;
}

static void read_str(io_service_t svc, const char *key, char *buf, size_t len)
{
    buf[0] = '\0';
    CFStringRef name = CFStringCreateWithCString(kCFAllocatorDefault, key,
                                                 kCFStringEncodingUTF8);
    if (name == NULL) return;

    CFTypeRef v = IORegistryEntryCreateCFProperty(svc, name, kCFAllocatorDefault, 0);
    CFRelease(name);
    if (v == NULL) return;

    if (CFGetTypeID(v) == CFStringGetTypeID())
        CFStringGetCString((CFStringRef)v, buf, (CFIndex)len, kCFStringEncodingUTF8);
    CFRelease(v);
}

/* The accept test, byte for byte as bluetoothd does it. */
static bool class_accepted(uint32_t device_class)
{
    uint32_t i = device_class - kClassBase;
    if (i > 0x1f) return false;
    return ((kClassMask >> i) & 1u) != 0;
}

static bool id_excluded(uint32_t vendor, uint32_t product)
{
    if (vendor == 0x05ac && product == 0x110a) return true;  /* USB-C headphones */
    if (vendor == 0x0451 && product == 0x3410) return true;
    return false;
}

int main(void)
{
    CFMutableDictionaryRef match = IOServiceMatching("IOUSBHostDevice");
    if (match == NULL) {
        fprintf(stderr, "could not build the matching dictionary\n");
        return 1;
    }

    io_iterator_t it = IO_OBJECT_NULL;
    kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, match, &it);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "IOServiceGetMatchingServices failed: 0x%x\n", kr);
        return 1;
    }

    printf("IOUSBHostDevice nodes, in the order bluetoothd sees them:\n\n");
    printf("  %-3s %-9s %-5s %-28s %s\n",
           "#", "ids", "class", "product name", "verdict");

    int n = 0, winner = -1, candidates = 0;
    char winner_line[256] = "";

    io_service_t svc;
    while ((svc = IOIteratorNext(it)) != IO_OBJECT_NULL) {
        uint32_t vendor = 0, product = 0, device_class = 0, location = 0;
        char name[128], vendor_name[128];

        bool have = read_u32(svc, "idVendor", &vendor)
                 && read_u32(svc, "idProduct", &product)
                 && read_u32(svc, "bDeviceClass", &device_class)
                 && read_u32(svc, "locationID", &location);

        read_str(svc, "USB Product Name", name, sizeof(name));
        read_str(svc, "USB Vendor Name", vendor_name, sizeof(vendor_name));
        if (name[0] == '\0') snprintf(name, sizeof(name), "(no name)");

        const char *verdict;
        bool is_dongle = false;
        if (!have) {
            verdict = "skipped: a property is missing or not a number";
        } else if (!class_accepted(device_class)) {
            verdict = "no: class not in { E0, EF, FF }";
        } else if (id_excluded(vendor, product)) {
            verdict = "no: on the exception list";
        } else {
            verdict = "candidate";
            is_dongle = true;
        }

        char ids[16];
        if (have) snprintf(ids, sizeof(ids), "%04x:%04x", vendor, product);
        else      snprintf(ids, sizeof(ids), "?");

        char cls[8];
        if (have) snprintf(cls, sizeof(cls), "0x%02x", device_class);
        else      snprintf(cls, sizeof(cls), "?");

        printf("  %-3d %-9s %-5s %-28.28s %s\n", n, ids, cls, name, verdict);

        if (is_dongle) candidates++;
        if (is_dongle && winner < 0) {
            winner = n;
            snprintf(winner_line, sizeof(winner_line),
                     "%04x:%04x  class 0x%02x  locationID 0x%08x  \"%s\" by \"%s\"",
                     vendor, product, device_class, location, name, vendor_name);
        }

        IOObjectRelease(svc);
        n++;
    }
    IOObjectRelease(it);

    printf("\n");
    if (winner < 0) {
        printf("No node passes the class and id tests, so bluetoothd cannot\n"
               "choose one: it would log \"External Bluetooth dongle not found\"\n"
               "and take an internal-module default. That is the outcome the\n"
               "advertising gate needs.\n");
    } else {
        printf("%d candidate(s). The first is node %d:\n  %s\n\n",
               candidates, winner, winner_line);
        printf("Each candidate has to stop being one before bluetoothd will say\n"
               "\"dongle not found\" -- but a candidate can still be rejected by\n"
               "the further check this tool does not reproduce, so read the\n"
               "[bm3_usb] line at boot to see which one it actually took.\n");
    }

    return 0;
}
