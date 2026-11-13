#!/bin/bash
# Notarization that survives notarytool: sourced by sign-notarize.sh.
#
#   . platforms/macos/notarize-lib.sh
#   nz_check_profile                  the keychain profile works, or says what to do
#   nz_notarize <file> <label>        submit (no --wait), poll until Accepted;
#                                     the submission id is kept in
#                                     $SCRATCH/<label>.submission
#   platforms/macos/notarize-lib.sh --self-test   on a fake notarytool
#
# Why not `notarytool submit --wait`: on 2026-11-13 it died with a Bus error
# twice while waiting (the submission went on at Apple; the script, under
# set -e, stopped), and both RCs were finished by hand-written poll loops.
# Here the submit is retried on its own (a crash before an id is printed is
# a new submission; Apple dedupes nothing, but a lost submission costs
# nothing either), and the wait is our loop of `notarytool info`, which
# rides out a crash or a network blip: NZ_MAX_ERRORS errors in a row
# before it gives up, and then it prints the command to finish by hand.
#
# A submission already made (the script was stopped while it waited):
# NZ_SUBMISSION_<LABEL>=<id> (label upper-cased, e.g. NZ_SUBMISSION_APP)
# waits on that one instead of submitting again.
#
# Env: NOTARY_PROFILE (qlc-notary), NOTARYTOOL (default "xcrun notarytool";
# the self-test puts a fake here), NZ_POLL (30 s), NZ_TIMEOUT (5400 s),
# NZ_MAX_ERRORS (10), NZ_SUBMIT_TRIES (3), SCRATCH.

NOTARY_PROFILE="${NOTARY_PROFILE:-qlc-notary}"
NOTARYTOOL="${NOTARYTOOL:-xcrun notarytool}"
NZ_POLL="${NZ_POLL:-30}"
NZ_TIMEOUT="${NZ_TIMEOUT:-5400}"
NZ_MAX_ERRORS="${NZ_MAX_ERRORS:-10}"
NZ_SUBMIT_TRIES="${NZ_SUBMIT_TRIES:-3}"

# The profile has gone from the keychain three times (twice it came back by
# itself once ender was docked again): say so in words, before an upload.
nz_check_profile() {
    local out
    if out=$($NOTARYTOOL history --keychain-profile "$NOTARY_PROFILE" 2>&1); then
        return 0
    fi
    echo "ERROR: the notarytool keychain profile '$NOTARY_PROFILE' does not work:"
    printf '%s\n' "$out" | tail -n 3 | sed 's/^/  /'
    echo "  It has vanished before and come back once the Mac was docked and its"
    echo "  login keychain unlocked: check that first. To store it again (needs the"
    echo "  Apple ID's app-specific password):"
    echo "    xcrun notarytool store-credentials $NOTARY_PROFILE --apple-id <Apple ID> --team-id B4N7HJ7VZ6"
    echo "  A test build needs no notarization at all: make rc HOSTS=... (docs/dev/RELEASE.md)."
    return 1
}

# The id in notarytool's output ("  id: 6e018b17-...").
nz_id_of() { awk '$1 == "id:" { print $2; exit }'; }

nz_notarize() { # <file> <label>
    local file=$1 label=$2 var sid="" out try=0 st errs=0 waited=0 rec
    rec="${SCRATCH:-${TMPDIR:-/tmp}}/$label.submission"
    var="NZ_SUBMISSION_$(printf '%s' "$label" | tr '[:lower:]-' '[:upper:]_')"
    sid="${!var:-}"
    if [ -n "$sid" ]; then
        echo "  waiting on the submission already made: $sid ($var)"
    else
        while [ "$try" -lt "$NZ_SUBMIT_TRIES" ]; do
            try=$((try + 1))
            out=$($NOTARYTOOL submit "$file" --keychain-profile "$NOTARY_PROFILE" 2>&1) || true
            printf '%s\n' "$out" | sed 's/^/  /'
            sid=$(printf '%s\n' "$out" | nz_id_of)
            [ -n "$sid" ] && break
            echo "  submit $try of $NZ_SUBMIT_TRIES gave no id"
            nz_check_profile >/dev/null || { nz_check_profile; return 1; }
            sleep "$NZ_POLL"
        done
        [ -n "$sid" ] || { echo "ERROR: could not submit $file for notarization."; return 1; }
    fi
    echo "$sid" > "$rec"
    echo "  submission $sid (kept in $rec; if this stops, rerun with $var=$sid)"
    while :; do
        if out=$($NOTARYTOOL info "$sid" --keychain-profile "$NOTARY_PROFILE" 2>&1); then
            errs=0
            st=$(printf '%s\n' "$out" | awk -F': *' '$1 ~ /^ *status$/ { print $2; exit }')
            case "$st" in
                Accepted) echo "  $label: Accepted"; return 0 ;;
                "In Progress"|"") ;;
                *) echo "ERROR: $label notarization: $st. Apple's log:"
                   $NOTARYTOOL log "$sid" --keychain-profile "$NOTARY_PROFILE" 2>&1 | tail -n 40
                   return 1 ;;
            esac
        else
            errs=$((errs + 1))
            echo "  notarytool info failed ($errs in a row): $(printf '%s\n' "$out" | tail -n 1)"
            if [ "$errs" -ge "$NZ_MAX_ERRORS" ]; then
                echo "ERROR: lost track of $sid. It may still be accepted: finish with"
                echo "  $var=$sid $0"
                return 1
            fi
        fi
        [ "$waited" -ge "$NZ_TIMEOUT" ] && { echo "ERROR: $sid not done after ${waited}s; rerun with $var=$sid"; return 1; }
        [ $((waited % 300)) -lt "$NZ_POLL" ] && echo "  $(date +%H:%M:%S) $label: ${st:-?} (${waited}s)"
        sleep "$NZ_POLL"
        waited=$((waited + NZ_POLL))
    done
}

# --self-test: a fake notarytool that crashes on the first submit, fails one
# info call, reports In Progress, then Accepted; and one that rejects.
nz_selftest() {
    local tmp fails=0 out
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/notarize-test.XXXXXX") || return 1
    ok() { echo "ok - $1"; }
    no() { echo "not ok - $1"; fails=$((fails + 1)); }
    cat > "$tmp/fake" <<'FAKE'
#!/bin/bash
# state in $FAKE_DIR: counters per verb
d="$FAKE_DIR"; v=$1
n=$(( $(cat "$d/$v" 2>/dev/null || echo 0) + 1 )); echo $n > "$d/$v"
case "$v" in
  history) [ -e "$d/noprofile" ] && { echo 'Error: No Keychain password item found for profile: qlc-notary'; exit 1; }; echo ok ;;
  submit) [ "$n" = 1 ] && [ -e "$d/crash" ] && { echo 'Bus error: 10'; exit 138; }
          echo "Conducting pre-submission checks..."; echo "  id: 11111111-2222-3333-4444-555555555555"; echo "  name: x" ;;
  info) [ "$n" = 1 ] && { echo 'Bus error: 10'; exit 138; }
        if [ -e "$d/reject" ]; then echo "  status: Invalid"
        elif [ "$n" -lt 3 ]; then echo "  status: In Progress"; else echo "  status: Accepted"; fi ;;
  log) echo "the log" ;;
esac
FAKE
    chmod +x "$tmp/fake"
    export FAKE_DIR="$tmp/s1"; mkdir -p "$FAKE_DIR"; touch "$FAKE_DIR/crash"
    out=$(NOTARYTOOL="$tmp/fake" NZ_POLL=0 SCRATCH="$tmp" nz_notarize /dev/null app 2>&1)
    [ $? -eq 0 ] && ok "a crashed submit and a crashed info are ridden out" || no "a crashed submit and a crashed info are ridden out: $out"
    [ "$(cat "$FAKE_DIR/submit")" = 2 ] && ok "submitted again after the crash" || no "submitted again after the crash"
    [ "$(cat "$tmp/app.submission" 2>/dev/null)" = 11111111-2222-3333-4444-555555555555 ] && ok "the id is kept" || no "the id is kept"
    export FAKE_DIR="$tmp/s2"; mkdir -p "$FAKE_DIR"
    out=$(NOTARYTOOL="$tmp/fake" NZ_POLL=0 SCRATCH="$tmp" NZ_SUBMISSION_DMG=abc nz_notarize /dev/null dmg 2>&1)
    [ $? -eq 0 ] && [ ! -e "$FAKE_DIR/submit" ] && ok "NZ_SUBMISSION_DMG waits without submitting" || no "NZ_SUBMISSION_DMG waits without submitting: $out"
    export FAKE_DIR="$tmp/s3"; mkdir -p "$FAKE_DIR"; touch "$FAKE_DIR/reject"
    out=$(NOTARYTOOL="$tmp/fake" NZ_POLL=0 SCRATCH="$tmp" nz_notarize /dev/null app 2>&1)
    [ $? -ne 0 ] && printf '%s' "$out" | grep -q "the log" && ok "a rejection fails with Apple's log" || no "a rejection fails with Apple's log: $out"
    export FAKE_DIR="$tmp/s4"; mkdir -p "$FAKE_DIR"; touch "$FAKE_DIR/noprofile"
    out=$(NOTARYTOOL="$tmp/fake" nz_check_profile 2>&1)
    [ $? -ne 0 ] && printf '%s' "$out" | grep -q "store-credentials" && ok "a missing profile says how to restore it" || no "a missing profile says how to restore it"
    export FAKE_DIR="$tmp/s5"; mkdir -p "$FAKE_DIR"
    out=$(NOTARYTOOL="$tmp/fake" NZ_POLL=0 NZ_MAX_ERRORS=1 SCRATCH="$tmp" nz_notarize /dev/null app 2>&1)
    [ $? -ne 0 ] && printf '%s' "$out" | grep -q "NZ_SUBMISSION_APP=1111" && ok "giving up names the id to finish with" || no "giving up names the id to finish with: $out"
    rm -rf "$tmp"
    if [ "$fails" -eq 0 ]; then echo "notarize-lib.sh self-test: ok"; return 0; fi
    echo "notarize-lib.sh self-test: $fails failed"; return 1
}

if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    case "${1:-}" in
        --self-test) nz_selftest; exit $? ;;
        *) sed -n '2,27p' "$0"; exit 0 ;;
    esac
fi
