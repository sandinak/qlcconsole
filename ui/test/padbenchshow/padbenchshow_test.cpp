/*
  Q Light Controller Plus
  padbenchshow_test.cpp

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
#include <QFileInfo>
#include <QLabel>
#include <QFile>
#include <QDir>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#include "showtreeview.h"
#include "programmercontroller.h"
#include "markplanner.h"
#include "mastertimer.h"
#include "programmingmanager.h"
#include "showtreeview.h"
#include "workbenchstack.h"
#include "workbench.h"
#undef protected
#undef private
#include "showtreecontroller.h"
#include "programmercontroller.h"
#include "venuecorrections.h"
#include "cuelookahead.h"
#include "cueoutput.h"
#include "chaser.h"
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "shutterautoopen.h"
#include "monitorproperties.h"
#include "qlcfixturedefcache.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "inputpatch.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlccapability.h"
#include "fixturegroup.h"
#include "stagetarget.h"
#include "focuscalls.h"
#include "focussession.h"
#include "showcontrol.h"
#include "mastertimer.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "universe.h"
#include "fixture.h"
#include "truss.h"
#include "doc.h"

#include "padbenchshow_test.h"

using namespace ShowTree;

namespace
{

/* Every name the checklist uses: one place, so the build and the check
   cannot drift apart. */
const QString kProduction = QStringLiteral("Pad Bench");
const QString kMain = QStringLiteral("Main");
const QString kSecond = QStringLiteral("Second");
const QString kPreshow = QStringLiteral("Pre-show");
const QString kSongProduction = QStringLiteral("Song Bench");
const QString kSongList = QStringLiteral("Songs");
const QString kSong = QStringLiteral("Bench Song");
const QString kVenue = QStringLiteral("Bench Room");
const QString kPerson = QStringLiteral("Bench Person");
const QString kHit = QStringLiteral("Breathe");
const QString kSpot1 = QStringLiteral("Spot 1");
const QString kSpot2 = QStringLiteral("Spot 2");
const QString kShutter = QStringLiteral("Shutter Spot");
const QString kWash1 = QStringLiteral("Wash 1");
const QString kWash2 = QStringLiteral("Wash 2");
const QString kSpots = QStringLiteral("Spots");
const QString kWash = QStringLiteral("Wash");
const float kPersonHeight = 1.626f;     // 5' 4"

struct CueSpec { QString number; QString name; };
const QList<CueSpec> kMainCues = {
    { "1", "Follow Spot" }, { "2", "Two Layers" }, { "3", "Shutter Closed" },
    { "4", "Shutter Opens" }, { "5", "Free Fly" },
};
const QList<CueSpec> kSecondCues = { { "1", "Second A" }, { "2", "Second B" } };
const QList<CueSpec> kPreshowCues = { { "1", "Venue Focus" }, { "2", "Walk-in" } };
/* The song, numbered as a show gets after a few cues were put in between:
   Renumber Song makes it 1.1.1, 1.1.2, 1.1.3, 1.2.1, 1.2.2 (and moves the
   sub-cue of 1.2.3 with it). */
const QList<CueSpec> kSongCues = {
    { "1.1.2", "Verse In" }, { "1.1.5", "Verse Lift" }, { "1.1.5.5", "Verse Point" },
    { "1.2.3", "Chorus In" }, { "1.2.8", "Chorus Out" },
};
const QString kSubCue = QStringLiteral("Chorus Sting");

/* Where cue 4 puts Shutter Spot (raw degrees on its 630 x 270 yoke). */
const int kPosBPan = 405, kPosBTilt = 170;
/* Cue 3's hold before cue 4 follows on its own. */
const uint kShutterWaitMs = 8000;

/* Pads that are not there, read as real ones by the HID plugin. */
const char *kDS4 = " DUALSHOCK 4 Wireless Controller (AA:BB:CC:00:00:01)";
const char *kDualSense = " DualSense Wireless Controller (AA:BB:CC:00:00:02)";
const char *kXbox = "Xbox Wireless Controller";

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

Fixture *fixtureNamed(Doc *doc, const QString &name)
{
    for (Fixture *f : doc->fixtures())
        if (f->name() == name)
            return f;
    return nullptr;
}

FixtureGroup *groupNamed(Doc *doc, const QString &name)
{
    for (FixtureGroup *g : doc->fixtureGroups())
        if (g->name() == name)
            return g;
    return nullptr;
}

QLCPalette *paletteNamed(Doc *doc, const QString &name)
{
    for (QLCPalette *p : doc->palettes())
        if (p->name() == name)
            return p;
    return nullptr;
}

StageTarget *targetNamed(Doc *doc, const QString &name)
{
    for (StageTarget *t : doc->monitorProperties()->stageTargets())
        if (t->name() == name)
            return t;
    return nullptr;
}

/* What the rig sends now, after the universe's own steps (auto-open among
   them): fixture @a name's channel named @a channel; -1/-2 when missing. */
int out(Doc *doc, const QString &name, const QString &channel)
{
    Fixture *fx = fixtureNamed(doc, name);
    if (fx == nullptr)
        return -1;
    for (quint32 c = 0; c < fx->channels(); ++c)
        if (fx->channel(c)->name() == channel)
        {
            QList<Universe *> us = doc->inputOutputMap()->claimUniverses();
            const QByteArray *post = us.at(int(fx->universe()))->postGMValues();
            const int a = int(fx->address() + c);
            const int v = a < post->size() ? uchar(post->at(a)) : 0;
            doc->inputOutputMap()->releaseUniverses(false);
            return v;
        }
    return -2;
}

/* The DMX value Shutter Spot's pan MSB has at @a degrees. */
int panValue(Fixture *fx, int degrees)
{
    const quint32 pan = fx->channelNumber(QLCChannel::Pan, QLCChannel::MSB);
    for (const SceneValue &sv : fx->positionToValues(QLCChannel::Pan, degrees))
        if (sv.channel == pan)
            return sv.value;
    return -1;
}

QLCFixtureMode *modeNamed(QLCFixtureDef *def, const QString &name)
{
    if (def == nullptr)
        return nullptr;
    for (QLCFixtureMode *m : def->modes())
        if (m->name() == name)
            return m;
    return nullptr;
}

const CueList *listNamed(ShowTreeController *st, const QString &production, const QString &name)
{
    for (const Production &p : st->productions())
        if (p.name == production)
            for (const CueList &l : p.lists)
                if (l.name == name && !l.looks && !l.isSubCues())
                    return &l;
    return nullptr;
}

const Production *productionNamed(ShowTreeController *st, const QString &name)
{
    for (const Production &p : st->productions())
        if (p.name == name)
            return &p;
    return nullptr;
}

/* A virtual pad's value on line @a line: the HID plugin's own input. */
bool padInput(Doc *doc, quint32 line, quint32 channel, uchar value)
{
    QLCIOPlugin *hid = doc->ioPluginCache()->plugin(QStringLiteral("HID"));
    if (hid == nullptr)
        return false;
    bool ok = false;
    QMetaObject::invokeMethod(hid, "injectVirtualInput", Qt::DirectConnection,
                              Q_RETURN_ARG(bool, ok), Q_ARG(quint32, line),
                              Q_ARG(quint32, channel), Q_ARG(uint, uint(value)));
    return ok;
}

}

void PadBenchShow_Test::initTestCase()
{
    /* The app's I/O plugins: the real HID plugin alone, with its stock
       profiles and three virtual pads -- no device of this machine's. */
    m_pluginDir = new QTemporaryDir();
    QVERIFY(m_pluginDir->isValid());
    const QFileInfo hid(QStringLiteral(QLC_HID_PLUGIN_FILE));
    QVERIFY2(hid.exists(), QLC_HID_PLUGIN_FILE);
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
    Doc *doc = m_app->doc();
    QVERIFY2(doc->ioPluginCache()->plugin(QStringLiteral("HID")) != nullptr, "the HID plugin did not load");
    doc->inputOutputMap()->loadProfiles(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "inputprofiles"));
    doc->effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    QVERIFY(doc->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures")));
}

void PadBenchShow_Test::cleanupTestCase()
{
    settle();
    m_app->doc()->masterTimer()->stopAllFunctions();
    m_app->doc()->setMode(Doc::Design);
    settle(100);
    m_app->doc()->resetModified();
    delete m_app;
    m_app = nullptr;
    delete m_pluginDir;
    m_pluginDir = nullptr;
}

bool PadBenchShow_Test::open(const QString &path)
{
    m_app->doc()->masterTimer()->stopAllFunctions();
    m_app->doc()->setMode(Doc::Design);
    settle(50);
    m_app->doc()->resetModified();
    m_app->clearDocument();
    const bool ok = m_app->loadXML(path) == QFile::NoError;
    m_app->doc()->resetModified();
    settle(200);
    return ok;
}

/****************************************************************************
 * The build
 ****************************************************************************/

void PadBenchShow_Test::build(const QString &path)
{
    m_app->doc()->masterTimer()->stopAllFunctions();
    m_app->doc()->setMode(Doc::Design);
    m_app->doc()->resetModified();
    m_app->clearDocument();
    settle(50);
    Doc *doc = m_app->doc();
    MonitorProperties *mp = doc->monitorProperties();
    QLCFixtureDefCache *cache = doc->fixtureDefCache();
    EffectScriptCache *scripts = doc->effectScriptRunner()->cache();
    ShowTreeController *st = doc->showTree();
    ProgrammerController *pc = doc->programmer();

    // ---- the stage: 24 ft wide, 20 ft deep, 14 ft to the grid; X stage
    // left, Y downstage, Z up, metres. A flown truss just inside the
    // downstage edge (front of house for the bench) carries the whole rig.
    mp->setGridUnits(MonitorProperties::Feet);
    mp->setGridSize(QVector3D(24, 14, 20));
    const QVector3D centre = mp->stageCentre();
    const float depth = 20 * 0.3048f;
    Truss *truss = mp->addTruss();
    truss->setName(QStringLiteral("FOH Truss"));
    truss->setType(Truss::Horizontal);
    truss->setProfile(Truss::SquareTruss);
    truss->setWidth(0.29f);
    truss->setOrigin(QVector3D(centre.x() - 3.0f, depth - 0.3f, 4.5f));
    truss->setDirection(QPointF(1, 0));
    truss->setLength(6.0f);
    mp->recomputeChildTrusses();

    // ---- fixtures, stock definitions only
    struct Rig { QString name, maker, model, mode; float along; };
    const QList<Rig> rig = {
        { kSpot1, "Martin", "MAC Encore Performance", "CLD", 1.5f },
        { kSpot2, "Martin", "MAC Encore Performance", "CLD", 4.5f },
        { kShutter, "American DJ", "Focus Spot Three Z", "21 Channel", 3.0f },
        { kWash1, "Chauvet", "SlimPAR 56", "7-Ch", 0.4f },
        { kWash2, "Chauvet", "SlimPAR 56", "7-Ch", 5.6f },
    };
    quint32 address = 0;
    for (const Rig &r : rig)
    {
        QLCFixtureDef *def = cache->fixtureDef(r.maker, r.model);
        QVERIFY2(def != nullptr, qPrintable(r.maker + " " + r.model));
        QLCFixtureMode *mode = modeNamed(def, r.mode);
        QVERIFY2(mode != nullptr, qPrintable(r.model + " " + r.mode));
        Fixture *f = new Fixture(doc);
        f->setName(r.name);
        f->setFixtureDefinition(def, mode);
        f->setUniverse(0);
        f->setAddress(address);
        QVERIFY2(doc->addFixture(f), qPrintable(r.name));
        address += quint32(mode->channels().count());

        // Hung under the truss, facing upstage at the stage.
        FixtureRigProps rp = mp->fixtureRigProps(f->id());
        rp.clearMounts();
        rp.trussId = truss->id();
        rp.trussOffset = r.along;
        rp.trussMountSide = FixtureRigProps::UnderHung;
        rp.mountingType = Truss::TopHung;
        rp.panZeroDir = 0.0f;
        mp->setFixtureRigProps(f->id(), rp);
        const QVector3D at = truss->positionAt(r.along);
        mp->setFixturePosition(f->id(), 0, 0, QVector3D(at.x() * 1000.0f, at.y() * 1000.0f, 0));
    }

    for (const QPair<QString, QStringList> &g : QList<QPair<QString, QStringList>>{
             { kSpots, { kSpot1, kSpot2 } }, { kWash, { kWash1, kWash2 } } })
    {
        FixtureGroup *grp = new FixtureGroup(doc);
        grp->setName(g.first);
        QVERIFY(doc->addFixtureGroup(grp));
        for (const QString &n : g.second)
            grp->assignFixture(fixtureNamed(doc, n)->id());
    }

    // ---- the person, centre stage
    StageTarget *person = mp->addStageTarget();
    person->setName(kPerson);
    person->setKind(StageTarget::Kind::Person);
    person->setPosition(QVector3D(centre.x(), centre.y(), 0));
    person->setPersonHeight(kPersonHeight);
    person->setColor(QColor("#ffb400"));
    mp->notifyStageTargetsChanged();

    // ---- universes: the rig's sends nowhere (preview only); the pad's is
    // a controller slot that takes whichever game controller is here.
    InputOutputMap *iom = doc->inputOutputMap();
    while (iom->universesCount() < 2)
        QVERIFY(iom->addUniverse());
    iom->setUniverseName(0, QStringLiteral("U01-Bench"));
    iom->setUniverseName(1, QStringLiteral("U02-Pad"));
    for (quint32 u = 0; u < iom->universesCount(); ++u)
        QCOMPARE(iom->outputPatchesCount(u), 0);

    // ---- palettes
    auto palette = [&](QLCPalette::PaletteType type, const QString &name) {
        QLCPalette *p = new QLCPalette(type, doc);
        p->setName(name);
        doc->addPalette(p);
        return p;
    };
    QLCPalette *full = palette(QLCPalette::Dimmer, QStringLiteral("Full"));
    full->setValue(255);
    QLCPalette *wash70 = palette(QLCPalette::Dimmer, QStringLiteral("Wash 70"));
    wash70->setValue(178);
    QLCPalette *half = palette(QLCPalette::Dimmer, QStringLiteral("Half"));
    half->setValue(128);
    QLCPalette *closed = palette(QLCPalette::Shutter, QStringLiteral("Shutter Closed"));
    closed->setValue(3);            // the 3Z's "Off" (0-7, ShutterClose)
    QLCPalette *opened = palette(QLCPalette::Shutter, QStringLiteral("Shutter Open"));
    QLCPalette *posB = palette(QLCPalette::PanTilt, QStringLiteral("Pos B"));
    posB->setValue(kPosBPan, kPosBTilt);
    QLCPalette *follow = palette(QLCPalette::Effect, QStringLiteral("Followspot"));
    follow->setScriptPath(scripts->scriptPath(QStringLiteral("followspot")));
    QVERIFY(!follow->scriptPath().isEmpty());
    follow->setEffectParamValues({ { QStringLiteral("framing"), 2.0 } });   // Waist Up
    QLCPalette *freeFly = palette(QLCPalette::Effect, QStringLiteral("Followspot Free Fly"));
    freeFly->setScriptPath(scripts->scriptPath(QStringLiteral("followspot")));
    QLCPalette *breathe = palette(QLCPalette::Effect, kHit);
    breathe->setScriptPath(scripts->scriptPath(QStringLiteral("dimmer-phaser")));
    QVERIFY(!breathe->scriptPath().isEmpty());
    breathe->setEffectParamValues({ { QStringLiteral("rate"), 0.5 }, { QStringLiteral("minDim"), 0.1 } });
    ShowTreeView *view = ShowTreeView::instance();
    QVERIFY(view != nullptr);
    for (const QString cmd : { "palette colour \"Blue\" #0000ff", "palette colour \"Red\" #ff0000",
                               "palette colour \"Amber\" #ffb000" })
        QVERIFY2(view->runCommand(cmd), qPrintable(cmd + ": " + view->commandMessage()));
    QLCPalette *blue = paletteNamed(doc, QStringLiteral("Blue"));
    QLCPalette *red = paletteNamed(doc, QStringLiteral("Red"));
    QLCPalette *amber = paletteNamed(doc, QStringLiteral("Amber"));
    QVERIFY(blue != nullptr && red != nullptr && amber != nullptr);
    const quint32 aim = st->aimPaletteFor(person->id());
    QVERIFY(doc->palette(aim) != nullptr);

    // ---- the focus calls: one light at a time, on the person
    FocusCalls *calls = pc->focusCalls();
    calls->clear();
    for (const QString &n : { kSpot1, kSpot2 })
    {
        FocusCall c;
        c.name = QStringLiteral("%1 on %2").arg(n, kPerson);
        c.fixtures << fixtureNamed(doc, n)->id();
        c.target.kind = FocusSession::Reference::Target;
        c.target.id = person->id();
        calls->append(c);
    }

    // ---- the production
    Target spots, wash, shutter;
    spots.groups << groupNamed(doc, kSpots)->id();
    wash.groups << groupNamed(doc, kWash)->id();
    shutter.fixtures << fixtureNamed(doc, kShutter)->id();
    auto look = [&](quint32 cue, quint32 layer, QLCPalette *p, const QString &effectKind = QString()) {
        Look l = st->lookForPalette(p->id());
        if (!effectKind.isEmpty())
            l.key = effectKey(effectKind);
        QVERIFY2(st->setLayerLook(cue, layer, l), qPrintable(p->name()));
    };
    auto addCues = [&](quint32 list, const QList<CueSpec> &specs) {
        QList<quint32> ids;
        for (int i = 0; i < specs.count(); ++i)
        {
            const quint32 id = st->addCue(list, i - 1, specs.at(i).name);
            Cue c = *st->cue(id);
            c.number = specs.at(i).number;
            QString why;
            [&]() { QVERIFY2(st->setCue(c, &why), qPrintable(specs.at(i).number + ": " + why)); }();
            ids << id;
        }
        return ids;
    };

    const quint32 prod = st->addProduction(kProduction);
    QCOMPARE(st->currentProduction(), prod);
    const quint32 mainList = st->addCueList(prod, kMain, ListMode::NonTracking);
    const quint32 secondList = st->addCueList(prod, kSecond, ListMode::NonTracking);
    const quint32 preList = st->addCueList(prod, kPreshow, ListMode::NonTracking);
    QVERIFY(st->setMainList(mainList));

    const QList<quint32> m = addCues(mainList, kMainCues);
    {   // 1 Follow Spot: the Spots follow the person, waist up
        const quint32 l = st->addLayer(m[0], spots);
        look(m[0], l, full);
        look(m[0], l, doc->palette(aim));
        look(m[0], l, follow, QStringLiteral("Position"));
    }
    {   // 2 Two Layers: the Spots and the Wash
        const quint32 l1 = st->addLayer(m[1], spots);
        look(m[1], l1, full);
        look(m[1], l1, amber);
        look(m[1], l1, doc->palette(aim));
        const quint32 l2 = st->addLayer(m[1], wash);
        look(m[1], l2, wash70);
        look(m[1], l2, blue);
    }
    {   /* 3 Shutter Closed: dimmer full behind a closed shutter, at the
           head's rest (no position of its own: move in black only sets
           what nothing else sets, so a position here would hold it) */
        const quint32 l = st->addLayer(m[2], shutter);
        look(m[2], l, full);
        look(m[2], l, closed);
        /* The wait gap: cue 3 holds 8 s, then cue 4 goes by itself. Move in
           black looks ahead only to a cue that comes on a clock; to a cue
           that waits for GO it sends nothing (CueLookahead reads the
           infinite hold as "now", inside the lead time). */
        Cue c = *st->cue(m[2]);
        c.hold = kShutterWaitMs;
        QVERIFY(st->setCue(c));
    }
    {   // 4 Shutter Opens: at B, open; move in black takes it there in 3
        const quint32 l = st->addLayer(m[3], shutter);
        look(m[3], l, full);
        look(m[3], l, opened);
        look(m[3], l, posB);
        Cue c = *st->cue(m[3]);
        c.fadeIn = 2000;
        QVERIFY(st->setCue(c));
    }
    {   // 5 Free Fly: the Followspot effect with no Aim look
        const quint32 l = st->addLayer(m[4], spots);
        look(m[4], l, full);
        look(m[4], l, freeFly, QStringLiteral("Position"));
    }
    // The hit under cue 1: the Wash breathes.
    {
        const quint32 hit = st->addHit(HitScope::Cue, m[0], kHit);
        QVERIFY(hit != invalidId());
        const quint32 l = st->addLayer(hit, wash);
        look(hit, l, full);
        look(hit, l, red);
        look(hit, l, breathe, QStringLiteral("Dimmer"));
        Cue h = *st->cue(hit);
        h.fadeIn = 500;
        h.fadeOut = 500;
        QVERIFY(st->setCue(h));
    }

    const QList<quint32> s = addCues(secondList, kSecondCues);
    for (int i = 0; i < s.count(); ++i)
    {
        const quint32 l = st->addLayer(s[i], wash);
        look(s[i], l, full);
        look(s[i], l, i == 0 ? red : blue);
    }

    const QList<quint32> p = addCues(preList, kPreshowCues);
    {   // Venue Focus: the Spots at half on the person, the calls armed
        const quint32 l = st->addLayer(p[0], spots);
        look(p[0], l, half);
        look(p[0], l, doc->palette(aim));
        QVERIFY(st->setCuesFocusCall({ p[0] }, true));
        const quint32 l2 = st->addLayer(p[1], wash);
        look(p[1], l2, half);
        look(p[1], l2, amber);
    }

    // Move in black: on for the show.
    pc->setAutoMoveInBlack(true);

    // ---- the song, in a production of its own numbered Show.Part.Iteration
    // (Pad Bench keeps the app's default numbering, which has no songs).
    const quint32 songProd = st->addProduction(kSongProduction);
    QVERIFY(st->setNumbering(songProd, Numbering::ShowPartIteration));
    const quint32 songList = st->addCueList(songProd, kSongList, ListMode::Tracking);
    QVERIFY(st->setMainList(songList));
    const QList<quint32> songCues = addCues(songList, kSongCues);
    for (int i = 0; i < songCues.count(); ++i)
    {
        const quint32 l = st->addLayer(songCues[i], wash);
        look(songCues[i], l, i % 2 == 0 ? full : half);
        look(songCues[i], l, i < 3 ? amber : red);
    }
    QString why;
    QVERIFY2(st->addSubCue(songCues[3], kSubCue, invalidId(), &why) != invalidId(), qPrintable(why));
    QList<Section> secs = st->sections(songList);
    QCOMPARE(secs.count(), 1);
    QVERIFY(st->setSectionName(songList, secs.first(), kSong));
    QCOMPARE(st->currentProduction(), prod);

    // ---- the pad's universe: whichever game controller is connected.
    // Set on the universe as the show file holds it: these virtual pads
    // must not be written into the show as its input.
    Universe::InputSlotDevice any;
    any.plugin = QStringLiteral("HID");
    any.anyGamepad = true;
    Universe *padUni = iom->universe(1);
    QVERIFY(padUni != nullptr);
    padUni->setInputSlot({ any });
    padUni->setInputSlotLive(-1);
    padUni->setInputSlotRoles(InputOutputMap::standardChannelRoles());

    QCOMPARE(m_app->saveXML(path), QFile::NoError);

    // The venue, applied, in the file beside the show.
    VenueCorrections *venues = pc->venueCorrections();
    QCOMPARE(venues->addVenue(kVenue), kVenue);
    QVERIFY(venues->setActiveVenue(kVenue));
    QVERIFY2(venues->save(&why), qPrintable(why));
    QVERIFY(QFileInfo::exists(VenueCorrections::sidecarPath(path)));
    doc->resetModified();

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString xml = QString::fromUtf8(f.readAll());
    QVERIFY2(!xml.contains(QStringLiteral("Wireless Controller")), "a virtual pad was saved into the show");
    QVERIFY(xml.contains(QStringLiteral("<InputSlot")));
}

/****************************************************************************
 * The check
 ****************************************************************************/

void PadBenchShow_Test::check(const QString &path)
{
    QVERIFY(open(path));
    Doc *doc = m_app->doc();
    MonitorProperties *mp = doc->monitorProperties();
    ShowTreeController *st = doc->showTree();
    ProgrammerController *pc = doc->programmer();
    InputOutputMap *iom = doc->inputOutputMap();

    // The rig.
    const QHash<QString, QString> models = {
        { kSpot1, "Martin|MAC Encore Performance|CLD" }, { kSpot2, "Martin|MAC Encore Performance|CLD" },
        { kShutter, "American DJ|Focus Spot Three Z|21 Channel" },
        { kWash1, "Chauvet|SlimPAR 56|7-Ch" }, { kWash2, "Chauvet|SlimPAR 56|7-Ch" },
    };
    QCOMPARE(doc->fixtures().count(), models.count());
    for (Fixture *fx : doc->fixtures())
    {
        QVERIFY2(models.contains(fx->name()), qPrintable(fx->name()));
        QVERIFY2(fx->fixtureDef() != nullptr && fx->fixtureMode() != nullptr, qPrintable(fx->name()));
        QCOMPARE(QStringList({ fx->fixtureDef()->manufacturer(), fx->fixtureDef()->model(),
                               fx->fixtureMode()->name() }).join('|'), models.value(fx->name()));
        QCOMPARE(fx->universe(), quint32(0));
        qInfo().noquote() << "[FIXTURE]" << fx->name() << "DMX" << fx->address() + 1 << models.value(fx->name())
                          << "at" << mp->fixtureRigPosition(fx->id());
    }
    // The Spots zoom and have an iris; the 3Z's shutter closes.
    for (const QString &n : { kSpot1, kSpot2 })
    {
        QVERIFY(fixtureNamed(doc, n) != nullptr);
        QVERIFY(out(doc, n, "Iris") >= 0);
        QVERIFY(out(doc, n, "Zoom") >= 0);
    }
    {
        Fixture *z3 = fixtureNamed(doc, kShutter);
        const QVector<quint32> closing = ShutterAutoOpen::closingShutters(z3);
        QCOMPARE(closing.count(), 1);
        bool close = false;
        for (const QLCCapability *cap : z3->channel(closing.first())->capabilities())
            close = close || ShutterAutoOpen::isCloseCapability(cap);
        QVERIFY(close);
    }
    QCOMPARE(groupNamed(doc, kSpots)->fixtureList().count(), 2);
    QCOMPARE(groupNamed(doc, kWash)->fixtureList().count(), 2);

    // The person, centre stage.
    StageTarget *person = targetNamed(doc, kPerson);
    QVERIFY(person != nullptr);
    QCOMPARE(person->kind(), StageTarget::Kind::Person);
    QVERIFY(qAbs(person->personHeight() - kPersonHeight) < 1e-3f);
    const QVector3D home = person->position();
    QVERIFY((QVector3D(home.x(), home.y(), 0) - mp->stageCentre()).length() < 0.01f);
    QVERIFY(paletteNamed(doc, QStringLiteral("Aim ") + kPerson) != nullptr);

    // Nothing goes out; the pad's universe takes any game controller.
    QVERIFY(iom->universesCount() >= 2);
    for (quint32 u = 0; u < iom->universesCount(); ++u)
        QCOMPARE(iom->outputPatchesCount(u), 0);
    QCOMPARE(iom->getUniverseNameByID(0), QStringLiteral("U01-Bench"));
    QCOMPARE(iom->getUniverseNameByID(1), QStringLiteral("U02-Pad"));
    const QList<Universe::InputSlotDevice> slot = iom->inputSlot(1);
    QCOMPARE(slot.count(), 1);
    QVERIFY(slot.first().anyGamepad);
    QCOMPARE(slot.first().plugin, QStringLiteral("HID"));

    // The venue, applied.
    QCOMPARE(pc->venueCorrections()->activeVenue(), kVenue);
    QCOMPARE(pc->focusCalls()->count(), 2);
    QCOMPARE(pc->focusCalls()->call(0).name, kSpot1 + " on " + kPerson);
    QCOMPARE(pc->focusCalls()->call(1).name, kSpot2 + " on " + kPerson);
    QVERIFY(pc->isAutoMoveInBlack());

    // The productions and their lists.
    const Production *prod = productionNamed(st, kProduction);
    QVERIFY(prod != nullptr);
    QCOMPARE(st->currentProduction(), prod->id);
    QCOMPARE(prod->numbering, Numbering::None);
    QStringList lists;
    for (const CueList &l : prod->lists)
        if (!l.looks && !l.isSubCues())
            lists << l.name;
    QCOMPARE(lists, QStringList({ kMain, kSecond, kPreshow }));
    const CueList *mainList = listNamed(st, kProduction, kMain);
    const CueList *preList = listNamed(st, kProduction, kPreshow);
    QVERIFY(mainList != nullptr && preList != nullptr && listNamed(st, kProduction, kSecond) != nullptr);
    QCOMPARE(st->mainList(prod->id), mainList->id);
    auto sameCues = [&](const CueList *l, const QList<CueSpec> &specs) {
        QCOMPARE(l->cues.count(), specs.count());
        for (int i = 0; i < specs.count(); ++i)
        {
            QCOMPARE(l->cues.at(i).number, specs.at(i).number);
            QCOMPARE(l->cues.at(i).name, specs.at(i).name);
            QStringList layers;
            for (const Layer &ly : l->cues.at(i).layers)
                layers << (ly.name.isEmpty() ? QStringLiteral("(%1)").arg(ly.target.signature()) : ly.name);
            qInfo().noquote() << QStringLiteral("[CUE] %1 / %2 %3").arg(l->name, l->cues.at(i).number,
                                                                       l->cues.at(i).name)
                              << "layers" << l->cues.at(i).layers.count();
        }
    };
    sameCues(mainList, kMainCues);
    sameCues(listNamed(st, kProduction, kSecond), kSecondCues);
    sameCues(preList, kPreshowCues);
    QVERIFY(st->isCueFocusCall(preList->cues.first().id));

    const Cue &c1 = mainList->cues.at(0);
    QCOMPARE(c1.layers.count(), 1);
    QCOMPARE(c1.layers.first().target.groups, QList<quint32>({ groupNamed(doc, kSpots)->id() }));
    bool hasAim = false, hasFollow = false;
    for (const Look &lk : c1.layers.first().looks)
    {
        const QLCPalette *pl = lk.palettes.isEmpty() ? nullptr : doc->palette(lk.palettes.first());
        if (pl == nullptr)
            continue;
        hasAim = hasAim || (pl->type() == QLCPalette::Aim && pl->stageTargetId() == person->id());
        if (pl->type() == QLCPalette::Effect && pl->name() == QStringLiteral("Followspot"))
        {
            hasFollow = true;
            QCOMPARE(pl->effectParamValues().value(QStringLiteral("framing")), 2.0);   // Waist Up
            QVERIFY(ProgrammerController::isFollowSpotPalette(pl));
        }
    }
    QVERIFY(hasAim && hasFollow);
    QCOMPARE(mainList->cues.at(1).layers.count(), 2);
    for (const Look &lk : mainList->cues.at(4).layers.first().looks)
        for (quint32 pid : lk.palettes)
            QVERIFY(doc->palette(pid) == nullptr || doc->palette(pid)->type() != QLCPalette::Aim);
    QVERIFY(st->moveInBlackInto(mainList->cues.at(3).id));

    // The hit, under cue 1.
    const QList<quint32> hits = st->hitsAt(HitScope::Cue, c1.id);
    QCOMPARE(hits.count(), 1);
    QCOMPARE(st->cue(hits.first())->name, kHit);
    QCOMPARE(st->hitPlace(hits.first()).isEmpty(), false);
    qInfo().noquote() << "[HIT]" << kHit << "place:" << st->hitPlace(hits.first());

    // The song: a Renumber Song away from tidy.
    const Production *songProd = productionNamed(st, kSongProduction);
    QVERIFY(songProd != nullptr);
    QCOMPARE(songProd->numbering, Numbering::ShowPartIteration);
    const CueList *songList = listNamed(st, kSongProduction, kSongList);
    QVERIFY(songList != nullptr);
    const QList<Section> secs = st->sections(songList->id);
    QCOMPARE(secs.count(), 1);
    QCOMPARE(secs.first().name, kSong);
    QString why;
    const QList<ShowTreeController::Renumbered> plan = st->renumberSongPlan(songList->id, secs.first(), 1, 1,
                                                                           true, &why);
    QVERIFY2(!plan.isEmpty(), qPrintable(why));
    for (const ShowTreeController::Renumbered &r : plan)
        qInfo().noquote() << "[RENUMBER SONG]" << r.from << "->" << r.to;

    // ---- it plays. The pad: whichever virtual pad is free takes the slot.
    QTRY_VERIFY_WITH_TIMEOUT(iom->inputSlotLive(1) == 0, 5000);
    InputPatch *ip = iom->inputPatch(1);
    QVERIFY(ip != nullptr && ip->isPatched());
    qInfo().noquote() << "[PAD]" << ip->inputName();
    QTRY_VERIFY_WITH_TIMEOUT(pc->isPanBound() && pc->isTiltBound(), 5000);
    const quint32 padLine = ip->input();

    doc->setMode(Doc::Operate);
    settle(200);
    auto go = [&](const CueList *l, int index) {
        ShowControl::Request r;
        r.command = ShowControl::Go;
        r.chaser = l->chaser;
        r.cue = l->cues.at(index).number;
        QVERIFY(m_app->runCommand(r, ShowControl::RouteRunTab));
    };

    // Cue 1: the Spots follow the person, and the stick walks her.
    go(mainList, 0);
    QTRY_VERIFY_WITH_TIMEOUT(pc->followSpotStatus().state != ProgrammerController::FollowSpotStatus::Off, 5000);
    QCOMPARE(pc->followSpotStatus().targetId, person->id());
    QTRY_VERIFY_WITH_TIMEOUT(m_app->followChip()->text().contains(QString::fromUtf8("Spots ← ") + kPerson), 3000);
    qInfo().noquote() << "[FOLLOW]" << m_app->followChip()->text();
    QTRY_VERIFY_WITH_TIMEOUT(out(doc, kSpot1, "Dimmer") > 0 && out(doc, kSpot2, "Dimmer") > 0, 5000);
    QVERIFY(st->isHitArmed(hits.first()));
    const QVector3D from = person->position();
    QVERIFY(padInput(doc, padLine, 0, 255));       // the left stick, hard right
    QTRY_VERIFY_WITH_TIMEOUT(person->position().x() > from.x() + 0.2f, 8000);
    QVERIFY(padInput(doc, padLine, 0, 128));

    // Cue 2: two layers, both lit; the hit is no longer armed.
    go(mainList, 1);
    QTRY_VERIFY_WITH_TIMEOUT(out(doc, kWash1, "Dimmer") > 0 && out(doc, kWash2, "Dimmer") > 0, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(out(doc, kSpot1, "Dimmer") > 0, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!st->isHitArmed(hits.first()), 3000);

    // Cue 3: dimmer up, shutter closed at A; move in black takes it to B,
    // still closed.
    Fixture *z3 = fixtureNamed(doc, kShutter);
    const int panB = panValue(z3, kPosBPan);
    QVERIFY(panB > 30);
    go(mainList, 2);
    QCOMPARE(mainList->cues.at(2).hold, kShutterWaitMs);
    QTRY_VERIFY_WITH_TIMEOUT(out(doc, kShutter, "Master Dimmer") > 200, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(out(doc, kShutter, "Strobe/Shutter") <= 7, 3000);
    auto diag = [&]() {
        QStringList d;
        d << QStringLiteral("main chaser %1").arg(mainList->chaser);
        for (Function *f : doc->functions())
            if (f->type() == Function::ChaserType && f->isRunning())
            {
                Chaser *ch = qobject_cast<Chaser *>(f);
                d << QStringLiteral("running chaser %1 '%2' dur %3 elapsed %4")
                         .arg(f->id()).arg(f->name()).arg(ch->currentStepDuration())
                         .arg(ch->currentRunningStep().m_elapsed);
            }
        for (const CueLookahead::UpcomingCue &u : CueLookahead::upcoming(doc, 60000))
        {
            const CueOutput::FixtureCue fc = CueOutput::computeCues(doc, u.functionId).value(z3->id());
            d << QStringLiteral("upcoming fid %1 in %2 chaser %3 step %4 wants %5 lit %6 int %7 non %8")
                     .arg(u.functionId).arg(u.fireInMs).arg(u.chaserId).arg(u.step)
                     .arg(st->moveInBlackForNext(u.chaserId, u.step, u.functionId, pc->isAutoMoveInBlack()))
                     .arg(fc.lit).arg(fc.intensity).arg(fc.nonIntensity.count());
        }
        d << QStringLiteral("planned %1 gap %2 enabled %3").arg(pc->m_markPlanner->plannedCount())
                 .arg(pc->m_markPlanner->darkGapMs()).arg(pc->m_markPlanner->isEnabled());
        return d.join(QStringLiteral(" | "));
    };
    QTRY_VERIFY2_WITH_TIMEOUT(qAbs(out(doc, kShutter, "Pan Movement") - panB) <= 2
                              && out(doc, kShutter, "Strobe/Shutter") <= 7,
                              qPrintable(QStringLiteral("pan %1, B %2, shutter %3 -- %4")
                                         .arg(out(doc, kShutter, "Pan Movement")).arg(panB)
                                         .arg(out(doc, kShutter, "Strobe/Shutter")).arg(diag())), 6000);
    qInfo().noquote() << "[MIB] in cue 3 the Shutter Spot is at B, shutter"
                      << out(doc, kShutter, "Strobe/Shutter");

    // Cue 4 follows on its own after the wait: open at B.
    QTRY_VERIFY_WITH_TIMEOUT(out(doc, kShutter, "Strobe/Shutter") > 7, 12000);
    QVERIFY(qAbs(out(doc, kShutter, "Pan Movement") - panB) <= 2);
    QCOMPARE(st->runningCue(mainList->id), mainList->cues.at(3).id);

    doc->masterTimer()->stopAllFunctions();
    doc->setMode(Doc::Design);
    settle(300);
    doc->resetModified();
}

void PadBenchShow_Test::buildSaveAndCheck()
{
    QTemporaryDir dir;
    QString path = qEnvironmentVariable("PADBENCH_OUT");
    if (path.isEmpty())
        path = dir.filePath(QStringLiteral("PadBench.qlcc"));
    build(path);
    if (QTest::currentTestFailed())
        return;
    qInfo().noquote() << "[SAVED]" << path << "+" << VenueCorrections::sidecarPath(path);
    check(path);
}

void PadBenchShow_Test::theCommittedShowChecksOut()
{
    const QString show = QStringLiteral(PADBENCH_COMMITTED);
    if (!QFileInfo::exists(show))
        QSKIP("test-workspaces/PadBench.qlcc is not there yet");
    // A copy, venues beside it: opening it never writes into the repo.
    QTemporaryDir dir;
    const QString copy = dir.filePath(QStringLiteral("PadBench.qlcc"));
    QVERIFY(QFile::copy(show, copy));
    QVERIFY(QFile::copy(VenueCorrections::sidecarPath(show), VenueCorrections::sidecarPath(copy)));
    check(copy);
}

/****************************************************************************
 * Opening a show over one that plays
 ****************************************************************************/

namespace
{

/* "Function start()" lines while the old show is torn down: who queued a
   function the teardown then deletes (said when the check fails). */
bool g_tearingDown = false;
QStringList g_startedInTeardown;
QtMessageHandler g_previousHandler = nullptr;

void noteStarts(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (g_tearingDown && msg.startsWith(QLatin1String("Function start()")))
        g_startedInTeardown << msg;
    if (g_previousHandler != nullptr)
        g_previousHandler(type, context, msg);
}

/* What the MasterTimer still holds that is not a function of the Doc: a
   pointer it would tick after the Doc deleted it. Pointers only, never
   followed. */
int strayFunctions(Doc *doc)
{
    MasterTimer *timer = doc->masterTimer();
    QSet<Function *> live;
    for (Function *f : doc->functions())
        live.insert(f);
    QMutexLocker locker(&timer->m_functionListMutex);
    int stray = 0;
    for (Function *f : std::as_const(timer->m_startQueue))
        if (!live.contains(f))
            ++stray;
    for (Function *f : std::as_const(timer->m_functionList))
        if (!live.contains(f))
            ++stray;
    return stray;
}

} // namespace

/* File > Open over a show that plays (Design: the Workbench previewing what
   it has loaded, the Show Tree previewing a cue, cue lists running): the
   old show's functions are deleted with the timer stopped, and it starts
   again on the new show at once. Whatever the old show's teardown started
   again must not be left in the timer for it to tick freed (crash
   2026-11-13: DanceHysteria-0.6, then PadBench.qlcc). */
void PadBenchShow_Test::openingAShowOverOneThatPlays()
{
    const QString pad = QStringLiteral(PADBENCH_COMMITTED);
    const QString dance = QStringLiteral(DANCE_SHOW);
    if (!QFileInfo::exists(pad) || !QFileInfo::exists(dance))
        QSKIP("the test shows are not there");
    QTemporaryDir dir;
    const QString padCopy = dir.filePath(QStringLiteral("PadBench.qlcc"));
    const QString danceCopy = dir.filePath(QStringLiteral("Dance.qlcc"));
    QVERIFY(QFile::copy(pad, padCopy));
    QVERIFY(QFile::copy(VenueCorrections::sidecarPath(pad), VenueCorrections::sidecarPath(padCopy)));
    QVERIFY(QFile::copy(dance, danceCopy));

    Doc *doc = m_app->doc();
    m_app->setWorkspaceView(App::EverythingView, false);
    ProgrammingManager *pm = ProgrammingManager::instance();
    QVERIFY(pm != nullptr);

    int strayAtCleared = -1;
    QMetaObject::Connection cleared = connect(doc, &Doc::cleared, this, [&]() {
        g_tearingDown = false;
        strayAtCleared = qMax(strayAtCleared, strayFunctions(doc));
    });
    g_previousHandler = qInstallMessageHandler(noteStarts);
    auto restore = qScopeGuard([&]() {
        qInstallMessageHandler(g_previousHandler);
        g_previousHandler = nullptr;
        g_tearingDown = false;
        disconnect(cleared);
    });

    /* Open the way File > Open does: no Stop all first, the timer running. */
    auto openOver = [&](const QString &path) {
        doc->resetModified();
        g_tearingDown = true;
        m_app->clearDocument();
        QCOMPARE(m_app->loadShowFile(path, App::NoRecovery), QFile::NoError);
        doc->resetModified();
        QCOMPARE(doc->mode(), Doc::Design);
        // The new show plays a while on the timer's thread.
        settle(60);
        QCOMPARE(strayFunctions(doc), 0);
    };

    QVERIFY(m_app->loadShowFile(danceCopy, App::NoRecovery) == QFile::NoError);
    doc->setMode(Doc::Design);
    settle(50);

    const int rounds = 6;
    for (int round = 0; round < rounds; ++round)
    {
        /* The dance show: Build's Workbench previews one function of each
           kind in turn; on odd rounds its cue lists run too. */
        m_app->switchToTabContaining(pm);
        QList<quint32> picks;
        for (Function::Type type : { Function::CollectionType, Function::ChaserType, Function::SceneType,
                                     Function::RGBMatrixType, Function::EFXType })
        {
            const QList<Function *> of = doc->functionsByType(type);
            if (!of.isEmpty())
                picks << of.at(round % of.size())->id();
        }
        QVERIFY(!picks.isEmpty());
        pm->showFunction(picks.at(round % picks.size()));
        if (round % 2 == 1)
            for (Function *f : doc->functionsByType(Function::ChaserType))
                f->start(doc->masterTimer(), FunctionParent::master());
        settle(80);

        /* Over it, PadBench. */
        openOver(padCopy);
        QCOMPARE(strayAtCleared, 0);

        /* PadBench: the Show Tree previews a cue; on odd rounds a list
           runs (GO) under it, and the Workbench walks a chaser. */
        QList<quint32> cues;
        QList<quint32> lists;
        for (const ShowTree::Production &p : doc->showTree()->productions())
            for (const ShowTree::CueList &l : p.lists)
            {
                lists << l.chaser;
                for (const ShowTree::Cue &c : l.cues)
                    cues << c.id;
            }
        QVERIFY(!cues.isEmpty());
        const quint32 cue = cues.at(round % cues.size());
        if (round % 3 != 2)
        {
            /* The cue open in the Show Tree's Workbench, playing in context
               -- or Solo, one layer of it. */
            ShowTreeView *tree = ShowTreeView::instance();
            QVERIFY(tree != nullptr);
            m_app->switchToTabContaining(tree);
            settle();
            QVERIFY(tree->openInWorkbench(cue));
            settle(30);
            Workbench *bench = tree->dockedWorkbench();
            QVERIFY(bench != nullptr && bench->stack() != nullptr);
            if (round % 3 == 1)
            {
                bench->stack()->setFocusLayer(0);
                bench->stack()->setSolo(true);
            }
        }
        else
            doc->showTree()->previewCue(cue);
        if (round % 2 == 1)
        {
            if (Chaser *ch = qobject_cast<Chaser *>(doc->function(lists.first())))
                doc->showControl()->go(ch, 0);
            const QList<Function *> chasers = doc->functionsByType(Function::ChaserType);
            if (!chasers.isEmpty())
                pm->showFunction(chasers.at(round % chasers.size())->id());
        }
        settle(80);

        /* And the dance show over that. */
        openOver(danceCopy);
        QCOMPARE(strayAtCleared, 0);
    }
    if (!g_startedInTeardown.isEmpty())
        qWarning() << "[openingAShowOverOneThatPlays] started while the old show was torn down:"
                   << g_startedInTeardown;
    doc->masterTimer()->stopAllFunctions();
    settle(50);
    QFAIL("WIP: keep the logs");
}

int main(int argc, char **argv)
{
    // A home of its own: nothing touches yours.
    QTemporaryDir home;
    qputenv("HOME", home.path().toUtf8());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    PadBenchShow_Test test;
    return QTest::qExec(&test, argc, argv);
}
