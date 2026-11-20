#!/bin/bash
# adv_patch_lldb.sh - patch the advertising gate in bluetoothd and check that it took.
#
# Restarts bluetoothd, catches the new process at exec, and patches three
# capability functions in its memory so it believes the controller has extended
# advertising. The chipset stays 0, so the USB transport is unchanged. Nothing
# on disk changes: the next restart of bluetoothd, or a reboot, undoes it.
#
# It refuses to restart the daemon while a Bluetooth device is connected: the
# controller keeps the link across the restart, and the new daemon then gets
# ACL data for a handle it never opened ("connection handle 0x0 is invalid"),
# which is a fatal error (reason 106) and leaves the relaunched daemon unable
# to start advertising (LeBroadcaster status 0x98). Disconnect first, or set
# BROADTEK_ALLOW_LINKS=1 to go ahead anyway.
#
# WARNING: any restart of bluetoothd can leave the controller missing until a
# reboot (see adv_watch.sh), and nothing here can repair that. After a restart
# it checks the controller and says so. Prefer the Lilu patch in the kext.
#
# Exit status: 0 the stack started with the patch and the daemon stayed up,
# 1 it did not (the daemon crashed, or stackDidStart was not OI_OK), 2 refused
# because a device is connected, 3 the patch itself did not apply.
#
#   sudo tools/adv_patch_lldb.sh
set -u

[ "$(id -u)" = 0 ] || { echo "run with sudo: sudo tools/adv_patch_lldb.sh" >&2; exit 1; }

here="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=/dev/null
. "$here/adv_common.sh"

if [ "${BROADTEK_ALLOW_LINKS:-0}" != 1 ]; then
    n="$(connected_count)"
    if [ "$n" != 0 ]; then
        echo "not restarting bluetoothd: $n device(s) connected (? means it could not be read)." >&2
        echo "Disconnect them (AirPods into the case), or BROADTEK_ALLOW_LINKS=1 to force." >&2
        exit 2
    fi
fi

start="$(date '+%Y-%m-%d %H:%M:%S')"
out="$(mktemp -t advpatch)"
trap 'rm -f "$out"' EXIT

/usr/bin/lldb -b \
    -o 'process attach --name bluetoothd --waitfor' \
    -o "command script import $here/adv_patch_lldb.py" \
    -o 'command script add -f adv_patch_lldb.patch adv_patch' \
    -o 'adv_patch' \
    -o 'detach' >"$out" 2>&1 &
lldb_pid=$!

# lldb must be waiting before the old daemon goes, or it attaches to that one.
sleep 3
pkill -x bluetoothd

( sleep 60; kill "$lldb_pid" 2>/dev/null ) &
watchdog=$!
wait "$lldb_pid"
kill "$watchdog" 2>/dev/null

grep -E 'patched|MISMATCH|FAILED|bluetoothd pid' "$out"
if [ "$(grep -c ': patched$' "$out")" != 3 ]; then
    echo "the patch did not apply to all three sites"
    exit 3
fi

echo "waiting 15 s for the stack to start..."
sleep 15
pid1="$(/usr/bin/pgrep -x bluetoothd)"
/usr/bin/log show --start "$start" --info --predicate 'process == "bluetoothd"' 2>/dev/null |
    grep -E 'stackDidStart|Stack is not|LE_StartAdvertising|SetExtendedAdvertising|Failed to start advertising|Invalid chipset|fatal error|invalid, ignoring ACL' |
    cut -c1-200 | tail -40 >"$out"
cat "$out"

# The daemon may crash a few seconds after it came up (that is how this
# failed before), so a good status is not enough on its own: it also has to
# still be the same process afterwards.
sleep 10
pid2="$(/usr/bin/pgrep -x bluetoothd)"
ok_line="$(grep 'LeBroadcaster.mm::stackDidStart() -- status' "$out" | tail -1)"
case "$ok_line" in
    *"status = OI_OK"*) ;;
    *) echo "FAIL: LeBroadcaster stackDidStart was not OI_OK (${ok_line:-no line})"; exit 1 ;;
esac
if [ -z "$pid1" ] || [ "$pid1" != "$pid2" ]; then
    echo "FAIL: bluetoothd restarted by itself (pid ${pid1:-none} -> ${pid2:-none})"
    exit 1
fi
if ! controller_present; then
    echo "FAIL: the Bluetooth controller is gone (Address NULL); only a reboot brings it back"
    exit 1
fi
echo "OK: stack started with the patch, bluetoothd pid $pid2"
exit 0
