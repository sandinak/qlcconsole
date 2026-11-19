#!/bin/bash
# Self-test of bundlefix.py on a fake tree: a plugin links a framework by
# @rpath (as Homebrew Qt 6.12's svg plugins link QtSvg.framework) that nothing
# else in the app uses, and one by absolute path; both must end up in the
# bundle and nothing may point outside it. Needs only clang, otool, python3.
#   platforms/macos/bundlefix-selftest.sh
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
T=$(mktemp -d "${TMPDIR:-/tmp}/bundlefix-test.XXXXXX") || exit 1
trap 'rm -rf "$T"' EXIT
fail=0
Q="$T/qt"; APP="$T/Fake.app"
mkdir -p "$Q/lib/QtFake.framework/Versions/A" "$Q/abs" "$APP/Contents/MacOS" "$APP/Contents/PlugIns/imageformats" "$APP/Contents/Frameworks"
echo 'int fake(void){return 1;}' > "$T/f.c"
echo 'int fake(void); int plug(void){return fake();}' > "$T/p.c"
echo 'int main(void){return 0;}' > "$T/m.c"
clang -dynamiclib -o "$Q/lib/QtFake.framework/Versions/A/QtFake" "$T/f.c" \
    -install_name @rpath/QtFake.framework/Versions/A/QtFake || exit 1
ln -s A "$Q/lib/QtFake.framework/Versions/Current"; ln -s Versions/Current/QtFake "$Q/lib/QtFake.framework/QtFake"
clang -dynamiclib -o "$Q/abs/libabs.dylib" "$T/f.c" -install_name "$Q/abs/libabs.dylib" || exit 1
clang -dynamiclib -o "$APP/Contents/PlugIns/imageformats/libqfake.dylib" "$T/p.c" \
    "$Q/lib/QtFake.framework/Versions/A/QtFake" -Wl,-rpath,"$Q/lib" || exit 1
clang -dynamiclib -o "$APP/Contents/PlugIns/imageformats/libqabs.dylib" "$T/p.c" "$Q/abs/libabs.dylib" || exit 1
clang -o "$APP/Contents/MacOS/Fake" "$T/m.c" || exit 1
out=$(QTDIR="$Q" python3 "$HERE/bundlefix.py" "$APP" 2>&1); rc=$?
[ $rc -eq 0 ] && echo "ok - bundlefix passes" || { echo "not ok - bundlefix passes: $out"; fail=1; }
[ -f "$APP/Contents/Frameworks/QtFake.framework/Versions/A/QtFake" ] && echo "ok - a plugin's @rpath framework is copied" || { echo "not ok - a plugin's @rpath framework is copied"; fail=1; }
[ -f "$APP/Contents/Frameworks/libabs.dylib" ] && echo "ok - an absolute library is copied" || { echo "not ok - an absolute library is copied"; fail=1; }
bad=$(find "$APP/Contents" -type f | while read -r f; do file "$f" | grep -q Mach-O || continue
  id=$(otool -D "$f" | sed -n 2p)
  otool -L "$f" | awk 'NR>1{print $1}' | grep -vxF "${id:-none}" | grep -v '^/usr/lib/\|^/System/' | while read -r r; do
    if [ "${r#@rpath/}" != "$r" ]; then [ -e "$APP/Contents/Frameworks/${r#@rpath/}" ] || echo "$f -> $r"; else echo "$f -> $r"; fi
  done; done)
[ -z "$bad" ] && echo "ok - nothing points outside the bundle" || { echo "not ok - outside refs: $bad"; fail=1; }
[ $fail -eq 0 ] && echo "bundlefix self-test: ok" || echo "bundlefix self-test: FAILED"
exit $fail
