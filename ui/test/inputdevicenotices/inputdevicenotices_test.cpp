/*
  Q Light Controller Plus - Test Unit
  inputdevicenotices_test.cpp

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
#include <QSignalSpy>

#include "inputdevicenotices_test.h"
#include "inputdevicenotices.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "iopluginstub.h"
#include "showstatus.h"
#include "universe.h"
#include "qlcfile.h"
#include "doc.h"

namespace
{
const QString kPad = QStringLiteral("DualSense Wireless Controller (aa-bb)");

/** The plugin's lines after a rescan: as a pad sleeping or waking. */
void rescanned(IOPluginStub *stub, const QStringList &lines)
{
    stub->m_lineNames = lines;
    stub->configure();      // emits configurationChanged()
}

ShowStatus::Entry footerEntry()
{
    foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
        if (e.key == QLatin1String("input.devices"))
            return e;
    return ShowStatus::Entry();
}
}

void InputDeviceNotices_Test::initTestCase()
{
    m_doc = new Doc(this);
    QDir dir(QStringLiteral("../../../engine/test/iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    m_stub = static_cast<IOPluginStub *>(
        m_doc->ioPluginCache()->plugin(QStringLiteral("I/O Plugin Stub")));
    QVERIFY2(m_stub != nullptr, "the I/O plugin stub did not load");
}

void InputDeviceNotices_Test::init()
{
    m_stub->m_linesAreHardware = true;
    m_stub->m_lineNames = QStringList() << "Keyboard" << kPad;
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(1, m_stub->name(), QString(), 1));
}

void InputDeviceNotices_Test::cleanup()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    iom->setInputPatch(1, KInputNone, QString(), QLCIOPlugin::invalidLine());
    iom->forgetOfflinePatches(1);
    m_stub->m_linesAreHardware = false;
    m_stub->m_lineNames.clear();
    ShowStatus::instance()->clearStatus(QStringLiteral("input.devices"));
}

void InputDeviceNotices_Test::aDisconnectIsSaidOnceSettled()
{
    InputDeviceNotices notices(m_doc);
    notices.setSettleTime(100);
    notices.settleNow();        // what is there now is known
    QSignalSpy said(&notices, &InputDeviceNotices::notice);
    QCOMPARE(notices.devices().count(), 1);
    QVERIFY(notices.devices().first().connected);

    rescanned(m_stub, QStringList() << "Keyboard");
    QCOMPARE(said.count(), 0);              // not at once: it may be a flap
    QTRY_COMPARE(said.count(), 1);
    QCOMPARE(said.at(0).at(0).toString(),
             QString("Input device '%1' disconnected — universe 2").arg(kPad));
    QCOMPARE(said.at(0).at(1).toBool(), true);

    // The footer says it while it is away.
    const ShowStatus::Entry e = footerEntry();
    QCOMPARE(e.severity, ShowStatus::Warning);
    QVERIFY2(e.summary.contains(kPad) && e.summary.contains("universe 2"), qPrintable(e.summary));

    // Nothing more said while nothing changes.
    notices.settleNow();
    QCOMPARE(said.count(), 1);
}

void InputDeviceNotices_Test::aReconnectIsSaidAndClearsTheFooter()
{
    InputDeviceNotices notices(m_doc);
    notices.setSettleTime(50);
    notices.settleNow();
    QSignalSpy said(&notices, &InputDeviceNotices::notice);

    rescanned(m_stub, QStringList() << "Keyboard");
    notices.settleNow();
    QCOMPARE(said.count(), 1);
    QVERIFY(footerEntry().key.isEmpty() == false);

    rescanned(m_stub, QStringList() << kPad << "Keyboard");
    QTRY_COMPARE(said.count(), 2);
    QVERIFY2(said.at(1).at(0).toString().contains("connected — universe 2"),
             qPrintable(said.at(1).at(0).toString()));
    QVERIFY(said.at(1).at(0).toString().contains("disconnected") == false);
    QCOMPARE(said.at(1).at(1).toBool(), false);
    QVERIFY(footerEntry().key.isEmpty());
}

void InputDeviceNotices_Test::aFlapIsNotSaid()
{
    /* A Bluetooth pad dropping out and straight back within the settle
       time: nothing to tell anyone. */
    InputDeviceNotices notices(m_doc);
    notices.setSettleTime(300);
    notices.settleNow();
    QSignalSpy said(&notices, &InputDeviceNotices::notice);

    rescanned(m_stub, QStringList() << "Keyboard");
    rescanned(m_stub, QStringList() << "Keyboard" << kPad);
    QVERIFY(m_doc->inputOutputMap()->inputPatch(1) != nullptr);
    QTest::qWait(500);
    QCOMPARE(said.count(), 0);
    QVERIFY(footerEntry().key.isEmpty());
}

void InputDeviceNotices_Test::absentWhenTheShowOpensIsSaid()
{
    rescanned(m_stub, QStringList() << "Keyboard");     // switched off
    InputDeviceNotices notices(m_doc);
    notices.reset();                                    // the show just opened
    QSignalSpy said(&notices, &InputDeviceNotices::notice);
    notices.settleNow();
    QCOMPARE(said.count(), 1);
    QVERIFY2(said.at(0).at(0).toString().contains("not connected — universe 2"),
             qPrintable(said.at(0).at(0).toString()));
    QCOMPARE(said.at(0).at(1).toBool(), true);
}

void InputDeviceNotices_Test::onlyDeviceInputsCount()
{
    // A protocol's input (Art-Net, OSC) is not a device that comes and goes.
    m_stub->m_linesAreHardware = false;
    InputDeviceNotices notices(m_doc);
    QVERIFY(notices.devices().isEmpty());
    QCOMPARE(InputDeviceNotices::kindName("HID"), QString("Controller"));
    QCOMPARE(InputDeviceNotices::kindName("MIDI"), QString("MIDI device"));
}

void InputDeviceNotices_Test::theDefaultSettleIsTwoAndAHalfSeconds()
{
    /* CHANGELOG [0.2.2]: the notice is "debounced 2.5 s so a Bluetooth
       flap says nothing". Out of the box (no setSettleTime) a pad gone for
       a moment says nothing, and the wait is 2.5 s. */
    InputDeviceNotices notices(m_doc);
    QCOMPARE(notices.settleTime(), 2500);
    notices.settleNow();
    QSignalSpy said(&notices, &InputDeviceNotices::notice);

    rescanned(m_stub, QStringList() << "Keyboard");
    QTest::qWait(200);                  // well inside the default settle
    QCOMPARE(said.count(), 0);
    rescanned(m_stub, QStringList() << "Keyboard" << kPad);
    notices.settleNow();                // back before it settled: a flap
    QCOMPARE(said.count(), 0);
    QVERIFY(footerEntry().key.isEmpty());
}

QTEST_MAIN(InputDeviceNotices_Test)
