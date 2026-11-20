#!/bin/sh
# Install the LaunchDaemon that puts the IRK back after each boot.
#
#   sudo tools/install_daemon.sh          install and start it
#   sudo tools/install_daemon.sh remove   take it out again
#
# The helper goes to /usr/local/libexec so the daemon does not depend on the
# build directory still existing, and is re-signed there because the keychain
# entitlement travels with the signature, not the file.
set -eu

[ "$(id -u)" = 0 ] || { echo "run with sudo: sudo $0" >&2; exit 1; }
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"

label=org.franciumos.broadtek.irk
plist=/Library/LaunchDaemons/$label.plist
bin=/usr/local/libexec/broadtek-irk-install
wrap=/usr/local/libexec/broadtek-irk-session

if [ "${1:-}" = remove ]; then
    launchctl bootout system "$plist" 2>/dev/null || true
    rm -f "$plist" "$bin" "$wrap"
    echo "removed $label"
    exit 0
fi

[ -f "$root/build/out/irk_install" ] || { echo "build it first: make irk-install" >&2; exit 1; }

mkdir -p /usr/local/libexec
install -m 755 "$root/build/out/irk_install" "$bin"
codesign -s - --entitlements "$here/keychain_probe.entitlements" -f "$bin" 2>/dev/null
install -m 755 "$here/irk_session.sh" "$wrap"
install -m 644 "$here/$label.plist" "$plist"
chown root:wheel "$plist" "$bin" "$wrap"

launchctl bootout system "$plist" 2>/dev/null || true
launchctl bootstrap system "$plist"

echo "installed $label"
echo "  helper:  $bin (run through $wrap)"
echo "  plist:   $plist"
echo "  log:     /var/log/broadtek-irk.log  (addresses and statuses only)"
echo
echo "it is running now; give it a few seconds, then:"
echo "  ioreg -l | grep '\"broadtek-keys-installed'"
