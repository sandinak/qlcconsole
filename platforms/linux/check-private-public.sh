#!/bin/bash
#
# A test may say `#define private public` / `#define protected public` (the
# stock QLC+ idiom) only where no standard header is first included under it.
# libstdc++ rejects a standard header that sees a redefined access specifier
# (MSYS2 GCC 16 stopped at <sstream> that way), and which project headers
# pull one in changes with every edit.
#
# Between the define and its #undef (or the end of the file) this fails:
#   - any `#include <...>` (a standard or Qt header): move it above the define;
#   - a project `#include "..."` whose own includes (followed through the
#     project's headers) name a standard header (<vector>, <memory>, ...) that
#     is not already included above the define.
# Printed as file:line with a one-line fix. The usual fix is one line above the
# define: #include "<path>/engine/test/common/stdprelude.h" (every std header
# the project's headers use); or #undef right after the header that needs it,
# or a friend in the class instead of the define.
#
#   check-private-public.sh --source REPO   every .cpp/.cc/.h under REPO
#   check-private-public.sh --self-test     this script's own test
#
# check-all.sh runs --self-test and --source before building.

PYCHECK=$(cat <<'PY'
import os, re, sys, collections

INC = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')
DEF = re.compile(r'^\s*#\s*define\s+(private|protected)\s+public(\s|/|$)')
UND = re.compile(r'^\s*#\s*undef\s+(private|protected)(\s|/|$)')
HINT = ("use a friend, or move std/Qt includes above the #define "
        "(e.g. #include \"engine/test/common/stdprelude.h\")")

def std(n):
    return '.' not in n and '/' not in n and not n.startswith('Q')

def build_index(root):
    idx = collections.defaultdict(list)
    for d, ds, fs in os.walk(root):
        ds[:] = [x for x in ds if not x.startswith('build') and x not in ('.git', 'dist')]
        for f in fs:
            if f.endswith(('.h', '.hpp')):
                idx[f].append(os.path.join(d, f))
    return idx

class Scan:
    def __init__(self, root):
        self.idx = build_index(root)
        self.direct = {}
        self.memo = {}

    def incs(self, p):
        if p not in self.direct:
            r = []
            try:
                for l in open(p, errors='replace'):
                    m = INC.match(l)
                    if m:
                        r.append((m.group(1), m.group(2)))
            except OSError:
                pass
            self.direct[p] = r
        return self.direct[p]

    def stdof(self, name, seen):
        out = set()
        for p in self.idx.get(os.path.basename(name), []):
            if p in seen:
                continue
            seen.add(p)
            for k, n in self.incs(p):
                if k == '<':
                    if std(n):
                        out.add(n)
                else:
                    out |= self.stdof(n, seen)
        return out

    def stdof_file(self, path, name, seen):
        # a quoted include is searched next to the including file first
        loc = os.path.normpath(os.path.join(os.path.dirname(path), name))
        if os.path.isfile(loc):
            seen.add(loc)
            out = set()
            for k, n in self.incs(loc):
                if k == '<':
                    if std(n):
                        out.add(n)
                else:
                    out |= self.stdof_file(loc, n, seen)
            return out
        return self.stdof(name, seen)

    def check(self, path):
        bad = []
        have = set()
        pdef = qdef = False
        for i, line in enumerate(open(path, errors='replace'), 1):
            if DEF.match(line):
                if 'private' in line.split('define')[1]:
                    pdef = True
                else:
                    qdef = True
                continue
            if UND.match(line):
                if 'private' in line.split('undef')[1]:
                    pdef = False
                else:
                    qdef = False
                continue
            m = INC.match(line)
            if not m:
                continue
            kind, name = m.groups()
            inside = pdef or qdef
            if kind == '<':
                if inside:
                    bad.append((i, line.strip(), 'a standard/Qt header under the define'))
                elif std(name):
                    have.add(name)
                continue
            need = self.stdof_file(path, name, set())
            if inside:
                miss = need - have
                if miss:
                    bad.append((i, line.strip(), 'pulls in <%s> first, under the define'
                                % '>, <'.join(sorted(miss)[:4])))
                    continue
            have |= need
        return bad

def main():
    mode, arg = sys.argv[1], sys.argv[2]
    s = Scan(arg)
    files = []
    if mode == 'file':
        files = [arg]
    else:
        for d, ds, fs in os.walk(arg):
            ds[:] = [x for x in ds if not x.startswith('build') and x not in ('.git', 'dist')]
            for f in fs:
                if f.endswith(('.cpp', '.cc', '.h')):
                    p = os.path.join(d, f)
                    try:
                        if any(DEF.match(l) for l in open(p, errors='replace')):
                            files.append(p)
                    except OSError:
                        pass
    files.sort()
    nbad = 0
    for p in files:
        res = s.check(p)
        if res:
            nbad += 1
            for ln, txt, why in res:
                print('%s:%d: %s (%s)' % (os.path.relpath(p, arg) if mode != 'file' else p, ln, txt, why))
                print('    fix: ' + HINT)
    if not files:
        print('check-private-public: no file with the define under ' + arg)
        sys.exit(1)
    if nbad:
        print('check-private-public: %d of %d file(s) first include a standard/Qt header under #define private/protected public.' % (nbad, len(files)))
        sys.exit(1)
    print('check-private-public: %d file(s) with the define, all clean.' % len(files))

main()
PY
)

check_source()
{
    python3 -I -c "$PYCHECK" dir "$1"
}

self_test()
{
    local tmp fails=0 rc
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/check-private-public.XXXXXX") || return 1
    printf '#pragma once\n#include <vector>\n' > "$tmp/vec.h"
    printf '#pragma once\n#include "vec.h"\n' > "$tmp/viavec.h"
    printf '#pragma once\n#include <QString>\n' > "$tmp/plain.h"
    t() # <want 0|1> <description> <file content>
    {
        printf '%b' "$3" > "$tmp/t.cpp"
        python3 -I -c "$PYCHECK" file "$tmp/t.cpp" >/dev/null; rc=$?
        if [ "$rc" -ne "$1" ]; then
            echo "self-test FAILED: $2 (exit $rc, wanted $1)"; fails=$((fails + 1))
        fi
    }
    t 0 "includes above" '#include <QtTest>\n#define private public\n#include "plain.h"\n#undef private\n'
    t 1 "Qt header under the define" '#define private public\n#include <QtTest>\n#include "plain.h"\n'
    t 1 "project header pulling <vector>" '#define private public\n#include "vec.h"\n'
    t 1 "pulling it through another header" '#define protected public\n#include "viavec.h"\n'
    t 0 "<vector> above the define" '#include <vector>\n#define private public\n#include "viavec.h"\n#include "plain.h"\n#undef private\n'
    t 0 "same via a header above" '#include "vec.h"\n#define private public\n#include "viavec.h"\n'
    t 0 "header after the undef" '#define private public\n#include "plain.h"\n#undef private\n#include "vec.h"\n#include <QtTest>\n'
    t 1 "only private undefined" '#define protected public\n#define private public\n#include "plain.h"\n#undef private\n#include <vector>\n'
    t 0 "both undefined" '#define protected public\n#define private public\n#include "plain.h"\n#undef private\n#undef protected\n#include <vector>\n'
    t 1 "no undef, <map> at the end" '#define private public\n#include "plain.h"\n#include <map>\n'
    rm -rf "$tmp"
    if [ "$fails" -gt 0 ]; then
        echo "check-private-public: self-test: $fails case(s) failed."; return 1
    fi
    echo "check-private-public: self-test passed."
    return 0
}

case "${1:-}" in
    --source)    [ -n "${2:-}" ] || { echo >&2 "usage: $0 --source REPO"; exit 2; }
                 check_source "$2" ;;
    --self-test) self_test ;;
    *)           echo >&2 "usage: $0 --source REPO | --self-test"; exit 2 ;;
esac
