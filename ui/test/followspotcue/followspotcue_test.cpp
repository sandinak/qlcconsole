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
#include "inputpatch.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
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
#include "followspotmover.h"
#include "followspotdrive.h"
#include "gamepadrole.h"
#include "lookeditor.h"
#include <QScrollArea>
#include <QDoubleSpinBox>
#include <QScrollBar>

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

/* His controller as the HID plugin lists it on his Mac (dink): a DualShock 4
   over Bluetooth, vendor 054C product 09CC. The real HID plugin is loaded
   (its playstation.qxhid profile matching it by vendor and product) with
   this pad made virtual -- HIDVirtualPad, the one part not real is the
   reading of the hardware. */
const char *kDS4 = " DUALSHOCK 4 Wireless Controller (DE:4C:09:77:77:93)";
/* Others he might connect (not in his show's slot): a DualSense, an Xbox pad. */
const char *kDualSense = " DualSense Wireless Controller (AA:BB:CC:DD:EE:01)";
const char *kXbox = "Xbox Wireless Controller";

QLCIOPlugin *hidPlugin(Doc *doc)
{
    return doc->ioPluginCache()->plugin(QStringLiteral("HID"));
}

/* A stick value as the DS4's reading gives it: the HID plugin's own
   valueChanged() from the pad's line, below every layer of the app. */
bool padInput(Doc *doc, quint32 channel, uchar value, const char *pad = kDS4)
{
    QLCIOPlugin *hid = hidPlugin(doc);
    if (hid == nullptr)
        return false;
    const int line = hid->inputs().indexOf(QString::fromUtf8(pad));
    bool ok = false;
    QMetaObject::invokeMethod(hid, "injectVirtualInput", Qt::DirectConnection,
                              Q_RETURN_ARG(bool, ok), Q_ARG(quint32, quint32(line)),
                              Q_ARG(quint32, channel), Q_ARG(uint, uint(value)));
    return ok;
}

/* U01-Office's input as his show has it: a controller slot -- his
   DualSense, a wired DS4, the Bluetooth DS4 -- with the "Sony DualShock 4"
   input profile, the show's channels in the stock pad layout. */
void patchLikeHis(Doc *doc)
{
    QList<Universe::InputSlotDevice> devs;
    for (const char *uid : { " DualSense Wireless Controller (4C:B9:50:A9:F7:91)",
                             "Sony Interactive Entertainment Wireless Controller", kDS4 })
    {
        Universe::InputSlotDevice d;
        d.plugin = QStringLiteral("HID");
        d.uid = QString::fromUtf8(uid);
        d.profile = QStringLiteral("Sony DualShock 4");
        devs << d;
    }
    doc->inputOutputMap()->setInputSlot(0, devs, InputOutputMap::standardChannelRoles());
}

/* QLC_FSPOT_SHOTS=<dir>: the footer chip, offscreen, at the window's
   minimum, laptop and wide sizes (Q207 review pictures; not in the gate). */
void shootFooter(App *app, const QString &prefix)
{
    const QString dir = qEnvironmentVariable("QLC_FSPOT_SHOTS");
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);
    app->setTheme(App::ThemeVSCodeDark);
    const QList<QPair<QString, QSize>> sizes = { { "1024x640", QSize(1024, 640) }, { "1280x800", QSize(1280, 800) },
                                                 { "1920x1080", QSize(1920, 1080) } };
    for (const auto &sz : sizes)
    {
        app->resize(sz.second);
        settle(400);
        const QPixmap full = app->grab();
        full.save(QString("%1/%2-footer-%3-window.png").arg(dir, prefix, sz.first));
        const int h = 44;
        full.copy(0, full.height() - h * int(full.devicePixelRatio()), full.width(), h * int(full.devicePixelRatio()))
            .save(QString("%1/%2-footer-%3.png").arg(dir, prefix, sz.first));
        qInfo().noquote() << "shot" << prefix << sz.first << app->size() << app->followChip()->text();
    }
    app->resize(1280, 800);
    settle(200);
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
    /* The app's I/O plugins: the real HID plugin alone, with its stock
       profiles and his DS4 as a virtual pad -- no device of this machine's. */
    m_pluginDir = new QTemporaryDir();
    QVERIFY(m_pluginDir->isValid());
    const QFileInfo hid(QStringLiteral(QLC_HID_PLUGIN_FILE));
    QVERIFY2(hid.exists(), QLC_HID_PLUGIN_FILE);
    // A copy, not a link: on Windows QFile::link makes a .lnk shortcut, which
    // the plugin loader does not see as a plugin.
    QVERIFY(QFile::copy(hid.absoluteFilePath(), m_pluginDir->filePath(hid.fileName())));
    qputenv("QLC_IOPLUGIN_DIR", m_pluginDir->path().toLocal8Bit());
    qputenv("QLC_HID_PROFILE_DIR", QByteArray(QLC_TEST_RESOURCE_ROOT) + "hidprofiles");
    qputenv("QLC_HID_VIRTUAL_PAD", QByteArray("054c:09cc:") + kDS4 + ";054c:0ce6:" + kDualSense
                                   + ";045e:0b13:" + kXbox);
    qputenv("QLC_HID_VIRTUAL_ONLY", "1");

    m_app = new App();
    m_app->init();
    m_app->showNormal();
    m_app->resize(1280, 800);
    QVERIFY2(hidPlugin(m_app->doc()) != nullptr, "the HID plugin did not load");
    QVERIFY(hidPlugin(m_app->doc())->inputs().contains(QString::fromUtf8(kDS4)));
    m_app->doc()->inputOutputMap()->loadProfiles(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "inputprofiles"));
}

void FollowSpotCue_Test::cleanupTestCase()
{
    settle();
    m_app->doc()->resetModified();
    delete m_app;
    m_app = nullptr;
    delete m_pluginDir;
    m_pluginDir = nullptr;
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
    /* Opened on the Show Tree, as he works: the Build tab is not shown
       (before 0.4.0 that was the only place a stick was ever bound, so
       this way it said "no stick" and nothing moved). */
    ShowTreeView *view = showTree();
    QVERIFY(open(copy));
    Doc *doc = m_app->doc();
    ProgrammerController *pc = doc->programmer();
    const QList<quint32> spots = { 4, 142 };       // AT1-3z, AT2-3z
    const quint32 cue112 = 103, scene = 110, look = 14;
    QVERIFY(!ProgrammingManager::instance()->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(pc->isPanBound() && pc->isTiltBound(), 3000);
    QVERIFY(!ProgrammingManager::instance()->isVisible());

    view = showTree();
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
    /* Nothing bound by hand: his show's U01-Office slot takes his DS4 as
       it opens, and that is the stick. */
    QTRY_VERIFY_WITH_TIMEOUT(pc->isPanBound() && pc->isTiltBound(), 3000);
    QCOMPARE(pc->followSpotStickName(), QStringLiteral("DUALSHOCK 4"));
    QCOMPARE(doc->inputOutputMap()->inputPatch(0)->inputName(), QString::fromUtf8(kDS4));
    qInfo().noquote() << "his U01-Office input:" << doc->inputOutputMap()->inputPatch(0)->inputName()
                      << "profile" << doc->inputOutputMap()->inputPatch(0)->profileName()
                      << "stick" << pc->panAxisLabel() << "/" << pc->tiltAxisLabel();
    QVERIFY(padInput(doc, 0, 128));
    QVERIFY(padInput(doc, 1, 128));

    auto follows = [&](const QString &how) {
        QTRY_VERIFY2_WITH_TIMEOUT(pc->followSpotStatus().sceneId == scene, qPrintable(how), 3000);
        QVERIFY2(pc->followSpotStatus().live, qPrintable(how));
        QVERIFY(doc->effectScriptRunner()->isPlaying(look));
        const QString before = panTilt(doc, spots);
        StageTarget *witch = doc->monitorProperties()->stageTarget(pc->followSpotStatus().targetId);
        QVERIFY(witch != nullptr);
        const QVector3D from = witch->positionIn(doc->monitorProperties()->liveBlocking());
        QVERIFY(padInput(doc, 0, 30));          // the left stick, to the left
        QTest::qWait(1200);
        const QString moving = m_app->followChip()->text();
        QVERIFY(padInput(doc, 0, 128));
        const QString after = panTilt(doc, spots);
        const QVector3D to = witch->positionIn(doc->monitorProperties()->liveBlocking());
        qInfo().noquote() << how << before << "->" << after << "| ES-Witch x" << from.x() << "->" << to.x()
                          << "z" << from.z() << "->" << to.z() << "|" << moving;
        QVERIFY2(after != before, qPrintable(how + ": " + after));
        QVERIFY2(to.x() < from.x() - 0.2f, qPrintable(how + ": she did not move left"));
        QVERIFY2(qAbs(to.z() - from.z()) < 1e-4f, qPrintable(how + ": off her height"));
        QVERIFY2(moving.contains(QString::fromUtf8("AT-Spots ← ES-Witch · DUALSHOCK 4 · stick moving")),
                 qPrintable(moving));
        QTest::qWait(300);
    };
    tree->setCurrentItem(row);
    follows("selected");
    shootFooter(m_app, "after");

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
    /* His DS4 patched as U01-Office's input, as his show has it -- and
       nothing else: no stick bound by hand, the Build tab not shown (before
       0.4.0 only showing it bound a stick, so a show run from the Show Tree
       or Run said "no stick" and nothing moved). */
    showTree();
    pc->clearAxisBindings();
    QVERIFY(!pc->isPanBound());
    patchLikeHis(doc);
    QTRY_VERIFY_WITH_TIMEOUT(pc->isPanBound() && pc->isTiltBound(), 2000);
    QVERIFY(!ProgrammingManager::instance()->isVisible());
    QCOMPARE(doc->inputOutputMap()->inputPatch(0)->inputName(), QString::fromUtf8(kDS4));
    QCOMPARE(pc->followSpotStickName(), QStringLiteral("DUALSHOCK 4"));

    // The stick right: the subject walks right at her own height, every
    // head's pan follows, and the footer says who follows whom.
    auto movesWithTheStick = [&](const QString &how) {
        QTRY_VERIFY2_WITH_TIMEOUT(pc->followSpotStatus().state != ProgrammerController::FollowSpotStatus::Off,
                                  qPrintable(how + ": no follow spot on"), 3000);
        QVERIFY2(pc->followSpotStatus().live, qPrintable(how));
        QTRY_VERIFY2_WITH_TIMEOUT(chip->isVisible(), qPrintable(how), 2000);
        QVERIFY2(chip->text().contains(QString::fromUtf8("AT-Spots ← ES-Witch")), qPrintable(chip->text()));
        QVERIFY2(!chip->text().contains("no stick"), qPrintable(chip->text()));
        QVERIFY2(chip->text().contains(QString::fromUtf8("· DUALSHOCK 4 · stick")), qPrintable(chip->text()));
        const QVector3D from = witch->position();
        const QList<int> before = pans();
        QVERIFY(padInput(doc, 0, 255));        // the left stick, hard right
        // Held until she has walked, not for a fixed half second: how far she
        // gets in that time depends on the runner's frame rate.
        QTRY_VERIFY2_WITH_TIMEOUT(witch->position().x() > from.x() + 0.2f,
                                  qPrintable(QString("%1: the subject did not move (%2 -> %3)").arg(how)
                                             .arg(from.x()).arg(witch->position().x())), 8000);
        QTest::qWait(200);
        const QList<int> after = pans();
        QVERIFY2(chip->text().contains(QString::fromUtf8("DUALSHOCK 4 · stick moving")),
                 qPrintable(how + ": " + chip->text()));
        qInfo().noquote() << how << "pan" << before << "->" << after << "| x" << from.x() << "->"
                          << witch->position().x() << "|" << chip->text();
        QVERIFY(padInput(doc, 0, 128));
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

    // The DS4 switched off: no stick, and the chip says so.
    doc->inputOutputMap()->setInputSlot(0, QList<Universe::InputSlotDevice>());
    doc->inputOutputMap()->setInputPatch(0, KInputNone, QString(), QLCIOPlugin::invalidLine());
    tree->setCurrentItem(row);
    QTRY_VERIFY_WITH_TIMEOUT(chip->isVisible() && chip->text().contains("no stick"), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!pc->isPanBound(), 3000);
    tree->setCurrentItem(row1);
    QTRY_VERIFY_WITH_TIMEOUT((witch->position() - home).length() < 1e-3f, 3000);
    doc->resetModified();
}

/* Whatever pad is patched as an input is the stick, found when it is
   patched -- by a plain patch in Connections too, not only a controller
   slot -- and named on the chip: a DualSense, an Xbox pad, on any universe. */
void FollowSpotCue_Test::theStickIsWhicheverPadIsPatched()
{
    Doc *doc = m_app->doc();
    ProgrammerController *pc = doc->programmer();
    InputOutputMap *iom = doc->inputOutputMap();
    QLCIOPlugin *hid = hidPlugin(doc);
    QVERIFY(hid != nullptr);
    pc->clearAxisBindings();
    struct Pad { const char *uid; const char *name; quint32 universe; };
    for (const Pad &pad : { Pad { kDualSense, "DualSense", 1 }, Pad { kXbox, "Xbox", 2 },
                            Pad { kDS4, "DUALSHOCK 4", 0 } })
    {
        const int line = hid->inputs().indexOf(QString::fromUtf8(pad.uid));
        QVERIFY2(line >= 0, pad.uid);
        QVERIFY(iom->setInputPatch(pad.universe, QStringLiteral("HID"), QString(), quint32(line)));
        QTRY_VERIFY2_WITH_TIMEOUT(pc->followSpotStickName() == QLatin1String(pad.name),
                                  qPrintable(pc->followSpotStickName()), 2000);
        QCOMPARE(pc->panAxisLabel().contains(QStringLiteral("Left Stick X")), true);
        // Its stick reaches the follow spot: the value arrives as pan.
        QVERIFY(padInput(doc, 0, 230, pad.uid));
        QTRY_COMPARE_WITH_TIMEOUT(pc->m_panNorm, 230 / 255.0f, 2000);
        QVERIFY(padInput(doc, 0, 128, pad.uid));
        QTRY_COMPARE_WITH_TIMEOUT(pc->m_panNorm, 128 / 255.0f, 2000);
        // Unpatched: no stick (the next pad is the one).
        QVERIFY(iom->setInputPatch(pad.universe, KInputNone, QString(), QLCIOPlugin::invalidLine()));
        QTRY_VERIFY_WITH_TIMEOUT(!pc->isPanBound(), 2000);
    }
    m_app->doc()->resetModified();
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

/* "can we use the right stick for fine movement and the left for coarse
   ... or D-pad up/down to alter the speed?" On his DS4 as the HID plugin
   reads it: D-pad Up and Down step the follow spot's speed (x0.25 .. x2)
   and the footer says so; the right stick moves the subject finely; the
   look's Top Speed shows in Settings > Units. QLC_FSPOT_SHOTS=<dir> for
   the pictures (the chip and the setting, at three window sizes). */
void FollowSpotCue_Test::theDpadStepsTheSpeedAndTheRightStickIsFine()
{
    m_app->doc()->resetModified();
    m_app->clearDocument();
    settle(50);
    Doc *doc = m_app->doc();
    doc->effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    ProgrammerController *pc = doc->programmer();
    MonitorProperties *props = doc->monitorProperties();

    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Feel Mover");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("ch");
    const char *names[] = { "Pan", "Pan fine", "Tilt", "Tilt fine", "Dimmer" };
    QLCFixtureHead head;
    for (int k = 0; k < 5; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[k]);
        ch->setGroup(k == 4 ? QLCChannel::Intensity : k >= 2 ? QLCChannel::Tilt : QLCChannel::Pan);
        ch->setControlByte(k == 1 || k == 3 ? QLCChannel::LSB : QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, quint32(k));
        if (k < 4)
            head.addChannel(quint32(k));
    }
    mode->insertHead(-1, head);
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    def->addMode(mode);
    FixtureGroup *spots = new FixtureGroup(doc);
    spots->setName("FOH Spots");
    QVERIFY(doc->addFixtureGroup(spots));
    for (int i = 0; i < 2; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString("FOH%1").arg(i + 1));
        f->setFixtureDefinition(def, mode);
        f->setUniverse(0);
        f->setAddress(quint32(10 + i * 10));
        QVERIFY(doc->addFixture(f));
        props->setFixturePosition(f->id(), 0, 0, QVector3D(i == 0 ? -2 : 2, -6, 6));
        FixtureRigProps rp;
        rp.mountingType = Truss::TopHung;
        props->setFixtureRigProps(f->id(), rp);
        spots->assignFixture(f->id());
    }
    StageTarget *lead = props->addStageTarget();
    lead->setName("Lead");
    lead->setKind(StageTarget::Kind::Person);
    const QVector3D home(0.0f, 3.0f, 0.0f);
    lead->setPosition(home);
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setName("On Lead");
    aim->setStageTargetId(lead->id());
    QVERIFY(doc->addPalette(aim));
    QLCPalette *follow = new QLCPalette(QLCPalette::Effect, doc);
    follow->setName("Followspot");
    follow->setScriptPath(doc->effectScriptRunner()->cache()->scriptPath("followspot"));
    QVERIFY(doc->addPalette(follow));
    Scene *scene = new Scene(doc);
    scene->setName("Follow Lead");
    doc->addFunction(scene);
    scene->addFixtureGroup(spots->id());
    scene->addPalette(aim->id());
    scene->addPalette(follow->id());

    // His DS4 is the stick; the follow spot is open in Build's Workbench.
    pc->clearAxisBindings();
    patchLikeHis(doc);
    QTRY_VERIFY_WITH_TIMEOUT(pc->isPanBound() && pc->isTiltBound(), 2000);
    Workbench *bench = ProgrammingManager::instance()->workbench();
    m_app->m_tab->setCurrentWidget(ProgrammingManager::instance());
    bench->load(scene->id());
    settle(300);
    QLabel *chip = m_app->followChip();
    QTRY_VERIFY_WITH_TIMEOUT(chip->isVisible() && chip->text().contains(QString::fromUtf8("FOH Spots ← Lead")), 3000);
    QVERIFY2(!chip->text().contains("Speed"), qPrintable(chip->text()));     // x1: nothing said

    // The pad's controls by their roles, as his profile names them.
    InputPatch *ip = doc->inputOutputMap()->inputPatch(0);
    auto deviceChannel = [&](GamepadRole::Role want) {
        for (quint32 c = 0; c < 64; ++c)
            if (pc->controllerRole(0, c) == want)
                return int(ip->deviceChannel(c));
        return -1;
    };
    const int up = deviceChannel(GamepadRole::DpadUp), down = deviceChannel(GamepadRole::DpadDown);
    const int rightX = deviceChannel(GamepadRole::RightX);
    QVERIFY2(up >= 0 && down >= 0 && rightX >= 0, qPrintable(QString("%1 %2 %3").arg(up).arg(down).arg(rightX)));
    auto press = [&](int ch) {
        QVERIFY(padInput(doc, quint32(ch), 255));
        settle(30);
        QVERIFY(padInput(doc, quint32(ch), 0));
        settle(30);
    };

    // D-pad Up: x1.5, said on the chip (and in its tooltip).
    press(up);
    QCOMPARE(pc->followSpotMover()->multiplier(), 1.5);
    QTRY_VERIFY_WITH_TIMEOUT(chip->text().contains(QString::fromUtf8("Speed ×1.5")), 2000);
    QVERIFY2(chip->toolTip().contains("D-pad Up and Down"), qPrintable(chip->toolTip()));
    shootFooter(m_app, "speed-x1.5");
    press(up);
    QCOMPARE(pc->followSpotMover()->multiplier(), 2.0);
    press(up);
    QCOMPARE(pc->followSpotMover()->multiplier(), 2.0);           // the top step holds
    for (double want : { 1.5, 1.0, 0.5 })
    {
        press(down);
        QCOMPARE(pc->followSpotMover()->multiplier(), want);
    }
    QTRY_VERIFY_WITH_TIMEOUT(chip->text().contains(QString::fromUtf8("Speed ×0.5")), 2000);
    press(up);
    QCOMPARE(pc->followSpotMover()->multiplier(), 1.0);
    // Back at x1 it is said a moment, then no more.
    QVERIFY2(chip->text().contains(QString::fromUtf8("Speed ×1")), qPrintable(chip->text()));
    QTRY_VERIFY_WITH_TIMEOUT(!chip->text().contains("Speed"), 4000);

    // The right stick moves the subject, finely: slower than the left.
    auto walk = [&](int ch, uchar v) -> float {
        lead->setPosition(home);
        const QVector3D from = lead->position();
        if (!padInput(doc, quint32(ch), v))
            return -1.0f;
        QTest::qWait(600);
        const float x = lead->position().x() - from.x();
        padInput(doc, quint32(ch), 128);
        QTest::qWait(500);
        return x;
    };
    const float fine = walk(rightX, 255);
    const float coarse = walk(0, 255);
    qInfo() << "in 0.6 s: right stick" << fine << "m, left stick" << coarse << "m";
    QVERIFY2(fine > 0.02f, qPrintable(QString::number(fine)));
    QVERIFY2(fine < 0.35f * coarse, qPrintable(QString("%1 vs %2").arg(fine).arg(coarse)));

    // The setting, where the follow spot look is edited: Top Speed in feet a
    // second (the default unit), a brisk walk.
    bench->editPalette(follow->id());
    settle(300);
    QDoubleSpinBox *top = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((top = m_app->findChild<QDoubleSpinBox *>("effectParam_topSpeed")) != nullptr, 3000);
    QCOMPARE(top->text(), QString::fromUtf8("6 ft/s"));    // units-ok: the default unit's speed, as shown
    const QString shots = qEnvironmentVariable("QLC_FSPOT_SHOTS");
    if (!shots.isEmpty())
    {
        QDir().mkpath(shots);
        m_app->setTheme(App::ThemeVSCodeDark);
        for (const QSize &sz : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
        {
            m_app->resize(sz);
            settle(400);
            for (QWidget *p = top->parentWidget(); p != nullptr; p = p->parentWidget())
                if (QScrollArea *area = qobject_cast<QScrollArea *>(p))
                {
                    area->ensureWidgetVisible(top, 20, 60);
                    area->horizontalScrollBar()->setValue(0);   // the labels, not the fields' edge
                    break;
                }
            settle(200);
            m_app->grab().save(QString("%1/top-speed-%2x%3.png").arg(shots).arg(sz.width()).arg(sz.height()));
            qInfo().noquote() << "shot top speed" << m_app->size() << top->isVisible() << top->text();
        }
        m_app->resize(1280, 800);
        settle(200);
    }
    // A brisker walk, typed in metres a second: the subject goes that fast.
    top->setValue(top->valueFromText("3 m/s"));     // units-ok: typed with its own unit
    settle(100);
    QVERIFY(qAbs(follow->effectParamValues().value("topSpeed") - 300.0) < 0.5);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(pc->followSpotTopSpeed() - 3.0) < 0.01, 2000);
    bench->load(Function::invalidId());
    settle(100);
    doc->resetModified();
}

QTEST_MAIN(FollowSpotCue_Test)
