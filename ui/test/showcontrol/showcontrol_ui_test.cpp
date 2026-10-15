/*
  Q Light Controller Plus - Test Unit
  showcontrol_ui_test.cpp

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

#include <QNetworkDatagram>
#include <QHostAddress>
#include <QUdpSocket>
#include <QTreeWidget>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QRegularExpression>
#include <QSettings>
#include <QtTest>

#define private public
#include "chasereditor.h"
#include "mastertimer.h"
#undef private
#include "showcontrollistener.h"
#include "showcontrolsyncpage.h"
#include "timecodesource.h"
#include "functionparent.h"
#include "showcontrol.h"
#include "chaserstep.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

#include "showcontrol_ui_test.h"

#define COL_CUE 8

namespace
{
QByteArray osc(const QByteArray &address)
{
    QByteArray b = address;
    do b.append('\0'); while (b.size() % 4);
    b.append(',');
    do b.append('\0'); while (b.size() % 4);
    return b;
}

QByteArray msc(int device, int command, const QByteArray &cue)
{
    QByteArray b;
    b.append(char(0xF0)).append(char(0x7F)).append(char(device)).append(char(0x02))
     .append(char(0x01)).append(char(command)).append(cue).append(char(0xF7));
    return b;
}

/** A free UDP port on this machine for the test to listen on. */
int freePort()
{
    QUdpSocket probe;
    probe.bind(QHostAddress(QHostAddress::LocalHost), 0);
    return probe.localPort();
}
}

void ShowControlUi_Test::initTestCase()
{
    // Settings and a home folder of the test's own: never the user's (Doc
    // looks for user scripts and fixtures under $HOME).
    QVERIFY(m_settingsDir.isValid());
    m_realHome = qgetenv("HOME");
    qputenv("HOME", m_settingsDir.path().toUtf8());
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    QCoreApplication::setApplicationName(QStringLiteral("showcontrol_ui_test"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
    QVERIFY(QSettings().fileName().startsWith(m_settingsDir.path()));
}

void ShowControlUi_Test::cleanupTestCase()
{
    qputenv("HOME", m_realHome);
}

void ShowControlUi_Test::init()
{
    m_doc = new Doc(this);
}

void ShowControlUi_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

Chaser *ShowControlUi_Test::makeList(int steps)
{
    Chaser *c = new Chaser(m_doc);
    c->setName("Main");
    c->setDurationMode(Chaser::Common);
    c->setDuration(Function::infiniteSpeed());
    m_doc->addFunction(c);
    for (int i = 0; i < steps; ++i)
    {
        Scene *s = new Scene(m_doc);
        m_doc->addFunction(s);
        c->addStep(ChaserStep(s->id()));
    }
    return c;
}

/* QLab's Network cue sends OSC to this computer: with the listener on, a
   GO by cue number plays that cue. Off, nothing listens. */
void ShowControlUi_Test::oscFromTheNetworkFiresTheCue()
{
    Chaser *c = makeList(3);
    const int port = freePort();

    ShowControlListener::setSettings(false, port, true, 1);
    ShowControlListener listener(m_doc);
    QVERIFY(listener.status().contains("off"));

    ShowControlListener::setSettings(true, port, true, 1);
    listener.apply();
    QVERIFY2(listener.status().contains(QString::number(port)), qPrintable(listener.status()));

    QUdpSocket qlab;
    qlab.writeDatagram(osc("/cue/2/start"), QHostAddress::LocalHost, quint16(port));
    QTRY_VERIFY(!listener.lastCommand().isEmpty());
    QVERIFY2(listener.lastCommand().contains("GO cue 2"), qPrintable(listener.lastCommand()));
    for (int i = 0; i < 3; ++i)
        m_doc->masterTimer()->timerTick();
    QVERIFY(c->isRunning());
    QCOMPARE(c->currentStepIndex(), 1);

    // One that can't be carried out says so.
    qlab.writeDatagram(osc("/cue/9/start"), QHostAddress::LocalHost, quint16(port));
    QTRY_VERIFY(listener.lastCommand().contains("cue 9"));
    QVERIFY(listener.lastCommand().contains("not done"));

    // Off again: the port is let go.
    ShowControlListener::setSettings(false, port, true, 1);
    listener.apply();
    QUdpSocket again;
    QVERIFY(again.bind(QHostAddress(QHostAddress::Any), quint16(port)));

    c->stop(FunctionParent::master());
    for (int i = 0; i < 3; ++i)
        m_doc->masterTimer()->timerTick();
}

/* The chaser editor: a step's cue typed in its Cue column, the rest showing
   their position; Renumber; the cue-list number and the Main cue list. */
void ShowControlUi_Test::theChaserEditorSetsCuesAndTheList()
{
    Chaser *c = makeList(3);
    ChaserEditor ed(nullptr, c, m_doc);
    QTreeWidget *tree = ed.m_tree;
    QCOMPARE(tree->headerItem()->text(COL_CUE), QString("Cue"));
    QCOMPARE(tree->header()->visualIndex(COL_CUE), 1);      // beside the step number

    QCOMPARE(tree->topLevelItem(1)->text(COL_CUE), QString("2"));
    QVERIFY(c->steps().at(1).cueNumber.isEmpty());        // shown, not stored

    tree->topLevelItem(1)->setText(COL_CUE, "12.5");
    QCOMPARE(c->steps().at(1).cueNumber, QString("12.5"));
    QCOMPARE(c->findCue("12.5"), 1);
    QVERIFY(c->steps().at(0).cueNumber.isEmpty());

    QPushButton *renumber = ed.findChild<QPushButton *>("chaserRenumberCues");
    QVERIFY(renumber != nullptr);
    renumber->click();
    QCOMPARE(c->steps().at(1).cueNumber, QString("2"));
    QCOMPARE(c->steps().at(2).cueNumber, QString("3"));
    QCOMPARE(ed.m_tree->topLevelItem(2)->text(COL_CUE), QString("3"));

    QLineEdit *list = ed.findChild<QLineEdit *>("chaserCueList");
    QVERIFY(list != nullptr);
    list->setText("4");
    emit list->editingFinished();
    QCOMPARE(c->cueListNumber(), QString("4"));
    QCOMPARE(m_doc->showControl()->cueList("4"), c);

    QCheckBox *main = ed.findChild<QCheckBox *>("chaserMainCueList");
    QVERIFY(main != nullptr);
    QVERIFY(!main->isChecked());
    main->setChecked(true);
    QCOMPARE(m_doc->showControl()->mainCueList(), c->id());
    main->setChecked(false);
    QCOMPARE(m_doc->showControl()->mainCueList(), Function::invalidId());

    // Unticking a step skips it; ticking puts it back.
    QCOMPARE(ed.m_tree->topLevelItem(0)->checkState(0), Qt::Checked);
    ed.m_tree->topLevelItem(0)->setCheckState(0, Qt::Unchecked);
    QVERIFY(c->isStepSkipped(0));
    QVERIFY(ed.m_tree->topLevelItem(0)->font(1).strikeOut());
    QCOMPARE(c->firstPlayableStep(), 1);
    ed.m_tree->topLevelItem(0)->setCheckState(0, Qt::Checked);
    QVERIFY(!c->isStepSkipped(0));
}

/* Connections' Show control & sync page shows the settings as saved --
   the same keys the Settings dialog wrote, so nothing is lost. */
void ShowControlUi_Test::thePageShowsTheCurrentSettings()
{
    const int port = freePort();
    ShowControlListener::setSettings(false, port, false, 7);
    ShowControlListener listener(m_doc);
    ShowControlSyncPage page(m_doc);
    page.setListener(&listener);

    QCheckBox *oscOn = page.findChild<QCheckBox *>("scOscEnabled");
    QSpinBox *oscPort = page.findChild<QSpinBox *>("scOscPort");
    QCheckBox *mscOn = page.findChild<QCheckBox *>("scMscEnabled");
    QSpinBox *device = page.findChild<QSpinBox *>("scMscDevice");
    QLabel *status = page.findChild<QLabel *>("scOscStatus");
    QVERIFY(oscOn && oscPort && mscOn && device && status);
    QVERIFY(!oscOn->isChecked());
    QCOMPARE(oscPort->value(), port);
    QVERIFY(!mscOn->isChecked());
    QCOMPARE(device->value(), 7);
    QVERIFY(!device->isEnabled());              // nothing to answer as, MSC off
    QVERIFY(status->text().contains("off"));
    QCOMPARE(page.findChild<QLabel *>("scOscLast")->text(), QString("Nothing received yet."));
    QCOMPARE(page.findChild<QLabel *>("scMscLast")->text(), QString("Nothing received yet."));

    // No MIDI input patched: it says so, and offers to patch one.
    QVERIFY(page.findChild<QLabel *>("scMscInputs")->text().contains("No MIDI input"));
    QPushButton *patch = page.findChild<QPushButton *>("scPatchMidi");
    QVERIFY(patch != nullptr);
    QSignalSpy wantsPatch(&page, &ShowControlSyncPage::patchMidiInputRequested);
    patch->click();
    QCOMPARE(wantsPatch.count(), 1);

    // QLab out: a place for it, nothing to press yet.
    QWidget *out = page.findChild<QWidget *>("scQLabOutGroup");
    QVERIFY(out != nullptr);
    QVERIFY(out->findChildren<QAbstractButton *>().isEmpty());
    QVERIFY(out->findChildren<QSpinBox *>().isEmpty());

    // With no listener yet (the page is built before it), nothing to edit.
    ShowControlSyncPage early(m_doc);
    QVERIFY(!early.findChild<QCheckBox *>("scOscEnabled")->isEnabled());
    QCOMPARE(early.findChild<QSpinBox *>("scOscPort")->value(), port);
}

/* Editing on the page saves the setting and puts it into effect at once. */
void ShowControlUi_Test::editingThePageChangesTheListener()
{
    const int port = freePort();
    ShowControlListener::setSettings(false, port, true, 1);
    ShowControlListener listener(m_doc);
    ShowControlSyncPage page(m_doc);
    page.setListener(&listener);
    ShowControl *sc = m_doc->showControl();

    QCheckBox *oscOn = page.findChild<QCheckBox *>("scOscEnabled");
    QLabel *status = page.findChild<QLabel *>("scOscStatus");
    oscOn->setChecked(true);
    QVERIFY(ShowControlListener::oscEnabled());
    QCOMPARE(listener.oscState(), ShowControlListener::OscListening);
    QVERIFY2(status->text().contains(QString::number(port)), qPrintable(status->text()));

    const int other = freePort();
    page.findChild<QSpinBox *>("scOscPort")->setValue(other);
    QCOMPARE(ShowControlListener::oscPort(), other);
    QVERIFY2(status->text().contains(QString::number(other)), qPrintable(status->text()));
    // The old port is let go.
    QUdpSocket again;
    QVERIFY(again.bind(QHostAddress(QHostAddress::Any), quint16(port)));

    page.findChild<QSpinBox *>("scMscDevice")->setValue(12);
    QCOMPARE(ShowControlListener::mscDeviceId(), 12);
    QCOMPARE(sc->mscDeviceId(), 12);
    page.findChild<QCheckBox *>("scMscEnabled")->setChecked(false);
    QVERIFY(!ShowControlListener::mscEnabled());
    QVERIFY(!sc->mscEnabled());
    QVERIFY(!page.findChild<QSpinBox *>("scMscDevice")->isEnabled());

    // And the other way: a change through the listener shows on the page.
    listener.setOscEnabled(false);
    QVERIFY(!oscOn->isChecked());
    QVERIFY(status->text().contains("off"));
}

/* What arrived last, and when, by the way it came. */
void ShowControlUi_Test::aCommandReceivedShowsAsLast()
{
    Chaser *c = makeList(3);
    m_doc->showControl()->setMainCueList(c->id());
    const int port = freePort();
    ShowControlListener::setSettings(true, port, true, 1);
    ShowControlListener listener(m_doc);
    ShowControlSyncPage page(m_doc);
    page.setListener(&listener);
    QLabel *oscLast = page.findChild<QLabel *>("scOscLast");
    QLabel *mscLast = page.findChild<QLabel *>("scMscLast");

    QUdpSocket qlab;
    qlab.writeDatagram(osc("/cue/2/start"), QHostAddress::LocalHost, quint16(port));
    QTRY_VERIFY2(oscLast->text().contains("GO cue 2"), qPrintable(oscLast->text()));
    QVERIFY(oscLast->text().contains(QRegularExpression("^\\d\\d:\\d\\d:\\d\\d")));
    QCOMPARE(mscLast->text(), QString("Nothing received yet."));

    // An address that isn't a cue command says what it was.
    qlab.writeDatagram(osc("/qlab/hello"), QHostAddress::LocalHost, quint16(port));
    QTRY_VERIFY2(oscLast->text().contains("/qlab/hello"), qPrintable(oscLast->text()));
    QVERIFY(oscLast->text().contains("not a cue command"));

    // MSC, as it arrives from a patched MIDI input.
    m_doc->showControl()->handleSysEx(0, msc(1, 0x01, QByteArray("3")));
    QVERIFY2(mscLast->text().contains("GO cue 3"), qPrintable(mscLast->text()));
    QVERIFY(mscLast->text().contains("Universe 1"));
    QVERIFY(oscLast->text().contains("/qlab/hello"));       // untouched
    // One for another device isn't ours.
    const QString before = mscLast->text();
    m_doc->showControl()->handleSysEx(0, msc(5, 0x01, QByteArray("1")));
    QCOMPARE(mscLast->text(), before);

    // GO pressed here is neither.
    m_doc->showControl()->execute({ ShowControl::Go, QString(), QString() });
    QCOMPARE(mscLast->text(), before);
    QVERIFY(oscLast->text().contains("/qlab/hello"));

    c->stop(FunctionParent::master());
    for (int i = 0; i < 3; ++i)
        m_doc->masterTimer()->timerTick();
}

/* Something else already has the port: the page says it can't listen. */
void ShowControlUi_Test::aPortInUseShowsAnError()
{
    QUdpSocket blocker;
    QVERIFY(blocker.bind(QHostAddress(QHostAddress::Any), 0, QUdpSocket::DontShareAddress));
    const int port = blocker.localPort();

    ShowControlListener::setSettings(false, port, true, 1);
    ShowControlListener listener(m_doc);
    ShowControlSyncPage page(m_doc);
    page.setListener(&listener);
    QLabel *status = page.findChild<QLabel *>("scOscStatus");

    page.findChild<QCheckBox *>("scOscEnabled")->setChecked(true);
    QCOMPARE(listener.oscState(), ShowControlListener::OscError);
    QCOMPARE(status->property("oscState").toInt(), int(ShowControlListener::OscError));
    QVERIFY2(status->text().contains("Can't listen") && status->text().contains(QString::number(port)),
             qPrintable(status->text()));

    // Freed, and asked again (the port set again): listening.
    blocker.close();
    listener.apply();
    QCOMPARE(listener.oscState(), ShowControlListener::OscListening);
    QCOMPARE(status->property("oscState").toInt(), int(ShowControlListener::OscListening));
}

/* MIDI timecode: the source the footer chip picks, and the feed's health. */
void ShowControlUi_Test::theTimecodeSourceAndHealth()
{
    ShowControlSyncPage page(m_doc);
    TimecodeSource *tc = m_doc->timecodeSource();
    QComboBox *source = page.findChild<QComboBox *>("scMtcSource");
    QLabel *health = page.findChild<QLabel *>("scMtcHealth");
    QLabel *status = page.findChild<QLabel *>("scMtcStatus");
    QVERIFY(source && health && status);
    QCOMPARE(source->currentData().toInt(), -1);
    QCOMPARE(health->property("health").toInt(), int(TimecodeSource::NoSource));
    QVERIFY(status->text().contains("None received"));

    // Code arrives on universe 2: it joins the list, and is measured.
    for (int i = 0; i < 6; ++i)
    {
        tc->updateTimeCode(1, 3600000 + quint32(i) * 40, 25);
        QTest::qWait(40);
        QTest::qWait(170);          // past the readout's throttle
    }
    QVERIFY(tc->isRunning());
    QVERIFY(health->property("health").toInt() != int(TimecodeSource::NoSource));
    QVERIFY2(status->text().contains("01:00:"), qPrintable(status->text()));
    const int at = source->findData(1);
    QVERIFY(at > 0);

    // Picking it here is the same as picking it on the chip.
    emit source->activated(at);
    QCOMPARE(tc->sourceUniverse(), 1);
    tc->setSourceUniverse(-1);
    QCOMPARE(source->currentData().toInt(), -1);
}

/* "Listen on": all MIDI inputs, or one port by name -- kept machine-wide,
   and a port not patched in this show is said plainly. */
void ShowControlUi_Test::mscOnOneChosenInput()
{
    ShowControlListener::setSettings(false, freePort(), true, 1);
    QSettings().remove("showcontrol/mscInputPlugin");
    QSettings().remove("showcontrol/mscInput");
    ShowControlListener listener(m_doc);
    ShowControlSyncPage page(m_doc);
    page.setListener(&listener);
    QComboBox *on = page.findChild<QComboBox *>("scMscInput");
    QLabel *inputs = page.findChild<QLabel *>("scMscInputs");
    QVERIFY(on && inputs);
    QCOMPARE(on->currentIndex(), 0);                         // all, by default
    QCOMPARE(on->currentText(), QString("All MIDI inputs"));
    QVERIFY(inputs->property("mscInputPatched").toBool());

    // Chosen (here through the listener, as the page's picker does).
    listener.setMscInput("MIDI", "IAC Driver Bus 1");
    QCOMPARE(QSettings().value("showcontrol/mscInputPlugin").toString(), QString("MIDI"));
    QCOMPARE(QSettings().value("showcontrol/mscInput").toString(), QString("IAC Driver Bus 1"));
    QCOMPARE(m_doc->showControl()->mscInput(), QString("IAC Driver Bus 1"));
    QCOMPARE(on->currentData().toStringList(), QStringList({ "MIDI", "IAC Driver Bus 1" }));
    // Not patched in this show: said so, and nothing quietly answers.
    QVERIFY(!inputs->property("mscInputPatched").toBool());
    QVERIFY2(inputs->text().contains("isn't patched"), qPrintable(inputs->text()));
    QVERIFY(m_doc->showControl()->mscUniverses().isEmpty());

    // A new page (the next run) reads it back.
    ShowControlSyncPage again(m_doc);
    again.setListener(&listener);
    QComboBox *on2 = again.findChild<QComboBox *>("scMscInput");
    QCOMPARE(on2->currentData().toStringList(), QStringList({ "MIDI", "IAC Driver Bus 1" }));

    // Back to all, on the page.
    emit on->activated(0);
    QVERIFY(!QSettings().contains("showcontrol/mscInput"));
    QVERIFY(m_doc->showControl()->mscInput().isEmpty());
    QVERIFY(inputs->property("mscInputPatched").toBool());
    QCOMPARE(on2->currentIndex(), 0);
}

QTEST_MAIN(ShowControlUi_Test)
