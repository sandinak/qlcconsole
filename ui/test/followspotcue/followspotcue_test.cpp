/*
  Q Light Controller Plus
  followspotcue_test.cpp

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
#include <QTemporaryDir>
#include <QTabWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#include "showtreeview.h"
#include "programmercontroller.h"
#include "effectscriptrunner.h"
#include "programmingmanager.h"
#include "workbench.h"
#undef protected
#undef private
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "inputoutputmap.h"
#include "stagetarget.h"
#include "qlcchannel.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcphysical.h"
#include "showtreemodel.h"
#include "universe.h"
#include "genericfader.h"
#include "fixture.h"
#include "scene.h"
#include "fixturegroup.h"
#include "collection.h"
#include "qlcpalette.h"
#include "showcontrol.h"
#include "effectscriptcache.h"
#include "mastertimer.h"
#include "truss.h"
#include "doc.h"

#include "followspotcue_test.h"

using namespace ShowTree;

namespace
{

void settle(int ms = 0)
{
    for (int i = 0; i < 3; ++i)
    {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    }
    if (ms > 0)
        QTest::qWait(ms);
}

QString panTilt(Doc *doc, const QList<quint32> &fixtures)
{
    QStringList out;
    QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
    for (quint32 id : fixtures)
    {
        Fixture *f = doc->fixture(id);
        if (f == nullptr)
            continue;
        const QByteArray dmx = ua.at(int(f->universe()))->preGMValues();
        const quint32 p = f->channelNumber(QLCChannel::Pan, QLCChannel::MSB);
        const quint32 t = f->channelNumber(QLCChannel::Tilt, QLCChannel::MSB);
        out << QString("%1 p%2 t%3").arg(f->name())
                   .arg(p == QLCChannel::invalid() ? -1 : uchar(dmx.at(int(f->address() + p))))
                   .arg(t == QLCChannel::invalid() ? -1 : uchar(dmx.at(int(f->address() + t))));
    }
    doc->inputOutputMap()->releaseUniverses(false);
    return out.join(" | ");
}

}

void FollowSpotCue_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
    m_app->showNormal();
    m_app->resize(1280, 800);
}

void FollowSpotCue_Test::cleanupTestCase()
{
    settle();
    m_app->doc()->resetModified();
    delete m_app;
    m_app = nullptr;
}

bool FollowSpotCue_Test::open(const QString &path)
{
    m_app->doc()->resetModified();
    m_app->clearDocument();
    const bool ok = m_app->loadXML(path) == QFile::NoError;
    m_app->doc()->resetModified();
    settle(50);
    return ok;
}

ShowTreeView *FollowSpotCue_Test::showTree() const
{
    ShowTreeView *view = ShowTreeView::instance();
    QWidget *w = view;
    while (w != nullptr && m_app->m_tab->indexOf(w) < 0)
        w = w->parentWidget();
    if (w != nullptr)
        m_app->m_tab->setCurrentWidget(w);
    settle();
    return view;
}

/* His show (a copy; QLC_FSPOT_SHOW=<DanceHysteria-0.6.qlcc>, not in the
   gate): 1.1.2's AT-Spots follow ES-Witch with the stick -- selected in the
   Show Tree, GO in Design, GO in Operate -- with another scene open in
   Build's Workbench. */
void FollowSpotCue_Test::hisShow()
{
    const QString show = qEnvironmentVariable("QLC_FSPOT_SHOW");
    if (show.isEmpty() || !QFileInfo::exists(show))
        QSKIP("set QLC_FSPOT_SHOW");
    QTemporaryDir dir;
    const QString copy = dir.filePath("show.qlcc");
    QVERIFY(QFile::copy(show, copy));
    m_app->doc()->effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    QVERIFY(open(copy));
    Doc *doc = m_app->doc();
    ProgrammerController *pc = doc->programmer();
    const QList<quint32> spots = { 4, 142 };       // AT1-3z, AT2-3z
    const quint32 cue112 = 103, scene = 110, look = 14;

    ShowTreeView *view = showTree();
    QTreeWidget *tree = view->m_tree;
    QTreeWidgetItem *row = nullptr;
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode
            && (*it)->data(0, ShowTreeView::IdRole).toUInt() == cue112)
            row = *it;
    QVERIFY(row != nullptr);

    // Another scene open in Build (1.1.2's T-Spots, an Aim on the Announcer).
    ProgrammingManager::instance()->workbench()->load(120);
    settle(200);
    pc->clearAxisBindings();
    pc->bindPanAxis();
    pc->slotControllerInputChanged(9, 0, 128, QString());
    pc->bindTiltAxis();
    pc->slotControllerInputChanged(9, 1, 128, QString());

    auto follows = [&](const QString &how) {
        QTRY_VERIFY2_WITH_TIMEOUT(pc->followSpotStatus().sceneId == scene, qPrintable(how), 3000);
        QVERIFY2(pc->followSpotStatus().live, qPrintable(how));
        QVERIFY(doc->effectScriptRunner()->isPlaying(look));
        const QString before = panTilt(doc, spots);
        pc->slotControllerInputChanged(9, 0, 30, QString());
        QTest::qWait(1200);
        pc->slotControllerInputChanged(9, 0, 128, QString());
        const QString after = panTilt(doc, spots);
        qInfo().noquote() << how << before << "->" << after << m_app->followChip()->text();
        QVERIFY2(after != before, qPrintable(how + ": " + after));
        QVERIFY(m_app->followChip()->text().contains(QString::fromUtf8("AT-Spots ← ES-Witch")));
        QTest::qWait(300);
    };
    tree->setCurrentItem(row);
    follows("selected");

    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.cue = "1.1.2";
    QVERIFY(m_app->runCommand(go, ShowControl::RouteRunTab));
    follows("GO in Design");

    doc->setMode(Doc::Operate);
    settle(300);
    QVERIFY(m_app->runCommand(go, ShowControl::RouteRunTab));
    follows("GO in Operate");
    doc->setMode(Doc::Design);
    settle(300);
    pc->clearAxisBindings();
}

/* His show again: every cue selected in the Show Tree, forward, back,
   twice, after nothing and skipping one, starts every effect it holds and
   the effect shows (its fixtures' output moves). */
void FollowSpotCue_Test::everyCueFiresItsEffects()
{
    const QString show = qEnvironmentVariable("QLC_FSPOT_SHOW");
    if (show.isEmpty() || !QFileInfo::exists(show))
        QSKIP("set QLC_FSPOT_SHOW");
    QTemporaryDir dir;
    const QString copy = dir.filePath("show.qlcc");
    QVERIFY(QFile::copy(show, copy));
    m_app->doc()->effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    m_app->doc()->effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "rgbscripts"), true);
    QVERIFY(open(copy));
    Doc *doc = m_app->doc();
    EffectScriptRunner *runner = doc->effectScriptRunner();
    ShowTreeView *view = showTree();
    QTreeWidget *tree = view->m_tree;
    QList<QTreeWidgetItem *> cues;
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode)
            cues << *it;
    QVERIFY(!cues.isEmpty());
    ShowTreeController *st = doc->showTree();
    int misses = 0, checks = 0;
    auto check = [&](QTreeWidgetItem *row, const QString &how) {
        tree->setCurrentItem(row);
        settle(350);
        const Cue *c = st->cue(row->data(0, ShowTreeView::IdRole).toUInt());
        const Collection *coll = c ? qobject_cast<const Collection *>(doc->function(c->collection)) : nullptr;
        if (coll == nullptr)
            return;
        for (quint32 fid : coll->functions())
        {
            Scene *s = qobject_cast<Scene *>(doc->function(fid));
            if (s == nullptr)
                continue;
            for (quint32 pid : s->palettes())
            {
                QLCPalette *p = doc->palette(pid);
                if (p == nullptr || p->type() != QLCPalette::Effect || runner->failures().contains(pid))
                    continue;
                ++checks;
                if (!s->isRunning() || !runner->isPlaying(pid))
                {
                    ++misses;
                    qInfo().noquote() << "NOT PLAYING" << how << row->text(0) << s->name() << p->name();
                    continue;
                }
                if (ProgrammerController::isFollowSpotPalette(p))
                    continue;   // it holds still until the stick moves it
                // It shows: the fixtures it plays change on some channel.
                QList<quint32> fx = s->fixtures();
                for (quint32 gid : s->fixtureGroups())
                    if (FixtureGroup *g = doc->fixtureGroup(gid))
                        fx += g->fixtureList();
                auto snap = [&]() {
                    QByteArray all;
                    QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
                    for (quint32 id : fx)
                        if (Fixture *f = doc->fixture(id))
                            all += ua.at(int(f->universe()))->preGMValues().mid(int(f->address()), int(f->channels()));
                    doc->inputOutputMap()->releaseUniverses(false);
                    return all;
                };
                const QByteArray first = snap();
                bool moved = false;
                for (int k = 0; k < 12 && !moved; ++k)
                {
                    QTest::qWait(100);
                    moved = snap() != first;
                }
                if (!moved)
                {
                    ++misses;
                    qInfo().noquote() << "STILL" << how << row->text(0) << s->name() << p->name();
                }
            }
        }
    };
    for (QTreeWidgetItem *r : cues)
        check(r, "forward");
    for (int i = cues.size() - 1; i >= 0; --i)
        check(cues.at(i), "back");
    for (QTreeWidgetItem *r : cues)
    {
        check(r, "again");
        tree->setCurrentItem(nullptr);
        settle(100);
        check(r, "after none");
    }
    for (int i = 0; i + 2 < cues.size(); ++i)
    {
        check(cues.at(i), "skip a");
        check(cues.at(i + 2), "skip b");
    }
    qInfo() << "checks" << checks << "misses" << misses;
    QCOMPARE(misses, 0);
}

/* The show as Branson has it (DanceHysteria 1.1.2, made here so the gate
   runs it): two movers in the AT-Spots group, a Person (ES-Witch) on a
   riser, cue 1.1.2's layer on the AT-Spots with an Aim look on her and the
   Followspot effect -- and, as nearly always, some other scene open in
   Build's Workbench. Selecting the cue in the Show Tree, GO in Run in
   Design and GO in Operate: every one lets the stick move her and the
   heads follow, and the footer says so. Before 0.4.0 the Workbench's scene
   kept the stick, nothing moved and every indicator said "off". */
void FollowSpotCue_Test::aCueFollowSpotMovesFromEveryRoute()
{
    m_app->doc()->resetModified();
    m_app->clearDocument();
    settle(50);
    Doc *doc = m_app->doc();
    EffectScriptCache *cache = doc->effectScriptRunner()->cache();
    cache->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    ProgrammerController *pc = doc->programmer();
    MonitorProperties *props = doc->monitorProperties();

    // Two spots with 16-bit pan, hung 6 m up across the stage.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Follow Spot Mover");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("ch");
    const char *names[] = { "Pan", "Pan fine", "Tilt", "Dimmer" };
    QLCFixtureHead head;
    for (int k = 0; k < 4; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[k]);
        ch->setGroup(k == 3 ? QLCChannel::Intensity : k == 2 ? QLCChannel::Tilt : QLCChannel::Pan);
        ch->setControlByte(k == 1 ? QLCChannel::LSB : QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, quint32(k));
        head.addChannel(quint32(k));
    }
    mode->insertHead(-1, head);
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    def->addMode(mode);

    FixtureGroup *spots = new FixtureGroup(doc);
    spots->setName("AT-Spots");
    QVERIFY(doc->addFixtureGroup(spots));
    QList<Fixture *> movers;
    for (int i = 0; i < 2; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString("AT%1").arg(i + 1));
        f->setFixtureDefinition(def, mode);
        f->setUniverse(0);
        f->setAddress(quint32(10 + i * 10));
        QVERIFY(doc->addFixture(f));
        props->setFixturePosition(f->id(), 0, 0, QVector3D(i == 0 ? -2 : 2, 0, 6));
        FixtureRigProps rp;
        rp.mountingType = Truss::TopHung;
        props->setFixtureRigProps(f->id(), rp);
        spots->assignFixture(f->id());
        movers << f;
    }
    StageTarget *witch = props->addStageTarget();
    witch->setName("ES-Witch");
    witch->setKind(StageTarget::Kind::Person);
    const QVector3D home(0.5f, 4.0f, 0.61f);     // on a riser
    witch->setPosition(home);

    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setName("Test Target");
    aim->setStageTargetId(witch->id());
    QVERIFY(doc->addPalette(aim));
    QLCPalette *follow = new QLCPalette(QLCPalette::Effect, doc);
    follow->setName("followspot");
    follow->setScriptPath(cache->scriptPath("followspot"));
    QVERIFY(!follow->scriptPath().isEmpty());
    QVERIFY(doc->addPalette(follow));
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer, doc);
    full->setName("Full");
    full->setValue(255);
    QVERIFY(doc->addPalette(full));

    // 1.1.1 lights nothing of theirs; 1.1.2 is the follow spot.
    ShowTreeController *st = doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("DH"), "00-Preshow", ListMode::Tracking);
    const quint32 c1 = st->addCue(list, -1, "Music Start");
    const quint32 c2 = st->addCue(list, 0, "Solo Start");
    for (quint32 id : { c1, c2 })
    {
        ShowTree::Cue cue = *st->cue(id);
        cue.number = id == c1 ? QStringLiteral("1.1.1") : QStringLiteral("1.1.2");
        QVERIFY(st->setCue(cue));
    }
    Target t;
    t.groups << spots->id();
    const quint32 layer = st->addLayer(c2, t);
    st->setLayerLook(c2, layer, st->lookForPalette(full->id()));
    st->setLayerLook(c2, layer, st->lookForPalette(aim->id()));
    Look fx = st->lookForPalette(follow->id());
    fx.key = effectKey(QStringLiteral("Position"));
    QVERIFY(st->setLayerLook(c2, layer, fx));
    settle(300);

    // Some other scene open in Build, as there nearly always is.
    Scene *other = new Scene(doc);
    other->setName("Wash");
    doc->addFunction(other);
    other->addFixtureGroup(spots->id());
    other->addPalette(full->id());
    ProgrammingManager::instance()->workbench()->load(other->id());
    settle(200);
    QCOMPARE(pc->m_focusedSceneId, other->id());

    auto pans = [&]() {
        QList<int> out;
        QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
        const QByteArray dmx = ua.first()->preGMValues();
        doc->inputOutputMap()->releaseUniverses(false);
        for (Fixture *f : movers)
            out << ((uchar(dmx.at(int(f->address()))) << 8) | uchar(dmx.at(int(f->address() + 1))));
        return out;
    };
    QLabel *chip = m_app->followChip();
    QVERIFY(chip != nullptr);
    pc->clearAxisBindings();
    pc->bindPanAxis();
    pc->slotControllerInputChanged(9, 0, 128, QString());
    pc->bindTiltAxis();
    pc->slotControllerInputChanged(9, 1, 128, QString());

    // The stick right: the subject walks right at her own height, every
    // head's pan follows, and the footer says who follows whom.
    auto movesWithTheStick = [&](const QString &how) {
        QTRY_VERIFY2_WITH_TIMEOUT(pc->followSpotStatus().state != ProgrammerController::FollowSpotStatus::Off,
                                  qPrintable(how + ": no follow spot on"), 3000);
        QVERIFY2(pc->followSpotStatus().live, qPrintable(how));
        QTRY_VERIFY2_WITH_TIMEOUT(chip->isVisible(), qPrintable(how), 2000);
        QVERIFY2(chip->text().contains(QString::fromUtf8("AT-Spots ← ES-Witch")), qPrintable(chip->text()));
        QVERIFY2(!chip->text().contains("no stick"), qPrintable(chip->text()));
        const QVector3D from = witch->position();
        const QList<int> before = pans();
        pc->slotControllerInputChanged(9, 0, 255, QString());
        QTest::qWait(500);
        const QList<int> after = pans();
        QVERIFY2(chip->text().contains("moving"), qPrintable(how + ": " + chip->text()));
        pc->slotControllerInputChanged(9, 0, 128, QString());
        QVERIFY2(witch->position().x() > from.x() + 0.2f,
                 qPrintable(QString("%1: the subject did not move (%2 -> %3)").arg(how)
                            .arg(from.x()).arg(witch->position().x())));
        QVERIFY2(qAbs(witch->position().z() - home.z()) < 1e-4f, qPrintable(how + ": off her riser"));
        for (int i = 0; i < movers.size(); ++i)
            QVERIFY2(after.at(i) != before.at(i),
                     qPrintable(QString("%1: %2 did not follow (%3)").arg(how, movers.at(i)->name()).arg(after.at(i))));
        QTest::qWait(600);
        QVERIFY2(!chip->text().contains("moving"), qPrintable(how + ": " + chip->text()));
    };

    // Selected in the Show Tree.
    ShowTreeView *view = showTree();
    QTreeWidget *tree = view->m_tree;
    QTreeWidgetItem *row = nullptr, *row1 = nullptr;
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode)
        {
            if ((*it)->data(0, ShowTreeView::IdRole).toUInt() == c2)
                row = *it;
            if ((*it)->data(0, ShowTreeView::IdRole).toUInt() == c1)
                row1 = *it;
        }
    QVERIFY(row != nullptr && row1 != nullptr);
    tree->setCurrentItem(row1);
    settle(200);
    QVERIFY(chip->isHidden());
    tree->setCurrentItem(row);
    movesWithTheStick("selected");

    // Away from it: she glides back where the show has her, the chip goes.
    tree->setCurrentItem(row1);
    QTRY_VERIFY_WITH_TIMEOUT((witch->position() - home).length() < 1e-3f, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(chip->isHidden(), 2000);

    // GO in Run, in Design.
    const quint32 chaser = st->cueList(list)->chaser;
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = chaser;
    go.cue = st->cue(c2)->number;
    QVERIFY(m_app->runCommand(go, ShowControl::RouteRunTab));
    movesWithTheStick("GO in Design");
    QVERIFY(doc->function(chaser)->isRunning());
    doc->masterTimer()->stopAllFunctions();
    QTRY_VERIFY_WITH_TIMEOUT((witch->position() - home).length() < 1e-3f, 3000);

    // GO in Operate.
    doc->setMode(Doc::Operate);
    settle(200);
    QVERIFY(m_app->runCommand(go, ShowControl::RouteRunTab));
    movesWithTheStick("GO in Operate");
    doc->masterTimer()->stopAllFunctions();
    doc->setMode(Doc::Design);
    settle(200);
    QTRY_VERIFY_WITH_TIMEOUT((witch->position() - home).length() < 1e-3f, 3000);

    // A follow spot open in the Workbench is programmed there: it has the stick.
    pc->clearAxisBindings();
    doc->resetModified();
}

/* "selecting a cue doesn't always fire its effect": the first cue selected
   breathed, the next ones held still. The preview's scenes are Override and
   each new one's fader went above the effect scripts' (made with the first
   effect), covering the effect from the second cue on. Every cue, in any
   order, again and back: the breath shows (DanceHysteria's sub-cues
   1.0.2.1-3 on the StepTops, one Breathe over them all, and another). */
void FollowSpotCue_Test::everyCueSelectedShowsItsEffect()
{
    m_app->doc()->resetModified();
    m_app->clearDocument();
    settle(50);
    Doc *doc = m_app->doc();
    doc->setMode(Doc::Design);
    EffectScriptCache *cache = doc->effectScriptRunner()->cache();
    cache->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));

    // RGB bars with no dimmer channel (the effect scales their colour).
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("RGB Bar");
    def->setType(QLCFixtureDef::LEDBarBeams);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("rgb");
    QLCFixtureHead head;
    const QLCChannel::PrimaryColour cols[] = { QLCChannel::Red, QLCChannel::Green, QLCChannel::Blue };
    for (int k = 0; k < 3; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("C%1").arg(k));
        ch->setGroup(QLCChannel::Intensity);
        ch->setColour(cols[k]);
        def->addChannel(ch);
        mode->insertChannel(ch, quint32(k));
        head.addChannel(quint32(k));
    }
    mode->insertHead(-1, head);
    def->addMode(mode);
    FixtureGroup *tops = new FixtureGroup(doc);
    tops->setName("StepTops");
    QVERIFY(doc->addFixtureGroup(tops));
    QList<Fixture *> bars;
    for (int i = 0; i < 2; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString("Top%1").arg(i + 1));
        f->setFixtureDefinition(def, mode);
        f->setUniverse(0);
        f->setAddress(quint32(100 + i * 3));
        QVERIFY(doc->addFixture(f));
        tops->assignFixture(f->id());
        bars << f;
    }
    QLCPalette *white = new QLCPalette(QLCPalette::Color, doc);
    white->setName("White");
    white->setValue(QLCPalette::colorToString(QColor(Qt::white), QColor(Qt::black)));
    QVERIFY(doc->addPalette(white));
    QList<quint32> dims;
    for (int v : { 188, 121, 250 })
    {
        QLCPalette *d = new QLCPalette(QLCPalette::Dimmer, doc);
        d->setName(QString("Dim %1").arg(v));
        d->setValue(v);
        QVERIFY(doc->addPalette(d));
        dims << d->id();
    }
    QList<quint32> breathes;
    for (const char *name : { "Breathe", "Pulse", "Candle" })
    {
        QLCPalette *b = new QLCPalette(QLCPalette::Effect, doc);
        b->setName(name);
        b->setScriptPath(cache->scriptPath("dimmer-phaser"));
        QVERIFY(!b->scriptPath().isEmpty());
        QMap<QString, double> params;
        params.insert("rate", 1.5);
        params.insert("minDim", 0.05);
        b->setEffectParamValues(params);
        QVERIFY(doc->addPalette(b));
        breathes << b->id();
    }

    // The lanterns flicker on the same universe all along (a layer of the
    // first cue, tracked through): the effects' fader is never let go of.
    FixtureGroup *lanterns = new FixtureGroup(doc);
    lanterns->setName("Lanterns");
    QVERIFY(doc->addFixtureGroup(lanterns));
    {
        Fixture *f = new Fixture(doc);
        f->setName("Lantern");
        f->setFixtureDefinition(def, mode);
        f->setUniverse(0);
        f->setAddress(200);
        QVERIFY(doc->addFixture(f));
        lanterns->assignFixture(f->id());
    }

    ShowTreeController *st = doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("DH"), "Ahhs", ListMode::Tracking);
    QList<quint32> cues;
    Target t;
    t.groups << tops->id();
    for (int i = 0; i < 4; ++i)
    {
        const quint32 c = st->addCue(list, i - 1, QString("Ahhs %1").arg(i + 1));
        const quint32 layer = st->addLayer(c, t);
        st->setLayerLook(c, layer, st->lookForPalette(white->id()));
        st->setLayerLook(c, layer, st->lookForPalette(dims.at(i % 3)));
        // One Breathe over the first three, as his sub-cues; a Pulse on the last.
        Look fx = st->lookForPalette(breathes.at(i == 3 ? 1 : 0));
        fx.key = effectKey(QStringLiteral("Dimmer"));
        QVERIFY(st->setLayerLook(c, layer, fx));
        if (i == 0)
        {
            Target lt;
            lt.groups << lanterns->id();
            const quint32 ll = st->addLayer(c, lt);
            st->setLayerLook(c, ll, st->lookForPalette(white->id()));
            st->setLayerLook(c, ll, st->lookForPalette(dims.at(0)));
            Look candle = st->lookForPalette(breathes.at(2));
            candle.key = effectKey(QStringLiteral("Dimmer"));
            QVERIFY(st->setLayerLook(c, ll, candle));
        }
        cues << c;
    }
    settle(300);

    ShowTreeView *view = showTree();
    QTreeWidget *tree = view->m_tree;
    auto rowOf = [&](quint32 cueId) -> QTreeWidgetItem * {
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode
                && (*it)->data(0, ShowTreeView::IdRole).toUInt() == cueId)
                return *it;
        return nullptr;
    };
    auto snap = [&]() {
        QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
        const QByteArray v = ua.first()->preGMValues().mid(100, 6);
        doc->inputOutputMap()->releaseUniverses(false);
        return v;
    };
    auto breathes_ = [&](int i, const QString &how) {
        QTreeWidgetItem *row = rowOf(cues.at(i));
        QVERIFY(row != nullptr);
        tree->setCurrentItem(row);
        settle(300);
        QSet<QByteArray> seen;
        for (int k = 0; k < 12; ++k)
        {
            seen.insert(snap());
            QTest::qWait(60);
        }
        QVERIFY2(seen.size() >= 3, qPrintable(QString("%1 cue %2: the effect does not show (%3 frames alike: %4)")
                                             .arg(how).arg(i + 1).arg(12 - seen.size() + 1)
                                             .arg(QString(snap().toHex(' ')))));
    };
    for (int i = 0; i < cues.size(); ++i)
        breathes_(i, "forward");
    for (int i = cues.size() - 1; i >= 0; --i)
        breathes_(i, "back");
    breathes_(1, "again");
    breathes_(1, "again");
    tree->setCurrentItem(nullptr);
    settle(200);
    breathes_(2, "after none");
    doc->resetModified();
}

QTEST_MAIN(FollowSpotCue_Test)
