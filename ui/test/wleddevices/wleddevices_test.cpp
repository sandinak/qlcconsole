/*
  Q Light Controller Plus
  wleddevices_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QTableWidget>
#include <QApplication>
#include <QComboBox>
#include <QSpinBox>
#include <QtTest>

#include "wleddevices_test.h"
#include "wleddevicesdialog.h"
#include "universepatcher.h"
#include "ioplugincache.h"
#include "inputoutputmap.h"
#include "qlcioplugin.h"
#include "outputpatch.h"
#include "patchundo.h"
#include "showstatus.h"
#include "apputil.h"
#include "fixture.h"
#include "doc.h"

typedef WledDevicesDialog::Device Device;

namespace
{
    /* Nothing listens on these: the WLED output may ask, and is refused at
       once, on this machine. Never a real lantern. */
    const QString A = QStringLiteral("127.0.0.1:1");
    const QString B = QStringLiteral("127.0.0.1:2");
    const QString C = QStringLiteral("127.0.0.1:3");

    Device dev(const QString &ip, int address = 0, int channels = 0)
    {
        Device d;
        d.ip = ip;
        d.address = address;
        d.channels = channels;
        return d;
    }

    QString saveIOMap(const InputOutputMap &iom)
    {
        QString out;
        QXmlStreamWriter writer(&out);
        writer.setAutoFormatting(true);
        writer.setAutoFormattingIndent(1);
        iom.saveXML(&writer);
        return out;
    }

    bool loadIOMap(InputOutputMap &iom, const QString &xml)
    {
        QXmlStreamReader reader(xml);
        reader.readNextStartElement();
        return iom.loadXML(reader);
    }

    /* The <Output> elements, each line trimmed. */
    QStringList outputs(const QString &xml)
    {
        QStringList out;
        QString current;
        foreach (QString line, xml.split('\n'))
        {
            line = line.trimmed();
            if (line.startsWith("<Output ") || current.isEmpty() == false)
                current += line;
            if (line.startsWith("</Output") || (line.startsWith("<Output ") && line.endsWith("/>")))
            {
                out << current;
                current.clear();
            }
        }
        return out;
    }

    QString wledShow(const QString &parameters)
    {
        return QString("<InputOutputMap>\n"
                       " <Universe Name=\"Lantern SL\" ID=\"0\">\n"
                       "  <Output Plugin=\"WLED\" UID=\"WLED (JSON over Wi-Fi)\" Line=\"0\">\n"
                       "   <PluginParameters %1/>\n"
                       "  </Output>\n"
                       " </Universe>\n"
                       "</InputOutputMap>\n").arg(parameters);
    }
}

void WledDevices_Test::initTestCase()
{
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
}

void WledDevices_Test::init()
{
    m_doc = new Doc(this);
    m_doc->ioPluginCache()->load(QDir(TEST_PLUGINS_DIR, "*wled*"));
    QVERIFY2(m_doc->ioPluginCache()->plugin("WLED") != nullptr, "the WLED output did not load");
}

void WledDevices_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void WledDevices_Test::parametersReadAndWritten()
{
    // A show from before backups: one device, all it ever said.
    QMap<QString, QVariant> old;
    old["outputIP"] = "172.18.2.50";
    old["wledAddress"] = "1";
    old["wledChannels"] = "15";
    QList<Device> one = WledDevicesDialog::fromParameters(old);
    QCOMPARE(one.count(), 1);
    QCOMPARE(one.first(), dev("172.18.2.50", 1, 15));
    QCOMPARE(WledDevicesDialog::toParameters(one).keys(),
             (QStringList{ "outputIP", "wledAddress", "wledChannels" }));

    // Two backups, the second reading its own channels.
    QList<Device> three = one;
    three << dev("172.18.2.51") << dev("172.18.2.52", 16, 4);
    const QMap<QString, QVariant> p = WledDevicesDialog::toParameters(three);
    QCOMPARE(p.keys(), (QStringList{ "outputIP", "outputIP2", "outputIP3", "wledAddress",
                                      "wledAddress3", "wledChannels", "wledChannels3" }));
    QCOMPARE(WledDevicesDialog::fromParameters(p), three);

    // A backup saying what the primary says is "as the primary".
    QMap<QString, QVariant> same = p;
    same["wledAddress2"] = "1";
    QCOMPARE(WledDevicesDialog::fromParameters(same), three);

    QVERIFY(WledDevicesDialog::isDeviceKey("outputIP"));
    QVERIFY(WledDevicesDialog::isDeviceKey("outputIP16"));
    QVERIFY(WledDevicesDialog::isDeviceKey("wledChannels2"));
    QVERIFY(!WledDevicesDialog::isDeviceKey("outputIP1"));
    QVERIFY(!WledDevicesDialog::isDeviceKey("outputIP17"));
    QVERIFY(!WledDevicesDialog::isDeviceKey("outputUni"));
}

void WledDevices_Test::showsRoundTripByteIdentical_data()
{
    QTest::addColumn<QString>("parameters");
    QTest::addColumn<int>("devices");
    QTest::newRow("one, as before backups")
        << QString("outputIP=\"%1\" wledAddress=\"1\" wledChannels=\"15\"").arg(A) << 1;
    QTest::newRow("one, address never said")
        << QString("outputIP=\"%1\"").arg(A) << 1;
    QTest::newRow("primary and a backup")
        << QString("outputIP=\"%1\" outputIP2=\"%2\" wledAddress=\"1\" wledChannels=\"15\"").arg(A, B) << 2;
    QTest::newRow("two backups, one its own")
        << QString("outputIP=\"%1\" outputIP2=\"%2\" outputIP3=\"%3\" wledAddress=\"1\" "
                   "wledAddress3=\"16\" wledChannels=\"15\" wledChannels3=\"4\"").arg(A, B, C) << 3;
}

/* Through the real WLED output: it is what keeps the parameters of a patch
   it has open, so a key it did not take would be lost on save. */
void WledDevices_Test::showsRoundTripByteIdentical()
{
    QFETCH(QString, parameters);
    QFETCH(int, devices);
    const QString original = wledShow(parameters);

    InputOutputMap iom(m_doc, 1);
    QVERIFY(loadIOMap(iom, original));
    OutputPatch *op = iom.outputPatch(0, 0);
    QVERIFY(op != nullptr && op->isPatched());
    QCOMPARE(op->pluginName(), QString("WLED"));
    QCOMPARE(WledDevicesDialog::fromParameters(op->getPluginParameters()).count(), devices);

    const QString saved = saveIOMap(iom);
    QCOMPARE(outputs(saved), outputs(original));

    // And again, from what was saved.
    InputOutputMap again(m_doc, 1);
    QVERIFY(loadIOMap(again, saved));
    QCOMPARE(saveIOMap(again), saved);

    // Applying the devices it has changes nothing.
    WledDevicesDialog::apply(op, WledDevicesDialog::fromParameters(op->getPluginParameters()));
    QCOMPARE(outputs(saveIOMap(iom)), outputs(original));
}

void WledDevices_Test::devicesAreValidated()
{
    QHash<QString, quint32> elsewhere;
    elsewhere.insert("172.18.2.53", 3);

    QVERIFY(WledDevicesDialog::problem({ dev("172.18.2.50", 1, 15) }, elsewhere).isEmpty());
    QVERIFY(WledDevicesDialog::problem({ dev("172.18.2.50", 1, 15), dev("lantern-2.local") },
                                       elsewhere).isEmpty());
    QVERIFY(WledDevicesDialog::problem({ dev("127.0.0.1:8080", 1, 15) }, elsewhere).isEmpty());
    QVERIFY(WledDevicesDialog::problem({ dev("fe80::1", 1, 15) }, elsewhere).isEmpty());

    QVERIFY(WledDevicesDialog::problem({}, elsewhere).contains("Add the device"));
    QVERIFY(WledDevicesDialog::problem({ dev("172.18.2.50", 1, 15), dev("") }, elsewhere)
                .contains("Backup 1 has no IP"));
    QVERIFY(WledDevicesDialog::problem({ dev("172.18.2.500", 1, 15) }, elsewhere)
                .contains("not an IP address"));
    QVERIFY(WledDevicesDialog::problem({ dev("172.18.2.50:99999", 1, 15) }, elsewhere)
                .contains("not an IP address"));
    QVERIFY(WledDevicesDialog::problem({ dev("172.18.2.50", 1, 15), dev("172.18.2.50") }, elsewhere)
                .contains("listed twice"));
    QVERIFY(WledDevicesDialog::problem({ dev("172.18.2.50", 1, 15), dev("172.18.2.53") }, elsewhere)
                .contains("already sent universe 4"));

    // In the dialog: OK only when there is nothing to say.
    WledDevicesDialog dlg({ dev("172.18.2.50", 1, 15) }, QStringList(), elsewhere);
    QVERIFY(dlg.problemText().isEmpty());
    dlg.addDevice();
    QVERIFY(dlg.problemText().contains("Backup 1 has no IP"));
    dlg.setDeviceIP(1, "172.18.2.53");
    QVERIFY(dlg.problemText().contains("already sent universe 4"));
    dlg.setDeviceIP(1, " 172.18.2.51 ");
    QVERIFY(dlg.problemText().isEmpty());
    QCOMPARE(dlg.devices().at(1).ip, QString("172.18.2.51"));
}

void WledDevices_Test::addReorderAndRemove()
{
    WledDevicesDialog dlg({ dev("172.18.2.50", 1, 15), dev("172.18.2.51"), dev("172.18.2.52", 16, 4) },
                          QStringList(), QHash<QString, quint32>());
    QCOMPARE(dlg.table()->rowCount(), 3);
    QCOMPARE(dlg.table()->verticalHeaderItem(0)->text(), QString("Primary"));
    QCOMPARE(dlg.table()->verticalHeaderItem(2)->text(), QString("Backup 2"));

    // The third to the top: the primary now, with what it read spelled out;
    // the old primary's address and channels go on as the others' "as primary".
    dlg.moveDevice(2, -1);
    dlg.moveDevice(1, -1);
    QCOMPARE(dlg.devices(), (QList<Device>{ dev("172.18.2.52", 16, 4), dev("172.18.2.50", 1, 15),
                                            dev("172.18.2.51", 1, 15) }));
    QSpinBox *addr = qobject_cast<QSpinBox *>(dlg.table()->cellWidget(1, 1));
    QComboBox *ch = qobject_cast<QComboBox *>(dlg.table()->cellWidget(2, 2));
    QVERIFY(addr != nullptr && ch != nullptr);
    QCOMPARE(addr->value(), 1);
    QCOMPARE(ch->currentData().toInt(), 15);

    // Back down: as they were.
    dlg.moveDevice(0, 1);
    dlg.moveDevice(1, 1);
    QCOMPARE(dlg.devices(), (QList<Device>{ dev("172.18.2.50", 1, 15), dev("172.18.2.51"),
                                            dev("172.18.2.52", 16, 4) }));

    // The primary removed: the first backup is the primary, reading what it read.
    dlg.removeDevice(0);
    QCOMPARE(dlg.devices(), (QList<Device>{ dev("172.18.2.51", 1, 15), dev("172.18.2.52", 16, 4) }));
    dlg.removeDevice(1);
    dlg.removeDevice(0);                    // the last one stays: a universe goes somewhere
    QCOMPARE(dlg.devices(), (QList<Device>{ dev("172.18.2.51", 1, 15) }));

    // A backup's own channel count, through its widget.
    dlg.addDevice("172.18.2.52");
    ch = qobject_cast<QComboBox *>(dlg.table()->cellWidget(1, 2));
    QCOMPARE(ch->currentData().toInt(), 0);  // as primary
    ch->setCurrentIndex(ch->findData(3));
    QCOMPARE(dlg.devices().at(1).channels, 3);
}

void WledDevices_Test::editedOnThePatchAsOneUndoStep()
{
    InputOutputMap *io = m_doc->inputOutputMap();
    while (io->universesCount() < 2)
        io->addUniverse();
    QVERIFY(io->setOutputPatch(0, "WLED", "", 0));
    QVERIFY(io->setOutputPatch(1, "WLED", "", 0));
    OutputPatch *op = io->outputPatch(0);
    op->setPluginParameter("outputIP", A);
    op->setPluginParameter("wledAddress", 1);
    op->setPluginParameter("wledChannels", 15);
    io->outputPatch(1)->setPluginParameter("outputIP", C);
    const QMap<QString, QVariant> before = op->getPluginParameters();
    QLCIOPlugin *wled = m_doc->ioPluginCache()->plugin("WLED");

    QStringList seen;
    QTimer::singleShot(0, this, [&]() {
        WledDevicesDialog *dlg = qobject_cast<WledDevicesDialog *>(QApplication::activeModalWidget());
        QVERIFY(dlg != nullptr);
        dlg->addDevice();
        dlg->setDeviceIP(1, C);
        seen << dlg->problemText();         // universe 2's
        dlg->setDeviceIP(1, B);
        seen << dlg->problemText();
        dlg->accept();
    });
    QVERIFY(WledDevicesDialog::edit(m_doc, 0, 0, nullptr));
    QCOMPARE(seen.count(), 2);
    QVERIFY2(seen.first().contains("already sent universe 2"), qPrintable(seen.first()));
    QVERIFY(seen.last().isEmpty());

    // On the patch, and so in the output it is open on.
    QMap<QString, QVariant> after = op->getPluginParameters();
    QCOMPARE(after.value("outputIP2").toString(), B);
    QCOMPARE(after.keys(), (QStringList{ "outputIP", "outputIP2", "wledAddress", "wledChannels" }));
    QCOMPARE(wled->getParameters(0, 0, QLCIOPlugin::Output), after);
    QVERIFY(wled->outputInfo(0).contains("2 devices"));

    // How Connections and the Fixture Manager describe the leg.
    UniversePatcher patcher(m_doc, nullptr);
    QCOMPARE(patcher.outputText(0).value(0), QString("WLED → %1 + 1 backup").arg(A));
    QCOMPARE(UniversePatcher::retargetText("WLED"), QString("WLED Devices / Backup Lanterns…"));

    // One step back: as it was, on the output too.
    QVERIFY(io->patchUndo()->canUndo());
    QVERIFY(io->patchUndo()->undo());
    op = io->outputPatch(0);
    QCOMPARE(op->getPluginParameters(), before);
    QVERIFY(!wled->outputInfo(0).contains("2 devices"));

    // Cancelled: nothing changes, nothing to undo.
    io->patchUndo()->clear();
    QTimer::singleShot(0, this, [&]() {
        if (QDialog *dlg = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
            dlg->reject();
    });
    QVERIFY(!WledDevicesDialog::edit(m_doc, 0, 0, nullptr));
    QVERIFY(!io->patchUndo()->canUndo());
}

void WledDevices_Test::theFooterNamesASilentBackup()
{
    InputOutputMap *io = m_doc->inputOutputMap();
    QVERIFY(io->setOutputPatch(0, "WLED", "", 0));
    Fixture *fx = new Fixture(m_doc);
    fx->setName("Lantern SL");
    fx->setChannels(3);
    fx->setUniverse(0);
    fx->setAddress(0);
    fx->setConnection(Fixture::Network);
    fx->setTransport(Fixture::WledTransport);
    fx->setIPAddress(A);
    QVERIFY(m_doc->addFixture(fx));
    // Neither answers: nothing listens at either, on this machine.
    io->outputPatch(0)->setPluginParameter("outputIP", A);
    io->outputPatch(0)->setPluginParameter("outputIP2", B);
    QLCIOPlugin *wled = m_doc->ioPluginCache()->plugin("WLED");
    QTRY_VERIFY_WITH_TIMEOUT(wled->targetProblems().count() == 2, 12000);
    QVERIFY(wled->targetProblems().first().endsWith("not answering · 0 of 2 answering"));
    QCOMPARE(wled->outputProblems(0, 0),
             QStringList{ QString("0 of 2 answering — %1, %2 not answering").arg(A, B) });

    AppUtil::updateFixtureReadiness(m_doc);
    ShowStatus::Entry wledEntry, outputsEntry;
    foreach (const ShowStatus::Entry &x, ShowStatus::instance()->allEntries())
    {
        if (x.key == "wled.targets")
            wledEntry = x;
        if (x.key == "outputs.problems")
            outputsEntry = x;
    }
    QCOMPARE(wledEntry.key, QString("wled.targets"));
    QCOMPARE(wledEntry.items.count(), 2);
    QVERIFY2(wledEntry.items.at(0).startsWith("Lantern SL (" + A), qPrintable(wledEntry.items.at(0)));
    QVERIFY2(wledEntry.items.at(1).startsWith("Lantern SL backup (" + B), qPrintable(wledEntry.items.at(1)));
    QVERIFY(outputsEntry.key.isEmpty());    // said once, not again as an output problem
}

QTEST_MAIN(WledDevices_Test)
