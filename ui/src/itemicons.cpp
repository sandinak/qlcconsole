/*
  Q Light Controller Plus
  itemicons.cpp

  Copyright (C) Branson Matheson

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

#include <QGuiApplication>
#include <QLinearGradient>
#include <QPainterPath>
#include <QIconEngine>
#include <QPalette>
#include <QtMath>
#include <QPainter>
#include <QPixmap>
#include <QHash>
#include <QSet>
#include <functional>

#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "showtreemodel.h"
#include "function.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlccapability.h"
#include "fixturegroup.h"
#include "stagetarget.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "itemicons.h"
#include "fixture.h"
#include "doc.h"

namespace
{

/* Every icon is drawn on a 32x32 canvas, scaled to the size asked for. */
typedef std::function<void(QPainter &, const QColor &ink)> Draw;

const QColor kLight(255, 204, 51);      // the beam, the level, the target
const QColor kAmber(150, 105, 0);       // their edge: reads on light and dark
const QColor kBody(122, 127, 135);      // a fixture's body: mid grey

/** Paints its icon on demand, at the size and pixel ratio asked for, with
 *  the theme's text colour as "ink" at that moment (a theme change needs
 *  no rebuild). Disabled: the same, faded. */
class PaintedIconEngine : public QIconEngine
{
public:
    PaintedIconEngine(const QString &key, const Draw &draw)
        : m_key(key), m_draw(draw) {}

    void paint(QPainter *p, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal side = qMin(rect.width(), rect.height());
        if (side <= 0)
            return;
        if (mode == QIcon::Disabled)
        {
            // Faded as a whole (not each stroke, which stacks up).
            const qreal dpr = p->device() != nullptr ? p->device()->devicePixelRatioF() : 1.0;
            const QPixmap pm = scaledPixmap(rect.size(), QIcon::Normal, state, dpr);
            p->save();
            p->setOpacity(p->opacity() * 0.4);
            p->drawPixmap(rect.topLeft(), pm);
            p->restore();
            return;
        }
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->translate(rect.x() + (rect.width() - side) / 2.0, rect.y() + (rect.height() - side) / 2.0);
        p->scale(side / 32.0, side / 32.0);
        m_draw(*p, QGuiApplication::palette().color(QPalette::Text));
        p->restore();
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        QPixmap pm(size * scale);
        pm.setDevicePixelRatio(scale);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        paint(&p, QRect(QPoint(0, 0), size), mode, state);
        return pm;
    }

    QSize actualSize(const QSize &size, QIcon::Mode, QIcon::State) override { return size; }
    QIconEngine *clone() const override { return new PaintedIconEngine(m_key, m_draw); }
    QString key() const override { return QStringLiteral("ItemIcons"); }
    QString iconName() override { return m_key; }

private:
    QString m_key;
    Draw m_draw;
};

/** The one icon for @p key, drawn by @p draw the first time it is asked for. */
QIcon cached(const QString &key, const Draw &draw)
{
    static QHash<QString, QIcon> cache;
    auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return *it;
    QIcon icon(new PaintedIconEngine(key, draw));
    cache.insert(key, icon);
    return icon;
}

QColor alpha(QColor c, int a)
{
    c.setAlpha(a);
    return c;
}

/* A fixture's beam, from a lens at (16, lensY), @p half wide at the top:
   solid enough to read at 16 px, fading toward its end. */
void drawBeam(QPainter &p, qreal lensY, qreal lensHalf, qreal half)
{
    QPolygonF beam;
    beam << QPointF(16 - lensHalf, lensY) << QPointF(16 - half, 0)
         << QPointF(16 + half, 0) << QPointF(16 + lensHalf, lensY);
    QLinearGradient g(0, lensY, 0, 0);
    g.setColorAt(0, kLight);
    g.setColorAt(1, alpha(kLight, 150));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawPolygon(beam);
}

/* The fixtures, bottom half the body, top half the beam -- the beam's
   shape is what tells them apart at 16 px. */
void drawKind(QPainter &p, const QColor &ink, ItemIcons::FixtureKind kind)
{
    const QPen edge(alpha(ink, 230), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    if (kind == ItemIcons::FixedWash)
    {
        // A PAR can pointing up, a wide beam; no yoke.
        drawBeam(p, 15, 8, 16);
        p.setPen(edge);
        p.setBrush(kBody);
        QPolygonF can;
        can << QPointF(11, 31) << QPointF(21, 31) << QPointF(25, 16) << QPointF(7, 16);
        p.drawPolygon(can);
        return;
    }

    // A moving head: a yoke on a base, the head in it, and its beam.
    if (kind == ItemIcons::MovingWash)
        drawBeam(p, 14, 6, 16);
    else if (kind == ItemIcons::MovingHead)
        drawBeam(p, 14, 4, 8);
    else
        drawBeam(p, 14, 3, 3);
    p.setPen(edge);
    p.setBrush(Qt::NoBrush);
    QPainterPath yoke;
    yoke.moveTo(5, 16);
    yoke.lineTo(5, 27);
    yoke.lineTo(27, 27);
    yoke.lineTo(27, 16);
    p.setPen(QPen(alpha(ink, 230), 3.5, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    p.drawPath(yoke);
    p.setPen(edge);
    p.setBrush(kBody);
    p.drawEllipse(QPointF(16, 19.5), 7.5, 6.5);
    p.drawRect(QRectF(10, 28.5, 12, 2.5));
    if (kind == ItemIcons::MovingSpot)
    {
        // The gobo it projects: a disc at the beam's end with its pattern.
        p.setPen(QPen(kAmber, 2));
        p.setBrush(kLight);
        p.drawEllipse(QPointF(16, 6), 5.5, 5.5);
        p.setBrush(kAmber);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(16, 6), 1.8, 1.8);
    }
}

/* A group's marker: a small badge of three heads, bottom right. */
void drawGroupBadge(QPainter &p, const QColor &ink)
{
    const QRectF badge(17, 19, 15, 13);
    p.setPen(QPen(alpha(ink, 230), 1.5));
    p.setBrush(QGuiApplication::palette().color(QPalette::Base));
    p.drawRoundedRect(badge, 3, 3);
    p.setPen(Qt::NoPen);
    p.setBrush(alpha(ink, 230));
    for (const QPointF &c : { QPointF(21, 25.5), QPointF(24.5, 25.5), QPointF(28, 25.5) })
        p.drawEllipse(c, 1.6, 1.6);
}

QColor shutterRing(const QColor &ink) { return alpha(ink, 210); }

} // namespace

namespace ItemIcons
{

/*********************************************************************
 * What a thing is
 *********************************************************************/

FixtureKind fixtureKind(const Fixture *fixture)
{
    if (fixture == nullptr || fixture->fixtureDef() == nullptr)
        return OtherKind;
    switch (fixture->type())
    {
        case QLCFixtureDef::ColorChanger:
        case QLCFixtureDef::MovingHead:
        case QLCFixtureDef::Other:
            break;
        default:
            return OtherKind;     // a dimmer, a bar, a hazer...: its own icon
    }

    bool pan = false, gobo = false, zoom = false, rgb = false, colour = false;
    for (quint32 i = 0; i < fixture->channels(); ++i)
    {
        const QLCChannel *ch = fixture->channel(i);
        if (ch == nullptr)
            continue;
        const QLCChannel::Preset preset = ch->preset();
        const QString name = ch->name().toLower();
        switch (ch->group())
        {
            case QLCChannel::Pan:
            case QLCChannel::Tilt:   pan = true; break;
            case QLCChannel::Gobo:
            case QLCChannel::Prism:  gobo = true; break;
            case QLCChannel::Colour: colour = true; break;
            default: break;
        }
        if (preset == QLCChannel::ShutterIrisMinToMax || preset == QLCChannel::ShutterIrisMaxToMin
            || preset == QLCChannel::ShutterIrisFine || name.contains(QLatin1String("iris"))
            || name.contains(QLatin1String("prism")) || name.contains(QLatin1String("gobo")))
            gobo = true;
        if (preset == QLCChannel::BeamZoomSmallBig || preset == QLCChannel::BeamZoomBigSmall
            || preset == QLCChannel::BeamZoomFine || name.contains(QLatin1String("zoom")))
            zoom = true;
        if (preset >= QLCChannel::IntensityRed && preset <= QLCChannel::IntensityHueFine)
            rgb = colour = true;
    }

    if (pan)
        return gobo ? MovingSpot : (zoom || rgb) ? MovingWash : MovingHead;
    if (fixture->type() == QLCFixtureDef::ColorChanger || colour)
        return FixedWash;
    return OtherKind;
}

FixtureKind groupKind(Doc *doc, const FixtureGroup *group)
{
    if (doc == nullptr || group == nullptr)
        return OtherKind;
    bool first = true;
    FixtureKind kind = OtherKind;
    for (quint32 fid : group->fixtureList())
    {
        const FixtureKind k = fixtureKind(doc->fixture(fid));
        if (first)
            kind = k;
        else if (k != kind)
            return OtherKind;
        first = false;
    }
    return kind;
}

ShutterState shutterState(Doc *doc, const QLCPalette *palette)
{
    if (palette != nullptr && palette->type() == QLCPalette::Strobe)
        return ShutterStrobe;
    if (palette == nullptr || doc == nullptr)
        return ShutterOpen;

    // What the engine puts out for it (QLCPalette::valuesFromFixtures) on
    // the first fixture with a shutter channel: a strobe-preset channel
    // gets 0, open; one with a ShutterOpen capability, that; otherwise the
    // look's value (0 meaning 255), whatever its capability there says.
    for (const Fixture *f : doc->fixtures())
    {
        if (f == nullptr || f->fixtureMode() == nullptr)
            continue;
        const quint32 shCh = f->fixtureMode()->channelNumber(QLCChannel::Shutter, QLCChannel::MSB);
        const QLCChannel *ch = shCh == QLCChannel::invalid() ? nullptr : f->channel(shCh);
        if (ch == nullptr)
            continue;
        if (ch->preset() == QLCChannel::ShutterStrobeSlowFast
            || ch->preset() == QLCChannel::ShutterStrobeFastSlow)
            return ShutterOpen;
        for (const QLCCapability *cap : ch->capabilities())
            if (cap->preset() == QLCCapability::ShutterOpen)
                return ShutterOpen;
        int v = palette->values().isEmpty() ? 0 : palette->intValue1();
        if (v == 0)
            v = 255;
        for (const QLCCapability *cap : ch->capabilities())
        {
            if (v < cap->min() || v > cap->max())
                continue;
            switch (cap->preset())
            {
                case QLCCapability::ShutterClose: return ShutterClosed;
                case QLCCapability::StrobeSlowToFast:
                case QLCCapability::StrobeFastToSlow:
                case QLCCapability::StrobeRandom:
                case QLCCapability::StrobeRandomSlowToFast:
                case QLCCapability::StrobeRandomFastToSlow:
                case QLCCapability::StrobeFrequency:
                case QLCCapability::StrobeFreqRange: return ShutterStrobe;
                default: break;
            }
            const QString name = cap->name().toLower();
            if (name.contains(QLatin1String("strobe")) || name.contains(QLatin1String("pulse")))
                return ShutterStrobe;
            if (name.contains(QLatin1String("close")) || name.contains(QLatin1String("blackout")))
                return ShutterClosed;
            return ShutterOpen;
        }
        return ShutterOpen;
    }
    return ShutterOpen;
}

int intensityPercent(const QLCPalette *palette)
{
    if (palette == nullptr || palette->values().isEmpty())
        return 0;
    return qBound(0, qRound(palette->value().toInt() * 100.0 / 255.0), 100);
}

/*********************************************************************
 * The icons, one function per kind
 *********************************************************************/

QIcon intensityIcon(int percent)
{
    percent = qBound(0, percent, 100);
    return cached(QStringLiteral("pie:%1").arg(percent), [percent](QPainter &p, const QColor &ink) {
        // The whole: an empty dial; the level: its lit share, from 12 o'clock.
        const QRectF r(3, 3, 26, 26);
        p.setPen(Qt::NoPen);
        p.setBrush(alpha(ink, 45));
        p.drawEllipse(r);
        p.setBrush(kLight);
        if (percent >= 100)
            p.drawEllipse(r);
        else if (percent > 0)
            p.drawPie(r, 90 * 16, -qRound(percent * 3.6 * 16));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(alpha(ink, 210), 2));
        p.drawEllipse(r);
        if (percent > 0 && percent < 100)
        {
            p.setPen(QPen(kAmber, 1.2));
            p.drawLine(QPointF(16, 16), QPointF(16, 4));
        }
    });
}

QIcon shutterIcon(ShutterState state, int ratePercent)
{
    ratePercent = state == ShutterStrobe ? qBound(-1, ratePercent, 100) : -1;
    const QString key = QStringLiteral("shutter:%1:%2").arg(int(state)).arg(ratePercent);
    return cached(key, [state, ratePercent](QPainter &p, const QColor &ink) {
        const QRectF outer(3, 3, 26, 26);
        if (state == ShutterOpen)
        {
            // The gate wide open: light through it, the blades drawn back
            // to its rim.
            p.setPen(Qt::NoPen);
            p.setBrush(kLight);
            p.drawEllipse(QRectF(6, 6, 20, 20));
            p.setPen(QPen(alpha(ink, 200), 1.6));
            for (int i = 0; i < 6; ++i)
            {
                const qreal a = qDegreesToRadians(i * 60.0);
                const qreal b = qDegreesToRadians(i * 60.0 + 35.0);
                p.drawLine(QPointF(16 + 8 * qCos(a), 16 + 8 * qSin(a)),
                           QPointF(16 + 12.5 * qCos(b), 16 + 12.5 * qSin(b)));
            }
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(shutterRing(ink), 2.5));
            p.drawEllipse(outer.adjusted(0.5, 0.5, -0.5, -0.5));
        }
        else if (state == ShutterClosed)
        {
            // The blades shut: no light, a dark disc with its blades.
            p.setPen(Qt::NoPen);
            p.setBrush(alpha(ink, 120));
            p.drawEllipse(outer);
            p.setPen(QPen(alpha(ink, 230), 1.4));
            for (int i = 0; i < 6; ++i)
            {
                const qreal a = qDegreesToRadians(i * 60.0);
                const qreal b = qDegreesToRadians(i * 60.0 + 70.0);
                p.drawLine(QPointF(16 + 2.5 * qCos(a), 16 + 2.5 * qSin(a)),
                           QPointF(16 + 12.5 * qCos(b), 16 + 12.5 * qSin(b)));
            }
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(shutterRing(ink), 2));
            p.drawEllipse(outer);
        }
        else
        {
            // A flash; its rate (when it has one) as the share of the ring lit.
            p.setPen(QPen(alpha(ink, 90), 3));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(outer.adjusted(1, 1, -1, -1));
            if (ratePercent >= 0)
            {
                p.setPen(QPen(kLight, 3, Qt::SolidLine, Qt::FlatCap));
                p.drawArc(outer.adjusted(1, 1, -1, -1), 90 * 16,
                          -qRound(qMax(8, ratePercent) * 3.6 * 16));
            }
            QPolygonF bolt;
            bolt << QPointF(18.5, 5) << QPointF(9.5, 18) << QPointF(15.5, 18)
                 << QPointF(13, 27.5) << QPointF(22.5, 13.5) << QPointF(16.5, 13.5);
            p.setPen(QPen(kAmber, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(kLight);
            p.drawPolygon(bolt);
        }
    });
}

QIcon pointTargetIcon()
{
    return cached(QStringLiteral("target:point"), [](QPainter &p, const QColor &) {
        // A yellow bullseye with a crosshair.
        p.setPen(QPen(kAmber, 1.6));
        p.setBrush(kLight);
        p.drawEllipse(QPointF(16, 16), 11, 11);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(16, 16), 6.5, 6.5);
        p.setBrush(kAmber);
        p.drawEllipse(QPointF(16, 16), 2.2, 2.2);
        p.setPen(QPen(kAmber, 2.2, Qt::SolidLine, Qt::FlatCap));
        p.drawLine(QPointF(16, 0.5), QPointF(16, 8));
        p.drawLine(QPointF(16, 24), QPointF(16, 31.5));
        p.drawLine(QPointF(0.5, 16), QPointF(8, 16));
        p.drawLine(QPointF(24, 16), QPointF(31.5, 16));
    });
}

QIcon lineTargetIcon()
{
    return cached(QStringLiteral("target:line"), [](QPainter &p, const QColor &) {
        // A line with a square handle at each end (as it is drawn on the
        // stage; here only a picture).
        const QPointF a(6.5, 25.5), b(25.5, 6.5);
        p.setPen(QPen(kAmber, 5.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(a, b);
        p.setPen(QPen(kLight, 3, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(a, b);
        p.setPen(QPen(kAmber, 1.6));
        p.setBrush(kLight);
        p.drawRect(QRectF(a.x() - 4.5, a.y() - 4.5, 9, 9));
        p.drawRect(QRectF(b.x() - 4.5, b.y() - 4.5, 9, 9));
    });
}

QIcon swatchIcon(const QColor &color)
{
    return cached(QStringLiteral("swatch:%1").arg(color.name(QColor::HexArgb)),
                  [color](QPainter &p, const QColor &ink) {
        p.setPen(QPen(alpha(ink, 150), 1.5));
        p.setBrush(color);
        p.drawRoundedRect(QRectF(3.5, 3.5, 25, 25), 4, 4);
    });
}

QIcon kindIcon(FixtureKind kind, bool group)
{
    if (kind == OtherKind)
        return QIcon(QStringLiteral(":/group.png"));
    const QString key = QStringLiteral("kind:%1:%2").arg(int(kind)).arg(group ? 1 : 0);
    return cached(key, [kind, group](QPainter &p, const QColor &ink) {
        if (!group)
        {
            drawKind(p, ink, kind);
            return;
        }
        // A group of them: the kind, a little smaller, with the badge.
        p.save();
        p.scale(0.85, 0.85);
        drawKind(p, ink, kind);
        p.restore();
        drawGroupBadge(p, ink);
    });
}

/*********************************************************************
 * Which icon a thing gets
 *********************************************************************/

QIcon targetIcon(const StageTarget *target)
{
    return target != nullptr && target->isLine() ? lineTargetIcon() : pointTargetIcon();
}

QIcon paletteIcon(Doc *doc, const QLCPalette *palette)
{
    if (palette == nullptr)
        return QIcon();
    switch (palette->type())
    {
        case QLCPalette::Color:
            return swatchIcon(palette->rgbValue());
        case QLCPalette::Dimmer:
            return intensityIcon(intensityPercent(palette));
        case QLCPalette::Shutter:
            return shutterIcon(shutterState(doc, palette));
        case QLCPalette::Strobe:
            return shutterIcon(ShutterStrobe, palette->values().isEmpty() ? -1
                               : qRound(palette->value().toFloat() * 100.0f));
        case QLCPalette::Aim:
        {
            MonitorProperties *mp = doc == nullptr ? nullptr : doc->monitorProperties();
            return targetIcon(mp == nullptr ? nullptr : mp->stageTarget(palette->stageTargetId()));
        }
        default:
            break;
    }
    const QString res = palette->iconResource();
    return res.isEmpty() ? QIcon() : QIcon(res);
}

QIcon fixtureIcon(const Fixture *fixture)
{
    if (fixture == nullptr)
        return QIcon(QStringLiteral(":/fixture.png"));
    const FixtureKind kind = fixtureKind(fixture);
    return kind == OtherKind ? QIcon(fixture->iconResource()) : kindIcon(kind);
}

QIcon groupIcon(Doc *doc, const FixtureGroup *group)
{
    return kindIcon(groupKind(doc, group), true);
}

QIcon targetsIcon(Doc *doc, const QList<quint32> &groups, const QList<quint32> &fixtures)
{
    if (doc == nullptr)
        return QIcon(QStringLiteral(":/group.png"));
    if (groups.isEmpty() && fixtures.size() == 1)
        return fixtureIcon(doc->fixture(fixtures.first()));
    QSet<quint32> all(fixtures.begin(), fixtures.end());
    for (quint32 gid : groups)
        if (const FixtureGroup *g = doc->fixtureGroup(gid))
            for (quint32 fid : g->fixtureList())
                all.insert(fid);
    bool first = true;
    FixtureKind kind = OtherKind;
    for (quint32 fid : all)
    {
        const FixtureKind k = fixtureKind(doc->fixture(fid));
        if (!first && k != kind)
            return kindIcon(OtherKind);
        kind = k;
        first = false;
    }
    return kindIcon(kind, true);
}

QIcon signatureIcon(Doc *doc, const QString &signature)
{
    if (doc == nullptr)
        return QIcon(QStringLiteral(":/group.png"));
    QList<quint32> groups, fixtures;
    for (const QString &part : signature.split(QLatin1Char('|')))
    {
        const QString ids = part.mid(2);
        if (part.startsWith(QLatin1String("x:")))
        {
            const Function *f = doc->function(ids.toUInt());
            return f != nullptr ? f->getIcon() : QIcon();
        }
        if (part.startsWith(QLatin1String("g:")))
            groups += ShowTree::stringToIds(ids);
        else if (part.startsWith(QLatin1String("f:")))
            fixtures += ShowTree::stringToIds(ids);
        else if (part.startsWith(QLatin1String("s:")) && doc->showTree() != nullptr)
            for (quint32 id : ShowTree::stringToIds(ids))
                if (const ShowTree::GroupSet *gs = doc->showTree()->groupSet(id))
                {
                    groups += gs->groups;
                    fixtures += gs->fixtures;
                }
    }
    return targetsIcon(doc, groups, fixtures);
}

} // namespace ItemIcons
