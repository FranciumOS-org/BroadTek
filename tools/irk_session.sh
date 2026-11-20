#!/bin/sh
# Run the IRK helper inside the logged-in user's session.
#
# The bond is in the user's keychain. A LaunchDaemon starts outside any login
# session, and there the keychain query fails with errSecParam (-50) however
# long it retries. launchctl asuser keeps root, which the kext requires, but
# moves the helper into the user's session, where the same query succeeds.
# So: wait for someone to log in at the console, then hand over.
bin=/usr/local/libexec/broadtek-irk-install

i=0
while :; do
    uid=$(stat -f %u /dev/console 2>/dev/null || echo 0)
    [ "$uid" != 0 ] && break
    i=$((i + 1))
    if [ "$i" -ge 120 ]; then
        echo "no one logged in after ten minutes; nothing installed"
        exit 1
    fi
    sleep 5
done

exec /bin/launchctl asuser "$uid" "$bin" "$@"
