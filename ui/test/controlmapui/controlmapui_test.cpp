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
#include <QXmlStreamReader>
#include <QtTest>

#include "controlmapui_test.h"
#include "outputrulekeeper.h"
#include "showcontrolsyncpage.h"
#include "controllearndialog.h"
#include "controlcollisionbox.h"
#include "controlowners.h"
#include "inputselectionwidget.h"
#include "virtualconsole/vcpropertieseditor.h"
#include "speedmasters.h"
#include "encoderbank.h"
#include "programmingoutline.h"
#include "showtreecontroller.h"
#include "virtualconsole/virtualconsole.h"
#include "virtualconsole/vcbutton.h"
#include "virtualconsole/vcframe.h"
#include "virtualconsole/vcframepageshortcut.h"
#include "virtualconsole/vcspeeddial.h"
#include "virtualconsole/vcspeeddialpreset.h"
#include "virtualconsole/vcmatrix.h"
#include "virtualconsole/vcmatrixcontrol.h"
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
        // ...refused, naming it (Q297): OK is off until Move It Here.
        ControlCollisionBox *box = d2.findChild<ControlCollisionBox *>("controlCollision");
        QVERIFY(box != nullptr && box->isRefusing());
        QVERIFY2(box->text().contains("Warm"), qPrintable(box->text()));
        QVERIFY(!d2.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
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
    QComboBox *how = qobject_cast<QComboBox *>(table->cellWidget(0, 3));
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
    QCOMPARE(table->columnCount(), 5);
    QCOMPARE(table->horizontalHeaderItem(4)->text(), QString("Its light"));
    QCOMPARE(table->horizontalHeaderItem(2)->text(), QString("Works"));
    QCOMPARE(table->item(0, 4)->text(), QString("Ready"));
    // The stop is ready at every moment: it is never dark while the show runs.
    QCOMPARE(table->item(1, 4)->text(), QString("Ready"));

    /* The emergency stop struck: the stop's own row says Stopped and the
       scene's row is dark until the next GO -- and the table is not
       rebuilt for a light, only the one column written. (What a running
       control's row says is ControlMap_Test's: it needs the timer.) */
    QSignalSpy rebuilt(&panel, &ControlMapPanel::summaryChanged);
    m_doc->outputRuleKeeper()->emergencyStop();
    QTRY_COMPARE(table->item(1, 4)->text(), QString("Stopped"));
    QCOMPARE(table->item(0, 4)->text(), QString("Dark"));
    QCOMPARE(rebuilt.count(), 0);
    m_doc->outputRuleKeeper()->noteGo();
    QTRY_COMPARE(table->item(0, 4)->text(), QString("Ready"));
    QCOMPARE(table->item(1, 4)->text(), QString("Ready"));

    // The dim switch: on a pad that cannot dim, a ready control stays dark,
    // and the row says which.
    QCheckBox *dim = panel.findChild<QCheckBox *>("controlMapDimReady");
    QVERIFY(dim != nullptr);
    QVERIFY(dim->isChecked());
    dim->setChecked(false);
    QVERIFY(!cm->dimWhenReady());
    QTRY_COMPARE(table->item(0, 4)->text(), QString("Ready (dark)"));
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
    QCOMPARE(table->item(0, 4)->text(), QString("No feedback line"));
    QCOMPARE(table->item(1, 4)->text(), QString("A key has no light"));
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
    QComboBox *mode = qobject_cast<QComboBox *>(table->cellWidget(0, 3));
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
    ControlCollisionBox *box = d2.findChild<ControlCollisionBox *>("controlCollision");
    QVERIFY2(box->isRefusing() && box->text().contains("Warm"), qPrintable(box->text()));
    // A MIDI press after a key: the MIDI control, the key's note goes.
    cm->handleInput(0, 12, 127);
    QCOMPARE(d2.universe(), 0u);
    QVERIFY(d2.findChild<QLabel *>("learnKeyNote")->isHidden());
    QVERIFY(!box->isRefusing());
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

/****************************************************************************
 * One job per control (CONTROL_SCOPES.md §8 phase 1, board Q297)
 ****************************************************************************/

namespace
{
/** A Virtual Console with one button on input (0, @a channel). */
VCButton *vcButtonOn(Doc *doc, quint32 channel, const QString &caption)
{
    if (VirtualConsole::instance() == nullptr)
        new VirtualConsole(nullptr, doc);
    VCButton *btn = new VCButton(VirtualConsole::instance()->contents(), doc);
    btn->setCaption(caption);
    btn->setInputSource(QSharedPointer<QLCInputSource>(new QLCInputSource(0, channel)));
    return btn;
}
}

/* Learn Trigger… on a control a Virtual Console button has: refused, naming
   the button; Move It Here takes it from the button and binds it here. */
void ControlMapUi_Test::aTriggerLearnRefusesAWidgetsControlAndMovesIt()
{
    Scene *a = scene("Warm");
    ControlMap *cm = m_doc->controlMap();
    auto vcGone = qScopeGuard([]() { delete VirtualConsole::instance(); });
    VCButton *btn = vcButtonOn(m_doc, 60, "House lights");
    QVERIFY(btn->checkInputSource(0, 60, 127, nullptr));

    ControlLearnDialog d(m_doc, ControlMap::function(a->id()));
    d.show();
    cm->handleInput(0, 60, 127);
    cm->handleInput(0, 60, 0);
    ControlCollisionBox *box = d.findChild<ControlCollisionBox *>("controlCollision");
    QVERIFY(box != nullptr && box->isVisibleTo(&d));
    QVERIFY2(box->text().contains("already does Button \"House lights\" (Virtual Console). One job per control."),
             qPrintable(box->text()));
    QPushButton *ok = d.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
    QVERIFY(!ok->isEnabled());
    d.accept();                                     // OK can't take it either
    QVERIFY(cm->isEmpty());
    QCOMPARE(box->moveButton()->text(), QString("Move It Here"));
    QVERIFY(box->moveButton()->isVisibleTo(&d));

    QSignalSpy fired(cm, &ControlMap::fired);
    box->moveButton()->click();
    // The button no longer hears it; the trigger does.
    QVERIFY(btn->inputSource(0).isNull());
    QVERIFY(!btn->checkInputSource(0, 60, 127, nullptr));
    const ControlMap::Binding *b = cm->binding(ControlMap::function(a->id()));
    QVERIFY(b != nullptr && b->channel == 60u);
    QVERIFY(!cm->isListening());
    cm->handleInput(0, 60, 0);
    cm->handleInput(0, 60, 127);
    QCOMPARE(fired.count(), 1);
}

/* The PMJ's controls come with its profile: refused with the reason, and no
   Move It Here. */
void ControlMapUi_Test::aPmjControlIsRefusedWithNoMoveItHere()
{
    Scene *a = scene("Warm");
    ControlMap *cm = m_doc->controlMap();
    InputOutputMap *iom = m_doc->inputOutputMap();
    QDir dir(QStringLiteral("../../../engine/test/iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY(!m_doc->ioPluginCache()->plugins().isEmpty());
    QVERIFY(iom->setInputPatch(0, m_doc->ioPluginCache()->plugins().at(0)->name(), QString(), 0));
    QLCInputProfile *prof = new QLCInputProfile();
    prof->setManufacturer("PMJ");
    prof->setModel("Black 1");
    QVERIFY(iom->addProfile(prof));
    QVERIFY(iom->setInputProfile(0, prof->name()));
    const quint32 ch3 = 32771;
    QCOMPARE(ControlOwners::pmjControlName(ch3), QString("Ch 3"));

    ControlLearnDialog d(m_doc, ControlMap::function(a->id()));
    d.show();
    cm->handleInput(0, ch3, 127);
    ControlCollisionBox *box = d.findChild<ControlCollisionBox *>("controlCollision");
    QVERIFY2(box->isRefusing() && box->text().contains("the PMJ Black 1's Ch 3 (PMJ overlay)"),
             qPrintable(box->text()));
    QVERIFY2(box->text().contains("can't be moved"), qPrintable(box->text()));
    QVERIFY(!box->moveButton()->isVisibleTo(&d));
    QVERIFY(box->cancelButton()->isVisibleTo(&d));
    QVERIFY(!ControlOwners::releaseAll(m_doc, box->owners()));
    box->cancelButton()->click();
    QVERIFY(!box->isRefusing());
    QVERIFY(!d.hasControl());
}

/* The encoder bank's learn refuses a trigger's control (and Move It Here
   moves it); a trigger's learn refuses an encoder's. */
void ControlMapUi_Test::theEncoderLearnRefusesAndMoves()
{
    Scene *a = scene("Warm");
    Scene *c = scene("Cool");
    ControlMap *cm = m_doc->controlMap();
    EncoderBank *enc = cm->encoderBank();
    EncoderBank::Device dev;
    dev.universe = 0;
    dev.encoders = 16;
    dev.encoderCount = 8;
    enc->setDevice(dev, QStringLiteral("custom"));
    QVERIFY(enc->isEnabled());

    // A trigger on encoder 3's control: refused, naming the encoder.
    {
        ControlLearnDialog d(m_doc, ControlMap::function(c->id()));
        d.show();
        cm->handleInput(0, 18, 127);
        ControlCollisionBox *box = d.findChild<ControlCollisionBox *>("controlCollision");
        QVERIFY2(box->isRefusing() && box->text().contains("Encoder 3 (encoder bank)"), qPrintable(box->text()));
        // A run: no Move It Here (it would take encoders 3-8 with it), and why.
        QVERIFY(!box->moveButton()->isVisibleTo(&d));
        QVERIFY2(box->text().contains("can't be moved one by one"), qPrintable(box->text()));
        QVERIFY(!ControlOwners::releaseAll(m_doc, box->owners()));
        QCOMPARE(enc->device().encoders, 16);
        QCOMPARE(enc->device().encoderCount, 8);
        QVERIFY(enc->isEnabled());
        d.reject();
    }
    // Every encoder of the run is so, the first too (it would turn the bank off).
    {
        const QList<ControlOwners::Owner> first = ControlOwners::owners(m_doc, 0, 16);
        QCOMPARE(first.count(), 1);
        QVERIFY(!first.first().movable);
        QVERIFY(!ControlOwners::release(m_doc, first.first()));
        QVERIFY(enc->isEnabled());
    }

    cm->setBinding({ 0, 40, ControlMap::function(a->id()), ControlMap::Toggle });
    ControlMapPanel panel(m_doc);
    panel.show();
    QAction *next = panel.findChild<QAction *>("encoderLearnNext");
    QVERIFY(next != nullptr);
    QVERIFY(QMetaObject::invokeMethod(&panel, "slotEncoderLearnMore", Q_ARG(QAction *, next)));
    QVERIFY(cm->isListening());
    cm->handleInput(0, 40, 127);
    ControlCollisionBox *box = panel.encoderCollision();
    QVERIFY2(box->isRefusing() && box->text().contains("Scene: Warm (Control Map)"), qPrintable(box->text()));
    QCOMPARE(enc->device().bankNext, -1);            // not taken yet
    box->moveButton()->click();
    QVERIFY(cm->binding(ControlMap::function(a->id())) == nullptr);
    QCOMPARE(enc->device().bankNext, 40);
    QVERIFY(!box->isRefusing());
    // The trigger no longer fires; the bank steps on it.
    QSignalSpy fired(cm, &ControlMap::fired);
    QSignalSpy bank(enc, &EncoderBank::bankChanged);
    cm->handleInput(0, 40, 0);
    cm->handleInput(0, 40, 127);
    enc->handleInput(0, 40, 127);
    QCOMPARE(fired.count(), 0);
    QCOMPARE(bank.count(), 1);
}

/* A Virtual Console widget's External Input (auto-detect), a speed master's
   Inputs… and the Grand Master's input refuse a control with another job,
   with Move It Here. */
void ControlMapUi_Test::theInputPickersRefuseAndMove()
{
    Scene *a = scene("Warm");
    ControlMap *cm = m_doc->controlMap();
    InputOutputMap *iom = m_doc->inputOutputMap();
    new VirtualConsole(nullptr, m_doc);
    auto vcGone = qScopeGuard([]() { delete VirtualConsole::instance(); });
    cm->setBinding({ 0, 60, ControlMap::function(a->id()), ControlMap::Toggle });

    // A widget's External Input, auto-detected.
    InputSelectionWidget isw(m_doc);
    QSignalSpy picked(&isw, &InputSelectionWidget::inputValueChanged);
    isw.m_autoDetectInputButton->setChecked(true);
    emit iom->inputValueChanged(0, 60, 127);
    QVERIFY(isw.collisionBox()->isRefusing());
    QVERIFY2(isw.collisionBox()->text().contains("already does Scene: Warm (Control Map). One job per control."),
             qPrintable(isw.collisionBox()->text()));
    QVERIFY(isw.inputSource().isNull());
    QCOMPARE(picked.count(), 0);
    isw.collisionBox()->moveButton()->click();
    QVERIFY(cm->binding(ControlMap::function(a->id())) == nullptr);
    QVERIFY(!isw.inputSource().isNull());
    QCOMPARE(isw.inputSource()->channel(), 60u);
    QCOMPARE(picked.count(), 1);
    isw.m_autoDetectInputButton->setChecked(false);

    // A speed master's input on another master's control.
    SpeedMasters *sm = m_doc->speedMasters();
    sm->setInput(1, SpeedMasters::RateControl, 0, 70);
    InputSelectionWidget tap(m_doc);
    ControlOwners::Owner self;
    self.kind = ControlOwners::SpeedMaster;
    self.index = 0;
    self.part = SpeedMasters::TapControl;
    tap.setListener(self);
    tap.m_autoDetectInputButton->setChecked(true);
    emit iom->inputValueChanged(0, 70, 127);
    QVERIFY2(tap.collisionBox()->isRefusing() && tap.collisionBox()->text().contains("Rate (Speed Masters)"),
             qPrintable(tap.collisionBox()->text()));
    tap.collisionBox()->moveButton()->click();
    QVERIFY(!sm->input(1, SpeedMasters::RateControl, nullptr, nullptr));
    tap.m_autoDetectInputButton->setChecked(false);

    // The Grand Master's input on a trigger's control.
    cm->setBinding({ 0, 61, ControlMap::function(a->id()), ControlMap::Toggle });
    VCPropertiesEditor vcpe(nullptr, VirtualConsole::instance()->properties(), iom);
    QVERIFY(QMetaObject::invokeMethod(&vcpe, "slotGrandMasterInputValueChanged",
                                      Q_ARG(quint32, 0), Q_ARG(quint32, 61)));
    ControlCollisionBox *gm = vcpe.grandMasterCollision();
    QVERIFY2(gm->isRefusing() && gm->text().contains("Scene: Warm"), qPrintable(gm->text()));
    QVERIFY(vcpe.properties().grandMasterInputUniverse() == InputOutputMap::invalidUniverse());
    gm->moveButton()->click();
    QCOMPARE(vcpe.properties().grandMasterInputChannel(), 61u);
    // Taken on OK, not before (the window applies only then).
    QVERIFY(gm->movesOnOk());
    QVERIFY(!cm->isEmpty());
    vcpe.accept();
    QVERIFY(cm->isEmpty());
}

/* Move It Here inside a window that applies on OK (a widget's Properties,
   the Virtual Console's Properties): the control is taken from the other
   listener on OK, never before -- Cancel leaves both as they were. */
void ControlMapUi_Test::aMoveInAPropertiesWindowWaitsForOk()
{
    Scene *a = scene("Warm");
    ControlMap *cm = m_doc->controlMap();
    InputOutputMap *iom = m_doc->inputOutputMap();
    new VirtualConsole(nullptr, m_doc);
    auto vcGone = qScopeGuard([]() { delete VirtualConsole::instance(); });
    const ControlMap::Target warm = ControlMap::function(a->id());
    cm->setBinding({ 0, 60, warm, ControlMap::Toggle });

    for (const bool ok : { false, true })
    {
        QDialog dlg;
        QVBoxLayout *lay = new QVBoxLayout(&dlg);
        InputSelectionWidget *isw = new InputSelectionWidget(m_doc, &dlg);
        lay->addWidget(isw);
        isw->m_autoDetectInputButton->setChecked(true);
        emit iom->inputValueChanged(0, 60, 127);
        isw->m_autoDetectInputButton->setChecked(false);
        QVERIFY(isw->collisionBox()->isRefusing());
        QVERIFY(isw->collisionBox()->movesOnOk());
        QVERIFY2(isw->collisionBox()->findChild<QLabel *>("collisionNote")->text().contains("when you press OK"),
                 qPrintable(isw->collisionBox()->findChild<QLabel *>("collisionNote")->text()));
        isw->collisionBox()->moveButton()->click();
        QVERIFY(!isw->inputSource().isNull());
        QCOMPARE(isw->inputSource()->channel(), 60u);
        // Not yet: the trigger keeps it until OK.
        QVERIFY2(cm->binding(warm) != nullptr, "Move It Here took the control before OK");
        QCOMPARE(isw->pendingMove().count(), 1);
        if (ok)
            dlg.accept();
        else
            dlg.reject();
        QCOMPARE(cm->binding(warm) == nullptr, ok);
    }

    // The Grand Master's input, Cancel: the trigger keeps its control.
    cm->setBinding({ 0, 61, warm, ControlMap::Toggle });
    {
        VCPropertiesEditor vcpe(nullptr, VirtualConsole::instance()->properties(), iom);
        QVERIFY(QMetaObject::invokeMethod(&vcpe, "slotGrandMasterInputValueChanged",
                                          Q_ARG(quint32, 0), Q_ARG(quint32, 61)));
        QVERIFY(vcpe.grandMasterCollision()->isRefusing());
        vcpe.grandMasterCollision()->moveButton()->click();
        QCOMPARE(vcpe.grandMasterPendingMove().count(), 1);
        vcpe.reject();
    }
    QVERIFY(cm->binding(warm) != nullptr && cm->binding(warm)->channel == 61u);
}

/* The emergency stop: nothing takes its control (no Move It Here), and its
   own Learn takes a control from anything else, with Move It Here. */
void ControlMapUi_Test::theStopIsNeverTakenButCanTake()
{
    Scene *a = scene("Warm");
    ControlMap *cm = m_doc->controlMap();
    InputOutputMap *iom = m_doc->inputOutputMap();
    auto vcGone = qScopeGuard([]() { delete VirtualConsole::instance(); });
    VCButton *btn = vcButtonOn(m_doc, 88, "Blackout");
    cm->setBinding({ 0, 9, ControlMap::emergencyStop(), ControlMap::StartOnly });

    // A widget's input on the stop's control: refused, no Move It Here.
    InputSelectionWidget isw(m_doc);
    isw.m_autoDetectInputButton->setChecked(true);
    emit iom->inputValueChanged(0, 9, 127);
    QVERIFY2(isw.collisionBox()->isRefusing()
             && isw.collisionBox()->text().contains("the emergency stop (Control Map)"),
             qPrintable(isw.collisionBox()->text()));
    QVERIFY(!isw.collisionBox()->moveButton()->isVisibleTo(&isw));
    QVERIFY(!ControlOwners::releaseAll(m_doc, isw.collisionBox()->owners()));
    QVERIFY(isw.inputSource().isNull());
    isw.m_autoDetectInputButton->setChecked(false);

    // A trigger's learn never hears it: the stop acts first, learning or not.
    {
        ControlLearnDialog d(m_doc, ControlMap::function(a->id()));
        d.show();
        cm->handleInput(0, 9, 0);
        cm->handleInput(0, 9, 127);
        QVERIFY(!d.hasControl());
        d.reject();
    }
    QVERIFY(cm->binding(ControlMap::emergencyStop())->channel == 9u);

    // The stop's own learn on the button's control: Move It Here takes it.
    cm->removeBinding(ControlMap::emergencyStop());
    ControlLearnDialog d(m_doc, ControlMap::emergencyStop());
    d.show();
    cm->handleInput(0, 88, 127);
    cm->handleInput(0, 88, 0);
    cm->handleInput(0, 88, 127);
    ControlCollisionBox *box = d.findChild<ControlCollisionBox *>("controlCollision");
    QVERIFY2(box->isRefusing() && box->text().contains("Button \"Blackout\""), qPrintable(box->text()));
    QVERIFY(box->moveButton()->isVisibleTo(&d));
    box->moveButton()->click();
    QVERIFY(btn->inputSource(0).isNull());
    const ControlMap::Binding *stop = cm->binding(ControlMap::emergencyStop());
    QVERIFY(stop != nullptr && stop->channel == 88u);
}

/* A show that already has two jobs on a control (saved before the refusal)
   loads as it is, and the hub's Who has it now lists it, with the fix. */
void ControlMapUi_Test::theHubListsControlsWithTwoJobs()
{
    Scene *a = scene("Warm");
    ControlMap *cm = m_doc->controlMap();
    auto vcGone = qScopeGuard([]() { delete VirtualConsole::instance(); });
    VCButton *btn = vcButtonOn(m_doc, 60, "House lights");
    // As a show loads it: no Learn, so nothing refused.
    QByteArray xml = QByteArrayLiteral("<ControlMap><Trigger Universe=\"0\" Channel=\"60\" Target=\"Function\" ID=\"")
            + QByteArray::number(a->id()) + QByteArrayLiteral("\" Mode=\"Toggle\"/></ControlMap>");
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(cm->loadXML(r));
    QCOMPARE(cm->count(), 1);
    QVERIFY(btn->checkInputSource(0, 60, 127, nullptr));

    ShowControlSyncPage page(m_doc);
    ControlSurfacesPanel *hub = page.surfaces();
    hub->refresh();
    const QString text = hub->status("collisions");
    QVERIFY2(text.contains("Scene: Warm (Control Map)") && text.contains("Button \"House lights\" (Virtual Console)"),
             qPrintable(text));
    QVERIFY2(text.contains("Move It Here") && text.contains("Control Map table ▸ Remove")
             && text.contains("External Input"), qPrintable(text));
    QVERIFY2(hub->summary().contains("1 control with two jobs"), qPrintable(hub->summary()));
    QVERIFY(!hub->findChild<QLabel *>("surfaceCollisionsStatus")->isHidden());
    QCOMPARE(ControlOwners::collisions(m_doc).count(), 1);
    QVERIFY(ControlOwners::collisions(m_doc).first().owners.first().place == ControlMap::everywhere());

    // Fixed (the trigger removed): one job again.
    cm->removeBinding(ControlMap::function(a->id()));
    QCOMPARE(hub->status("collisions"), QString("Every control has one job"));
    QVERIFY(hub->findChild<QLabel *>("surfaceCollisionsStatus")->isHidden());
}

/* With QLC_SHOT_DIR: the refusal in the Learn dialog (movable and fixed)
   and the hub's Who has it now row, at three sizes (UX review). */
void ControlMapUi_Test::collisionShots()
{
    const QByteArray env = qgetenv("QLC_SHOT_DIR");
    if (env.isEmpty())
        QSKIP("QLC_SHOT_DIR is not set");
    const QDir dir(QString::fromLocal8Bit(env));
    QDir().mkpath(dir.absolutePath());
    Scene *a = scene("Warm wash");
    ControlMap *cm = m_doc->controlMap();
    auto vcGone = qScopeGuard([]() { delete VirtualConsole::instance(); });
    vcButtonOn(m_doc, 60, "House lights");
    {
        ControlLearnDialog d(m_doc, ControlMap::function(a->id()));
        d.show();
        QVERIFY(QTest::qWaitForWindowExposed(&d));
        cm->handleInput(0, 60, 127);
        QCoreApplication::processEvents();
        d.adjustSize();
        QVERIFY(d.grab().save(dir.absoluteFilePath("collision-learn.png")));
        d.reject();
    }
    // A widget's External Input on the stop's control: refused, no Move It Here.
    cm->setBinding({ 0, 9, ControlMap::emergencyStop(), ControlMap::StartOnly });
    {
        QDialog d;
        QVBoxLayout *lay = new QVBoxLayout(&d);
        InputSelectionWidget *isw = new InputSelectionWidget(m_doc, &d);
        lay->addWidget(isw);
        d.show();
        QVERIFY(QTest::qWaitForWindowExposed(&d));
        isw->m_autoDetectInputButton->setChecked(true);
        emit m_doc->inputOutputMap()->inputValueChanged(0, 9, 127);
        isw->m_autoDetectInputButton->setChecked(false);
        QVERIFY(isw->collisionBox()->isRefusing());
        QCoreApplication::processEvents();
        d.adjustSize();
        QVERIFY(d.grab().save(dir.absoluteFilePath("collision-input-stop.png")));
    }
    // ... on a trigger's control, in a window with OK: taken on OK, said.
    cm->setBinding({ 0, 61, ControlMap::function(a->id()), ControlMap::Toggle });
    {
        QDialog d;
        QVBoxLayout *lay = new QVBoxLayout(&d);
        InputSelectionWidget *isw = new InputSelectionWidget(m_doc, &d);
        lay->addWidget(isw);
        d.show();
        QVERIFY(QTest::qWaitForWindowExposed(&d));
        isw->m_autoDetectInputButton->setChecked(true);
        emit m_doc->inputOutputMap()->inputValueChanged(0, 61, 127);
        isw->m_autoDetectInputButton->setChecked(false);
        QVERIFY(isw->collisionBox()->isRefusing());
        QCoreApplication::processEvents();
        d.adjustSize();
        QVERIFY(d.grab().save(dir.absoluteFilePath("collision-input-onok.png")));
    }
    cm->removeBinding(ControlMap::function(a->id()));
    // An encoder of the bank's run: refused with no Move It Here, and why.
    {
        EncoderBank::Device dev;
        dev.universe = 0;
        dev.encoders = 16;
        dev.encoderCount = 8;
        cm->encoderBank()->setDevice(dev, QStringLiteral("custom"));
        ControlLearnDialog d(m_doc, ControlMap::function(a->id()));
        d.show();
        QVERIFY(QTest::qWaitForWindowExposed(&d));
        cm->handleInput(0, 18, 127);
        QCoreApplication::processEvents();
        d.adjustSize();
        QVERIFY(d.grab().save(dir.absoluteFilePath("collision-learn-encoder.png")));
        d.reject();
        cm->encoderBank()->setEnabled(false);
    }
    // A show that has two jobs on one control: the hub lists it.
    cm->setBinding({ 0, 60, ControlMap::function(a->id()), ControlMap::Toggle });
    InputOutputManager mgr(nullptr, m_doc);
    for (const QSize &sz : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
    {
        mgr.resize(sz);
        mgr.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mgr));
        mgr.showShowControlPage();
        mgr.findChild<ConnectionsTree *>()->selectShowControlRow(QStringLiteral("surfaces"));
        QCoreApplication::processEvents();
        QVERIFY(mgr.grab().save(dir.absoluteFilePath(QString("collision-hub-%1x%2.png").arg(sz.width()).arg(sz.height()))));
    }
}

/****************************************************************************
 * Places for triggers (CONTROL_SCOPES.md §8 phase 3, board Q295, Q296, Q303)
 ****************************************************************************/

namespace
{
/** A production with a cue list of two cues; @a cues: their ids. */
quint32 placesShow(Doc *doc, QList<quint32> *cues, quint32 *prod = nullptr)
{
    ShowTreeController *st = doc->showTree();
    const quint32 p = st->addProduction("Show");
    const quint32 list = st->addCueList(p, "Act 1");
    *cues << st->addCue(list, -1, "Top") << st->addCue(list, 0, "Chorus");
    if (prod != nullptr)
        *prod = p;
    return list;
}

/** Opens a context menu (@a open), chooses Learn Trigger… on it and returns
 *  the place its dialog offers first (Works), and its text. */
ControlMap::Place placeLearnOffers(const std::function<void()> &open, QString *text, bool *seen,
                                   QStringList *items = nullptr)
{
    ControlMap::Place place;
    *seen = false;
    QTimer dialogWatch;
    dialogWatch.setInterval(10);
    QObject::connect(&dialogWatch, &QTimer::timeout, [&]() {
        ControlLearnDialog *d = qobject_cast<ControlLearnDialog *>(QApplication::activeModalWidget());
        if (d == nullptr)
            return;
        dialogWatch.stop();
        place = d->place();
        QComboBox *works = d->findChild<QComboBox *>("learnPlace");
        *text = works->currentText();
        if (items != nullptr)
            for (int i = 0; i < works->count(); ++i)
                *items << works->itemText(i);
        *seen = true;
        d->reject();
    });
    QTimer driver;
    driver.setInterval(10);
    QObject::connect(&driver, &QTimer::timeout, [&]() {
        QMenu *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (menu == nullptr)
            return;
        driver.stop();
        for (QAction *a : menu->actions())
            if (a->objectName() == QLatin1String("learnTrigger"))
            {
                dialogWatch.start();
                a->trigger();
                break;
            }
        menu->close();
    });
    driver.start();
    open();
    return place;
}
}

/* Learn Trigger… offers the place (Works): where it was learned by default,
   the places around it, and everywhere -- the old behaviour, kept. */
void ControlMapUi_Test::theLearnDialogOffersThePlace()
{
    QList<quint32> cues;
    quint32 prod = 0;
    const quint32 list = placesShow(m_doc, &cues, &prod);
    ControlMap *cm = m_doc->controlMap();
    {
        ControlLearnDialog d(m_doc, ControlMap::cue(cues.at(0)), nullptr, ControlMap::inCue(cues.at(0)));
        QComboBox *works = d.findChild<QComboBox *>("learnPlace");
        QVERIFY(works != nullptr && works->isEnabled());
        // A jump to a cue: everywhere by default (it brings the cue live),
        // the cue offered.
        QVERIFY(d.place() == ControlMap::everywhere());
        d.setPlace(ControlMap::inCue(cues.at(0)));
        QVERIFY(d.place() == ControlMap::inCue(cues.at(0)));
        // The cue, its list, its production, everywhere.
        QCOMPARE(works->count(), 4);
        QVERIFY2(works->itemText(0).startsWith("In cue "), qPrintable(works->itemText(0)));
        QCOMPARE(works->itemText(1), QString("In cue list Act 1"));
        QCOMPARE(works->itemText(2), QString("In production Show"));
        QCOMPARE(works->itemText(3), QString("Everywhere"));
        QVERIFY(d.learnKey(Qt::Key_F5, Qt::NoModifier));
        QVERIFY2(d.findChild<QLabel *>("learnPlaceNote")->text().contains("in cue"),
                 qPrintable(d.findChild<QLabel *>("learnPlaceNote")->text()));
        d.accept();
    }
    QVERIFY(cm->binding(ControlMap::cue(cues.at(0)))->place == ControlMap::inCue(cues.at(0)));
    // A list: the list, its production, everywhere; another place chosen is kept.
    {
        ControlLearnDialog d(m_doc, ControlMap::cueList(list), nullptr, ControlMap::inList(list));
        QCOMPARE(d.findChild<QComboBox *>("learnPlace")->count(), 3);
        d.setPlace(ControlMap::inProduction(prod));
        QVERIFY(d.learnKey(Qt::Key_F7, Qt::NoModifier));
        d.accept();
    }
    QVERIFY(cm->binding(ControlMap::cueList(list))->place == ControlMap::inProduction(prod));
    // Learned with no place (a scene from Build): everywhere, as before places.
    Scene *a = scene("Warm");
    {
        ControlLearnDialog d(m_doc, ControlMap::function(a->id()));
        QVERIFY(d.place() == ControlMap::everywhere());
        QVERIFY(d.learnKey(Qt::Key_F8, Qt::NoModifier));
        d.accept();
    }
    QVERIFY(cm->binding(ControlMap::function(a->id()))->place == ControlMap::everywhere());
    // The emergency stop: everywhere, and nothing else to choose.
    {
        ControlLearnDialog d(m_doc, ControlMap::emergencyStop());
        QVERIFY(!d.findChild<QComboBox *>("learnPlace")->isEnabled());
        QVERIFY(d.place() == ControlMap::everywhere());
    }
    // The menu says where a learned one works.
    QWidget owner;
    QMenu m;
    QAction *act = ControlLearnDialog::addTo(&m, m_doc, ControlMap::cue(cues.at(0)), &owner);
    QVERIFY2(act->text().contains("· in cue"), qPrintable(act->text()));
}

/* Right-click ▸ Learn Trigger… in the Show Tree: the place is where you
   right-clicked -- the cue, the list, the hit's own place. */
void ControlMapUi_Test::theShowTreeLearnsWhereYouRightClicked()
{
    QList<quint32> cues;
    const quint32 list = placesShow(m_doc, &cues);
    ShowTreeController *st = m_doc->showTree();
    const quint32 hit = st->addHit(ShowTree::HitScope::Cue, cues.at(1), "Stab");
    QVERIFY(hit != ShowTree::invalidId());

    ShowTreeView view(nullptr, m_doc);
    view.resize(900, 700);
    view.show();
    QCoreApplication::processEvents();
    view.tree()->expandAll();
    QStringList items;
    auto offered = [&view, &items](quint32 id, QString *text) {
        bool seen = false;
        items.clear();
        if (!view.selectNode(id))
            return ControlMap::Place{ ControlMap::Everywhere, 0 };
        QTreeWidgetItem *item = view.tree()->currentItem();
        view.tree()->scrollToItem(item);
        const QPoint pos = view.tree()->visualItemRect(item).center();
        const ControlMap::Place p = placeLearnOffers([&]() {
            QMetaObject::invokeMethod(&view, "slotTreeContextMenu", Q_ARG(QPoint, pos));
        }, text, &seen, &items);
        return seen ? p : ControlMap::Place{ ControlMap::ListPlace, 0 };
    };
    QString text;
    // A GO on a list or a jump to a cue brings that place live: everywhere
    // by default (its own place would arm it only once it is already live),
    // the place right-clicked offered.
    QVERIFY(offered(list, &text) == ControlMap::everywhere());
    QCOMPARE(text, QString("Everywhere"));
    QVERIFY2(items.contains("In cue list Act 1"), qPrintable(items.join(" | ")));
    QVERIFY(offered(cues.at(0), &text) == ControlMap::everywhere());
    QVERIFY2(items.contains("In cue list Act 1") && items.first().startsWith("In cue "),
             qPrintable(items.join(" | ")));
    // A hit works at its own place, and is offered nothing wider: there it
    // could not fire, and would hold the control dark.
    QVERIFY(offered(hit, &text) == ControlMap::inCue(cues.at(1)));
    QVERIFY2(!items.contains("Everywhere") && !items.contains("In cue list Act 1"), qPrintable(items.join(" | ")));
}

/* A GO learned from a cue list's or a cue's right-click, its Works left as
   it was, fires in Operate: it starts a list that isn't running and jumps to
   a cue that isn't live (learned at its own place it never could). */
void ControlMapUi_Test::aGoLearnedFromTheTreeFiresInOperate()
{
    QList<quint32> cues;
    quint32 prod = 0;
    placesShow(m_doc, &cues, &prod);
    ShowTreeController *st = m_doc->showTree();
    const quint32 encore = st->addCueList(prod, "Encore");
    const quint32 last = st->addCue(encore, -1, "Bows");
    QVERIFY(last != ShowTree::invalidId());
    ControlMap *cm = m_doc->controlMap();
    const quint32 f9 = ControlMap::keyChannel(Qt::Key_F9, Qt::NoModifier);
    const quint32 f10 = ControlMap::keyChannel(Qt::Key_F10, Qt::NoModifier);
    {
        ControlLearnDialog d(m_doc, ControlMap::cueList(encore), nullptr, ControlMap::inList(encore));
        QVERIFY(d.learnKey(Qt::Key_F9, Qt::NoModifier));
        d.accept();
    }
    {
        ControlLearnDialog d(m_doc, ControlMap::cue(cues.at(1)), nullptr, ControlMap::inCue(cues.at(1)));
        QVERIFY(d.learnKey(Qt::Key_F10, Qt::NoModifier));
        d.accept();
    }
    QVERIFY(cm->binding(ControlMap::cueList(encore)) != nullptr);
    QVERIFY(cm->binding(ControlMap::cue(cues.at(1))) != nullptr);
    m_doc->setMode(Doc::Operate);
    // Nothing running, Encore not the main list, cue 2 not live: armed all the same.
    QCOMPARE(cm->armedOn(ControlMap::KeyboardUniverse, f9).count(), 1);
    QCOMPARE(cm->armedOn(ControlMap::KeyboardUniverse, f10).count(), 1);
    m_doc->setMode(Doc::Design);
}

/* Q296: a trigger at another place shares the control, and Learn says which
   one has it where. Only the same place is refused -- and a button may Step
   Through Them there (Q303). */
void ControlMapUi_Test::onlyTheSamePlaceIsRefused()
{
    QList<quint32> cues;
    placesShow(m_doc, &cues);
    Scene *warm = scene("Warm");
    Scene *strobe = scene("Strobe");
    Scene *cool = scene("Cool");
    ControlMap *cm = m_doc->controlMap();
    const quint32 f6 = ControlMap::keyChannel(Qt::Key_F6, Qt::NoModifier);
    cm->setBinding({ ControlMap::KeyboardUniverse, f6, ControlMap::function(warm->id()), ControlMap::Toggle });
    {
        ControlLearnDialog d(m_doc, ControlMap::function(strobe->id()), nullptr, ControlMap::inCue(cues.at(1)));
        QVERIFY(d.learnKey(Qt::Key_F6, Qt::NoModifier));
        ControlCollisionBox *box = d.findChild<ControlCollisionBox *>();
        QVERIFY2(!box->isRefusing(), qPrintable(box->text()));
        const QString note = d.findChild<QLabel *>("learnPlaceNote")->text();
        QVERIFY2(note.contains("Scene: Warm everywhere, except Scene: Strobe in cue"), qPrintable(note));
        // Everywhere instead: the same place as Warm, so refused.
        d.setPlace(ControlMap::everywhere());
        QVERIFY(box->isRefusing());
        QVERIFY2(box->text().contains("Warm"), qPrintable(box->text()));
        d.setPlace(ControlMap::inCue(cues.at(1)));
        QVERIFY(!box->isRefusing());
        d.accept();
    }
    QCOMPARE(cm->bindingsOn(ControlMap::KeyboardUniverse, f6).count(), 2);
    // A third, everywhere: refused (Warm is there), with Step Through Them.
    {
        ControlLearnDialog d(m_doc, ControlMap::function(cool->id()));
        d.show();
        QVERIFY(QTest::qWaitForWindowExposed(&d));
        QVERIFY(d.learnKey(Qt::Key_F6, Qt::NoModifier));
        ControlCollisionBox *box = d.findChild<ControlCollisionBox *>();
        QVERIFY(box->isRefusing());
        QVERIFY2(!box->text().contains("Strobe"), qPrintable(box->text()));    // another place: not in the way
        // The key by its name (not its low bits, "Key · 5").
        QVERIFY2(box->text().startsWith("Key · F6 already does Scene: Cool") == false
                 && box->text().startsWith("Key · F6 already does Scene: Warm"), qPrintable(box->text()));
        QVERIFY(box->offersStepThrough());
        QVERIFY(box->stepButton()->isVisible());
        QCOMPARE(box->stepButton()->text(), QString("Step Through Them"));
        QTest::mouseClick(box->stepButton(), Qt::LeftButton);
    }
    QCOMPARE(cm->bindingsOn(ControlMap::KeyboardUniverse, f6).count(), 3);
    QCOMPARE(cm->armedOn(ControlMap::KeyboardUniverse, f6).count(), 2);     // Warm and Cool step
    // Stepped through by choice: not "two jobs on one control" in the hub.
    QVERIFY(ControlOwners::collisions(m_doc).isEmpty());
    // Another listener is refused whatever the place: one job per control.
    auto vcGone = qScopeGuard([]() { delete VirtualConsole::instance(); });
    vcButtonOn(m_doc, 60, "House lights");
    {
        ControlLearnDialog d(m_doc, ControlMap::function(strobe->id()), nullptr, ControlMap::inCue(cues.at(0)));
        cm->handleInput(0, 60, 127);
        QCoreApplication::processEvents();
        ControlCollisionBox *box = d.findChild<ControlCollisionBox *>();
        QVERIFY(box->isRefusing());
        QVERIFY(!box->offersStepThrough());
        cm->handleInput(0, 60, 0);
    }
}

/* Two triggers step through one control at one place (Step Through Them).
   Opening one's dialog again and changing only its mode is not "another job
   on the control": OK is accepted, the sibling stays, they still step. */
void ControlMapUi_Test::aSteppingTriggerReopensAndSavesOnItsOwnControl()
{
    Scene *warm = scene("Warm");
    Scene *cool = scene("Cool");
    ControlMap *cm = m_doc->controlMap();
    const quint32 f6 = ControlMap::keyChannel(Qt::Key_F6, Qt::NoModifier);
    cm->setBinding({ ControlMap::KeyboardUniverse, f6, ControlMap::function(warm->id()), ControlMap::Toggle });
    {
        ControlLearnDialog d(m_doc, ControlMap::function(cool->id()));
        d.show();
        QVERIFY(QTest::qWaitForWindowExposed(&d));
        QVERIFY(d.learnKey(Qt::Key_F6, Qt::NoModifier));
        ControlCollisionBox *box = d.findChild<ControlCollisionBox *>();
        QVERIFY(box->offersStepThrough());
        QTest::mouseClick(box->stepButton(), Qt::LeftButton);
    }
    QCOMPARE(cm->armedOn(ControlMap::KeyboardUniverse, f6).count(), 2);

    ControlLearnDialog d(m_doc, ControlMap::function(warm->id()));
    d.show();
    QVERIFY(QTest::qWaitForWindowExposed(&d));
    d.setMode(ControlMap::Momentary);
    ControlCollisionBox *box = d.findChild<ControlCollisionBox *>();
    QVERIFY2(!box->isRefusing(), qPrintable(box->text()));
    d.accept();
    QCOMPARE(d.result(), int(QDialog::Accepted));
    QVERIFY(cm->binding(ControlMap::function(cool->id())) != nullptr);
    QCOMPARE(cm->binding(ControlMap::function(warm->id()))->mode, ControlMap::Momentary);
    QCOMPARE(cm->bindingsOn(ControlMap::KeyboardUniverse, f6).count(), 2);
    QCOMPARE(cm->armedOn(ControlMap::KeyboardUniverse, f6).count(), 2);     // still stepping

    // Another control is checked as usual: F7 is free, so it moves there.
    ControlLearnDialog e(m_doc, ControlMap::function(warm->id()));
    QVERIFY(e.learnKey(Qt::Key_F7, Qt::NoModifier));
    e.accept();
    QCOMPARE(cm->bindingsOn(ControlMap::KeyboardUniverse, f6).count(), 1);
}

/* Move It Here from a widget's own sub-control (a Speed Dial preset, a
   Matrix custom control, a Frame page shortcut): the sub-control fires and
   saves through its own source, so it is released there, is one job in the
   hub, and stays released through save and reload. */
void ControlMapUi_Test::moveItHereLetsGoOfASubControlsOwnInput()
{
    auto vcGone = qScopeGuard([]() { delete VirtualConsole::instance(); });
    if (VirtualConsole::instance() == nullptr)
        new VirtualConsole(nullptr, m_doc);
    QWidget *parent = VirtualConsole::instance()->contents();
    ControlMap *cm = m_doc->controlMap();

    VCSpeedDial *dial = new VCSpeedDial(parent, m_doc);
    VCSpeedDialPreset preset(1);
    preset.m_name = "Half";
    preset.m_inputSource = QSharedPointer<QLCInputSource>(new QLCInputSource(0, 60));
    dial->addPreset(preset);

    VCMatrix *matrix = new VCMatrix(parent, m_doc);
    VCMatrixControl control(2);
    control.m_type = VCMatrixControl::Color1;
    control.m_inputSource = QSharedPointer<QLCInputSource>(new QLCInputSource(0, 61));
    matrix->addCustomControl(control);

    VCFrame *frame = new VCFrame(parent, m_doc);
    frame->setHeaderVisible(true);
    frame->setMultipageMode(true);
    frame->setTotalPagesNumber(2);
    {
        VCFramePageShortcut s0(0, VCFrame::shortcutsBaseInputSourceId);
        VCFramePageShortcut s1(1, VCFrame::shortcutsBaseInputSourceId + 1);
        s1.m_inputSource = QSharedPointer<QLCInputSource>(new QLCInputSource(0, 62));
        frame->setShortcuts({ &s0, &s1 });
    }

    struct Case { VCWidget *w; quint32 ch; QString scene; };
    const QList<Case> cases = { { dial, 60, "Warm" }, { matrix, 61, "Cool" }, { frame, 62, "Strobe" } };
    for (const Case &c : cases)
    {
        // Seen as one job, before.
        const QList<ControlOwners::Owner> before = ControlOwners::owners(m_doc, 0, c.ch);
        QCOMPARE(before.count(), 1);
        QCOMPARE(before.first().kind, ControlOwners::VCInput);

        Scene *s = scene(c.scene);
        ControlLearnDialog d(m_doc, ControlMap::function(s->id()));
        d.show();
        cm->handleInput(0, c.ch, 127);
        cm->handleInput(0, c.ch, 0);
        ControlCollisionBox *box = d.findChild<ControlCollisionBox *>("controlCollision");
        QVERIFY2(box != nullptr && box->isRefusing(), qPrintable(c.scene));
        box->moveButton()->click();

        // The sub-control's own source is gone: it no longer answers.
        QVERIFY2(c.w->subInputSources().isEmpty(), qPrintable(c.scene));
        const ControlMap::Binding *b = cm->binding(ControlMap::function(s->id()));
        QVERIFY(b != nullptr && b->channel == c.ch);
        // One job, in the hub and in the owners.
        const QList<ControlOwners::Owner> after = ControlOwners::owners(m_doc, 0, c.ch);
        QCOMPARE(after.count(), 1);
        QCOMPARE(after.first().kind, ControlOwners::Trigger);

        // Saved without it, and reloaded without it.
        QString xml;
        {
            QXmlStreamWriter w(&xml);
            QVERIFY(c.w->saveXML(&w));
        }
        QVERIFY2(!xml.contains(QString("Channel=\"%1\"").arg(c.ch)), qPrintable(xml));
    }
    QVERIFY(ControlOwners::collisions(m_doc).isEmpty());

    // Reload each from its XML: nothing comes back.
    VCSpeedDial *dial2 = new VCSpeedDial(parent, m_doc);
    VCMatrix *matrix2 = new VCMatrix(parent, m_doc);
    VCFrame *frame2 = new VCFrame(parent, m_doc);
    for (VCWidget *w : QList<VCWidget *>{ dial, matrix, frame })
    {
        QString xml;
        {
            QXmlStreamWriter wr(&xml);
            QVERIFY(w->saveXML(&wr));
        }
        VCWidget *fresh = w == dial ? static_cast<VCWidget *>(dial2)
                          : w == matrix ? static_cast<VCWidget *>(matrix2) : static_cast<VCWidget *>(frame2);
        QXmlStreamReader r(xml);
        QVERIFY(r.readNextStartElement());
        QVERIFY(fresh->loadXML(r));
        QVERIFY(fresh->subInputSources().isEmpty());
    }
}

/* The Control Map's Works column and the hub's Triggers by place row: each
   trigger's place, and which one has the control now. */
void ControlMapUi_Test::theTableAndTheHubShowEachPlace()
{
    QList<quint32> cues;
    placesShow(m_doc, &cues);
    Scene *warm = scene("Warm");
    Scene *strobe = scene("Strobe");
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 60, ControlMap::function(warm->id()), ControlMap::Toggle });
    cm->setBinding({ 0, 60, ControlMap::function(strobe->id()), ControlMap::Toggle, ControlMap::inCue(cues.at(1)) });

    ControlMapPanel panel(m_doc);
    panel.show();
    QTableWidget *table = panel.table();
    QCOMPARE(table->rowCount(), 2);
    QComboBox *works0 = qobject_cast<QComboBox *>(table->cellWidget(0, 2));
    QComboBox *works1 = qobject_cast<QComboBox *>(table->cellWidget(1, 2));
    QVERIFY(works0 != nullptr && works1 != nullptr);
    QCOMPARE(works0->currentText(), QString("Everywhere"));
    QVERIFY2(works1->currentText().startsWith("In cue "), qPrintable(works1->currentText()));
    // Warm has everywhere on this control: not offered to Strobe (moving
    // there would take Warm's trigger without a word), and refused.
    QCOMPARE(works1->findText("Everywhere"), -1);
    QVERIFY(!cm->setPlace(ControlMap::function(strobe->id()), ControlMap::everywhere()));
    QCOMPARE(cm->bindingsOn(0, 60).count(), 2);
    // Moved from the table: in the list.
    const int at = works1->findText("In cue list Act 1");
    QVERIFY(at >= 0);
    works1->setCurrentIndex(at);
    emit works1->activated(at);
    QVERIFY(cm->binding(ControlMap::function(strobe->id()))->place.scope == ControlMap::ListPlace);
    cm->setPlace(ControlMap::function(strobe->id()), ControlMap::inCue(cues.at(1)));

    ShowControlSyncPage page(m_doc);
    page.resize(1000, 800);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    ControlSurfacesPanel *hub = page.surfaces();
    hub->refresh();
    QVERIFY(hub->isVisible());
    QVERIFY(!hub->findChild<QLabel *>("surfacePlacesStatus")->isHidden());
    QString text = hub->status("places");
    QVERIFY2(text.contains("Scene: Warm everywhere, except Scene: Strobe in cue"), qPrintable(text));
    QVERIFY2(text.contains("Now: Scene: Warm (everywhere)"), qPrintable(text));
    // Design, cue 2 selected in the Show Tree: its trigger has the control now.
    cm->setSelectedTargets({ ControlMap::cue(cues.at(1)) });
    QTRY_VERIFY2(hub->status("places").contains("Now: Scene: Strobe (in cue"), qPrintable(hub->status("places")));
    cm->setSelectedTargets({});
    QTRY_VERIFY(hub->status("places").contains("Now: Scene: Warm"));
    // All everywhere again (an old show): the row goes.
    cm->removeBinding(ControlMap::function(strobe->id()));
    hub->refresh();
    QVERIFY(hub->findChild<QLabel *>("surfacePlacesStatus")->isHidden());
}

/* The desk's light follows what is armed now: the control shows the
   narrowest armed trigger's light, and the table says why one is dark. */
void ControlMapUi_Test::theLightFollowsWhatIsArmedNow()
{
    QDir dir(QStringLiteral("../../../engine/test/iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(),
             "no I/O plugin stub in ../../../engine/test/iopluginstub");
    QLCIOPlugin *stub = m_doc->ioPluginCache()->plugins().at(0);
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    QVERIFY(iom->setOutputPatch(0, stub->name(), QString(), 0, true));

    QList<quint32> cues;
    placesShow(m_doc, &cues);
    Scene *warm = scene("Warm");
    Scene *strobe = scene("Strobe");
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 60, ControlMap::function(strobe->id()), ControlMap::Toggle, ControlMap::inCue(cues.at(1)) });
    ControlMapPanel panel(m_doc);
    panel.show();
    QTableWidget *table = panel.table();
    // Alone, at a cue that is not live: dark, and the table says why.
    QCOMPARE(cm->controlLamp(0, 60), ControlMap::LampOff);
    QCOMPARE(table->item(0, 4)->text(), QString("Dark (not live here)"));
    // Everywhere beside it: that one has the control, so it is ready.
    cm->setBinding({ 0, 60, ControlMap::function(warm->id()), ControlMap::Toggle });
    QCOMPARE(cm->controlLamp(0, 60), ControlMap::LampStandby);
    // Cue 2 selected (Design): Strobe has it; Warm is dark under it.
    cm->setSelectedTargets({ ControlMap::cue(cues.at(1)) });
    QTRY_COMPARE(table->item(1, 4)->text(), QString("Dark (a narrower place has it)"));
    QCOMPARE(table->item(0, 4)->text(), QString("Ready"));
    QCOMPARE(cm->bindingOn(0, 60)->target, ControlMap::function(strobe->id()));
    cm->setSelectedTargets({});

    // A hit's trigger everywhere (as every old show has it), alone on its
    // control: dark because the hit's own cue is not live -- not "a narrower
    // place has it", which nothing does.
    const quint32 hit = m_doc->showTree()->addHit(ShowTree::HitScope::Cue, cues.at(1), "Stab");
    QVERIFY(hit != ShowTree::invalidId());
    cm->setBinding({ 0, 62, ControlMap::hit(hit), ControlMap::Toggle });
    QCOMPARE(cm->binding(ControlMap::hit(hit))->place, ControlMap::everywhere());
    m_doc->setMode(Doc::Operate);           // nothing running: cue 2 is not live
    auto design = qScopeGuard([this]() { m_doc->setMode(Doc::Design); });
    panel.refresh();
    int row = -1;
    const QList<ControlMap::Binding> all = cm->bindings();
    for (int r = 0; r < all.count(); ++r)
        if (all.at(r).target == ControlMap::hit(hit))
            row = r;
    QVERIFY(row >= 0);
    QVERIFY(!cm->surfaceMayFire(ControlMap::hit(hit)));
    QCOMPARE(cm->controlLamp(0, 62), ControlMap::LampOff);
    QTRY_COMPARE(table->item(row, 4)->text(), QString("Dark (its hit is not live here)"));
}

/* With QLC_SHOT_DIR: Learn with Works and the line that says who has the
   control where; the Control Map table's Works; the hub's Triggers by place
   (UX review, three sizes). */
void ControlMapUi_Test::placeShots()
{
    const QByteArray env = qgetenv("QLC_SHOT_DIR");
    if (env.isEmpty())
        QSKIP("QLC_SHOT_DIR is not set");
    const QDir dir(QString::fromLocal8Bit(env));
    QDir().mkpath(dir.absolutePath());
    QList<quint32> cues;
    const quint32 list = placesShow(m_doc, &cues);
    Scene *warm = scene("Warm wash");
    Scene *strobe = scene("Strobe");
    ControlMap *cm = m_doc->controlMap();
    const quint32 f6 = ControlMap::keyChannel(Qt::Key_F6, Qt::NoModifier);
    cm->setBinding({ ControlMap::KeyboardUniverse, f6, ControlMap::function(warm->id()), ControlMap::Toggle });
    cm->setBinding({ 0, 61, ControlMap::cueList(list), ControlMap::Toggle, ControlMap::inList(list) });
    {
        ControlLearnDialog d(m_doc, ControlMap::function(strobe->id()), nullptr, ControlMap::inCue(cues.at(1)));
        d.show();
        QVERIFY(QTest::qWaitForWindowExposed(&d));
        QVERIFY(d.learnKey(Qt::Key_F6, Qt::NoModifier));
        QCoreApplication::processEvents();
        d.adjustSize();
        QVERIFY(d.grab().save(dir.absoluteFilePath("places-learn.png")));
        d.setPlace(ControlMap::everywhere());
        QCoreApplication::processEvents();
        d.adjustSize();
        QVERIFY(d.grab().save(dir.absoluteFilePath("places-learn-same-place.png")));
        d.setPlace(ControlMap::inCue(cues.at(1)));
        d.accept();
    }
    cm->setSelectedTargets({ ControlMap::cue(cues.at(1)) });
    QCoreApplication::processEvents();
    InputOutputManager mgr(nullptr, m_doc);
    for (const QSize &sz : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
    {
        mgr.resize(sz);
        mgr.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mgr));
        mgr.showShowControlPage();
        ConnectionsTree *tree = mgr.findChild<ConnectionsTree *>();
        tree->selectShowControlRow(QStringLiteral("surfaces"));
        QCoreApplication::processEvents();
        // The Triggers by place row, in view (it is the last row).
        QLabel *places = mgr.findChild<QLabel *>("surfacePlacesStatus");
        QVERIFY(places != nullptr && places->isVisibleTo(&mgr));
        for (QScrollArea *area : mgr.findChildren<QScrollArea *>())
            if (area->isAncestorOf(places))
                area->ensureWidgetVisible(places);
        QCoreApplication::processEvents();
        const QString tag = QString("%1x%2").arg(sz.width()).arg(sz.height());
        QVERIFY(mgr.grab().save(dir.absoluteFilePath(QString("places-hub-%1.png").arg(tag))));
        tree->selectShowControlRow(QStringLiteral("map"));
        QCoreApplication::processEvents();
        QVERIFY(mgr.grab().save(dir.absoluteFilePath(QString("places-table-%1.png").arg(tag))));
    }
}

/* UX-36: at the smallest size the Control Map table's Fires column wraps
   what each control fires, never cuts it ("Scene: War…"), so two rows on
   one key can be told apart. */
void ControlMapUi_Test::theFiresColumnNeverCutsAName()
{
    QList<quint32> cues;
    const quint32 list = placesShow(m_doc, &cues);
    Scene *warm = scene("Warm wash");
    Scene *strobe = scene("Strobe");
    ControlMap *cm = m_doc->controlMap();
    const quint32 f6 = ControlMap::keyChannel(Qt::Key_F6, Qt::NoModifier);
    cm->setBinding({ ControlMap::KeyboardUniverse, f6, ControlMap::function(warm->id()), ControlMap::Toggle });
    cm->setBinding({ 0, 62, ControlMap::cueList(list), ControlMap::Toggle, ControlMap::inList(list) });
    cm->setBinding({ ControlMap::KeyboardUniverse, f6, ControlMap::function(strobe->id()), ControlMap::Toggle,
                     ControlMap::inCue(cues.at(1)) });
    InputOutputManager mgr(nullptr, m_doc);
    mgr.resize(1024, 640);
    mgr.show();
    QVERIFY(QTest::qWaitForWindowExposed(&mgr));
    mgr.showShowControlPage();
    mgr.findChild<ConnectionsTree *>()->selectShowControlRow(QStringLiteral("map"));
    QCoreApplication::processEvents();
    QTableWidget *table = mgr.findChild<QTableWidget *>("controlMapTable");
    QVERIFY(table != nullptr && table->isVisible());
    QCOMPARE(table->rowCount(), 3);
    QTRY_VERIFY(table->columnWidth(1) > 0);
    QCoreApplication::processEvents();
    const QFontMetrics fm(table->font());
    for (int r = 0; r < table->rowCount(); ++r)
    {
        const QString text = table->item(r, 1)->text();
        const int width = table->columnWidth(1) - 8;           // the cell's text margins
        const QRect need = fm.boundingRect(QRect(0, 0, width, 10000), Qt::TextWordWrap, text);
        QVERIFY2(need.width() <= width && need.height() <= table->rowHeight(r),
                 qPrintable(QString("row %1 \"%2\" needs %3x%4, has %5x%6").arg(r).arg(text)
                            .arg(need.width()).arg(need.height()).arg(width).arg(table->rowHeight(r))));
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
             QStringList({ "map", "estop", "encoder", "vc", "masters", "pmj", "web", "pads", "collisions",
                          "places" }));
    QVERIFY(hub->setUpButton("places") == nullptr);         // read only
    QVERIFY(hub->findChild<QLabel *>("surfacePlacesStatus")->isHidden());       // no trigger at a place
    QCOMPARE(hub->status("collisions"), QString("Every control has one job"));
    QVERIFY(hub->setUpButton("collisions") == nullptr);     // read only
    QVERIFY(hub->findChild<QLabel *>("surfaceCollisionsStatus")->isHidden());   // nothing to say
    for (const QString &key : ControlSurfacesPanel::keys())
    {
        if (key == QLatin1String("collisions") || key == QLatin1String("places"))
            continue;
        QVERIFY2(hub->setUpButton(key) != nullptr, qPrintable(key));
        QCOMPARE(hub->setUpButton(key)->text(), QString("Set Up…"));
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

/* Cancel in the collision box leaves both triggers exactly as they were.
   It used to put the dialog back on its own control but leave Works at
   the place that was refused: OK then bound it there and replaced the
   trigger that had it, without a word. */
void ControlMapUi_Test::cancelInTheCollisionBoxLeavesBothTriggers()
{
    QList<quint32> cues;
    placesShow(m_doc, &cues);
    Scene *warm = scene("Warm");
    Scene *strobe = scene("Strobe");
    ControlMap *cm = m_doc->controlMap();
    const quint32 f6 = ControlMap::keyChannel(Qt::Key_F6, Qt::NoModifier);
    cm->setBinding({ ControlMap::KeyboardUniverse, f6, ControlMap::function(warm->id()), ControlMap::Toggle });
    cm->setBinding({ ControlMap::KeyboardUniverse, f6, ControlMap::function(strobe->id()), ControlMap::Toggle,
                     ControlMap::inCue(cues.at(1)) });
    const QList<ControlMap::Binding> before = cm->bindings();
    {
        ControlLearnDialog d(m_doc, ControlMap::function(strobe->id()));
        d.show();
        QVERIFY(QTest::qWaitForWindowExposed(&d));
        ControlCollisionBox *box = d.findChild<ControlCollisionBox *>("controlCollision");
        // Works moved onto Warm's place: refused.
        d.setPlace(ControlMap::everywhere());
        QVERIFY(box->isRefusing());
        QVERIFY2(box->text().contains("Warm"), qPrintable(box->text()));
        QTest::mouseClick(box->cancelButton(), Qt::LeftButton);
        QVERIFY(!box->isRefusing());
        // Back where it was, and OK changes nothing.
        QCOMPARE(d.place(), ControlMap::inCue(cues.at(1)));
        QVERIFY(d.hasControl());
        d.accept();
    }
    QCOMPARE(cm->bindings(), before);
    // OK straight after a refused place (no Cancel) never takes it either.
    {
        ControlLearnDialog d(m_doc, ControlMap::function(strobe->id()));
        d.setPlace(ControlMap::everywhere());
        QVERIFY(!d.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
        d.accept();
        QVERIFY(d.result() != QDialog::Accepted);
    }
    QCOMPARE(cm->bindings(), before);
}

/* A trigger whose place was deleted, or whose place is from a newer
   version: the hub's Triggers by place row and the table say so. */
void ControlMapUi_Test::theHubSaysAPlaceDeletedOrFromANewerVersion()
{
    QList<quint32> cues;
    placesShow(m_doc, &cues);
    Scene *warm = scene("Warm");
    Scene *strobe = scene("Strobe");
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 60, ControlMap::function(strobe->id()), ControlMap::Toggle, ControlMap::inCue(cues.at(1)) });
    QVERIFY(m_doc->showTree()->removeCue(cues.at(1)));
    QCOMPARE(cm->binding(ControlMap::function(strobe->id()))->place.scope, ControlMap::ProductionPlace);
    const QByteArray xml =
        QString("<ControlMap><Trigger Universe=\"0\" Channel=\"61\" Target=\"Function\" ID=\"%1\" "
                "Scope=\"Effect\" ScopeOf=\"3\"/></ControlMap>").arg(warm->id()).toUtf8();
    // Loaded beside the first (as a show would have them both).
    ControlMap::Binding strobeB = *cm->binding(ControlMap::function(strobe->id()));
    QXmlStreamReader r(xml);
    QVERIFY(r.readNextStartElement());
    QVERIFY(cm->loadXML(r));
    QCOMPARE(cm->binding(ControlMap::function(warm->id()))->place.scope, ControlMap::NewerPlace);
    cm->setBinding(strobeB);
    QVERIFY(m_doc->showTree()->removeCue(cues.at(0)));      // nothing of it to widen: still said

    ShowControlSyncPage page(m_doc);
    page.resize(1000, 800);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    ControlSurfacesPanel *hub = page.surfaces();
    hub->refresh();
    QVERIFY(!hub->findChild<QLabel *>("surfacePlacesStatus")->isHidden());
    const QString text = hub->status("places");
    QVERIFY2(text.contains("place from a newer version"), qPrintable(text));
    QVERIFY2(text.contains("it never fires here"), qPrintable(text));

    // The widened one: said in the table's Works and the hub (set again by
    // loadXML above, so widened anew here).
    QVERIFY(m_doc->showTree()->undo());
    QVERIFY(m_doc->showTree()->undo());
    QVERIFY(m_doc->showTree()->cue(cues.at(1)) != nullptr);
    cm->setPlace(ControlMap::function(strobe->id()), ControlMap::inCue(cues.at(1)));
    QVERIFY(m_doc->showTree()->removeCue(cues.at(1)));
    hub->refresh();
    QVERIFY2(hub->status("places").contains("which was deleted"), qPrintable(hub->status("places")));
    ControlMapPanel panel(m_doc);
    panel.show();
    QTableWidget *table = panel.table();
    bool saidWidened = false, saidNewer = false;
    for (int row = 0; row < table->rowCount(); ++row)
    {
        QComboBox *works = qobject_cast<QComboBox *>(table->cellWidget(row, 2));
        QVERIFY(works != nullptr);
        saidWidened |= works->toolTip().contains("which was deleted");
        saidNewer |= works->toolTip().contains("newer version");
    }
    QVERIFY(saidWidened);
    QVERIFY(saidNewer);
}

/* Move It Here takes a control from all its owners or none: one that can't
   let go leaves every one with its control (it used to release the first
   ones, then fail, leaving the control with no owner at all). */
void ControlMapUi_Test::moveItHereIsAllOrNothing()
{
    Scene *warm = scene("Warm");
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, 60, ControlMap::function(warm->id()), ControlMap::Toggle });
    QList<ControlOwners::Owner> owners = ControlOwners::owners(m_doc, 0, 60);
    QCOMPARE(owners.count(), 1);
    // A widget input whose widget is gone: it can't let go.
    ControlOwners::Owner gone;
    gone.kind = ControlOwners::VCInput;
    gone.thing = QStringLiteral("Button");
    QVERIFY(!ControlOwners::canRelease(m_doc, gone));
    QVERIFY(!ControlOwners::releaseAll(m_doc, owners + QList<ControlOwners::Owner>{ gone }));
    QVERIFY(cm->binding(ControlMap::function(warm->id())) != nullptr);

    // One fails half-way (the same trigger twice: gone by the second): the
    // speed master released before it gets its control back, and the trigger.
    SpeedMasters *sm = m_doc->speedMasters();
    sm->setInput(0, SpeedMasters::TapControl, 0, 60);
    owners = ControlOwners::owners(m_doc, 0, 60);
    QCOMPARE(owners.count(), 2);
    ControlOwners::Owner trigger;
    for (const ControlOwners::Owner &o : std::as_const(owners))
        if (o.kind == ControlOwners::Trigger)
            trigger = o;
    QVERIFY(!ControlOwners::releaseAll(m_doc, owners + QList<ControlOwners::Owner>{ trigger }));
    QVERIFY(cm->binding(ControlMap::function(warm->id())) != nullptr);
    QCOMPARE(cm->binding(ControlMap::function(warm->id()))->channel, 60u);
    quint32 u = 99, ch = 99;
    QVERIFY(sm->input(0, SpeedMasters::TapControl, &u, &ch));
    QCOMPARE(u, 0u);
    QCOMPARE(ch, 60u);
    // All can: all released.
    QVERIFY(ControlOwners::releaseAll(m_doc, owners));
    QVERIFY(cm->binding(ControlMap::function(warm->id())) == nullptr);
    QVERIFY(!sm->input(0, SpeedMasters::TapControl, &u, &ch));
}

/* The encoder bank's preset, Custom turned back on and another universe
   hand it a whole run of controls: asked as a Learn asks, refused with the
   same box (it used to take them without asking). */
void ControlMapUi_Test::anEncoderPresetAsksWhoHasItsControls()
{
    Scene *warm = scene("Warm");
    ControlMap *cm = m_doc->controlMap();
    EncoderBank *enc = cm->encoderBank();
    const QString key = EncoderBank::presetKeys().first();
    EncoderBank::Device d;
    d.universe = 0;
    QVERIFY(EncoderBank::preset(key, &d));
    QVERIFY(d.encoders >= 0 && d.encoderCount >= 3);
    const quint32 taken = quint32(d.encoders) + 2;
    cm->setBinding({ 0, taken, ControlMap::function(warm->id()), ControlMap::Toggle });

    ControlMapPanel panel(m_doc);
    panel.show();
    QComboBox *preset = panel.findChild<QComboBox *>("encoderPreset");
    const int at = preset->findData(key);
    QVERIFY(at >= 0);
    QVERIFY(QMetaObject::invokeMethod(&panel, "slotEncoderPreset", Q_ARG(int, at)));
    ControlCollisionBox *box = panel.encoderCollision();
    QVERIFY2(box->isRefusing() && box->text().contains("Scene: Warm (Control Map)"), qPrintable(box->text()));
    QVERIFY(!enc->isEnabled());
    QCOMPARE(enc->device().encoders, -1);
    // Cancel: nothing changes.
    box->cancelButton()->click();
    QVERIFY(!enc->isEnabled());
    QVERIFY(cm->binding(ControlMap::function(warm->id())) != nullptr);
    QCOMPARE(preset->currentData().toString(), QString());
    // Again, Move It Here: the trigger lets go, the preset is set.
    QVERIFY(QMetaObject::invokeMethod(&panel, "slotEncoderPreset", Q_ARG(int, at)));
    QVERIFY(box->isRefusing());
    box->moveButton()->click();
    QVERIFY(cm->binding(ControlMap::function(warm->id())) == nullptr);
    QVERIFY(enc->isEnabled());
    QCOMPARE(enc->presetKey(), key);

    // Off, a trigger learned on one of its controls, then Custom on again: refused.
    enc->setEnabled(false);
    cm->setBinding({ 0, taken, ControlMap::function(warm->id()), ControlMap::Toggle });
    const int custom = preset->findData(QStringLiteral("custom"));
    QVERIFY(custom >= 0);
    QVERIFY(QMetaObject::invokeMethod(&panel, "slotEncoderPreset", Q_ARG(int, custom)));
    QVERIFY(box->isRefusing());
    QVERIFY(!enc->isEnabled());
    box->cancelButton()->click();
    QVERIFY(!enc->isEnabled());
    QVERIFY(cm->binding(ControlMap::function(warm->id())) != nullptr);

    // On, another universe where a trigger has one of its controls: refused.
    cm->removeBinding(ControlMap::function(warm->id()));
    enc->setEnabled(true);
    QVERIFY(enc->isEnabled());
    cm->setBinding({ 1, taken, ControlMap::function(warm->id()), ControlMap::Toggle });
    panel.refresh();
    QComboBox *universe = panel.findChild<QComboBox *>("encoderUniverse");
    QVERIFY(universe->count() > 1);
    universe->setCurrentIndex(1);
    QVERIFY(QMetaObject::invokeMethod(&panel, "slotEncoderEdited"));
    QVERIFY(box->isRefusing());
    QCOMPARE(enc->device().universe, 0u);
    box->cancelButton()->click();
    QCOMPARE(enc->device().universe, 0u);
    QCOMPARE(universe->currentIndex(), 0);
    QVERIFY(cm->binding(ControlMap::function(warm->id())) != nullptr);
}

/* A run refused for two owners on two of its controls names each with its
   own control (it used to put both under the first taken control). */
void ControlMapUi_Test::anEncoderRefusalNamesEachOwnersControl()
{
    Scene *warm = scene("Warm");
    Scene *cool = scene("Cool");
    ControlMap *cm = m_doc->controlMap();
    EncoderBank *enc = cm->encoderBank();
    EncoderBank::Device dev;
    dev.universe = 0;
    dev.encoders = 16;
    dev.encoderCount = 8;
    enc->setDevice(dev, QStringLiteral("custom"));
    cm->setBinding({ 0, 41, ControlMap::function(warm->id()), ControlMap::Toggle });
    cm->setBinding({ 0, 44, ControlMap::function(cool->id()), ControlMap::Toggle });

    ControlMapPanel panel(m_doc);
    panel.show();
    QAction *pushes = panel.findChild<QAction *>("encoderLearnPushes");
    QVERIFY(pushes != nullptr);
    QVERIFY(QMetaObject::invokeMethod(&panel, "slotEncoderLearnMore", Q_ARG(QAction *, pushes)));
    cm->handleInput(0, 40, 127);
    ControlCollisionBox *box = panel.encoderCollision();
    QVERIFY(box->isRefusing());
    const QString text = box->text();
    const QString c41 = ControlOwners::controlName(m_doc, 0, 41);
    const QString c44 = ControlOwners::controlName(m_doc, 0, 44);
    QVERIFY2(text.contains(QString("%1 already does Scene: Warm (Control Map)").arg(c41)), qPrintable(text));
    QVERIFY2(text.contains(QString("%1 already does Scene: Cool (Control Map)").arg(c44)), qPrintable(text));
    QVERIFY2(!text.contains(QString("%1 already does Scene: Warm (Control Map) and").arg(c41)), qPrintable(text));
    cm->handleInput(0, 40, 0);
}

/* One picker for many presets (Speed Dial, Matrix, XY Pad): Move It Here
   for preset A, then preset B selected (the picker shows B's source), then
   OK -- the control preset A keeps is still taken from its trigger. Two
   moves for two presets take both controls; a move whose preset then
   learned another control takes nothing. */
void ControlMapUi_Test::aMoveForEachPresetIsTakenOnOk()
{
    Scene *a = scene("Warm");
    Scene *b = scene("Cool");
    Scene *c = scene("Blue");
    ControlMap *cm = m_doc->controlMap();
    InputOutputMap *iom = m_doc->inputOutputMap();
    new VirtualConsole(nullptr, m_doc);
    auto vcGone = qScopeGuard([]() { delete VirtualConsole::instance(); });
    const ControlMap::Target warm = ControlMap::function(a->id());
    const ControlMap::Target cool = ControlMap::function(b->id());
    const ControlMap::Target blue = ControlMap::function(c->id());
    cm->setBinding({ 0, 60, warm, ControlMap::Toggle });
    cm->setBinding({ 0, 61, cool, ControlMap::Toggle });
    cm->setBinding({ 0, 62, blue, ControlMap::Toggle });

    QDialog dlg;
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    InputSelectionWidget *isw = new InputSelectionWidget(m_doc, &dlg);
    lay->addWidget(isw);
    // As the Properties windows keep a preset's source: the picker's own.
    QSharedPointer<QLCInputSource> presetA;
    QSharedPointer<QLCInputSource> presetB(new QLCInputSource(0, 80));
    QSharedPointer<QLCInputSource> presetC(new QLCInputSource(0, 81));
    QSharedPointer<QLCInputSource> *editing = &presetA;
    connect(isw, &InputSelectionWidget::inputValueChanged, &dlg, [&]() { *editing = isw->inputSource(); });
    auto hear = [&](quint32 ch) {
        isw->m_autoDetectInputButton->setChecked(true);
        emit iom->inputValueChanged(0, ch, 127);
        isw->m_autoDetectInputButton->setChecked(false);
    };

    hear(60);                                   // preset A: Warm's control
    QVERIFY(isw->collisionBox()->isRefusing());
    isw->collisionBox()->moveButton()->click();
    QCOMPARE(presetA->channel(), 60u);

    editing = &presetB;                         // preset B selected
    isw->setInputSource(presetB);
    hear(61);                                   // Cool's control
    QVERIFY(isw->collisionBox()->isRefusing());
    isw->collisionBox()->moveButton()->click();
    QCOMPARE(presetB->channel(), 61u);

    editing = &presetC;                         // preset C: moved, then changed its mind
    isw->setInputSource(presetC);
    hear(62);
    QVERIFY(isw->collisionBox()->isRefusing());
    isw->collisionBox()->moveButton()->click();
    hear(63);                                   // a free control
    QCOMPARE(presetC->channel(), 63u);

    isw->setInputSource(presetB);               // another preset showing at OK
    editing = &presetB;
    QVERIFY(cm->binding(warm) != nullptr && cm->binding(cool) != nullptr);
    dlg.accept();
    QVERIFY2(cm->binding(warm) == nullptr, "preset A's control still fires Warm too");
    QVERIFY2(cm->binding(cool) == nullptr, "preset B's control still fires Cool too");
    QVERIFY2(cm->binding(blue) != nullptr && cm->binding(blue)->channel == 62u,
             "Blue lost its control though preset C let it go");
}

/* An encoder-bank Learn heard on another universe moves the whole bank
   there: its other controls (pushes, Next bank...) are asked there too,
   as a universe chosen in the box is, not only the run just heard. */
void ControlMapUi_Test::anEncoderLearnOnAnotherUniverseAsksForTheWholeBank()
{
    Scene *a = scene("Warm");
    ControlMap *cm = m_doc->controlMap();
    EncoderBank *enc = cm->encoderBank();
    EncoderBank::Device dev;
    dev.universe = 0;
    dev.encoders = 0;
    dev.encoderCount = 8;
    dev.pushes = 32;
    dev.bankNext = 50;
    enc->setDevice(dev, QStringLiteral("custom"));
    QVERIFY(enc->isEnabled());
    const ControlMap::Target warm = ControlMap::function(a->id());
    cm->setBinding({ 1, 50, warm, ControlMap::Toggle });   // the second controller

    ControlMapPanel panel(m_doc);
    panel.show();
    ControlCollisionBox *box = panel.encoderCollision();

    // Encoders turned on universe 2: CC 0-7 are free there, its CC 50 is not.
    QVERIFY(QMetaObject::invokeMethod(&panel, "slotEncoderLearn"));
    QVERIFY(cm->isListening());
    cm->handleInput(1, 0, 127);
    QVERIFY2(box->isRefusing() && box->text().contains("Scene: Warm (Control Map)"), qPrintable(box->text()));
    QCOMPARE(enc->device().universe, 0u);          // not moved
    QCOMPARE(enc->device().bankNext, 50);
    cm->handleInput(1, 0, 0);
    box->moveButton()->click();
    QVERIFY(cm->binding(warm) == nullptr);
    QCOMPARE(enc->device().universe, 1u);
    QCOMPARE(enc->device().encoders, 0);

    // Next bank learned back on universe 1: the encoders there are a trigger's.
    cm->setBinding({ 0, 3, warm, ControlMap::Toggle });
    QAction *next = panel.findChild<QAction *>("encoderLearnNext");
    QVERIFY(next != nullptr);
    QVERIFY(QMetaObject::invokeMethod(&panel, "slotEncoderLearnMore", Q_ARG(QAction *, next)));
    cm->handleInput(0, 90, 127);
    QVERIFY2(box->isRefusing() && box->text().contains("Scene: Warm (Control Map)"), qPrintable(box->text()));
    QCOMPARE(enc->device().universe, 1u);
    QCOMPARE(enc->device().bankNext, 50);
    cm->handleInput(0, 90, 0);
}
