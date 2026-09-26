/*
  Q Light Controller Plus
  beamoptics.h

  What a fixture's gobo wheels, gobo rotation and prism do to its beam, as one
  mask across the beam: where light gets through, and how much of it.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef BEAMOPTICS_H
#define BEAMOPTICS_H

#include <QByteArray>
#include <QImage>
#include <QString>

class Fixture;
class QLCCapability;

namespace BeamOptics
{
    /** The mask's side, in pixels. It covers the beam's cross-section as the
     *  square [-1, 1] x [-1, 1]: x along the beam's "side" axis, y along its
     *  "up" axis, the cone's edge at radius 1. */
    static const int kMaskSize = 64;

    struct Optics
    {
        /** Grayscale8, kMaskSize square; 255 = light passes. Null = an open
         *  beam (no gobo, no prism): draw it plain. */
        QImage mask;
        /** Share of the open beam the mask lets through, 0..1 (1 when open). */
        double coverage = 1.0;
        /** Something is turning (a gobo or the prism): keep redrawing. */
        bool animated = false;
        /** Prism facets in use: 0 = none, 1 = linear (three in a row), else N. */
        int prismFacets = 0;
    };

    /** The optics of @p fx for its channel values @p values at time
     *  @p seconds (turning gobos and prisms turn with it). Gobo wheels: every
     *  Gobo channel that carries pictures, at the slot its value selects.
     *  Gobo rotation: a Gobo channel without pictures that indexes or spins,
     *  applied to the nearest picture wheel before it. Prism: the Prism
     *  channel that inserts one (facets from its capability name -- "6 facet
     *  prism", "Linear prism"), turned by a Prism channel that rotates. */
    Optics opticsFor(const Fixture *fx, const QByteArray &values, double seconds);

    /** How much light a gobo picture lets through, as a kMaskSize Grayscale8
     *  mask: clear or white passes, black blocks (how gobo images are drawn:
     *  the metal black, the holes clear). Cached by path; null if it can't be
     *  read. */
    QImage goboMask(const QString &path);

    /** Facets a prism capability inserts: 0 = none, 1 = linear, else N. */
    int prismFacets(const QLCCapability *cap);

    /** Degrees per second a rotation capability turns at @p value: positive
     *  clockwise, negative counter-clockwise, 0 stopped. */
    double rotationSpeed(const QLCCapability *cap, int value);

    /** A fixed angle, in degrees, when @p cap indexes (RotationIndexed);
     *  otherwise -1. */
    double indexedAngle(const QLCCapability *cap, int value);

    /** Sample @p mask (see kMaskSize) at beam coordinates (@p x, @p y),
     *  bilinear, 0..1. Outside the cone (radius > 1) is dark. */
    double sample(const QImage &mask, double x, double y);
}

#endif
