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
#include <QtMath>
#include "qlcchannel.h"
#include "truss.h"
#include "qlcfixturehead.h"
#include "qlccapability.h"
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
    /* Widest the lens goes: a zoom at its narrowest would dim the fixture from
       angles it can plainly be seen from. 0 stays 0 = undeclared. */
    t.beamDeg = float(qMax(phys.lensDegreesMax(), phys.lensDegreesMin()));

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

/* The shared core: reduce ONE set of channels to a colour and a level.
 *
 * Scoping this to a channel list rather than always walking the whole fixture
 * is the whole point. Taking the per-primary maximum across a 64-pixel bar
 * turns any pattern that is not uniform into white -- red pixels and blue
 * pixels together report max-red AND max-blue, so a step front running a
 * confetti effect drew as a pale wash instead of its actual colours. */
static bool channelSetLiveState(QLCFixtureMode *mode, const QByteArray &v,
                                const QList<quint32> &chans,
                                QColor &colour, uchar &dimmer)
{

    int r = -1, g = -1, b = -1, w = -1, a = -1;
    int master = -1;                 // a plain (colourless) Intensity channel
    QColor wheel;                    // a colour-wheel capability, if one is set

    foreach (quint32 c, chans)
    {
        if (int(c) >= v.size())
            continue;
        QLCChannel *ch = mode->channel(c);
        if (ch == nullptr)
            continue;
        const int val = uchar(v.at(int(c)));

        if (ch->group() == QLCChannel::Intensity && ch->colour() == QLCChannel::NoColour)
        {
            master = qMax(master, val);
            continue;
        }
        if (ch->group() != QLCChannel::Intensity && ch->group() != QLCChannel::Colour)
            continue;

        switch (ch->colour())
        {
        case QLCChannel::Red:   r = qMax(r, val); break;
        case QLCChannel::Green: g = qMax(g, val); break;
        case QLCChannel::Blue:  b = qMax(b, val); break;
        case QLCChannel::White: w = qMax(w, val); break;
        case QLCChannel::Amber: a = qMax(a, val); break;
        case QLCChannel::NoColour:
            // A wheel: take the colour its current capability names.
            if (ch->group() == QLCChannel::Colour)
            {
                if (QLCCapability *cap = ch->searchCapability(uchar(val)))
                {
                    const QColor c1 = cap->resource(0).value<QColor>();
                    if (c1.isValid() && c1 != Qt::black)
                        wheel = c1;
                }
            }
            break;
        default: break;
        }
    }

    /* uchar casts matter: QByteArray::at() is signed, so anything over 127
       would go negative and darken the fixture past half. */
    if (r >= 0 || g >= 0 || b >= 0 || w >= 0 || a >= 0)
    {
        int rr = qMax(0, r), gg = qMax(0, g), bb = qMax(0, b);
        if (w > 0) { rr += w; gg += w; bb += w; }
        if (a > 0) { rr += a; gg += qRound(a * 0.494); }
        colour = QColor(qMin(rr, 255), qMin(gg, 255), qMin(bb, 255));
    }
    else if (wheel.isValid())
    {
        colour = wheel;
    }

    /* The level: an explicit dimmer if there is one, otherwise the brightest
       emitter -- an RGB fixture with no dimmer is as bright as its channels. */
    if (master >= 0)
        dimmer = uchar(master);
    else if (colour.isValid())
        dimmer = uchar(qMax(colour.red(), qMax(colour.green(), colour.blue())));
    else
        dimmer = 0;

    return true;
}

bool fixtureLiveState(Fixture *fx, QColor &colour, uchar &dimmer)
{
    return fixtureLiveState(fx, fx != nullptr ? fx->channelValues() : QByteArray(),
                            colour, dimmer);
}

bool fixtureLiveState(Fixture *fx, const QByteArray &v, QColor &colour, uchar &dimmer)
{
    if (fx == nullptr)
        return false;
    QLCFixtureMode *mode = fx->fixtureMode();
    if (mode == nullptr)
        return false;

    if (v.isEmpty())
        return false;

    QList<quint32> all;
    all.reserve(int(fx->channels()));
    for (quint32 c = 0; c < fx->channels(); ++c)
        all << c;

    return channelSetLiveState(mode, v, all, colour, dimmer);
}

bool fixtureHeadLiveState(Fixture *fx, int head, QColor &colour, uchar &dimmer)
{
    return fixtureHeadLiveState(fx, fx != nullptr ? fx->channelValues() : QByteArray(),
                                head, colour, dimmer);
}

bool fixtureHeadLiveState(Fixture *fx, const QByteArray &v, int head,
                          QColor &colour, uchar &dimmer)
{
    if (fx == nullptr || head < 0)
        return false;
    QLCFixtureMode *mode = fx->fixtureMode();
    if (mode == nullptr || head >= mode->heads().size())
        return false;

    if (v.isEmpty())
        return false;

    const QList<quint32> chans = mode->heads().at(head).channels();
    if (chans.isEmpty())
        return false;

    if (channelSetLiveState(mode, v, chans, colour, dimmer) == false)
        return false;

    /* A pixel head usually carries only its own R/G/B; the master dimmer is a
       fixture-wide channel that belongs to no head. Without this, every pixel
       of a bar sitting at 10% would draw at full. */
    bool headHasMaster = false;
    foreach (quint32 c, chans)
    {
        QLCChannel *ch = mode->channel(c);
        if (ch != nullptr && ch->group() == QLCChannel::Intensity
            && ch->colour() == QLCChannel::NoColour)
        {
            headHasMaster = true;
            break;
        }
    }
    if (headHasMaster == false)
    {
        int master = -1;
        for (quint32 c = 0; c < quint32(mode->channels().size()) && int(c) < v.size(); ++c)
        {
            QLCChannel *ch = mode->channel(c);
            if (ch != nullptr && ch->group() == QLCChannel::Intensity
                && ch->colour() == QLCChannel::NoColour)
                master = qMax(master, int(uchar(v.at(int(c)))));
        }
        if (master >= 0)
            dimmer = uchar(qRound(dimmer * (master / 255.0)));
    }

    return true;
}

bool fixtureAimDirection(Fixture *fx, const FixtureRigProps &rp, QVector3D &dir)
{
    if (fx == nullptr)
        return false;
    QLCFixtureMode *mode = fx->fixtureMode();
    if (mode == nullptr || mode->heads().isEmpty())
        return false;

    const QLCFixtureHead &head = mode->heads().first();
    const quint32 panCh  = head.channelNumber(QLCChannel::Pan,  QLCChannel::MSB);
    const quint32 tiltCh = head.channelNumber(QLCChannel::Tilt, QLCChannel::MSB);
    if (panCh == QLCChannel::invalid() && tiltCh == QLCChannel::invalid())
        return false;                       // not a mover: nothing to point

    const QByteArray v = fx->channelValues();

    /* Same mapping MonitorFixtureItem uses, so the plot and the rig view agree
       about where a head is looking: the centre DMX value is 0 degrees and the
       range is centred on it. */
    auto degrees = [&](quint32 ch, double maxDeg) {
        if (ch == QLCChannel::invalid() || int(ch) >= v.size())
            return 0.0;
        const double val = double(uchar(v.at(int(ch))));
        return (val * maxDeg) / 255.0 - (maxDeg / 2.0);
    };
    const double panMax  = (mode->physical().focusPanMax()  != 0)
                         ? mode->physical().focusPanMax()  : 360.0;
    const double tiltMax = (mode->physical().focusTiltMax() != 0)
                         ? mode->physical().focusTiltMax() : 270.0;

    const double yaw  = qDegreesToRadians(double(rp.panZeroDir)
                                          + degrees(panCh, panMax)
                                          + double(rp.panOffsetDeg));
    const double tilt = qDegreesToRadians(degrees(tiltCh, tiltMax)
                                          + double(rp.tiltOffsetDeg));

    // Horizontal bearing: 0 = downstage (+Y), turning clockwise from above.
    const QVector3D horiz(float(-qSin(yaw)), float(qCos(yaw)), 0.0f);
    // Tilt lifts the beam off straight-down toward that bearing.
    dir = QVector3D(horiz.x() * float(qSin(tilt)),
                    horiz.y() * float(qSin(tilt)),
                    float(-qCos(tilt)));
    if (dir.length() < 1e-6f)
        return false;
    dir.normalize();
    return true;
}
