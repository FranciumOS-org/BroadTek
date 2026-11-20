#!/bin/sh
# BroadTek: install the Apple Watch key helper into macOS.
#
# Run this AFTER BroadTek.kext is in your EFI and you have restarted with it:
#
#     sudo ./install.sh
#
# What it adds: a small LaunchDaemon that, after each login, reads your Apple
# Watch's Bluetooth key (its IRK) from your own keychain, where iCloud Keychain
# has already put it, and hands it to BroadTek in kernel memory. Without it the
# Mac cannot recognise the Watch's private address, and Watch unlock never
# starts. The key is never printed, logged or written anywhere; the log at
# /var/log/broadtek-irk.log holds addresses and statuses only.
#
# AirDrop and Handoff do not need this. To remove it: sudo ./uninstall.sh
set -eu

[ "$(id -u)" = 0 ] || { echo "run with sudo: sudo $0" >&2; exit 1; }
here="$(cd "$(dirname "$0")" && pwd)"

label=org.franciumos.broadtek.irk
plist=/Library/LaunchDaemons/$label.plist
bin=/usr/local/libexec/broadtek-irk-install
wrap=/usr/local/libexec/broadtek-irk-session

for f in broadtek-irk-install broadtek-irk-session keychain.entitlements "$label.plist"; do
    [ -f "$here/Helper/$f" ] || { echo "missing Helper/$f: run this from the unzipped release folder" >&2; exit 1; }
done

# 1. The kext has to be running: the helper talks to it.
if ! ioreg -r -c BroadTekConfig | grep -q BroadTekConfig; then
    echo "BroadTek is not running." >&2
    echo "Add BroadTek.kext to your EFI (OpenCore config.plist, Kernel > Add), restart," >&2
    echo "then run this again." >&2
    exit 1
fi

# 2. Watch unlock needs the full stage (boot-arg -broadtekrw).
if ! ioreg -l | grep -q '"broadtek-rewriting" = "full"'; then
    echo "note: BroadTek is not at its full stage. Watch unlock needs boot-arg"
    echo "      -broadtekrw; add it to boot-args in your config.plist and restart."
fi
args="$(nvram boot-args 2>/dev/null | cut -f2- || true)"

# 3. The keychain hands Bluetooth keys only to Apple-signed programs unless
#    AMFI is off. With AMFI on, the helper installs but can never read the key.
case " $args " in
    *amfi_get_out_of_my_way=0x1*|*amfi_get_out_of_my_way=1*|*amfi=0x80*) ;;
    *) echo "note: AMFI looks enabled (no amfi_get_out_of_my_way=0x1 in boot-args)."
       echo "      The helper will be installed, but macOS will refuse it the Watch's"
       echo "      key, so Watch unlock will not work. AirDrop and Handoff are unaffected." ;;
esac

mkdir -p /usr/local/libexec
install -m 755 "$here/Helper/broadtek-irk-install" "$bin"
install -m 755 "$here/Helper/broadtek-irk-session" "$wrap"
# The keychain entitlement travels with the signature, so sign it here.
codesign -s - --entitlements "$here/Helper/keychain.entitlements" -f "$bin" 2>/dev/null
install -m 644 "$here/Helper/$label.plist" "$plist"
chown root:wheel "$plist" "$bin" "$wrap"
xattr -c "$bin" "$wrap" "$plist" 2>/dev/null || true

launchctl bootout system "$plist" 2>/dev/null || true
launchctl bootstrap system "$plist"

echo "installed $label"
printf "waiting for the first key install"
i=0
while [ $i -lt 15 ]; do
    if ioreg -l | grep -q '"broadtek-resolving-list" = [1-9]'; then
        echo
        echo "done: BroadTek has your Watch's key. It is put back after every login."
        echo "Turn on System Settings > Touch ID & Password > Use Apple Watch to unlock."
        exit 0
    fi
    printf .
    sleep 1
    i=$((i + 1))
done
echo
echo "no key yet. See /var/log/broadtek-irk.log:"
tail -5 /var/log/broadtek-irk.log 2>/dev/null || true
echo "\"0 bond(s)\" means no Watch is in this Mac's keychain: check that iCloud Keychain"
echo "is on and the Watch uses the same Apple ID. \"entitlement refused\" means AMFI is on."
