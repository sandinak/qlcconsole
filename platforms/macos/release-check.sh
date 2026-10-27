#!/bin/bash
# Pre-release check of a macOS DMG: proves the DMG and the app in it are what
# we mean to publish, before anything is published.
#
#   platforms/macos/release-check.sh <dmg> [options]
#   make release-check                      (the newest dist/*.dmg)
#
# Options:
#   --host <ssh-host>     run the whole check on another Mac (a clean one,
#                         e.g. EvoAudio): copies this script, the DMG, its
#                         SHA256SUMS, the entitlements and an offscreen
#                         plugin to a temp dir there, runs, copies the
#                         results back and removes everything it put there
#   --sums <file>         SHA256SUMS to check against (default: the one
#                         next to the DMG)
#   --version <x.y.z>     expected release (default: from the DMG's name)
#   --rev <sha>           expected git rev (default: from the DMG's name)
#   --entitlements <f>    expected entitlements (default: the repo's)
#   --offscreen-plugin <f> libqoffscreen.dylib to run the app headless with,
#                         when the bundle ships none (default: the bundle's
#                         own, else $QTDIR's, else ~/Qt-aqt/<Qt version>)
#   --ls-real             register the LaunchServices test copy outside the
#                         temp dirs, so it can become the default handler
#                         (on by default with --host: a clean machine)
#   --resource-manifest <f> sha256 of every resource file the repo installs
#                         (default: made from the checkout's resources/; a
#                         --host run sends it along): every file in the
#                         bundle's Fixtures, RGBScripts, ... must be one of
#                         them, byte for byte -- nothing from the builder's
#                         home folder
#   --out <dir>           where the TAP / JSON results go
#                         (default: build-release-check/ in the repo, or a
#                         temp dir)
#   --waive <id,id,...>   known failures to report as TODO instead of
#                         failing (also RELEASE_CHECK_WAIVE)
#
# What it never does: launch the app with a window (every run is
# QT_QPA_PLATFORM=offscreen + -platform offscreen), launch a quarantined app
# (Gatekeeper is checked with spctl only), touch the user's settings (each
# run gets its own HOME and isolated QSettings), or leave anything behind:
# every copy it makes is unregistered and deleted, and crash reports it
# causes are removed (and fail the check).
#
# Output: TAP on stdout and in <out>/release-check.tap, JSON in
# <out>/release-check.json, a summary at the end. Exit 0 = every check
# passed (waived ones aside), 1 = a check failed, 2 = could not run.
# No pipefail: `cmd | grep -q` would fail on the SIGPIPE grep -q causes.
set -u
export LC_ALL=C

SCRIPTDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPTDIR/../.." 2>/dev/null && pwd)"
[ -f "$REPO/VERSION" ] || REPO=""

TEAM_ID="${TEAM_ID:-B4N7HJ7VZ6}"
BUNDLE_ID="${BUNDLE_ID:-com.bransonmatheson.qlcconsole}"
MIN_OS="${MIN_OS:-12.0}"
LSREGISTER=/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/LaunchServices.framework/Versions/A/Support/lsregister

die() { echo "release-check: $*" >&2; exit 2; }

DMG="" HOST="" SUMS="" EXP_VERSION="" EXP_REV="" ENT="" OFFSCREEN="" LS_REAL=0 OUT="" MANIFEST=""
WAIVE="${RELEASE_CHECK_WAIVE:-}"
while [ $# -gt 0 ]; do
  case "$1" in
    --host) HOST="$2"; shift 2 ;;
    --sums) SUMS="$2"; shift 2 ;;
    --version) EXP_VERSION="$2"; shift 2 ;;
    --rev) EXP_REV="$2"; shift 2 ;;
    --entitlements) ENT="$2"; shift 2 ;;
    --offscreen-plugin) OFFSCREEN="$2"; shift 2 ;;
    --ls-real) LS_REAL=1; shift ;;
    --no-ls-real) LS_REAL=0; shift ;;
    --resource-manifest) MANIFEST="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --waive) WAIVE="${WAIVE:+$WAIVE,}$2"; shift 2 ;;
    -h|--help) sed -n "2,/^# No pipefail/p" "$0" | sed "\$d; s/^# \{0,1\}//"; exit 0 ;;
    -*) die "unknown option $1" ;;
    *) [ -z "$DMG" ] || die "one DMG only"; DMG="$1"; shift ;;
  esac
done
[ -n "$DMG" ] || die "usage: $0 <dmg> [--host <ssh-host>] (--help)"
[ -f "$DMG" ] || die "no such DMG: $DMG"
DMG="$(cd "$(dirname "$DMG")" && pwd)/$(basename "$DMG")"
DMGNAME="$(basename "$DMG")"
[ -n "$SUMS" ] || SUMS="$(dirname "$DMG")/SHA256SUMS"
[ -n "$ENT" ] || ENT="$SCRIPTDIR/qlcconsole.entitlements"

# qlcconsole-<version>-<flavour>-<date>-<rev>.dmg
if [ -z "$EXP_VERSION" ]; then
  EXP_VERSION="$(echo "$DMGNAME" | sed -n 's/^qlcconsole-\([0-9][0-9.]*\)-.*\.dmg$/\1/p')"
fi
if [ -z "$EXP_REV" ]; then
  EXP_REV="$(echo "$DMGNAME" | sed -n 's/^.*-\([0-9a-f]\{7,40\}\)\.dmg$/\1/p')"
fi

# What the app should carry, from the source when there is one (a remote run
# gets these from the local side).
if [ -z "${EXPECT_LANGS:-}" ] && [ -n "$REPO" ] && [ -f "$REPO/translate.sh" ]; then
  EXPECT_LANGS="$(sed -n 's/^UI_LANGS="\(.*\)"$/\1/p' "$REPO/translate.sh")"
fi
EXPECT_LANGS="${EXPECT_LANGS:-de_DE es_ES fr_FR it_IT nl_NL cz_CZ pt_BR ca_ES ja_JP}"
if [ -z "${EXPECT_HELP:-}" ] && [ -n "$REPO" ] && [ -f "$REPO/ui/src/help.qrc" ]; then
  EXPECT_HELP="$(sed -n 's/.*alias="help\/\([^"]*\)".*/\1/p' "$REPO/ui/src/help.qrc" | tr '\n' ' ')"
fi
EXPECT_HELP="${EXPECT_HELP:-getting-started.md whats-new.md README.md updates.md}"
IO_PLUGINS="${IO_PLUGINS:-artnet dmxusb e131 enttecwing hidplugin loopback midiplugin os2l osc peperoni udmx wled}"

# Resource folders: <repo resources/ dir>:<bundle Resources/ dir>.
RES_DIRS="fixtures:Fixtures rgbscripts:RGBScripts miditemplates:MidiTemplates modifierstemplates:ModifiersTemplates
  bundles:Bundles inputprofiles:InputProfiles effectscripts:EffectScripts effectpresets:EffectPresets gobos:Gobos
  deviceprofiles:DeviceProfiles hidprofiles:HIDProfiles licenses:licenses"
make_manifest() { # -> stdout: "<sha256>  <BundleDir>/<path>" for every file the repo installs
  local pair src dst
  for pair in $RES_DIRS; do
    src="${pair%%:*}"; dst="${pair#*:}"
    [ -d "$REPO/resources/$src" ] || continue
    ( cd "$REPO/resources/$src" && find . -type f ! -name CMakeLists.txt ! -name Makefile ! -name '.*' -print0 \
        | xargs -0 shasum -a 256 ) | sed "s|  \./|  $dst/|"
  done
  # COPYING is installed as licenses/Apache-2.0.txt.
  echo "$(shasum -a 256 "$REPO/COPYING" | awk '{print $1}')  licenses/Apache-2.0.txt"
}

# ---------------------------------------------------------------------------
# --host: run all of this on another Mac, then clean up there.
# ---------------------------------------------------------------------------
if [ -n "$HOST" ]; then
  SSH=(ssh -o BatchMode=yes -o ConnectTimeout=10 "$HOST")
  "${SSH[@]}" true || die "cannot ssh to $HOST"
  [ -n "$OUT" ] || OUT="${REPO:+$REPO/build-release-check}"
  [ -n "$OUT" ] || OUT="$(mktemp -d "${TMPDIR:-/tmp}/qlc-relcheck-out.XXXXXX")"
  OUT="$OUT/$(echo "$HOST" | tr -c 'A-Za-z0-9._-\n' _)"
  mkdir -p "$OUT"
  # An offscreen plugin for the remote run: found here, since the clean
  # machine has no Qt. (Same lookup as below, without the mounted DMG.)
  if [ -z "$OFFSCREEN" ]; then
    for c in "${OFFSCREEN_PLUGIN:-}" "${QTDIR:+$QTDIR/plugins/platforms/libqoffscreen.dylib}" \
             "$HOME/Qt-aqt/6.9.3/macos/plugins/platforms/libqoffscreen.dylib"; do
      [ -n "$c" ] && [ -f "$c" ] && { OFFSCREEN="$c"; break; }
    done
  fi
  R="$("${SSH[@]}" 'mktemp -d /tmp/qlc-relcheck.XXXXXX')" || die "no temp dir on $HOST"
  case "$R" in /tmp/qlc-relcheck.*) ;; *) die "odd remote temp dir '$R'" ;; esac
  cleanup_remote() {
    "${SSH[@]}" "/bin/rm -rf '$R'; test ! -e '$R'" \
      && echo "# $HOST: removed $R" || echo "# WARNING: could not remove $R on $HOST" >&2
  }
  trap cleanup_remote EXIT
  echo "# copying the DMG and the check to $HOST:$R"
  files=("$0" "$DMG" "$ENT")
  if [ -z "$MANIFEST" ] && [ -n "$REPO" ]; then
    mkdir -p "$OUT/.manifest"; make_manifest > "$OUT/.manifest/resources.sha256"; MANIFEST="$OUT/.manifest/resources.sha256"
  fi
  [ -n "$MANIFEST" ] && files+=("$MANIFEST")
  [ -f "$SUMS" ] && { mkdir -p "$OUT/.sums"; cp "$SUMS" "$OUT/.sums/SHA256SUMS"; files+=("$OUT/.sums/SHA256SUMS"); }
  [ -n "$OFFSCREEN" ] && files+=("$OFFSCREEN")
  scp -q -o BatchMode=yes "${files[@]}" "$HOST:$R/" || die "copy to $HOST failed"
  rm -rf "$OUT/.sums"
  args=("$R/$DMGNAME" --sums "$R/SHA256SUMS" --entitlements "$R/$(basename "$ENT")"
        --version "$EXP_VERSION" --rev "$EXP_REV" --out "$R/out")
  [ -n "$OFFSCREEN" ] && args+=(--offscreen-plugin "$R/$(basename "$OFFSCREEN")")
  [ -n "$MANIFEST" ] && args+=(--resource-manifest "$R/$(basename "$MANIFEST")")
  [ "${LS_REAL_REMOTE:-1}" = 1 ] && args+=(--ls-real)
  [ -n "$WAIVE" ] && args+=(--waive "$WAIVE")
  q=""; for a in "${args[@]}"; do q="$q '$a'"; done
  "${SSH[@]}" "EXPECT_LANGS='$EXPECT_LANGS' EXPECT_HELP='$EXPECT_HELP' IO_PLUGINS='$IO_PLUGINS' \
    TEAM_ID='$TEAM_ID' BUNDLE_ID='$BUNDLE_ID' MIN_OS='$MIN_OS' \
    /bin/bash '$R/$(basename "$0")' $q"
  rc=$?
  scp -q -o BatchMode=yes "$HOST:$R/out/*" "$OUT/" 2>/dev/null || true
  rm -rf "$OUT/.manifest"
  echo "# results copied to $OUT"
  exit $rc
fi

# ---------------------------------------------------------------------------
# Local run.
# ---------------------------------------------------------------------------
[ "$(uname -s)" = Darwin ] || die "macOS only"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/qlc-relcheck.XXXXXX")" || die "mktemp failed"
WORK="$(cd "$WORK" && pwd -P)"
if [ -z "$OUT" ]; then
  if [ -n "$REPO" ] && [ -w "$REPO" ]; then OUT="$REPO/build-release-check"; else OUT="$WORK/out"; fi
fi
mkdir -p "$OUT" || die "cannot make $OUT"
TAP="$OUT/release-check.tap"; JSON="$OUT/release-check.json"
: > "$TAP"; : > "$JSON.tmp"
MNT="" LSDIR="" REGISTERED=""
DIAG="$HOME/Library/Logs/DiagnosticReports"
ls -1 "$DIAG" 2>/dev/null | sort > "$WORK/diag.before"

unregister_ours() {
  # LaunchServices registers the app on the mounted DMG by itself, and may
  # register a copy that runs: none of them may outlive the check.
  local p
  for p in "$REGISTERED" "${MNT:+$MNT/qlcconsole.app}" "$WORK/run/qlcconsole.app" "${LSDIR:+$LSDIR/qlcconsole.app}"; do
    [ -n "$p" ] && "$LSREGISTER" -u "$p" >/dev/null 2>&1
  done
  REGISTERED=""
}
cleanup() {
  unregister_ours
  [ -n "$MNT" ] && hdiutil detach -quiet "$MNT" >/dev/null 2>&1 \
    || { [ -n "$MNT" ] && hdiutil detach -force -quiet "$MNT" >/dev/null 2>&1; }
  [ -n "$LSDIR" ] && rm -rf "$LSDIR"
  rm -rf "$WORK"
}
trap cleanup EXIT
trap 'exit 2' INT TERM

# --- TAP / JSON -------------------------------------------------------------
N=0 NFAIL=0 NWAIVED=0 NSKIP=0
FAILED_LIST="" WAIVED_LIST=""
jesc() { printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g' | tr '\n\t' '  '; }
record() { # <status> <id> <detail>
  printf '  {"id":"%s","status":"%s","detail":"%s"},\n' "$(jesc "$2")" "$1" "$(jesc "$3")" >> "$JSON.tmp"
}
emit() { echo "$1" | tee -a "$TAP"; }
waived() { case ",$WAIVE," in *",$1,"*) return 0 ;; esac; return 1; }
pass() { N=$((N+1)); emit "ok $N - $1${2:+ # $2}"; record pass "$1" "${2:-}"; }
fail() {
  N=$((N+1))
  if waived "$1"; then
    NWAIVED=$((NWAIVED+1)); WAIVED_LIST="$WAIVED_LIST
  $1: ${2:-}"
    emit "not ok $N - $1 # TODO waived: ${2:-}"; record waived "$1" "${2:-}"
  else
    NFAIL=$((NFAIL+1)); FAILED_LIST="$FAILED_LIST
  $1: ${2:-}"
    emit "not ok $N - $1${2:+ # $2}"; record fail "$1" "${2:-}"
  fi
}
skip() { N=$((N+1)); NSKIP=$((NSKIP+1)); emit "ok $N - $1 # SKIP ${2:-}"; record skip "$1" "${2:-}"; }
note() { emit "# $*"; }
check() { # <id> <detail-if-fail> <command...>: pass when the command does
  local id="$1" why="$2"; shift 2
  if "$@" >/dev/null 2>&1; then pass "$id"; else fail "$id" "$why"; fi
}
section() { emit "# --- $* ---"; }

note "qlcconsole macOS release check: $DMGNAME"
note "host $(hostname -s) $(sw_vers -productVersion) $(uname -m); expect version=$EXP_VERSION rev=$EXP_REV team=$TEAM_ID"

# --- the Mach-O reader (perl: no Xcode tools needed on a clean Mac) ----------
cat > "$WORK/macho.pl" <<'PERL'
# perl macho.pl <file|dir>...  -> one line per fact, Mach-O files only:
#   ARCH <file> <arch> <filetype> <minos>
#   DYLIB <file> <arch> <kind> <name> <current-version>
#   ID <file> <arch> <name> <current-version>
#   RPATH <file> <arch> <path>
use strict; use warnings; use File::Find;
my %cpu = (0x01000007 => 'x86_64', 0x0100000c => 'arm64', 7 => 'i386', 12 => 'arm');
my %ft = (2 => 'exe', 6 => 'dylib', 8 => 'bundle');
sub ver { my $v = shift; sprintf "%d.%d.%d", $v >> 16, ($v >> 8) & 0xff, $v & 0xff }
sub cstr { my ($b, $o) = @_; my $s = substr($b, $o); $s =~ s/\0.*//s; $s }
sub thin {
  my ($f, $b) = @_;
  my $m = unpack('V', $b); return unless $m == 0xfeedfacf || $m == 0xfeedface;
  my ($cpu, $sub, $type, $ncmds) = unpack('x4 V V V V', $b);
  my $arch = $cpu{$cpu} // sprintf('cpu%x', $cpu);
  my $off = $m == 0xfeedfacf ? 32 : 28; my $minos = '-'; my @out;
  for (1 .. $ncmds) {
    my ($cmd, $size) = unpack('V V', substr($b, $off, 8));
    last unless $size;
    my $lc = substr($b, $off, $size);
    if ($cmd == 0xc || $cmd == 0x80000018 || $cmd == 0x8000001f || $cmd == 0x20 || $cmd == 0x80000023) {
      my ($no, $ts, $cur) = unpack('x8 V V V', $lc);
      my $k = {0xc => 'load', 0x80000018 => 'weak', 0x8000001f => 'reexport', 0x20 => 'lazy', 0x80000023 => 'upward'}->{$cmd};
      push @out, "DYLIB\t$f\t$arch\t$k\t" . cstr($lc, $no) . "\t" . ver($cur);
    } elsif ($cmd == 0xd) {
      my ($no, $ts, $cur) = unpack('x8 V V V', $lc);
      push @out, "ID\t$f\t$arch\t" . cstr($lc, $no) . "\t" . ver($cur);
    } elsif ($cmd == 0x8000001c) {
      push @out, "RPATH\t$f\t$arch\t" . cstr($lc, unpack('x8 V', $lc));
    } elsif ($cmd == 0x32) {
      $minos = ver(unpack('x12 V', $lc));
    } elsif ($cmd == 0x24 && $minos eq '-') {
      $minos = ver(unpack('x8 V', $lc));
    }
    $off += $size;
  }
  print "ARCH\t$f\t$arch\t" . ($ft{$type} // $type) . "\t$minos\n";
  print "$_\n" for @out;
}
sub one {
  my $f = shift; return unless -f $f && !-l $f && -s $f > 64;
  open(my $h, '<:raw', $f) or return; read($h, my $hd, 8);
  my $m = unpack('N', $hd);
  my $lm = unpack('V', $hd);
  if ($m == 0xcafebabe || $m == 0xcafebabf) {
    my $n = unpack('x4 N', $hd); return if $n == 0 || $n > 16;   # a Java class file
    my $esz = $m == 0xcafebabe ? 20 : 32;
    read($h, my $tab, $n * $esz);
    for my $i (0 .. $n - 1) {
      my ($o, $s);
      if ($m == 0xcafebabe) { ($o, $s) = unpack('x8 N N', substr($tab, $i * $esz, $esz)); }
      else { my @q = unpack('x8 N N N N', substr($tab, $i * $esz, $esz)); $o = $q[0] * 2**32 + $q[1]; $s = $q[2] * 2**32 + $q[3]; }
      seek($h, $o, 0); read($h, my $b, $s < 4194304 ? $s : 4194304); thin($f, $b);
    }
  } elsif ($lm == 0xfeedfacf || $lm == 0xfeedface) {
    seek($h, 0, 0); read($h, my $b, 4194304); thin($f, $b);
  }
}
for my $a (@ARGV) {
  if (-d $a) { find({ no_chdir => 1, wanted => sub { one($File::Find::name) } }, $a) } else { one($a) }
}
PERL
macho() { perl "$WORK/macho.pl" "$@"; }

# Does <file> contain each <string> stored UTF-16BE (how rcc names resources)?
cat > "$WORK/utf16.pl" <<'PERL'
use strict; my $f = shift; local $/; open(my $h, '<:raw', $f) or exit 2; my $d = <$h>;
for my $s (@ARGV) { my $u = join('', map { "\0$_" } split //, $s); print((index($d, $u) >= 0 ? "HAS" : "MISSING") . "\t$s\n"); }
PERL

plist() { /usr/libexec/PlistBuddy -c "Print :$2" "$1" 2>/dev/null; }

# ===========================================================================
section "DMG"
# ===========================================================================
if [ -f "$SUMS" ]; then
  want="$(awk -v n="$DMGNAME" '$2 == n || $2 == "*"n {print $1}' "$SUMS" | head -1)"
  have="$(shasum -a 256 "$DMG" | awk '{print $1}')"
  if [ -z "$want" ]; then fail dmg.sha256sums "$DMGNAME has no line in $SUMS"
  elif [ "$want" = "$have" ]; then pass dmg.sha256sums "$have"
  else fail dmg.sha256sums "SHA256SUMS says $want, the file is $have"; fi
else
  fail dmg.sha256sums "no SHA256SUMS at $SUMS"
fi

cs_dmg="$(codesign -dvvv "$DMG" 2>&1)"
check dmg.signature "codesign --verify fails on the DMG" codesign --verify --strict "$DMG"
if echo "$cs_dmg" | grep -q "^TeamIdentifier=$TEAM_ID\$" && echo "$cs_dmg" | grep -q '^Authority=Developer ID Application'; then
  pass dmg.team "Developer ID, $TEAM_ID"
else
  fail dmg.team "not signed by Developer ID team $TEAM_ID: $(echo "$cs_dmg" | grep -m1 '^Authority=\|TeamIdentifier' )"
fi
# The ticket: stapler validates it when the Xcode tools are there (never
# call xcrun without them: that pops an install dialog); otherwise codesign
# reports it (macOS 14 and later print "Notarization Ticket=stapled").
stapled() { # <path> <codesign -dvvv output>
  if xcode-select -p >/dev/null 2>&1; then xcrun stapler validate -q "$1" >/dev/null 2>&1
  else echo "$2" | grep -q '^Notarization Ticket=stapled'; fi
}
if stapled "$DMG" "$cs_dmg"; then pass dmg.stapled
else fail dmg.stapled "no valid notarization ticket stapled to the DMG"; fi
gk="$(spctl -a -t open --context context:primary-signature -v "$DMG" 2>&1)"
if echo "$gk" | grep -q ': accepted' && echo "$gk" | grep -q 'source=Notarized Developer ID'; then
  pass dmg.gatekeeper "accepted, Notarized Developer ID"
else
  fail dmg.gatekeeper "$(echo "$gk" | tr '\n' ' ')"
fi

MNT="$WORK/mnt"; mkdir -p "$MNT"
if ! hdiutil attach -nobrowse -readonly -noautoopen -mountpoint "$MNT" "$DMG" >"$WORK/attach.log" 2>&1; then
  MNT=""
  fail dmg.mount "hdiutil attach failed: $(tail -1 "$WORK/attach.log")"
  echo "cannot go on without the DMG's contents" >&2
  exit 1
fi
pass dmg.mount
APP="$MNT/qlcconsole.app"
if [ -d "$APP" ] && [ "$(readlink "$MNT/Applications")" = /Applications ]; then
  pass dmg.layout "qlcconsole.app + Applications link"
else
  fail dmg.layout "expected qlcconsole.app and an Applications link: $(ls "$MNT" | tr '\n' ' ')"
fi
[ -d "$APP" ] || { echo "no qlcconsole.app in the DMG" >&2; exit 1; }
C="$APP/Contents"

# ===========================================================================
section "Signature"
# ===========================================================================
check app.codesign "codesign --verify --deep --strict fails" codesign --verify --deep --strict "$APP"
cs_app="$(codesign -dvvv "$APP" 2>&1)"
if echo "$cs_app" | grep -q "^TeamIdentifier=$TEAM_ID\$" && echo "$cs_app" | grep -q '^Authority=Developer ID Application'; then
  pass app.team "$TEAM_ID"; else fail app.team "$(echo "$cs_app" | grep -m2 'Authority=\|TeamIdentifier')"; fi
echo "$cs_app" | grep -q '^Timestamp=' && pass app.timestamp || fail app.timestamp "no secure timestamp"
stapled "$APP" "$cs_app" && pass app.stapled || fail app.stapled "no valid notarization ticket stapled to the app"
gk="$(spctl -a -t exec -vv "$APP" 2>&1)"
if echo "$gk" | grep -q ': accepted' && echo "$gk" | grep -q 'source=Notarized Developer ID'; then
  pass app.gatekeeper "accepted, Notarized Developer ID"; else fail app.gatekeeper "$(echo "$gk" | tr '\n' ' ')"; fi

# Hardened runtime and entitlements on every executable that carries them.
plutil -convert xml1 -o "$WORK/ent.want" "$ENT" 2>/dev/null || fail app.entitlements-source "cannot read $ENT"
exes="$C/MacOS/qlcconsole-launcher $C/MacOS/qlcconsole $C/MacOS/qlcconsole-fixtureeditor"
noruntime=""
for f in $( { find "$C" -type f -perm -u+x; find "$C" -type f -name "*.dylib"; } | sort -u); do
  file -b "$f" | grep -q Mach-O || continue
  codesign -dv "$f" 2>&1 | grep -q 'flags=.*runtime' || noruntime="$noruntime ${f#$C/}"
done
[ -z "$noruntime" ] && pass app.hardened-runtime "every Mach-O" || fail app.hardened-runtime "no hardened runtime:$noruntime"
for f in $exes; do
  id="app.entitlements.$(basename "$f")"
  [ -f "$f" ] || { fail "$id" "missing ${f#$C/}"; continue; }
  codesign -d --entitlements - --xml "$f" > "$WORK/ent.raw" 2>/dev/null
  if [ ! -s "$WORK/ent.raw" ]; then fail "$id" "no entitlements"; continue; fi
  plutil -convert xml1 -o "$WORK/ent.have" "$WORK/ent.raw" 2>/dev/null
  if cmp -s "$WORK/ent.want" "$WORK/ent.have"; then pass "$id" "match qlcconsole.entitlements"
  else fail "$id" "differ from $(basename "$ENT"): $(diff "$WORK/ent.want" "$WORK/ent.have" | grep '^[<>].*key' | tr '\n' ' ')"; fi
done

# ===========================================================================
section "Mach-O: architectures, library paths, deployment target"
# ===========================================================================
macho "$C" > "$WORK/macho.txt"
nmacho="$(awk -F'\t' '$1=="ARCH"{print $2}' "$WORK/macho.txt" | sort -u | wc -l | tr -d ' ')"
note "$nmacho Mach-O files"
notuni="$(awk -F'\t' '$1=="ARCH"{a[$2]=a[$2]" "$3} END{for(f in a) if (a[f] !~ /x86_64/ || a[f] !~ /arm64/) print f":"a[f]}' "$WORK/macho.txt" | sed "s|$C/||")"
[ -z "$notuni" ] && pass macho.universal "$nmacho files, x86_64 + arm64" || fail macho.universal "not universal: $(echo "$notuni" | tr '\n' ' ')"
# Absolute references outside the OS, and rpaths that point off the bundle.
abs="$(awk -F'\t' '$1=="DYLIB" && $5 ~ /^\// && $5 !~ /^\/(usr\/lib|System\/Library)\// {print $2" -> "$5}' "$WORK/macho.txt" | sort -u | sed "s|$C/||")"
[ -z "$abs" ] && pass macho.no-absolute-deps || fail macho.no-absolute-deps "$(echo "$abs" | head -10 | tr '\n' ';')"
badrp="$(awk -F'\t' '$1=="RPATH" && $4 !~ /^@(loader|executable)_path/ {print $2" rpath "$4}' "$WORK/macho.txt" | sort -u | sed "s|$C/||")"
[ -z "$badrp" ] && pass macho.rpaths "only @loader_path / @executable_path" || fail macho.rpaths "$(echo "$badrp" | head -10 | tr '\n' ';')"
# Every @rpath reference resolves inside the bundle (package-local.sh's own
# rule: Frameworks/), and every @loader_path/@executable_path one too.
unres=""
while IFS=$'\t' read -r _ f arch kind name ver; do
  case "$name" in
    @rpath/*) [ -e "$C/Frameworks/${name#@rpath/}" ] || unres="$unres ${f#$C/}->$name" ;;
    @loader_path/*) [ -e "$(dirname "$f")/${name#@loader_path/}" ] || unres="$unres ${f#$C/}->$name" ;;
    @executable_path/*) [ -e "$C/MacOS/${name#@executable_path/}" ] || unres="$unres ${f#$C/}->$name" ;;
  esac
done < <(awk -F'\t' '$1=="DYLIB"' "$WORK/macho.txt" | sort -u)
[ -z "$unres" ] && pass macho.deps-resolve "every @rpath/@loader_path reference is in the bundle" \
  || fail macho.deps-resolve "$(echo "$unres" | tr ' ' '\n' | sort -u | head -12 | tr '\n' ' ')"
plist_min="$(plist "$C/Info.plist" LSMinimumSystemVersion)"
verle() { [ "$(printf '%s\n%s\n' "$1" "$2" | sort -t. -k1,1n -k2,2n -k3,3n | head -1)" = "$1" ]; }
toonew="$(awk -F'\t' '$1=="ARCH"{print $5"\t"$2}' "$WORK/macho.txt" | sort -u | while IFS=$'\t' read -r v f; do
  [ "$v" = - ] && continue; verle "$v" "${plist_min:-0}.0" || echo "${f#$C/}($v)"; done | sort -u)"
[ -z "$toonew" ] && pass macho.minos "no binary needs more than macOS ${plist_min:-?}" \
  || fail macho.minos "need a newer macOS than LSMinimumSystemVersion ${plist_min:-?}: $(echo "$toonew" | head -8 | tr '\n' ' ')"

# ===========================================================================
section "Plugins"
# ===========================================================================
P="$C/PlugIns"
for q in platforms/libqcocoa styles/libqmacstyle iconengines/libqsvgicon imageformats/libqjpeg \
         imageformats/libqgif imageformats/libqico imageformats/libqsvg imageformats/libqicns \
         tls/libqsecuretransportbackend networkinformation/libqscnetworkreachability; do
  check "plugin.qt.$(basename "$q" | sed 's/^libq*//')" "missing PlugIns/$q.dylib" test -f "$P/$q.dylib"
done
if ls "$P"/multimedia/*.dylib >/dev/null 2>&1; then
  pass plugin.qt.multimedia "$(cd "$P/multimedia" && ls | tr '\n' ' ')"
else
  fail plugin.qt.multimedia "no PlugIns/multimedia backend"
fi
# SQL: only if something links QtSql.
if grep -q 'QtSql.framework' "$WORK/macho.txt"; then
  ls "$P"/sqldrivers/*.dylib >/dev/null 2>&1 && pass plugin.qt.sqldrivers || fail plugin.qt.sqldrivers "QtSql is linked but no PlugIns/sqldrivers"
else
  skip plugin.qt.sqldrivers "nothing links QtSql"
fi
for p in $IO_PLUGINS; do
  check "plugin.io.$p" "missing PlugIns/lib$p.dylib" test -f "$P/lib$p.dylib"
done
check plugin.audio.sndfile "missing PlugIns/Audio/libsndfileplugin.dylib" test -f "$P/Audio/libsndfileplugin.dylib"
if [ -f "$P/platforms/libqoffscreen.dylib" ]; then
  pass plugin.qt.offscreen "shipped"
else
  fail plugin.qt.offscreen "no PlugIns/platforms/libqoffscreen.dylib: the app cannot run headless (-platform offscreen) as shipped"
fi
ffv="$(strings "$C/Frameworks/libavcodec."*.dylib 2>/dev/null | grep -m1 -- '--enable-\|--disable-' || true)"
if [ -n "$ffv" ]; then
  if echo "$ffv" | grep -q -- '--enable-gpl'; then fail ffmpeg.licence "FFmpeg built --enable-gpl"
  else pass ffmpeg.licence "LGPL (no --enable-gpl)"; fi
else
  ls "$C/Frameworks/"libav* >/dev/null 2>&1 && note "FFmpeg present, configuration string not found" || skip ffmpeg.licence "no FFmpeg"
fi
if ls "$C/Frameworks/"libav* >/dev/null 2>&1; then
  # (L)GPL code ships with its licence text.
  want="GNU LESSER GENERAL PUBLIC LICENSE"; echo "$ffv" | grep -q -- '--enable-gpl' && want="GNU GENERAL PUBLIC LICENSE"
  if grep -rlq "$want" "$C/Resources" 2>/dev/null; then pass ffmpeg.licence-text "$want shipped"
  else fail ffmpeg.licence-text "FFmpeg ships without the $want text"; fi
fi

# ===========================================================================
section "Info.plist"
# ===========================================================================
I="$C/Info.plist"
v="$(plist "$I" CFBundleIdentifier)"; [ "$v" = "$BUNDLE_ID" ] && pass plist.bundle-id "$v" || fail plist.bundle-id "'$v', want $BUNDLE_ID"
v="$(plist "$I" CFBundleShortVersionString)"; [ "$v" = "$EXP_VERSION" ] && pass plist.short-version "$v" || fail plist.short-version "'$v', want $EXP_VERSION"
PLIST_BUILD="$(plist "$I" CFBundleVersion)"
[ -n "$PLIST_BUILD" ] && pass plist.build-present "$PLIST_BUILD" || fail plist.build-present "no CFBundleVersion"
v="$(plist "$I" LSMinimumSystemVersion)"; [ "$v" = "$MIN_OS" ] && pass plist.min-os "$v" || fail plist.min-os "'$v', want $MIN_OS"
v="$(plist "$I" CFBundleExecutable)"; [ -n "$v" ] && [ -x "$C/MacOS/$v" ] && pass plist.executable "$v" || fail plist.executable "'$v' is not an executable in MacOS/"
v="$(plist "$I" CFBundleIconFile)"; [ -n "$v" ] && [ -s "$C/Resources/$v" ] && pass plist.icon "$v" || fail plist.icon "'$v' not in Resources/"
v="$(plist "$I" CFBundlePackageType)"; [ "$v" = APPL ] && pass plist.package-type || fail plist.package-type "'$v'"
for k in NSMicrophoneUsageDescription NSLocalNetworkUsageDescription; do
  v="$(plist "$I" $k)"; [ -n "$v" ] && pass "plist.$k" || fail "plist.$k" "missing (the app uses it: macOS refuses access without it)"
done
# Document types: every claimed extension has a name, a role and an icon that
# exists, and is backed by a declared UTI (exported or imported).
CLAIMED=""
i=0
while plist "$I" "CFBundleDocumentTypes:$i" >/dev/null; do
  name="$(plist "$I" "CFBundleDocumentTypes:$i:CFBundleTypeName")"
  role="$(plist "$I" "CFBundleDocumentTypes:$i:CFBundleTypeRole")"
  icon="$(plist "$I" "CFBundleDocumentTypes:$i:CFBundleTypeIconFile")"
  j=0; exts=""
  while e="$(plist "$I" "CFBundleDocumentTypes:$i:CFBundleTypeExtensions:$j")"; do exts="$exts $e"; j=$((j+1)); done
  j=0; utis=""
  while u="$(plist "$I" "CFBundleDocumentTypes:$i:LSItemContentTypes:$j")"; do utis="$utis $u"; j=$((j+1)); done
  for e in $exts; do CLAIMED="$CLAIMED $e"; done
  what="${exts# }${utis:+ /$utis}"
  if [ -n "$name" ] && [ -n "$role" ] && [ -n "$icon" ] && [ -s "$C/Resources/$icon" ]; then
    pass "plist.doctype.${exts# }" "$name, $role, $what"
  else
    fail "plist.doctype.${exts# }" "name='$name' role='$role' icon='$icon' (must exist in Resources)"
  fi
  i=$((i+1))
done
[ $i -gt 0 ] || fail plist.doctypes "no CFBundleDocumentTypes"
for e in qlcc qxw qxf; do
  case " $CLAIMED " in *" $e "*) ;; *) fail "plist.claims.$e" "the app does not claim .$e" ;; esac
done
# UTI declarations for each claimed extension.
utidecl() { # <ext>: prints the UTI that declares it, if any
  local key i j e
  for key in UTExportedTypeDeclarations UTImportedTypeDeclarations; do
    i=0
    while plist "$I" "$key:$i" >/dev/null; do
      j=0
      while e="$(plist "$I" "$key:$i:UTTypeTagSpecification:public.filename-extension:$j")"; do
        [ "$e" = "$1" ] && { plist "$I" "$key:$i:UTTypeIdentifier"; return 0; }
        j=$((j+1))
      done
      e="$(plist "$I" "$key:$i:UTTypeTagSpecification:public.filename-extension")"
      [ "$e" = "$1" ] && { plist "$I" "$key:$i:UTTypeIdentifier"; return 0; }
      i=$((i+1))
    done
  done
  return 1
}
for e in $CLAIMED; do
  if u="$(utidecl "$e")"; then pass "plist.uti.$e" "$u"
  else fail "plist.uti.$e" "no UTExportedTypeDeclarations/UTImportedTypeDeclarations entry for .$e: macOS gives it a dyn.* type"; fi
done

# ===========================================================================
section "Resources"
# ===========================================================================
R="$C/Resources"
FM="$R/Fixtures/FixturesMap.xml"
if [ -s "$FM" ]; then
  nmap="$(grep -c '<F ' "$FM")"
  # Every map entry is a file (Fixtures/<maker>/<n>.qxf).
  perl -ne 'BEGIN{$m=""} $m=$1 if /<M n="([^"]*)"/; if (/<F n="([^"]*)"/) { my $n = $1; $n =~ s/&amp;/&/g; $n =~ s/&apos;/\x27/g; $n =~ s/&quot;/"/g; print "$m/$n.qxf\n" }' "$FM" > "$WORK/fm.list"
  miss="$(while IFS= read -r p; do [ -f "$R/Fixtures/$p" ] || echo "$p"; done < "$WORK/fm.list")"
  [ -z "$miss" ] && pass res.fixtures-map "$nmap definitions, all present" \
    || fail res.fixtures-map "$(echo "$miss" | wc -l | tr -d ' ') map entries with no file: $(echo "$miss" | head -5 | tr '\n' ' ')"
  [ "$nmap" -ge 500 ] || fail res.fixtures-count "only $nmap fixture definitions in the map"
else
  fail res.fixtures-map "no Resources/Fixtures/FixturesMap.xml"
fi
loose="$(find "$R/Fixtures" -maxdepth 1 -name '*.qxf' | sed "s|$R/||")"
[ -z "$loose" ] && pass res.fixtures-no-loose "every definition is in a maker folder, as the repo has them" \
  || fail res.fixtures-no-loose "$(echo "$loose" | wc -l | tr -d ' ') definitions loose in Fixtures/, not from the repo (a builder's user folder?): $(echo "$loose" | head -6 | tr '\n' ';')"
# Every resource file is the repo's, byte for byte: nothing from the
# builder's home folder (personal fixtures, test scripts, a newer copy of a
# stock script) rides along.
if [ -z "$MANIFEST" ] && [ -n "$REPO" ]; then MANIFEST="$WORK/resources.sha256"; make_manifest > "$MANIFEST"; fi
if [ -n "$MANIFEST" ] && [ -s "$MANIFEST" ]; then
  for pair in $RES_DIRS; do
    dst="${pair#*:}"
    [ -d "$R/$dst" ] || continue
    ( cd "$R" && find "$dst" -type f -print0 | xargs -0 shasum -a 256 ) > "$WORK/shipped.sha256"
    foreign="$(awk 'NR==FNR { want[$0] = 1; next } !($0 in want) { sub(/^[0-9a-f]+  /, ""); print }' "$MANIFEST" "$WORK/shipped.sha256")"
    n="$(grep -c . "$WORK/shipped.sha256")"
    [ -z "$foreign" ] && pass "res.from-repo.$dst" "$n files, all byte-identical to the repo's" \
      || fail "res.from-repo.$dst" "$(echo "$foreign" | wc -l | tr -d ' ') of $n files are not the repo's: $(echo "$foreign" | head -6 | tr '\n' ';')"
  done
else
  skip res.from-repo "no repo checkout and no --resource-manifest to compare with"
fi
dups="$(find "$R" -name '* [0-9].*' -o -name '* copy.*' -o -name '* copy [0-9]*.*' | sed "s|$R/||")"
[ -z "$dups" ] && pass res.no-finder-duplicates \
  || fail res.no-finder-duplicates "$(echo "$dups" | wc -l | tr -d ' ') Finder-duplicate files ship: $(echo "$dups" | head -6 | tr '\n' ';')"
check res.sample "no Resources/Sample.qxw" test -s "$R/Sample.qxw"
for d in RGBScripts EffectScripts EffectPresets InputProfiles MidiTemplates ModifiersTemplates Gobos Web; do
  n="$(find "$R/$d" -type f 2>/dev/null | wc -l | tr -d ' ')"
  [ "$n" -gt 0 ] && pass "res.dir.$d" "$n files" || fail "res.dir.$d" "Resources/$d missing or empty"
done
missl=""; for l in $EXPECT_LANGS; do [ -s "$R/Translations/qlcconsole_$l.qm" ] || missl="$missl $l"; done
[ -z "$missl" ] && pass res.translations "$EXPECT_LANGS" || fail res.translations "missing qlcconsole_<lang>.qm:$missl"
# The help is compiled in (ui/src/help.qrc): its names sit in libqlcplusui.
UI="$C/Frameworks/libqlcplusui.dylib"
if [ -f "$UI" ]; then
  missh="$(perl "$WORK/utf16.pl" "$UI" $EXPECT_HELP | awk '$1=="MISSING"{print $2}' | tr '\n' ' ')"
  [ -z "$missh" ] && pass res.help "$(echo $EXPECT_HELP | wc -w | tr -d ' ') guides compiled in" || fail res.help "guides not in libqlcplusui: $missh"
else
  fail res.help "no Frameworks/libqlcplusui.dylib"
fi

# ===========================================================================
section "LaunchServices registration (no app is launched)"
# ===========================================================================
# A copy, registered with lsregister, its claims read back, unregistered.
# In a temp dir LaunchServices marks it launch-disabled and never makes it a
# default handler; --ls-real puts it under $HOME (a clean machine) so the
# default handler can be checked too.
if [ "$LS_REAL" = 1 ]; then LSDIR="$(mktemp -d "$HOME/.qlc-relcheck.XXXXXX")"; else LSDIR="$WORK/ls"; mkdir -p "$LSDIR"; fi
LSDIR="$(cd "$LSDIR" && pwd -P)"
ditto "$APP" "$LSDIR/qlcconsole.app"
"$LSREGISTER" -f "$LSDIR/qlcconsole.app" && REGISTERED="$LSDIR/qlcconsole.app"
"$LSREGISTER" -dump 2>/dev/null | awk -v p="$LSDIR/qlcconsole.app" '
  index($0, "path:") == 1 && index($0, p " (") { f = 1 }
  f && /^-{20}/ { exit } f' > "$WORK/ls.rec"
if [ -s "$WORK/ls.rec" ]; then
  pass ls.registered "$(grep -m1 '^identifier:' "$WORK/ls.rec" | awk '{print $2}')"
  claimed="$(grep -m1 '^claimed UTIs:' "$WORK/ls.rec" | sed 's/^claimed UTIs: *//')"
  note "LaunchServices claims: $claimed"
  for e in $CLAIMED; do
    # A declared type is claimed by its UTI; an undeclared one as "dyn.* (.ext)".
    u="$(echo "$claimed" | tr ',' '\n' | sed -n "s/^ *\([^ ]*\) (\.$e)\$/\1/p" | head -1)"
    if [ -z "$u" ] && d="$(utidecl "$e")"; then
      echo "$claimed" | tr ',' '\n' | sed 's/^ *//; s/ .*//' | grep -qxF "$d" && u="$d"
    fi
    if [ -z "$u" ]; then fail "ls.claims.$e" "LaunchServices has no claim for .$e"
    else pass "ls.claims.$e" "$u"; fi
  done
else
  fail ls.registered "lsregister did not register the copy"
fi
mkdir -p "$WORK/samples"; for e in $CLAIMED; do : > "$WORK/samples/sample.$e"; done
cat > "$WORK/handlers.js" <<'JS'
ObjC.import('AppKit');
function run(argv) {
  var ws = $.NSWorkspace.sharedWorkspace, out = [];
  for (var i = 0; i < argv.length; i++) {
    var p = argv[i], url = $.NSURL.fileURLWithPath(p), ext = p.split('.').pop();
    var t = ws.typeOfFileError(p, null), def = ws.URLForApplicationToOpenURL(url);
    out.push([ext, t.isNil() ? '' : t.js, def.isNil() ? '' : def.path.js].join('\t'));
  }
  return out.join('\n');
}
JS
osascript -l JavaScript "$WORK/handlers.js" "$WORK"/samples/sample.* > "$WORK/handlers.txt" 2>"$WORK/handlers.err"
while IFS=$'\t' read -r e uti def; do
  [ -n "$e" ] || continue
  if [ "$LS_REAL" = 1 ] && [ "$e" = qlcc ]; then
    # Nothing else claims .qlcc on a clean machine: the copy must be it.
    if [ "$def" = "$LSDIR/qlcconsole.app" ]; then pass ls.default.qlcc "the registered copy opens .qlcc ($uti)"
    else fail ls.default.qlcc "the default handler for .qlcc is '${def:-none}', not the registered copy"; fi
  else
    note "default handler for .$e ($uti): ${def:-none}"
  fi
done < "$WORK/handlers.txt"
"$LSREGISTER" -u "$LSDIR/qlcconsole.app" >/dev/null 2>&1; REGISTERED=""

# ===========================================================================
section "Gatekeeper on a quarantined copy (spctl only, never launched)"
# ===========================================================================
xattr -w com.apple.quarantine "0081;$(printf '%x' "$(date +%s)");Safari;" "$LSDIR/qlcconsole.app"
gk="$(spctl -a -t exec -vv "$LSDIR/qlcconsole.app" 2>&1)"
if echo "$gk" | grep -q ': accepted' && echo "$gk" | grep -q 'source=Notarized Developer ID'; then
  pass gatekeeper.quarantined "accepted, Notarized Developer ID"
else
  fail gatekeeper.quarantined "$(echo "$gk" | tr '\n' ' ')"
fi
rm -rf "$LSDIR/qlcconsole.app"

# ===========================================================================
section "Headless runs (offscreen, own HOME and settings)"
# ===========================================================================
# A copy with an offscreen platform plugin (the bundle's own if it ships one),
# its quarantine stripped and re-signed ad hoc -- so it is NOT the signed
# app; the signed one was checked above.
RUNAPP="$WORK/run/qlcconsole.app"
mkdir -p "$WORK/run" "$WORK/home"
ditto "$APP" "$RUNAPP"; xattr -cr "$RUNAPP"
QTVER="$(plist "$C/Frameworks/QtCore.framework/Resources/Info.plist" CFBundleVersion)"
if [ ! -f "$RUNAPP/Contents/PlugIns/platforms/libqoffscreen.dylib" ]; then
  if [ -z "$OFFSCREEN" ]; then
    for c in "${OFFSCREEN_PLUGIN:-}" "${QTDIR:+$QTDIR/plugins/platforms/libqoffscreen.dylib}" \
             "$HOME/Qt-aqt/$QTVER/macos/plugins/platforms/libqoffscreen.dylib"; do
      [ -n "$c" ] && [ -f "$c" ] && { OFFSCREEN="$c"; break; }
    done
  fi
  if [ -n "$OFFSCREEN" ] && [ -f "$OFFSCREEN" ]; then
    pv="$(macho "$OFFSCREEN" | awk -F'\t' '$1=="DYLIB" && $5 ~ /QtGui.framework/ {print $6; exit}')"
    note "supplying $OFFSCREEN (built against QtGui $pv; the bundle has Qt $QTVER) to a re-signed copy"
    [ "$pv" = "$QTVER" ] || note "WARNING: offscreen plugin is Qt $pv, the bundle is Qt $QTVER"
    cp "$OFFSCREEN" "$RUNAPP/Contents/PlugIns/platforms/libqoffscreen.dylib"
  fi
fi
codesign --force --deep --sign - "$RUNAPP" >/dev/null 2>&1
RUN_OK=1
[ -f "$RUNAPP/Contents/PlugIns/platforms/libqoffscreen.dylib" ] || RUN_OK=0
canrun() { # <arch>
  case "$1" in
    arm64) [ "$(sysctl -n hw.optional.arm64 2>/dev/null)" = 1 ] ;;
    x86_64) [ "$(uname -m)" = x86_64 ] || /usr/bin/pgrep -q oahd || [ -e /Library/Apple/usr/share/rosetta/rosetta ] ;;
  esac
}
runapp() { # <arch> <timeout-s> <log> <args...>
  local arch="$1" t="$2" log="$3"; shift 3
  ( cd "$WORK/run" && env HOME="$WORK/home" CFFIXED_USER_HOME="$WORK/home" \
      QT_QPA_PLATFORM=offscreen QLC_HELP_SHOTS="$WORK/shots" QLC_HELP_SHOTS_ONLY=__relcheck_none__ \
      perl -e 'alarm shift; exec @ARGV' "$t" /usr/bin/arch "-$arch" \
      "$RUNAPP/Contents/MacOS/qlcconsole" -platform offscreen "$@" ) > "$log" 2>&1
}
if [ "$RUN_OK" = 0 ]; then
  for a in arm64 x86_64; do skip "run.version.$a" "no offscreen plugin to run with (--offscreen-plugin)"; done
  skip run.load-sample "no offscreen plugin"
else
  BUILDN=""
  for a in arm64 x86_64; do
    if ! canrun "$a"; then skip "run.version.$a" "this Mac cannot run $a"; continue; fi
    runapp "$a" 60 "$WORK/version.$a.log" --version; rc=$?
    line="$(grep -m1 '^qlcconsole ' "$WORK/version.$a.log")"
    if [ $rc = 0 ] && echo "$line" | grep -q "^qlcconsole $EXP_VERSION (build .*, $EXP_REV" ; then
      pass "run.version.$a" "$line"
      BUILDN="$(echo "$line" | sed -n 's/.*+qlcconsole\.\([0-9]*\),.*/\1/p')"
    else
      fail "run.version.$a" "exit $rc: ${line:-$(tail -2 "$WORK/version.$a.log" | tr '\n' ' ')} (want $EXP_VERSION, $EXP_REV)"
    fi
  done
  if [ -n "$BUILDN" ]; then
    [ "$PLIST_BUILD" = "$BUILDN" ] && pass plist.build-matches "CFBundleVersion $PLIST_BUILD = build $BUILDN" \
      || fail plist.build-matches "CFBundleVersion '$PLIST_BUILD' but the binary is build $BUILDN"
  fi
  # Load the bundled Sample.qxw (a copy) and quit: the help-pictures hook with
  # a filter that matches no picture quits as soon as the show is in, with
  # its own QSettings. -d 0 logs plugin and workspace loading.
  cp "$R/Sample.qxw" "$WORK/run/Sample.qxw"
  ARCH="$(uname -m)"; [ "$ARCH" = arm64 ] || ARCH=x86_64
  runapp "$ARCH" 180 "$WORK/load.log" -d 0 -o "$WORK/run/Sample.qxw"; rc=$?
  cp "$WORK/load.log" "$OUT/load-sample.log" 2>/dev/null
  if [ $rc = 0 ] && grep -q '^\[HELPSHOT\] done, 0 failed' "$WORK/load.log" && grep -q 'addWidgetInMap' "$WORK/load.log"; then
    pass run.load-sample "Sample.qxw loaded ($ARCH), exit 0"
  else
    fail run.load-sample "exit $rc$( [ $rc -ge 128 ] && echo ' (killed: timeout or crash)'); see $OUT/load-sample.log"
  fi
  miss=""
  for p in $IO_PLUGINS; do grep -q "lib$p.dylib\" loaded library\|from \"lib$p.dylib\"" "$WORK/load.log" || miss="$miss $p"; done
  [ -z "$miss" ] && pass run.io-plugins-loaded "$(echo $IO_PLUGINS | wc -w | tr -d ' ') I/O plugins" || fail run.io-plugins-loaded "not loaded:$miss"
  grep -q 'libsndfileplugin.dylib" loaded library\|Loaded audio decoder plugin' "$WORK/load.log" \
    && pass run.audio-plugin-loaded || fail run.audio-plugin-loaded "libsndfileplugin did not load"
  cannot="$(grep -i 'Cannot load library\|Library not loaded\|could not be loaded' "$WORK/load.log" | head -3 | tr '\n' ' ')"
  [ -z "$cannot" ] && pass run.no-load-errors || fail run.no-load-errors "$cannot"
  ff="$(grep -m1 'FFmpeg version' "$WORK/load.log")"; [ -n "$ff" ] && note "$ff"
fi

# ===========================================================================
section "Clean up"
# ===========================================================================
unregister_ours
# (The mounted volume itself keeps a record, as for any DMG anyone opens;
# lsregister cannot remove that one. The apps' records go.)
left="$("$LSREGISTER" -dump 2>/dev/null | grep -E "^path: +($WORK|${LSDIR:-$WORK})/.*\.app \(" | sed 's/^path: *//')"
[ -z "$left" ] && pass ls.unregistered "no LaunchServices record of the check's app copies is left" \
  || fail ls.unregistered "still registered: $(echo "$left" | tr '\n' ' ')"

# ===========================================================================
section "Crash reports"
# ===========================================================================
sleep 2
ls -1 "$DIAG" 2>/dev/null | sort > "$WORK/diag.after"
new="$(comm -13 "$WORK/diag.before" "$WORK/diag.after" | grep -i 'qlcconsole\|qlcplus' || true)"
if [ -z "$new" ]; then
  pass no-crash-reports
else
  fail no-crash-reports "the runs crashed: $(echo "$new" | tr '\n' ' ') (removed)"
  echo "$new" | while IFS= read -r f; do cp "$DIAG/$f" "$OUT/" 2>/dev/null; rm -f "$DIAG/$f"; done
fi

# ===========================================================================
emit "1..$N"
{ echo "["; sed '$ s/,$//' "$JSON.tmp"; echo "]"; } > "$JSON"; rm -f "$JSON.tmp"
echo
echo "==== qlcconsole release check: $DMGNAME on $(hostname -s) ===="
echo "$N checks: $((N - NFAIL - NWAIVED - NSKIP)) passed, $NFAIL failed, $NWAIVED waived, $NSKIP skipped"
[ -n "$FAILED_LIST" ] && echo "FAILED:$FAILED_LIST"
[ -n "$WAIVED_LIST" ] && echo "Waived (known, not blocking):$WAIVED_LIST"
echo "TAP: $TAP"
echo "JSON: $JSON"
[ "$NFAIL" = 0 ]
