#!/bin/sh
# Install the advertising watcher: sudo tools/install_adv_watch.sh
# Undo: sudo tools/install_adv_watch.sh remove
set -eu
[ "$(id -u)" = 0 ] || { echo "run with sudo" >&2; exit 1; }
label=org.franciumos.broadtek.advwatch
plist=/Library/LaunchDaemons/$label.plist
dir=/usr/local/libexec/broadtek
launchctl bootout system "$plist" 2>/dev/null || true
if [ "${1:-}" = remove ]; then
    rm -f "$plist"; rm -rf "$dir"
    echo "removed; restart Bluetooth or the Mac to drop the patch"
    exit 0
fi
here="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$dir"
cp "$here/adv_watch.sh" "$here/adv_common.sh" "$here/adv_patch_lldb.sh" "$here/adv_patch_lldb.py" "$dir/"
chown -R root:wheel "$dir"; chmod 755 "$dir" "$dir"/*.sh; chmod 644 "$dir"/*.py
cat > "$plist" <<P
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>Label</key><string>$label</string>
<key>ProgramArguments</key><array><string>$dir/adv_watch.sh</string></array>
<key>RunAtLoad</key><true/>
<key>KeepAlive</key><true/>
<key>StandardOutPath</key><string>/var/log/broadtek-advwatch.log</string>
<key>StandardErrorPath</key><string>/var/log/broadtek-advwatch.log</string>
</dict></plist>
P
chown root:wheel "$plist"; chmod 644 "$plist"
launchctl bootstrap system "$plist"
echo "installed; it patches bluetoothd about a minute from now and after each boot"
