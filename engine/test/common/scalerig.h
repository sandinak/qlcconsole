/*
  Q Light Controller Plus - test support
  scalerig.h

  A made-up rig the size and shape of the shows we run (board Q360): about
  260 fixtures and 26,000 channels over 80 universes, 50 of the universes
  holding only one or two devices because of cabling (a pixel bar co-located
  with its Art-Net node gets a universe of its own: power and DMX in, not
  three cables). Built from the fixture library and the generic RGB panel,
  never from a show file. ui/test/showscale holds the gate's size check to it.

  Licensed under the Apache License, Version 2.0
*/

#ifndef QLC_TEST_SCALERIG_H
#define QLC_TEST_SCALERIG_H

#include <QList>
#include <QString>

class Doc;

namespace ScaleRig
{
/* The shape, universe by universe (zero-based):
     0-29   one upstage pixel bar each ("UST"): a generic RGBW panel of
            96 pixels, 384 channels -- 30 sparse universes;
     30-39  two movers each (Martin ERA 300 Profile, 21 ch: pan/tilt + fine,
            zoom, gobos, prism) -- sparse;
     40-49  two RGBW washes each (American DJ Micro Wash RGBW, 7 ch) -- sparse;
     50-75  two pixel bars each, 64 RGBW pixels (256 ch): 512 channels;
     76-77  27 WLED Lanterns each (Effect, 15 ch);
     78     12 movers and 12 washes;
     79     60 house dimmers (1 ch).
   260 fixtures, 26,598 channels, 6,208 pixel heads. */
const int kUniverses = 80;
const int kSparseUniverses = 50;
const int kFixtures = 260;
const int kChannels = 26598;
const int kPixelHeads = 6208;
const int kCues = 40;

struct Built
{
    QList<quint32> usts, bars, movers, washes, lanterns, dimmers;

    // Fixture groups, the Show Tree's targets
    quint32 pixelGroup = 0;     ///< every pixel head: a row per bar (96 wide)
    quint32 moverGroup = 0;
    quint32 washGroup = 0;
    quint32 lanternGroup = 0;
    quint32 houseGroup = 0;
    QList<quint32> groups;      ///< every group, the above first

    quint32 matrix = 0;         ///< RGB matrix (a script) over pixelGroup
    quint32 efx = 0;            ///< pan/tilt EFX on every mover
    QList<quint32> chasers;     ///< three chasers of looks, per-step fades

    quint32 cueList = 0;        ///< the Show Tree list: kCues cues with fades
    quint32 cueChaser = 0;      ///< ...and the chaser it bakes to
};

/** Build the rig and its show into @a doc, whose fixture definitions and
 *  RGB scripts are loaded. False, with @a why, if a definition is missing. */
bool build(Doc *doc, Built *out, QString *why);

/** The universes holding at most two fixtures */
int sparseUniverses(Doc *doc);
/** Every fixture's channels, added up */
int channels(Doc *doc);
}

#endif
