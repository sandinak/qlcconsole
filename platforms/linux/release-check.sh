#!/bin/bash
# Pre-release check of the Linux AppImage: proves the AppImage and what it
# runs are correct before anything is published.
#
#   platforms/linux/release-check.sh <AppImage> [options]
#
# Options:
#   --host <ssh-host>      run it on another Linux machine (copies this
#                          script, the AppImage and the offscreen plugin to a
#                          temp dir there, runs, copies the results back,
#                          removes the temp dir)
#   --sums <file>          SHA256SUMS to check it against (optional)
#   --version <x.y.z>      expected release (default: the repo's VERSION)
#   --rev <sha>            expected git rev (default: from the file name)
#   --offscreen-plugin <f> libqoffscreen.so (same Qt as the AppImage) for the
#                          headless runs when the AppImage ships none
#                          (default: $QTDIR/plugins/platforms/)
#   --out <dir>            where the TAP / JSON results go
#   --waive <id,id,...>    known failures to report as TODO, not fail
#                          (also RELEASE_CHECK_WAIVE)
#
# It never opens a window: every run is QT_QPA_PLATFORM=offscreen with
# -platform offscreen, with HOME and the XDG dirs pointed at a temp dir, so
# the user's settings and data are not touched. It extracts the AppImage
# (--appimage-extract, no FUSE needed) into a temp dir and removes it.
#
# Output: TAP on stdout and <out>/release-check.tap, JSON in
# <out>/release-check.json, a summary. Exit 0 = all passed (waived aside),
# 1 = a check failed, 2 = could not run.
set -u
export LC_ALL=C.UTF-8 2>/dev/null || export LC_ALL=C

SCRIPTDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPTDIR/../.." 2>/dev/null && pwd)"
[ -f "$REPO/VERSION" ] || REPO=""

die() { echo "release-check: $*" >&2; exit 2; }

AI="" HOST="" SUMS="" EXP_VERSION="" EXP_REV="" OFFSCREEN="" OUT=""
WAIVE="${RELEASE_CHECK_WAIVE:-}"
while [ $# -gt 0 ]; do
  case "$1" in
    --host) HOST="$2"; shift 2 ;;
    --sums) SUMS="$2"; shift 2 ;;
    --version) EXP_VERSION="$2"; shift 2 ;;
    --rev) EXP_REV="$2"; shift 2 ;;
    --offscreen-plugin) OFFSCREEN="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --waive) WAIVE="${WAIVE:+$WAIVE,}$2"; shift 2 ;;
    -h|--help) sed -n '2,/^set -u/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;;
    -*) die "unknown option $1" ;;
    *) [ -z "$AI" ] || die "one AppImage only"; AI="$1"; shift ;;
  esac
done
[ -n "$AI" ] || die "usage: $0 <AppImage> [--host <ssh-host>] (--help)"
[ -f "$AI" ] || die "no such file: $AI"
AI="$(cd "$(dirname "$AI")" && pwd)/$(basename "$AI")"
AINAME="$(basename "$AI")"
[ -n "$EXP_VERSION" ] || { [ -n "$REPO" ] && EXP_VERSION="$(tr -d '\r\n' < "$REPO/VERSION")"; }
[ -n "$EXP_VERSION" ] || EXP_VERSION="$(echo "$AINAME" | sed -n 's/^qlcconsole-\([0-9][0-9.]*\)-.*/\1/p')"
[ -n "$EXP_REV" ] || EXP_REV="$(echo "$AINAME" | sed -n 's/.*-\([0-9a-f]\{7,40\}\)\.AppImage$/\1/p')"
[ -n "$OFFSCREEN" ] || { [ -n "${QTDIR:-}" ] && [ -f "$QTDIR/plugins/platforms/libqoffscreen.so" ] && OFFSCREEN="$QTDIR/plugins/platforms/libqoffscreen.so"; }
if [ -z "${EXPECT_HELP:-}" ] && [ -n "$REPO" ] && [ -f "$REPO/ui/src/help.qrc" ]; then
  EXPECT_HELP="$(sed -n 's/.*alias="help\/\([^"]*\)".*/\1/p' "$REPO/ui/src/help.qrc" | tr '\n' ' ')"
fi
EXPECT_HELP="${EXPECT_HELP:-getting-started.md whats-new.md README.md updates.md}"
if [ -z "${EXPECT_LANGS:-}" ] && [ -n "$REPO" ] && [ -f "$REPO/translate.sh" ]; then
  EXPECT_LANGS="$(sed -n 's/^UI_LANGS="\(.*\)"$/\1/p' "$REPO/translate.sh")"
fi
EXPECT_LANGS="${EXPECT_LANGS:-de_DE es_ES fr_FR it_IT nl_NL cz_CZ pt_BR ca_ES ja_JP}"
IO_PLUGINS="${IO_PLUGINS:-artnet dmxusb e131 enttecwing hidplugin loopback midiplugin os2l osc peperoni udmx wled}"

# ---------------------------------------------------------------------------
# --host
# ---------------------------------------------------------------------------
if [ -n "$HOST" ]; then
  SSH=(ssh -o BatchMode=yes -o ConnectTimeout=10 "$HOST")
  "${SSH[@]}" true || die "cannot ssh to $HOST"
  [ -n "$OUT" ] || OUT="${REPO:+$REPO/build-release-check}"
  [ -n "$OUT" ] || OUT="$(mktemp -d)"
  OUT="$OUT/$(echo "$HOST" | tr -c 'A-Za-z0-9._-\n' _)"; mkdir -p "$OUT"
  R="$("${SSH[@]}" 'mktemp -d /tmp/qlc-relcheck.XXXXXX')" || die "no temp dir on $HOST"
  case "$R" in /tmp/qlc-relcheck.*) ;; *) die "odd remote temp dir '$R'" ;; esac
  cleanup_remote() { "${SSH[@]}" "rm -rf '$R'" && echo "# $HOST: removed $R" || echo "# WARNING: could not remove $R on $HOST" >&2; }
  trap cleanup_remote EXIT
  files=("$0" "$AI"); [ -n "$OFFSCREEN" ] && files+=("$OFFSCREEN")
  [ -n "$SUMS" ] && files+=("$SUMS")
  scp -q -o BatchMode=yes "${files[@]}" "$HOST:$R/" || die "copy to $HOST failed"
  args=("$R/$AINAME" --version "$EXP_VERSION" --rev "$EXP_REV" --out "$R/out")
  [ -n "$OFFSCREEN" ] && args+=(--offscreen-plugin "$R/$(basename "$OFFSCREEN")")
  [ -n "$SUMS" ] && args+=(--sums "$R/$(basename "$SUMS")")
  [ -n "$WAIVE" ] && args+=(--waive "$WAIVE")
  q=""; for a in "${args[@]}"; do q="$q '$a'"; done
  "${SSH[@]}" "EXPECT_HELP='$EXPECT_HELP' EXPECT_LANGS='$EXPECT_LANGS' IO_PLUGINS='$IO_PLUGINS' bash '$R/$(basename "$0")' $q"
  rc=$?
  scp -q -o BatchMode=yes "$HOST:$R/out/*" "$OUT/" 2>/dev/null || true
  echo "# results copied to $OUT"
  exit $rc
fi

# ---------------------------------------------------------------------------
[ "$(uname -s)" = Linux ] || die "Linux only"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/qlc-relcheck.XXXXXX")" || die "mktemp failed"
[ -n "$OUT" ] || { if [ -n "$REPO" ] && [ -w "$REPO" ]; then OUT="$REPO/build-release-check"; else OUT="$WORK/out"; fi; }
mkdir -p "$OUT" || die "cannot make $OUT"
TAP="$OUT/release-check.tap"; JSON="$OUT/release-check.json"
: > "$TAP"; : > "$JSON.tmp"
cleanup() {
  # Anything still running from our tree goes too.
  pkill -f "^$WORK/" 2>/dev/null
  rm -rf "$WORK"
}
trap cleanup EXIT
trap 'exit 2' INT TERM

N=0 NFAIL=0 NWAIVED=0 NSKIP=0 FAILED_LIST="" WAIVED_LIST=""
jesc() { printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g' | tr '\n\t' '  '; }
record() { printf '  {"id":"%s","status":"%s","detail":"%s"},\n' "$(jesc "$2")" "$1" "$(jesc "$3")" >> "$JSON.tmp"; }
emit() { echo "$1" | tee -a "$TAP"; }
waived() { case ",$WAIVE," in *",$1,"*) return 0 ;; esac; return 1; }
pass() { N=$((N+1)); emit "ok $N - $1${2:+ # $2}"; record pass "$1" "${2:-}"; }
fail() {
  N=$((N+1))
  if waived "$1"; then NWAIVED=$((NWAIVED+1)); WAIVED_LIST="$WAIVED_LIST
  $1: ${2:-}"; emit "not ok $N - $1 # TODO waived: ${2:-}"; record waived "$1" "${2:-}"
  else NFAIL=$((NFAIL+1)); FAILED_LIST="$FAILED_LIST
  $1: ${2:-}"; emit "not ok $N - $1${2:+ # $2}"; record fail "$1" "${2:-}"; fi
}
skip() { N=$((N+1)); NSKIP=$((NSKIP+1)); emit "ok $N - $1 # SKIP ${2:-}"; record skip "$1" "${2:-}"; }
note() { emit "# $*"; }
section() { emit "# --- $* ---"; }
check() { local id="$1" why="$2"; shift 2; if "$@" >/dev/null 2>&1; then pass "$id"; else fail "$id" "$why"; fi; }

note "qlcconsole AppImage release check: $AINAME"
note "host $(hostname) $(. /etc/os-release 2>/dev/null; echo "${PRETTY_NAME:-?}") $(uname -m); expect version=$EXP_VERSION rev=${EXP_REV:-any}"

# ===========================================================================
section "AppImage"
# ===========================================================================
if [ -n "$SUMS" ]; then
  want="$(awk -v n="$AINAME" '$2 == n || $2 == "*"n {print $1}' "$SUMS" | head -1)"
  have="$(sha256sum "$AI" | awk '{print $1}')"
  if [ -z "$want" ]; then fail appimage.sha256sums "no line for $AINAME"
  elif [ "$want" = "$have" ]; then pass appimage.sha256sums "$have"
  else fail appimage.sha256sums "SHA256SUMS says $want, the file is $have"; fi
else
  skip appimage.sha256sums "no --sums"
fi
check appimage.elf-x86_64 "not an x86-64 ELF" sh -c "file -b '$AI' | grep -q 'ELF 64-bit.*x86-64'"
cp "$AI" "$WORK/q.AppImage"; chmod +x "$WORK/q.AppImage"
( cd "$WORK" && ./q.AppImage --appimage-extract > "$WORK/extract.log" 2>&1 ); rc=$?
D="$WORK/squashfs-root"
if [ $rc = 0 ] && [ -x "$D/AppRun" ]; then
  pass appimage.extract "$(find "$D" -type f | wc -l) files"
else
  fail appimage.extract "--appimage-extract exit $rc: $(tail -2 "$WORK/extract.log" | tr '\n' ' ')"
  echo "cannot go on without the AppImage's contents" >&2; exit 1
fi
rm -f "$WORK/q.AppImage"

# ===========================================================================
section "ELF dependencies"
# ===========================================================================
# As AppRun sets it up.
LDP="$D/usr/lib/:$D/usr/lib/i386-linux-gnu/:$D/usr/lib/x86_64-linux-gnu/:$D/usr/lib32/:$D/usr/lib64/:$D/lib/:$D/lib/i386-linux-gnu/:$D/lib/x86_64-linux-gnu/:$D/lib32/:$D/lib64/"
# What any desktop Linux can be assumed to have (roughly AppImage's
# excludelist): anything else from outside the AppImage is a dependency a
# user's machine may lack.
BASE='^(ld-linux.*|linux-vdso|libc|libm|libdl|libpthread|librt|libresolv|libutil|libanl|libnsl|libstdc\+\+|libgcc_s|libatomic|libGL|libEGL|libGLX|libGLdispatch|libOpenGL|libGLESv2|libdrm|libgbm|libX11|libX11-xcb|libxcb.*|libXau|libXdmcp|libXext|libXrender|libXrandr|libXfixes|libXi|libXcursor|libXinerama|libxkbcommon|libxkbcommon-x11|libSM|libICE|libfontconfig|libfreetype|libharfbuzz|libexpat|libz|libuuid|libdbus-1|libglib-2\.0|libgobject-2\.0|libgthread-2\.0|libgmodule-2\.0|libgio-2\.0|libasound|libpulse.*|libudev|libsystemd|libgpg-error|libgcrypt|libcap|libselinux|libpcre.*|libmount|libblkid|libffi|libbz2|liblzma|libzstd|liblz4|libbrotli.*|libpng16|libkeyutils|libkrb5.*|libk5crypto|libcom_err|libgssapi_krb5|libusb-1\.0|libsndfile|libFLAC|libogg|libvorbis.*|libopus|libmpg123|libmp3lame|libasyncns|libwrap|libapparmor)\.so'
: > "$WORK/deps.txt"
nelf=0
while IFS= read -r f; do
  [ "$(head -c4 "$f" 2>/dev/null | od -An -c | tr -d ' ')" = '177ELF' ] || continue
  nelf=$((nelf+1))
  LD_LIBRARY_PATH="$LDP" ldd "$f" 2>/dev/null | while read -r a b c _; do
    rel="${f#$D/}"
    if [ "$b" = "=>" ]; then
      if [ "$c" = not ]; then echo "MISSING	$rel	$a"
      elif [ "${c#$D/}" = "$c" ]; then echo "SYSTEM	$rel	$a	$c"; fi
    fi
  done >> "$WORK/deps.txt"
  if command -v readelf >/dev/null; then
    readelf -d "$f" 2>/dev/null | sed -n 's/.*(R\(UN\)\{0,1\}PATH).*\[\(.*\)\]/\2/p' | tr ':' '\n' | while read -r p; do
      case "$p" in '$ORIGIN'*|'${ORIGIN}'*|'') ;; *) echo "RPATH	${f#$D/}	$p" ;; esac
    done >> "$WORK/deps.txt"
  fi
done < <(find "$D" -type f \( -perm -u+x -o -name '*.so*' \))
note "$nelf ELF files"
miss="$(awk -F'\t' '$1=="MISSING"{print $3" (by "$2")"}' "$WORK/deps.txt" | sort -u)"
[ -z "$miss" ] && pass elf.deps-found "every dependency resolves" \
  || fail elf.deps-found "$(echo "$miss" | wc -l) not found: $(echo "$miss" | head -8 | tr '\n' ';')"
sysqt="$(awk -F'\t' '$1=="SYSTEM" && $3 ~ /^libQt/ {print $3" (by "$2")"}' "$WORK/deps.txt" | sort -u)"
[ -z "$sysqt" ] && pass elf.no-system-qt "no Qt library comes from the system" \
  || fail elf.no-system-qt "Qt libraries resolved from the system, not the AppImage (mixes Qt versions): $(echo "$sysqt" | head -8 | tr '\n' ';')"
extra="$(awk -F'\t' '$1=="SYSTEM"{print $3}' "$WORK/deps.txt" | sort -u | grep -Ev "$BASE" | grep -v '^libQt')"
if [ -z "$extra" ]; then
  pass elf.deps-bundled "everything beyond the base system is bundled"
else
  who="$(awk -F'\t' '$1=="SYSTEM"{print $3"\t"$2}' "$WORK/deps.txt" | grep -Ev "$BASE" | awk -F'\t' '{print $2}' | sort | uniq -c | sort -rn | head -6 | awk '{print $2"("$1")"}' | tr '\n' ' ')"
  fail elf.deps-bundled "$(echo "$extra" | wc -l) libraries taken from this machine, not bundled (missing on a machine without them): $(echo "$extra" | head -15 | tr '\n' ' ')... needed by: $who"
fi
rp="$(awk -F'\t' '$1=="RPATH"{print $2": "$3}' "$WORK/deps.txt" | sort -u)"
if ! command -v readelf >/dev/null; then skip elf.rpath "no readelf"
elif [ -z "$rp" ]; then pass elf.rpath "only \$ORIGIN-relative RPATH/RUNPATH"
else fail elf.rpath "RPATH/RUNPATH not relative to \$ORIGIN (a CWD-relative one loads libraries from wherever the app is started; an absolute one points at the build machine): $(echo "$rp" | head -6 | tr '\n' ';')"; fi
bp="$(grep -l -a -E '/home/runner/work|/opt/Qt/[0-9]' "$D/usr/bin/qlcconsole" "$D"/usr/lib/libqlcplus*.so 2>/dev/null | sed "s|$D/||" | tr '\n' ' ')"
[ -n "$bp" ] && note "build-machine paths appear as strings in: $bp (sources' __FILE__ etc.; not loaded from)"

# ===========================================================================
section "Desktop integration"
# ===========================================================================
DESK="$(ls "$D"/*.desktop 2>/dev/null | head -1)"
if [ -n "$DESK" ]; then
  pass desktop.file "$(basename "$DESK")"
  if command -v desktop-file-validate >/dev/null; then
    v="$(desktop-file-validate "$DESK" 2>&1 | grep -v 'hint:')"
    [ -z "$v" ] && pass desktop.validate || fail desktop.validate "$(echo "$v" | head -3 | tr '\n' ' ')"
  else
    skip desktop.validate "no desktop-file-validate"
  fi
  icon="$(sed -n 's/^Icon=//p' "$DESK" | head -1)"
  if [ -n "$icon" ] && ls "$D/$icon".* >/dev/null 2>&1 && [ -e "$D/.DirIcon" ]; then
    pass desktop.icon "$(cd "$D" && ls "$icon".* | tr '\n' ' ').DirIcon"
  else
    fail desktop.icon "Icon=$icon has no $icon.png/.svg (or no .DirIcon) at the AppImage root"
  fi
  exec_line="$(sed -n 's/^Exec=//p' "$DESK" | head -1)"
  note "Exec=$exec_line"
  # AppRun runs Exec's words, then the AppImage's arguments: any word before
  # them other than the program swallows the first one.
  set -- $exec_line
  shift
  pre=""; for w in "$@"; do case "$w" in %*) ;; *) pre="$pre $w" ;; esac; done
  [ -z "$pre" ] && pass desktop.exec-args "Exec= adds nothing before the user's arguments" \
    || fail desktop.exec-args "Exec= puts '${pre# }' before every argument: './qlcconsole.AppImage --version' runs 'qlcconsole${pre} --version' (the app starts instead of printing its version; -o FILE opens '-o')"
  mt="$(sed -n 's/^MimeType=//p' "$DESK" | tr ';' ' ')"
  [ -n "$mt" ] && pass desktop.mimetypes "$mt" || fail desktop.mimetypes "no MimeType= in $(basename "$DESK")"
else
  fail desktop.file "no .desktop file at the AppImage root"; mt=""
fi
MIMEXML="$(ls "$D"/usr/share/mime/packages/*.xml 2>/dev/null)"
for desk in "$D"/usr/share/applications/*.desktop; do
  [ -f "$desk" ] || continue
  for m in $(sed -n 's/^MimeType=//p' "$desk" | tr ';' ' '); do mt="$mt $m"; done
done
allglobs=""
for m in $(echo "$mt" | tr ' ' '\n' | sort -u); do
  [ -n "$m" ] || continue
  g="$(cat $MIMEXML /dev/null 2>/dev/null | awk -v m="$m" '$0 ~ "mime-type type=\""m"\"" {f=1} f && /<glob/ {match($0, /pattern="[^"]*"/); print substr($0, RSTART+9, RLENGTH-10)} f && /<\/mime-type>/ {f=0}' | tr '\n' ' ')"
  if [ -n "$g" ]; then pass "mime.$m" "$g"; allglobs="$allglobs $g"
  else fail "mime.$m" "claimed by a .desktop file but not defined in usr/share/mime/packages"; fi
done
for e in qlcc qxw qxf; do
  case " $allglobs " in *"*.$e "*) pass "mime.glob.$e" ;; *) fail "mime.glob.$e" "no MIME type the AppImage claims matches *.$e" ;; esac
done
ls "$D"/usr/share/metainfo/*.xml >/dev/null 2>&1 && pass desktop.metainfo || fail desktop.metainfo "no AppStream metainfo"

# ===========================================================================
section "Plugins and data"
# ===========================================================================
QP="$D/usr/plugins"
for q in platforms/libqxcb xcbglintegrations/libqxcb-glx-integration imageformats/libqsvg imageformats/libqjpeg \
         imageformats/libqgif imageformats/libqico iconengines/libqsvgicon multimedia/libffmpegmediaplugin; do
  check "plugin.qt.$(basename "$q" | sed 's/^libq*//')" "missing usr/plugins/$q.so" test -f "$QP/$q.so"
done
ls "$QP"/platforms/libqwayland* >/dev/null 2>&1 || note "no Wayland platform plugin: Wayland sessions run it through XWayland"
if [ -f "$QP/platforms/libqoffscreen.so" ]; then pass plugin.qt.offscreen "shipped"
else fail plugin.qt.offscreen "no usr/plugins/platforms/libqoffscreen.so: it cannot run headless (-platform offscreen) as shipped"; fi
IOD="$(find "$D" -type d -path '*plugins/qlcconsole' | head -1)"
for p in $IO_PLUGINS; do check "plugin.io.$p" "missing lib$p.so" test -f "$IOD/lib$p.so"; done
SH="$(find "$D" -type d -path '*share/qlcconsole' | head -1)"
FM="$SH/fixtures/FixturesMap.xml"
if [ -s "$FM" ]; then
  nmap="$(grep -c '<F ' "$FM")"
  perl -ne 'BEGIN{$m=""} $m=$1 if /<M n="([^"]*)"/; if (/<F n="([^"]*)"/) { my $n = $1; $n =~ s/&amp;/&/g; $n =~ s/&apos;/\x27/g; $n =~ s/&quot;/"/g; print "$m/$n.qxf\n" }' "$FM" > "$WORK/fm.list"
  missf="$(while IFS= read -r p; do [ -f "$SH/fixtures/$p" ] || echo "$p"; done < "$WORK/fm.list")"
  [ -z "$missf" ] && pass res.fixtures-map "$nmap definitions, all present" \
    || fail res.fixtures-map "$(echo "$missf" | wc -l) map entries with no file: $(echo "$missf" | head -5 | tr '\n' ' ')"
else
  fail res.fixtures-map "no share/qlcconsole/fixtures/FixturesMap.xml"
fi
check res.sample "no share/qlcconsole/Sample.qxw" test -s "$SH/Sample.qxw"
for d in rgbscripts effectscripts inputprofiles web gobos; do
  n="$(find "$SH/$d" -type f 2>/dev/null | wc -l)"; [ "$n" -gt 0 ] && pass "res.dir.$d" "$n files" || fail "res.dir.$d" "missing or empty"
done
missl=""; for l in $EXPECT_LANGS; do [ -s "$SH/translations/qlcconsole_$l.qm" ] || missl="$missl $l"; done
[ -z "$missl" ] && pass res.translations "$EXPECT_LANGS" || fail res.translations "missing qlcconsole_<lang>.qm:$missl"
dups="$(find "$D" -name '* [0-9].*' -o -name '* copy.*' | sed "s|$D/||")"
[ -z "$dups" ] && pass res.no-duplicates || fail res.no-duplicates "$(echo "$dups" | head -5 | tr '\n' ';')"
UI="$D/usr/lib/libqlcplusui.so"
if [ -f "$UI" ]; then
  missh="$(perl -e 'my $f = shift; local $/; open(my $h, "<:raw", $f) or exit 2; my $d = <$h>;
    for my $s (@ARGV) { my $u = join("", map { "\0$_" } split //, $s); print "$s " if index($d, $u) < 0 }' "$UI" $EXPECT_HELP)"
  [ -z "$missh" ] && pass res.help "$(echo $EXPECT_HELP | wc -w) guides compiled in" || fail res.help "not in libqlcplusui: $missh"
else
  fail res.help "no usr/lib/libqlcplusui.so"
fi
av="$(ls "$D"/usr/lib/libavcodec.so* 2>/dev/null | head -1)"
[ -n "$av" ] || av="$(awk -F'\t' '$1=="SYSTEM" && $3 ~ /^libavcodec/ {print $4; exit}' "$WORK/deps.txt")"
if [ -n "$av" ]; then
  conf="$(strings "$av" 2>/dev/null | grep -m1 -- '--prefix=\|--enable-' )"
  case "$av" in "$D"/*) where="bundled" ;; *) where="from this machine: $av" ;; esac
  if echo "$conf" | grep -q -- '--enable-gpl'; then fail ffmpeg.licence "FFmpeg ($where) is built --enable-gpl"
  else pass ffmpeg.licence "LGPL ($where)"; fi
  # (L)GPL code ships with its licence text.
  want="GNU LESSER GENERAL PUBLIC LICENSE"; echo "$conf" | grep -q -- '--enable-gpl' && want="GNU GENERAL PUBLIC LICENSE"
  if grep -rlq "$want" "$SH" 2>/dev/null; then pass ffmpeg.licence-text "$want shipped"
  else fail ffmpeg.licence-text "FFmpeg ships without the $want text"; fi
fi

# ===========================================================================
section "Headless runs (offscreen, own HOME)"
# ===========================================================================
added=""
if [ ! -f "$QP/platforms/libqoffscreen.so" ] && [ -n "$OFFSCREEN" ] && [ -f "$OFFSCREEN" ]; then
  cp "$OFFSCREEN" "$QP/platforms/libqoffscreen.so"; added=1
  note "supplied $OFFSCREEN to the extracted copy"
fi
mkdir -p "$WORK/home" "$WORK/run"
runapp() { # <timeout> <log> <command...>: exit 124 on a timeout
  local t="$1" log="$2"; shift 2
  ( cd "$WORK/run" && exec env -u DISPLAY -u WAYLAND_DISPLAY HOME="$WORK/home" XDG_CONFIG_HOME="$WORK/home/.config" \
      XDG_DATA_HOME="$WORK/home/.local/share" XDG_CACHE_HOME="$WORK/home/.cache" XDG_RUNTIME_DIR="$WORK/home/run" \
      QT_QPA_PLATFORM=offscreen timeout -k 10 "$t" "$@" ) > "$log" 2>&1
}
mkdir -p "$WORK/home/run"; chmod 700 "$WORK/home/run"
if [ -f "$QP/platforms/libqoffscreen.so" ]; then
  # The binary, with AppRun's environment ...
  runapp 60 "$WORK/version.log" env LD_LIBRARY_PATH="$LDP" sh -c "cd '$D/usr' && exec ./bin/qlcconsole --version -platform offscreen"; rc=$?
  line="$(grep -m1 '^qlcconsole ' "$WORK/version.log")"
  if [ $rc = 0 ] && echo "$line" | grep -q "^qlcconsole $EXP_VERSION (build .*, ${EXP_REV}"; then pass run.version "$line"
  else fail run.version "exit $rc: '${line}' (want $EXP_VERSION ${EXP_REV})"; fi
  # ... and through AppRun, the way a user runs the AppImage.
  runapp 60 "$WORK/apprun-version.log" "$D/AppRun" --version -platform offscreen; rc=$?
  if [ $rc = 0 ] && grep -q "^qlcconsole $EXP_VERSION " "$WORK/apprun-version.log"; then pass run.apprun-version "exits after printing the version"
  else fail run.apprun-version "'qlcconsole.AppImage --version' did not exit (exit $rc after 60 s): the app started instead"; fi
  # Load the bundled Sample.qxw (a copy) and quit through the help-pictures
  # hook (its own QSettings; a filter that matches no picture). The file
  # comes first, as a file manager passes it: AppRun puts Exec='s --open
  # before it.
  cp "$SH/Sample.qxw" "$WORK/run/Sample.qxw"
  runapp 240 "$WORK/load.log" env QLC_HELP_SHOTS="$WORK/shots" QLC_HELP_SHOTS_ONLY=__relcheck_none__ \
    "$D/AppRun" "$WORK/run/Sample.qxw" -platform offscreen -d 0; rc=$?
  cp "$WORK/load.log" "$OUT/load-sample.log"
  if [ $rc = 0 ] && grep -q '^\[HELPSHOT\] done, 0 failed' "$WORK/load.log" && grep -q 'addWidgetInMap' "$WORK/load.log"; then
    pass run.load-sample "Sample.qxw loaded through AppRun, exit 0"
  else
    fail run.load-sample "exit $rc; see $OUT/load-sample.log"
  fi
  missp=""; for p in $IO_PLUGINS; do grep -q "from \"lib$p.so\"" "$WORK/load.log" || missp="$missp $p"; done
  [ -z "$missp" ] && pass run.io-plugins-loaded "$(echo $IO_PLUGINS | wc -w) I/O plugins" || fail run.io-plugins-loaded "not loaded:$missp"
  ff="$(grep -m1 'FFmpeg version' "$WORK/load.log")"
  [ -n "$ff" ] && pass run.multimedia "$ff" || fail run.multimedia "the FFmpeg multimedia backend did not load (see $OUT/load-sample.log)"
else
  skip run.version "no offscreen plugin (--offscreen-plugin)"; skip run.load-sample "no offscreen plugin"
fi
core="$(find "$WORK/run" "$WORK/home" -maxdepth 2 -name 'core*' -type f 2>/dev/null | head -1)"
[ -z "$core" ] && pass no-core-dumps || fail no-core-dumps "$core"

emit "1..$N"
{ echo "["; sed '$ s/,$//' "$JSON.tmp"; echo "]"; } > "$JSON"; rm -f "$JSON.tmp"
echo
echo "==== qlcconsole release check: $AINAME on $(hostname) ===="
echo "$N checks: $((N - NFAIL - NWAIVED - NSKIP)) passed, $NFAIL failed, $NWAIVED waived, $NSKIP skipped"
[ -n "$FAILED_LIST" ] && echo "FAILED:$FAILED_LIST"
[ -n "$WAIVED_LIST" ] && echo "Waived (known, not blocking):$WAIVED_LIST"
echo "TAP: $TAP"
echo "JSON: $JSON"
[ "$NFAIL" = 0 ]
