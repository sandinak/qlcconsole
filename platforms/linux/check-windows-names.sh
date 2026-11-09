#!/bin/bash
#
# windows.h defines near and far as macros (empty), so a variable with either
# name compiles on macOS and Linux and is a syntax error on Windows: it broke
# CI's Windows build for a day without any gate seeing it. This fails the gate
# on either word in the code of a .cpp/.h (comments and strings don't count).
#
#   check-windows-names.sh --source REPO   every .cpp/.h under REPO's sources
#   check-windows-names.sh --self-test     this script's own test

exec python3 -I - "$@" <<'PY'
import os, re, sys

# Comments and literals out, line numbers kept.
STRIP = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)
BAD = re.compile(r'\b(near|far)\b')

def hits(text):
    code = STRIP.sub(lambda m: '\n' * m.group(0).count('\n'), text)
    return [(code.count('\n', 0, m.start()) + 1, m.group(1)) for m in BAD.finditer(code)]

mode = sys.argv[1] if len(sys.argv) > 1 else ''
if mode == '--self-test':
    assert hits('const QPointF far = a();\n') == [(1, 'far')]
    assert hits('int x;\nuse(near);\n') == [(2, 'near')]
    assert hits('// as far as it goes\n/* near\n far */ int a;\nQString("so far");\n') == []
    assert hits('int farther, nearest, far_end;\n') == []
    print('check-windows-names: self-test passed')
    sys.exit(0)
if mode != '--source' or len(sys.argv) != 3:
    print('usage: check-windows-names.sh --source REPO | --self-test')
    sys.exit(2)

bad = 0
for top in ('engine', 'ui', 'main', 'launcher', 'fixtureeditor', 'plugins', 'webaccess', 'ai'):
    for d, _, files in os.walk(os.path.join(sys.argv[2], top)):
        for f in files:
            if f.endswith(('.cpp', '.h')):
                p = os.path.join(d, f)
                with open(p, encoding='utf-8', errors='replace') as fh:
                    for line, word in hits(fh.read()):
                        print('%s:%d: "%s" is a macro on Windows: rename it' % (p, line, word))
                        bad += 1
sys.exit(1 if bad else 0)
PY
