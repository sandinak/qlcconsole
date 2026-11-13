#!/bin/bash
# Install a signed test build as /Applications/qlcconsole.app on our own Macs,
# over ssh, without notarization (`make rc`, `make install-test`).
#
#   platforms/macos/install-test-app.sh [--app PATH] HOST...
#   platforms/macos/install-test-app.sh --self-test
#
# HOST: a name in gate-hosts.conf (dink, newaudio, ...: its ssh destination
# is used), any ssh destination (branson@kronk.local), or "local" for this
# Mac. The app (default ~/qlcconsole.app, as package-local.sh +
# `NOTARIZE=0 sign-notarize.sh` leave it) is zipped with ditto, sent over
# ssh, unpacked there, checked with codesign, and moved into place; the app
# it replaces is kept as /Applications/qlcconsole.app.prev (one generation;
# Finder and LaunchServices don't take that for an app). It refuses a host
# where qlcconsole is running.
#
# Why no notarization: a file copied over ssh carries no quarantine flag, so
# Gatekeeper never assesses it, and a Developer ID signature is all the app
# needs to run (and to keep its Local Network and microphone permissions).
# Notarization is for the DMG we publish (make dmg / make beta). On
# 2026-11-13 a test RC waited an hour on notarytool; this is the path that
# doesn't.
#
# Env: APPS_DIR (default /Applications), VERIFY=0 skips codesign (the
# self-test's fake app), GATE_HOSTS (the conf to read names from).
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP="$HOME/qlcconsole.app"
APPS_DIR="${APPS_DIR:-/Applications}"
VERIFY="${VERIFY:-1}"
CONF="${GATE_HOSTS:-$HERE/gate-hosts.conf}"
SSHOPT=(-o BatchMode=yes -o ConnectTimeout=10)

# The script each host runs (stdin), after the zip is at $1.
remote_install() {
    cat <<EOF
set -u
Z="\$1"; A="$APPS_DIR"; N=qlcconsole.app
if pgrep -f "^\$A/\$N/Contents/MacOS/" >/dev/null 2>&1; then
    echo "  qlcconsole is running there: quit it first"; rm -f "\$Z"; exit 3
fi
T=\$(mktemp -d "\${TMPDIR:-/tmp}/qlc-install.XXXXXX") || exit 1
ditto -x -k "\$Z" "\$T" || { echo "  could not unpack"; rm -rf "\$T" "\$Z"; exit 1; }
rm -f "\$Z"
[ -d "\$T/\$N" ] || { echo "  the zip holds no \$N"; rm -rf "\$T"; exit 1; }
xattr -dr com.apple.quarantine "\$T/\$N" 2>/dev/null
if [ "$VERIFY" = 1 ] && ! codesign --verify --deep --strict "\$T/\$N" 2>&1; then
    echo "  codesign does not verify it: not installed"; rm -rf "\$T"; exit 1
fi
mkdir -p "\$A" || exit 1
rm -rf "\$A/\$N.prev"
[ -d "\$A/\$N" ] && { mv "\$A/\$N" "\$A/\$N.prev" || { rm -rf "\$T"; exit 1; }; }
mv "\$T/\$N" "\$A/\$N" || { echo "  could not move it into \$A"; [ -d "\$A/\$N.prev" ] && mv "\$A/\$N.prev" "\$A/\$N"; rm -rf "\$T"; exit 1; }
rm -rf "\$T"
P="\$A/\$N/Contents/Info.plist"
v=\$(/usr/libexec/PlistBuddy -c "Print CFBundleShortVersionString" "\$P" 2>/dev/null)
b=\$(/usr/libexec/PlistBuddy -c "Print CFBundleVersion" "\$P" 2>/dev/null)
echo "  installed \$A/\$N: \${v:-?} (\${b:-?}) on \$(hostname -s)"
EOF
}

# A conf name -> its ssh destination; anything else as given.
dest_of() {
    local name dest
    [ -f "$CONF" ] && while read -r name dest _; do
        case "$name" in ''|'#'*) continue ;; esac
        [ "$name" = "$1" ] && { echo "$dest"; return; }
    done < "$CONF"
    echo "$1"
}

install_on() { # <host> <zip>
    local d; d=$(dest_of "$1")
    echo "== $1${d:+ ($d)}"
    if [ "$d" = local ]; then
        local z; z=$(mktemp "${TMPDIR:-/tmp}/qlc-app.XXXXXX") && cp "$2" "$z" \
            && remote_install | bash -s "$z"
    else
        ssh "${SSHOPT[@]}" "$d" 'z=$(mktemp "${TMPDIR:-/tmp}/qlc-app.XXXXXX") && cat > "$z" && echo "$z"' < "$2" > "$TMPZ.where" \
            || { echo "  could not copy to $d"; return 1; }
        remote_install | ssh "${SSHOPT[@]}" "$d" bash -s "$(cat "$TMPZ.where")"
    fi
}

selftest() {
    local tmp fails=0 out
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/install-test.XXXXXX") || return 1
    ok() { echo "ok - $1"; }
    no() { echo "not ok - $1"; fails=$((fails + 1)); }
    mkdir -p "$tmp/src/qlcconsole.app/Contents/MacOS"
    /usr/libexec/PlistBuddy -c "Add CFBundleShortVersionString string 9.9.9" -c "Add CFBundleVersion string 4242" \
        "$tmp/src/qlcconsole.app/Contents/Info.plist" >/dev/null
    mkdir -p "$tmp/Apps/qlcconsole.app/Contents"; echo old > "$tmp/Apps/qlcconsole.app/Contents/old"
    out=$(APPS_DIR="$tmp/Apps" VERIFY=0 "$0" --app "$tmp/src/qlcconsole.app" local 2>&1)
    printf '%s' "$out" | grep -q "9.9.9 (4242)" && ok "installs and says the version" || no "installs and says the version: $out"
    [ -f "$tmp/Apps/qlcconsole.app.prev/Contents/old" ] && ok "keeps the old one as .prev" || no "keeps the old one as .prev"
    [ -f "$tmp/Apps/qlcconsole.app/Contents/Info.plist" ] && ok "the new one is in place" || no "the new one is in place"
    out=$(APPS_DIR="$tmp/Apps" "$0" --app "$tmp/src/qlcconsole.app" local 2>&1)
    [ $? -ne 0 ] && [ -f "$tmp/Apps/qlcconsole.app/Contents/Info.plist" ] && ok "an unsigned app is refused and nothing moves" \
        || no "an unsigned app is refused and nothing moves: $out"
    out=$("$0" --app "$tmp/nosuch.app" local 2>&1)
    [ $? -ne 0 ] && ok "no app: refused" || no "no app: refused"
    rm -rf "$tmp"
    if [ "$fails" -eq 0 ]; then echo "install-test-app.sh self-test: ok"; return 0; fi
    echo "install-test-app.sh self-test: $fails failed"; return 1
}

HOSTS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --app) shift; APP="${1:?--app needs a path}" ;;
        --self-test) selftest; exit $? ;;
        -h|--help) sed -n '2,27p' "$0"; exit 0 ;;
        -*) echo >&2 "install-test-app.sh: unknown option '$1'"; exit 2 ;;
        *) HOSTS+=("$1") ;;
    esac
    shift
done
[ ${#HOSTS[@]} -gt 0 ] || { echo >&2 "usage: install-test-app.sh [--app PATH] HOST... (a gate-hosts.conf name, an ssh destination, or local)"; exit 2; }
[ -d "$APP/Contents" ] || { echo >&2 "install-test-app.sh: no app at $APP (make rc builds and signs it)"; exit 1; }

TMPZ=$(mktemp -d "${TMPDIR:-/tmp}/qlc-rc.XXXXXX")/app
trap 'rm -rf "$(dirname "$TMPZ")"' EXIT
/usr/bin/ditto -c -k --keepParent "$APP" "$TMPZ.zip" || { echo >&2 "could not zip $APP"; exit 1; }
FAILED=""
for h in "${HOSTS[@]}"; do
    install_on "$h" "$TMPZ.zip" || FAILED="$FAILED $h"
done
if [ -n "$FAILED" ]; then echo "NOT installed on:$FAILED"; exit 1; fi
echo "Installed on: ${HOSTS[*]} (signed, not notarized: for testing on our own Macs)"
