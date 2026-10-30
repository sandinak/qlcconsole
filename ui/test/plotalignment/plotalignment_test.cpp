/*
  Q Light Controller Plus - Test Unit
  plotalignment_test.cpp

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

#include <QtTest>
#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsLineItem>
#include <QScrollBar>
#include <QPainter>
#include <QTextStream>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QScopedPointer>
#include <QDir>
#include <QFileInfo>
#include <QtMath>
#include <functional>

#define protected public
#define private public
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "monitorruler.h"
#include "monitorfixtureitem.h"
#include "targetitem.h"
#include "trussitem.h"
#include "toweritem.h"
#include "platformitem.h"
#include "pipeitem.h"
#include "standitem.h"
#include "sceneryitem.h"
#include "stagepieceitem.h"
#include "structurestudioview.h"
#include "studioplaneview.h"
#undef private
#undef protected

#include "plotalignment_test.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "qlcfixturedefcache.h"
#include "showembedding.h"
#include "qlcfile.h"
#include "doc.h"
#include "fixture.h"
#include "truss.h"
#include "stageplatform.h"
#include "righeads.h"
#include "fixturevisualtraits.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include <QMetaEnum>

namespace
{

/* ---------------------------------------------------------------------- *
 *  Loading
 * ---------------------------------------------------------------------- */

/* The Engine part of a workspace into @p doc (no I/O, no App), with the
   definitions the show carries put in use first, as App::loadXML does. */
bool loadShow(Doc *doc, const QString &path)
{
    if (doc->showEmbedding()->readWorkspace(path))
        doc->showEmbedding()->apply(QFileInfo(path).absolutePath());
    QXmlStreamReader *xml = QLCFile::getXMLReader(path);
    if (xml == nullptr || xml->device() == nullptr || xml->hasError())
        return false;
    while (!xml->atEnd())
        if (xml->readNext() == QXmlStreamReader::DTD)
            break;
    bool ok = false;
    if (xml->readNextStartElement() && xml->name() == QLatin1String("Workspace"))
    {
        doc->setWorkspacePath(QFileInfo(path).absolutePath());
        while (xml->readNextStartElement())
        {
            if (xml->name() == QLatin1String("Engine"))
                ok = doc->loadXML(*xml, false);
            else
                xml->skipCurrentElement();
        }
    }
    QLCFile::releaseXMLReader(xml);
    return ok;
}

void settle(int ms = 0)
{
    QCoreApplication::processEvents();
    if (ms > 0)
        QTest::qWait(ms);
    QCoreApplication::processEvents();
}

/* ---------------------------------------------------------------------- *
 *  The plot under test: a Monitor in a sized parent, offscreen
 * ---------------------------------------------------------------------- */

struct Plot
{
    QWidget parent;
    QScopedPointer<Monitor> mon;
    MonitorGraphicsView *gv = nullptr;

    explicit Plot(Doc *doc, QSize size = QSize(1400, 900))
    {
        mon.reset(new Monitor(&parent, doc));
        parent.resize(size);
        mon->resize(size);
        parent.show();
        settle(20);
        gv = mon->m_graphicsView;
    }
    ~Plot() { mon.reset(); }

    /* Zoom @p zoom about the grid's centre, turned @p quarterTurns. */
    void setView(double zoom, int quarterTurns)
    {
        gv->setViewRotation(quarterTurns);
        QTransform t;
        t.rotate(90.0 * gv->m_viewRotation);
        t.scale(zoom, zoom);
        gv->setTransform(t);
        gv->centerOnGrid();
        emit gv->rulersChanged();
        settle();
    }

    /* Ruler pixel -> viewport pixel along the ruler's axis. */
    double rulerToViewport(bool horiz) const
    {
        QWidget *area = gv->parentWidget();
        const QPoint vp = gv->viewport()->mapTo(area, QPoint(0, 0));
        MonitorRuler *r = horiz ? mon->m_hRuler : mon->m_vRuler;
        const QPoint rp = r->mapTo(area, QPoint(0, 0));
        return horiz ? double(rp.x() - vp.x()) : double(rp.y() - vp.y());
    }

    /* What a mouse move over an empty corner of the canvas leaves on the
       rulers: the app's only trigger for the band before the fix. */
    void hoverCorner()
    {
        const QPoint at(2, 2);
        QMouseEvent me(QEvent::MouseMove, QPointF(at), QPointF(gv->viewport()->mapToGlobal(at)),
                       Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(gv->viewport(), &me);
    }

    /* The band as the ruler holds it, in viewport pixels (lo, hi). */
    QPair<double, double> band(bool horiz) const
    {
        MonitorRuler *r = horiz ? mon->m_hRuler : mon->m_vRuler;
        const auto range = r->itemRange();
        if (double(range.first) < 0 && double(range.second) < 0)
            return qMakePair(qQNaN(), qQNaN());
        const double off = rulerToViewport(horiz);
        const double a = double(range.first) + off, b = double(range.second) + off;
        return qMakePair(qMin(a, b), qMax(a, b));
    }

    /* The horizontal (or vertical) ruler's reading at viewport pixel @p px:
       linear between the two ticks around it, as the eye reads it. NaN when
       the ticks don't spread along the axis (unusable). */
    double rulerReading(bool horiz, double px) const
    {
        const QVector<MonitorGraphicsView::RulerTick> ticks = gv->rulerTicks(horiz);
        QVector<QPair<double, double>> pts;
        for (const auto &t : ticks)
        {
            // The signed value: a centre-line ruler labels its ticks unsigned.
            pts.append(qMakePair(double(t.pixel), double(t.value)));
        }
        std::sort(pts.begin(), pts.end());
        for (int i = 0; i + 1 < pts.size(); i++)
        {
            const double p0 = pts.at(i).first, p1 = pts.at(i + 1).first;
            if (px >= p0 && px <= p1 && p1 - p0 > 0.5)
                return pts.at(i).second + (px - p0) / (p1 - p0) * (pts.at(i + 1).second - pts.at(i).second);
        }
        return qQNaN();
    }
};

/* ---------------------------------------------------------------------- *
 *  The truth: an item's own geometry (what it IS on the stage), in item
 *  coordinates -- not its paint extent.
 * ---------------------------------------------------------------------- */

QRectF itemGeometry(const QGraphicsItem *it, QString *kind)
{
    if (auto *t = dynamic_cast<const TargetItem *>(it))
    {
        *kind = t->isLine() ? "target-line" : (t->footprintRect().isValid() ? "target-area" : "target");
        const qreal r = TargetItem::kRadius;
        QRectF g(-r, -r, 2 * r, 2 * r);              // the ring: the mark itself
        if (t->footprintRect().isValid())
            g |= t->footprintRect();
        if (t->isLine())
            g |= t->m_curve.boundingRect();
        if (!t->figurePath().isEmpty())
            g |= t->figurePath().boundingRect();     // a person's shoulders
        return g;
    }
    if (auto *f = dynamic_cast<const MonitorFixtureItem *>(it))
    {
        *kind = "fixture";
        return f->iconRect();
    }
    if (auto *tr = dynamic_cast<const TrussItem *>(it))
    {
        *kind = "truss";
        return tr->footprintRect();
    }
    if (auto *tw = dynamic_cast<const TowerItem *>(it))
    {
        *kind = "tower";
        return tw->footprintRect();
    }
    if (auto *p = dynamic_cast<const PlatformItem *>(it))
    {
        *kind = "platform";
        return QRectF(0, 0, p->m_pxW, p->m_pxD);
    }
    if (auto *pp = dynamic_cast<const PipeItem *>(it))
    {
        *kind = "pipe";
        if (pp->m_horizontal)
            return QRectF(QPointF(0, 0), pp->m_pxEnd).normalized();
        const qreal r = qMax(pp->m_pxBaseR, pp->m_pxPipe / 2.0f);
        return QRectF(-r, -r, 2 * r, 2 * r);
    }
    if (auto *s = dynamic_cast<const StandItem *>(it))
    {
        *kind = "stand";
        return QRectF(-s->m_pxBaseR, -s->m_pxBaseR, 2 * s->m_pxBaseR, 2 * s->m_pxBaseR);
    }
    if (auto *sc = dynamic_cast<const SceneryItem *>(it))
    {
        *kind = "scenery";
        return sc->m_bounds;
    }
    if (auto *sp = dynamic_cast<const StagePieceItem *>(it))
    {
        *kind = "stagepiece";
        return sp->m_hit.boundingRect();
    }
    *kind = QString();
    return QRectF();
}

QString itemName(const QGraphicsItem *it)
{
    if (auto *t = dynamic_cast<const TargetItem *>(it))
        return t->target() ? t->target()->name() : QString();
    if (auto *f = dynamic_cast<const MonitorFixtureItem *>(it))
        return QStringLiteral("fixture %1").arg(f->fixtureID());
    return QString();
}

/* The item's geometry in viewport pixels. */
QRectF geometryInViewport(MonitorGraphicsView *gv, const QGraphicsItem *it, QString *kind)
{
    const QRectF g = itemGeometry(it, kind);
    if (kind->isEmpty())
        return QRectF();
    const QPolygonF scene = it->mapToScene(g);
    return gv->viewportTransform().map(scene).boundingRect();
}

QList<QGraphicsItem *> selectableItems(MonitorGraphicsView *gv)
{
    QList<QGraphicsItem *> out;
    foreach (QGraphicsItem *gi, gv->m_scene->items())
        if (gi->parentItem() == nullptr && gi->isVisible()
            && (gi->flags() & QGraphicsItem::ItemIsSelectable))
            out << gi;
    return out;
}

/* "SR 1' 4\"" / "SL 2.4" / "CL" -> signed units across (+ stage left). */
double parseAcross(const QString &readout, bool feet, bool *ok)
{
    *ok = false;
    const QString first = readout.section(QStringLiteral(" · "), 0, 0).trimmed();
    if (first == QLatin1String("CL"))
    {
        *ok = true;
        return 0.0;
    }
    static const QRegularExpression ft(QStringLiteral("^(SL|SR) (\\d+)'(?: (\\d+)\")?$"));
    static const QRegularExpression m(QStringLiteral("^(SL|SR) ([0-9.]+)(?: m)?$"));
    QRegularExpressionMatch mm = (feet ? ft : m).match(first);
    if (!mm.hasMatch())
        return 0.0;
    double v = feet ? mm.captured(2).toDouble() + mm.captured(3).toDouble() / 12.0
                    : mm.captured(2).toDouble();
    *ok = true;
    return mm.captured(1) == QLatin1String("SR") ? -v : v;
}

/* A crop around @p focus (viewport px) of the plot WITH its rulers,
   magnified, the item's own extent drawn in magenta across both rulers. */
QImage annotatedCrop(Plot &plot, const QRectF &truthVp, const QRectF &bandVp, int scaleUp = 3)
{
    QWidget *area = plot.gv->parentWidget();
    QImage img = area->grab().toImage();
    const qreal dpr = img.devicePixelRatio();
    const QPoint vpAt = plot.gv->viewport()->mapTo(area, QPoint(0, 0));
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, false);
    QPen pen(QColor(255, 0, 255), 0);
    p.setPen(pen);
    const QRectF t = truthVp.translated(vpAt);
    // Truth extent: down through the top ruler and across the left one.
    p.drawLine(QPointF(t.left(), 0), QPointF(t.left(), t.bottom()));
    p.drawLine(QPointF(t.right(), 0), QPointF(t.right(), t.bottom()));
    p.drawLine(QPointF(0, t.top()), QPointF(t.right(), t.top()));
    p.drawLine(QPointF(0, t.bottom()), QPointF(t.right(), t.bottom()));
    p.end();
    QRectF focus = truthVp.united(bandVp.isValid() ? bandVp : truthVp).translated(vpAt);
    focus = focus.adjusted(-60, -60, 60, 60);
    focus.setTop(0);
    focus.setLeft(qMax(qreal(0), focus.left()));
    QRect crop(QPoint(int(focus.left() * dpr), 0), QPoint(int(focus.right() * dpr), int(focus.bottom() * dpr)));
    crop = crop.intersected(QRect(QPoint(0, 0), img.size()));
    QImage out = img.copy(crop);
    return out.scaled(out.size() * scaleUp, Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

/* ---------------------------------------------------------------------- *
 *  A small canonical stage for the gate tests
 * ---------------------------------------------------------------------- */

/* Dance Hysteria's stage (50 x 36 ft) -- in metres that is 15.24 x 10.97,
   which the plot draws as whole cells -- with a person 1 ft stage right of
   centre, an area, a truss, a deck and a free fixture. */
struct Canonical
{
    StageTarget *person = nullptr;
    StageTarget *area = nullptr;
    quint32 trussId = 0;
    quint32 fixtureId = 0;
};

Canonical buildCanonical(Doc *doc, MonitorProperties::GridUnits units, bool names = true)
{
    Canonical c;
    MonitorProperties *props = doc->monitorProperties();
    props->setGridUnits(MonitorProperties::Feet);
    props->setGridSize(QVector3D(50.0f, 10.0f, 36.0f));
    props->convertGridUnits(units);
    props->setTargetPositionsVisible(names);
    c.person = props->addStageTarget();
    c.person->setName(names ? "Announcer" : "");
    c.person->setColor(QColor(255, 0, 255));
    c.person->setPosition(QVector3D(float(24.0 * 0.3048), 9.596f, 0.0f));   // SR 1'
    c.area = props->addStageTarget();
    c.area->setName(names ? "Area" : "");
    c.area->setKind(StageTarget::Kind::Area);
    c.area->setFootprint(QSizeF(2.0, 1.5));
    c.area->setPosition(QVector3D(3.0f, 6.7f, 0.0f));
    Truss *t = props->addTruss();
    t->setName("Pipe 1");
    t->setOrigin(QVector3D(4.0f, 3.0f, 5.0f));
    t->setLength(6.0f);
    t->setWidth(0.3f);
    t->setDirection(QPointF(1, 0));
    c.trussId = t->id();
    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(9.0f); deck->setOriginY(2.0f);
    Fixture *fx = new Fixture(doc);
    fx->setName("Free par");
    fx->setChannels(3);
    doc->addFixture(fx);
    c.fixtureId = fx->id();
    props->setFixturePosition(fx->id(), 0, 0, QVector3D(11500.0f, 7200.0f, 0.0f));
    return c;
}

void addViewRows()
{
    QTest::addColumn<int>("units");
    QTest::addColumn<int>("rot");
    QTest::addColumn<double>("zoom");
    for (int u : { int(MonitorProperties::Feet), int(MonitorProperties::Meters) })
        for (int rot : { 0, 1, 2 })
            for (double zoom : { 1.0, 4.0, 0.25, 2.5 })
                QTest::newRow(qPrintable(QStringLiteral("%1 rot%2 x%3").arg(u == 0 ? "m" : "ft").arg(rot).arg(zoom)))
                    << u << rot << zoom;
}

/* Half a device pixel, in logical pixels -- and never tighter than the
   0.5 px the plan asks for. */
double tolerancePx()
{
    return 0.5;
}

} // namespace

/* ---------------------------------------------------------------------- *
 *  Gate tests
 * ---------------------------------------------------------------------- */

void PlotAlignment_Test::initTestCase()
{
    qInfo() << "device pixel ratio" << qApp->devicePixelRatio();
}

void PlotAlignment_Test::rulersRegisterWithTheViewport()
{
    QScopedPointer<Doc> doc(new Doc(nullptr));
    buildCanonical(doc.data(), MonitorProperties::Feet);
    Plot plot(doc.data(), QSize(1000, 700));
    QCOMPARE(plot.rulerToViewport(true), 0.0);
    QCOMPARE(plot.rulerToViewport(false), 0.0);
}

void PlotAlignment_Test::rulerTicksSitOnTheirGridLines_data()
{
    addViewRows();
}

/* Independent of the ruler code: at each tick's pixel, ask the view what
   world position is there (mapToScene, pixelsToRealPosition) and which world
   axis changes along the ruler; the label must be that coordinate. */
void PlotAlignment_Test::rulerTicksSitOnTheirGridLines()
{
    QFETCH(int, units);
    QFETCH(int, rot);
    QFETCH(double, zoom);
    QScopedPointer<Doc> doc(new Doc(nullptr));
    buildCanonical(doc.data(), MonitorProperties::GridUnits(units));
    Plot plot(doc.data(), QSize(1000, 700));
    plot.setView(zoom, rot);
    MonitorGraphicsView *gv = plot.gv;
    const double unitMm = gv->m_unitValue;
    const double cellVp = gv->m_cellPixels * zoom;
    const QRect vr = gv->viewport()->rect();

    for (bool horiz : { true, false })
    {
        const auto ticks = gv->rulerTicks(horiz);
        QVERIFY(ticks.size() > 2);
        // World at a viewport point (unrounded).
        auto worldAt = [&](double along) {
            const QPointF vp = horiz ? QPointF(along, vr.height() / 2.0) : QPointF(vr.width() / 2.0, along);
            const QPointF sc = gv->viewportTransform().inverted().map(vp);
            return gv->pixelsToRealPosition(sc.x(), sc.y());
        };
        const QPointF w0 = worldAt(100), w1 = worldAt(110);
        const bool worldY = qAbs(w1.y() - w0.y()) > qAbs(w1.x() - w0.x());
        int checked = 0;
        for (const auto &t : ticks)
        {
            if (t.pixel < 0 || t.pixel > (horiz ? vr.width() : vr.height()))
                continue;
            const double label = t.value;
            // The label says the value: unsigned from the centre line ("CL" at 0).
            if (!worldY && gv->rulerCountsFromCentreLine())
                QCOMPARE(t.label, t.value == 0.0 ? QStringLiteral("CL") : QString::number(qAbs(qRound(t.value))));
            else
                QCOMPARE(t.label.toDouble(), double(t.value));
            const QPointF w = worldAt(t.pixel);
            const double expect = (worldY ? w.y() : w.x()) / unitMm - gv->rulerZeroUnits(worldY);
            const double errPx = qAbs(label - expect) * cellVp;
            if (errPx > tolerancePx())
                QFAIL(qPrintable(QStringLiteral("%1 ruler tick '%2' at %3 px is %4 there: off by %5 px")
                    .arg(horiz ? "top" : "left", t.label).arg(t.pixel).arg(expect).arg(errPx)));
            checked++;
        }
        QVERIFY2(checked >= 2, horiz ? "top ruler has no ticks on screen" : "left ruler has no ticks on screen");
    }
}

void PlotAlignment_Test::theBandSpansTheItem_data()
{
    addViewRows();
}

/* Every kind in the canonical plot, selected alone: the band on each ruler
   (read back from the ruler widget, in its own pixels, through the strip's
   registration) spans the item's own geometry. For the person, the band's
   middle is also where the target IS (projectMm of its position). */
void PlotAlignment_Test::theBandSpansTheItem()
{
    QFETCH(int, units);
    QFETCH(int, rot);
    QFETCH(double, zoom);
    QScopedPointer<Doc> doc(new Doc(nullptr));
    const Canonical c = buildCanonical(doc.data(), MonitorProperties::GridUnits(units));
    Plot plot(doc.data(), QSize(1000, 700));
    plot.setView(zoom, rot);
    MonitorGraphicsView *gv = plot.gv;

    const QList<QGraphicsItem *> items = selectableItems(gv);
    QVERIFY(items.size() >= 5);
    int checked = 0;
    for (QGraphicsItem *it : items)
    {
        QString kind;
        geometryInViewport(gv, it, &kind);
        if (kind.isEmpty())
            continue;
        // Scrolled to it, off centre: the scroll is part of the mapping.
        gv->centerOn(it->mapToScene(MonitorGraphicsView::itemGeometryRect(it).center()) + QPointF(37 / zoom, 23 / zoom));
        settle();
        const QRectF truth = geometryInViewport(gv, it, &kind);
        gv->m_scene->clearSelection();
        it->setSelected(true);
        settle();                                   // no mouse: the band follows the selection
        for (bool horiz : { true, false })
        {
            const auto b = plot.band(horiz);
            QVERIFY2(!qIsNaN(b.first), qPrintable(kind + " selected, no band"));
            const double tLo = horiz ? truth.left() : truth.top();
            const double tHi = horiz ? truth.right() : truth.bottom();
            if (qAbs(b.first - tLo) > tolerancePx() || qAbs(b.second - tHi) > tolerancePx())
                QFAIL(qPrintable(QStringLiteral("%1 %2 band %3..%4 px, the item is %5..%6 px")
                    .arg(kind, horiz ? "top" : "left").arg(b.first).arg(b.second).arg(tLo).arg(tHi)));
        }
        if (TargetItem *ti = dynamic_cast<TargetItem *>(it); ti != nullptr && ti->target() == c.person)
        {
            const QVector3D p = c.person->position();
            const QPointF at = gv->sceneToViewport(gv->projectMm(p.x() * 1000.0, p.y() * 1000.0, p.z() * 1000.0));
            const auto bh = plot.band(true), bv = plot.band(false);
            QVERIFY(qAbs((bh.first + bh.second) / 2 - at.x()) <= tolerancePx());
            QVERIFY(qAbs((bv.first + bv.second) / 2 - at.y()) <= tolerancePx());
        }
        checked++;
    }
    QVERIFY(checked >= 3);
}

void PlotAlignment_Test::theBandFollowsTheViewWithoutTheMouse()
{
    QScopedPointer<Doc> doc(new Doc(nullptr));
    const Canonical c = buildCanonical(doc.data(), MonitorProperties::Feet);
    Plot plot(doc.data(), QSize(1000, 700));
    MonitorGraphicsView *gv = plot.gv;
    TargetItem *ti = gv->m_targetItems.value(c.person->id());
    QVERIFY(ti != nullptr);

    auto centreError = [&]() {
        QString kind;
        const QRectF truth = geometryInViewport(gv, gv->m_targetItems.value(c.person->id()), &kind);
        const auto b = plot.band(true);
        return qIsNaN(b.first) ? 1e9 : qAbs((b.first + b.second) / 2 - truth.center().x());
    };

    ti->setSelected(true);                          // as the Layers panel does
    settle();
    QVERIFY2(centreError() <= tolerancePx(), "selected without the mouse");
    gv->scale(3.0, 3.0);
    emit gv->rulersChanged();                       // as a keyboard zoom does
    settle();
    QVERIFY2(centreError() <= tolerancePx(), "after a zoom");
    gv->horizontalScrollBar()->setValue(gv->horizontalScrollBar()->value() + 137);
    settle();
    QVERIFY2(centreError() <= tolerancePx(), "after a scroll");
    gv->setViewRotation(2);
    gv->m_targetItems.value(c.person->id())->setSelected(true);   // items are rebuilt
    settle();
    QVERIFY2(centreError() <= tolerancePx(), "after a half turn");
    gv->m_scene->clearSelection();
    settle();
    QVERIFY2(qIsNaN(plot.band(true).first), "nothing selected, still a band");
}

void PlotAlignment_Test::theCentreLineIsTheReadoutsCentre_data()
{
    QTest::addColumn<int>("units");
    QTest::newRow("ft") << int(MonitorProperties::Feet);
    QTest::newRow("m") << int(MonitorProperties::Meters);
}

void PlotAlignment_Test::theCentreLineIsTheReadoutsCentre()
{
    QFETCH(int, units);
    QScopedPointer<Doc> doc(new Doc(nullptr));
    buildCanonical(doc.data(), MonitorProperties::GridUnits(units));
    MonitorProperties *props = doc->monitorProperties();
    Plot plot(doc.data(), QSize(1000, 700));
    MonitorGraphicsView *gv = plot.gv;
    QVERIFY(gv->m_centerLineV != nullptr && gv->m_centerLineH != nullptr);
    const QVector3D sc = props->stageCentre();
    const QPointF at = gv->sceneToViewport(gv->projectMm(sc.x() * 1000.0, sc.y() * 1000.0, 0.0));
    const QPointF v = gv->sceneToViewport(gv->m_centerLineV->line().p1());
    const QPointF h = gv->sceneToViewport(gv->m_centerLineH->line().p1());
    QVERIFY2(qAbs(v.x() - at.x()) <= tolerancePx(),
             qPrintable(QStringLiteral("centre line at %1 px, stage centre at %2 px").arg(v.x()).arg(at.x())));
    QVERIFY2(qAbs(h.y() - at.y()) <= tolerancePx(),
             qPrintable(QStringLiteral("cross line at %1 px, stage centre at %2 px").arg(h.y()).arg(at.y())));
}

/* The person is 1 ft stage right: the readout says so, and the rulers read
   1 ft between it and the centre line. The ruler counts from the centre line
   too (0, marked CL, on it; Q179), unless a ⌖ origin is set. */
void PlotAlignment_Test::theReadoutAgreesWithTheRuler()
{
    QScopedPointer<Doc> doc(new Doc(nullptr));
    const Canonical c = buildCanonical(doc.data(), MonitorProperties::Feet);
    Plot plot(doc.data(), QSize(1000, 700));
    MonitorGraphicsView *gv = plot.gv;
    TargetItem *ti = gv->m_targetItems.value(c.person->id());
    QVERIFY(ti != nullptr);
    QCOMPARE(ti->positionReadout().section(QStringLiteral(" · "), 0, 0), QStringLiteral("SR 1'"));
    for (double zoom : { 1.0, 4.0 })
    {
        plot.setView(zoom, 0);
        ti = gv->m_targetItems.value(c.person->id());
        const double atItem = plot.rulerReading(true, gv->sceneToViewport(ti->scenePos()).x());
        const double atLine = plot.rulerReading(true, gv->sceneToViewport(gv->m_centerLineV->line().p1()).x());
        const double tol = tolerancePx() / (gv->m_cellPixels * zoom);
        QVERIFY2(qAbs((atItem - atLine) - (-1.0)) <= tol,
                 qPrintable(QStringLiteral("ruler reads %1 ft from the centre line").arg(atItem - atLine)));
        QVERIFY2(qAbs(atLine) <= tol, qPrintable(QStringLiteral("the ruler reads %1 at the centre line, not 0").arg(atLine)));
        QVERIFY2(qAbs(atItem - (-1.0)) <= tol, qPrintable(QStringLiteral("the ruler reads %1 at the person, not SR 1").arg(atItem)));
        // The CL tick is on the centre line, and stage right is on the left.
        bool sawCl = false;
        for (const auto &t : gv->rulerTicks(true))
            if (t.label == QStringLiteral("CL"))
            {
                sawCl = true;
                QVERIFY(qAbs(t.pixel - gv->sceneToViewport(gv->m_centerLineV->line().p1()).x()) <= tolerancePx());
            }
        QVERIFY2(sawCl, "no CL tick on the top ruler");
        QVERIFY(gv->stageRightFirst(true));
    }
    // A half turn: stage left comes first.
    plot.setView(1.0, 2);
    QVERIFY(!gv->stageRightFirst(true));
    // A ⌖ origin is a zero the user chose: the ruler counts from it again.
    gv->setStageOriginMetres(QPointF(2.0, 3.0));
    QVERIFY(!gv->rulerCountsFromCentreLine());
    QCOMPARE(gv->rulerZeroUnits(false), gv->originUnitsH());
}

void PlotAlignment_Test::aQuarterTurnRePlacesFixtures()
{
    QScopedPointer<Doc> doc(new Doc(nullptr));
    const Canonical c = buildCanonical(doc.data(), MonitorProperties::Feet);
    Plot plot(doc.data(), QSize(1000, 700));                  // not square: a turn refits
    MonitorGraphicsView *gv = plot.gv;
    for (int rot : { 1, 2, 3, 0 })
    {
        gv->setViewRotation(rot);
        settle();
        MonitorFixtureItem *fi = gv->m_fixtures.value(c.fixtureId);
        QVERIFY(fi != nullptr);
        const QPointF shown = fi->pos();
        const QSizeF size = fi->iconRect().size();
        gv->updateFixture(c.fixtureId);                        // where it belongs now
        QVERIFY2(QLineF(shown, fi->pos()).length() <= tolerancePx() && size == fi->iconRect().size(),
                 qPrintable(QStringLiteral("turn %1: fixture at %2,%3, belongs at %4,%5")
                     .arg(rot).arg(shown.x()).arg(shown.y()).arg(fi->pos().x()).arg(fi->pos().y())));
    }
}

void PlotAlignment_Test::theRenderedBandCoversTheRenderedItem_data()
{
    QTest::addColumn<int>("rot");
    QTest::addColumn<double>("zoom");
    for (int rot : { 0, 2 })
        for (double zoom : { 1.0, 2.5, 4.0 })
            QTest::newRow(qPrintable(QStringLiteral("rot%1 x%2").arg(rot).arg(zoom))) << rot << zoom;
}

/* Pixels, not geometry: grab the plot with its rulers, find the magenta
   person's columns on the canvas and the band's columns on the top ruler. */
void PlotAlignment_Test::theRenderedBandCoversTheRenderedItem()
{
    QFETCH(int, rot);
    QFETCH(double, zoom);
    QScopedPointer<Doc> doc(new Doc(nullptr));
    const Canonical c = buildCanonical(doc.data(), MonitorProperties::Feet, false);
    Plot plot(doc.data(), QSize(1000, 700));
    plot.setView(zoom, rot);
    MonitorGraphicsView *gv = plot.gv;
    TargetItem *ti = gv->m_targetItems.value(c.person->id());
    QVERIFY(ti != nullptr);
    gv->centerOn(ti);
    ti->setSelected(true);
    settle(10);

    QWidget *area = gv->parentWidget();
    const QImage img = area->grab().toImage().convertToFormat(QImage::Format_RGB32);
    const qreal dpr = img.devicePixelRatio();
    const QPoint vpAt = gv->viewport()->mapTo(area, QPoint(0, 0));
    const QPointF centre = gv->sceneToViewport(ti->scenePos()) + QPointF(vpAt);
    const qreal r = TargetItem::kRadius * zoom;

    // The person on the canvas: magenta-ish pixels in the ring's rows.
    int lo = INT_MAX, hi = INT_MIN;
    for (int y = int((centre.y() - r) * dpr); y <= int((centre.y() + r) * dpr); y++)
        for (int x = int((centre.x() - 3 * r) * dpr); x <= int((centre.x() + 3 * r) * dpr); x++)
        {
            if (x < 0 || y < 0 || x >= img.width() || y >= img.height())
                continue;
            const QColor col = img.pixelColor(x, y);
            if (col.red() > 150 && col.blue() > 150 && col.green() < 120)
            {
                lo = qMin(lo, x);
                hi = qMax(hi, x);
            }
        }
    QVERIFY2(lo <= hi, "the person is not on the canvas");
    const double itemMid = (lo + hi + 1) / 2.0 / dpr;

    // The band on the top ruler: orange-ish columns in the row above the ticks.
    const int ry = int(3 * dpr);
    int blo = INT_MAX, bhi = INT_MIN;
    for (int x = 0; x < img.width(); x++)
    {
        const QColor col = img.pixelColor(x, ry);
        if (col.red() - col.blue() > 40)
        {
            blo = qMin(blo, x);
            bhi = qMax(bhi, x);
        }
    }
    QVERIFY2(blo <= bhi, "no band on the top ruler");
    const double bandMid = (blo + bhi + 1) / 2.0 / dpr;
    const double bandW = (bhi - blo + 1) / dpr;
    // A device pixel either way for each edge's rasterisation.
    QVERIFY2(qAbs(bandMid - itemMid) <= 1.0,
             qPrintable(QStringLiteral("band middle %1 px, person drawn about %2 px").arg(bandMid).arg(itemMid)));
    QVERIFY2(qAbs(bandW - 2 * r) <= 2.0,
             qPrintable(QStringLiteral("band %1 px wide, the ring is %2 px").arg(bandW).arg(2 * r)));
}

/* ---------------------------------------------------------------------- *
 *  The plot's remaining alignment rows (ALIGNMENT.md #17, #19, #20, #22, #23)
 * ---------------------------------------------------------------------- */

/* #17: every grid line ends where the grid does, at every quarter turn, and
   the grid sits in the middle of the canvas. They ran to the widget's size
   less the offset, which under a turn is most of a window past the stage. */
void PlotAlignment_Test::gridLinesEndWhereTheGridDoes()
{
    QScopedPointer<Doc> doc(new Doc(nullptr));
    buildCanonical(doc.data(), MonitorProperties::Feet);
    Plot plot(doc.data(), QSize(1000, 700));                  // not square
    MonitorGraphicsView *gv = plot.gv;
    gv->setGridSubdivisions(2);                                // sub-lines too
    for (int rot : { 0, 1, 2, 3 })
    {
        plot.setView(1.0, rot);
        const qreal x0 = gv->m_xOffset, y0 = gv->m_yOffset;
        const qreal x1 = x0 + gv->m_gridExtent.width() * gv->m_cellPixels;
        const qreal y1 = y0 + gv->m_gridExtent.height() * gv->m_cellPixels;
        QVERIFY(!gv->m_gridItems.isEmpty());
        for (QGraphicsLineItem *gl : std::as_const(gv->m_gridItems))
        {
            const QLineF l = gl->line();
            for (const QPointF &e : { l.p1(), l.p2() })
                if (e.x() < x0 - 0.5 || e.x() > x1 + 0.5 || e.y() < y0 - 0.5 || e.y() > y1 + 0.5)
                    QFAIL(qPrintable(QStringLiteral("turn %1: a grid line ends at %2,%3, the grid is %4..%5 x %6..%7")
                        .arg(rot).arg(e.x()).arg(e.y()).arg(x0).arg(x1).arg(y0).arg(y1)));
            // ...and reaches across all of it.
            const bool vertical = qAbs(l.p1().x() - l.p2().x()) < 0.01;
            const qreal len = l.length(), want = vertical ? (y1 - y0) : (x1 - x0);
            QVERIFY2(qAbs(len - want) <= 0.5,
                     qPrintable(QStringLiteral("turn %1: a grid line is %2 px long, the grid %3 px").arg(rot).arg(len).arg(want)));
        }
        // The grid drawn is the grid centred: its middle is the canvas's.
        QRectF drawn;
        for (QGraphicsLineItem *gl : std::as_const(gv->m_gridItems))
            drawn |= QRectF(gl->line().p1(), gl->line().p2()).normalized().adjusted(-0.01, -0.01, 0.01, 0.01);
        const QPointF mid = gv->sceneToViewport(drawn.center());
        const QPointF vc = QRectF(gv->viewport()->rect()).center();
        QVERIFY2(QLineF(mid, vc).length() <= 1.0,
                 qPrintable(QStringLiteral("turn %1: the grid's middle is at %2,%3, the canvas's at %4,%5")
                     .arg(rot).arg(mid.x()).arg(mid.y()).arg(vc.x()).arg(vc.y())));
    }
}

/* ALIGNMENT.md #18: the grid is the stage's real size. A 50 ft stage in metres
   is 15.24 cells wide: the outline ends at 15.24, the centre line is 7.62
   in, and a fixture at 15.1 m is on the grid where one at 15.5 m is not. */
void PlotAlignment_Test::theGridIsTheRealStageSize()
{
    QScopedPointer<Doc> doc(new Doc(nullptr));
    const Canonical c = buildCanonical(doc.data(), MonitorProperties::Meters);
    MonitorProperties *props = doc->monitorProperties();
    Plot plot(doc.data(), QSize(1000, 700));
    MonitorGraphicsView *gv = plot.gv;
    const double w = props->gridSize().x(), d = props->gridSize().z();
    QVERIFY2(qAbs(w - 15.24) < 0.01, qPrintable(QString::number(w)));
    QVERIFY(qAbs(gv->m_gridExtent.width() - w) < 1e-3 && qAbs(gv->m_gridExtent.height() - d) < 1e-3);

    // The outline: the outermost lines sit at the real size.
    QRectF drawn;
    for (QGraphicsLineItem *gl : std::as_const(gv->m_gridItems))
        drawn |= QRectF(gl->line().p1(), gl->line().p2()).normalized();
    QVERIFY2(qAbs(drawn.width() - w * gv->m_cellPixels) <= 0.6 && qAbs(drawn.height() - d * gv->m_cellPixels) <= 0.6,
             qPrintable(QStringLiteral("outline %1 x %2 px, the stage is %3 x %4").arg(drawn.width()).arg(drawn.height())
                 .arg(w * gv->m_cellPixels).arg(d * gv->m_cellPixels)));
    // Centred on the canvas, and the centre line is the stage's middle.
    const QPointF mid = gv->sceneToViewport(drawn.center());
    QVERIFY2(QLineF(mid, QRectF(gv->viewport()->rect()).center()).length() <= 1.0, "grid not centred");
    QVERIFY2(qAbs(gv->m_centerLineV->line().x1() - (drawn.left() + drawn.width() / 2)) <= 0.6, "centre line off the middle");

    // Off the grid is measured against 15.24, not 15.
    MonitorProperties *mp = props;
    mp->setFixturePosition(c.fixtureId, 0, 0, QVector3D(15100.0f, 5000.0f, 0.0f));
    QVERIFY(!gv->fixturesOffGrid().contains(c.fixtureId));
    mp->setFixturePosition(c.fixtureId, 0, 0, QVector3D(15500.0f, 5000.0f, 0.0f));
    QVERIFY(gv->fixturesOffGrid().contains(c.fixtureId));
}

/* #19: the Snap setting (Full, 1/2, 1/4) is the step a move snaps to,
   whatever the grid draws; it only switched snapping on. */
void PlotAlignment_Test::snapStepIsTheSnapSetting()
{
    QScopedPointer<Doc> doc(new Doc(nullptr));
    buildCanonical(doc.data(), MonitorProperties::Feet);
    Plot plot(doc.data(), QSize(1000, 700));
    MonitorGraphicsView *gv = plot.gv;
    gv->setGridSubdivisions(1);                                // the grid draws whole cells
    for (int div : { 1, 2, 4 })
    {
        gv->setSnapDivisions(div);
        const qreal inc = qreal(gv->m_cellPixels) / div;
        const QPointF want(gv->m_xOffset + 3 * gv->m_cellPixels + inc, gv->m_yOffset + 2 * gv->m_cellPixels + inc);
        const QPointF got = gv->snapScenePos(want + QPointF(0.3 * inc, -0.3 * inc));
        QVERIFY2(QLineF(got, want).length() <= 0.01,
                 qPrintable(QStringLiteral("snap 1/%1: landed at %2,%3, the step is at %4,%5")
                     .arg(div).arg(got.x()).arg(got.y()).arg(want.x()).arg(want.y())));
    }
}

/* #19 and #20: a free fixture snaps by its CENTRE -- the point its light
   comes from -- not the corner of its icon. */
void PlotAlignment_Test::aDroppedFixtureSnapsItsCentre()
{
    QScopedPointer<Doc> doc(new Doc(nullptr));
    const Canonical c = buildCanonical(doc.data(), MonitorProperties::Feet);
    Plot plot(doc.data(), QSize(1000, 700));
    MonitorGraphicsView *gv = plot.gv;
    gv->setSnapDivisions(2);
    MonitorFixtureItem *fi = gv->m_fixtures.value(c.fixtureId);
    QVERIFY(fi != nullptr);
    const qreal inc = qreal(gv->m_cellPixels) / 2;
    // Off the grid by a third of a step, centre-wise.
    const QPointF centreTo(gv->m_xOffset + 30 * inc + inc / 3, gv->m_yOffset + 20 * inc + inc / 3);
    fi->setPos(centreTo - fi->iconRect().center());
    gv->slotFixtureMoved(fi);
    fi = gv->m_fixtures.value(c.fixtureId);
    const QPointF centre = fi->iconCentreScene();
    auto offStep = [&](qreal v, qreal o) { const qreal k = (v - o) / inc; return qAbs(k - qRound(k)) * inc; };
    QVERIFY2(offStep(centre.x(), gv->m_xOffset) <= 0.5 && offStep(centre.y(), gv->m_yOffset) <= 0.5,
             qPrintable(QStringLiteral("the fixture's centre is at %1,%2: not on a half-cell line")
                 .arg(centre.x()).arg(centre.y())));
}

/* #20: a free fixture's icon is centred on the point its beam and aim line
   start from (RigHeads::fixtureCentre), in every view, and a drop that
   doesn't move it leaves its stored position alone. */
void PlotAlignment_Test::aFreeFixtureIsDrawnWhereItsLightIs()
{
    for (int pov : { int(MonitorGraphicsView::PovTop), int(MonitorGraphicsView::PovFront), int(MonitorGraphicsView::PovSide) })
    {
        QScopedPointer<Doc> doc(new Doc(nullptr));
        const Canonical c = buildCanonical(doc.data(), MonitorProperties::Feet);
        MonitorProperties *props = doc->monitorProperties();
        props->setFixturePosition(c.fixtureId, 0, 0, QVector3D(11500.0f, 7200.0f, 3.5f));
        Plot plot(doc.data(), QSize(1000, 700));
        MonitorGraphicsView *gv = plot.gv;
        gv->setViewPOV(MonitorGraphicsView::ViewPOV(pov));
        settle();
        MonitorFixtureItem *fi = gv->m_fixtures.value(c.fixtureId);
        QVERIFY(fi != nullptr);
        const QVector3D lc = RigHeads::fixtureCentre(doc.data(), c.fixtureId);
        const QPointF want = gv->projectMm(lc.x() * 1000.0, lc.y() * 1000.0, lc.z() * 1000.0);
        const QPointF got = fi->iconCentreScene();
        QVERIFY2(QLineF(got, want).length() <= tolerancePx(),
                 qPrintable(QStringLiteral("view %1: the icon's centre is at %2,%3, its light at %4,%5")
                     .arg(pov).arg(got.x()).arg(got.y()).arg(want.x()).arg(want.y())));
        // Three drops in place: no creep.
        const QVector3D before = props->fixturePosition(c.fixtureId, 0, 0);
        for (int i = 0; i < 3; i++)
        {
            gv->slotFixtureMoved(gv->m_fixtures.value(c.fixtureId));
            settle();
        }
        const QVector3D after = props->fixturePosition(c.fixtureId, 0, 0);
        QVERIFY2(qAbs(after.x() - before.x()) < 1.0f && qAbs(after.y() - before.y()) < 1.0f
                     && qAbs(after.z() - before.z()) < 0.001f,
                 qPrintable(QStringLiteral("view %1: dropped in place, it moved from %2,%3,%4 to %5,%6,%7")
                     .arg(pov).arg(before.x()).arg(before.y()).arg(before.z())
                     .arg(after.x()).arg(after.y()).arg(after.z())));
    }
}

/* #22: at a quarter turn the name label is painted upright, so its hit and
   repaint areas have to turn with it, not stay in the unturned rect. */
void PlotAlignment_Test::theLabelHitAreaTurnsWithIt()
{
    QScopedPointer<Doc> doc(new Doc(nullptr));
    const Canonical c = buildCanonical(doc.data(), MonitorProperties::Feet);
    doc->fixture(c.fixtureId)->setName("Par 12");
    Plot plot(doc.data(), QSize(1000, 700));
    MonitorGraphicsView *gv = plot.gv;
    gv->showFixturesLabels(true);
    for (int rot : { 0, 1, 3 })
    {
        plot.setView(4.0, rot);
        MonitorFixtureItem *fi = gv->m_fixtures.value(c.fixtureId);
        QVERIFY(fi != nullptr && fi->m_labelVisibility);
        const QRectF L = fi->m_labelRect;
        QVERIFY2(L.width() > 1.3 * L.height(), "the label must be wider than tall for this test");
        // Upright on screen: as wide as it is in the item, across the screen.
        const QPointF cV = gv->sceneToViewport(fi->mapToScene(L.center()));
        const qreal z = gv->viewScale();
        const QTransform toScene = gv->viewportTransform().inverted();
        for (const QPointF &d : { QPointF(0.45 * L.width() * z, 0), QPointF(-0.45 * L.width() * z, 0),
                                  QPointF(0, 0.4 * L.height() * z), QPointF(0, -0.4 * L.height() * z) })
        {
            const QPointF q = fi->mapFromScene(toScene.map(cV + d));
            QVERIFY2(fi->shape().contains(q),
                     qPrintable(QStringLiteral("turn %1: a click on the label at %2,%3 px from its middle misses it")
                         .arg(rot).arg(d.x()).arg(d.y())));
            QVERIFY2(fi->boundingRect().contains(q),
                     qPrintable(QStringLiteral("turn %1: the label at %2,%3 px from its middle is outside the repaint area")
                         .arg(rot).arg(d.x()).arg(d.y())));
        }
    }
}

/* #23: the selection halo is a 6 px screen-wide pen 3 scene px out from the
   body. Zoomed out, 3 screen px are more than 3 scene px: the repaint area
   has to cover it, or deselecting leaves halo behind on the canvas. */
void PlotAlignment_Test::theHaloStaysInsideTheRepaintArea()
{
    QScopedPointer<Doc> doc(new Doc(nullptr));
    const Canonical c = buildCanonical(doc.data(), MonitorProperties::Feet);
    Plot plot(doc.data(), QSize(1000, 700));
    MonitorGraphicsView *gv = plot.gv;
    for (double zoom : { 1.0, 0.5, 0.25 })
    {
        plot.setView(zoom, 0);
        MonitorFixtureItem *fi = gv->m_fixtures.value(c.fixtureId);
        QVERIFY(fi != nullptr);
        fi->setSelected(true);
        settle();
        const qreal out = 3.0 + 3.0 / zoom;      // offset + half the screen-wide pen
        const QRectF halo = fi->iconRect().adjusted(-out, -out, out, out);
        QVERIFY2(fi->boundingRect().contains(halo),
                 qPrintable(QStringLiteral("x%1: the halo reaches %2 scene px out, the repaint area %3")
                     .arg(zoom).arg(out).arg(-fi->boundingRect().left())));
    }
}

/* ---------------------------------------------------------------------- *
 *  The structure editor / Rig Overview (StructureStudioView) and the studio
 *  group editor (StudioPlaneView): ALIGNMENT.md A2-A10, the zeros, the grid
 * ---------------------------------------------------------------------- */

namespace
{

/* A 10 x 10 m stage in metres with nothing on it. */
void bareStage(Doc *doc)
{
    MonitorProperties *props = doc->monitorProperties();
    props->setGridUnits(MonitorProperties::Meters);
    props->setGridSize(QVector3D(10.0f, 6.0f, 10.0f));
}

/* One fixture of each shape the studio draws: every stage look, a moving
   head and a generic unit by definition, and two LED bars (thin and thick). */
struct Sample { QString name; quint32 fid; };

QList<Sample> addSampleFixture(Doc *doc, int which, int *count)
{
    if (doc->fixtureDefCache()->fixtureDef("Branson", "LED Movinghead+Circle") == nullptr)
    {
        QDir dir(TEST_FIXTURE_DIR);
        dir.setFilter(QDir::Files);
        dir.setNameFilters(QStringList() << QStringLiteral("*.qxf"));
        doc->fixtureDefCache()->load(dir);
    }
    // One look per shape the studio draws (the rest share these).
    const QList<Fixture::StageLook> looks = { Fixture::MovingSpotLook, Fixture::ParCanLook,
                                              Fixture::FollowspotLook, Fixture::StaffLook,
                                              Fixture::LanternLook };
    *count = looks.size() + 4;
    Fixture *fx = new Fixture(doc);
    QString name;
    if (which < looks.size())
    {
        fx->setChannels(1);
        fx->setStageLook(looks.at(which));
        name = QString::fromLatin1(QMetaEnum::fromType<Fixture::StageLook>().valueToKey(looks.at(which)));
    }
    else if (which == looks.size())
    {
        fx->setChannels(3);
        name = QStringLiteral("generic");
    }
    else if (which == looks.size() + 1)
    {
        if (QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef("Branson", "LED Movinghead+Circle"))
            fx->setFixtureDefinition(def, def->modes().first());
        else
            fx->setChannels(3);
        name = QStringLiteral("moving head");
    }
    else
    {
        const bool thick = which == looks.size() + 3;
        QLCFixtureDef *def = fx->genericRGBPanelDef(8, Fixture::RGB, false);
        QLCFixtureMode *mode = fx->genericRGBPanelMode(def, Fixture::RGB, false, 1000, thick ? 400 : 60);
        fx->setFixtureDefinition(def, mode);
        name = thick ? QStringLiteral("thick LED bar") : QStringLiteral("LED bar");
    }
    fx->setName(name);
    doc->addFixture(fx);
    return { Sample{ name, fx->id() } };
}

/* The rig view's frame as pixels, at its own device pixel ratio. */
QImage grabStudio(QWidget *w)
{
    return w->grab().toImage().convertToFormat(QImage::Format_RGB32);
}

int colourDiff(QRgb a, QRgb b)
{
    return qAbs(qRed(a) - qRed(b)) + qAbs(qGreen(a) - qGreen(b)) + qAbs(qBlue(a) - qBlue(b));
}

/* Square dilation of a mask by @p r pixels. */
QVector<bool> dilate(const QVector<bool> &m, int w, int h, int r)
{
    QVector<bool> a(m.size(), false), out(m.size(), false);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (m[y * w + x])
                for (int k = qMax(0, x - r); k <= qMin(w - 1, x + r); k++)
                    a[y * w + k] = true;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (a[y * w + x])
                for (int k = qMax(0, y - r); k <= qMin(h - 1, y + r); k++)
                    out[k * w + x] = true;
    return out;
}

const char *planeName(int p)
{
    static const char *n[] = { "Top", "Front", "Side", "Angled" };
    return (p >= 0 && p < 4) ? n[p] : "?";
}

} // namespace

/* A2: a member of a turned studio frame. Turning the frame turns the
   fixture's own yaw with it (turnPlatform), so its body is drawn along its
   own axis; the bar's ends turned it AGAIN by the frame, so the hit line,
   the plain bar drawing and the rotate knob were off the body by
   2·ℓ·sin(θ/2). */
void PlotAlignment_Test::studioBarEndsFollowTheBody()
{
    QScopedPointer<Doc> doc(new Doc(nullptr));
    bareStage(doc.data());
    MonitorProperties *props = doc->monitorProperties();
    Fixture *fx = new Fixture(doc.data());
    fx->setName("Strip");
    fx->setChannels(3);
    doc->addFixture(fx);
    props->setFixturePosition(fx->id(), 0, 0, QVector3D(5000, 5000, 1.0f));
    const quint32 gid = 7;
    props->createGroup(gid, "Frame", 0);
    props->setGroupHasFrame(gid, true);
    props->setGroupFrame(gid, QVector3D(3.0f, 4.0f, 0.5f), 30.0f);
    props->setFixtureGroup(fx->id(), gid);
    FixtureRigProps rp = props->fixtureRigProps(fx->id());
    rp.groupLocal = QVector3D(1.0f, 0.5f, 0.0f);
    rp.studioYaw = 30.0f;                    // as turnPlatform() leaves it
    props->setFixtureRigProps(fx->id(), rp);
    QCOMPARE(props->fixtureFrameGroup(fx->id()), gid);

    StructureStudioView view(doc.data(), StructureStudioView::StageKind, 0);
    view.resize(700, 500);
    view.setPlane(StructureStudioView::Top);
    view.reload();
    const FixtureVisualTraits traits = classifyFixture(fx);
    QVector3D box[8];
    view.fixtureBoxCorners(fx->id(), traits, box);
    const QVector3D body = box[1] - box[0];                   // its length, as drawn
    const QVector3D ends = view.fixtureEndB(fx->id()) - view.fixtureEndA(fx->id());
    QVERIFY2((ends - body).length() < 0.005f,
             qPrintable(QStringLiteral("the bar's ends run %1,%2, its body %3,%4")
                 .arg(ends.x()).arg(ends.y()).arg(body.x()).arg(body.y())));
    // ...and they straddle the body's centre.
    const QVector3D mid = (view.fixtureEndA(fx->id()) + view.fixtureEndB(fx->id())) * 0.5f;
    QVERIFY((mid - view.fixtureCentre(fx->id())).length() < 0.005f);
}

/* A3-A7: a click lands on a fixture where, and only where, the fixture is
   drawn. Every shape the studio draws, alone in the Rig Overview, in each
   plane: render it with and without the fixture; every pixel it drew (well
   inside its edge) must pick it, and nothing more than 12 px from anything
   it drew may. */
void PlotAlignment_Test::studioHitAreaIsWhatIsDrawn_data()
{
    QTest::addColumn<int>("plane");
    QTest::addColumn<int>("which");
    QScopedPointer<Doc> probe(new Doc(nullptr));
    int count = 0;
    addSampleFixture(probe.data(), 0, &count);
    for (int plane : { int(StructureStudioView::Top), int(StructureStudioView::Front),
                       int(StructureStudioView::Side), int(StructureStudioView::Angled) })
        for (int which = 0; which < count; which++)
        {
            QScopedPointer<Doc> d(new Doc(nullptr));
            int n = 0;
            const QString name = addSampleFixture(d.data(), which, &n).first().name;
            QTest::newRow(qPrintable(QStringLiteral("%1 %2").arg(planeName(plane), name))) << plane << which;
        }
}

void PlotAlignment_Test::studioHitAreaIsWhatIsDrawn()
{
    QFETCH(int, plane);
    QFETCH(int, which);
    /* Picking is in logical pixels; one pass covers it. test.sh runs the
       program again at a scale of 2 for the overlays, which this needn't. */
    if (qApp->devicePixelRatio() > 1.0)
        QSKIP("the pick is checked once, at a scale of 1");
    QScopedPointer<Doc> doc(new Doc(nullptr));
    bareStage(doc.data());
    MonitorProperties *props = doc->monitorProperties();
    int n = 0;
    const Sample smp = addSampleFixture(doc.data(), which, &n).first();
    props->setFixturePosition(smp.fid, 0, 0, QVector3D(5000, 5000, 3.0f));
    // A second, far away, so the fit is the stage's and not one fixture's.
    Fixture *anchor = new Fixture(doc.data());
    anchor->setChannels(1);
    doc->addFixture(anchor);
    props->setFixturePosition(anchor->id(), 0, 0, QVector3D(500, 500, 0.0f));
    const quint32 hidden = props->addLayer("hidden");
    props->setLayerVisible(hidden, false);
    props->setFixtureLayer(anchor->id(), hidden);

    QWidget host;
    StructureStudioView *view = new StructureStudioView(doc.data(), StructureStudioView::StageKind, 0, &host);
    host.resize(640, 520);
    view->resize(640, 520);
    view->setBeams(false);
    view->setHaze(0.0);                  // the body, not the light round it
    view->setAmbient(1.0);               // lit, so a dark body shows against the floor
    view->setPlane(StructureStudioView::Plane(plane));
    // Fitted to both, then the far one hidden: the near one fills a good part of the view.
    props->setFixtureLayer(anchor->id(), 0);
    view->reload();
    props->setFixtureLayer(anchor->id(), hidden);
    view->zoomAt(view->w2s(RigHeads::fixtureCentre(doc.data(), smp.fid)), view->m_scale * 1.6);
    host.show();
    settle();

    const QImage with = grabStudio(view);
    props->setFixtureLayer(smp.fid, hidden);
    view->rigMayHaveMoved();
    const QImage without = grabStudio(view);
    props->setFixtureLayer(smp.fid, 0);
    view->rigMayHaveMoved();
    (void)grabStudio(view);                          // the frame the picks are made against
    QCOMPARE(with.size(), without.size());
    if (qEnvironmentVariableIsSet("ALIGN_DEBUG"))
    {
        const QString base = qEnvironmentVariable("ALIGN_DEBUG") + QStringLiteral("/%1-%2").arg(planeName(plane), smp.name);
        with.save(base + "-with.png");
        without.save(base + "-without.png");
        QImage hitMap(with.size(), QImage::Format_RGB32);
        const qreal r = with.devicePixelRatio() > 0 ? with.devicePixelRatio() : 1.0;
        for (int y = 0; y < with.height(); y++)
            for (int x = 0; x < with.width(); x++)
                hitMap.setPixel(x, y, view->hitTestFixture(QPointF(x / r, y / r)) == smp.fid
                                      ? qRgb(255, 0, 255) : with.pixel(x, y));
        hitMap.save(base + "-hits.png");
    }

    const int W = with.width(), H = with.height();
    const qreal dpr = with.devicePixelRatio() > 0 ? with.devicePixelRatio() : 1.0;
    QVector<bool> drawn(W * H, false), body(W * H, false);
    int drawnCount = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            const int d = colourDiff(with.pixel(x, y), without.pixel(x, y));
            drawn[y * W + x] = d > 12;
            body[y * W + x] = d > 60;
            drawnCount += d > 12;
        }
    QVERIFY2(drawnCount > 20, qPrintable(QStringLiteral("%1 drew %2 px").arg(smp.name).arg(drawnCount)));

    // Inside: the body, eroded by a device pixel or two of anti-aliasing.
    const int er = qCeil(1.5 * dpr);
    int inside = 0, missed = 0;
    QPointF firstMiss;
    const int inStep = qRound(3 * dpr), outStep = qRound(8 * dpr);   // logical px
    for (int y = er; y < H - er; y += inStep)
        for (int x = er; x < W - er; x += inStep)
        {
            bool core = true;
            for (int dy = -er; dy <= er && core; dy++)
                for (int dx = -er; dx <= er && core; dx++)
                    core = body[(y + dy) * W + (x + dx)];
            if (!core)
                continue;
            inside++;
            const QPointF at(x / dpr, y / dpr);
            if (view->hitTestFixture(at) != smp.fid)
            {
                if (missed++ == 0)
                    firstMiss = at;
            }
        }
    /* A lantern at an angle draws its glow round its glass: light, not
       body, and not what a click is for. */
    const bool glows = plane == int(StructureStudioView::Angled)
                       && classifyFixture(doc->fixture(smp.fid)).kind == FixtureSilhouette::Lantern;
    QVERIFY2(missed == 0 || (glows && missed * 10 < inside),
             qPrintable(QStringLiteral("%1 in %2: %3 of %4 drawn points miss it (first at %5,%6)")
                 .arg(smp.name, planeName(plane)).arg(missed).arg(inside).arg(firstMiss.x()).arg(firstMiss.y())));

    // Outside: well clear of everything it drew.
    const QVector<bool> near = dilate(drawn, W, H, qCeil(12 * dpr));
    int hits = 0;
    QPointF firstHit;
    for (int y = 0; y < H; y += outStep)
        for (int x = 0; x < W; x += outStep)
        {
            if (near[y * W + x])
                continue;
            const QPointF at(x / dpr, y / dpr);
            if (view->hitTestFixture(at) == smp.fid)
            {
                if (hits++ == 0)
                    firstHit = at;
            }
        }
    QVERIFY2(hits == 0,
             qPrintable(QStringLiteral("%1 in %2: %3 points more than 12 px from anything drawn pick it (first at %4,%5)")
                 .arg(smp.name, planeName(plane)).arg(hits).arg(firstHit.x()).arg(firstHit.y())));
}

/* A3: in the 45° view a click picks what is in FRONT: a fixture hidden
   inside a solid riser is not picked through it. */
void PlotAlignment_Test::studioAngledPickHonoursWhatIsInFront()
{
    QScopedPointer<Doc> doc(new Doc(nullptr));
    bareStage(doc.data());
    MonitorProperties *props = doc->monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(3.0f); deck->setOriginY(3.0f);
    deck->setWidth(4.0f); deck->setDepth(4.0f); deck->setHeight(2.0f);
    Fixture *fx = new Fixture(doc.data());
    fx->setName("Buried");
    fx->setChannels(1);
    fx->setStageLook(Fixture::ParCanLook);
    doc->addFixture(fx);
    props->setFixturePosition(fx->id(), 0, 0, QVector3D(5000, 5000, 1.0f));   // inside the riser
    const quint32 hidden = props->addLayer("hidden");
    props->setLayerVisible(hidden, false);

    QWidget host;
    StructureStudioView *view = new StructureStudioView(doc.data(), StructureStudioView::StageKind, 0, &host);
    host.resize(640, 520);
    view->resize(640, 520);
    view->setBeams(false);
    view->setPlane(StructureStudioView::Angled);
    view->reload();
    host.show();
    settle();
    const QImage with = grabStudio(view);
    props->setFixtureLayer(fx->id(), hidden);
    const QImage without = grabStudio(view);
    props->setFixtureLayer(fx->id(), 0);
    (void)grabStudio(view);
    int seen = 0;
    for (int y = 0; y < with.height(); y++)
        for (int x = 0; x < with.width(); x++)
            seen += colourDiff(with.pixel(x, y), without.pixel(x, y)) > 30;
    QVERIFY2(seen < 4, qPrintable(QStringLiteral("the buried fixture shows %1 px: not a test of picking").arg(seen)));
    const QPointF at = view->w2s(RigHeads::fixtureCentre(doc.data(), fx->id()));
    QVERIFY2(view->hitTestFixture(at) != fx->id(), "a click on the riser's top picked the fixture inside it");
}

/* A9: the readout's depth is the buffer's value for the pixel under the
   pointer, taken at its centre; solving at the pointer's own x, y put the
   answer half a pixel off on a slope. On the top of a riser the height is
   exact wherever in the pixel the pointer is. */
void PlotAlignment_Test::studioReadoutSolvesWhereDepthWasSampled()
{
    QScopedPointer<Doc> doc(new Doc(nullptr));
    bareStage(doc.data());
    MonitorProperties *props = doc->monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(3.0f); deck->setOriginY(3.0f);
    deck->setWidth(4.0f); deck->setDepth(4.0f); deck->setHeight(2.0f);
    QWidget host;
    StructureStudioView *view = new StructureStudioView(doc.data(), StructureStudioView::StageKind, 0, &host);
    host.resize(640, 520);
    view->resize(640, 520);
    view->setBeams(false);
    view->setPlane(StructureStudioView::Angled);
    view->reload();
    host.show();
    settle();
    (void)grabStudio(view);
    const QPointF top = view->w2s(QVector3D(5.0f, 5.0f, 2.0f));
    int checked = 0;
    for (double dx : { 0.0, 0.3, 0.5, 0.8, -0.4 })
        for (double dy : { 0.0, 0.2, 0.5, 0.9, -0.3 })
        {
            int known = 0; bool on = false;
            const QVector3D w = view->worldAt(top + QPointF(dx, dy), &known, &on);
            if (!on)
                continue;
            checked++;
            QVERIFY2(qAbs(w.z() - 2.0f) < 0.005f,
                     qPrintable(QStringLiteral("at +%1,%2 the top of the riser reads Z %3").arg(dx).arg(dy).arg(w.z())));
        }
    QVERIFY2(checked > 0, "nothing was under the pointer: the depth buffer was empty");
}

/* A10: the rulers are read over the rig, so they paint over it: a deck
   reaching under them leaves them as they were. */
void PlotAlignment_Test::studioRulersPaintOverTheRig()
{
    QScopedPointer<Doc> doc(new Doc(nullptr));
    bareStage(doc.data());
    MonitorProperties *props = doc->monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(0.0f); deck->setOriginY(0.0f);
    deck->setWidth(10.0f); deck->setDepth(10.0f); deck->setHeight(3.0f);
    const quint32 hidden = props->addLayer("hidden");
    props->setLayerVisible(hidden, false);

    QWidget host;
    StructureStudioView *view = new StructureStudioView(doc.data(), StructureStudioView::StageKind, 0, &host);
    host.resize(640, 520);
    view->resize(640, 520);
    view->setPlane(StructureStudioView::Front);
    view->reload();
    view->zoomAt(QPointF(320, 260), view->m_scale * 3.0);     // the deck fills the view, rulers too
    host.show();
    settle();
    const QImage with = grabStudio(view);
    deck->setLayerId(hidden);
    const QImage without = grabStudio(view);
    const qreal dpr = with.devicePixelRatio() > 0 ? with.devicePixelRatio() : 1.0;
    const int band = qFloor(22 * dpr) - 1, strip = qFloor(34 * dpr) - 1;
    int differ = 0, deckSeen = 0;
    for (int y = 0; y < with.height(); y++)
        for (int x = 0; x < with.width(); x++)
        {
            const bool inRuler = y >= with.height() - band || x < strip;
            const bool d = colourDiff(with.pixel(x, y), without.pixel(x, y)) > 30;
            if (inRuler)
                differ += d;
            else
                deckSeen += d;
        }
    QVERIFY2(deckSeen > 1000, "the deck isn't drawn: not a test of the rulers");
    QVERIFY2(differ == 0, qPrintable(QStringLiteral("the deck paints over %1 ruler px").arg(differ)));
}

/* The three zeros: in the Rig Overview the ruler counts across the stage
   from the centre line, as the plot's ruler and the target readouts do --
   or from the ⌖ origin when one is set. It counted from the stage's corner. */
void PlotAlignment_Test::studioRulerCountsFromThePlotsZero()
{
    QScopedPointer<Doc> doc(new Doc(nullptr));
    bareStage(doc.data());
    MonitorProperties *props = doc->monitorProperties();
    Fixture *fx = new Fixture(doc.data());
    fx->setChannels(1);
    doc->addFixture(fx);
    props->setFixturePosition(fx->id(), 0, 0, QVector3D(2000, 3000, 3.0f));
    StructureStudioView view(doc.data(), StructureStudioView::StageKind, 0);
    view.resize(700, 500);
    for (int plane : { int(StructureStudioView::Top), int(StructureStudioView::Front) })
    {
        view.setPlane(StructureStudioView::Plane(plane));
        view.reload();
        const double cl = view.project(props->stageCentre()).x();
        QVERIFY2(qAbs(view.structureCentreA() - cl) < 1e-6,
                 qPrintable(QStringLiteral("%1: the ruler's 0 is at %2 m, the centre line at %3 m")
                     .arg(planeName(plane)).arg(view.structureCentreA()).arg(cl)));
    }
    // The cursor readout counts from the same zero: H 0 on the centre line.
    view.setPlane(StructureStudioView::Top);
    view.reload();
    const QPointF onCl = view.w2s(QVector3D(props->stageCentre().x(), 4.0f, 0.0f));
    QVERIFY2(view.cursorReadoutText(onCl).startsWith(QStringLiteral("H 0.00 ")),
             qPrintable(QStringLiteral("on the centre line the readout says %1").arg(view.cursorReadoutText(onCl))));
    props->setStageOrigin(QPointF(2.5, 1.0));
    view.reload();
    QVERIFY2(qAbs(view.structureCentreA() - 2.5) < 1e-6, "with a ⌖ origin set, the ruler counts from it");
    const QPointF onOrigin = view.w2s(QVector3D(2.5f, 1.0f, 0.0f));
    QVERIFY2(view.cursorReadoutText(onOrigin).startsWith(QStringLiteral("H 0.00  V 0.00 ")),
             qPrintable(QStringLiteral("on the origin the readout says %1").arg(view.cursorReadoutText(onOrigin))));
}

/* A8 in the studio group editor: a drag keeps the spot you grabbed under the
   pointer. It put the fixture's centre on the pointer, so it jumped by
   wherever on the bar you clicked. */
void PlotAlignment_Test::studioDragKeepsTheGrabOffset()
{
    QScopedPointer<Doc> doc(new Doc(nullptr));
    bareStage(doc.data());
    MonitorProperties *props = doc->monitorProperties();
    Fixture *fx = new Fixture(doc.data());
    fx->setName("Strip");
    fx->setChannels(3);
    doc->addFixture(fx);
    props->setFixturePosition(fx->id(), 0, 0, QVector3D(5000, 5000, 1.0f));
    const quint32 gid = 9;
    props->createGroup(gid, "Frame", 0);
    props->setGroupHasFrame(gid, true);
    props->setGroupFrame(gid, QVector3D(2.0f, 2.0f, 0.0f), 0.0f);
    props->setFixtureGroup(fx->id(), gid);
    FixtureRigProps rp = props->fixtureRigProps(fx->id());
    rp.groupLocal = QVector3D(1.0f, 1.0f, 0.0f);
    props->setFixtureRigProps(fx->id(), rp);

    StudioPlaneView pv(doc.data(), gid);
    pv.resize(600, 400);
    pv.reload();
    auto centrePx = [&]() { return pv.worldToScreen(pv.project(props->fixtureRigProps(fx->id()).groupLocal)); };
    const QPointF c0 = centrePx();
    const QPointF grab = c0 + QPointF(6, 0);                 // on the bar, off its centre
    QCOMPARE(pv.hitTest(grab), fx->id());
    const QPointF to = grab + QPointF(40, 25);
    QMouseEvent press(QEvent::MouseButtonPress, grab, grab, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    pv.mousePressEvent(&press);
    QMouseEvent move(QEvent::MouseMove, to, to, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    pv.mouseMoveEvent(&move);
    QMouseEvent rel(QEvent::MouseButtonRelease, to, to, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    pv.mouseReleaseEvent(&rel);
    const QPointF c1 = centrePx();
    QVERIFY2(QLineF(c1 - c0, QPointF(40, 25)).length() < 0.5,
             qPrintable(QStringLiteral("dragged 40,25 px, the fixture moved %1,%2 px").arg((c1 - c0).x()).arg((c1 - c0).y())));
}

/* The studio group editor's grid steps 1-2-5 in the show's own units: it
   doubled forever (0.1, 0.2, 0.4, 0.8 m), whatever the units. */
void PlotAlignment_Test::studioPlaneGridStepsAreRound_data()
{
    QTest::addColumn<int>("units");
    QTest::addColumn<double>("scale");
    for (int u : { int(MonitorProperties::Meters), int(MonitorProperties::Feet) })
        for (double sc : { 40.0, 70.0, 130.0, 260.0 })
            QTest::newRow(qPrintable(QStringLiteral("%1 %2 px/m").arg(u == int(MonitorProperties::Feet) ? "ft" : "m").arg(sc)))
                << u << sc;
}

void PlotAlignment_Test::studioPlaneGridStepsAreRound()
{
    QFETCH(int, units);
    QFETCH(double, scale);
    QScopedPointer<Doc> doc(new Doc(nullptr));
    bareStage(doc.data());
    MonitorProperties *props = doc->monitorProperties();
    props->setGridUnits(MonitorProperties::GridUnits(units));
    const quint32 gid = 11;
    props->createGroup(gid, "Frame", 0);
    props->setGroupHasFrame(gid, true);
    StudioPlaneView pv(doc.data(), gid);
    pv.resize(800, 300);
    pv.reload();
    pv.show();
    settle();                                 // shown and fitted; then set its zoom
    pv.m_scale = scale;
    pv.m_originPx = QPointF(13.0, 150.0);
    const QImage img = grabStudio(&pv);
    const qreal dpr = img.devicePixelRatio() > 0 ? img.devicePixelRatio() : 1.0;
    // Along a row below the legend: the grid's columns (and the origin axis).
    const int y = qRound(260 * dpr);
    const QRgb bg = qRgb(30, 32, 36);
    QVector<double> lines;
    for (int x = 0; x < img.width(); )
    {
        if (colourDiff(img.pixel(x, y), bg) > 6)
        {
            double sum = 0, wsum = 0;
            int x2 = x;
            while (x2 < img.width() && colourDiff(img.pixel(x2, y), bg) > 6)
            {
                const double w = colourDiff(img.pixel(x2, y), bg);
                sum += x2 * w; wsum += w; x2++;
            }
            lines << (sum / wsum + 0.5) / dpr;
            x = x2;
        }
        else
            x++;
    }
    QVERIFY2(lines.size() >= 3, "no grid on the canvas");
    double stepPx = 1e9;
    for (int i = 1; i < lines.size(); i++)
        stepPx = qMin(stepPx, lines.at(i) - lines.at(i - 1));
    const bool feet = units == int(MonitorProperties::Feet);
    const double stepUnits = stepPx / scale / (feet ? 0.3048 : 1.0);
    QList<double> round;
    if (feet)
        round << 1.0 / 12 << 0.25 << 0.5 << 1 << 2 << 5 << 10 << 20 << 50;
    else
        round << 0.1 << 0.2 << 0.5 << 1 << 2 << 5 << 10 << 20 << 50;
    bool ok = false;
    for (double r : round)
        ok = ok || qAbs(stepUnits - r) / r < 0.03;
    QVERIFY2(ok, qPrintable(QStringLiteral("the grid steps %1 %2 (%3 px)").arg(stepUnits).arg(feet ? "ft" : "m").arg(stepPx)));
    QVERIFY2(stepPx >= 20.0 && stepPx <= 60.0, qPrintable(QStringLiteral("grid lines %1 px apart").arg(stepPx)));
}

/* ---------------------------------------------------------------------- *
 *  The measuring harness (not part of the gate)
 * ---------------------------------------------------------------------- */

void PlotAlignment_Test::measureShows()
{
    const QStringList shows = qEnvironmentVariable("ALIGN_SHOWS").split(';', Qt::SkipEmptyParts);
    const QString outDir = qEnvironmentVariable("ALIGN_OUT");
    if (shows.isEmpty() || outDir.isEmpty())
        QSKIP("set ALIGN_SHOWS and ALIGN_OUT to measure real shows");
    QDir().mkpath(outDir + "/shots");
    const QString tag = qEnvironmentVariable("ALIGN_TAG", "before");
    const qreal dpr = qApp->devicePixelRatio();

    QFile csv(outDir + QStringLiteral("/measure-%1-dpr%2.csv").arg(tag).arg(dpr));
    QVERIFY(csv.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QTextStream out(&csv);
    out << "show,units,rot,zoom,scrolled,dpr,check,item,kind,"
           "err_lo_px,err_hi_px,err_centre_px,err_width_px,err_units,detail\n";

    struct Worst { double px = 0; QString file; };
    QHash<QString, Worst> worst;          // per show+check, one PNG

    for (const QString &show : shows)
    {
        const QString base = QFileInfo(show).completeBaseName();
        for (MonitorProperties::GridUnits units : { MonitorProperties::Feet, MonitorProperties::Meters })
        {
            QScopedPointer<Doc> doc(new Doc(nullptr));
            QVERIFY2(loadShow(doc.data(), show), qPrintable(show));
            MonitorProperties *props = doc->monitorProperties();
            props->convertGridUnits(units);
            const bool feet = (units == MonitorProperties::Feet);
            const QString unitName = feet ? "ft" : "m";
            const double mPerUnit = feet ? 0.3048 : 1.0;

            Plot plot(doc.data());
            MonitorGraphicsView *gv = plot.gv;

            auto row = [&](const QString &cfg, const QString &check, const QString &item,
                           const QString &kind, double lo, double hi, double centre,
                           double width, double units_, const QString &detail) {
                out << base << ',' << unitName << ',' << cfg << ',' << dpr << ','
                    << check << ',' << '"' << item << '"' << ',' << kind << ','
                    << lo << ',' << hi << ',' << centre << ',' << width << ',' << units_
                    << ',' << '"' << QString(detail).replace('"', '\'') << '"' << '\n';
            };

            // Ruler strip vs viewport: one number for the whole plot.
            {
                const QString cfg = "0,1,0";
                row(cfg, "ruler-registration-h", "", "", plot.rulerToViewport(true), 0, 0, 0, 0, "");
                row(cfg, "ruler-registration-v", "", "", plot.rulerToViewport(false), 0, 0, 0, 0, "");
            }

            // Centre line vs the readout's centre (stageCentre), and the grid
            // vs the stage size.
            {
                const QPointF scc = gv->realPositionToPixels(props->stageCentre().x() * 1000.0, 0);
                const double lineX = gv->m_centerLineV ? gv->m_centerLineV->line().x1() : qQNaN();
                const double cellPx = gv->m_cellPixels;
                row("0,1,0", "centre-line-vs-stageCentre", "", "", lineX - scc.x(), 0, lineX - scc.x(), 0,
                    (lineX - scc.x()) / cellPx,
                    QStringLiteral("grid %1x%2 drawn %3x%4").arg(props->gridSize().x()).arg(props->gridSize().z())
                        .arg(gv->m_gridSize.width()).arg(gv->m_gridSize.height()));
            }

            for (int rot : { 0, 2, 1 })
            {
                for (double zoom : { 1.0, 4.0, 0.25, 2.5 })
                {
                    for (bool scrolled : { false, true })
                    {
                        if (scrolled && zoom < 2.0)
                            continue;
                        plot.setView(zoom, rot);
                        const double cellVp = gv->m_cellPixels * zoom;
                        const QString cfg = QStringLiteral("%1,%2,%3").arg(rot).arg(zoom).arg(scrolled ? 1 : 0);

                        // Ruler ticks spread along their axis?
                        for (bool horiz : { true, false })
                        {
                            const auto ticks = gv->rulerTicks(horiz);
                            double lo = 1e9, hi = -1e9;
                            for (const auto &t : ticks) { lo = qMin(lo, double(t.pixel)); hi = qMax(hi, double(t.pixel)); }
                            const double span = ticks.size() > 1 ? (hi - lo) / (ticks.size() - 1) : 0;
                            row(cfg, horiz ? "ruler-tick-spacing-h" : "ruler-tick-spacing-v", "", "",
                                0, 0, 0, span - cellVp, (span - cellVp) / cellVp,
                                QStringLiteral("%1 ticks, mean step %2 px vs cell %3 px").arg(ticks.size()).arg(span).arg(cellVp));
                        }

                        QList<QGraphicsItem *> items = selectableItems(gv);
                        if (scrolled)
                        {
                            // Scroll so the first target (or item) is near a corner.
                            QGraphicsItem *pick = nullptr;
                            for (QGraphicsItem *it : items)
                                if (dynamic_cast<TargetItem *>(it)) { pick = it; break; }
                            if (pick == nullptr && !items.isEmpty())
                                pick = items.first();
                            if (pick)
                                gv->centerOn(pick->scenePos() + QPointF(200 / zoom, 150 / zoom));
                            settle();
                        }
                        for (QGraphicsItem *it : items)
                        {
                            QString kind;
                            QRectF truth = geometryInViewport(gv, it, &kind);
                            if (kind.isEmpty() || (truth.width() <= 0 && truth.height() <= 0))
                                continue;
                            if (!QRectF(gv->viewport()->rect()).intersects(truth.adjusted(-1, -1, 1, 1)))
                                continue;
                            gv->m_scene->clearSelection();
                            it->setSelected(true);
                            settle();
                            // Group members come along (by design): the truth is
                            // the union of what is selected.
                            foreach (QGraphicsItem *o, gv->m_scene->selectedItems())
                            {
                                QString k2;
                                const QRectF g2 = geometryInViewport(gv, o, &k2);
                                if (!k2.isEmpty())
                                    truth |= g2;
                            }
                            plot.hoverCorner();
                            const QString name = itemName(it);
                            for (bool horiz : { true, false })
                            {
                                const auto b = plot.band(horiz);
                                const double tLo = horiz ? truth.left() : truth.top();
                                const double tHi = horiz ? truth.right() : truth.bottom();
                                if (qIsNaN(b.first))
                                {
                                    row(cfg, horiz ? "band-h" : "band-v", name, kind, qQNaN(), qQNaN(), qQNaN(), qQNaN(), qQNaN(), "no band");
                                    continue;
                                }
                                const double eLo = b.first - tLo, eHi = b.second - tHi;
                                const double eC = (b.first + b.second) / 2 - (tLo + tHi) / 2;
                                const double eW = (b.second - b.first) - (tHi - tLo);
                                row(cfg, horiz ? "band-h" : "band-v", name, kind, eLo, eHi, eC, eW,
                                    eC / cellVp, QStringLiteral("band %1..%2 item %3..%4")
                                        .arg(b.first, 0, 'f', 1).arg(b.second, 0, 'f', 1).arg(tLo, 0, 'f', 1).arg(tHi, 0, 'f', 1));
                                const double bad = qMax(qAbs(eLo), qAbs(eHi));
                                const QString key = base + unitName + (horiz ? "band-h" : "band-v") + kind;
                                if (bad > worst.value(key).px && bad > 0.5)
                                {
                                    const QString file = QStringLiteral("%1/shots/%2-%3-%4-%5-%6-rot%7-z%8%9-dpr%10.png")
                                        .arg(outDir, tag, base, unitName, horiz ? "bandh" : "bandv", kind)
                                        .arg(rot).arg(zoom).arg(scrolled ? "-scrolled" : "").arg(dpr);
                                    const QRectF bandVp = horiz ? QRectF(b.first, truth.top(), b.second - b.first, truth.height())
                                                                : QRectF(truth.left(), b.first, truth.width(), b.second - b.first);
                                    if (!worst.value(key).file.isEmpty())
                                        QFile::remove(worst.value(key).file);
                                    annotatedCrop(plot, truth, bandVp).save(file);
                                    worst[key] = Worst{ bad, file };
                                }
                            }

                            // The ruler's reading at the item's centre vs its world
                            // position (in ruler terms), and the readout vs the ruler.
                            const QPointF c = truth.center();
                            if (TargetItem *ti = dynamic_cast<TargetItem *>(it))
                            {
                                const QPointF world = gv->pixelsToRealPosition(ti->scenePos().x(), ti->scenePos().y());
                                const double expectH = world.x() / 1000.0 / mPerUnit - gv->rulerZeroUnits(false);
                                const double gotH = plot.rulerReading(true, c.x());
                                row(cfg, "ruler-reading-at-item-h", name, kind, 0, 0, 0, 0, gotH - expectH,
                                    QStringLiteral("ruler %1 expected %2").arg(gotH, 0, 'f', 3).arg(expectH, 0, 'f', 3));
                                const QString ro = ti->positionReadout();
                                bool ok = false;
                                const double across = parseAcross(ro, feet, &ok);
                                if (ok && rot == 0)
                                {
                                    const double cl = props->stageCentre().x() / mPerUnit - gv->rulerZeroUnits(false);
                                    const double rulerAcross = gotH - cl;
                                    row(cfg, "readout-vs-ruler-across", name, kind, 0, 0, 0, 0, across - rulerAcross,
                                        QStringLiteral("readout '%1' = %2; ruler %3 - CL %4 = %5")
                                            .arg(ro).arg(across, 0, 'f', 3).arg(gotH, 0, 'f', 3).arg(cl, 0, 'f', 3).arg(rulerAcross, 0, 'f', 3));
                                }
                                // Ring centre vs projected world position.
                                const QPointF proj = gv->projectMm(ti->target()->position().x() * 1000.0,
                                                                   ti->target()->position().y() * 1000.0,
                                                                   ti->target()->position().z() * 1000.0);
                                if (!ti->isLine())
                                    row(cfg, "ring-centre-vs-position", name, kind, 0, 0,
                                        QLineF(gv->mapFromScene(proj), gv->mapFromScene(ti->scenePos())).length(), 0, 0, "");
                            }
                        }
                        gv->m_scene->clearSelection();
                        settle();
                    }
                }
            }

            // Named shots, the same in every run: the user's case (the person
            // called Announcer, or the first target, at 4x), and the whole
            // plot a quarter turn round.
            {
                plot.setView(4.0, 0);
                TargetItem *named = nullptr;
                for (TargetItem *ti : gv->m_targetItems)
                    if (named == nullptr || (ti->target() && ti->target()->name() == QLatin1String("Announcer")))
                        named = ti;
                if (named != nullptr)
                {
                    gv->centerOn(named->scenePos() + QPointF(200 / 4.0, 150 / 4.0));
                    gv->m_scene->clearSelection();
                    named->setSelected(true);
                    settle();
                    plot.hoverCorner();
                    QString kind;
                    const QRectF truth = geometryInViewport(gv, named, &kind);
                    const auto b = plot.band(true);
                    annotatedCrop(plot, truth, QRectF(b.first, truth.top(), b.second - b.first, truth.height()))
                        .save(QStringLiteral("%1/shots/%2-%3-%4-selected-target-z4-dpr%5.png")
                              .arg(outDir, tag, base, unitName).arg(dpr));
                }
                plot.setView(1.0, 1);
                gv->m_scene->clearSelection();
                settle();
                gv->parentWidget()->grab().save(QStringLiteral("%1/shots/%2-%3-%4-quarter-turn-dpr%5.png")
                                                .arg(outDir, tag, base, unitName).arg(dpr));
            }

            // Band staleness: select, hover, then zoom/scroll/select without moving.
            plot.setView(1.0, 0);
            QList<QGraphicsItem *> items = selectableItems(gv);
            QGraphicsItem *pick = nullptr;
            for (QGraphicsItem *it : items)
                if (dynamic_cast<TargetItem *>(it)) { pick = it; break; }
            if (pick != nullptr)
            {
                gv->m_scene->clearSelection();
                pick->setSelected(true);
                plot.hoverCorner();
                auto stale = [&](const QString &what) {
                    QString kind;
                    const QRectF truth = geometryInViewport(gv, pick, &kind);
                    const auto b = plot.band(true);
                    const double eC = qIsNaN(b.first) ? qQNaN() : (b.first + b.second) / 2 - truth.center().x();
                    row("0,1,0", "band-stale-after-" + what, itemName(pick), kind, 0, 0, eC, 0,
                        eC / (gv->m_cellPixels * gv->viewScale()), qIsNaN(b.first) ? "no band" : "");
                };
                gv->scale(3.0, 3.0); emit gv->rulersChanged(); settle();
                stale("zoom");
                gv->horizontalScrollBar()->setValue(gv->horizontalScrollBar()->value() + 137); settle();
                stale("scroll");
                gv->m_scene->clearSelection(); settle();
                QGraphicsItem *other = nullptr;
                for (QGraphicsItem *it : items)
                    if (it != pick && dynamic_cast<TargetItem *>(it)) { other = it; break; }
                if (other) { other->setSelected(true); settle(); pick = other; stale("select-without-mouse"); }
            }
        }
    }
    csv.close();
    qInfo() << "wrote" << csv.fileName();
}

QTEST_MAIN(PlotAlignment_Test)
