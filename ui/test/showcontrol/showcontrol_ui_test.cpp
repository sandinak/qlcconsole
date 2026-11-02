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
#include <QNetworkInterface>
#include <QCryptographicHash>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#include "chasereditor.h"
#include "connectionstree.h"
#include "mastertimer.h"
#undef private
#include "showcontrollistener.h"
#include "showcontrolsyncpage.h"
#include "timecodesource.h"
#include "functionparent.h"
#include "showcontrol.h"
#include "showwebaccess.h"
#include "qrencoder.h"
#include "chaserstep.h"
#include "chaser.h"
#include "scene.h"
#include "inputoutputmap.h"
#include "doc.h"
#include "qlcfile.h"
#include "ioplugincache.h"
#include "../../../engine/test/iopluginstub/iopluginstub.h"
#include "inputpatch.h"

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

/** A free UDP port on this machine for the test to listen on: one nothing
 *  holds on any address, shared or not (the listener binds Any, sharing). */
int freePort()
{
    QUdpSocket probe;
    probe.bind(QHostAddress(QHostAddress::Any), 0, QUdpSocket::DontShareAddress);
    return probe.localPort();
}

/** Turns OSC on for @a listener on a free port and returns the port. A port
 *  free a moment ago can be taken before the listener binds it (another
 *  test program, overlapping gates): then another one is tried. */
int listenOnAFreePort(ShowControlListener &listener)
{
    for (int attempt = 0; attempt < 10; ++attempt)
    {
        ShowControlListener::setSettings(true, freePort(), true, 1);
        listener.apply();
        if (listener.oscState() == ShowControlListener::OscListening)
            return ShowControlListener::oscPort();
        qWarning() << "OSC listener could not bind:" << listener.status();
    }
    return -1;
}

/** However busy the machine (a full gate, several at once), a datagram over
 *  localhost arrives well within this. */
const int kDatagramTimeout = 30000;
}

void ShowControlUi_Test::initTestCase()
{
    // Settings and a home folder of the test's own: never the user's (Doc
    // looks for user scripts and fixtures under $HOME).
    QVERIFY(m_settingsDir.isValid());
    m_realHome = qgetenv("HOME");
    m_realProfile = qgetenv("USERPROFILE");
    qputenv("HOME", m_settingsDir.path().toUtf8());
    qputenv("USERPROFILE", m_settingsDir.path().toUtf8());   // Windows' user folder
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    QCoreApplication::setApplicationName(QStringLiteral("showcontrol_ui_test"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
    QVERIFY(QSettings().fileName().startsWith(m_settingsDir.path()));
}

void ShowControlUi_Test::cleanupTestCase()
{
    qputenv("HOME", m_realHome);
    if (m_realProfile.isEmpty())
        qunsetenv("USERPROFILE");
    else
        qputenv("USERPROFILE", m_realProfile);
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
    int port = freePort();

    ShowControlListener::setSettings(false, port, true, 1);
    ShowControlListener listener(m_doc);
    QVERIFY(listener.status().contains("off"));

    port = listenOnAFreePort(listener);
    QVERIFY2(port > 0, qPrintable(listener.status()));
    QVERIFY2(listener.status().contains(QString::number(port)), qPrintable(listener.status()));

    QUdpSocket qlab;
    qlab.writeDatagram(osc("/cue/2/start"), QHostAddress::LocalHost, quint16(port));
    QTRY_VERIFY_WITH_TIMEOUT(!listener.lastCommand().isEmpty(), kDatagramTimeout);
    QVERIFY2(listener.lastCommand().contains("GO cue 2"), qPrintable(listener.lastCommand()));
    for (int i = 0; i < 3; ++i)
        m_doc->masterTimer()->timerTick();
    QVERIFY(c->isRunning());
    QCOMPARE(c->currentStepIndex(), 1);

    // One that can't be carried out says so.
    qlab.writeDatagram(osc("/cue/9/start"), QHostAddress::LocalHost, quint16(port));
    QTRY_VERIFY_WITH_TIMEOUT(listener.lastCommand().contains("cue 9"), kDatagramTimeout);
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

    // One line per link for Connections ▸ Devices; both off here.
    const QList<QStringList> rows = page.summary();
    QCOMPARE(rows.count(), 5);
    QCOMPARE(rows.at(0).at(0), QString("osc"));
    QCOMPARE(rows.at(0).at(2), QString("Off"));
    QCOMPARE(rows.at(1).at(1), QString("MIDI Show Control in"));
    QCOMPARE(rows.at(1).at(2), QString("Off"));
    QCOMPARE(rows.at(2).at(0), QString("mtc"));
    QCOMPARE(rows.at(3).at(0), QString("web"));
    QCOMPARE(rows.at(3).at(2), QString("Off"));
    QCOMPARE(rows.at(4).at(0), QString("map"));
    QCOMPARE(rows.at(4).at(2), QString("None"));
    QVERIFY(page.findChild<QWidget *>("scQLabOutGroup") != nullptr);       // QLab out, in the show

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
    ShowControlListener::setSettings(false, freePort(), true, 1);
    ShowControlListener listener(m_doc);
    const int port = listenOnAFreePort(listener);
    QVERIFY2(port > 0, qPrintable(listener.status()));
    ShowControlSyncPage page(m_doc);
    page.setListener(&listener);
    QLabel *oscLast = page.findChild<QLabel *>("scOscLast");
    QLabel *mscLast = page.findChild<QLabel *>("scMscLast");

    // Each wait is on what the page shows, for as long as a busy machine
    // could take to hand a datagram over -- never on a fixed delay.
    QUdpSocket qlab;
    qlab.writeDatagram(osc("/cue/2/start"), QHostAddress::LocalHost, quint16(port));
    QTRY_VERIFY2_WITH_TIMEOUT(oscLast->text().contains("GO cue 2"), qPrintable(oscLast->text()),
                              kDatagramTimeout);
    QVERIFY(oscLast->text().contains(QRegularExpression("^\\d\\d:\\d\\d:\\d\\d")));
    QCOMPARE(mscLast->text(), QString("Nothing received yet."));

    // An address that isn't a cue command says what it was.
    qlab.writeDatagram(osc("/qlab/hello"), QHostAddress::LocalHost, quint16(port));
    QTRY_VERIFY2_WITH_TIMEOUT(oscLast->text().contains("/qlab/hello"), qPrintable(oscLast->text()),
                              kDatagramTimeout);
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

    // LTC from the audio input is a source of its own in the same list, and
    // the readout says where the code came from. (Not picked here: that
    // would open the audio input.)
    QVERIFY(source->findData(TimecodeSource::LtcUniverse) > 0);
    tc->updateTimeCode(quint32(TimecodeSource::LtcUniverse), 7200000, 25);
    QTest::qWait(220);
    tc->updateTimeCode(quint32(TimecodeSource::LtcUniverse), 7200040, 25);
    QTRY_VERIFY2(status->text().contains("audio input (LTC)"), qPrintable(status->text()));
    QVERIFY2(status->text().contains("02:00:"), qPrintable(status->text()));
}

/* Art-Net TimeCode in: an Art-Net input is a timecode source in the list
   (named for what it is), code arriving on it is followed, and the readout
   names the universe it came from. The input is the I/O stub standing in for
   the Art-Net plugin. */
void ShowControlUi_Test::anArtNetInputIsOfferedAsATimecodeSourceAndNamed()
{
    QDir dir("../../../engine/test/iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(),
             "the I/O plugin stub did not load (build the iopluginstub target)");
    IOPluginStub *stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_nameOverride = "ArtNet";
    stub->m_lineNames = QStringList() << "192.168.1.5";

    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->universesCount() >= 3);
    iom->setUniverseName(2, "Truss Art-Net");
    QVERIFY(iom->setInputPatch(2, "ArtNet", "192.168.1.5", 0));
    QCOMPARE(iom->inputPatch(2)->pluginName(), QString("ArtNet"));

    ShowControlSyncPage page(m_doc);
    TimecodeSource *tc = m_doc->timecodeSource();
    QComboBox *source = page.findChild<QComboBox *>("scMtcSource");
    QLabel *status = page.findChild<QLabel *>("scMtcStatus");
    QVERIFY(source && status);

    // Offered, named as Art-Net TimeCode on that input; an unpatched universe is not
    const int at = source->findData(2);
    QVERIFY(at > 0);
    QVERIFY2(source->itemText(at).contains("Art-Net TimeCode (192.168.1.5)"),
             qPrintable(source->itemText(at)));
    QCOMPARE(source->findData(0), -1);

    // Picked, and code arrives on it
    emit source->activated(at);
    QCOMPARE(tc->sourceUniverse(), 2);
    for (int i = 0; i < 6; ++i)
    {
        emit stub->timeCodeChanged(UINT_MAX, 0, 3600000 + quint32(i) * 40, 25);
        QTest::qWait(210);
    }
    QVERIFY(tc->isRunning());
    QCOMPARE(tc->lastUniverse(), 2);
    QCOMPARE(tc->fps(), 25);
    QVERIFY2(status->text().contains("01:00:"), qPrintable(status->text()));
    QVERIFY2(status->text().contains("25 fps"), qPrintable(status->text()));
    QVERIFY2(status->text().contains(QString("Truss Art-Net")), qPrintable(status->text()));

    stub->m_nameOverride.clear();
    stub->m_lineNames.clear();
}

void ShowControlUi_Test::manyUniversesRebuildTheListOnce()
{
    ShowControlSyncPage page(m_doc);
    TimecodeSource *tc = m_doc->timecodeSource();
    QComboBox *source = page.findChild<QComboBox *>("scMtcSource");
    QVERIFY(source != nullptr);
    InputOutputMap *iom = m_doc->inputOutputMap();

    // Picked before it exists (a show's setting read before its universes).
    tc->setSourceUniverse(700);
    QElapsedTimer t;
    t.start();
    while (iom->universesCount() < 1000)
        QVERIFY(iom->addUniverse());
    // Added, with nothing rebuilt yet: one pass over 1000 is cheap, 1000
    // passes are not.
    QVERIFY2(t.elapsed() < 5000, qPrintable(QString::number(t.elapsed())));
    QCOMPARE(source->findData(700), -1);

    // Back to the event loop: rebuilt, once, with all of them.
    QTRY_VERIFY(source->findData(700) > 0);
    QCOMPARE(source->currentData().toInt(), 700);
    tc->setSourceUniverse(-1);
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

/* Q84: OSC, MSC and timecode are rows in Connections ▸ Devices; picking
   one shows only its settings beside the tree, and their state follows. */
void ShowControlUi_Test::theRowsInConnectionsDevices()
{
    const int port = freePort();
    ShowControlListener::setSettings(false, port, false, 3);
    ShowControlListener listener(m_doc);
    ConnectionsTree tree(m_doc);
    // The tree's right side takes the page over, as in Connections.
    ShowControlSyncPage &page = *new ShowControlSyncPage(m_doc);
    page.setListener(&listener);
    tree.setShowControlPanel(&page);
    tree.setShowControlRows(page.summary());
    connect(&page, &ShowControlSyncPage::summaryChanged, &tree, [&]() {
        tree.setShowControlRows(page.summary());
    });
    connect(&tree, &ConnectionsTree::showControlRowSelected, &page, &ShowControlSyncPage::showSection);
    tree.refresh();

    QTreeWidget *view = tree.findChild<QTreeWidget *>();
    QVERIFY(view != nullptr);
    QList<QTreeWidgetItem *> found = view->findItems("Show control & sync", Qt::MatchExactly, 0);
    QCOMPARE(found.count(), 1);
    QTreeWidgetItem *branch = found.first();
    QCOMPARE(branch->childCount(), 5);
    QCOMPARE(branch->child(0)->text(0), QString("OSC in"));
    QCOMPARE(branch->child(0)->text(1), QString("Off"));
    QCOMPARE(branch->child(2)->text(0), QString("MIDI timecode"));
    QCOMPARE(branch->child(3)->text(0), QString("Web access"));
    QCOMPARE(branch->child(3)->text(1), QString("Off"));

    // Pick MSC: only its box on the right.
    tree.selectShowControlRow("msc");
    QVERIFY(!page.findChild<QWidget *>("scMscGroup")->isHidden());
    QVERIFY(page.findChild<QWidget *>("scOscGroup")->isHidden());
    QVERIFY(page.findChild<QWidget *>("scMtcGroup")->isHidden());
    // The branch itself: all of them.
    tree.selectShowControlRow(QString());
    QVERIFY(!page.findChild<QWidget *>("scOscGroup")->isHidden());

    // Turning OSC on updates its row, without waiting for a rebuild.
    page.findChild<QCheckBox *>("scOscEnabled")->setChecked(true);
    QTRY_VERIFY(branch->child(0)->text(1).startsWith(QString("Port %1").arg(port)));
    listener.setOscEnabled(false);

    // Web access running: its row says where.
    m_doc->webAccess()->setStatus(ShowWebAccess::Running, QStringLiteral("http://127.0.0.1:9999/"));
    QTRY_COMPARE(branch->child(3)->text(1), QString("Port 9999 · this computer only"));
    m_doc->webAccess()->setStatus(ShowWebAccess::Off, QString());
}

/* No server here (that's the webaccess library's, WebAccess_Test): the box
   edits the show's setting, and shows the state the server reports. */
void ShowControlUi_Test::theWebAccessBoxEditsTheShow()
{
    ShowWebAccess *web = m_doc->webAccess();
    ShowControlSyncPage page(m_doc);
    QCheckBox *on = page.findChild<QCheckBox *>("scWebEnabled");
    QSpinBox *port = page.findChild<QSpinBox *>("scWebPort");
    QCheckBox *network = page.findChild<QCheckBox *>("scWebNetwork");
    QCheckBox *login = page.findChild<QCheckBox *>("scWebLogin");
    QLabel *where = page.findChild<QLabel *>("scWebWhere");
    QLabel *status = page.findChild<QLabel *>("scWebStatus");
    QVERIFY(on && port && network && login && where && status);
    QVERIFY(!on->isChecked());
    QCOMPARE(port->value(), 9999);
    QCOMPARE(status->text(), QString("Off"));
    QVERIFY2(where->text().contains("http://127.0.0.1:9999/") && where->text().contains("this computer only"),
             qPrintable(where->text()));

    m_doc->resetModified();
    on->setChecked(true);
    port->setValue(8200);
    network->setChecked(true);
    login->setChecked(true);
    QVERIFY(web->isEnabled());
    QCOMPARE(web->port(), 8200);
    QVERIFY(web->network());
    QVERIFY(web->login());
    QVERIFY(m_doc->isModified());
    QVERIFY2(where->text().contains("http://127.0.0.1:8200/") && !where->text().contains("this computer only"),
             qPrintable(where->text()));

    // What the server reports.
    web->setStatus(ShowWebAccess::Failed, QString(), QStringLiteral("Port 8200 is in use by another program."));
    QCOMPARE(status->property("webState").toInt(), int(ShowWebAccess::Failed));
    QVERIFY(status->text().contains("in use"));
    QCOMPARE(page.summary().value(3).value(2), QString("Port 8200 · could not start"));
    web->setStatus(ShowWebAccess::Running, QStringLiteral("http://<this computer's address>:8200/"));
    QCOMPARE(status->text(), QString("Running on port 8200, reachable from the network."));
    QCOMPARE(page.summary().value(3).value(2), QString("Port 8200 · network"));
    // Started with -w on another port: the box says that one.
    web->setStatus(ShowWebAccess::CommandLine, QStringLiteral("http://127.0.0.1:8300/"), QStringLiteral("(-w)"));
    QCOMPARE(status->text(), QString("Running on port 8300, this computer only. (-w)"));
    QVERIFY2(where->text().contains("http://127.0.0.1:8300/"), qPrintable(where->text()));
    QCOMPARE(page.summary().value(3).value(2), QString("Port 8300 · this computer only · from -w"));

    // Another show opened: the box follows it.
    m_doc->clearContents();
    QVERIFY(!on->isChecked());
    QCOMPARE(port->value(), 9999);
    QVERIFY(!network->isChecked());
    QVERIFY(!login->isChecked());
    web->setStatus(ShowWebAccess::Off, QString());
}

QTEST_MAIN(ShowControlUi_Test)

/* A QR code reader, test-side and written apart from the encoder's tables
   where it can be: format information from the standard's list (error
   correction M), the function patterns by their rules, the zigzag read, the
   Reed-Solomon check (every block must divide evenly), then the byte-mode
   payload. Returns the text, or a null string with @a why. */
static QString decodeQr(const QVector<quint8> &m, int n, QString *why)
{
    static const int formatM[8] = { 0x5412, 0x5125, 0x5E7C, 0x5B4B, 0x45F9, 0x40CE, 0x4F97, 0x4AA0 };
    const int ver = (n - 17) / 4;
    auto at = [&](int x, int y) { return int(m[y * n + x]); };
    int bits1 = 0, bits2 = 0;
    // Copy one: round the top-left finder; copy two: split over the other two.
    const int c1[15][2] = { { 8, 0 }, { 8, 1 }, { 8, 2 }, { 8, 3 }, { 8, 4 }, { 8, 5 }, { 8, 7 }, { 8, 8 },
                            { 7, 8 }, { 5, 8 }, { 4, 8 }, { 3, 8 }, { 2, 8 }, { 1, 8 }, { 0, 8 } };
    for (int i = 0; i < 15; ++i)
        bits1 |= at(c1[i][0], c1[i][1]) << i;
    for (int i = 0; i < 8; ++i)
        bits2 |= at(n - 1 - i, 8) << i;
    for (int i = 8; i < 15; ++i)
        bits2 |= at(8, n - 15 + i) << i;
    int mask = -1;
    for (int k = 0; k < 8; ++k)
        if (formatM[k] == bits1 && formatM[k] == bits2)
            mask = k;
    if (mask < 0)
    {
        *why = QStringLiteral("format information %1 / %2 is not level M").arg(bits1, 0, 16).arg(bits2, 0, 16);
        return QString();
    }
    if (at(8, n - 8) != 1)
    {
        *why = QStringLiteral("no dark module");
        return QString();
    }

    static const int align[11][4] = { { 0 }, { 0 }, { 6, 18, 0 }, { 6, 22, 0 }, { 6, 26, 0 }, { 6, 30, 0 },
                                      { 6, 34, 0 }, { 6, 22, 38, 0 }, { 6, 24, 42, 0 }, { 6, 26, 46, 0 },
                                      { 6, 28, 50, 0 } };
    auto fixed = [&](int x, int y) {
        if ((x < 9 && y < 9) || (x >= n - 8 && y < 9) || (x < 9 && y >= n - 8))
            return true;
        if (x == 6 || y == 6)
            return true;
        if (ver >= 7 && ((x < 6 && y >= n - 11) || (y < 6 && x >= n - 11)))
            return true;
        int cnt = 0;
        while (align[ver][cnt] != 0)
            ++cnt;
        for (int i = 0; i < cnt; ++i)
            for (int j = 0; j < cnt; ++j)
            {
                if ((i == 0 && j == 0) || (i == 0 && j == cnt - 1) || (i == cnt - 1 && j == 0))
                    continue;
                if (qAbs(x - align[ver][i]) <= 2 && qAbs(y - align[ver][j]) <= 2)
                    return true;
            }
        return false;
    };
    auto masked = [&](int x, int y) {
        switch (mask)
        {
            case 0: return (x + y) % 2 == 0;
            case 1: return y % 2 == 0;
            case 2: return x % 3 == 0;
            case 3: return (x + y) % 3 == 0;
            case 4: return (x / 3 + y / 2) % 2 == 0;
            case 5: return x * y % 2 + x * y % 3 == 0;
            case 6: return (x * y % 2 + x * y % 3) % 2 == 0;
            default: return ((x + y) % 2 + x * y % 3) % 2 == 0;
        }
    };
    QByteArray words;
    int acc = 0, nbits = 0;
    for (int right = n - 1; right >= 1; right -= 2)
    {
        if (right == 6)
            right = 5;
        const bool up = ((right + 1) & 2) == 0;
        for (int v = 0; v < n; ++v)
            for (int j = 0; j < 2; ++j)
            {
                const int x = right - j, y = up ? n - 1 - v : v;
                if (fixed(x, y))
                    continue;
                acc = (acc << 1) | (at(x, y) ^ (masked(x, y) ? 1 : 0));
                if (++nbits == 8)
                {
                    words.append(char(acc));
                    acc = nbits = 0;
                }
            }
    }
    // Level M: blocks and error correction words by version (standard's table).
    static const int eccLen[11] = { 0, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26 };
    static const int nBlocks[11] = { 0, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5 };
    const int total = words.size();
    const int blocks = nBlocks[ver], ecc = eccLen[ver];
    const int shortBlocks = blocks - total % blocks, shortLen = total / blocks;
    QVector<QByteArray> block(blocks);
    int pos = 0;
    for (int i = 0; i < shortLen - ecc + 1; ++i)
        for (int j = 0; j < blocks; ++j)
            if (i < shortLen - ecc || j >= shortBlocks)
                block[j].append(words[pos++]);
    for (int i = 0; i < ecc; ++i)
        for (int j = 0; j < blocks; ++j)
            block[j].append(words[pos++]);
    // GF(256) with 0x11D: each block's syndromes 1, a, a^2... must all be 0.
    quint8 expT[512], logT[256] = {};
    for (int i = 0, x = 1; i < 255; ++i)
    {
        expT[i] = quint8(x);
        logT[x] = quint8(i);
        x <<= 1;
        if (x & 0x100)
            x ^= 0x11D;
    }
    for (int i = 255; i < 512; ++i)
        expT[i] = expT[i - 255];
    QByteArray data;
    for (const QByteArray &b : block)
    {
        for (int root = 0; root < ecc; ++root)
        {
            quint8 sum = 0;
            for (char c : b)
            {
                sum = sum == 0 ? 0 : expT[logT[sum] + root];
                sum ^= quint8(c);
            }
            if (sum != 0)
            {
                *why = QStringLiteral("a block does not check out");
                return QString();
            }
        }
        data.append(b.left(b.size() - ecc));
    }
    // Byte mode: 0100, count (8 bits; 16 from version 10), bytes.
    QVector<bool> bits;
    for (char c : data)
        for (int i = 7; i >= 0; --i)
            bits << (((quint8(c) >> i) & 1) != 0);
    int p = 0;
    auto take = [&](int count) {
        int v = 0;
        for (int i = 0; i < count; ++i)
            v = (v << 1) | (p < bits.size() && bits[p++] ? 1 : 0);
        return v;
    };
    if (take(4) != 4)
    {
        *why = QStringLiteral("not byte mode");
        return QString();
    }
    const int count = take(ver < 10 ? 8 : 16);
    QByteArray text;
    for (int i = 0; i < count; ++i)
        text.append(char(take(8)));
    return QString::fromUtf8(text);
}

/* The phone Run page's address as a QR code: whatever the encoder makes
   reads back (with every mask, and across the versions) as the same text. */
void ShowControlUi_Test::theQrCodeReadsBackAsTheAddress()
{
    const QStringList payloads {
        QStringLiteral("http://192.168.1.20:9999/run"),
        QStringLiteral("http://10.0.0.7:8200/run"),
        QStringLiteral("http://172.18.2.45:9999/run?x=") + QString(60, QLatin1Char('a')),       // a few blocks
        QStringLiteral("http://[fe80::1ff:fe23:4567:890a]:9999/run/") + QString(150, QLatin1Char('Z')),   // version 9-10
        QStringLiteral("caf\u00e9 \u2603"),                                                            // UTF-8 bytes
    };
    int lastVersion = 0;
    for (const QString &url : payloads)
    {
        for (int mask = -1; mask < 8; ++mask)
        {
            int n = 0, ver = 0;
            const QVector<quint8> m = QrEncoder::encode(url, &n, mask, &ver);
            QVERIFY2(!m.isEmpty(), qPrintable(url));
            QCOMPARE(n, ver * 4 + 17);
            QString why;
            const QString back = decodeQr(m, n, &why);
            QVERIFY2(back == url, qPrintable(QStringLiteral("%1 (mask %2): %3").arg(url).arg(mask).arg(why)));
            lastVersion = ver;
        }
    }
    QVERIFY(lastVersion >= 1);
    // Version 10 is reached, and one byte more than it holds is refused.
    int n = 0, ver = 0;
    QVERIFY(!QrEncoder::encode(QString(QrEncoder::MaxBytes, QLatin1Char('x')), &n, -1, &ver).isEmpty());
    QCOMPARE(ver, 10);
    QVERIFY(QrEncoder::encode(QString(QrEncoder::MaxBytes + 1, QLatin1Char('x')), &n).isEmpty());
    QVERIFY(QrEncoder::encode(QString(), &n).isEmpty());

    // A fixed code stays the same code (read back by Apple's CIDetector
    // once, when written, with a longer one of version 4 and one of 10): a hash of its modules.
    const QVector<quint8> fixedCode = QrEncoder::encode(QStringLiteral("http://192.168.1.20:9999/run"), &n);
    QCOMPARE(n, 29);
    QCOMPARE(QString::fromLatin1(QCryptographicHash::hash(QByteArray(reinterpret_cast<const char *>(fixedCode.constData()),
                                                                     fixedCode.size()), QCryptographicHash::Sha1).toHex()),
             QStringLiteral("c96a2998a7bba8f82829d9200662d8f2cd95d07f"));

    const QImage img = QrEncoder::image(QStringLiteral("http://192.168.1.20:9999/run"), 3);
    QCOMPARE(img.width(), (29 + 8) * 3);
    QCOMPARE(qGray(img.pixel(0, 0)), 255);          // the quiet zone
    QCOMPARE(qGray(img.pixel(4 * 3, 4 * 3)), 0);    // the finder's corner
}

/* On the Show Control page the code sits next to "Open:" and carries the
   /run address of a network address -- only when the page is reachable from
   the network and this computer has an address to give. */
void ShowControlUi_Test::theWebBoxShowsTheRunPageAsAQrCode()
{
    ShowWebAccess *web = m_doc->webAccess();
    ShowControlSyncPage page(m_doc);
    QLabel *qr = page.findChild<QLabel *>("scWebQr");
    QCheckBox *network = page.findChild<QCheckBox *>("scWebNetwork");
    QLabel *where = page.findChild<QLabel *>("scWebWhere");
    QVERIFY(qr && network && where);
    QVERIFY(qr->isHidden());                         // this computer only: no use to a phone

    web->setPort(8200);
    network->setChecked(true);
    QString address;
    for (const QHostAddress &a : QNetworkInterface::allAddresses())
        if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback())
        {
            address = a.toString();
            break;
        }
    if (address.isEmpty())
    {
        QVERIFY(qr->isHidden());
        QSKIP("No network address on this computer");
    }
    const QString url = QStringLiteral("http://%1:8200/run").arg(address);
    QVERIFY(!qr->isHidden());
    QCOMPARE(qr->property("qrUrl").toString(), url);
    QVERIFY(qr->toolTip().contains(url));
    QVERIFY(!qr->pixmap().isNull());
    // The picture is the code of that address.
    QCOMPARE(qr->pixmap().toImage().convertToFormat(QImage::Format_RGB32),
             QrEncoder::image(url, 4).convertToFormat(QImage::Format_RGB32));
    network->setChecked(false);
    QVERIFY(qr->isHidden());
}

/* QLab out is the show's own setting: the boxes edit it, it marks the show
   changed, and a page opened on a show that has it shows it. */
void ShowControlUi_Test::theQLabOutBoxEditsTheShow()
{
    ShowControl *sc = m_doc->showControl();
    ShowControlSyncPage page(m_doc);
    QVERIFY(!sc->outSettings().oscEnabled);
    QVERIFY(!sc->outSettings().mscEnabled);
    m_doc->resetModified();

    page.findChild<QLineEdit *>("scOutHost")->setText("10.0.0.7");
    emit page.findChild<QLineEdit *>("scOutHost")->editingFinished();
    page.findChild<QSpinBox *>("scOutPort")->setValue(53001);
    page.findChild<QCheckBox *>("scOutOsc")->setChecked(true);
    page.findChild<QComboBox *>("scOutPath")->setCurrentText("/go");
    page.findChild<QSpinBox *>("scOutDevice")->setValue(4);
    QComboBox *cues = page.findChild<QComboBox *>("scOutCues");
    cues->setCurrentIndex(1);
    emit cues->activated(1);
    const ShowControl::OutSettings o = sc->outSettings();
    QVERIFY(o.oscEnabled);
    QCOMPARE(o.oscHost, QString("10.0.0.7"));
    QCOMPARE(o.oscPort, 53001);
    QCOMPARE(o.oscPath, QString("/go"));
    QCOMPARE(o.mscDevice, 4);
    QCOMPARE(o.cues, ShowControl::OutNumberedCues);
    QVERIFY(m_doc->isModified());

    // Another page on the same show shows what it has.
    ShowControlSyncPage again(m_doc);
    QVERIFY(again.findChild<QCheckBox *>("scOutOsc")->isChecked());
    QCOMPARE(again.findChild<QLineEdit *>("scOutHost")->text(), QString("10.0.0.7"));
    QCOMPARE(again.findChild<QSpinBox *>("scOutPort")->value(), 53001);
    QCOMPARE(again.findChild<QComboBox *>("scOutCues")->currentIndex(), 1);

    // A GO sent says so.
    sc->setOutSender([](const ShowControl::OutMessage &) { return true; });
    Chaser *c = new Chaser(m_doc);
    m_doc->addFunction(c);
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    ChaserStep step(scene->id());
    step.cueNumber = "5";
    c->addStep(step);
    sc->setMainCueList(c->id());
    ShowControl::Request r;
    r.cue = "5";
    QVERIFY(sc->execute(r));
    QVERIFY2(page.findChild<QLabel *>("scOutLast")->text().contains("/go"),
             qPrintable(page.findChild<QLabel *>("scOutLast")->text()));
    c->stop(FunctionParent::master());
}
