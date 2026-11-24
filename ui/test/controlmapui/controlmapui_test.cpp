/*
  Q Light Controller Plus - Unit test
  controlmapui_test.cpp

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

#include <QDialogButtonBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QScopeGuard>
#include <QApplication>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QKeyEvent>
#include <QLabel>
#include <QTimer>
#include <QMenu>
#include <QtTest>

#include "controlmapui_test.h"
#include "outputrulekeeper.h"
#include "showcontrolsyncpage.h"
#include "controllearndialog.h"
#include "programmingoutline.h"
#include "showtreecontroller.h"
#include "virtualconsole/virtualconsole.h"
#include "virtualconsole/vcbutton.h"
#include "virtualconsole/vcframe.h"
#include "controlmappanel.h"
#include "controlsurfacespanel.h"
#include "connectionstree.h"
#include "inputoutputmanager.h"
#include "speedmasterspanel.h"
#include "showfeatures.h"
#include "showwebaccess.h"
#include <QTreeWidget>
#include <QScrollArea>
#include <QGroupBox>
#include "showtreeview.h"
#include "keytriggers.h"
#include "cuerunner.h"
#include "inputoutputmap.h"
#include "iopluginstub.h"
#include "qlcioplugin.h"
#include "qlcfile.h"
#include <QDir>
#include "qlcinputprofile.h"
#include "qlcinputchannel.h"
#include "controlmap.h"
#include "scene.h"
#include "doc.h"

namespace
{
/** The objectNames of the actions of the context menu @a open pops up
 *  (the menu is closed without choosing anything). */
QStringList menuActions(const std::function<void()> &open)
{
    QStringList names;
    QTimer driver;
    driver.setInterval(10);
    QObject::connect(&driver, &QTimer::timeout, [&]() {
        QMenu *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (menu == nullptr)
            return;
        driver.stop();
        for (QAction *a : menu->actions())
            names << a->objectName();
        menu->close();
    });
    driver.start();
    open();
    return names;
}
}

void ControlMapUi_Test::init()
{
    m_doc = new Doc(this);
}

void ControlMapUi_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

Scene *ControlMapUi_Test::scene(const QString &name)
{
    Scene *s = new Scene(m_doc);
    s->setName(name);
    m_doc->addFunction(s);
    return s;
}

void ControlMapUi_Test::theLearnDialogBindsWhatItHears()
{
    Scene *a = scene("Warm");
    ControlMap *cm = m_doc->controlMap();
    ControlLearnDialog d(m_doc, ControlMap::function(a->id()));
    d.show();
    QVERIFY(cm->isListening());
    QVERIFY(!d.hasControl());
    QPushButton *ok = d.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
    QVERIFY(!ok->isEnabled());
    QVERIFY(d.findChild<QLabel *>("learnTarget")->text().contains("Warm"));
    // No input patched: it says where to patch one.
    QVERIFY(d.findChild<QLabel *>("learnNote")->text().contains("patch"));

    cm->handleInput(0, 188, 127);
    QVERIFY(d.hasControl());
    QCOMPARE(d.channel(), 188u);
    QVERIFY(ok->isEnabled());
    QVERIFY(cm->isListening());                     // another press changes it
    QVERIFY(cm->isEmpty());                         // nothing bound before OK
    cm->handleInput(0, 188, 0);
    cm->handleInput(0, 190, 64);
    QCOMPARE(d.channel(), 190u);
    QVERIFY(d.findChild<QLabel *>("learnHeard")->text().contains("Channel 191"));

    QComboBox *mode = d.findChild<QComboBox *>("learnMode");
    QCOMPARE(mode->count(), 4);
    d.setMode(ControlMap::Momentary);
    ok->click();
    QVERIFY(!cm->isListening());
    const ControlMap::Binding *b = cm->binding(ControlMap::function(a->id()));
    QVERIFY(b != nullptr);
    QCOMPARE(b->channel, 190u);
    QCOMPARE(b->mode, ControlMap::Momentary);

    // Cancel binds nothing and stops listening.
    Scene *c = scene("Cool");
    {
        ControlLearnDialog d2(m_doc, ControlMap::function(c->id()));
        d2.show();
        cm->handleInput(0, 5, 127);
        // A control already in use: it says what it fires now.
        cm->handleInput(0, 5, 0);
        cm->handleInput(0, 190, 127);
        QVERIFY(d2.findChild<QLabel *>("learnHeard")->text().contains("Warm"));
        d2.reject();
    }
    QVERIFY(!cm->isListening());
    QCOMPARE(cm->count(), 1);

    // A cue list's modes: GO, GO while held, Stop.
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("Show"), "Act 1");
    ControlLearnDialog d3(m_doc, ControlMap::cueList(list));
    QCOMPARE(d3.findChild<QComboBox *>("learnMode")->count(), 3);
    QCOMPARE(d3.findChild<QComboBox *>("learnMode")->itemText(0), QString("GO"));
}

void ControlMapUi_Test::theLearnDialogShowsAndRemovesABinding()
{
    Scene *a = scene("Warm");
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 12, ControlMap::function(a->id()), ControlMap::StopOnly });
    ControlLearnDialog d(m_doc, ControlMap::function(a->id()));
    QVERIFY(d.hasControl());
    QCOMPARE(d.mode(), ControlMap::StopOnly);
    QPushButton *remove = d.findChild<QPushButton *>("learnRemove");
    QVERIFY(remove != nullptr && !remove->isHidden());

    // OK without a new press: the mode changes, the control stays.
    d.setMode(ControlMap::Toggle);
    d.accept();
    QCOMPARE(cm->binding(ControlMap::function(a->id()))->mode, ControlMap::Toggle);
    QCOMPARE(cm->binding(ControlMap::function(a->id()))->channel, 12u);

    ControlLearnDialog d2(m_doc, ControlMap::function(a->id()));
    d2.findChild<QPushButton *>("learnRemove")->click();
    QVERIFY(cm->isEmpty());
    QVERIFY(!cm->isListening());
}

void ControlMapUi_Test::theEmergencyStopLearnsOneButtonWithASecondPress()
{
#ifdef CONTROLMAP_HAS_EMERGENCY_STOP
    ControlMap *cm = m_doc->controlMap();
    ControlMapPanel panel(m_doc);
    panel.resize(700, 400);
    panel.show();
    QPushButton *button = panel.findChild<QPushButton *>("controlMapEmergencyStop");
    QVERIFY(button != nullptr);
    QVERIFY(button->isVisibleTo(&panel));
    QCOMPARE(button->text(), QString("Emergency Stop…"));

    ControlLearnDialog d(m_doc, ControlMap::emergencyStop());
    d.show();
    QVERIFY(d.findChild<QLabel *>("learnTarget")->text().contains("Emergency stop (ALL OFF)"));
    QComboBox *mode = d.findChild<QComboBox *>("learnMode");
    QCOMPARE(mode->count(), 1);                     // one way to act
    QVERIFY(!mode->isEnabled());
    QPushButton *ok = d.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
    QLabel *note = d.findChild<QLabel *>("learnNote");

    // A computer key is refused, in words.
    QVERIFY(d.learnKey(Qt::Key_F5, Qt::NoModifier));
    QVERIFY(!d.hasControl());
    QLabel *keyNote = d.findChild<QLabel *>("learnKeyNote");
    QVERIFY(keyNote->isVisibleTo(&d));
    QVERIFY2(keyNote->text().contains("cannot be the emergency stop"), qPrintable(keyNote->text()));

    // A fader: refused, the dialog says why and keeps listening.
    for (int v : { 3, 20, 80, 127 })
        cm->handleInput(0, 20, uchar(v));
    QVERIFY(!d.hasControl());
    QVERIFY(note->isVisibleTo(&d));
    QVERIFY2(note->text().contains("fader"), qPrintable(note->text()));
    QVERIFY(cm->isListening());

    // A button: the first press asks for a second, OK stays off.
    cm->handleInput(0, 188, 127);
    QVERIFY(!d.hasControl());
    QVERIFY(!ok->isEnabled());
    QVERIFY2(note->text().contains("press it again"), qPrintable(note->text()));
    cm->handleInput(0, 188, 0);
    cm->handleInput(0, 188, 127);
    QVERIFY(d.hasControl());
    QCOMPARE(d.channel(), 188u);
    QVERIFY(ok->isEnabled());
    ok->click();
    const ControlMap::Binding *b = cm->binding(ControlMap::emergencyStop());
    QVERIFY(b != nullptr);
    QCOMPARE(b->channel, 188u);

    // The panel lists it, with no choice of how it acts, and names the control.
    QVERIFY2(button->text().startsWith("Emergency Stop: ") && button->text().contains("Channel 189"),
             qPrintable(button->text()));
    QTableWidget *table = panel.table();
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 1)->text(), QString("Emergency stop (ALL OFF)"));
    QComboBox *how = qobject_cast<QComboBox *>(table->cellWidget(0, 2));
    QVERIFY(how != nullptr && !how->isEnabled() && how->count() == 1);

    // Learning again from the dialog shows the control it has.
    ControlLearnDialog again(m_doc, ControlMap::emergencyStop());
    QVERIFY(again.hasControl());
    again.findChild<QPushButton *>("learnRemove")->click();
    QVERIFY(cm->binding(ControlMap::emergencyStop()) == nullptr);
#else
    QFAIL("this build has no emergency stop on the Control Map");
#endif
}

void ControlMapUi_Test::aStopThatCannotWorkIsFlaggedOnTheButtonAndTheRow()
{
    ControlMap *cm = m_doc->controlMap();
    ControlMapPanel panel(m_doc);
    panel.resize(900, 400);
    panel.show();
    QPushButton *button = panel.findChild<QPushButton *>("controlMapEmergencyStop");
    QVERIFY(button != nullptr);
    // The stop sits on a CC that turns out to move like a fader.
    cm->setBinding({ 0, 7, ControlMap::emergencyStop(), ControlMap::StartOnly });
    QVERIFY2(!button->text().contains("fader"), qPrintable(button->text()));
    QVERIFY(!button->property("stopFlagged").toBool());
    QSignalSpy rebuilt(&panel, &ControlMapPanel::summaryChanged);   // a table rebuild says so
    QList<int> sweep;
    for (int i = 1; i <= 100; ++i)
        sweep << i % 120 + 1;
    for (int v : sweep)
        cm->handleInput(0, 7, uchar(v));
    // Said on the button and the row, without rebuilding the table for each message.
    QVERIFY2(button->text().contains("moves like a fader"), qPrintable(button->text()));
    QVERIFY(button->property("stopFlagged").toBool());
    QCOMPARE(rebuilt.count(), 0);
    // A visible mark: the red style, which the button wears.
    QVERIFY(button->styleSheet().contains("b00020"));
    QTableWidget *table = panel.table();
    QCOMPARE(table->rowCount(), 1);
    QVERIFY2(table->item(0, 0)->text().contains("moves like a fader"), qPrintable(table->item(0, 0)->text()));
    // ALL OFF in the Last line does not hide the flag the next instant.
    cm->handleInput(0, 7, 0);
    cm->handleInput(0, 7, 50);
    QLabel *last = panel.findChild<QLabel *>("controlMapLast");
    QVERIFY(last != nullptr);
    QVERIFY2(last->text().contains("ALL OFF") && last->text().contains("moves like a fader"), qPrintable(last->text()));
    QVERIFY2(last->text().contains("Emergency stop: moves like a fader"), qPrintable(last->text()));
    // An unrelated learn and removal keep it.
    Scene *a = scene("Warm");
    cm->setBinding({ 0, 9, ControlMap::function(a->id()), ControlMap::Toggle });
    cm->removeBinding(ControlMap::function(a->id()));
    QVERIFY2(button->text().contains("moves like a fader"), qPrintable(button->text()));
    // Learned again on a button: the flags go.
    cm->setBinding({ 0, 188, ControlMap::emergencyStop(), ControlMap::StartOnly });
    QVERIFY2(!button->text().contains("fader"), qPrintable(button->text()));
    QVERIFY(button->styleSheet().isEmpty());
}

void ControlMapUi_Test::aStopOnAProfileFaderIsWarnedOnceAndOnlyOnItsOwnLine()
{
    ControlMap *cm = m_doc->controlMap();
    InputOutputMap *iom = m_doc->inputOutputMap();
    QDir dir(QStringLiteral("../../../engine/test/iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../../../engine/test/iopluginstub");
    QLCIOPlugin *stub = m_doc->ioPluginCache()->plugins().at(0);
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    QLCInputProfile *prof = new QLCInputProfile();
    prof->setManufacturer("Fake");
    prof->setModel("Faders");
    QLCInputChannel *fader = new QLCInputChannel();
    fader->setType(QLCInputChannel::Slider);
    prof->insertChannel(5, fader);
    QVERIFY(iom->addProfile(prof));
    QVERIFY(iom->setInputProfile(0, prof->name()));
    ControlMapPanel panel(m_doc);
    panel.resize(900, 400);
    panel.show();
    Scene *a = scene("Warm");
    cm->setBinding({ 0, 9, ControlMap::function(a->id()), ControlMap::Toggle });
    cm->setBinding({ 0, 5, ControlMap::emergencyStop(), ControlMap::StartOnly });
    QPushButton *button = panel.findChild<QPushButton *>("controlMapEmergencyStop");
    QLabel *last = panel.findChild<QLabel *>("controlMapLast");
    QVERIFY(button != nullptr && last != nullptr);
    // Red, saying it is a fader, and not "dead".
    QVERIFY2(button->text().contains("fader"), qPrintable(button->text()));
    QVERIFY(button->property("stopFlagged").toBool());
    QVERIFY(button->styleSheet().contains("b00020"));
    // It fires; the Last line names the problem once, in the stop's words.
    cm->handleInput(0, 5, 100);
    QVERIFY2(last->text().contains("ALL OFF"), qPrintable(last->text()));
    QCOMPARE(last->text().count("fader or knob"), 1);
    QVERIFY2(last->text().contains("Emergency stop: is a fader or knob"), qPrintable(last->text()));
    // A line about another control does not carry the stop's warning.
    cm->handleInput(0, 9, 127);
    QVERIFY2(last->text().contains("Warm") && !last->text().contains("fader"), qPrintable(last->text()));
}

/* CS3. The panel's "Its light" column says what the desk's LED says, so
   the press rule can be read without the desk: Ready, Running, Stopped,
   Dark. The dim switch changes what is sent, not what the row says. */
void ControlMapUi_Test::thePanelSaysWhatEachLightShows()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    QDir dir(QStringLiteral("../../../engine/test/iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(),
             "no I/O plugin stub in ../../../engine/test/iopluginstub");
    QLCIOPlugin *stub = m_doc->ioPluginCache()->plugins().at(0);
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    QVERIFY(iom->setOutputPatch(0, stub->name(), QString(), 0, true));   // the feedback line

    Scene *a = scene("Warm");
    ControlMap *cm = m_doc->controlMap();
    ControlMapPanel panel(m_doc);
    panel.resize(900, 400);
    panel.show();
    cm->setBinding({ 0, 188, ControlMap::function(a->id()), ControlMap::Toggle });
    cm->setBinding({ 0, 190, ControlMap::emergencyStop(), ControlMap::StartOnly });
    QTableWidget *table = panel.table();
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->columnCount(), 4);
    QCOMPARE(table->horizontalHeaderItem(3)->text(), QString("Its light"));
    QCOMPARE(table->item(0, 3)->text(), QString("Ready"));
    // The stop is ready at every moment: it is never dark while the show runs.
    QCOMPARE(table->item(1, 3)->text(), QString("Ready"));

    /* The emergency stop struck: the stop's own row says Stopped and the
       scene's row is dark until the next GO -- and the table is not
       rebuilt for a light, only the one column written. (What a running
       control's row says is ControlMap_Test's: it needs the timer.) */
    QSignalSpy rebuilt(&panel, &ControlMapPanel::summaryChanged);
    m_doc->outputRuleKeeper()->emergencyStop();
    QTRY_COMPARE(table->item(1, 3)->text(), QString("Stopped"));
    QCOMPARE(table->item(0, 3)->text(), QString("Dark"));
    QCOMPARE(rebuilt.count(), 0);
    m_doc->outputRuleKeeper()->noteGo();
    QTRY_COMPARE(table->item(0, 3)->text(), QString("Ready"));
    QCOMPARE(table->item(1, 3)->text(), QString("Ready"));

    // The dim switch: on a pad that cannot dim, a ready control stays dark,
    // and the row says which.
    QCheckBox *dim = panel.findChild<QCheckBox *>("controlMapDimReady");
    QVERIFY(dim != nullptr);
    QVERIFY(dim->isChecked());
    dim->setChecked(false);
    QVERIFY(!cm->dimWhenReady());
    QTRY_COMPARE(table->item(0, 3)->text(), QString("Ready (dark)"));
}

/* A control that can never light says so, rather than leaving a column
   that never changes: a computer key, and a universe with no feedback line. */
void ControlMapUi_Test::thePanelSaysWhenAControlHasNoLight()
{
    Scene *a = scene("Warm");
    Scene *b = scene("Cool");
    ControlMap *cm = m_doc->controlMap();
    ControlMapPanel panel(m_doc);
    panel.resize(900, 400);
    panel.show();
    // Nothing patched at all: no light can reach a control.
    cm->setBinding({ 0, 188, ControlMap::function(a->id()), ControlMap::Toggle });
    cm->setBinding({ ControlMap::KeyboardUniverse, ControlMap::keyChannel(Qt::Key_F5, Qt::NoModifier),
                     ControlMap::function(b->id()), ControlMap::Toggle });
    QTableWidget *table = panel.table();
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->item(0, 3)->text(), QString("No feedback line"));
    QCOMPARE(table->item(1, 3)->text(), QString("A key has no light"));
}

void ControlMapUi_Test::anEncoderLearnStopsListeningWhenThePanelHides()
{
    ControlMap *cm = m_doc->controlMap();
    ControlMapPanel panel(m_doc);
    panel.show();
    QComboBox *preset = panel.encoderBox()->findChildren<QComboBox *>().first();
    preset->setCurrentIndex(1);
    emit preset->activated(1);
    QPushButton *learn = panel.encoderBox()->findChild<QPushButton *>("encoderLearn");
    QVERIFY(learn != nullptr && learn->isEnabled());
    learn->click();
    QVERIFY(cm->isListening());
    panel.hide();
    QVERIFY2(!cm->isListening(), "a learn left waiting kept listening after the panel went");
    QCOMPARE(learn->text(), QString("Learn Encoder 1…"));
}

void ControlMapUi_Test::theMenuActionSaysWhatIsBound()
{
    Scene *a = scene("Warm");
    QMenu menu;
    QAction *learn = ControlLearnDialog::addTo(&menu, m_doc, ControlMap::function(a->id()), nullptr);
    QVERIFY(learn != nullptr);
    QCOMPARE(learn->text(), QString("Learn Trigger…"));
    QCOMPARE(learn->objectName(), QString("learnTrigger"));
    m_doc->controlMap()->setBinding({ 0, 7, ControlMap::function(a->id()), ControlMap::Toggle });
    QAction *bound = ControlLearnDialog::addTo(&menu, m_doc, ControlMap::function(a->id()), nullptr);
    QVERIFY2(bound->text().startsWith("Trigger: ") && bound->text().contains("Channel 8"),
             qPrintable(bound->text()));
    // Nothing to fire: no action.
    QVERIFY(ControlLearnDialog::addTo(&menu, m_doc, ControlMap::Target(), nullptr) == nullptr);
}

void ControlMapUi_Test::buildOffersLearnTrigger()
{
    Scene *a = scene("Warm");
    ProgrammingOutline outline(m_doc);
    outline.resize(400, 300);
    outline.show();
    outline.rebuild();
    QTreeWidgetItem *item = outline.sceneItem(a->id());
    QVERIFY(item != nullptr);
    outline.scrollToItem(item);
    const QStringList names = menuActions([&]() {
        emit outline.customContextMenuRequested(outline.visualItemRect(item).center());
    });
    QVERIFY2(names.contains("learnTrigger"), qPrintable(names.join(", ")));
}

void ControlMapUi_Test::theShowTreeOffersLearnTrigger()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Show");
    const quint32 list = st->addCueList(prod, "Act 1");
    const quint32 cue = st->addCue(list, -1, "Top");
    const quint32 hit = st->addLook(prod, "Stab");
    const quint32 sub = st->addSubCue(cue, "Sub");
    QVERIFY(sub != ShowTree::invalidId());

    ShowTreeView view(nullptr, m_doc);
    view.resize(900, 700);
    view.show();
    QCoreApplication::processEvents();
    view.tree()->expandAll();
    auto namesOn = [&view](quint32 id) {
        if (!view.selectNode(id))
            return QStringList{ QStringLiteral("(no node)") };
        QTreeWidgetItem *item = view.tree()->currentItem();
        view.tree()->scrollToItem(item);
        const QPoint pos = view.tree()->visualItemRect(item).center();
        return menuActions([&]() {
            QMetaObject::invokeMethod(&view, "slotTreeContextMenu", Q_ARG(QPoint, pos));
        });
    };
    for (quint32 id : { list, cue, hit })
    {
        const QStringList names = namesOn(id);
        QVERIFY2(names.contains("learnTrigger"), qPrintable(QString::number(id) + ": " + names.join(", ")));
    }
    // A sub-cue: its cue's GO plays it, so no trigger of its own.
    const QStringList subNames = namesOn(sub);
    QVERIFY2(!subNames.contains("learnTrigger"), qPrintable(subNames.join(", ")));
}

void ControlMapUi_Test::thePanelListsChangesAndRemoves()
{
    Scene *a = scene("Warm");
    Scene *b = scene("Cool");
    ControlMap *cm = m_doc->controlMap();
    ControlMapPanel panel(m_doc);
    panel.setProperty("noConfirm", true);
    panel.show();
    QCOMPARE(panel.summary(), QString("None"));
    QVERIFY(!panel.findChild<QLabel *>("controlMapEmpty")->isHidden());

    QSignalSpy summary(&panel, &ControlMapPanel::summaryChanged);
    cm->setBinding({ 0, 1, ControlMap::function(a->id()), ControlMap::Toggle });
    cm->setBinding({ 0, 2, ControlMap::function(b->id()), ControlMap::Toggle });
    QVERIFY(summary.count() >= 2);
    QCOMPARE(panel.summary(), QString("2 triggers"));
    QTableWidget *table = panel.table();
    QCOMPARE(table->rowCount(), 2);
    QVERIFY(table->item(0, 1)->text().contains("Warm"));
    QVERIFY(table->item(1, 0)->text().contains("Channel 3"));

    // The mode, from the table.
    QComboBox *mode = qobject_cast<QComboBox *>(table->cellWidget(0, 2));
    QVERIFY(mode != nullptr);
    const int momentary = mode->findData(int(ControlMap::Momentary));
    mode->setCurrentIndex(momentary);
    emit mode->activated(momentary);
    QCOMPARE(cm->binding(ControlMap::function(a->id()))->mode, ControlMap::Momentary);

    // Remove the selected one.
    table->selectRow(1);
    panel.findChild<QPushButton *>("controlMapRemove")->click();
    QCOMPARE(cm->count(), 1);
    QVERIFY(cm->binding(ControlMap::function(b->id())) == nullptr);
    QCOMPARE(table->rowCount(), 1);

    // Renamed: the table follows.
    a->setName("Amber");
    QTRY_VERIFY(table->item(0, 1)->text().contains("Amber"));

    panel.findChild<QPushButton *>("controlMapRemoveAll")->click();
    QVERIFY(cm->isEmpty());
    QCOMPARE(table->rowCount(), 0);

    // What fired last.
    emit cm->fired("Started Scene: Amber");
    QVERIFY(panel.findChild<QLabel *>("controlMapLast")->text().contains("Amber"));
}

void ControlMapUi_Test::theShowControlPageHasTheMap()
{
    Scene *a = scene("Warm");
    ShowControlSyncPage page(m_doc);
    const QList<QStringList> rows = page.summary();
    QCOMPARE(rows.last().at(0), QString("map"));
    QCOMPARE(rows.last().at(1), QString("Control Map"));
    QCOMPARE(rows.last().at(2), QString("None"));
    QVERIFY(page.findChild<ControlMapPanel *>() != nullptr);

    QSignalSpy changed(&page, &ShowControlSyncPage::summaryChanged);
    m_doc->controlMap()->setBinding({ 0, 1, ControlMap::function(a->id()), ControlMap::Toggle });
    QVERIFY(changed.count() >= 1);
    QCOMPARE(page.summary().last().at(2), QString("1 trigger"));

    page.showSection("map");
    QVERIFY(!page.findChild<QWidget *>("scMapGroup")->isHidden());
    QVERIFY(page.findChild<QWidget *>("scOscGroup")->isHidden());
}

/* A computer key pressed in the dialog is learned as a MIDI press is:
   "Key · F5", another changes it, OK binds it with the mode chosen. */
void ControlMapUi_Test::theLearnDialogLearnsAComputerKey()
{
    Scene *a = scene("Warm");
    ControlMap *cm = m_doc->controlMap();
    ControlLearnDialog d(m_doc, ControlMap::function(a->id()));
    d.show();
    QVERIFY(QTest::qWaitForWindowExposed(&d));
    QComboBox *mode = d.findChild<QComboBox *>("learnMode");
    mode->setFocus();
    QTest::keyClick(mode, Qt::Key_F6);      // through the dialog's key filter
    QVERIFY(d.hasControl());
    QCOMPARE(d.universe(), ControlMap::KeyboardUniverse);
    QVERIFY2(d.findChild<QLabel *>("learnHeard")->text().contains("Key · F6"),
             qPrintable(d.findChild<QLabel *>("learnHeard")->text()));
    QVERIFY(!d.findChild<QLabel *>("learnKeyNote")->isHidden());
    QVERIFY(d.learnKey(Qt::Key_F5, Qt::NoModifier));     // another: it changes
    QCOMPARE(d.channel(), quint32(Qt::Key_F5));
    QVERIFY(d.findChild<QLabel *>("learnHeard")->text().contains("Key · F5"));
    QVERIFY(!d.learnKey(Qt::Key_Shift, Qt::ShiftModifier));   // a modifier alone: waits
    QCOMPARE(d.channel(), quint32(Qt::Key_F5));
    QVERIFY(cm->isEmpty());
    d.setMode(ControlMap::Momentary);
    d.accept();
    const ControlMap::Binding *b = cm->binding(ControlMap::function(a->id()));
    QVERIFY(b != nullptr);
    QVERIFY(ControlMap::isKey(*b));
    QCOMPARE(b->channel, quint32(Qt::Key_F5));
    QCOMPARE(b->mode, ControlMap::Momentary);

    // A key already firing something else: the dialog says so.
    Scene *c = scene("Cool");
    ControlLearnDialog d2(m_doc, ControlMap::function(c->id()));
    QVERIFY(d2.learnKey(Qt::Key_F5, Qt::NoModifier));
    QVERIFY(d2.findChild<QLabel *>("learnHeard")->text().contains("Warm"));
    // A MIDI press after a key: the MIDI control, the key's note goes.
    cm->handleInput(0, 12, 127);
    QCOMPARE(d2.universe(), 0u);
    QVERIFY(d2.findChild<QLabel *>("learnKeyNote")->isHidden());
}

/* Keys the dialog needs, the app's edit keys, a menu shortcut and a VC
   button's key are refused, saying why. */
void ControlMapUi_Test::theLearnDialogRefusesTakenKeys()
{
    Scene *a = scene("Warm");
    QWidget window;
    QAction *blackout = new QAction("&Blackout", &window);
    blackout->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    window.addAction(blackout);

    new VirtualConsole(nullptr, m_doc);
    auto vcGone = qScopeGuard([]() { delete VirtualConsole::instance(); });
    VCButton *btn = new VCButton(VirtualConsole::instance()->contents(), m_doc);
    btn->setCaption("House lights");
    btn->setKeySequence(QKeySequence(Qt::Key_F9));

    ControlLearnDialog d(m_doc, ControlMap::function(a->id()));
    QLabel *note = d.findChild<QLabel *>("learnKeyNote");
    auto refused = [&](int key, Qt::KeyboardModifiers mods, const QString &says, bool dialogKey) {
        const bool usedUp = d.learnKey(key, mods);
        return !d.hasControl() && usedUp == !dialogKey && !note->isHidden() && note->text().contains(says);
    };
    QVERIFY2(refused(Qt::Key_K, Qt::ControlModifier, "Blackout", false), qPrintable(note->text()));
    QVERIFY2(refused(Qt::Key_F9, Qt::NoModifier, "Virtual Console Button \"House lights\"", false),
             qPrintable(note->text()));
    QVERIFY2(refused(Qt::Key_F1, Qt::NoModifier, "edit keys", false), qPrintable(note->text()));
    QVERIFY2(refused(Qt::Key_Delete, Qt::NoModifier, "edit keys", false), qPrintable(note->text()));
    for (int k : { int(Qt::Key_Escape), int(Qt::Key_Tab), int(Qt::Key_Return), int(Qt::Key_Space),
                   int(Qt::Key_Up) })
        QVERIFY2(refused(k, Qt::NoModifier, "works this dialog", true), qPrintable(note->text()));
    QVERIFY(KeyTriggers::refusal(ControlMap::keyChannel(Qt::Key_F5, Qt::NoModifier)).isEmpty());
    QVERIFY(d.learnKey(Qt::Key_F5, Qt::NoModifier));
    QVERIFY(d.hasControl());
}

namespace
{
/** A key event sent as the system sends one (through the app's filters). */
bool sendKey(QWidget *to, QEvent::Type type, int key, bool autoRepeat = false)
{
    QKeyEvent ev(type, key, Qt::NoModifier, QString(), autoRepeat);
    QApplication::sendEvent(to, &ev);
    return ev.isAccepted();
}
}

/* Bound keys reach their trigger in Operate, from the window in front;
   never in Design (not armed), never while a text field has the keyboard;
   auto-repeat isn't a press; the release lets go. */
void ControlMapUi_Test::aKeyFiresInOperateNotInATextField()
{
    Scene *a = scene("Warm");
    ControlMap *cm = m_doc->controlMap();
    KeyTriggers keys(m_doc);
    QVERIFY(!keys.isWatching());                    // no key bound: no filter
    const quint32 f5 = ControlMap::keyChannel(Qt::Key_F5, Qt::NoModifier);
    cm->setBinding({ ControlMap::KeyboardUniverse, f5, ControlMap::function(a->id()), ControlMap::Momentary });
    QVERIFY(keys.isWatching());

    QWidget window;
    QVBoxLayout *lay = new QVBoxLayout(&window);
    QPushButton *button = new QPushButton("Somewhere", &window);
    QLineEdit *field = new QLineEdit(&window);
    lay->addWidget(button);
    lay->addWidget(field);
    window.show();
    window.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&window));
    button->setFocus();
    QTRY_COMPARE(QApplication::focusWidget(), button);

    QSignalSpy fired(cm, &ControlMap::fired);
    // Design, not armed: the key goes on to the widget.
    sendKey(button, QEvent::KeyPress, Qt::Key_F5);
    sendKey(button, QEvent::KeyRelease, Qt::Key_F5);
    QCOMPARE(fired.count(), 0);

    m_doc->setMode(Doc::Operate);
    sendKey(button, QEvent::KeyPress, Qt::Key_F5);
    QCOMPARE(fired.count(), 1);
    QVERIFY(cm->isKeyDown(f5));
    sendKey(button, QEvent::KeyPress, Qt::Key_F5, true);    // auto-repeat: not a press
    sendKey(button, QEvent::KeyRelease, Qt::Key_F5, true);
    QVERIFY(cm->isKeyDown(f5));
    QCOMPARE(fired.count(), 1);
    sendKey(button, QEvent::KeyRelease, Qt::Key_F5);
    QVERIFY(!cm->isKeyDown(f5));

    // A text field has the keyboard: its key, typed.
    cm->setBinding({ ControlMap::KeyboardUniverse, quint32(Qt::Key_H), ControlMap::function(a->id()),
                     ControlMap::Toggle });
    field->setFocus();
    QTRY_COMPARE(QApplication::focusWidget(), field);
    QVERIFY(!KeyTriggers::focusBlocks(field).isEmpty());
    const int before = fired.count();
    QTest::keyClick(field, Qt::Key_H);
    QCOMPARE(fired.count(), before);
    QCOMPARE(field->text(), QString("h"));
    QVERIFY(!cm->isKeyDown(quint32(Qt::Key_H)));
    // Back on the button: it fires.
    button->setFocus();
    QTRY_COMPARE(QApplication::focusWidget(), button);
    QVERIFY(KeyTriggers::focusBlocks(button).isEmpty());
    QTest::keyClick(button, Qt::Key_H);
    QCOMPARE(fired.count(), before + 1);
    // The last key binding gone: the filter goes.
    cm->removeAll();
    QVERIFY(!keys.isWatching());
}

/* In Design a key is armed by what the Show Tree has selected and by the
   Run tab's cue list, and the menu says "Armed". */
void ControlMapUi_Test::aKeyIsArmedFromTheShowTreeAndTheRunTab()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Show");
    const quint32 list = st->addCueList(prod, "Act 1");
    const quint32 list2 = st->addCueList(prod, "Act 2");
    const quint32 cue = st->addCue(list, -1, "Top");
    const quint32 prod2 = st->addProduction("Other");
    const quint32 hit = st->addLook(prod2, "Stab");
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ ControlMap::KeyboardUniverse, quint32(Qt::Key_F5), ControlMap::hit(hit), ControlMap::Toggle });
    cm->setBinding({ ControlMap::KeyboardUniverse, quint32(Qt::Key_F6), ControlMap::cue(cue), ControlMap::Toggle });

    // The Run tab follows Act 2: Act 1's cue isn't armed.
    {
        CueRunner run(m_doc, CueRunner::Full);
        run.setFollow(list2);
        QCOMPARE(cm->runList(), list2);
        QVERIFY(!cm->isArmed(ControlMap::cue(cue)));
        run.setFollow(list);
        QCOMPARE(cm->runList(), list);
        QVERIFY(cm->isArmed(ControlMap::cue(cue)));
        run.setFollow(list2);
    }
    QVERIFY(!cm->isArmed(ControlMap::hit(hit)));

    ShowTreeView view(nullptr, m_doc);
    view.resize(900, 700);
    view.show();
    QCoreApplication::processEvents();
    view.tree()->expandAll();
    QVERIFY(view.selectNode(hit));
    QVERIFY(cm->isArmed(ControlMap::hit(hit)));
    QVERIFY(!cm->isArmed(ControlMap::cue(cue)));
    QMenu menu;
    QAction *a = ControlLearnDialog::addTo(&menu, m_doc, ControlMap::hit(hit), nullptr);
    QVERIFY2(a->text() == QString("Trigger: Key · F5 · Armed…"), qPrintable(a->text()));
    QVERIFY(view.selectNode(list));
    QVERIFY(cm->isArmed(ControlMap::cue(cue)));
    QVERIFY(!cm->isArmed(ControlMap::hit(hit)));
    a = ControlLearnDialog::addTo(&menu, m_doc, ControlMap::hit(hit), nullptr);
    QCOMPARE(a->text(), QString("Trigger: Key · F5…"));
    // Operate: no "Armed" (everything may fire).
    m_doc->setMode(Doc::Operate);
    a = ControlLearnDialog::addTo(&menu, m_doc, ControlMap::cue(cue), nullptr);
    QCOMPARE(a->text(), QString("Trigger: Key · F6…"));
}

void ControlMapUi_Test::thePanelAndMenuListKeys()
{
    Scene *a = scene("Warm");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Show");
    const quint32 list = st->addCueList(prod, "Act 1");
    ControlMap *cm = m_doc->controlMap();
    // The production's only list is the Run tab's: armed. On another: not.
    QVERIFY(cm->isArmed(ControlMap::cueList(list)));
    cm->setRunTabList(st->addCueList(prod, "Act 2"));
    ControlMapPanel panel(m_doc);
    panel.show();
    cm->setBinding({ ControlMap::KeyboardUniverse, quint32(Qt::Key_F5), ControlMap::function(a->id()),
                     ControlMap::Toggle });
    cm->setBinding({ ControlMap::KeyboardUniverse, quint32(Qt::Key_F6), ControlMap::cueList(list),
                     ControlMap::Toggle });
    cm->setBinding({ 0, 3, ControlMap::function(scene("Cool")->id()), ControlMap::Toggle });
    QTableWidget *table = panel.table();
    QCOMPARE(table->rowCount(), 3);
    QCOMPARE(table->item(0, 0)->text(), QString("Key · F5"));
    QCOMPARE(table->item(1, 0)->text(), QString("Key · F6"));
    cm->setSelectedTargets({ ControlMap::cueList(list) });
    QCOMPARE(table->item(1, 0)->text(), QString("Key · F6 · Armed"));
    QCOMPARE(table->item(0, 0)->text(), QString("Key · F5"));     // a scene: Operate only
    QVERIFY(table->item(2, 0)->text().contains("Channel 4"));
    cm->setSelectedTargets({});
    QCOMPARE(table->item(1, 0)->text(), QString("Key · F6"));
    QCOMPARE(panel.summary(), QString("3 triggers"));
}

/* Offscreen pictures of the dialog listening and with "Key · F5" learned,
   at three sizes (KEYTRIG_SHOTS: the folder; unset, nothing is saved). */
void ControlMapUi_Test::learnKeyShots()
{
    const QString shots = qEnvironmentVariable("KEYTRIG_SHOTS");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Show");
    const quint32 hit = st->addLook(prod, "Stab");
    const QList<QSize> sizes = { QSize(420, 300), QSize(560, 360), QSize(800, 480) };
    for (const QSize &size : sizes)
    {
        ControlLearnDialog d(m_doc, ControlMap::hit(hit));
        d.resize(size);
        d.show();
        QVERIFY(QTest::qWaitForWindowExposed(&d));
        QCoreApplication::processEvents();
        QVERIFY(d.findChild<QLabel *>("learnHeard")->text().contains("Listening"));
        const QString tag = QString("%1x%2").arg(size.width()).arg(size.height());
        if (!shots.isEmpty())
        {
            QDir().mkpath(shots);
            QVERIFY(d.grab().save(QDir(shots).absoluteFilePath("listening-" + tag + ".png")));
        }
        QVERIFY(d.learnKey(Qt::Key_F5, Qt::NoModifier));
        QCoreApplication::processEvents();
        QVERIFY(d.findChild<QLabel *>("learnHeard")->text().contains("Key · F5"));
        // Every label fits the dialog's width.
        for (QLabel *l : d.findChildren<QLabel *>())
            if (l->isVisible())
                QVERIFY2(l->geometry().right() <= d.width(), qPrintable(l->objectName()));
        if (!shots.isEmpty())
            QVERIFY(d.grab().save(QDir(shots).absoluteFilePath("learned-F5-" + tag + ".png")));
        d.reject();
    }
}

QTEST_MAIN(ControlMapUi_Test)

/* The Control surfaces hub (Connections ▸ Devices ▸ Show control & sync):
   one row for every place a surface is set up. */
void ControlMapUi_Test::theSurfacesHubListsEveryPlace()
{
    InputOutputManager mgr(nullptr, m_doc);
    QTreeWidget *view = mgr.findChild<ConnectionsTree *>()->findChild<QTreeWidget *>();
    QVERIFY(view != nullptr);
    const QList<QTreeWidgetItem *> found = view->findItems("Show control & sync", Qt::MatchExactly, 0);
    QCOMPARE(found.count(), 1);
    QCOMPARE(found.first()->child(0)->text(0), QString("Control surfaces"));

    ShowControlSyncPage *page = mgr.findChild<ShowControlSyncPage *>();
    QVERIFY(page != nullptr);
    ControlSurfacesPanel *hub = page->surfaces();
    QVERIFY(hub != nullptr);
    QCOMPARE(ControlSurfacesPanel::keys(),
             QStringList({ "map", "estop", "encoder", "vc", "masters", "pmj", "web", "pads" }));
    for (const QString &key : ControlSurfacesPanel::keys())
    {
        QVERIFY2(hub->setUpButton(key) != nullptr, qPrintable(key));
        QCOMPARE(hub->setUpButton(key)->text(), QString("Set up…"));
        QVERIFY2(!hub->status(key).isEmpty(), qPrintable(key));
    }
    QCOMPARE(hub->status("map"), QString("No triggers learned"));
    QCOMPARE(hub->status("estop"), QString("Emergency stop: not learned"));
    QCOMPARE(hub->status("web"), QString("Companion: web access off"));
    QCOMPARE(hub->status("pads"), QString("No pad connected"));
}

void ControlMapUi_Test::eachSetUpLandsOnItsPlace()
{
    InputOutputManager mgr(nullptr, m_doc);
    mgr.resize(1100, 700);
    mgr.show();
    QVERIFY(QTest::qWaitForWindowExposed(&mgr));
    ShowControlSyncPage *page = mgr.findChild<ShowControlSyncPage *>();
    QTreeWidget *view = mgr.findChild<ConnectionsTree *>()->findChild<QTreeWidget *>();
    ControlSurfacesPanel *hub = page->surfaces();
    ControlMapPanel *map = page->findChild<ControlMapPanel *>();
    auto picked = [&]() {
        return view->currentItem() != nullptr ? view->currentItem()->text(0) : QString();
    };

    hub->setUpButton("map")->click();
    QCOMPARE(picked(), QString("Control Map"));
    QVERIFY(page->findChild<QWidget *>("scMapGroup")->isVisible());
    QVERIFY(page->findChild<QWidget *>("scOscGroup")->isHidden());
    QVERIFY(map->table()->isVisibleTo(page) || page->findChild<QWidget *>("controlMapEmpty")->isVisibleTo(page));

    hub->setUpButton("web")->click();
    QCOMPARE(picked(), QString("Web access"));
    QVERIFY(page->findChild<QWidget *>("scWebGroup")->isVisible());
    QVERIFY(page->findChild<QWidget *>("scMapGroup")->isHidden());

    hub->setUpButton("estop")->click();
    QCOMPARE(picked(), QString("Control Map"));
    QVERIFY(map->stopButton()->isVisible());
    QVERIFY(map->stopButton()->hasFocus() || map->stopButton()->isVisibleTo(page));

    hub->setUpButton("encoder")->click();
    QVERIFY(map->encoderBox()->isVisible());

    // The Virtual Console is the App's tab: the hub asks for it.
    QSignalSpy vc(&mgr, &InputOutputManager::virtualConsoleRequested);
    hub->setUpButton("vc")->click();
    QCOMPARE(vc.count(), 1);

    // The masters: their own panel comes up.
    hub->setUpButton("masters")->click();
    SpeedMastersPanel *panel = nullptr;
    for (QWidget *w : QApplication::topLevelWidgets())
        if (SpeedMastersPanel *p = qobject_cast<SpeedMastersPanel *>(w))
            panel = p;
    QVERIFY(panel != nullptr && panel->isVisible());
    panel->close();

    // The PMJ and the pads are patched in the wiring: nothing picked there.
    hub->setUpButton("map")->click();
    hub->setUpButton("pads")->click();
    QVERIFY(view->selectedItems().isEmpty());
    hub->setUpButton("map")->click();
    hub->setUpButton("pmj")->click();
    QVERIFY(view->selectedItems().isEmpty());
}

void ControlMapUi_Test::theSurfacesStatusFollowsTheShow()
{
    Scene *a = scene("Warm");
    ShowControlSyncPage page(m_doc);
    ControlSurfacesPanel *hub = page.surfaces();
    ControlMap *cm = m_doc->controlMap();
    QSignalSpy changed(&page, &ShowControlSyncPage::summaryChanged);
    QCOMPARE(hub->status("map"), QString("No triggers learned"));

    cm->setBinding({ 0, 1, ControlMap::function(a->id()), ControlMap::Toggle });
    QCOMPARE(hub->status("map"), QString("1 trigger learned"));
    QVERIFY(changed.count() >= 1);
    QCOMPARE(page.summary().first().at(0), QString("surfaces"));
    QVERIFY(page.summary().first().at(2).startsWith("1 trigger learned"));

    // The stop is not a trigger: it has its own line.
    cm->setBinding({ 0, 9, ControlMap::emergencyStop(), ControlMap::Toggle });
    QCOMPARE(hub->status("map"), QString("1 trigger learned"));
    QVERIFY(!hub->status("estop").contains("not learned"));
    QVERIFY(hub->status("estop").startsWith("Emergency stop:"));

    cm->setBinding({ 0, 2, ControlMap::function(a->id()), ControlMap::Toggle });
    QCOMPARE(hub->status("map"), QString("1 trigger learned"));   // the same scene: moved, not added

    m_doc->webAccess()->setStatus(ShowWebAccess::Running, QStringLiteral("http://127.0.0.1:9999/"));
    QVERIFY(hub->status("web").startsWith("Web access on"));
}

void ControlMapUi_Test::surfacesOffKeepsTheStop()
{
    ShowControlSyncPage page(m_doc);
    page.show();
    ControlSurfacesPanel *hub = page.surfaces();
    const auto restore = qScopeGuard([]() { ShowFeatures::instance()->reset(); });
    ShowFeatures::instance()->setOn(ShowFeatures::ControlSurfaces, false);
    QVERIFY(hub->findChild<QLabel *>("surfacesFeatureOff")->isVisibleTo(hub));
    QVERIFY(hub->findChild<QLabel *>("surfacesFeatureOff")->text().contains("Tabs & Features"));
    QCOMPARE(hub->status("map"), QString("Off for this show"));
    QCOMPARE(hub->status("encoder"), QString("Off for this show"));
    QVERIFY(!hub->setUpButton("map")->isEnabled());
    QVERIFY(!hub->setUpButton("encoder")->isEnabled());
    // The stop row stays, with its button.
    QVERIFY(hub->setUpButton("estop")->isEnabled());
    QVERIFY(hub->status("estop").startsWith("Emergency stop"));
    QVERIFY(hub->findChild<QWidget *>("surfaceEmergencyStopSetUp")->isVisibleTo(hub));

    ShowFeatures::instance()->setOn(ShowFeatures::ControlSurfaces, true);
    QVERIFY(hub->setUpButton("map")->isEnabled());
    QVERIFY(!hub->findChild<QLabel *>("surfacesFeatureOff")->isVisibleTo(hub));
}

/* With QLC_SHOT_DIR: the hub at three sizes (UX review). */
void ControlMapUi_Test::surfacesShots()
{
    const QByteArray dir = qgetenv("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR is not set");
    QDir().mkpath(QString::fromLocal8Bit(dir));
    InputOutputManager mgr(nullptr, m_doc);
    const QList<QSize> sizes = { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) };
    for (const QSize &sz : sizes)
    {
        mgr.resize(sz);
        mgr.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mgr));
        mgr.showShowControlPage();
        mgr.findChild<ConnectionsTree *>()->selectShowControlRow(QStringLiteral("surfaces"));
        QCoreApplication::processEvents();
        QVERIFY(mgr.grab().save(QDir(QString::fromLocal8Bit(dir))
                                    .absoluteFilePath(QString("surfaces-%1x%2.png").arg(sz.width()).arg(sz.height()))));
    }
}
