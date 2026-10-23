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
#include <QtTest>

#define private public
#define protected public
#include "universe.h"
#include "iopluginstub.h"
#undef private
#undef protected

#include "outputrules_test.h"
#include "outputrulekeeper.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "grandmaster.h"
#include "qlcfile.h"
#include "parkeffect.h"
#include "fixture.h"
#include "doc.h"

#define TESTPLUGINDIR "../iopluginstub"

/* The circuits an Eos show patches as plain dimmers: a house light
   (Blackout + GM exempt), a stage dimmer (no rules), a power relay
   (Switch) and a work light (OnAtOpen). One channel each. */
enum { House = 10, Stage = 11, Relay = 12, Work = 13 };

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
    m_uni->dismissFader(m_fader);
    m_fader.clear();
    m_doc->inputOutputMap()->setBlackout(false);
    m_doc->inputOutputMap()->setOutputInhibited(false);
    m_doc->inputOutputMap()->setGrandMasterValue(255);
    m_doc->inputOutputMap()->setGrandMasterChannelMode(GrandMaster::Intensity);
    m_doc->outputRuleKeeper()->setOnAtOpenWhen(OutputRuleKeeper::kDefaultOnAtOpenWhen);
    m_doc->outputRuleKeeper()->setBlindHoldsExempt(OutputRuleKeeper::kDefaultBlindHoldsExempt);
    m_doc->setMode(Doc::Design);
    m_doc->clearContents();
    m_doc->outputRuleKeeper()->rebuild();
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    m_uni = ua.first();
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
    QCOMPARE(k->onAtOpenWhen(), OutputRuleKeeper::kDefaultOnAtOpenWhen);
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

QTEST_GUILESS_MAIN(OutputRules_Test)
