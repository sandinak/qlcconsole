#!/bin/bash
# Build the libraries the plugins need -- libusb + libftdi (DMX USB), fftw and
# libsndfile (audio) -- as UNIVERSAL dylibs (arm64 + x86_64), for a release
# that runs on Intel Macs too. Homebrew's are only ever the host's arch.
#
#   platforms/macos/universal-deps.sh [prefix]     (default ~/unideps)
#
# package-local.sh UNIVERSAL=1 builds against the result. Sources are
# downloaded into <prefix>/src; delete <prefix> to start over.
set -euo pipefail

P="${1:-$HOME/unideps}"
SRC="$P/src"
mkdir -p "$SRC" "$P/lib/pkgconfig"
cd "$SRC"

ARCHS="-arch arm64 -arch x86_64"
MIN="-mmacosx-version-min=12.0"      # Qt 6.9's own minimum
export CFLAGS="$ARCHS $MIN -O2" CXXFLAGS="$ARCHS $MIN -O2" LDFLAGS="$ARCHS $MIN"
export PKG_CONFIG_PATH="$P/lib/pkgconfig" PKG_CONFIG_LIBDIR="$P/lib/pkgconfig"
J=${J:-$(sysctl -n hw.perflevel0.logicalcpu 2>/dev/null || sysctl -n hw.ncpu)}
AC="--prefix=$P --disable-dependency-tracking --disable-static"
CM=(-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
    -DCMAKE_INSTALL_PREFIX="$P" -DCMAKE_PREFIX_PATH="$P" -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_IGNORE_PREFIX_PATH="/opt/homebrew;/usr/local"
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5)

fetch() { # <url>
    local f; f=$(basename "$1")
    [ -f "$f" ] || curl -sSLf -o "$f" "$1"
    echo "$f"
}
step() { echo; echo "== $*"; }

step "libusb"
tar xf "$(fetch https://github.com/libusb/libusb/releases/download/v1.0.27/libusb-1.0.27.tar.bz2)"
# The SDK may declare pipe2() (macOS 27); an older Mac has none, and libusb
# would fail to load there. The deployment target is 12.
(cd libusb-1.0.27 && ac_cv_func_pipe2=no ./configure $AC >/dev/null \
    && make -j"$J" >/dev/null && make install >/dev/null)

step "libftdi"
tar xf "$(fetch https://www.intra2net.com/en/developer/libftdi/download/libftdi1-1.5.tar.bz2)"
(cd libftdi1-1.5 && cmake -S . -B b "${CM[@]}" -DFTDI_EEPROM=OFF -DEXAMPLES=OFF \
    -DPYTHON_BINDINGS=OFF -DDOCUMENTATION=OFF -DFTDIPP=OFF -DLINK_PYTHON_LIBRARY=OFF \
    -DLIBUSB_INCLUDE_DIR="$P/include/libusb-1.0" -DLIBUSB_LIBRARIES="$P/lib/libusb-1.0.dylib" >/dev/null \
    && cmake --build b -j"$J" >/dev/null && cmake --install b >/dev/null)

step "fftw"
tar xf "$(fetch https://www.fftw.org/fftw-3.3.10.tar.gz)"
(cd fftw-3.3.10 && ./configure $AC --enable-shared --disable-fortran >/dev/null \
    && make -j"$J" >/dev/null && make install >/dev/null)

# MP3 (libsndfile's MPEG codec needs both: mpg123 decodes, LAME encodes). Both
# LGPL-2.1, shared, so the app stays relinkable (resources/licenses/THIRD-PARTY.txt).
# Assembly off: a universal build can't use one arch's, and the C paths are fast
# enough to decode a song in well under a second.
step "mpg123 (MP3 decode)"
tar xf "$(fetch https://downloads.sourceforge.net/project/mpg123/mpg123/1.32.10/mpg123-1.32.10.tar.bz2)"
(cd mpg123-1.32.10 && ./configure $AC --disable-components --enable-libmpg123 \
    --with-cpu=generic --disable-modules >/dev/null \
    && make -j"$J" >/dev/null && make install >/dev/null)

step "LAME (MP3 encode, required by libsndfile's MPEG support)"
tar xf "$(fetch https://downloads.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz)"
(cd lame-3.100 && ./configure $AC --disable-frontend --disable-decoder --disable-nasm \
    --disable-gtktest >/dev/null \
    && make -j"$J" >/dev/null && make install >/dev/null)

step "libsndfile (MP3 via mpg123 + LAME; no Ogg/FLAC/Opus)"
tar xf "$(fetch https://github.com/libsndfile/libsndfile/releases/download/1.2.2/libsndfile-1.2.2.tar.xz)"
(cd libsndfile-1.2.2 && cmake -S . -B b "${CM[@]}" -DBUILD_SHARED_LIBS=ON \
    -DENABLE_EXTERNAL_LIBS=OFF -DENABLE_MPEG=ON -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTING=OFF -DINSTALL_MANPAGES=OFF >/dev/null \
    && cmake --build b -j"$J" >/dev/null && cmake --install b >/dev/null)

step "check: every library both arches"
bad=0
for f in "$P"/lib/*.dylib; do
    [ -L "$f" ] && continue
    a=$(lipo -archs "$f")
    echo "  $(basename "$f"): $a"
    case "$a" in *x86_64*arm64*|*arm64*x86_64*) ;; *) bad=1 ;; esac
done
[ "$bad" = 0 ] || { echo "ERROR: a library is not universal"; exit 1; }
echo "OK: $P"
