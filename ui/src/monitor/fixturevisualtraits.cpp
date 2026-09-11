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
    /* The declared lens RANGE. A fixed head has these equal (or only one set);
       a zoom head has both, and where it currently sits between them is a live
       question -- see fixtureBeamAngle(). 0 stays 0 = undeclared. */
    t.beamMinDeg = float(qMin(phys.lensDegreesMin(), phys.lensDegreesMax()));
    t.beamMaxDeg = float(qMax(phys.lensDegreesMin(), phys.lensDegreesMax()));

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
/* Mix two colours. Emitters BLEND toward their own colour rather than adding
 * into the RGB channels: adding is what makes a white-boosted red clip to pink
 * and then to white, which is not what the fixture is doing. */
static QColor blendColour(const QColor &a, const QColor &b, double mix)
{
    mix = qBound(0.0, mix, 1.0);
    return QColor(qRound(a.red()   * (1.0 - mix) + b.red()   * mix),
                  qRound(a.green() * (1.0 - mix) + b.green() * mix),
                  qRound(a.blue()  * (1.0 - mix) + b.blue()  * mix));
}

/* The shared core: reduce ONE set of channels to a colour and a level.
 *
 * Scoping this to a channel list rather than always walking the whole fixture
 * is the whole point. Taking the per-primary maximum across a 64-pixel bar
 * turns any pattern that is not uniform into white -- red pixels and blue
 * pixels together report max-red AND max-blue, so a step front running a
 * confetti effect drew as a pale wash instead of its actual colours.
 *
 * Colour models covered, which is ALL of QLCChannel::PrimaryColour:
 *   - additive RGB;
 *   - subtractive CMY -- every mover with a colour-mixing flag system. These
 *     were silently ignored, so such a fixture never showed its live colour at
 *     all, whatever it was doing;
 *   - White, Amber, UV, Lime and Indigo emitters, blended over the base;
 *   - a colour wheel's current capability.
 * (Modelled on QLC+ 5's FixtureUtils::headColor(), which had all of this right
 * while this had five of the twelve.) */
static bool channelSetLiveState(QLCFixtureMode *mode, const QByteArray &v,
                                const QList<quint32> &chans,
                                QColor &colour, uchar &dimmer)
{
    int r = -1, g = -1, b = -1;          // additive
    int cy = -1, ma = -1, ye = -1;       // subtractive
    int w = -1, am = -1, uv = -1, li = -1, ind = -1;
    int master = -1;                     // a plain (colourless) Intensity channel
    QColor wheel;                        // a colour-wheel capability, if one is set

    foreach (quint32 c, chans)
    {
        if (int(c) >= v.size())
            continue;
        QLCChannel *ch = mode->channel(c);
        if (ch == nullptr)
            continue;
        /* uchar cast matters: QByteArray::at() is signed, so anything over 127
           would go negative and darken the fixture past half. */
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
        case QLCChannel::Red:     r   = qMax(r, val);   break;
        case QLCChannel::Green:   g   = qMax(g, val);   break;
        case QLCChannel::Blue:    b   = qMax(b, val);   break;
        case QLCChannel::Cyan:    cy  = qMax(cy, val);  break;
        case QLCChannel::Magenta: ma  = qMax(ma, val);  break;
        case QLCChannel::Yellow:  ye  = qMax(ye, val);  break;
        case QLCChannel::White:   w   = qMax(w, val);   break;
        case QLCChannel::Amber:   am  = qMax(am, val);  break;
        case QLCChannel::UV:      uv  = qMax(uv, val);  break;
        case QLCChannel::Lime:    li  = qMax(li, val);  break;
        case QLCChannel::Indigo:  ind = qMax(ind, val); break;
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

    QColor mixed(0, 0, 0);
    bool found = false;

    if (r >= 0 || g >= 0 || b >= 0)
    {
        mixed.setRgb(qMax(0, r), qMax(0, g), qMax(0, b));
        found = true;
    }
    if (cy >= 0 || ma >= 0 || ye >= 0)
    {
        /* Subtractive: DMX 0 on every flag is FULL WHITE light, not black. */
        mixed.setCmyk(qMax(0, cy), qMax(0, ma), qMax(0, ye), 0);
        found = true;
    }
    if (w   >= 0) { if (w   > 0) mixed = blendColour(mixed, QColor(255, 255, 255), w   / 255.0); found = true; }
    if (am  >= 0) { if (am  > 0) mixed = blendColour(mixed, QColor(255, 126,   0), am  / 255.0); found = true; }
    if (uv  >= 0) { if (uv  > 0) mixed = blendColour(mixed, QColor(148,   0, 211), uv  / 255.0); found = true; }
    if (li  >= 0) { if (li  > 0) mixed = blendColour(mixed, QColor(173, 255,  47), li  / 255.0); found = true; }
    if (ind >= 0) { if (ind > 0) mixed = blendColour(mixed, QColor( 75,   0, 130), ind / 255.0); found = true; }

    if (found)
        colour = mixed;
    else if (wheel.isValid())
        colour = wheel;

    /* The level: an explicit dimmer if there is one, otherwise the brightest
       emitter -- an RGB fixture with no dimmer is as bright as its channels.
     *
       A subtractive fixture is the exception: its flags say what COLOUR passes,
       never how much, so with no dimmer channel the honest reading is "on". */
    const bool subtractiveOnly = (cy >= 0 || ma >= 0 || ye >= 0)
                                 && r < 0 && g < 0 && b < 0;
    if (master >= 0)
        dimmer = uchar(master);
    else if (subtractiveOnly)
        dimmer = 255;
    else if (colour.isValid())
        dimmer = uchar(qMax(colour.red(), qMax(colour.green(), colour.blue())));
    else
        dimmer = 0;

    /* THERE IS NO SUCH THING AS BLACK LIGHT.
     *
       A fixture whose emitters are all at zero is not emitting black -- it is
       not emitting. The dimmer channel says how much of the mix leaves the
       lamp, and there is no mix, so the answer is nothing however far up that
       dimmer is pushed.
     *
       Taken literally the other way round, a look with its colours stripped out
       but its dimmer still up produced a full-strength BLACK beam, which the
       renderer then dutifully painted over the stage as a dark cone. Note the
       test is on the MIXED colour and only when emitters were actually found:
       a dimmer-only fixture never gets here (it has no colour to be black), and
       a subtractive fixture with every flag out is passing WHITE, not black. */
    if (found && mixed.red() == 0 && mixed.green() == 0 && mixed.blue() == 0)
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
                          QColor &colour, uchar &dimmer, int masterLevel)
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
        const int master = (masterLevel >= -1) ? masterLevel
                                               : fixtureMasterLevel(fx, v);
        if (master >= 0)
            dimmer = uchar(qRound(dimmer * (master / 255.0)));
    }

    return true;
}

int fixtureMasterLevel(Fixture *fx, const QByteArray &v)
{
    if (fx == nullptr)
        return -1;
    QLCFixtureMode *mode = fx->fixtureMode();
    if (mode == nullptr)
        return -1;

    /* Hoisted deliberately: channels() returns the list BY VALUE, so leaving it
       in the loop condition copied it once per channel. With this called once
       per head of a 64-pixel bar it came to twelve thousand list copies per
       fixture per frame -- measured at 49 ms of a 90 ms frame across the rig,
       which is the whole difference between live and static. */
    const int count = qMin(mode->channels().size(), v.size());
    int master = -1;
    for (int c = 0; c < count; ++c)
    {
        QLCChannel *ch = mode->channel(quint32(c));
        if (ch != nullptr && ch->group() == QLCChannel::Intensity
            && ch->colour() == QLCChannel::NoColour)
            master = qMax(master, int(uchar(v.at(c))));
    }
    return master;
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

double fixtureBeamAngle(Fixture *fx, const QByteArray &v,
                        const FixtureVisualTraits &traits)
{
    const double lo = double(traits.beamMinDeg);
    const double hi = double(traits.beamMaxDeg);

    if (hi <= 0.0)
        return 0.0;                       // no lens declared at all
    if (lo <= 0.0 || qAbs(hi - lo) < 0.51)
        return hi;                        // a fixed cone

    if (fx == nullptr)
        return hi;
    QLCFixtureMode *mode = fx->fixtureMode();
    if (mode == nullptr || v.isEmpty())
        return hi;

    /* Where the zoom is sitting. Which way the channel runs is part of the
       definition and both directions are common, so it has to be read rather
       than assumed. */
    const int count = qMin(mode->channels().size(), v.size());
    for (int c = 0; c < count; ++c)
    {
        QLCChannel *ch = mode->channel(quint32(c));
        if (ch == nullptr)
            continue;
        double frac = -1.0;
        if (ch->preset() == QLCChannel::BeamZoomSmallBig)
            frac = uchar(v.at(c)) / 255.0;
        else if (ch->preset() == QLCChannel::BeamZoomBigSmall)
            frac = 1.0 - uchar(v.at(c)) / 255.0;
        if (frac >= 0.0)
            return lo + (hi - lo) * qBound(0.0, frac, 1.0);
    }

    /* A range declared but no zoom channel to drive it: the widest is the safer
       read, since a narrow cone hides a fixture that is plainly visible. */
    return hi;
}
