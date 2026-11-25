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
#include <QScopeGuard>

#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTreeWidget>

#include "inputdevicenotices_test.h"
#include "inputdevicenotices.h"
#include "inputoutputpatcheditor.h"
#include "showtreecontroller.h"
#include "virtualconsole.h"
#include "controlmap.h"
#include "deskrecord.h"
#include "inputoutputmap.h"
#include "inputpatch.h"
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

/** Answers the repatch warning when it comes up: Patch or Cancel. Returns
 *  how many it answered (by the next turn of the event loop that finds one). */
QTimer *answerRepatch(bool patch, int *answered)
{
    QTimer *t = new QTimer;
    t->setInterval(10);
    QObject::connect(t, &QTimer::timeout, t, [patch, answered]() {
        for (QWidget *w : QApplication::topLevelWidgets())
        {
            QMessageBox *box = qobject_cast<QMessageBox *>(w);
            if (box == nullptr || box->objectName() != QLatin1String("repatchWarning") || !box->isVisible())
                continue;
            for (QAbstractButton *b : box->buttons())
                if ((patch && box->buttonRole(b) == QMessageBox::AcceptRole)
                    || (!patch && box->buttonRole(b) == QMessageBox::RejectRole))
                {
                    ++*answered;
                    b->click();
                    return;
                }
        }
    });
    t->start();
    return t;
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
    // This computer's desk lives in the settings: the test's own, never the user's.
    QVERIFY(m_settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
    QSettings().remove(DeskRecord::settingsGroup());
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
    m_stub->m_lineDeviceKind.clear();
    ShowStatus::instance()->clearStatus(QStringLiteral("input.devices"));
}

void InputDeviceNotices_Test::aPadOfTheSameNameIsNotTakenOverSilently_data()
{
    QTest::addColumn<bool>("otherPatched");
    QTest::newRow("the other pad is no universe's (a spare)") << false;
    QTest::newRow("the other pad is universe 3's") << true;
}

void InputDeviceNotices_Test::aPadOfTheSameNameIsNotTakenOverSilently()
{
    /* Two pads of one name; the one universe 2 is patched to goes, the
       other stays connected. Universe 2 waits for its own device, and says
       so. Only when the other is no universe's is the operator told how to
       use it here -- never to unplug another universe's pad (round-8
       review). */
    QFETCH(bool, otherPatched);
    const QString pad = QStringLiteral("Pad");
    InputOutputMap *iom = m_doc->inputOutputMap();
    m_stub->m_lineNames = QStringList() << pad << pad;
    m_stub->m_deviceIds = { { 0, 11 }, { 1, 12 } };
    QVERIFY(iom->setInputPatch(1, m_stub->name(), QString(), 0));
    if (otherPatched)
        QVERIFY(iom->setInputPatch(2, m_stub->name(), QString(), 1));
    auto restore = qScopeGuard([&]() {
        m_stub->m_deviceIds.clear();
        iom->setInputPatch(2, KInputNone, QString(), QLCIOPlugin::invalidLine());
        iom->forgetOfflinePatches(2);
    });
    InputDeviceNotices notices(m_doc);
    notices.setSettleTime(100);
    notices.settleNow();
    QSignalSpy said(&notices, &InputDeviceNotices::notice);
    // The first goes: the other moves onto line 0.
    m_stub->m_deviceIds = { { 0, 12 } };
    rescanned(m_stub, QStringList() << pad);
    QVERIFY2(iom->inputPatch(1) == nullptr || iom->inputPatch(1)->input() == QLCIOPlugin::invalidLine(),
             "universe 2 took the other pad of that name");
    QTRY_COMPARE(said.count(), 1);
    const QString want = otherPatched
        ? QString("Input device 'Pad' disconnected — universe 2 waits for its own device.")
        : QString("Input device 'Pad' disconnected — universe 2 waits for its own device. Another 'Pad' is "
                  "connected: to use it here, unplug it and plug it back in, or patch it in Inputs/Outputs.");
    QCOMPARE(said.at(0).at(0).toString(), want);
    QVERIFY2(otherPatched == false || said.at(0).at(0).toString().contains("unplug") == false,
             "another universe's pad was offered as a spare");
    const ShowStatus::Entry e = footerEntry();
    QCOMPARE(e.summary, QString("Input device 'Pad' not connected — universe 2 waits for its own device"));
}

void InputDeviceNotices_Test::onlyTheFirstWaitingUniverseIsToldToReplug()
{
    /* Three pads of one name: universes 2 and 3 on the first two, the
       third a spare no universe has. Both patched pads go: both universes
       wait. A spare unplugged and plugged back goes to universe 2, the
       first in order (retryOfflinePatches()): only universe 2 is told to
       replug it; universe 3 only waits (review of c27f096). */
    const QString pad = QStringLiteral("Pad");
    InputOutputMap *iom = m_doc->inputOutputMap();
    m_stub->m_lineNames = QStringList() << pad << pad << pad;
    m_stub->m_deviceIds = { { 0, 11 }, { 1, 12 }, { 2, 13 } };
    QVERIFY(iom->setInputPatch(1, m_stub->name(), QString(), 0));
    QVERIFY(iom->setInputPatch(2, m_stub->name(), QString(), 1));
    auto restore = qScopeGuard([&]() {
        m_stub->m_deviceIds.clear();
        iom->setInputPatch(2, KInputNone, QString(), QLCIOPlugin::invalidLine());
        iom->forgetOfflinePatches(2);
    });
    InputDeviceNotices notices(m_doc);
    notices.setSettleTime(100);
    notices.settleNow();
    QSignalSpy said(&notices, &InputDeviceNotices::notice);
    // Both patched pads go: the spare moves onto line 0.
    m_stub->m_deviceIds = { { 0, 13 } };
    rescanned(m_stub, QStringList() << pad);
    QVERIFY(iom->inputPatch(1) == nullptr || iom->inputPatch(1)->input() == QLCIOPlugin::invalidLine());
    QVERIFY(iom->inputPatch(2) == nullptr || iom->inputPatch(2)->input() == QLCIOPlugin::invalidLine());
    QTRY_COMPARE(said.count(), 1);
    QCOMPARE(said.at(0).at(0).toString(),
             QString("Input device 'Pad' disconnected — universe 2 waits for its own device. Another 'Pad' is "
                     "connected: to use it here, unplug it and plug it back in, or patch it in Inputs/Outputs."
                     " · Input device 'Pad' disconnected — universe 3 waits for its own device."));
    // The advice holds: the spare replugged (a new device) goes to universe 2.
    m_stub->m_deviceIds.clear();
    m_stub->m_lineCount = 0;
    rescanned(m_stub, QStringList());          // unplugged: nothing listed
    m_stub->m_lineCount = 4;
    m_stub->m_deviceIds = { { 0, 14 } };
    rescanned(m_stub, QStringList() << pad);
    QVERIFY2(iom->inputPatch(1) != nullptr && iom->inputPatch(1)->input() == 0,
             "the replugged spare did not go to universe 2");
    QVERIFY(iom->inputPatch(2) == nullptr || iom->inputPatch(2)->input() == QLCIOPlugin::invalidLine());
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

void InputDeviceNotices_Test::aPadAsleepWhileIdleIsSaidAndRepatchedWhenWoken()
{
    /* RC2: an Xbox pad left idle switches itself off -- no tool running,
       the only device on its plugin, so its plugin's list goes empty. Said
       in amber and on the footer, and patched again when it wakes. */
    const QString xbox = QStringLiteral("Xbox Wireless Controller (0b13)");
    InputOutputMap *iom = m_doc->inputOutputMap();
    iom->setInputPatch(1, KInputNone, QString(), QLCIOPlugin::invalidLine());
    rescanned(m_stub, QStringList() << xbox);
    QVERIFY(iom->setInputPatch(1, m_stub->name(), QString(), 0));
    InputDeviceNotices notices(m_doc);
    notices.setSettleTime(50);
    notices.settleNow();
    QSignalSpy said(&notices, &InputDeviceNotices::notice);
    QSignalSpy gone(iom, &InputOutputMap::inputDeviceDisconnected);
    QSignalSpy back(iom, &InputOutputMap::inputDeviceConnected);

    m_stub->m_lineCount = 0;
    rescanned(m_stub, QStringList());          // asleep: nothing listed
    m_stub->m_lineCount = 4;
    QCOMPARE(gone.count(), 1);
    QVERIFY(iom->inputPatch(1) == nullptr);
    QTRY_COMPARE(said.count(), 1);
    QCOMPARE(said.at(0).at(0).toString(),
             QString("Input device '%1' disconnected — universe 2").arg(xbox));
    QCOMPARE(said.at(0).at(1).toBool(), true);  // the amber kind
    QCOMPARE(footerEntry().severity, ShowStatus::Warning);

    rescanned(m_stub, QStringList() << xbox);   // woken
    QCOMPARE(back.count(), 1);
    QVERIFY(iom->inputPatch(1) != nullptr);
    QCOMPARE(iom->inputPatch(1)->input(), quint32(0));
    QTRY_COMPARE(said.count(), 2);
    QVERIFY(footerEntry().key.isEmpty());
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

void InputDeviceNotices_Test::aControllerSlotSaysWhichIsInUse()
{
    const QString ds4 = QStringLiteral("DualShock 4 Wireless Controller (cc-dd)");
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->addInputSlotDevice(1, m_stub->name(), ds4));
    InputDeviceNotices notices(m_doc);
    notices.setSettleTime(50);
    notices.settleNow();
    QSignalSpy said(&notices, &InputDeviceNotices::notice);
    QCOMPARE(notices.devices().first().slotCount, 2);
    QCOMPARE(notices.devices().first().slotPosition, 1);

    // The DualSense off, the DS4 on: no alarm, just which one is in use.
    rescanned(m_stub, QStringList() << "Keyboard" << ds4);
    QTRY_COMPARE(said.count(), 1);
    QCOMPARE(said.at(0).at(0).toString(),
             QString("Input device '%1' in use — universe 2 (2 of 2 in its list)").arg(ds4));
    QCOMPARE(said.at(0).at(1).toBool(), false);
    QVERIFY(footerEntry().key.isEmpty());

    // Both gone: amber, and the footer lists it.
    rescanned(m_stub, QStringList() << "Keyboard");
    QTRY_COMPARE(said.count(), 2);
    QVERIFY2(said.at(1).at(0).toString().contains("none of its 2 controllers"),
             qPrintable(said.at(1).at(0).toString()));
    QCOMPARE(said.at(1).at(1).toBool(), true);
    QCOMPARE(footerEntry().severity, ShowStatus::Warning);
}

void InputDeviceNotices_Test::anotherPadIsOfferedAndAdopted()
{
    const QString xbox = QStringLiteral("Xbox Wireless Controller (0b13)");
    InputOutputMap *iom = m_doc->inputOutputMap();
    InputDeviceNotices notices(m_doc);
    notices.setSettleTime(50);
    notices.settleNow();
    QSignalSpy offered(&notices, &InputDeviceNotices::offer);

    // At the next venue: the DualSense is not here, an Xbox pad is.
    rescanned(m_stub, QStringList() << "Keyboard");
    m_stub->m_lineDeviceKind[1] = "xbox";
    rescanned(m_stub, QStringList() << "Keyboard" << xbox);
    QVERIFY(iom->inputPatch(1) == nullptr);     // never taken by itself
    QTRY_COMPARE(offered.count(), 1);
    QCOMPARE(offered.at(0).at(1).toString(),
             QString("Use '%1' for universe 2 (and remember it)").arg(xbox));
    QVERIFY(offered.at(0).at(0).toString().contains(kPad));
    QCOMPARE(offered.at(0).at(2).toUInt(), quint32(1));
    // Offered once while it stands.
    notices.settleNow();
    QCOMPARE(offered.count(), 1);

    QVERIFY(notices.adopt(1, offered.at(0).at(3).toString(), offered.at(0).at(4).toString()));
    QVERIFY(iom->inputPatch(1) != nullptr);
    QCOMPARE(iom->inputPatch(1)->inputUID(), xbox);
    QCOMPARE(iom->inputSlot(1).count(), 2);
    QCOMPARE(iom->inputSlot(1).at(0).uid, kPad);
    QVERIFY(m_doc->isModified());
    notices.settleNow();
    QCOMPARE(offered.count(), 1);
}

namespace
{
/** The show's controls on universe 2 (where the pad is): the stop, the
 *  Grand Master and GO of the list "Main". Taken away again when it goes. */
struct DeskJobs
{
    explicit DeskJobs(Doc *doc) : m_doc(doc)
    {
        ShowTreeController *st = doc->showTree();
        const quint32 prod = st->addProduction(QStringLiteral("Show"));
        const quint32 main = st->addCueList(prod, QStringLiteral("Main"));
        ControlMap *cm = doc->controlMap();
        cm->setBinding({ 1, 10, ControlMap::emergencyStop(), ControlMap::Toggle });
        cm->setBinding({ 1, 11, ControlMap::cueList(main), ControlMap::Toggle });
        m_vc = new VirtualConsole(nullptr, doc);
        m_vc->restoreGrandMasterInput(1, 12);
    }
    ~DeskJobs()
    {
        m_doc->controlMap()->removeAll();
        delete m_vc;
    }
    Doc *m_doc;
    VirtualConsole *m_vc = nullptr;
};
}

void InputDeviceNotices_Test::namesLostJobs()
{
    DeskJobs jobs(m_doc);
    QCOMPARE(InputDeviceNotices::jobsText(m_doc, 1), QString("Emergency Stop, Grand Master, GO of Main"));
    QVERIFY(InputDeviceNotices::jobsText(m_doc, 2).isEmpty());     // nothing listens there

    InputDeviceNotices notices(m_doc);
    notices.setSettleTime(100);
    notices.settleNow();
    QSignalSpy said(&notices, &InputDeviceNotices::notice);
    rescanned(m_stub, QStringList() << "Keyboard");                 // the pad goes
    QTRY_COMPARE(said.count(), 1);
    QCOMPARE(said.at(0).at(0).toString(),
             QString("Input device '%1' disconnected — universe 2. Not working until it is back: "
                     "Emergency Stop, Grand Master, GO of Main.").arg(kPad));
    QCOMPARE(said.at(0).at(1).toBool(), true);
    // The footer's line says it too.
    const ShowStatus::Entry e = footerEntry();
    QVERIFY2(e.items.join("\n").contains("Emergency Stop, Grand Master, GO of Main"),
             qPrintable(e.items.join("\n")));
}

void InputDeviceNotices_Test::repatchWarnsAndLists()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    QCOMPARE(InputDeviceNotices::repatchText(m_doc, 1, QStringLiteral("Keyboard")), QString());   // nothing listens
    DeskJobs jobs(m_doc);

    const QString moves = InputDeviceNotices::repatchText(m_doc, 1, QStringLiteral("Keyboard"));
    QCOMPARE(moves, QString("Universe 2's controls move from '%1' to 'Keyboard': Emergency Stop, Grand Master, "
                            "GO of Main. '%1' will do none of them.").arg(kPad));
    QVERIFY(InputDeviceNotices::repatchText(m_doc, 1, kPad).isEmpty());   // the same device
    QVERIFY(InputDeviceNotices::repatchText(m_doc, 1, QString()).contains("stop working: Emergency Stop"));
    QVERIFY(InputDeviceNotices::repatchText(m_doc, 2, QStringLiteral("Keyboard")).isEmpty());

    // In Inputs/Outputs: ticking another line asks; Cancel leaves the patch.
    InputOutputPatchEditor editor(nullptr, 1, iom, m_doc);
    QTreeWidget *tree = editor.findChild<QTreeWidget *>(QStringLiteral("m_mapTree"));
    QVERIFY(tree != nullptr);
    QTreeWidgetItem *keyboard = nullptr;
    for (int i = 0; i < tree->topLevelItemCount() && keyboard == nullptr; ++i)
    {
        // One row per plugin line: the plugin's name, then the line's number (hidden).
        QTreeWidgetItem *row = tree->topLevelItem(i);
        if (row->text(0) == m_stub->name() && row->text(6) == QLatin1String("0"))
            keyboard = row;
    }
    QVERIFY2(keyboard != nullptr, "the stub's first line is not in the editor");
    int answered = 0;
    {
        QScopedPointer<QTimer> no(answerRepatch(false, &answered));
        keyboard->setCheckState(2, Qt::Checked);
    }
    QCOMPARE(answered, 1);
    QVERIFY(iom->inputPatch(1) != nullptr);
    QCOMPARE(iom->inputPatch(1)->input(), quint32(1));              // still the pad
    QCOMPARE(keyboard->checkState(2), Qt::Unchecked);

    // Patch: it moves.
    {
        QScopedPointer<QTimer> yes(answerRepatch(true, &answered));
        keyboard->setCheckState(2, Qt::Checked);
    }
    QCOMPARE(answered, 2);
    QVERIFY(iom->inputPatch(1) != nullptr);
    QCOMPARE(iom->inputPatch(1)->input(), quint32(0));
}

void InputDeviceNotices_Test::wakeNoticeSaysSlept()
{
    InputDeviceNotices notices(m_doc);
    notices.setSettleTime(100);
    notices.settleNow();
    QSignalSpy said(&notices, &InputDeviceNotices::notice);
    notices.noteWake(40200);
    QCOMPARE(said.count(), 1);
    QCOMPARE(said.at(0).at(0).toString(), QString("Slept 40 s, finding devices…"));
    QCOMPARE(said.at(0).at(1).toBool(), false);
    notices.noteWake(5 * 60 * 1000);
    QCOMPARE(said.last().at(0).toString(), QString("Slept 5 min, finding devices…"));

    // Nothing patched to a device: nothing to find, nothing said.
    m_doc->inputOutputMap()->setInputPatch(1, KInputNone, QString(), QLCIOPlugin::invalidLine());
    const int before = said.count();
    notices.noteWake(40000);
    QCOMPARE(said.count(), before);
}

void InputDeviceNotices_Test::aDeviceStillMissingAfterAWakeIsSaidAgain()
{
    InputDeviceNotices notices(m_doc);
    notices.setSettleTime(50);
    notices.settleNow();
    QSignalSpy said(&notices, &InputDeviceNotices::notice);

    rescanned(m_stub, QStringList() << "Keyboard");      // the pad unplugged
    notices.settleNow();
    QCOMPARE(said.count(), 1);
    QVERIFY(said.at(0).at(0).toString().contains("disconnected — universe 2"));
    QCOMPARE(said.at(0).at(1).toBool(), true);

    // Nothing changes: nothing more said.
    notices.settleNow();
    QCOMPARE(said.count(), 1);

    // The laptop sleeps; the wake line takes the notice over...
    notices.noteWake(40000);
    QCOMPARE(said.count(), 2);
    QCOMPARE(said.at(1).at(1).toBool(), false);
    // ...and once it settles, the pad still missing is said again, in amber.
    QTRY_COMPARE(said.count(), 3);
    QVERIFY2(said.at(2).at(0).toString().contains(kPad) && said.at(2).at(0).toString().contains("universe 2"),
             qPrintable(said.at(2).at(0).toString()));
    QCOMPARE(said.at(2).at(1).toBool(), true);
    QVERIFY(footerEntry().key.isEmpty() == false);

    // Only once: the next settle with nothing changed says nothing.
    notices.settleNow();
    QCOMPARE(said.count(), 3);
}

void InputDeviceNotices_Test::anIgnoredStopDumpIsSaid()
{
    DeskJobs jobs(m_doc);           // universe 2: STOP on 10, GO of Main on 11
    ControlMap *cm = m_doc->controlMap();
    InputOutputMap *iom = m_doc->inputOutputMap();
    cm->setDumpWindowMs(5000);      // never over while the test runs
    auto restore = qScopeGuard([cm]() { cm->setDumpWindowMs(500); });

    InputDeviceNotices notices(m_doc);
    notices.setSettleTime(60000);   // the replug's own notice: not in this test
    notices.settleNow();
    QSignalSpy said(&notices, &InputDeviceNotices::notice);

    // The desk comes back, its stop at rest, and its dump presses it: ignored,
    // and said where the replug is said (never only in a debug log).
    emit iom->inputDeviceConnected(1, m_stub->name(), kPad);
    cm->handleInput(1, 10, 0);
    cm->handleInput(1, 10, 127);
    QCOMPARE(said.count(), 1);
    QCOMPARE(said.at(0).at(0).toString(),
             QString("Ignored STOP's first value after replug (state dump) — universe 2"));
    QCOMPARE(said.at(0).at(0).toString(), InputDeviceNotices::stateDumpText("STOP", 1));
    QCOMPARE(said.at(0).at(1).toBool(), false);

    // A release and a press after it (a double tap): it fires, nothing said.
    QSignalSpy fired(cm, &ControlMap::fired);
    cm->handleInput(1, 10, 0);
    cm->handleInput(1, 10, 127);
    QCOMPARE(said.count(), 1);
    QCOMPARE(fired.count(), 1);
    cm->handleInput(1, 10, 0);

    // The GO's dump of "pressed" (no release first): ignored and said by name.
    cm->handleInput(1, 11, 127);
    QCOMPARE(said.count(), 2);
    QCOMPARE(said.at(1).at(0).toString(),
             QString("Ignored GO of Main's first value after replug (state dump) — universe 2"));
    QCOMPARE(said.at(1).at(1).toBool(), false);
    // Repeated, it is not said again.
    cm->handleInput(1, 11, 127);
    QCOMPARE(said.count(), 2);
    cm->handleInput(1, 11, 0);
}

namespace
{
/** This computer's STOP on the pad of universe 2, channel 20; cleared when it goes. */
struct DeskStop
{
    explicit DeskStop(Doc *doc) : m_desk(doc->deskRecord())
    {
        DeskRecord::Binding b;
        b.identity = m_desk->identityOfUniverse(1);
        b.device = kPad;
        b.plugin = QStringLiteral("HID");
        b.channel = 20;
        b.control = QStringLiteral("Button 1");
        m_desk->setBinding(DeskRecord::Stop, b);
    }
    ~DeskStop() { m_desk->clear(DeskRecord::Stop); }
    DeskRecord *m_desk;
};
}

void InputDeviceNotices_Test::namesThisComputersStopWhenItsDeviceGoes()
{
    DeskStop stop(m_doc);
    QVERIFY(m_doc->deskRecord()->isLive(DeskRecord::Stop));
    QCOMPARE(InputDeviceNotices::jobsText(m_doc, 1), QString("this computer's STOP"));
    QVERIFY(InputDeviceNotices::jobsText(m_doc, 2).isEmpty());

    InputDeviceNotices notices(m_doc);
    notices.setSettleTime(50);
    notices.settleNow();
    QSignalSpy said(&notices, &InputDeviceNotices::notice);
    rescanned(m_stub, QStringList() << "Keyboard");      // the pad unplugged
    QVERIFY(!m_doc->deskRecord()->isLive(DeskRecord::Stop));
    notices.settleNow();
    QCOMPARE(said.count(), 1);
    QCOMPARE(said.at(0).at(0).toString(),
             QString("Input device '%1' disconnected — universe 2. Not working until it is back: "
                     "this computer's STOP.").arg(kPad));
    QVERIFY2(footerEntry().items.join("\n").contains("this computer's STOP"),
             qPrintable(footerEntry().items.join("\n")));

    // Still missing after a wake: said again, with the STOP named.
    notices.noteWake(40000);
    QTRY_COMPARE(said.count(), 3);
    QVERIFY2(said.at(2).at(0).toString().contains("Not working until it is back: this computer's STOP."),
             qPrintable(said.at(2).at(0).toString()));

    // Back: live there again.
    rescanned(m_stub, QStringList() << "Keyboard" << kPad);
    QTRY_VERIFY(m_doc->deskRecord()->isLive(DeskRecord::Stop));
}

void InputDeviceNotices_Test::repatchSaysTheDeskStaysWithItsDevice()
{
    DeskStop stop(m_doc);
    QVERIFY(m_doc->deskRecord()->isLive(DeskRecord::Stop));
    // Only the desk on that universe: it does not move to the new device.
    QCOMPARE(InputDeviceNotices::repatchText(m_doc, 1, QStringLiteral("Keyboard")),
             QString("This computer's STOP stops working until '%1' is patched again.").arg(kPad));
    QCOMPARE(InputDeviceNotices::repatchText(m_doc, 1, QString()),
             QString("This computer's STOP stops working until '%1' is patched again.").arg(kPad));
    QVERIFY(InputDeviceNotices::repatchText(m_doc, 1, kPad).isEmpty());     // the same device

    // With the show's jobs: they move, the desk's STOP has its own sentence.
    DeskJobs jobs(m_doc);
    QCOMPARE(InputDeviceNotices::repatchText(m_doc, 1, QStringLiteral("Keyboard")),
             QString("Universe 2's controls move from '%1' to 'Keyboard': Emergency Stop, Grand Master, "
                     "GO of Main. '%1' will do none of them. This computer's STOP stops working until "
                     "'%1' is patched again.").arg(kPad));
}

QTEST_MAIN(InputDeviceNotices_Test)
