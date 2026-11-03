/*
  Q Light Controller Plus
  lineplanview.cpp

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

#include <QContextMenuEvent>
#include <QCloseEvent>
#include <QHideEvent>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>
#include <QSettings>
#include <QShortcut>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QRectF>
#include <QFontMetricsF>
#include <QLineF>
#include <algorithm>
#include <QtGlobal>
#include <climits>
#include <cmath>

#include "lineplanview.h"
#include "linespreadpreview.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "linecurve.h"
#include "linespread.h"
#include "aimsolver.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "beamlanding.h"
#include "scene.h"
#include "doc.h"

namespace
{
// From above, as LineFormation's: x across, y up / downstage (downstage at the bottom).
QPointF plan(const QVector3D &v)
{
    return QPointF(v.x(), v.y());
}
}

LinePlanView::LinePlanView(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("lineplanView"));
    setMinimumSize(200, 140);
    m_popOut = new QToolButton(this);
    m_popOut->setObjectName(QStringLiteral("linePlanPopOut"));
    m_popOut->setText(QStringLiteral("\u2922"));
    m_popOut->setAutoRaise(true);
    m_popOut->setCursor(Qt::ArrowCursor);
    m_popOut->setToolTip(tr("Open the strip and the plan in a window"));
    connect(m_popOut, &QToolButton::clicked, this, &LinePlanView::popOutRequested);
    setToolTip(tr("The plan, from above: the line, each light where it hangs and its beam to its point "
                  "(numbered as the strip above). Rings: where the previous cue put the same lights -- "
                  "the beams sweep from there over the cue's fade."));
}

void LinePlanView::setFormation(const Scene *scene, const QLCPalette *look,
                                const Scene *prevScene, const QLCPalette *prevLook)
{
    m_line.clear();
    m_beams.clear();
    m_form = LineFormation();
    m_stage = QRectF();
    m_marks.clear();
    m_otherLines.clear();
    MonitorProperties *props = m_doc != nullptr ? m_doc->monitorProperties() : nullptr;
    const StageTarget *target = (props != nullptr && look != nullptr)
            ? props->stageTarget(look->stageTargetId()) : nullptr;
    if (target == nullptr || !target->isLine() || scene == nullptr)
    {
        update();
        return;
    }
    const LineCurve curve(props->targetLinePoints(target));
    for (const QVector3D &s : curve.samples())
        m_line << plan(s);

    m_form = LineFormation::build(m_doc, scene, look);

    // The previous cue's points for the same lights, when it aimed them at
    // a line too.
    QHash<quint32, QVector3D> before;
    if (prevScene != nullptr && prevLook != nullptr)
    {
        const StageTarget *pt = props->stageTarget(prevLook->stageTargetId());
        if (pt != nullptr && pt->isLine())
            before = AimSolver::linePoints(m_doc, prevLook, pt,
                                           AimSolver::aimHeads(m_doc, prevScene, prevLook->id()));
    }

    for (const LineFormation::Light &l : m_form.lights())
    {
        if (!l.placed)
            continue;
        Beam b;
        b.fixture = l.fixture;
        b.number = l.number;
        b.name = l.name;
        b.from = l.from;
        b.to = l.to;
        b.group = l.group;
        b.colour = m_form.colourFor(l);
        if (before.contains(l.fixture))
        {
            b.hasPrevious = true;
            b.previous = plan(before.value(l.fixture));
        }
        m_beams << b;
    }
    update();
}

void LinePlanView::setPositions(const QList<QList<quint32>> &groups,
                                const QList<const QLCPalette *> &palettes)
{
    m_line.clear();
    m_beams.clear();
    m_marks.clear();
    m_otherLines.clear();
    m_stage = QRectF();
    m_form = LineFormation::positions(m_doc, groups, palettes);
    MonitorProperties *props = m_doc != nullptr ? m_doc->monitorProperties() : nullptr;
    if (props != nullptr)
    {
        const double width = double(props->gridSize().x() * props->gridUnitMetres());
        const double front = double(props->downstageEdgeY());
        if (width > 0.0 && front > 0.0)
            m_stage = QRectF(0.0, 0.0, width, front);
        for (const StageTarget *t : props->stageTargets())
        {
            if (t == nullptr)
                continue;
            if (t->isLine())
            {
                QList<QPointF> pts;
                const LineCurve curve(props->targetLinePoints(t));
                for (const QVector3D &v : curve.samples())
                    pts << plan(v);
                m_otherLines << pts;
            }
            else
                m_marks << qMakePair(plan(props->targetAimPoint(t)), t->name());
        }
    }
    for (const LineFormation::Light &l : m_form.lights())
    {
        Beam b;
        b.fixture = l.fixture;
        b.number = l.number;
        b.name = l.name;
        b.from = l.from;
        b.to = l.to;
        b.group = l.group;
        b.colour = m_form.colourFor(l);
        b.lands = l.landing == BeamLanding::OnStage;
        b.hits = l.hits;
        m_beams << b;
    }
    update();
}

void LinePlanView::setDimmed(bool dimmed)
{
    if (m_dimmed == dimmed)
        return;
    m_dimmed = dimmed;
    update();
}

void LinePlanView::setPoppable(bool poppable)
{
    m_poppable = poppable;
    m_popOut->setVisible(poppable);
}

double LinePlanView::drawScale() const
{
    return qBound(1.0, qMin(width() / 260.0, height() / 170.0), 3.5);
}

void LinePlanView::resizeEvent(QResizeEvent *)
{
    m_popOut->move(width() - m_popOut->width() - 2, 2);
}

void LinePlanView::contextMenuEvent(QContextMenuEvent *event)
{
    if (!m_poppable)
        return;
    QMenu menu(this);
    QAction *open = menu.addAction(tr("Open in a window"));
    if (menu.exec(event->globalPos()) == open)
        emit popOutRequested();
}

void LinePlanView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(30, 32, 38));
    const double s = drawScale();
    QFont f = p.font();
    f.setPointSizeF(8.0 * s);
    p.setFont(f);
    p.setPen(QColor(120, 120, 130));
    p.drawText(rect().adjusted(6, 4, -6, -4), Qt::AlignTop | Qt::AlignLeft, tr("Plan (top)"));
    const bool raw = m_form.isRawPosition();
    if (m_line.isEmpty() && !raw)
    {
        p.drawText(rect(), Qt::AlignCenter, tr("Not aimed at a line"));
        return;
    }
    if (raw && m_beams.isEmpty())
    {
        p.drawText(rect(), Qt::AlignCenter, tr("No light here has a stored pan or tilt to show"));
        return;
    }

    // Fit everything, the same scale both ways, at least 2 m each way.
    // A stored position is fitted to its lights and where they land, not
    // to the whole stage: the stage is drawn for reference, clipped.
    QRectF box(m_line.isEmpty() ? m_beams.first().from : m_line.first(), QSizeF(0, 0));
    auto grow = [&box](const QPointF &pt) {
        box = box.united(QRectF(pt, QSizeF(0.0001, 0.0001)));
    };
    for (const QPointF &pt : m_line)
        grow(pt);
    /* A stored position can land far out in the house: drawn to scale, the
       stage would shrink to a dot. Beams are drawn to where they leave a
       frame of the stage and 4 m round it (ending in the hollow ring that
       says they miss the stage). */
    QRectF frame;
    if (raw)
    {
        QRectF hung(m_beams.first().from, QSizeF(0.0001, 0.0001));
        for (const Beam &b : m_beams)
            hung = hung.united(QRectF(b.from, QSizeF(0.0001, 0.0001)));
        frame = m_stage.isValid() ? m_stage.adjusted(-4.0, -4.0, 4.0, 4.0).united(hung)
                                  : hung.adjusted(-10.0, -10.0, 10.0, 10.0);
    }
    auto shown = [&frame](const Beam &b) {
        if (!frame.isValid() || frame.contains(b.to))
            return b.to;
        // Where the segment from -> to leaves the frame.
        const QPointF d = b.to - b.from;
        double t = 1.0;
        if (d.x() > 0) t = qMin(t, (frame.right() - b.from.x()) / d.x());
        if (d.x() < 0) t = qMin(t, (frame.left() - b.from.x()) / d.x());
        if (d.y() > 0) t = qMin(t, (frame.bottom() - b.from.y()) / d.y());
        if (d.y() < 0) t = qMin(t, (frame.top() - b.from.y()) / d.y());
        return b.from + d * qMax(0.0, t);
    };
    for (const Beam &b : m_beams)
    {
        grow(b.from);
        grow(shown(b));
        if (b.hasPrevious)
            grow(b.previous);
    }
    const QPointF c = box.center();
    const double w = qMax(2.0, box.width()) * 1.15, h = qMax(2.0, box.height()) * 1.15;
    // Room at the foot for the caption, and for names when they show.
    const double foot = (s >= 1.6 ? 44.0 : 20.0) * s;
    const double head = (s >= 1.6 ? 40.0 : 16.0) * s;
    const QRectF area = QRectF(rect()).adjusted(12 * s, head, -12 * s, -foot);
    const double scale = qMin(area.width() / w, area.height() / h);
    auto toScreen = [&](const QPointF &pt) {
        return QPointF(area.center().x() + (pt.x() - c.x()) * scale,
                       area.center().y() + (pt.y() - c.y()) * scale);
    };

    // A stored position: the stage, its targets and lines, for reference.
    if (raw)
    {
        if (m_stage.isValid())
        {
            p.setPen(QPen(QColor(90, 92, 100), 1.0 * s, Qt::DashLine));
            p.setBrush(QColor(40, 42, 50));
            p.drawRect(QRectF(toScreen(m_stage.topLeft()), toScreen(m_stage.bottomRight()))
                       .intersected(QRectF(rect()).adjusted(-2, -2, 2, 2)));
        }
        p.setBrush(Qt::NoBrush);
        for (const QList<QPointF> &l : m_otherLines)
        {
            QPolygonF poly;
            for (const QPointF &pt : l)
                poly << toScreen(pt);
            p.setPen(QPen(QColor(255, 180, 0, 120), 1.5 * s));
            p.drawPolyline(poly);
        }
    }

    // The line, its ends A and B as the strip names them.
    if (!m_line.isEmpty())
    {
        QPolygonF poly;
        for (const QPointF &pt : m_line)
            poly << toScreen(pt);
        p.setPen(QPen(QColor(255, 180, 0), 2.0 * s));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(poly);
        p.setPen(QColor(150, 150, 160));
        p.drawText(QRectF(poly.first() - QPointF(14 * s, 14 * s), QSizeF(12 * s, 12 * s)), Qt::AlignCenter, QStringLiteral("A"));
        p.drawText(QRectF(poly.last() + QPointF(2 * s, -14 * s), QSizeF(12 * s, 12 * s)), Qt::AlignCenter, QStringLiteral("B"));
    }

    const double r = 6.0 * s;
    // The previous cue's points: rings, and the way each beam sweeps.
    for (const Beam &b : m_beams)
    {
        if (!b.hasPrevious)
            continue;
        const QPointF was = toScreen(b.previous), is = toScreen(b.to);
        p.setPen(QPen(QColor(140, 170, 255, 170), s, Qt::DashLine));
        if (QLineF(was, is).length() > 1.0)
            p.drawLine(was, is);
        p.setPen(QPen(QColor(140, 170, 255), 1.5 * s));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(was, r + 2, r + 2);
    }
    // Beams first, so every square and point sits on top of them: each from
    // where the light hangs to its point, in its group's colour -- lights
    // that swap or flip are seen crossing.
    const bool grouped = m_form.grouped();
    for (const Beam &b : m_beams)
    {
        QColor c = grouped ? b.colour : QColor(255, 230, 160);
        c.setAlpha(grouped ? 200 : 110);
        p.setPen(QPen(c, (grouped ? 1.6 : 1.0) * s, b.lands ? Qt::SolidLine : Qt::DashLine));
        p.drawLine(toScreen(b.from), toScreen(shown(b)));
    }
    // The lights where they hang.
    QList<QRectF> lightRects;
    for (const Beam &b : m_beams)
    {
        const QPointF from = toScreen(b.from);
        p.setPen(QPen(QColor(20, 20, 24), s));
        p.setBrush(grouped ? b.colour : QColor(170, 170, 180));
        const QRectF sq(from - QPointF(3 * s, 3 * s), QSizeF(6 * s, 6 * s));
        p.drawRect(sq);
        lightRects << sq.adjusted(-2 * s, -2 * s, 2 * s, 2 * s);
    }
    /* Their points. Points that land on top of each other are drawn once,
       a little bigger, "x N" -- numbers piled on one spot can't be read. */
    QList<QList<int>> clusters;
    QList<QPointF> clusterAt;
    for (int i = 0; i < m_beams.count(); ++i)
    {
        const QPointF at = toScreen(shown(m_beams.at(i)));
        int found = -1;
        for (int k = 0; k < clusters.count() && found < 0; ++k)
            if (QLineF(clusterAt.at(k), at).length() < 1.6 * r)
                found = k;
        if (found < 0)
        {
            clusters << QList<int>({ i });
            clusterAt << at;
        }
        else
            clusters[found] << i;
    }
    m_pointMarks.clear();
    for (int k = 0; k < clusters.count(); ++k)
    {
        const QList<int> &members = clusters.at(k);
        const Beam &b = m_beams.at(members.first());
        bool sameColour = true;
        for (int i : members)
            if (m_beams.at(i).colour != b.colour)
                sameColour = false;
        const QColor fill = sameColour && b.colour.isValid() ? b.colour : QColor(150, 150, 160);
        const double rr = members.count() > 1 ? r * 1.35 : r;
        const QPointF to = members.count() > 1 ? clusterAt.at(k) : toScreen(shown(b));
        // Off the stage or up in the air: a hollow ring where the beam goes.
        if (!b.lands)
        {
            p.setPen(QPen(fill, 1.5 * s));
            p.setBrush(QColor(30, 32, 38));
        }
        else
        {
            p.setPen(QPen(QColor(20, 20, 24), s));
            p.setBrush(fill);
        }
        p.drawEllipse(to, rr, rr);
        if (!b.lands)
            p.setPen(QColor(220, 220, 230));
        else
            p.setPen(grouped || !sameColour ? LineFormation::groupInk() : LineFormation::pointInk());
        const QString text = members.count() > 1 ? QStringLiteral("\u00d7%1").arg(members.count())
                                                 : QString::number(b.number);
        p.drawText(QRectF(to.x() - rr, to.y() - rr, 2 * rr, 2 * rr), Qt::AlignCenter, text);
        m_pointMarks << members.count();
        lightRects << QRectF(to.x() - rr, to.y() - rr, 2 * rr, 2 * rr);
    }

    // A stored position's targets, for reference: a cross, and its name
    // only where it is clear of every light and point.
    if (raw)
    {
        QFont mf = p.font();
        mf.setPointSizeF(7.0 * s);
        p.setFont(mf);
        const QFontMetricsF mfm(mf);
        for (const auto &m : m_marks)
        {
            const QPointF at = toScreen(m.first);
            if (!rect().contains(at.toPoint()))
                continue;
            p.setPen(QPen(QColor(255, 180, 0, 160), 1.2 * s));
            p.drawLine(at - QPointF(4 * s, 0), at + QPointF(4 * s, 0));
            p.drawLine(at - QPointF(0, 4 * s), at + QPointF(0, 4 * s));
            const double tw = mfm.horizontalAdvance(m.second) + 2.0, th = mfm.height();
            const QList<QRectF> tries { QRectF(at.x() + 5 * s, at.y() - th - 2 * s, tw, th),
                                        QRectF(at.x() - 5 * s - tw, at.y() - th - 2 * s, tw, th),
                                        QRectF(at.x() + 5 * s, at.y() + 2 * s, tw, th),
                                        QRectF(at.x() - 5 * s - tw, at.y() + 2 * s, tw, th) };
            for (const QRectF &t : tries)
            {
                bool clear = QRectF(rect()).contains(t);
                for (const QRectF &o : lightRects)
                    if (o.intersects(t))
                        clear = false;
                if (!clear)
                    continue;
                p.setPen(QColor(150, 150, 160));
                p.drawText(t, Qt::AlignLeft | Qt::AlignVCenter, m.second);
                lightRects << t;
                break;
            }
        }
        p.setFont(f);
    }

    // Big enough to read: each light's name where it hangs, laid out so no
    // two overlap -- a name that would sit on another drops a row, with a
    // leader to its light; one with no room is left out (the tooltip has it).
    m_labelRects.clear();
    if (s >= 1.6)
    {
        const QFontMetricsF fm(p.font());
        const double rowH = fm.height() + 1.0 * s;
        const double maxW = 110.0 * s;
        QList<int> order;
        for (int i = 0; i < m_beams.count(); ++i)
            if (!m_beams.at(i).name.isEmpty())
                order << i;
        std::sort(order.begin(), order.end(), [this](int a, int b) {
            return m_beams.at(a).from.x() < m_beams.at(b).from.x();
        });
        const QRectF room = QRectF(rect()).adjusted(2, 2, -2, -2);
        QList<QLineF> leaders;
        QStringList texts;
        // The title is taken.
        // The title, the lights, their points and the targets' names are taken.
        QList<QRectF> taken = lightRects;
        taken << QRectF(0, 0, 90.0 * s, 16.0 * s);
        for (int i : order)
        {
            const Beam &b = m_beams.at(i);
            const QPointF from = toScreen(b.from);
            const QString text = fm.elidedText(b.name, Qt::ElideMiddle, maxW);
            const double w = fm.horizontalAdvance(text) + 2.0 * s;
            // On the side away from its beam, so the name never sits on it.
            const bool up = toScreen(shown(b)).y() > from.y();
            for (int row = 0; row < 4; ++row)
            {
                QRectF box(from.x() - w / 2, up ? from.y() - 6.0 * s - (row + 1) * rowH
                                                : from.y() + 6.0 * s + row * rowH, w, rowH);
                if (box.left() < room.left())
                    box.moveLeft(room.left());
                if (box.right() > room.right())
                    box.moveRight(room.right());
                if (!room.contains(box))
                    break;
                bool clear = true;
                for (const QRectF &o : taken)
                    if (o.intersects(box))
                        clear = false;
                for (const QRectF &o : m_labelRects)
                    if (o.intersects(box))
                    {
                        clear = false;
                        break;
                    }
                if (!clear)
                    continue;
                if (row > 0)
                    leaders << (up ? QLineF(from - QPointF(0, 3.0 * s), QPointF(from.x(), box.bottom()))
                                   : QLineF(from + QPointF(0, 3.0 * s), QPointF(from.x(), box.top())));
                m_labelRects << box;
                texts << text;
                break;
            }
        }
        // Leaders first, then each name on its own patch of background, so
        // a leader passing a name on the row above never runs through it.
        p.setPen(QPen(QColor(110, 110, 120), 0.8 * s));
        for (const QLineF &l : leaders)
            p.drawLine(l);
        for (int i = 0; i < m_labelRects.count(); ++i)
        {
            p.fillRect(m_labelRects.at(i), QColor(30, 32, 38));
            p.setPen(QColor(190, 190, 200));
            p.drawText(m_labelRects.at(i), Qt::AlignCenter, texts.at(i));
        }
    }

    // What the colours and lines show.
    const QString cap = m_form.caption();
    if (!cap.isEmpty())
    {
        QFont cf = p.font();
        cf.setPointSizeF(7.0 * s);
        p.setFont(cf);
        p.setPen(QColor(150, 150, 160));
        p.drawText(rect().adjusted(6, 4, -6, -3), Qt::AlignBottom | Qt::AlignLeft | Qt::TextWordWrap, cap);
    }
    if (m_dimmed)
        p.fillRect(rect(), QColor(30, 32, 38, 150));
}

// ---------------------------------------------------------------------------

QString LinePlanWindow::geometryKey()
{
    return QStringLiteral("showtree/linePlanWindow/geometry");
}

LinePlanWindow::LinePlanWindow(Doc *doc, QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("linePlanWindow"));
    setWindowTitle(tr("Line aim: order and plan"));
    setAttribute(Qt::WA_DeleteOnClose);
    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(8, 8, 8, 8);
    m_strip = new LineSpreadPreview(doc, this);
    m_strip->setObjectName(QStringLiteral("linePlanWindowStrip"));
    m_strip->setPoppable(false);
    m_strip->setMinimumSize(200, 56);
    m_strip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_plan = new LinePlanView(doc, this);
    m_plan->setObjectName(QStringLiteral("linePlanWindowPlan"));
    m_plan->setPoppable(false);
    m_plan->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_note = new QLabel(this);
    m_note->setObjectName(QStringLiteral("linePlanWindowNote"));
    m_note->setWordWrap(true);
    m_note->hide();
    v->addWidget(m_note);
    v->addWidget(m_strip, 1);
    v->addWidget(m_plan, 3);

    new QShortcut(QKeySequence(Qt::Key_Escape), this, SLOT(close()));
    new QShortcut(QKeySequence::Close, this, SLOT(close()));

    resize(640, 560);
    const QByteArray geo = QSettings().value(geometryKey()).toByteArray();
    if (!geo.isEmpty())
        restoreGeometry(geo);
}

LinePlanWindow::~LinePlanWindow()
{
}

void LinePlanWindow::setFormation(const Scene *scene, const QLCPalette *look,
                                  const Scene *prevScene, const QLCPalette *prevLook)
{
    m_strip->setLook(scene, look);
    m_strip->show();
    m_plan->setFormation(scene, look, prevScene, prevLook);
    m_strip->setDimmed(false);
    m_plan->setDimmed(false);
    m_note->hide();
}

void LinePlanWindow::setPositions(const QList<QList<quint32>> &groups, const QList<const QLCPalette *> &palettes)
{
    m_strip->hide();
    m_plan->setPositions(groups, palettes);
    m_plan->setDimmed(false);
    m_note->hide();
}

void LinePlanWindow::setNoAim()
{
    m_strip->setDimmed(true);
    m_plan->setDimmed(true);
    m_note->setText(tr("The selection has no line aim or stored position. Showing the last plan."));
    m_note->show();
}

QString LinePlanWindow::message() const
{
    return m_note->isVisible() ? m_note->text() : QString();
}

void LinePlanWindow::saveGeometryNow()
{
    QSettings().setValue(geometryKey(), saveGeometry());
}

void LinePlanWindow::hideEvent(QHideEvent *ev)
{
    saveGeometryNow();
    QWidget::hideEvent(ev);
}

void LinePlanWindow::closeEvent(QCloseEvent *ev)
{
    saveGeometryNow();
    QWidget::closeEvent(ev);
}
