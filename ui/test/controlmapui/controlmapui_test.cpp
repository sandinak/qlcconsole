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
#include <QApplication>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTimer>
#include <QMenu>
#include <QtTest>

#include "controlmapui_test.h"
#include "showcontrolsyncpage.h"
#include "controllearndialog.h"
#include "programmingoutline.h"
#include "showtreecontroller.h"
#include "controlmappanel.h"
#include "showtreeview.h"
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

void ControlMapUi_Test::theMenuActionSaysWhatIsBound()
{
    Scene *a = scene("Warm");
    QMenu menu;
    QAction *learn = ControlLearnDialog::addTo(&menu, m_doc, ControlMap::function(a->id()), nullptr);
    QVERIFY(learn != nullptr);
    QCOMPARE(learn->text(), QString("Learn trigger…"));
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

QTEST_MAIN(ControlMapUi_Test)
