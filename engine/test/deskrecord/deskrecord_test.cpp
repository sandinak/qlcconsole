/*
  Q Light Controller Plus - Unit test
  deskrecord_test.cpp

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
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QSettings>
#include <QtTest>

#include "../common/stdprelude.h"
#define private public
#include "mastertimer.h"
#undef private

#include "deskrecord_test.h"
#include "controllerkeepalive.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "outputrulekeeper.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "ioplugincache.h"
#include "iopluginstub.h"
#include "qlcinputprofile.h"
#include "qlcinputchannel.h"
#include "fixturegroup.h"
#include "showcontrol.h"
#include "showoperate.h"
#include "showreview.h"
#include "deskrecord.h"
#include "controlmap.h"
#include "qlcpalette.h"
#include "inputpatch.h"
#include "qlcfile.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

using D = DeskRecord;

namespace
{
const quint32 kStop = 200;      // a note: the STOP button
const quint32 kGo = 202;
const quint32 kBack = 203;
const quint32 kGm = 7;          // a CC: the GM fader
}

void DeskRecord_Test::init()
{
    QSettings().remove(DeskRecord::settingsGroup());
    m_doc = new Doc(this);
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
    m_stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    // A MIDI box, as the MIDI plugin names it, and a second device.
    m_stub->m_nameOverride = "MIDI";
    m_stub->m_lineNames = QStringList() << "APC MINI" << "nanoKONTROL2";
    m_stub->m_lineLocations.clear();
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, m_stub->name(), QString(), 0));
    m_doc->deskRecord()->rebind();

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Dim 1");
    fxi->setAddress(0);
    fxi->setUniverse(0);
    fxi->setChannels(1);
    m_doc->addFixture(fxi);
}

void DeskRecord_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    tick(3);
    if (m_stub != nullptr)
    {
        m_stub->m_nameOverride.clear();
        m_stub->m_lineNames.clear();
        m_stub->m_lineLocations.clear();
    }
    delete m_doc;
    m_doc = nullptr;
    m_stub = nullptr;
    QSettings().remove(DeskRecord::settingsGroup());
}

void DeskRecord_Test::send(quint32 universe, quint32 channel, uchar value)
{
    emit m_doc->inputOutputMap()->inputValueChanged(universe, channel, value);
    QCoreApplication::processEvents();
}

void DeskRecord_Test::tick(int n)
{
    for (int i = 0; i < n; ++i)
        m_doc->masterTimer()->timerTick();
    QCoreApplication::processEvents();
}

void DeskRecord_Test::learn(int role, quint32 universe, quint32 channel)
{
    D *desk = m_doc->deskRecord();
    QVERIFY(desk->learn(D::Role(role)));
    if (D::takesFader(D::Role(role)))
    {
        send(universe, channel, 40);
        send(universe, channel, 60);
    }
    else if (role == D::Stop)
    {
        send(universe, channel, 127);
        send(universe, channel, 0);
        send(universe, channel, 127);
        send(universe, channel, 0);
    }
    else
    {
        send(universe, channel, 127);
        send(universe, channel, 0);
    }
    QVERIFY(!desk->isLearning());
    QVERIFY(desk->isSet(D::Role(role)));
}

Scene *DeskRecord_Test::scene(const QString &name)
{
    Scene *s = new Scene(m_doc);
    s->setName(name);
    const quint32 fid = m_doc->fixtures().at(0)->id();
    s->addFixture(fid);
    s->setValue(fid, 0, 255);
    m_doc->addFunction(s);
    return s;
}

quint32 DeskRecord_Test::twoLists(quint32 *act2)
{
    ShowTreeController *st = m_doc->showTree();
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("All");
    m_doc->addFixtureGroup(g);
    g->assignFixture(m_doc->fixtures().at(0)->id());
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer, m_doc);
    full->setName("Full");
    full->setValue(255);
    m_doc->addPalette(full);
    const quint32 prod = st->addProduction("Show");
    quint32 ids[2];
    for (int l = 0; l < 2; ++l)
    {
        ids[l] = st->addCueList(prod, QString("Act %1").arg(l + 1));
        for (int i = 0; i < 2; ++i)
        {
            ShowTree::Target t;
            t.groups << g->id();
            const quint32 c = st->addCue(ids[l], i - 1, QString("Cue %1").arg(i + 1));
            st->setLayerLook(c, st->addLayer(c, t), st->lookForPalette(full->id()));
        }
    }
    *act2 = ids[1];
    return ids[0];
}

/*****************************************************************************
 * The STOP
 *****************************************************************************/

void DeskRecord_Test::stopFiresInEmptyShow()
{
    D *desk = m_doc->deskRecord();
    ControlMap *cm = m_doc->controlMap();
    learn(D::Stop, 0, kStop);
    // No binding in the show: the Control Map hears nothing, the desk does.
    QVERIFY(cm->isEmpty());
    QVERIFY(!cm->isInputConnected());
    QVERIFY(desk->isInputConnected());
    QVERIFY(desk->isLive(D::Stop));

    Scene *s = scene("Warm");
    s->start(m_doc->masterTimer(), FunctionParent::master());
    tick();
    QVERIFY(s->isRunning());
    QSignalSpy fired(desk, &D::fired);
    QSignalSpy struck(m_doc->outputRuleKeeper(), &OutputRuleKeeper::emergencyStopStruck);
    send(0, kStop, 127);
    tick(3);
    QCOMPARE(fired.count(), 1);
    QCOMPARE(fired.first().first().toString(), QString("ALL OFF"));
    QCOMPARE(struck.count(), 1);
    QVERIFY(!s->isRunning());
    // A held press that resends is the one press; the next press fires again.
    send(0, kStop, 127);
    QCOMPARE(fired.count(), 1);
    send(0, kStop, 0);
    send(0, kStop, 127);
    QCOMPARE(fired.count(), 2);

    // File > New: the show is gone, the desk's STOP is not.
    m_doc->clearContents();
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, m_stub->name(), QString(), 0));
    send(0, kStop, 0);
    send(0, kStop, 127);
    QCOMPARE(fired.count(), 3);
}

void DeskRecord_Test::stopMatrix_data()
{
    QTest::addColumn<QString>("when");
    QTest::newRow("Design") << "design";
    QTest::newRow("Operate") << "operate";
    QTest::newRow("a trigger Learn open") << "learn";
    QTest::newRow("the stop's Learn open") << "stoplearn";
    QTest::newRow("this desk learning GO") << "desklearn";
    QTest::newRow("locked by the show's PIN") << "pin";
    QTest::newRow("during a load") << "load";
    QTest::newRow("after a repatch") << "repatch";
}

void DeskRecord_Test::stopMatrix()
{
    QFETCH(QString, when);
    D *desk = m_doc->deskRecord();
    ControlMap *cm = m_doc->controlMap();
    learn(D::Stop, 0, kStop);
    QSignalSpy fired(desk, &D::fired);
    auto press = [this]() {
        send(0, kStop, 0);
        send(0, kStop, 127);
    };

    if (when == "design")
        m_doc->setMode(Doc::Design);
    else if (when == "operate")
        m_doc->setMode(Doc::Operate);
    else if (when == "learn")
        cm->learn(ControlMap::function(scene("A")->id()));
    else if (when == "stoplearn")
        cm->learn(ControlMap::emergencyStop());
    else if (when == "desklearn")
        QVERIFY(desk->learn(D::Go));
    else if (when == "pin")
    {
        QVERIFY(m_doc->operateSettings()->setPin("1234"));
        QVERIFY(desk->isLocked());
    }
    else if (when == "repatch")
    {
        QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, KInputNone, QString(), QLCIOPlugin::invalidLine()));
        QVERIFY(m_doc->inputOutputMap()->setInputPatch(3, m_stub->name(), QString(), 0));
        QCOMPARE(desk->universeOf(D::Stop), 3u);
        send(3, kStop, 0);
        send(3, kStop, 127);
        QCOMPARE(fired.count(), 1);
        return;
    }
    else if (when == "load")
    {
        // A show with enough fixtures that the load reports progress.
        for (int i = 1; i < 24; ++i)
        {
            Fixture *f = new Fixture(m_doc);
            f->setName(QString("Dim %1").arg(i + 1));
            f->setAddress(quint32(i));
            f->setUniverse(0);
            f->setChannels(1);
            m_doc->addFixture(f);
        }
        QString xml;
        {
            QXmlStreamWriter w(&xml);
            w.writeStartDocument();
            QVERIFY(m_doc->saveXML(&w));
            w.writeEndDocument();
        }
        m_doc->clearContents();
        QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, m_stub->name(), QString(), 0));
        int during = 0;
        connect(m_doc, &Doc::loadProgress, this, [&]() {
            if (m_doc->loadStatus() != Doc::Loading || during > 0)
                return;
            ++during;
            press();
        });
        QXmlStreamReader r(xml);
        r.readNextStartElement();      // <Engine>
        QVERIFY(m_doc->loadXML(r, false));
        QCOMPARE(during, 1);
        QCOMPARE(fired.count(), 1);
        return;
    }
    press();
    QCOMPARE(fired.count(), 1);
    QCOMPARE(fired.first().first().toString(), QString("ALL OFF"));
    if (when == "stoplearn" || when == "learn")
        QVERIFY(cm->isListening());     // the press was not the learn's
    if (when == "desklearn")
        QVERIFY(desk->isLearning());
}

void DeskRecord_Test::neverWrittenToShow()
{
    auto save = [this]() {
        QString xml;
        QXmlStreamWriter w(&xml);
        w.writeStartDocument();
        m_doc->saveXML(&w);
        w.writeEndDocument();
        return xml;
    };
    scene("Warm");
    const QString before = save();
    learn(D::Stop, 0, kStop);
    learn(D::Go, 0, kGo);
    learn(D::GrandMaster, 0, kGm);
    m_doc->deskRecord()->setOn(D::Back, false);
    const QString after = save();
    QCOMPARE(after, before);
    QVERIFY(!after.contains("midi:"));      // no identity (the patch names its own device)
    QVERIFY(!after.contains("thisComputersDesk", Qt::CaseInsensitive));
    // Kept with this computer: another show (a new Doc) has it.
    Doc other(nullptr);
    QVERIFY(other.deskRecord()->isSet(D::Stop));
    QCOMPARE(other.deskRecord()->binding(D::Go).channel, kGo);
}

void DeskRecord_Test::rebindsByIdentityOnRepatch()
{
    D *desk = m_doc->deskRecord();
    InputOutputMap *iom = m_doc->inputOutputMap();
    learn(D::Go, 0, kGo);
    QCOMPARE(desk->universeOf(D::Go), 0u);
    QVERIFY(iom->isStaticControl(0, kGo));
    QSignalSpy changed(desk, &D::changed);

    // The APC goes to universe 3 and another box takes universe 1: the GO follows the APC.
    QVERIFY(iom->setInputPatch(0, KInputNone, QString(), QLCIOPlugin::invalidLine()));
    QVERIFY(iom->setInputPatch(3, m_stub->name(), QString(), 0));
    QVERIFY(iom->setInputPatch(1, m_stub->name(), QString(), 1));
    QCOMPARE(desk->universeOf(D::Go), 3u);
    QVERIFY(changed.count() > 0);
    QVERIFY(iom->isStaticControl(3, kGo));
    QVERIFY(!iom->isStaticControl(0, kGo));
    QVERIFY(!iom->isStaticControl(1, kGo));       // the nanoKONTROL's same note is the show's
    QVERIFY(desk->isDeskControl(3, kGo));
    QVERIFY(!desk->isDeskControl(1, kGo));
    // Unpatched: not live, and the device is here (Patch It can).
    QVERIFY(iom->setInputPatch(3, KInputNone, QString(), QLCIOPlugin::invalidLine()));
    QCOMPARE(desk->state(D::Go), D::NotPatched);
    QVERIFY(iom->staticControls().isEmpty());
    // Unplugged: not connected.
    m_stub->m_lineNames = QStringList() << "nanoKONTROL2";
    desk->rebind();
    QCOMPARE(desk->state(D::Go), D::NotConnected);
}

void DeskRecord_Test::twoSameNameDevicesWarn()
{
    D *desk = m_doc->deskRecord();
    // macOS: two APC minis read the same name; nothing tells them apart.
    m_stub->m_lineNames = QStringList() << "APC MINI" << "APC MINI";
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(1, m_stub->name(), QString(), 1));
    desk->rebind();
    learn(D::Go, 0, kGo);
    QVERIFY(desk->duplicateWarning().contains("Two devices are called APC MINI"));
    QVERIFY(desk->reviewNeedsAttention());
    // The first patched one has it, never both.
    QCOMPARE(desk->universeOf(D::Go), 0u);
    QVERIFY(!m_doc->inputOutputMap()->isStaticControl(1, kGo));
    // Windows names the second "2- APC MINI": told apart, no warning.
    m_stub->m_lineNames = QStringList() << "APC MINI" << "2- APC MINI";
    desk->rebind();
    QVERIFY(desk->duplicateWarning().isEmpty());
}

/*****************************************************************************
 * GO and Back
 *****************************************************************************/

void DeskRecord_Test::goTargetsRunTabListAndNamesIt()
{
    D *desk = m_doc->deskRecord();
    ControlMap *cm = m_doc->controlMap();
    quint32 act2 = 0;
    const quint32 act1 = twoLists(&act2);
    learn(D::Go, 0, kGo);
    learn(D::Back, 0, kBack);
    QSignalSpy target(desk, &D::goTargetChanged);
    cm->setRunTabList(act2);
    QCOMPARE(desk->goTargetText(), QString::fromUtf8("GO → Act 2"));
    QCOMPARE(target.count(), 1);
    QCOMPARE(target.first().first().toString(), desk->goTargetText());

    ShowControl *sc = m_doc->showControl();
    sc->setGoGuardMs(ShowControl::maxGoGuardMs());
    auto unhook = qScopeGuard([sc]() { sc->setGoGuardMs(0); });
    QSignalSpy fired(desk, &D::fired);
    QSignalSpy held(sc, &ShowControl::commandHeld);
    send(0, kGo, 127);
    QCOMPARE(fired.count(), 1);
    QCOMPARE(fired.first().first().toString(), QString("GO Act 2"));
    // One press: held down and resending is not a second GO.
    send(0, kGo, 127);
    QCOMPARE(fired.count(), 1);
    // The double-GO guard holds a second press inside its window.
    send(0, kGo, 0);
    send(0, kGo, 127);
    QCOMPARE(fired.count(), 1);
    QCOMPARE(held.count(), 1);
    send(0, kGo, 0);
    // The list changes: GO follows the Run tab.
    sc->setGoGuardMs(0);
    cm->setRunTabList(act1);
    QCOMPARE(desk->goTargetText(), QString::fromUtf8("GO → Act 1"));
    send(0, kGo, 127);
    QCOMPARE(fired.last().first().toString(), QString("GO Act 1"));
    send(0, kGo, 0);
    tick(3);                    // the list runs (Back needs a running list)
    send(0, kGo, 127);          // cue 2: there is a cue to go back to
    send(0, kGo, 0);
    tick(3);
    send(0, kBack, 127);
    QCOMPARE(fired.last().first().toString(), QString("Back: Act 1"));
}

void DeskRecord_Test::learnRefusesWrongControlKind()
{
    D *desk = m_doc->deskRecord();
    QSignalSpy refused(desk, &D::learnRefused);
    // GO on a fader: values move with no 0 between.
    QVERIFY(desk->learn(D::Go));
    send(0, kGm, 30);
    send(0, kGm, 31);
    send(0, kGm, 33);
    QVERIFY(refused.count() >= 1);
    QVERIFY(refused.last().first().toString().contains("fader or knob"));
    QVERIFY(!desk->isSet(D::Go));
    desk->cancelLearn();

    // The Grand Master on a button: one value, then 0.
    refused.clear();
    QVERIFY(desk->learn(D::GrandMaster));
    send(0, kStop, 127);
    send(0, kStop, 0);
    QCOMPARE(refused.count(), 1);
    QVERIFY(refused.first().first().toString().contains("The Grand Master takes a fader"));
    QVERIFY(!desk->isSet(D::GrandMaster));
    // Then the fader: learned.
    send(0, kGm, 10);
    send(0, kGm, 20);
    QVERIFY(desk->isSet(D::GrandMaster));
    QCOMPARE(desk->binding(D::GrandMaster).channel, kGm);

    // STOP needs press, release, press: one press is not enough.
    QVERIFY(desk->learn(D::Stop));
    QSignalSpy confirming(desk, &D::learnConfirming);
    send(0, kStop, 127);
    send(0, kStop, 0);
    QCOMPARE(confirming.count(), 1);
    QVERIFY(!desk->isSet(D::Stop));
    send(0, kStop, 127);
    QVERIFY(desk->isSet(D::Stop));
    // The press that taught it is not a STOP.
    QSignalSpy fired(desk, &D::fired);
    send(0, kStop, 127);
    QCOMPARE(fired.count(), 0);
    send(0, kStop, 0);

    // A profile that says Slider: GO refused at once; one that says Button: GM refused.
    QLCInputProfile *profile = new QLCInputProfile();
    profile->setManufacturer("Desk");
    profile->setModel("test");
    QLCInputChannel *fader = new QLCInputChannel();
    fader->setType(QLCInputChannel::Slider);
    profile->insertChannel(50, fader);
    QLCInputChannel *button = new QLCInputChannel();
    button->setType(QLCInputChannel::Button);
    profile->insertChannel(51, button);
    m_doc->inputOutputMap()->addProfile(profile);
    QVERIFY(m_doc->inputOutputMap()->setInputProfile(0, "Desk test"));
    refused.clear();
    QVERIFY(desk->learn(D::Back));
    send(0, 50, 127);
    QCOMPARE(refused.count(), 1);
    desk->cancelLearn();
    refused.clear();
    QVERIFY(desk->learn(D::GrandMaster));
    send(0, 51, 127);
    QCOMPARE(refused.count(), 1);
    send(0, 50, 64);
    QCOMPARE(desk->binding(D::GrandMaster).channel, 50u);

    // Another role's control is refused, naming it.
    refused.clear();
    QVERIFY(desk->learn(D::Back));
    send(0, kStop, 0);
    send(0, kStop, 127);       // STOP still stops the show ...
    QCOMPARE(fired.count(), 1);
    QVERIFY(desk->isLearning()); // ... and is no Back
    QVERIFY(!desk->isSet(D::Back));
}

/*****************************************************************************
 * The show's own controls (Q309, Q314, Q315)
 *****************************************************************************/

void DeskRecord_Test::bothStopsLive()
{
    D *desk = m_doc->deskRecord();
    ControlMap *cm = m_doc->controlMap();
    learn(D::Stop, 0, kStop);
    // The show's own stop on another control: both are live (Q315).
    cm->setBinding({ 0, kStop + 1, ControlMap::emergencyStop(), ControlMap::Toggle });
    QSignalSpy deskFired(desk, &D::fired);
    QSignalSpy showFired(cm, &ControlMap::fired);
    send(0, kStop + 1, 127);
    QCOMPARE(showFired.count(), 1);
    QCOMPARE(deskFired.count(), 0);
    send(0, kStop, 127);
    QCOMPARE(deskFired.count(), 1);
    QCOMPARE(showFired.count(), 1);
    QVERIFY(cm->deskBlockReason(*cm->binding(ControlMap::emergencyStop())).isEmpty());

    // The show's stop on the desk's STOP: one ALL OFF (the desk's), not two.
    cm->setBinding({ 0, kStop, ControlMap::emergencyStop(), ControlMap::Toggle });
    send(0, kStop, 0);
    send(0, kStop, 127);
    QCOMPARE(deskFired.count(), 2);
    QCOMPARE(showFired.count(), 1);

    // The show's stop on a control the desk uses for GO: blocked (Q315).
    learn(D::Go, 0, kGo);
    cm->setBinding({ 0, kGo, ControlMap::emergencyStop(), ControlMap::Toggle });
    QVERIFY(cm->deskBlockReason(*cm->binding(ControlMap::emergencyStop())).contains("this computer's GO"));
    send(0, kGo, 127);
    QCOMPARE(showFired.count(), 1);
}

void DeskRecord_Test::sameControlDeskWins()
{
    D *desk = m_doc->deskRecord();
    ControlMap *cm = m_doc->controlMap();
    Scene *s = scene("Warm");
    // This computer's GO, then a show with a trigger on that control.
    learn(D::Go, 0, kGo);
    cm->setBinding({ 0, kGo, ControlMap::function(s->id()), ControlMap::Toggle });
    // Kept in the show, not armed here, listed as blocked with the reason.
    QVERIFY(cm->binding(ControlMap::function(s->id())) != nullptr);
    QVERIFY(!cm->isArmedOnControl(*cm->binding(ControlMap::function(s->id()))));
    const QList<QPair<ControlMap::Binding, QString>> blocked = cm->deskBlocked();
    QCOMPARE(blocked.count(), 1);
    QCOMPARE(blocked.first().first.target, ControlMap::function(s->id()));
    QVERIFY(blocked.first().second.contains("blocked on this computer"));
    QVERIFY(blocked.first().second.contains("GO"));
    send(0, kGo, 127);
    tick();
    QVERIFY(!s->isRunning());
    send(0, kGo, 0);
    // A Learn on the desk's control is told whose it is.
    QSignalSpy refused(cm, &ControlMap::refused);
    cm->learn(ControlMap::function(s->id()));
    send(0, kGo, 127);
    QCOMPARE(refused.count(), 1);
    QVERIFY(refused.first().first().toString().contains("this computer's GO"));
    cm->stopListening();
    send(0, kGo, 0);
    // The desk's GO turned off on this computer: the trigger is armed again.
    QVERIFY(desk->setOn(D::Go, false));
    QVERIFY(cm->deskBlocked().isEmpty());
    send(0, kGo, 127);
    tick();
    QVERIFY(s->isRunning());
}

void DeskRecord_Test::showFallbackOnlyForARoleThisComputerLacks()
{
    D *desk = m_doc->deskRecord();
    ControlMap *cm = m_doc->controlMap();
    InputOutputMap *iom = m_doc->inputOutputMap();
    quint32 act2 = 0;
    const quint32 act1 = twoLists(&act2);
    cm->setRunTabList(act1);
    // The show's own GO of the Run tab's list, on the other box.
    QVERIFY(iom->setInputPatch(1, m_stub->name(), QString(), 1));
    cm->setBinding({ 1, 10, ControlMap::cueList(act1), ControlMap::Toggle });
    const ControlMap::Binding showGo = *cm->binding(ControlMap::cueList(act1));
    QVERIFY(cm->deskBlockReason(showGo).isEmpty());
    QVERIFY(!iom->deskGrandMaster());
    learn(D::Go, 0, kGo);
    // This computer has a GO: the show's is the fallback, not armed.
    QVERIFY(cm->deskBlockReason(showGo).contains("fallback"));
    QVERIFY(!cm->isArmedOnControl(showGo));
    // Another list's GO is not the Run tab's: still the show's.
    cm->setBinding({ 1, 11, ControlMap::cueList(act2), ControlMap::Toggle });
    QVERIFY(cm->deskBlockReason(*cm->binding(ControlMap::cueList(act2))).isEmpty());
    // The desk's GO off: the show's GO is armed again.
    QVERIFY(desk->setOn(D::Go, false));
    QVERIFY(cm->deskBlockReason(showGo).isEmpty());
    // The Grand Master: the show's input is the fallback while this has none.
    learn(D::GrandMaster, 0, kGm);
    QVERIFY(iom->deskGrandMaster());
    QVERIFY(desk->setOn(D::GrandMaster, false));
    QVERIFY(!iom->deskGrandMaster());
}

void DeskRecord_Test::perComputerTickTurnsOneRoleOff()
{
    D *desk = m_doc->deskRecord();
    InputOutputMap *iom = m_doc->inputOutputMap();
    learn(D::Go, 0, kGo);
    learn(D::Stop, 0, kStop);
    QVERIFY(desk->setOn(D::Go, false));
    QCOMPARE(desk->state(D::Go), D::Off);
    QVERIFY(!desk->isDeskControl(0, kGo));
    QVERIFY(!iom->isStaticControl(0, kGo));
    QVERIFY(iom->isStaticControl(0, kStop));       // only the one role
    QSignalSpy fired(desk, &D::fired);
    send(0, kGo, 127);
    QCOMPARE(fired.count(), 0);
    // Kept with this computer.
    Doc other(nullptr);
    QVERIFY(!other.deskRecord()->binding(D::Go).on);
    QVERIFY(other.deskRecord()->binding(D::Stop).on);
    QVERIFY(desk->setOn(D::Go, true));
    QVERIFY(iom->isStaticControl(0, kGo));
}

/*****************************************************************************
 * The record
 *****************************************************************************/

void DeskRecord_Test::teacherLockFreezesRecord()
{
    // Q319: the show's PIN guards desk changes.
    D *desk = m_doc->deskRecord();
    learn(D::Go, 0, kGo);
    QVERIFY(m_doc->operateSettings()->setPin("1234"));
    QVERIFY(desk->isLocked());
    QSignalSpy refused(desk, &D::refused);
    QVERIFY(!desk->learn(D::Stop));
    QVERIFY(!desk->setOn(D::Go, false));
    QVERIFY(!desk->clear(D::Go));
    QTemporaryDir dir;
    const QString file = dir.filePath("desk.qlcdesk");
    QVERIFY(desk->exportTo(file));          // reading it out is no change
    QString error;
    QVERIFY(!desk->importFrom(file, &error));
    QVERIFY(error.contains("PIN"));
    QVERIFY(refused.count() >= 4);
    QVERIFY(desk->isSet(D::Go));
    QVERIFY(desk->binding(D::Go).on);
    // The wrong PIN keeps it locked; the right one opens it.
    QVERIFY(!desk->unlock("0000"));
    QVERIFY(desk->isLocked());
    QVERIFY(desk->unlock("1234"));
    QVERIFY(desk->setOn(D::Go, false));
    // Another show (File > New) never changes the record.
    m_doc->clearContents();
    QVERIFY(desk->isSet(D::Go));
    QVERIFY(!desk->binding(D::Go).on);
}

void DeskRecord_Test::exportImportRoundTrip()
{
    D *desk = m_doc->deskRecord();
    learn(D::Stop, 0, kStop);
    learn(D::GrandMaster, 0, kGm);
    learn(D::Go, 0, kGo);
    QVERIFY(desk->setOn(D::Go, false));
    D::Binding was[D::RoleCount];
    for (D::Role r : D::roles())
        was[r] = desk->binding(r);
    QTemporaryDir dir;
    const QString file = dir.filePath("desk.qlcdesk");
    QString error;
    QVERIFY2(desk->exportTo(file, &error), qPrintable(error));
    QFile f(file);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(f.size() < 4096);           // a small file
    QVERIFY(f.readAll().contains("midi:APC MINI"));
    f.close();
    // Another computer: nothing set, then the file.
    QSettings().remove(DeskRecord::settingsGroup());
    for (D::Role r : D::roles())
        desk->setBinding(r, D::Binding());
    QVERIFY(!desk->anySet());
    QVERIFY2(desk->importFrom(file, &error), qPrintable(error));
    for (D::Role r : D::roles())
        QVERIFY(desk->binding(r) == was[r]);
    QVERIFY(desk->isLive(D::Stop));
    QCOMPARE(desk->state(D::Go), D::Off);
    QVERIFY(!desk->isSet(D::Back));
    // Kept with this computer, too.
    Doc other(nullptr);
    QVERIFY(other.deskRecord()->binding(D::Stop) == was[D::Stop]);
    // Not a desk file: refused, nothing changed.
    const QString bad = dir.filePath("bad.qlcdesk");
    QFile b(bad);
    QVERIFY(b.open(QIODevice::WriteOnly));
    b.write("<Workspace/>");
    b.close();
    QVERIFY(!desk->importFrom(bad, &error));
    QVERIFY(desk->isSet(D::Stop));
}

void DeskRecord_Test::patchItIsNamedAndUndoable()
{
    D *desk = m_doc->deskRecord();
    InputOutputMap *iom = m_doc->inputOutputMap();
    learn(D::Go, 0, kGo);
    // A show that patches something else on universe 1 and not the APC.
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 1));
    QCOMPARE(desk->state(D::Go), D::NotPatched);
    QCOMPARE(desk->missingDevices(), QStringList() << "APC MINI");
    QString what;
    const quint32 u = desk->patchIt(&what);
    QCOMPARE(u, 1u);                    // the first universe with a free input
    QCOMPARE(what, QString("Patched APC MINI to Universe 2's input."));
    QVERIFY(desk->isLive(D::Go));
    QCOMPARE(desk->universeOf(D::Go), 1u);
    QVERIFY(m_doc->isModified());
    QVERIFY(desk->canUndoPatch());
    QVERIFY(desk->undoPatchIt(&what));
    QVERIFY(what.contains("APC MINI"));
    QCOMPARE(desk->state(D::Go), D::NotPatched);
    QVERIFY(iom->inputPatch(1) == nullptr);
    QVERIFY(iom->inputPatch(0) != nullptr);   // the show's own patch untouched
    QVERIFY(!desk->canUndoPatch());
}

void DeskRecord_Test::grandMasterPicksUpAfterLoadReplugWake()
{
    D *desk = m_doc->deskRecord();
    InputOutputMap *iom = m_doc->inputOutputMap();
    qint64 now = 10000;
    desk->setClock([&now]() { return now; });
    learn(D::GrandMaster, 0, kGm);
    iom->setGrandMasterValue(255);
    // Followed at once until a load, a replug or a wake.
    desk->rearmGrandMaster();
    now += 1000;
    send(0, kGm, 255);
    QCOMPARE(int(iom->grandMasterValue()), 255);
    send(0, kGm, 200);
    QCOMPARE(int(iom->grandMasterValue()), 200);
    iom->setGrandMasterValue(255);

    auto check = [&](const char *what) {
        QSignalSpy waiting(desk, &D::grandMasterWaitingChanged);
        // A dump in the first 500 ms: nothing, even a 0.
        now += 100;
        send(0, kGm, 0);
        QVERIFY2(iom->grandMasterValue() == 255, what);
        // A parked fader brushed: waits, and says how to pick up.
        now += 600;
        send(0, kGm, 3);
        send(0, kGm, 30);
        QVERIFY2(iom->grandMasterValue() == 255, what);
        QVERIFY2(desk->grandMasterWaiting(), what);
        QVERIFY2(desk->grandMasterHint().contains("100 %"), what);
        QVERIFY2(waiting.count() >= 1, what);
        // Up to the master: picked up, then it follows.
        send(0, kGm, 253);
        send(0, kGm, 180);
        QVERIFY2(iom->grandMasterValue() == 180, what);
        QVERIFY2(!desk->grandMasterWaiting(), what);
        iom->setGrandMasterValue(255);
    };
    // A load.
    emit m_doc->loaded();
    check("load");
    // A replug.
    emit iom->inputDeviceConnected(0, m_stub->name(), "APC MINI");
    check("replug");
    // A wake.
    ControllerKeepAlive *ka = m_doc->programmer()->controllerKeepAlive();
    QVERIFY(ka != nullptr);
    ka->setSystemState(ControllerKeepAlive::Asleep);
    ka->setSystemState(ControllerKeepAlive::Waking);
    check("wake");
    // A pull to 0 after the dump window applies at once, waiting or not.
    emit m_doc->loaded();
    now += 600;
    send(0, kGm, 0);
    QCOMPARE(int(iom->grandMasterValue()), 0);
}

void DeskRecord_Test::reviewLinesSayWhatIsReachable()
{
    D *desk = m_doc->deskRecord();
    learn(D::Stop, 0, kStop);
    learn(D::Go, 0, kGo);
    QStringList lines = ShowReview::thisComputer(m_doc);
    QCOMPARE(lines.count(), 5);         // four roles and what GO plays
    QVERIFY(lines.at(0).startsWith("STOP: APC MINI"));
    QVERIFY(lines.at(0).contains("reachable now"));
    QVERIFY(lines.at(1).startsWith("Grand Master: not set"));
    QVERIFY(lines.at(2).startsWith("GO: APC MINI"));
    QVERIFY(lines.at(4).startsWith("GO: the Run tab has no cue list"));
    QVERIFY(!desk->reviewNeedsAttention());
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, KInputNone, QString(), QLCIOPlugin::invalidLine()));
    lines = ShowReview::thisComputer(m_doc);
    QCOMPARE(lines.at(0), QString("STOP: APC MINI is not patched in this show"));
    QVERIFY(desk->reviewNeedsAttention());
}

QTEST_MAIN(DeskRecord_Test)
