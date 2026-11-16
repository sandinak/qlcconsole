/*
  Q Light Controller Plus
  controllerjobs_test.cpp

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
#include <QSignalSpy>
#include <QSettings>
#include <QXmlStreamWriter>

#include "controllerjobs_test.h"
#include "programmercontroller.h"
#include "controllerjobs.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "iopluginstub.h"
#include "qlcioplugin.h"
#include "qlcfile.h"
#include "doc.h"

using J = ControllerJobs;

void ControllerJobs_Test::init()
{
    QSettings().remove(ControllerJobs::settingsGroup());
    m_doc = new Doc(this);
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
    m_stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    m_stub->m_lineNames.clear();
    m_stub->m_lineLocations.clear();
}

void ControllerJobs_Test::cleanup()
{
    if (m_stub != nullptr)
    {
        m_stub->m_lineNames.clear();
        m_stub->m_lineLocations.clear();
    }
    delete m_doc;
    m_doc = nullptr;
    m_stub = nullptr;
    QSettings().remove(ControllerJobs::settingsGroup());
}

void ControllerJobs_Test::anAddressInTheName_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("mac");
    QTest::newRow("macOS colons") << "DUALSHOCK 4 Wireless Controller (DE:4C:09:77:77:93)" << "de:4c:09:77:77:93";
    QTest::newRow("macOS dashes") << "Sony DualSense Wireless Controller (de-4c-09-77-77-93)" << "de:4c:09:77:77:93";
    QTest::newRow("leading space") << " DUALSHOCK 4 Wireless Controller (AA:BB:CC:00:00:01)" << "aa:bb:cc:00:00:01";
    QTest::newRow("no serial") << "Xbox Wireless Controller" << "";
    QTest::newRow("a serial, not an address") << "Xbox Controller (3033363030353735)" << "";
    QTest::newRow("mixed separators") << "Pad (DE:4C-09:77:77:93)" << "";
    QTest::newRow("seven pairs") << "Pad (DE:4C:09:77:77:93:11)" << "";
    QTest::newRow("vendor:product") << "HID Input Device (54C:9CC)" << "";
}

void ControllerJobs_Test::anAddressInTheName()
{
    QFETCH(QString, name);
    QFETCH(QString, mac);
    QCOMPARE(ControllerJobs::macIn(name), mac);
}

void ControllerJobs_Test::theAddressThenThePortThenTheName()
{
    // The address wins over the port: a Bluetooth pad has no fixed port.
    QCOMPARE(J::identityFor("HID", "DualSense Wireless Controller (DE:4C:09:77:77:93)", "Bluetooth 1a2b"),
             QString("mac:de:4c:09:77:77:93"));
    QCOMPARE(J::identityFor("HID", "Xbox Wireless Controller", "USB 14200000"),
             QString("port:HID:USB 14200000"));
    QCOMPARE(J::identityFor("HID", " Xbox Wireless Controller ", ""),
             QString("name:HID:Xbox Wireless Controller"));
    // The same address under another plugin or name is the same pad.
    QCOMPARE(J::identityFor("Other", "Pad (de-4c-09-77-77-93)", "x"),
             J::identityFor("HID", "DualSense (DE:4C:09:77:77:93)", "y"));
}

void ControllerJobs_Test::twoOfAModelAreToldApart()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    // Same name, two addresses.
    m_stub->m_lineNames = QStringList() << "DUALSHOCK 4 Wireless Controller (AA:BB:CC:00:00:01)"
                                        << "DUALSHOCK 4 Wireless Controller (AA:BB:CC:00:00:02)";
    QVERIFY(J::identityOf(m_stub, 0) != J::identityOf(m_stub, 1));
    // Same name, no address, two ports.
    m_stub->m_lineNames = QStringList() << "Wireless Controller" << "Wireless Controller";
    m_stub->m_lineLocations = { { 0, "USB 14200000" }, { 1, "USB 14300000" } };
    QVERIFY(J::identityOf(m_stub, 0) != J::identityOf(m_stub, 1));
    QVERIFY(!J::isNameOnly(J::identityOf(m_stub, 0)));
    // On universes: each universe's pad, by the line patched there.
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 1));
    QVERIFY(iom->setInputPatch(1, m_stub->name(), QString(), 0));
    J *jobs = m_doc->programmer()->controllerJobs();
    QCOMPARE(jobs->identityOfUniverse(0), J::identityOf(m_stub, 1));
    QCOMPARE(jobs->identityOfUniverse(1), J::identityOf(m_stub, 0));
    // A job off on one is off on that one only.
    jobs->setJob(jobs->identityOfUniverse(0), J::RunCues, false);
    QVERIFY(!jobs->has(0, J::RunCues));
    QVERIFY(jobs->has(1, J::RunCues));
    // The pads swap universes: the jobs follow the pad.
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 0));
    QVERIFY(iom->setInputPatch(1, m_stub->name(), QString(), 1));
    QVERIFY(jobs->has(0, J::RunCues));
    QVERIFY(!jobs->has(1, J::RunCues));
    // Neither: the name only, which two of a model share (and say so).
    m_stub->m_lineLocations.clear();
    QCOMPARE(J::identityOf(m_stub, 0), J::identityOf(m_stub, 1));
    QVERIFY(J::isNameOnly(J::identityOf(m_stub, 0)));
    // Nothing on a line: no identity.
    QCOMPARE(J::identityOf(m_stub, 7), QString());
    QCOMPARE(J::identityOf(nullptr, 0), QString());
}

void ControllerJobs_Test::everyJobByDefault()
{
    J *jobs = m_doc->programmer()->controllerJobs();
    QVERIFY(jobs->saved().isEmpty());
    m_stub->m_lineNames = QStringList() << "Pad A (AA:BB:CC:00:00:01)" << "Pad B";
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, m_stub->name(), QString(), 0));
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(1, m_stub->name(), QString(), 1));
    for (quint32 u = 0; u < 4; ++u)        // two pads, two universes with none
        for (const J::Job job : J::jobs())
            QVERIFY2(jobs->has(u, job), qPrintable(QString("u%1 %2").arg(u).arg(J::jobKey(job))));
    QCOMPARE(jobs->jobsOf("mac:00:00:00:00:00:00"), J::Jobs(J::AllJobs));
    QCOMPARE(J::jobs().size(), 3);
    // Something saved for another pad changes nothing for these.
    jobs->setJobs("mac:00:00:00:00:00:09", J::NoJobs);
    for (quint32 u = 0; u < 4; ++u)
        for (const J::Job job : J::jobs())
            QVERIFY(jobs->has(u, job));
}

void ControllerJobs_Test::keptWithThisComputer()
{
    J *jobs = m_doc->programmer()->controllerJobs();
    QSignalSpy changed(jobs, &J::jobsChanged);
    const QString a = "mac:aa:bb:cc:00:00:01", b = "port:HID:USB 14200000";
    jobs->setJobs(a, J::FollowSpot);
    jobs->setJobs(b, J::NoJobs);
    QCOMPARE(changed.count(), 2);
    jobs->setJobs(a, J::FollowSpot);            // no change: no signal
    QCOMPARE(changed.count(), 2);
    QCOMPARE(changed.at(0).at(0).toString(), a);

    // Another store (the next run of the app) reads them back.
    {
        J again(m_doc);
        QCOMPARE(again.jobsOf(a), J::Jobs(J::FollowSpot));
        QCOMPARE(again.jobsOf(b), J::Jobs(J::NoJobs));      // none at all is kept as none
        QCOMPARE(again.saved(), QStringList() << a << b);
    }
    QSettings s;
    s.beginGroup(J::settingsGroup());
    QCOMPARE(s.childKeys().size(), 2);
    s.endGroup();

    // All three again: the key goes (the default is no key).
    jobs->setJob(a, J::RunCues, true);
    jobs->setJob(a, J::Focus, true);
    QCOMPARE(jobs->jobsOf(a), J::Jobs(J::AllJobs));
    s.beginGroup(J::settingsGroup());
    QCOMPARE(s.childKeys().size(), 1);
    s.endGroup();
    jobs->resetAll();
    QVERIFY(jobs->saved().isEmpty());
    QVERIFY(J(m_doc).saved().isEmpty());
    // An empty identity (nothing patched) is never saved.
    jobs->setJobs(QString(), J::NoJobs);
    QVERIFY(jobs->saved().isEmpty());
}

void ControllerJobs_Test::aPortWithSlashesIsOneKey()
{
    J *jobs = m_doc->programmer()->controllerJobs();
    const QString onLinux = "port:HID:USB usb-0000:00:14.0-2/input0";
    const QString onWindows = "port:HID:\\\\?\\hid#vid_054c&pid_09cc#7&2a0b&0&0000#{4d1e55b2}";
    jobs->setJobs(onLinux, J::Focus);
    jobs->setJobs(onWindows, J::RunCues);
    J again(m_doc);
    QCOMPARE(again.jobsOf(onLinux), J::Jobs(J::Focus));
    QCOMPARE(again.jobsOf(onWindows), J::Jobs(J::RunCues));
    QSettings s;
    s.beginGroup(J::settingsGroup());
    QCOMPARE(s.childKeys().size(), 2);
    QVERIFY(s.childGroups().isEmpty());          // no '/' made a group of it
}

void ControllerJobs_Test::neverInTheShow()
{
    m_stub->m_lineNames = QStringList() << "Pad (AA:BB:CC:00:00:01)";
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, m_stub->name(), QString(), 0));
    J *jobs = m_doc->programmer()->controllerJobs();
    jobs->setJobs(jobs->identityOfUniverse(0), J::FollowSpot);
    QString xml;
    QXmlStreamWriter w(&xml);
    w.writeStartDocument();
    QVERIFY(m_doc->saveXML(&w));
    w.writeEndDocument();
    QVERIFY(!xml.isEmpty());
    QVERIFY(!xml.contains("controllerJobs", Qt::CaseInsensitive));
    QVERIFY(!xml.contains("mac:aa:bb", Qt::CaseInsensitive));
    // A show opened (a new Doc) has this computer's jobs, not its own.
    Doc other(nullptr);
    QCOMPARE(other.programmer()->controllerJobs()->jobsOf(jobs->identityOfUniverse(0)), J::Jobs(J::FollowSpot));
}

void ControllerJobs_Test::theDescriptionsAPersonReads()
{
    QCOMPARE(J::describe("mac:de:4c:09:77:77:93"), QString("Bluetooth DE:4C:09:77:77:93"));
    QCOMPARE(J::describe("port:HID:USB 14200000"), QString("USB port 14200000"));
    QCOMPARE(J::describe("port:HID:usb-0000:00:14.0-2/input0"), QString("port usb-0000:00:14.0-2/input0"));
    QVERIFY(J::describe("name:HID:Pad").contains("can't be told apart"));
    QCOMPARE(J::describe(QString()), QString());
    QCOMPARE(J::modelName("DUALSHOCK 4 Wireless Controller (DE:4C:09:77:77:93)"),
             QString("DUALSHOCK 4 Wireless Controller"));
    QCOMPARE(J::shortName("DUALSHOCK 4 Wireless Controller (DE:4C:09:77:77:93)", "mac:de:4c:09:77:77:93"),
             QString::fromUtf8("DUALSHOCK 4 (\u202677:93)"));
    QCOMPARE(J::shortName("Wireless Controller", "port:HID:USB 14200000"),
             QString("Wireless Controller (USB port 14200000)"));
    QCOMPARE(J::shortName("Wireless Controller", "name:HID:Wireless Controller"),
             QString("Wireless Controller"));
    for (const J::Job job : J::jobs())
        QVERIFY(!J::jobName(job).isEmpty() && !J::jobKey(job).isEmpty());
}

void ControllerJobs_Test::anIdentityIsWorkedOutOncePerDevice()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    m_stub->m_lineNames = QStringList() << "Wireless Controller" << "Wireless Controller";
    m_stub->m_lineLocations = { { 0, "USB 14200000" }, { 1, "USB 14300000" } };
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 0));
    J *jobs = m_doc->programmer()->controllerJobs();
    const QString first = jobs->identityOfUniverse(0);
    QCOMPARE(first, J::identityOf(m_stub, 0));
    // Asked on every event: the plugin is asked once.
    m_stub->m_locationAsks = 0;
    for (int i = 0; i < 100; ++i)
    {
        QCOMPARE(jobs->identityOfUniverse(0), first);
        QVERIFY(jobs->has(0, J::RunCues));
    }
    QCOMPARE(m_stub->m_locationAsks, 0);
    // Patched to the other pad: that pad.
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 1));
    QCOMPARE(jobs->identityOfUniverse(0), J::identityOf(m_stub, 1));
    // The same pad moved to another port, said by a hot-plug: found again.
    m_stub->m_lineLocations = { { 0, "USB 14200000" }, { 1, "USB 14500000" } };
    emit iom->inputDeviceDisconnected(0, m_stub->name(), "Wireless Controller");
    emit iom->inputDeviceConnected(0, m_stub->name(), "Wireless Controller");
    QCOMPARE(jobs->identityOfUniverse(0), J::identityOf(m_stub, 1));
    QVERIFY(jobs->identityOfUniverse(0).endsWith("USB 14500000"));
    // Unpatched: none.
    QVERIFY(iom->setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine()));
    QCOMPARE(jobs->identityOfUniverse(0), QString());
}

QTEST_MAIN(ControllerJobs_Test)
