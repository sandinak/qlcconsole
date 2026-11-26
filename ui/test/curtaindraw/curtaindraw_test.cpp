/*
  Q Light Controller Plus
  curtaindraw_test.cpp

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
#include <QElapsedTimer>
#include <QVector3D>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QDir>
#include <QFile>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "sceneryitem.h"
#include "structurestudioview.h"
#undef protected
#undef private
#include "doc.h"
#include "chaser.h"
#include "chaseraction.h"
#include "scene.h"
#include "scenery.h"
#include "fixture.h"
#include "universe.h"
#include "mastertimer.h"
#include "inputoutputmap.h"
#include "monitorproperties.h"
#include "functionparent.h"
#include "qlcfixturedefcache.h"
#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "showcontrol.h"
#include "outputrulekeeper.h"
#include "curtaindraw_test.h"

using namespace ShowTree;

namespace
{

void settle(int ms)
{
    QElapsedTimer t;
    t.start();
    do
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    while (t.elapsed() < ms);
}

}

void CurtainDraw_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
    m_app->showNormal();
    m_app->resize(1280, 800);
    QVERIFY(m_app->doc()->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures")));
    QVERIFY(m_app->doc()->fixtureDefCache()->fixtureDef("Generic", "Stage Curtain") != nullptr);
}

void CurtainDraw_Test::cleanupTestCase()
{
    stopAll();
    m_app->doc()->resetModified();
    delete m_app;
    m_app = nullptr;
}

double CurtainDraw_Test::drawnOpen(quint32 sceneryId, quint32 fixtureId, const char *what)
{
    Monitor *mon = Monitor::instance();
    MonitorGraphicsView *gv = mon != nullptr ? mon->m_graphicsView : nullptr;
    SceneryItem *item = gv != nullptr ? gv->m_sceneryItems.value(sceneryId, nullptr) : nullptr;
    Doc *doc = m_app->doc();
    Fixture *fx = doc->fixture(fixtureId);
    int out = -1;
    if (fx != nullptr)
    {
        QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
        if (int(fx->universe()) < ua.size())
        {
            const QByteArray *v = ua.at(int(fx->universe()))->postGMValues();
            if (v != nullptr && int(fx->address()) < v->size())
                out = uchar(v->at(int(fx->address())));
        }
        doc->inputOutputMap()->releaseUniverses(false);
    }
    const double drawn = (item != nullptr && item->hasPose()) ? item->pose().open : -1.0;
    qInfo().noquote() << what << "on the wire" << out
                      << "fixture" << (fx != nullptr ? fx->channelValues().toHex() : QByteArray())
                      << "plot draws open" << drawn;
    return drawn;
}

void CurtainDraw_Test::loadAndGo(Chaser *c, const QString &number)
{
    ShowControl::Request load;
    load.command = ShowControl::Load;
    load.chaser = c->id();
    load.cue = number;
    m_app->doc()->showControl()->run(load, ShowControl::RouteRunTab);
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = c->id();
    m_app->doc()->showControl()->run(go, ShowControl::RouteRunTab);
}

void CurtainDraw_Test::jump(Chaser *c, int step)
{
    ChaserAction action;
    action.m_action = ChaserSetStepIndex;
    action.m_stepIndex = step;
    action.m_masterIntensity = 1.0;
    action.m_stepIntensity = 1.0;
    action.m_fadeMode = Chaser::FromFunction;
    c->setAction(action);
    if (!c->isRunning())
        c->start(m_app->doc()->masterTimer(), FunctionParent::master());
}

void CurtainDraw_Test::stopAll()
{
    if (m_app == nullptr)
        return;
    m_app->doc()->showTree()->stopPreview();
    m_app->doc()->masterTimer()->stopAllFunctions();
    settle(200);
}

/* What the wire has for fixture @p fx's channel @p ch now (-1: none). */
int CurtainDraw_Test::onWire(quint32 fixtureId, quint32 ch)
{
    Doc *doc = m_app->doc();
    Fixture *fx = doc->fixture(fixtureId);
    int out = -1;
    if (fx == nullptr)
        return out;
    QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
    if (int(fx->universe()) < ua.size())
    {
        const QByteArray *v = ua.at(int(fx->universe()))->postGMValues();
        if (v != nullptr && int(fx->address() + ch) < v->size())
            out = uchar(v->at(int(fx->address() + ch)));
    }
    doc->inputOutputMap()->releaseUniverses(false);
    return out;
}

bool CurtainDraw_Test::markShown(quint32 sceneryId)
{
    Monitor *mon = Monitor::instance();
    SceneryItem *item = mon != nullptr && mon->m_graphicsView != nullptr
                      ? mon->m_graphicsView->m_sceneryItems.value(sceneryId, nullptr) : nullptr;
    return item != nullptr && item->heldMark();
}

/* Board Q350, his report ("clicking between cues in the Show Tree, in
   Design: the curtain stays closed on screen; clicked again while it opens,
   it stops"): in Design a cue picked while editing holds the curtain where
   it physically is -- nothing on the wire moves it, the preview's own
   scene neither -- while the plot (and the Rig Overview) draw it where the
   picked cue's tracked state puts it, marked "held while editing". A GO
   moves it for real; Operate is the run's. His structure: the curtain
   fixture on a universe with no output, driving the Main drape, its scene
   in every cue but the first. */
void CurtainDraw_Test::aPickedCueDrawsTheCurtainAndHoldsTheStage()
{
    Doc *doc = m_app->doc();
    m_app->clearDocument();
    doc->setMode(Doc::Design);
    while (doc->inputOutputMap()->universesCount() < 4)
        QVERIFY(doc->inputOutputMap()->addUniverse());
    QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef("Generic", "Stage Curtain");
    QVERIFY(def != nullptr && def->mode("Open + Fly") != nullptr);
    Fixture *curtain = new Fixture(doc);
    curtain->setName("Main drape curtain");
    curtain->setFixtureDefinition(def, def->mode("Open + Fly"));
    curtain->setUniverse(3);                  // "U04-Control": no output patched
    curtain->setAddress(0);
    QVERIFY(doc->addFixture(curtain));
    Fixture *dimmer = new Fixture(doc);
    dimmer->setName("Dimmer");
    dimmer->setChannels(1);
    dimmer->setUniverse(0);
    dimmer->setAddress(0);
    QVERIFY(doc->addFixture(dimmer));

    // A 12 x 10 m stage, the drape across it (in the shots).
    doc->monitorProperties()->setGridUnits(MonitorProperties::Meters);
    doc->monitorProperties()->setGridSize(QVector3D(12.0f, 6.0f, 10.0f));
    Scenery *drape = doc->monitorProperties()->addScenery();
    drape->setKind(Scenery::MainDrape);
    drape->setOriginX(6.0f); drape->setOriginY(6.0f);
    drape->setWidth(10.0f);
    drape->setState(Scenery::Open);
    drape->setDriverFixture(curtain->id());

    Scene *open = new Scene(doc);
    open->setName("Curtain Open");
    open->setValue(curtain->id(), 0, 255);
    open->setValue(curtain->id(), 1, 0);
    open->setFadeInSpeed(2000);
    QVERIFY(doc->addFunction(open));
    Scene *half = new Scene(doc);
    half->setName("Half");
    half->setValue(dimmer->id(), 0, 128);
    QVERIFY(doc->addFunction(half));
    Scene *full = new Scene(doc);
    full->setName("Full");
    full->setValue(dimmer->id(), 0, 255);
    QVERIFY(doc->addFunction(full));

    ShowTreeController *st = doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("Show"), "Show list");
    auto layer = [&](quint32 cue, quint32 function) {
        Target t;
        t.function = function;
        return st->addLayer(cue, t) != ShowTree::invalidId();
    };
    const quint32 preset = st->addCue(listId, -1, "Preset");
    QVERIFY(layer(preset, half->id()));
    const quint32 c1 = st->addCue(listId, 0, "Curtain");
    QVERIFY(layer(c1, open->id()) && layer(c1, half->id()));
    const quint32 c2 = st->addCue(listId, 1, "Song");
    QVERIFY(layer(c2, open->id()) && layer(c2, full->id()));
    {
        // His curtain cue: a Mix, fading in over 2 s.
        Cue timed = *st->cue(c1);
        timed.fadeIn = 2000;
        QVERIFY(st->setCue(timed));
    }
    const CueList *list = st->cueList(listId);
    QVERIFY(list != nullptr);
    Chaser *c = qobject_cast<Chaser *>(doc->function(list->chaser));
    QVERIFY(c != nullptr);
    doc->showControl()->setMainCueList(c->id());
    const QString n1 = st->cueNumber(*list, 1);

    // The Lighting Studio's plot, as he has it up.
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr);
    m_app->m_tab->setCurrentIndex(m_app->m_tab->indexOf(mon));
    mon->updateView();
    settle(300);
    QVERIFY(mon->m_graphicsView != nullptr);
    const quint32 sid = drape->id(), fid = curtain->id();
    QVERIFY(mon->m_graphicsView->m_sceneryItems.contains(sid));
    OutputRuleKeeper *keeper = doc->outputRuleKeeper();
    QVERIFY(keeper != nullptr);
    QCOMPARE(drawnOpen(sid, fid, "loaded:"), 0.0);    // nothing plays on it: closed
    QVERIFY(!markShown(sid));

    // The cue that opens it picked: drawn open, marked; the stage stays shut.
    st->pickCue(c1);
    QTRY_COMPARE_WITH_TIMEOUT(drawnOpen(sid, fid, "1 picked:"), 1.0, 2000);
    QVERIFY(markShown(sid));
    settle(2500);                              // longer than its fade
    QCOMPARE(onWire(fid, 0), 0);
    QCOMPARE(drawnOpen(sid, fid, "1 picked, its fade over:"), 1.0);
    // The next one (the curtain tracks on): the same.
    st->pickCue(c2);
    settle(400);
    QCOMPARE(drawnOpen(sid, fid, "2 picked:"), 1.0);
    QVERIFY(markShown(sid));
    QCOMPARE(onWire(fid, 0), 0);
    // The preset (nothing sends it): drawn where it is, still held.
    st->pickCue(preset);
    settle(300);
    QCOMPARE(drawnOpen(sid, fid, "preset picked:"), 0.0);
    QVERIFY(markShown(sid));

    // A GO from the Run moves it for real -- the cue's 2 s fade from where
    // it was held, though its scene is at full under the preview: a motor
    // is never told to jump.
    st->pickCue(c1);
    settle(300);
    QCOMPARE(onWire(fid, 0), 0);
    loadAndGo(c, n1);
    {
        const QString why = travels(fid, 0, 255, 1200, 4000);
        QVERIFY2(why.isEmpty(), qPrintable("GO after a pick: " + why));
    }
    QTRY_VERIFY_WITH_TIMEOUT(!markShown(sid), 1000);
    QCOMPARE(drawnOpen(sid, fid, "GO, opened:"), 1.0);

    /* The same from a start that is not the Run's (a VC cue list, OSC): the
       list is already playing the curtain's scene, at full under the hold,
       when the preview lets go -- still the cue's fade from where it was. */
    auto closeIt = [&]() {
        stopAll();
        jump(c, 0);
        settle(300);
        stopAll();
        QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
        ua.at(3)->reset(0, 2);
        doc->inputOutputMap()->releaseUniverses(true);
    };
    closeIt();
    QTRY_COMPARE_WITH_TIMEOUT(onWire(fid, 0), 0, 1000);
    st->pickCue(c1);
    settle(2500);                              // the scene at full, held
    QCOMPARE(onWire(fid, 0), 0);
    jump(c, 1);
    {
        const QString why = travels(fid, 0, 255, 1200, 4000);
        QVERIFY2(why.isEmpty(), qPrintable("a list started after a pick: " + why));
    }

    // Picked while it opens: held where it is, drawn where the cue puts it.
    closeIt();
    QTRY_COMPARE_WITH_TIMEOUT(onWire(fid, 0), 0, 1000);
    loadAndGo(c, n1);
    QTRY_VERIFY_WITH_TIMEOUT(onWire(fid, 0) > 20, 1500);
    QVERIFY2(onWire(fid, 0) < 255, "still opening");
    st->pickCue(c2);
    settle(200);
    const int stopped = onWire(fid, 0);
    QVERIFY2(stopped > 0 && stopped < 255, qPrintable(QString::number(stopped)));
    settle(800);
    QCOMPARE(onWire(fid, 0), stopped);
    QCOMPARE(drawnOpen(sid, fid, "2 picked mid-move:"), 1.0);
    QVERIFY(markShown(sid));
    // For the UX review: the plot with the drape marked.
    const QString shots = qEnvironmentVariable("QLC_SHOT_DIR");
    if (!shots.isEmpty())
    {
        QDir().mkpath(shots);
        for (const QSize &size : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
        {
            m_app->resize(size);
            settle(300);
            mon->m_graphicsView->centerOn(mon->m_graphicsView->m_sceneryItems.value(sid));
            settle(100);
            mon->m_graphicsView->grab().save(QDir(shots).filePath(
                QStringLiteral("curtain-held-while-editing-%1x%2.png").arg(size.width()).arg(size.height())));
        }
        m_app->resize(1280, 800);
    }

    // The Rig Overview draws it the same.
    {
        StructureStudioView view(doc, StructureStudioView::StageKind, 0);
        view.setLiveValues(true);
        QCOMPARE(view.computeDrapePose(drape).open, 1.0);
    }

    // Into Operate: let go to the run, which plays the cue that opens it --
    // over that cue's fade from where it was held, never a jump.
    doc->setMode(Doc::Operate);
    {
        const QString why = travels(fid, 0, 255, 1200, 4000);
        QVERIFY2(why.isEmpty(), qPrintable("into Operate: " + why));
    }
    QTRY_VERIFY_WITH_TIMEOUT(!markShown(sid), 1000);
    QVERIFY(!keeper->machineryHeldForEdit());

    // Operate: the run's -- a jump moves it, nothing holds it.
    stopAll();
    jump(c, 2);
    QTRY_COMPARE_WITH_TIMEOUT(onWire(fid, 0), 255, 4000);
    QTRY_COMPARE_WITH_TIMEOUT(drawnOpen(sid, fid, "Operate, jumped:"), 1.0, 1000);
    QVERIFY(!markShown(sid));
    stopAll();
    doc->setMode(Doc::Design);
}

QString CurtainDraw_Test::travels(quint32 fixtureId, quint32 ch, int to, int notBefore, int within)
{
    /* Watched every 10 ms: no step bigger than a 2 s fade's 40 ms worth
       (the wire is sampled, a frame can be missed), at @p to no sooner than
       @p notBefore ms, and there within @p within. Empty: it did. */
    QElapsedTimer t;
    t.start();
    int last = onWire(fixtureId, ch);
    int biggest = 0;
    qint64 reached = -1;
    QStringList trace;
    trace << QStringLiteral("0:%1").arg(last);
    while (t.elapsed() < within)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QTest::qSleep(10);
        const int now = onWire(fixtureId, ch);
        if (now != last)
            trace << QStringLiteral("%1:%2").arg(t.elapsed()).arg(now);
        biggest = qMax(biggest, qAbs(now - last));
        last = now;
        if (now == to && reached < 0)
            reached = t.elapsed();
        if (reached >= 0 && t.elapsed() > reached + 300)
            break;
    }
    if (biggest <= 40 && reached >= notBefore && last == to)
        return QString();
    return QStringLiteral("to %1: biggest step %2, reached at %3 ms, ends %4; %5")
        .arg(to).arg(biggest).arg(reached).arg(last).arg(trace.mid(0, 60).join(QLatin1Char(' ')));
}

/* His show (a copy, never in the gate): QLC_CURTAIN_SHOW=<the .qlcc>, or
   QLC_CURTAIN_SHOW_QZ=<base64 of its qCompress()> to send it to a pool
   host. Scenery 1 "Main drape", driven by fixture 182 on U04-Control: in
   Design, 1.0.1 then 1.1.1 clicked in the Show Tree. */
void CurtainDraw_Test::hisCurtain()
{
    QTemporaryDir dir;
    QString path = qEnvironmentVariable("QLC_CURTAIN_SHOW");
    const QByteArray qz = qgetenv("QLC_CURTAIN_SHOW_QZ");
    if (path.isEmpty() && !qz.isEmpty())
    {
        path = dir.filePath("show.qlcc");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(qUncompress(QByteArray::fromBase64(qz)));
    }
    if (path.isEmpty())
        QSKIP("set QLC_CURTAIN_SHOW");
    Doc *doc = m_app->doc();
    doc->setMode(Doc::Design);
    m_app->clearDocument();
    QVERIFY(m_app->loadXML(path) == QFile::NoError);
    doc->resetModified();
    Chaser *c = nullptr;
    for (Function *f : doc->functions())
        if (f->name() == "Production Show" && f->type() == Function::ChaserType)
            c = qobject_cast<Chaser *>(f);
    QVERIFY(c != nullptr);
    doc->showControl()->setMainCueList(c->id());
    Fixture *cur = nullptr;
    for (Fixture *f : doc->fixtures())
        if (f->name() == "Main drape 2 curtain")
            cur = f;
    QVERIFY(cur != nullptr);
    ShowTreeController *st = doc->showTree();
    quint32 listId = ShowTree::invalidId();
    st->productionOfChaser(c->id(), &listId);
    const CueList *list = st->cueList(listId);
    QVERIFY(list != nullptr);
    auto cueId = [&](const char *n) {
        for (int i = 0; i < list->cues.count(); ++i)
            if (st->cueNumber(*list, i) == QLatin1String(n))
                return list->cues.at(i).id;
        return ShowTree::invalidId();
    };
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr);
    m_app->m_tab->setCurrentIndex(m_app->m_tab->indexOf(mon));
    mon->updateView();
    settle(500);
    const quint32 sid = 1, fid = cur->id();
    QVERIFY(mon->m_graphicsView->m_sceneryItems.contains(sid));
    const int was = onWire(fid, 0);
    drawnOpen(sid, fid, "loaded:");
    for (const char *n : { "1.0.1", "1.1.1", "1.0.2.1", "1.1.2" })
    {
        st->pickCue(cueId(n));
        settle(3500);
        QVERIFY2(drawnOpen(sid, fid, n) == 1.0, n);
        QVERIFY2(markShown(sid), n);
        QCOMPARE(onWire(fid, 0), was);
    }
    st->stopPreview();
    stopAll();
    doc->resetModified();
}

QTEST_MAIN(CurtainDraw_Test)
