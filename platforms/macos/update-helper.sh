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

prepare() {
  local staged="" mnt="" app="" newteam runteam
  fail() {
    [ -n "$mnt" ] && hdiutil detach -quiet -force "$mnt" >/dev/null 2>&1
    [ -n "$mnt" ] && rmdir "$mnt" 2>/dev/null
    [ -n "$staged" ] && rm -rf "$staged"
    echo "ERROR: $*"
    exit 1
  }
  [ -f "$DMG" ] || fail "no disk image at $DMG"
  [ -d "$DEST" ] || fail "no folder $DEST"
  [ -w "$DEST" ] || fail "$DEST is not writable"
  [ -d "$RUNNING" ] || fail "no running app at $RUNNING"
  case "$STAGED_NAME" in */*|"") fail "bad staged name" ;; esac

  mnt=$(mktemp -d "${TMPDIR:-/tmp}/qlcconsole-dmg.XXXXXX") || fail "no temporary folder"
  hdiutil attach -nobrowse -readonly -noautoopen -noverify -mountpoint "$mnt" "$DMG" >/dev/null 2>&1 \
    || { rmdir "$mnt"; mnt=""; fail "the disk image would not open"; }
  app=$(find "$mnt" -maxdepth 1 -name '*.app' -type d | head -1)
  [ -n "$app" ] || fail "the disk image holds no app"

  staged="$DEST/$STAGED_NAME"
  rm -rf "$staged"
  ditto "$app" "$staged" || fail "could not copy the app out of the disk image"
  hdiutil detach -quiet "$mnt" >/dev/null 2>&1 || hdiutil detach -quiet -force "$mnt" >/dev/null 2>&1
  rmdir "$mnt" 2>/dev/null; mnt=""

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
