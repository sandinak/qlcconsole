/*
  Q Light Controller Plus - Unit test
  controlmap_test.cpp

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
#include <QEventLoop>
#include <QSignalSpy>
#include <QBuffer>
#include <QScopeGuard>
#include <QtTest>

#include "../common/stdprelude.h"
#define private public
#include "mastertimer.h"
#undef private
#include "controlmap_test.h"
#include "outputrulekeeper.h"
#include "qlcfile.h"
#include "showtreecontroller.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "iopluginstub.h"
#include "qlcioplugin.h"
#include "showcontrol.h"
#include "fixturegroup.h"
#include "controlmap.h"
#include "inputpatch.h"
#include "qlcinputprofile.h"
#include "qlcinputchannel.h"
#include "qlcpalette.h"
#include "universe.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

namespace
{
QByteArray saveShow(Doc *doc)
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.writeStartDocument();
    w.writeStartElement("Workspace");
    doc->saveXML(&w);
    w.writeEndElement();
    w.writeEndDocument();
    return xml;
}

bool loadShow(Doc *doc, const QByteArray &xml)
{
    QXmlStreamReader r(xml);
    r.readNextStartElement();       // Workspace
    r.readNextStartElement();       // Engine
    return doc->loadXML(r, false);
}

const quint32 kNote = 128 + 60;     // the MIDI plugin's note 60 on MIDI ch 1

/** The last light sent to @a channel; -1: none. */
int lastLight(const IOPluginStub *stub, quint32 channel)
{
    for (int i = stub->m_feedback.count() - 1; i >= 0; --i)
        if (stub->m_feedback.at(i).first == channel)
            return stub->m_feedback.at(i).second;
    return -1;
}
}

void ControlMap_Test::initTestCase()
{
}

void ControlMap_Test::init()
{
    m_doc = new Doc(this);
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
    m_stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    m_stub->m_feedback.clear();
    m_stub->m_feedbackParams.clear();

    // The controller: the stub as universe 1's input, and its feedback line.
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 0));
    QVERIFY(iom->setOutputPatch(0, m_stub->name(), QString(), 0, true));

    for (int i = 0; i < 4; ++i)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Dim %1").arg(i + 1));
        fxi->setAddress(quint32(i));
        fxi->setUniverse(0);
        fxi->setChannels(1);
        m_doc->addFixture(fxi);
    }
}

void ControlMap_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    tick(3);
    delete m_doc;
    m_doc = nullptr;
    m_stub = nullptr;
}

void ControlMap_Test::send(quint32 channel, uchar value)
{
    m_stub->emitValueChanged(UINT_MAX, 0, channel, value);
    m_doc->inputOutputMap()->inputPatch(0)->flush(0);
    QCoreApplication::processEvents();
}

void ControlMap_Test::tick(int n)
{
    for (int i = 0; i < n; ++i)
        m_doc->masterTimer()->timerTick();
    QCoreApplication::processEvents();     // started / stopped, queued
    /* The lights are worked out once per turn of the event loop, not once
       per event (one pass asks the Chaser mutexes the MasterTimer thread
       holds), so the pass the first round asked for lands in this one. */
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

Scene *ControlMap_Test::scene(const QString &name, int channel)
{
    Scene *s = new Scene(m_doc);
    s->setName(name);
    const quint32 fid = m_doc->fixtures().at(channel)->id();
    s->addFixture(fid);
    s->setValue(fid, 0, 255);
    m_doc->addFunction(s);
    return s;
}

quint32 ControlMap_Test::cueList(int cues, QList<quint32> *cueIds)
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
    const quint32 listId = st->addCueList(prod, "Act 1");
    for (int i = 0; i < cues; ++i)
    {
        ShowTree::Target t;
        t.groups << g->id();
        const quint32 c = st->addCue(listId, i - 1, QString("Cue %1").arg(i + 1));
        st->setLayerLook(c, st->addLayer(c, t), st->lookForPalette(full->id()));
        if (cueIds != nullptr)
            *cueIds << c;
    }
    return listId;
}

/* Learn: the next press (not a release) binds that control; learning
   doesn't fire, and the press that taught it isn't a press of the binding. */
void ControlMap_Test::learningBindsTheNextPress()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    QSignalSpy changed(cm, &ControlMap::bindingsChanged);
    QSignalSpy heard(cm, &ControlMap::heard);

    cm->learn(ControlMap::function(a->id()));
    QVERIFY(cm->isListening());
    send(kNote, 0);                                  // a release: not heard
    QVERIFY(cm->binding(ControlMap::function(a->id())) == nullptr);
    send(kNote, 127);
    QCOMPARE(heard.count(), 1);
    QVERIFY(!cm->isListening());
    const ControlMap::Binding *b = cm->binding(ControlMap::function(a->id()));
    QVERIFY(b != nullptr);
    QCOMPARE(b->universe, 0u);
    QCOMPARE(b->channel, kNote);
    QCOMPARE(b->mode, ControlMap::Toggle);
    QCOMPARE(changed.count(), 1);
    QVERIFY(m_doc->isModified());
    tick();
    QVERIFY(!a->isRunning());                        // learning didn't fire it
    send(kNote, 0);                                  // its release does nothing
    tick();
    QVERIFY(!a->isRunning());
    send(kNote, 127);                                // the next press does
    tick();
    QVERIFY(a->isRunning());

    // listen() only says what was pressed.
    cm->listen();
    send(kNote + 1, 100);
    QCOMPARE(heard.count(), 2);
    QCOMPARE(heard.last().at(1).toUInt(), kNote + 1);
    QCOMPARE(cm->count(), 1);
}

void ControlMap_Test::toggleStartsAndStops()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kNote, ControlMap::function(a->id()), ControlMap::Toggle });

    send(kNote, 127);
    tick();
    QVERIFY(a->isRunning());
    send(kNote, 0);                                  // release: nothing
    tick();
    QVERIFY(a->isRunning());
    send(kNote, 127);
    tick();
    QVERIFY(!a->isRunning());

    // A fader (a CC going on sending values) is one press, not many.
    send(kNote, 0);
    send(kNote, 20);
    send(kNote, 60);
    send(kNote, 90);
    tick();
    QVERIFY(a->isRunning());

    // Another control does nothing.
    send(kNote + 2, 127);
    tick();
    QVERIFY(a->isRunning());

    // Toggle stops it whatever started it.
    send(kNote, 0);
    a->stop(FunctionParent::master());
    tick();
    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick();
    send(kNote, 127);
    tick();
    QVERIFY(!a->isRunning());
}

void ControlMap_Test::momentaryIsOnWhileHeld()
{
    Scene *a = scene("Flash", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kNote, ControlMap::function(a->id()), ControlMap::Momentary });

    send(kNote, 127);
    tick();
    QVERIFY(a->isRunning());
    send(kNote, 0);
    tick();
    QVERIFY(!a->isRunning());

    // Running already (started elsewhere): the release leaves it running.
    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick();
    send(kNote, 127);
    send(kNote, 0);
    tick();
    QVERIFY(a->isRunning());
}

void ControlMap_Test::startOnlyAndStopOnly()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kNote, ControlMap::function(a->id()), ControlMap::StartOnly });
    cm->setBinding({ 0, kNote + 1, ControlMap::function(a->id()), ControlMap::StopOnly });
    // One target, one control: the second replaced the first.
    QCOMPARE(cm->count(), 1);
    QVERIFY(!cm->isBound(0, kNote));

    Scene *b = scene("Cool", 1);
    cm->setBinding({ 0, kNote, ControlMap::function(b->id()), ControlMap::StartOnly });
    QVERIFY(cm->setMode(ControlMap::function(a->id()), ControlMap::StopOnly));

    for (int i = 0; i < 2; ++i)
    {
        send(kNote, 127);
        send(kNote, 0);
        tick();
        QVERIFY(b->isRunning());                     // start only: twice is still on
    }
    send(kNote + 1, 127);                            // stop only, on a stopped one
    send(kNote + 1, 0);
    tick();
    QVERIFY(!a->isRunning());
    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick();
    send(kNote + 1, 127);
    tick();
    QVERIFY(!a->isRunning());
}

void ControlMap_Test::oneControlOneTarget()
{
    Scene *a = scene("A", 0);
    Scene *b = scene("B", 1);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kNote, ControlMap::function(a->id()), ControlMap::Toggle });
    cm->setBinding({ 0, kNote, ControlMap::function(b->id()), ControlMap::Toggle });
    QCOMPARE(cm->count(), 1);
    QVERIFY(cm->binding(ControlMap::function(a->id())) == nullptr);
    send(kNote, 127);
    tick();
    QVERIFY(b->isRunning());
    QVERIFY(!a->isRunning());

    QVERIFY(cm->removeBinding(ControlMap::function(b->id())));
    QVERIFY(!cm->removeBinding(ControlMap::function(b->id())));
    send(kNote, 0);
    send(kNote, 127);
    tick();
    QVERIFY(b->isRunning());                         // nothing fired: still on
    QVERIFY(cm->isEmpty());
}

/* The control's light: on while what it fires runs, off when it stops,
   sent on binding and on removal, with the profile's parameters. */
void ControlMap_Test::theLightFollowsWhatItFires()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kNote, ControlMap::function(a->id()), ControlMap::Toggle });
    QVERIFY(!m_stub->m_feedback.isEmpty());
    // Dim: it isn't running, and a press would fire it (CS3).
    QCOMPARE(m_stub->m_feedback.last(), qMakePair(kNote, uchar(30)));

    send(kNote, 127);
    tick();
    QCOMPARE(m_stub->m_feedback.last(), qMakePair(kNote, uchar(255)));
    send(kNote, 0);
    send(kNote, 127);
    tick(3);
    QCOMPARE(m_stub->m_feedback.last(), qMakePair(kNote, uchar(30)));

    // Started from elsewhere (a VC button, the Workbench): lit too.
    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick();
    QCOMPARE(m_stub->m_feedback.last(), qMakePair(kNote, uchar(255)));

    // Plugged back in: every light sent again.
    m_stub->m_feedback.clear();
    cm->resendFeedback();
    QCOMPARE(m_stub->m_feedback.count(), 1);
    QCOMPARE(m_stub->m_feedback.last(), qMakePair(kNote, uchar(255)));

    // Removed: off.
    cm->removeBinding(ControlMap::function(a->id()));
    QCOMPARE(m_stub->m_feedback.last(), qMakePair(kNote, uchar(0)));
}

/* CS3. The light has five things to say, each read from the one place that
   owns the answer: dark (a press would do nothing), dim (ready), bright
   (running), two blinks (the guard held a GO) and the stop's own lit.
   Here: ready, running, and a Stop-only control with nothing to stop. */
void ControlMap_Test::theLightSaysReadyRunningAndDark()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    const ControlMap::Target target = ControlMap::function(a->id());
    cm->setBinding({ 0, kNote, target, ControlMap::Toggle });

    // Ready: dim, which is MIDI velocity 15 (the value halves on the wire).
    QCOMPARE(cm->lamp(*cm->binding(target)), ControlMap::LampStandby);
    QCOMPARE(lastLight(m_stub, kNote), 30);
    QCOMPARE(ControlMap::lampName(ControlMap::LampStandby), QString("Ready"));

    send(kNote, 127);
    tick();
    QVERIFY(a->isRunning());
    QCOMPARE(cm->lamp(*cm->binding(target)), ControlMap::LampRunning);
    QCOMPARE(lastLight(m_stub, kNote), 255);

    // Stop only while it runs: bright, and a press stops it.
    QVERIFY(cm->setMode(target, ControlMap::StopOnly));
    QCOMPARE(lastLight(m_stub, kNote), 255);
    send(kNote, 0);
    send(kNote, 127);
    tick(3);
    QVERIFY(!a->isRunning());
    // Nothing to stop: a press of it does nothing, so it is dark -- not dim.
    QCOMPARE(cm->lamp(*cm->binding(target)), ControlMap::LampOff);
    QCOMPARE(lastLight(m_stub, kNote), 0);
    QCOMPARE(ControlMap::lampName(ControlMap::LampOff), QString("Dark"));

    // Back to Toggle: ready again (the mode change alone sends the light).
    QVERIFY(cm->setMode(target, ControlMap::Toggle));
    QCOMPARE(lastLight(m_stub, kNote), 30);

    // The thing it fired is gone: dark, as its binding goes with it.
    QVERIFY(m_doc->deleteFunction(a->id()));
    QCOMPARE(lastLight(m_stub, kNote), 0);
}

/* The press rule itself (board Q269), shown on the desk: a hit whose place
   is not live is dark, so the operator does not find out by pressing. */
void ControlMap_Test::aLightIsDarkWhereAPressWouldBeRefused()
{
    ShowTreeController *st = m_doc->showTree();
    QList<quint32> cues;
    const quint32 listId = cueList(2, &cues);
    // A hit under cue 1: its place is live only while that cue is.
    const quint32 hit = st->addHit(ShowTree::HitScope::Cue, cues.at(0), "Stab");
    QVERIFY(hit != ShowTree::invalidId());
    ShowTree::Target t;
    t.groups << m_doc->fixtureGroups().at(0)->id();
    st->setLayerLook(hit, st->addLayer(hit, t), st->lookForPalette(m_doc->palettes().at(0)->id()));

    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 20, ControlMap::hit(hit), ControlMap::Toggle });
    const ControlMap::Binding *b = cm->binding(ControlMap::hit(hit));
    QVERIFY(b != nullptr);

    // Operate with the list stopped: the cue is not live, so dark.
    m_doc->setMode(Doc::Operate);
    QCoreApplication::processEvents();
    tick(2);
    QVERIFY(!st->isHitArmed(hit));
    QCOMPARE(cm->lamp(*b), ControlMap::LampOff);
    QCOMPARE(lastLight(m_stub, 20), 0);

    // Cue 1 live: the place is live, so the pad is dim -- press me.
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    QVERIFY(chaser != nullptr);
    chaser->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    QCOMPARE(st->runningCue(listId), cues.at(0));
    QVERIFY(st->isHitArmed(hit));
    QCOMPARE(cm->lamp(*b), ControlMap::LampStandby);
    QCOMPARE(lastLight(m_stub, 20), 30);

    // GO to cue 2: cue 1 has left the stage, so the pad goes dark -- worked
    // out on the cue change, not per frame (hitArmingChanged).
    ShowControl::Request r;
    r.chaser = chaser->id();
    r.command = ShowControl::Go;
    QVERIFY(m_doc->showControl()->run(r, ShowControl::RouteOther));
    tick(3);
    QCOMPARE(st->runningCue(listId), cues.at(1));
    QVERIFY(!st->isHitArmed(hit));
    QCOMPARE(lastLight(m_stub, 20), 0);
}

/* After an emergency stop every light is dark until the next GO, and the
   stop's own control is lit: one status source with the footer and the pad
   (OutputRuleKeeper::emergencyStopped()). */
void ControlMap_Test::theLightGoesDarkAfterTheStopUntilTheNextGo()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kNote, ControlMap::function(a->id()), ControlMap::Toggle });
    cm->setBinding({ 0, 40, ControlMap::emergencyStop(), ControlMap::StartOnly });
    QCOMPARE(lastLight(m_stub, kNote), 30);
    // The stop is ready at every moment, so it can be found in the dark.
    QCOMPARE(lastLight(m_stub, 40), 30);

    m_doc->outputRuleKeeper()->emergencyStop();
    tick(3);
    QVERIFY(m_doc->outputRuleKeeper()->emergencyStopped());
    QCOMPARE(cm->lamp(*cm->binding(ControlMap::emergencyStop())), ControlMap::LampStopped);
    QCOMPARE(lastLight(m_stub, 40), 255);      // the stop says the show is stopped
    QCOMPARE(lastLight(m_stub, kNote), 0);     // dark until the next GO
    QCOMPARE(ControlMap::lampName(ControlMap::LampStopped), QString("Stopped"));

    // The first GO after the stop ends it: the lights come back.
    m_doc->outputRuleKeeper()->noteGo();
    tick(3);
    QVERIFY(!m_doc->outputRuleKeeper()->emergencyStopped());
    QCOMPARE(lastLight(m_stub, 40), 30);       // ready again
    QCOMPARE(lastLight(m_stub, kNote), 30);
}

/* The double-GO guard held a command: every control that GOes that list
   blinks twice and then says what it says now -- the same notice the Run
   tab, the footer and the pad get (guard rule 2). */
void ControlMap_Test::aHeldGoBlinksTheControlTwice()
{
    QList<quint32> cues;
    const quint32 listId = cueList(3, &cues);
    ShowTreeController *st = m_doc->showTree();
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    QVERIFY(chaser != nullptr);

    ControlMap *cm = m_doc->controlMap();
    cm->setBlinkMs(1);
    cm->setBinding({ 0, 10, ControlMap::cueList(listId), ControlMap::Toggle });
    ShowControl *sc = m_doc->showControl();
    sc->setGoGuardMs(ShowControl::maxGoGuardMs());   // long, so a loaded
    auto unhook = qScopeGuard([sc]() { sc->setGoGuardMs(0); });   // machine still holds

    send(10, 127);                      // the first GO is carried out
    tick(2);
    QVERIFY(chaser->isRunning());
    QSignalSpy held(sc, &ShowControl::commandHeld);
    m_stub->m_feedback.clear();
    send(10, 0);
    send(10, 127);                      // a second GO inside the window
    tick(2);
    QCOMPARE(held.count(), 1);

    // On, off, on, off, then the truth (bright: the list runs).
    QTRY_COMPARE(m_stub->m_feedback.count(), 5);
    QList<int> blink;
    for (const auto &f : m_stub->m_feedback)
        if (f.first == 10u)
            blink << int(f.second);
    QCOMPARE(blink, QList<int>({ 255, 0, 255, 0, 255 }));
    QVERIFY(!cm->isBlinking(*cm->binding(ControlMap::cueList(listId))));
    QVERIFY(chaser->isRunning());       // the held GO did not advance the list
    QCOMPARE(st->runningCue(listId), cues.at(0));

    /* The trigger removed in the middle of a blink: it stops at once and
       goes dark, rather than blink on for a control that fires nothing.
       Slow steps, so the chain is certainly still in flight. */
    cm->setBlinkMs(50);
    send(10, 0);
    send(10, 127);
    tick(2);
    QVERIFY2(cm->isBlinking(*cm->binding(ControlMap::cueList(listId))), "the second hold did not blink");
    QVERIFY(cm->removeBinding(ControlMap::cueList(listId)));
    QCOMPARE(lastLight(m_stub, 10), 0);
    m_stub->m_feedback.clear();
    QTest::qWait(250);                  // the three steps left would have fallen here
    QVERIFY2(m_stub->m_feedback.isEmpty(), "a removed trigger's control was still blinking");
}

/* A cue list a GO would do nothing with -- every cue cut for the rehearsal
   -- is dark, read from ShowControl::standbyStep() and not from the
   Control Map's own guess. */
void ControlMap_Test::aCueListWithNothingToGoIsDark()
{
    QList<quint32> cues;
    const quint32 listId = cueList(2, &cues);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 10, ControlMap::cueList(listId), ControlMap::Toggle });
    QCOMPARE(lastLight(m_stub, 10), 30);        // ready: GO plays cue 1

    // Every cue cut: a GO would play nothing, so the control goes dark --
    // and it follows the edit by itself (one pass per batch of edits).
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setCuesSkipped(cues, true));
    tick(2);
    QCOMPARE(cm->lamp(*cm->binding(ControlMap::cueList(listId))), ControlMap::LampOff);
    QCOMPARE(lastLight(m_stub, 10), 0);

    QVERIFY(st->setCuesSkipped({ cues.at(1) }, false));
    tick(2);
    QCOMPARE(lastLight(m_stub, 10), 30);
}

/* A pad whose lights take a colour in the velocity and not a brightness
   (the APCs) shows nothing at a dim level: the dim light switches off, a
   ready control stays dark, and running and stopped are unchanged. Saved
   with the show; a show with it on saves as before. */
void ControlMap_Test::theDimLightIsSwitchableAndSavedWithTheShow()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kNote, ControlMap::function(a->id()), ControlMap::Toggle });
    QVERIFY(cm->dimWhenReady());
    QCOMPARE(lastLight(m_stub, kNote), 30);

    cm->setDimWhenReady(false);
    QCOMPARE(lastLight(m_stub, kNote), 0);      // ready, and dark
    // Still bright while it runs.
    send(kNote, 127);
    tick();
    QCOMPARE(lastLight(m_stub, kNote), 255);
    send(kNote, 0);
    send(kNote, 127);
    tick(3);
    QCOMPARE(lastLight(m_stub, kNote), 0);
    // The lamp itself still says Ready: only the value sent changes.
    QCOMPARE(cm->lamp(*cm->binding(ControlMap::function(a->id()))), ControlMap::LampStandby);

    const QByteArray xml = saveShow(m_doc);
    QVERIFY(xml.contains("Lights=\"onoff\""));
    QVERIFY(loadShow(m_doc, xml));
    QVERIFY(!m_doc->controlMap()->dimWhenReady());
    QCOMPARE(lastLight(m_stub, kNote), 0);

    m_doc->controlMap()->setDimWhenReady(true);
    const QByteArray on = saveShow(m_doc);
    QVERIFY(!on.contains("Lights="));           // a show with it on saves as before
    QVERIFY(loadShow(m_doc, on));
    QVERIFY(m_doc->controlMap()->dimWhenReady());
}

/* A desk that vanishes mid-show and comes back (a USB hub, a knocked
   cable, the same board re-enumerated as another device) has a dark board
   when it returns: every light is sent again, changed or not, and each
   says what it says now and not what it said when the cable went. */
void ControlMap_Test::theLightsComeBackWhenTheControllerDoes()
{
    Scene *a = scene("Warm", 0);
    Scene *b = scene("Cool", 1);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kNote, ControlMap::function(a->id()), ControlMap::Toggle });
    cm->setBinding({ 0, kNote + 1, ControlMap::function(b->id()), ControlMap::Toggle });
    send(kNote, 127);
    tick(2);
    QVERIFY(a->isRunning());
    QCOMPARE(lastLight(m_stub, kNote), 255);

    InputOutputMap *iom = m_doc->inputOutputMap();
    emit iom->inputDeviceDisconnected(0, m_stub->name(), QString("Fake"));
    tick(3);
    // While it is gone the scene is stopped from the Workbench: the board
    // cannot be told, and must not be believed when it comes back.
    a->stop(FunctionParent::master());
    tick(3);
    m_stub->m_feedback.clear();

    // Back -- as another device behind the same patch.
    emit iom->inputDeviceConnected(0, m_stub->name(), QString("Fake 2"));
    QCoreApplication::processEvents();
    QCOMPARE(m_stub->m_feedback.count(), 2);        // both lights, not only what changed
    QCOMPARE(lastLight(m_stub, kNote), 30);         // ready, not the 255 it was left at
    QCOMPARE(lastLight(m_stub, kNote + 1), 30);

    // A universe that is no longer patched at all: nothing crashes and
    // nothing is sent (the show runs on without a desk).
    iom->setOutputPatch(0, QString(), QString(), 0, true);
    QVERIFY(iom->feedbackPatch(0) == nullptr);
    m_stub->m_feedback.clear();
    cm->resendFeedback();
    QVERIFY(m_stub->m_feedback.isEmpty());
    send(kNote, 127);
    tick(2);
    QVERIFY(a->isRunning());                        // it still fires
}

/* Review finding 2. A new show takes the desk dark with it. Without this
   a File > New left every learned control lit as the last show had it --
   and with the bindings gone there was nothing left to correct them. */
void ControlMap_Test::aNewShowTakesTheLightsWithIt()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kNote, ControlMap::function(a->id()), ControlMap::Toggle });
    cm->setBinding({ 0, 40, ControlMap::emergencyStop(), ControlMap::StartOnly });
    send(kNote, 127);
    tick(2);
    QVERIFY(a->isRunning());
    QCOMPARE(lastLight(m_stub, kNote), 255);
    QCOMPARE(lastLight(m_stub, 40), 30);

    m_stub->m_feedback.clear();
    cm->reset();                            // a new show
    QVERIFY(cm->isEmpty());
    // Every control it lit is sent 0: nothing of the old show is left lit.
    QCOMPARE(m_stub->m_feedback.count(), 2);
    QCOMPARE(lastLight(m_stub, kNote), 0);
    QCOMPARE(lastLight(m_stub, 40), 0);
    // And nothing is left that would light them again.
    m_stub->m_feedback.clear();
    cm->resendFeedback();
    QVERIFY(m_stub->m_feedback.isEmpty());
}

/* Review finding 3. The dim switch is for a pad whose lights are colours;
   it must never take the panic button's light away, because a stop nobody
   can find on a dark stage is not a stop. */
void ControlMap_Test::theDimSwitchNeverDarkensTheStop()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kNote, ControlMap::function(a->id()), ControlMap::Toggle });
    cm->setBinding({ 0, 40, ControlMap::emergencyStop(), ControlMap::StartOnly });
    QCOMPARE(lastLight(m_stub, kNote), 30);
    QCOMPARE(lastLight(m_stub, 40), 30);

    cm->setDimWhenReady(false);
    QCOMPARE(lastLight(m_stub, kNote), 0);      // a scene's ready light goes
    QCOMPARE(lastLight(m_stub, 40), 30);        // the stop's does not

    // Still lit while the show is stopped, and dim again after a GO.
    m_doc->outputRuleKeeper()->emergencyStop();
    tick(3);
    QCOMPARE(lastLight(m_stub, 40), 255);
    m_doc->outputRuleKeeper()->noteGo();
    tick(3);
    QCOMPARE(lastLight(m_stub, 40), 30);
    // Saved and loaded back, the stop still has its light.
    QVERIFY(loadShow(m_doc, saveShow(m_doc)));
    QVERIFY(!m_doc->controlMap()->dimWhenReady());
    QCOMPARE(lastLight(m_stub, 40), 30);
}

/* Review finding 1. A collection starting many scenes sends one
   functionStarted each; the lights are worked out once for the batch, not
   once per scene -- a pass asks ShowControl for each cue-list binding's
   standby cue, and that takes the Chaser mutexes the MasterTimer thread
   holds while it steps. */
void ControlMap_Test::manyFunctionsAtOnceAreOneLightPass()
{
    QList<quint32> cues;
    const quint32 listId = cueList(2, &cues);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 10, ControlMap::cueList(listId), ControlMap::Toggle });
    QList<Scene *> scenes;
    for (int i = 0; i < 4; ++i)
        scenes << scene(QString("S%1").arg(i), i);
    cm->setBinding({ 0, kNote, ControlMap::function(scenes.at(0)->id()), ControlMap::Toggle });
    tick(2);

    QSignalSpy passes(cm, &ControlMap::lampsChanged);
    for (Scene *sc : scenes)
        sc->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    for (Scene *sc : scenes)
        QVERIFY(sc->isRunning());
    QCOMPARE(lastLight(m_stub, kNote), 255);        // the light is right
    QCOMPARE(passes.count(), 1);                    // and was worked out once

    // The same on the way back down.
    passes.clear();
    for (Scene *sc : scenes)
        sc->stop(FunctionParent::master());
    tick(3);
    QCOMPARE(lastLight(m_stub, kNote), 30);
    QCOMPARE(passes.count(), 1);
}

/* A light nothing changed about is not sent again: a mode switch or a
   production change with a bank of pads must not flood the MIDI line. */
void ControlMap_Test::aLightThatDidNotChangeIsNotSentAgain()
{
    Scene *a = scene("Warm", 0);
    Scene *b = scene("Cool", 1);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kNote, ControlMap::function(a->id()), ControlMap::Toggle });
    cm->setBinding({ 0, kNote + 1, ControlMap::function(b->id()), ControlMap::Toggle });

    m_stub->m_feedback.clear();
    m_doc->setMode(Doc::Operate);               // a scene fires in both modes
    QCoreApplication::processEvents();
    tick(2);
    QCOMPARE(m_stub->m_feedback.count(), 0);    // nothing to say: nothing sent

    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    QCOMPARE(m_stub->m_feedback.count(), 1);    // one light, not both
    QCOMPARE(m_stub->m_feedback.last(), qMakePair(kNote, uchar(255)));

    // A device plugged back in sends every light, changed or not.
    m_stub->m_feedback.clear();
    cm->resendFeedback();
    QCOMPARE(m_stub->m_feedback.count(), 2);
}

void ControlMap_Test::aCueListGoesAndACueJumps()
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
    const quint32 listId = st->addCueList(prod, "Act 1");
    QList<quint32> cues;
    for (int i = 0; i < 3; ++i)
    {
        ShowTree::Target t;
        t.groups << g->id();
        const quint32 c = st->addCue(listId, i - 1, QString("Cue %1").arg(i + 1));
        st->setLayerLook(c, st->addLayer(c, t), st->lookForPalette(full->id()));
        cues << c;
    }
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    QVERIFY(chaser != nullptr);

    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 10, ControlMap::cueList(listId), ControlMap::Toggle });
    cm->setBinding({ 0, 11, ControlMap::cue(cues.at(2)), ControlMap::Toggle });
    cm->setBinding({ 0, 12, ControlMap::cueList(listId), ControlMap::StopOnly });
    QCOMPARE(cm->count(), 2);                       // one control per target: 12 replaced 10
    cm->setBinding({ 0, 10, ControlMap::cueList(listId), ControlMap::Toggle });
    QCOMPARE(cm->targetName(ControlMap::cueList(listId)), QString("Cue list: Act 1"));
    QVERIFY(cm->targetName(ControlMap::cue(cues.at(2))).contains("Act 1"));

    // The one GO path (docs/design/RUN_COMMANDS.md): each GO and Stop
    // comes through ShowControl::run(), as the Control Map's.
    QList<ShowControl::Route> routes;
    ShowControl *sc = m_doc->showControl();
    sc->setRunHandler([&routes, sc](const ShowControl::Request &r, ShowControl::Route route) {
        routes << route;
        return sc->execute(r);
    });
    auto unhook = qScopeGuard([sc]() { sc->setRunHandler(nullptr); });

    QSignalSpy fired(cm, &ControlMap::fired);
    send(10, 127);
    tick(2);
    QVERIFY(chaser->isRunning());
    QCOMPARE(st->runningCue(listId), cues.at(0));
    QCOMPARE(fired.count(), 1);
    send(10, 0);
    send(10, 127);                                   // GO again: the next cue, not a stop
    tick(2);
    QVERIFY(chaser->isRunning());
    QCOMPARE(st->runningCue(listId), cues.at(1));

    send(11, 127);                                   // that cue
    tick(2);
    QCOMPARE(st->runningCue(listId), cues.at(2));
    QCOMPARE(lastLight(m_stub, 11), 255);

    // Stop only: the list stops.
    cm->setMode(ControlMap::cueList(listId), ControlMap::StopOnly);
    send(10, 0);
    send(10, 127);
    tick(3);
    QVERIFY(!chaser->isRunning());

    // Momentary: GO on the press, stopped on the release.
    cm->setMode(ControlMap::cueList(listId), ControlMap::Momentary);
    send(10, 0);
    send(10, 127);
    tick(2);
    QVERIFY(chaser->isRunning());
    send(10, 0);
    tick(3);
    QVERIFY(!chaser->isRunning());

    // Every GO and Stop the controls above sent. (Not a seventh: the release
    // of the Stop-only press, once the mode was changed to Momentary, used to
    // send a Stop of its own; a press that started nothing has no release.)
    QCOMPARE(routes.count(), 6);
    for (ShowControl::Route r : routes)
        QCOMPARE(r, ShowControl::RouteControlMap);
}

/* Make Sub-cues (2026-11-12) keeps the cue's id, so a control learned on
   cue 3 still plays it once it is sub-cue 2.2: its cue's step at that
   sub-cue, as /go 2.2 does -- and armed with its cue's list. */
void ControlMap_Test::aCueMadeASubCueStillJumps()
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
    const quint32 listId = st->addCueList(prod, "Act 1");
    QList<quint32> cues;
    for (int i = 0; i < 3; ++i)
    {
        ShowTree::Target t;
        t.groups << g->id();
        const quint32 c = st->addCue(listId, i - 1, QString("Cue %1").arg(i + 1));
        st->setLayerLook(c, st->addLayer(c, t), st->lookForPalette(full->id()));
        cues << c;
    }
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 11, ControlMap::cue(cues.at(2)), ControlMap::Toggle });
    QString msg;
    QVERIFY2(st->makeSubCues({ cues.at(2) }, cues.at(1), &msg), qPrintable(msg));
    QVERIFY(st->isSubCue(cues.at(2)));
    QVERIFY(cm->targetExists(ControlMap::cue(cues.at(2))));
    QVERIFY(cm->targetName(ControlMap::cue(cues.at(2))).contains("2.2"));
    cm->setRunTabList(listId);
    QVERIFY(cm->isArmed(ControlMap::cue(cues.at(2))));
    cm->setRunTabList(ShowTree::invalidId());

    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    QVERIFY(chaser != nullptr);
    QList<QPair<int, int>> taken;
    const QMetaObject::Connection conn = connect(m_doc->showControl(), &ShowControl::cueTaken, this,
        [&taken](ShowControl::Command, quint32, int, int, int step, int sub) { taken << qMakePair(step, sub); });
    auto unhook = qScopeGuard([conn]() { QObject::disconnect(conn); });
    QStringList said;
    connect(m_doc->showControl(), &ShowControl::commandHandled, this,
            [&said](const QString &what, bool done) { said << QStringLiteral("%1 %2").arg(what).arg(done); });
    QCOMPARE(chaser->subCueCount(1), 2);
    QCOMPARE(ShowControl::cueName(chaser, 1, 1), QString("2.2"));
    send(11, 127);
    tick(2);
    QVERIFY2(chaser->isRunning(), qPrintable(said.join(" / ")));
    QCOMPARE(chaser->currentStepIndex(), 1);
    QCOMPARE(taken.count(), 1);
    QCOMPARE(taken.last(), qMakePair(1, 1));         // cue 2's step, at sub-cue 2.2
    QCOMPARE(chaser->currentSubCueIndex(), 1);
    send(11, 0);

    // A control on cue 2 itself (it has sub-cues: its step plays their
    // chaser) jumps to it too, after cue 1.
    cm->setBinding({ 0, 12, ControlMap::cue(cues.at(0)), ControlMap::Toggle });
    cm->setBinding({ 0, 13, ControlMap::cue(cues.at(1)), ControlMap::Toggle });
    send(12, 127);
    tick(2);
    QCOMPARE(st->runningCue(listId), cues.at(0));
    send(13, 127);
    tick(2);
    QCOMPARE(chaser->currentStepIndex(), 1);
    QCOMPARE(taken.last().first, 1);
    send(12, 0);
    send(13, 0);
}

void ControlMap_Test::aHitFires()
{
    ShowTreeController *st = m_doc->showTree();
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("All");
    m_doc->addFixtureGroup(g);
    g->assignFixture(m_doc->fixtures().at(0)->id());
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer, m_doc);
    full->setValue(255);
    m_doc->addPalette(full);
    const quint32 prod = st->addProduction("Show");
    const quint32 hit = st->addLook(prod, "Stab");
    ShowTree::Target t;
    t.groups << g->id();
    st->setLayerLook(hit, st->addLayer(hit, t), st->lookForPalette(full->id()));
    const quint32 coll = st->cue(hit)->collection;
    QVERIFY(m_doc->function(coll) != nullptr);

    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 20, ControlMap::hit(hit), ControlMap::Momentary });
    QCOMPARE(cm->targetName(ControlMap::hit(hit)), QString("Hit: Stab"));
    send(20, 127);
    tick();
    QVERIFY(m_doc->function(coll)->isRunning());
    QCOMPARE(m_stub->m_feedback.last(), qMakePair(quint32(20), uchar(255)));
    send(20, 0);
    tick(3);
    QVERIFY(!m_doc->function(coll)->isRunning());
    // Dim, not dark: the hit's place is live, so a press would fire it (CS3).
    QCOMPARE(m_stub->m_feedback.last(), qMakePair(quint32(20), uchar(30)));
}

void ControlMap_Test::savedWithTheShowAndLoadedBack()
{
    Scene *a = scene("A", 0);
    Scene *b = scene("B", 1);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kNote, ControlMap::function(a->id()), ControlMap::Toggle });
    cm->setBinding({ 0, 7, ControlMap::function(b->id()), ControlMap::Momentary });
    cm->setBinding({ 0, 8, ControlMap::cueList(4242), ControlMap::StopOnly });
    const QList<ControlMap::Binding> before = cm->bindings();
    const QByteArray xml = saveShow(m_doc);
    QVERIFY(xml.contains("<ControlMap>"));
    QVERIFY(xml.contains("Mode=\"Momentary\""));

    // A newer build's target, and junk: left out; the rest loads.
    QByteArray withNew = xml;
    withNew.replace("<ControlMap>", "<ControlMap><Trigger Universe=\"0\" Channel=\"99\" Target=\"Laser\" ID=\"1\"/>"
                                    "<Trigger Universe=\"x\"/><Other/>");

    Doc *other = new Doc(this);
    QVERIFY(loadShow(other, withNew));
    QCOMPARE(other->controlMap()->bindings(), before);
    delete other;

    // New (clearContents): none.
    m_doc->clearContents();
    QVERIFY(m_doc->controlMap()->isEmpty());
}

void ControlMap_Test::aShowWithoutBindingsSavesAsBefore()
{
    scene("A", 0);
    QVERIFY(!saveShow(m_doc).contains("ControlMap"));
}

void ControlMap_Test::aDeletedFunctionTakesItsBinding()
{
    Scene *a = scene("A", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kNote, ControlMap::function(a->id()), ControlMap::Toggle });
    QVERIFY(m_doc->deleteFunction(a->id()));
    QVERIFY(cm->isEmpty());
    QCOMPARE(m_stub->m_feedback.last(), qMakePair(kNote, uchar(0)));
}

/* No bindings, not learning: input doesn't reach the map at all (the
   input path costs nothing more than before). */
void ControlMap_Test::nothingListensWithoutBindings()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    ControlMap *cm = m_doc->controlMap();
    QVERIFY(!QObject::disconnect(iom, &InputOutputMap::inputValueChanged, cm, &ControlMap::handleInput));
    Scene *a = scene("A", 0);
    cm->setBinding({ 0, kNote, ControlMap::function(a->id()), ControlMap::Toggle });
    QVERIFY(QObject::disconnect(iom, &InputOutputMap::inputValueChanged, cm, &ControlMap::handleInput));
    QVERIFY(QObject::connect(iom, &InputOutputMap::inputValueChanged, cm, &ControlMap::handleInput));
    cm->removeAll();
    QVERIFY(!QObject::disconnect(iom, &InputOutputMap::inputValueChanged, cm, &ControlMap::handleInput));
    cm->listen();
    QVERIFY(QObject::disconnect(iom, &InputOutputMap::inputValueChanged, cm, &ControlMap::handleInput));
    QVERIFY(QObject::connect(iom, &InputOutputMap::inputValueChanged, cm, &ControlMap::handleInput));
    cm->stopListening();
    QVERIFY(!QObject::disconnect(iom, &InputOutputMap::inputValueChanged, cm, &ControlMap::handleInput));
}

void ControlMap_Test::namesSayWhatAControlIs()
{
    ControlMap *cm = m_doc->controlMap();
    // The stub is no MIDI plugin and has no profile: the channel.
    QVERIFY(cm->controlName(0, 5).endsWith("Channel 6"));
    m_stub->m_channelNames.insert(5, "Pad 1");
    QVERIFY(cm->controlName(0, 5).endsWith("Pad 1"));
    QCOMPARE(cm->targetName(ControlMap::function(9999)), QString("(gone)"));
    QVERIFY(!cm->targetExists(ControlMap::cue(9999)));
    QCOMPARE(ControlMap::modeName(ControlMap::StopOnly, ControlMap::CueListTarget), QString("Stop the list"));
    ControlMap::Mode m;
    QVERIFY(ControlMap::stringToMode("StartOnly", &m));
    QCOMPARE(m, ControlMap::StartOnly);
}

/* A computer key: universe KeyboardUniverse, channel the key and its
   modifiers; named "Key · F5"; one control, one thing, as any control. */
void ControlMap_Test::aComputerKeyIsAControl()
{
    const quint32 f5 = ControlMap::keyChannel(Qt::Key_F5, Qt::NoModifier);
    QCOMPARE(f5, quint32(Qt::Key_F5));
    QCOMPARE(ControlMap::keyChannel(Qt::Key_1, Qt::ShiftModifier | Qt::KeypadModifier),
             quint32(QKeyCombination(Qt::ShiftModifier, Qt::Key_1).toCombined()));
    QCOMPARE(ControlMap::keyChannel(Qt::Key_Shift, Qt::ShiftModifier), 0u);   // a modifier alone
    QCOMPARE(ControlMap::keyChannel(0, Qt::NoModifier), 0u);

    ControlMap *cm = m_doc->controlMap();
    QCOMPARE(cm->controlName(ControlMap::KeyboardUniverse, f5), QString("Key · F5"));
    Scene *a = scene("Warm", 0);
    Scene *b = scene("Cool", 1);
    QVERIFY(!cm->hasKeyBindings());
    cm->setBinding({ ControlMap::KeyboardUniverse, f5, ControlMap::function(a->id()), ControlMap::Toggle });
    QVERIFY(cm->hasKeyBindings());
    QVERIFY(ControlMap::isKey(*cm->binding(ControlMap::function(a->id()))));
    // The same key for another: it moves.
    cm->setBinding({ ControlMap::KeyboardUniverse, f5, ControlMap::function(b->id()), ControlMap::Toggle });
    QCOMPARE(cm->count(), 1);
    QVERIFY(cm->binding(ControlMap::function(a->id())) == nullptr);
    // A key sends no light.
    m_stub->m_feedback.clear();
    cm->resendFeedback();
    QVERIFY(m_stub->m_feedback.isEmpty());
    // Deleting the function takes its key.
    m_doc->deleteFunction(b->id());
    QVERIFY(cm->isEmpty());
    QVERIFY(!cm->hasKeyBindings());
}

/* A scene's key: nothing in Design, fires in Operate; held is one press;
   the release lets go even after the mode changed. */
void ControlMap_Test::aKeyFiresInOperateOnly()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    const quint32 f5 = ControlMap::keyChannel(Qt::Key_F5, Qt::NoModifier);
    cm->setBinding({ ControlMap::KeyboardUniverse, f5, ControlMap::function(a->id()), ControlMap::Toggle });

    QCOMPARE(m_doc->mode(), Doc::Design);
    QVERIFY(!cm->keyMayFire(ControlMap::function(a->id())));
    QVERIFY(!cm->handleKey(f5, true));              // not used up: it goes to the widget
    tick();
    QVERIFY(!a->isRunning());
    QVERIFY(!cm->handleKey(f5, false));

    m_doc->setMode(Doc::Operate);
    QVERIFY(cm->handleKey(f5, true));
    QVERIFY(cm->isKeyDown(f5));
    QVERIFY(cm->handleKey(f5, true));               // held: still the one press
    tick();
    QVERIFY(a->isRunning());
    QVERIFY(cm->handleKey(f5, false));
    tick();
    QVERIFY(a->isRunning());                        // Toggle: the release does nothing
    QVERIFY(cm->handleKey(f5, true));
    QVERIFY(cm->handleKey(f5, false));
    tick(3);
    QVERIFY(!a->isRunning());

    // Momentary: on while held; back in Design the release still lets go.
    cm->setMode(ControlMap::function(a->id()), ControlMap::Momentary);
    QVERIFY(cm->handleKey(f5, true));
    tick();
    QVERIFY(a->isRunning());
    m_doc->setMode(Doc::Design);
    QVERIFY(cm->handleKey(f5, false));
    tick(3);
    QVERIFY(!a->isRunning());

    // The app lost the keyboard: every key held lets go.
    m_doc->setMode(Doc::Operate);
    QVERIFY(cm->handleKey(f5, true));
    tick();
    QVERIFY(a->isRunning());
    cm->releaseKeys();
    tick(3);
    QVERIFY(!a->isRunning());
    QVERIFY(!cm->isKeyDown(f5));

    // A key that isn't bound: not used up.
    QVERIFY(!cm->handleKey(ControlMap::keyChannel(Qt::Key_F6, Qt::NoModifier), true));
}

namespace
{
/** A production with a cue list of two cues (each on fixture 0's group at
 *  full) and a hit of the production. */
struct KeyShow
{
    quint32 prod, list, cue1, cue2, hit, group, palette;
};

KeyShow keyShow(Doc *doc)
{
    ShowTreeController *st = doc->showTree();
    FixtureGroup *g = new FixtureGroup(doc);
    g->setName("All");
    doc->addFixtureGroup(g);
    g->assignFixture(doc->fixtures().at(0)->id());
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer, doc);
    full->setName("Full");
    full->setValue(255);
    doc->addPalette(full);
    KeyShow s;
    s.group = g->id();
    s.palette = full->id();
    s.prod = st->addProduction("Show");
    s.list = st->addCueList(s.prod, "Act 1");
    ShowTree::Target t;
    t.groups << g->id();
    s.cue1 = st->addCue(s.list, -1, "One");
    st->setLayerLook(s.cue1, st->addLayer(s.cue1, t), st->lookForPalette(full->id()));
    s.cue2 = st->addCue(s.list, 0, "Two");
    st->setLayerLook(s.cue2, st->addLayer(s.cue2, t), st->lookForPalette(full->id()));
    s.hit = st->addLook(s.prod, "Stab");
    st->setLayerLook(s.hit, st->addLayer(s.hit, t), st->lookForPalette(full->id()));
    return s;
}
}

/* A hit's key does what the hit's MIDI trigger does, press for press. */
void ControlMap_Test::aKeyHitFiresAsItsMidiTriggerDoes()
{
    const KeyShow show = keyShow(m_doc);
    ControlMap *cm = m_doc->controlMap();
    Function *coll = m_doc->function(m_doc->showTree()->cue(show.hit)->collection);
    QVERIFY(coll != nullptr);
    m_doc->setMode(Doc::Operate);
    const quint32 key = ControlMap::keyChannel(Qt::Key_1, Qt::ShiftModifier);

    for (ControlMap::Mode mode : { ControlMap::Toggle, ControlMap::Momentary })
    {
        // The MIDI note first, then the key, the same presses.
        QList<bool> midi, keys;
        cm->setBinding({ 0, 20, ControlMap::hit(show.hit), mode });
        for (int i = 0; i < 2; ++i)
        {
            send(20, 127); tick(); midi << coll->isRunning();
            send(20, 0); tick(3); midi << coll->isRunning();
        }
        coll->stop(FunctionParent::master());
        tick(3);
        cm->setBinding({ ControlMap::KeyboardUniverse, key, ControlMap::hit(show.hit), mode });
        QCOMPARE(cm->count(), 1);
        for (int i = 0; i < 2; ++i)
        {
            cm->handleKey(key, true); tick(); keys << coll->isRunning();
            cm->handleKey(key, false); tick(3); keys << coll->isRunning();
        }
        coll->stop(FunctionParent::master());
        tick(3);
        QCOMPARE(keys, midi);
        QCOMPARE(midi.first(), true);
    }
}

/* In Design a key fires what is armed by the Run tab's list: the list,
   its cues, the hits living in it; not another list's. */
void ControlMap_Test::aKeyIsArmedInDesignByTheRunList()
{
    const KeyShow show = keyShow(m_doc);
    ShowTreeController *st = m_doc->showTree();
    const quint32 other = st->addCueList(show.prod, "Act 2");
    const quint32 otherCue = st->addCue(other, -1, "Elsewhere");
    ControlMap *cm = m_doc->controlMap();
    QCOMPARE(m_doc->mode(), Doc::Design);

    // The Run tab on Act 2: Act 1 isn't armed.
    QSignalSpy arming(cm, &ControlMap::armingChanged);
    cm->setRunTabList(other);
    QCOMPARE(arming.count(), 1);
    QCOMPARE(cm->runList(), other);
    QVERIFY(cm->isArmed(ControlMap::cueList(other)));
    QVERIFY(cm->isArmed(ControlMap::cue(otherCue)));
    QVERIFY(!cm->isArmed(ControlMap::cueList(show.list)));
    QVERIFY(!cm->isArmed(ControlMap::cue(show.cue2)));
    QVERIFY(cm->isArmed(ControlMap::hit(show.hit)));     // the production's hit: Act 2 is in it
    QVERIFY(!cm->isArmed(ControlMap::function(st->cue(show.cue2)->collection)));   // a scene: never

    const quint32 f5 = ControlMap::keyChannel(Qt::Key_F5, Qt::NoModifier);
    cm->setBinding({ ControlMap::KeyboardUniverse, f5, ControlMap::cue(show.cue2), ControlMap::Toggle });
    QVERIFY(!cm->handleKey(f5, true));                   // not armed in Design: nothing
    tick(2);
    QCOMPARE(st->runningCue(show.list), ShowTree::invalidId());

    // The Run tab on Act 1: its cue's key fires.
    cm->setRunTabList(show.list);
    QVERIFY(cm->isArmed(ControlMap::cue(show.cue2)));
    QVERIFY(cm->handleKey(f5, true));
    tick(2);
    QCOMPARE(st->runningCue(show.list), show.cue2);
    QVERIFY(cm->handleKey(f5, false));

    // The Run tab on the production's own list: whatever ShowControl plays.
    cm->setRunTabList(ShowTree::invalidId());
    const Chaser *main = m_doc->showControl()->cueList(QString());
    if (main != nullptr)
    {
        quint32 lid = ShowTree::invalidId();
        st->productionOfChaser(main->id(), &lid);
        QCOMPARE(cm->runList(), lid);
    }

    // A production hit in another production isn't armed by it.
    const quint32 prod2 = st->addProduction("Other show");
    const quint32 hit2 = st->addLook(prod2, "Theirs");
    cm->setRunTabList(show.list);
    QVERIFY(!cm->isArmed(ControlMap::hit(hit2)));
    // A hit under a cue: armed by its cue's list.
    const quint32 cueHit = st->addHit(ShowTree::HitScope::Cue, show.cue1, "Breathe");
    QVERIFY(cm->isArmed(ControlMap::hit(cueHit)));
    cm->setRunTabList(other);
    QVERIFY(!cm->isArmed(ControlMap::hit(cueHit)));
}

/* In Design a key fires what is selected in the Show Tree: a list (with
   its cues and hits), a cue (with its hits), a hit. */
void ControlMap_Test::aKeyIsArmedInDesignByTheSelection()
{
    const KeyShow show = keyShow(m_doc);
    ShowTreeController *st = m_doc->showTree();
    const quint32 other = st->addCueList(show.prod, "Act 2");
    const quint32 cueHit = st->addHit(ShowTree::HitScope::Cue, show.cue1, "Breathe");
    ControlMap *cm = m_doc->controlMap();
    cm->setRunTabList(other);                         // the Run tab elsewhere
    const quint32 prod2 = st->addProduction("Other show");
    const quint32 hit2 = st->addLook(prod2, "Theirs");
    QVERIFY(!cm->isArmed(ControlMap::hit(hit2)));

    const quint32 key = ControlMap::keyChannel(Qt::Key_H, Qt::NoModifier);
    cm->setBinding({ ControlMap::KeyboardUniverse, key, ControlMap::hit(hit2), ControlMap::Toggle });
    Function *coll = m_doc->function(st->cue(hit2)->collection);
    QVERIFY(!cm->handleKey(key, true));
    tick();
    QVERIFY(coll == nullptr || !coll->isRunning());

    QSignalSpy arming(cm, &ControlMap::armingChanged);
    cm->setSelectedTargets({ ControlMap::hit(hit2) });
    QCOMPARE(arming.count(), 1);
    cm->setSelectedTargets({ ControlMap::hit(hit2) });   // the same: no signal
    QCOMPARE(arming.count(), 1);
    QVERIFY(cm->isArmed(ControlMap::hit(hit2)));
    QVERIFY(cm->handleKey(key, true));
    QVERIFY(cm->handleKey(key, false));

    // A list selected: it, its cues and the hits under them.
    cm->setSelectedTargets({ ControlMap::cueList(show.list) });
    QVERIFY(cm->isArmed(ControlMap::cueList(show.list)));
    QVERIFY(cm->isArmed(ControlMap::cue(show.cue2)));
    QVERIFY(cm->isArmed(ControlMap::hit(cueHit)));
    QVERIFY(!cm->isArmed(ControlMap::hit(hit2)));
    // A cue selected: it and its hits, not the other cue.
    cm->setSelectedTargets({ ControlMap::cue(show.cue1) });
    QVERIFY(cm->isArmed(ControlMap::cue(show.cue1)));
    QVERIFY(cm->isArmed(ControlMap::hit(cueHit)));
    QVERIFY(!cm->isArmed(ControlMap::cue(show.cue2)));
    QVERIFY(!cm->isArmed(ControlMap::cueList(show.list)));
    // Nothing selected: nothing armed by it.
    cm->setSelectedTargets({});
    QVERIFY(!cm->isArmed(ControlMap::cue(show.cue1)));
    // Operate: a cue may fire; a hit only where its place is live (Q277), so
    // hit2, whose place is idle here, may not even in Operate.
    m_doc->setMode(Doc::Operate);
    QVERIFY(cm->keyMayFire(ControlMap::cue(show.cue1)));
    QVERIFY(!m_doc->showTree()->isHitArmed(hit2));
    QVERIFY(!cm->keyMayFire(ControlMap::hit(hit2)));
    // Its place goes live (its production becomes the current one): it may.
    QVERIFY(m_doc->showTree()->setCurrentProduction(prod2));
    QVERIFY(m_doc->showTree()->isHitArmed(hit2));
    QVERIFY(cm->keyMayFire(ControlMap::hit(hit2)));
}

/* Saved as Key="…" (no Universe: a build without keys skips it), loaded back. */
void ControlMap_Test::keysAreSavedWithTheShow()
{
    Scene *a = scene("A", 0);
    Scene *b = scene("B", 1);
    ControlMap *cm = m_doc->controlMap();
    const quint32 shift1 = ControlMap::keyChannel(Qt::Key_1, Qt::ShiftModifier);
    cm->setBinding({ ControlMap::KeyboardUniverse, shift1, ControlMap::function(a->id()), ControlMap::Momentary });
    cm->setBinding({ 0, 7, ControlMap::function(b->id()), ControlMap::Toggle });
    const QList<ControlMap::Binding> before = cm->bindings();
    const QByteArray xml = saveShow(m_doc);
    QVERIFY2(xml.contains("<Trigger Key=\"Shift+1\" Target=\"Function\""), xml.constData());

    Doc *other = new Doc(this);
    QVERIFY(loadShow(other, xml));
    QCOMPARE(other->controlMap()->bindings(), before);
    QVERIFY(other->controlMap()->hasKeyBindings());
    delete other;

    // A key this build can't read: left out, the rest loads.
    QByteArray junk = xml;
    junk.replace("Key=\"Shift+1\"", "Key=\"\"");
    Doc *third = new Doc(this);
    QVERIFY(loadShow(third, junk));
    QCOMPARE(third->controlMap()->count(), 1);
    delete third;
}

/*****************************************************************************
 * CS1: the surface safety floor
 *
 * The invariant (board Q269): a MIDI or OSC press starts a function in any
 * mode; a cue list or cue in Operate, and in Design only what is armed; a hit
 * only where its place is live in Operate, and in Design only what is armed
 * (as a computer key). What runs can always be let go. Then every target x
 * mode x state is swept against it, never an example.
 *****************************************************************************/

void ControlMap_Test::sendAll(quint32 channel, const QList<int> &values)
{
    for (int v : values)
        send(channel, uchar(v));
}

namespace
{
struct SurfaceShow
{
    KeyShow k;
    quint32 other = 0, otherCue = 0;    // Act 2 and its cue: not the Run tab's list
    quint32 hitList = 0, hitCue = 0;    // hits placed at Act 2, and at its cue
};

SurfaceShow surfaceShow(Doc *doc)
{
    SurfaceShow s;
    s.k = keyShow(doc);
    ShowTreeController *st = doc->showTree();
    s.other = st->addCueList(s.k.prod, "Act 2");
    s.otherCue = st->addCue(s.other, -1, "Elsewhere");
    ShowTree::Target t;
    t.groups << s.k.group;
    const quint32 full = s.k.palette;
    auto hitAt = [&](ShowTree::HitScope scope, quint32 place, const char *name) {
        const quint32 h = st->addLook(s.k.prod, name);
        st->setLayerLook(h, st->addLayer(h, t), st->lookForPalette(full));
        st->setHitScope({ h }, scope, place);
        return h;
    };
    s.hitList = hitAt(ShowTree::HitScope::List, s.other, "At Act 2");
    s.hitCue = hitAt(ShowTree::HitScope::Cue, s.otherCue, "At its cue");
    return s;
}

/** Whether @a target is doing its thing now (a function runs, a list plays). */
bool going(Doc *doc, const ControlMap::Target &target)
{
    ShowTreeController *st = doc->showTree();
    switch (target.type)
    {
        case ControlMap::FunctionTarget:
            return doc->function(target.id)->isRunning();
        case ControlMap::HitTarget:
            return doc->function(st->cue(target.id)->collection)->isRunning();
        case ControlMap::CueListTarget:
            return doc->function(st->cueList(target.id)->chaser)->isRunning();
        case ControlMap::CueTarget:
            return doc->function(st->listOfCue(target.id)->chaser)->isRunning();
        default:
            break;
    }
    return false;
}
}

void ControlMap_Test::aSurfacePressFollowsTheModeAndTheArming()
{
    sweepTargets(false);
}

void ControlMap_Test::aKeyFollowsTheSurfaceRule()
{
    // Board Q277: a computer key follows the same rule, so a hit on a key
    // fires in Operate only where its place is live (a scene or an effect on a
    // key stays Operate-only: a key is also typing).
    sweepTargets(true);
}

void ControlMap_Test::sweepTargets(bool viaKey)
{
    const SurfaceShow show = surfaceShow(m_doc);
    ShowTreeController *st = m_doc->showTree();
    ControlMap *cm = m_doc->controlMap();
    m_doc->showControl()->setGoGuardMs(0);      // not under test here
    Scene *scn = scene("Warm", 0);
    QCOMPARE(cm->runList(), show.k.list);       // Act 1 is the Run tab's

    enum State { Plain, SelectedSelf, SelectedList, PlaceLive };
    struct Case
    {
        const char *name;
        ControlMap::Target target;
        State state;
        bool design, operate;
    };
    const ControlMap::Target act2 = ControlMap::cueList(show.other);
    const QList<Case> cases = {
        // A scene or an effect: Build tries them in Design, Operate runs them.
        { "scene", ControlMap::function(scn->id()), Plain, true, true },
        // The Run tab's list and its cue: armed by the run list.
        { "run list", ControlMap::cueList(show.k.list), Plain, true, true },
        { "run list cue", ControlMap::cue(show.k.cue2), Plain, true, true },
        // Another list: Operate GOes it; Design only when it is selected.
        { "other list", act2, Plain, false, true },
        { "other list selected", act2, SelectedSelf, true, true },
        { "other cue", ControlMap::cue(show.otherCue), Plain, false, true },
        { "other cue selected", ControlMap::cue(show.otherCue), SelectedSelf, true, true },
        { "other cue, its list selected", ControlMap::cue(show.otherCue), SelectedList, true, true },
        // A hit whose place is the production: live while it is current.
        { "production hit", ControlMap::hit(show.k.hit), Plain, true, true },
        // A hit at Act 2 / at its cue: Act 2 is not playing, so not live.
        // Operate fires only a live place, even a hit that is selected.
        { "list hit, place idle", ControlMap::hit(show.hitList), Plain, false, false },
        { "list hit, selected", ControlMap::hit(show.hitList), SelectedSelf, true, false },
        { "list hit, its list selected", ControlMap::hit(show.hitList), SelectedList, true, false },
        { "list hit, place live", ControlMap::hit(show.hitList), PlaceLive, true, true },
        { "cue hit, place idle", ControlMap::hit(show.hitCue), Plain, false, false },
        { "cue hit, selected", ControlMap::hit(show.hitCue), SelectedSelf, true, false },
        { "cue hit, place live", ControlMap::hit(show.hitCue), PlaceLive, true, true },
    };

    int ran = 0;
    for (const Case &c : cases)
    {
        for (Doc::Mode mode : { Doc::Design, Doc::Operate })
        {
            for (ControlMap::Mode how : { ControlMap::Toggle, ControlMap::StartOnly, ControlMap::Momentary })
            {
                const QString label = QString("%1, %2, %3").arg(c.name)
                    .arg(mode == Doc::Design ? "Design" : "Operate").arg(ControlMap::modeToString(how));
                // A clean stage.
                cm->removeAll();
                for (Function *f : m_doc->functions())
                    f->stop(FunctionParent::master());
                tick(3);
                m_doc->setMode(Doc::Design);
                cm->setRunTabList(ShowTree::invalidId());
                cm->setSelectedTargets({});
                if (c.state == SelectedSelf)
                    cm->setSelectedTargets({ c.target });
                if (c.state == SelectedList)
                    cm->setSelectedTargets({ ControlMap::cueList(show.other) });
                if (c.state == PlaceLive)
                {
                    // Act 2 plays; a cue hit is live while its cue is on.
                    m_doc->function(st->cueList(show.other)->chaser)->start(m_doc->masterTimer(),
                                                                          FunctionParent::master());
                    tick(3);
                    QVERIFY2(st->isHitArmed(c.target.id), qPrintable(label));
                }
                m_doc->setMode(mode);
                tick();

                const quint32 f5 = ControlMap::keyChannel(Qt::Key_F5, Qt::NoModifier);
#ifdef CONTROLMAP_HAS_EMERGENCY_STOP
                QSignalSpy blocked(cm, &ControlMap::blocked);
#endif
                if (viaKey)
                    cm->setBinding({ ControlMap::KeyboardUniverse, f5, c.target, how });
                else
                    cm->setBinding({ 0, 20, c.target, how });
                if (viaKey)
                    cm->handleKey(f5, true);
                else
                    send(20, 127);
                tick(2);
                const bool did = going(m_doc, c.target);
                bool want = mode == Doc::Design ? c.design : c.operate;
                // A scene or an effect on a key: Operate only.
                if (viaKey && c.target.type == ControlMap::FunctionTarget)
                    want = mode == Doc::Operate;
                QVERIFY2(did == want, qPrintable(QString("%1: fired %2, wanted %3").arg(label).arg(did).arg(want)));
#ifdef CONTROLMAP_HAS_EMERGENCY_STOP
                // A press held back says why, on a key too (the footer shows it).
                if (!want && how != ControlMap::StopOnly)
                    QVERIFY2(blocked.count() == 1, qPrintable(label + ": no reason given"));
#endif
                if (viaKey)
                    cm->handleKey(f5, false);
                else
                    send(20, 0);
                tick(3);
                ++ran;
            }
        }
    }
    QCOMPARE(ran, int(cases.size()) * 2 * 3);
}

void ControlMap_Test::aRefusedPressHasNothingToRelease()
{
    const SurfaceShow show = surfaceShow(m_doc);
    ShowTreeController *st = m_doc->showTree();
    ControlMap *cm = m_doc->controlMap();
    m_doc->showControl()->setGoGuardMs(0);
    Chaser *act2 = qobject_cast<Chaser *>(m_doc->function(st->cueList(show.other)->chaser));
    QVERIFY(act2 != nullptr);

    // Design, Act 2 not armed: a "GO while held" is refused, and its release
    // must not stop the list that something else is playing.
    cm->setBinding({ 0, 20, ControlMap::cueList(show.other), ControlMap::Momentary });
    act2->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    QVERIFY(act2->isRunning());
    send(20, 127);
    tick();
    send(20, 0);
    tick(3);
    QVERIFY2(act2->isRunning(), "the release of a refused press stopped the list");
}

void ControlMap_Test::aRefusedPressSaysSo()
{
#ifdef CONTROLMAP_HAS_EMERGENCY_STOP
    const SurfaceShow show = surfaceShow(m_doc);
    ControlMap *cm = m_doc->controlMap();
    m_doc->showControl()->setGoGuardMs(0);
    QSignalSpy blocked(cm, &ControlMap::blocked);

    cm->setBinding({ 0, 20, ControlMap::cueList(show.other), ControlMap::Toggle });
    send(20, 127);
    tick();
    QCOMPARE(blocked.count(), 1);
    const QString why = blocked.last().at(0).toString();
    QVERIFY2(why.contains("Not fired: Cue list: Act 2") && why.contains("armed"), qPrintable(why));
    send(20, 0);

    // Operate, a hit whose place is idle: says its place is not live.
    m_doc->setMode(Doc::Operate);
    cm->setBinding({ 0, 21, ControlMap::hit(show.hitList), ControlMap::Toggle });
    send(21, 127);
    tick(2);
    QCOMPARE(blocked.count(), 2);
    QVERIFY2(blocked.last().at(0).toString().contains("place is not live"),
             qPrintable(blocked.last().at(0).toString()));
    QVERIFY(!going(m_doc, ControlMap::hit(show.hitList)));
    send(21, 0);
#else
    QFAIL("this build does not say why a press was refused");
#endif
}

void ControlMap_Test::whatRunsCanAlwaysBeLetGo()
{
    const SurfaceShow show = surfaceShow(m_doc);
    ShowTreeController *st = m_doc->showTree();
    ControlMap *cm = m_doc->controlMap();
    m_doc->showControl()->setGoGuardMs(0);
    const ControlMap::Target hit = ControlMap::hit(show.hitList);
    Function *act2 = m_doc->function(st->cueList(show.other)->chaser);
    Function *coll = m_doc->function(st->cue(show.hitList)->collection);

    // Live place, Operate: the hit plays. Its place then ends (Act 2 stops):
    // a Toggle press still lets the hit go, and a Stop-only press stops it.
    m_doc->setMode(Doc::Operate);
    act2->start(m_doc->masterTimer(), FunctionParent::master());
    tick(3);
    cm->setBinding({ 0, 20, hit, ControlMap::Toggle });
    send(20, 127);
    tick(2);
    QVERIFY(coll->isRunning());
    send(20, 0);
    act2->stop(FunctionParent::master());
    tick(3);
    QVERIFY(!st->isHitArmed(hit.id));
    QVERIFY(coll->isRunning());                  // a hit completes on its own
    send(20, 127);
    tick(3);
    QVERIFY2(!coll->isRunning(), "a press could not let go of a hit whose place ended");
    send(20, 0);

    coll->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    cm->setBinding({ 0, 20, hit, ControlMap::StopOnly });
    send(20, 127);
    tick(3);
    QVERIFY(!coll->isRunning());
    send(20, 0);
}

void ControlMap_Test::aControllerGoneLetsGoOfWhatItHeld()
{
    // A held "on while held" button, then the controller is unplugged: its
    // release never comes. Whatever it held is let go, and the next press on
    // that control is a press (not swallowed as "still down").
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kNote, ControlMap::function(a->id()), ControlMap::Momentary });
    send(kNote, 127);
    tick(2);
    QVERIFY(a->isRunning());

    InputOutputMap *iom = m_doc->inputOutputMap();
    emit iom->inputDeviceDisconnected(0, m_stub->name(), QString("Fake"));
    tick(3);
    QVERIFY2(!a->isRunning(), "a controller that went away left its held button on");
    emit iom->inputDeviceConnected(0, m_stub->name(), QString("Fake"));
    send(kNote, 127);                           // the replugged button, pressed again
    tick(2);
    QVERIFY2(a->isRunning(), "the press after a replug was swallowed");
    send(kNote, 0);
    tick(3);
    QVERIFY(!a->isRunning());
}

#ifdef CONTROLMAP_HAS_EMERGENCY_STOP
namespace
{
const quint32 kStopNote = 128 + 61;
const quint32 kCC = 20;             // a CC: could be a fader, or a pad that sends CC

bool stopBound(ControlMap *cm)
{
    return cm->binding(ControlMap::emergencyStop()) != nullptr;
}
}
#endif

void ControlMap_Test::theStopIsLearnedOnAButtonWithASecondPress()
{
#ifdef CONTROLMAP_HAS_EMERGENCY_STOP
    ControlMap *cm = m_doc->controlMap();
    QSignalSpy heard(cm, &ControlMap::heard);
    QSignalSpy confirming(cm, &ControlMap::confirming);
    QSignalSpy refused(cm, &ControlMap::refused);

    cm->learn(ControlMap::emergencyStop());
    send(kStopNote, 0);                             // a release alone: nothing
    QCOMPARE(confirming.count(), 0);
    send(kStopNote, 127);                           // the first press asks for a second
    QCOMPARE(confirming.count(), 1);
    QCOMPARE(heard.count(), 0);
    QVERIFY(!stopBound(cm));
    // A held button repeating itself is still the first press.
    sendAll(kStopNote, { 127, 127, 127 });
    QCOMPARE(heard.count(), 0);
    QVERIFY(!stopBound(cm));
    // Another control in between starts over.
    send(kStopNote + 1, 127);
    QCOMPARE(confirming.count(), 2);
    send(kStopNote + 1, 0);
    send(kStopNote, 127);                           // back to the first: fresh again
    QCOMPARE(confirming.count(), 3);
    QVERIFY(!stopBound(cm));
    // Released and pressed again: bound, once.
    send(kStopNote, 0);
    send(kStopNote, 127);
    QCOMPARE(heard.count(), 1);
    QCOMPARE(refused.count(), 0);
    const ControlMap::Binding *b = cm->binding(ControlMap::emergencyStop());
    QVERIFY(b != nullptr);
    QCOMPARE(b->channel, kStopNote);
    QVERIFY(!cm->isListening());
    // The held second press and its release fire nothing.
    QSignalSpy handled(m_doc->showControl(), &ShowControl::commandHandled);
    send(kStopNote, 127);                           // still held
    send(kStopNote, 0);
    tick();
    QCOMPARE(handled.count(), 0);
    QCOMPARE(cm->targetName(ControlMap::emergencyStop()), QString("Emergency stop (ALL OFF)"));
    // listen() (the plain one) still hears on one press.
    cm->listen();
    send(kStopNote + 2, 127);
    QCOMPARE(heard.count(), 2);
#else
    QFAIL("this build has no emergency stop on the Control Map");
#endif
}

void ControlMap_Test::theStopRefusesAFaderWhateverItSends()
{
#ifdef CONTROLMAP_HAS_EMERGENCY_STOP
    ControlMap *cm = m_doc->controlMap();
    QSignalSpy heard(cm, &ControlMap::heard);
    QSignalSpy refused(cm, &ControlMap::refused);
    cm->learn(ControlMap::emergencyStop());

    // A fader moved up, down and up again, through every value: it is never
    // a button, even when the sweep passes through 0 and starts over.
    QList<int> sweep;
    for (int v = 0; v <= 127; v += 3)
        sweep << v;
    for (int v = 127; v >= 0; v -= 3)
        sweep << v;
    sweep << 0 << 0 << 5 << 9 << 127 << 0 << 127;
    sendAll(kCC, sweep);
    QCOMPARE(heard.count(), 0);
    QVERIFY(!stopBound(cm));
    QVERIFY(refused.count() >= 1);
    QVERIFY2(refused.first().at(0).toString().contains("fader"), qPrintable(refused.first().at(0).toString()));
    QVERIFY(cm->isListening());                     // learning goes on for a real button

    // A fader parked at the top that only ever sends 127: no release, so no
    // confirm however many times it repeats.
    cm->learn(ControlMap::emergencyStop());
    sendAll(kCC + 1, { 127, 127, 127, 127 });
    QVERIFY(!stopBound(cm));
    // And a button after all that still learns.
    send(kStopNote, 127);
    send(kStopNote, 0);
    send(kStopNote, 127);
    QVERIFY(stopBound(cm));
    QCOMPARE(heard.count(), 1);
#else
    QFAIL("this build has no emergency stop on the Control Map");
#endif
}

void ControlMap_Test::theStopRefusesAControlTheProfileCallsAFader()
{
#ifdef CONTROLMAP_HAS_EMERGENCY_STOP
    InputOutputMap *iom = m_doc->inputOutputMap();
    QLCInputProfile *prof = new QLCInputProfile();
    prof->setManufacturer("Fake");
    prof->setModel("Desk");
    QLCInputChannel *fader = new QLCInputChannel();
    fader->setType(QLCInputChannel::Slider);
    fader->setName("Fader 1");
    prof->insertChannel(5, fader);
    QLCInputChannel *knob = new QLCInputChannel();
    knob->setType(QLCInputChannel::Encoder);
    prof->insertChannel(6, knob);
    QLCInputChannel *pad = new QLCInputChannel();
    pad->setType(QLCInputChannel::Button);
    prof->insertChannel(7, pad);
    QVERIFY(iom->addProfile(prof));
    QVERIFY(iom->setInputProfile(0, prof->name()));

    ControlMap *cm = m_doc->controlMap();
    QVERIFY(!cm->isButtonControl(0, 5));
    QVERIFY(!cm->isButtonControl(0, 6));
    QVERIFY(cm->isButtonControl(0, 7));
    QVERIFY(cm->isButtonControl(0, 99));            // not in the profile: a button until told otherwise
    QSignalSpy refused(cm, &ControlMap::refused);
    cm->learn(ControlMap::emergencyStop());
    sendAll(5, { 127, 0, 127 });                    // 0/127 only, but the profile says fader
    sendAll(6, { 127, 0, 127 });
    QCOMPARE(refused.count(), 4);               // each press of each, refused
    QVERIFY(!stopBound(cm));
    sendAll(7, { 127, 0, 127 });
    QVERIFY(stopBound(cm));
#else
    QFAIL("this build has no emergency stop on the Control Map");
#endif
}

void ControlMap_Test::everyPressOfTheStopIsAllOff()
{
#ifdef CONTROLMAP_HAS_EMERGENCY_STOP
    const SurfaceShow show = surfaceShow(m_doc);
    ShowTreeController *st = m_doc->showTree();
    ControlMap *cm = m_doc->controlMap();
    cm->setStopDebounceMs(0);                       // each press below is its own
    Scene *a = scene("Warm", 0);
    Function *act1 = m_doc->function(st->cueList(show.k.list)->chaser);
    cm->setBinding({ 0, kStopNote, ControlMap::emergencyStop(), ControlMap::Toggle });
    QSignalSpy handled(m_doc->showControl(), &ShowControl::commandHandled);
    QSignalSpy stopped(m_doc->showControl(), &ShowControl::cueListStopped);

    for (Doc::Mode mode : { Doc::Design, Doc::Operate })
    {
        m_doc->setMode(mode);
        // Not a toggle: the 1st, 2nd and 3rd press are each ALL OFF, never "resume".
        for (int i = 0; i < 3; ++i)
        {
            a->start(m_doc->masterTimer(), FunctionParent::master());
            act1->start(m_doc->masterTimer(), FunctionParent::master());
            tick(2);
            QVERIFY(a->isRunning() && act1->isRunning());
            const int before = handled.count();
            send(kStopNote, 127);
            tick(3);
            QVERIFY2(!a->isRunning() && !act1->isRunning(), "ALL OFF left something running");
            QCOMPARE(handled.count(), before + 1);
            QVERIFY(handled.last().at(0).toString().contains("ALL OFF"));
            QVERIFY(handled.last().at(1).toBool());
            // The release does nothing: nothing started, nothing more handled.
            send(kStopNote, 0);
            tick();
            QCOMPARE(handled.count(), before + 1);
            QVERIFY(!a->isRunning() && !act1->isRunning());
        }
    }
    QVERIFY(stopped.count() >= 6);
    // Any mode the binding says: the stop has one way to act.
    cm->setBinding({ 0, kStopNote, ControlMap::emergencyStop(), ControlMap::Momentary });
    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    send(kStopNote, 127);
    tick(3);
    QVERIFY(!a->isRunning());
    send(kStopNote, 0);
#else
    QFAIL("this build has no emergency stop on the Control Map");
#endif
}

void ControlMap_Test::theStopIsNotHeldByTheDoubleGoGuard()
{
#ifdef CONTROLMAP_HAS_EMERGENCY_STOP
    const SurfaceShow show = surfaceShow(m_doc);
    ShowTreeController *st = m_doc->showTree();
    ControlMap *cm = m_doc->controlMap();
    m_doc->showControl()->setGoGuardMs(1000);
    Function *act1 = m_doc->function(st->cueList(show.k.list)->chaser);
    cm->setBinding({ 0, 30, ControlMap::cueList(show.k.list), ControlMap::Toggle });
    cm->setBinding({ 0, kStopNote, ControlMap::emergencyStop(), ControlMap::Toggle });
    QSignalSpy handled(m_doc->showControl(), &ShowControl::commandHandled);
    m_doc->setMode(Doc::Operate);

    // A GO, and within the guard's second the stop. A Stop of the list would
    // be held ("Second GO held"); ALL OFF must not be.
    send(30, 127);
    send(30, 0);
    tick(2);
    QVERIFY(act1->isRunning());
    send(kStopNote, 127);
    tick(3);
    QVERIFY2(!act1->isRunning(), "the stop was held by the double-GO guard");
    QVERIFY(handled.last().at(0).toString().contains("ALL OFF"));
    QVERIFY(handled.last().at(1).toBool());
    send(kStopNote, 0);
    // And a GO right after the stop is a fresh start, not held by the stop.
    send(30, 127);
    send(30, 0);
    tick(2);
    QVERIFY(act1->isRunning());
#else
    QFAIL("this build has no emergency stop on the Control Map");
#endif
}

void ControlMap_Test::theStopSurvivesAReplugAndRepeats()
{
#ifdef CONTROLMAP_HAS_EMERGENCY_STOP
    ControlMap *cm = m_doc->controlMap();
    Scene *a = scene("Warm", 0);
    cm->setBinding({ 0, kStopNote, ControlMap::emergencyStop(), ControlMap::Toggle });
    QSignalSpy handled(m_doc->showControl(), &ShowControl::commandHandled);

    // A held button repeating itself (some controllers resend a held note):
    // one ALL OFF, not a stream.
    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    sendAll(kStopNote, { 127, 127, 127, 127, 127 });
    tick(3);
    QCOMPARE(handled.count(), 1);
    QVERIFY(!a->isRunning());

    // Unplugged while held: the release never arrives. Replugged, the next
    // press is a press -- it is not "still down".
    InputOutputMap *iom = m_doc->inputOutputMap();
    emit iom->inputDeviceDisconnected(0, m_stub->name(), QString("Fake"));
    emit iom->inputDeviceConnected(0, m_stub->name(), QString("Fake"));
    cm->setStopDebounceMs(0);
    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    send(kStopNote, 127);
    tick(3);
    QCOMPARE(handled.count(), 2);
    QVERIFY2(!a->isRunning(), "the press after a replug did not stop the show");

    // Stuck down with no event at all (a silent USB reset): still a press.
    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    send(kStopNote, 127);
    tick(3);
    QCOMPARE(handled.count(), 3);
    QVERIFY(!a->isRunning());
#else
    QFAIL("this build has no emergency stop on the Control Map");
#endif
}

void ControlMap_Test::aStopOnAProfileFaderStillFires()
{
#ifdef CONTROLMAP_HAS_EMERGENCY_STOP
    // A show file (or a profile edited later) that puts the stop on a control
    // the profile calls a fader: it still fires (board Q283), flagged.
    InputOutputMap *iom = m_doc->inputOutputMap();
    QLCInputProfile *prof = new QLCInputProfile();
    prof->setManufacturer("Fake");
    prof->setModel("Faders");
    QLCInputChannel *fader = new QLCInputChannel();
    fader->setType(QLCInputChannel::Slider);
    prof->insertChannel(5, fader);
    QVERIFY(iom->addProfile(prof));
    QVERIFY(iom->setInputProfile(0, prof->name()));

    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 5, ControlMap::emergencyStop(), ControlMap::Toggle });
    Scene *a = scene("Warm", 0);
    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    QSignalSpy handled(m_doc->showControl(), &ShowControl::commandHandled);
    QSignalSpy blocked(cm, &ControlMap::blocked);
    sendAll(5, { 0, 10, 40, 127, 127, 0, 127 });
    tick(3);
    QVERIFY2(!a->isRunning(), "a fader stop did not fire");
    QVERIFY(handled.count() >= 1);
    QVERIFY(cm->stopHasProblem());
    QVERIFY2(blocked.count() >= 1 && blocked.first().at(0).toString().contains("fader"),
             "the fader warning was not said");
#else
    QFAIL("this build has no emergency stop on the Control Map");
#endif
}

void ControlMap_Test::aRefusedAllOffDoesNotHoldBackTheNext()
{
    // The second's wait starts when ALL OFF ran, not when it was refused.
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kStopNote, ControlMap::emergencyStop(), ControlMap::Toggle });
    QSignalSpy handled(m_doc->showControl(), &ShowControl::commandHandled);
    QSignalSpy fired(cm, &ControlMap::fired);
    // Make run() refuse: a handler that says no.
    m_doc->showControl()->setRunHandler([](const ShowControl::Request &, ShowControl::Route) { return false; });
    send(kStopNote, 127);
    QCOMPARE(fired.count(), 0);
    m_doc->showControl()->setRunHandler(nullptr);
    send(kStopNote, 127);                           // no 0 between, well inside a second
    QCOMPARE(fired.count(), 1);
}

void ControlMap_Test::aStopWithNoIdLoadsAsTheStop()
{
    const QByteArray xml =
        "<ControlMap><Trigger Universe=\"0\" Channel=\"40\" Target=\"EmergencyStop\"/></ControlMap>";
    QXmlStreamReader r(xml);
    QVERIFY(r.readNextStartElement());              // ControlMap
    QVERIFY(m_doc->controlMap()->loadXML(r));
    QVERIFY(m_doc->controlMap()->binding(ControlMap::emergencyStop()) != nullptr);
}

void ControlMap_Test::theStopIsSavedWithTheShow()
{
#ifdef CONTROLMAP_HAS_EMERGENCY_STOP
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kStopNote, ControlMap::emergencyStop(), ControlMap::Toggle });
    cm->setBinding({ 0, 9, ControlMap::function(scene("Warm", 0)->id()), ControlMap::Toggle });
    const QList<ControlMap::Binding> before = cm->bindings();
    const QByteArray xml = saveShow(m_doc);
    QVERIFY2(xml.contains("Target=\"EmergencyStop\""), xml.constData());
    Doc *other = new Doc(this);
    QVERIFY(loadShow(other, xml));
    QCOMPARE(other->controlMap()->bindings(), before);
    QVERIFY(other->controlMap()->binding(ControlMap::emergencyStop()) != nullptr);
    delete other;
    // One stop: binding it again elsewhere moves it.
    cm->setBinding({ 0, kStopNote + 5, ControlMap::emergencyStop(), ControlMap::Toggle });
    QCOMPARE(cm->count(), 2);
    QCOMPARE(cm->binding(ControlMap::emergencyStop())->channel, kStopNote + 5);
#else
    QFAIL("this build has no emergency stop on the Control Map");
#endif
}

/*****************************************************************************
 * CS1 review fixes
 *****************************************************************************/

namespace
{
/** Whether the stop is bound to a control that cannot work it. */
bool stopFlagged(ControlMap *cm)
{
    return cm->stopHasProblem();
}
}

void ControlMap_Test::theStopWorksWhileALearnIsListening()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kStopNote, ControlMap::emergencyStop(), ControlMap::Toggle });
    cm->setStopDebounceMs(0);
    // An Encoder learn left listening (its panel closed): the stop still stops.
    cm->listen();
    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    send(kStopNote, 127);
    tick(3);
    QVERIFY2(!a->isRunning(), "a learn left listening ate the emergency stop");
    QVERIFY(cm->isListening());                     // and it did not become a choice
    send(kStopNote, 0);

    // A Learn Trigger dialog open (it listens again after each press): the
    // same, and the stop's own control is never offered as a choice.
    QSignalSpy heard(cm, &ControlMap::heard);
    cm->listen();
    send(5, 127);
    QCOMPARE(heard.count(), 1);
    send(5, 0);
    cm->listen();
    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    send(kStopNote, 127);
    tick(3);
    QVERIFY2(!a->isRunning(), "an open Learn Trigger dialog ate the emergency stop");
    QCOMPARE(heard.count(), 1);
    send(kStopNote, 0);
}

void ControlMap_Test::aRealSecondPressIsNotDebounced()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kStopNote, ControlMap::emergencyStop(), ControlMap::Toggle });
    QSignalSpy handled(m_doc->showControl(), &ShowControl::commandHandled);
    // Stop, something else starts, stop again: all inside 250 ms.
    for (int i = 0; i < 3; ++i)
    {
        a->start(m_doc->masterTimer(), FunctionParent::master());
        tick(2);
        send(kStopNote, 127);
        tick(3);
        QVERIFY2(!a->isRunning(), qPrintable(QString("press %1 did not stop").arg(i + 1)));
        send(kStopNote, 0);
    }
    QCOMPARE(handled.count(), 3);
}

void ControlMap_Test::aStopOnAProfileKnobFiresAndSaysSo()
{
    // Learned on a CC with no profile; later a profile types the channel a
    // knob: every press used to return in silence.
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 7, ControlMap::emergencyStop(), ControlMap::Toggle });
    QVERIFY(!stopFlagged(cm));
    InputOutputMap *iom = m_doc->inputOutputMap();
    QLCInputProfile *prof = new QLCInputProfile();
    prof->setManufacturer("Fake");
    prof->setModel("Late");
    QLCInputChannel *knob = new QLCInputChannel();
    knob->setType(QLCInputChannel::Knob);
    prof->insertChannel(7, knob);
    QVERIFY(iom->addProfile(prof));
    QVERIFY(iom->setInputProfile(0, prof->name()));
    QVERIFY(stopFlagged(cm));
    QSignalSpy blocked(cm, &ControlMap::blocked);
    QSignalSpy handled(m_doc->showControl(), &ShowControl::commandHandled);
    send(7, 127);
    QCOMPARE(blocked.count(), 1);
    QCOMPARE(handled.count(), 1);                   // ALL OFF ran (board Q283)
    QVERIFY2(blocked.last().at(0).toString().contains("fader or knob"), qPrintable(blocked.last().at(0).toString()));
    // Under the press rule: the held press repeating is not a second ALL OFF,
    // a press after a 0 is.
    send(7, 127);
    QCOMPARE(handled.count(), 1);
    send(7, 0);
    send(7, 127);
    QCOMPARE(handled.count(), 2);
}

void ControlMap_Test::aBoundFaderIsFlaggedLoudlyAndStillStops()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 7, ControlMap::emergencyStop(), ControlMap::Toggle });
    QSignalSpy handled(m_doc->showControl(), &ShowControl::commandHandled);
    QSignalSpy blocked(cm, &ControlMap::blocked);
#ifdef CONTROLMAP_STOP_FLAGS
    QSignalSpy state(cm, &ControlMap::stopStateChanged);
#endif
    // A show file put the stop on a CC fader the profile does not know: a
    // fast sweep, 100 messages, is one press and one footer line.
    QList<int> sweep;
    for (int i = 0; i < 100; ++i)
        sweep << (i < 50 ? i * 2 + 1 : 200 - i * 2 + 1) % 127 + 1;
    sendAll(7, sweep);
    QCOMPARE(handled.count(), 1);
    QCOMPARE(blocked.count(), 1);
    QVERIFY2(blocked.first().at(0).toString().contains("moves like a fader"), qPrintable(blocked.first().at(0).toString()));
    QVERIFY(cm->stopHasProblem());
#ifdef CONTROLMAP_STOP_FLAGS
    QCOMPARE(state.count(), 1);
    QCOMPARE(cm->stopProblem(), QString("moves like a fader"));
#endif
    // The flag outlives an unrelated learn and removal (the panel keeps it red).
    Scene *b = scene("Cool", 1);
    cm->setBinding({ 0, 9, ControlMap::function(b->id()), ControlMap::Toggle });
    cm->removeBinding(ControlMap::function(b->id()));
    QVERIFY(cm->stopHasProblem());
    // The fader at rest (a 0), then moved: it stops the show again, never lost.
    send(7, 0);
    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    send(7, 30);
    tick(3);
    QVERIFY(!a->isRunning());
    QCOMPARE(handled.count(), 2);
    QCOMPARE(blocked.count(), 1);                   // said once, not on every move
    // Bound again (on a button): the flag goes.
    cm->setBinding({ 0, 8, ControlMap::emergencyStop(), ControlMap::Toggle });
    QVERIFY(!cm->stopHasProblem());
}

void ControlMap_Test::theStopFiresPerThePressRule()
{
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 7, ControlMap::emergencyStop(), ControlMap::Toggle });
    QSignalSpy handled(m_doc->showControl(), &ShowControl::commandHandled);
    struct Row
    {
        const char *what;
        QList<int> values;
        int gapMs;          // 1000: the default; 0: every message is "after a rest"
        int delayMs;        // real time between the messages
        int fires;
        bool flagged;
    };
    const QList<Row> rows = {
        { "a button, held and repeating", { 127, 127, 127, 127, 127, 0 }, 1000, 0, 1, false },
        { "a velocity pad", { 127, 0, 90, 0, 60, 0 }, 1000, 0, 3, false },
        { "a lost release then a softer press, quickly", { 100, 80, 0, 100 }, 1000, 0, 2, true },
        { "a lost release, a softer press after a rest", { 100, 80 }, 150, 200, 2, false },
        { "a lost release, the same value after a rest", { 100, 100 }, 0, 0, 2, false },
        { "a relative encoder ticking, never 0", { 1, 1, 1, 127, 1, 127 }, 1000, 0, 1, true },
        { "a slow fader, each step after a rest", { 10, 11, 12 }, 150, 200, 3, false },
        { "a fast fader run", { 5, 20, 60, 127, 60, 5 }, 1000, 0, 1, true },
        { "a fader that rests at 0 between moves", { 0, 10, 20, 0, 0, 10, 0 }, 1000, 0, 2, true },
    };
    for (const Row &r : rows)
    {
        cm->setBinding({ 0, 7, ControlMap::emergencyStop(), ControlMap::Toggle });   // a fresh start
        cm->handleInput(0, 7, 0);
        InputOutputMap *iom = m_doc->inputOutputMap();
        emit iom->inputDeviceConnected(0, m_stub->name(), QString("Fake"));          // first message after a replug
        cm->setStopDebounceMs(r.gapMs);
        const int before = handled.count();
        for (int v : r.values)
        {
            send(7, uchar(v));
            if (r.delayMs > 0)
                QTest::qWait(r.delayMs);
        }
        QVERIFY2(handled.count() - before == r.fires,
                 qPrintable(QString("%1: %2 fires, wanted %3").arg(r.what).arg(handled.count() - before).arg(r.fires)));
        QVERIFY2(cm->stopHasProblem() == r.flagged, qPrintable(QString("%1: flagged %2").arg(r.what).arg(cm->stopHasProblem())));
        // Never lost: after a 0 the next press fires.
        send(7, 0);
        const int mid = handled.count();
        send(7, 100);
        QVERIFY2(handled.count() == mid + 1, qPrintable(QString("%1: the next press was lost").arg(r.what)));
        send(7, 0);
        cm->removeBinding(ControlMap::emergencyStop());
    }
}

void ControlMap_Test::aDeviceThatDropsItsZerosStillStops()
{
    // 127 every 0.6 s for 3 s and no 0 ever: the stop is sent about once a
    // second, never once and then dead.
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 7, ControlMap::emergencyStop(), ControlMap::Toggle });
    QSignalSpy handled(m_doc->showControl(), &ShowControl::commandHandled);
    for (int i = 0; i < 6; ++i)
    {
        a->start(m_doc->masterTimer(), FunctionParent::master());
        tick(2);
        send(7, 127);
        tick(2);
        if (i < 5)
            QTest::qWait(600);
    }
    QVERIFY2(handled.count() >= 3, qPrintable(QString("only %1 ALL OFF in 3 s").arg(handled.count())));
    QVERIFY(!cm->stopHasProblem());                     // the same value: not a fader
}

void ControlMap_Test::aStopWithAnotherIdLoadsAsTheOneStop()
{
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 9, ControlMap::emergencyStop(), ControlMap::Toggle });
    QByteArray xml = saveShow(m_doc);
    QVERIFY2(xml.contains("Target=\"EmergencyStop\""), xml.constData());
    // A file with the id 5, and a second stop with id 6 on another control.
    QByteArray odd = xml;
    odd.replace("Target=\"EmergencyStop\" ID=\"0\"", "Target=\"EmergencyStop\" ID=\"5\"");
    QVERIFY(odd != xml);
    Doc *other = new Doc(this);
    QVERIFY(loadShow(other, odd));
    QCOMPARE(other->controlMap()->count(), 1);
    const ControlMap::Binding *b = other->controlMap()->binding(ControlMap::emergencyStop());
    QVERIFY2(b != nullptr, "a stop with another id was loaded as a second stop the panel cannot see");
    QCOMPARE(b->channel, 9u);
    delete other;
    QByteArray two = odd;
    two.replace("</ControlMap>", "<Trigger Universe=\"0\" Channel=\"11\" Target=\"EmergencyStop\" ID=\"6\"/></ControlMap>");
    Doc *third = new Doc(this);
    QVERIFY(loadShow(third, two));
    QCOMPARE(third->controlMap()->count(), 1);      // one stop
    delete third;
}

void ControlMap_Test::aSecondValueAfterTheLearnPressEndsTheSwallow()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->learn(ControlMap::emergencyStop());
    sendAll(kStopNote, { 127, 0, 127 });            // bound; the release of this press is lost
    QVERIFY(cm->binding(ControlMap::emergencyStop()) != nullptr);
    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    send(kStopNote, 127);                           // may be the hold repeating
    send(kStopNote, 127);                           // a second value: a real press
    tick(3);
    QVERIFY2(!a->isRunning(), "real presses in the first second were swallowed");
}

void ControlMap_Test::aLearnTakesAHeldControlAndLetsGoOfIt()
{
    Scene *a = scene("Warm", 0);
    Scene *b = scene("Cool", 1);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 20, ControlMap::function(a->id()), ControlMap::Momentary });
    send(20, 127);
    tick(2);
    QVERIFY(a->isRunning());
    cm->learn(ControlMap::function(b->id()));
    send(20, 127);                                  // the held control is the one learned
    tick(3);
    QVERIFY2(!a->isRunning(), "the learn left what the control held running");
    send(20, 0);
    tick(2);
    QVERIFY(!a->isRunning());
    send(20, 127);                                  // B's first press is a press
    tick(2);
    QVERIFY2(b->isRunning(), "the first press after the learn was eaten");
    send(20, 0);
}

void ControlMap_Test::aResetLetsGoOfTheInput()
{
#ifdef CONTROLMAP_HAS_INPUT_PROBE
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 20, ControlMap::function(a->id()), ControlMap::Momentary });
    send(20, 127);
    cm->removeBinding(ControlMap::function(a->id()));
    QVERIFY(cm->isInputConnected());                // held: its release is still coming
    cm->reset();                                    // a new show
    QVERIFY2(!cm->isInputConnected(), "input stayed connected after reset with nothing bound");
#else
    QFAIL("this build cannot say whether input is connected");
#endif
}

void ControlMap_Test::aLostReleaseAtLearnTimeDoesNotSwallowTheStop()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->learn(ControlMap::emergencyStop());
    sendAll(kStopNote, { 127, 0, 127 });            // bound; the release of this press is lost
    QVERIFY(cm->binding(ControlMap::emergencyStop()) != nullptr);
    QTest::qWait(1100);
    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    send(kStopNote, 127);                           // the first real press, no replug
    tick(3);
    QVERIFY2(!a->isRunning(), "a lost release at learn time swallowed the first real stop press");
}

void ControlMap_Test::aBindingRemovedWhileHeldStillLetsGo()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 20, ControlMap::function(a->id()), ControlMap::Momentary });
    send(20, 127);
    tick(2);
    QVERIFY(a->isRunning());
    cm->removeBinding(ControlMap::function(a->id()));
    send(20, 0);
    tick(3);
    QVERIFY2(!a->isRunning(), "a scene held by a button stayed on after its binding was removed");
    // Remove All the same.
    cm->setBinding({ 0, 20, ControlMap::function(a->id()), ControlMap::Momentary });
    send(20, 127);
    tick(2);
    cm->removeAll();
    send(20, 0);
    tick(3);
    QVERIFY(!a->isRunning());
}

void ControlMap_Test::aStopOnAKeyDoesNothingAndSaysSo()
{
    ControlMap *cm = m_doc->controlMap();
    const quint32 f9 = ControlMap::keyChannel(Qt::Key_F9, Qt::NoModifier);
    cm->setBinding({ ControlMap::KeyboardUniverse, f9, ControlMap::emergencyStop(), ControlMap::Toggle });
    QVERIFY(cm->stopHasProblem());
    QSignalSpy blocked(cm, &ControlMap::blocked);
    QSignalSpy handled(m_doc->showControl(), &ShowControl::commandHandled);
    for (int i = 0; i < 20; ++i)                    // held: auto-repeat
        QVERIFY(!cm->handleKey(f9, true));
    QCOMPARE(blocked.count(), 1);
    QVERIFY(blocked.last().at(0).toString().contains("computer key"));
    QCOMPARE(handled.count(), 0);
    cm->handleKey(f9, false);
    cm->handleKey(f9, true);
    QCOMPARE(blocked.count(), 2);                   // a new press says it again
}

void ControlMap_Test::aHeldRefusedPressSaysItOnce()
{
    const SurfaceShow show = surfaceShow(m_doc);
    ControlMap *cm = m_doc->controlMap();
    const quint32 key = ControlMap::keyChannel(Qt::Key_F7, Qt::NoModifier);
    cm->setBinding({ ControlMap::KeyboardUniverse, key, ControlMap::cueList(show.other), ControlMap::Toggle });
    QSignalSpy blocked(cm, &ControlMap::blocked);
    for (int i = 0; i < 30; ++i)                    // a key held: auto-repeat
        QVERIFY(!cm->handleKey(key, true));
    QCOMPARE(blocked.count(), 1);
    cm->handleKey(key, false);
    cm->handleKey(key, true);
    QCOMPARE(blocked.count(), 2);                   // a new press says it again
}

void ControlMap_Test::aReplugFreesThePressThatTaughtTheStop()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    cm->learn(ControlMap::emergencyStop());
    sendAll(kStopNote, { 127, 0, 127 });            // bound; this press is still held
    QVERIFY(cm->binding(ControlMap::emergencyStop()) != nullptr);
    InputOutputMap *iom = m_doc->inputOutputMap();
    emit iom->inputDeviceDisconnected(0, m_stub->name(), QString("Fake"));   // its release never comes
    emit iom->inputDeviceConnected(0, m_stub->name(), QString("Fake"));
    a->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    cm->setStopDebounceMs(0);
    send(kStopNote, 127);
    tick(3);
    QVERIFY2(!a->isRunning(), "the first press after a replug was swallowed");
}

void ControlMap_Test::aReleaseActsOnWhatWasPressed()
{
    Scene *a = scene("Warm", 0);
    ControlMap *cm = m_doc->controlMap();
    m_doc->showControl()->setGoGuardMs(0);
    // The mode changes while the button is held: the release still lets go.
    cm->setBinding({ 0, 20, ControlMap::function(a->id()), ControlMap::Momentary });
    send(20, 127);
    tick(2);
    QVERIFY(a->isRunning());
    cm->setMode(ControlMap::function(a->id()), ControlMap::StartOnly);
    send(20, 0);
    tick(3);
    QVERIFY2(!a->isRunning(), "a mode change while held left the scene on");

    // The control is bound to something else while held: the release is the
    // scene's, and a list that runs for another reason is not stopped.
    const SurfaceShow show = surfaceShow(m_doc);
    Function *act1 = m_doc->function(m_doc->showTree()->cueList(show.k.list)->chaser);
    cm->setMode(ControlMap::function(a->id()), ControlMap::Momentary);
    send(20, 127);
    tick(2);
    QVERIFY(a->isRunning());
    act1->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    cm->setBinding({ 0, 20, ControlMap::cueList(show.k.list), ControlMap::Momentary });
    send(20, 0);
    tick(3);
    QVERIFY2(!a->isRunning(), "the release did not let go of what the press started");
    QVERIFY2(act1->isRunning(), "the release stopped a list the press never started");
}

QTEST_GUILESS_MAIN(ControlMap_Test)
