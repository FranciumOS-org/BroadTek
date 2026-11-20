# The LE advertising stack never starts, and why

Traced 2026-11-03 against `/usr/sbin/bluetoothd` on this machine (macOS 26,
Darwin 25.6.0). Addresses are from the x86_64 slice, where
**vmaddr = 0x100000000 + file offset**:

```sh
lipo -thin x86_64 /usr/sbin/bluetoothd -output btd.x86
objdump -d --start-address=0x... --stop-address=0x... btd.x86
```

The binary is stripped, so nothing below is a symbol; the names are the ones
the log strings and the control flow justify.

## The symptom

Every Continuity advertiser is refused, forever, about twice a second:

    Server.LE.Advertise  Received 'start advertising' request ... manufacturer
        data [ 07 FF 4C 00 10 02 0B 00 ] ... { com.apple.sharingd }
    Server.LE.Advertise  Stack is not ready, waiting for the stack to start
    Server.LE.Advertise  Stack is not started, updateAdvertisement returning

and once at startup:

    LeBroadcaster.mm::stackDidStart() -- status != OI_OK -- status = 0x0098
    [Stack.VSC] Invalid chipset 0
    Server.Core] MUC - enhancedFilterAcceptListSupported? 0 chipset 0x0, 306

So this Mac emits **no** BLE advertisements at all. BLE *scanning* works
normally — dozens of devices found, parsed and handed to sharingd — which is
why the split is easy to miss. Advertising is what every Continuity feature
is built on, so AirDrop (the Mac never appears on the iPhone), Handoff,
Universal Clipboard, Watch unlock and iPhone Mirroring all fail together.

## The chain, top down

`LeBroadcaster::stackDidStart` is at `0x100698087`. The advertise gate it
feeds is at `0x10069edda`, which tests one byte at `+0x31` of the broadcaster
and logs "Stack is not ready" when it is not 1. That byte stays 0 because
stackDidStart fails:

```
100698087  LeBroadcaster::stackDidStart
1006980eb:   callq *0x1d0(%rcx)      ; on the device-capability singleton
                                     ;   (accessor 0x10039422a -> 0x100b3ab60)
                                     ; a dispatch_once-cached model check whose
                                     ;   argument is the CFString "AppleTV"
100698107:   xorb  $0x1, %al         ; so al = !isAppleTV = 1 on a Mac
10069810c:   leal  (%rax,%rax,2), %edi ; edi = 3
10069811b:   callq 0x100219d8e       ; returns 0x98
```

**`edi = 3` is correct and normal on a Mac.** Mode 3 is *resolvable random
address*; mode 0, the public address, is the Apple TV case. An earlier reading
of this as a "legacy vendor-specific fallback" was wrong — the `"AppleTV"`
check is the whole of that branch, and nothing about it is broken here.

`0x100219d8e` is `LE_GAP_GenerateAddress`-and-init. Its RRA path
(`0x10021b2eb` mode 3, via the jump table at `0x10021b6d0`) succeeds: the IRK
is non-zero (the zero-IRK branch returns `0x67`, "Cannot generate current
user's RRA when the IRK is set to 0's") and `ah()` at `0x10016215d` is plain
CommonCrypto AES-128, which cannot fail here. The `0x98` comes from the next
call, `LE_InitMultipleAdvertising` at `0x1001f77d2`, and it is one test:

```
1001f7870:   cmpb $0x1, 0x100b2fe21
1001f7877:   jne  0x1001f797f        ; -> movl $0x98, %ebx ; return 0x98
```

## The gate is a chipset number, not an HCI feature

`0x100b2fe21` has exactly three writers, all one-line setters:

```
0x1001f7e2b   g_multiAdvSupported = arg
0x1001f7e38   g_extAdvSupported   = arg;  if (arg) g_multiAdvSupported = 1
0x1001f7e50   g_maxAdvInstances   = arg        (0x100b294d0)
```

and their only callers are the controller-capability parse at `0x100510e58`,
`0x100510e67` and `0x100510ef4`. Both values there come from the
device-capability singleton — `*0x218` for extended advertising, `*0x268` for
multiple advertising — **not from HCI**. Each is a dispatch_once block that
ORs four platform-class predicates and then asks the *chipset* singleton
(`0x100394472`), and that query is the whole decision:

```
10033b712  chipset vtable *0x9c0   (multiple advertising supported?)
10033b716:   movl 0x334(%rdi), %ecx       ; the chipset number
10033b71c:   cmpl $0x7d0, %ecx            ; 2000
10033b722:   jb   0x10033b738
10033b724:   cmpl $0xf9f, %ecx            ; 3999
10033b72a:   ja   0x10033b74d
...
```

which reduces to: **true for chipset 19–25, 2001–3999, 5000–5999 and >= 6000;
false for 0, for 1, for exactly 2000, and for 4000.** The advertising
*instance count* (`*0xf88`, `0x100357f9c`) reads the same `+0x334` and gives
16 instances for chipset 9–19.

This is the same shape as the LL Privacy dead end in STATE.md §3, and it is
worth being blunt about: **the controller already reports what macOS wants and
macOS does not look.** BroadTek's own capture has

    Stack.HCI  LE local supported features BD 5F 66 3C 01 00 00 00

where byte 1 = `0x5F` has **bit 12 (LE Extended Advertising) set**. bluetoothd
reads that line, logs it, and then decides extended-advertising support from
`+0x334` anyway. Faking LE features cannot move this gate, exactly as faking
bit 6 could not move IRK handover.

## The chipset enum, decoded

The names are a consecutive string table at file offset `0x9612db`, in enum
order. Two independent checks confirm the indexing: the classifier calls
`setChipset(0)` while logging "setting chipset to THIRD_PARTY_DONGLE", and
`setChipset(1)` while logging "setting chipset to default BCM_4350C2".

```
 0 THIRD_PARTY_DONGLE    9 BCM_43430      18 BCM_4364B3
 1 BCM_4350C2           10 BCM_43452      19 BCM_4377     <-- passes, 16 inst.
 2 BCM_20702A3          11 BCM_4355B3     20 BCM_4378     <-- passes
 3 BCM_20702B0          12 BCM_4355C0     21 BCM_4378B3   <-- passes
 4 BCM_4350             13 BCM_4355C1     22 BCM_4387     <-- passes
 5 BCM_4350A0           14 BCM_4355       23 BCM_4388     <-- passes
 6 BCM_4350C5           15 BCM_4357       24 BCM_4388C2   <-- passes
 7 BCM_20703A1          16 BCM_4364       25 BCM_4399     <-- passes
 8 BCM_20703A2          17 BCM_4364B0     26 APPLE_M1 ...
```

The accepted set is precisely the modern Apple Broadcom parts. Note that
`BCM_4350C2` (1), the value bluetoothd itself falls back to for a dongle when
NVRAM says not to switch, **does not pass** — so "pretend to be a 4350C2" is
not a route.

## Where the chipset number is assigned

`setChipset(int)` is `0x10033c402` (writes `0x334(%rdi)`); the wrapper the
classifier calls is `0x1001609d7`. The classifier is `0x1001d1be0`-ish, and
its branches are:

| log line | value set |
| --- | --- |
| `Found external Bluetooth dongle -- setting chipset to THIRD_PARTY_DONGLE` | `0` |
| same branch, transport type `0xb` | `4000` |
| `External Bluetooth dongle %s -- setting chipset to default BCM_4350C2` | `1` |
| `NVRAM setting ... is set to 'never' -- setting chipset to default BCM_4350C2` | `1` |
| **`External Bluetooth dongle not found -- setting chipset to default Apple PCIe`** | **`0xbb8` = 3000** |
| `External Bluetooth dongle not found -- setting chipset to default APPLE_VIRTUAL` | (see `0x1001d1df1`) |

**The internal-module default, 3000, passes the gate.** Of the values a
dongle can ever receive — 0, 1, 4000 — none does. The entire difference
between a working advertising stack and no advertising at all is which of
those branches runs.

The branch is chosen by a transport/device-type value dispatched through the
jump table at `0x1001d1f48` (12 entries), after a test at `0x1001d1c67`
(`callq *0x50` on `0x100b1ac10`) that fills it. The NVRAM variable
`bluetoothHostControllerSwitchBehavior` is read by
`[GetExternalDongleSwitchBehavior]` and steers part of this; `'never'` is one
of its values. Also present: `Using external Bluetooth dongle, do not read
info from NVRAM` — so for a non-dongle the chipset *is* read from NVRAM,
which is a settable surface.

## What this means for BroadTek

The card is USB `13D3:3571` and bluetoothd classifies it as an external
dongle, so it lands on chipset 0 and the advertising stack never starts.
There is no preference that can override this; see the next section for the
one that looks like it can and does not.

Two routes, in order of promise:

1. **Make the classifier not take the external-dongle branch**, so it falls
   through to the Apple PCIe default of 3000. The observable that decides it
   is now known and it is one byte: the `bDeviceClass` IORegistry property on
   the `IOUSBHostDevice` node. See the next section -- this is the thing to
   try first.
2. **Find whether the chipset number can be set from NVRAM** on the path that
   reads it, and whether that path can be reached. OpenCore can set NVRAM
   variables, so if a reachable NVRAM key feeds `+0x334`, this is cheap.

Both are above the HCI stream, so neither is a hook of the kind BroadTek
already has. That is a change in the shape of this project, and worth
deciding deliberately rather than drifting into.

## The dongle test itself: one byte, `bDeviceClass`

Traced further the same day. The third condition above is a function-pointer
table entry, `*0x50` of the ops table at `0x100b1ac10`, which is
`0x1000e996f` -- the probe whose log line is
`[bm3_usb][Found_USB_Dongle] -- Found USB Device :`. It is plain IOKit:

```
IOServiceMatching("IOUSBHostDevice")
IOServiceGetMatchingServices(...)
for each node, read: "USB Vendor Name", "USB Product Name",
                     "idVendor", "idProduct", "bDeviceClass", "locationID"
```

All of `idVendor`, `idProduct`, `bDeviceClass` and `locationID` must be
present and readable as numbers (`CFNumberGetValue` with type 9 = sInt32);
any missing one skips that node. Then the entire verdict is one test on
`bDeviceClass`:

```
1000e9b9d:   movl -0x34(%rbp), %eax      ; bDeviceClass
1000e9ba0:   movl $0xffffff20, %ecx      ; -0xe0
1000e9ba5:   addl %ecx, %eax
1000e9ba7:   cmpl $0x1f, %eax
1000e9bae:   ja   0x1000e9cb5            ; reject: outside [0xe0, 0xff]
1000e9bb4:   movl $0x80008001, %ecx      ; bits 0, 15 and 31
1000e9bb9:   btl  %eax, %ecx
1000e9bbc:   jae  0x1000e9cb5            ; reject: bit clear
```

`0x80008001` has bits 0, 15 and 31 set, so the accepted set is exactly

    bDeviceClass in { 0xE0, 0xEF, 0xFF }

that is **Wireless Controller, Miscellaneous, or Vendor Specific**. Nothing
else about the device matters -- not the vendor ID, not the product ID, not
the name. The names and IDs are only logged.

The RTL8852BE's Bluetooth function is USB `13D3:3571` with
`bDeviceClass = 0xE0`, so it matches, and that single byte is the whole reason
this machine is `THIRD_PARTY_DONGLE` with chipset 0 and no advertising. It
also explains why spoofing the vendor ID to Apple's `0x004C`, which was
already done, changed nothing.

`bDeviceClass` is an IORegistry property on the `IOUSBHostDevice` node, so
this is reachable from outside bluetoothd -- by OpenCore device properties or
by a kext -- which is what makes it the first thing to try. Check the current
value with:

```sh
ioreg -r -c IOUSBHostDevice -l | grep -A40 -i "3571" | \
    grep -iE "bDeviceClass|idVendor|idProduct|locationID|USB Product Name"
```

### The fifth check, partly decoded

After the four tests, and before it sets its "found" flag, bluetoothd calls
`0x1000ed0c0` on the node. It reads the registry entry name (falling back to
the string "unknown") and then opens the device:

```
IOCreatePlugInInterfaceForService(service,
    9DC7B780-9EC0-11D4-A54F-000A27052861,   ; kIOUSBDeviceUserClientTypeID
    C244E858-109C-11D4-91D4-0050E4C6426F)   ; kIOCFPlugInInterfaceID
-> QueryInterface(56AD089D-878D-4BEA-A1F5-2C8DC43E8A98)  ; a USB device interface
```

and then works with that interface; the function is large and the rest of it
is not traced. The UUID byte lists are built with
`CFUUIDGetConstantUUIDWithBytes` one byte per argument, which is why they do
not appear as strings.

What matters is that it is a real inspection of the device over USB rather
than another property read, so it is the plausible place where a webcam gets
rejected and a Bluetooth radio does not. It is **not** established that it
does reject the webcam, and the experiment does not depend on the answer,
because it changes every candidate.

There are two explicit exceptions, checked after the class test: a node is
*not* taken as a dongle if its IDs are `05AC:110A` (logged as "Apple USB-C to
3.5mm Headphone") or `0451:3410`. A following block compares the vendor
against `0x0A12` and `0x0A5C` and a `0B05:190E` pair, but the control flow
there falls through unconditionally to accept, so those comparisons have no
effect. Everything else with an accepted `bDeviceClass` is taken.

### Which device wins is not yet known, and it decides the experiment

Measured on this machine, two USB nodes satisfy the test:

```
"USB Product Name" = "Bluetooth Radio"       Realtek 13D3:3571  bDeviceClass 224 (0xE0)
"USB Product Name" = "USB2.0 HD UVC WebCam"  Sonix   322E:2233  bDeviceClass 239 (0xEF)
```

Nothing in the probe checks that the device is Bluetooth -- the names are only
logged -- and the webcam is not on the two-entry exception list. So the node
bluetoothd settles on depends on the order `IOServiceGetMatchingServices`
returns them, and it may well be the **webcam**. If it is, changing the
Realtek's `bDeviceClass` alone will not move anything, and the webcam's would
have to change too (or instead).

bluetoothd says which it picked:

    [bm3_usb][GetProductAndVendorID] -- Found external Bluetooth dongle --
        vendorID = 0x%04x, produceID = 0x%04x

**Settle that before changing any property.** Note that the `[bm3_usb]` lines
are `printf` to stderr through `0x10024fa53`, not os_log -- which is why they
carry no timestamp or subsystem in a capture, and why `log show` with a
`process == "bluetoothd"` predicate does not return them. They have to be
caught live, at the boot or Bluetooth restart where the classification runs.

### The risk, which has to be tested rather than reasoned about

If bluetoothd no longer recognises the device as a Bluetooth dongle, the open
question is whether it still *uses* it. This probe is a classifier inside
`GetProductAndVendorID`, and the HCI transport is found separately -- the log
already reports `HCIT UART` for a device that is physically USB, so the two
are not the same lookup. But that is a hypothesis, and the failure mode if it
is wrong is no Bluetooth at all until the property is put back.

So the test is: change `bDeviceClass` to something outside
`{0xE0, 0xEF, 0xFF}`, restart, and read, in this order:

1. `Found external Bluetooth dongle` should be **absent** from the log.
2. `setting chipset to default Apple PCIe` (chipset 3000) or one of the other
   non-dongle branches should appear instead.
3. `system_profiler SPBluetoothDataType` should no longer say
   `Chipset: THIRD_PARTY_DONGLE`.
4. `LeBroadcaster.mm::stackDidStart()` should report `OI_OK` rather than
   `0x0098`, and `Server.LE.Advertise` should stop saying "Stack is not
   ready".
5. Bluetooth must still **work** -- AirPods, keyboard, mouse. If the
   controller is no longer claimed at all, revert.

That sequence distinguishes all the interesting outcomes, including the one
where the chipset changes and the advertising gate still refuses, which would
mean the platform predicates in the capability blocks matter too.

## The chipset number comes from a VSC reply, which is BroadTek's own layer

This supersedes the "one untraced thread" note below, and it is the most
promising thing found so far.

`setChipset` (`0x10033c402`, via the wrapper `0x1001609d7`) has **two**
clusters of callers, not one:

* `0x1001d1c81` and friends -- the USB classifier, which is where
  `THIRD_PARTY_DONGLE` (0) comes from and which is a dead end for the reasons
  below.
* `0x100171ec6`, `0x100171ee9`, `0x100171f77`, `0x100172079` -- inside
  **`initVSCmdReadChipConfigVersionInfoCB`**, the completion callback for the
  vendor-specific command `BD_VSC_READ_VERBOSE_CONFIG_VERSION_INFO`.

So the chipset number is normally taken from **what the controller says when
asked**. That is an HCI vendor command and its reply, which is exactly the
stream BroadTek already hooks and already rewrites -- unlike the USB
classifier, which sits above it and cannot be reached from a kext.

### The chip-id byte to chipset map, decoded

The callback sets `BCM_4350C2` (1) first and then refines it from one byte of
the reply (`0x100171f7c` onwards, including the jump table at `0x1001720f0`
for 0xa7-0xaf). An unmapped byte leaves it at 1, which fails the gate.

| chip id | chipset | name | passes the advertising gate? |
| --- | --- | --- | --- |
| 0x3f | 2 | BCM_20702A3 | no |
| 0x49 | 3 | BCM_20702B0 | no |
| 0x50 | 5 | BCM_4350A0 | no |
| 0x5c | 4 | BCM_4350 | no |
| 0x66 | 7 | BCM_20703A1 | no |
| 0x7b | 17 | BCM_4364B0 | no |
| 0x7e | 8 | BCM_20703A2 | no |
| 0x85 | 14 | BCM_4355 | no |
| **0x92** | **19** | **BCM_4377** | **yes, and 16 advertising instances** |
| 0x96 | 18 | BCM_4364B3 | no |
| 0x98 | 20 | BCM_4378 | yes |
| 0xa7 | 22 | BCM_4387 | yes |
| 0xa9 | 21 | BCM_4378B3 | yes |
| 0xab, 0xaf | 23 | BCM_4388 | yes |
| 0xb8 | 24 | BCM_4388C2 | yes |
| anything else | 1 | BCM_4350C2 | no |

**0x92 is the target.** It is the lowest byte that both passes
`*0x9c0` (chipset 19 satisfies `c - 19 < 7`) and reaches the
`*0xf88` branch that yields 16 advertising instances, and BCM_4377 is a
Bluetooth 5.0 part, so the rest of what macOS then expects of it is the
closest to what this controller can actually do.

A second site in the same callback, `0x100171ec1`, sets 5001 unconditionally
and then refines a 16-bit id -- `0x7923` to 5000, `0x7922` to 4000. 5000 and
5001 both pass the gate. That looks like a newer family and is probably not
the path this controller takes, but it is a second way in if the byte map is
not reachable.

### Checked and ruled out: the company-id / sub-version override

`[Device]` has three keys that look made for this -- `OverrideCompanyId`,
`NewCompanyId`, `NewSubVersion` -- read at `0x1005148ce` onwards under a guard
that the chipset is <= 5999, so chipset 0 qualifies. They call `*0x8d8` on the
chipset singleton with the two 16-bit values, which dispatches a block to
`0x1001eebbb`.

That turns out to go the **wrong way**: it issues a vendor command *to the
controller* to write a new company id and sub-version, and its failure path
logs `VSC failed %!`. It programs the hardware, it does not change
bluetoothd's own chipset number, and on this controller it would simply fail.
Not a route.

The whole override surface is now extracted into
[BLUETOOTHD_PREFERENCES.md](BLUETOOTHD_PREFERENCES.md) -- every two-level
lookup in the binary, 570 keys across 104 domains, including a 207-key `[LE]`
domain and an `[AE]` domain of extended-advertising switches
(`DisableExtAdvertising`, `EnableBackgroundExtendedAdvertising`,
`SetExtendedAdvertisingParametersV2`). Nothing in it writes `+0x334`, which is
why none of it moves the gate; it is there so the next person does not have to
re-derive it, and because `[AE]` is worth a look if the VSC route closes.

### Checked and ruled out: `AE` / `DisableExtAdvertising`

The gate has two inputs, not one: `0x1001f7e38` sets `g_multiAdvSupported = 1`
whenever **extended** advertising is supported, so the extended-advertising
capability (`*0x218`, once-block `0x10040cbf2`) is a second way in. And it has
a preference override of its own -- domain `AE`, key `DisableExtAdvertising`.

It cannot help, and the reason is one instruction:

```
10040cd31:   xorl %r14d, %r14d         ; r14d = 0
10040cd73:   movb -0x41(%rbp), %al     ; the preference value
10040cd76:   xorb $0x1, %al            ; al = !DisableExtAdvertising
10040cd78:   cmpb $0x0, 0x100b3b758    ; the chipset-derived value
10040cd82:   cmovnel %eax, %r14d       ; only when the chipset value is NONZERO
10040cd86:   movb %r14b, 0x100b3b758
```

that is, `result = chipsetValue ? !pref : 0`. The `cmovne` makes the
preference **disable-only**: it can turn extended advertising off on hardware
that has it, and can do nothing at all on hardware that does not. With
chipset 0 the stored value stays 0 however the key is set.

And the chipset-derived value it gates on, `*0x918` (`0x10033abec`), is the
same range test as `*0x9c0` -- `[0x7d0, 0xf9f]` with 0x7d0 itself excluded,
plus 19-25 and >= 6000 -- so chipset 0 is false there too.

So **both** inputs to `g_multiAdvSupported` are chipset-gated, and no
preference anywhere can enable either. That is the whole preference surface
exhausted; see BLUETOOTHD_PREFERENCES.md.

### Answered 2026-11-03 15:57: the VSC is never sent. Every route is closed.

Captured with `--info`, the supported-VSC list printed under
`THIRD_PARTY_DONGLE supports the following VSCs` is **empty** -- the loop that
logs each supported entry produces nothing. Every vendor command bluetoothd
subsequently attempts is answered locally:

    Sending: BD_VSC_LE_META_ADD_IRK_TO_LIST
    VSC BD_VSC_LE_META_ADD_IRK_TO_LIST, not supported
    Sending: BD_VSC_OLYMPIC_SET_EXT_ADV_LIMITS
    VSC BD_VSC_OLYMPIC_SET_EXT_ADV_LIMITS, not supported
    ... and so on for every one

each pair sharing a timestamp to the microsecond, so nothing reaches the
controller. This is the same mechanism §3 recorded for LL Privacy, now seen to
be general rather than specific to the IRK command.

**`BD_VSC_READ_VERBOSE_CONFIG_VERSION_INFO` does not appear at all** -- it is
not attempted, let alone sent. So `initVSCmdReadChipConfigVersionInfoCB` never
runs, and the chipset number is never corrected from the controller. The
dependency is circular and there is no way in:

    chipset 0  ->  no VSC is supported
               ->  the version VSC is never sent
               ->  the callback never runs
               ->  chipset stays 0

### Correction 2026-11-03: "every route is closed" was wrong

The paragraph below overstated what had been shown. What is proven is narrower:
**at chipset 0** the VSC set is empty, so the version VSC is never sent and the
number cannot be corrected. That is a statement about chipset 0, not about
every reachable state -- and chipset 0 is not the only state the classifier can
produce.

Inside the dongle-found path, after `setChipset(0)`, there is a second test:

```
1001d1c81:   callq setChipset            ; 0 = THIRD_PARTY_DONGLE
1001d1c86:   cmpl $0xb, -0x50(%rbp)      ; the type the probe wrote
1001d1c91:   jne  ...
1001d1c97:   movl $0xfa0, %edi           ; 4000
1001d1c9c:   callq setChipset            ; replaces the 0
1001d1ca1:   movl $0xb, %r15d
```

and the probe writes that type `0xb` (`0x1000e9d9b`,
`movl $0xb, (%rax)`) when the device's ids match one of **seven** pairs in the
table at `0x100b1ad60`:

```
0489:e0d8  0489:e0e0  0489:e0f1  0489:e0f2  0489:e0f6
0e8d:7922  13d3:3568
```

Those are **MediaTek** parts -- `0e8d:7922` is an MT7922, the `0489:` entries
are its Foxconn-badged modules, and `13d3:3568` is the MediaTek sibling of this
card's own `13d3:3571`. So macOS has a first-class *non-Apple* controller path,
and the other `setChipset` cluster decoded above is its own:

| reported id | chipset | passes the gate? |
| --- | --- | --- |
| (default in that callback) | 5001 | **yes** |
| 0x7923 (MT7923) | 5000 | **yes** |
| 0x7922 (MT7922) | 4000 | no |

Crucially this branch is **inside** the dongle-found path, so
`gUseInternalController` stays FALSE and the controller is still used. It
changes the chipset number *without* costing the controller -- which is exactly
what the `bDeviceClass` route could not do.

### The route that has not been tried

Two steps, both using machinery BroadTek already has and has already been seen
to work:

1. **Make bluetoothd read different USB ids.** It takes `idVendor` and
   `idProduct` from the IO registry, so overriding those two properties on the
   `IOUSBHostDevice` node -- exactly as `kern_usbclass.cpp` already overrides
   `bDeviceClass`, on `gIOMatchedNotification`, after the real drivers have
   matched -- makes the probe match `0e8d:7922` and assign chipset 4000.
   `bDeviceClass` stays `0xE0` and `FoundBluetoothInterfaces()` still inspects
   the card's real interfaces, so both still pass. The dry run already proved
   the notifier fires in time and sees this node.
2. **Then see whether chipset 4000 has a non-empty VSC set.** If it does, the
   circularity is broken: `BD_VSC_READ_VERBOSE_CONFIG_VERSION_INFO` gets sent,
   and BroadTek can rewrite the reply so the 16-bit id reads `0x7923` ->
   chipset 5000 -> the gate passes. Rewriting a reply on the way up is the same
   edit that already demonstrably reached bluetoothd for the LE features line
   (§3: `FD` where the controller sent `BD`).

Step 1 is one boot and tells you step 2's answer for free, because the VSC list
is logged. The risk is the mirror of the earlier one and much smaller:
bluetoothd will send MediaTek vendor commands to a Realtek controller, which
will fail -- but it keeps the controller, so the failure mode is "Bluetooth
behaves as badly as it does now", not "no Bluetooth".

There is also a cruder route that was dismissed too quickly: **patching
bluetoothd in memory**. One byte, `g_multiAdvSupported` at `0x100b2fe21`, or a
hook on `setChipset`. This machine already boots with
`csr-active-config 0xA03` and AMFIPass, which is most of what injecting into a
platform binary needs. It is fragile across updates and it is not this
project's style, but it is not impossible, and saying otherwise was wrong.

### The conclusion, now proven rather than suspected


`g_multiAdvSupported` gates the LE advertising stack. It has two inputs and
both are decided by the chipset number. The chipset number has exactly two
sources and both are closed:

1. **The USB classifier** gives `THIRD_PARTY_DONGLE` (0). It cannot be
   steered, because `gUseInternalController = !foundDongle` and the same
   verdict chooses whether the controller is used at all -- hiding the radio
   costs the controller, not the classification.
2. **The VSC reply** could give a passing value (chip id `0x92` -> chipset 19,
   BCM_4377, with 16 advertising instances), but the command is never sent
   precisely *because* the chipset is 0.

And no preference can substitute: `SupportsMultipleAdvertising` writes an
unrelated field, `AE`/`DisableExtAdvertising` is disable-only by `cmovne`, and
the company-id keys program the controller rather than bluetoothd. The full
570-key surface is in BLUETOOTHD_PREFERENCES.md and none of it writes `+0x334`.

So BLE advertising cannot be reached **by way of chipset 0**, which is where
this machine currently sits. Read the correction above before treating that as
final: the MediaTek classification path changes the chipset while keeping the
controller, and it has not been tried. AirDrop, Handoff, Universal
Clipboard, Watch unlock and iPhone Mirroring all fail together and all fail
for this one reason.

That matches STATE.md §3's conclusion for LL Privacy, and the two are now
understood to be the same wall: macOS decides what the controller can do from
its own chipset classification, reads the controller's real capabilities,
logs them, and ignores them. The remedy is an Apple-supported controller --
a BCM94360-class card or a USB BCM20702 adapter.

### What would still be worth doing

Nothing in Bluetooth. The AWDL side of AirDrop is healthy and was measured
working; see AWDL_HANDOFF.md and the AirPort_RTW89 repository. If a supported
Bluetooth controller is ever fitted, the advertising stack should start on its
own and the AWDL work is already in place to meet it.

### How to capture it


The unified log on this machine rolls within minutes, so `--last boot` is
empty by the time anyone looks. Restart the daemon and read immediately:

```sh
sudo pkill bluetoothd; sleep 12
log show --last 1m --info --predicate 'process == "bluetoothd"' | \
    grep -E "BD_VSC_|supports the following" | head -80
```

**`--info` is required.** The header line
`%{public}s supports the following VSCs` is emitted at `OS_LOG_TYPE_DEFAULT`
(`xorl %esi, %esi` before `os_log_type_enabled` at `0x100378b8e`), but the
per-VSC names in the loop that follows are at `OS_LOG_TYPE_INFO`
(`movl $0x1, %esi` at `0x100378c11`). `log show` drops Info by default, so
without the flag the header appears with an empty list under it and it looks
as though no VSC is supported. That is a measurement artefact, not a result.

Note that an earlier conclusion here -- that the USB probe does not re-run on
a `pkill` restart -- was **unsound**. It was inferred from an empty grep over
a six-hour window, which is explained by the log rolling rather than by the
probe not running. Treat a restart as probably sufficient, and if the
`bm3_usb` lines do appear then the whole investigation needs no reboots.

## Measured 2026-11-03: the bDeviceClass route is self-defeating. Do not run stage `class`.

The dry run was worth its boot -- it settled two things and killed the plan.

**The fifth check is `FoundBluetoothInterfaces()`, and it does reject the
webcam.** From the boot log with `-broadtekclassdry`:

    [bm3_usb][Found_USB_Dongle] -- FoundBluetoothInterfaces() for deviceClass
        (239), idVendor = 0x322E, idProduct = 0x2233 returned false
    [bm3_usb][Found_USB_Dongle] -- FoundBluetoothInterfaces() for deviceClass
        (224), idVendor = 0x13D3, idProduct = 0x3571 returned true
    [bm3_usb][Found_USB_Dongle] -- setting gUseInternalController to FALSE
    Found external Bluetooth dongle -- setting chipset to THIRD_PARTY_DONGLE
    [bm3_usb][GetProductAndVendorID] -- Use external Bluetooth USB Host Controller
    [bm3_usb][GetProductAndVendorID] -- Found external Bluetooth dongle --
        vendorID = 0x13D3, produceID = 0x3571

So only the Bluetooth radio was ever a real candidate, and the webcam never
mattered. `dongle_probe` is right to call both "candidates" and right to say
the boot log is the authority.

The kext side works: `broadtek-usbclass` read `"dry run"` and the notifier saw
both nodes, so `gIOMatchedNotification` fires early enough and the property
write would have landed.

**But the dongle verdict also chooses the controller.** The single writer of
`gUseInternalController`, at the end of `Found_USB_Dongle`:

```
1000e9e1a:   movq -0x50(%rbp), %r14      ; the "found a dongle" flag
1000e9e21:   xorb $0x1, %al              ; al = !found
1000e9e23:   movb %al, 0x100b1aed0       ; gUseInternalController
```

and `GetProductAndVendorID` branches on it at `0x1000ee745`: FALSE takes the
external path that reports the dongle's ids and leads to
`Configuring bluetooth device: Bluetooth USB Host Controller`, TRUE logs
"Use internal Bluetooth USB Host Controller" and returns failure.

This machine has no internal controller. So hiding the radio from the class
test does not buy chipset 3000 -- it sets `gUseInternalController` to TRUE,
leaves bluetoothd with no controller at all, and the LE stack never starts for
a different and less interesting reason. `stackDidStart` would never run, so
the experiment cannot even answer the question it was built for.

The chipset number and the controller selection are the same boolean, written
two instructions apart, and no IO registry property can separate them. Stage
`class` is therefore kept only as a diagnostic for some other question; it
should not be run against this one.

### What that means for the project

The advertising gate is **not reachable from below bluetoothd**. BroadTek's
whole approach -- hook the HCI stream and make the controller look like
something it is not -- cannot help here, because the refusal happens above
HCI, keyed on a classification that is decided by USB enumeration and that
simultaneously decides whether the controller is used at all.

That leaves only what §3 already concluded for LL Privacy, and for the same
underlying reason: replace the Bluetooth side with an Apple-supported
controller. The difference is that this is now measured rather than suspected,
and the mechanism is written down, so it does not need discovering again.

One thread is left untraced, and it is the only one with any promise: there is
a second `setChipset` call site at `0x1004a6e4e` that takes its value from a
getter rather than a constant, on the path that NVRAM feeds ("Using external
Bluetooth dongle, do not read info from NVRAM"). An attempt to identify that
getter went to the wrong vtable and landed on an unrelated AirPods key, so
nothing about it is established. If anyone picks this up, that is where to
start -- and the question to answer first is whether that path can run while
`gUseInternalController` is FALSE.

## Running the experiment


Built and ready; nothing has been loaded or installed. Three pieces:

**`make dongle-probe`** builds `build/out/dongle_probe`, which reproduces the
four tests above against the live IO registry and lists the candidates. It
needs no privileges and changes nothing. On this machine, today:

```
  #   ids       class product name                 verdict
  0   322e:2233 0xef  USB2.0 HD UVC WebCam         candidate
  1   13d3:3571 0xe0  Bluetooth Radio              candidate
```

Both qualify -- the webcam on `0xEF` and the radio on `0xE0` -- so the webcam
has to be considered too, even though the fifth check may already be rejecting
it.

**`-broadtekclassdry`** (install stage `classdry`) logs which nodes the kext
*would* change and changes nothing. Worth one boot on its own: it confirms the
notifier fires before anything is at risk.

**`-broadtekclass`** (install stage `class`) replaces the `bDeviceClass`
property of every qualifying node with `0x00`, which is outside
`{0xE0, 0xEF, 0xFF}`. `broadtekclass=<n>` picks a different value, and a value
that is itself in the accepted set is refused and logged.

`src/plugin/kern_usbclass.cpp` does this on `gIOMatchedNotification`, not
`gIOFirstPublishNotification`: by then the node's own drivers have matched, so
changing a property they matched on cannot change which driver claimed it.
bluetoothd's probe is a userspace read much later, so being late costs nothing
and removes the one way this could take the webcam or the radio away entirely.

### The order to run it in

```sh
make kext                                   # then the user installs
sudo efi/install.sh --stage classdry        # restart
```

After that restart:

```sh
ioreg -l | grep -i broadtek-usbclass        # "dry run", and the log names the nodes
./build/out/dongle_probe                    # unchanged: still two candidates
```

Then the real thing:

```sh
sudo efi/install.sh --stage class           # restart
```

and read, in this order -- each line distinguishes a different outcome:

1. `ioreg -l | grep -i broadtek-usbclass` -- should name both nodes and their
   old and new classes. If it says "waiting for a device", the notifier never
   fired and nothing else below means anything.
2. `./build/out/dongle_probe` -- should now report **no candidates**.
3. `log show --last boot --predicate 'process == "bluetoothd"' | grep -iE
   "bm3_usb|external Bluetooth dongle|setting chipset"` -- should say
   **dongle not found** and name an internal default instead of
   `THIRD_PARTY_DONGLE`. This is also the only place that says which node
   bluetoothd had been choosing before.
4. `system_profiler SPBluetoothDataType | grep -i chipset` -- should no longer
   say `THIRD_PARTY_DONGLE`.
5. `log show --last boot --predicate 'process == "bluetoothd"' | grep -iE
   "LeBroadcaster|Stack is not"` -- the prize: `stackDidStart` reporting
   `OI_OK` instead of `0x0098`, and no more "Stack is not ready".
6. **Bluetooth must still work** -- AirPods, keyboard, mouse, and the webcam
   in Photo Booth. If the controller or the camera is no longer claimed,
   revert.

An outcome where 1–4 succeed and 5 still fails is informative rather than a
failure: it would mean the four platform-class predicates in the capability
blocks matter as well, and those are not chipset-based.

Revert with `sudo efi/install.sh --stage off` (or `--stage classdry`) and a
restart; the property is only ever set at runtime, so a boot without the
boot-arg is a clean machine. `sudo efi/uninstall.sh` backs the whole plugin
out.

### What has and has not been verified

The kext builds and links, and the two new objects compile clean under
`-Wall -Werror`. `make test` could **not** be run in the environment this was
written in: the Command Line Tools there have no
`libclang_rt.asan_osx_dynamic.dylib`, so the sanitized `hosttest` binary
cannot start. The same tests were built without sanitizers and pass
(`all checks passed`), and `tests/configure_test.py` passes. Run `make test`
on a machine with a full Xcode before loading, because the rule in
docs/STATE.md §10 is worth keeping.

## The preference that looks like the answer and is not

Do not spend time on this; it was tried and measured.

There are eleven override keys in a `"LE"` domain, `SupportsMultipleAdvertising`
among them, reached through the preferences singleton `0x1003943f2`, vtable
slot `*0x48` (`0x10036b8fe`):

```objc
id v = CFPreferencesCopyAppValue(@"LE", @"com.apple.MobileBluetooth.debug");
if (![v isKindOfClass:NSDictionary.class]) return false;
id val = [v objectForKey:@"SupportsMultipleAdvertising"];
if (![val isKindOfClass:NSString.class]) return false;      // a *string*
if ([val isEqualToString:@"TRUE"] || [val isEqualToString:@"YES"]) *out = true;
else if ([val isEqualToString:@"FALSE"] || [val isEqualToString:@"NO"]) *out = false;
else return false;
```

So it is `/Library/Preferences/com.apple.MobileBluetooth.debug.plist`, an `LE`
sub-dictionary, and the value is a **string** — a `-bool true` fails the
`isKindOfClass:` check, reads as "not found", and logs nothing at all, which
costs an experiment to discover:

```sh
sudo defaults write /Library/Preferences/com.apple.MobileBluetooth.debug \
    LE -dict-add SupportsMultipleAdvertising -string TRUE
sudo pkill bluetoothd
```

Done correctly it does fire:

    Server.Core] Warning: Overriding Multiple Advertising support - enabled

and it changes nothing — `status = 0x0098` exactly as before. The override
writes `+0xa0` on the *chipset-capability* object (ctor `0x100339b60`, vtable
`0x100ab5950`), and the gate at `0x1001f7870` is fed from `+0x334` of the
chipset singleton by way of `*0x9c0`. Two unrelated fields. Delete it again:

```sh
sudo defaults delete /Library/Preferences/com.apple.MobileBluetooth.debug LE
```

The other ten keys in that domain, for the record:
`DisableSensorTrack`, `forceHostSlowDownLEScanOnACLProtectMode`,
`ForceRssiThresholdOrderType19`, `AppleExtendedAdvertisingReport`,
`OverrideSupportLEAdvTxPowerIncrease`, `OverrideSupportLEConnTxPowerIncrease`,
`SupportsOutboxSoftwareUpdateAdvertising`, `SupportsChannelSounding`,
`SupportsChannelSoundingBasedOnRegion`, `SupportsChannelSoundingPhyLog`.

`move3PPLEMSToLegacyMode` in `com.apple.Bluetooth.plist` is unrelated despite
the name: it is third-party LE **mice** (`numberOfAppleMSOrTP`,
`is15msSupportedByController`).

## The adverts had no address (2026-11-18): fixed by stage `advaddr`

With the advertising gate patched, bluetoothd reported `Started advertising
successfully`, but a same-account iPhone never showed a Handoff banner from this
Mac. The capture in 978c1ac (`broadtek-adv-cmds`, decoded by
`tools/adv_decode.py`) showed why:
- every extended advertising set (0, 2, 3) is set up by 0x2036 with own address
  type random;
- bluetoothd sends only the legacy LE Set Random Address (0x2005);
- LE Set Advertising Set Random Address (0x2035) is never sent.

So no set carried bluetoothd's RPA. The Realtek accepts the enable anyway, where
the spec says it should refuse.

b36913e (`-broadtekadvaddr`, `sudo efi/install.sh --stage advaddr`) sends 0x2035
with the latest 0x2005 address before each enable of such a set. It waits for the
reply and gives bluetoothd a NOP Command Complete in its place. Measured: 89 sent,
89 replies, status 0, no timeouts. **The Handoff banner appears on the iPhone.**
It is the only command BroadTek sends itself.
