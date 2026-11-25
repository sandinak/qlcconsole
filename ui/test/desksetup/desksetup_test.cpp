/*
  Q Light Controller Plus - Unit test
  desksetup_test.cpp

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

#include <QTemporaryDir>
#include <QMainWindow>
#include <QPushButton>
#include <QSignalSpy>
#include <QStatusBar>
#include <QCheckBox>
#include <QSettings>
#include <QLabel>
#include <QtTest>
#include <QDir>

#include "desksetup_test.h"
#include "controlsurfacespanel.h"
#include "showcontrolsyncpage.h"
#include "inputoutputmanager.h"
#include "showtreecontroller.h"
#include "connectionstree.h"
#include "desksetuppanel.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "iopluginstub.h"
#include "virtualconsole.h"
#include "showoperate.h"
#include "deskrecord.h"
#include "controlmap.h"
#include "deskchip.h"
#include "qlcfile.h"
#include "doc.h"

using D = DeskRecord;

void DeskSetup_Test::initTestCase()
{
    QVERIFY(m_settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
}

void DeskSetup_Test::init()
{
    QSettings().remove(DeskRecord::settingsGroup());
    DeskSetupPanel::setPinAsker(nullptr);
    DeskSetupPanel::setFileAsker(nullptr);
    m_doc = new Doc(this);
    new VirtualConsole(nullptr, m_doc);
    QDir dir(QStringLiteral("../../../engine/test/iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../../../engine/test/iopluginstub");
    m_stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    m_stub->m_nameOverride = QStringLiteral("MIDI");
    m_stub->m_lineNames = QStringList() << QStringLiteral("APC MINI") << QStringLiteral("nanoKONTROL2");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, m_stub->name(), QString(), 0));
}

void DeskSetup_Test::cleanup()
{
    DeskSetupPanel::setPinAsker(nullptr);
    DeskSetupPanel::setFileAsker(nullptr);
    delete VirtualConsole::instance();
    if (m_stub != nullptr)
    {
        m_stub->m_nameOverride.clear();
        m_stub->m_lineNames.clear();
    }
    delete m_doc;
    m_doc = nullptr;
    m_stub = nullptr;
    QSettings().remove(DeskRecord::settingsGroup());
}

void DeskSetup_Test::send(quint32 universe, quint32 channel, uchar value)
{
    emit m_doc->inputOutputMap()->inputValueChanged(universe, channel, value);
    QCoreApplication::processEvents();
}

void DeskSetup_Test::learnGo()
{
    D *desk = m_doc->deskRecord();
    QVERIFY(desk->learn(D::Go));
    send(0, 202, 127);
    send(0, 202, 0);
    QVERIFY(desk->isLive(D::Go));
}

void DeskSetup_Test::thePanelLearnsEachRole()
{
    DeskSetupPanel panel(m_doc);
    D *desk = m_doc->deskRecord();
    for (D::Role r : D::roles())
    {
        QVERIFY(panel.learnButton(r) != nullptr);
        QCOMPARE(panel.statusLabel(r)->text(), QStringLiteral("not set"));
        QVERIFY(!panel.onBox(r)->isEnabled());
    }
    QVERIFY(!panel.exportButton()->isEnabled());

    // GO: Learn…, a press: learned, and the line says on what.
    panel.learnButton(D::Go)->click();
    QVERIFY(desk->isLearning());
    QCOMPARE(panel.learnButton(D::Go)->text(), QStringLiteral("Cancel"));
    QVERIFY(panel.learnLine()->text().contains("Press the button for GO"));
    send(0, 202, 127);
    send(0, 202, 0);
    QVERIFY(!desk->isLearning());
    QVERIFY2(panel.statusLabel(D::Go)->text().startsWith("APC MINI"), qPrintable(panel.statusLabel(D::Go)->text()));
    QVERIFY(panel.statusLabel(D::Go)->text().contains("Universe 1"));
    QVERIFY(panel.learnLine()->text().startsWith("GO learned"));
    QVERIFY(panel.onBox(D::Go)->isChecked());
    QCOMPARE(panel.learnButton(D::Go)->text(), QStringLiteral("Learn…"));

    // The Grand Master on a button: refused, in words, on the panel.
    panel.learnButton(D::GrandMaster)->click();
    send(0, 203, 127);
    send(0, 203, 0);
    QVERIFY(panel.learnLine()->text().contains("The Grand Master takes a fader"));
    QVERIFY(!desk->isSet(D::GrandMaster));
    send(0, 7, 20);
    send(0, 7, 40);
    QVERIFY(desk->isSet(D::GrandMaster));

    // STOP: one press asks for the second.
    panel.learnButton(D::Stop)->click();
    send(0, 200, 127);
    send(0, 200, 0);
    QCOMPARE(panel.learnLine()->text(), QStringLiteral("Release it and press it again."));
    send(0, 200, 127);
    send(0, 200, 0);
    QVERIFY(desk->isLive(D::Stop));
    QVERIFY(panel.exportButton()->isEnabled());

    // Cancel stops a learn and keeps what was there.
    panel.learnButton(D::Back)->click();
    QVERIFY(desk->isLearning());
    panel.learnButton(D::Back)->click();
    QVERIFY(!desk->isLearning());
    QVERIFY(!desk->isSet(D::Back));
}

/* An open learn takes the next control touched, and the desk's other roles
   went quiet while it listened: it ends with the panel, or by itself. */
void DeskSetup_Test::anOpenLearnEndsWithThePanel()
{
    D *desk = m_doc->deskRecord();
    QWidget host;
    DeskSetupPanel *panel = new DeskSetupPanel(m_doc, &host);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    // Hidden (another tab, Operate's page): the learn ends, nothing learned.
    panel->learnButton(D::Back)->click();
    QVERIFY(desk->isLearning());
    panel->hide();
    QVERIFY2(!desk->isLearning(), "a learn stayed open on a hidden panel");
    send(0, 203, 127);
    send(0, 203, 0);
    QVERIFY(!desk->isSet(D::Back));
    // Left open on screen: it ends by itself, and the panel says so.
    panel->show();
    desk->setLearnTimeoutMs(50);
    panel->learnButton(D::Back)->click();
    QCOMPARE(panel->learnButton(D::Back)->text(), QStringLiteral("Cancel"));
    QTRY_VERIFY(!desk->isLearning());
    QVERIFY2(panel->learnLine()->text().contains("nothing was pressed"), qPrintable(panel->learnLine()->text()));
    QVERIFY(!panel->learnLine()->isHidden());
    QCOMPARE(panel->learnButton(D::Back)->text(), QStringLiteral("Learn…"));
    desk->setLearnTimeoutMs(30000);
}

void DeskSetup_Test::theOnTickTurnsOneRoleOff()
{
    learnGo();
    DeskSetupPanel panel(m_doc);
    D *desk = m_doc->deskRecord();
    QVERIFY(panel.onBox(D::Go)->isEnabled());
    QVERIFY(panel.onBox(D::Go)->isChecked());
    panel.onBox(D::Go)->click();
    QCOMPARE(desk->state(D::Go), D::Off);
    QVERIFY(panel.statusLabel(D::Go)->text().contains("off on this computer"));
    QVERIFY(!m_doc->inputOutputMap()->isStaticControl(0, 202));
    panel.onBox(D::Go)->click();
    QVERIFY(desk->isLive(D::Go));
}

void DeskSetup_Test::thePinGuardsThePanel()
{
    learnGo();
    QVERIFY(m_doc->operateSettings()->setPin(QStringLiteral("1234")));
    DeskSetupPanel panel(m_doc);
    D *desk = m_doc->deskRecord();
    int asked = 0;
    bool answer = false;
    DeskSetupPanel::setPinAsker([&](QWidget *, const QString &why) {
        ++asked;
        return why.contains("PIN") && answer;
    });
    // Wrong (or no) PIN: no learn, the tick stays as it was.
    panel.learnButton(D::Stop)->click();
    QCOMPARE(asked, 1);
    QVERIFY(!desk->isLearning());
    panel.onBox(D::Go)->click();
    QCOMPARE(asked, 2);
    QVERIFY(panel.onBox(D::Go)->isChecked());
    QVERIFY(desk->isLive(D::Go));
    // The PIN: the change goes through. One PIN, one change: the next asks again.
    answer = true;
    panel.onBox(D::Go)->click();
    QCOMPARE(asked, 3);
    QCOMPARE(desk->state(D::Go), D::Off);
    QVERIFY2(desk->isLocked(), "the PIN stayed open after the tick");
    panel.onBox(D::Go)->click();
    QCOMPARE(asked, 4);
    QVERIFY(desk->isLive(D::Go));

    // A learn, then another change: each asks.
    panel.learnButton(D::Stop)->click();
    QCOMPARE(asked, 5);
    QVERIFY(desk->isLearning());
    send(0, 200, 127);
    send(0, 200, 0);
    send(0, 200, 127);
    send(0, 200, 0);
    QVERIFY(desk->isSet(D::Stop));
    QVERIFY2(desk->isLocked(), "the PIN stayed open after the learn");
    answer = false;
    panel.learnButton(D::Back)->click();
    QCOMPARE(asked, 6);
    QVERIFY(!desk->isLearning());
    panel.onBox(D::Go)->click();
    QCOMPARE(asked, 7);
    QVERIFY(desk->isLive(D::Go));
    // An import cancelled at its file dialog leaves it locked.
    answer = true;
    DeskSetupPanel::setFileAsker([](QWidget *, bool) { return QString(); });
    panel.importButton()->click();
    QCOMPARE(asked, 8);
    QVERIFY2(desk->isLocked(), "a cancelled import left the PIN open");
    DeskSetupPanel::setFileAsker(nullptr);
    answer = false;
    panel.importButton()->click();
    QCOMPARE(asked, 9);
}

void DeskSetup_Test::exportAndImportFromThePanel()
{
    learnGo();
    QTemporaryDir dir;
    const QString file = dir.filePath(QStringLiteral("desk.qlcdesk"));
    DeskSetupPanel::setFileAsker([&](QWidget *, bool) { return file; });
    DeskSetupPanel panel(m_doc);
    panel.exportButton()->click();
    QVERIFY(QFile::exists(file));
    D *desk = m_doc->deskRecord();
    QVERIFY(desk->clear(D::Go));
    QVERIFY(!desk->isSet(D::Go));
    panel.importButton()->click();
    QVERIFY(desk->isLive(D::Go));
    QVERIFY(panel.statusLabel(D::Go)->text().startsWith("APC MINI"));
}

void DeskSetup_Test::theChipIsGreenAmberOrRed()
{
    DeskChip chip(m_doc);
    D *desk = m_doc->deskRecord();
    // Nothing learned: no chip.
    QCOMPARE(chip.colour(), DeskChip::Hidden);
    QVERIFY(chip.isHidden());
    learnGo();
    QCOMPARE(chip.colour(), DeskChip::Green);
    QCOMPARE(chip.text(), QStringLiteral("Desk"));
    QCOMPARE(chip.property("deskState").toString(), QStringLiteral("green"));
    // One role off: amber, saying which.
    QVERIFY(desk->setOn(D::Go, false));
    QCOMPARE(chip.colour(), DeskChip::Amber);
    QCOMPARE(chip.text(), QStringLiteral("Desk: GO off"));
    QVERIFY(desk->setOn(D::Go, true));
    QCOMPARE(chip.colour(), DeskChip::Green);
    // The device not in this show's patch: red, with Patch It.
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, m_stub->name(), QString(), 1));
    QCOMPARE(chip.colour(), DeskChip::Red);
    QCOMPARE(chip.text(), QStringLiteral("Desk: APC MINI not patched · Patch It"));
    // Unplugged altogether: amber (Patch It can't help), at once.
    m_stub->m_lineNames = QStringList() << QStringLiteral("nanoKONTROL2");
    desk->rebind();
    QCOMPARE(chip.colour(), DeskChip::Amber);
    QVERIFY(chip.text().contains("not connected"));
    // Plugged in again: red with Patch It, with nothing else to refresh it.
    m_stub->m_lineNames = QStringList() << QStringLiteral("APC MINI") << QStringLiteral("nanoKONTROL2");
    desk->rebind();
    QCOMPARE(chip.colour(), DeskChip::Red);
    // On a universe passing its input through to DMX: amber, Patch It can't help.
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(1, m_stub->name(), QString(), 0));
    m_doc->inputOutputMap()->setUniversePassthrough(1, true);
    QCOMPARE(chip.colour(), DeskChip::Amber);
    QVERIFY(chip.text().contains("passthrough"));
    m_doc->inputOutputMap()->setUniversePassthrough(1, false);
    QCOMPARE(chip.colour(), DeskChip::Green);
}

void DeskSetup_Test::patchItFromTheChipIsNamedAndUndone()
{
    learnGo();
    DeskChip chip(m_doc);
    QSignalSpy said(&chip, &DeskChip::said);
    QSignalSpy setUp(&chip, &DeskChip::setUpRequested);
    D *desk = m_doc->deskRecord();
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, KInputNone, QString(), QLCIOPlugin::invalidLine()));
    QCOMPARE(chip.colour(), DeskChip::Red);
    chip.click();
    QVERIFY(desk->isLive(D::Go));
    QCOMPARE(said.count(), 1);
    QCOMPARE(said.first().first().toString(), QStringLiteral("Patched APC MINI to Universe 1's input."));
    QCOMPARE(chip.text(), QStringLiteral("Desk: patched APC MINI · Undo"));
    QCOMPARE(setUp.count(), 0);
    // Undo, from the same chip.
    chip.click();
    QCOMPARE(desk->state(D::Go), D::NotPatched);
    QCOMPARE(said.count(), 2);
    QVERIFY(said.last().first().toString().contains("APC MINI"));
    QCOMPARE(chip.colour(), DeskChip::Red);
    // Patch It again; once the Undo time is over, a click opens the page.
    chip.setUndoMs(10);
    chip.click();
    QTRY_COMPARE(chip.text(), QStringLiteral("Desk"));
    chip.click();
    QCOMPARE(setUp.count(), 1);
    QVERIFY(desk->isLive(D::Go));

    // Patch It, then another show loaded: its Undo is gone, never run on that show.
    chip.setUndoMs(15000);
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, KInputNone, QString(), QLCIOPlugin::invalidLine()));
    QCOMPARE(chip.colour(), DeskChip::Red);
    chip.click();
    QVERIFY(chip.text().endsWith(QStringLiteral("Undo")));
    const quint32 count = m_doc->inputOutputMap()->universesCount();
    emit m_doc->loaded();
    QVERIFY2(!chip.text().contains(QStringLiteral("Undo")), "the chip offered Undo in another show");
    chip.click();
    QVERIFY(desk->isLive(D::Go));
    QCOMPARE(m_doc->inputOutputMap()->universesCount(), count);
    QCOMPARE(setUp.count(), 2);
}

void DeskSetup_Test::theChipNamesWhatGoPlays()
{
    learnGo();
    DeskChip chip(m_doc);
    QVERIFY(chip.toolTip().contains("GO: the Run tab has no cue list"));
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction(QStringLiteral("Show"));
    const quint32 list = st->addCueList(prod, QStringLiteral("Act 2 main"));
    m_doc->controlMap()->setRunTabList(list);
    QVERIFY2(chip.toolTip().contains(QString::fromUtf8("GO → Act 2 main")), qPrintable(chip.toolTip()));
    QVERIFY(chip.toolTip().contains("This computer's desk"));
    QVERIFY(chip.toolTip().contains("GO: APC MINI"));
}

void DeskSetup_Test::theHubShowsTheDeskFirst()
{
    ControlSurfacesPanel hub(m_doc);
    DeskSetupPanel *desk = hub.deskPanel();
    QVERIFY(desk != nullptr);
    QCOMPARE(desk->parentWidget(), &hub);
    // Who has it now names a show trigger the desk blocks (Q309).
    learnGo();
    ControlMap *cm = m_doc->controlMap();
    const quint32 prod = m_doc->showTree()->addProduction(QStringLiteral("Show"));
    const quint32 list = m_doc->showTree()->addCueList(prod, QStringLiteral("Act 1"));
    cm->setBinding({ 0, 202, ControlMap::cueListBack(list), ControlMap::Toggle });
    hub.refresh();
    const QString who = hub.status(QStringLiteral("collisions"));
    QVERIFY2(who.contains("blocked on this computer"), qPrintable(who));
    QVERIFY(who.contains("this computer's GO"));
}

void DeskSetup_Test::deskShots()
{
    const QByteArray env = qgetenv("QLC_SHOT_DIR");
    if (env.isEmpty())
        QSKIP("QLC_SHOT_DIR is not set");
    const QDir out(QString::fromLocal8Bit(env));
    QDir().mkpath(out.absolutePath());
    // A desk: STOP and GO on the APC (patched), the GM on the nanoKONTROL (not in this show).
    D *desk = m_doc->deskRecord();
    QVERIFY(desk->learn(D::Stop));
    send(0, 200, 127);
    send(0, 200, 0);
    send(0, 200, 127);
    send(0, 200, 0);
    learnGo();
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(1, m_stub->name(), QString(), 1));
    QVERIFY(desk->learn(D::GrandMaster));
    send(1, 7, 30);
    send(1, 7, 60);
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(1, KInputNone, QString(), QLCIOPlugin::invalidLine()));
    QCOMPARE(desk->state(D::GrandMaster), D::NotPatched);

    InputOutputManager mgr(nullptr, m_doc);
    const QList<QSize> sizes = { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) };
    for (const QSize &sz : sizes)
    {
        mgr.resize(sz);
        mgr.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mgr));
        mgr.showShowControlSection(QStringLiteral("surfaces"));
        QCoreApplication::processEvents();
        QVERIFY(mgr.grab().save(out.absoluteFilePath(QString("desk-surfaces-%1x%2.png").arg(sz.width()).arg(sz.height()))));
    }
    mgr.hide();

    // The footer: the chip red, with Patch It, at the right of a status bar.
    QMainWindow win;
    DeskChip *chip = new DeskChip(m_doc, &win);
    QLabel *mode = new QLabel(QStringLiteral("DESIGN"), &win);
    win.statusBar()->addPermanentWidget(chip);
    win.statusBar()->addPermanentWidget(mode);
    win.setCentralWidget(new QWidget(&win));
    for (const QSize &sz : sizes)
    {
        win.resize(sz);
        win.show();
        QVERIFY(QTest::qWaitForWindowExposed(&win));
        QCoreApplication::processEvents();
        QCOMPARE(chip->colour(), DeskChip::Red);
        QVERIFY(chip->isVisible());
        QVERIFY(win.grab().save(out.absoluteFilePath(QString("desk-footer-%1x%2.png").arg(sz.width()).arg(sz.height()))));
    }
    win.hide();

    /* Amber: the GM's box patched on a universe passing its input through
       to DMX (the panel's row says why); green: passthrough off, all live. */
    InputOutputMap *iom = m_doc->inputOutputMap();
    iom->setUniversePassthrough(1, true);
    QVERIFY(iom->setInputPatch(1, m_stub->name(), QString(), 1));
    QVERIFY(desk->isOnPassthrough(D::GrandMaster));
    QCOMPARE(chip->colour(), DeskChip::Amber);
    QVERIFY(chip->text().contains("passthrough"));
    for (const QSize &sz : sizes)
    {
        mgr.resize(sz);
        mgr.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mgr));
        mgr.showShowControlSection(QStringLiteral("surfaces"));
        QCoreApplication::processEvents();
        QVERIFY(mgr.grab().save(out.absoluteFilePath(
            QString("desk-surfaces-passthrough-%1x%2.png").arg(sz.width()).arg(sz.height()))));
    }
    mgr.hide();
    for (const QSize &sz : sizes)
    {
        win.resize(sz);
        win.show();
        QVERIFY(QTest::qWaitForWindowExposed(&win));
        QCoreApplication::processEvents();
        QCOMPARE(chip->colour(), DeskChip::Amber);
        QVERIFY(win.grab().save(out.absoluteFilePath(
            QString("desk-footer-amber-%1x%2.png").arg(sz.width()).arg(sz.height()))));
    }
    iom->setUniversePassthrough(1, false);
    QVERIFY(desk->isLive(D::GrandMaster));
    QCOMPARE(chip->colour(), DeskChip::Green);
    for (const QSize &sz : sizes)
    {
        win.resize(sz);
        win.show();
        QVERIFY(QTest::qWaitForWindowExposed(&win));
        QCoreApplication::processEvents();
        QCOMPARE(chip->colour(), DeskChip::Green);
        QVERIFY(win.grab().save(out.absoluteFilePath(
            QString("desk-footer-green-%1x%2.png").arg(sz.width()).arg(sz.height()))));
    }
    for (const QSize &sz : sizes)
    {
        mgr.resize(sz);
        mgr.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mgr));
        mgr.showShowControlSection(QStringLiteral("surfaces"));
        QCoreApplication::processEvents();
        QVERIFY(mgr.grab().save(out.absoluteFilePath(
            QString("desk-surfaces-live-%1x%2.png").arg(sz.width()).arg(sz.height()))));
    }
}

QTEST_MAIN(DeskSetup_Test)
