# BroadTek: development notes

Broadcom behaviour on Realtek hardware: a Lilu plugin that gives a Bluetooth
controller without LL Privacy the one thing macOS needs from it — resolving
the private addresses Apple devices advertise with.

Written for an RTL8852BE combo card (USB `13D3:3571`) on macOS Sequoia, but
nothing in it is Realtek-specific: it works on the HCI stream, so any
controller whose LE features lack LL Privacy should behave the same.

## The problem

Apple devices advertise from a resolvable private address (RPA) that changes
every 15 minutes. To connect to one, a controller normally stores the device's
identity resolving key (IRK) and resolves the address itself. A controller
without LL Privacy cannot, so:

- Instant Hotspot fails — sharingd reports `BLE client connect timeout`, and
  the iPhone never appears to answer.
- An Apple Watch approval prompt ("Check your watch") never reaches the watch.

macOS puts the device's *identity* address on the controller's Filter Accept
List and waits. The phone is advertising from an RPA, nothing ever matches, and
the connection is cancelled after about 10 seconds.

## What BroadTek does

It sits on the HCI stream between bluetoothd and the controller and emulates
the missing feature in software:

1. Reports LL Privacy as supported, so macOS hands over the IRKs.
2. Keeps the resolving list itself and answers its HCI commands.
3. Resolves RPAs in advertising reports with AES-128 `ah()`.
4. Puts the *current* RPA on the controller's accept list whenever macOS asks
   for an identity address it has a key for.
5. Rewrites the peer address in connection-complete events back to the
   identity address, so macOS sees the device it asked for.

The design, including why the obvious route (intercepting Apple's Broadcom
vendor command for IRKs) does not work, is in [docs/DESIGN.md](docs/DESIGN.md).

## Picking this up

[docs/STATE.md](docs/STATE.md) is the handover: the machine, what has been
established (including one route that looks open in the logs but is not),
what is built, what is open, and how to read the next result.

## Status

**The LE link works.** On the development machine, with a key read out of the
keychain, macOS now establishes an encrypted GATT connection to an Apple
Watch that advertises from a resolvable private address -- which it could not
do before:

```
Outgoing connection to device "My Watch" ... was successful
Gatt device connected on transport 2 ... with status 0
Encryption is now enabled for device "My Watch"
```

Every step of the design is confirmed against real traffic: the IRK resolves
advertisements, the peer's current private address is substituted into the
Filter Accept List command macOS sends, that substitution reaches the
controller, and the connection-complete event is translated back so macOS
sees the identity address it asked for.

Two things are worth knowing before expecting too much:

- **macOS never hands over the IRK.** It gates private-address resolution on
  its own chipset classification rather than on the controller's HCI
  features, so faking LL Privacy changes nothing. The key is read from the
  system keychain instead and handed to the kext by a helper. See
  [docs/STATE.md](docs/STATE.md) §3.
- **The Apple Watch approval prompt still does not arrive -- but the reason is
  no longer anything BroadTek can affect.** With a key installed, the Watch was
  enrolled as an unlock key over BroadTek's LE link, which flipped the
  `canUnlockDevice` eligibility gate to YES; the unlock session then connects
  and encrypts to the Watch's identity address (bluetoothd's own log), proving
  the link end to end. The prompt is held above Bluetooth, on **AWDL ranging**
  (`withinRange:false`, `awdl0` inactive): the approval flow runs over the
  working link and stalls waiting for proximity ranging, which the Wi-Fi driver
  on this machine does not implement. That is outside this project. See
  [docs/STATE.md](docs/STATE.md) §8.

## Keeping it working

The key lives only in kernel memory, so it has to be installed after each
boot. A LaunchDaemon does that:

```bash
make kext irk-install
sudo efi/install.sh --stage full        # then restart
sudo tools/install_daemon.sh
```

## Build

```bash
make test     # the core in userspace, ASan/UBSan
make kext     # build/out/BroadTek.kext (runs the test first)
```

Nothing here loads a kext.

## Trying it

Needs Lilu in the EFI, and OpenCore with unsigned kexts allowed
(`csr-active-config` already permits this on the development machine).

```bash
make kext
sudo efi/install.sh
```

`install.sh` copies the kext next to Lilu, adds its `config.plist` entry after
Lilu, and adds the boot-arg `-broadtekdbg`. It keeps the config as it was in
`EFI/OC/config.plist.pre-broadtek`, and changes nothing if the EFI volume is
short of room. If OpenCore is on more than one volume it lists them and stops;
pass the one this machine boots from:

```bash
sudo efi/install.sh <partition-UUID>
```

Then restart, and read what the hooks saw:

```bash
ioreg -l | grep -i broadtek
```

That works at any time and needs no sudo. The same facts are logged as well,
but the kernel message buffer is small and wraps within a few minutes of boot,
so `sudo dmesg | grep -i broadtek` only helps if run straight after login.
`log show` is no use at all here: it carries no kernel lines on this machine.

What to look for is `broadtek-opcodes-uc`, the distinct HCI command opcodes
seen on the user-client path, as little-endian byte pairs. `0c03` (HCI Reset)
or `2003` (LE Read Local Supported Features) there means this is bluetoothd's
stream and the hook is in the right place. The counters alone are not enough:
the first hardware run had 253 commands on the other hook and every one of
them was the in-kernel firmware loader, not bluetoothd. Settling which path
carries which traffic is the whole point of this phase.

Only once commands *and* events both show up is it worth trying
`-broadtekrw`.

To back out: `sudo efi/uninstall.sh` and restart. That switches the entry off
and takes the boot-args out again. `-broadtekoff` also disables the plugin, and
it does nothing at all without `-broadtekdbg` or `-broadtekrw`.

## Testing against hardware

Once rewriting is on:

1. **Instant Hotspot** — pick the iPhone under "Personal Hotspot" in the Wi-Fi
   menu. Pass: it connects, and sharingd logs no `BLE client connect timeout`.
2. **Apple Watch approval** — trigger a watch approval (unlocking a System
   Settings pane, or `sudo` if watch approval is on). Pass: **the prompt
   arrives on the watch.** A full Auto Unlock also needs Wi-Fi ranging over
   AWDL, which the Wi-Fi driver on this machine does not do yet, so a prompt
   that arrives but does not complete the unlock is still a pass for BroadTek.
3. **Regression** — AirPods, Bluetooth keyboard and mouse still pair and
   reconnect.

```bash
sudo dmesg | grep -i broadtek
log show --last 10m --predicate 'process == "bluetoothd"' | grep -Ei 'resolv|irk|connect'
```

## Licence

MIT. Builds against [Lilu](https://github.com/acidanthera/Lilu) and
[MacKernelSDK](https://github.com/acidanthera/MacKernelSDK), both in
`third_party/`.
