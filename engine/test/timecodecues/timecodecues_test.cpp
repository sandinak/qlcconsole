/*
  Q Light Controller Plus - Test Unit
  timecodecues_test.cpp

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

#include <QtTest>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "timecodecues.h"
#include "timecodesource.h"
#include "showtreecontroller.h"
#include "showcontrol.h"
#include "chaser.h"
#include "doc.h"
#include "timecodecues_test.h"

using namespace ShowTree;

void TimecodeCues_Test::init()
{
    m_doc = new Doc(this);
    m_calls.clear();
    // Every run() lands here, in order; the engine's own handler is none.
    m_doc->showControl()->setRunHandler([this](const ShowControl::Request &r, ShowControl::Route route) {
        m_calls << Call{ r.cue, r.chaser, int(route), int(r.command) };
        return m_doc->showControl()->execute(r);
    });
    m_doc->timecodeSource();        // makes TimecodeCues
}

void TimecodeCues_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

quint32 TimecodeCues_Test::makeList(int cues, const QList<int> &timesMs, bool follow)
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->productions().isEmpty() ? st->addProduction("P") : st->productions().first().id;
    const quint32 list = st->addCueList(prod, "L");
    m_cues.clear();
    for (int i = 0; i < cues; ++i)
    {
        m_cues << st->addCue(list, i - 1, QString("C%1").arg(i + 1));
        if (i < timesMs.size() && timesMs.at(i) >= 0)
            st->setCueTimecode(m_cues.last(), timesMs.at(i));
    }
    if (follow)
        st->setListFollowTimecode(list, true);
    return list;
}

void TimecodeCues_Test::feed(quint32 ms)
{
    m_doc->timecodeSource()->updateTimeCode(0, ms, 30);
}

void TimecodeCues_Test::firesEachCueAtItsTime()
{
    const quint32 list = makeList(3, { 1000, 2000, 3000 });
    const quint32 ch = m_doc->showTree()->cueList(list)->chaser;
    const QString n1 = m_doc->showTree()->cueNumber(*m_doc->showTree()->cueList(list), 0);
    const QString n2 = m_doc->showTree()->cueNumber(*m_doc->showTree()->cueList(list), 1);
    const QString n3 = m_doc->showTree()->cueNumber(*m_doc->showTree()->cueList(list), 2);

    feed(0);
    feed(500);
    QVERIFY(m_calls.isEmpty());
    feed(1000);                                 // exactly on its time
    QCOMPARE(m_calls.size(), 1);
    QCOMPARE(m_calls.at(0).cue, n1);
    feed(1500);
    QCOMPARE(m_calls.size(), 1);                // nothing passed
    feed(2040);
    feed(3000);
    QCOMPARE(m_calls.size(), 3);
    QCOMPARE(m_calls.at(1).cue, n2);
    QCOMPARE(m_calls.at(2).cue, n3);
    for (const Call &c : m_calls)
        QCOMPARE(c.chaser, ch);
    feed(9000);                                 // past the last: nothing more
    QCOMPARE(m_calls.size(), 3);
}

void TimecodeCues_Test::goesThroughTheOneGoPath()
{
    const quint32 list = makeList(2, { 1000, 2000 });
    const quint32 ch = m_doc->showTree()->cueList(list)->chaser;
    feed(0);
    feed(1000);
    QCOMPARE(m_calls.size(), 1);
    QCOMPARE(m_calls.at(0).route, int(ShowControl::RouteTimecode));
    QCOMPARE(m_calls.at(0).command, int(ShowControl::Go));
    QCOMPARE(m_calls.at(0).chaser, ch);
    QCOMPARE(ShowControl::routeName(ShowControl::RouteTimecode), QStringLiteral("timecode"));
    QVERIFY(!ShowControl::isConsoleRoute(ShowControl::RouteTimecode));
    // And it really went on: the chaser is on that cue.
    Chaser *c = qobject_cast<Chaser *>(m_doc->function(ch));
    QVERIFY(c != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(c->currentStepIndex(), 0, 2000);
}

void TimecodeCues_Test::aJumpForwardFiresOnlyTheLatest()
{
    const quint32 list = makeList(4, { 1000, 2000, 3000, 4000 });
    const ShowTreeController *st = m_doc->showTree();
    feed(0);
    feed(3500);                                 // passed 1, 2 and 3
    QCOMPARE(m_calls.size(), 1);
    QCOMPARE(m_calls.at(0).cue, st->cueNumber(*st->cueList(list), 2));
    feed(4000);
    QCOMPARE(m_calls.size(), 2);
    QCOMPARE(m_calls.at(1).cue, st->cueNumber(*st->cueList(list), 3));
}

void TimecodeCues_Test::aJumpBackRearms()
{
    const quint32 list = makeList(3, { 1000, 2000, 3000 });
    const ShowTreeController *st = m_doc->showTree();
    feed(0);
    feed(3000);
    QCOMPARE(m_calls.size(), 1);                // the latest: cue 3
    feed(500);                                  // locate back: nothing fires for the jump
    QCOMPARE(m_calls.size(), 1);
    feed(1000);                                 // and playing on fires them again
    QCOMPARE(m_calls.size(), 2);
    QCOMPARE(m_calls.at(1).cue, st->cueNumber(*st->cueList(list), 0));
    feed(2000);
    feed(3000);
    QCOMPARE(m_calls.size(), 4);
    QCOMPARE(m_calls.at(3).cue, st->cueNumber(*st->cueList(list), 2));
}

void TimecodeCues_Test::jitterIsNotAJumpBack()
{
    makeList(1, { 1000 });
    feed(0);
    feed(1010);
    QCOMPARE(m_calls.size(), 1);
    feed(900);                                  // a hiccup, under kJitterMs
    feed(1020);
    QCOMPARE(m_calls.size(), 1);                // no second fire
}

void TimecodeCues_Test::aDropoutHolds()
{
    makeList(3, { 1000, 2000, 3000 });
    feed(0);
    feed(1000);
    QCOMPARE(m_calls.size(), 1);
    // No timecode: the watchdog flips it to not running, and nothing fires.
    TimecodeSource *tc = m_doc->timecodeSource();
    QTRY_VERIFY_WITH_TIMEOUT(!tc->isRunning(), 3000);
    QCOMPARE(m_calls.size(), 1);
    // It comes back where it was: the next cue at its time, the first not again.
    feed(1010);
    QCOMPARE(m_calls.size(), 1);
    feed(2000);
    QCOMPARE(m_calls.size(), 2);
}

void TimecodeCues_Test::cuesWithNoTimeAreSkipped()
{
    const quint32 list = makeList(3, { 1000, -1, 3000 });
    const ShowTreeController *st = m_doc->showTree();
    feed(0);
    feed(5000);                                 // the latest with a time: cue 3
    QCOMPARE(m_calls.size(), 1);
    QCOMPARE(m_calls.at(0).cue, st->cueNumber(*st->cueList(list), 2));
    // A skipped cue with a time is left alone as well.
    m_calls.clear();
    QVERIFY(m_doc->showTree()->setCuesSkipped({ m_cues[2] }, true));
    feed(0);
    feed(4000);
    QCOMPARE(m_calls.size(), 1);
    QCOMPARE(m_calls.at(0).cue, st->cueNumber(*st->cueList(list), 0));
}

void TimecodeCues_Test::aManualGoBetweenTimecodeCuesChangesNothing()
{
    const quint32 list = makeList(4, { 1000, -1, 3000, -1 });
    const ShowTreeController *st = m_doc->showTree();
    const quint32 chId = st->cueList(list)->chaser;
    ShowControl *sc = m_doc->showControl();
    feed(0);
    feed(1000);                                 // cue 1 by timecode
    QCOMPARE(m_calls.size(), 1);
    ShowControl::Request go;                    // GO by hand: cue 2 (no time)
    go.command = ShowControl::Go;
    go.chaser = chId;
    QVERIFY(sc->run(go, ShowControl::RouteKey));
    QCOMPARE(m_calls.size(), 2);
    QCOMPARE(int(m_calls.at(1).route), int(ShowControl::RouteKey));
    feed(3000);                                 // cue 3 still goes at its time, by number
    QCOMPARE(m_calls.size(), 3);
    QCOMPARE(int(m_calls.at(2).route), int(ShowControl::RouteTimecode));
    QCOMPARE(m_calls.at(2).cue, st->cueNumber(*st->cueList(list), 2));
    Chaser *c = qobject_cast<Chaser *>(m_doc->function(chId));
    QTRY_COMPARE_WITH_TIMEOUT(c->currentStepIndex(), 2, 2000);
}

void TimecodeCues_Test::onlyListsThatFollowFire()
{
    makeList(2, { 1000, 2000 }, false);
    feed(0);
    feed(2500);
    QVERIFY(m_calls.isEmpty());
    QVERIFY(!m_doc->timecodeCues()->anyFollowing());
}

void TimecodeCues_Test::nextCueAndCountdown()
{
    const quint32 list = makeList(3, { 1000, 2000, 3000 });
    const ShowTreeController *st = m_doc->showTree();
    const quint32 ch = st->cueList(list)->chaser;
    TimecodeCues *tcc = m_doc->timecodeCues();
    QVERIFY(tcc->anyFollowing());
    QString cue;
    qint32 ms = 0;
    feed(1500);
    QVERIFY(tcc->next(ch, &cue, &ms));
    QCOMPARE(ms, 2000);
    QCOMPARE(cue, st->cueNumber(*st->cueList(list), 1));
    feed(3000);
    QVERIFY(!tcc->next(ch, &cue, &ms));
    QVERIFY(!tcc->next(ch + 1000, &cue, &ms));
}

void TimecodeCues_Test::parseAndFormat()
{
    qint32 ms = 0;
    QVERIFY(TimecodeCues::parse("00:01:23:10", 30, &ms));
    QCOMPARE(TimecodeCues::format(ms, 30), QString("00:01:23:10"));
    QVERIFY(TimecodeCues::parse("01:02:03:04", 25, &ms));
    QCOMPARE(ms, ((1 * 60 + 2) * 60 + 3) * 1000 + 160);
    QCOMPARE(TimecodeCues::format(ms, 25), QString("01:02:03:04"));
    QVERIFY(TimecodeCues::parse("1:23:10", 30, &ms));
    QCOMPARE(TimecodeCues::format(ms, 30), QString("00:01:23:10"));
    QVERIFY(TimecodeCues::parse("00;00;05;00", 30, &ms));
    QCOMPARE(ms, 5000);
    for (const char *bad : { "", "abc", "1", "00:61:00:00", "00:00:00:30", "-1:00:00:00", "1:2:3:4:5" })
        QVERIFY2(!TimecodeCues::parse(QString(bad), 30, &ms), bad);
    for (int fps : { 24, 25, 30 })
        for (int f = 0; f < fps; ++f)
        {
            qint32 v = 0;
            const QString text = QString("00:00:07:%1").arg(f, 2, 10, QChar('0'));
            QVERIFY(TimecodeCues::parse(text, fps, &v));
            QCOMPARE(TimecodeCues::format(v, fps), text);
        }
}

namespace
{
QByteArray engineXml(Doc *doc)
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.writeStartDocument();
    doc->saveXML(&w);
    w.writeEndDocument();
    return xml;
}
}

void TimecodeCues_Test::saveAndLoad()
{
    const quint32 list = makeList(3, { 1000, -1, 83333 });
    const QByteArray xml = engineXml(m_doc);
    QVERIFY(xml.contains("FollowTimecode=\"1\""));
    QVERIFY(xml.contains("Timecode=\"83333\""));

    Doc *other = new Doc(this);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(other->loadXML(r, false));
    const CueList *l = other->showTree()->cueList(list);
    QVERIFY(l != nullptr);
    QVERIFY(l->followTimecode);
    QCOMPARE(l->cues.size(), 3);
    QCOMPARE(l->cues.at(0).timecode, 1000);
    QCOMPARE(l->cues.at(1).timecode, -1);
    QCOMPARE(l->cues.at(2).timecode, 83333);
    delete other;

    // Undo takes a time away again.
    QVERIFY(m_doc->showTree()->setCueTimecode(m_cues[1], 500));
    QVERIFY(!m_doc->showTree()->setCueTimecode(m_cues[1], 500));    // same: no step
    QVERIFY(m_doc->showTree()->undo());
    QCOMPARE(m_doc->showTree()->cue(m_cues[1])->timecode, -1);
}

void TimecodeCues_Test::anOldShowFollowsNothing()
{
    makeList(2, { -1, -1 }, false);
    const QByteArray xml = engineXml(m_doc);
    QVERIFY(!xml.contains("FollowTimecode"));
    QVERIFY(!xml.contains(" Timecode=\"") || !xml.contains("<Cue "));
    feed(0);
    feed(5000);
    QVERIFY(m_calls.isEmpty());
}

QTEST_MAIN(TimecodeCues_Test)
