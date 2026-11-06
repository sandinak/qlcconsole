#!/bin/bash
# Fails when C++ code names something after a macro that <windows.h> defines.
#
# MinGW's <windows.h> (pulled in by Qt and the plugins) does
#   #define near
#   #define far
# so a variable, lambda or parameter called `near` or `far` compiles on macOS
# and Linux and then breaks only the Windows build in CI ("expected
# unqualified-id before '=' token"). It happened in 0.4.0 and twice more in
# 0.4.1 (a lambda in showtreecommand.cpp, a QString in monitor.cpp). The gate
# never builds with MinGW, so check-all.sh runs this before anything builds.
#
#   platforms/linux/check-win-macros.sh [--source <repo>]
#
# Looks at code, not comments or strings: an identifier `near`/`far` followed
# by `=` (not `==`), `(`, `;`, `)` or `[` (not `,`: prose in a block comment). Exit 1 lists each place.

set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
[ "${1:-}" = "--source" ] && [ -n "${2:-}" ] && REPO="$2"

hits=$(cd "$REPO" && git ls-files -- '*.cpp' '*.h' '*.hpp' '*.cc' 2>/dev/null \
    | grep -v -E '^(qmlui|hotplugmonitor/src/win32|plugins/.*/win32)/' \
    | xargs grep -n -E '(^|[^A-Za-z0-9_."])(near|far)[[:space:]]*(=[^=]|\(|;|\)|\[)' 2>/dev/null \
    | grep -v -E ':[0-9]+:[[:space:]]*(//|\*|/\*)' \
    | grep -v -E '"[^"]*\b(near|far)\b[^"]*"')

if [ -n "$hits" ]; then
    echo "check-win-macros: <windows.h> defines 'near' and 'far' as empty macros;"
    echo "these names break the Windows (MinGW) build. Rename them:"
    echo "$hits"
    exit 1
fi
exit 0
