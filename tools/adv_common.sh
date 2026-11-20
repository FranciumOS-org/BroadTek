# Shared by adv_watch.sh and adv_patch_lldb.sh. Sourced, not run.

# Number of connected Bluetooth devices, or "?" when it cannot be read.
connected_count() {
    local sp
    sp="$(/usr/sbin/system_profiler SPBluetoothDataType 2>/dev/null)"
    [ -n "$sp" ] || { echo '?'; return; }
    printf '%s\n' "$sp" | /usr/bin/awk '
        /^      Connected:/     { on = 1; next }
        /^      Not Connected:/ { on = 0 }
        on && /^          [^ ].*:$/ { n++ }
        END { print n + 0 }'
}

# 0 when the Bluetooth controller is present (a real address and State On).
# After bluetoothd goes down, Apple's USB transport driver can be gone for the
# rest of the boot: the controller then reads Address NULL, State Off, Chipset
# BCM_4350C2, Transport UART, and only a reboot brings it back.
controller_present() {
    local sp
    sp="$(/usr/sbin/system_profiler SPBluetoothDataType 2>/dev/null)"
    printf '%s\n' "$sp" | grep -q 'Address: NULL' && return 1
    printf '%s\n' "$sp" | grep -q 'State: On'
}
