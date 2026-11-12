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
#include <QSignalSpy>
#include <QBuffer>
#include <QScopeGuard>
#include <QtTest>

#include "../common/stdprelude.h"
#define private public
#include "mastertimer.h"
#undef private
#include "controlmap_test.h"
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
    QCOMPARE(m_stub->m_feedback.last(), qMakePair(kNote, uchar(0)));   // off: it isn't running

    send(kNote, 127);
    tick();
    QCOMPARE(m_stub->m_feedback.last(), qMakePair(kNote, uchar(255)));
    send(kNote, 0);
    send(kNote, 127);
    tick(3);
    QCOMPARE(m_stub->m_feedback.last(), qMakePair(kNote, uchar(0)));

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

    QCOMPARE(routes.count(), 7);   // every GO and Stop the controls above sent
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
    QCOMPARE(st->runningCue(listId), cues.at(1));
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
    QCOMPARE(st->runningCue(listId), cues.at(1));
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
    QCOMPARE(m_stub->m_feedback.last(), qMakePair(quint32(20), uchar(0)));
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
    quint32 prod, list, cue1, cue2, hit;
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
    // Operate: everything may fire.
    m_doc->setMode(Doc::Operate);
    QVERIFY(cm->keyMayFire(ControlMap::cue(show.cue1)));
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

QTEST_GUILESS_MAIN(ControlMap_Test)
