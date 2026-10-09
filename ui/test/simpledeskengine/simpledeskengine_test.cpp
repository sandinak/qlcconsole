/*
  Q Light Controller Plus - Test Unit
  simpledeskengine_test.cpp

  Copyright (c) Massimo Callegari

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

#define protected public
#define private public
#include "simpledeskengine.h"
#include <QSignalSpy>
#include "inputoutputmap.h"
#include "showstatus.h"
#include "simpledesk.h"
#include "virtualconsole.h"
#include "doc.h"
#include "cue.h"
#include "cuestack.h"
#undef protected
#undef private

#include <QBuffer>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "simpledeskengine_test.h"

void SimpleDeskEngine_Test::init()
{
    m_doc = new Doc(this);
}

void SimpleDeskEngine_Test::cleanup()
{
    delete m_doc;
}

void SimpleDeskEngine_Test::values()
{
    SimpleDeskEngine eng(m_doc);
    eng.setValue(5, 100);
    QCOMPARE(eng.value(5), uchar(100));
    QVERIFY(eng.hasChannel(5));
    eng.resetChannel(5);
    QVERIFY(!eng.hasChannel(5));
}

void SimpleDeskEngine_Test::cueStack()
{
    SimpleDeskEngine eng(m_doc);
    CueStack* stack = eng.cueStack(0);
    QVERIFY(stack != NULL);
    QCOMPARE(eng.cueStack(0), stack);
}

void SimpleDeskEngine_Test::resetUniverse()
{
    SimpleDeskEngine eng(m_doc);
    eng.setValue(5, 100);
    eng.setValue(520, 50);

    eng.resetUniverse(1);
    QVERIFY(!eng.hasChannel(520));
    QVERIFY(eng.hasChannel(5));
    QCOMPARE(eng.m_commandQueue.last().first, int(SimpleDeskEngine::ResetUniverse));
    QCOMPARE(eng.m_commandQueue.last().second, quint32(1));
}

void SimpleDeskEngine_Test::xml()
{
    SimpleDeskEngine eng(m_doc);
    CueStack* cs = eng.cueStack(3);
    Cue cue("Test");
    cue.setValue(0, 255);
    cs->appendCue(cue);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);
    eng.saveXML(&xmlWriter);
    xmlWriter.setDevice(nullptr);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    SimpleDeskEngine eng2(m_doc);
    QVERIFY(eng2.loadXML(xmlReader) == true);
    CueStack* cs2 = eng2.cueStack(3);
    QCOMPARE(cs2->cues().size(), 1);
    QCOMPARE(cs2->cues().at(0).name(), QString("Test"));
}

void SimpleDeskEngine_Test::heldChannelsAreInTheFooterUntilReleased()
{
    m_doc->inputOutputMap()->setUniverseName(1, "U02-Lantern");
    VirtualConsole *vc = new VirtualConsole(nullptr, m_doc);   // the desk's grand master reads it
    auto dropVc = qScopeGuard([vc]() { delete vc; });
    SimpleDesk desk(nullptr, m_doc);
    SimpleDeskEngine *eng = desk.m_engine;
    QSignalSpy held(eng, &SimpleDeskEngine::heldChannelsChanged);

    auto entry = []() {
        foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
            if (e.key == "simpledesk.held")
                return e;
        return ShowStatus::Entry();
    };

    // Taking a channel says so; moving one already held does not.
    eng->setValue((1 << 9) | 0, 255);
    eng->setValue((1 << 9) | 6, 120);
    eng->setValue((1 << 9) | 6, 200);
    QCOMPARE(held.count(), 2);
    QCOMPARE(eng->heldChannels().size(), 2);
    QTRY_VERIFY(!entry().key.isEmpty());
    const ShowStatus::Entry e = entry();
    QCOMPARE(e.severity, ShowStatus::Warning);
    QVERIFY2(e.summary.contains("2 channel"), qPrintable(e.summary));
    QCOMPARE(e.items, QStringList() << "U02-Lantern — 2 channel(s): 1, 7");
    QCOMPARE(e.itemData.first().toInt(), 1);

    // Released: gone from the desk and from the footer.
    desk.releaseUniverse(1);
    QVERIFY(eng->heldChannels().isEmpty());
    QTRY_VERIFY(entry().key.isEmpty());
}

QTEST_MAIN(SimpleDeskEngine_Test)
