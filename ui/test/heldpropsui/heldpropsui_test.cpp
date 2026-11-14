/*
  Q Light Controller Plus - Test Unit
  heldpropsui_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QtTest>
#include <QApplication>
#include <QPainter>
#include <QMenu>
#include <QDir>

#include "../../../engine/test/common/stdprelude.h"
#define protected public
#define private public
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "structurestudioview.h"
#include "targetitem.h"
#undef private
#undef protected

#include "heldpropsui_test.h"
#include "monitorproperties.h"
#include "qlcfixturedef.h"
#include "contextmenus.h"
#include "stageplatform.h"
#include "candleremote.h"
#include "blockingview.h"
#include "stagetarget.h"
#include "handprops.h"
#include "heldprops.h"
#include "fixture.h"
#include "doc.h"

namespace
{

StageTarget *add(MonitorProperties *props, StageTarget::Kind k, float x, float y, const QString &name)
{
    StageTarget *t = props->addStageTarget();
    t->setName(name);
    t->setKind(k);
    t->setPosition(QVector3D(x, y, 0.0f));
    return t;
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
        gv->updateTargets();
    }
    ~Plot() { delete mon; }
};

QAction *findAction(QMenu *menu, const QString &name)
{
    for (QAction *a : menu->actions())
    {
        if (a->objectName() == name)
            return a;
        if (a->menu() != nullptr)
            if (QAction *in = findAction(a->menu(), name))
                return in;
    }
    return nullptr;
}

QMenu *targetMenu(MonitorGraphicsView *gv, quint32 id, QMenu *menu)
{
    ContextMenu cm(menu);
    gv->addHandPropItems(cm, id);
    cm.finish();
    return menu;
}

int flamePixels(const QImage &img)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
        {
            const QColor c = img.pixelColor(x, y);
            if (c.alpha() > 40 && c.red() > 200 && c.green() > 120 && c.green() < 215 && c.blue() < 130)
                ++n;
        }
    return n;
}

} // namespace

void HeldPropsUi_Test::giveEachIsOneUndoStep()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    QList<StageTarget *> people;
    for (int i = 0; i < 3; ++i)
        people << add(props, StageTarget::Kind::Person, 2.0f + 2.0f * i, 3.0f, QString("Choir %1").arg(i + 1));
    Plot plot(&doc);
    const int before = props->stageTargets().size();
    for (StageTarget *p : people)
        plot.gv->m_targetItems.value(p->id())->setSelected(true);
    plot.gv->m_menuSelection = plot.gv->selectedMapObjects();     // as the right-press keeps it
    QCOMPARE(plot.gv->m_menuSelection.size(), 3);

    QMenu menu;
    targetMenu(plot.gv, people.first()->id(), &menu);
    QVERIFY(ContextMenu::checkCanon(&menu).isEmpty());
    QAction *candle = findAction(&menu, "givecandle");
    QVERIFY(candle != nullptr);
    QVERIFY(findAction(&menu, "heldByNobody") == nullptr);       // people are not held
    const int undoBefore = plot.gv->m_moveUndo.size();
    candle->trigger();

    QCOMPARE(props->stageTargets().size(), before + 3);
    QSet<quint32> holders;
    for (StageTarget *t : props->stageTargets())
        if (t->kind() == StageTarget::Kind::Candle)
        {
            const StageTarget *h = props->targetHolder(t, 0);
            QVERIFY(h != nullptr);
            holders.insert(h->id());
            QVERIFY(plot.gv->m_targetItems.contains(t->id()));
        }
    QCOMPARE(holders.size(), 3);                                  // one each
    QCOMPARE(plot.gv->m_moveUndo.size(), undoBefore + 1);         // one step

    plot.gv->undoLastMove();
    QCOMPARE(props->stageTargets().size(), before);
    for (StageTarget *t : props->stageTargets())
        QVERIFY(t->kind() != StageTarget::Kind::Candle);
}

void HeldPropsUi_Test::heldByAndHandOver()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StageTarget *ann = add(props, StageTarget::Kind::Person, 2.0f, 3.0f, "Ann");
    StageTarget *bob = add(props, StageTarget::Kind::Person, 6.0f, 3.0f, "Bob");
    StageTarget *staff = add(props, StageTarget::Kind::Staff, 8.0f, 1.0f, "Staff");
    const quint32 scene2 = props->addBlockingSet("Scene 2");
    BlockingView::of(&doc)->setPick(scene2);
    Plot plot(&doc);
    QCOMPARE(BlockingView::of(&doc)->blocking(), scene2);

    // Held By ▸ Ann, in the blocking shown: hers there, not in Default.
    {
        QMenu menu;
        targetMenu(plot.gv, staff->id(), &menu);
        QVERIFY(ContextMenu::checkCanon(&menu).isEmpty());
        QAction *nobody = findAction(&menu, "heldByNobody");
        QVERIFY(nobody != nullptr && nobody->isChecked());
        QAction *toAnn = findAction(&menu, QString("heldBy%1").arg(ann->id()));
        QVERIFY(toAnn != nullptr);
        toAnn->trigger();
    }
    QCOMPARE(props->targetHolder(staff, scene2), ann);
    QVERIFY(props->targetHolder(staff, 0) == nullptr);
    TargetItem *ti = plot.gv->m_targetItems.value(staff->id());
    QVERIFY(ti != nullptr);
    const QVector3D hand = props->handPoint(ann, scene2);
    const QPointF at = plot.gv->projectMm(hand.x() * 1000.0, hand.y() * 1000.0, hand.z() * 1000.0);
    QVERIFY2(QLineF(ti->pos(), at).length() < 1.0, "drawn in her hand");

    // Dropped on Bob: his.
    const QVector3D bobAt = props->targetPosition(bob, scene2);
    ti->setPos(plot.gv->projectMm(bobAt.x() * 1000.0 + 150.0, bobAt.y() * 1000.0, 0.0));
    QVERIFY(plot.gv->dropHandProp(ti, { ti }));
    QCOMPARE(props->targetHolder(staff, scene2), bob);
    QCOMPARE(plot.gv->m_moveUndo.last().type, MonitorGraphicsView::UndoEntry::RigEdit);

    // Dropped on the bare deck: put down there, in this blocking only.
    ti = plot.gv->m_targetItems.value(staff->id());
    ti->setPos(plot.gv->projectMm(4000.0, 1000.0, 0.0));
    QVERIFY(plot.gv->dropHandProp(ti, { ti }));
    QVERIFY(props->targetHolder(staff, scene2) == nullptr);
    QCOMPARE(staff->holdIn(scene2), StageTarget::Hold::OwnPlace);
    const QVector3D put = props->targetPosition(staff, scene2);
    QVERIFY2(qAbs(put.x() - 4.0f) < 0.05f && qAbs(put.y() - 1.0f) < 0.05f, "put down where dropped");
    QVERIFY(qAbs(staff->position().x() - 8.0f) < 1e-4f);         // Default's place untouched

    // A person is not a lone prop: the usual drop runs.
    TargetItem *annItem = plot.gv->m_targetItems.value(ann->id());
    QVERIFY(!plot.gv->dropHandProp(annItem, { annItem }));
    // Undo walks it back: on the deck -> Bob's -> Ann's.
    plot.gv->undoLastMove();
    QCOMPARE(props->targetHolder(props->stageTarget(staff->id()), scene2)->name(), QString("Bob"));
    BlockingView::of(&doc)->setPick(BlockingView::followCue());
}

void HeldPropsUi_Test::drawnInTheHandOrNotAtAll()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StageTarget *ann = add(props, StageTarget::Kind::Person, 3.0f, 3.0f, "Ann");
    StageTarget *staff = add(props, StageTarget::Kind::Staff, 8.0f, 1.0f, "Staff");
    props->setTargetHold(staff, 0, int(StageTarget::Hold::HeldBy), ann->id());
    const quint32 gone = props->addBlockingSet("No staff");
    props->setTargetHold(staff, gone, int(StageTarget::Hold::OffStage));
    BlockingView::of(&doc)->setPick(0);
    Plot plot(&doc);
    TargetItem *ti = plot.gv->m_targetItems.value(staff->id());
    QVERIFY(ti != nullptr && ti->isVisible());
    QVERIFY(ti->positionReadout().isEmpty());                    // no readout for a prop in a hand
    QVERIFY(ti->toolTip().contains("Ann"));

    // She is moved: it follows (repositioned, not rebuilt).
    const QPointF was = ti->pos();
    ann->setPosition(QVector3D(5.0f, 3.0f, 0.0f));
    plot.gv->repositionTargetItems();
    QVERIFY(ti->pos().x() > was.x() + 100.0);

    // From the front: a pole from the deck up, as long as the staff.
    plot.gv->setViewPOV(MonitorGraphicsView::PovFront);
    plot.gv->updateTargets();
    ti = plot.gv->m_targetItems.value(staff->id());
    const double ppm = 1000.0 * plot.gv->m_cellPixels / plot.gv->m_unitValue;
    QVERIFY2(qAbs((ti->m_feet.y() - ti->m_crown.y()) - 1.8 * ppm) < 2.0,
             qPrintable(QString("%1 px for %2").arg(ti->m_feet.y() - ti->m_crown.y()).arg(1.8 * ppm)));
    QVERIFY(ti->boundingRect().contains(ti->m_crown));
    plot.gv->setViewPOV(MonitorGraphicsView::PovTop);

    // In the blocking without it: not drawn.
    BlockingView::of(&doc)->setPick(gone);
    plot.gv->updateTargets();
    QVERIFY(!plot.gv->m_targetItems.value(staff->id())->isVisible());
    QVERIFY(plot.gv->m_targetItems.value(ann->id())->isVisible());
    BlockingView::of(&doc)->setPick(BlockingView::followCue());
}

void HeldPropsUi_Test::candleStatesDrawDifferently()
{
    using F = HandProps::Flame;
    auto draw = [](F f, bool fromAbove) {
        QImage img(80, 80, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.translate(40, 55);
        HandProps::paint(&p, StageTarget::Kind::Candle, f, QPointF(0, 0),
                         fromAbove ? QPointF(0, 0) : QPointF(0, -15), 100.0, QColor(255, 196, 110), false, 1234);
        return img;
    };
    for (bool above : { false, true })
    {
        const QImage unknown = draw(F::Unknown, above), dark = draw(F::Dark, above), full = draw(F::Full, above),
                     dim = draw(F::Dimmed, above), flick = draw(F::Flicker, above);
        QCOMPARE(flamePixels(unknown), 0);                          // drawn unlit
        QCOMPARE(flamePixels(dark), 0);
        QVERIFY(unknown != dark);                                   // dashed: not known
        QVERIFY(flamePixels(full) > 0);
        QVERIFY2(flamePixels(dim) < flamePixels(full),              // smaller, not only paler
                 qPrintable(QString("%1 vs %2").arg(flamePixels(dim)).arg(flamePixels(full))));
        QVERIFY(flick != full);
    }
    // The words for each, for the tooltip.
    QVERIFY(HandProps::flameText(F::Flicker).contains("flicker"));
    CandleRemote::State s;
    QCOMPARE(HandProps::flameFor(s), F::Unknown);
    s = CandleRemote::apply(s, CandleRemote::On);
    QCOMPARE(HandProps::flameFor(s), F::Full);
    s = CandleRemote::apply(s, CandleRemote::Dim);
    QCOMPARE(HandProps::flameFor(s), F::Dimmed);
    s = CandleRemote::apply(s, CandleRemote::Candle);
    QCOMPARE(HandProps::flameFor(s), F::FlickerDimmed);
    s = CandleRemote::apply(s, CandleRemote::Brighten);
    QCOMPARE(HandProps::flameFor(s), F::Flicker);
    s = CandleRemote::apply(s, CandleRemote::Off);
    QCOMPARE(HandProps::flameFor(s), F::Dark);

    // A staff draws a pole, not a flame.
    QImage pole(80, 80, QImage::Format_ARGB32_Premultiplied);
    pole.fill(Qt::transparent);
    {
        QPainter p(&pole);
        p.translate(40, 75);
        HandProps::paint(&p, StageTarget::Kind::Staff, F::Full, QPointF(0, 0), QPointF(0, -60), 100.0,
                         QColor(176, 120, 64), false, 0);
    }
    QCOMPARE(pole.pixelColor(40, 40).alpha(), 255);
    QCOMPARE(pole.pixelColor(10, 40).alpha(), 0);
}

void HeldPropsUi_Test::drawnInTheRigView()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(0.0f); deck->setOriginY(0.0f);
    deck->setWidth(10.0f); deck->setDepth(5.0f); deck->setHeight(0.4f);
    StageTarget *staff = add(props, StageTarget::Kind::Staff, 5.0f, 2.5f, "Staff");
    const quint32 gone = props->addBlockingSet("No staff");
    props->setTargetHold(staff, gone, int(StageTarget::Hold::OffStage));
    BlockingView::of(&doc)->setPick(gone);
    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.resize(700, 450);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    const QImage without = v.grab().toImage();
    BlockingView::of(&doc)->setPick(0);
    v.reload();
    const QImage with = v.grab().toImage();
    QVERIFY(with != without);
    BlockingView::of(&doc)->setPick(BlockingView::followCue());
}

/* The pictures for review: five people, Ann with a staff, three with
   candles -- lit, flickering, dimmed (three remotes, so all three show at
   once) -- in cue "Candles", and cue "One goes out" where the third has
   lost hers. QLC_HELDPROPS_TAG=before draws the props as the build before
   them saw them: plain marks at their own places. */
void HeldPropsUi_Test::shots()
{
    const QString dir = qEnvironmentVariable("QLC_HELDPROPS_SHOTS");
    if (dir.isEmpty())
        QSKIP("set QLC_HELDPROPS_SHOTS to a folder to write the pictures");
    QDir().mkpath(dir);
    const QString tag = qEnvironmentVariable("QLC_HELDPROPS_TAG", "after");
    const bool before = tag == QLatin1String("before");

    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(0.0f); deck->setOriginY(0.0f);
    deck->setWidth(10.0f); deck->setDepth(5.0f); deck->setHeight(0.4f);
    props->setGridSize(QVector3D(11.0f, 6.0f, 5.0f));
    const char *names[] = { "Ann", "Bea", "Cal", "Dee", "Eli" };
    QList<StageTarget *> people;
    for (int i = 0; i < 5; ++i)
        people << add(props, StageTarget::Kind::Person, 1.5f + 1.8f * i, 3.0f, names[i]);
    const quint32 candles = props->addBlockingSet("Candles");
    const quint32 oneOut = props->addBlockingSet("One goes out");
    for (StageTarget *p : people)            // the line steps down for the candles
    {
        StageTarget::Blocked b;
        b.hasOffset = true;
        b.offset = QVector3D(0.0f, 0.8f, 0.0f);
        p->setBlocked(candles, b);
        p->setBlocked(oneOut, b);
    }

    QLCFixtureDef *def = new QLCFixtureDef();
    QVERIFY(def->loadXML(QStringLiteral(QLC_TEST_RESOURCE_ROOT "fixtures/Generic/Generic-IR-Candle-Remote.qxf"))
            == QFile::NoError);
    QVERIFY(doc.fixtureDefCache()->addFixtureDef(def));
    QList<quint32> remotes;
    for (int i = 0; i < 3; ++i)
    {
        Fixture *r = new Fixture(&doc);
        r->setName(QString("Candles %1").arg(i + 1));
        r->setFixtureDefinition(def, def->modes().first());
        r->setUniverse(0);
        r->setAddress(500 + 2 * i);
        QVERIFY(doc.addFixture(r));
        remotes << r->id();
    }

    StageTarget *staff = add(props, StageTarget::Kind::Staff, 9.0f, 0.6f, "Staff");
    props->setTargetHold(staff, 0, int(StageTarget::Hold::HeldBy), people[0]->id());
    QList<quint32> made;
    for (int i = 0; i < 3; ++i)
        made << HeldProps::giveEach(props, { people[i + 1]->id() }, StageTarget::Kind::Candle, candles);
    props->setTargetHold(props->stageTarget(made[0]), oneOut, int(StageTarget::Hold::HeldBy), people[1]->id());
    props->setTargetHold(props->stageTarget(made[1]), oneOut, int(StageTarget::Hold::HeldBy), people[2]->id());
    props->setTargetHold(props->stageTarget(made[2]), oneOut, int(StageTarget::Hold::OffStage));
    CandleRemote *cr = CandleRemote::of(&doc);
    const int presses[3][2] = { { 1, 4 }, { 1, 3 }, { 1, 5 } };     // lit; flicker; dimmed
    for (int i = 0; i < 3; ++i)
    {
        props->stageTarget(made[i])->setLitBy(remotes[i]);
        cr->feed(remotes[i], presses[i][0], 30);
        cr->feed(remotes[i], presses[i][1], 30);
    }
    if (before)
        for (StageTarget *t : props->stageTargets())
            if (StageTarget::isHandPropKind(t->kind()))
            {
                for (quint32 set : t->blockedSets())
                    t->clearBlocked(set);
                t->setKind(StageTarget::Kind::None);
            }

    const QSize sizes[] = { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) };
    const char *sn[] = { "1024x640", "1280x800", "1920x1080" };
    const struct { quint32 set; const char *name; } cues[] = { { candles, "cue-candles" }, { oneOut, "cue-one-out" } };
    for (int i = 0; i < 3; ++i)
        for (const auto &cue : cues)
        {
            BlockingView::of(&doc)->setPick(cue.set);
            {
                Monitor mon(nullptr, &doc);
                mon.resize(sizes[i]);
                mon.show();
                QTest::qWait(150);
                MonitorGraphicsView *gv = mon.findChild<MonitorGraphicsView *>();
                gv->updateTargets();
                QTest::qWait(100);
                QVERIFY(mon.grab().save(QStringLiteral("%1/%2-plot-%3-%4.png").arg(dir, tag, cue.name, sn[i])));
            }
            StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
            v.resize(sizes[i]);
            v.show();
            v.reload();
            v.setPlane(StructureStudioView::Angled);
            QTest::qWait(150);
            QVERIFY(v.grab().save(QStringLiteral("%1/%2-45deg-%3-%4.png").arg(dir, tag, cue.name, sn[i])));
        }
    BlockingView::of(&doc)->setPick(BlockingView::followCue());
}

QTEST_MAIN(HeldPropsUi_Test)
