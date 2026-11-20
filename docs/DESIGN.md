# BroadTek design

Goal: let a Realtek USB Bluetooth controller without LL Privacy (RTL8852BE combo,
USB `13D3:3571`) open LE connections to Apple devices that advertise with
resolvable private addresses (RPAs): Instant Hotspot on an iPhone, and the
Apple Watch approval prompt.

## What the logs show (macOS Sequoia 15.8.1)

- bluetoothd is the HCI host. It finds the dongle itself (`[bm3_usb]`), opens
  its USB interface through `AppleUSBHostInterfaceUserClient`, and sends HCI
  commands and receives events over USB from userspace. IOBluetoothFamily is not
  in the HCI path.
- The dongle is classified `THIRD_PARTY_DONGLE` (chipset 0). For that chipset
  bluetoothd answers every Broadcom VSC with "not supported" *itself*: "Sending"
  and "not supported" carry the same timestamp, so the command never reaches
  the controller. This applies to `BD_VSC_LE_META_ADD_IRK_TO_LIST`.
  **The IRK VSC therefore cannot be intercepted below bluetoothd.**
- LE local supported features are `BD 5F 66 3C 01 00 00 00`. Byte 0 is `0xBD`,
  so bit 6 (LL Privacy) is clear. Extended advertising (bit 12) is set.
- Without controller resolution, bluetoothd puts the peer's identity address on
  the Filter Accept List and starts an accept-list connection. The peer
  advertises from an RPA, nothing matches, and macOS cancels after about 10 s.

## The standard route is closed as well (2026-10-15)

Step 1 below was built and run on hardware. It works: bluetoothd logs
`LE local supported features FD 5F 66 3C`, the `FD` being BroadTek's edit of
the controller's `BD`. It makes no difference, because macOS does not decide
this from the controller's features:

```
Server.Core  MUC - enhancedFilterAcceptListSupported? 0 chipset 0x0, 284
Server.Core  LE Feature Vendor List is disabled (hw N)
Server.Core  THIRD_PARTY_DONGLE supports the following VSCs      (empty)
```

Both of Apple's RPA-resolution mechanisms, the IRK vendor command and the
standard HCI resolving list, are gated on the chipset classification. A
`THIRD_PARTY_DONGLE` is offered neither, so no `0x2027` is ever sent and
**BroadTek is never given an IRK.** Steps 2 to 5 below are therefore
unreachable as written, not because of how they are implemented but because
their input never arrives.

What survives: the transport. Events are read and rewritten on hardware,
commands are seen, and `src/core` is unchanged and tested. The approach
below remains correct about *what to do with an IRK* -- it is only wrong
about where one comes from. `docs/STATE.md` §3 has the evidence and the
remaining options.

## Approach (as designed; step 1 verified, steps 2-5 blocked)

Since the Broadcom route is closed, BroadTek uses the standard HCI route:

1. **Report LL Privacy.** Set bit 6 in the LE Read Local Supported Features
   reply and the resolving-list bits in the Read Local Supported Commands
   reply, so bluetoothd hands IRKs over with the standard commands.
2. **Emulate the resolving list in the host.** Let these commands go to the
   controller, which does not know them, and rewrite the Command Complete it
   sends to refuse each one into the success a privacy-capable controller would
   have returned. Keep their state:
   LE Add/Remove/Clear Device To Resolving List (`0x2027`/`0x2028`/`0x2029`),
   LE Read Resolving List Size (`0x202A`), LE Read Peer/Local RPA
   (`0x202B`/`0x202C`), LE Set Address Resolution Enable (`0x202D`),
   LE Set RPA Timeout (`0x202E`) and LE Set Privacy Mode (`0x204E`).
3. **Resolve RPAs in software.** In LE Advertising Report (subevent `0x02`) and
   LE Extended Advertising Report (`0x0D`), check every random address whose top
   two bits are `01` against each stored IRK using `ah()` (Core spec Vol 3
   Part H 2.2.2, AES-128). On a match, remember identity ↔ current RPA and
   report the identity address with type `0x02`/`0x03` (resolved public/random),
   as a privacy-capable controller would.
4. **Connect to the RPA.** When bluetoothd adds or removes an identity address
   with a known IRK on the Filter Accept List (`0x2011`/`0x2012`), rewrite that
   address to the last-seen RPA, type random. Same for a direct LE (Extended)
   Create Connection (`0x200D`/`0x2043`). Own address types `0x02`/`0x03` are
   mapped to `0x00`/`0x01`.
5. **Translate back.** In LE Connection Complete (`0x01`) and LE Enhanced
   Connection Complete (`0x0A`), replace the RPA with the identity address and
   set the peer address type to `0x02`/`0x03`. For the enhanced event, also
   fill in Peer Resolvable Private Address.

## Code layout

- `src/core/` — freestanding C, no kernel or libc dependencies: AES-128, `ah()`,
  the resolving list, and `bt_hci_on_command()` / `bt_hci_on_event()`, which
  edit HCI packets in place and say whether a command is answered locally.
- `tests/` — a userspace test against the spec's `ah()` sample vector and
  scripted HCI exchanges (features rewrite, resolving-list emulation, report
  rewrite, accept-list addition, connection-complete translation).

## Kernel side

Getting the packets requires a kernel hook on the USB path bluetoothd uses.
`src/plugin/` hooks `IOUSBHostPipe::controlRequest` (commands) and
`IOUSBHostPipe::io` (events) through Lilu. The hooks are in and commands are
confirmed to travel that way; the event side is not yet confirmed. See
`docs/STATE.md` §7 for what the hardware has shown so far, and §5a for why
neither direction needs a packet injected.

The plugin stays observe-only (log, never edit) unless `-broadtekrw` is passed,
and nothing is loaded without the user's go-ahead.

## Test cases (on hardware, once a kernel side exists)

1. **Instant Hotspot**: pick the iPhone in the Wi-Fi menu. Pass: the LE link
   comes up (no `BLE client connect timeout` in sharingd) and the hotspot turns on.
2. **Apple Watch approval**: trigger an "approve with Apple Watch" prompt
   (e.g. `sudo` with watch approval, or unlocking System Settings). Pass: the
   prompt **arrives on the watch** (LE link up). A full Auto Unlock also needs
   Wi-Fi ranging over AWDL, which AirPort_RTW89 does not have yet, so it is not
   a pass criterion.
3. Regression: existing LE accessories (keyboard/mouse/AirPods) still connect.

Log check: `log show --last 10m --predicate 'process == "bluetoothd"' | grep -Ei 'resolving|IRK|connect'`.
