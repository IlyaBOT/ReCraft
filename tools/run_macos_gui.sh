#!/bin/sh
# Vesper CI: execute window tests in the runner user's Aqua bootstrap session.
# launchctl changes the session, not the account or its environment.
set -eu
runner_uid=$(id -u)
if ! /bin/launchctl print "gui/$runner_uid" >/dev/null 2>&1; then
    printf '%s\n' "ReCraft CI: no GUI session for runner account $(id -un) (uid $runner_uid)." >&2
    printf '%s\n' "Log into the macOS desktop with this account and run the runner as its LaunchAgent." >&2
    /usr/bin/stat -f 'Current console account: %Su' /dev/console >&2
    exit 1
fi
printf '%s\n' "ReCraft CI: running window checks in gui/$runner_uid." >&2
exec /bin/launchctl asuser "$runner_uid" /usr/bin/env "$@"
