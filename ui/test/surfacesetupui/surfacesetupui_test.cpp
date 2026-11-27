/*
  Q Light Controller Plus
  surfacesetupui_test.cpp

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

#include <QApplication>
#include <QPushButton>
#include <QSettings>
#include <QLabel>
#include <QScrollArea>
#include <QDir>
#include <QtTest>

#include "surfacesetupui_test.h"
#include "simmididevice.h"
#include "simopendeck.h"
#include "surfacesetupdialog.h"
#include "controlsurfacespanel.h"
#include "showcontrolsyncpage.h"
#include "inputoutputmanager.h"
#include "connectionstree.h"
#include "surfacesetup.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "qlcfile.h"
#include "doc.h"

namespace
{
const QString kPmj = QStringLiteral("PMJ Black 1");

QString profile(const QString &file)
{
    return QStringLiteral(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("inputprofiles/") + file;
}

void settle()
{
    for (int i = 0; i < 3; ++i)
    {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    }
}

QPushButton *button(QWidget *w, const char *name)
{
    return w->findChild<QPushButton *>(QString::fromLatin1(name));
}

QString label(QWidget *w, const char *name)
{
    QLabel *l = w->findChild<QLabel *>(QString::fromLatin1(name));
    return l != nullptr ? l->text() : QStringLiteral("(no %1)").arg(QString::fromLatin1(name));
}
}

void SurfaceSetupUi_Test::initTestCase()
{
    QVERIFY(m_backups.isValid());
    SurfaceSetup::setBackupDir(m_backups.path());
}

void SurfaceSetupUi_Test::init()
{
    m_doc = new Doc(this);
    QDir dir(QStringLiteral("../../../engine/test/iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub");
    m_stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    m_stub->m_feedback.clear();
    m_stub->m_feedbackParams.clear();
    m_stub->m_feedbackLines.clear();
    m_stub->m_lineNames.clear();
    m_stub->m_showControlSent.clear();
}

void SurfaceSetupUi_Test::cleanup()
{
    m_stub->m_lineNames.clear();
    m_stub->m_linesAreHardware = false;
    m_stub->m_networkDmx = true;
    m_stub->m_hasFeedback = false;
    m_stub->m_onShowControl = nullptr;
    m_stub->m_onRescan = nullptr;
    m_stub->m_showControlSent.clear();
    delete m_doc;
    m_doc = nullptr;
    m_stub = nullptr;
    QSettings().remove(QStringLiteral("surfacesetup"));
    QDir(m_backups.path()).removeRecursively();
    QDir().mkpath(m_backups.path());
}

void SurfaceSetupUi_Test::theDialogChecksAlignsAndUndoes()
{
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj, profile("PMJ-Black-1.qxi"));
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    SimOpenDeck deck(m_stub, &pmj);

    SurfaceSetupDialog d(m_doc);
    d.show();
    QVERIFY(QTest::qWaitForWindowExposed(&d));
    settle();
    // Opening checks (a read): factory order, no lamp under its key.
    QVERIFY2(d.statusText().startsWith("54 of 54 lamps aren't under their buttons"), qPrintable(d.statusText()));
    QCOMPARE(deck.writes(), 0);
    QCOMPARE(label(&d, "surfaceSetupDeviceName"), QString("PMJ Black 1 (universe 1)"));
    QPushButton *align = button(&d, "surfaceSetupAlign");
    QPushButton *undo = button(&d, "surfaceSetupUndo");
    QVERIFY(align->isVisible() && align->isEnabled());
    QVERIFY(undo->isVisible() && !undo->isEnabled());
    QVERIFY(button(&d, "surfaceSetupSend")->isHidden());
    QVERIFY(label(&d, "surfaceSetupBackup").isEmpty());

    align->click();
    settle();
    QVERIFY2(d.statusText().startsWith("All 54 lamps are under their buttons"), qPrintable(d.statusText()));
    QCOMPARE(deck.litBy(9, 0), 17);         // Go lights the lamp under Go
    QVERIFY2(label(&d, "surfaceSetupBackup").contains(m_backups.path()), qPrintable(label(&d, "surfaceSetupBackup")));
    QVERIFY(!align->isEnabled());
    QVERIFY(undo->isEnabled());

    undo->click();
    settle();
    QVERIFY2(d.statusText().startsWith("54 of 54"), qPrintable(d.statusText()));
    QCOMPARE(deck.litBy(9, 0), -1);
    QVERIFY(!undo->isEnabled());
    QVERIFY(label(&d, "surfaceSetupError").isEmpty());
}

void SurfaceSetupUi_Test::inOperateTheDialogAsksFirst()
{
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj, profile("PMJ-Black-1.qxi"));
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    SimOpenDeck deck(m_stub, &pmj);
    m_doc->setMode(Doc::Operate);
    SurfaceSetupDialog d(m_doc);
    d.show();
    settle();
    d.setConfirmInOperate(false);
    button(&d, "surfaceSetupAlign")->click();
    settle();
    QCOMPARE(deck.writes(), 0);
    QVERIFY2(label(&d, "surfaceSetupError").contains("Nothing was written"), qPrintable(label(&d, "surfaceSetupError")));
    d.setConfirmInOperate(true);
    button(&d, "surfaceSetupAlign")->click();
    settle();
    QCOMPARE(deck.writes(), 54 * 4);
    m_doc->setMode(Doc::Design);
}

void SurfaceSetupUi_Test::identifyPairsByPress()
{
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj, profile("PMJ-Black-1.qxi"));
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    SimOpenDeck deck(m_stub, &pmj);
    const auto before = deck.tables();
    SurfaceSetupDialog d(m_doc);
    d.show();
    settle();
    QCOMPARE(d.identifyLamp(), -1);
    button(&d, "surfaceSetupIdentify")->click();
    settle();
    QCOMPARE(d.identifyLamp(), 0);
    QVERIFY(d.findChild<QWidget *>("surfaceSetupIdentifyBox")->isVisible());
    QVERIFY(label(&d, "surfaceSetupIdentifyText").startsWith("Lamp 1 of 128 is flashing"));
    // Every lamp answers its own note on channel 1 while identifying.
    QCOMPARE(deck.litBy(1, 0), 0);
    // Lamp 1: nothing; lamp 2: the person presses Go under it.
    button(&d, "surfaceSetupSkip")->click();
    QCOMPARE(d.identifyLamp(), 1);
    QVERIFY(pmj.noteOn(9, 0, 127));
    settle();
    QCOMPARE(d.identifyLamp(), 2);
    QVERIFY(label(&d, "surfaceSetupIdentifyText").contains("1 paired so far"));
    // Go again: it has its lamp, nothing moves on.
    QVERIFY(pmj.noteOff(9, 0));
    QVERIFY(pmj.noteOn(9, 0, 127));
    settle();
    QCOMPARE(d.identifyLamp(), 2);
    button(&d, "surfaceSetupFinish")->click();
    settle();
    QCOMPARE(d.identifyLamp(), -1);
    QCOMPARE(deck.tables(), before);
    const QList<SurfaceSetup::LampPair> pairs = m_doc->surfaceSetup()->pairing(0);
    QCOMPARE(pairs.size(), 1);
    QCOMPARE(pairs.first().led, 1);
    QCOMPARE(pairs.first().note, 0);
    QCOMPARE(pairs.first().channel, 9);
}

void SurfaceSetupUi_Test::anInitOnlySurfaceOffersSendAgain()
{
    const QString apc = QStringLiteral("Akai APC40 mkII");
    SimMidiDevice board(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, apc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(board.plug(), qPrintable(board.lastError()));
    SurfaceSetupDialog d(m_doc);
    d.show();
    settle();
    QVERIFY2(d.statusText().contains("Ableton mode is sent when it connects"), qPrintable(d.statusText()));
    QVERIFY(button(&d, "surfaceSetupAlign")->isHidden());
    QVERIFY(button(&d, "surfaceSetupIdentify")->isHidden());
    QVERIFY(button(&d, "surfaceSetupMode")->isHidden());      // no mode to press for
    QPushButton *send = button(&d, "surfaceSetupSend");
    QVERIFY(send->isVisible());
    m_stub->m_showControlSent.clear();
    send->click();
    QCOMPARE(m_stub->m_showControlSent.size(), 1);
    QCOMPARE(QString::fromLatin1(m_stub->m_showControlSent.first().second.toHex(' ')),
             QString("f0 47 7f 29 60 00 04 41 09 00 05 f7"));
}

void SurfaceSetupUi_Test::theHubRowAndRescan()
{
    ControlSurfacesPanel hub(m_doc);
    QCOMPARE(hub.status("setup"), QString("All patched controllers are set up · Rescan to look for a new one"));
    QVERIFY(hub.setUpButton("setup") != nullptr);
    QCOMPARE(hub.setUpButton("setup")->text(), QString("Set Up…"));
    QVERIFY(hub.rescanButton() != nullptr);

    // Nothing comes on: said, with what to check.
    m_stub->m_linesAreHardware = true;
    hub.rescanButton()->click();
    QVERIFY2(hub.status("setup").contains("No new controller found. The computer itself doesn't see one"),
             qPrintable(hub.status("setup")));
    QVERIFY(m_stub->m_rescanCalled > 0);

    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj, profile("PMJ-Black-1.qxi"));
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    SimOpenDeck deck(m_stub, &pmj);
    settle();
    QVERIFY2(hub.status("setup").contains("PMJ Black 1 (universe 1): Not checked yet."), qPrintable(hub.status("setup")));
    m_doc->surfaceSetup()->check(0);
    QVERIFY2(hub.status("setup").contains("54 of 54 lamps aren't under their buttons"), qPrintable(hub.status("setup")));
}

void SurfaceSetupUi_Test::patchFeedbackIsOneClick()
{
    m_stub->m_hasFeedback = true;
    m_stub->m_linesAreHardware = true;      // a USB/MIDI line, not a network one
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj, profile("PMJ-Black-1.qxi"));
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    SimOpenDeck deck(m_stub, &pmj);
    // plug() patches the feedback line as a person does: take it off.
    QVERIFY(m_doc->inputOutputMap()->setOutputPatch(0, QString(), QString(), 0, true, 0));
    QVERIFY(m_doc->inputOutputMap()->feedbackPatch(0) == nullptr);

    ControlSurfacesPanel hub(m_doc);
    hub.show();
    QVERIFY(hub.pmjFeedbackButton() != nullptr);
    QVERIFY(hub.pmjFeedbackButton()->isVisibleTo(&hub));
    QVERIFY2(hub.status("pmj").contains("no feedback line"), qPrintable(hub.status("pmj")));

    SurfaceSetupDialog d(m_doc);
    d.show();
    QVERIFY(QTest::qWaitForWindowExposed(&d));
    settle();
    QPushButton *dlgBtn = button(&d, "surfaceSetupPatchFeedback");
    QVERIFY(dlgBtn != nullptr && dlgBtn->isVisibleTo(&d));

    const QString shotDir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (shotDir.isEmpty() == false)
    {
        QDir().mkpath(shotDir);
        const QList<QSize> sizes { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) };
        for (const QSize &z : sizes)
        {
            hub.resize(z);
            d.resize(z.width() / 2, qMax(320, z.height() / 2));
            settle();
            hub.grab().save(QStringLiteral("%1/feedback-hub-%2x%3.png").arg(shotDir).arg(z.width()).arg(z.height()));
            d.grab().save(QStringLiteral("%1/feedback-setup-%2x%3.png").arg(shotDir).arg(z.width()).arg(z.height()));
        }
    }
    // The dialog's button first: one click, and it is gone.
    dlgBtn->click();
    settle();
    QVERIFY(m_doc->inputOutputMap()->feedbackPatch(0) != nullptr);
    QVERIFY(dlgBtn->isHidden());
    // Unpatched again: the hub's button pairs it the same way.
    QVERIFY(m_doc->inputOutputMap()->setOutputPatch(0, QString(), QString(), 0, true, 0));
    settle();
    QVERIFY(m_doc->inputOutputMap()->feedbackPatch(0) == nullptr);
    QVERIFY(hub.pmjFeedbackButton()->isVisibleTo(&hub));
    hub.pmjFeedbackButton()->click();
    settle();
    QVERIFY(m_doc->inputOutputMap()->feedbackPatch(0) != nullptr);
    QVERIFY(hub.pmjFeedbackButton()->isHidden());
    QVERIFY2(hub.status("pmj").contains("with its feedback line"), qPrintable(hub.status("pmj")));
}

void SurfaceSetupUi_Test::everyBoardOffersPatchFeedback()
{
    m_stub->m_hasFeedback = true;
    SimMidiDevice fp(m_doc, m_stub, QStringLiteral("FaderPort 2"), 0, QStringLiteral("FaderPort 2"),
                     profile("PreSonus-FaderPort-2.qxi"));
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40"), 1, QStringLiteral("APC40"),
                      profile("Akai-APC40.qxi"));
    QVERIFY2(fp.plug(), qPrintable(fp.lastError()));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setOutputPatch(0, QString(), QString(), 0, true, 0));
    QVERIFY(iom->setOutputPatch(1, QString(), QString(), 0, true, 0));

    ControlSurfacesPanel hub(m_doc);
    hub.show();
    settle();
    QPushButton *b = hub.setUpButton("feedback");
    QVERIFY(b != nullptr && b->isVisibleTo(&hub));
    QVERIFY2(hub.status("feedback").contains("Universe 1:") && hub.status("feedback").contains("Universe 2:"),
             qPrintable(hub.status("feedback")));
    QSignalSpy spy(&hub, &ControlSurfacesPanel::patchFeedbackRequested);
    b->click();
    settle();
    QVERIFY(iom->feedbackPatch(0) != nullptr && iom->feedbackPatch(1) != nullptr);
    QCOMPARE(spy.count(), 0);
    QVERIFY(b->isHidden());
}

void SurfaceSetupUi_Test::noUniqueMatchLandsOnConnections()
{
    m_stub->m_hasFeedback = true;
    SimMidiDevice fp(m_doc, m_stub, QStringLiteral("FaderPort 2"), 0, QStringLiteral("FaderPort 2"),
                     profile("PreSonus-FaderPort-2.qxi"));
    QVERIFY2(fp.plug(), qPrintable(fp.lastError()));
    // A second output of the same name: no single match to pick.
    m_stub->m_lineNames << QStringLiteral("FaderPort 2");
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setOutputPatch(0, QString(), QString(), 0, true, 0));
    QVERIFY(iom->feedbackPatch(0) == nullptr);

    InputOutputManager mgr(nullptr, m_doc);
    mgr.show();
    settle();
    ControlSurfacesPanel *panel = mgr.findChild<ControlSurfacesPanel *>();
    ConnectionsTree *tree = mgr.findChild<ConnectionsTree *>();
    QVERIFY(panel != nullptr && tree != nullptr);
    QPushButton *b = panel->setUpButton("feedback");
    QVERIFY(b != nullptr);
    b->click();
    settle();
    QVERIFY(iom->feedbackPatch(0) == nullptr);              // nothing guessed
    QCOMPARE(tree->selectedUniverse(), quint32(0));          // the board's input is selected
    QCOMPARE(tree->noteText(), QString::fromUtf8("No single output matches this board. Right-click "
                 "the board's output \u25b8 Patch a Universe Here (feedback)\u2026"));
}

void SurfaceSetupUi_Test::shots()
{
    const QByteArray env = qgetenv("QLC_SHOT_DIR");
    if (env.isEmpty())
        QSKIP("QLC_SHOT_DIR is not set");
    const QDir dir(QString::fromLocal8Bit(env));
    QDir().mkpath(dir.absolutePath());
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj, profile("PMJ-Black-1.qxi"));
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    SimOpenDeck deck(m_stub, &pmj);
    for (const QSize &sz : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
    {
        const QString tag = QString("%1x%2").arg(sz.width()).arg(sz.height());
        {
            SurfaceSetupDialog d(m_doc);
            d.resize(qMin(sz.width() - 80, 560 + (sz.width() - 1024) / 4), 300);
            d.show();
            QVERIFY(QTest::qWaitForWindowExposed(&d));
            settle();
            QVERIFY(d.grab().save(dir.absoluteFilePath(QString("setup-dialog-check-%1.png").arg(tag))));
            button(&d, "surfaceSetupAlign")->click();
            settle();
            QVERIFY(d.grab().save(dir.absoluteFilePath(QString("setup-dialog-aligned-%1.png").arg(tag))));
            button(&d, "surfaceSetupUndo")->click();
            settle();
            button(&d, "surfaceSetupIdentify")->click();
            settle();
            QVERIFY(d.grab().save(dir.absoluteFilePath(QString("setup-dialog-identify-%1.png").arg(tag))));
            button(&d, "surfaceSetupFinish")->click();
            settle();
        }
        InputOutputManager mgr(nullptr, m_doc);
        mgr.resize(sz);
        mgr.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mgr));
        mgr.showShowControlPage();
        mgr.findChild<ConnectionsTree *>()->selectShowControlRow(QStringLiteral("surfaces"));
        settle();
        if (QPushButton *r = mgr.findChild<QPushButton *>("surfaceRescan"))
            r->click();
        settle();
        // The Controller setup row in view.
        if (QScrollArea *scroll = mgr.findChild<ShowControlSyncPage *>()->findChild<QScrollArea *>())
            scroll->ensureWidgetVisible(mgr.findChild<QWidget *>("surfaceRescan"), 50, 200);
        settle();
        QVERIFY(mgr.grab().save(dir.absoluteFilePath(QString("setup-hub-%1.png").arg(tag))));
    }
}

QTEST_MAIN(SurfaceSetupUi_Test)
