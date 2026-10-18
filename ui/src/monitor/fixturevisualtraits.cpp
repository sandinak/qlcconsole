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

#include "headlayout.h"
#include "fixturevisualtraits.h"
#include "righeads.h"
#include "aimsolver.h"
#include "fixture.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include <QtMath>
#include <QRegularExpression>
#include "qlcchannel.h"
#include "truss.h"
#include "qlcfixturehead.h"
#include "qlccapability.h"
#include "qlcpalette.h"
#include "monitorproperties.h"
#include "doc.h"
#include <QVector3D>
#include "qlcphysical.h"

FixtureVisualTraits classifyFixture(Fixture *fx)
{
    FixtureVisualTraits t;
    if (fx == nullptr)
        return t;
    t.headCount = qMax(1, fx->heads());

    /* Standing in for a definition this computer does not have (the show
       came from another machine): a generic dimmer of the same channel
       count, whose made-up physical is 300 mm per channel. A 64-channel bar
       then drew as a 19 m wall of 64 heads, every one overlapping its
       neighbours and outlined in the overlap warning's red -- the whole rig
       looked broken rather than "these definitions are missing". Drawn as
       one small box instead: nothing is known about it but where it hangs. */
    if (fx->definitionUnavailable())
    {
        t.kind = FixtureSilhouette::Generic;
        t.headCount = 1;
        t.physW = t.physH = t.physD = 0.25f;
        return t;
    }

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
    t.beamsPerHead = phys.lensBeamsPerHead();
    /* The definition's own word on what it looks like and how it lights,
       over what its Type suggests: a WLED lantern is a colour changer by
       type and a lantern by body. */
    if (phys.bodyShape() == QLCPhysical::LanternBody)
        t.kind = FixtureSilhouette::Lantern;
    t.omni = (phys.emission() == QLCPhysical::OmniEmission);
    t.laser = (phys.emission() == QLCPhysical::LaserEmission);
    t.stars = (phys.emission() == QLCPhysical::StarEmission);
    t.flame = (phys.emission() == QLCPhysical::FlameEmission);

    /* The declared pixel grid, when it is THIS mode's: one grid is declared
       for every mode, and a 32-cell grid on a 16-head mode drew a bar as two
       rows of eight (HeadLayout::declaredGrid()). */
    const QSize declared = HeadLayout::declaredGrid(mode);
    if (declared.isValid())
        t.layout = declared;

    QSet<int> colours;
    QSet<int> panTiltHeads;
    bool shapesItsBeam = false;     // gobo, focus or iris: a profile
    for (quint32 c = 0; c < fx->channels(); ++c)
    {
        QLCChannel *ch = mode->channel(c);
        if (ch == nullptr)
            continue;
        switch (ch->preset())
        {
        case QLCChannel::BeamFocusNearFar: case QLCChannel::BeamFocusFarNear:
        case QLCChannel::BeamFocusFine:
        case QLCChannel::ShutterIrisMinToMax: case QLCChannel::ShutterIrisMaxToMin:
        case QLCChannel::ShutterIrisFine:
            shapesItsBeam = true;
            break;
        default:
            break;
        }
        if (ch->group() == QLCChannel::Gobo)
            shapesItsBeam = true;
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
    /* A colour ring is not a mover unit: a two-head definition (spot +
       ring) is still one head on its yoke. */
    t.ringChannel = findColourRing(fx, &t.ringHead);
    if (t.ringHead >= 0 && t.headCount > 1)
        t.headCount -= 1;
    t.rgbw = colours.size() >= 3;   // at least Red+Green+Blue
    t.multiHeadPanTilt = panTiltHeads.size() >= 2;
    t.edgeSoftness = poolEdgeSoftness(shapesItsBeam,
                                      fx->fixtureDef() != nullptr ? int(fx->fixtureDef()->type()) : -1,
                                      phys.lensName(), t.beamMaxDeg);
    return t;
}

float poolEdgeSoftness(bool shapesItsBeam, int fixtureType, const QString &lensName,
                       double beamMaxDeg)
{
    float soft = 1.0f;                                  // a wash
    if (shapesItsBeam || fixtureType == int(QLCFixtureDef::LEDBarBeams))
        soft = 0.1f;                                    // a profile, a beam bar
    else if (lensName.compare(QLatin1String("PC"), Qt::CaseInsensitive) == 0
             || lensName.compare(QLatin1String("Fresnel"), Qt::CaseInsensitive) == 0)
        soft = 1.25f;
    if (beamMaxDeg > 0.0 && beamMaxDeg < 12.0)
        soft = qMin(soft, 0.4f);                        // a tight beam is fairly crisp
    return soft;
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
                                QColor &colour, uchar &dimmer,
                                bool valuesAreComplete = true)
{
    int r = -1, g = -1, b = -1;          // additive
    int cy = -1, ma = -1, ye = -1;       // subtractive
    int w = -1, am = -1, uv = -1, li = -1, ind = -1;
    int master = -1;                     // a plain (colourless) Intensity channel
    QColor wheel;                        // a colour-wheel capability, if one is set
    bool shutterClosed = false;          // a shut shutter emits nothing at all

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
        /* A CLOSED SHUTTER emits nothing, whatever the dimmer says -- it is a
           piece of metal in front of the lamp. Strobe capabilities are the
           opposite case and must NOT count: a strobing fixture is emitting,
           just intermittently, and reading it as dark would blank every strobe
           look in the rig. Only an explicit ShutterClose closes it. */
        if (ch->group() == QLCChannel::Shutter)
        {
            /* Only when the values are the WHOLE picture.
             *
               A look resolved from palettes fills in the channels it sets and
               leaves the rest at zero -- and zero on a shutter channel is
               usually the "closed" capability. Reading that as a shut shutter
               turned off every fixture whose look does not happen to mention
               one: a spot went from a beam to a bare aim line. An unset channel
               is UNKNOWN, not closed. Live DMX is a complete picture and does
               mean what it says. */
            if (valuesAreComplete)
            {
                if (QLCCapability *cap = ch->searchCapability(uchar(val)))
                    if (cap->preset() == QLCCapability::ShutterClose)
                        shutterClosed = true;
            }
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
            /* A wheel: take the colour its current capability shows -- its
               picture colour, else the colour its name says ("Blue"; a
               ring's "off" is black, so no colour). The first wheel lit
               wins: a mover's own wheel comes before its ring. */
            if (ch->group() == QLCChannel::Colour && !wheel.isValid())
            {
                if (QLCCapability *cap = ch->searchCapability(uchar(val)))
                {
                    const QColor c1 = QLCPalette::capabilityColour(cap);
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

    /* And nothing gets past a shut shutter. */
    if (shutterClosed)
        dimmer = 0;

    /* WITHOUT A DIMMER, THE LEVEL IS IN THE COLOUR -- SAY IT ONCE.
     *
       Every reader draws colour x dimmer. With a dimmer channel that is right:
       the colour channels say what mix, the dimmer how much of it. With none,
       the level above IS the brightest emitter, and the colour carried it as
       well -- so an RGB bar at 19% blue (0,1,48) drew as 48 x 19% = 9: black
       on the Rig Overview while the bar on stage was plainly blue. (A Dimmer
       look on a fixture with no dimmer channel scales its colour, so every
       dimmed pixel bar showed it.) Hand back the hue at full, and the level
       once, in the dimmer. */
    if (master < 0 && !subtractiveOnly && found && dimmer > 0 && dimmer < 255)
    {
        const double up = 255.0 / dimmer;
        colour = QColor(qMin(255, qRound(colour.red() * up)),
                        qMin(255, qRound(colour.green() * up)),
                        qMin(255, qRound(colour.blue() * up)));
    }

    return true;
}

bool fixtureLiveState(Fixture *fx, QColor &colour, uchar &dimmer)
{
    return fixtureLiveState(fx, fx != nullptr ? fx->channelValues() : QByteArray(),
                            colour, dimmer);
}

bool fixtureLiveState(Fixture *fx, const QByteArray &v, QColor &colour, uchar &dimmer,
                      bool valuesAreComplete)
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

    return channelSetLiveState(mode, v, all, colour, dimmer, valuesAreComplete);
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

bool fixtureHeadBeamState(Fixture *fx, const QByteArray &v, int head,
                          QColor &colour, uchar &dimmer, bool valuesAreComplete)
{
    if (fx == nullptr || head < 0)
        return false;
    QLCFixtureMode *mode = fx->fixtureMode();
    if (mode == nullptr || head >= mode->heads().size() || v.isEmpty())
        return false;

    auto isMaster = [&](quint32 c) {
        QLCChannel *ch = mode->channel(c);
        return ch != nullptr && ch->group() == QLCChannel::Intensity
               && ch->colour() == QLCChannel::NoColour;
    };

    QList<quint32> chans = mode->heads().at(head).channels();
    bool headHasMaster = false;
    foreach (quint32 c, chans)
        headHasMaster = headHasMaster || isMaster(c);

    const int count = qMin(mode->channels().size(), v.size());
    int master = -1;
    for (int c = 0; c < count; ++c)
    {
        if (mode->headForChannel(quint32(c)) != -1)
            continue;
        if (isMaster(quint32(c)))
            master = qMax(master, int(uchar(v.at(c))));
        else
            chans << quint32(c);        // shutter, wheel, shared colour
    }

    if (channelSetLiveState(mode, v, chans, colour, dimmer, valuesAreComplete) == false)
        return false;
    if (headHasMaster == false && master >= 0)
        dimmer = uchar(qRound(dimmer * (master / 255.0)));
    return true;
}

bool fixtureAimDirection(Fixture *fx, const FixtureRigProps &rp, QVector3D &dir,
                         int headIndex)
{
    if (fx == nullptr)
        return false;
    QLCFixtureMode *mode = fx->fixtureMode();
    if (mode == nullptr || mode->heads().isEmpty())
        return false;

    const QLCFixtureHead *head = &mode->heads().at(
        (headIndex >= 0 && headIndex < mode->heads().size()) ? headIndex : 0);
    if (head->channelNumber(QLCChannel::Pan,  QLCChannel::MSB) == QLCChannel::invalid()
        && head->channelNumber(QLCChannel::Tilt, QLCChannel::MSB) == QLCChannel::invalid())
        head = &mode->heads().first();
    const quint32 panCh  = head->channelNumber(QLCChannel::Pan,  QLCChannel::MSB);
    const quint32 tiltCh = head->channelNumber(QLCChannel::Tilt, QLCChannel::MSB);
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

    /* Exactly the inverse of AimSolver::solve, step for step, because these
       two are the same model read in opposite directions: the solver turns
       "point there" into these DMX values and this turns them back into the
       ray every view draws. Anything the solver applies on the way in has to
       be undone here on the way out, in reverse order -- the mirrors first,
       then the offsets, then the way up the head is rigged. */
    const bool standing = AimSolver::floorMounted(rp);

    double panDeg = degrees(panCh, panMax);
    double tiltDeg = degrees(tiltCh, tiltMax);

    // Flipping a head over reverses its pan sense; the invert flag toggles
    // from there. Mirroring about the centre is negation in centred degrees.
    bool invertPan = rp.panInvert;
    if (standing)
        invertPan = !invertPan;
    if (invertPan)
        panDeg = -panDeg;
    if (rp.tiltInvert)
        tiltDeg = -tiltDeg;

    const double yaw = qDegreesToRadians(double(rp.panZeroDir) + panDeg
                                         - double(rp.panOffsetDeg));
    /* Elevation of the beam above the horizon. A hanging head is centred on
       straight down, a standing one on straight up -- which is why a head
       standing on a truss top cannot point at its own feet however much tilt
       it has. */
    const double elevDeg = standing
        ? (tiltDeg - double(rp.tiltOffsetDeg) + 90.0)
        : (tiltDeg - double(rp.tiltOffsetDeg) - 90.0);
    const double elev = qDegreesToRadians(elevDeg);

    // Horizontal bearing: 0 = downstage (+Y), turning clockwise from above.
    const QVector3D horiz(float(-qSin(yaw)), float(qCos(yaw)), 0.0f);
    dir = QVector3D(horiz.x() * float(qCos(elev)),
                    horiz.y() * float(qCos(elev)),
                    float(qSin(elev)));
    if (dir.length() < 1e-6f)
        return false;
    dir.normalize();
    return true;
}

double fixtureBeamAngle(Fixture *fx, const QByteArray &v,
                        const FixtureVisualTraits &traits)
{
    if (double(traits.beamMaxDeg) <= 0.0)
        return 0.0;                       // no lens declared at all
    if (fx == nullptr || v.isEmpty())
        return double(traits.beamMaxDeg); // the widest is the safer read
    /* The engine's rule -- zoom, and an iris -- so the beam drawn is the one
       the follow spot aims (RigHeads::beamAngle()). */
    return RigHeads::beamAngle(fx, v);
}

QVector3D fixtureWorldCentre(Doc *doc, quint32 fid)
{
    // The rule lives in the engine, where effects lay their grids out from it.
    return RigHeads::fixtureCentre(doc, fid);
}

/*****************************************************************************
 * Colour rings
 *****************************************************************************/

int findColourRing(const Fixture *fx, int *head)
{
    if (head != nullptr)
        *head = -1;
    const QLCFixtureMode *mode = fx != nullptr ? const_cast<Fixture *>(fx)->fixtureMode() : nullptr;
    if (mode == nullptr)
        return -1;

    /* A ring is driven by a colour channel with no colour of its own (a
       macro or wheel). Most fixtures have none -- a pixel bar's channels are
       all Red/Green/Blue/White -- so stop here, before the per-head typing
       below, which is heads x channels and runs on every paint. */
    bool anyMacro = false;
    for (quint32 c = 0; !anyMacro && c < quint32(mode->channels().size()); ++c)
    {
        const QLCChannel *ch = mode->channel(c);
        anyMacro = ch != nullptr && ch->group() == QLCChannel::Colour
                   && ch->colour() == QLCChannel::NoColour;
    }
    if (!anyMacro)
        return -1;

    auto firstColour = [mode](const QList<quint32> &chans) {
        for (quint32 c : chans)
        {
            const QLCChannel *ch = mode->channel(c);
            if (ch != nullptr && ch->group() == QLCChannel::Colour && ch->controlByte() == QLCChannel::MSB)
                return int(c);
        }
        return -1;
    };

    // 1. The definition says so: a ring (or colour-macro) head.
    for (int h = 0; h < mode->heads().size(); ++h)
    {
        const QString type = HeadKinds::typeOf(mode, h);
        if (type == QLatin1String("Ring")
            || (type == QLatin1String("Macro") && mode->heads().size() > 1))
        {
            const int c = firstColour(mode->heads().at(h).channels());
            if (c >= 0)
            {
                if (head != nullptr)
                    *head = h;
                return c;
            }
        }
    }

    // 2./3. One that doesn't: by name, else by its slots.
    int first = -1;
    for (quint32 c = 0; c < quint32(mode->channels().size()); ++c)
    {
        const QLCChannel *ch = mode->channel(c);
        if (ch == nullptr || ch->group() != QLCChannel::Colour || ch->controlByte() != QLCChannel::MSB
            || ch->colour() != QLCChannel::NoColour)
            continue;
        const QString name = ch->name().toLower();
        if (name.contains(QLatin1String("circle")) || name.contains(QLatin1String("ring"))
            || name.contains(QLatin1String("halo")))
            return int(c);
        if (first < 0)
        {
            first = int(c);
            continue;
        }
        bool off = false, chase = false;
        for (const QLCCapability *cap : ch->capabilities())
        {
            const QColor col = QLCPalette::capabilityColour(cap);
            if (col.isValid() && col.value() < 16)
                off = true;
            if (cap->name().contains(QLatin1String("chase"), Qt::CaseInsensitive))
                chase = true;
        }
        if (off && chase)
            return int(c);
    }
    return -1;
}

/* A word of a slot's name as a colour ("Red", "Purple"), or invalid. */
static QColor ringWordColour(const QString &word)
{
    QLCCapability cap(0, 0, word);
    return QLCPalette::capabilityColour(&cap);
}

QVector<QColor> colourRingSegments(const QString &slotName, double seconds, int segments,
                                   bool *animated)
{
    if (animated != nullptr)
        *animated = false;
    segments = qMax(1, segments);
    QVector<QColor> out(segments, QColor(Qt::black));

    QLCCapability cap(0, 0, slotName);
    const QColor solid = QLCPalette::capabilityColour(&cap);
    if (solid.isValid())
    {
        out.fill(solid);                        // black for off
        return out;
    }

    // Not one colour: a chase of the colours it names.
    QString n = slotName.toLower();
    n.replace(QLatin1String("chase"), QLatin1String(" "));
    const QStringList words = n.split(QRegularExpression(QStringLiteral("[/\\-+&,\\s]+")),
                                      Qt::SkipEmptyParts);
    QVector<QColor> cols;
    bool random = false;
    for (const QString &w : words)
    {
        if (w == QLatin1String("rgb"))
            cols << QColor(Qt::red) << QColor(Qt::green) << QColor(Qt::blue);
        else if (w == QLatin1String("random") || w == QLatin1String("rainbow"))
            random = true;
        else
        {
            const QColor c = ringWordColour(w);
            if (c.isValid() && c.value() >= 16)
                cols << c;
        }
    }
    if (cols.isEmpty() && !random)
        return out;                             // nothing we can read: dark

    if (animated != nullptr)
        *animated = true;
    const int step = int(qFloor(seconds * 4.0));
    for (int i = 0; i < segments; ++i)
    {
        if (random)
        {
            // A fresh colour per segment each step: the same for one instant.
            quint32 x = quint32(i * 2654435761u) ^ quint32(step * 40503u);
            x ^= x >> 13;
            x *= 0x5bd1e995u;
            x ^= x >> 15;
            out[i] = cols.isEmpty() || (x & 1u) ? QColor::fromHsv(int(x % 360u), 255, 255)
                                                 : cols.at(int((x >> 1) % quint32(cols.size())));
        }
        else
        {
            out[i] = cols.at(((i + step) % cols.size() + cols.size()) % cols.size());
        }
    }
    return out;
}

QVector<QColor> colourRingLive(const Fixture *fx, const QByteArray &values, double seconds,
                               int segments, bool *animated)
{
    if (animated != nullptr)
        *animated = false;
    const int c = findColourRing(fx);
    if (c < 0 || c >= values.size())
        return QVector<QColor>();
    const QLCChannel *ch = const_cast<Fixture *>(fx)->fixtureMode()->channel(quint32(c));
    if (ch == nullptr)
        return QVector<QColor>();
    const QLCCapability *cap = ch->searchCapability(uchar(values.at(c)));
    return colourRingSegments(cap != nullptr ? cap->name() : QString(), seconds, segments, animated);
}
