#!/usr/bin/env python3
"""Per-directory and per-risk-area coverage from coverage.sh's per-file.tsv,
as the Markdown tables in docs/dev/TEST_MATRIX.md, "Coverage".

  python3 -I tools/coverage-areas.py per-file.tsv               (one run)
  python3 -I tools/coverage-areas.py before.tsv after.tsv       (before -> after)
"""
import sys, re, collections
AREAS = [
 ('Show-file load/save', r'engine/src/(doc|qlcfile|showembedding|qxwimporter|fixturedefupdates|venuecorrections)\.cpp|ui/src/(app|showfiles?|importsheet|eosimportwizard)\.cpp|engine/src/eosimporter\.cpp'),
 ('Engine output and merge', r'engine/src/(universe|genericfader|fadechannel|grandmaster|mastertimer.*|inputoutputmap|outputpatch|inputpatch|channelmodifier|scene|chaser|chaserrunner|collection|efx|efxfixture|rgbmatrix|cuelookahead|markplanner|show|showrunner|showfunction|script)\.cpp'),
 ('Network plugins', r'plugins/(artnet/src|E1\.31|osc|interfaces)/'),
 ('Show Tree controller', r'engine/src/showtree[a-z]*\.cpp|engine/src/treecue|ui/src/treecuestack'),
 ('Venues', r'engine/src/venue[a-z]*\.cpp|engine/src/rigcalibration'),
 ('Focus/refine sessions', r'engine/src/(focus[a-z]*|refine[a-z]*)\.cpp'),
 ('Web access (auth, base)', r'webaccess/src/webaccess(auth|base)\.cpp'),
 ('Undo', r'engine/src/(paletteops|patchundo)\.cpp|ui/src/workbench(stack)?\.cpp'),
]
def load(p):
    d = {}
    for i, line in enumerate(open(p)):
        if i == 0: continue
        f, l, lh, b, bh = line.rstrip('\n').split('\t')
        d[f] = tuple(int(x or 0) for x in (l, lh, b, bh))
    return d
def agg(d, pred):
    t = [0, 0, 0, 0]; n = 0
    for f, v in d.items():
        if pred(f):
            n += 1
            for i in range(4): t[i] += v[i]
    return n, t
def pct(a, b): return 100.0 * a / b if b else 0.0
def topdir(f):
    p = f.split('/')
    if p[0] == 'plugins': return '/'.join(p[:2])
    return '/'.join(p[:2]) if len(p) > 2 else p[0]
ds = [load(p) for p in sys.argv[1:]]
def table(title, keys, pred_for):
    print(f'\n### {title}\n')
    if len(ds) == 1:
        print('| | files | lines | line % | branches | branch % |\n|---|---|---|---|---|---|')
    else:
        print('| | lines before | line % before | line % after | Δ | branch % before | branch % after | Δ |\n|---|---|---|---|---|---|---|---|')
    for k in keys:
        rs = [agg(d, pred_for(k)) for d in ds]
        if len(ds) == 1:
            n, t = rs[0]
            print(f'| {k} | {n} | {t[0]} | {pct(t[1],t[0]):.1f} | {t[2]} | {pct(t[3],t[2]):.1f} |')
        else:
            (n0, a), (n1, b) = rs
            la, lb, ba, bb = pct(a[1],a[0]), pct(b[1],b[0]), pct(a[3],a[2]), pct(b[3],b[2])
            print(f'| {k} | {a[0]} | {la:.1f} | {lb:.1f} | {lb-la:+.1f} | {ba:.1f} | {bb:.1f} | {bb-ba:+.1f} |')
table('Total', ['All measured'], lambda k: (lambda f: True))
table('By risk area', [a for a, _ in AREAS], lambda k: (lambda f, rx=dict(AREAS)[k]: re.search(rx, f) is not None))
dirs = sorted({topdir(f) for f in ds[0]})
table('By directory', dirs, lambda k: (lambda f: topdir(f) == k))
if len(ds) == 1:
    print('\n### Files in the risk areas, lowest line coverage first\n')
    print('| file | area | lines | line % | branch % |\n|---|---|---|---|---|')
    rows = []
    for f, v in ds[0].items():
        for a, rx in AREAS:
            if re.search(rx, f):
                rows.append((pct(v[1], v[0]), f, a, v)); break
    for p, f, a, v in sorted(rows)[:45]:
        print(f'| {f} | {a} | {v[0]} | {p:.1f} | {pct(v[3],v[2]):.1f} |')
else:
    print('\n### Files that moved most (lines newly hit)\n')
    print('| file | lines | before % | after % |\n|---|---|---|---|')
    mv = []
    for f, v in ds[1].items():
        o = ds[0].get(f, (v[0], 0, 0, 0))
        mv.append((v[1] - o[1], f, o, v))
    for d, f, o, v in sorted(mv, reverse=True)[:30]:
        print(f'| {f} | {v[0]} | {pct(o[1],o[0]):.1f} | {pct(v[1],v[0]):.1f} |')
