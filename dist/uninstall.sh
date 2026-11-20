#!/bin/sh
# BroadTek: remove the Apple Watch key helper from macOS.
#
#     sudo ./uninstall.sh
#
# This only removes what install.sh added. BroadTek.kext itself lives in your
# EFI: take it out of config.plist there.
set -eu

[ "$(id -u)" = 0 ] || { echo "run with sudo: sudo $0" >&2; exit 1; }

label=org.franciumos.broadtek.irk
plist=/Library/LaunchDaemons/$label.plist

launchctl bootout system "$plist" 2>/dev/null || true
rm -f "$plist" /usr/local/libexec/broadtek-irk-install /usr/local/libexec/broadtek-irk-session
echo "removed $label (the key already in kernel memory is gone after the next restart)"
