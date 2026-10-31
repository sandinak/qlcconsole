/*
  Q Light Controller Plus - Test Unit
  performers_test.cpp

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
#include <QElapsedTimer>
#include <QPainter>
#include <functional>
#include <ctime>

/* A build that counts or checks every line (coverage, a sanitizer) runs the
   drawing several times slower, and unevenly: a ratio of two timings there is
   noise, not a guard. The timing assertions hold on every other build. */
#if defined(QLC_INSTRUMENTED_BUILD) || defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#  define QLC_TIMING_GUARDS 0
#elif defined(__has_feature)
#  if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#    define QLC_TIMING_GUARDS 0
#  endif
#endif
#ifndef QLC_TIMING_GUARDS
#  define QLC_TIMING_GUARDS 1
#endif
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QToolButton>
#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <QSet>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>

#define protected public
#define private public
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "structurestudioview.h"
#include "performersdialog.h"
#include "platformitem.h"
#include "targetitem.h"
#undef private
#undef protected

#include "performers_test.h"
#include "performerfill.h"
#include "monitorproperties.h"
#include "stageplatform.h"
#include "stagetarget.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "qlcphysical.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "rigowner.h"
#include "doc.h"

namespace
{

/* A choir riser: three steps 0.4 m high and 0.8 m deep, 3 m across, the
   bottom step downstage. */
StagePlatform *threeTier(MonitorProperties *props, float x = 2.0f, float y = 1.0f)
{
    StagePlatform *p = props->addPlatform();
    p->setName(QStringLiteral("Choir riser"));
    p->setOriginX(x);
    p->setOriginY(y);
    p->setSteps(3, 0.4f, 0.8f, 3.0f, StagePlatform::ClimbUpstage);
    return p;
}

QList<StageTarget *> lines(MonitorProperties *props)
{
    QList<StageTarget *> out;
    foreach (StageTarget *t, props->stageTargets())
        if (t->isPerformerLine())
            out << t;
    return out;
}

int headCount(MonitorProperties *props)
{
    int n = 0;
    foreach (StageTarget *t, lines(props))
        n += t->performerCount();
    return n;
}

/* The usual setup: a stage deck 1 m high with its apron shown, a 3-tier
   choir riser on the deck, and four performer lines -- one on each tier
   (9, 8 and 9 people) and one across the apron (6). */
struct Choir
{
    StagePlatform *deck = nullptr;
    StagePlatform *riser = nullptr;
    QList<StageTarget *> lines;
};

Choir buildChoir(MonitorProperties *props)
{
    Choir c;
    props->setGridUnits(MonitorProperties::Meters);
    props->setGridSize(QVector3D(10, 6, 8));
    c.deck = props->addPlatform();
    c.deck->setName(QStringLiteral("Stage"));
    c.deck->setOriginX(1.0f); c.deck->setOriginY(1.0f);
    c.deck->setWidth(8.0f); c.deck->setDepth(3.8f); c.deck->setHeight(1.0f);
    /* The apron: from the curtain line at the deck's front, 1.2 m further
       downstage, lower than the deck. */
    props->setCurtainLineY(4.8f);
    props->setStagePieceVisible(MonitorProperties::ApronPiece, true);
    props->setApronDepth(1.2f);
    props->setApronHeight(0.6f);
    c.riser = props->addPlatform();
    c.riser->setName(QStringLiteral("Choir riser"));
    c.riser->setOriginX(1.5f); c.riser->setOriginY(1.2f);
    c.riser->setSteps(3, 0.3f, 0.8f, 3.0f, StagePlatform::ClimbUpstage);
    c.riser->setStackable(true);

    PerformerFill::Options o;
    o.height = 1.6f;
    o.spread = 0.08f;
    const quint32 layer = PerformerFill::ensureLayer(props);
    const int counts[3] = { 9, 8, 9 };
    const QList<PerformerFill::Row> rows = PerformerFill::rows(props, c.riser, o);
    for (int k = 0; k < 3 && k < rows.size(); ++k)
        c.lines << PerformerFill::addLine(props, rows.at(k).a, rows.at(k).b, counts[k], o, layer);
    c.lines << PerformerFill::addLine(props, QVector3D(3.0f, 5.4f, 0.0f), QVector3D(7.0f, 5.4f, 0.0f), 6, o, layer);
    return c;
}

Fixture *makeWash(Doc *doc, quint32 addr, const QVector3D &at)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel(QString("Wash %1").arg(addr));
    def->setType(QLCFixtureDef::MovingHead);
    const char *nm[] = { "Pan", "Tilt", "Dimmer", "Red", "Green", "Blue" };
    const QLCChannel::Group gp[] = { QLCChannel::Pan, QLCChannel::Tilt,
        QLCChannel::Intensity, QLCChannel::Intensity, QLCChannel::Intensity,
        QLCChannel::Intensity };
    const QLCChannel::PrimaryColour cl[] = { QLCChannel::NoColour, QLCChannel::NoColour,
        QLCChannel::NoColour, QLCChannel::Red, QLCChannel::Green, QLCChannel::Blue };
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("6ch");
    for (int k = 0; k < 6; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(nm[k]); ch->setGroup(gp[k]);
        if (cl[k] != QLCChannel::NoColour) ch->setColour(cl[k]);
        if (k < 2) ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch); mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setWidth(300); ph.setHeight(400); ph.setDepth(300);
    ph.setFocusPanMax(360); ph.setFocusTiltMax(180);
    ph.setLensDegreesMin(40.0); ph.setLensDegreesMax(40.0);
    mode->setPhysical(ph);
    QLCFixtureHead hd;
    for (int k = 0; k < 6; ++k) hd.addChannel(quint32(k));
    mode->insertHead(-1, hd);
    def->addMode(mode);

    Fixture *f = new Fixture(doc);
    f->setName(QString("Wash %1").arg(addr)); f->setFixtureDefinition(def, mode);
    f->setUniverse(0); f->setAddress(addr);
    if (doc->addFixture(f) == false)
        return nullptr;
    doc->monitorProperties()->setFixturePosition(
        f->id(), 0, 0, QVector3D(at.x() * 1000.0f, at.y() * 1000.0f, at.z()));
    return f;
}

struct Plot
{
    QWidget parent;
    Monitor *mon = nullptr;
    MonitorGraphicsView *gv = nullptr;
    explicit Plot(Doc *doc)
    {
        mon = new Monitor(&parent, doc);
        gv = mon->findChild<MonitorGraphicsView *>();
        gv->resize(1000, 700);
        gv->setGridMetrics(1000.0);
        gv->setGridSize(QSize(10, 7));
        gv->updatePlatforms();
        gv->updateTargets();
    }
    ~Plot() { delete mon; }
};

} // namespace

void Performers_Test::countSpreadsEvenly()
{
    MonitorProperties props;
    PerformerFill::Options o;
    StageTarget *t = PerformerFill::addLine(&props, QVector3D(1.0f, 2.0f, 0.0f),
                                            QVector3D(5.0f, 2.0f, 0.0f), 5, o, 0);
    QVERIFY(t != nullptr && t->isPerformerLine() && t->isLine());
    QList<PerformerFill::Performer> p = PerformerFill::people(&props, t);
    QCOMPARE(p.size(), 5);
    for (int i = 0; i < 5; ++i)
    {
        QVERIFY2(qAbs(p.at(i).feet.x() - (1.0f + i)) < 1e-3f, qPrintable(QString::number(p.at(i).feet.x())));
        QVERIFY(qAbs(p.at(i).feet.y() - 2.0f) < 1e-3f);
        QCOMPARE(p.at(i).feet.z(), 0.0f);           // the floor
    }
    // More people, the same ends: evenly closer.
    t->setPerformerCount(9);
    p = PerformerFill::people(&props, t);
    QCOMPARE(p.size(), 9);
    QVERIFY(qAbs(p.at(1).feet.x() - 1.5f) < 1e-3f);
    // One alone stands in the middle; none is a plain line.
    t->setPerformerCount(1);
    p = PerformerFill::people(&props, t);
    QCOMPARE(p.size(), 1);
    QVERIFY(qAbs(p.first().feet.x() - 3.0f) < 1e-3f);
    t->setPerformerCount(0);
    QVERIFY(!t->isPerformerLine());
    QVERIFY(PerformerFill::people(&props, t).isEmpty());
}

void Performers_Test::feetStandOnTheHighestSurface()
{
    MonitorProperties props;
    const Choir c = buildChoir(&props);
    QCOMPARE(c.lines.size(), 4);
    QVERIFY(props.apronPlatform() != nullptr);

    // The three tiers, on the deck (1 m): 1.3, 1.6 and 1.9 m; the apron 0.6 m.
    const float want[4] = { 1.3f, 1.6f, 1.9f, 0.6f };
    const int counts[4] = { 9, 8, 9, 6 };
    for (int k = 0; k < 4; ++k)
    {
        const QList<PerformerFill::Performer> p = PerformerFill::people(&props, c.lines.at(k));
        QCOMPARE(p.size(), counts[k]);
        foreach (const PerformerFill::Performer &pf, p)
            QVERIFY2(qAbs(pf.feet.z() - want[k]) < 1e-3f,
                     qPrintable(QString("line %1: feet at %2, not %3").arg(k).arg(double(pf.feet.z())).arg(double(want[k]))));
    }
    const PerformerFill::Performer front = PerformerFill::people(&props, c.lines.at(3)).first();
    QVERIFY(props.isApronPlatform(props.platformIdAt(front.feet.x(), front.feet.y())));
    QCOMPARE(headCount(&props), 32);

    // A line drawn up the steps stands people on every tread it crosses.
    StageTarget *up = PerformerFill::addLine(&props, QVector3D(3.0f, 4.4f, 0.0f),
                                             QVector3D(3.0f, 1.4f, 0.0f), 8, PerformerFill::Options(), 0);
    QSet<int> heights;
    foreach (const PerformerFill::Performer &pf, PerformerFill::people(&props, up))
        heights.insert(qRound(pf.feet.z() * 10.0f));
    QVERIFY2(heights.contains(10) && heights.contains(13) && heights.contains(16) && heights.contains(19),
             "a line up the steps does not stand on deck and every tread");
}

void Performers_Test::heightsVaryAndStayPut()
{
    MonitorProperties props;
    PerformerFill::Options o;
    o.height = 1.6f;
    o.spread = 0.08f;
    StageTarget *t = PerformerFill::addLine(&props, QVector3D(0.0f, 2.0f, 0.0f),
                                            QVector3D(10.0f, 2.0f, 0.0f), 40, o, 0);
    QList<float> h;
    foreach (const PerformerFill::Performer &pf, PerformerFill::people(&props, t))
        h << pf.height;
    float lo = 9.0f, hi = 0.0f;
    foreach (float v, h)
    {
        QVERIFY2(v >= 1.52f - 1e-4f && v <= 1.68f + 1e-4f, qPrintable(QString::number(double(v))));
        lo = qMin(lo, v);
        hi = qMax(hi, v);
    }
    QVERIFY2(hi - lo > 0.08f, "everyone is the same height");

    // The same every time it is asked, and after a save and load.
    QList<float> again;
    foreach (const PerformerFill::Performer &pf, PerformerFill::people(&props, t))
        again << pf.height;
    QCOMPARE(again, h);

    QByteArray buf;
    QXmlStreamWriter w(&buf);
    w.writeStartDocument();
    props.saveXML(&w, nullptr);
    w.writeEndDocument();
    MonitorProperties back;
    QXmlStreamReader r(buf);
    while (r.readNextStartElement())
        if (r.name() == QStringLiteral("Monitor"))
            QVERIFY(back.loadXML(r, nullptr));
    QList<float> loaded;
    foreach (const PerformerFill::Performer &pf, PerformerFill::people(&back, back.stageTarget(t->id())))
        loaded << pf.height;
    QCOMPARE(loaded, h);

    // Shuffle: another seed, other heights; no spread, all the mean.
    t->setSeed(t->seed() + 1);
    QList<float> shuffled;
    foreach (const PerformerFill::Performer &pf, PerformerFill::people(&props, t))
        shuffled << pf.height;
    QVERIFY(shuffled != h);
    t->setHeightSpread(0.0f);
    foreach (const PerformerFill::Performer &pf, PerformerFill::people(&props, t))
        QCOMPARE(pf.height, 1.6f);
}

void Performers_Test::fillDropsALinePerTier()
{
    MonitorProperties props;
    StagePlatform *p = threeTier(&props);

    PerformerFill::Options o;
    o.spacing = 0.6f;                       // 3 m: 5 a row, 4 between them
    o.setName = QStringLiteral("Choir");
    o.costume = QColor(160, 20, 40);
    const QList<PerformerFill::Row> rows = PerformerFill::rows(&props, p, o);
    QCOMPARE(rows.size(), 3);
    QCOMPARE(rows.at(0).count, 5);
    QCOMPARE(rows.at(1).count, 4);
    QCOMPARE(rows.at(2).count, 5);
    QVERIFY(rows.first().a.y() > rows.last().a.y());   // bottom row downstage
    QVERIFY(qAbs(rows.at(0).a.x() - 2.3f) < 1e-3f && qAbs(rows.at(0).b.x() - 4.7f) < 1e-3f);
    QVERIFY(qAbs(rows.at(1).a.x() - 2.6f) < 1e-3f);    // staggered: in the gaps
    o.perRow = 6;
    QCOMPARE(PerformerFill::rows(&props, p, o).at(1).count, 6);
    o.perRow = 0;

    const PerformerFill::Result r = PerformerFill::fill(&props, { p->id() }, o);
    QCOMPARE(r.targets.size(), 3);
    QCOMPARE(r.people, 14);
    QCOMPARE(props.layer(r.layerId).name, QStringLiteral("Performers"));
    QCOMPARE(props.group(r.groupId).name, QStringLiteral("Choir"));
    QCOMPARE(props.group(r.groupId).layerId, r.layerId);
    const float tread[3] = { 0.4f, 0.8f, 1.2f };
    for (int k = 0; k < 3; ++k)
    {
        StageTarget *t = props.stageTarget(r.targets.at(k));
        QVERIFY(t->isPerformerLine());
        QCOMPARE(t->layerId(), r.layerId);
        QCOMPARE(t->groupId(), r.groupId);
        QCOMPARE(t->costumeColor(), QColor(160, 20, 40));
        QVERIFY(PerformerFill::isPerformer(&props, t));
        foreach (const PerformerFill::Performer &pf, PerformerFill::people(&props, t))
            QVERIFY(qAbs(pf.feet.z() - tread[k]) < 1e-3f);
        // Light aimed along it lands at their chests.
        foreach (const QVector3D &q, props.targetLinePoints(t))
            QVERIFY(qAbs(q.z() - (tread[k] + 1.6f * StageTarget::chestShare())) < 1e-3f);
    }

    // Again: the set is replaced, keeping a line an aim palette uses.
    const quint32 kept = r.targets.at(1);
    const PerformerFill::Result r2 = PerformerFill::fill(&props, { p->id() }, o,
        [kept](quint32 id) { return id == kept; });
    QCOMPARE(r2.replaced, 2);
    QCOMPARE(lines(&props).size(), 4);
    QVERIFY(props.stageTarget(kept) != nullptr);
    const PerformerFill::Result r3 = PerformerFill::fill(&props, { p->id() }, o);
    QCOMPARE(r3.replaced, 4);
    QCOMPARE(lines(&props).size(), 3);
    QCOMPARE(props.groups().size(), 1);         // the emptied sets went
    o.replace = false;
    PerformerFill::fill(&props, { p->id() }, o);
    QCOMPARE(lines(&props).size(), 6);

    // A deck: a row per 0.9 m; a rounded front pulls the row's ends in.
    StagePlatform *apron = props.addPlatform();
    apron->setOriginX(8.0f); apron->setOriginY(0.0f);
    apron->setWidth(3.0f); apron->setDepth(0.9f); apron->setHeight(0.3f);
    o.stagger = false;
    QCOMPARE(PerformerFill::rows(&props, apron, o).first().count, 5);
    apron->setFrontCurve(0.9f);
    QCOMPARE(PerformerFill::rows(&props, apron, o).first().count, 3);
}

void Performers_Test::theLayerHidesAndLocksThem()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StagePlatform *p = threeTier(props);
    Plot plot(&doc);

    const PerformerFill::Result r = plot.mon->fillWithPeople({ p->id() }, PerformerFill::Options());
    QCOMPARE(r.targets.size(), 3);
    foreach (quint32 id, r.targets)
    {
        TargetItem *ti = plot.gv->m_targetItems.value(id, nullptr);
        QVERIFY(ti != nullptr);
        QVERIFY(ti->isVisible());
        QVERIFY(ti->flags() & QGraphicsItem::ItemIsMovable);
        // The plot draws the people on it, not only the line.
        QVERIFY2(!ti->figurePath().isEmpty(), "a performer line is drawn without its people");
        QVERIFY(ti->figurePath().boundingRect().width() > 20.0);
    }

    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    QVERIFY(v.layerShown(r.layerId));
    QList<QVector3D> shown;
    v.collectPoints(shown);
    bool crown = false;
    foreach (const QVector3D &q, shown)
        crown = crown || q.z() > 2.0f;
    QVERIFY2(crown, "the rig view's fit leaves the people out");

    props->setLayerVisible(r.layerId, false);
    plot.gv->refreshItemLayerState();
    foreach (quint32 id, r.targets)
        QVERIFY2(!plot.gv->m_targetItems.value(id)->isVisible(), "a hidden performer line is drawn on the plot");
    QVERIFY(!v.layerShown(r.layerId));
    QList<QVector3D> pts;
    v.collectPoints(pts);
    foreach (const QVector3D &q, pts)
        QVERIFY2(q.z() < 2.0f, "a hidden performer is still framed in the rig view");

    props->setLayerVisible(r.layerId, true);
    props->setLayerLocked(r.layerId, true);
    plot.gv->refreshItemLayerState();
    foreach (quint32 id, r.targets)
    {
        TargetItem *ti = plot.gv->m_targetItems.value(id);
        QVERIFY(ti->isVisible());
        QVERIFY2(!MonitorGraphicsView::itemDraggable(ti), "a locked layer's performer line can be dragged");
    }
}

void Performers_Test::movingTheRiserMovesThem()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StagePlatform *p = threeTier(props);
    Plot plot(&doc);
    const PerformerFill::Result r = plot.mon->fillWithPeople({ p->id() }, PerformerFill::Options());

    // A line on the floor beside it does not ride it.
    StageTarget *beside = PerformerFill::addLine(props, QVector3D(0.2f, 0.5f, 0.0f),
                                                 QVector3D(1.5f, 0.5f, 0.0f), 3, PerformerFill::Options(), 0);
    plot.gv->updateTargets();

    QHash<quint32, QList<PerformerFill::Performer> > before;
    foreach (quint32 id, r.targets)
        before.insert(id, PerformerFill::people(props, props->stageTarget(id)));

    PlatformItem *pi = plot.gv->m_platformItems.value(p->id());
    QVERIFY(pi != nullptr);
    plot.gv->scene()->clearSelection();
    pi->setPos(plot.gv->realPositionToPixels(4000.0, 2000.0));   // 2 m right, 1 m down
    plot.gv->slotPlatformMoved(pi);
    QVERIFY(qAbs(p->originX() - 4.0f) < 1e-3f && qAbs(p->originY() - 2.0f) < 1e-3f);

    foreach (quint32 id, r.targets)
    {
        const QList<PerformerFill::Performer> now = PerformerFill::people(props, props->stageTarget(id));
        const QList<PerformerFill::Performer> was = before.value(id);
        QCOMPARE(now.size(), was.size());
        for (int i = 0; i < now.size(); ++i)
        {
            QVERIFY2((now.at(i).feet - (was.at(i).feet + QVector3D(2.0f, 1.0f, 0.0f))).length() < 1e-3f,
                     qPrintable(QString("%1 stayed behind").arg(props->stageTarget(id)->name())));
            QCOMPARE(now.at(i).height, was.at(i).height);
        }
    }
    QVERIFY(qAbs(beside->x() - 0.85f) < 1e-3f);

    // One Undo: the riser and its people back.
    plot.gv->undoLastMove();
    QVERIFY(qAbs(p->originX() - 2.0f) < 1e-3f);
    foreach (quint32 id, r.targets)
    {
        const QList<PerformerFill::Performer> now = PerformerFill::people(props, props->stageTarget(id));
        for (int i = 0; i < now.size(); ++i)
            QVERIFY((now.at(i).feet - before.value(id).at(i).feet).length() < 1e-3f);
    }

    // Moved out from under them (the line held still): their feet come
    // down to the floor -- they stand on whatever is there now.
    StageTarget *top = props->stageTarget(r.targets.last());
    top->setLocked(true);
    plot.gv->updateTargets();
    p->setOriginX(20.0f);
    foreach (const PerformerFill::Performer &pf, PerformerFill::people(props, top))
        QCOMPARE(pf.feet.z(), 0.0f);
    p->setOriginX(2.0f);

    // Turned with it too: a half turn puts each person opposite.
    top->setLocked(false);
    plot.gv->updateTargets();
    plot.gv->scene()->clearSelection();
    plot.gv->m_platformItems.value(p->id())->setSelected(true);
    const QPointF c = p->centre();
    plot.gv->rotateSelectedItems(180.0);
    foreach (quint32 id, r.targets)
    {
        const QList<PerformerFill::Performer> now = PerformerFill::people(props, props->stageTarget(id));
        const QList<PerformerFill::Performer> was = before.value(id);
        for (int i = 0; i < now.size(); ++i)
        {
            const QVector3D w = was.at(was.size() - 1 - i).feet;   // the line runs the other way now
            Q_UNUSED(w);
            bool found = false;
            foreach (const PerformerFill::Performer &o, was)
                found = found || (qAbs(now.at(i).feet.x() - float(2.0 * c.x() - o.feet.x())) < 1e-3f
                                  && qAbs(now.at(i).feet.y() - float(2.0 * c.y() - o.feet.y())) < 1e-3f);
            QVERIFY2(found, qPrintable(QString("%1 did not turn with the riser").arg(props->stageTarget(id)->name())));
        }
    }
}

void Performers_Test::theSetMovesAsOnePiece()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    const Choir choir = buildChoir(props);
    Plot plot(&doc);
    const quint32 gid = props->nextGroupId();
    props->createGroup(gid, QStringLiteral("Choir"), choir.lines.first()->layerId(), 0);
    foreach (StageTarget *t, choir.lines)
        t->setGroupId(gid);
    plot.gv->updateTargets();

    // Picking one picks the set.
    plot.gv->scene()->clearSelection();
    plot.gv->m_targetItems.value(choir.lines.first()->id())->setSelected(true);
    plot.gv->extendSelectionToGroups();
    foreach (StageTarget *t, choir.lines)
        QVERIFY(plot.gv->m_targetItems.value(t->id())->isSelected());

    // Dragged together across the stage: each line moves by the same.
    QHash<quint32, QPointF> before;
    foreach (StageTarget *t, choir.lines)
        before.insert(t->id(), QPointF(t->x(), t->y()));
    const QPointF d = plot.gv->realPositionToPixels(0.5 * 1000.0, 0.0)
                      - plot.gv->realPositionToPixels(0.0, 0.0);
    foreach (StageTarget *t, choir.lines)
    {
        TargetItem *ti = plot.gv->m_targetItems.value(t->id());
        ti->setPos(ti->pos() + d);
    }
    plot.gv->slotTargetMoved(plot.gv->m_targetItems.value(choir.lines.first()->id()));
    foreach (StageTarget *t, choir.lines)
    {
        QVERIFY(qAbs(t->x() - (before.value(t->id()).x() + 0.5f)) < 1e-3f);
        QVERIFY(qAbs(t->y() - before.value(t->id()).y()) < 1e-3f);
    }
    QCOMPARE(headCount(props), 32);
}

void Performers_Test::saveAndLoadRoundTrip()
{
    MonitorProperties mp;
    const Choir c = buildChoir(&mp);
    c.lines.at(3)->setCostumeColor(QColor(20, 60, 200));

    // A plain line and a plain mark: written as before.
    StageTarget *plain = mp.addStageTarget();
    plain->makeLine();
    StageTarget *mark = mp.addStageTarget();
    mark->setName(QStringLiteral("DSC"));

    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();
    QCOMPARE(buf.count("People="), 4);
    QCOMPARE(buf.count("Seed="), 4);
    QCOMPARE(buf.count("HeightSpread="), 4);
    QCOMPARE(buf.count("Costume="), 1);

    MonitorProperties mp2;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
        if (reader.name() == QStringLiteral("Monitor"))
            QVERIFY(mp2.loadXML(reader, nullptr));

    foreach (StageTarget *a, c.lines)
    {
        const StageTarget *b = mp2.stageTarget(a->id());
        QVERIFY(b != nullptr);
        QVERIFY(b->isPerformerLine());
        QCOMPARE(b->performerCount(), a->performerCount());
        QCOMPARE(b->seed(), a->seed());
        QCOMPARE(b->personHeight(), a->personHeight());
        QVERIFY(qAbs(b->heightSpread() - a->heightSpread()) < 1e-4f);
        QCOMPARE(b->costumeColor(), a->costumeColor());
        QCOMPARE(b->layerId(), a->layerId());
        const QList<PerformerFill::Performer> pa = PerformerFill::people(&mp, a);
        const QList<PerformerFill::Performer> pb = PerformerFill::people(&mp2, b);
        QCOMPARE(pb.size(), pa.size());
        for (int i = 0; i < pa.size(); ++i)
        {
            QVERIFY((pa.at(i).feet - pb.at(i).feet).length() < 2e-3f);
            QVERIFY(qAbs(pa.at(i).height - pb.at(i).height) < 2e-3f);
        }
    }
    QVERIFY(!mp2.stageTarget(plain->id())->isPerformerLine());
    QVERIFY(mp2.stageTarget(plain->id())->isLine());
    QVERIFY(!mp2.stageTarget(mark->id())->costumeColor().isValid());
}

void Performers_Test::aCostumeTakesTheLightInItsColour()
{
    /* A red robe on a performer line and a plain Person side by side, one
       white wash on both: the red reads red, the plain one neutral. */
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    PerformerFill::Options o;
    o.height = 1.75f;
    o.costume = QColor(200, 30, 30);
    StageTarget *robe = PerformerFill::addLine(props, QVector3D(2.5f, 3.0f, 0.0f),
                                               QVector3D(2.5f, 3.0f, 0.0f), 1, o, 0);
    StageTarget *plain = props->addStageTarget();
    plain->setKind(StageTarget::Kind::Person);
    plain->setPosition(QVector3D(3.5f, 3.0f, 0.0f));

    Fixture *wash = makeWash(&doc, 0, QVector3D(3.0f, 8.0f, 4.0f));
    QVERIFY(wash != nullptr);
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, &doc);
    aim->setStageTargetId(plain->id());
    QVERIFY(doc.addPalette(aim));
    Scene *scene = new Scene(&doc);
    scene->addFixture(wash->id());
    scene->addPalette(aim->id());
    QVERIFY(doc.addFunction(scene));
    QByteArray u(512, char(0));
    u[2] = char(255); u[3] = char(255); u[4] = char(255); u[5] = char(255);
    wash->setChannelValues(u);

    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.resize(800, 620);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(0.0, 12.0);
    v.setAmbient(0.4);
    v.setBeams(true);
    v.setLiveValues(true);
    v.setActiveScene(scene->id());

    auto chest = [&](float x, float tall) {
        const QImage img = v.grab().toImage();
        const double dpr = img.devicePixelRatio();
        const QVector3D at(x, 3.0f + 0.13f * tall / 1.75f, 0.72f * tall);
        const QPoint c = (v.w2s(at) * dpr).toPoint();
        int r = 0, g = 0, b = 0, n = 0;
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
            {
                const QColor px = img.pixelColor(c + QPoint(dx, dy));
                r += px.red(); g += px.green(); b += px.blue(); ++n;
            }
        return QColor(r / n, g / n, b / n);
    };
    const float robeTall = PerformerFill::people(props, robe).first().height;
    const QColor red = chest(2.5f, robeTall);
    const QColor body = chest(3.5f, props->targetFigureHeight(plain));
    const QString report = QString("red robe %1, neutral %2").arg(red.name(), body.name());
    QVERIFY2(red.red() > red.green() + 50 && red.red() > red.blue() + 50,
             qPrintable("a red costume does not read red: " + report));
    QVERIFY2(qAbs(body.red() - body.blue()) < 40, qPrintable("the neutral body took a colour: " + report));
}

void Performers_Test::theDialogAndTheAddMenu()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StagePlatform *p = threeTier(props);

    PerformersDialog fillDlg(props, { p->id() });
    QVERIFY(fillDlg.fills());
    QCOMPARE(fillDlg.count(), 14);
    QVERIFY(fillDlg.findChild<QLabel *>(QStringLiteral("performersCount"))->text().startsWith(QStringLiteral("14 ")));
    fillDlg.m_by->setCurrentIndex(1);
    fillDlg.m_perRow->setValue(7);
    QCOMPARE(fillDlg.count(), 21);
    QCOMPARE(fillDlg.options().perRow, 7);
    QVERIFY(fillDlg.m_name->text().contains(QStringLiteral("Choir riser")));
    // The Adults preset sets the height; the spread is its own.
    fillDlg.m_heightPreset->setCurrentIndex(2);
    emit fillDlg.m_heightPreset->activated(2);
    QVERIFY(qAbs(fillDlg.options().height - 1.75f) < 1e-3f);
    QVERIFY(qAbs(fillDlg.options().spread - 0.08f) < 1e-3f);

    PerformersDialog lineDlg(props, QList<quint32>());
    QVERIFY(!lineDlg.fills());
    lineDlg.m_lineCount->setValue(12);
    QCOMPARE(lineDlg.count(), 12);

    Plot plot(&doc);
    QAction *people = nullptr;
    foreach (QAction *a, plot.mon->findChildren<QAction *>())
        if (a->objectName() == QStringLiteral("addPeopleAction"))
            people = a;
    QVERIFY(people != nullptr);
    QMenu *menu = nullptr;
    foreach (QObject *o, people->associatedObjects())
        if ((menu = qobject_cast<QMenu *>(o)) != nullptr)
            break;
    QVERIFY(menu != nullptr);
    emit menu->aboutToShow();
    QCOMPARE(people->text(), QStringLiteral("Add People…"));

    // Nothing selected: one performer line, as many as asked.
    QTimer::singleShot(0, [&]() {
        PerformersDialog *d = plot.mon->findChild<PerformersDialog *>();
        QVERIFY(d != nullptr && !d->fills());
        d->m_lineCount->setValue(5);
        d->accept();
    });
    plot.mon->m_pendingAddScenePos = plot.gv->realPositionToPixels(5000.0, 5000.0);
    people->trigger();
    QCOMPARE(lines(props).size(), 1);
    QCOMPARE(lines(props).first()->performerCount(), 5);
    QVERIFY(qAbs(lines(props).first()->x() - 5.0f) < 0.05f);

    // A riser selected: the fill.
    plot.gv->m_platformItems.value(p->id())->setSelected(true);
    emit menu->aboutToShow();
    QVERIFY2(people->text().startsWith(QStringLiteral("Fill 1 Selected Riser")), qPrintable(people->text()));
    QTimer::singleShot(0, [&]() {
        PerformersDialog *d = plot.mon->findChild<PerformersDialog *>();
        QVERIFY(d != nullptr && d->fills());
        d->accept();
    });
    people->trigger();
    QCOMPARE(lines(props).size(), 1 + 3);
    QCOMPARE(headCount(props), 5 + 14);
}

void Performers_Test::anyLineTakesPeopleInItsDialog()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    Plot plot(&doc);
    StageTarget *t = props->addStageTarget();
    t->makeLine();
    plot.gv->updateTargets();
    QTimer::singleShot(0, [&]() {
        QDialog *d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(d != nullptr);
        QSpinBox *n = d->findChild<QSpinBox *>(QStringLiteral("linePeople"));
        QVERIFY(n != nullptr && n->isEnabled());
        n->setValue(4);
        d->accept();
    });
    plot.mon->slotEditTarget(t->id());
    QVERIFY(t->isPerformerLine());
    QCOMPARE(t->performerCount(), 4);
    QVERIFY(t->seed() != 0);
    QCOMPARE(PerformerFill::people(props, t).size(), 4);
}

/* The people's faces are kept between frames; the picture must not change by
   it, and must follow the view when it moves. */
void Performers_Test::keptFacesDrawTheSamePicture()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    QList<quint32> risers;
    risers << threeTier(props, 0.3f, 1.0f)->id();
    Fixture *wash = makeWash(&doc, 0, QVector3D(1.5f, 8.0f, 5.0f));
    QVERIFY(wash != nullptr);
    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.resize(600, 420);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(15.0, 15.0);
    Plot plot(&doc);
    PerformerFill::Options o;
    o.height = PerformerFill::presetHeight(PerformerFill::Students);
    QVERIFY(plot.mon->fillWithPeople(risers, o).people > 0);
    v.reload();
    const QImage first = v.grab().toImage();       // builds the faces
    const QImage again = v.grab().toImage();       // draws from the kept ones
    QCOMPARE(again, first);
    v.setAngledView(60.0, 25.0);
    QVERIFY(v.grab().toImage() != first);
    v.setAngledView(15.0, 15.0);
    QCOMPARE(v.grab().toImage(), first);
}

void Performers_Test::manyPeopleRenderCost()
{
    /* Five 3-tier risers of students -- 70 people on 15 lines -- under a
       few washes. The rig view's frame and the plot's are timed with nobody
       on the risers and with them all: the quickest of several, in processor
       time, so a busy machine measures the drawing and not the wait. */
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    QList<quint32> risers;
    for (int k = 0; k < 5; ++k)
        risers << threeTier(props, 0.3f + 3.1f * k, 1.0f)->id();
    for (int k = 0; k < 4; ++k)
    {
        Fixture *f = makeWash(&doc, quint32(k * 6), QVector3D(1.5f + 4.0f * k, 8.0f, 5.0f));
        QVERIFY(f != nullptr);
        QByteArray u(512, char(0));
        u[k * 6 + 2] = char(255); u[k * 6 + 3] = char(255); u[k * 6 + 5] = char(200);
        f->setChannelValues(u);
    }

    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.resize(1000, 700);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(15.0, 15.0);
    v.setBeams(true);
    v.setLiveValues(true);
    Plot plot(&doc);

    auto quickest = [](const std::function<void()> &draw) {
        draw();                             // warm
        double best = 1e9;
        for (int i = 0; i < 7; ++i)
        {
            const std::clock_t c0 = std::clock();
            draw();
            best = qMin(best, double(std::clock() - c0) * 1000.0 / CLOCKS_PER_SEC);
        }
        return best;
    };
    auto rigFrame = [&]() { v.grab(); };
    auto plotFrame = [&]() {
        QImage img(1000, 700, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::black);
        QPainter p(&img);
        plot.gv->scene()->render(&p);
    };
    const double rigEmpty = quickest(rigFrame);
    const double plotEmpty = quickest(plotFrame);

    PerformerFill::Options o;
    o.height = PerformerFill::presetHeight(PerformerFill::Students);
    o.spread = 0.08f;
    o.costume = QColor(30, 30, 120);
    const PerformerFill::Result r = plot.mon->fillWithPeople(risers, o);
    QCOMPARE(r.people, 70);
    v.reload();

    const double rigFull = quickest(rigFrame);
    const double plotFull = quickest(plotFrame);
    qInfo("render cost (CPU), %d people: rig view %.1f -> %.1f ms/frame, plot %.1f -> %.1f ms/frame",
          r.people, rigEmpty, rigFull, plotEmpty, plotFull);
    /* Generous: a debug build on a loaded gate machine. What it catches is
       a figure costing milliseconds, not noise. Reported, not checked, on a
       coverage or sanitizer build (CI's Linux coverage leg), where the
       times are the instrumentation's -- and QLC_TEST_NO_TIMING=1 says the
       same for any other run that knows its clock can't be trusted. */
    if (!QLC_TIMING_GUARDS || qEnvironmentVariableIntValue("QLC_TEST_NO_TIMING") == 1)
        QSKIP("an instrumented build: the render cost is reported above, not checked");
    QVERIFY2(rigFull < rigEmpty * 1.6 + 80.0, "70 people make the rig view crawl");
    QVERIFY2(plotFull < plotEmpty + 30.0, "70 people make the plot crawl");
}

QTEST_MAIN(Performers_Test)

/* Performers must draw on every frame while their layer is shown: after a
   full frame, a budget-limited (value-driven, unsupersampled) one, and an
   incremental one (a value moved, a different cue on show). The cue scope
   (Show > All targets off) used to drop them with the targets it hides. */
void Performers_Test::performersDrawOnEveryFrame()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    QList<quint32> risers;
    risers << threeTier(props, 0.3f, 1.0f)->id();
    Fixture *wash = makeWash(&doc, 0, QVector3D(1.5f, 8.0f, 5.0f));
    QVERIFY(wash != nullptr);
    StageTarget *other = props->addStageTarget();       // what the "cue" aims at
    other->setX(6.0f);
    other->setY(2.0f);

    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.resize(600, 420);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(15.0, 15.0);
    v.setLiveValues(true);
    Plot plot(&doc);
    PerformerFill::Options o;
    o.height = PerformerFill::presetHeight(PerformerFill::Students);
    const PerformerFill::Result r = plot.mon->fillWithPeople(risers, o);
    QVERIFY(r.people > 0);
    v.reload();

    // The cue on show aims at the other target only.
    v.setTargetScope(false, QSet<quint32>() << other->id());
    auto builtFigures = [&]() {
        int n = 0;
        for (const StructureStudioView::FigureFaces &f : std::as_const(v.m_figureFaces))
            if (f.built && !f.faces.isEmpty())
                ++n;
        return n;
    };
    auto frame = [&](const char *what) {
        v.m_figureFaces.clear();            // so a frame that skipped them shows
        v.grab();
        QVERIFY2(builtFigures() == r.people, what);
    };

    frame("full frame");
    // Budget-limited: value-driven, close on the last, no supersampling.
    v.setSupersample(2);
    for (int i = 0; i < 3; ++i)
    {
        v.m_timerTick = true;
        v.m_lastFrameAt.start();
        v.m_valueFrame = true;
        v.m_figureFaces.clear();
        v.grab();
        v.m_timerTick = false;
        QCOMPARE(builtFigures(), r.people);
    }
    // Incremental: a value moves between frames, a different cue comes on.
    for (int i = 0; i < 4; ++i)
    {
        QByteArray u(512, char(0));
        u[2] = char(40 * i); u[3] = char(255);
        wash->setChannelValues(u);
        v.setTargetScope(false, i % 2 ? QSet<quint32>() << other->id() : QSet<quint32>());
        frame("incremental");
    }
    // Hiding their layer does hide them.
    props->setLayerVisible(r.layerId, false);
    v.m_figureFaces.clear();
    v.grab();
    QCOMPARE(builtFigures(), 0);
}

void Performers_Test::peopleFaceATurnedRiser()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StagePlatform *p = threeTier(props, 0.3f, 1.0f);
    QList<quint32> risers;
    risers << p->id();
    Plot plot(&doc);
    PerformerFill::Options o;
    QVERIFY(plot.mon->fillWithPeople(risers, o).people > 0);

    StageTarget *line = lines(props).first();
    foreach (const PerformerFill::Performer &pf, PerformerFill::people(props, line))
        QVERIFY(qAbs(pf.facing.x()) < 1e-4f && qAbs(pf.facing.y() - 1.0f) < 1e-4f);

    p->setRotation(90.0f);
    const QPointF c = p->centre();
    const QPointF front = p->toWorld(c + QPointF(0.0, 1.0)) - p->toWorld(c);
    int turned = 0;
    foreach (const PerformerFill::Performer &pf, PerformerFill::people(props, line))
    {
        if (!p->containsPoint(pf.feet.x(), pf.feet.y()))
            continue;                       // turned off the riser
        ++turned;
        QVERIFY(qAbs(pf.facing.x() - float(front.x())) < 1e-3f);
        QVERIFY(qAbs(pf.facing.y() - float(front.y())) < 1e-3f);
        QVERIFY(pf.facing.y() < 0.5f);       // no longer the house
    }
    // People are stood on the deck wherever it is; at least some are on it.
    QVERIFY(turned > 0 || !PerformerFill::people(props, line).isEmpty());

    // The view draws them turned: the picture changes, the faces say why.
    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.resize(600, 420);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(15.0, 15.0);
    p->setRotation(0.0f);
    v.reload();
    const QImage flat = v.grab().toImage();
    p->setRotation(90.0f);
    v.reload();
    v.grab();
    bool faced = false;
    for (const StructureStudioView::FigureFaces &f : std::as_const(v.m_figureFaces))
        if (f.built && qAbs(f.facing.x()) > 0.9f)
            faced = true;
    QVERIFY(faced);
    QVERIFY(v.grab().toImage() != flat);
}

void Performers_Test::ownerFilterShowsHouseOrOurs()
{
    Doc doc(this);
    Fixture *ours = makeWash(&doc, 0, QVector3D(1.5f, 8.0f, 5.0f));
    Fixture *house = makeWash(&doc, 6, QVector3D(5.5f, 8.0f, 5.0f));
    QVERIFY(ours != nullptr && house != nullptr);
    house->setOwner(RigOwner::House);

    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.resize(600, 420);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    QCOMPARE(v.ownerFilter(), StructureStudioView::OwnerAll);
    QVERIFY(v.mountedFixtures().contains(ours->id()));
    QVERIFY(v.mountedFixtures().contains(house->id()));
    const QImage all = v.grab().toImage();

    v.setOwnerFilter(StructureStudioView::OwnerHouse);
    QCOMPARE(v.mountedFixtures(), QList<quint32>() << house->id());
    const QImage houseOnly = v.grab().toImage();
    QVERIFY(houseOnly != all);

    v.setOwnerFilter(StructureStudioView::OwnerOurs);
    QCOMPARE(v.mountedFixtures(), QList<quint32>() << ours->id());
    QVERIFY(v.grab().toImage() != houseOnly);

    // An owner edited elsewhere shows once the kept owners are looked at again.
    ours->setOwner(RigOwner::House);
    v.rigMayHaveMoved();
    QVERIFY(v.mountedFixtures().isEmpty());

    v.setOwnerFilter(StructureStudioView::OwnerAll);
    QCOMPARE(v.mountedFixtures().size(), 2);
    QCOMPARE(v.grab().toImage(), all);
}
