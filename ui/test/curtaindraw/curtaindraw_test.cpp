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

/* "open the curtain with its cue, then go to a later cue: the curtain stays
   closed on screen; jump while it is opening and the traveler stops where it
   is". His structure: the curtain fixture on a universe with no output,
   driving the Main drape; a scene opening it that every cue plays; a jump
   from the cue that opens it, done and mid-fade, in Design (the Run, a
   cue picked) and in Operate. The engine has the curtain open every time
   (TrackBack_Test::hisCurtain); this is what the plot DRAWS. */
void CurtainDraw_Test::theDrapeIsDrawnWhereItsCueLeftIt()
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
    open->setFadeInSpeed(1500);
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
    const quint32 dark = st->addCue(listId, -1, "Preset");
    QVERIFY(layer(dark, half->id()));
    const quint32 c1 = st->addCue(listId, 0, "Curtain");
    QVERIFY(layer(c1, open->id()) && layer(c1, half->id()));
    const quint32 c2 = st->addCue(listId, 1, "Song");
    QVERIFY(layer(c2, open->id()) && layer(c2, full->id()));
    const quint32 c3 = st->addCue(listId, 2, "Later");
    QVERIFY(layer(c3, open->id()) && layer(c3, half->id()));
    const CueList *list = st->cueList(listId);
    QVERIFY(list != nullptr);
    Chaser *c = qobject_cast<Chaser *>(doc->function(list->chaser));
    QVERIFY(c != nullptr);
    doc->showControl()->setMainCueList(c->id());

    // The Lighting Studio's plot, as he has it up.
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr);
    m_app->m_tab->setCurrentIndex(m_app->m_tab->indexOf(mon));
    mon->updateView();
    settle(300);
    QVERIFY(mon->m_graphicsView != nullptr);
    QVERIFY(mon->m_graphicsView->m_sceneryItems.contains(drape->id()));
    const quint32 sid = drape->id(), fid = curtain->id();
    QCOMPARE(drawnOpen(sid, fid, "loaded:"), 0.0);    // nothing plays on it: closed

    auto closeIt = [&]() {
        stopAll();
        // Closed again: the preset cue, the curtain's channel let go.
        jump(c, 0);
        settle(400);
        stopAll();
        QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
        ua.at(3)->reset(0, 2);
        doc->inputOutputMap()->releaseUniverses(true);
        settle(300);
    };
    auto check = [&](const char *what) {
        // Long past the fade: the drape drawn open, as the wire has it.
        QTRY_VERIFY2_WITH_TIMEOUT(drawnOpen(sid, fid, what) == 1.0, what, 4000);
    };

    for (const Doc::Mode mode : { Doc::Design, Doc::Operate })
    {
        closeIt();
        doc->setMode(mode);
        // Opened by its cue, done; then a later cue.
        jump(c, 1);
        settle(2000);
        check("opened by its cue:");
        jump(c, 3);
        settle(600);
        check("then a later cue:");

        // Jumped away while it opens.
        closeIt();
        jump(c, 1);
        settle(500);
        drawnOpen(sid, fid, "opening:");
        jump(c, 3);
        check("jumped while opening:");

        // The Run's click: Load, GO, mid-fade.
        closeIt();
        loadAndGo(c, st->cueNumber(*st->cueList(listId), 1));
        settle(500);
        drawnOpen(sid, fid, "GO, opening:");
        loadAndGo(c, st->cueNumber(*st->cueList(listId), 2));
        check("Load + GO a later cue while opening:");

        if (mode == Doc::Design)
        {
            // Design: a cue picked previews it.
            closeIt();
            st->pickCue(c1);
            settle(500);
            drawnOpen(sid, fid, "picked, opening:");
            st->pickCue(c3);
            check("then a later cue picked:");
            st->stopPreview();
        }
    }
    stopAll();
    doc->setMode(Doc::Design);
}

/* His show (a copy, never in the gate): QLC_CURTAIN_SHOW=<the .qlcc>, or
   QLC_CURTAIN_SHOW_QZ=<base64 of its qCompress()> to send it to a pool
   host. Scenery 1 "Main drape", driven by fixture 182 on U04-Control. */
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
    auto idx = [&](const QString &cue) {
        for (int i = 0; i < c->stepsCount(); ++i)
            if (c->steps().at(i).note == cue || c->stepCue(i) == cue)
                return i;
        return -1;
    };
    const int s101 = idx("1.0.1"), s111 = idx("1.1.1");
    QVERIFY(s101 > 0 && s111 > s101);
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr);
    m_app->m_tab->setCurrentIndex(m_app->m_tab->indexOf(mon));
    mon->updateView();
    settle(500);
    const quint32 sid = 1, fid = cur->id();
    QVERIFY(mon->m_graphicsView->m_sceneryItems.contains(sid));
    drawnOpen(sid, fid, "loaded:");
    auto check = [&](const char *what) {
        QTRY_VERIFY2_WITH_TIMEOUT(drawnOpen(sid, fid, what) == 1.0, what, 6000);
    };
    for (const Doc::Mode mode : { Doc::Design, Doc::Operate })
    {
        stopAll();
        doc->setMode(mode);
        jump(c, s101 - 1);
        settle(1500);
        jump(c, s101);
        settle(800);
        drawnOpen(sid, fid, "1.0.1 opening:");
        jump(c, s111);
        check("then a jump to 1.1.1:");
        jump(c, s101);
        settle(4500);
        check("1.0.1 done:");
        loadAndGo(c, QStringLiteral("1.1.2"));
        check("then Load 1.1.2 + GO:");
    }
    stopAll();
    doc->setMode(Doc::Design);
    doc->resetModified();
}

QTEST_MAIN(CurtainDraw_Test)
