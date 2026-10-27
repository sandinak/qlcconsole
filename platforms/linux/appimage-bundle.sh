#!/bin/bash
# Makes an AppImage staging tree (an AppDir, after `cmake --install` with
# -Dappimage=ON) self-contained, so the AppImage runs on a stock desktop
# Linux (Ubuntu 22.04 and later) with no Qt and none of qlcconsole's other
# libraries installed:
#
#   platforms/linux/appimage-bundle.sh <AppDir> <QTDIR>
#
#   1. Qt plugins: xcb, offscreen (headless runs: -platform offscreen) and
#      minimal; the xcb GL integrations; image formats (svg, jpeg, gif, ico);
#      the SVG icon engine; the FFmpeg multimedia backend; TLS; the compose /
#      ibus input contexts.
#   2. Every library any ELF file in the tree needs, copied into usr/lib,
#      until nothing new turns up -- except the base system every desktop
#      has (BASE below: glibc, libstdc++, GL/EGL/X11/xcb, fontconfig,
#      freetype, glib, dbus, ALSA/PulseAudio, libusb, libudev, ...; roughly
#      AppImage's excludelist, and what release-check.sh accepts from the
#      system). Qt's own libraries, its LGPL FFmpeg and FFmpeg's stubs,
#      libftdi1, FFTW, libmad, ICU... all come along.
#   3. RUNPATH $ORIGIN-relative on every ELF file: usr/bin -> ../lib,
#      plugins -> wherever usr/lib is from them. Nothing points at the build
#      machine, nothing depends on the current directory.
#
# Needs ldd, patchelf, readelf. Fails when a dependency is not found.
set -euo pipefail

APPDIR="$(cd "${1:?usage: $0 <AppDir> <QTDIR>}" && pwd)"
QTDIR="$(cd "${2:?usage: $0 <AppDir> <QTDIR>}" && pwd)"
U="$APPDIR/usr"
QP="$U/plugins"
command -v patchelf >/dev/null || { echo "appimage-bundle: patchelf is needed" >&2; exit 1; }

# What a desktop Linux has (keep in step with BASE in release-check.sh; this
# one bundles a few more: the xcb-util family and libxcb-xinerama, which a
# stock Ubuntu desktop can lack and Qt's xcb plugin needs).
BASE='^(ld-linux.*|linux-vdso|libc|libm|libdl|libpthread|librt|libresolv|libutil|libanl|libnsl|libstdc\+\+|libgcc_s|libatomic|libGL|libEGL|libGLX|libGLdispatch|libOpenGL|libGLESv2|libdrm|libgbm|libX11|libX11-xcb|libxcb|libxcb-(dri2|dri3|glx|present|sync|xfixes|shm|randr|render|shape|xkb)|libXau|libXdmcp|libXext|libXrender|libXrandr|libXfixes|libXi|libXcursor|libXinerama|libxkbcommon|libxkbcommon-x11|libSM|libICE|libfontconfig|libfreetype|libharfbuzz|libexpat|libz|libuuid|libdbus-1|libglib-2\.0|libgobject-2\.0|libgthread-2\.0|libgmodule-2\.0|libgio-2\.0|libasound|libpulse.*|libudev|libsystemd|libgpg-error|libgcrypt|libcap|libselinux|libpcre.*|libmount|libblkid|libffi|libbz2|liblzma|libzstd|liblz4|libbrotli.*|libpng16|libkeyutils|libkrb5.*|libk5crypto|libcom_err|libgssapi_krb5|libusb-1\.0|libsndfile|libFLAC|libogg|libvorbis.*|libopus|libmpg123|libmp3lame|libasyncns|libwrap|libapparmor)\.so'

say() { echo "appimage-bundle: $*"; }

# --- 1. Qt plugins -----------------------------------------------------------
plugin() { # <dir> <name>... : each must exist
  local d="$1" p; shift
  mkdir -p "$QP/$d"
  for p in "$@"; do
    [ -f "$QTDIR/plugins/$d/$p.so" ] || { echo "appimage-bundle: no $QTDIR/plugins/$d/$p.so" >&2; exit 1; }
    cp -fL "$QTDIR/plugins/$d/$p.so" "$QP/$d/"
  done
}
plugin platforms libqxcb libqoffscreen libqminimal
plugin xcbglintegrations libqxcb-glx-integration libqxcb-egl-integration
plugin imageformats libqsvg libqjpeg libqgif libqico
plugin iconengines libqsvgicon
plugin multimedia libffmpegmediaplugin
plugin tls libqopensslbackend libqcertonlybackend
plugin platforminputcontexts libcomposeplatforminputcontextplugin libibusplatforminputcontextplugin
say "Qt plugins: $(cd "$QP" && find . -name '*.so' | sort | tr '\n' ' ')"

# --- 2. Dependencies -----------------------------------------------------------
is_elf() { [ "$(head -c4 "$1" 2>/dev/null | od -An -c | tr -d ' ')" = '177ELF' ]; }
elfs() { find "$APPDIR" -type f \( -name '*.so*' -o -perm -u+x \) ! -path "$APPDIR/AppRun" | while IFS= read -r f; do is_elf "$f" && echo "$f"; done; }
round=0
while :; do
  round=$((round + 1))
  added=0; missing=""
  while IFS= read -r f; do
    while read -r name arrow path _; do
      [ "$arrow" = "=>" ] || continue
      if [ "$path" = not ]; then
        echo "$name" | grep -Eq "$BASE" || missing="$missing $name(${f#$APPDIR/})"
        continue
      fi
      case "$path" in "$APPDIR"/*) continue ;; esac
      echo "$name" | grep -Eq "$BASE" && continue
      [ -e "$U/lib/$name" ] && continue
      cp -L "$path" "$U/lib/$name"; chmod u+w "$U/lib/$name"
      say "  + $name (from $path, for ${f#$APPDIR/})"
      added=$((added + 1))
    done < <(LD_LIBRARY_PATH="$U/lib:$QTDIR/lib" ldd "$f" 2>/dev/null)
  done < <(elfs)
  [ -z "$missing" ] || { echo "appimage-bundle: not found:$missing" >&2; exit 1; }
  [ "$added" = 0 ] && break
  [ "$round" -lt 10 ] || { echo "appimage-bundle: dependencies still growing after 10 rounds" >&2; exit 1; }
done
say "dependencies complete after $round round(s)"

# --- 3. RUNPATH ----------------------------------------------------------------
n=0
while IFS= read -r f; do
  rel="$(realpath --relative-to="$(dirname "$f")" "$U/lib")"
  want='$ORIGIN'; [ "$rel" = . ] || want="\$ORIGIN/$rel"
  have="$(readelf -d "$f" 2>/dev/null | sed -n 's/.*(R\(UN\)\{0,1\}PATH).*\[\(.*\)\]/\2/p' | head -1)"
  # Only what has dependencies of its own needs one (a data-only ELF is fine).
  readelf -d "$f" 2>/dev/null | grep -q '(NEEDED)' || continue
  if [ "$have" != "$want" ]; then
    chmod u+w "$f"
    patchelf --set-rpath "$want" "$f"
    n=$((n + 1))
  fi
done < <(elfs)
say "RUNPATH set on $n file(s)"

# Anything left that is not $ORIGIN-relative fails here, not on a user's machine.
bad=""
while IFS= read -r f; do
  for p in $(readelf -d "$f" 2>/dev/null | sed -n 's/.*(R\(UN\)\{0,1\}PATH).*\[\(.*\)\]/\2/p' | tr ':' ' '); do
    case "$p" in '$ORIGIN'*) ;; *) bad="$bad ${f#$APPDIR/}:$p" ;; esac
  done
done < <(elfs)
[ -z "$bad" ] || { echo "appimage-bundle: RUNPATH not \$ORIGIN-relative:$bad" >&2; exit 1; }
say "done: $(elfs | wc -l) ELF files, $(ls "$U/lib" | wc -l) entries in usr/lib"
