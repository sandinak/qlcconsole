/*
  Q Light Controller Plus
  fixturevisualtraits.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QSet>

#include "fixturevisualtraits.h"
#include "fixture.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "qlcphysical.h"

FixtureVisualTraits classifyFixture(Fixture *fx)
{
    FixtureVisualTraits t;
    if (fx == nullptr)
        return t;
    t.headCount = qMax(1, fx->heads());

    if (fx->fixtureDef() != nullptr)
    {
        switch (fx->fixtureDef()->type())
        {
        case QLCFixtureDef::MovingHead:
        case QLCFixtureDef::Scanner:
            t.kind = FixtureSilhouette::Mover;
            break;
        case QLCFixtureDef::ColorChanger:
        case QLCFixtureDef::Dimmer:
        case QLCFixtureDef::Strobe:
            t.kind = FixtureSilhouette::Par;
            break;
        case QLCFixtureDef::LEDBarBeams:
        case QLCFixtureDef::LEDBarPixels:
            t.kind = FixtureSilhouette::Bar;
            break;
        default:
            t.kind = FixtureSilhouette::Generic;
            break;
        }
    }

    QLCFixtureMode *mode = fx->fixtureMode();
    if (mode == nullptr)
        return t;

    const QLCPhysical &phys = mode->physical();
    if (phys.width()  > 0) t.physW = float(phys.width())  / 1000.0f;
    if (phys.height() > 0) t.physH = float(phys.height()) / 1000.0f;
    if (phys.depth()  > 0) t.physD = float(phys.depth())  / 1000.0f;

    /* The declared pixel grid, when it can actually hold this mode's heads.
       layoutSize() defaults to 1x1, and some definitions declare a layout that
       belongs to a different mode's head count. */
    const QSize declared = phys.layoutSize();
    if (declared.width() > 0 && declared.height() > 0 && declared != QSize(1, 1)
        && declared.width() * declared.height() >= fx->heads())
        t.layout = declared;

    QSet<int> colours;
    QSet<int> panTiltHeads;
    for (quint32 c = 0; c < fx->channels(); ++c)
    {
        QLCChannel *ch = mode->channel(c);
        if (ch == nullptr)
            continue;
        if (ch->group() == QLCChannel::Beam)
        {
            switch (ch->preset())
            {
            case QLCChannel::BeamFocusNearFar: case QLCChannel::BeamFocusFarNear:
            case QLCChannel::BeamFocusFine:
            case QLCChannel::BeamZoomSmallBig: case QLCChannel::BeamZoomBigSmall:
            case QLCChannel::BeamZoomFine:
                t.hasFocus = true;
                break;
            default:
                break;
            }
        }
        if (ch->group() == QLCChannel::Colour)
        {
            if (ch->colour() != QLCChannel::NoColour)
                colours.insert(int(ch->colour()));
            else
                t.hasWheel = true;   // a wheel/mix channel with no single primary colour
        }
        else if (ch->group() == QLCChannel::Intensity && ch->colour() != QLCChannel::NoColour)
        {
            colours.insert(int(ch->colour()));
        }
        if (ch->group() == QLCChannel::Pan || ch->group() == QLCChannel::Tilt)
        {
            const int h = mode->headForChannel(c);
            if (h >= 0)
                panTiltHeads.insert(h);
        }
    }
    t.rgbw = colours.size() >= 3;   // at least Red+Green+Blue
    t.multiHeadPanTilt = panTiltHeads.size() >= 2;
    return t;
}

// One base+yoke-arms+head unit inside $r -- see moverElevationPath() below,
// which calls this once per physical head, each in its own slice of the
// fixture's total width, instead of one unit stretched across all of them.
static void addMoverUnit(QPainterPath &path, const QRectF &r, bool hung)
{
    const qreal baseH = qMax(2.0, r.height() * 0.16);
    const QRectF baseRect = hung ? QRectF(r.left(), r.top(), r.width(), baseH)
                                  : QRectF(r.left(), r.bottom() - baseH, r.width(), baseH);
    path.addRoundedRect(baseRect, baseH * 0.25, baseH * 0.25);

    // Arms rise from the base nearly the full remaining height -- a real
    // yoke, not a partial stub with the head floating separately above it.
    const qreal armW = qMax(1.5, r.width() * 0.16);
    const qreal armMargin = r.height() * 0.04;
    const qreal armNear = hung ? r.top() + baseH : r.bottom() - baseH;    // at the base
    const qreal armFar  = hung ? r.bottom() - armMargin : r.top() + armMargin; // near the far edge
    const qreal armTop  = qMin(armNear, armFar);
    const qreal armBot  = qMax(armNear, armFar);
    if (armBot <= armTop)
        return;

    path.addRoundedRect(QRectF(r.left(), armTop, armW, armBot - armTop), armW * 0.3, armW * 0.3);
    path.addRoundedRect(QRectF(r.right() - armW, armTop, armW, armBot - armTop), armW * 0.3, armW * 0.3);

    // Head: nested BETWEEN the arms (not floating above them) -- pivots
    // near their far end and spans the same height they do, the way a real
    // moving head's barrel hangs between its yoke arms rather than
    // perching on top of it. Narrower than the full gap so the arms still
    // read as distinct from the head. An oval in this side-on elevation (a
    // barrel aimed along the tilt axis projects as an ellipse from the
    // side; only viewed down its own axis does it read as a circle -- see
    // moverPlanPath() for that top-down case).
    const qreal headW = qMax(2.0, r.width() - armW * 2.2);
    path.addEllipse(QRectF(r.center().x() - headW * 0.5, armTop, headW, armBot - armTop));
}

QPainterPath moverElevationPath(const QRectF &r, bool hung, int headCount)
{
    QPainterPath path;
    // Several subpaths (base/arms/head, per unit) get unioned into one
    // filled shape; the default OddEven rule punches a HOLE anywhere they
    // overlap (they aren't meant to punch holes in each other) --
    // WindingFill treats overlapping same-wound subpaths as one solid
    // union instead.
    path.setFillRule(Qt::WindingFill);

    // A genuinely multi-head fixture (e.g. a twin-head unit) gets one full
    // base+arms+head per head, each centred in its own equal slice of the
    // fixture's total width -- not one unit stretched across all of them.
    const int n = qMax(1, headCount);
    const qreal slotW = r.width() / n;
    for (int i = 0; i < n; ++i)
        addMoverUnit(path, QRectF(r.left() + slotW * i, r.top(), slotW, r.height()), hung);

    return path;
}

// One rounded-square base + notched-circle head inside $r -- see
// moverPlanPath() below, called once per physical head.
static void addMoverPlanUnit(QPainterPath &path, const QRectF &r)
{
    const qreal inset = qMin(r.width(), r.height()) * 0.1;
    const QRectF baseSq = r.adjusted(inset, inset, -inset, -inset);
    path.addRoundedRect(baseSq, baseSq.width() * 0.12, baseSq.height() * 0.12);

    const QPointF c = baseSq.center();
    const qreal headR = qMin(baseSq.width(), baseSq.height()) * 0.5 * 0.75;

    // Flat chord on the local "front" edge (rect +Y / bottom -- the same
    // downstage-default convention used for facing=0 elsewhere), reading as
    // where the beam exits without needing this shape to track live pan. A
    // barrel pointed straight up (see moverElevationPath()) projects as a
    // circle from directly above, so this stays round -- only its front
    // edge is flattened.
    QPainterPath head;
    head.addEllipse(c, headR, headR);
    QPainterPath cutAway;
    const qreal cutY = c.y() + headR * 0.3;
    cutAway.addRect(QRectF(c.x() - headR * 2, cutY, headR * 4, headR * 2));
    head = head.subtracted(cutAway);

    path.addPath(head);
}

QPainterPath moverPlanPath(const QRectF &r, int headCount)
{
    QPainterPath path;
    // Same reasoning as moverElevationPath(): each unit's base square and
    // head circle overlap on purpose (the head sits ON the base, seen from
    // above) and must union solid, not punch a hole in each other.
    path.setFillRule(Qt::WindingFill);

    const int n = qMax(1, headCount);
    const qreal slotW = r.width() / n;
    for (int i = 0; i < n; ++i)
        addMoverPlanUnit(path, QRectF(r.left() + slotW * i, r.top(), slotW, r.height()));

    return path;
}
