# For the AWDL session

Short version of what BroadTek did, and the one thing it now needs from the
Wi-Fi side. Full detail is in [STATE.md](STATE.md) §8 (2026-10-17).

## What works now

BroadTek (the Bluetooth kext) gives the Realtek RTL8852BE controller the one
thing it lacked: it resolves the resolvable private addresses Apple devices
advertise with, and opens LE connections to them. Proven on hardware:

- The Apple Watch was **enrolled as an unlock key** over BroadTek's LE link
  (`sharingd` logged `success: true`, the LTK is stored).
- `canUnlockDevice` is now **YES** — the macOS eligibility gate is cleared.
- The unlock session **connects and encrypts** to the Watch (bluetoothd:
  `Outgoing LE Connection complete ... Public 8C:00:00:00:00:02 status 0`,
  `Gatt device connected`, `Encryption is now enabled`).

So the Bluetooth link to the Watch is complete and working end to end.

## Where it stops

The "Check your watch" approval prompt still does not appear. It is blocked
**above Bluetooth, on AWDL ranging** — not on anything BroadTek does:

- `sharingd` reaches `Creating BLE connection for unlock` and
  `Sending SDAutoUnlockAuthPromptRequest` over the working link, then tears it
  down after ~5s.
- The attempt metric: `withinRange: false`, `awdlBringUpTime: 0`,
  `awdlRangingTime: 0`.
- The NearbyInfo records for the attempt carry the `Ranging` device flag
  (`DF 0x30 < PeerMe Ranging >`), and the flow starts a `WirelessProximity`
  scan with `range: 0`.
- `awdl0` reports `status: inactive`.

The approval flow runs over a perfect LE link and stalls waiting for proximity
ranging over AWDL, which AirPort_RTW89 does not implement yet.

## What we need from AWDL

For `sharingd`'s ranging handshake to turn `withinRange` true:

1. `awdl0` needs to come up and be **active**, not `inactive`.
2. It needs to actually perform **ranging** (the Wi-Fi FTM / proximity ranging
   the approval flow invokes) with the Watch/iPhone, so `awdlRangingTime` is
   non-zero and `withinRange` becomes true.

Nothing on the BroadTek side needs to change. Once `awdl0` can range, just
retry the approval (`sudo -k; sudo true`, watch the Watch) — the whole chain
above is already in place.

## How to re-test, once ranging works

```bash
# BroadTek must be live with the key installed (it reinstalls at each boot):
ioreg -l | grep -iE 'broadtek-(build|resolved)'     # resolved should climb

# trigger and watch both sides:
log stream --predicate 'process == "sharingd"' --info --debug   # tab 1
sudo -k; sudo true                                               # tab 2
```

Pass = the prompt arrives on the Watch, and the attempt metric shows
`withinRange: true` with a non-zero `awdlRangingTime`.
