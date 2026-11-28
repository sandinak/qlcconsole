/*
  Q Light Controller Plus - qlcconsole
  footericons.cpp

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

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>

#include "footericons.h"
#include "surfacechips.h"

namespace FooterIcons
{

const QList<Entry> &entries()
{
    // legacyChip: App::FooterChip values (Load 0, Power 1, GrandMaster 2, SpeedMasters 3).
    static const QList<Entry> list = {
        { "readiness", QCoreApplication::translate("FooterIcons", "Readiness (Ready / Not ready)"), Ready, true, -1 },
        { "venue",     QCoreApplication::translate("FooterIcons", "Venue"), Pin, false, -1 },
        { "teaching",  QCoreApplication::translate("FooterIcons", "Teaching show"), Cap, false, -1 },
        { "student",   QCoreApplication::translate("FooterIcons", "Student level"), Book, true, -1 },
        { "review",    QCoreApplication::translate("FooterIcons", "Show Review"), Checklist, false, -1 },
        { "rignet",    QCoreApplication::translate("FooterIcons", "Rig network only"), Network, false, -1 },
        { "showonly",  QCoreApplication::translate("FooterIcons", "Output rules (show only)"), Funnel, false, -1 },
        { "hazard",    QCoreApplication::translate("FooterIcons", "Hazards"), Warning, true, -1 },
        { "update",    QCoreApplication::translate("FooterIcons", "Update available"), Download, false, -1 },
        { "gm",        QCoreApplication::translate("FooterIcons", "Grand Master"), Fader, false, 2 },
        { "heldmasters", QCoreApplication::translate("FooterIcons", "Master held below full"), Fader, true, -1 },
        { "speed",     QCoreApplication::translate("FooterIcons", "Speed masters"), Metronome, false, 3 },
        { "next",      QCoreApplication::translate("FooterIcons", "Next cue"), NextCue, false, -1 },
        { "timecode",  QCoreApplication::translate("FooterIcons", "Timecode"), Clock, false, -1 },
        { "load",      QCoreApplication::translate("FooterIcons", "Engine load"), Gauge, false, 0 },
        { "power",     QCoreApplication::translate("FooterIcons", "Power estimate"), Bolt, false, 1 },
        { "dangle",    QCoreApplication::translate("FooterIcons", "Move in black warning"), Warning, false, -1 },
        { "timeline",  QCoreApplication::translate("FooterIcons", "Under timeline control"), Timeline, false, -1 },
        { "selection", QCoreApplication::translate("FooterIcons", "Selected fixtures"), Lamp, false, -1 },
        { "padmode",   QCoreApplication::translate("FooterIcons", "Pad grid mode"), Grid, false, -1 },
        { "pad",       QCoreApplication::translate("FooterIcons", "Pad (game controller)"), Gamepad, true, -1 },
        { "follow",    QCoreApplication::translate("FooterIcons", "Follow spot"), Spot, false, -1 },
        { "overrides", QCoreApplication::translate("FooterIcons", "Overrides holding the rig"), Hold, true, -1 },
        { "test",      QCoreApplication::translate("FooterIcons", "Test mode holding channels"), Flask, true, -1 },
        { "desk",      QCoreApplication::translate("FooterIcons", "This computer's desk: not patched"), Board, true, -1 },
        { "surfaces",  QCoreApplication::translate("FooterIcons", "Control surfaces"), Board, false, -1 },
        { "editlive",  QCoreApplication::translate("FooterIcons", "Edit Live"), Pencil, true, -1 },
        { "stage",     QCoreApplication::translate("FooterIcons", "What lights the stage"), Stage, false, -1 },
        { "programmer", QCoreApplication::translate("FooterIcons", "Programmer changes"), Sliders, false, -1 },
        { "saved",     QCoreApplication::translate("FooterIcons", "Saved / unsaved"), Disk, false, -1 },
    };
    return list;
}

const Entry *find(const QString &id)
{
    for (const Entry &e : entries())
        if (e.id == id)
            return &e;
    return nullptr;
}

QIcon icon(Glyph glyph, const QColor &colour, int size)
{
    if (glyph == Board)
        return SurfaceChips::boardIcon(colour, size);
    const qreal dpr = qApp != nullptr ? qApp->devicePixelRatio() : 1.0;
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal u = size / 14.0;
    QPen pen(colour, 1.2 * u, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    auto P = [u](qreal x, qreal y) { return QPointF(x * u, y * u); };
    auto R = [u](qreal x, qreal y, qreal w, qreal h) { return QRectF(x * u, y * u, w * u, h * u); };
    auto poly = [&](std::initializer_list<QPointF> pts) { p.drawPolygon(QPolygonF(QList<QPointF>(pts))); };
    switch (glyph)
    {
    case Ready:         // a circle with a tick
        p.drawEllipse(R(1.5, 1.5, 11, 11));
        p.drawPolyline(QPolygonF({ P(4.3, 7.2), P(6.3, 9.2), P(9.8, 5) }));
        break;
    case Pin:           // a map pin
    {
        QPainterPath path;
        path.moveTo(P(7, 13));
        path.cubicTo(P(2.5, 8.2), P(2.5, 7), P(2.5, 5.6));
        path.arcTo(R(2.5, 1.2, 9, 9), 180, -180);
        path.cubicTo(P(11.5, 7), P(11.5, 8.2), P(7, 13));
        p.drawPath(path);
        p.drawEllipse(P(7, 5.6), 1.6 * u, 1.6 * u);
        break;
    }
    case Cap:           // a mortarboard
        poly({ P(7, 2.5), P(13, 5.5), P(7, 8.5), P(1, 5.5) });
        p.drawPolyline(QPolygonF({ P(3.8, 7.2), P(3.8, 10), P(7, 11.5), P(10.2, 10), P(10.2, 7.2) }));
        break;
    case Book:          // an open book
        p.drawPolyline(QPolygonF({ P(7, 4), P(7, 12) }));
        p.drawPolyline(QPolygonF({ P(7, 4), P(4, 2.8), P(1.5, 3.4), P(1.5, 11), P(4, 10.4), P(7, 12) }));
        p.drawPolyline(QPolygonF({ P(7, 4), P(10, 2.8), P(12.5, 3.4), P(12.5, 11), P(10, 10.4), P(7, 12) }));
        break;
    case Checklist:     // two ticked lines
        p.drawPolyline(QPolygonF({ P(1.5, 4), P(2.7, 5.3), P(4.6, 2.8) }));
        p.drawLine(P(7, 4), P(12.5, 4));
        p.drawPolyline(QPolygonF({ P(1.5, 9.5), P(2.7, 10.8), P(4.6, 8.3) }));
        p.drawLine(P(7, 9.5), P(12.5, 9.5));
        break;
    case Network:       // three nodes, joined
        p.drawLine(P(7, 3.5), P(3, 10.5));
        p.drawLine(P(7, 3.5), P(11, 10.5));
        p.drawLine(P(3, 10.5), P(11, 10.5));
        p.setBrush(colour);
        for (const QPointF &c : { P(7, 3.5), P(3, 10.5), P(11, 10.5) })
            p.drawEllipse(c, 1.6 * u, 1.6 * u);
        break;
    case Funnel:
        poly({ P(1.5, 2.5), P(12.5, 2.5), P(8, 7.5), P(8, 12), P(6, 11), P(6, 7.5) });
        break;
    case Warning:       // a triangle with a mark
        poly({ P(7, 1.8), P(13, 12), P(1, 12) });
        p.drawLine(P(7, 5.5), P(7, 8.8));
        p.setBrush(colour);
        p.drawEllipse(P(7, 10.5), 0.5 * u, 0.5 * u);
        break;
    case Download:      // an arrow into a tray
        p.drawLine(P(7, 1.5), P(7, 8.5));
        p.drawPolyline(QPolygonF({ P(4, 5.8), P(7, 8.8), P(10, 5.8) }));
        p.drawPolyline(QPolygonF({ P(2, 9.5), P(2, 12.5), P(12, 12.5), P(12, 9.5) }));
        break;
    case Fader:         // a slot with a cap
        p.drawLine(P(7, 1.5), P(7, 12.5));
        p.setBrush(colour);
        p.drawRoundedRect(R(3.8, 5.2, 6.4, 3.4), 0.8 * u, 0.8 * u);
        break;
    case Metronome:
        poly({ P(4, 12.5), P(10, 12.5), P(8.3, 2), P(5.7, 2) });
        p.drawLine(P(7, 10), P(9.6, 4));
        break;
    case NextCue:       // play, then a bar: what comes next
        p.setBrush(colour);
        poly({ P(2, 2.5), P(9, 7), P(2, 11.5) });
        p.drawLine(P(11.5, 2.5), P(11.5, 11.5));
        break;
    case Clock:
        p.drawEllipse(R(1.5, 1.5, 11, 11));
        p.drawPolyline(QPolygonF({ P(7, 3.8), P(7, 7), P(9.4, 8.4) }));
        break;
    case Gauge:         // a dial
    {
        QPainterPath arc;
        arc.arcMoveTo(R(1.5, 2.5, 11, 11), 0);
        arc.arcTo(R(1.5, 2.5, 11, 11), 0, 180);
        p.drawPath(arc);
        p.drawLine(P(1.5, 8), P(12.5, 8));
        p.drawLine(P(7, 8), P(10, 4.5));
        break;
    }
    case Bolt:          // a lightning bolt
        p.setBrush(colour);
        poly({ P(8, 1), P(3, 8), P(6.5, 8), P(5.5, 13), P(11, 5.5), P(7.5, 5.5) });
        break;
    case Timeline:      // lanes with a playhead
        p.drawLine(P(1.5, 4), P(8, 4));
        p.drawLine(P(1.5, 7), P(12.5, 7));
        p.drawLine(P(1.5, 10), P(6, 10));
        p.drawLine(P(9, 1.5), P(9, 12.5));
        break;
    case Lamp:          // a fixture: yoke, head and beam
        p.drawLine(P(2.5, 4), P(2.5, 7));
        p.drawLine(P(2.5, 7), P(11.5, 7));
        p.drawLine(P(11.5, 7), P(11.5, 4));
        p.drawRoundedRect(R(4.5, 2, 5, 5), 1 * u, 1 * u);
        poly({ P(4.5, 9), P(9.5, 9), P(11.5, 12.5), P(2.5, 12.5) });
        break;
    case Grid:
        for (const QPointF &c : { P(2, 2), P(8, 2), P(2, 8), P(8, 8) })
            p.drawRect(QRectF(c, QSizeF(4 * u, 4 * u)));
        break;
    case Gamepad:
        p.drawRoundedRect(R(1, 3.5, 12, 7.5), 3 * u, 3 * u);
        p.drawLine(P(4, 5.5), P(4, 9));
        p.drawLine(P(2.3, 7.2), P(5.7, 7.2));
        p.setBrush(colour);
        p.drawEllipse(P(9, 7.8), 0.7 * u, 0.7 * u);
        p.drawEllipse(P(10.8, 6.4), 0.7 * u, 0.7 * u);
        break;
    case Spot:          // a target
        p.drawEllipse(R(2.5, 2.5, 9, 9));
        p.drawLine(P(7, 0.8), P(7, 4));
        p.drawLine(P(7, 10), P(7, 13.2));
        p.drawLine(P(0.8, 7), P(4, 7));
        p.drawLine(P(10, 7), P(13.2, 7));
        break;
    case Hold:          // a hand-held level: a bar across a ring
        p.drawEllipse(R(1.5, 1.5, 11, 11));
        p.drawLine(P(4, 7), P(10, 7));
        p.drawLine(P(4, 4.8), P(4, 9.2));
        p.drawLine(P(10, 4.8), P(10, 9.2));
        break;
    case Flask:
        p.drawPolyline(QPolygonF({ P(5.2, 1.5), P(5.2, 5.5), P(1.8, 12), P(12.2, 12), P(8.8, 5.5), P(8.8, 1.5) }));
        p.drawLine(P(4.2, 1.5), P(9.8, 1.5));
        break;
    case Board:
        break;
    case Pencil:        // a pencil in a circle
        p.drawEllipse(R(1, 1, 12, 12));
        poly({ P(4.2, 9.8), P(4.6, 8), P(9.2, 3.8), P(10.4, 5), P(6, 9.4) });
        break;
    case Stage:         // a spot's beam on a stage
        poly({ P(5.5, 1.5), P(8.5, 1.5), P(12.5, 9), P(1.5, 9) });
        p.drawLine(P(1, 12), P(13, 12));
        break;
    case Sliders:
        p.drawLine(P(2, 4), P(12, 4));
        p.drawLine(P(2, 10), P(12, 10));
        p.setBrush(colour);
        p.drawEllipse(P(9, 4), 1.5 * u, 1.5 * u);
        p.drawEllipse(P(5, 10), 1.5 * u, 1.5 * u);
        break;
    case Disk:          // a floppy disk
        p.drawPolygon(QPolygonF({ P(1.8, 1.8), P(10.8, 1.8), P(12.2, 3.2), P(12.2, 12.2), P(1.8, 12.2) }));
        p.drawRect(R(4, 1.8, 5, 3.4));
        p.drawRect(R(3.8, 8, 6.4, 4.2));
        break;
    }
    p.end();
    return QIcon(pm);
}

}   // namespace FooterIcons

FooterBadge::FooterBadge(const QString &id, QWidget *chip, QWidget *parent)
    : QLabel(parent)
    , m_id(id)
    , m_chip(chip)
{
    setObjectName(QStringLiteral("footerIcon_%1").arg(id));
    setAlignment(Qt::AlignCenter);
    setFixedWidth(iconWidth());
    if (const FooterIcons::Entry *e = FooterIcons::find(id))
    {
        setToolTip(e->label);
        setAccessibleName(e->label);
    }
    setProperty("footerBadge", true);
    refresh();
}

void FooterBadge::refresh()
{
    const FooterIcons::Entry *e = FooterIcons::find(m_id);
    if (e == nullptr)
        return;
    setPixmap(FooterIcons::icon(e->glyph, palette().color(QPalette::WindowText), 14)
                  .pixmap(QSize(14, 14)));
}

void FooterBadge::changeEvent(QEvent *event)
{
    QLabel::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange)
        refresh();
}
