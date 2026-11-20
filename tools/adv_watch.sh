#!/bin/bash
# adv_watch.sh - report on bluetoothd and the Bluetooth controller. It changes nothing.
#
# It used to restart bluetoothd and patch the new process with lldb. That can
# cost the controller for the rest of the boot: when bluetoothd goes down,
# launchd removes Apple's DriverKit transport (com.apple.IOUserBluetoothSerialDriver,
# "service only configured to run once"), and when it does not come back the
# controller reads Address NULL / State Off / Chipset BCM_4350C2 until a reboot
# (seen 2026-11-16, after 2026-11-11 had survived). The patch is applied by the
# BroadTek kext at exec instead (Lilu); tools/adv_patch_lldb.sh remains as a
# manual experiment.
#
# This log line per change is what to read:
#   bluetoothd pid N, stackDidStart OI_OK|0x98|unknown, controller present|MISSING
set -u
here="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=/dev/null
. "$here/adv_common.sh"

last=
sleep 60
while :; do
    pid="$(/usr/bin/pgrep -x bluetoothd)"
    line="$(/usr/bin/log show --last 10m --info --predicate "process == \"bluetoothd\" && processID == ${pid:-0}" 2>/dev/null |
        grep 'LeBroadcaster.mm::stackDidStart() -- status' | tail -1)"
    case "$line" in
        *"status = OI_OK"*) st=OI_OK ;;
        *"status != OI_OK"*) st="$(echo "$line" | sed -E 's/.*status = (0x[0-9a-fA-F]+).*/\1/')" ;;
        *) st=unknown ;;
    esac
    if controller_present; then ctl=present; else ctl=MISSING; fi
    now="bluetoothd pid ${pid:-none}, stackDidStart $st, controller $ctl"
    if [ "$now" != "$last" ]; then
        echo "$(date): $now"
        [ "$ctl" = MISSING ] && echo "$(date): the controller is gone; only a reboot brings it back"
        last="$now"
    fi
    sleep 30
done
