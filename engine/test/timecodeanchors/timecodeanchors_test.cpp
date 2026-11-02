/*
  Q Light Controller Plus - Test Unit
  timecodeanchors_test.cpp

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
#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "timecodeanchors.h"
#include "operationregistry.h"
#include "showtreecontroller.h"
#include "timecodesource.h"
#include "timecodecues.h"
#include "showcontrol.h"
#include "midimarkers.h"
#include "doc.h"
#include "timecodeanchors_test.h"

using namespace ShowTree;

namespace
{

QByteArray vlq(quint32 n)
{
    QByteArray out;
    out.prepend(char(n & 0x7F));
    while ((n >>= 7) != 0)
        out.prepend(char((n & 0x7F) | 0x80));
    return out;
}

QByteArray meta(quint32 delta, int type, const QByteArray &data)
{
    return vlq(delta) + char(0xFF) + char(type) + vlq(quint32(data.size())) + data;
}

QByteArray be(quint32 v, int bytes)
{
    QByteArray out;
    for (int i = bytes - 1; i >= 0; --i)
        out += char((v >> (8 * i)) & 0xFF);
    return out;
}

QByteArray midiFile(quint16 division, const QByteArray &trackEvents)
{
    const QByteArray body = trackEvents + vlq(0) + char(0xFF) + char(0x2F) + char(0);
    return QByteArray("MThd") + be(6, 4) + be(0, 2) + be(1, 2) + be(division, 2)
           + QByteArray("MTrk") + be(quint32(body.size()), 4) + body;
}

} // namespace

void TimecodeAnchors_Test::init()
{
    m_doc = new Doc(this);
    m_doc->showControl()->setRunHandler([this](const ShowControl::Request &r, ShowControl::Route) {
        return m_doc->showControl()->execute(r);
    });
    m_cues.clear();
}

void TimecodeAnchors_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

quint32 TimecodeAnchors_Test::makeShow(const QStringList &numbers, const QStringList &names)
{
    ShowTreeController *st = m_doc->showTree();
    m_prod = st->addProduction("Show");
    const quint32 list = st->addCueList(m_prod, "Show");
    st->setNumbering(m_prod, Numbering::ShowPartIteration);
    m_cues.clear();
    for (int i = 0; i < numbers.count(); ++i)
    {
        const quint32 id = st->addCue(list, i - 1, QStringLiteral("C%1").arg(i));
        Cue c = *st->cue(id);
        c.number = numbers.at(i);
        QString why;
        if (!st->setCue(c, &why))
            qWarning() << "number" << numbers.at(i) << why;
        m_cues << id;
    }
    const QList<Section> secs = st->sections(list);
    for (int i = 0; i < names.count() && i < secs.count(); ++i)
        st->setSectionName(list, secs.at(i), names.at(i));
    st->setListFollowTimecode(list, true);
    st->clearUndo();
    return list;
}

int TimecodeAnchors_Test::sectionIndex(quint32 list, const QString &name) const
{
    const QList<Section> secs = m_doc->showTree()->sections(list);
    for (int i = 0; i < secs.count(); ++i)
        if (secs.at(i).name == name)
            return i;
    return -1;
}

qint32 TimecodeAnchors_Test::timeOf(int cue) const
{
    const Cue *c = m_doc->showTree()->cue(m_cues.at(cue));
    return c == nullptr ? -2 : c->timecode;
}

/****************************************************************************
 * The anchor resolve
 ****************************************************************************/

void TimecodeAnchors_Test::resolveCountsFromTheSectionStart()
{
    QCOMPARE(resolveTimecode(10000, 2500), qint32(12500));
    QCOMPARE(resolveTimecode(1000, -2500), qint32(0));          // never before 0

    CueList l;
    l.id = 1;
    const QStringList numbers { "1", "1.1", "2", "2.1", "2.2" };
    for (int i = 0; i < numbers.count(); ++i)
    {
        Cue c;
        c.id = quint32(10 + i);
        c.number = numbers.at(i);
        l.cues << c;
    }
    SectionHead h1, h2;
    h1.key = "1";
    h1.start = 10000;
    h2.key = "2";
    h2.start = 60000;
    l.sections << h1 << h2;
    l.cues[1].timecode = 0;
    l.cues[1].tcFromSection = true;
    l.cues[1].tcOffset = 4000;
    l.cues[3].timecode = 0;
    l.cues[3].tcFromSection = true;
    l.cues[3].tcOffset = 1500;
    l.cues[4].timecode = 99000;                 // a time of its own

    QVERIFY(resolveTimecodes(l, Numbering::ShowPartIteration));
    QCOMPARE(l.cues.at(1).timecode, qint32(14000));
    QCOMPARE(l.cues.at(3).timecode, qint32(61500));
    QCOMPARE(l.cues.at(4).timecode, qint32(99000));
    QVERIFY(!resolveTimecodes(l, Numbering::ShowPartIteration));     // nothing more to do

    // Shifting section 2: its cue follows, the time of its own stays.
    QVERIFY(setSectionStart(l, Numbering::ShowPartIteration, 1, 66000, false));
    QCOMPARE(l.cues.at(3).timecode, qint32(67500));
    QCOMPARE(l.cues.at(3).tcOffset, qint32(1500));
    QCOMPARE(l.cues.at(4).timecode, qint32(99000));
    QCOMPARE(l.cues.at(1).timecode, qint32(14000));
    QVERIFY(!setSectionStart(l, Numbering::ShowPartIteration, 1, 66000, false));   // the same
    QVERIFY(!setSectionStart(l, Numbering::ShowPartIteration, 7, 1, false));       // no such section

    // Clearing a start: its cues keep their times, as their own.
    QVERIFY(setSectionStart(l, Numbering::ShowPartIteration, 1, -1, false));
    QCOMPARE(l.cues.at(3).timecode, qint32(67500));
    QVERIFY(!l.cues.at(3).tcFromSection);

    // Under no scheme there are no sections: nothing moves.
    CueList none = l;
    QCOMPARE(sectionsOf(none, Numbering::None).count(), 0);
}

void TimecodeAnchors_Test::anAnchorCueTimesTheCuesAfterIt()
{
    const quint32 list = makeShow({ "1", "1.1", "1.2", "1.3", "2" }, { "Verse", "Chorus" });
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setSectionStart(list, st->sections(list).at(0), 10000));
    QVERIFY(st->setCueTimecode(m_cues.at(1), 15000));
    QVERIFY(st->setCueTimecode(m_cues.at(2), 20000));
    QVERIFY(st->setCueTimecode(m_cues.at(3), 22000));
    QVERIFY(st->setCueTimecodeAnchor(m_cues.at(2), true));
    QVERIFY(st->isCueTimecodeAnchor(m_cues.at(2)));
    // Nothing moved; 1.3 counts from the anchor now.
    QCOMPARE(timeOf(3), qint32(22000));
    QCOMPARE(st->cue(m_cues.at(3))->tcOffset, qint32(2000));
    QString from;
    QVERIFY(st->cueTimecodeBase(m_cues.at(3), &from));
    QCOMPARE(from, QStringLiteral("cue 1.2"));
    QVERIFY(st->cueTimecodeBase(m_cues.at(1), &from));
    QCOMPARE(from, QStringLiteral("1 — Verse"));

    // Moving the anchor moves the cue after it, not the one before.
    QVERIFY(st->setCueTimecode(m_cues.at(2), 21000));
    QCOMPARE(timeOf(3), qint32(23000));
    QCOMPARE(timeOf(1), qint32(15000));

    const QList<TimecodeAnchor> anchors = st->timecodeAnchors(list);
    QCOMPARE(anchors.count(), 2);
    QCOMPARE(anchors.at(0).kind, TimecodeAnchor::SectionStart);
    QCOMPARE(anchors.at(0).ms, qint32(10000));
    QCOMPARE(anchors.at(0).follow, 2);                  // 1.1 and the anchor itself
    QCOMPARE(anchors.at(1).kind, TimecodeAnchor::CueAnchor);
    QCOMPARE(anchors.at(1).cue, m_cues.at(2));
    QCOMPARE(anchors.at(1).ms, qint32(21000));
    QCOMPARE(anchors.at(1).follow, 1);
}

void TimecodeAnchors_Test::movingASectionMovesItsCues()
{
    const quint32 list = makeShow({ "1", "1.1", "2", "2.1", "2.2", "3", "3.1" }, { "Intro", "Chorus 2", "Outro" });
    ShowTreeController *st = m_doc->showTree();
    QList<Section> secs = st->sections(list);
    QVERIFY(st->setSectionStart(list, secs.at(0), 0));
    QVERIFY(st->setSectionStart(list, secs.at(1), 30000));
    QVERIFY(st->setSectionStart(list, secs.at(2), 60000));
    QVERIFY(st->setCueTimecode(m_cues.at(2), 30000));
    QVERIFY(st->setCueTimecode(m_cues.at(3), 34000));
    QVERIFY(st->setCueTimecode(m_cues.at(4), 40000));
    QVERIFY(st->setCueTimecode(m_cues.at(6), 62000));
    QVERIFY(st->cue(m_cues.at(3))->tcFromSection);
    QCOMPARE(st->cue(m_cues.at(3))->tcOffset, qint32(4000));
    QCOMPARE(st->sectionStart(list, st->sections(list).at(1)), qint32(30000));

    // One number changes; the section's cues follow, the others don't.
    st->clearUndo();
    QVERIFY(st->setSectionStart(list, st->sections(list).at(1), 36000));
    QCOMPARE(timeOf(2), qint32(36000));
    QCOMPARE(timeOf(3), qint32(40000));
    QCOMPARE(timeOf(4), qint32(46000));
    QCOMPARE(timeOf(6), qint32(62000));
    QCOMPARE(st->undoText(), QStringLiteral("Set section start"));
    QVERIFY(st->undo());
    QCOMPARE(timeOf(3), qint32(34000));
    QCOMPARE(st->sectionStart(list, st->sections(list).at(1)), qint32(30000));
    QVERIFY(!st->canUndo());
}

void TimecodeAnchors_Test::laterSectionsFollowWhenAsked()
{
    const quint32 list = makeShow({ "1", "1.1", "2", "2.1", "3", "3.1", "3.2" }, { "A", "B", "C" });
    ShowTreeController *st = m_doc->showTree();
    QList<Section> secs = st->sections(list);
    QVERIFY(st->setSectionStart(list, secs.at(0), 1000));
    QVERIFY(st->setSectionStart(list, secs.at(1), 20000));
    QVERIFY(st->setSectionStart(list, secs.at(2), 40000));
    QVERIFY(st->setCueTimecode(m_cues.at(3), 25000));
    QVERIFY(st->setCueTimecode(m_cues.at(5), 45000));
    // A time of its own in a later section (an older cue): it follows too.
    QVERIFY(st->setSectionStart(list, st->sections(list).at(2), -1));
    QVERIFY(st->setCueTimecode(m_cues.at(6), 50000));
    QVERIFY(!st->cue(m_cues.at(6))->tcFromSection);
    QVERIFY(st->setSectionStart(list, st->sections(list).at(2), 40000));

    // B made 3 s longer: B's start stays... B's END moves, so move C with follow.
    QVERIFY(st->setSectionStart(list, st->sections(list).at(1), 23000, true));
    QCOMPARE(timeOf(3), qint32(28000));
    QCOMPARE(st->sectionStart(list, st->sections(list).at(2)), qint32(43000));
    QCOMPARE(timeOf(5), qint32(48000));
    QCOMPARE(timeOf(6), qint32(53000));
    QCOMPARE(st->sectionStart(list, st->sections(list).at(0)), qint32(1000));   // earlier: stays
    QVERIFY(st->undo());
    QCOMPARE(timeOf(5), qint32(45000));
    QCOMPARE(timeOf(6), qint32(50000));

    // Without follow only B moves.
    QVERIFY(st->setSectionStart(list, st->sections(list).at(1), 23000, false));
    QCOMPARE(timeOf(3), qint32(28000));
    QCOMPARE(timeOf(5), qint32(45000));
    QCOMPARE(st->sectionStart(list, st->sections(list).at(2)), qint32(40000));
}

void TimecodeAnchors_Test::aNewTimeCountsFromItsSection()
{
    const quint32 list = makeShow({ "1", "1.1", "2", "2.1" }, { "A", "B" });
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setSectionStart(list, st->sections(list).at(0), 5000));
    QVERIFY(st->setCueTimecode(m_cues.at(1), 7000));
    QVERIFY(st->cue(m_cues.at(1))->tcFromSection);
    QCOMPARE(st->cue(m_cues.at(1))->tcOffset, qint32(2000));
    // B has no start: an absolute time, as before.
    QVERIFY(st->setCueTimecode(m_cues.at(3), 33000));
    QVERIFY(!st->cue(m_cues.at(3))->tcFromSection);
    // Cleared: no time, no offset.
    QVERIFY(st->setCueTimecode(m_cues.at(1), -1));
    QCOMPARE(timeOf(1), qint32(-1));
    QVERIFY(!st->cue(m_cues.at(1))->tcFromSection);
    // A section with no name keeps its start (its head isn't dropped).
    QVERIFY(st->setSectionName(list, st->sections(list).at(0), QString()));
    QCOMPARE(st->sectionStart(list, st->sections(list).at(0)), qint32(5000));
}

void TimecodeAnchors_Test::anchorOnTheCuePage()
{
    const quint32 list = makeShow({ "1", "1.1", "1.2" }, { "A" });
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setCueTimecode(m_cues.at(1), 4000));
    QVERIFY(st->setCueTimecodeAnchor(m_cues.at(1), true));
    QVERIFY(!st->setCueTimecodeAnchor(m_cues.at(1), true));          // no change, no step
    // With no section start, a cue after the anchor counts from the anchor.
    QVERIFY(st->setCueTimecode(m_cues.at(2), 6000));
    QVERIFY(st->cue(m_cues.at(2))->tcFromSection);
    QCOMPARE(st->cue(m_cues.at(2))->tcOffset, qint32(2000));
    QVERIFY(st->setCueTimecode(m_cues.at(1), 5000));
    QCOMPARE(timeOf(2), qint32(7000));
    // No longer an anchor: the cue after keeps its time, as its own.
    QVERIFY(st->setCueTimecodeAnchor(m_cues.at(1), false));
    QCOMPARE(timeOf(2), qint32(7000));
    QVERIFY(!st->cue(m_cues.at(2))->tcFromSection);
    QCOMPARE(st->timecodeAnchors(list).count(), 0);
}

void TimecodeAnchors_Test::convertingAnOlderShow()
{
    // An older show: absolute times, no section starts.
    const quint32 list = makeShow({ "1", "1.1", "1.2", "2", "2.1" }, { "A", "B" });
    ShowTreeController *st = m_doc->showTree();
    const QList<qint32> times { -1, 12000, 15000, 30500, 33000 };
    for (int i = 0; i < times.count(); ++i)
        if (times.at(i) >= 0)
            st->setCueTimecode(m_cues.at(i), times.at(i));
    for (int i = 0; i < times.count(); ++i)
        QVERIFY(!st->cue(m_cues.at(i))->tcFromSection);
    QCOMPARE(st->absoluteTimedCues(list), 4);
    // Nothing is converted until asked.
    QCOMPARE(st->timecodeAnchors(list).count(), 0);

    st->clearUndo();
    int n = 0;
    QVERIFY(st->convertToSectionTimes(list, &n));
    QCOMPARE(n, 4);
    QCOMPARE(st->undoText(), QStringLiteral("Convert to section times"));
    for (int i = 0; i < times.count(); ++i)
        QCOMPARE(timeOf(i), times.at(i));                // every cue where it was
    QCOMPARE(st->sectionStart(list, st->sections(list).at(0)), qint32(12000));
    QCOMPARE(st->sectionStart(list, st->sections(list).at(1)), qint32(30500));
    QCOMPARE(st->cue(m_cues.at(2))->tcOffset, qint32(3000));
    QCOMPARE(st->absoluteTimedCues(list), 0);
    QVERIFY(!st->convertToSectionTimes(list, &n));      // nothing left to convert
    QCOMPARE(n, 0);

    // Now one number moves a section.
    QVERIFY(st->setSectionStart(list, st->sections(list).at(1), 31500));
    QCOMPARE(timeOf(4), qint32(34000));
    QVERIFY(st->undo());
    QVERIFY(st->undo());                                // and the convert, in one step
    QCOMPARE(st->absoluteTimedCues(list), 4);
    QCOMPARE(st->sectionStart(list, st->sections(list).at(0)), qint32(-1));
}

void TimecodeAnchors_Test::saveAndLoadKeepTheAnchors()
{
    const quint32 list = makeShow({ "1", "1.1", "1.2", "2", "2.1" }, { "A", "B" });
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setSectionStart(list, st->sections(list).at(0), 10000));
    QVERIFY(st->setSectionStart(list, st->sections(list).at(1), 40000));
    QVERIFY(st->setCueTimecode(m_cues.at(1), 12000));
    QVERIFY(st->setCueTimecode(m_cues.at(2), 14000));
    QVERIFY(st->setCueTimecodeAnchor(m_cues.at(1), true));
    QVERIFY(st->setCueTimecode(m_cues.at(4), 41000));

    QByteArray xml;
    {
        QBuffer buf(&xml);
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buf);
        w.writeStartDocument();
        st->saveXML(&w);
        w.writeEndDocument();
    }
    // An older build reads Timecode=: the resolved time is there.
    QVERIFY(xml.contains("Timecode=\"14000\""));
    QVERIFY(xml.contains("TimecodeOffset=\"2000\""));
    QVERIFY(xml.contains("TimecodeAnchor=\"1\""));
    QVERIFY(xml.contains("Start=\"40000\""));

    Doc other(this);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(other.showTree()->loadXML(r));
    const ShowTreeController *ot = other.showTree();
    const CueList *l = ot->cueList(list);
    QVERIFY(l != nullptr);
    QCOMPARE(ot->sectionStart(list, ot->sections(list).at(1)), qint32(40000));
    QCOMPARE(l->cue(m_cues.at(2))->timecode, qint32(14000));
    QVERIFY(l->cue(m_cues.at(2))->tcFromSection);
    QCOMPARE(l->cue(m_cues.at(2))->tcOffset, qint32(2000));
    QVERIFY(l->cue(m_cues.at(1))->tcAnchor);
    QCOMPARE(ot->timecodeAnchors(list), st->timecodeAnchors(list));

    // An older show's XML (no Start=, no offsets) loads as absolute times.
    QByteArray old = xml;
    old.replace("TimecodeOffset=", "X1=").replace("Start=", "X2=").replace("TimecodeAnchor=", "X3=");
    Doc older(this);
    QXmlStreamReader r2(old);
    r2.readNextStartElement();
    QVERIFY(older.showTree()->loadXML(r2));
    const CueList *ol = older.showTree()->cueList(list);
    QCOMPARE(ol->cue(m_cues.at(2))->timecode, qint32(14000));
    QVERIFY(!ol->cue(m_cues.at(2))->tcFromSection);
    QCOMPARE(older.showTree()->timecodeAnchors(list).count(), 0);
}

void TimecodeAnchors_Test::timecodeFiresAtTheMovedTime()
{
    const quint32 list = makeShow({ "1", "1.1", "2", "2.1" }, { "A", "B" });
    ShowTreeController *st = m_doc->showTree();
    QStringList fired;
    m_doc->showControl()->setRunHandler([&](const ShowControl::Request &r, ShowControl::Route route) {
        if (route == ShowControl::RouteTimecode)
            fired << r.cue;
        return m_doc->showControl()->execute(r);
    });
    m_doc->timecodeSource();
    QVERIFY(st->setSectionStart(list, st->sections(list).at(1), 20000));
    QVERIFY(st->setCueTimecode(m_cues.at(3), 22000));
    QVERIFY(st->setSectionStart(list, st->sections(list).at(1), 26000));   // 2.1 at 28 s now
    TimecodeSource *src = m_doc->timecodeSource();
    src->updateTimeCode(0, 0, 30);
    src->updateTimeCode(0, 22000, 30);
    QVERIFY(fired.isEmpty());
    src->updateTimeCode(0, 28000, 30);
    QCOMPARE(fired, QStringList { QStringLiteral("2.1") });
}

void TimecodeAnchors_Test::anchorsChangedSignal()
{
    const quint32 list = makeShow({ "1", "1.1", "2" }, { "A", "B" });
    ShowTreeController *st = m_doc->showTree();
    QSignalSpy spy(st, &ShowTreeController::timecodeAnchorsChanged);
    QVERIFY(st->setSectionStart(list, st->sections(list).at(0), 1000));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toUInt(), list);
    // An edit that changes no anchor says nothing.
    Cue c = *st->cue(m_cues.at(1));
    c.name = "Renamed";
    QVERIFY(st->setCue(c));
    QCOMPARE(spy.count(), 1);
    // A cue that follows the start is part of it (its count changes).
    QVERIFY(st->setCueTimecode(m_cues.at(1), 3000));
    QCOMPARE(spy.count(), 2);
    QVERIFY(st->undo());                                // the time: its count changes
    QVERIFY(st->undo());                                // the rename: nothing
    QCOMPARE(spy.count(), 3);
    QVERIFY(st->undo());                                // the start
    QCOMPARE(spy.count(), 4);
    QCOMPARE(st->timecodeAnchors(list).count(), 0);
}

/****************************************************************************
 * Logic markers
 ****************************************************************************/

void TimecodeAnchors_Test::midiMarkersFromTheFixture()
{
    const QString path = QFINDTESTDATA("markers.mid");
    QVERIFY(!path.isEmpty());
    const MidiMarkers::Result r = MidiMarkers::readFile(path);
    QVERIFY2(r.ok, qPrintable(r.error));
    QCOMPARE(r.smpteOffsetMs, qint64(-1));
    QCOMPARE(r.tempoChanges, 2);
    QCOMPARE(r.markers.count(), 5);
    // 120 bpm, then 60 bpm from tick 3840; one marker on the second track.
    const QStringList names { "Intro", "Verse 1", "CHORUS-2", "Bridge", "Outro" };
    const QList<qint64> ms { 0, 2000, 4000, 5000, 7000 };
    for (int i = 0; i < 5; ++i)
    {
        QCOMPARE(r.markers.at(i).name, names.at(i));
        QCOMPARE(r.markers.at(i).ms, ms.at(i));
    }
    QVERIFY(!MidiMarkers::readFile(path + ".missing").ok);
}

void TimecodeAnchors_Test::midiSmpteOffsetAndDivision()
{
    // SMPTE division, 25 fps x 40 ticks: a tick is a millisecond. The
    // offset is 01:00:00:00 at 25 fps.
    QByteArray events = meta(0, 0x54, QByteArray::fromHex("2100000000"));
    events += meta(1500, 0x06, QByteArray("Verse"));
    events += meta(0, 0x06, QByteArray("Caf\xc3\xa9"));             // UTF-8
    events += meta(10, 0x06, QByteArray("Ol\xe9"));                 // Latin-1
    events += meta(10, 0x06, QByteArray());                         // no name: skipped
    const quint16 division = quint16((quint16(quint8(-25)) << 8) | 40);
    const MidiMarkers::Result r = MidiMarkers::read(midiFile(division, events));
    QVERIFY2(r.ok, qPrintable(r.error));
    QCOMPARE(r.smpteOffsetMs, qint64(3600000));
    QCOMPARE(r.markers.count(), 3);
    QCOMPARE(r.markers.at(0).ms, qint64(1500));
    QCOMPARE(r.markers.at(1).name, QString::fromUtf8("Caf\xc3\xa9"));
    QCOMPARE(r.markers.at(2).name, QString::fromUtf8("Ol\xc3\xa9"));
    QCOMPARE(r.markers.at(2).ms, qint64(1510));
}

void TimecodeAnchors_Test::midiGarbageIsRefused()
{
    QVERIFY(!MidiMarkers::read(QByteArray()).ok);
    QVERIFY(!MidiMarkers::read(QByteArray("RIFF....WAVEfmt ")).ok);
    QByteArray type2 = midiFile(480, QByteArray());
    type2[9] = 2;
    QVERIFY(!MidiMarkers::read(type2).ok);
    // Every cut of the fixture reads without running off the end.
    QFile f(QFINDTESTDATA("markers.mid"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray whole = f.readAll();
    for (int n = 0; n < whole.size(); ++n)
    {
        const MidiMarkers::Result r = MidiMarkers::read(whole.left(n));
        QVERIFY(r.markers.count() <= 5);
    }
    // A data byte with no status stops that track, it doesn't crash.
    const MidiMarkers::Result bad = MidiMarkers::read(midiFile(480, vlq(0) + char(0x40) + char(0x40)));
    QVERIFY(bad.ok);
    QCOMPARE(bad.markers.count(), 0);
}

void TimecodeAnchors_Test::markerPlanPreviewApplyAndUndo()
{
    const quint32 list = makeShow({ "1", "1.1", "2", "2.1", "3", "3.1", "3.2", "4", "4.1", "5", "5.1" },
                                  { "Intro", "Verse 1", "Chorus 2", "Bridge", "Encore" });
    ShowTreeController *st = m_doc->showTree();
    const QList<qint32> starts { 0, 2000, 3000, 5000, 9000 };
    for (int i = 0; i < starts.count(); ++i)
        QVERIFY(st->setSectionStart(list, st->sections(list).at(i), starts.at(i)));
    const QList<qint32> times { 0, 500, 2000, 2500, 3000, 3200, 3700, 5000, 5300, 9000, 9100 };
    for (int i = 0; i < times.count(); ++i)
        st->setCueTimecode(m_cues.at(i), times.at(i));
    QCOMPARE(st->absoluteTimedCues(list), 0);
    st->clearUndo();

    const MidiMarkers::Result markers = MidiMarkers::readFile(QFINDTESTDATA("markers.mid"));
    const MarkerPlan plan = MarkerPlan::make(st, list, markers);
    QCOMPARE(plan.offsetMs, qint64(0));
    QCOMPARE(plan.moves.count(), 4);
    QVERIFY(plan.changes());
    QCOMPARE(plan.unmatched, QStringList { QStringLiteral("Outro at 0:07.0") });
    QCOMPARE(plan.missing, QStringList { QStringLiteral("5 — Encore") });
    const QStringList lines = plan.lines();
    QVERIFY2(lines.contains(QStringLiteral("3 — Chorus 2 moved +1.0 s, 3 cue(s) follow")), qPrintable(lines.join('|')));
    QVERIFY(lines.contains(QStringLiteral("1 — Intro stays at 0:00.0")));
    QVERIFY(lines.contains(QStringLiteral("Marker not matched: Outro at 0:07.0")));
    QVERIFY(lines.contains(QStringLiteral("No marker for 5 — Encore: it stays where it is")));
    // The preview changed nothing.
    QCOMPARE(timeOf(5), qint32(3200));

    QVERIFY(MarkerPlan::apply(st, list, plan));
    QCOMPARE(st->undoText(), QStringLiteral("Import Logic markers"));
    QCOMPARE(st->sectionStart(list, st->sections(list).at(2)), qint32(4000));
    QCOMPARE(timeOf(4), qint32(4000));
    QCOMPARE(timeOf(5), qint32(4200));
    QCOMPARE(timeOf(6), qint32(4700));
    QCOMPARE(timeOf(9), qint32(9000));                  // Encore: no marker, stays
    QCOMPARE(timeOf(1), qint32(500));
    // One undo step puts all of it back.
    QVERIFY(st->undo());
    QVERIFY(!st->canUndo());
    QCOMPARE(timeOf(5), qint32(3200));
    QCOMPARE(st->sectionStart(list, st->sections(list).at(2)), qint32(3000));

    // Applying a plan that changes nothing is no step.
    QVERIFY(MarkerPlan::apply(st, list, plan));
    const MarkerPlan again = MarkerPlan::make(st, list, markers);
    QVERIFY(!again.changes());
    QVERIFY(!MarkerPlan::apply(st, list, again));

    QCOMPARE(MarkerPlan::matchKey(QStringLiteral(" CHORUS-2 ")), QStringLiteral("chorus 2"));
    QCOMPARE(MarkerPlan::formatDelta(-1500), QStringLiteral("-1.5 s"));
    QCOMPARE(MarkerPlan::formatTime(3723400), QStringLiteral("1:02:03.4"));
}

void TimecodeAnchors_Test::markerPlanHourOffset()
{
    const quint32 list = makeShow({ "1", "2" }, { "Verse 1", "Bridge" });
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setSectionStart(list, st->sections(list).at(0), 3600000 + 2000));
    QVERIFY(st->setSectionStart(list, st->sections(list).at(1), 3600000 + 4000));
    // decided: the session's 01:00:00:00 start is added to markers from 0.
    const MarkerPlan plan = MarkerPlan::make(st, list, MidiMarkers::readFile(QFINDTESTDATA("markers.mid")));
    QCOMPARE(plan.offsetMs, qint64(3600000));
    QCOMPARE(plan.moves.count(), 2);
    QCOMPARE(plan.moves.at(0).to, qint32(3600000 + 2000));
    QCOMPARE(plan.moves.at(1).to, qint32(3600000 + 5000));
    // The file's own SMPTE offset wins.
    MidiMarkers::Result withOffset = MidiMarkers::readFile(QFINDTESTDATA("markers.mid"));
    withOffset.smpteOffsetMs = 1000;
    QCOMPARE(MarkerPlan::make(st, list, withOffset).moves.at(0).to, qint32(3000));
}

void TimecodeAnchors_Test::markersOntoAnOlderShow()
{
    // No starts, absolute times: the markers set the starts; the cues stay
    // where they are and count from them from now on.
    const quint32 list = makeShow({ "1", "1.1", "2", "2.1" }, { "Verse 1", "Bridge" });
    ShowTreeController *st = m_doc->showTree();
    st->setCueTimecode(m_cues.at(1), 2600);
    st->setCueTimecode(m_cues.at(3), 5400);
    st->clearUndo();
    const MarkerPlan plan = MarkerPlan::make(st, list, MidiMarkers::readFile(QFINDTESTDATA("markers.mid")));
    QVERIFY(plan.lines().contains(QStringLiteral("2 — Bridge starts at 0:05.0, 1 cue(s) count from it now")));
    QVERIFY(MarkerPlan::apply(st, list, plan));
    QCOMPARE(timeOf(3), qint32(5400));
    QVERIFY(st->cue(m_cues.at(3))->tcFromSection);
    QCOMPARE(st->cue(m_cues.at(3))->tcOffset, qint32(400));
    QVERIFY(st->undo());
    QVERIFY(!st->cue(m_cues.at(3))->tcFromSection);
}

/****************************************************************************
 * Ripple
 ****************************************************************************/

void TimecodeAnchors_Test::rippleInsertAndRemove()
{
    const quint32 list = makeShow({ "1", "1.1", "2", "2.1", "3", "3.1" }, { "A", "B", "C" });
    ShowTreeController *st = m_doc->showTree();
    const QList<qint32> starts { 0, 20000, 40000 };
    for (int i = 0; i < 3; ++i)
        QVERIFY(st->setSectionStart(list, st->sections(list).at(i), starts.at(i)));
    const QList<qint32> times { 0, 5000, 20000, 25000, 40000, 45000 };
    for (int i = 0; i < times.count(); ++i)
        st->setCueTimecode(m_cues.at(i), times.at(i));
    // One older absolute cue in the middle.
    QVERIFY(st->setCueTimecode(m_cues.at(3), -1));
    QVERIFY(st->setSectionStart(list, st->sections(list).at(1), -1));
    QVERIFY(st->setCueTimecode(m_cues.at(3), 25000));
    QVERIFY(st->setSectionStart(list, st->sections(list).at(1), 20000));
    QVERIFY(!st->cue(m_cues.at(3))->tcFromSection);
    st->clearUndo();

    // Insert 2.5 s at 2.1: it and everything after move; 2 starts before it.
    QVERIFY(st->rippleTimecode(m_cues.at(3), 2500));
    QCOMPARE(st->undoText(), QStringLiteral("Insert time"));
    QCOMPARE(timeOf(1), qint32(5000));
    QCOMPARE(timeOf(2), qint32(20000));
    QCOMPARE(timeOf(3), qint32(27500));
    QCOMPARE(timeOf(4), qint32(42500));
    QCOMPARE(timeOf(5), qint32(47500));
    QCOMPARE(st->sectionStart(list, st->sections(list).at(1)), qint32(20000));
    QCOMPARE(st->sectionStart(list, st->sections(list).at(2)), qint32(42500));
    QVERIFY(st->undo());
    QCOMPARE(timeOf(5), qint32(45000));

    // At a section's first cue its start moves too.
    QVERIFY(st->rippleTimecode(m_cues.at(2), 1000));
    QCOMPARE(st->sectionStart(list, st->sections(list).at(1)), qint32(21000));
    QCOMPARE(timeOf(2), qint32(21000));
    QVERIFY(st->undo());

    // Remove: everything after moves earlier, never before 0.
    QVERIFY(st->rippleTimecode(m_cues.at(4), -1500));
    QCOMPARE(st->undoText(), QStringLiteral("Remove time"));
    QCOMPARE(timeOf(4), qint32(38500));
    QCOMPARE(timeOf(5), qint32(43500));
    QCOMPARE(timeOf(3), qint32(25000));
    QVERIFY(st->undo());
    QVERIFY(st->rippleTimecode(m_cues.at(1), -60000));
    QCOMPARE(timeOf(5), qint32(0));
    QVERIFY(st->undo());
    QVERIFY(!st->rippleTimecode(m_cues.at(1), 0));
}

void TimecodeAnchors_Test::stretchASection()
{
    const quint32 list = makeShow({ "1", "2", "2.1", "2.2", "3", "3.1" }, { "A", "Chorus 2", "C" });
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setSectionStart(list, st->sections(list).at(1), 20000));
    QVERIFY(st->setSectionStart(list, st->sections(list).at(2), 30000));
    const QList<qint32> times { 1000, 20000, 22500, 25000, 30000, 33000 };
    for (int i = 0; i < times.count(); ++i)
        st->setCueTimecode(m_cues.at(i), times.at(i));
    QCOMPARE(sectionLength(*st->cueList(list), Numbering::ShowPartIteration, 1), qint32(10000));
    st->clearUndo();

    QVERIFY(st->stretchSection(list, st->sections(list).at(1), 12000));
    QCOMPARE(st->undoText(), QStringLiteral("Stretch section"));
    QCOMPARE(timeOf(0), qint32(1000));
    QCOMPARE(timeOf(1), qint32(20000));
    QCOMPARE(timeOf(2), qint32(23000));
    QCOMPARE(timeOf(3), qint32(26000));
    QCOMPARE(st->sectionStart(list, st->sections(list).at(2)), qint32(32000));
    QCOMPARE(timeOf(4), qint32(32000));
    QCOMPARE(timeOf(5), qint32(35000));
    QVERIFY(st->undo());
    QCOMPARE(timeOf(3), qint32(25000));
    QCOMPARE(timeOf(5), qint32(33000));

    // The last section runs to its last timed cue.
    QCOMPARE(sectionLength(*st->cueList(list), Numbering::ShowPartIteration, 2), qint32(3000));
    QVERIFY(st->stretchSection(list, st->sections(list).at(2), 6000));
    QCOMPARE(timeOf(5), qint32(36000));
    QVERIFY(st->undo());

    // A section with nothing to measure says why.
    QString why;
    QVERIFY(st->setCueTimecode(m_cues.at(0), -1));
    QVERIFY(!st->stretchSection(list, st->sections(list).at(0), 5000, &why));
    QVERIFY(!why.isEmpty());
    QVERIFY(!st->stretchSection(list, st->sections(list).at(1), 10000, &why));     // that long already
}

void TimecodeAnchors_Test::rippleAndStretchTyped()
{
    const quint32 list = makeShow({ "1", "2", "2.1", "2.2", "3", "3.1" }, { "A", "Chorus 2", "C" });
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setSectionStart(list, st->sections(list).at(1), 20000));
    QVERIFY(st->setSectionStart(list, st->sections(list).at(2), 30000));
    const QList<qint32> times { 1000, 20000, 22500, 25000, 30000, 33000 };
    for (int i = 0; i < times.count(); ++i)
        st->setCueTimecode(m_cues.at(i), times.at(i));
    st->clearUndo();

    OperationRegistry reg(m_doc);
    ShowTreeCommand::Context ctx;
    ctx.production = m_prod;
    ctx.list = list;
    QJsonObject args;
    QCOMPARE(reg.resolveLine("ripple +2.5 at 2.1", &args), QStringLiteral("ripple_timecode"));
    QCOMPARE(args.value("seconds").toDouble(), 2.5);
    QCOMPARE(args.value("cue").toString(), QStringLiteral("2.1"));
    ShowTreeCommand::Result r = reg.runLine("ripple +2.5 at 2.1", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(timeOf(2), qint32(25000));
    QCOMPARE(timeOf(5), qint32(35500));
    QVERIFY(st->undo());
    QCOMPARE(timeOf(5), qint32(33000));

    r = reg.runLine("ripple -0:01 at 3", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(timeOf(4), qint32(29000));
    QVERIFY(st->undo());

    r = reg.runLine("stretch Chorus 2 to 0:12", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(timeOf(3), qint32(26000));
    QCOMPARE(st->undoText(), QStringLiteral("Stretch section"));
    QVERIFY(st->undo());

    // Refused, with why: no such cue, no such section, words that don't fit.
    QVERIFY(!reg.runLine("ripple +2.5 at 9.9", ctx).ok);
    QVERIFY(!reg.runLine("stretch Bridge to 0:12", ctx).ok);
    QVERIFY(!reg.runLine("ripple sideways", ctx).ok);
    QVERIFY(!reg.runLine("stretch Chorus 2", ctx).ok);
    QVERIFY(!st->canUndo());

    // The selected cue when none is named.
    ctx.cue = m_cues.at(3);
    QVERIFY(reg.runLine("ripple 1", ctx).ok);
    QCOMPARE(timeOf(3), qint32(26000));
    QCOMPARE(timeOf(2), qint32(22500));
}

/****************************************************************************
 * Re-learn
 ****************************************************************************/

void TimecodeAnchors_Test::learnSetsEachSectionStart()
{
    const quint32 list = makeShow({ "1", "1.1", "2", "2.1", "3" }, { "A", "B", "C" });
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setSectionStart(list, st->sections(list).at(0), 1000));
    QVERIFY(st->setSectionStart(list, st->sections(list).at(1), 20000));
    st->setCueTimecode(m_cues.at(1), 3000);
    st->setCueTimecode(m_cues.at(3), 24000);
    st->clearUndo();

    // The new export plays; tap at each section start.
    TimecodeSource *src = m_doc->timecodeSource();
    SectionLearn learn(st, list);
    QCOMPARE(learn.section(), 0);
    QCOMPARE(learn.sectionTitle(), QStringLiteral("1 — A"));
    src->updateTimeCode(0, 1500, 30);
    QVERIFY(learn.tap(qint32(src->positionMs())));
    QCOMPARE(learn.section(), 1);
    QCOMPARE(timeOf(1), qint32(3500));                  // A's cue followed
    QVERIFY(!learn.tap(1000));                          // before the last tap: refused
    src->updateTimeCode(0, 22000, 30);
    QVERIFY(learn.tap(qint32(src->positionMs())));
    QCOMPARE(timeOf(3), qint32(26000));
    // C had no start: it gets one; nothing of it moves.
    QVERIFY(learn.tap(40000));
    QVERIFY(learn.done());
    QVERIFY(!learn.tap(50000));
    QCOMPARE(learn.learned(), 3);
    QCOMPARE(st->sectionStart(list, st->sections(list).at(2)), qint32(40000));
    // Each tap was its own undo step.
    QVERIFY(st->undo());
    QVERIFY(st->undo());
    QCOMPARE(timeOf(3), qint32(24000));
    QVERIFY(st->undo());
    QVERIFY(!st->canUndo());
    QCOMPARE(timeOf(1), qint32(3000));

    // From a later section, and an empty list learns nothing.
    QCOMPARE(SectionLearn(st, list, 2).section(), 2);
    QVERIFY(SectionLearn(st, list, 9).done());
}

QTEST_MAIN(TimecodeAnchors_Test)
