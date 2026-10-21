#!/usr/bin/env python3
#
# Finds -- and with --strip removes -- the colour-profile chunks of the PNGs
# the app ships: iCCP, sRGB, gAMA, cHRM and cICP.
#
# Why: Qt turns any of those chunks into a QColorSpace on the loaded QImage,
# and Qt 6.11.x on macOS frees that colour space's CGColorSpace before
# CGImageCreate reads it (QTBUG-147602, fixed upstream in qtbase a481ede673
# but not in Homebrew's 6.11.1). Every QImage::toCGImage() of such an image
# -- a cursor, a drag pixmap, an icon handed to AppKit -- can then crash the
# app. An untagged PNG loads with no colour space and never takes that path.
#
# Nothing is lost in the app: QPainter draws a QImage's pixels as they are,
# whatever colour space it carries, so the app never colour-managed these
# images. --strip only drops whole chunks; the IDAT (pixel) data is copied
# byte for byte.
#
#   tools/png-colour-chunks.py [--strip] [dir-or-file ...]
#
# With no paths it checks the source tree this script lives in: the PNGs git
# tracks in a checkout, else (a plain copy) every PNG outside build*, .git and
# a top-level qt/ (where CI unpacks Qt). Without --strip it lists every PNG that still
# carries one of the chunks and exits 1 if there is any -- unittest.sh runs it
# that way as part of the test gate.

import os
import struct
import subprocess
import sys
import zlib

SIGNATURE = b'\x89PNG\r\n\x1a\n'
COLOUR_CHUNKS = {b'iCCP', b'sRGB', b'gAMA', b'cHRM', b'cICP'}


def read_chunks(data, path):
    if not data.startswith(SIGNATURE):
        raise ValueError('%s: not a PNG' % path)
    pos = len(SIGNATURE)
    chunks = []
    while pos + 12 <= len(data):
        length, = struct.unpack('>I', data[pos:pos + 4])
        end = pos + 12 + length
        if end > len(data):
            raise ValueError('%s: truncated chunk at byte %d' % (path, pos))
        chunks.append((data[pos + 4:pos + 8], data[pos:end]))
        pos = end
        if chunks[-1][0] == b'IEND':
            break
    return chunks, data[pos:]


def strip(data, path):
    chunks, trailer = read_chunks(data, path)
    kept = [raw for kind, raw in chunks if kind not in COLOUR_CHUNKS]
    out = SIGNATURE + b''.join(kept) + trailer
    for _, raw in chunks:      # sanity: every chunk we keep is intact
        kind, body, crc = raw[4:8], raw[8:-4], raw[-4:]
        if struct.pack('>I', zlib.crc32(kind + body) & 0xffffffff) != crc:
            raise ValueError('%s: bad CRC in %s' % (path, kind.decode('latin1')))
    return out


def tracked_pngs(top):
    """PNGs git tracks under top, or None if top isn't a git checkout."""
    if not os.path.exists(os.path.join(top, '.git')):
        return None
    try:
        out = subprocess.run(['git', '-C', top, 'ls-files', '-z'],
                             capture_output=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return None
    return sorted(os.path.join(top, f) for f in out.decode('utf-8').split('\0')
                  if f.lower().endswith('.png'))


def pngs(paths):
    for p in paths:
        if os.path.isfile(p):
            yield p
            continue
        # A checkout: only what the repo ships. CI unpacks Qt itself inside
        # the checkout (./qt), and Qt's own PNGs are not ours to fix.
        tracked = tracked_pngs(p)
        if tracked is not None:
            for f in tracked:
                yield f
            continue
        # A plain copy (check-all.sh gates a git archive): walk it, skipping
        # build trees, .git and a top-level Qt install.
        for root, dirs, files in os.walk(p):
            top = os.path.samefile(root, p)
            dirs[:] = sorted(d for d in dirs
                             if not d.startswith('build') and d != '.git'
                             and not (top and d == 'qt'))
            for f in sorted(files):
                if f.lower().endswith('.png'):
                    yield os.path.join(root, f)


def main(argv):
    do_strip = '--strip' in argv
    paths = [a for a in argv if a != '--strip']
    if not paths:
        paths = [os.path.dirname(os.path.dirname(os.path.abspath(__file__)))]

    scanned = found = saved = 0
    for path in pngs(paths):
        scanned += 1
        with open(path, 'rb') as f:
            data = f.read()
        chunks, _ = read_chunks(data, path)
        kinds = sorted({k.decode('latin1') for k, _ in chunks} &
                       {k.decode('latin1') for k in COLOUR_CHUNKS})
        if not kinds:
            continue
        found += 1
        if do_strip:
            out = strip(data, path)
            with open(path, 'wb') as f:
                f.write(out)
            saved += len(data) - len(out)
            print('stripped %s: %s' % (' '.join(kinds), path))
        else:
            print('%s: %s' % (path, ' '.join(kinds)))

    if do_strip:
        print('%d PNGs scanned, %d stripped, %d bytes saved'
              % (scanned, found, saved))
        return 0
    if found:
        print('%d of %d PNGs carry a colour profile (iCCP/sRGB/gAMA/cHRM/cICP).'
              % (found, scanned))
        print('Qt 6.11 on macOS can crash converting such an image for AppKit'
              ' (QTBUG-147602). Fix: tools/png-colour-chunks.py --strip')
        return 1
    print('PNG colour profiles: none in %d PNGs' % scanned)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
