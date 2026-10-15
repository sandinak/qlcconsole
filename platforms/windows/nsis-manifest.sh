#!/bin/sh
# Writes the installer's file lists for qlcconsole.nsi, from what is actually
# in the deploy directory (by default the current one).
#
#   manifest-install.nsh   - File / File /r for every top-level entry
#   manifest-uninstall.nsh - the matching Delete / RMDir /r
#
# Top-level granularity is enough: every folder in the deploy dir (Qt plugin
# folders, Fixtures, RGBScripts, ...) belongs to qlcconsole wholesale, so the
# uninstaller can remove it recursively without touching anything the user
# put next to the program.
#
# Run it after `cmake --install` and windeployqt, before makensis.

set -e

cd "${1:-.}"

INSTALL=manifest-install.nsh
UNINSTALL=manifest-uninstall.nsh

: > "$INSTALL"
: > "$UNINSTALL"

for entry in * ; do
    case "$entry" in
        *.nsi|*.nsh|*-setup.exe|uninstall.exe) continue ;;
    esac
    # NSIS expands $ in strings; a literal one is written $$.
    name=$(printf '%s' "$entry" | sed 's/\$/$$/g')
    if [ -d "$entry" ]; then
        printf 'File /r "%s"\n' "$name" >> "$INSTALL"
        printf 'RMDir /r "$INSTDIR\\%s"\n' "$name" >> "$UNINSTALL"
    else
        printf 'File "%s"\n' "$name" >> "$INSTALL"
        printf 'Delete "$INSTDIR\\%s"\n' "$name" >> "$UNINSTALL"
    fi
done

echo "nsis-manifest: $(wc -l < "$INSTALL") top-level entries listed in $(pwd)"
