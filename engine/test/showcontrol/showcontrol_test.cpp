/*
  Q Light Controller Plus
  showcontrol_test.cpp

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
#include <QtEndian>
#include <QtTest>
#include <cstring>

#define private public
#include "mastertimer.h"
#undef private
#include "inputoutputmap.h"
#include "functionparent.h"
#include "showcontrol.h"
#include "chaserstep.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

#include "showcontrol_test.h"

namespace
{
QByteArray msc(int device, int format, int command, const QByteArray &data)
{
    QByteArray b;
    b.append(char(0xF0)).append(char(0x7F)).append(char(device)).append(char(0x02))
     .append(char(format)).append(char(command)).append(data).append(char(0xF7));
    return b;
}

void oscPad(QByteArray &b)
{
    do
        b.append('\0');
    while (b.size() % 4 != 0);
}

/** An OSC message: @a address with no arguments, or one int, float or
    string. */
QByteArray osc(const QString &address, const QVariant &arg = QVariant())
{
    QByteArray b = address.toUtf8();
    oscPad(b);
    if (!arg.isValid())
    {
        b.append(',');
        oscPad(b);
        return b;
    }
    QByteArray value;
    char tag;
    if (arg.userType() == QMetaType::Int)
    {
        tag = 'i';
        value.resize(4);
        qToBigEndian<qint32>(arg.toInt(), reinterpret_cast<uchar *>(value.data()));
    }
    else if (arg.userType() == QMetaType::Float || arg.userType() == QMetaType::Double)
    {
        tag = 'f';
        const float f = arg.toFloat();
        quint32 raw;
        std::memcpy(&raw, &f, sizeof raw);
        value.resize(4);
        qToBigEndian<quint32>(raw, reinterpret_cast<uchar *>(value.data()));
    }
    else
    {
        tag = 's';
        value = arg.toString().toUtf8();
        oscPad(value);
    }
    b.append(',').append(tag);
    oscPad(b);
    return b + value;
}

QByteArray bundle(const QList<QByteArray> &messages)
{
    QByteArray b("#bundle");
    b.append('\0');
    b.append(QByteArray(8, '\0'));
    b[15] = 1;                              // "immediately"
    for (const QByteArray &m : messages)
    {
        QByteArray size(4, '\0');
        qToBigEndian<qint32>(m.size(), reinterpret_cast<uchar *>(size.data()));
        b += size + m;
    }
    return b;
}
}

void ShowControl_Test::init()
{
    m_doc = new Doc(this);
}

void ShowControl_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

Chaser *ShowControl_Test::makeList(int steps, const QString &name)
{
    Chaser *c = new Chaser(m_doc);
    c->setName(name);
    // Steps that hold until the next GO.
    c->setDurationMode(Chaser::Common);
    c->setDuration(Function::infiniteSpeed());
    m_doc->addFunction(c);
    for (int i = 0; i < steps; ++i)
    {
        Scene *s = new Scene(m_doc);
        s->setName(QString("%1 step %2").arg(name).arg(i + 1));
        m_doc->addFunction(s);
        c->addStep(ChaserStep(s->id()));
    }
    return c;
}

void ShowControl_Test::aStepsCueIsItsNumberOrItsPosition()
{
    Chaser *c = makeList(3, "Main");
    QCOMPARE(c->stepCue(0), QString("1"));
    QCOMPARE(c->stepCue(2), QString("3"));
    QCOMPARE(c->stepCue(3), QString());

    ChaserStep step = c->steps().at(1);
    step.cueNumber = "12.5";
    c->replaceStep(step, 1);
    QCOMPARE(c->stepCue(1), QString("12.5"));
    QCOMPARE(c->findCue("12.5"), 1);
    QCOMPARE(c->findCue("2"), -1);            // it has a number of its own now
    QCOMPARE(c->findCue("3"), 2);

    c->renumberCues();
    QCOMPARE(c->steps().at(1).cueNumber, QString("2"));
    QCOMPARE(c->findCue("2"), 1);
}

void ShowControl_Test::cuesMatchAsNumbersOrAsText()
{
    QVERIFY(Chaser::cuesMatch("12.5", "12.50"));
    QVERIFY(Chaser::cuesMatch("7", " 7.0 "));
    QVERIFY(!Chaser::cuesMatch("1.1", "1.10.1"));
    QVERIFY(Chaser::cuesMatch("Intro", "intro"));
    QVERIFY(!Chaser::cuesMatch("12", "12a"));
    QVERIFY(!Chaser::cuesMatch("", ""));
}

void ShowControl_Test::cueNumbersAndTheMainListAreSaved()
{
    Chaser *c = makeList(2, "Main");
    c->setCueListNumber("2");
    ChaserStep step = c->steps().at(0);
    step.cueNumber = "0.5";
    c->replaceStep(step, 0);
    m_doc->showControl()->setMainCueList(c->id());

    QByteArray xml;
    {
        QXmlStreamWriter w(&xml);
        w.writeStartDocument();
        w.writeStartElement("Test");
        QVERIFY(c->saveXML(&w));
        QVERIFY(m_doc->showControl()->saveXML(&w));
        w.writeEndElement();
        w.writeEndDocument();
    }
    QVERIFY(xml.contains("CueList=\"2\""));
    QVERIFY(xml.contains("Cue=\"0.5\""));

    Chaser loaded(m_doc);
    m_doc->showControl()->reset();
    QCOMPARE(m_doc->showControl()->mainCueList(), quint32(UINT_MAX));
    QXmlStreamReader r(xml);
    r.readNextStartElement();               // Test
    r.readNextStartElement();               // Function
    QVERIFY(loaded.loadXML(r));
    r.readNextStartElement();               // ShowControl
    QVERIFY(m_doc->showControl()->loadXML(r));
    QCOMPARE(loaded.cueListNumber(), QString("2"));
    QCOMPARE(loaded.steps().at(0).cueNumber, QString("0.5"));
    QCOMPARE(loaded.stepCue(1), QString("2"));
    QCOMPARE(m_doc->showControl()->mainCueList(), c->id());

    // A step's cue rides along when a step is copied (editors work on copies).
    const ChaserStep copy = ChaserStep::fromVariant(loaded.steps().at(0).toVariant());
    QCOMPARE(copy.cueNumber, QString("0.5"));
}

void ShowControl_Test::aCommandFindsItsList()
{
    ShowControl *sc = m_doc->showControl();
    QCOMPARE(sc->cueList(QString()), (Chaser *)nullptr);

    Chaser *a = makeList(2, "A");
    QCOMPARE(sc->cueList(QString()), a);      // the only one: no setup needed

    Chaser *b = makeList(2, "B");
    QCOMPARE(sc->cueList(QString()), (Chaser *)nullptr);   // two: which?
    sc->setMainCueList(b->id());
    QCOMPARE(sc->cueList(QString()), b);

    a->setCueListNumber("3");
    QCOMPARE(sc->cueList("3"), a);
    QCOMPARE(sc->cueList("3.0"), a);
    QCOMPARE(sc->cueList("4"), (Chaser *)nullptr);
}

void ShowControl_Test::goPlaysTheCueAndThenTheNext()
{
    MasterTimer *timer = m_doc->masterTimer();
    Chaser *c = makeList(4, "Main");
    ChaserStep step = c->steps().at(2);
    step.cueNumber = "12.5";
    c->replaceStep(step, 2);
    ShowControl *sc = m_doc->showControl();
    QSignalSpy said(sc, &ShowControl::commandHandled);

    QVERIFY(sc->execute({ ShowControl::Go, QString(), QString() }));
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
    QVERIFY(c->isRunning());
    QCOMPARE(c->currentStepIndex(), 0);     // from stopped: the first
    QCOMPARE(said.last().at(1).toBool(), true);
    QVERIFY(said.last().at(0).toString().contains("GO cue 1"));

    QVERIFY(sc->execute({ ShowControl::Go, "12.5", QString() }));
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
    QCOMPARE(c->currentStepIndex(), 2);

    QVERIFY(sc->execute({ ShowControl::Go, QString(), QString() }));
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
    QCOMPARE(c->currentStepIndex(), 3);     // running: the next

    c->stop(FunctionParent::master());
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
}

void ShowControl_Test::loadPutsACueOnStandby()
{
    MasterTimer *timer = m_doc->masterTimer();
    Chaser *c = makeList(4, "Main");
    ShowControl *sc = m_doc->showControl();

    QVERIFY(sc->execute({ ShowControl::Load, "3", QString() }));
    QCOMPARE(sc->loadedStep(c->id()), 2);
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
    QVERIFY(!c->isRunning());               // LOAD doesn't fire

    QVERIFY(sc->execute({ ShowControl::Go, QString(), QString() }));
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
    QCOMPARE(c->currentStepIndex(), 2);
    QCOMPARE(sc->loadedStep(c->id()), -1);  // used up

    QVERIFY(!sc->execute({ ShowControl::Load, QString(), QString() }));   // a cue, please

    c->stop(FunctionParent::master());
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
}

void ShowControl_Test::pauseResumeStopAndAllOff()
{
    MasterTimer *timer = m_doc->masterTimer();
    Chaser *c = makeList(3, "Main");
    ShowControl *sc = m_doc->showControl();

    QVERIFY(!sc->execute({ ShowControl::Pause, QString(), QString() }));  // not playing
    QVERIFY(sc->execute({ ShowControl::Go, "2", QString() }));
    for (int i = 0; i < 3; ++i)
        timer->timerTick();

    QVERIFY(sc->execute({ ShowControl::Pause, QString(), QString() }));
    QVERIFY(c->isPaused());
    QVERIFY(sc->execute({ ShowControl::Resume, QString(), QString() }));
    QVERIFY(!c->isPaused());

    QVERIFY(!sc->execute({ ShowControl::Stop, "3", QString() }));   // not the one playing
    QVERIFY(c->isRunning());
    QVERIFY(sc->execute({ ShowControl::Stop, "2", QString() }));
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
    QVERIFY(!c->isRunning());

    QVERIFY(sc->execute({ ShowControl::Go, QString(), QString() }));
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
    QVERIFY(c->isRunning());
    QVERIFY(sc->execute({ ShowControl::AllOff, QString(), QString() }));
    for (int i = 0; i < 5; ++i)
        timer->timerTick();
    QVERIFY(!c->isRunning());
}

void ShowControl_Test::aCueThatIsntThereDoesNothing()
{
    MasterTimer *timer = m_doc->masterTimer();
    Chaser *c = makeList(3, "Main");
    makeList(2, "Other");
    ShowControl *sc = m_doc->showControl();
    QSignalSpy said(sc, &ShowControl::commandHandled);

    // Two lists and no main one.
    QVERIFY(!sc->execute({ ShowControl::Go, "1", QString() }));
    QVERIFY(said.last().at(0).toString().contains("no main cue list"));

    sc->setMainCueList(c->id());
    QVERIFY(!sc->execute({ ShowControl::Go, "9", QString() }));
    QVERIFY(said.last().at(0).toString().contains("no such cue"));
    QVERIFY(!sc->execute({ ShowControl::Go, "1", "7" }));
    QVERIFY(said.last().at(0).toString().contains("no cue list 7"));
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
    QVERIFY(!c->isRunning());
}

void ShowControl_Test::mscIsDecoded()
{
    ShowControl::Request r;

    QVERIFY(ShowControl::parseMsc(msc(1, 0x01, 0x01, QByteArray("12.5\0" "2", 6)), 1, &r));
    QCOMPARE(r.command, ShowControl::Go);
    QCOMPARE(r.cue, QString("12.5"));
    QCOMPARE(r.list, QString("2"));

    QVERIFY(ShowControl::parseMsc(msc(1, 0x01, 0x01, QByteArray()), 1, &r));
    QCOMPARE(r.cue, QString());               // GO with no cue: the next

    QVERIFY(ShowControl::parseMsc(msc(1, 0x7F, 0x02, QByteArray()), 1, &r));
    QCOMPARE(r.command, ShowControl::Pause);  // MSC STOP halts; RESUME goes on
    QVERIFY(ShowControl::parseMsc(msc(1, 0x01, 0x03, QByteArray()), 1, &r));
    QCOMPARE(r.command, ShowControl::Resume);
    QVERIFY(ShowControl::parseMsc(msc(1, 0x02, 0x05, QByteArray("4", 1)), 1, &r));
    QCOMPARE(r.command, ShowControl::Load);
    QCOMPARE(r.cue, QString("4"));
    QVERIFY(ShowControl::parseMsc(msc(1, 0x01, 0x08, QByteArray()), 1, &r));
    QCOMPARE(r.command, ShowControl::AllOff);
    QVERIFY(ShowControl::parseMsc(msc(1, 0x01, 0x0A, QByteArray()), 1, &r));
    QCOMPARE(r.command, ShowControl::Reset);
    QVERIFY(ShowControl::parseMsc(msc(1, 0x01, 0x0B, QByteArray("3", 1)), 1, &r));
    QCOMPARE(r.command, ShowControl::Stop);   // GO_OFF

    // TIMED_GO: the time comes first, and is ignored.
    QVERIFY(ShowControl::parseMsc(msc(1, 0x01, 0x04, QByteArray("\x00\x00\x05\x00\x00" "7", 6)), 1, &r));
    QCOMPARE(r.command, ShowControl::Go);
    QCOMPARE(r.cue, QString("7"));

    // Without its F0 / F7.
    QVERIFY(ShowControl::parseMsc(msc(1, 0x01, 0x01, "8").mid(1).chopped(1), 1, &r));
    QCOMPARE(r.cue, QString("8"));

    // Not ours to carry out: a sound command, an unknown command, not MSC.
    QVERIFY(!ShowControl::parseMsc(msc(1, 0x10, 0x01, "1"), 1, &r));
    QVERIFY(!ShowControl::parseMsc(msc(1, 0x01, 0x06, "1"), 1, &r));     // SET
    QVERIFY(!ShowControl::parseMsc(QByteArray("\xF0\x7F\x01\x01\x01\x00\x00\x00\x00\xF7", 10), 1, &r)); // MTC
    QVERIFY(!ShowControl::parseMsc(QByteArray(), 1, &r));
}

void ShowControl_Test::mscIsOnlyForUs()
{
    ShowControl::Request r;
    QVERIFY(ShowControl::parseMsc(msc(5, 0x01, 0x01, "1"), 5, &r));
    QVERIFY(!ShowControl::parseMsc(msc(6, 0x01, 0x01, "1"), 5, &r));     // another device
    QVERIFY(ShowControl::parseMsc(msc(0x7F, 0x01, 0x01, "1"), 5, &r));   // all-call
    QVERIFY(ShowControl::parseMsc(msc(0x72, 0x01, 0x01, "1"), 5, &r));   // a group
}

void ShowControl_Test::mscFromAPatchedInputFiresTheCue()
{
    MasterTimer *timer = m_doc->masterTimer();
    Chaser *c = makeList(3, "Main");
    ShowControl *sc = m_doc->showControl();

    // As a patched MIDI input hands it on (Doc wires the map to us).
    emit m_doc->inputOutputMap()->inputSysExReceived(0, msc(1, 0x01, 0x01, "2"));
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
    QVERIFY(c->isRunning());
    QCOMPARE(c->currentStepIndex(), 1);

    // Turned off, it's ignored.
    sc->setMscEnabled(false);
    emit m_doc->inputOutputMap()->inputSysExReceived(0, msc(1, 0x01, 0x01, "3"));
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
    QCOMPARE(c->currentStepIndex(), 1);

    // Said to be MSC, from the universe it came in on; GO pressed here isn't.
    sc->setMscEnabled(true);
    QSignalSpy from(sc, &ShowControl::commandFrom);
    emit m_doc->inputOutputMap()->inputSysExReceived(0, msc(1, 0x01, 0x01, "3"));
    sc->execute({ ShowControl::Go, QString(), QString() });
    QCOMPARE(from.count(), 2);
    QCOMPARE(from.at(0).at(0).value<ShowControl::Source>(), ShowControl::Msc);
    QCOMPARE(from.at(0).at(1).toUInt(), 0u);
    QCOMPARE(from.at(1).at(0).value<ShowControl::Source>(), ShowControl::Local);
    QCOMPARE(from.at(1).at(1).toUInt(), UINT_MAX);
    for (int i = 0; i < 3; ++i)
        timer->timerTick();

    c->stop(FunctionParent::master());
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
}

void ShowControl_Test::oscIsDecoded()
{
    auto one = [](const QByteArray &d) {
        const QList<ShowControl::Request> l = ShowControl::parseOsc(d);
        return l.count() == 1 ? l.first() : ShowControl::Request{ ShowControl::Reset, "none", "none" };
    };

    // QLab's own: /go with no argument.
    ShowControl::Request r = one(osc("/go"));
    QCOMPARE(r.command, ShowControl::Go);
    QCOMPARE(r.cue, QString());
    QCOMPARE(r.list, QString());

    QCOMPARE(one(osc("/go", 12.5f)).cue, QString("12.5"));
    QCOMPARE(one(osc("/go", 7)).cue, QString("7"));
    QCOMPARE(one(osc("/go", QString("A1"))).cue, QString("A1"));
    QCOMPARE(one(osc("/go/3")).cue, QString("3"));
    QCOMPARE(one(osc("/cue/4.5/start")).cue, QString("4.5"));
    QCOMPARE(one(osc("/cue/4/GO")).command, ShowControl::Go);
    QCOMPARE(one(osc("/cue/4/load")).command, ShowControl::Load);
    QCOMPARE(one(osc("/load", 4)).cue, QString("4"));
    QCOMPARE(one(osc("/cue/4/stop")).command, ShowControl::Stop);

    r = one(osc("/qlcconsole/list/2/go/5"));
    QCOMPARE(r.command, ShowControl::Go);
    QCOMPARE(r.list, QString("2"));
    QCOMPARE(r.cue, QString("5"));

    QCOMPARE(one(osc("/pause", 1.0f)).cue, QString());    // a button's 1 isn't a cue
    QCOMPARE(one(osc("/resume")).command, ShowControl::Resume);
    QCOMPARE(one(osc("/stop")).command, ShowControl::Stop);
    QCOMPARE(one(osc("/reset")).command, ShowControl::Reset);
    QCOMPARE(one(osc("/panic")).command, ShowControl::AllOff);
    QCOMPARE(one(osc("/alloff")).command, ShowControl::AllOff);

    QVERIFY(ShowControl::parseOsc(osc("/fader/1", 0.5f)).isEmpty());
    QVERIFY(ShowControl::parseOsc(osc("/cue/4/explode")).isEmpty());
    QVERIFY(ShowControl::parseOsc(QByteArray("junk")).isEmpty());
    QVERIFY(ShowControl::parseOsc(QByteArray()).isEmpty());

    const QList<ShowControl::Request> both =
        ShowControl::parseOsc(bundle({ osc("/load", 2), osc("/go") }));
    QCOMPARE(both.count(), 2);
    QCOMPARE(both.at(0).command, ShowControl::Load);
    QCOMPARE(both.at(1).command, ShowControl::Go);
}

void ShowControl_Test::oscFiresTheCue()
{
    MasterTimer *timer = m_doc->masterTimer();
    Chaser *c = makeList(3, "Main");
    Chaser *other = makeList(3, "Other");
    other->setCueListNumber("2");
    m_doc->showControl()->setMainCueList(c->id());

    QSignalSpy from(m_doc->showControl(), &ShowControl::commandFrom);
    QCOMPARE(m_doc->showControl()->handleOsc(osc("/cue/3/start")), 1);
    QCOMPARE(m_doc->showControl()->handleOsc(osc("/list/2/go")), 1);
    QCOMPARE(from.count(), 2);
    QCOMPARE(from.at(0).at(0).value<ShowControl::Source>(), ShowControl::Osc);
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
    QCOMPARE(c->currentStepIndex(), 2);
    QVERIFY(other->isRunning());
    QCOMPARE(other->currentStepIndex(), 0);

    QCOMPARE(m_doc->showControl()->handleOsc(osc("/panic")), 1);
    for (int i = 0; i < 5; ++i)
        timer->timerTick();
    QVERIFY(!c->isRunning());
    QVERIFY(!other->isRunning());
}

void ShowControl_Test::removingTheMainListForgetsIt()
{
    Chaser *c = makeList(1, "Main");
    makeList(1, "Other");
    m_doc->showControl()->setMainCueList(c->id());
    QVERIFY(m_doc->deleteFunction(c->id()));
    QCOMPARE(m_doc->showControl()->mainCueList(), quint32(UINT_MAX));
}

/* A skipped step: GO, Next and Back pass over it; a jump straight to it
   (GO cue 2) still plays it. Saved with the show. */
void ShowControl_Test::skippedStepsArePassedOver()
{
    MasterTimer *timer = m_doc->masterTimer();
    Chaser *c = makeList(5, "Main");
    ShowControl *sc = m_doc->showControl();
    c->setStepSkipped(0, true);
    c->setStepSkipped(2, true);
    QVERIFY(c->isStepSkipped(2));
    QCOMPARE(c->firstPlayableStep(), 1);

    auto tick = [timer]() { for (int i = 0; i < 3; ++i) timer->timerTick(); };

    QVERIFY(sc->execute({ ShowControl::Go, QString(), QString() }));
    tick();
    QCOMPARE(c->currentStepIndex(), 1);         // 1 is skipped
    QVERIFY(sc->execute({ ShowControl::Go, QString(), QString() }));
    tick();
    QCOMPARE(c->currentStepIndex(), 3);         // past 3
    QVERIFY(sc->execute({ ShowControl::Back, QString(), QString() }));
    tick();
    QCOMPARE(c->currentStepIndex(), 1);         // back past it too
    QVERIFY(sc->execute({ ShowControl::Go, "3", QString() }));
    tick();
    QCOMPARE(c->currentStepIndex(), 2);         // asked for by number: played

    // Every step skipped: GO from stopped has nothing to play.
    c->stop(FunctionParent::master());
    tick();
    for (int i = 0; i < c->stepsCount(); ++i)
        c->setStepSkipped(i, true);
    QCOMPARE(c->firstPlayableStep(), -1);
    QVERIFY(!sc->execute({ ShowControl::Go, QString(), QString() }));

    // Saved, and copied with the step.
    c->setStepSkipped(1, false);
    QByteArray xml;
    {
        QXmlStreamWriter w(&xml);
        QVERIFY(c->saveXML(&w));
    }
    QVERIFY(xml.contains("Skip=\"1\""));
    Chaser loaded(m_doc);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(loaded.loadXML(r));
    QVERIFY(loaded.isStepSkipped(0));
    QVERIFY(!loaded.isStepSkipped(1));
    QVERIFY(ChaserStep::fromVariant(loaded.steps().at(0).toVariant()).skip);
}

/* Steps that run on by themselves (a hold) pass over a skipped one too. */
void ShowControl_Test::autoFollowPassesOverSkippedSteps()
{
    MasterTimer *timer = m_doc->masterTimer();
    Chaser *c = makeList(4, "Main");
    c->setDuration(MasterTimer::tick() * 3);     // each step: three ticks
    c->setRunOrder(Function::SingleShot);
    c->setStepSkipped(1, true);

    QList<int> played;
    c->start(timer, FunctionParent::master());
    for (int i = 0; i < 40; ++i)
    {
        timer->timerTick();
        const int now = c->currentStepIndex();
        if (c->isRunning() && now >= 0 && (played.isEmpty() || played.last() != now))
            played << now;
    }
    QCOMPARE(played, QList<int>({ 0, 2, 3 }));
}

/* What GO plays next: the first not skipped; then the next; a LOAD wins. */
void ShowControl_Test::standbyIsWhatGoPlaysNext()
{
    MasterTimer *timer = m_doc->masterTimer();
    Chaser *c = makeList(4, "Main");
    ShowControl *sc = m_doc->showControl();
    c->setStepSkipped(0, true);
    auto tick = [timer]() { for (int i = 0; i < 3; ++i) timer->timerTick(); };

    QCOMPARE(sc->standbyStep(c), 1);
    QVERIFY(sc->execute({ ShowControl::Go, QString(), QString() }));
    tick();
    QCOMPARE(sc->standbyStep(c), 2);
    QVERIFY(sc->execute({ ShowControl::Load, "4", QString() }));
    QCOMPARE(sc->standbyStep(c), 3);
    QVERIFY(sc->execute({ ShowControl::Go, QString(), QString() }));
    tick();
    QCOMPARE(c->currentStepIndex(), 3);
    QCOMPARE(sc->standbyStep(c), 1);            // looping round, past the skipped one

    QCOMPARE(ShowControl::parseOsc(QByteArray("/back\0\0\0,\0\0\0", 12)).value(0).command,
             ShowControl::Back);

    c->stop(FunctionParent::master());
    tick();
}

QTEST_MAIN(ShowControl_Test)
