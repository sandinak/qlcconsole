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
#include "scene.h"
#include "doc.h"

namespace
{
QPointF plan(const QVector3D &v)
{
    return QPointF(v.x(), v.z());
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
    if (m_line.isEmpty())
    {
        p.drawText(rect(), Qt::AlignCenter, tr("Not aimed at a line"));
        return;
    }

    // Fit everything, the same scale both ways, at least 2 m each way.
    QRectF box(m_line.first(), QSizeF(0, 0));
    auto grow = [&box](const QPointF &pt) {
        box = box.united(QRectF(pt, QSizeF(0.0001, 0.0001)));
    };
    for (const QPointF &pt : m_line)
        grow(pt);
    for (const Beam &b : m_beams)
    {
        grow(b.from);
        grow(b.to);
        if (b.hasPrevious)
            grow(b.previous);
    }
    const QPointF c = box.center();
    const double w = qMax(2.0, box.width()) * 1.15, h = qMax(2.0, box.height()) * 1.15;
    // Room at the foot for the caption, and for names when they show.
    const double foot = (s >= 1.6 ? 44.0 : 20.0) * s;
    const QRectF area = QRectF(rect()).adjusted(12 * s, 16 * s, -12 * s, -foot);
    const double scale = qMin(area.width() / w, area.height() / h);
    auto toScreen = [&](const QPointF &pt) {
        return QPointF(area.center().x() + (pt.x() - c.x()) * scale,
                       area.center().y() + (pt.y() - c.y()) * scale);
    };

    // The line, its ends A and B as the strip names them.
    QPolygonF poly;
    for (const QPointF &pt : m_line)
        poly << toScreen(pt);
    p.setPen(QPen(QColor(255, 180, 0), 2.0 * s));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(poly);
    p.setPen(QColor(150, 150, 160));
    p.drawText(QRectF(poly.first() - QPointF(14 * s, 14 * s), QSizeF(12 * s, 12 * s)), Qt::AlignCenter, QStringLiteral("A"));
    p.drawText(QRectF(poly.last() + QPointF(2 * s, -14 * s), QSizeF(12 * s, 12 * s)), Qt::AlignCenter, QStringLiteral("B"));

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
        p.setPen(QPen(c, (grouped ? 1.6 : 1.0) * s));
        p.drawLine(toScreen(b.from), toScreen(b.to));
    }
    for (const Beam &b : m_beams)
    {
        const QPointF from = toScreen(b.from), to = toScreen(b.to);
        p.setPen(QPen(QColor(20, 20, 24), s));
        p.setBrush(grouped ? b.colour : QColor(170, 170, 180));
        p.drawRect(QRectF(from - QPointF(3 * s, 3 * s), QSizeF(6 * s, 6 * s)));
        p.setBrush(b.colour.isValid() ? b.colour : LineFormation::pointColour());
        p.drawEllipse(to, r, r);
        p.setPen(grouped ? LineFormation::groupInk() : LineFormation::pointInk());
        p.drawText(QRectF(to.x() - r, to.y() - r, 2 * r, 2 * r), Qt::AlignCenter, QString::number(b.number));
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
        for (int i : order)
        {
            const Beam &b = m_beams.at(i);
            const QPointF from = toScreen(b.from);
            const QString text = fm.elidedText(b.name, Qt::ElideMiddle, maxW);
            const double w = fm.horizontalAdvance(text) + 2.0 * s;
            for (int row = 0; row < 4; ++row)
            {
                QRectF box(from.x() - w / 2, from.y() + 5.0 * s + row * rowH, w, rowH);
                if (box.left() < room.left())
                    box.moveLeft(room.left());
                if (box.right() > room.right())
                    box.moveRight(room.right());
                if (!room.contains(box))
                    break;
                bool clear = true;
                for (const QRectF &o : m_labelRects)
                    if (o.intersects(box))
                    {
                        clear = false;
                        break;
                    }
                if (!clear)
                    continue;
                if (row > 0)
                    leaders << QLineF(from + QPointF(0, 3.0 * s), QPointF(from.x(), box.top()));
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
    m_plan->setFormation(scene, look, prevScene, prevLook);
    m_strip->setDimmed(false);
    m_plan->setDimmed(false);
    m_note->hide();
}

void LinePlanWindow::setNoAim()
{
    m_strip->setDimmed(true);
    m_plan->setDimmed(true);
    m_note->setText(tr("The selection has no line aim. Showing the last plan."));
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
