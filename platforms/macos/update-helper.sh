#!/bin/bash
# qlcconsole's macOS update helper. Built into the app (ui/src/update.qrc)
# and written out to a temporary file when it is needed; run by
# UpdateInstaller, never by hand. Two jobs:
#
#   prepare  Copy the .app out of a downloaded DMG to beside the running
#            app, and prove it is ours: a valid, deep, strict code
#            signature, accepted by Gatekeeper (spctl), from the same Team
#            ID and with the same bundle identifier as the running app.
#            Prints "STAGED: <path>" and "TEAM: <id>"; on any failure
#            removes what it copied, prints "ERROR: <why>", exits 1.
#
#       update-helper.sh prepare --dmg <file.dmg> --dest-dir <dir>
#                                --running-app <path.app> [--staged-name <name>]
#
#   install  Run detached after the app has been told to quit: wait for its
#            process to end, re-check the staged copy's signature, swap it
#            in (current -> backup, staged -> current, the backup moved back
#            if the second move fails), and reopen it on the same show when
#            asked to. The backup stays until the new version has started
#            once (UpdateManager removes it then).
#
#       update-helper.sh install --pid <pid> --current <path.app>
#                                --new <staged.app> --backup <path.app>
#                                [--team-id <id>] [--relaunch] [--show <file.qxw>]
#                                [--log <file>]
#
# Testing only (ui/test/updates): --insecure-test-allow-adhoc lets prepare
# accept an ad-hoc signed app (no Team ID, no Gatekeeper); QLC_UPDATE_OPEN
# replaces /usr/bin/open for the relaunch. The app never passes either.
set -u

MODE="${1:-}"; shift || true

DMG="" DEST="" RUNNING="" STAGED_NAME=".qlcconsole-update.app" ALLOW_ADHOC=0
PID="" CURRENT="" NEW="" BACKUP="" TEAM="" RELAUNCH=0 SHOW="" LOG=""
while [ $# -gt 0 ]; do
  case "$1" in
    --dmg) DMG="$2"; shift ;;
    --dest-dir) DEST="$2"; shift ;;
    --running-app) RUNNING="$2"; shift ;;
    --staged-name) STAGED_NAME="$2"; shift ;;
    --insecure-test-allow-adhoc) ALLOW_ADHOC=1 ;;
    --pid) PID="$2"; shift ;;
    --current) CURRENT="$2"; shift ;;
    --new) NEW="$2"; shift ;;
    --backup) BACKUP="$2"; shift ;;
    --team-id) TEAM="$2"; shift ;;
    --relaunch) RELAUNCH=1 ;;
    --show) SHOW="$2"; shift ;;
    --log) LOG="$2"; shift ;;
    *) echo "ERROR: unknown argument $1"; exit 2 ;;
  esac
  shift
done

log() {
  if [ -n "$LOG" ]; then
    echo "$(date '+%Y-%m-%d %H:%M:%S') $*" >> "$LOG"
  else
    echo "$*"
  fi
}

# The Team ID an app is signed by; empty for ad-hoc or unsigned.
team_of() {
  local t
  t=$(codesign -dv --verbose=2 "$1" 2>&1 | sed -n 's/^TeamIdentifier=//p' | head -1)
  [ "$t" = "not set" ] && t=""
  echo "$t"
}

bundle_id_of() {
  /usr/libexec/PlistBuddy -c "Print CFBundleIdentifier" "$1/Contents/Info.plist" 2>/dev/null
}

# Is something mounted at $1 (a real path: mount(8) prints /private/var/...)?
mounted_at() {
  mount | grep -qF " on $1 ("
}

# Mount the disk image $1 read-only at the folder $2. hdiutil works through
# diskimagesd, which turns requests away while it is busy with other images
# ("Resource busy", "Resource temporarily unavailable"): try a few times
# before giving up. An image that is simply bad fails the same way every
# time, so those errors end it at once. On failure, prints hdiutil's last
# message.
attach_dmg() {
  local dmg="$1" mnt="$2" out="" try
  for try in 1 2 3 4 5; do
    out=$(hdiutil attach -nobrowse -readonly -noautoopen -noverify -mountpoint "$mnt" "$dmg" 2>&1 >/dev/null) \
      && mounted_at "$mnt" && return 0
    # Half there (attached, then failed -- possibly with no file system
    # mounted at all): let it go before the next try.
    detach_dmg "$mnt"
    detach_image "$dmg"
    case "$out" in
      *"not recognized"*|*"corrupt"*|*"no mountable file systems"*|*"No such file"*|*"Permission denied"*)
        break ;;
    esac
    [ "$try" -lt 5 ] && sleep "$try"
  done
  echo "$out" | grep -v '^[[:space:]]*$' | tail -1
  return 1
}

# Unmount $1 and wait until it is really gone, forcing it if asking nicely
# does not do. Returns 1 if it is still mounted after all that.
detach_dmg() {
  local mnt="$1" try
  for try in 1 2 3 4 5 6; do
    mounted_at "$mnt" || return 0
    if [ "$try" -le 2 ]; then
      hdiutil detach -quiet "$mnt" >/dev/null 2>&1
    else
      hdiutil detach -quiet -force "$mnt" >/dev/null 2>&1
    fi
    mounted_at "$mnt" || return 0
    sleep 1
  done
  ! mounted_at "$mnt"
}

# The devices attached from the disk image file $1, one per line; with
# "unmounted" as $2, only those with no file system mounted anywhere.
devices_of() {
  local real
  real=$(cd "$(dirname "$1")" 2>/dev/null && echo "$(pwd -P)/$(basename "$1")")
  hdiutil info 2>/dev/null | awk -F '\t' -v a="$1" -v b="$real" -v only="${2:-}" '
    function flush() { if (ours && dev != "" && (only != "unmounted" || !mounted)) print dev }
    /^image-path/ {
      flush(); dev = ""; mounted = 0
      p = $0; sub(/^image-path[ \t]*:[ \t]*/, "", p); ours = (p == a || p == b); next
    }
    ours && /^\/dev\/disk[0-9]/ {
      d = $1; sub(/[ \t]+$/, "", d)
      if (dev == "") dev = d
      if (NF >= 3 && $3 ~ /^\//) mounted = 1
    }
    END { flush() }'
}

# Detach the devices this run attached from the disk image file $1, mounted
# or not: an attach that fails part way can leave one attached with nothing
# mounted, which no mount point would ever find again. Ones that were there
# before this run started (PRE_DEVICES) are someone else's and stay.
# Such a device can refuse to go for a moment while the system is still
# probing it, so this waits until they are all really gone (or gives up
# after about ten seconds).
PRE_DEVICES=""
detach_image() {
  local dev devs try
  for try in 1 2 3 4 5 6 7 8 9 10; do
    devs=""
    for dev in $(devices_of "$1" "${2:-}"); do
      case " $PRE_DEVICES " in *" $dev "*) ;; *) devs="$devs $dev" ;; esac
    done
    [ -z "$devs" ] && return 0
    for dev in $devs; do
      if [ "$try" -le 2 ]; then
        hdiutil detach -quiet "$dev" >/dev/null 2>&1
      else
        hdiutil detach -quiet -force "$dev" >/dev/null 2>&1
      fi
    done
    sleep 1
  done
  return 1
}

prepare() {
  local staged="" mnt="" app="" newteam runteam why
  release_dmg() {
    if [ -n "$mnt" ]; then
      detach_dmg "$mnt"
      detach_image "$DMG"
      rmdir "$mnt" 2>/dev/null
      mnt=""
    fi
  }
  fail() {
    release_dmg
    [ -n "$staged" ] && rm -rf "$staged"
    echo "ERROR: $*"
    exit 1
  }
  # Stopped part way (the app quitting, a test timing out): never leave the
  # disk image mounted behind.
  trap 'release_dmg; [ -n "$staged" ] && rm -rf "$staged"; exit 1' HUP INT TERM
  [ -f "$DMG" ] || fail "no disk image at $DMG"
  [ -d "$DEST" ] || fail "no folder $DEST"
  [ -w "$DEST" ] || fail "$DEST is not writable"
  [ -d "$RUNNING" ] || fail "no running app at $RUNNING"
  case "$STAGED_NAME" in */*|"") fail "bad staged name" ;; esac

  # An attach that failed part way -- an earlier try, or something that
  # opened the download to scan it -- can leave the image attached with
  # nothing mounted, and then every attach of it fails ("Resource
  # temporarily unavailable") until it goes. Let those go first; one that is
  # mounted somewhere is someone's, and stays.
  detach_image "$DMG" unmounted
  PRE_DEVICES=$(devices_of "$DMG" | tr '\n' ' ')
  # A mount point of this run's own: several helpers may be at it at once.
  mnt=$(mktemp -d "${TMPDIR:-/tmp}/qlcconsole-dmg.$$.XXXXXX") || fail "no temporary folder"
  mnt=$(cd "$mnt" && pwd -P) || fail "no temporary folder"
  if ! why=$(attach_dmg "$DMG" "$mnt"); then
    rmdir "$mnt" 2>/dev/null; mnt=""
    fail "the disk image would not open${why:+ ($why)}"
  fi
  app=$(find "$mnt" -maxdepth 1 -name '*.app' -type d | head -1)
  [ -n "$app" ] || fail "the disk image holds no app"

  staged="$DEST/$STAGED_NAME"
  rm -rf "$staged"
  ditto "$app" "$staged" || fail "could not copy the app out of the disk image"
  release_dmg

  codesign --verify --deep --strict "$staged" >/dev/null 2>&1 \
    || fail "the new app's code signature is not valid"
  newteam=$(team_of "$staged")
  runteam=$(team_of "$RUNNING")
  if [ "$ALLOW_ADHOC" != 1 ]; then
    [ -n "$runteam" ] || fail "this copy of qlcconsole is not a signed release, so it cannot tell a genuine update; download it from the release page"
    spctl --assess --type execute "$staged" >/dev/null 2>&1 \
      || fail "Gatekeeper does not accept the new app"
  fi
  [ "$newteam" = "$runteam" ] \
    || fail "the new app is signed by a different developer (${newteam:-none}, not ${runteam:-none})"
  [ "$(bundle_id_of "$staged")" = "$(bundle_id_of "$RUNNING")" ] \
    || fail "the new app is not qlcconsole (bundle identifier differs)"

  echo "STAGED: $staged"
  echo "TEAM: $newteam"
  exit 0
}

install() {
  local waited=0
  log "install: pid=$PID current=$CURRENT new=$NEW backup=$BACKUP relaunch=$RELAUNCH show=$SHOW"
  [ -n "$CURRENT" ] && [ -n "$NEW" ] && [ -n "$BACKUP" ] || { log "ERROR: missing paths"; exit 2; }

  # Wait for qlcconsole to quit -- five minutes at most (a save dialog, a
  # slow shutdown); after that, leave everything as it is.
  if [ -n "$PID" ]; then
    while kill -0 "$PID" 2>/dev/null; do
      sleep 0.5
      waited=$((waited + 1))
      if [ "$waited" -ge 600 ]; then
        log "ERROR: qlcconsole (pid $PID) did not quit; not installing"
        exit 1
      fi
    done
  fi

  [ -d "$NEW" ] || { log "ERROR: the staged app $NEW is gone"; exit 1; }
  if [ -n "$TEAM" ]; then
    # It sat beside the app while qlcconsole ran: check it again.
    codesign --verify --deep --strict "$NEW" >/dev/null 2>&1 \
      || { log "ERROR: the staged app's signature no longer verifies"; rm -rf "$NEW"; exit 1; }
    [ "$(team_of "$NEW")" = "$TEAM" ] \
      || { log "ERROR: the staged app's Team ID changed"; rm -rf "$NEW"; exit 1; }
  fi

  rm -rf "$BACKUP"
  if ! mv "$CURRENT" "$BACKUP"; then
    log "ERROR: could not move $CURRENT aside"
    exit 1
  fi
  if ! mv "$NEW" "$CURRENT"; then
    log "ERROR: could not move the new app into place; putting the old one back"
    mv "$BACKUP" "$CURRENT"
    exit 1
  fi
  touch "$CURRENT"    # LaunchServices notices the new bundle
  log "installed $CURRENT (previous kept at $BACKUP)"

  if [ "$RELAUNCH" = 1 ]; then
    local opener="${QLC_UPDATE_OPEN:-/usr/bin/open}"
    if [ -n "$SHOW" ] && [ -f "$SHOW" ]; then
      "$opener" -n "$CURRENT" --args -o "$SHOW"
    else
      "$opener" -n "$CURRENT"
    fi
    log "reopened $CURRENT"
  fi
  exit 0
}

case "$MODE" in
  prepare) prepare ;;
  install) install ;;
  *) echo "usage: $0 prepare|install ..."; exit 2 ;;
esac
