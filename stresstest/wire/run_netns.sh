#!/bin/sh
# Linux: run a command in a network namespace of its own, where loopback is
# the ONLY interface (nothing can leave the machine), as the calling user,
# with HOME set to the sandbox. Capture tools inside call `sudo -n`.
#
#   sh run_netns.sh SANDBOX_HOME cmd args...
# (needs passwordless sudo; the namespace ends with the command)
[ $# -ge 2 ] || { echo "usage: $0 SANDBOX_HOME cmd..." >&2; exit 2; }
H=$1; shift
U=$(id -un); UID_=$(id -u); GID_=$(id -g)
exec sudo -n unshare -n sh -c '
    ip link set lo up || exit 1
    H=$1; U=$2; UI=$3; GI=$4; shift 4
    exec setpriv --reuid="$UI" --regid="$GI" --init-groups \
        env HOME="$H" USER="$U" LOGNAME="$U" PATH="$PATH" "$@"' sh "$H" "$U" "$UID_" "$GID_" "$@"
