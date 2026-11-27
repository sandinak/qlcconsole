/*
  Q Light Controller Plus
  hazardarmingshow_test.cpp

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
#include <QXmlStreamReader>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#undef protected
#undef private
#include "showtreecontroller.h"
#include "qlcfixturedefcache.h"
#include "inputoutputmap.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "fixturegroup.h"
#include "fixturerest.h"
#include "deskrecord.h"
#include "controlmap.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "doc.h"

#include "hazardarmingshow_test.h"

using namespace ShowTree;

namespace
{

const QString kProduction = QStringLiteral("Hazard Arming");
const QString kList = QStringLiteral("Main");
const QString kJet = QStringLiteral("CO2 jet");
const QString kHazer = QStringLiteral("Hazer");
const QString kWash = QStringLiteral("Wash");
const QString kUniverse = QStringLiteral("U01-Hazard-Probe");
const QStringList kCues = { QStringLiteral("Preset"), QStringLiteral("CO2 hit"), QStringLiteral("Haze on"),
                            QStringLiteral("Haze off"), QStringLiteral("Blackout") };

/* The OLA probe (the Pi on the show VLAN): Art-Net 0:0:15, unicast. Never
   0:0:0, which is the office rig's. */
const QString kProbeIp = QStringLiteral("172.18.2.218");
const int kProbeUniverse = 15;

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

QLCPalette *paletteNamed(Doc *doc, const QString &name)
{
    for (QLCPalette *p : doc->palettes())
        if (p->name() == name)
            return p;
    return nullptr;
}

FixtureGroup *groupNamed(Doc *doc, const QString &name)
{
    for (FixtureGroup *g : doc->fixtureGroups())
        if (g->name() == name)
            return g;
    return nullptr;
}

const CueList *listOf(ShowTreeController *st)
{
    for (const Production &p : st->productions())
        if (p.name == kProduction)
            for (const CueList &l : p.lists)
                if (l.name == kList)
                    return &l;
    return nullptr;
}

/* What cue @p cue sets on group @p group: the palette's name, or empty. */
QString setOn(Doc *doc, const Cue &cue, quint32 group)
{
    for (const Layer &l : cue.layers)
        if (l.target.groups.contains(group))
            for (const Look &lk : l.looks)
                if (!lk.palettes.isEmpty())
                    if (QLCPalette *p = doc->palette(lk.palettes.first()))
                        return p->name();
    return QString();
}
}

void HazardArmingShow_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
    m_app->showNormal();
    m_app->resize(1280, 800);
    QVERIFY(m_app->doc()->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures")));
}

void HazardArmingShow_Test::cleanupTestCase()
{
    settle();
    m_app->doc()->masterTimer()->stopAllFunctions();
    m_app->doc()->setMode(Doc::Design);
    settle(100);
    m_app->doc()->resetModified();
    delete m_app;
    m_app = nullptr;
}

bool HazardArmingShow_Test::open(const QString &path)
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

void HazardArmingShow_Test::build(const QString &path)
{
    m_app->doc()->masterTimer()->stopAllFunctions();
    m_app->doc()->setMode(Doc::Design);
    m_app->doc()->resetModified();
    m_app->clearDocument();
    settle(50);
    Doc *doc = m_app->doc();
    ShowTreeController *st = doc->showTree();

    /* ---- the rig on universe 1: the CO2 jet on channel 1 and the hazer on
       channel 2 (one channel each, so the probe reads them at a glance),
       then the wash -- a stock RGB par -- from channel 3. Each says what it
       behaves as, set by the LD, not guessed. */
    quint32 address = 0;
    auto add = [&](const QString &name, int behaviour, QLCFixtureDef *def, QLCFixtureMode *mode) {
        Fixture *f = new Fixture(doc);
        f->setName(name);
        if (def != nullptr && mode != nullptr)
            f->setFixtureDefinition(def, mode);
        else
            f->setChannels(1);
        f->setUniverse(0);
        f->setAddress(address);
        address += f->channels();
        if (!doc->addFixture(f))
            return quint32(Fixture::invalidId());
        f->setBehaviourSetting(behaviour);
        FixtureGroup *g = new FixtureGroup(doc);
        g->setName(name);
        doc->addFixtureGroup(g);
        g->assignFixture(f->id());
        return g->id();
    };
    QLCFixtureDef *par = doc->fixtureDefCache()->fixtureDef(QStringLiteral("Chauvet"), QStringLiteral("SlimPAR 56"));
    QVERIFY(par != nullptr);
    QLCFixtureMode *parMode = nullptr;
    for (QLCFixtureMode *m : par->modes())
        if (m->name() == QLatin1String("7-Ch"))
            parMode = m;
    QVERIFY(parMode != nullptr);
    const quint32 jet = add(kJet, Fixture::HazardBurstBehaviour, nullptr, nullptr);
    const quint32 hazer = add(kHazer, Fixture::HazardRunningBehaviour, nullptr, nullptr);
    const quint32 wash = add(kWash, Fixture::LightBehaviour, par, parMode);
    QVERIFY(jet != Fixture::invalidId() && hazer != Fixture::invalidId() && wash != Fixture::invalidId());

    doc->inputOutputMap()->setUniverseName(0, kUniverse);

    // ---- levels
    auto level = [&](const QString &name, int v) {
        QLCPalette *p = new QLCPalette(QLCPalette::Dimmer, doc);
        p->setName(name);
        p->setValue(v);
        doc->addPalette(p);
        return p->id();
    };
    const quint32 full = level(QStringLiteral("Full"), 255), half = level(QStringLiteral("Half"), 128),
                  out = level(QStringLiteral("Out"), 0);

    // ---- the cues
    auto set = [st](quint32 cue, quint32 group, quint32 palette) {
        Target t;
        t.groups << group;
        return st->setTargetLook(cue, t, st->lookForPalette(palette));
    };
    const quint32 prod = st->addProduction(kProduction);
    const quint32 list = st->addCueList(prod, kList, ListMode::Tracking);
    QVERIFY(st->setMainList(list));
    QList<quint32> cues;
    for (int i = 0; i < kCues.size(); ++i)
        cues << st->addCue(list, i - 1, kCues.at(i));
    // Preset: the wash up, both hazards safe.
    QVERIFY(set(cues[0], wash, full));
    QVERIFY(set(cues[0], jet, out));
    QVERIFY(set(cues[0], hazer, out));
    // CO2 hit: the jet out (only while the Hold is held), the wash at half.
    QVERIFY(set(cues[1], jet, full));
    QVERIFY(set(cues[1], wash, half));
    // Haze on: the jet back to safe, the hazer running (once Armed).
    QVERIFY(set(cues[2], jet, out));
    QVERIFY(set(cues[2], hazer, full));
    QVERIFY(set(cues[2], wash, full));
    // Haze off.
    QVERIFY(set(cues[3], hazer, out));
    // Blackout: everything out.
    QVERIFY(set(cues[4], wash, out));
    QVERIFY(set(cues[4], jet, out));
    QVERIFY(set(cues[4], hazer, out));
    doc->controlMap()->setRunTabList(list);
    if (QTest::currentTestFailed())
        return;

    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    doc->resetModified();

    // ---- the universe's Art-Net output, as the shows of the rig write it
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QString xml = QString::fromUtf8(f.readAll());
    f.close();
    const QString from = QStringLiteral("<Universe Name=\"%1\" ID=\"0\"/>").arg(kUniverse);
    QVERIFY2(xml.contains(from), "the universe element is not as expected");
    xml.replace(from, QStringLiteral(
        "<Universe Name=\"%1\" ID=\"0\">\n"
        "    <Output Plugin=\"ArtNet\" UID=\"172.18.2.219\" Line=\"2\" Network=\"172.18.2.0/24\">\n"
        "     <PluginParameters outputIP=\"%2\" outputUni=\"%3\"/>\n"
        "    </Output>\n"
        "   </Universe>").arg(kUniverse, kProbeIp).arg(kProbeUniverse));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(xml.toUtf8());
    f.close();
}

/****************************************************************************
 * The check
 ****************************************************************************/

void HazardArmingShow_Test::checkOutputXml(const QString &path)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray all = f.readAll();
    // The kinds are in the file as the LD set them, not left to a guess.
    QVERIFY(all.contains("<BehavesAs>HazardBurst</BehavesAs>"));
    QVERIFY(all.contains("<BehavesAs>HazardRunning</BehavesAs>"));
    QXmlStreamReader xml(all);
    int outputs = 0;
    bool inFirstUniverse = false;
    while (!xml.atEnd())
    {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("Universe"))
            inFirstUniverse = xml.attributes().value("ID") == QLatin1String("0");
        else if (xml.isStartElement() && xml.name() == QLatin1String("PluginParameters"))
        {
            ++outputs;
            // Every patched universe in the file: the probe's, never 0:0:0.
            QCOMPARE(xml.attributes().value("outputIP").toString(), kProbeIp);
            QCOMPARE(xml.attributes().value("outputUni").toInt(), kProbeUniverse);
            QVERIFY(inFirstUniverse);
        }
        else if (xml.isStartElement() && xml.name() == QLatin1String("Output"))
            QCOMPARE(xml.attributes().value("Plugin").toString(), QStringLiteral("ArtNet"));
    }
    QVERIFY2(!xml.hasError(), qPrintable(xml.errorString()));
    QCOMPARE(outputs, 1);
}

void HazardArmingShow_Test::check(const QString &path)
{
    checkOutputXml(path);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(open(path));
    Doc *doc = m_app->doc();
    ShowTreeController *st = doc->showTree();

    // The rig and what each behaves as.
    QCOMPARE(doc->fixtures().count(), 3);
    Fixture *jet = fixtureNamed(doc, kJet);
    Fixture *hazer = fixtureNamed(doc, kHazer);
    Fixture *wash = fixtureNamed(doc, kWash);
    QVERIFY(jet != nullptr && hazer != nullptr && wash != nullptr);
    QCOMPARE(jet->behaviourSetting(), int(Fixture::HazardBurstBehaviour));
    QCOMPARE(hazer->behaviourSetting(), int(Fixture::HazardRunningBehaviour));
    QCOMPARE(FixtureRest::behaviour(jet), Fixture::HazardBurstBehaviour);
    QCOMPARE(FixtureRest::behaviour(hazer), Fixture::HazardRunningBehaviour);
    QCOMPARE(FixtureRest::behaviour(wash), Fixture::LightBehaviour);
    QCOMPARE(jet->address(), quint32(0));
    QCOMPARE(hazer->address(), quint32(1));
    QCOMPARE(wash->address(), quint32(2));
    QCOMPARE(QStringList({ wash->fixtureDef()->manufacturer(), wash->fixtureDef()->model(), wash->fixtureMode()->name() })
                 .join('|'), QStringLiteral("Chauvet|SlimPAR 56|7-Ch"));
    for (Fixture *fx : doc->fixtures())
        QCOMPARE(fx->universe(), quint32(0));
    QCOMPARE(doc->inputOutputMap()->universeNames().value(0), kUniverse);

    // The cues, in order, and what each sets.
    const CueList *list = listOf(st);
    QVERIFY(list != nullptr);
    QCOMPARE(list->cues.count(), kCues.count());
    for (int i = 0; i < kCues.count(); ++i)
        QCOMPARE(list->cues.at(i).name, kCues.at(i));
    FixtureGroup *gJet = groupNamed(doc, kJet), *gHazer = groupNamed(doc, kHazer), *gWash = groupNamed(doc, kWash);
    QVERIFY(gJet != nullptr && gHazer != nullptr && gWash != nullptr);
    QVERIFY(paletteNamed(doc, QStringLiteral("Full")) != nullptr);
    QCOMPARE(setOn(doc, list->cues.at(0), gJet->id()), QStringLiteral("Out"));
    QCOMPARE(setOn(doc, list->cues.at(0), gHazer->id()), QStringLiteral("Out"));
    QCOMPARE(setOn(doc, list->cues.at(0), gWash->id()), QStringLiteral("Full"));
    QCOMPARE(setOn(doc, list->cues.at(1), gJet->id()), QStringLiteral("Full"));
    QCOMPARE(setOn(doc, list->cues.at(2), gHazer->id()), QStringLiteral("Full"));
    QCOMPARE(setOn(doc, list->cues.at(2), gJet->id()), QStringLiteral("Out"));
    QCOMPARE(setOn(doc, list->cues.at(3), gHazer->id()), QStringLiteral("Out"));
    QCOMPARE(setOn(doc, list->cues.at(4), gWash->id()), QStringLiteral("Out"));

    /* The desk's Hazard hold / arm / kill: unlearned, so they are learned
       on the bench's own board (this computer's desk, not the show's). */
    DeskRecord *desk = doc->deskRecord();
    QVERIFY(desk != nullptr);
    for (DeskRecord::Role r : { DeskRecord::HazardHold, DeskRecord::HazardArm, DeskRecord::HazardKill })
        QVERIFY2(!desk->isSet(r), qPrintable(DeskRecord::roleName(r)));
}

void HazardArmingShow_Test::buildSaveAndCheck()
{
    QTemporaryDir dir;
    QString path = qEnvironmentVariable("HAZARD_OUT");
    if (path.isEmpty())
        path = dir.filePath(QStringLiteral("hazard-arming.qlcc"));
    build(path);
    if (QTest::currentTestFailed())
        return;
    check(path);
    // The pool's --shots brings this back: that is how the committed file is made.
    const QString shots = qEnvironmentVariable("QLC_SHOT_DIR");
    if (!shots.isEmpty() && !QTest::currentTestFailed())
    {
        const QString dest = QDir(shots).filePath(QStringLiteral("hazard-arming.qlcc"));
        QFile::remove(dest);        // a second leg's run writes over the first's
        QVERIFY(QFile::copy(path, dest));
    }
}

void HazardArmingShow_Test::theCommittedShowChecksOut()
{
    const QString show = QStringLiteral(HAZARD_COMMITTED);
    // Committed, so missing is a failure (a rename or a bad archive must not pass as a skip).
    QVERIFY2(QFileInfo::exists(show), qPrintable("committed show is missing: " + show));
    QTemporaryDir dir;
    const QString copy = dir.filePath(QStringLiteral("hazard-arming.qlcc"));
    QVERIFY(QFile::copy(show, copy));
    check(copy);
}

int main(int argc, char **argv)
{
    // A home of its own: nothing touches yours.
    QTemporaryDir home;
    qputenv("HOME", home.path().toUtf8());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    HazardArmingShow_Test test;
    return QTest::qExec(&test, argc, argv);
}
