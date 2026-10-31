#!/bin/bash
# Builds the qlcconsole .deb for Debian 13 (trixie) / Raspberry Pi OS, arm64
# (also works on a trixie amd64 box). Decided (can change): a .deb, not an
# AppImage -- the Pi's own apt carries Qt 6.8.2, the package can depend on it,
# apt removes it cleanly, and udev rules / MIME / icons go where the system
# looks. The x86_64 AppImage (appimage-bundle.sh) stays the generic Linux build.
#
#   platforms/linux/make-pi-deb.sh --remote rpi5 [--rev REV] [--jobs 3]
#       bundles the committed REV (default HEAD), builds it on HOST in
#       ~/pkgbuild/<rev> (never the host's own checkout), copies the .deb to
#       dist/ here. nice -n 10; refuses to start while a cmake/ninja runs there.
#   platforms/linux/make-pi-deb.sh --local <srcdir> [--jobs N] [--out DIR]
#       what the remote mode runs on the Pi: configure (Release, -Werror off
#       the dev default), build, `cmake --install` into a staging root, and
#       dpkg-deb. Depends: are computed by dpkg-shlibdeps from the real
#       binaries, plus the Qt platform/image plugins a bare apt Qt may lack.
#
# Version: <VERSION>+git<N>.<sha>  (VERSION = the release file at the repo
# root, N = commits since the QLC+_4.14.2 fork point, sha = short git rev).
# Nothing is published; the result is dist/qlcconsole_<version>_<arch>.deb.
set -euo pipefail

JOBS=3 REMOTE="" REV=HEAD SRC="" OUT=""
while [ $# -gt 0 ]; do
  case "$1" in
    --remote) REMOTE="$2"; shift 2;;
    --rev)    REV="$2"; shift 2;;
    --jobs)   JOBS="$2"; shift 2;;
    --local)  SRC="$2"; shift 2;;
    --out)    OUT="$2"; shift 2;;
    *) echo "usage: $0 --remote HOST [--rev REV] [--jobs N] | --local SRCDIR [--out DIR]" >&2; exit 2;;
  esac
done

if [ -n "$REMOTE" ]; then
  HERE="$(cd "$(dirname "$0")/../.." && pwd)"
  SHA="$(git -C "$HERE" rev-parse --short "$REV")"
  TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
  git -C "$HERE" update-ref refs/pideb/rev "$SHA"
  git -C "$HERE" bundle create -q "$TMP/src.bundle" refs/pideb/rev refs/tags/QLC+_4.14.2 2>/dev/null \
    || git -C "$HERE" bundle create -q "$TMP/src.bundle" refs/pideb/rev
  git -C "$HERE" update-ref -d refs/pideb/rev
  ssh "$REMOTE" 'if pgrep -x cmake >/dev/null || pgrep -x ninja >/dev/null; then echo "a build is already running on this host" >&2; exit 1; fi; mkdir -p ~/pkgbuild'
  scp -q "$TMP/src.bundle" "$REMOTE:pkgbuild/$SHA.bundle"
  scp -q "$0" "$REMOTE:pkgbuild/make-pi-deb.sh"
  ssh "$REMOTE" "set -e; cd ~/pkgbuild; rm -rf $SHA; git init -q $SHA; git -C $SHA fetch -q ../$SHA.bundle "refs/*:refs/*"; git -C $SHA checkout -q --detach $SHA; \
    nice -n 10 bash make-pi-deb.sh --local ~/pkgbuild/$SHA --jobs $JOBS --out ~/pkgbuild/out" 2>&1 | tee "${PIDEB_LOG:-/dev/null}"
  mkdir -p "$HERE/dist"
  scp -q "$REMOTE:pkgbuild/out/qlcconsole_*+git*.$SHA_*.deb" "$HERE/dist/" 2>/dev/null \
    || scp -q "$REMOTE:pkgbuild/out/*$SHA*.deb" "$HERE/dist/"
  ls -l "$HERE"/dist/*"$SHA"*.deb
  exit 0
fi

[ -n "$SRC" ] || { echo "need --remote HOST or --local SRCDIR" >&2; exit 2; }
SRC="$(cd "$SRC" && pwd)"; OUT="${OUT:-$SRC/../out}"; mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
ARCH="$(dpkg --print-architecture)"
VER_FILE="$(head -1 "$SRC/VERSION")"
N="$(git -C "$SRC" rev-list --count QLC+_4.14.2..HEAD 2>/dev/null || echo 0)"
SHA="$(git -C "$SRC" rev-parse --short HEAD)"
VERSION="${VER_FILE}+git${N}.${SHA}"
BUILD="$SRC/build-deb" STAGE="$SRC/stage-deb"

cmake -S "$SRC" -B "$BUILD" -G Ninja -DQLC_NO_TESTS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=OFF
ninja -C "$BUILD" -j"$JOBS"

rm -rf "$STAGE"; DESTDIR="$STAGE" cmake --install "$BUILD" --strip
# the install rules write udev rules under /etc/udev/rules.d (conffiles)
[ -d "$STAGE/etc/udev/rules.d" ] || { echo "no udev rules installed" >&2; exit 1; }
mkdir -p "$STAGE/DEBIAN" "$STAGE/usr/share/doc/qlcconsole"
install -m644 "$SRC/COPYING" "$STAGE/usr/share/doc/qlcconsole/copyright"
[ -f "$SRC/CHANGELOG.md" ] && gzip -9nc "$SRC/CHANGELOG.md" > "$STAGE/usr/share/doc/qlcconsole/changelog.gz"

# Depends from the binaries themselves. dpkg-shlibdeps wants a debian/control.
SD="$(mktemp -d)"; mkdir "$SD/debian"; printf 'Source: qlcconsole\n\nPackage: qlcconsole\nArchitecture: any\n' > "$SD/debian/control"
LIBS="$(find "$STAGE/usr/lib" -maxdepth 2 -name 'libqlcconsole*.so*' -printf '-l%h\n' | sort -u | tr '\n' ' ')"
BINS="$(find "$STAGE/usr" -type f \( -perm -u+x -o -name '*.so*' \) -exec sh -c 'file -b "$1" | grep -q ELF' _ {} \; -print)"
SHL="$(cd "$SD" && dpkg-shlibdeps -O -Sunused -e$(echo "$BINS" | tr '\n' ' ' | sed 's/ / -e/g;s/-e$//') $LIBS 2>/dev/null \
      | sed -n 's/^shlibs:Depends=//p')"
# plus what is loaded at run time rather than linked
EXTRA="shared-mime-info, qt6-image-formats-plugins, qt6-svg-plugins, qt6-multimedia-plugins | libqt6multimedia6, libqt6svg6"
case "$ARCH" in *) EXTRA="$EXTRA, qt6-qpa-plugins";; esac
SIZE="$(du -sk "$STAGE/usr" "$STAGE/etc" | awk '{s+=$1} END{print s}')"
cat > "$STAGE/DEBIAN/control" <<CTL
Package: qlcconsole
Version: $VERSION
Architecture: $ARCH
Section: misc
Priority: optional
Maintainer: Branson Matheson <branson.matheson@gmail.com>
Installed-Size: $SIZE
Depends: $SHL, $EXTRA
Recommends: libasound2t64 | libasound2
Homepage: https://github.com/sandinak/qlcconsole
Description: qlcconsole - a fork of Q Light Controller Plus, a DMX lighting desk
 Show Tree, Build workbench and the QLC+ 4 engine for professional lighting
 fixtures. Includes the USB DMX udev rules (Enttec, FTDI, uDMX, Peperoni,
 HID) and the stock fixture library. Built for a Raspberry Pi 5 as the show
 machine; also installs on any Debian 13 system with Qt 6.8.
CTL
(cd "$STAGE/etc" && find udev -type f -printf '/etc/%p\n') > "$STAGE/DEBIAN/conffiles"
cat > "$STAGE/DEBIAN/postinst" <<'PI'
#!/bin/sh
set -e
if [ "$1" = configure ]; then
  command -v update-mime-database >/dev/null && update-mime-database /usr/share/mime || true
  command -v gtk-update-icon-cache >/dev/null && gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor 2>/dev/null || true
  command -v update-desktop-database >/dev/null && update-desktop-database -q || true
  # USB DMX adapters plugged in before the install: reload and re-trigger
  if command -v udevadm >/dev/null; then
    udevadm control --reload-rules 2>/dev/null || true
    udevadm trigger --subsystem-match=usb --action=add 2>/dev/null || true
  fi
fi
exit 0
PI
cat > "$STAGE/DEBIAN/postrm" <<'PR'
#!/bin/sh
set -e
command -v update-mime-database >/dev/null && update-mime-database /usr/share/mime || true
command -v udevadm >/dev/null && udevadm control --reload-rules 2>/dev/null || true
exit 0
PR
chmod 755 "$STAGE/DEBIAN/postinst" "$STAGE/DEBIAN/postrm"
fakeroot dpkg-deb --build -Zxz "$STAGE" "$OUT/qlcconsole_${VERSION}_${ARCH}.deb"
dpkg-deb -I "$OUT/qlcconsole_${VERSION}_${ARCH}.deb" | sed -n '1,14p'
echo "make-pi-deb: $OUT/qlcconsole_${VERSION}_${ARCH}.deb"
