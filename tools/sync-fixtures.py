#!/usr/bin/env python3
#
# Brings QLC+ upstream's fixture definitions into resources/fixtures, once
# a release (docs/dev/RELEASE.md). Upstream is read from a local checkout
# with `git show`, so its working tree and branch are never touched.
#
# Each .qxf is merged three ways: the base is the upstream commit of the
# last sync (tools/sync-fixtures.base), "ours" is the fork's file, "theirs"
# is upstream's at --ref.
#
#   new        upstream-only file                    -> added
#   updated    upstream changed, the fork didn't     -> taken
#   merged     both changed, git merge-file is clean -> merged
#   conflict   both changed, the merge clashes       -> the fork's file kept, reported
#   fork-edit  the fork changed, upstream didn't     -> kept
#   fork-only  not upstream at all                   -> kept
#   removed    upstream deleted it, the fork didn't  -> removed
#   kept       upstream deleted it, the fork edited  -> kept, reported
#
# Then FixturesMap.xml is rebuilt (scripts/fixtures-tool.py --map), the
# definitions are checked (scripts/check: schema + fixtures-tool --validate)
# and, when everything passed, the new upstream commit is written to the
# base file. Nothing is committed.
#
#   tools/sync-fixtures.py [--upstream ~/git/qlcplus] [--ref origin/master]
#                          [--no-fetch] [--dry-run] [--no-check]
#
# Upstream master is the source: QLC+ 4 is in maintenance and new
# definitions land on master only (the .qxf format is the same in 4 and 5;
# the script warns when upstream's fixture.xsd moved, which is when a
# definition might use something the 4.x engine can't read).

import argparse
import os
import subprocess
import sys
import tempfile

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
FIXTURES = os.path.join(REPO, 'resources', 'fixtures')
BASE_FILE = os.path.join(REPO, 'tools', 'sync-fixtures.base')
UP_PREFIX = 'resources/fixtures/'


def git(upstream, *args, binary=False):
    out = subprocess.run(['git', '-C', upstream] + list(args), check=True,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return out.stdout if binary else out.stdout.decode()


def upstream_tree(upstream, rev):
    """{relative path: blob id} of every .qxf at rev."""
    files = {}
    for line in git(upstream, 'ls-tree', '-r', rev, '--', UP_PREFIX).splitlines():
        meta, path = line.split('\t', 1)
        if path.endswith('.qxf'):
            files[path[len(UP_PREFIX):]] = meta.split()[2]
    return files


def blob(upstream, blob_id):
    return git(upstream, 'cat-file', 'blob', blob_id, binary=True)


def fork_files():
    files = set()
    for root, _dirs, names in os.walk(FIXTURES):
        if os.path.relpath(root, FIXTURES).split(os.sep)[0] == 'scripts':
            continue
        for n in names:
            if n.endswith('.qxf'):
                files.add(os.path.relpath(os.path.join(root, n), FIXTURES).replace(os.sep, '/'))
    return files


def read(rel):
    with open(os.path.join(FIXTURES, rel), 'rb') as f:
        return f.read()


def write(rel, data):
    path = os.path.join(FIXTURES, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(data)


def merge3(ours, base, theirs):
    """git merge-file; returns (merged bytes, clean?)."""
    with tempfile.TemporaryDirectory() as tmp:
        paths = []
        for name, data in (('ours', ours), ('base', base), ('theirs', theirs)):
            p = os.path.join(tmp, name)
            with open(p, 'wb') as f:
                f.write(data)
            paths.append(p)
        r = subprocess.run(['git', 'merge-file', '-p'] + paths,
                           stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        return r.stdout, r.returncode == 0


def main():
    ap = argparse.ArgumentParser(description='Sync fixture definitions from QLC+ upstream.')
    ap.add_argument('--upstream', default=os.path.expanduser('~/git/qlcplus'),
                    help='local QLC+ checkout (default ~/git/qlcplus)')
    ap.add_argument('--ref', default='origin/master', help='upstream revision to sync to')
    ap.add_argument('--base', help='upstream commit of the last sync (default: tools/sync-fixtures.base)')
    ap.add_argument('--no-fetch', action='store_true', help="don't git fetch upstream first")
    ap.add_argument('--dry-run', action='store_true', help='report only; change nothing')
    ap.add_argument('--no-check', action='store_true', help="don't run scripts/check afterwards")
    args = ap.parse_args()

    if not os.path.isdir(os.path.join(args.upstream, '.git')):
        sys.exit('no QLC+ checkout at %s (--upstream)' % args.upstream)
    if not args.no_fetch:
        remote = args.ref.split('/')[0] if '/' in args.ref else 'origin'
        print('Fetching %s in %s ...' % (remote, args.upstream))
        git(args.upstream, 'fetch', '--quiet', '--tags', remote)

    base_rev = args.base
    if not base_rev:
        with open(BASE_FILE) as f:
            base_rev = f.read().split()[0]
    ref_sha = git(args.upstream, 'rev-parse', args.ref + '^{commit}').strip()
    base_sha = git(args.upstream, 'rev-parse', base_rev + '^{commit}').strip()
    when = git(args.upstream, 'log', '-1', '--format=%cs', ref_sha).strip()
    print('Upstream %s = %s (%s); last sync %s' % (args.ref, ref_sha[:9], when, base_sha[:9]))

    if git(args.upstream, 'diff', '--stat', base_sha, ref_sha,
           '--', 'resources/schemas/fixture.xsd').strip():
        print('WARNING: upstream changed resources/schemas/fixture.xsd since the last sync;'
              ' check new definitions load in the 4.x engine.')

    base = upstream_tree(args.upstream, base_sha)
    theirs = upstream_tree(args.upstream, ref_sha)
    ours = fork_files()

    report = {k: [] for k in ('new', 'updated', 'merged', 'conflict', 'fork-edit',
                              'fork-only', 'removed', 'kept')}
    for rel in sorted(ours | set(theirs)):
        if rel not in theirs:
            if rel not in base:
                report['fork-only'].append(rel)
            elif read(rel) == blob(args.upstream, base[rel]):
                report['removed'].append(rel)
                if not args.dry_run:
                    os.remove(os.path.join(FIXTURES, rel))
            else:
                report['kept'].append(rel)
            continue
        up = blob(args.upstream, theirs[rel])
        if rel not in ours:
            report['new'].append(rel)
            if not args.dry_run:
                write(rel, up)
            continue
        mine = read(rel)
        if mine == up:
            continue
        old = blob(args.upstream, base[rel]) if rel in base else None
        if old is not None and mine == old:
            report['updated'].append(rel)
            if not args.dry_run:
                write(rel, up)
        elif old is not None and up == old:
            report['fork-edit'].append(rel)
        else:
            merged, clean = merge3(mine, old if old is not None else b'', up)
            if clean and old is not None:
                report['merged'].append(rel)
                if not args.dry_run:
                    write(rel, merged)
            else:
                report['conflict'].append(rel)

    labels = {
        'new': 'New from upstream (added)',
        'updated': 'Updated upstream, untouched here (taken)',
        'merged': 'Changed on both sides, merged cleanly',
        'conflict': 'CONFLICT: changed on both sides, the fork\'s file kept -- merge by hand',
        'fork-edit': 'Fork edits, upstream unchanged (kept)',
        'fork-only': 'Fork-only definitions (kept)',
        'removed': 'Deleted upstream, untouched here (removed)',
        'kept': 'Deleted upstream but edited here (kept -- check)',
    }
    for key, label in labels.items():
        print('\n%s: %d' % (label, len(report[key])))
        for rel in report[key]:
            print('  ' + rel)

    changed = report['new'] or report['updated'] or report['merged'] or report['removed']
    if args.dry_run:
        print('\nDry run: nothing changed.')
        return 1 if report['conflict'] else 0

    if changed:
        print('\nRebuilding FixturesMap.xml ...')
        subprocess.run([sys.executable, 'scripts/fixtures-tool.py', '--map'], cwd=FIXTURES,
                       check=True, stdout=subprocess.DEVNULL)
    if not args.no_check:
        print('Checking the definitions (scripts/check) ...')
        r = subprocess.run(['./check'], cwd=os.path.join(FIXTURES, 'scripts'),
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        tail = r.stdout.decode(errors='replace').strip().splitlines()
        print('\n'.join(tail[-15:]))
        if r.returncode != 0:
            print('\nThe check failed; the base stays at %s. Fix the files above and rerun.' % base_sha[:9])
            return 1

    with open(BASE_FILE, 'w') as f:
        f.write('%s %s %s\n' % (ref_sha, args.ref, when))
    total = len(fork_files())
    print('\n%d definitions. Base is now %s (%s). Log it in docs/dev/UPSTREAM.md;'
          ' CHANGELOG: "%d definitions from QLC+ upstream".'
          % (total, ref_sha[:9], when, len(report['new'])))
    return 1 if report['conflict'] else 0


if __name__ == '__main__':
    sys.exit(main())
