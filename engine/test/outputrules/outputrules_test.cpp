/*
  Q Light Controller Plus - Test Unit
  outputrules_test.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QBuffer>
#include <QElapsedTimer>
#include <QtTest>

#include "../common/stdprelude.h"
#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#include "iopluginstub.h"
#include "effectscriptrunner.h"
#undef private
#undef protected

#include "outputrules_test.h"
#include "functionparent.h"
#include "chaserstep.h"
#include "chaser.h"
#include "scene.h"
#include "collection.h"
#include "showcontrol.h"
#include "script.h"
#include "qlcpalette.h"
#include "fixturegroup.h"
#include <QTemporaryDir>
#include <QScopeGuard>
#include "genericdmxsource.h"
#include "channelmodifier.h"
#include "outputrulekeeper.h"
#include "fixturerest.h"
#include "controlmap.h"
#include "inputoutputmap.h"
#include "outputpatch.h"
#include "ioplugincache.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "grandmaster.h"
#include "qlcfile.h"
#include "parkeffect.h"
#include "markeffect.h"
#include "outputlayer.h"
#include "cuestack.h"
#include "cue.h"
#include "highlighteffect.h"
#include "testchannels.h"
#include "programmerflasher.h"
#include "lastlookeffect.h"
#include "focussession.h"
#include "programmercontroller.h"
#include "monitorproperties.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"
#include "show.h"
#include <functional>
#include "rgbplain.h"
#include "rgbmatrix.h"
#include "efx.h"
#include "track.h"
#include "showfunction.h"
#include <QThread>
#include <atomic>

#define TESTPLUGINDIR "../iopluginstub"

/* The circuits an Eos show patches as plain dimmers: a house light
   (Blackout + GM exempt), a stage dimmer (no rules), a power relay
   (Switch) and a work light (OnAtOpen). One channel each. */
enum { House = 10, Stage = 11, Relay = 12, Work = 13, Flame = 14 };

void OutputRules_Test::initTestCase()
{
    m_doc = new Doc(this);
    QDir dir(TESTPLUGINDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY(m_doc->ioPluginCache()->plugins().size() != 0);
    m_stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(m_doc->inputOutputMap()->setOutputPatch(0, m_stub->name(), QString(), 0));

    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    m_uni = ua.first();
    m_doc->inputOutputMap()->releaseUniverses(false);
}

void OutputRules_Test::cleanupTestCase()
{
    delete m_doc;
}

void OutputRules_Test::init()
{
    /* The universe of the moment: a show load that brings its own I/O
       replaces them, and a stale pointer here crashed cleanup()'s reset(). */
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    m_uni = ua.first();
    m_doc->inputOutputMap()->releaseUniverses(false);
    addDimmer("House", House, Fixture::ExemptBlackout | Fixture::ExemptGrandMaster);
    addDimmer("Stage", Stage, 0);
    addDimmer("Relay", Relay, Fixture::Switch);
    addDimmer("Work", Work, Fixture::OnAtOpen);
    m_doc->outputRuleKeeper()->rebuild();
    m_fader = m_uni->requestFader();
}

void OutputRules_Test::cleanup()
{
    stopFunctions();
    m_stub->m_dropUnchanged = false;
    for (int a = 0; a < 32; a++)
        m_uni->setChannelModifier(ushort(a), nullptr);
    m_doc->programmer()->focusSession()->stop();
    m_doc->lastLook()->clear();
    m_doc->testChannels()->setOn(false);
    m_uni->dismissFader(m_fader);
    m_fader.clear();
    m_doc->inputOutputMap()->setBlackout(false);
    m_doc->inputOutputMap()->setOutputInhibited(false);
    m_doc->inputOutputMap()->setGrandMasterValue(255);
    m_doc->inputOutputMap()->setGrandMasterChannelMode(GrandMaster::Intensity);
    m_doc->outputRuleKeeper()->setOnAtOpenWhen(OutputRuleKeeper::defaultOnAtOpenWhen());
    m_doc->outputRuleKeeper()->setBlindHoldsExempt(OutputRuleKeeper::defaultBlindHoldsExempt());
    m_doc->setMode(Doc::Design);
    m_doc->clearContents();
    m_doc->outputRuleKeeper()->rebuild();
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    m_uni = ua.first();
    /* Every fader let go: a stopped cue's fade-out (4 s in some tests) used
       to run on into the next tests -- one wrote the flame's channel every
       frame, and R8-2 passed in the suite only because of it. */
    for (Universe *u : ua)
        for (const QSharedPointer<GenericFader> &f : u->faders())
            u->dismissFader(f);
    m_doc->inputOutputMap()->releaseUniverses(false);
    m_uni->reset();
    for (int i = 0; i < 4; i++)
        m_uni->processFaders();
}

Fixture *OutputRules_Test::addDimmer(const QString &name, int address, int rulesMask)
{
    Fixture *f = new Fixture(m_doc);
    f->setName(name);
    f->setChannels(1);
    f->setAddress(quint32(address));
    f->setUniverse(0);
    f->setOutputRules(Fixture::OutputRules(rulesMask));
    if (m_doc->addFixture(f) == false)
    {
        delete f;
        return nullptr;
    }
    return f;
}

Fixture *OutputRules_Test::addSpot(const QString &name, int address)
{
    // One definition for the whole run (the cache owns it).
    QLCFixtureDef *def = m_spotDef;
    if (def == nullptr)
    {
        def = new QLCFixtureDef();
        def->setManufacturer(QStringLiteral("Test"));
        def->setModel(QStringLiteral("Ladder Spot"));
        QLCFixtureMode *mode = new QLCFixtureMode(def);
        mode->setName(QStringLiteral("2ch"));
        QLCChannel *dim = new QLCChannel();
        dim->setName(QStringLiteral("Dimmer"));
        dim->setGroup(QLCChannel::Intensity);
        def->addChannel(dim);
        mode->insertChannel(dim, 0);
        QLCChannel *cyan = new QLCChannel();
        cyan->setName(QStringLiteral("Cyan"));
        cyan->setGroup(QLCChannel::Colour);
        cyan->setColour(QLCChannel::Cyan);
        def->addChannel(cyan);
        mode->insertChannel(cyan, 1);
        def->addMode(mode);
        if (!m_doc->fixtureDefCache()->addFixtureDef(def))
        {
            delete def;
            return nullptr;
        }
        m_spotDef = def;
    }
    Fixture *f = new Fixture(m_doc);
    f->setName(name);
    f->setFixtureDefinition(def, def->modes().first());
    f->setUniverse(0);
    f->setAddress(quint32(address));
    if (m_doc->addFixture(f) == false)
    {
        delete f;
        return nullptr;
    }
    // On the plot, hung over the stage: Focus mode can light it.
    m_doc->monitorProperties()->setFixturePosition(f->id(), 0, 0, QVector3D(4000, 2000, 5));
    return f;
}

int OutputRules_Test::tickFrame(int address)
{
    m_doc->masterTimer()->timerTick();
    return frame(address);
}

void OutputRules_Test::startFocus(quint32 fid, float level)
{
    FocusSession *s = m_doc->programmer()->focusSession();
    s->setFlashEnabled(false);     // a steady light to read
    FocusSession::Options o;
    o.fixtures = { fid };
    o.level = level;
    QString why;
    QVERIFY2(s->start(o, &why), qPrintable(why));
}

void OutputRules_Test::checkGrandMasterAndBlackout(int address, int full)
{
    put(House, 180);                       // Ignore Blackout: proves the frame went out
    QCOMPARE(frame(address), full);
    QCOMPARE(int(uchar(m_stub->m_universe.at(address))), full);

    m_doc->inputOutputMap()->setGrandMasterValue(128);
    const int half = frame(address);
    QVERIFY2(qAbs(half - full / 2) <= 2, qPrintable(QString("%1 at half GM is %2").arg(full).arg(half)));
    QCOMPARE(int(uchar(m_stub->m_universe.at(address))), half);
    m_doc->inputOutputMap()->setGrandMasterValue(0);
    QCOMPARE(frame(address), 0);
    m_doc->inputOutputMap()->setGrandMasterValue(255);
    QCOMPARE(frame(address), full);

    m_doc->inputOutputMap()->setBlackout(true);
    frame(address);
    QCOMPARE(int(uchar(m_stub->m_universe.at(address))), 0);
    QCOMPARE(int(uchar(m_stub->m_universe.at(House))), 180);
    m_doc->inputOutputMap()->setBlackout(false);
    frame(address);
    QCOMPARE(int(uchar(m_stub->m_universe.at(address))), full);
}

int OutputRules_Test::frame(int address)
{
    m_uni->processFaders();
    return uchar(m_uni->postGMValues()->at(address));
}

void OutputRules_Test::put(int address, uchar value)
{
    GenericFader::ChannelRef fc = m_fader->getChannelFader(m_doc, m_uni, Fixture::invalidId(), quint32(address));
    fc->setStart(value);
    fc->setCurrent(value);
    fc->setTarget(value);
    fc.release();
    // A hand on a fader, moved now (Show only, S3): stamped in this session.
    m_fader->stampLiveEpoch();
}

void OutputRules_Test::rulesRoundTripAsText()
{
    const Fixture::OutputRules all = Fixture::ExemptBlackout | Fixture::ExemptGrandMaster
                                     | Fixture::Switch | Fixture::OnAtOpen;
    QCOMPARE(Fixture::outputRulesToString(all), QString("Blackout,GrandMaster,Switch,OnAtOpen"));
    QCOMPARE(Fixture::stringToOutputRules("Blackout,GrandMaster,Switch,OnAtOpen"), all);
    QCOMPARE(Fixture::stringToOutputRules(" Switch , Bogus"), Fixture::OutputRules(Fixture::Switch));
    QCOMPARE(Fixture::outputRulesToString(Fixture::OutputRules()), QString());
    QCOMPARE(m_doc->outputRuleKeeper()->ruleFixtureCount(), 3);
}

void OutputRules_Test::savedOnlyWhenSetAndSkippedByOlderReaders()
{
    auto save = [](Fixture *f) {
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter xw(&buf);
        f->saveXML(&xw);
        xw.writeEndDocument();
        return buf.data();
    };
    Fixture *house = m_doc->fixture(m_doc->fixtureForAddress(House));
    Fixture *stage = m_doc->fixture(m_doc->fixtureForAddress(Stage));
    QVERIFY(house != nullptr && stage != nullptr);
    const QByteArray withRules = save(house);
    QVERIFY(withRules.contains("<OutputRules>Blackout,GrandMaster</OutputRules>"));
    QVERIFY(save(stage).contains("OutputRules") == false);

    // Loads back.
    {
        QXmlStreamReader xr(withRules);
        xr.readNextStartElement();
        Fixture copy(m_doc);
        QVERIFY(copy.loadXML(xr, m_doc, m_doc->fixtureDefCache()));
        QCOMPARE(copy.outputRules(), Fixture::ExemptBlackout | Fixture::ExemptGrandMaster);
    }
    // A tag this build does not know is skipped, the rest still loads: what
    // an older build (or upstream QLC+) does with <OutputRules>.
    {
        QByteArray future = withRules;
        future.replace("<OutputRules>", "<SomethingNewer>x</SomethingNewer><OutputRules>");
        QXmlStreamReader xr(future);
        xr.readNextStartElement();
        Fixture copy(m_doc);
        QVERIFY(copy.loadXML(xr, m_doc, m_doc->fixtureDefCache()));
        QCOMPARE(copy.name(), QString("House"));
        QCOMPARE(copy.address(), quint32(House));
    }
}

void OutputRules_Test::grandMasterLeavesExemptAlone()
{
    put(House, 200);
    put(Stage, 200);
    QCOMPARE(frame(House), 200);
    QCOMPARE(frame(Stage), 200);

    m_doc->inputOutputMap()->setGrandMasterValue(0);
    QCOMPARE(frame(House), 200);
    QCOMPARE(frame(Stage), 0);
    m_doc->inputOutputMap()->setGrandMasterValue(128);
    QCOMPARE(frame(House), 200);
    QVERIFY(frame(Stage) > 90 && frame(Stage) < 110);
}

void OutputRules_Test::grandMasterAllChannelsMode()
{
    m_doc->inputOutputMap()->setGrandMasterChannelMode(GrandMaster::AllChannels);
    m_doc->inputOutputMap()->setGrandMasterValue(0);
    put(House, 77);
    put(Stage, 77);
    QCOMPARE(frame(House), 77);
    QCOMPARE(frame(Stage), 0);
}

void OutputRules_Test::blackoutKeepsExemptLive()
{
    put(House, 180);    // HTP intensity: Blackout zeroes these
    put(Stage, 180);
    frame(House);
    m_doc->inputOutputMap()->setBlackout(true);
    frame(House);
    const QByteArray wire = m_stub->m_universe.mid(0, 512);
    QCOMPARE(int(uchar(wire.at(House))), 180);
    QCOMPARE(int(uchar(wire.at(Stage))), 0);

    // Live, not frozen: the house light follows the show under Blackout.
    put(House, 90);
    frame(House);
    QCOMPARE(int(uchar(m_stub->m_universe.at(House))), 90);
    QCOMPARE(int(uchar(m_stub->m_universe.at(Stage))), 0);

    m_doc->inputOutputMap()->setBlackout(false);
    frame(House);
    QCOMPARE(int(uchar(m_stub->m_universe.at(Stage))), 180);
}

/* Park against the rest of the output (B1): a hard override of the cue,
   past the Grand Master, but Blackout still takes it out. */
void OutputRules_Test::parkBeatsACueAboveIt()
{
    const quint32 stage = m_doc->fixtureForAddress(Stage);
    put(Stage, 200);                       // a running cue
    QCOMPARE(frame(Stage), 200);

    {
        ParkEffect park(m_doc);
        park.parkFixture(stage, { { 0, 50 } });
        QCOMPARE(frame(Stage), 50);        // not HTP-merged: the park wins
        put(Stage, 255);
        QCOMPARE(frame(Stage), 50);

        // Let go: the cue's level is back on the very next frame.
        park.unparkFixture(stage);
        QCOMPARE(frame(Stage), 255);

        park.parkFixture(stage, { { 0, 50 } });
        QCOMPARE(frame(Stage), 50);
    }                                      // a park that goes away lets go too
    QCOMPARE(frame(Stage), 255);
}

void OutputRules_Test::parkIgnoresTheGrandMaster()
{
    Fixture *spare = addDimmer("Spare", 14, 0);
    QVERIFY(spare != nullptr);
    ParkEffect park(m_doc);
    park.parkFixture(m_doc->fixtureForAddress(Stage), { { 0, 200 } });
    put(14, 200);                          // the same level, not parked
    m_doc->inputOutputMap()->setGrandMasterValue(128);
    QCOMPARE(frame(Stage), 200);
    QCOMPARE(frame(14), 100);

    // Unparked, the Grand Master has it again.
    put(Stage, 200);
    park.unparkAll();
    QCOMPARE(frame(Stage), 100);
}

void OutputRules_Test::blackoutTakesAParkOut()
{
    ParkEffect park(m_doc);
    park.parkFixture(m_doc->fixtureForAddress(Stage), { { 0, 200 } });
    put(House, 180);
    QCOMPARE(frame(Stage), 200);
    m_doc->inputOutputMap()->setBlackout(true);
    frame(Stage);
    QCOMPARE(int(uchar(m_stub->m_universe.at(Stage))), 0);
    QCOMPARE(int(uchar(m_stub->m_universe.at(House))), 180);   // Ignore Blackout

    m_doc->inputOutputMap()->setBlackout(false);
    frame(Stage);
    QCOMPARE(int(uchar(m_stub->m_universe.at(Stage))), 200);   // still parked
}

/* Highlight, Flash and Focus are over everything that runs (B1), but only
   Park goes past the Grand Master: these three are scaled by it, and
   Blackout takes them out (Universe::FaderPriority, CHANGELOG "Park,
   Highlight and a held last look hold their dimmers"). */
void OutputRules_Test::theGrandMasterAndBlackoutApplyToAHighlight()
{
    HighlightEffect highlight(m_doc);
    highlight.setFixtures({ m_doc->fixtureForAddress(Stage) });
    checkGrandMasterAndBlackout(Stage, 255);
    highlight.setFixtures({});
    QCOMPARE(frame(Stage), 0);
}

void OutputRules_Test::theGrandMasterAndBlackoutApplyToAFlash()
{
    ProgrammerFlasher flash(m_doc);
    // Long enough never to end by the clock here: let go by going away.
    flash.flashFixture(m_doc->fixtureForAddress(Stage), 600000);
    QCOMPARE(tickFrame(Stage), 255);       // dark: flashed up
    checkGrandMasterAndBlackout(Stage, 255);
}

void OutputRules_Test::theGrandMasterAndBlackoutApplyToFocus()
{
    const quint32 stage = m_doc->fixtureForAddress(Stage);
    m_doc->monitorProperties()->setFixturePosition(stage, 0, 0, QVector3D(4000, 2000, 5));
    startFocus(stage, 1.0f);
    checkGrandMasterAndBlackout(Stage, 255);
    m_doc->programmer()->focusSession()->stop();
    QCOMPARE(frame(Stage), 0);
}

/* The ladder (Universe::FaderPriority): Park, Focus, Flash, Highlight, then
   what runs, and a held last look under all of it. Each pair on one light:
   the higher one goes out, and letting go of it shows the one under it. */
void OutputRules_Test::parkBeatsFocusAndFlash()
{
    Fixture *spot = addSpot("Spot", 20);
    QVERIFY(spot != nullptr);
    const int dim = 20;
    ParkEffect park(m_doc);

    startFocus(spot->id(), 0.6f);
    QCOMPARE(frame(dim), 153);
    park.parkFixture(spot->id(), { { 0, 50 } });
    QCOMPARE(frame(dim), 50);
    park.unparkFixture(spot->id());
    QCOMPARE(frame(dim), 153);             // Focus again
    m_doc->programmer()->focusSession()->stop();
    QCOMPARE(frame(dim), 0);

    {
        ProgrammerFlasher flash(m_doc);
        flash.flashFixture(spot->id(), 600000);
        QCOMPARE(tickFrame(dim), 255);
        park.parkFixture(spot->id(), { { 0, 50 } });
        QCOMPARE(tickFrame(dim), 50);
        park.unparkFixture(spot->id());
        QCOMPARE(tickFrame(dim), 255);     // the flash again
    }
    QCOMPARE(tickFrame(dim), 0);
}

void OutputRules_Test::focusAndFlashBeatHighlight()
{
    Fixture *spot = addSpot("Spot", 20);
    QVERIFY(spot != nullptr);
    const int dim = 20;
    HighlightEffect highlight(m_doc);
    highlight.setFixtures({ spot->id() });
    QCOMPARE(frame(dim), 255);

    // Focus over Highlight.
    startFocus(spot->id(), 0.3f);
    QCOMPARE(frame(dim), 77);
    m_doc->programmer()->focusSession()->stop();
    QCOMPARE(frame(dim), 255);             // the highlight again

    // Flash over Highlight: the light is up, so the flash takes it down.
    {
        ProgrammerFlasher flash(m_doc);
        flash.flashFixture(spot->id(), 600000);
        QCOMPARE(tickFrame(dim), 0);
    }
    QCOMPARE(tickFrame(dim), 255);
    highlight.setFixtures({});
    QCOMPARE(frame(dim), 0);

    // Focus over Flash.
    {
        ProgrammerFlasher flash(m_doc);
        flash.flashFixture(spot->id(), 600000);
        QCOMPARE(tickFrame(dim), 255);
        startFocus(spot->id(), 0.3f);
        QCOMPARE(tickFrame(dim), 77);
        m_doc->programmer()->focusSession()->stop();
        QCOMPARE(tickFrame(dim), 255);     // the flash again
    }
    QCOMPARE(tickFrame(dim), 0);
}

void OutputRules_Test::highlightBeatsARunningCue()
{
    Fixture *spot = addSpot("Spot", 20);
    QVERIFY(spot != nullptr);
    put(20, 100);                          // a running cue: dimmer and colour
    put(21, 200);
    QCOMPARE(frame(20), 100);
    QCOMPARE(int(uchar(m_uni->postGMValues()->at(21))), 200);

    HighlightEffect highlight(m_doc);
    highlight.setFixtures({ spot->id() });
    QCOMPARE(frame(20), 255);
    QCOMPARE(int(uchar(m_uni->postGMValues()->at(21))), 0);    // open white: no cyan
    QCOMPARE(int(uchar(m_stub->m_universe.at(21))), 0);
    highlight.setFixtures({});
    QCOMPARE(frame(20), 100);
    QCOMPARE(int(uchar(m_uni->postGMValues()->at(21))), 200);
}

void OutputRules_Test::aHeldLastLookIsUnderEverything()
{
    Fixture *spot = addSpot("Spot", 20);
    QVERIFY(spot != nullptr);
    const int dim = 20, cyan = 21;
    auto at = [this](int address) { return int(uchar(m_uni->postGMValues()->at(address))); };
    LastLookEffect *hold = m_doc->lastLook();
    LastLookEffect::ChannelHold hd;
    hd.fixtureId = spot->id();
    hd.universeId = 0;
    hd.address = dim;
    hd.value = 200;
    LastLookEffect::ChannelHold hc = hd;
    hc.address = cyan;
    hold->hold({ hd, hc });
    QCOMPARE(frame(dim), 200);
    QCOMPARE(at(cyan), 200);

    // A cue takes the colour it plays.
    put(cyan, 90);
    frame(dim);
    QCOMPARE(at(cyan), 90);
    m_uni->dismissFader(m_fader);
    m_fader = m_uni->requestFader();
    frame(dim);
    QCOMPARE(frame(dim), 200);
    QCOMPARE(at(cyan), 200);

    // Highlight.
    {
        HighlightEffect highlight(m_doc);
        highlight.setFixtures({ spot->id() });
        QCOMPARE(frame(dim), 255);
        QCOMPARE(at(cyan), 0);
    }
    QCOMPARE(frame(dim), 200);
    QCOMPARE(at(cyan), 200);

    // Flash: the look is up, so the flash takes it down.
    {
        ProgrammerFlasher flash(m_doc);
        flash.flashFixture(spot->id(), 600000);
        QCOMPARE(tickFrame(dim), 0);
    }
    QCOMPARE(tickFrame(dim), 200);

    // Focus.
    startFocus(spot->id(), 0.3f);
    QCOMPARE(frame(dim), 77);
    m_doc->programmer()->focusSession()->stop();
    QCOMPARE(frame(dim), 200);

    // Park.
    {
        ParkEffect park(m_doc);
        park.parkFixture(spot->id(), { { 0, 50 } });
        QCOMPARE(frame(dim), 50);
    }
    QCOMPARE(frame(dim), 200);
    QCOMPARE(at(cyan), 200);
}

void OutputRules_Test::switchSnapsAtHalf()
{
    put(Relay, 102);    // 40%
    QCOMPARE(frame(Relay), 0);
    put(Relay, 153);    // 60%
    QCOMPARE(frame(Relay), 255);
    put(Relay, 127);
    QCOMPARE(frame(Relay), 0);
    put(Relay, 128);
    QCOMPARE(frame(Relay), 255);
    // What the show asked for is kept; only what goes out is snapped.
    QCOMPARE(int(m_uni->preGMValue(Relay)), 128);
}

void OutputRules_Test::switchNeverFades()
{
    Fixture *relay = m_doc->fixture(m_doc->fixtureForAddress(Relay));
    Fixture *stage = m_doc->fixture(m_doc->fixtureForAddress(Stage));

    // A 10 s fade-in, as a scene sets it up: the relay goes at once, the
    // dimmer beside it fades.
    auto fadeIn = [this](Fixture *f) {
        GenericFader::ChannelRef fc = m_fader->getChannelFader(m_doc, m_uni, f->id(), 0);
        fc->setStart(0);
        fc->setCurrent(0);
        fc->setTarget(255);
        fc->setElapsed(0);
        fc->setReady(false);
        fc->setFadeTime(fc->canFade() ? 10000 : 0);
        return fc->canFade();
    };
    QVERIFY(fadeIn(relay) == false);
    QVERIFY(fadeIn(stage));
    QCOMPARE(frame(Relay), 255);
    QVERIFY(frame(Stage) < 50);
}

void OutputRules_Test::grandMasterCannotSwitchARelayOff()
{
    put(Relay, 255);
    m_doc->inputOutputMap()->setGrandMasterValue(77);   // 30%
    QCOMPARE(frame(Relay), 255);
    m_doc->inputOutputMap()->setGrandMasterValue(0);
    QCOMPARE(frame(Relay), 255);
    m_doc->inputOutputMap()->setGrandMasterValueMode(GrandMaster::Limit);
    QCOMPARE(frame(Relay), 255);
    m_doc->inputOutputMap()->setGrandMasterValueMode(GrandMaster::Reduce);
}

void OutputRules_Test::powerLimitsLeaveARelayAlone()
{
    Universe::PowerLimit lim;
    lim.fixtureId = 1;
    lim.base = Relay;
    lim.model.emitters << PowerModel::Emitter{ 0, 1.0, {} }
                       << PowerModel::Emitter{ Stage - Relay, 1.0, {} };
    lim.model.maxAmps = 1.0;    // asks 2 A at full: held to half
    QByteArray out(512, char(0));
    out[Relay] = char(255);
    out[Stage] = char(255);
    // Whatever the model asks, the relay channel itself is never scaled.
    m_uni->m_powerLimits.clear();
    m_uni->m_powerLimits << lim;
    QVERIFY(m_uni->applyPowerLimits(out));
    QCOMPARE(int(uchar(out.at(Relay))), 255);
    QVERIFY(uchar(out.at(Stage)) < 200);
    m_uni->m_powerLimits.clear();
}

void OutputRules_Test::onAtOpenFollowsItsSetting()
{
    OutputRuleKeeper *k = m_doc->outputRuleKeeper();
    k->setOnAtOpenWhen(OutputRuleKeeper::OnAtOperate);
    QCOMPARE(k->onAtOpenWhen(), OutputRuleKeeper::OnAtOperate);

    // Design, never been in Operate: dark.
    QVERIFY(k->onAtOpenArmed() == false);
    QCOMPARE(frame(Work), 0);
    // To Operate: on.
    m_doc->setMode(Doc::Operate);
    QVERIFY(k->onAtOpenArmed());
    QCOMPARE(frame(Work), 255);
    // Back to Design: stays on (nobody is left in the dark).
    m_doc->setMode(Doc::Design);
    QCOMPARE(frame(Work), 255);

    // "When the show opens": on before any Operate.
    m_doc->clearContents();
    m_doc->outputRuleKeeper()->rebuild();
    QVERIFY(k->onAtOpenArmed() == false);
    addDimmer("Work", Work, Fixture::OnAtOpen);
    m_doc->outputRuleKeeper()->rebuild();
    QCOMPARE(frame(Work), 0);
    k->setOnAtOpenWhen(OutputRuleKeeper::OnAtShowOpen);
    QVERIFY(m_doc->isModified());
    QCOMPARE(frame(Work), 255);
    // And back: off again until Operate.
    k->setOnAtOpenWhen(OutputRuleKeeper::OnAtOperate);
    QCOMPARE(frame(Work), 0);
    k->setOnAtOpenWhen(OutputRuleKeeper::OnAtShowOpen);
    QCOMPARE(frame(Work), 255);

    // The Grand Master scales it (it is not GM-exempt).
    m_doc->inputOutputMap()->setGrandMasterValue(0);
    QCOMPARE(frame(Work), 0);
    m_doc->inputOutputMap()->setGrandMasterValue(255);

    // A new show: the default setting, and waits for its own Operate.
    m_doc->clearContents();
    QCOMPARE(k->onAtOpenWhen(), OutputRuleKeeper::defaultOnAtOpenWhen());
    QVERIFY(k->onAtOpenArmed() == false);
}

/* Moving the Grand Master recomputes the output straight away (no frame in
   between): an On-when-not-in-use relay must not read 0 there, not even
   for one frame. */
void OutputRules_Test::grandMasterNeverDropsAnOnAtOpenRelay()
{
    QVERIFY(addDimmer("Pump", 15, Fixture::OnAtOpen | Fixture::Switch) != nullptr);
    QVERIFY(addDimmer("Fans", 16, Fixture::OnAtOpen | Fixture::Switch
                                  | Fixture::ExemptGrandMaster) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);                       // arms On when not in use
    QVERIFY(m_doc->outputRuleKeeper()->onAtOpenArmed());
    QCOMPARE(frame(15), 255);
    QCOMPARE(int(uchar(m_stub->m_universe.at(15))), 255);

    auto out = [this](int address) { return int(uchar(m_uni->postGMValues()->at(address))); };
    for (uchar gm : { uchar(255), uchar(77), uchar(0), uchar(255) })
    {
        m_doc->inputOutputMap()->setGrandMasterValue(gm);
        // Before any frame: what the GM change itself left.
        QCOMPARE(out(15), 255);
        QCOMPARE(out(16), 255);
        QCOMPARE(out(Work), gm == 0 ? 0 : int(gm));     // a plain one dims with the GM
        // And what goes out on the next one.
        frame(15);
        QCOMPARE(int(uchar(m_stub->m_universe.at(15))), 255);
        QCOMPARE(int(uchar(m_stub->m_universe.at(16))), 255);
    }
}

void OutputRules_Test::onAtOpenSettingIsSavedWithTheShow()
{
    OutputRuleKeeper *k = m_doc->outputRuleKeeper();
    auto save = [k]() {
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter xw(&buf);
        k->saveXML(&xw);
        xw.writeEndDocument();
        return buf.data();
    };
    // The default (Operate) writes nothing: shows save as they did.
    k->setOnAtOpenWhen(OutputRuleKeeper::OnAtOperate);
    QVERIFY(save().contains("OutputRuleSettings") == false);

    k->setOnAtOpenWhen(OutputRuleKeeper::OnAtShowOpen);
    const QByteArray xml = save();
    QVERIFY(xml.contains("<OutputRuleSettings OnAtOpen=\"ShowOpen\"/>"));

    k->setOnAtOpenWhen(OutputRuleKeeper::OnAtOperate);
    QXmlStreamReader xr(xml);
    xr.readNextStartElement();
    k->loadXML(xr);
    QCOMPARE(k->onAtOpenWhen(), OutputRuleKeeper::OnAtShowOpen);
    QVERIFY(k->onAtOpenArmed());

    // Through a whole show file too.
    QBuffer show;
    show.open(QIODevice::WriteOnly);
    {
        QXmlStreamWriter xw(&show);
        xw.writeStartElement("Workspace");
        m_doc->saveXML(&xw);
        xw.writeEndElement();
        xw.writeEndDocument();
    }
    QVERIFY(show.data().contains("OutputRuleSettings"));
    m_doc->clearContents();
    QCOMPARE(k->onAtOpenWhen(), OutputRuleKeeper::OnAtOperate);
    QXmlStreamReader sr(show.data());
    sr.readNextStartElement();      // Workspace
    sr.readNextStartElement();      // Engine
    QVERIFY(m_doc->loadXML(sr, false));   // the universes stay (m_uni)
    QCOMPARE(k->onAtOpenWhen(), OutputRuleKeeper::OnAtShowOpen);
}

void OutputRules_Test::onAtOpenYieldsToTheShow()
{
    m_doc->outputRuleKeeper()->setOnAtOpenWhen(OutputRuleKeeper::OnAtShowOpen);
    QCOMPARE(frame(Work), 255);
    put(Work, 0);       // the show takes it out
    QCOMPARE(frame(Work), 0);
    put(Work, 60);
    QCOMPARE(frame(Work), 60);
    m_uni->dismissFader(m_fader);   // the show lets go: back to full
    m_fader = m_uni->requestFader();
    frame(Work);
    frame(Work);
    QCOMPARE(frame(Work), 255);
}

void OutputRules_Test::blindFollowsItsSetting()
{
    m_doc->outputRuleKeeper()->setBlindHoldsExempt(true);
    put(House, 200);
    put(Stage, 200);
    frame(House);
    m_stub->m_universe.fill(char(7));
    m_doc->inputOutputMap()->setOutputInhibited(true);
    frame(House);
    QCOMPARE(int(uchar(m_stub->m_universe.at(House))), 200);
    QCOMPARE(int(uchar(m_stub->m_universe.at(Stage))), 0);

    // Off: Blind sends nothing at all, as before.
    m_doc->outputRuleKeeper()->setBlindHoldsExempt(false);
    m_stub->m_universe.fill(char(7));
    put(House, 100);
    frame(House);
    QCOMPARE(int(uchar(m_stub->m_universe.at(House))), 7);
    QCOMPARE(int(uchar(m_stub->m_universe.at(Stage))), 7);
}

void OutputRules_Test::unpatchClearsTheBits()
{
    const quint32 id = m_doc->fixtureForAddress(House);
    QVERIFY(m_uni->channelCapabilities(House) & Universe::GMExempt);
    QVERIFY(m_uni->channelCapabilities(House) & Universe::BlackoutExempt);
    QVERIFY(m_uni->channelCapabilities(House) & Universe::HTP);
    QVERIFY(m_doc->unpatchFixture(id));
    m_doc->outputRuleKeeper()->rebuild();
    QCOMPARE(int(m_uni->channelCapabilities(House) & Universe::OutputRuleBits), 0);

    // Rules off on a patched one clear too.
    Fixture *relay = m_doc->fixture(m_doc->fixtureForAddress(Relay));
    relay->setOutputRules(Fixture::OutputRules());
    m_doc->outputRuleKeeper()->rebuild();
    QCOMPARE(int(m_uni->channelCapabilities(Relay) & Universe::OutputRuleBits), 0);
    put(Relay, 100);
    QCOMPARE(frame(Relay), 100);

    // And deleting.
    QVERIFY(m_doc->deleteFixture(m_doc->fixtureForAddress(Work)));
    m_doc->outputRuleKeeper()->rebuild();
    QCOMPARE(int(m_uni->channelCapabilities(Work) & Universe::OutputRuleBits), 0);
}

void OutputRules_Test::movedFixtureTakesItsBits()
{
    const quint32 id = m_doc->fixtureForAddress(House);
    QVERIFY(m_doc->patchFixture(id, 0, 100));
    m_doc->outputRuleKeeper()->rebuild();
    QCOMPARE(int(m_uni->channelCapabilities(House) & Universe::OutputRuleBits), 0);
    QVERIFY(m_uni->channelCapabilities(100) & Universe::GMExempt);
}

void OutputRules_Test::cloneAndCapabilityKeepTheRules()
{
    // Re-applying a channel's role (a patch, a forced HTP) keeps its rules.
    m_uni->setChannelCapability(House, QLCChannel::Intensity);
    QVERIFY(m_uni->channelCapabilities(House) & Universe::GMExempt);
    m_uni->setChannelCapability(House, QLCChannel::Intensity, Universe::LTP);
    QVERIFY(m_uni->channelCapabilities(House) & Universe::BlackoutExempt);
    QCOMPARE(int(m_uni->channelCapabilities(House) & (Universe::LTP | Universe::HTP)), int(Universe::LTP));

    // replaceFixtures (the fixture remap's engine side) copies them.
    QList<Fixture *> list;
    foreach (Fixture *f, m_doc->fixtures())
    {
        Fixture *c = new Fixture(m_doc);
        c->setID(f->id());
        c->setName(f->name());
        c->setChannels(f->channels());
        c->setAddress(f->address());
        c->setUniverse(f->universe());
        c->setOutputRules(f->outputRules());
        list << c;
    }
    QVERIFY(m_doc->replaceFixtures(list));
    qDeleteAll(list);
    Fixture *relay = m_doc->fixture(m_doc->fixtureForAddress(Relay));
    QVERIFY(relay != nullptr);
    QCOMPARE(relay->outputRules(), Fixture::OutputRules(Fixture::Switch));
}


/****************************************************************************
 * Show only
 ****************************************************************************/

Scene *OutputRules_Test::startScene(const QList<QPair<int, int> > &values, quint32 parentType, quint32 parentId)
{
    Scene *s = new Scene(m_doc);
    for (const QPair<int, int> &v : values)
    {
        const quint32 fid = m_doc->fixtureForAddress(quint32(v.first));
        Fixture *fx = m_doc->fixture(fid);
        if (fx == nullptr)
            continue;
        s->setValue(fid, quint32(v.first) - fx->address(), uchar(v.second));
    }
    m_doc->addFunction(s);
    s->start(m_doc->masterTimer(), FunctionParent(FunctionParent::Type(parentType), parentId));
    return s;
}

void OutputRules_Test::stopFunctions()
{
    // Until nothing runs: a collection lets its members go a tick later.
    for (int i = 0; i < 20; i++)
    {
        foreach (Function *f, m_doc->functions())
            if (f->isRunning())
                f->stop(FunctionParent::master());
        m_doc->masterTimer()->timerTick();
        if (m_doc->masterTimer()->runningFunctions() == 0)
            break;
    }
    // And their faders gone (a fade out, a dismissed fader's last frame).
    for (int i = 0; i < 4; i++)
        m_uni->processFaders();
}

int OutputRules_Test::sent(int address) const
{
    return int(uchar(m_stub->m_universe.at(address)));
}

void OutputRules_Test::showOnlySavedAndLoaded()
{
    QCOMPARE(Fixture::outputRulesToString(Fixture::ShowOnly | Fixture::Switch), QString("Switch,ShowOnly"));
    QCOMPARE(Fixture::stringToOutputRules("Switch,ShowOnly"), Fixture::ShowOnly | Fixture::Switch);
    Fixture *flame = addDimmer("Flame", Flame, Fixture::ShowOnly | Fixture::Switch);
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter xw(&buf);
    flame->saveXML(&xw);
    xw.writeEndDocument();
    QVERIFY(buf.data().contains("<OutputRules>Switch,ShowOnly</OutputRules>"));
    QXmlStreamReader xr(buf.data());
    xr.readNextStartElement();
    Fixture copy(m_doc);
    QVERIFY(copy.loadXML(xr, m_doc, m_doc->fixtureDefCache()));
    QCOMPARE(copy.outputRules(), Fixture::ShowOnly | Fixture::Switch);
    QCOMPARE(OutputRuleKeeper::describe(Fixture::ShowOnly),
             QString("Show only (held outside a real run in Operate)"));
}

void OutputRules_Test::designHoldsAShowOnlyRelay()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly | Fixture::Switch);
    m_doc->outputRuleKeeper()->rebuild();
    QCOMPARE(m_uni->showOnlyChannelCount(), 1);
    QCOMPARE(m_doc->mode(), Doc::Design);

    // A cue tried out in Design -- from the Run tab, a real parent -- and
    // the Workbench's preview alike: the stage light previews, the flame
    // relay stays open.
    startScene({ { Stage, 200 }, { Flame, 255 } }, FunctionParent::ManualVCWidget, 1);
    QCOMPARE(tickFrame(Stage), 200);
    QCOMPARE(sent(Stage), 200);
    QCOMPARE(sent(Flame), 0);
    // The plot draws what it would do.
    QCOMPARE(int(uchar(m_uni->postGMValues()->at(Flame))), 255);
    QVERIFY(m_uni->showOnlyHeld());

    startScene({ { Flame, 255 } }, FunctionParent::AutoVCWidget, 0xFFFFFF57);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 0);
    stopFunctions();
}

void OutputRules_Test::operateGoDrivesItAndDesignTakesItBack()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly | Fixture::Switch);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_uni->showOnlyLive());
    startScene({ { Stage, 120 }, { Flame, 255 } }, FunctionParent::ManualVCWidget, 1);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 255);
    QCOMPARE(sent(Stage), 120);
    QVERIFY(m_uni->showOnlyHeld() == false);

    // Back to Design, the cue still running: safe on the next frame.
    m_doc->setMode(Doc::Design);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 0);
    QCOMPARE(sent(Stage), 120);
    stopFunctions();
}

void OutputRules_Test::operatePreviewIsKeptOffIt()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly | Fixture::Switch);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    // The Workbench, live in Operate (its preview parent).
    const FunctionParent workbench = FunctionParent::preview(0x7);
    Scene *s = startScene({ { Stage, 90 }, { Flame, 255 } }, workbench.type(), workbench.id());
    tickFrame(Flame);
    QCOMPARE(sent(Stage), 90);
    QCOMPARE(sent(Flame), 0);
    QCOMPARE(int(uchar(m_uni->postGMValues()->at(Flame))), 0);  // not even stored

    // The same scene from a source that is the show (an automatic widget's,
    // armed by the operator in Operate: R6-3): it is the show now.
    const FunctionParent go(FunctionParent::AutoVCWidget, 2);
    Function::armAutoSource(go);
    s->start(m_doc->masterTimer(), go);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 255);
    // The GO lets go, the preview plays on: held again.
    s->stop(go);
    QVERIFY(s->stopped() == false);
    tickFrame(Flame);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 0);
    QCOMPARE(sent(Stage), 90);
    stopFunctions();
}

void OutputRules_Test::operatePreviewChildIsKeptOffIt()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly | Fixture::Switch);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    Scene *step = new Scene(m_doc);
    step->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    step->setValue(m_doc->fixtureForAddress(Stage), 0, 70);
    m_doc->addFunction(step);
    Chaser *c = new Chaser(m_doc);
    ChaserStep cs(step->id());
    cs.hold = 100000;
    c->addStep(cs);
    m_doc->addFunction(c);
    c->start(m_doc->masterTimer(), FunctionParent::preview(0x5));
    for (int i = 0; i < 4; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Stage), 70);
    QCOMPARE(sent(Flame), 0);
    stopFunctions();
}

void OutputRules_Test::highlightFocusAndTestsDontDriveIt()
{
    Fixture *spot = addSpot("Laser-ish Spot", 20);
    QVERIFY(spot != nullptr);
    spot->setOutputRules(Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);

    HighlightEffect highlight(m_doc);
    highlight.setFixtures({ spot->id() });
    frame(20);
    QCOMPARE(sent(20), 0);
    highlight.setFixtures({});

    FocusSession *f = m_doc->programmer()->focusSession();
    f->setFlashEnabled(false);
    FocusSession::Options o;
    o.fixtures = { spot->id() };
    o.level = 1.0f;
    o.allowOperate = true;
    QString why;
    QVERIFY2(f->start(o, &why), qPrintable(why));
    tickFrame(20);
    QCOMPARE(sent(20), 0);
    f->stop();

    // A fader its owner marks (Locate, a fixture test, the programmer)...
    QSharedPointer<GenericFader> test = m_uni->requestFader(Universe::Override);
    test->setHoldsShowOnly(true);
    {
        GenericFader::ChannelRef fc = test->getChannelFader(m_doc, m_uni, spot->id(), 0);
        fc->setStart(255); fc->setCurrent(255); fc->setTarget(255);
    }
    frame(20);
    QCOMPARE(sent(20), 0);
    m_uni->dismissFader(test);
    // ...and one it doesn't (the Simple Desk: a hand on the fader) drives it.
    put(20, 180);
    frame(20);
    QCOMPARE(sent(20), 180);
}

void OutputRules_Test::blindHoldsIt()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly | Fixture::Switch | Fixture::ExemptBlackout);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    put(House, 200);
    put(Flame, 255);
    frame(Flame);
    QCOMPARE(sent(Flame), 255);
    m_doc->inputOutputMap()->setOutputInhibited(true);
    QVERIFY(m_uni->showOnlyHeld());
    frame(Flame);
    QCOMPARE(sent(House), 200);     // Blackout-exempt: still live through Blind
    QCOMPARE(sent(Flame), 0);       // Show only: Blind holds it
    QCOMPARE(m_doc->outputRuleKeeper()->showOnlyHeldBy(), OutputRuleKeeper::HeldByBlind);
    m_doc->inputOutputMap()->setOutputInhibited(false);
    frame(Flame);
    QCOMPARE(sent(Flame), 0);       // R7-4: Blind ended, a new session -- set again to drive
    put(Flame, 255);
    frame(Flame);
    QCOMPARE(sent(Flame), 255);
}

void OutputRules_Test::goingLiveStartsFromSafe()
{
    Fixture *spot = addSpot("Curtain-ish", 20);
    spot->setOutputRules(Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    // Design: something leaves the colour (LTP) channel at 200, stored.
    put(21, 200);
    frame(21);
    QCOMPARE(sent(21), 0);
    QCOMPARE(int(uchar(m_uni->postGMValues()->at(21))), 200);
    m_uni->dismissFader(m_fader);
    m_fader = m_uni->requestFader();
    frame(21);
    QCOMPARE(int(uchar(m_uni->postGMValues()->at(21))), 200);   // LTP: kept
    // Into Operate with nothing playing it: safe, not the leftover.
    m_doc->setMode(Doc::Operate);
    frame(21);
    QCOMPARE(sent(21), 0);
    QCOMPARE(int(uchar(m_uni->postGMValues()->at(21))), 0);
}

void OutputRules_Test::safeValuePerKind()
{
    Fixture *spot = addSpot("Spot", 20);
    // A dimmer is dark; a colour channel at its default (0 here).
    QCOMPARE(int(OutputRuleKeeper::safeValue(spot, 0)), 0);
    QCOMPARE(int(OutputRuleKeeper::safeValue(spot, 1)), 0);
    QCOMPARE(int(OutputRuleKeeper::safeValue(nullptr, 0)), 0);
    // A plain dimmer (no definition) is all output: 0.
    Fixture *flame = addDimmer("Flame", Flame, Fixture::ShowOnly);
    QCOMPARE(int(OutputRuleKeeper::safeValue(flame, 0)), 0);
    m_doc->outputRuleKeeper()->rebuild();
    QCOMPARE(int(uchar(m_doc->outputRuleKeeper()->safeValuesFor(0).at(Flame))), 0);
}

void OutputRules_Test::heldTextSaysWhy()
{
    OutputRuleKeeper *k = m_doc->outputRuleKeeper();
    QVERIFY(k->showOnlyHeldText().isEmpty());       // none: nothing to say
    addDimmer("Flame L", Flame, Fixture::ShowOnly);
    k->rebuild();
    QSignalSpy spy(k, &OutputRuleKeeper::showOnlyChanged);
    QCOMPARE(k->showOnlyHeldText(), QString("Flame L held: Design"));
    m_doc->setMode(Doc::Operate);
    QVERIFY(spy.count() >= 1);
    QVERIFY(k->showOnlyHeldText().isEmpty());
    m_doc->inputOutputMap()->setOutputInhibited(true);
    QCOMPARE(k->showOnlyHeldText(), QString("Flame L held: Blind"));
    m_doc->inputOutputMap()->setOutputInhibited(false);
    addDimmer("Flame R", Flame + 1, Fixture::ShowOnly);
    k->rebuild();
    m_doc->setMode(Doc::Design);
    QCOMPARE(k->showOnlyHeldText(), QString("2 Show-only fixtures held: Design"));
}

void OutputRules_Test::showWithoutItIsUnchanged()
{
    QCOMPARE(m_uni->showOnlyChannelCount(), 0);
    QVERIFY(m_uni->showOnlyHeld() == false);
    QVERIFY(m_doc->outputRuleKeeper()->showOnlyFixtures().isEmpty());
    put(Stage, 150);
    frame(Stage);
    QCOMPARE(sent(Stage), 150);
    // A preview's scene in Operate writes as it always did.
    addDimmer("Plain", Flame, 0);
    m_doc->outputRuleKeeper()->rebuild();
    QCOMPARE(m_uni->showOnlyChannelCount(), 0);
    m_doc->setMode(Doc::Operate);
    const FunctionParent workbench = FunctionParent::preview(0x7);
    startScene({ { Flame, 77 } }, workbench.type(), workbench.id());
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 77);
    stopFunctions();
}

/****************************************************************************
 * Show only: the safety review's items
 ****************************************************************************/

void OutputRules_Test::blackoutAndBlindNeverSendAPreviewsValue()
{
    Fixture *spot = addSpot("Laser-ish", 20);
    spot->setOutputRules(Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    // A preview (the Workbench) sets its LTP colour channel.
    QSharedPointer<GenericFader> preview = m_uni->requestFader(Universe::Override);
    preview->setHoldsShowOnly(true);
    {
        GenericFader::ChannelRef fc = preview->getChannelFader(m_doc, m_uni, spot->id(), 1);
        fc->setStart(200); fc->setCurrent(200); fc->setTarget(200);
    }
    frame(21);
    QCOMPARE(sent(21), 0);
    m_doc->inputOutputMap()->setBlackout(true);
    frame(21);
    QCOMPARE(sent(21), 0);
    m_doc->inputOutputMap()->setBlackout(false);
    // Blind with a house light exempt: Blind sends Blackout's frame.
    put(House, 200);
    m_doc->inputOutputMap()->setOutputInhibited(true);
    frame(21);
    QCOMPARE(sent(House), 200);
    QCOMPARE(sent(21), 0);
    m_doc->inputOutputMap()->setOutputInhibited(false);
    m_uni->dismissFader(preview);
}

void OutputRules_Test::everySendHoldsIt()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    put(Flame, 255);                     // Design: stored, held
    frame(Flame);
    QCOMPARE(sent(Flame), 0);
    m_stub->m_universe.fill(char(7));
    m_doc->inputOutputMap()->setBlackout(true);
    QCOMPARE(sent(Flame), 0);
    m_doc->inputOutputMap()->setBlackout(false);   // sends the stored frame
    QCOMPARE(sent(Flame), 0);
    m_doc->inputOutputMap()->setOutputInhibited(true);
    m_doc->inputOutputMap()->setOutputInhibited(false);   // leaving Blind sends too
    QCOMPARE(sent(Flame), 0);
    // A plugin that sends only changes: the next frame is not a leak
    // repeated, and going live is a changed frame.
    m_stub->m_dropUnchanged = true;
    frame(Flame);
    QCOMPARE(sent(Flame), 0);
    m_doc->setMode(Doc::Operate);
    put(Flame, 255);                    // set in Operate (S3): set in Design is not the show
    frame(Flame);
    QVERIFY(m_stub->m_lastDataChanged);
    QCOMPARE(sent(Flame), 255);
    m_doc->setMode(Doc::Design);
    frame(Flame);
    QVERIFY(m_stub->m_lastDataChanged);
    QCOMPARE(sent(Flame), 0);
}

void OutputRules_Test::aPreviewsFadeOutNeverReachesIt()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    const FunctionParent workbench = FunctionParent::preview(0x7);
    Scene *s = startScene({ { Stage, 200 }, { Flame, 255 } }, workbench.type(), workbench.id());
    s->setFadeOutSpeed(300);        // over by the end: nothing left for the next test
    tickFrame(Flame);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 0);
    QCOMPARE(sent(Stage), 200);
    s->stop(workbench);
    int stageMax = 0;
    for (int i = 0; i < 30; i++)
    {
        tickFrame(Flame);
        QCOMPARE(sent(Flame), 0);
        stageMax = qMax(stageMax, sent(Stage));
    }
    QVERIFY(stageMax > 0);      // the rest did fade out
    QCOMPARE(sent(Stage), 0);
}

void OutputRules_Test::aSwitchMidFrameSendsNoPreview()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    QCOMPARE(m_doc->mode(), Doc::Design);
    // A preview's fader; Operate arrives while the universe writes it.
    QSharedPointer<GenericFader> preview = m_uni->requestFader(Universe::Override);
    preview->setHoldsShowOnly(true);
    {
        GenericFader::ChannelRef fc = preview->getChannelFader(m_doc, m_uni, Fixture::invalidId(), Flame);
        fc->setStart(255); fc->setCurrent(255); fc->setTarget(255);
    }
    preview->setMonitoring(true);
    Universe *uni = m_uni;
    QMetaObject::Connection c = connect(preview.data(), &GenericFader::preWriteData, this,
                                        [uni](quint32, const QByteArray &) { uni->setShowOnlyLive(true); },
                                        Qt::DirectConnection);
    frame(Flame);
    disconnect(c);
    QCOMPARE(sent(Flame), 0);
    // The next frame is live: the preview is kept off it there.
    frame(Flame);
    QCOMPARE(sent(Flame), 0);
    m_uni->setShowOnlyLive(false);
    m_uni->dismissFader(preview);
}

void OutputRules_Test::editorTestsAndEditorSourcesDontDriveIt()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    const FunctionParent test = FunctionParent::editorTest(0x1);
    QVERIFY(test.isNotTheShow() && test.isPreview() == false);
    Scene *s = startScene({ { Stage, 60 }, { Flame, 255 } }, test.type(), test.id());
    tickFrame(Flame);
    QCOMPARE(sent(Stage), 60);
    QCOMPARE(sent(Flame), 0);
    s->start(m_doc->masterTimer(), FunctionParent::master());   // a GO
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 255);
    stopFunctions();
    tickFrame(Flame);
    tickFrame(Flame);

    // The Scene editor's own output (unticking its Blind).
    GenericDMXSource *src = new GenericDMXSource(m_doc);
    src->setHoldsShowOnly(true);
    src->set(m_doc->fixtureForAddress(Flame), 0, 255);
    src->set(m_doc->fixtureForAddress(Stage), 0, 90);
    src->setOutputEnabled(true);
    tickFrame(Flame);
    QCOMPARE(sent(Stage), 90);
    QCOMPARE(sent(Flame), 0);
    delete src;
}

void OutputRules_Test::markingItWhileOnIsSentAtOnce()
{
    addDimmer("Flame L", Flame, Fixture::ShowOnly);   // one already held
    Fixture *r = addDimmer("Flame R", Flame + 1, 0);
    m_doc->outputRuleKeeper()->rebuild();
    put(Flame + 1, 255);
    frame(Flame);
    QCOMPARE(sent(Flame + 1), 255);
    m_stub->m_dropUnchanged = true;
    frame(Flame);
    frame(Flame);
    r->setOutputRules(Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    frame(Flame);
    QCOMPARE(sent(Flame + 1), 0);
}

void OutputRules_Test::safeValueGoesThroughTheModifier()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    ChannelModifier inverted;
    inverted.setModifierMap({ qMakePair(uchar(0), uchar(255)), qMakePair(uchar(255), uchar(0)) });
    m_uni->setChannelModifier(ushort(Flame), &inverted);
    put(Flame, 255);
    frame(Flame);
    // Off on this channel is 255 out: what the modifier makes of 0.
    QCOMPARE(sent(Flame), 255);
    // Taken off, the channel's zero is 0 again (it kept the modifier's 255
    // and sent it every frame -- a stale modifier zero, found here).
    m_uni->setChannelModifier(ushort(Flame), nullptr);
    m_doc->setMode(Doc::Operate);
    m_uni->dismissFader(m_fader);
    m_fader = m_uni->requestFader();
    frame(Flame);
    frame(Flame);
    QCOMPARE(sent(Flame), 0);
}

void OutputRules_Test::childrenFollowTheirParentsGate()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    Scene *member = new Scene(m_doc);
    member->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    member->setValue(m_doc->fixtureForAddress(Stage), 0, 50);
    m_doc->addFunction(member);
    Collection *col = new Collection(m_doc);
    col->addFunction(member->id());
    m_doc->addFunction(col);
    col->start(m_doc->masterTimer(), FunctionParent::preview(0x7));
    for (int i = 0; i < 3; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Stage), 50);
    QCOMPARE(sent(Flame), 0);
    // A GO on the same collection: it is the show now, and so is its member.
    col->start(m_doc->masterTimer(), FunctionParent(FunctionParent::ManualVCWidget, 3));
    for (int i = 0; i < 4; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Flame), 255);
}

void OutputRules_Test::showOnlyWinsOverOnAtOpen()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly | Fixture::OnAtOpen);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_doc->outputRuleKeeper()->onAtOpenArmed());
    frame(Flame);
    frame(Flame);
    QCOMPARE(sent(Work), 255);      // a plain On-when-not-in-use light is on
    QCOMPARE(sent(Flame), 0);       // Show only: nothing drives it, it is off
}

void OutputRules_Test::markingItLiveDropsAStoredValue()
{
    Fixture *spot = addSpot("Curtain-ish", 20);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    put(21, 200);                   // something left its LTP channel at 200
    frame(21);
    m_uni->dismissFader(m_fader);
    m_fader = m_uni->requestFader();
    frame(21);
    QCOMPARE(sent(21), 200);        // LTP: kept
    spot->setOutputRules(Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    frame(21);
    QCOMPARE(sent(21), 0);
    QCOMPARE(int(uchar(m_uni->postGMValues()->at(21))), 0);
}

void OutputRules_Test::showOnlyFrameCost()
{
    // 300 channels of a look, written by a fader every frame.
    for (int a = 100; a < 400; a++)
        put(a, uchar(a & 0xff));
    QList<Fixture *> lasers;
    for (int i = 0; i < 64; i++)
        lasers << addDimmer(QString("Laser %1").arg(i), 400 + i, 0);
    m_doc->outputRuleKeeper()->rebuild();
    auto perFrame = [this](int frames) {
        for (int i = 0; i < 50; i++)
            m_uni->processFaders();
        QElapsedTimer t;
        t.start();
        for (int i = 0; i < frames; i++)
            m_uni->processFaders();
        return double(t.nsecsElapsed()) / frames;
    };
    const double none = perFrame(2000);
    for (Fixture *f : lasers)
        f->setOutputRules(Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    const double held = perFrame(2000);
    m_doc->setMode(Doc::Operate);
    QSharedPointer<GenericFader> preview = m_uni->requestFader(Universe::Override);
    preview->setHoldsShowOnly(true);
    for (int a = 400; a < 464; a++)
    {
        GenericFader::ChannelRef fc = preview->getChannelFader(m_doc, m_uni, Fixture::invalidId(), quint32(a));
        fc->setStart(255); fc->setCurrent(255); fc->setTarget(255);
    }
    const double live = perFrame(2000);
    m_uni->dismissFader(preview);
    qInfo("Show only, one universe frame: %.0f ns with none, %.0f ns with 64 held (Design), "
          "%.0f ns live with a preview's fader on them", none, held, live);
}

/****************************************************************************
 * Show only: the second review
 ****************************************************************************/

void OutputRules_Test::goingLiveSendsNoStoredPreview()
{
    Fixture *spot = addSpot("Laser-ish", 20);
    spot->setOutputRules(Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    // Design: a preview leaves the LTP colour channel at 200, stored.
    QSharedPointer<GenericFader> preview = m_uni->requestFader(Universe::Override);
    preview->setHoldsShowOnly(true);
    {
        GenericFader::ChannelRef fc = preview->getChannelFader(m_doc, m_uni, spot->id(), 1);
        fc->setStart(200); fc->setCurrent(200); fc->setTarget(200);
    }
    frame(21);
    m_uni->dismissFader(preview);
    frame(21);
    QCOMPARE(int(uchar(m_uni->postGMValues()->at(21))), 200);
    // Operate, and a send before any frame (leaving Blind, Blackout).
    m_doc->setMode(Doc::Operate);
    m_uni->dumpOutput(m_uni->postGMValues()->mid(0, m_uni->usedChannels()), true);
    QCOMPARE(sent(21), 0);
}

void OutputRules_Test::aGoOnAListRunningFromAPreviewDrivesIt()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    Scene *step = new Scene(m_doc);
    step->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    step->setValue(m_doc->fixtureForAddress(Stage), 0, 70);
    m_doc->addFunction(step);
    Chaser *c = new Chaser(m_doc);
    c->setName("Lasers");
    ChaserStep cs(step->id());
    cs.hold = 100000;
    c->addStep(cs);
    m_doc->addFunction(c);
    c->start(m_doc->masterTimer(), FunctionParent::preview(0x7));   // the Workbench
    for (int i = 0; i < 4; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Stage), 70);
    QCOMPARE(sent(Flame), 0);
    // A GO from the Run tab on the list already playing.
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = c->id();
    m_doc->showControl()->run(go, ShowControl::RouteRunTab);
    for (int i = 0; i < 4; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Flame), 255);
}

void OutputRules_Test::aListLeftRunningFromDesignWaitsForAGo()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *step = new Scene(m_doc);
    step->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    step->setValue(m_doc->fixtureForAddress(Stage), 0, 80);
    m_doc->addFunction(step);
    Chaser *c = new Chaser(m_doc);
    c->setName("Run");
    ChaserStep cs(step->id());
    cs.hold = 100000;
    c->addStep(cs);
    m_doc->addFunction(c);
    // Tried out in Design (the Run tab), left running.
    c->start(m_doc->masterTimer(), FunctionParent::master());
    for (int i = 0; i < 4; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Flame), 0);           // Design
    m_doc->setMode(Doc::Operate);
    for (int i = 0; i < 4; i++)
    {
        tickFrame(Flame);
        QCOMPARE(sent(Flame), 0);       // not the show yet
        QCOMPARE(sent(Stage), 80);      // the rest plays on
    }
    // A GO in Operate on it: the show.
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = c->id();
    m_doc->showControl()->run(go, ShowControl::RouteRunTab);
    for (int i = 0; i < 4; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Flame), 255);
}

void OutputRules_Test::aParkSetInDesignHoldsItInOperate()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    OutputLayer park(m_doc, Universe::Park, FadeChannel::Override, QStringLiteral("Park"));
    park.set({ { m_doc->fixtureForAddress(Flame), 0, 255 } });
    frame(Flame);
    QCOMPARE(sent(Flame), 0);           // Design
    m_doc->setMode(Doc::Operate);
    frame(Flame);
    QCOMPARE(sent(Flame), 0);           // set in Design: not the show
    // Parked again, live (ParkEffect renews the fixture it parks).
    park.set({ { m_doc->fixtureForAddress(Flame), 0, 255 } },
             [](const OutputLayer::Level &) { return true; });
    frame(Flame);
    QCOMPARE(sent(Flame), 255);
    park.clear();
}

void OutputRules_Test::whatAScriptUnderATestStartsIsHeld()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    Scene *scene = new Scene(m_doc);
    scene->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    scene->setValue(m_doc->fixtureForAddress(Stage), 0, 40);
    m_doc->addFunction(scene);
    Script *script = new Script(m_doc);
    m_doc->addFunction(script);
    script->setData(QString("stoponexit:false\nstartfunction:%1\n").arg(scene->id()));
    script->start(m_doc->masterTimer(), FunctionParent::editorTest(0x5));   // the Script editor's Test
    for (int i = 0; i < 6; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Stage), 40);
    QCOMPARE(sent(Flame), 0);
}

void OutputRules_Test::aPreviewsEffectIsHeldFromTheFirstLiveFrame()
{
    Fixture *flame = addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->assignFixture(flame->id());
    m_doc->addFixtureGroup(grp);
    QTemporaryDir dir;
    QFile js(dir.filePath("full.js"));
    QVERIFY(js.open(QIODevice::WriteOnly));
    js.write("(function() { var e = {}; e.apiVersion = 1; e.name = 'Full';"
             " e.tick = function(fixtures) { return fixtures.map(function() { return { dimmer: 1.0 }; }); };"
             " return e; })()");
    js.close();
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, m_doc);
    look->setScriptPath(js.fileName());
    m_doc->addPalette(look);
    Scene *scene = new Scene(m_doc);
    scene->addFixtureGroup(grp->id());
    scene->addPalette(look->id());
    m_doc->addFunction(scene);
    /* The effect goes first, pass or fail: stopped from a tick on this
       thread, the runner would claim the universes the tick holds (in the
       app it hears the stop on another thread). */
    auto teardown = qScopeGuard([this, scene]() {
        m_doc->effectScriptRunner()->destroyInstancesForScene(scene->id(), false);
    });
    scene->start(m_doc->masterTimer(), FunctionParent::preview(0x7));
    for (int i = 0; i < 6; i++)
    {
        m_doc->masterTimer()->timerTick();
        m_doc->effectScriptRunner()->slotTick();
        frame(Flame);
    }
    QVERIFY2(uchar(m_uni->postGMValues()->at(Flame)) > 0, "the effect draws on the plot");
    QCOMPARE(sent(Flame), 0);           // Design
    // Operate: no publish in between -- held from the first live frame.
    m_doc->setMode(Doc::Operate);
    m_doc->masterTimer()->timerTick();
    frame(Flame);
    QCOMPARE(sent(Flame), 0);
    m_doc->masterTimer()->timerTick();
    m_doc->effectScriptRunner()->slotTick();
    m_doc->masterTimer()->timerTick();
    frame(Flame);
    QCOMPARE(sent(Flame), 0);
}

/* A level (Spotlight Chase on a wash with no dimmer and no colour of its
   own) scales what is under it: on a Show-only wash in Operate, played by
   a preview, every emitter stays held at its safe value -- a level never
   reads the held channel up. */
void OutputRules_Test::aLevelOnAShowOnlyWashStaysHeld()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(QStringLiteral("Test"));
    def->setModel(QStringLiteral("Show-only RGB wash"));
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(QStringLiteral("3ch"));
    const QLCChannel::Preset presets[] = { QLCChannel::IntensityRed, QLCChannel::IntensityGreen,
                                           QLCChannel::IntensityBlue };
    for (int c = 0; c < 3; ++c)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("c%1").arg(c));
        ch->setPreset(presets[c]);
        def->addChannel(ch);
        mode->insertChannel(ch, quint32(c));
    }
    def->addMode(mode);
    QVERIFY(m_doc->fixtureDefCache()->addFixtureDef(def));
    Fixture *wash = new Fixture(m_doc);
    wash->setName(QStringLiteral("Wash"));
    wash->setFixtureDefinition(def, mode);
    wash->setUniverse(0);
    wash->setAddress(40);
    wash->setOutputRules(Fixture::ShowOnly);
    QVERIFY(m_doc->addFixture(wash));
    m_doc->outputRuleKeeper()->rebuild();

    QLCPalette *look = new QLCPalette(QLCPalette::Effect, m_doc);
    look->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/spotlight-chase.js");
    m_doc->addPalette(look);
    Scene *scene = new Scene(m_doc);
    scene->addFixture(wash->id());
    scene->addPalette(look->id());
    m_doc->addFunction(scene);
    auto teardown = qScopeGuard([this, scene]() {
        m_doc->effectScriptRunner()->destroyInstancesForScene(scene->id(), false);
    });
    m_doc->setMode(Doc::Operate);
    scene->start(m_doc->masterTimer(), FunctionParent::preview(0x7));
    for (int i = 0; i < 12; i++)
    {
        m_doc->masterTimer()->timerTick();
        m_doc->effectScriptRunner()->slotTick();
        m_doc->masterTimer()->timerTick();
        for (int c = 0; c < 3; ++c)
        {
            frame(40 + c);
            QVERIFY2(sent(40 + c) == int(OutputRuleKeeper::safeValue(wash, quint32(c))),
                     qPrintable(QString("emitter %1 at %2 in a preview in Operate").arg(c).arg(sent(40 + c))));
        }
    }
}

void OutputRules_Test::notOursStaysZeroUnderTheHold()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    ChannelModifier inverted;           // a safe value that is not 0 out
    inverted.setModifierMap({ qMakePair(uchar(0), uchar(255)), qMakePair(uchar(255), uchar(0)) });
    m_uni->setChannelModifier(ushort(Flame), &inverted);
    frame(Flame);
    QCOMPARE(sent(Flame), 255);
    m_uni->setNotOurs(false, { qMakePair(int(Flame), int(Flame)) });
    frame(Flame);
    QCOMPARE(sent(Flame), 0);           // the venue's desk keeps it: nothing of ours
    m_uni->setNotOurs(false, {});
    m_uni->setChannelModifier(ushort(Flame), nullptr);
}

void OutputRules_Test::markingOneMidShowResetsOnlyIt()
{
    Fixture *spot = addSpot("Curtain-ish", 20);
    spot->setOutputRules(Fixture::ShowOnly);
    addDimmer("Flame", Flame, 0);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    put(21, 150);                       // the show drives its LTP channel at 150
    frame(21);
    QCOMPARE(sent(21), 150);
    Fixture *flame = m_doc->fixture(m_doc->fixtureForAddress(Flame));
    flame->setOutputRules(Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    frame(21);
    QCOMPARE(sent(21), 150);            // another fixture made Show only: this one untouched
    // S0: its writer gone, the LTP channel is not kept -- cut to safe.
    m_uni->dismissFader(m_fader);
    m_fader = m_uni->requestFader();
    frame(21);
    QCOMPARE(sent(21), int(OutputRuleKeeper::safeValue(spot, 1)));
}

/****************************************************************************
 * Third safety review (review/048-show-only-3)
 ****************************************************************************/

void OutputRules_Test::aDesignParkStaysHeldWhenAnotherFixtureIsParkedLive()
{
    // A fogger parked at full in Design (haze for focus), the show then
    // opened in Operate: held, as the rule says. The operator parks a
    // broken mover -- another fixture -- during the show.
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    ParkEffect park(m_doc);
    park.parkFixture(m_doc->fixtureForAddress(Flame), { { 0u, uchar(255) } });
    frame(Flame);
    QCOMPARE(sent(Flame), 0);           // Design
    m_doc->setMode(Doc::Operate);
    frame(Flame);
    QCOMPARE(sent(Flame), 0);           // parked in Design: not the show
    park.parkFixture(m_doc->fixtureForAddress(Stage), { { 0u, uchar(100) } });
    frame(Flame);
    QCOMPARE(sent(Stage), 100);
    // Nobody touched the fogger's park in Operate: still held.
    QCOMPARE(sent(Flame), 0);
    park.unparkAll();
}

void OutputRules_Test::aPreviewStoppedBeforeItsFirstTickSendsNothing()
{
    // Operate: the Workbench previews a layer and a GO takes the stage
    // (stageTakenForRun() stops it) inside the same 20 ms tick.
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    const FunctionParent workbench = FunctionParent::preview(0x7);
    Scene *s = new Scene(m_doc);
    s->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    s->setValue(m_doc->fixtureForAddress(Stage), 0, 90);
    s->setFadeOutSpeed(400);
    m_doc->addFunction(s);
    s->start(m_doc->masterTimer(), workbench);
    s->stop(workbench);
    int flameMax = 0;
    for (int i = 0; i < 30; i++)
    {
        tickFrame(Flame);
        flameMax = qMax(flameMax, sent(Flame));
    }
    QCOMPARE(flameMax, 0);
}

void OutputRules_Test::aSimpleDeskCueStackLeftRunningFromDesignIsHeld()
{
    // Simple Desk's cue stack (SimpleDeskEngine::writeDMX() drives it on
    // the timer thread, as here): a fog cue played in Design, left on.
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    CueStack cs(m_doc);
    QMap<uint, uchar> values;
    values[uint(Flame)] = 255;
    cs.appendCue(Cue(values));
    cs.start();
    cs.nextCue();
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    m_doc->inputOutputMap()->releaseUniverses(false);
    cs.preRun();
    cs.write(ua);
    frame(Flame);
    QCOMPARE(sent(Flame), 0);           // Design
    m_doc->setMode(Doc::Operate);
    int flameMax = 0;
    for (int i = 0; i < 4; i++)
    {
        cs.write(ua);
        frame(Flame);
        flameMax = qMax(flameMax, sent(Flame));
    }
    cs.stop();
    cs.postRun(m_doc->masterTimer(), ua);
    // A cue left running from Design: no GO in Operate, so held.
    QCOMPARE(flameMax, 0);
}

void OutputRules_Test::blackoutWinsOverShowOnlyIgnoringBlackout()
{
    // Both boxes ticked in Output Rules (the dialog allows it).
    addDimmer("Flame", Flame, Fixture::ShowOnly | Fixture::ExemptBlackout);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    startScene({ { Flame, 255 } }, FunctionParent::Master, 0);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 255);         // a GO drives it
    m_doc->inputOutputMap()->setBlackout(true);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 0);           // Blackout must always win
    m_doc->inputOutputMap()->setBlackout(false);
}

void OutputRules_Test::emergencyStopTakesItOutAtOnce()
{
    // A GO drives the laser; its scene has a fade out. The pad's
    // emergency stop is ShowControl AllOff.
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    Scene *s = new Scene(m_doc);
    s->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    s->setFadeOutSpeed(3000);
    m_doc->addFunction(s);
    s->start(m_doc->masterTimer(), FunctionParent::master());
    // Whatever happens below, its fade is over before the next test.
    auto drain = qScopeGuard([this]() {
        for (int i = 0; i < 200; i++)
            tickFrame(Flame);
    });
    tickFrame(Flame);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 255);
    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    QVERIFY(m_doc->showControl()->run(off, ShowControl::RoutePad));
    for (int i = 0; i < 3; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Flame), 0);           // 60 ms after "Everything is off"
}

void OutputRules_Test::aVcStepButtonOnADesignListDrivesIt()
{
    // A plain chaser (not a Show Tree list) tried out in Design and left
    // running; in Operate the VC "Chaser step next" button
    // (VCButton::ChaserStepNext -> Doc::stepCurrentChaser) is the GO.
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *one = new Scene(m_doc);
    one->setValue(m_doc->fixtureForAddress(Stage), 0, 50);
    m_doc->addFunction(one);
    Scene *two = new Scene(m_doc);
    two->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(two);
    Chaser *c = new Chaser(m_doc);
    ChaserStep a(one->id()), b(two->id());
    a.hold = 100000;
    b.hold = 100000;
    c->addStep(a);
    c->addStep(b);
    m_doc->addFunction(c);
    c->start(m_doc->masterTimer(), FunctionParent::master());
    for (int i = 0; i < 4; i++)
        tickFrame(Flame);
    m_doc->setMode(Doc::Operate);
    for (int i = 0; i < 2; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Flame), 0);           // left running from Design: held
    m_doc->stepCurrentChaser(+1);
    for (int i = 0; i < 6; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Flame), 255);         // the VC's step is a GO in Operate
}

/****************************************************************************
 * Third review: the fixes' own tests
 ****************************************************************************/

void OutputRules_Test::aDesignMarkStaysHeldWhenAnotherIsMarkedLive()
{
    Fixture *a = addSpot("Laser-ish", 20);
    a->setOutputRules(Fixture::ShowOnly);
    Fixture *b = addSpot("Mover", 30);
    m_doc->outputRuleKeeper()->rebuild();
    MarkEffect mark(m_doc);
    mark.markFixture(a->id(), { { 1u, uchar(200) } });     // its colour, in the dark
    frame(21);
    QCOMPARE(sent(21), 0);              // Design
    m_doc->setMode(Doc::Operate);
    frame(21);
    QCOMPARE(sent(21), 0);              // marked in Design: not the show
    mark.markFixture(b->id(), { { 1u, uchar(100) } });
    frame(21);
    QCOMPARE(sent(31), 100);
    QCOMPARE(sent(21), 0);              // untouched in Operate: still held
    mark.unmarkAll();
}

void OutputRules_Test::aDesignLastLookStaysHeldAtTheNextGo()
{
    Fixture *flame = addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    LastLookEffect *look = m_doc->lastLook();
    LastLookEffect::ChannelHold hf;
    hf.fixtureId = flame->id();
    hf.universeId = 0;
    hf.address = Flame;
    hf.value = 200;
    LastLookEffect::ChannelHold hs;
    hs.fixtureId = m_doc->fixtureForAddress(Stage);
    hs.universeId = 0;
    hs.address = Stage;
    hs.value = 120;
    look->hold({ hf, hs });             // captured in Design
    frame(Flame);
    QCOMPARE(sent(Flame), 0);
    m_doc->setMode(Doc::Operate);
    frame(Flame);
    QCOMPARE(sent(Stage), 120);
    QCOMPARE(sent(Flame), 0);
    // The next GO takes the stage light: the rest of the look stays as it was.
    look->releaseFixtures({ hs.fixtureId });
    frame(Flame);
    QCOMPARE(sent(Flame), 0);
    // Captured live (a show stopping in Operate): still never a Show-only
    // channel (S3: a capture is no operator action).
    look->hold({ hf });
    frame(Flame);
    QCOMPARE(sent(Flame), 0);
    look->clear();
}

void OutputRules_Test::emergencyStopLatchesUntilTheNextGo()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    put(Flame, 255);                    // the Simple Desk: a hand on its fader
    frame(Flame);
    QCOMPARE(sent(Flame), 255);
    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    m_stub->m_universe.fill(char(7));
    QVERIFY(m_doc->showControl()->run(off, ShowControl::RoutePad));
    QCOMPARE(sent(Flame), 0);           // at once, before any frame
    QCOMPARE(m_doc->outputRuleKeeper()->showOnlyHeldBy(), OutputRuleKeeper::HeldByEmergencyStop);
    for (int i = 0; i < 3; i++)
    {
        frame(Flame);
        QCOMPARE(sent(Flame), 0);       // the fader still up: held
    }
    // The next GO in Operate lets go.
    Scene *step = new Scene(m_doc);
    step->setValue(m_doc->fixtureForAddress(Stage), 0, 60);
    m_doc->addFunction(step);
    Chaser *c = new Chaser(m_doc);
    ChaserStep cs(step->id());
    cs.hold = 100000;
    c->addStep(cs);
    m_doc->addFunction(c);
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = c->id();
    m_doc->showControl()->run(go, ShowControl::RouteRunTab);
    // The footer's "emergency stop" goes at the GO; the output is still
    // held by the rule (E4: no release).
    QVERIFY(m_doc->outputRuleKeeper()->emergencyStopped() == false);
    frame(Flame);
    // Nothing from before the stop comes back (E4): the fader left up is
    // as old as the stop's session.
    QCOMPARE(sent(Flame), 0);
    // Moved again by a hand after the stop (E3): the show again.
    put(Flame, 255);
    frame(Flame);
    QCOMPARE(sent(Flame), 255);
}

void OutputRules_Test::aShowOnlyFixtureMovedMidShowIsHeldAtOnce()
{
    Fixture *flame = addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    // A preview lights the address the flame is about to move to.
    QSharedPointer<GenericFader> preview = m_uni->requestFader(Universe::Override);
    preview->setHoldsShowOnly(true);
    {
        GenericFader::ChannelRef fc = preview->getChannelFader(m_doc, m_uni, Fixture::invalidId(), quint32(Flame + 2));
        fc->setStart(255); fc->setCurrent(255); fc->setTarget(255);
    }
    frame(Flame);
    QCOMPARE(sent(Flame + 2), 255);     // nothing Show only there yet
    flame->setAddress(quint32(Flame + 2));      // re-patched, no event loop
    frame(Flame);
    QCOMPARE(sent(Flame + 2), 0);
    m_uni->dismissFader(preview);
}

void OutputRules_Test::theHeldTextSaysEmergencyStop()
{
    addDimmer("Flame L", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    m_doc->outputRuleKeeper()->emergencyStop();
    QCOMPARE(m_doc->outputRuleKeeper()->showOnlyHeldText(),
             QString("Flame L held: emergency stop -- a GO or a control you move drives them again"));
    // The footer says it until a GO or a new session.
    QVERIFY(m_doc->outputRuleKeeper()->emergencyStopped());
    m_doc->setMode(Doc::Design);
    QCOMPARE(m_doc->outputRuleKeeper()->showOnlyHeldText(), QString("Flame L held: Design"));
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_doc->outputRuleKeeper()->emergencyStopped() == false);
}

/* Test mode on Connections' DMX map: values held over what plays, on the wire. */
void OutputRules_Test::testChannelsSitOnTopAndReleaseTogether()
{
    TestChannels *t = m_doc->testChannels();
    put(Stage, 100);                        // what the show gives it
    frame(Stage);
    QCOMPARE(sent(Stage), 100);
    QVERIFY(t->setValue(0, Stage, 200) == false);   // not on yet
    QVERIFY(t->setOn(true));
    QVERIFY(t->setValue(0, Stage, 200));
    QVERIFY(t->setValue(0, 300, 50));       // no fixture there: an address
    frame(Stage);
    QCOMPARE(sent(Stage), 200);             // on the wire, over the cue
    QCOMPARE(sent(300), 50);
    QCOMPARE(t->bannerText(), QString("TEST MODE: 2 channels held over the show. Click Test to let them go."));
    QCOMPARE(t->chipText(), QString("Test: 2 held"));
    // The cue moves on; the held value stays on top.
    put(Stage, 120);
    frame(Stage);
    QCOMPARE(sent(Stage), 200);
    // Off: all of them let go at once, each back to the show's.
    QVERIFY(t->setOn(false));
    frame(Stage);
    QCOMPARE(sent(Stage), 120);
    QCOMPARE(frame(300), 0);
    QCOMPARE(t->heldCount(), 0);
}

/* Pan, a colour wheel, a gobo and a fog channel keep the last value written
   when nothing else writes them: Test off must put them back. */
void OutputRules_Test::aReleasedLtpChannelGoesBackToWhatItWas()
{
    addSpot("Spot", 20);                     // address 21 is its colour channel (LTP)
    put(21, 100);                            // a cue gave it 100 ...
    frame(21);
    QCOMPARE(sent(21), 100);
    m_uni->dismissFader(m_fader);            // ... and has stopped: the channel keeps it
    m_fader.clear();
    frame(21);
    QCOMPARE(sent(21), 100);
    TestChannels *t = m_doc->testChannels();
    QVERIFY(t->setOn(true));
    QVERIFY(t->setValue(0, 21, 200));
    frame(21);
    QCOMPARE(sent(21), 200);
    QVERIFY(t->setOn(false));
    frame(21);
    QCOMPARE(sent(21), 100);                 // not left at the test's 200
    m_fader = m_uni->requestFader();
}

void OutputRules_Test::aPatchChangeEndsTestMode()
{
    TestChannels *t = m_doc->testChannels();
    QVERIFY(t->setOn(true));
    QVERIFY(t->setValue(0, 300, 50));
    addSpot("Late", 40);                     // a fixture added under it
    QVERIFY(!t->isOn());
    QCOMPARE(t->heldCount(), 0);
    QVERIFY(t->endedBecause().contains("patch"));
    QVERIFY(t->setValue(0, 300, 50) == false);
    QVERIFY(t->setOn(true));                 // on again: the reason is cleared
    QVERIFY(t->endedBecause().isEmpty());
    QVERIFY(t->setValue(0, Stage, 255));
    m_doc->inputOutputMap()->addUniverse();  // a universe added
    QVERIFY(!t->isOn());
    QVERIFY(t->setOn(true));
    QVERIFY(t->setValue(0, Stage, 255));
    QVERIFY(m_doc->deleteFixture(m_doc->fixtures().last()->id()));    // one removed
    QVERIFY(!t->isOn());
    frame(Stage);
    QVERIFY(sent(Stage) != 255);
}

void OutputRules_Test::theStopAndOperateEndTestMode()
{
    TestChannels *t = m_doc->testChannels();
    put(Stage, 100);
    QVERIFY(t->setOn(true));
    QVERIFY(t->setValue(0, Stage, 255));
    frame(Stage);
    QCOMPARE(sent(Stage), 255);
    m_doc->outputRuleKeeper()->emergencyStop();      // the e-stop and All Off
    QVERIFY(!t->isOn());
    QCOMPARE(t->heldCount(), 0);
    QVERIFY(t->setValue(0, Stage, 255) == false);    // stays off until turned on again
    frame(Stage);
    QVERIFY2(sent(Stage) != 255, qPrintable(QString::number(sent(Stage))));
    QVERIFY(t->setOn(true));
    QVERIFY(t->setValue(0, Stage, 77));

    // Operate refuses it, with the reason, and entering Operate ends it.
    m_doc->setMode(Doc::Operate);
    QVERIFY(!t->isOn());
    QString why;
    QVERIFY(!t->setOn(true, &why));
    QVERIFY2(why.contains("Operate"), qPrintable(why));
    QVERIFY(!t->isOn());
}

void OutputRules_Test::aShowOnlyChannelStaysSafeInTestMode()
{
    addDimmer("Flame L", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    TestChannels *t = m_doc->testChannels();
    QVERIFY(t->setOn(true));
    QVERIFY(t->setValue(0, Flame, 255));
    QVERIFY(t->setValue(0, Stage, 255));
    frame(Flame);
    QCOMPARE(sent(Stage), 255);
    QCOMPARE(sent(Flame), 0);               // Show only keeps its rule
    t->setOn(false);
}

/****************************************************************************
 * Fourth safety review (review/048-show-only-4)
 ****************************************************************************/

void OutputRules_Test::emergencyStopThenAGoDoesNotRefireTheShowsLastLook()
{
    // Operate: the Show tab's show has a laser (a Show-only flame here) on
    // a track. The operator hits All Off (the pad's e-stop), then GOes the
    // next cue on the main list, which has only a stage light.
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    QVERIFY(m_doc->lastLookEnabled());
    Scene *fire = new Scene(m_doc);
    fire->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(fire);
    Show *show = new Show(m_doc);
    m_doc->addFunction(show);
    Track *t = new Track(fire->id(), show);
    show->addTrack(t);
    ShowFunction *sf = new ShowFunction(show->getLatestShowFunctionId());
    sf->setFunctionID(fire->id());
    sf->setStartTime(0);
    sf->setDuration(100000);
    t->addShowFunction(sf);
    show->start(m_doc->masterTimer(), FunctionParent::master());
    for (int i = 0; i < 5; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Flame), 255);         // the show drives it: right

    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    QVERIFY(m_doc->showControl()->run(off, ShowControl::RoutePad));
    for (int i = 0; i < 5; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Flame), 0);           // the e-stop: out
    QVERIFY(show->isRunning() == false);

    Scene *stage = new Scene(m_doc);
    stage->setValue(m_doc->fixtureForAddress(Stage), 0, 60);
    m_doc->addFunction(stage);
    Chaser *c = new Chaser(m_doc);
    ChaserStep cs(stage->id());
    cs.hold = 100000;
    c->addStep(cs);
    m_doc->addFunction(c);
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = c->id();
    QVERIFY(m_doc->showControl()->run(go, ShowControl::RouteRunTab));
    int flameMax = 0;
    for (int i = 0; i < 6; i++)
    {
        tickFrame(Flame);
        flameMax = qMax(flameMax, sent(Flame));
    }
    QCOMPARE(sent(Stage), 60);          // the GO took
    m_doc->lastLook()->clear();
    // Nothing in the show after the stop drives the laser: it stays out.
    QCOMPARE(flameMax, 0);
}

void OutputRules_Test::emergencyStopThenAGoDoesNotRefireAStoredLtpLevel()
{
    // A laser's mode channel is LTP (a colour channel stands in for it).
    // A GO in Operate turns it on; All Off; the next GO is another list.
    Fixture *laser = addSpot("Laser", 20);
    laser->setOutputRules(Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    const int safe = int(uchar(m_uni->m_showOnlySafe.at(21)));
    QVERIFY(safe != 255);
    Scene *on = new Scene(m_doc);
    on->setValue(laser->id(), 1, 255);
    m_doc->addFunction(on);
    Chaser *a = new Chaser(m_doc);
    ChaserStep sa(on->id());
    sa.hold = 100000;
    a->addStep(sa);
    m_doc->addFunction(a);
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = a->id();
    QVERIFY(m_doc->showControl()->run(go, ShowControl::RouteRunTab));
    for (int i = 0; i < 5; i++)
        tickFrame(21);
    QCOMPARE(sent(21), 255);            // the show drives it: right

    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    QVERIFY(m_doc->showControl()->run(off, ShowControl::RoutePad));
    for (int i = 0; i < 5; i++)
        tickFrame(21);
    QCOMPARE(sent(21), safe);           // the e-stop: out
    QCOMPARE(m_doc->masterTimer()->runningFunctions(), 0);

    Scene *stage = new Scene(m_doc);
    stage->setValue(m_doc->fixtureForAddress(Stage), 0, 60);
    m_doc->addFunction(stage);
    Chaser *b = new Chaser(m_doc);
    ChaserStep sb(stage->id());
    sb.hold = 100000;
    b->addStep(sb);
    m_doc->addFunction(b);
    go.chaser = b->id();
    QVERIFY(m_doc->showControl()->run(go, ShowControl::RouteRunTab));
    int laserMax = 0;
    for (int i = 0; i < 6; i++)
    {
        tickFrame(21);
        laserMax = qMax(laserMax, sent(21));
    }
    QCOMPARE(sent(Stage), 60);          // the GO took
    // Nothing drives the laser after the stop: it stays at its safe value.
    QCOMPARE(laserMax, safe);
}

void OutputRules_Test::aVcStepButtonDoesNotClaimAnEditorTest()
{
    // Operate: the main list runs (a GO). The operator opens a chaser in
    // its editor and presses Test (editorTest parent): held, as the rule
    // says. Then the VC "step next" button, meant for the show.
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    Scene *stage = new Scene(m_doc);
    stage->setValue(m_doc->fixtureForAddress(Stage), 0, 60);
    m_doc->addFunction(stage);
    Chaser *main = new Chaser(m_doc);
    ChaserStep ms(stage->id());
    ms.hold = 100000;
    main->addStep(ms);
    main->addStep(ms);
    m_doc->addFunction(main);
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = main->id();
    QVERIFY(m_doc->showControl()->run(go, ShowControl::RouteRunTab));
    for (int i = 0; i < 3; i++)
        tickFrame(Flame);

    Scene *dark = new Scene(m_doc);
    dark->setValue(m_doc->fixtureForAddress(Stage), 0, 10);
    m_doc->addFunction(dark);
    Scene *fire = new Scene(m_doc);
    fire->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(fire);
    Chaser *tested = new Chaser(m_doc);
    ChaserStep ta(dark->id()), tb(fire->id());
    ta.hold = 100000;
    tb.hold = 100000;
    tested->addStep(ta);
    tested->addStep(tb);
    m_doc->addFunction(tested);
    tested->start(m_doc->masterTimer(), FunctionParent::editorTest(0x1));
    for (int i = 0; i < 3; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Flame), 0);
    m_doc->stepCurrentChaser(+1);
    int flameMax = 0;
    for (int i = 0; i < 6; i++)
    {
        tickFrame(Flame);
        flameMax = qMax(flameMax, sent(Flame));
    }
    // The editor's Test is not the show: the step button must not fire it.
    QCOMPARE(flameMax, 0);
}

/****************************************************************************
 * Fourth review: the fixes' own tests
 ****************************************************************************/

void OutputRules_Test::emergencyStopDropsALastLookCapturedBeforeIt()
{
    // A last look never holds a Show-only channel (S3): captured live
    // before a stop, it has the stage light and not the flame.
    Fixture *flame = addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    LastLookEffect::ChannelHold hf;
    hf.fixtureId = flame->id();
    hf.universeId = 0;
    hf.address = Flame;
    hf.value = 255;
    LastLookEffect::ChannelHold hs;
    hs.fixtureId = m_doc->fixtureForAddress(Stage);
    hs.universeId = 0;
    hs.address = Stage;
    hs.value = 90;
    m_doc->lastLook()->hold({ hf, hs });
    frame(Flame);
    QCOMPARE(sent(Stage), 90);
    QCOMPARE(sent(Flame), 0);
    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    QVERIFY(m_doc->showControl()->run(off, ShowControl::RoutePad));
    QCoreApplication::processEvents();
    frame(Flame);
    QCOMPARE(sent(Flame), 0);
    QCOMPARE(sent(Stage), 90);          // the rest of the look stays
    m_doc->lastLook()->clear();
}

void OutputRules_Test::aVcStepButtonStepsTheRootOfANestedList()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    Scene *inner1 = new Scene(m_doc);
    inner1->setValue(m_doc->fixtureForAddress(Stage), 0, 30);
    m_doc->addFunction(inner1);
    Chaser *nested = new Chaser(m_doc);
    ChaserStep n1(inner1->id());
    n1.hold = 100000;
    nested->addStep(n1);
    nested->addStep(n1);
    m_doc->addFunction(nested);
    Scene *after = new Scene(m_doc);
    after->setValue(m_doc->fixtureForAddress(Stage), 0, 80);
    m_doc->addFunction(after);
    Chaser *root = new Chaser(m_doc);
    ChaserStep r1(nested->id()), r2(after->id());
    r1.hold = 100000;
    r2.hold = 100000;
    root->addStep(r1);
    root->addStep(r2);
    m_doc->addFunction(root);
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = root->id();
    QVERIFY(m_doc->showControl()->run(go, ShowControl::RouteRunTab));
    for (int i = 0; i < 4; i++)
        tickFrame(Stage);
    QVERIFY(nested->isRunning());       // the precondition: a list inside a list
    auto claimedByItself = [](const Function *f) {
        for (const FunctionParent &p : f->sources())
            if (p.type() != FunctionParent::Function)
                return true;
        return false;
    };
    QVERIFY(!claimedByItself(nested));
    m_doc->stepCurrentChaser(+1);       // the last chaser started is the nested one
    for (int i = 0; i < 4; i++)
        tickFrame(Stage);
    // Stepped (and claimed) as its root list: the nested one never gets a
    // source of its own, so it follows the root's gate.
    QVERIFY(!nested->isRunning() || !claimedByItself(nested));
    QVERIFY(root->isRunning());
}

void OutputRules_Test::aMoveAcrossUniversesMarksAtOnceAndRebuildsOnce()
{
    Fixture *flame = addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    const QList<Universe *> ua = m_doc->inputOutputMap()->universes();
    QVERIFY(ua.size() > 1);
    m_doc->setMode(Doc::Operate);
    QCoreApplication::processEvents();  // nothing pending from before
    const int before = m_doc->outputRuleKeeper()->rebuildCount();
    flame->setUniverse(1);
    flame->setAddress(30);
    // Held at the new address at once, with no event loop between...
    QVERIFY(ua.at(1)->channelCapabilities(30) & Universe::ShowOnlyOutput);
    QVERIFY((m_uni->channelCapabilities(Flame) & Universe::ShowOnlyOutput) == 0);
    QCOMPARE(m_doc->outputRuleKeeper()->rebuildCount(), before);
    // ...and one full rebuild after, not one per change.
    QCoreApplication::processEvents();
    QCOMPARE(m_doc->outputRuleKeeper()->rebuildCount(), before + 1);
    QVERIFY(ua.at(1)->channelCapabilities(30) & Universe::ShowOnlyOutput);
}

void OutputRules_Test::anEmergencyStopFromAnotherThreadActsAtOnce()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    put(Flame, 255);
    frame(Flame);
    QCOMPARE(sent(Flame), 255);
    OutputRuleKeeper *rules = m_doc->outputRuleKeeper();
    QThread *t = QThread::create([rules]() { rules->emergencyStop(); });   // a plugin's thread
    t->start();
    QVERIFY(t->wait(5000));
    delete t;
    QVERIFY(rules->emergencyStopped());
    QCOMPARE(sent(Flame), 0);           // sent from that thread, at once
    QCoreApplication::processEvents();  // the footer and the last look, here
    QCOMPARE(rules->showOnlyHeldBy(), OutputRuleKeeper::HeldByEmergencyStop);
}

/****************************************************************************
 * Fifth review (of df050e0ce6): red until fixed
 ****************************************************************************/

void OutputRules_Test::aShowsFadeOutAfterTheStopStaysOut()
{
    // A flame cue with a 4 s fade-out, GO'd in Operate.
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    Scene *fire = new Scene(m_doc);
    fire->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    fire->setFadeOutSpeed(4000);
    m_doc->addFunction(fire);
    Chaser *a = new Chaser(m_doc);
    ChaserStep sa(fire->id());
    sa.hold = 100000;
    a->addStep(sa);
    m_doc->addFunction(a);
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = a->id();
    QVERIFY(m_doc->showControl()->run(go, ShowControl::RouteRunTab));
    for (int i = 0; i < 5; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Flame), 255);         // the show drives it: right

    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    QVERIFY(m_doc->showControl()->run(off, ShowControl::RoutePad));
    for (int i = 0; i < 3; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Flame), 0);           // the e-stop: out at once

    // The next GO is another list, a moment later (the fade still runs).
    Scene *stage = new Scene(m_doc);
    stage->setValue(m_doc->fixtureForAddress(Stage), 0, 60);
    m_doc->addFunction(stage);
    Chaser *b = new Chaser(m_doc);
    ChaserStep sb(stage->id());
    sb.hold = 100000;
    b->addStep(sb);
    m_doc->addFunction(b);
    go.chaser = b->id();
    QVERIFY(m_doc->showControl()->run(go, ShowControl::RouteRunTab));
    int flameMax = 0;
    for (int i = 0; i < 6; i++)
    {
        tickFrame(Flame);
        flameMax = qMax(flameMax, sent(Flame));
    }
    QCOMPARE(sent(Stage), 60);          // the GO took
    // What the stop cut must not come back: not even its fade-out.
    QCOMPARE(flameMax, 0);
}

/****************************************************************************
 * The e-stop rules, swept over every writer (fifth review)
 ****************************************************************************/

Fixture *OutputRules_Test::addShowOnlyDef(const QString &name, int address,
                                          const QList<QPair<QLCChannel::Group, QLCChannel::PrimaryColour> > &chans)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(QStringLiteral("Sweep"));
    def->setModel(name);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(QStringLiteral("Mode"));
    int i = 0;
    for (const auto &c : chans)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QStringLiteral("%1 %2").arg(name).arg(i));
        ch->setGroup(c.first);
        if (c.first == QLCChannel::Intensity)
            ch->setColour(c.second);
        if (c.first == QLCChannel::Pan || c.first == QLCChannel::Tilt)
            ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, quint32(i++));
    }
    def->addMode(mode);
    mode->cacheHeads();
    if (!m_doc->fixtureDefCache()->addFixtureDef(def))
    {
        delete def;
        return nullptr;
    }
    Fixture *f = new Fixture(m_doc);
    f->setName(name);
    f->setFixtureDefinition(def, mode);
    f->setUniverse(0);
    f->setAddress(quint32(address));
    f->setOutputRules(Fixture::ShowOnly);
    if (!m_doc->addFixture(f))
    {
        delete f;
        return nullptr;
    }
    m_doc->outputRuleKeeper()->rebuild();
    return f;
}

Chaser *OutputRules_Test::listOf(Function *step)
{
    Chaser *c = new Chaser(m_doc);
    ChaserStep cs(step->id());
    cs.hold = 100000;
    c->addStep(cs);
    m_doc->addFunction(c);
    return c;
}

void OutputRules_Test::goList(Chaser *c)
{
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = c->id();
    QVERIFY(m_doc->showControl()->run(go, ShowControl::RouteRunTab));
}

void OutputRules_Test::sweep(int address, const std::function<void()> &tickOnce,
                             const std::function<void()> &setUp, const std::function<void()> &again,
                             const std::function<void()> &afterStop)
{
    // Set up in Operate: it drives the Show-only channel.
    int safe = int(uchar(m_uni->m_showOnlySafe.at(address)));
    // Q326: a pan or tilt stays where the stop found it, not at safe.
    const bool holds = FixtureRest::holdsPositionAt(m_doc, quint32(address));
    m_doc->setMode(Doc::Operate);
    setUp();
    int most = 0;
    for (int i = 0; i < 8; i++)
    {
        tickOnce();
        most = qMax(most, qAbs(sent(address) - safe));
    }
    QVERIFY2(most > 0, "the writer drove it before the stop");
    if (holds)
        safe = sent(address);
    // E1: All Off, cut at once.
    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    QVERIFY(m_doc->showControl()->run(off, ShowControl::RoutePad));
    QCOMPARE(sent(address), safe);
    if (afterStop)
        afterStop();
    // E4: a GO on an unrelated list makes nothing else current.
    Scene *stage = new Scene(m_doc);
    stage->setValue(m_doc->fixtureForAddress(Stage), 0, 60);
    m_doc->addFunction(stage);
    goList(listOf(stage));
    most = 0;
    for (int i = 0; i < 10; i++)
    {
        tickOnce();
        most = qMax(most, qAbs(sent(address) - safe));
    }
    QCOMPARE(sent(Stage), 60);
    QCOMPARE(most, 0);
    // E3: its own operator action makes it the show again.
    if (again)
    {
        again();
        most = 0;
        for (int i = 0; i < 8; i++)
        {
            tickOnce();
            most = qMax(most, qAbs(sent(address) - safe));
        }
        QVERIFY2(most > 0, "its own operator action drives it again");
    }
}

void OutputRules_Test::sweepFunctionFadeOut()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *fire = new Scene(m_doc);
    fire->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    fire->setFadeOutSpeed(4000);
    m_doc->addFunction(fire);
    Chaser *a = listOf(fire);
    sweep(Flame, [this]() { tickFrame(Flame); }, [this, a]() { goList(a); }, [this, a]() { goList(a); });
}

void OutputRules_Test::sweepChaserStepFade()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *fire = new Scene(m_doc);
    fire->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(fire);
    Chaser *a = new Chaser(m_doc);
    ChaserStep st(fire->id());
    st.fadeIn = 3000;                   // stopped mid-fade
    st.fadeOut = 3000;
    st.hold = 100000;
    a->addStep(st);
    m_doc->addFunction(a);
    sweep(Flame, [this]() { tickFrame(Flame); }, [this, a]() { goList(a); }, [this, a]() { goList(a); });
}

void OutputRules_Test::sweepEfx()
{
    Fixture *mover = addShowOnlyDef("Mover", 30, { { QLCChannel::Pan, QLCChannel::NoColour },
                                                   { QLCChannel::Tilt, QLCChannel::NoColour } });
    QVERIFY(mover != nullptr);
    EFX *efx = new EFX(m_doc);
    QVERIFY(efx->addFixture(mover->id(), 0));
    m_doc->addFunction(efx);
    Chaser *a = listOf(efx);
    /* The stop holds pan and tilt where they are (Q326): run again, the
       circle must move off that point within the sweep's eight frames to be
       seen driving it, so one round is 400 ms, not the default. */
    efx->setDuration(400);
    sweep(30, [this]() { tickFrame(30); }, [this, a]() { goList(a); }, [this, a]() { goList(a); });
}

void OutputRules_Test::sweepRgbMatrix()
{
    Fixture *bar = addShowOnlyDef("Bar", 40, { { QLCChannel::Intensity, QLCChannel::Red },
                                               { QLCChannel::Intensity, QLCChannel::Green },
                                               { QLCChannel::Intensity, QLCChannel::Blue } });
    QVERIFY(bar != nullptr);
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setSize(QSize(1, 1));
    grp->assignFixture(bar->id(), QLCPoint(0, 0));
    m_doc->addFixtureGroup(grp);
    RGBMatrix *mtx = new RGBMatrix(m_doc);
    mtx->setFixtureGroup(grp->id());
    mtx->setAlgorithm(new RGBPlain(m_doc));
    mtx->setColor(0, QColor(255, 0, 0));
    m_doc->addFunction(mtx);
    Chaser *a = listOf(mtx);
    sweep(40, [this]() { tickFrame(40); }, [this, a]() { goList(a); }, [this, a]() { goList(a); });
}

void OutputRules_Test::sweepPark()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    ParkEffect park(m_doc);
    const quint32 fid = m_doc->fixtureForAddress(Flame);
    sweep(Flame, [this]() { tickFrame(Flame); },
          [&park, fid]() { park.parkFixture(fid, { { 0u, uchar(255) } }); },
          [&park, fid]() { park.parkFixture(fid, { { 0u, uchar(255) } }); });
    park.unparkAll();
}

void OutputRules_Test::sweepMark()
{
    Fixture *spot = addSpot("Laser-ish", 20);
    spot->setOutputRules(Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    MarkEffect mark(m_doc);
    const quint32 fid = spot->id();
    sweep(21, [this]() { tickFrame(21); },
          [&mark, fid]() { mark.markFixture(fid, { { 1u, uchar(200) } }); },
          [&mark, fid]() { mark.markFixture(fid, { { 1u, uchar(200) } }); });
    mark.unmarkAll();
}

void OutputRules_Test::sweepLastLook()
{
    // A last look never holds a Show-only channel (S3): it never drives it,
    // before a stop or after, so there is nothing to sweep but that.
    Fixture *flame = addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    LastLookEffect::ChannelHold h;
    h.fixtureId = flame->id();
    h.universeId = 0;
    h.address = Flame;
    h.value = 255;
    m_doc->lastLook()->hold({ h });
    frame(Flame);
    QCOMPARE(sent(Flame), 0);
    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    QVERIFY(m_doc->showControl()->run(off, ShowControl::RoutePad));
    m_doc->lastLook()->hold({ h });     // a show stopped by it captures after it
    frame(Flame);
    QCOMPARE(sent(Flame), 0);
    m_doc->lastLook()->clear();
}

void OutputRules_Test::sweepCueStack()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    CueStack cs(m_doc);
    QMap<uint, uchar> values;
    values[uint(Flame)] = 255;
    cs.appendCue(Cue(values));
    cs.appendCue(Cue(values));
    QList<Universe *> ua = m_doc->inputOutputMap()->universes();
    auto tickStack = [this, &cs, ua]() {
        if (!cs.isStarted())
            cs.preRun();
        cs.write(ua);
        tickFrame(Flame);
    };
    sweep(Flame, tickStack, [&cs]() { cs.start(); cs.nextCue(); }, [&cs]() { cs.nextCue(); });
    cs.stop();
    cs.postRun(m_doc->masterTimer(), ua);
}

void OutputRules_Test::sweepEffectRunner()
{
    Fixture *flame = addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->assignFixture(flame->id());
    m_doc->addFixtureGroup(grp);
    QTemporaryDir dir;
    QFile js(dir.filePath("full.js"));
    QVERIFY(js.open(QIODevice::WriteOnly));
    js.write("(function() { var e = {}; e.apiVersion = 1; e.name = 'Full';"
             " e.tick = function(fixtures) { return fixtures.map(function() { return { dimmer: 1.0 }; }); };"
             " return e; })()");
    js.close();
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, m_doc);
    look->setScriptPath(js.fileName());
    m_doc->addPalette(look);
    Scene *scene = new Scene(m_doc);
    scene->addFixtureGroup(grp->id());
    scene->addPalette(look->id());
    m_doc->addFunction(scene);
    auto teardown = qScopeGuard([this, scene]() {
        m_doc->effectScriptRunner()->destroyInstancesForScene(scene->id(), false);
    });
    Chaser *a = listOf(scene);
    auto tickFx = [this]() {
        m_doc->masterTimer()->timerTick();
        m_doc->effectScriptRunner()->slotTick();
        m_doc->masterTimer()->timerTick();
        frame(Flame);
    };
    /* What the app does when the stopped scene is told stopped (queued
       there; from a tick on this thread the runner would claim the
       universes the tick holds): its effect let go. */
    sweep(Flame, tickFx, [this, a]() { goList(a); }, [this, a]() { goList(a); },
          [this, scene]() { m_doc->effectScriptRunner()->destroyInstancesForScene(scene->id(), true); });
}

/****************************************************************************
 * Sixth review (of 1391a70ca1), the engine's part
 ****************************************************************************/

/* R6-4 and E6: what passthrough brought in before a channel was made Show
   only never goes out on it, and a reset leaves it at its safe value --
   not the input's, not 0's "nothing set" computed past the rules. */
void OutputRules_Test::passthroughAndAResetLeaveAShowOnlyChannelAtSafe()
{
    m_uni->setPassthrough(true);
    m_uni->slotInputValueChanged(m_uni->id(), Flame, 255, QString());  // before the rule
    m_uni->slotInputValueChanged(m_uni->id(), Stage, 140, QString());
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    frame(Flame);
    QCOMPARE(sent(Flame), 0);
    QCOMPARE(int(uchar(m_uni->postGMValues()->at(Flame))), 0);
    QCOMPARE(sent(Stage), 140);                     // an ordinary channel: as before
    m_uni->reset(Flame - 2, 4);
    QCOMPARE(int(uchar(m_uni->postGMValues()->at(Flame))), 0);
    QCOMPARE(int(uchar(m_uni->postGMValues()->at(Stage))), 140);
    m_uni->reset();
    QCOMPARE(int(uchar(m_uni->postGMValues()->at(Flame))), 0);
    QCOMPARE(int(uchar(m_uni->preGMValue(Flame))), 0);
}

/* A channel stamped by its writer, channel by channel, for what was
   patched there: a Show-only fixture moved onto it mid-show is held until
   the writer stamps it again. */
void OutputRules_Test::aShowOnlyFixtureMovedOntoAStampedChannelWaits()
{
    Fixture *flame = addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    enum { Free = 40 };
    QSharedPointer<GenericFader> desk = m_uni->requestFader();
    GenericFader::ChannelRef fc = desk->getChannelFader(m_doc, m_uni, Fixture::invalidId(), quint32(Free));
    fc->setStart(150);
    fc->setCurrent(150);
    fc->setTarget(150);
    fc.release();
    desk->setChannelLiveEpoch(Free, GenericFader::liveEpoch());
    frame(Free);
    QCOMPARE(sent(Free), 150);
    flame->setAddress(Free);
    frame(Free);
    frame(Free);
    QCOMPARE(sent(Free), 0);
    desk->setChannelLiveEpoch(Free, GenericFader::stampEpoch());    // stamped again, by a hand
    frame(Free);
    QCOMPARE(sent(Free), 150);
    m_uni->dismissFader(desk);
}

/* R6-6, in the engine: a child takes its parent's stamp and keeps it for
   the session once the parent is gone; a new session leaves it old. */
void OutputRules_Test::aChildKeepsItsParentsStampForTheSession()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    Scene *fog = new Scene(m_doc);
    fog->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(fog);
    Script *script = new Script(m_doc);
    QVERIFY(script->setData(QStringLiteral("stoponexit:false\nstartfunction:%1\n").arg(fog->id())));
    m_doc->addFunction(script);
    script->start(m_doc->masterTimer(), FunctionParent::master());
    for (int i = 0; i < 8; i++)
        tickFrame(Flame);
    QVERIFY(!script->isRunning());
    QVERIFY(fog->isRunning());
    QCOMPARE(sent(Flame), 255);
    QVERIFY(fog->previewGate()->load() == false);
    // Another session (the switch to Operate again): old, held.
    m_doc->setMode(Doc::Design);
    m_doc->setMode(Doc::Operate);
    for (int i = 0; i < 3; i++)
        tickFrame(Flame);
    QVERIFY(fog->isRunning());
    QCOMPARE(sent(Flame), 0);
    stopFunctions();
}

/* R6-3: an automatic widget's source is the show only once armed in this
   session; its own first start arms nothing. */
void OutputRules_Test::anAutomaticSourceIsHeldUntilArmed()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    const FunctionParent clock(FunctionParent::AutoVCWidget, 9);
    QCOMPARE(Function::autoSourceEpoch(clock), quint32(0));
    Scene *s = startScene({ { Flame, 255 } }, clock.type(), clock.id());
    for (int i = 0; i < 3; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Flame), 0);
    QCOMPARE(Function::autoSourceEpoch(clock), quint32(0));        // still not armed
    s->stop(clock);
    tickFrame(Flame);
    Function::armAutoSource(clock);
    s->start(m_doc->masterTimer(), clock);
    for (int i = 0; i < 3; i++)
        tickFrame(Flame);
    QCOMPARE(sent(Flame), 255);
    stopFunctions();
}

/****************************************************************************
 * Eighth review (of 1e5ccc7b62): red until fixed
 ****************************************************************************/

/* R8-1: a Park set in Design is held in Operate (it counts for nothing),
   but its write past the Grand Master still marks the channel "past GM"
   for the frame (Universe::writePastGM()). writeFader() puts the GO's
   level back -- and updatePostGMValue() then skips the Grand Master. */
void OutputRules_Test::r8_aHeldParkTakesTheGoPastTheGrandMaster()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    // Parked at 0 in Design (to be sure it stays off): not the show.
    OutputLayer park(m_doc, Universe::Park, FadeChannel::Override | FadeChannel::PastGM,
                     QStringLiteral("Park"));
    park.set({ { m_doc->fixtureForAddress(Flame), 0, 0 } });
    m_doc->setMode(Doc::Operate);
    const FunctionParent go = FunctionParent::master();
    startScene({ { Flame, 255 } }, go.type(), go.id());     // a GO
    tickFrame(Flame);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 255);
    // The operator pulls the Grand Master down to nothing.
    m_doc->inputOutputMap()->setGrandMasterValue(0);
    tickFrame(Flame);
    tickFrame(Flame);
    const int out = sent(Flame);
    park.clear();
    stopFunctions();
    QCOMPARE(out, 0);
}

/* R8-2: bitsFor() leaves On-when-not-in-use off a Show-only fixture's
   own channels, but not off another fixture's that shares them: the
   channel has both bits and updatePostGMValue() puts it at full. */
void OutputRules_Test::r8_anOnAtOpenFixtureOnItsAddressDrivesIt()
{
    addDimmer("Laser", Flame, Fixture::ShowOnly);
    Fixture *work = addDimmer("Work 2", 40, Fixture::OnAtOpen);
    QVERIFY(work != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    work->setAddress(Flame);            // re-patched over it (a mistake)
    QCoreApplication::processEvents();  // the rebuild
    m_doc->outputRuleKeeper()->rebuild();
    // On as soon as the show is open (either setting: Operate arms it too).
    m_doc->outputRuleKeeper()->setOnAtOpenWhen(OutputRuleKeeper::OnAtShowOpen);
    m_doc->setMode(Doc::Operate);       // no GO
    m_uni->m_held[Flame].store(0);      // nothing left set by an earlier test
    tickFrame(Flame);
    tickFrame(Flame);
    qWarning() << "R8-2: caps" << Qt::hex << int(m_uni->channelCapabilities(Flame)) << Qt::dec
             << "armed" << m_uni->onAtOpenArmed() << "post" << int(uchar(m_uni->postGMValues()->at(Flame)))
             << "work at" << work->address() << "patched" << work->isPatched()
             << "sent" << sent(Flame);
    QCOMPARE(sent(Flame), 0);
}

/* R8-3: markShowOnly() takes a new mark only for a channel that was not
   Show only. A Show-only fixture moved by one channel keeps the old mark
   on the channels it still covers, so a GO from before the move still
   counts there (S0: "later than the channel's own: a fixture patched or
   moved onto it"). */
void OutputRules_Test::r8_aGoFromBeforeARepatchStillDrivesIt()
{
    enum { At = 30 };
    Fixture *laser = new Fixture(m_doc);
    laser->setName(QStringLiteral("Laser"));
    laser->setChannels(2);
    laser->setAddress(At);
    laser->setUniverse(0);
    laser->setOutputRules(Fixture::ShowOnly);
    QVERIFY(m_doc->addFixture(laser));
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    Scene *s = new Scene(m_doc);
    s->setValue(laser->id(), 0, 255);
    s->setValue(laser->id(), 1, 200);
    m_doc->addFunction(s);
    s->start(m_doc->masterTimer(), FunctionParent::master());     // a GO
    tickFrame(At);
    tickFrame(At);
    QCOMPARE(sent(At), 255);
    QCOMPARE(sent(At + 1), 200);
    // Re-patched one channel up, mid-show: nothing since is a GO.
    laser->setAddress(At + 1);
    QCoreApplication::processEvents();
    for (int i = 0; i < 3; i++)
        tickFrame(At);
    const int ch0 = sent(At + 1), ch1 = sent(At + 2);
    stopFunctions();
    QCOMPARE(ch0, 0);
    QCOMPARE(ch1, 0);
}

/* R8-4: InputOutputMap::applyBlackout(), setOutputInhibited(),
   OutputRuleKeeper::emergencyStop() and Monitor::sendMaintenance() copy
   postGMValues() with no frame lock and then dumpOutput() it -- live, with
   no hold. Universe::writeFader() lets a held writer write and only then
   puts the Show-only channels back: a copy taken in between has the held
   writer's level, and it is sent. The universe thread here is a second
   thread running processFaders(); the copy and the send are those calls'. */
void OutputRules_Test::r8_aSendBetweenTwoFramesCarriesAHeldWriter()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    // A preview (the Workbench's Play Live) on the whole universe, the flame too.
    QSharedPointer<GenericFader> preview = m_uni->requestFader(Universe::Override);
    preview->setHoldsShowOnly(true);
    for (int c = 0; c < 512; c++)
    {
        if (c == House || c == Stage || c == Relay || c == Work)
            continue;
        GenericFader::ChannelRef fc = preview->getChannelFader(m_doc, m_uni, Fixture::invalidId(), quint32(c));
        fc->setStart(255);
        fc->setCurrent(255);
        fc->setTarget(255);
    }
    frame(Flame);
    QCOMPARE(sent(Flame), 0);
    QCOMPARE(int(uchar(m_uni->postGMValues()->at(Flame))), 0);

    std::atomic<bool> stop { false };
    Universe *uni = m_uni;
    QThread *t = QThread::create([uni, &stop]() {
        while (!stop.load())
            uni->processFaders();
    });
    t->start();
    QByteArray caught;
    for (int i = 0; i < 200000 && caught.isEmpty(); i++)
    {
        // What applyBlackout()/setOutputInhibited()/emergencyStop() copy
        const QByteArray pg = m_uni->postGMValues()->mid(0, m_uni->usedChannels());
        if (pg.size() > Flame && pg.at(Flame) != 0)
            caught = pg;
    }
    stop.store(true);
    QVERIFY(t->wait(5000));
    delete t;
    int out = 0;
    if (!caught.isEmpty())
    {
        m_uni->dumpOutput(caught, true);   // ...and send
        out = sent(Flame);
    }
    m_uni->dismissFader(preview);
    frame(Flame);
    QVERIFY2(caught.isEmpty(), qPrintable(QString("a copy between two frames had the flame at %1; "
                                                  "dumpOutput() sent %2")
                                          .arg(caught.isEmpty() ? 0 : int(uchar(caught.at(Flame)))).arg(out)));
}

/* R8-5: EffectScriptRunner's fader for the show is setTrustedForShowOnly()
   (showStamp() 0xFFFFFFFF) and the writes on it are sorted by their
   scenes' stamps against the session only -- never against the channel's
   mark. The effect's writes follow the fixture to its new address (its
   address is read every tick), where the marks are new: S0 says a writer
   counts there only when stamped after the move. */
void OutputRules_Test::r8_anEffectFollowsARepatchPastTheMarks()
{
    Fixture *flame = addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->assignFixture(flame->id());
    m_doc->addFixtureGroup(grp);
    QTemporaryDir dir;
    QFile js(dir.filePath("full.js"));
    QVERIFY(js.open(QIODevice::WriteOnly));
    js.write("(function() { var e = {}; e.apiVersion = 1; e.name = 'Full';"
             " e.tick = function(fixtures) { return fixtures.map(function() { return { dimmer: 1.0 }; }); };"
             " return e; })()");
    js.close();
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, m_doc);
    look->setScriptPath(js.fileName());
    m_doc->addPalette(look);
    Scene *scene = new Scene(m_doc);
    scene->addFixtureGroup(grp->id());
    scene->addPalette(look->id());
    m_doc->addFunction(scene);
    auto teardown = qScopeGuard([this, scene]() {
        m_doc->effectScriptRunner()->destroyInstancesForScene(scene->id(), false);
    });
    Chaser *a = listOf(scene);
    enum { Moved = 60 };
    auto tickFx = [this]() {
        m_doc->masterTimer()->timerTick();
        m_doc->effectScriptRunner()->slotTick();
        m_doc->masterTimer()->timerTick();
        frame(Flame);
    };
    m_doc->setMode(Doc::Operate);
    qWarning("R8-5: GO");
    goList(a);                                  // a GO
    int most = 0;
    for (int i = 0; i < 8; i++)
    {
        tickFx();
        most = qMax(most, sent(Flame));
    }
    QVERIFY2(most > 0, "the GO'd effect drives the flame");
    // Re-patched mid-show to a free address: no GO since.
    qWarning() << "R8-5: drove" << most << "; moving";
    flame->setAddress(Moved);
    qWarning("R8-5: moved");
    QCoreApplication::processEvents();
    qWarning("R8-5: events done");
    QVERIFY(m_uni->channelCapabilities(Moved) & Universe::ShowOnlyOutput);
    most = 0;
    for (int i = 0; i < 8; i++)
    {
        tickFx();
        most = qMax(most, sent(Moved));
    }
    qWarning() << "R8-5: after move" << most;
    /* Let the effect go first: a tick on this thread would have the
       runner claim the universes the tick holds (as sweepEffectRunner). */
    m_doc->effectScriptRunner()->destroyInstancesForScene(scene->id(), true);
    stopFunctions();
    QCOMPARE(most, 0);
}

/* R8-6: Universe::dumpOutputFrame() in Blind sends only when Blind holds
   exempt circuits and the universe has one; otherwise it returns -- for
   the frame, for Blackout (applyBlackout()) and for the emergency stop's
   own send (OutputRuleKeeper::emergencyStop()) alike. The output keeps
   the last frame sent before Blind (a USB interface refreshes it, a node
   holds it): the GO'd laser stays on through All Off and Blackout, while
   the footer says "held: Blind". */
void OutputRules_Test::r8_allOffAndBlackoutInBlindNeverReachTheRig()
{
    // A universe with no house light on it (the usual laser universe).
    QVERIFY(m_doc->deleteFixture(m_doc->fixtureForAddress(House)));
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    QVERIFY(m_uni->hasBlackoutExempt(UNIVERSE_SIZE) == false);
    m_doc->setMode(Doc::Operate);
    const FunctionParent go = FunctionParent::master();
    startScene({ { Flame, 255 } }, go.type(), go.id());     // a GO
    tickFrame(Flame);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 255);
    m_doc->inputOutputMap()->setOutputInhibited(true);      // Blind
    tickFrame(Flame);
    const int inBlind = sent(Flame);
    // The pad's emergency stop, in Blind.
    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    QVERIFY(m_doc->showControl()->run(off, ShowControl::RoutePad));
    for (int i = 0; i < 3; i++)
        tickFrame(Flame);
    const int afterAllOff = sent(Flame);
    m_doc->inputOutputMap()->setBlackout(true);
    tickFrame(Flame);
    const int afterBlackout = sent(Flame);
    m_doc->inputOutputMap()->setBlackout(false);
    m_doc->inputOutputMap()->setOutputInhibited(false);
    stopFunctions();
    qWarning() << "R8-6: sent in Blind" << inBlind << "after All Off" << afterAllOff
             << "after Blackout" << afterBlackout;
    QCOMPARE(afterAllOff, 0);
    QCOMPARE(afterBlackout, 0);
}

/****************************************************************************
 * S0, every send
 ****************************************************************************/

void OutputRules_Test::everySendGoesThroughTheS0Builder()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    const FunctionParent go = FunctionParent::master();
    startScene({ { Flame, 200 } }, go.type(), go.id());     // a GO: the S0 value is 200
    // A held writer on the flame (a preview), full.
    QSharedPointer<GenericFader> preview = m_uni->requestFader(Universe::Override);
    preview->setHoldsShowOnly(true);
    {
        GenericFader::ChannelRef fc = preview->getChannelFader(m_doc, m_uni, Fixture::invalidId(), quint32(Flame));
        fc->setStart(255);
        fc->setCurrent(255);
        fc->setTarget(255);
    }
    auto cleanupPreview = qScopeGuard([this, preview]() { m_uni->dismissFader(preview); });
    tickFrame(Flame);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 200);                         // the frame

    // A plugin's own send of whatever it holds: the flame from S0.
    m_uni->dumpOutput(QByteArray(m_uni->usedChannels(), char(255)), true);
    QCOMPARE(sent(Flame), 200);
    // A maintenance send (another channel written, sent between frames).
    m_uni->write(Stage, 90, true);
    m_uni->sendCurrent();
    QCOMPARE(sent(Flame), 200);
    QCOMPARE(sent(Stage), 90);
    // Blackout on: safe; off: the S0 value again.
    m_doc->inputOutputMap()->setBlackout(true);
    QCOMPARE(sent(Flame), 0);
    m_doc->inputOutputMap()->setBlackout(false);
    QCOMPARE(sent(Flame), 200);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 200);

    // Blind on: one frame with it at safe; in Blind nothing else is sent;
    // Blind off: a new session -- safe until a GO.
    m_doc->inputOutputMap()->setOutputInhibited(true);
    QCOMPARE(sent(Flame), 0);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 0);
    m_doc->inputOutputMap()->setBlackout(true);         // Blackout in Blind: sent
    QCOMPARE(sent(Flame), 0);
    m_doc->inputOutputMap()->setBlackout(false);
    QCOMPARE(sent(Flame), 0);
    m_doc->inputOutputMap()->setOutputInhibited(false);
    QCOMPARE(sent(Flame), 0);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 0);

    // A GO again, then the emergency stop between two frames.
    stopFunctions();
    startScene({ { Flame, 200 } }, go.type(), go.id());
    tickFrame(Flame);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 200);
    m_doc->outputRuleKeeper()->emergencyStop();
    QCOMPARE(sent(Flame), 0);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 0);
    m_uni->dumpOutput(QByteArray(m_uni->usedChannels(), char(255)), true);
    QCOMPARE(sent(Flame), 0);                           // a plugin's send after it: still safe
    stopFunctions();
}

void OutputRules_Test::aSafeValueIsNeverScaledHeldOrLive()
{
    Fixture *g = addShowOnlyDef(QStringLiteral("Gobo wheel"), 40,
                                { qMakePair(QLCChannel::Gobo, QLCChannel::NoColour) });
    QVERIFY(g != nullptr);
    const_cast<QLCFixtureDef *>(g->fixtureDef())->setType(QLCFixtureDef::MovingHead);
    const_cast<QLCChannel *>(g->channel(0))->setDefaultValue(40);   // "open" is 40
    m_doc->outputRuleKeeper()->rebuild();
    QCOMPARE(int(OutputRuleKeeper::safeValue(g, 0)), 40);
    m_doc->inputOutputMap()->setGrandMasterChannelMode(GrandMaster::AllChannels);
    m_doc->inputOutputMap()->setGrandMasterValue(128);
    frame(40);
    QCOMPARE(sent(40), 40);                             // held (Design): unscaled
    m_doc->setMode(Doc::Operate);
    frame(40);
    frame(40);
    QCOMPARE(sent(40), 40);                             // live, nothing drives it: the same
}

/****************************************************************************
 * Ninth review (of 690eeeab48): red until fixed
 ****************************************************************************/

/* R9-1: OutputPatch::setPending() (InputOutputMap::holdOutputPatch():
   Teaching mode's "hold real output", the rig-network gate) closes the
   plugin line at once -- no frame with the Show-only channels at safe goes
   out first. A node or a USB interface keeps the last frame it was given
   (the stub keeps it, as an Art-Net node holding its last look does): the
   GO'd laser stays on, and All Off, Blackout, a cue letting go never reach
   it again. */
void OutputRules_Test::r9_aHeldOutputKeepsTheLaserOnThroughAllOff()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    const FunctionParent go = FunctionParent::master();
    startScene({ { Flame, 255 } }, go.type(), go.id());     // a GO
    tickFrame(Flame);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 255);
    auto restore = qScopeGuard([this]() {
        m_doc->inputOutputMap()->setOutputPatch(0, m_stub->name(), QString(), 0);
    });
    // The output goes on hold, as holdOutputPatch() does it.
    QVERIFY(m_uni->setOutputPatchPending(m_stub, m_stub->outputs().at(0), 0, 0, QString()));
    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    QVERIFY(m_doc->showControl()->run(off, ShowControl::RoutePad));
    for (int i = 0; i < 3; i++)
        tickFrame(Flame);
    const int onTheLine = sent(Flame);
    stopFunctions();
    QVERIFY2(onTheLine == 0, qPrintable(QString("the held line's last frame has the flame at %1 "
                                                "after All Off").arg(onTheLine)));
}

/* R9-1, re-patched: Universe::setOutputPatch() -> OutputPatch::set()
   closes the old line and opens the new one; the old line's last frame is
   the GO'd laser, on. */
void OutputRules_Test::r9_aMovedOutputKeepsTheLaserOnThroughAllOff()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    const FunctionParent go = FunctionParent::master();
    startScene({ { Flame, 255 } }, go.type(), go.id());     // a GO
    tickFrame(Flame);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 255);
    auto restore = qScopeGuard([this]() {
        m_doc->inputOutputMap()->setOutputPatch(0, m_stub->name(), QString(), 0);
    });
    // Mid-show, the universe is sent to another line (the I/O tab is open in Operate).
    QVERIFY(m_doc->inputOutputMap()->setOutputPatch(0, m_stub->name(), QString(), 1));
    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    QVERIFY(m_doc->showControl()->run(off, ShowControl::RoutePad));
    for (int i = 0; i < 3; i++)
        tickFrame(Flame);
    const int oldLine = sent(Flame);
    const int newLine = int(uchar(m_stub->m_universe.at(512 + Flame)));
    stopFunctions();
    QCOMPARE(newLine, 0);
    QVERIFY2(oldLine == 0, qPrintable(QString("the old line's last frame has the flame at %1 "
                                              "after All Off").arg(oldLine)));
}

/* R9-2: Doc::patchFixtures() applies every fixture to the universes
   (setChannelDefaultValue(): the definition's default, the channel not yet
   Show only), calls setModified() and only then emits fixtureChanged() --
   when OutputRuleKeeper::markNow() makes the channels Show only. The
   universe thread runs on its own: a frame in that window (here: one run
   from Doc::modified, emitted inside it) sends the laser's default. In
   Design too -- the channel is not Show only yet, so nothing holds it. */
void OutputRules_Test::r9_aLaserBeingPatchedSendsItsDefault()
{
    enum { At = 100 };
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(QStringLiteral("Review9"));
    def->setModel(QStringLiteral("Laser R9"));
    def->setType(QLCFixtureDef::Laser);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(QStringLiteral("1ch"));
    QLCChannel *ch = new QLCChannel();
    ch->setName(QStringLiteral("Mode"));
    ch->setGroup(QLCChannel::Effect);
    ch->setDefaultValue(200);               // e.g. "auto program"
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    def->addMode(mode);
    mode->cacheHeads();
    QVERIFY(m_doc->fixtureDefCache()->addFixtureDef(def));
    Fixture *laser = new Fixture(m_doc);
    laser->setName(QStringLiteral("Laser R9"));
    laser->setFixtureDefinition(def, mode);
    laser->setOutputRules(Fixture::ShowOnly);
    laser->setPatched(false);               // in the inventory
    QVERIFY(m_doc->addFixture(laser));
    m_doc->outputRuleKeeper()->rebuild();
    QCOMPARE(int(OutputRuleKeeper::safeValue(laser, 0)), 0);
    frame(At);
    QCOMPARE(sent(At), 0);

    int inWindow = 0;
    QMetaObject::Connection c = connect(m_doc, &Doc::modified, this, [this, &inWindow](bool) {
        frame(At);                          // the universe thread's frame
        inWindow = qMax(inWindow, sent(At));
    });
    QString why;
    const bool ok = m_doc->patchFixtures({ Doc::FixturePatch{ laser->id(), 0, At } }, &why);
    disconnect(c);
    QVERIFY2(ok, qPrintable(why));
    QVERIFY(m_uni->channelCapabilities(At) & Universe::ShowOnlyOutput);    // marked after
    frame(At);
    QCOMPARE(sent(At), 0);
    QVERIFY2(inWindow == 0, qPrintable(QString("a frame inside patchFixtures() sent the laser's "
                                               "default %1 (not Show only yet)").arg(inWindow)));
}

/* R9-3: S0 says a writer that does not count "sets no flag". writeRelative()
   calls markWritten() on every channel before it checks writeBlocked(): a
   held relative writer leaves the Show-only channel "written this frame"
   (isChannelSet(), channelSnapshot().set, the auto-open shutter test). */
void OutputRules_Test::r9_aHeldRelativeWriterSetsTheWrittenFlag()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    m_uni->m_held[Flame].store(0);
    frame(Flame);
    frame(Flame);
    QVERIFY(m_uni->channelSnapshot(Flame).set == false);
    // A preview's EFX in relative mode on it: held.
    QSharedPointer<GenericFader> efx = m_uni->requestFader();
    efx->setHoldsShowOnly(true);
    {
        GenericFader::ChannelRef fc = efx->getChannelFader(m_doc, m_uni, Fixture::invalidId(), quint32(Flame));
        fc->addFlag(FadeChannel::Relative);
        fc->setStart(200);
        fc->setCurrent(200);
        fc->setTarget(200);
    }
    frame(Flame);
    const Universe::ChannelSnapshot snap = m_uni->channelSnapshot(Flame);
    m_uni->dismissFader(efx);
    frame(Flame);
    QCOMPARE(sent(Flame), 0);
    QVERIFY2(snap.set == false, qPrintable(QString("a held writer marked the Show-only channel written "
                                                   "(%1 frames since)").arg(snap.framesSinceWrite)));
}

/****************************************************************************
 * Ninth review: the fixes' own tests
 ****************************************************************************/

void OutputRules_Test::everyClosingLineGetsShowOnlySafeLast()
{
    enum How { RePatch, Hold, Unpatch, PatchDestroyed, UniverseRemoved, Retarget };
    const QList<QPair<How, const char *> > hows = {
        { RePatch, "re-patched to another line" },
        { Hold, "held (Teaching's hold of real output, the rig-network gate)" },
        { Unpatch, "unpatched" },
        { PatchDestroyed, "its patch destroyed" },
        { UniverseRemoved, "its universe removed" },
        { Retarget, "aimed at another node (outputIP)" },
    };
    /* Review 10: as real plugins send. Sync: inside writeUniverse() (Art-Net,
       E1.31). Async: from their own thread (a USB widget, WLED) -- a frame
       counts only once the send thread has sent it (pumpAsync(), each
       frame), or flushOutput(); closing the line drops what is kept. */
    enum Mode { Sync, Async };
    InputOutputMap *io = m_doc->inputOutputMap();
    const quint32 Line = 2, Other = 3;
    auto onLine = [this](quint32 line, int address) {
        return int(uchar(m_stub->m_universe.at(int(line) * 512 + address)));
    };
    auto atTarget = [this](const QString &target, int address) {
        const QByteArray f = m_stub->m_lastByTarget.value(target);
        return address < f.size() ? int(uchar(f.at(address))) : -1;
    };
    auto addOn = [this](const QString &name, quint32 uni, int address, int rules) -> Fixture * {
        Fixture *f = new Fixture(m_doc);
        f->setName(name);
        f->setChannels(1);
        f->setUniverse(uni);
        f->setAddress(quint32(address));
        f->setOutputRules(Fixture::OutputRules(rules));
        if (m_doc->addFixture(f) == false)
        {
            delete f;
            return nullptr;
        }
        return f;
    };
    auto modeOff = qScopeGuard([this]() {
        m_stub->m_async = false;
        m_stub->m_asyncPending.clear();
        m_stub->m_lastByTarget.clear();
    });

    for (Mode mode : { Sync, Async })
    for (const auto &how : hows)
    {
        const QString what = QString("%1 (%2)").arg(QString::fromLatin1(how.second),
                                                    mode == Async ? "a plugin that sends from its own thread"
                                                                  : "a plugin that sends at once");
        // The universe removed is the last one (only the last can go).
        const bool last = how.first == UniverseRemoved;
        const quint32 uniIdx = last ? io->universesCount() - 1 : 0;
        const int idx = last ? 0 : 1;                   // universe 1 keeps its line 0 at index 0
        Fixture *flame = addOn("Flame", uniIdx, Flame, Fixture::ShowOnly);
        Fixture *stage = last ? addOn("Stage", uniIdx, Stage, 0)
                              : m_doc->fixture(m_doc->fixtureForAddress(quint32(Stage)));
        QVERIFY(flame != nullptr && stage != nullptr);
        m_doc->outputRuleKeeper()->rebuild();
        Universe *u = io->universes().at(int(uniIdx));
        QVERIFY2(io->setOutputPatch(uniIdx, m_stub->name(), QString(), Line, false, idx), qPrintable(what));
        if (how.first == Retarget)
            u->outputPatch(idx)->setPluginParameter(QStringLiteral("outputIP"), QStringLiteral("10.0.0.1"));
        m_stub->m_async = mode == Async;
        m_stub->m_lastByTarget.clear();

        // Mid-show: a GO'd cue has the laser on, a dimmer at 100.
        m_doc->setMode(Doc::Operate);
        Scene *s = new Scene(m_doc);
        s->setValue(flame->id(), 0, 255);
        s->setValue(stage->id(), 0, 100);
        m_doc->addFunction(s);
        s->start(m_doc->masterTimer(), FunctionParent::master());
        auto tick = [this, &u]() {
            m_doc->masterTimer()->timerTick();
            if (u != nullptr)
                u->processFaders();
            m_stub->pumpAsync();                        // the send thread's pass
        };
        tick();
        tick();
        QVERIFY2(onLine(Line, Flame) == 255, qPrintable(what));
        QCOMPARE(onLine(Line, Stage), 100);
        // The show's frame since, handed over but not sent yet (Async)
        u->processFaders();

        switch (how.first)
        {
            case RePatch:
                QVERIFY(io->setOutputPatch(uniIdx, m_stub->name(), QString(), Other, false, idx));
            break;
            case Hold:
                QVERIFY(u->setOutputPatchPending(m_stub, m_stub->outputs().at(int(Line)), idx, Line, QString()));
            break;
            case Unpatch:
                QVERIFY(io->setOutputPatch(uniIdx, QString(), QString(), QLCIOPlugin::invalidLine(), false, idx));
            break;
            case PatchDestroyed:
                delete u->m_outputPatchList.takeAt(idx);
            break;
            case UniverseRemoved:
                QVERIFY(io->removeUniverse(int(uniIdx)));
                u = nullptr;
            break;
            case Retarget:
                u->outputPatch(idx)->setPluginParameter(QStringLiteral("outputIP"), QStringLiteral("10.0.0.2"));
            break;
        }
        if (how.first == Retarget)
        {
            // The old node's last frame: the laser at safe, the rest as it was.
            QVERIFY2(atTarget("10.0.0.1/2", Flame) == 0,
                     qPrintable(QString("%1: the old node's last frame has the laser at %2")
                                .arg(what).arg(atTarget("10.0.0.1/2", Flame))));
            QCOMPARE(atTarget("10.0.0.1/2", Stage), 100);
        }
        else
        {
            // The closing line's last frame: the laser at safe, the rest as it was.
            QVERIFY2(onLine(Line, Flame) == 0,
                     qPrintable(QString("%1: its last frame has the laser at %2").arg(what).arg(onLine(Line, Flame))));
            QVERIFY2(onLine(Line, Stage) == 100, qPrintable(what));
            QVERIFY2(m_stub->m_openOutputs.contains(Line) == false, qPrintable(what));
        }

        if (last)
        {
            QVERIFY(io->addUniverse(uniIdx));
            u = io->universes().at(int(uniIdx));
        }
        else
        {
            // Nothing more reaches it; All Off goes out where the universe is.
            m_stub->m_universe[int(Line) * 512 + Stage] = char(77);
            ShowControl::Request off;
            off.command = ShowControl::AllOff;
            QVERIFY(m_doc->showControl()->run(off, ShowControl::RoutePad));
            for (int i = 0; i < 3; i++)
                tick();
            if (how.first == Retarget)
            {
                QCOMPARE(atTarget("10.0.0.1/2", Stage), 100);     // no frame to the old node since
                QCOMPARE(atTarget("10.0.0.2/2", Flame), 0);
                QCOMPARE(atTarget("10.0.0.2/2", Stage), 0);       // All Off reached the new one
            }
            else
                QVERIFY2(onLine(Line, Stage) == 77, qPrintable(what));
            QCOMPARE(sent(Flame), 0);
            if (how.first == RePatch)
                QCOMPARE(onLine(Other, Flame), 0);
        }

        stopFunctions();
        m_stub->m_async = false;
        m_stub->m_asyncPending.clear();
        if (!last && u->outputPatchesCount() > idx)
            QVERIFY(io->setOutputPatch(uniIdx, QString(), QString(), QLCIOPlugin::invalidLine(), false, idx));
        QCOMPARE(u->outputPatchesCount(), last ? 0 : 1);
        m_doc->setMode(Doc::Design);
        QVERIFY(m_doc->deleteFunction(s->id()));
        QVERIFY(m_doc->deleteFixture(flame->id()));
        if (last)
            QVERIFY(m_doc->deleteFixture(stage->id()));
        m_doc->outputRuleKeeper()->rebuild();
        m_stub->m_universe.fill(char(0));
    }
}

void OutputRules_Test::aRetargetInOneStepClosesOnlyTheOldNode()
{
    QVERIFY(OutputPatch::isTargetParameter(QStringLiteral("outputPort")));     // OSC
    QVERIFY(OutputPatch::isTargetParameter(QStringLiteral("transmitMode")) == false);
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    InputOutputMap *io = m_doc->inputOutputMap();
    auto flameAt = [this](const char *target) {
        const QByteArray f = m_stub->m_lastByTarget.value(QString::fromLatin1(target));
        return Flame < f.size() ? int(uchar(f.at(Flame))) : -1;
    };
    QVERIFY(io->setOutputPatch(0, m_stub->name(), QString(), 2, false, 1));
    OutputPatch *op = m_uni->outputPatch(1);
    op->setPluginParameters({ { QStringLiteral("outputIP"), QStringLiteral("10.0.0.1") },
                              { QStringLiteral("outputUni"), 1 } });
    m_doc->setMode(Doc::Operate);
    const FunctionParent go = FunctionParent::master();
    startScene({ { Flame, 255 } }, go.type(), go.id());
    tickFrame(Flame);
    tickFrame(Flame);
    m_stub->m_lastByTarget.clear();
    tickFrame(Flame);
    QCOMPARE(flameAt("10.0.0.1/2"), 255);

    // Both at once: one closing frame, to the old node only.
    op->setPluginParameters({ { QStringLiteral("outputIP"), QStringLiteral("10.0.0.2") },
                              { QStringLiteral("outputUni"), 2 } });
    QCOMPARE(flameAt("10.0.0.1/2"), 0);
    QVERIFY2(m_stub->m_lastByTarget.contains("10.0.0.2/2") == false,
             "the new node was sent a frame meant for the old one");
    tickFrame(Flame);
    QCOMPARE(flameAt("10.0.0.2/2"), 255);

    // Unset in the same step (sACN taking its unicast address in place of
    // outputIP): still one closing frame, to where it was going.
    m_stub->m_lastByTarget.clear();
    op->setPluginParameters({ { QStringLiteral("ucastIP"), QStringLiteral("10.0.0.3") } },
                            { QStringLiteral("outputIP") });
    QCOMPARE(flameAt("10.0.0.2/2"), 0);
    QCOMPARE(m_stub->m_lastByTarget.size(), 1);
    QVERIFY(op->getPluginParameters().contains(QStringLiteral("outputIP")) == false);
    stopFunctions();
    QVERIFY(io->setOutputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine(), false, 1));
    m_stub->m_lastByTarget.clear();
    m_stub->m_universe.fill(char(0));
}

void OutputRules_Test::aPatchOpenedAimedSendsNothingToTheDefaultTarget()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    const FunctionParent go = FunctionParent::master();
    startScene({ { Flame, 255 } }, go.type(), go.id());
    tickFrame(Flame);
    m_stub->m_lastByTarget.clear();

    InputOutputMap *io = m_doc->inputOutputMap();
    QMap<QString, QVariant> aim;
    aim.insert(QStringLiteral("outputIP"), QStringLiteral("10.0.0.9"));
    aim.insert(QStringLiteral("wledAddress"), 1);
    QVERIFY(io->setOutputPatch(0, m_stub->name(), QString(), 2, false, 1, aim));
    QCOMPARE(m_uni->outputPatch(1)->getPluginParameters().value("outputIP").toString(),
             QString("10.0.0.9"));
    // Nothing at all on the new line yet: no frame, no closing frame, to
    // whatever it would have sent to before it was aimed.
    foreach (const QString &k, m_stub->m_lastByTarget.keys())
        QVERIFY2(k.endsWith("/2") == false,
                 qPrintable(QString("a frame went to %1 before the patch was aimed").arg(k)));
    tickFrame(Flame);
    foreach (const QString &k, m_stub->m_lastByTarget.keys())
        QVERIFY2(k.endsWith("/2") == false || k == "10.0.0.9/2", qPrintable(k));
    QCOMPARE(int(uchar(m_stub->m_lastByTarget.value("10.0.0.9/2").at(Flame))), 255);

    // The same target again, an int as a string: nothing sent, no closing frame.
    m_stub->m_lastByTarget.clear();
    m_uni->outputPatch(1)->setPluginParameter(QStringLiteral("wledAddress"), QStringLiteral("1"));
    m_uni->outputPatch(1)->setPluginParameters({ { QStringLiteral("outputIP"), QStringLiteral("10.0.0.9") } });
    QVERIFY(m_stub->m_lastByTarget.isEmpty());

    stopFunctions();
    QVERIFY(io->setOutputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine(), false, 1));
    m_stub->m_lastByTarget.clear();
    m_stub->m_universe.fill(char(0));
}

void OutputRules_Test::aClosingLineWithNoShowOnlyGetsNothingExtra()
{
    QCOMPARE(m_uni->showOnlyChannelCount(), 0);
    InputOutputMap *io = m_doc->inputOutputMap();
    QVERIFY(io->setOutputPatch(0, m_stub->name(), QString(), 2, false, 1));
    put(Stage, 100);
    frame(Stage);
    QCOMPARE(int(uchar(m_stub->m_universe.at(1024 + Stage))), 100);
    m_stub->m_universe[1024 + Stage] = char(77);
    QVERIFY(io->setOutputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine(), false, 1));
    // Closed as it always was: no frame of ours on the way out.
    QCOMPARE(int(uchar(m_stub->m_universe.at(1024 + Stage))), 77);
    QCOMPARE(m_uni->outputPatchesCount(), 1);
    m_stub->m_universe.fill(char(0));
}

void OutputRules_Test::aReleasedLineStartsFromTheCurrentBuild()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    const FunctionParent go = FunctionParent::master();
    startScene({ { Flame, 255 } }, go.type(), go.id());
    tickFrame(Flame);
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 255);
    auto restore = qScopeGuard([this]() {
        m_doc->inputOutputMap()->setOutputPatch(0, m_stub->name(), QString(), 0);
    });
    QVERIFY(m_uni->setOutputPatchPending(m_stub, m_stub->outputs().at(0), 0, 0, QString()));
    QCOMPARE(sent(Flame), 0);                   // its closing frame
    tickFrame(Flame);
    m_stub->m_universe[Flame] = char(77);      // nothing reaches a held line
    tickFrame(Flame);
    QCOMPARE(sent(Flame), 77);

    // Released, a plugin that sends only changes: the first frame goes out,
    // and it is the current S0 build (the GO still drives the laser).
    m_stub->m_dropUnchanged = true;
    QVERIFY(m_uni->resolveOutputPatch(0, 0));
    tickFrame(Flame);
    QVERIFY(m_stub->m_lastDataChanged);
    QCOMPARE(sent(Flame), 255);
    stopFunctions();
}

void OutputRules_Test::patchListChangesUnderTheFrameLock()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    put(Flame, 200);
    Universe *uni = m_uni;
    std::atomic<bool> stop { false };
    std::atomic<int> frames { 0 };
    // The universe thread's side: frames, and sends between them.
    QThread *t = QThread::create([uni, &stop, &frames]() {
        while (stop.load() == false)
        {
            uni->processFaders();
            uni->sendCurrent();
            uni->sendHeld();
            frames.fetch_add(1);
        }
    });
    t->start();
    for (int i = 0; i < 300; i++)
    {
        QVERIFY(uni->setOutputPatch(m_stub, 2, 1));                         // added
        QVERIFY(uni->setOutputPatch(m_stub, 3, 1));                         // moved
        QVERIFY(uni->setOutputPatchPending(m_stub, QStringLiteral("x"), 1)); // held
        QVERIFY(uni->setOutputPatch(m_stub, 2, 2));                         // another added
        QVERIFY(uni->setOutputPatch(nullptr, QLCIOPlugin::invalidLine(), 2));
        QVERIFY(uni->setOutputPatch(nullptr, QLCIOPlugin::invalidLine(), 1));
    }
    stop.store(true);
    t->wait();
    delete t;
    QVERIFY(frames.load() > 0);
    QCOMPARE(uni->outputPatchesCount(), 1);
    // Every line closed at safe; line 0 carries the show.
    QCOMPARE(int(uchar(m_stub->m_universe.at(1024 + Flame))), 0);
    QCOMPARE(int(uchar(m_stub->m_universe.at(1536 + Flame))), 0);
    m_stub->m_universe.fill(char(0));
}

void OutputRules_Test::aHeldAutoRemoveChannelLetsGoOfNothing()
{
    addDimmer("Flame", Flame, Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    m_uni->m_held[Flame].store(0);
    m_uni->m_letGoBy[Flame].store(0);
    frame(Flame);
    frame(Flame);
    // A one-shot writer (a slider let go, a programmer edit) on it: held.
    QSharedPointer<GenericFader> f = m_uni->requestFader();
    f->setHoldsShowOnly(true);
    {
        GenericFader::ChannelRef fc = f->getChannelFader(m_doc, m_uni, Fixture::invalidId(), quint32(Flame));
        fc->addFlag(FadeChannel::AutoRemove);
        fc->setStart(200);
        fc->setCurrent(200);
        fc->setTarget(200);
    }
    frame(Flame);
    const quint32 token = f->token();
    const Universe::ChannelSnapshot snap = m_uni->channelSnapshot(Flame);
    const quint32 letGoBy = m_uni->m_letGoBy[Flame].load();
    const bool held = m_uni->m_held[Flame].load() != 0;
    m_uni->dismissFader(f);
    frame(Flame);
    QCOMPARE(sent(Flame), 0);
    QVERIFY(snap.set == false);
    QVERIFY2(held == false, "a held writer left the channel \"set\"");
    QVERIFY(token != 0);
    QVERIFY2(letGoBy != token, "a held writer is named as letting go of the channel");
}

/*****************************************************************************
 * The emergency stop is "Everything is off" (eval/0412, E8 and E9)
 *****************************************************************************/

void OutputRules_Test::theEmergencyStopPutsAFoggerAtRest()
{
    /* E8: Stop all puts every fixture at rest (UX-70: "a fog machine kept
       pumping"), and so does the emergency stop -- the pad's four, the phone
       page's, an OSC or MSC ALL OFF, all ShowControl AllOff -- through the
       same code (FixtureRest::restAfterStop()). Before, the stop cut only
       Show-only channels: a fogger not marked Show only kept the fog level
       its cue left on its LTP channel while the pad said "Emergency stop.
       Everything is off." */
    enum { Fog = 50 };
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(QStringLiteral("Test"));
    def->setModel(QStringLiteral("Eval Fogger"));
    def->setType(QLCFixtureDef::Smoke);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(QStringLiteral("1ch"));
    QLCChannel *fog = new QLCChannel();
    fog->setName(QStringLiteral("Fog"));
    fog->setGroup(QLCChannel::Effect);         // LTP, as a fogger's output often is
    def->addChannel(fog);
    mode->insertChannel(fog, 0);
    def->addMode(mode);
    QVERIFY(m_doc->fixtureDefCache()->addFixtureDef(def));
    Fixture *fogger = new Fixture(m_doc);
    fogger->setName(QStringLiteral("Fogger"));
    fogger->setFixtureDefinition(def, mode);
    fogger->setUniverse(0);
    fogger->setAddress(quint32(Fog));
    QVERIFY(m_doc->addFixture(fogger));
    m_doc->outputRuleKeeper()->rebuild();

    Scene *s = new Scene(m_doc);
    s->setValue(fogger->id(), 0, 200);
    m_doc->addFunction(s);
    s->start(m_doc->masterTimer(), FunctionParent::master());
    for (int i = 0; i < 5; i++)
        tickFrame(Fog);
    QCOMPARE(sent(Fog), 200);                   // the cue's fog

    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    QVERIFY(m_doc->showControl()->run(off, ShowControl::RoutePad));
    for (int i = 0; i < 10; i++)
        tickFrame(Fog);
    QCOMPARE(m_doc->masterTimer()->runningFunctions(), 0);
    QVERIFY2(sent(Fog) == 0, qPrintable(QString("the fogger still gets %1 after the emergency stop "
                                                "(Stop all would have put it at rest)").arg(sent(Fog))));
}

void OutputRules_Test::theEmergencyStopEndsTheFocusLight()
{
    /* E9, padrun.h rule 6: in FOCUS "the four-together stop still stops";
       focus-calls.md: "still stops everything"; the pad says "Emergency
       stop. Everything is off." A venue or quick focus lights its fixture
       through its own layer (Universe::Focus, over everything but Park),
       which stopping functions does not touch: the stop ends the focus. */
    const quint32 stage = m_doc->fixtureForAddress(Stage);
    m_doc->monitorProperties()->setFixturePosition(stage, 0, 0, QVector3D(4000, 2000, 5));
    startFocus(stage, 1.0f);
    QCOMPARE(frame(Stage), 255);
    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    QVERIFY(m_doc->showControl()->run(off, ShowControl::RoutePad));
    int after = 0;
    for (int i = 0; i < 3; i++)
        after = tickFrame(Stage);
    QVERIFY2(after == 0, qPrintable(QString("the focus light is at %1 after \"Everything is off\"").arg(after)));
    QCOMPARE(sent(Stage), 0);
    // Ended, not paused: every surface reads the tool from the session.
    QVERIFY(!m_doc->programmer()->focusSession()->isActive());
}

QTEST_GUILESS_MAIN(OutputRules_Test)

/* Review 10: Blind sends nothing of the preview. A fixture patched (or a
   channel first written) after Blind started lies past the frozen frame's
   end; Universe::closingFrame() fills that tail from the LIVE values -- the
   preview -- and a line re-patched, held or unpatched in Blind gets them as
   its last frame, which the node then keeps. */
void OutputRules_Test::review10_aClosingLineInBlindGetsNoPreview()
{
    InputOutputMap *io = m_doc->inputOutputMap();
    m_doc->outputRuleKeeper()->setBlindHoldsExempt(false);   // Blind: nothing goes out
    io->setOutputInhibited(true);                             // Blind
    // A universe made in Blind (added, or a show loaded in Blind): its
    // frozen frame is empty -- R9's setInhibitOutput() at creation.
    QVERIFY(io->addUniverse());
    const quint32 uni = io->universesCount() - 1;
    Universe *u = io->universes().at(int(uni));
    QVERIFY(u->inhibitOutput());
    qInfo("frozen frame of the new universe: %d channels", int(u->m_blindFrozen.size()));
    QVERIFY(io->setOutputPatch(uni, m_stub->name(), QString(), 2, false, 0));

    auto addOn = [this, uni](const QString &name, int address, int rules) {
        Fixture *f = new Fixture(m_doc);
        f->setName(name);
        f->setChannels(1);
        f->setUniverse(uni);
        f->setAddress(quint32(address));
        f->setOutputRules(Fixture::OutputRules(rules));
        if (!m_doc->addFixture(f)) { delete f; return (Fixture *)nullptr; }
        return f;
    };
    Fixture *flame = addOn("Flame2", 5, Fixture::ShowOnly);
    Fixture *wash = addOn("Wash", 3, 0);
    QVERIFY(flame && wash);
    m_doc->outputRuleKeeper()->rebuild();
    // Programmed in Blind: a preview only
    QSharedPointer<GenericFader> fader = u->requestFader();
    {
        GenericFader::ChannelRef fc = fader->getChannelFader(m_doc, u, Fixture::invalidId(), 3);
        fc->setStart(200);
        fc->setCurrent(200);
        fc->setTarget(200);
        fc.release();
        fader->stampLiveEpoch();
    }
    u->processFaders();
    u->processFaders();
    qInfo("the wash in the preview: %d", int(uchar(u->postGMValues()->at(3))));
    const int before = uchar(m_stub->m_universe.at(1024 + 3));

    // Re-patched to another interface while still in Blind
    QVERIFY(io->setOutputPatch(uni, m_stub->name(), QString(), 3, false, 0));
    const int wash2 = uchar(m_stub->m_universe.at(1024 + 3));
    const int flame2 = uchar(m_stub->m_universe.at(1024 + 5));

    QVERIFY(io->setOutputPatch(uni, QString(), QString(), QLCIOPlugin::invalidLine(), false, 0));
    u->dismissFader(fader);
    fader.clear();
    QVERIFY(m_doc->deleteFixture(flame->id()));
    QVERIFY(m_doc->deleteFixture(wash->id()));
    QVERIFY(io->removeUniverse(int(uni)));
    m_doc->outputRuleKeeper()->rebuild();
    m_stub->m_universe.fill(char(0));

    QCOMPARE(before, 0);                    // Blind: nothing went out
    QCOMPARE(flame2, 0);
    QVERIFY2(wash2 == 0, qPrintable(QString("the closing frame in Blind sent the preview: the wash at %1")
                                    .arg(wash2)));
}

/****************************************************************************
 * Q326: the emergency stop holds a Show-only head where it is
 ****************************************************************************/

Fixture *OutputRules_Test::addShowOnlyMover(const QString &name, int address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    // One definition per call: the cache refuses a second of the same name.
    static int made = 0;
    def->setManufacturer(QStringLiteral("Q326"));
    def->setModel(QStringLiteral("%1 %2").arg(name).arg(++made));
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(QStringLiteral("6ch"));
    struct Ch { const char *name; QLCChannel::Group group; QLCChannel::ControlByte byte; };
    const Ch chans[] = {
        { "Dimmer", QLCChannel::Intensity, QLCChannel::MSB },
        { "Pan", QLCChannel::Pan, QLCChannel::MSB },
        { "Pan fine", QLCChannel::Pan, QLCChannel::LSB },
        { "Tilt", QLCChannel::Tilt, QLCChannel::MSB },
        { "Tilt fine", QLCChannel::Tilt, QLCChannel::LSB },
        { "Colour", QLCChannel::Colour, QLCChannel::MSB },
    };
    quint32 i = 0;
    for (const Ch &c : chans)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString::fromLatin1(c.name));
        ch->setGroup(c.group);
        ch->setControlByte(c.byte);
        def->addChannel(ch);
        mode->insertChannel(ch, i++);
    }
    def->addMode(mode);
    mode->cacheHeads();
    if (!m_doc->fixtureDefCache()->addFixtureDef(def))
    {
        delete def;
        return nullptr;
    }
    Fixture *f = new Fixture(m_doc);
    f->setName(name);
    f->setFixtureDefinition(def, mode);
    f->setUniverse(0);
    f->setAddress(quint32(address));
    f->setOutputRules(Fixture::ShowOnly);
    if (!m_doc->addFixture(f))
    {
        delete f;
        return nullptr;
    }
    m_doc->outputRuleKeeper()->rebuild();
    return f;
}

void OutputRules_Test::checkMoverHoldsThroughStop(const std::function<void()> &stop)
{
    enum { Dim = 50, Pan, PanFine, Tilt, TiltFine, Colour };
    Fixture *mover = addShowOnlyMover(QStringLiteral("Mover"), Dim);
    QVERIFY(mover != nullptr);
    for (int a = Dim; a <= Colour; a++)
        QCOMPARE(int(uchar(m_uni->m_showOnlySafe.at(a))), 0);
    m_doc->setMode(Doc::Operate);
    Scene *look = new Scene(m_doc);
    look->setValue(mover->id(), 0, 255);
    look->setValue(mover->id(), 1, 128);
    look->setValue(mover->id(), 2, 40);
    look->setValue(mover->id(), 3, 100);
    look->setValue(mover->id(), 4, 7);
    look->setValue(mover->id(), 5, 90);
    look->setFadeOutSpeed(2000);        // what it lets go of fades: held all the same
    m_doc->addFunction(look);
    goList(listOf(look));
    auto drain = qScopeGuard([this]() {
        for (int i = 0; i < 120; i++)
            tickFrame(Dim);
    });
    for (int i = 0; i < 4; i++)
        tickFrame(Dim);
    QCOMPARE(sent(Dim), 255);
    QCOMPARE(sent(Pan), 128);
    QCOMPARE(sent(PanFine), 40);
    QCOMPARE(sent(Tilt), 100);
    QCOMPARE(sent(TiltFine), 7);
    QCOMPARE(sent(Colour), 90);

    m_stub->m_universe.fill(char(7));   // the stop's own send fills it in
    stop();
    QVERIFY(m_doc->outputRuleKeeper()->emergencyStopped());
    // The stop's own send, then every frame after it: the head never moves.
    for (int i = 0; i < 7; i++)
    {
        if (i > 0)
            tickFrame(Dim);
        const QString at = QStringLiteral("frame %1 after the stop").arg(i);
        QVERIFY2(sent(Pan) == 128, qPrintable(at + QStringLiteral(": pan %1").arg(sent(Pan))));
        QVERIFY2(sent(PanFine) == 40, qPrintable(at + QStringLiteral(": pan fine %1").arg(sent(PanFine))));
        QVERIFY2(sent(Tilt) == 100, qPrintable(at + QStringLiteral(": tilt %1").arg(sent(Tilt))));
        QVERIFY2(sent(TiltFine) == 7, qPrintable(at + QStringLiteral(": tilt fine %1").arg(sent(TiltFine))));
        // The rest at its safe value, at once.
        QVERIFY2(sent(Dim) == 0, qPrintable(at + QStringLiteral(": dimmer %1").arg(sent(Dim))));
        QVERIFY2(sent(Colour) == 0, qPrintable(at + QStringLiteral(": colour %1").arg(sent(Colour))));
    }
    QVERIFY(m_uni->showOnlyHoldingPosition(Pan));
    QVERIFY(m_uni->showOnlyHoldingPosition(TiltFine));
    QVERIFY(m_uni->showOnlyHoldingPosition(Dim) == false);
    QVERIFY(m_uni->showOnlyHoldingPosition(Colour) == false);
}

void OutputRules_Test::showOnlyMoverHoldsPositionOnStop()
{
    // The emergency stop itself (the pad's, OutputRuleKeeper's).
    checkMoverHoldsThroughStop([this]() { m_doc->outputRuleKeeper()->emergencyStop(); });
}

void OutputRules_Test::showOnlyMoverHoldsOnScreenStopAll()
{
    // The top bar's Stop All (App::slotControlPanic()).
    checkMoverHoldsThroughStop([this]() { QVERIFY(m_doc->showControl()->allOff(ShowControl::RouteKey)); });
}

void OutputRules_Test::showOnlyMoverHoldsOnControlMapStop()
{
    // A desk button bound as the Control Map's emergency stop, pressed.
    ControlMap *cm = m_doc->controlMap();
    QVERIFY(cm != nullptr);
    cm->setBinding({ 0, 40, ControlMap::emergencyStop(), ControlMap::StartOnly });
    auto unbind = qScopeGuard([cm]() { cm->reset(); });
    checkMoverHoldsThroughStop([cm]() {
        cm->handleInput(0, 40, 0);
        cm->handleInput(0, 40, 127);
    });
}

void OutputRules_Test::showOnlyMoverHoldsOnPhoneAllOff()
{
    // The phone's ALL OFF (WebAccessRun: ShowControl::allOff(RouteWeb)).
    checkMoverHoldsThroughStop([this]() { QVERIFY(m_doc->showControl()->allOff(ShowControl::RouteWeb)); });
}

void OutputRules_Test::showOnlyMoverIsDrivenAgainAfterTheStop()
{
    enum { Dim = 50, Pan, PanFine, Tilt };
    checkMoverHoldsThroughStop([this]() { QVERIFY(m_doc->showControl()->allOff(ShowControl::RoutePad)); });
    if (QTest::currentTestFailed())
        return;
    // A hand on the pan fader after the stop (E3): the head follows it.
    put(Pan, 200);
    tickFrame(Pan);
    QCOMPARE(sent(Pan), 200);
    QCOMPARE(sent(Tilt), 100);          // the rest of the head still held
    QVERIFY(m_uni->showOnlyHoldingPosition(Pan) == false);
    QVERIFY(m_uni->showOnlyHoldingPosition(Tilt));
    // The next GO drives tilt as any cue does.
    const quint32 fid = m_doc->fixtureForAddress(Dim);
    Scene *next = new Scene(m_doc);
    next->setValue(fid, 3, 30);
    m_doc->addFunction(next);
    goList(listOf(next));
    for (int i = 0; i < 4; i++)
        tickFrame(Tilt);
    QCOMPARE(sent(Tilt), 30);
    QCOMPARE(sent(Pan), 200);
    QCOMPARE(sent(PanFine), 40);        // never moved: still held
}
