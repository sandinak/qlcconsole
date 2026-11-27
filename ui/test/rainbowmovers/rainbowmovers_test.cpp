/*
  Q Light Controller Plus
  rainbowmovers_test.cpp

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
#include <QSet>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#undef protected
#undef private
#include "showtreecontroller.h"
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "effectinstance.h"
#include "qlcfixturedefcache.h"
#include "inputoutputmap.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlccapability.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

#include "rainbowmovers_test.h"

using namespace ShowTree;

namespace
{

const QString kProduction = QStringLiteral("Rainbow Movers");
const QString kList = QStringLiteral("Main");
const QString kOn = QStringLiteral("Rainbow on");
const QString kOff = QStringLiteral("Rainbow off");
const QString kCmy = QStringLiteral("CMY Mover");
const QString kWheel = QStringLiteral("Wheel Mover");
const QString kRgb = QStringLiteral("RGB Par");
const QString kUniverse = QStringLiteral("U01-Rainbow-Probe");

/* The OLA probe (the Pi on the show VLAN): Art-Net 0:0:15, unicast. Never
   0:0:0, which is the office rig's. */
const QString kProbeIp = QStringLiteral("172.18.2.17");
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

QLCFixtureMode *modeNamed(QLCFixtureDef *def, const QString &name)
{
    if (def == nullptr)
        return nullptr;
    for (QLCFixtureMode *m : def->modes())
        if (m->name() == name)
            return m;
    return nullptr;
}

QLCPalette *paletteNamed(Doc *doc, const QString &name)
{
    for (QLCPalette *p : doc->palettes())
        if (p->name() == name)
            return p;
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
}

void RainbowMovers_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
    m_app->showNormal();
    m_app->resize(1280, 800);
    Doc *doc = m_app->doc();
    doc->effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    QVERIFY(doc->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures")));
}

void RainbowMovers_Test::cleanupTestCase()
{
    settle();
    m_app->doc()->masterTimer()->stopAllFunctions();
    m_app->doc()->setMode(Doc::Design);
    settle(100);
    m_app->doc()->resetModified();
    delete m_app;
    m_app = nullptr;
}

bool RainbowMovers_Test::open(const QString &path)
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

void RainbowMovers_Test::build(const QString &path)
{
    m_app->doc()->masterTimer()->stopAllFunctions();
    m_app->doc()->setMode(Doc::Design);
    m_app->doc()->resetModified();
    m_app->clearDocument();
    settle(50);
    Doc *doc = m_app->doc();
    QLCFixtureDefCache *cache = doc->fixtureDefCache();
    EffectScriptCache *scripts = doc->effectScriptRunner()->cache();
    ShowTreeController *st = doc->showTree();

    // ---- fixtures: stock definitions only, one after the other on universe 1
    struct Rig { QString name, maker, model, mode; };
    const QList<Rig> rig = {
        { kCmy, "Ayrton", "Rivale Profile", "Standard" },       // cyan, magenta, yellow (and a wheel it leaves be)
        { kWheel, "American DJ", "Focus Spot Three Z", "21 Channel" },  // a colour wheel and nothing else
        { kRgb, "Chauvet", "SlimPAR 56", "7-Ch" },              // red, green, blue: the comparison
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
    }
    QVERIFY(address <= 512);

    // ---- the universe's name (its output is put in the saved file below)
    InputOutputMap *iom = doc->inputOutputMap();
    iom->setUniverseName(0, kUniverse);

    // ---- palettes
    auto palette = [&](QLCPalette::PaletteType type, const QString &name) {
        QLCPalette *p = new QLCPalette(type, doc);
        p->setName(name);
        doc->addPalette(p);
        return p;
    };
    QLCPalette *full = palette(QLCPalette::Dimmer, QStringLiteral("Full"));
    full->setValue(255);
    QLCPalette *dark = palette(QLCPalette::Dimmer, QStringLiteral("Dark"));
    dark->setValue(0);
    palette(QLCPalette::Shutter, QStringLiteral("Shutter Open"));
    QLCPalette *rainbow = palette(QLCPalette::Effect, QStringLiteral("Rainbow"));
    rainbow->setScriptPath(scripts->scriptPath(QStringLiteral("rainbow")));
    QVERIFY(!rainbow->scriptPath().isEmpty());
    QLCPalette *open = paletteNamed(doc, QStringLiteral("Shutter Open"));
    QVERIFY(open != nullptr);

    // ---- the production: Rainbow on, Rainbow off
    Target all;
    for (const Rig &r : rig)
        all.fixtures << fixtureNamed(doc, r.name)->id();
    auto look = [&](quint32 cue, quint32 layer, QLCPalette *p) {
        Look l = st->lookForPalette(p->id());
        QVERIFY2(st->setLayerLook(cue, layer, l), qPrintable(p->name()));
    };
    const quint32 prod = st->addProduction(kProduction);
    const quint32 list = st->addCueList(prod, kList, ListMode::NonTracking);
    QVERIFY(st->setMainList(list));
    const quint32 on = st->addCue(list, -1, kOn);
    const quint32 off = st->addCue(list, 0, kOff);
    {   // on: all three at full, the shutters open, the rainbow running
        const quint32 l = st->addLayer(on, all);
        look(on, l, full);
        look(on, l, open);
        look(on, l, rainbow);
    }
    {   // off: no effect in it, the lights dark
        const quint32 l = st->addLayer(off, all);
        look(off, l, dark);
    }
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

void RainbowMovers_Test::checkOutputXml(const QString &path)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QXmlStreamReader xml(&f);
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

void RainbowMovers_Test::check(const QString &path)
{
    checkOutputXml(path);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(open(path));
    Doc *doc = m_app->doc();
    ShowTreeController *st = doc->showTree();

    // The rig.
    const QHash<QString, QString> models = {
        { kCmy, "Ayrton|Rivale Profile|Standard" },
        { kWheel, "American DJ|Focus Spot Three Z|21 Channel" },
        { kRgb, "Chauvet|SlimPAR 56|7-Ch" },
    };
    QCOMPARE(doc->fixtures().count(), models.count());
    for (Fixture *fx : doc->fixtures())
    {
        QVERIFY2(models.contains(fx->name()), qPrintable(fx->name()));
        QCOMPARE(QStringList({ fx->fixtureDef()->manufacturer(), fx->fixtureDef()->model(),
                               fx->fixtureMode()->name() }).join('|'), models.value(fx->name()));
        QCOMPARE(fx->universe(), quint32(0));
    }
    QCOMPARE(doc->inputOutputMap()->universeNames().value(0), kUniverse);

    // The cue list: Rainbow on (all three, the Rainbow), Rainbow off (no effect).
    const CueList *list = listOf(st);
    QVERIFY(list != nullptr);
    QCOMPARE(list->cues.count(), 2);
    QCOMPARE(list->cues.at(0).name, kOn);
    QCOMPARE(list->cues.at(1).name, kOff);
    QLCPalette *rainbow = paletteNamed(doc, QStringLiteral("Rainbow"));
    QVERIFY(rainbow != nullptr);
    QVERIFY(rainbow->scriptPath().endsWith(QLatin1String("rainbow.js")));
    auto effectIn = [&](const Cue &c) {
        bool seen = false;
        for (const Layer &l : c.layers)
            for (const Look &lk : l.looks)
                seen = seen || lk.palettes.contains(rainbow->id());
        return seen;
    };
    QVERIFY(effectIn(list->cues.at(0)));
    QVERIFY(!effectIn(list->cues.at(1)));
    QCOMPARE(list->cues.at(0).layers.count(), 1);
    QCOMPARE(list->cues.at(0).layers.first().target.fixtures.count(), 3);

    checkEffect();
}

void RainbowMovers_Test::checkEffect()
{
    Doc *doc = m_app->doc();
    QLCPalette *rainbow = paletteNamed(doc, QStringLiteral("Rainbow"));
    QVERIFY(rainbow != nullptr);
    Fixture *cmy = fixtureNamed(doc, kCmy);
    Fixture *wheel = fixtureNamed(doc, kWheel);
    Fixture *rgb = fixtureNamed(doc, kRgb);
    QVERIFY(cmy != nullptr && wheel != nullptr && rgb != nullptr);

    Scene *scene = new Scene(doc);
    doc->addFunction(scene);
    for (Fixture *f : { cmy, wheel, rgb })
        scene->addFixture(f->id());
    scene->addPalette(rainbow->id());
    EffectInstance inst(doc, scene->id(), rainbow->id());
    QVERIFY(inst.isValid());
    for (Fixture *f : { cmy, wheel, rgb })
        QVERIFY2(inst.effectiveFixtureIds().contains(f->id()), qPrintable(f->name()));

    // The colour wheel's wslots, as the definition names them.
    QList<QPair<int, int>> wslots;
    for (quint32 c = 0; c < wheel->channels(); ++c)
    {
        const QLCChannel *ch = wheel->channel(c);
        if (ch->group() != QLCChannel::Colour)
            continue;
        for (QLCCapability *cap : ch->capabilities())
            if (cap->presetType() == QLCCapability::SingleColor)
                wslots << qMakePair(int(cap->min()), int(cap->max()));
    }
    QVERIFY(wslots.size() >= 6);
    auto onASlot = [&](int v) {
        for (const auto &s : wslots)
            if (v >= s.first && v <= s.second)
                return true;
        return false;
    };

    // One full turn of the rainbow (5 s) on a fixed clock, every 100 ms.
    QList<int> cyan, magenta, yellow, wheelValues, red, green, blue;
    auto channelOf = [](Fixture *f, quint32 ch) { return f->channel(ch)->preset(); };
    for (int t = 0; t <= 5000; t += 100)
    {
        inst.setOffStageClock(t);
        inst.runTick();
        int c = -1, m = -1, y = -1, w = -1, r = -1, g = -1, b = -1;
        for (const EffectInstance::DmxWrite &wr : inst.dmxWrites())
        {
            if (wr.fixtureId == cmy->id())
            {
                switch (channelOf(cmy, wr.channel))
                {
                case QLCChannel::IntensityCyan: c = wr.value; break;
                case QLCChannel::IntensityMagenta: m = wr.value; break;
                case QLCChannel::IntensityYellow: y = wr.value; break;
                default: break;
                }
            }
            else if (wr.fixtureId == wheel->id() && wheel->channel(wr.channel)->group() == QLCChannel::Colour)
                w = wr.value;
            else if (wr.fixtureId == rgb->id())
            {
                switch (channelOf(rgb, wr.channel))
                {
                case QLCChannel::IntensityRed: r = wr.value; break;
                case QLCChannel::IntensityGreen: g = wr.value; break;
                case QLCChannel::IntensityBlue: b = wr.value; break;
                default: break;
                }
            }
        }
        const QString at = QStringLiteral(" at %1 ms").arg(t);
        QVERIFY2(c >= 0 && m >= 0 && y >= 0, qPrintable("no CMY written" + at));
        QVERIFY2(w >= 0, qPrintable("no wheel value written" + at));
        QVERIFY2(r >= 0 && g >= 0 && b >= 0, qPrintable("no RGB written" + at));
        cyan << c; magenta << m; yellow << y; wheelValues << w; red << r; green << g; blue << b;
    }

    /* CMY mixes: each filter moves in small steps (a fade, never a jump),
       over most of its range, and the same hue on the RGB par fades too. */
    auto smooth = [](const QList<int> &v, const QString &what) {
        int biggest = 0, lo = 255, hi = 0;
        QSet<int> distinct;
        for (int i = 0; i < v.size(); ++i)
        {
            lo = qMin(lo, v.at(i));
            hi = qMax(hi, v.at(i));
            distinct << v.at(i);
            if (i > 0)
                biggest = qMax(biggest, qAbs(v.at(i) - v.at(i - 1)));
        }
        QVERIFY2(biggest <= 40, qPrintable(QString("%1 jumps by %2").arg(what).arg(biggest)));
        QVERIFY2(hi - lo >= 200, qPrintable(QString("%1 only moves %2..%3").arg(what).arg(lo).arg(hi)));
        QVERIFY2(distinct.size() >= 15, qPrintable(QString("%1 has %2 values").arg(what).arg(distinct.size())));
    };
    smooth(cyan, "cyan");
    if (QTest::currentTestFailed()) return;
    smooth(magenta, "magenta");
    if (QTest::currentTestFailed()) return;
    smooth(yellow, "yellow");
    if (QTest::currentTestFailed()) return;
    smooth(red, "red");
    if (QTest::currentTestFailed()) return;
    smooth(green, "green");
    if (QTest::currentTestFailed()) return;
    smooth(blue, "blue");
    if (QTest::currentTestFailed()) return;

    /* The wheel steps: every value is on a colour slot, it changes slot
       several times in the turn, and it never fades -- when it changes, it
       lands on another slot's whole range. */
    QSet<int> slotsSeen;
    int changes = 0;
    for (int i = 0; i < wheelValues.size(); ++i)
    {
        QVERIFY2(onASlot(wheelValues.at(i)), qPrintable(QString("wheel value %1 is on no colour slot").arg(wheelValues.at(i))));
        for (int s = 0; s < wslots.size(); ++s)
            if (wheelValues.at(i) >= wslots.at(s).first && wheelValues.at(i) <= wslots.at(s).second)
                slotsSeen << s;
        if (i > 0 && wheelValues.at(i) != wheelValues.at(i - 1))
            ++changes;
    }
    QVERIFY2(slotsSeen.size() >= 4, qPrintable(QString("the wheel only visited %1 wslots").arg(slotsSeen.size())));
    QVERIFY2(changes >= 4, qPrintable(QString("the wheel only changed %1 times").arg(changes)));
    // A step, not a fade: far fewer changes than samples, and no two in a row all the way round.
    QVERIFY2(changes < wheelValues.size() / 2, qPrintable(QString("the wheel changed on %1 of %2 samples").arg(changes).arg(wheelValues.size())));
}

void RainbowMovers_Test::buildSaveAndCheck()
{
    QTemporaryDir dir;
    QString path = qEnvironmentVariable("RAINBOW_OUT");
    if (path.isEmpty())
        path = dir.filePath(QStringLiteral("rainbow-movers.qlcc"));
    build(path);
    if (QTest::currentTestFailed())
        return;
    check(path);
    // The pool's --shots brings this back: that is how the committed file is made.
    const QString shots = qEnvironmentVariable("QLC_SHOT_DIR");
    if (!shots.isEmpty() && !QTest::currentTestFailed())
        QVERIFY(QFile::copy(path, QDir(shots).filePath(QStringLiteral("rainbow-movers.qlcc"))));
}

void RainbowMovers_Test::theCommittedShowChecksOut()
{
    const QString show = QStringLiteral(RAINBOW_COMMITTED);
    // Committed, so missing is a failure (a rename or a bad archive must not pass as a skip).
    QVERIFY2(QFileInfo::exists(show), qPrintable("committed show is missing: " + show));
    QTemporaryDir dir;
    const QString copy = dir.filePath(QStringLiteral("rainbow-movers.qlcc"));
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
    RainbowMovers_Test test;
    return QTest::qExec(&test, argc, argv);
}
