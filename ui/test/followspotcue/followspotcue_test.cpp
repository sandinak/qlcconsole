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
#include <QSettings>
#include <QPushButton>
#include <QFile>
#include <QDir>
#include <QElapsedTimer>
#include <QtTest>
#include <QScopeGuard>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#include "showtreeview.h"
#include "programmercontroller.h"
#include "effectscriptrunner.h"
#include "focussession.h"
#include "programmingmanager.h"
#include "controllertestdialog.h"
#include "workbench.h"
#include "inputdevicenotices.h"
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
#include "followspotoperator.h"
#include "monitor.h"
#include "structurestudioview.h"
#include <QDockWidget>
#include "measurementunits.h"
#include "followspotframing.h"
#include "paletteops.h"
#include "aimsolver.h"
#include <QPainter>
#include <QToolTip>
#include <QComboBox>
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
/* A worn DUALSHOCK 4 whose sticks do not come back to the middle, and one
   whose rest this computer remembers from before. */
const char *kWorn = " DUALSHOCK 4 Wireless Controller (AA:00:00:00:00:01)";
const char *kRemembered = " DUALSHOCK 4 Wireless Controller (AA:00:00:00:00:02)";

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
                                   + ";045e:0b13:" + kXbox + ";054c:09cc:" + kWorn
                                   + ";054c:09cc:" + kRemembered);
    /* Where the HID plugin remembers a stick's rest: a file of this test's,
       with one remembered for kRemembered at +0.12 / -0.05. */
    m_stickSettings = new QTemporaryDir();
    QVERIFY(m_stickSettings->isValid());
    const QString ini = m_stickSettings->filePath(QStringLiteral("sticks.ini"));
    qputenv("QLC_HID_STICK_SETTINGS", ini.toLocal8Bit());
    // A stick off the middle is said after 1.5 s here, not 3 s.
    qputenv("QLC_HID_STICK_DRIFT_MS", "1500");
    {
        QString key = QStringLiteral("054c:09cc:") + QString::fromUtf8(kRemembered).trimmed();
        for (QChar &c : key)
            if (!c.isLetterOrNumber())
                c = QLatin1Char('_');
        QSettings(ini, QSettings::IniFormat).setValue(QStringLiteral("hid/stickRest/") + key,
                                                      QStringList { "0=0.1200", "1=-0.0500" });
    }
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
    delete m_stickSettings;
    m_stickSettings = nullptr;
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
    const int kGlideMs = 15000;
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
        /* Every head's pan written, and the chip saying so, while the stick
           is still held: waited for, not a fixed wait. Both arrive on the
           follow spot's own ticks, which a loaded runner delays. */
        auto allFollowed = [&]() {
            const QList<int> now = pans();
            for (int i = 0; i < movers.size(); ++i)
                if (now.at(i) == before.at(i))
                    return false;
            return true;
        };
        QTRY_VERIFY2_WITH_TIMEOUT(allFollowed(), qPrintable(how + ": not every head followed"), 8000);
        const QList<int> after = pans();
        QTRY_VERIFY2_WITH_TIMEOUT(chip->text().contains(QString::fromUtf8("DUALSHOCK 4 · stick moving")),
                                  qPrintable(how + ": " + chip->text()), 8000);
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
        /* Let go: "moving" goes once the stick has been still 400 ms, said
           on the next follow-spot tick -- waited for, not a fixed 600 ms. */
        QTRY_VERIFY2_WITH_TIMEOUT(!chip->text().contains("moving"), qPrintable(how + ": " + chip->text()), 8000);
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

    /* Away from it: she glides back where the show has her, the chip goes.
       The glide steps one timer interval per follow-spot tick (not by the
       wall clock), so on a loaded runner it takes longer than its fade:
       waited for generously, still failing if she never gets home. */
    tree->setCurrentItem(row1);
    QTRY_VERIFY_WITH_TIMEOUT((witch->position() - home).length() < 1e-3f, kGlideMs);
    QTRY_VERIFY_WITH_TIMEOUT(chip->isHidden(), 5000);

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
    QTRY_VERIFY_WITH_TIMEOUT((witch->position() - home).length() < 1e-3f, kGlideMs);

    // GO in Operate.
    doc->setMode(Doc::Operate);
    settle(200);
    QVERIFY(m_app->runCommand(go, ShowControl::RouteRunTab));
    movesWithTheStick("GO in Operate");
    doc->masterTimer()->stopAllFunctions();
    doc->setMode(Doc::Design);
    settle(200);
    QTRY_VERIFY_WITH_TIMEOUT((witch->position() - home).length() < 1e-3f, kGlideMs);

    // The DS4 switched off: no stick, and the chip says so.
    doc->inputOutputMap()->setInputSlot(0, QList<Universe::InputSlotDevice>());
    doc->inputOutputMap()->setInputPatch(0, KInputNone, QString(), QLCIOPlugin::invalidLine());
    tree->setCurrentItem(row);
    QTRY_VERIFY_WITH_TIMEOUT(chip->isVisible() && chip->text().contains("no stick"), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!pc->isPanBound(), 3000);
    tree->setCurrentItem(row1);
    QTRY_VERIFY_WITH_TIMEOUT((witch->position() - home).length() < 1e-3f, kGlideMs);
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
    /* No pad left patched by an earlier test (the one before unpatches its
       DS4 only when it passes): each pad below must be the only one. */
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        iom->setInputSlot(u, QList<Universe::InputSlotDevice>());
        if (iom->inputPatch(u) != nullptr)
            QVERIFY(iom->setInputPatch(u, KInputNone, QString(), QLCIOPlugin::invalidLine()));
    }
    QTRY_VERIFY_WITH_TIMEOUT(!pc->isPanBound(), 2000);
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

namespace
{
// A -1..1 stick position as the pad's reading gives it (128 the middle).
uchar stickAt(double n)
{
    return uchar(n >= 0 ? 128 + qRound(n * 127) : 128 + qRound(n * 128));
}

QVariantMap stickAxis(QLCIOPlugin *hid, int line, int channel)
{
    for (const QVariant &v : hid->inputStickAxes(quint32(line)))
        if (v.toMap().value(QStringLiteral("channel")).toInt() == channel)
            return v.toMap();
    return QVariantMap();
}

// QLC_DRIFT_SHOTS=<dir>: @p w at the three review sizes (Q207; not in the gate).
void shootDrift(QWidget *w, const QString &name)
{
    const QString dir = qEnvironmentVariable("QLC_DRIFT_SHOTS");
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);
    const QSize was = w->size();
    for (const QSize &sz : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
    {
        w->resize(sz);
        settle(300);
        w->grab().save(QString("%1/%2-%3x%4.png").arg(dir, name).arg(sz.width()).arg(sz.height()));
    }
    w->resize(was);
    settle(100);
}
}

void FollowSpotCue_Test::aWornStickMovesNothing()
{
    Doc *doc = m_app->doc();
    ProgrammerController *pc = doc->programmer();
    InputOutputMap *iom = doc->inputOutputMap();
    QLCIOPlugin *hid = hidPlugin(doc);
    QVERIFY(hid != nullptr);
    const float mid = 128 / 255.0f;
    pc->clearAxisBindings();

    /* The symptom: a stick resting at +0.12 was a held push to every tool
       (the Focus mode stick and followspot.js alike), so heads crept. */
    QVERIFY(FocusSession::stickVelocity(stickAt(0.12) / 255.0f, 0.05f) != 0.0f);

    for (const char *uid : { kWorn, kXbox })
    {
        const QString how = QString::fromUtf8(uid).trimmed();
        const int line = hid->inputs().indexOf(QString::fromUtf8(uid));
        QVERIFY2(line >= 0, uid);
        // Connected: its sticks where a worn pad's rest, nobody touching it.
        QVERIFY(iom->setInputPatch(0, QStringLiteral("HID"), QString(), quint32(line)));
        QTRY_VERIFY_WITH_TIMEOUT(pc->isPanBound() && pc->isTiltBound(), 2000);
        QVERIFY(padInput(doc, 0, stickAt(0.12), uid));
        QVERIFY(padInput(doc, 1, stickAt(-0.07), uid));
        QVERIFY(padInput(doc, 3, stickAt(0.05), uid));
        QVERIFY(padInput(doc, 4, 128, uid));
        // A moment later its rest is learnt, and it reads the middle...
        QTRY_VERIFY2_WITH_TIMEOUT(stickAxis(hid, line, 0).value("learnt").toBool()
                                  && stickAxis(hid, line, 1).value("learnt").toBool(), qPrintable(how), 2000);
        QVERIFY2(qAbs(stickAxis(hid, line, 0).value("rest").toDouble() - 0.12) < 0.02, qPrintable(how));
        QVERIFY2(qAbs(stickAxis(hid, line, 1).value("rest").toDouble() + 0.07) < 0.02, qPrintable(how));
        QTRY_COMPARE_WITH_TIMEOUT(pc->m_panNorm, mid, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(pc->m_tiltNorm, mid, 1000);
        // ...and nothing moves, tick after tick (the Focus stick, the follow spot).
        for (int i = 0; i < 25; ++i)
        {
            QTest::qWait(40);
            QCOMPARE(pc->m_panNorm, mid);
            QCOMPARE(pc->m_tiltNorm, mid);
            QCOMPARE(FocusSession::stickVelocity(pc->m_panNorm, pc->joystickDeadzone()), 0.0f);
            QCOMPARE(FocusSession::stickVelocity(pc->m_tiltNorm, pc->joystickDeadzone()), 0.0f);
        }
        // Full travel still reaches both ends.
        QVERIFY(padInput(doc, 0, 255, uid));
        QTRY_COMPARE_WITH_TIMEOUT(pc->m_panNorm, 1.0f, 1000);
        QVERIFY(padInput(doc, 0, 0, uid));
        QTRY_COMPARE_WITH_TIMEOUT(pc->m_panNorm, 0.0f, 1000);
        QVERIFY(padInput(doc, 1, 255, uid));
        QTRY_COMPARE_WITH_TIMEOUT(pc->m_tiltNorm, 1.0f, 1000);
        QVERIFY(padInput(doc, 1, stickAt(-0.07), uid));
        QVERIFY(padInput(doc, 0, stickAt(0.12), uid));      // let go
        // A slow push, held still once the pad has settled in, is kept.
        QTest::qWait(2100);
        QVERIFY(padInput(doc, 0, stickAt(0.4), uid));
        QTRY_VERIFY2_WITH_TIMEOUT(pc->m_panNorm > 0.6f, qPrintable(QString::number(pc->m_panNorm)), 2000);
        QTest::qWait(600);                    // and still there after
        QVERIFY2(pc->m_panNorm > 0.6f, qPrintable(QString::number(pc->m_panNorm)));
        QVERIFY(FocusSession::stickVelocity(pc->m_panNorm, pc->joystickDeadzone()) > 0.0f);
        QVERIFY(padInput(doc, 0, stickAt(0.12), uid));
        QTRY_COMPARE_WITH_TIMEOUT(pc->m_panNorm, mid, 1000);
    }

    /* Later the worn stick comes to rest further out (+0.30): nothing
       re-centres it by itself, but once it has sat there 3 s (1.5 s in
       this test) with nothing
       else touched, the notice line says so -- once -- with Centre Sticks. */
    /* Said once a session per controller: on a loaded runner a step above
       (the slow push, full travel) can sit 1.5 s before the next and be
       said already -- right, but then this one never is (1 of 6 copies
       failed so under load). Not said yet, as this part starts. */
    QVERIFY(m_app->m_inputNotices != nullptr);
    m_app->m_inputNotices->m_driftSaid.clear();
    const int worn = hid->inputs().indexOf(QString::fromUtf8(kWorn));
    QVERIFY(iom->setInputPatch(0, QStringLiteral("HID"), QString(), quint32(worn)));
    QTRY_VERIFY_WITH_TIMEOUT(pc->isPanBound(), 2000);
    QTest::qWait(2200);                       // past the time it learns by itself
    m_app->hideViewNotice();
    QElapsedTimer pushed;
    pushed.start();
    QVERIFY(padInput(doc, 0, stickAt(0.42), kWorn));
    QTRY_VERIFY_WITH_TIMEOUT(pc->m_panNorm > 0.6f, 2000);           // not swallowed
    QTest::qWait(600);
    /* Not said early: only while the 1.5 s it must sit there can't have
       passed -- a loaded runner's 600 ms wait can be longer than that. */
    if (pushed.elapsed() < 1500)
        QVERIFY(!m_app->m_viewNotice->isVisible());
    /* Said once the plugin's stick timer sees it has sat 1.5 s: that tick
       and its queued signal come late on a loaded runner, so waited for
       generously (it failed so under load with 3 s). The notice stays 8 s. */
    QTRY_VERIFY_WITH_TIMEOUT(m_app->m_viewNotice->isVisible(), 15000);
    const QString said = m_app->m_viewNoticeText->text();
    qInfo().noquote() << "drift notice:" << said;
    QVERIFY2(said.contains(QStringLiteral("DUALSHOCK 4 Wireless Controller's stick isn't resting in the middle (Left Stick X +0.")),
             qPrintable(said));
    QVERIFY(m_app->m_viewNoticeButton->isVisible());
    QCOMPARE(m_app->m_viewNoticeButton->text(), QStringLiteral("Centre Sticks"));
    QVERIFY(pc->m_panNorm > 0.6f);           // still not re-centred by itself
    shootDrift(m_app, QStringLiteral("drift-notice"));

    // Centre Sticks: its rest is learnt again and it reads the middle.
    m_app->m_viewNoticeButton->click();
    QVERIFY2(m_app->m_viewNoticeText->text().contains(QStringLiteral("let go of them")),
             qPrintable(m_app->m_viewNoticeText->text()));
    QTRY_COMPARE_WITH_TIMEOUT(pc->m_panNorm, mid, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(m_app->m_viewNoticeText->text().contains(QStringLiteral("sticks are centred")), 2000);
    // Once a session: a stick off the middle again is not said again.
    QVERIFY(padInput(doc, 0, stickAt(-0.1), kWorn));
    QTest::qWait(2000);
    QVERIFY2(!m_app->m_viewNoticeText->text().contains(QStringLiteral("isn't resting")),
             qPrintable(m_app->m_viewNoticeText->text()));

    /* On the pad: View + LS (Create + L3 on this one), in any role. Its
       stick now rests at -0.20; nothing else happens on the press. */
    QVERIFY(padInput(doc, 0, stickAt(-0.2), kWorn));
    QTest::qWait(400);
    QVERIFY(pc->m_panNorm < 0.45f);
    const int view = 6 + 6, ls = 6 + 8;      // the axes, then View and LS
    QVERIFY(padInput(doc, quint32(view), 255, kWorn));
    settle(50);    // a press and its release are never in the same instant
    QVERIFY(padInput(doc, quint32(ls), 255, kWorn));
    // The press reaches the controller from the input thread, when it can.
    QTRY_VERIFY_WITH_TIMEOUT(pc->m_centreChord, 2000);
    QVERIFY(padInput(doc, quint32(ls), 0, kWorn));
    settle(50);
    QVERIFY(padInput(doc, quint32(view), 0, kWorn));
    QTRY_COMPARE_WITH_TIMEOUT(pc->m_panNorm, mid, 2000);
    QVERIFY(qAbs(stickAxis(hid, worn, 0).value("rest").toDouble() + 0.2) < 0.02);
    QVERIFY(!pc->m_centreChord);

    // The Controller Test window shows each stick: reads, rests at, deadzone, what the tools get.
    QVERIFY(padInput(doc, 0, stickAt(0.1), kWorn));
    ControllerTestDialog *test = ControllerTestDialog::open(doc, m_app);
    QVERIFY(test != nullptr);
    settle(300);
    QVERIFY2(test->stickRowCount() >= 4, qPrintable(QString::number(test->stickRowCount())));
    QTreeWidget *sticks = test->m_sticks;
    QTreeWidgetItem *lx = nullptr;
    for (int i = 0; i < sticks->topLevelItemCount(); ++i)
        if (sticks->topLevelItem(i)->text(0) == QString::fromUtf8(kWorn).trimmed()
            && sticks->topLevelItem(i)->text(1) == QStringLiteral("Left Stick X")
            && sticks->topLevelItem(i)->text(3) == QStringLiteral("-0.20"))
            lx = sticks->topLevelItem(i);
    QVERIFY(lx != nullptr);
    qInfo().noquote() << "sticks row:" << lx->text(0) << lx->text(1) << lx->text(2) << lx->text(3)
                      << lx->text(4) << lx->text(5);
    QCOMPARE(lx->text(2), QStringLiteral("+0.10"));
    QCOMPARE(lx->text(4), QStringLiteral("±0.16"));
    QVERIFY2(lx->text(5).startsWith(QStringLiteral("+0.2")), qPrintable(lx->text(5)));
    QVERIFY(test->m_centreButton->isEnabled());
    sticks->scrollToItem(lx);
    sticks->setCurrentItem(lx);
    shootDrift(test, QStringLiteral("controller-test-sticks"));
    QVERIFY(padInput(doc, 0, stickAt(0.1), kWorn));
    QCOMPARE(test->centreSticks() > 0, true);
    QTRY_VERIFY_WITH_TIMEOUT(test->m_sticksNote->text().contains(QStringLiteral("are centred")), 4000);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(stickAxis(hid, worn, 0).value("rest").toDouble() - 0.10) < 0.01, 4000);
    QTRY_COMPARE_WITH_TIMEOUT(pc->m_panNorm, mid, 1000);
    test->close();
    settle(100);

    /* A pad whose rest this computer remembers (+0.12 / -0.05): right from
       its first reading, before it could have learnt anything. */
    const int remembered = hid->inputs().indexOf(QString::fromUtf8(kRemembered));
    QVERIFY(remembered >= 0);
    QVERIFY(qAbs(stickAxis(hid, remembered, 0).value("rest").toDouble() - 0.12) < 1e-6);
    QVERIFY(iom->setInputPatch(0, QStringLiteral("HID"), QString(), quint32(remembered)));
    QTRY_VERIFY_WITH_TIMEOUT(pc->isPanBound(), 2000);
    QVERIFY(padInput(doc, 0, stickAt(0.12), kRemembered));
    QVERIFY(padInput(doc, 1, stickAt(-0.05), kRemembered));
    QTRY_COMPARE_WITH_TIMEOUT(pc->m_panNorm, mid, 200);
    QTRY_COMPARE_WITH_TIMEOUT(pc->m_tiltNorm, mid, 200);
    // And what is learnt is remembered for next time.
    const QSettings saved(qEnvironmentVariable("QLC_HID_STICK_SETTINGS"), QSettings::IniFormat);
    QString kept;
    for (const QString &k : saved.allKeys())
        if (k.contains(QStringLiteral("AA_00_00_00_00_01")))
            kept = saved.value(k).toStringList().join(',');
    // Its last Centre Sticks (the Controller Test's, at +0.10).
    QVERIFY2(kept.startsWith(QStringLiteral("0=0.10")), qPrintable(kept));

    QVERIFY(iom->setInputPatch(0, KInputNone, QString(), QLCIOPlugin::invalidLine()));
    m_app->hideViewNotice();
    m_app->doc()->resetModified();
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
    // Build is a tab of Everything (the qlcconsole view hides it, Q228).
    const App::WorkspaceView viewWas = m_app->workspaceView();
    m_app->setWorkspaceView(App::EverythingView, false);
    auto viewBack = qScopeGuard([this, viewWas]() { m_app->setWorkspaceView(viewWas, false); });
    m_app->showContext("Build");
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
    // 10 s: building the Look Editor took over 3 under load on a Pi 5.
    QTRY_VERIFY_WITH_TIMEOUT((top = m_app->findChild<QDoubleSpinBox *>("effectParam_topSpeed")) != nullptr, 10000);
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

/* A spot operator opens and closes the iris while following. On his DS4:
   D-pad Right and Left step the follow spot's size on the subject while it
   plays, the footer says it in Settings > Units (feet and inches), and
   Save Positions keeps it in the look; with no size set they step the
   beam angle instead. The Followspot editor offers the Framing.
   QLC_FSPOT_SHOTS=<dir>: the chip and the editor's rows at three sizes. */
void FollowSpotCue_Test::theDpadSizesTheBeamAndSavePositionsKeepsIt()
{
    m_app->doc()->resetModified();
    m_app->clearDocument();
    settle(50);
    Doc *doc = m_app->doc();
    doc->effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    ProgrammerController *pc = doc->programmer();
    MonitorProperties *props = doc->monitorProperties();
    QCOMPARE(int(MeasurementUnits::preferredLength()), int(MeasurementUnits::Feet));

    // Two zoom spots on a pipe out front, 6 m up, a 2-30 degree lens each.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Zoom Mover");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("ch");
    const char *names[] = { "Pan", "Pan fine", "Tilt", "Tilt fine", "Dimmer", "Zoom" };
    QLCFixtureHead head;
    for (int k = 0; k < 6; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[k]);
        if (k == 5)
        {
            ch->setGroup(QLCChannel::Beam);
            ch->setPreset(QLCChannel::BeamZoomSmallBig);
        }
        else
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
    ph.setLensDegreesMin(2);
    ph.setLensDegreesMax(30);
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
    lead->setPersonHeight(1.80f);
    lead->setPosition(QVector3D(0.0f, 14.0f, 0.0f));     // a long, flat throw
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setName("On Lead");
    aim->setStageTargetId(lead->id());
    QVERIFY(doc->addPalette(aim));
    QLCPalette *follow = new QLCPalette(QLCPalette::Effect, doc);
    follow->setName("Followspot");
    follow->setScriptPath(doc->effectScriptRunner()->cache()->scriptPath("followspot"));
    follow->setEffectParamValue("sizeOnSubject", 4.0 * 12.0 * 2.54);    // 4 ft on the subject
    QVERIFY(doc->addPalette(follow));
    Scene *scene = new Scene(doc);
    scene->setName("Follow Lead");
    doc->addFunction(scene);
    scene->addFixtureGroup(spots->id());
    scene->addPalette(aim->id());
    scene->addPalette(follow->id());

    pc->clearAxisBindings();
    patchLikeHis(doc);
    QTRY_VERIFY_WITH_TIMEOUT(pc->isPanBound() && pc->isTiltBound(), 2000);
    Workbench *bench = ProgrammingManager::instance()->workbench();
    // Build is a tab of Everything (the qlcconsole view hides it, Q228).
    const App::WorkspaceView viewWas = m_app->workspaceView();
    m_app->setWorkspaceView(App::EverythingView, false);
    auto viewBack = qScopeGuard([this, viewWas]() { m_app->setWorkspaceView(viewWas, false); });
    m_app->showContext("Build");
    bench->load(scene->id());
    settle(300);
    EffectScriptRunner *runner = doc->effectScriptRunner();
    QTRY_VERIFY_WITH_TIMEOUT(runner->isPlaying(follow->id()), 3000);
    QLabel *chip = m_app->followChip();
    QTRY_VERIFY_WITH_TIMEOUT(chip->isVisible() && chip->text().contains(QString::fromUtf8("FOH Spots ← Lead")), 3000);
    QVERIFY2(!chip->text().contains("Size"), qPrintable(chip->text()));    // the look's own: not said

    InputPatch *ip = doc->inputOutputMap()->inputPatch(0);
    auto deviceChannel = [&](GamepadRole::Role want) {
        for (quint32 c = 0; c < 64; ++c)
            if (pc->controllerRole(0, c) == want)
                return int(ip->deviceChannel(c));
        return -1;
    };
    const int right = deviceChannel(GamepadRole::DpadRight), left = deviceChannel(GamepadRole::DpadLeft);
    QVERIFY2(right >= 0 && left >= 0, qPrintable(QString("%1 %2").arg(right).arg(left)));
    auto press = [&](int ch) {
        QVERIFY(padInput(doc, quint32(ch), 255));
        settle(30);
        QVERIFY(padInput(doc, quint32(ch), 0));
        settle(30);
    };
    auto pad = [&]() {
        FollowSpotFraming::Pad p;
        runner->followSpotPad(follow->id(), p);
        return p;
    };
    // The first head's zoom as it goes out.
    Fixture *foh1 = doc->fixture(spots->fixtureList().first());
    auto zoomOut = [&]() {
        QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
        const int v = uchar(ua.first()->preGMValues().at(int(foh1->address()) + 5));
        doc->inputOutputMap()->releaseUniverses(false);
        return v;
    };
    // The look's own values, as the effect plays them.
    auto setLook = [&](const QString &name, double v) {
        follow->setEffectParamValue(name, v);
        runner->updateEffectParams(follow->id());
        settle(150);
    };
    QPushButton *save = m_app->findChild<QPushButton *>("savePositions");
    QVERIFY(save != nullptr);
    QTemporaryDir tmp;
    m_app->setFileName(tmp.filePath("follow.qxw"));
    /* What the chip and its tooltip said the moment the save was confirmed.
       "Saved: ..." is shown for three seconds by the wall clock and the save
       says it at once; read later, a loaded runner can be past those three
       seconds before the check runs (it failed so under load). */
    QString savedChip, savedStatus;
    auto savePositions = [&]() {
        QTRY_VERIFY_WITH_TIMEOUT(save->isEnabled(), 2000);    // the pad's change alone enables it
        save->click();                                        // armed
        save->click();                                        // confirmed
        savedChip = chip->text();
        savedStatus = pc->followSpotStatusText();
        settle(100);
    };
    const double inch = 2.54;

    // A Custom width: Right opens it a step, to a whole inch -- 4' to 4' 6",
    // in feet and inches -- for the run, not the look.
    press(right);
    QVERIFY2(qAbs(pad().widthCm - 54 * inch) < 0.01, qPrintable(QString::number(pad().widthCm)));
    QTRY_VERIFY_WITH_TIMEOUT(chip->text().contains(QString::fromUtf8("Width 4' 6\"").toHtmlEscaped()), 2000);
    QVERIFY2(chip->toolTip().contains("Left and Right"), qPrintable(chip->toolTip()));
    QCOMPARE(follow->effectParamValues().value("sizeOnSubject"), 48 * inch);
    QVERIFY(runner->paramOverrides(follow->id()).isEmpty());     // not an override (a VC store never takes it)
    shootFooter(m_app, "size-4ft6");
    press(left);
    press(left);
    QVERIFY2(qAbs(pad().widthCm - 43 * inch) < 0.01, qPrintable(QString::number(pad().widthCm)));
    QTRY_VERIFY_WITH_TIMEOUT(chip->text().contains(QString::fromUtf8("Width 3' 7\"").toHtmlEscaped()), 2000);

    // Save Positions -- the button, enabled by the pad's change alone --
    // keeps it in the look, as one undo step, and says so.
    savePositions();
    QVERIFY(qAbs(follow->effectParamValues().value("sizeOnSubject") - 43 * inch) < 0.01);
    QVERIFY(!pad().isSet());
    QVERIFY2(savedChip.contains(QString::fromUtf8("Saved: Width 3' 7\"").toHtmlEscaped()), qPrintable(savedChip));
    QVERIFY(doc->paletteOps()->undo());
    QCOMPARE(follow->effectParamValues().value("sizeOnSubject"), 48 * inch);

    // A Framing, from a head that comes in from above (level < 1): Left
    // closes it -- the zoom goes narrower, not wider -- ten points a step.
    setLook("framing", 1);
    QTRY_VERIFY_WITH_TIMEOUT(pc->followSpotStatus().framing == 1, 2000);
    {
        AimSolver::Frame frame;
        QVERIFY(AimSolver::frameFor(doc, foh1->id(), frame));
        const QVector3D to = props->targetAimPoint(lead, true) - frame.position;
        QVERIFY(std::hypot(double(to.x()), double(to.y())) / double(to.length()) < 0.99);
    }
    settle(300);
    const int framedZoom = zoomOut();
    press(left);
    QCOMPARE(pad().scale, 0.9);
    QTRY_VERIFY_WITH_TIMEOUT(zoomOut() < framedZoom, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(chip->text().contains(QString("Head & Shoulders 90%").toHtmlEscaped()), 2000);
    press(right);
    press(right);
    QCOMPARE(pad().scale, 1.1);
    QTRY_VERIFY_WITH_TIMEOUT(zoomOut() > framedZoom, 2000);
    // Where the scene aims the head now, live: the saved look must aim it
    // the same (it used to turn Custom and aim lower).
    auto aimedZ = [&]() {
        for (const AimSolver::SceneAim &a : AimSolver::sceneAims(doc, scene))
            if (a.fixtureId == foh1->id())
                return a.point.z();
        return -1.0f;
    };
    settle(300);
    const float liveZ = aimedZ();
    qInfo() << "framed aim live" << liveZ << "chest" << props->targetAimPoint(lead, true).z() << "zoom" << zoomOut();
    // A tight beam framed: over the chest, where an unframed look stays.
    QVERIFY2(liveZ > props->targetAimPoint(lead, true).z() + 0.01f,
             qPrintable(QString("%1 vs chest %2").arg(liveZ).arg(props->targetAimPoint(lead, true).z())));
    savePositions();
    QCOMPARE(follow->effectParamValues().value("framing"), 1.0);          // the Framing stays
    QCOMPARE(follow->effectParamValues().value("framingSize"), 110.0);
    QVERIFY2(savedChip.contains(QString("Saved: Head & Shoulders at 110%").toHtmlEscaped()), qPrintable(savedChip));
    QVERIFY2(savedStatus.contains("Saved: Head & Shoulders at 110%"), qPrintable(savedStatus));
    QVERIFY(AimSolver::sceneFramesSubject(doc, scene));
    settle(300);
    QVERIFY2(qAbs(aimedZ() - liveZ) < 0.005f,
             qPrintable(QString("saved, it aims at %1 m; live it was %2 m").arg(aimedZ()).arg(liveZ)));
    settle(2500);

    // At the lens's limit: the footer says so and the pad goes no further.
    setLook("framing", 0);
    {
        // A width a few steps short of the lens's widest (30 degrees) here.
        AimSolver::Frame frame;
        QVERIFY(AimSolver::frameFor(doc, foh1->id(), frame));
        const double dist = double((props->targetAimPoint(lead, true) - frame.position).length());
        setLook("sizeOnSubject", 100.0 * 2.0 * dist * std::tan(qDegreesToRadians(11.0)));
    }
    int presses = 0;
    while (!chip->text().contains("(widest)") && presses++ < 12)
    {
        press(right);
    }
    QVERIFY2(chip->text().contains("(widest)"), qPrintable(chip->text()));
    const double widest = pad().widthCm;
    press(right);
    QCOMPARE(pad().widthCm, widest);
    QVERIFY2(chip->text().contains("(widest)"), qPrintable(chip->text()));
    press(left);
    QVERIFY(pad().widthCm < widest);
    QTRY_VERIFY_WITH_TIMEOUT(!chip->text().contains("(widest)"), 2000);

    // No size: the beam angle, a whole degree a step, down to the lens's
    // narrowest (2 degrees) and no further.
    setLook("sizeOnSubject", 0.0);
    runner->setFollowSpotPad(follow->id(), FollowSpotFraming::Pad());
    presses = 0;
    while (!chip->text().contains("(narrowest)") && presses++ < 60)
        press(left);
    QVERIFY2(chip->text().contains(QString::fromUtf8("Beam 2° (narrowest)")), qPrintable(chip->text()));
    QCOMPARE(pad().beamDeg, 2.0);
    press(left);
    QCOMPARE(pad().beamDeg, 2.0);
    press(right);
    QCOMPARE(pad().beamDeg, 3.0);
    savePositions();
    QCOMPARE(follow->effectParamValues().value("beamAngle"), 3.0);

    // The editor: Framing, and the rows it decides greyed with the reason.
    bench->editPalette(follow->id());
    settle(300);
    if (LookEditor *le = bench->lookEditor())
        le->refreshEffectParams();      // an earlier test's look had this id
    settle(100);
    auto shown = [&](auto *dummy, const char *name) {
        using W = std::remove_pointer_t<decltype(dummy)>;
        W *found = nullptr;
        for (W *w : m_app->findChildren<W *>(name))
            if (w->isVisibleTo(m_app))
                found = w;
        return found;
    };
    QComboBox *framing = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((framing = shown((QComboBox *)nullptr, "effectParam_framing")) != nullptr, 10000);
    QStringList items;
    for (int i = 0; i < framing->count(); ++i)
        items << framing->itemText(i);
    QCOMPARE(items, QStringList({ "Custom", "Head & Shoulders", "Waist Up", "Full Body" }));
    QDoubleSpinBox *width = shown((QDoubleSpinBox *)nullptr, "effectParam_sizeOnSubject");
    QDoubleSpinBox *angle = shown((QDoubleSpinBox *)nullptr, "effectParam_beamAngle");
    QDoubleSpinBox *fsize = shown((QDoubleSpinBox *)nullptr, "effectParam_framingSize");
    QVERIFY(width != nullptr && angle != nullptr && fsize != nullptr);
    QCOMPARE(angle->text(), QString::fromUtf8("3.0°"));
    // Custom, no width: the angle counts, the Framing Size doesn't.
    QVERIFY(angle->isEnabled() && width->isEnabled() && !fsize->isEnabled());
    QVERIFY2(fsize->toolTip().startsWith("Not used with Framing: Custom"), qPrintable(fsize->toolTip()));
    framing->setCurrentIndex(2);    // waist up
    settle(100);
    QCOMPARE(follow->effectParamValues().value("framing"), 2.0);
    QVERIFY(!width->isEnabled() && !angle->isEnabled() && fsize->isEnabled());
    QVERIFY2(width->toolTip().startsWith("Set by Framing: Waist Up"), qPrintable(width->toolTip()));
    QVERIFY2(angle->toolTip().startsWith("Set by Framing: Waist Up"), qPrintable(angle->toolTip()));
    const QString shots = qEnvironmentVariable("QLC_FSPOT_SHOTS");
    if (!shots.isEmpty())
    {
        QDir().mkpath(shots);
        m_app->setTheme(App::ThemeVSCodeDark);
        for (const QSize &sz : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
        {
            m_app->resize(sz);
            settle(400);
            for (QWidget *p = framing->parentWidget(); p != nullptr; p = p->parentWidget())
                if (QScrollArea *area = qobject_cast<QScrollArea *>(p))
                {
                    area->ensureWidgetVisible(angle, 20, 60);
                    break;
                }
            settle(200);
            QPixmap pic = m_app->grab();
            pic.save(QString("%1/framing-rows-%2x%3.png").arg(shots).arg(sz.width()).arg(sz.height()));
            if (sz.width() == 1280)
            {
                // The greyed row with its tooltip, drawn where it shows.
                const QPoint at = width->mapTo(m_app, QPoint(width->width() / 2, width->height()));
                QToolTip::showText(width->mapToGlobal(QPoint(width->width() / 2, width->height())),
                                   width->toolTip(), width);
                settle(300);
                QPainter painter(&pic);
                for (QWidget *w : QApplication::topLevelWidgets())
                    if (w->inherits("QTipLabel") && w->isVisible())
                        painter.drawPixmap(qMin(at.x(), m_app->width() - w->width()), at.y() + 4, w->grab());
                painter.end();
                pic.save(QString("%1/framing-greyed-tooltip-1280x800.png").arg(shots));
                QToolTip::hideText();
            }
            qInfo().noquote() << "shot framing rows" << m_app->size() << framing->isVisible() << width->toolTip();
        }
        m_app->resize(1280, 800);
        settle(200);
    }

    // A running cue's follow spot (nothing in the Workbench): the pad sizes
    // it, and Save Positions keeps it in its look too.
    bench->clear();
    settle(100);
    setLook("framing", 3);
    if (!scene->isRunning())
        scene->start(doc->masterTimer(), FunctionParent::master());
    QTRY_VERIFY_WITH_TIMEOUT(pc->followSpotStatus().live && runner->isPlaying(follow->id()), 3000);
    press(right);
    QVERIFY(pad().isSet());
    QTRY_VERIFY_WITH_TIMEOUT(chip->text().contains("Full Body 120%"), 2000);
    savePositions();
    QCOMPARE(follow->effectParamValues().value("framingSize"), 120.0);   // 110% on the look, a step more
    QCOMPARE(follow->effectParamValues().value("framing"), 3.0);
    scene->stop(FunctionParent::master());
    settle(200);
    doc->resetModified();
}

/* A spot op in the SL tower: set on the Followspot editor's Operator row,
   or from the pad (hold LS, push toward centre stage, let go). The stick
   is turned to them -- pushed up, the subject goes the way they face --
   the footer says "from SL tower" and the Rig Overview has the operator's
   views. QLC_FSPOT_SHOTS=<dir>: before/ (front of house, as a show from
   before has it) and after/, the chip, the row and the rig view at the
   minimum, laptop and wide sizes. */
void FollowSpotCue_Test::theOperatorViewpointTurnsTheStick()
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
    def->setModel("Operator Mover");
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
    spots->setName("AT-Spots");
    QVERIFY(doc->addFixtureGroup(spots));
    const QVector3D centre = props->stageCentre();
    for (int i = 0; i < 2; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString("AT%1").arg(i + 1));
        f->setFixtureDefinition(def, mode);
        f->setUniverse(0);
        f->setAddress(quint32(10 + i * 10));
        QVERIFY(doc->addFixture(f));
        props->setFixturePosition(f->id(), 0, 0,
                                  QVector3D((centre.x() + (i == 0 ? -2.0f : 2.0f)) * 1000.0f, 0.0f, 6000.0f));
        FixtureRigProps rp;
        rp.mountingType = Truss::TopHung;
        props->setFixtureRigProps(f->id(), rp);
        spots->assignFixture(f->id());
    }
    StageTarget *witch = props->addStageTarget();
    witch->setName("ES-Witch");
    witch->setKind(StageTarget::Kind::Person);
    const QVector3D home(centre.x(), centre.y(), 0.0f);
    witch->setPosition(home);
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setName("On Witch");
    aim->setStageTargetId(witch->id());
    QVERIFY(doc->addPalette(aim));
    QLCPalette *follow = new QLCPalette(QLCPalette::Effect, doc);
    follow->setName("Followspot");
    follow->setScriptPath(doc->effectScriptRunner()->cache()->scriptPath("followspot"));
    QVERIFY(doc->addPalette(follow));
    Scene *scene = new Scene(doc);
    scene->setName("Follow Witch");
    doc->addFunction(scene);
    scene->addFixtureGroup(spots->id());
    scene->addPalette(aim->id());
    scene->addPalette(follow->id());

    pc->clearAxisBindings();
    patchLikeHis(doc);
    QTRY_VERIFY_WITH_TIMEOUT(pc->isPanBound() && pc->isTiltBound(), 2000);
    Workbench *bench = ProgrammingManager::instance()->workbench();
    // Build is a tab of Everything (the qlcconsole view hides it, Q228).
    const App::WorkspaceView viewWas = m_app->workspaceView();
    m_app->setWorkspaceView(App::EverythingView, false);
    auto viewBack = qScopeGuard([this, viewWas]() { m_app->setWorkspaceView(viewWas, false); });
    m_app->showContext("Build");
    bench->load(scene->id());
    settle(300);
    QLabel *chip = m_app->followChip();
    QTRY_VERIFY_WITH_TIMEOUT(chip->isVisible() && chip->text().contains(QString::fromUtf8("AT-Spots ← ES-Witch")),
                             3000);
    // Front of house, as every show from before: nothing said.
    QVERIFY(pc->followSpotOperator().isFrontOfHouse());
    QVERIFY2(!chip->text().contains("from "), qPrintable(chip->text()));

    // The Followspot editor's Operator row.
    bench->editPalette(follow->id());
    settle(300);
    if (LookEditor *le = bench->lookEditor())
        le->refreshEffectParams();
    settle(100);
    auto shown = [&](auto *dummy, const char *name) {
        using W = std::remove_pointer_t<decltype(dummy)>;
        W *found = nullptr;
        for (W *w : m_app->findChildren<W *>(name))
            if (w->isVisibleTo(m_app))
                found = w;
        return found;
    };
    QComboBox *where = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((where = shown((QComboBox *)nullptr, "followSpotOperatorPreset")) != nullptr, 10000);
    QComboBox *facing = shown((QComboBox *)nullptr, "followSpotOperatorFacing");
    QDoubleSpinBox *eye = shown((QDoubleSpinBox *)nullptr, "followSpotOperatorEye");
    QLabel *forWho = shown((QLabel *)nullptr, "followSpotOperatorFor");
    QVERIFY(facing != nullptr && eye != nullptr && forWho != nullptr);
    QCOMPARE(where->currentText(), QStringLiteral("Front of house"));
    QCOMPARE(facing->currentData().toInt(), 0);
    QCOMPARE(facing->count(), 24);
    // The eye height in Settings > Units (feet and inches by default).
    QCOMPARE(eye->text(), MeasurementUnits::formatLength(1600.0));
    QVERIFY2(forWho->text().contains("DUALSHOCK 4"), qPrintable(forWho->text()));

    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr);
    StructureStudioView *rig = mon->stageOverviewView(true);
    QVERIFY(rig != nullptr);
    QComboBox *rigView = rig->parentWidget()->findChild<QComboBox *>(QStringLiteral("rigView"));
    QVERIFY(rigView != nullptr);
    QDockWidget *rigDock = nullptr;
    for (QWidget *w = rig; w != nullptr && rigDock == nullptr; w = w->parentWidget())
        rigDock = qobject_cast<QDockWidget *>(w);
    QVERIFY(rigDock != nullptr);

    const QString shots = qEnvironmentVariable("QLC_FSPOT_SHOTS");
    auto shoot = [&](const QString &phase) {
        if (shots.isEmpty())
            return;
        const QString dir = shots + QLatin1Char('/') + phase;
        QDir().mkpath(dir);
        m_app->setTheme(App::ThemeVSCodeDark);
        const QList<QPair<QString, QSize>> sizes = { { "min", QSize(1024, 640) }, { "laptop", QSize(1280, 800) },
                                                     { "wide", QSize(1920, 1080) } };
        rigDock->hide();
        for (const auto &sz : sizes)
        {
            m_app->resize(sz.second);
            settle(400);
            for (QWidget *p = where->parentWidget(); p != nullptr; p = p->parentWidget())
                if (QScrollArea *area = qobject_cast<QScrollArea *>(p))
                {
                    area->ensureWidgetVisible(where, 20, 60);
                    area->horizontalScrollBar()->setValue(0);
                    break;
                }
            settle(200);
            const QPixmap full = m_app->grab();
            full.save(QString("%1/operator-setting-%2.png").arg(dir, sz.first));
            const int h = 44;
            full.copy(0, full.height() - h * int(full.devicePixelRatio()), full.width(),
                      h * int(full.devicePixelRatio()))
                .save(QString("%1/footer-chip-%2.png").arg(dir, sz.first));
            qInfo().noquote() << "shot" << phase << sz.first << chip->text();
        }
        rigDock->show();
        for (const auto &sz : sizes)
        {
            rigDock->resize(sz.second);
            settle(500);
            rigDock->grab().save(QString("%1/rig-view-%2.png").arg(dir, sz.first));
        }
        m_app->resize(1280, 800);
        settle(200);
    };
    shoot("before");

    // The SL tower, picked on the row: its facing, and the footer says so.
    where->setCurrentIndex(where->findData(int(FollowSpotOperator::StageLeftTower)));
    settle(100);
    QCOMPARE(pc->followSpotOperator().preset, FollowSpotOperator::StageLeftTower);
    QCOMPARE(pc->followSpotOperator().facing, 270);
    QCOMPARE(facing->currentData().toInt(), 270);
    QCOMPARE(pc->followSpotMover()->facing(), 270);
    QTRY_VERIFY_WITH_TIMEOUT(chip->text().contains("from SL tower"), 2000);
    QVERIFY(doc->isModified());

    // From the pad: hold LS, push up and a little left, let go.
    InputPatch *ip = doc->inputOutputMap()->inputPatch(0);
    auto deviceChannel = [&](GamepadRole::Role want) {
        for (quint32 c = 0; c < 64; ++c)
            if (pc->controllerRole(0, c) == want)
                return int(ip->deviceChannel(c));
        return -1;
    };
    const int ls = deviceChannel(GamepadRole::LS);
    QVERIFY(ls >= 0);
    QVERIFY(padInput(doc, quint32(ls), 255));
    // The press reaches the programmer through the input map: wait for it
    // (a fixed 30 ms was too short on a busy CI runner).
    QTRY_VERIFY_WITH_TIMEOUT(pc->isRecalibratingFacing(), 3000);
    /* On the screen, not only in the voice (0.4.4 on his DualSense: "nothing
       happened"): the footer and the look's line say the hold at once. */
    QTRY_VERIFY2_WITH_TIMEOUT(chip->text().contains("setting the facing"), qPrintable(chip->text()), 2000);
    QVERIFY2(chip->text().contains("let go of L3"), qPrintable(chip->text()));     // a PlayStation pad's word
    QVERIFY(pc->followSpotStatus().settingFacing);
    QVERIFY2(pc->followSpotStatusText().contains("setting the facing"), qPrintable(pc->followSpotStatusText()));
    const QVector3D before = witch->position();
    QVERIFY(padInput(doc, 0, 64));      // 30 degrees left of straight up
    QVERIFY(padInput(doc, 1, 17));
    QTest::qWait(400);
    QVERIFY2((witch->position() - before).length() < 1e-3f, "the subject moved while the facing was set");
    QVERIFY(padInput(doc, 0, 128));
    QVERIFY(padInput(doc, 1, 128));
    settle(60);
    QVERIFY(padInput(doc, quint32(ls), 0));
    QTRY_VERIFY_WITH_TIMEOUT(!pc->isRecalibratingFacing(), 3000);
    QCOMPARE(pc->followSpotOperator().facing, 300);       // 270 + 30
    QTRY_COMPARE_WITH_TIMEOUT(facing->currentData().toInt(), 300, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(chip->text().contains(QString::fromUtf8("from SL tower, facing 300°")), 2000);
    // ...and what it set, for a moment after.
    QVERIFY2(chip->text().contains(QString::fromUtf8("Facing 300°")), qPrintable(chip->text()));
    QVERIFY(!pc->followSpotStatus().settingFacing);

    /* Pushed the way they already face: kept, and said so -- not silence
       (from a tower, pushing straight at centre stage is the preset's own). */
    auto holdAndPush = [&](uchar x, uchar y) {
        QVERIFY(padInput(doc, quint32(ls), 255));
        QTRY_VERIFY_WITH_TIMEOUT(pc->isRecalibratingFacing(), 3000);
        QVERIFY(padInput(doc, 0, x));
        QVERIFY(padInput(doc, 1, y));
        settle(60);
        QVERIFY(padInput(doc, 0, 128));
        QVERIFY(padInput(doc, 1, 128));
        settle(60);
        QVERIFY(padInput(doc, quint32(ls), 0));
        QTRY_VERIFY_WITH_TIMEOUT(!pc->isRecalibratingFacing(), 3000);
    };
    holdAndPush(64, 17);
    QCOMPARE(pc->followSpotOperator().facing, 300);
    QTRY_VERIFY2_WITH_TIMEOUT(chip->text().contains(QString::fromUtf8("Facing 300°, as it was")),
                              qPrintable(chip->text()), 2000);
    // Too small a push: unchanged, and how to do it.
    holdAndPush(140, 116);
    QCOMPARE(pc->followSpotOperator().facing, 300);
    QTRY_VERIFY2_WITH_TIMEOUT(chip->text().contains("Facing unchanged: push a stick further while you hold L3"),
                              qPrintable(chip->text()), 2000);

    // Pushed up, the subject goes the way they face.
    witch->setPosition(home);
    QVERIFY(padInput(doc, 1, 0));
    QTest::qWait(600);
    QVERIFY(padInput(doc, 1, 128));
    QTest::qWait(300);
    const QVector3D moved = witch->position() - home;
    const double r = qDegreesToRadians(300.0);
    const QVector3D fwd(float(std::sin(r)), float(-std::cos(r)), 0.0f);
    QVERIFY2(moved.length() > 0.1f && QVector3D::dotProduct(moved.normalized(), fwd) > 0.98f,
             qPrintable(QString("%1, %2").arg(double(moved.x())).arg(double(moved.y()))));

    // The View list is in sight without scrolling the bar, at the smallest window.
    rigDock->show();
    rigDock->resize(1024, 640);
    settle(300);
    {
        const QRect at(rigView->mapTo(rigDock, QPoint(0, 0)), rigView->size());
        QVERIFY2(rigView->isVisible() && at.right() < rigDock->width() && at.left() >= 0,
                 qPrintable(QString("View at %1..%2 of %3").arg(at.left()).arg(at.right()).arg(rigDock->width())));
    }

    // The Rig Overview: from the operator, and the plan turned to them.
    const double az = rig->angledAzimuth(), el = rig->angledElevation();
    QVERIFY(!rig->operatorMark().on);
    const QImage pictureBefore = rig->grab().toImage();
    rigView->setCurrentIndex(rigView->findText("Operator"));
    settle(100);
    QCOMPARE(rig->plane(), StructureStudioView::Angled);
    QCOMPARE(rig->angledAzimuth(), 60.0);       // -300: from stage left, a little downstage
    QVERIFY(qAbs(rig->angledElevation() - pc->followSpotOperator().viewElevation(props)) < 1e-6);
    // The picture itself changed, and says whose view it is.
    QVERIFY(rig->grab().toImage() != pictureBefore);
    QVERIFY(rig->operatorMark().on);
    QCOMPARE(rig->operatorMark().facing, 300);
    QVERIFY2(rig->operatorMark().caption.contains(QString::fromUtf8("Operator: SL tower, facing 300°"))
                 && rig->operatorMark().caption.contains("DUALSHOCK 4"),
             qPrintable(rig->operatorMark().caption));
    QCOMPARE(rig->operatorMark().label, QString::fromUtf8("spot op · SL tower"));
    shoot("after");
    rigView->setCurrentIndex(rigView->findText("Operator plan"));
    settle(100);
    QCOMPARE(rig->plane(), StructureStudioView::Top);
    QCOMPARE(rig->rotation(), 1);   // stage left at the bottom
    QVERIFY(rig->operatorMark().on);
    if (!shots.isEmpty())
        for (const auto &sz : QList<QPair<QString, QSize>>({ { "min", QSize(1024, 640) },
                                                            { "laptop", QSize(1280, 800) },
                                                            { "wide", QSize(1920, 1080) } }))
        {
            rigDock->resize(sz.second);
            settle(500);
            rigDock->grab().save(QString("%1/after/rig-plan-%2.png").arg(shots, sz.first));
        }
    /* The pad asleep or unplugged (0.4.4: the Operator views showed front
       of house -- a plain plan -- once his DualSense had gone): its operator
       stays its own, said as not connected. */
    InputOutputMap *iom = doc->inputOutputMap();
    iom->setInputSlot(0, QList<Universe::InputSlotDevice>());
    QVERIFY(iom->setInputPatch(0, KInputNone, QString(), QLCIOPlugin::invalidLine()));
    QTRY_VERIFY_WITH_TIMEOUT(!pc->isPanBound() && !pc->isTiltBound(), 3000);
    QVERIFY(pc->followSpotStickName().isEmpty());
    QCOMPARE(pc->followSpotOperatorStick(), QStringLiteral("DUALSHOCK 4"));
    QCOMPARE(pc->followSpotOperator().preset, FollowSpotOperator::StageLeftTower);
    QCOMPARE(pc->followSpotOperator().facing, 300);
    settle(100);
    QCOMPARE(rig->rotation(), 1);
    QVERIFY2(rig->operatorMark().caption.contains("SL tower"), qPrintable(rig->operatorMark().caption));
    QTRY_VERIFY_WITH_TIMEOUT(shown((QLabel *)nullptr, "followSpotOperatorFor") != nullptr
                                 && shown((QLabel *)nullptr, "followSpotOperatorFor")
                                        ->text().contains("DUALSHOCK 4 (not connected)"),
                             2000);

    /* His pad, a DualSense, on its own: its stick click comes through the
       real HID plugin and its PlayStation profile as LS, and the hold sets
       the DualSense's own operator (front of house until now). */
    {
        QLCIOPlugin *hid = hidPlugin(doc);
        const int dsLine = hid->inputs().indexOf(QString::fromUtf8(kDualSense));
        QVERIFY(dsLine >= 0);
        QVERIFY(iom->setInputPatch(0, QStringLiteral("HID"), QString(), quint32(dsLine)));
        QTRY_VERIFY_WITH_TIMEOUT(pc->isPanBound() && pc->followSpotStickName() == QLatin1String("DualSense"), 3000);
        QVERIFY(pc->followSpotOperator().isFrontOfHouse());
        int dsLs = -1;
        for (quint32 c = 0; c < 64 && dsLs < 0; ++c)
            if (pc->controllerRole(0, c) == GamepadRole::LS)
                dsLs = int(iom->inputPatch(0)->deviceChannel(c));
        QVERIFY(dsLs >= 0);
        QTRY_VERIFY_WITH_TIMEOUT(pc->followSpotStatus().state != ProgrammerController::FollowSpotStatus::Off
                                     && pc->followSpotStatus().state
                                            != ProgrammerController::FollowSpotStatus::NoStick,
                                 3000);
        QVERIFY(padInput(doc, quint32(dsLs), 255, kDualSense));
        QTRY_VERIFY_WITH_TIMEOUT(pc->isRecalibratingFacing(), 3000);
        QTRY_VERIFY2_WITH_TIMEOUT(chip->text().contains("let go of L3"), qPrintable(chip->text()), 2000);
        QVERIFY(padInput(doc, 0, 64, kDualSense));
        QVERIFY(padInput(doc, 1, 17, kDualSense));
        settle(60);
        QVERIFY(padInput(doc, 0, 128, kDualSense));
        QVERIFY(padInput(doc, 1, 128, kDualSense));
        settle(60);
        QVERIFY(padInput(doc, quint32(dsLs), 0, kDualSense));
        QTRY_VERIFY_WITH_TIMEOUT(!pc->isRecalibratingFacing(), 3000);
        QCOMPARE(pc->followSpotOperator().facing, 30);      // front of house, 30 degrees left of up
        QTRY_VERIFY2_WITH_TIMEOUT(chip->text().contains(QString::fromUtf8("Facing 30°")), qPrintable(chip->text()),
                                  2000);
        pc->setFollowSpotOperator(FollowSpotOperator());
        QVERIFY(iom->setInputPatch(0, KInputNone, QString(), QLCIOPlugin::invalidLine()));
        QTRY_VERIFY_WITH_TIMEOUT(!pc->isPanBound(), 3000);
    }
    patchLikeHis(doc);
    QTRY_VERIFY_WITH_TIMEOUT(pc->isPanBound() && pc->isTiltBound(), 3000);
    QCOMPARE(pc->followSpotStickName(), QStringLiteral("DUALSHOCK 4"));
    QCOMPARE(pc->followSpotOperator().facing, 300);

    // Any other view: the angle and the turn it had, and no mark.
    rigView->setCurrentIndex(rigView->findData(int(StructureStudioView::Angled)));
    settle(100);
    QVERIFY(!rig->operatorMark().on);
    QCOMPARE(rig->rotation(), 0);
    QVERIFY(qAbs(rig->angledAzimuth() - az) < 1e-6 && qAbs(rig->angledElevation() - el) < 1e-6);
    rigDock->close();
    settle(100);

    // Back to front of house on the row: nothing said.
    where->setCurrentIndex(where->findData(int(FollowSpotOperator::FrontOfHouse)));
    settle(100);
    QVERIFY(pc->followSpotOperator().isFrontOfHouse());
    QTRY_VERIFY_WITH_TIMEOUT(!chip->text().contains("from "), 2000);
    bench->load(Function::invalidId());
    settle(100);
    doc->resetModified();
}

QTEST_MAIN(FollowSpotCue_Test)
