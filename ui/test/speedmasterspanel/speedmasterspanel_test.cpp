/*
  Q Light Controller Plus - Test Unit
  speedmasterspanel_test.cpp

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
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTreeWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QSlider>
#include <QToolButton>
#include <QApplication>
#include <QDialog>

#define private public
#include "chasereditor.h"
#undef private
#include "speedmasterspanel.h"
#include "speedmastersfooter.h"
#include "inputselectionwidget.h"
#include "qlcinputsource.h"
#include "inputoutputmap.h"
#include "collectioneditor.h"
#include "speedmasters.h"
#include "collection.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

#include "speedmasterspanel_test.h"

void SpeedMastersPanel_Test::init()
{
    m_doc = new Doc(this);
}

void SpeedMastersPanel_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void SpeedMastersPanel_Test::theRowsShowTheMasters()
{
    SpeedMasters *sm = m_doc->speedMasters();
    sm->setName(2, "Strobes");
    sm->setBpm(2, 140);
    SpeedMastersPanel panel(m_doc);
    QCOMPARE(panel.row(2).name->text(), QString("Strobes"));
    QCOMPARE(panel.row(2).bpm->value(), 140.0);
    QVERIFY(panel.row(0).name->text().isEmpty());          // the default isn't a name
    QVERIFY(panel.row(0).tap->isEnabled());
    QVERIFY(!panel.row(0).ratio->isEnabled());
    // Master 1's list of things to follow names master 3 by its name.
    QVERIFY(panel.row(0).source->findText("Strobes") >= 0);

    panel.row(1).name->setText("Movers");
    emit panel.row(1).name->editingFinished();
    QCOMPARE(sm->name(1), QString("Movers"));
    QCOMPARE(sm->assignmentName(SpeedMasters::Master1 + 1), QString("Movers"));
}

void SpeedMastersPanel_Test::aRowFollowsAndGoesBackToItsOwn()
{
    SpeedMasters *sm = m_doc->speedMasters();
    SpeedMastersPanel panel(m_doc);
    const SpeedMastersPanel::Row &r = panel.row(1);
    r.ratio->setCurrentIndex(r.ratio->findData(2.0));
    r.source->setCurrentIndex(r.source->findData(int(SpeedMasters::MainClock)));
    QCOMPARE(sm->mode(1), SpeedMasters::Follow);
    QCOMPARE(sm->follows(1), int(SpeedMasters::MainClock));
    QCOMPARE(sm->ratio(1), 2.0);
    QVERIFY(!r.tap->isEnabled());
    QVERIFY(!r.bpm->isEnabled());
    QVERIFY(r.ratio->isEnabled());

    r.source->setCurrentIndex(r.source->findData(-1));
    QCOMPARE(sm->mode(1), SpeedMasters::Own);
    QVERIFY(r.tap->isEnabled());
}

void SpeedMastersPanel_Test::aLoopIsRefusedAndTheRowPutBack()
{
    SpeedMasters *sm = m_doc->speedMasters();
    QVERIFY(sm->setFollow(1, 0, 1.0));
    SpeedMastersPanel panel(m_doc);
    const SpeedMastersPanel::Row &r = panel.row(0);
    r.source->setCurrentIndex(r.source->findData(1));     // 1 → 2 → 1
    QCOMPARE(sm->mode(0), SpeedMasters::Own);
    QCOMPARE(r.source->currentData().toInt(), -1);
}

void SpeedMastersPanel_Test::tapRateAndPauseReachTheMaster()
{
    SpeedMasters *sm = m_doc->speedMasters();
    SpeedMastersPanel panel(m_doc);
    const SpeedMastersPanel::Row &r = panel.row(3);
    r.rate->setValue(150);
    QCOMPARE(sm->rate(3), 1.5);
    QCOMPARE(sm->bpm(3), 180.0);
    r.pause->setChecked(true);
    QVERIFY(sm->isPaused(3));
    r.pause->setChecked(false);
    r.bpm->setValue(90);
    QCOMPARE(sm->setBpm(3), 90.0);
    r.tap->click();
    QTest::qWait(500);
    r.tap->click();
    QVERIFY2(qAbs(sm->bpm(3) - 120.0) < 15, qPrintable(QString::number(sm->bpm(3))));
    QTRY_VERIFY(qAbs(r.bpm->value() * 1.5 - sm->bpm(3)) < 1.0);   // the panel shows it
}

void SpeedMastersPanel_Test::aChaseAndItsStepsAreGivenAMaster()
{
    Scene *a = new Scene(m_doc);
    a->setName("A");
    m_doc->addFunction(a);
    Chaser *chase = new Chaser(m_doc);
    chase->addStep(ChaserStep(a->id()));
    chase->addStep(ChaserStep(a->id()));
    m_doc->addFunction(chase);

    ChaserEditor ed(nullptr, chase, m_doc);
    QComboBox *chaseSpeed = ed.findChild<QComboBox *>("chaserSpeedMaster");
    QVERIFY(chaseSpeed != nullptr);
    chaseSpeed->setCurrentIndex(chaseSpeed->findData(SpeedMasters::Master1));
    QCOMPARE(chase->speedMaster(), int(SpeedMasters::Master1));

    QTreeWidget *tree = ed.m_tree;
    QVERIFY(tree->columnCount() >= 8);
    QTreeWidgetItem *second = tree->topLevelItem(1);
    QVERIFY(second->text(7).isEmpty());                    // as the chase
    // The Speed column's choice, as its drop-down sets it.
    const int idx = SpeedMastersPanel::choiceValues(m_doc).indexOf(SpeedMasters::MainClock);
    second->setData(7, Qt::UserRole, idx);
    second->setData(7, Qt::DisplayRole, QString("Main clock"));
    QCOMPARE(chase->steps().at(1).speedMaster, int(SpeedMasters::MainClock));
    QCOMPARE(chase->steps().at(0).speedMaster, int(SpeedMasters::Inherit));
    QCOMPARE(second->text(7), QString("Main clock"));

    // When a GO lands.
    QComboBox *goLands = ed.findChild<QComboBox *>("chaserGoQuantize");
    QVERIFY(goLands != nullptr);
    QCOMPARE(goLands->currentData().toInt(), int(Chaser::GoNow));
    goLands->setCurrentIndex(goLands->findData(int(Chaser::GoOnBar)));
    QCOMPARE(chase->goQuantize(), Chaser::GoOnBar);
}

void SpeedMastersPanel_Test::aCollectionIsGivenAMaster()
{
    Collection *col = new Collection(m_doc);
    m_doc->addFunction(col);
    CollectionEditor ed(nullptr, col, m_doc);
    QComboBox *speed = ed.findChild<QComboBox *>("collectionSpeedMaster");
    QVERIFY(speed != nullptr);
    QCOMPARE(speed->currentData().toInt(), int(SpeedMasters::Inherit));
    speed->setCurrentIndex(speed->findData(SpeedMasters::Master1 + 3));
    QCOMPARE(col->speedMaster(), int(SpeedMasters::Master1 + 3));
}

void SpeedMastersPanel_Test::theCountShowsTheMastersInUse()
{
    SpeedMasters *sm = m_doc->speedMasters();
    SpeedMastersPanel panel(m_doc);
    panel.show();
    QSpinBox *count = panel.findChild<QSpinBox *>("speedMasterCount");
    QVERIFY(count != nullptr);
    QCOMPARE(count->value(), 4);
    QVERIFY(panel.row(3).name->isVisible());
    QVERIFY(!panel.row(4).name->isVisible());

    count->setValue(8);
    QCOMPARE(sm->count(), 8);
    QVERIFY(panel.row(7).name->isVisible());
    QVERIFY(!panel.row(8).name->isVisible());
    // Any of the eight can follow another of them, the main clock or the show.
    QVERIFY(panel.row(0).source->findData(7) >= 0);
    QVERIFY(panel.row(0).source->findData(8) < 0);
    QVERIFY(panel.row(0).source->findData(int(SpeedMasters::ShowTimeline)) >= 0);
    // ...and everywhere a master is chosen offers the eight.
    QCOMPARE(SpeedMastersPanel::choiceValues(m_doc).size(), 3 + 8);

    // A typed tempo applies as it's typed.
    QDoubleSpinBox *bpm = panel.row(0).bpm;
    bpm->selectAll();
    QTest::keyClicks(bpm, "95");
    QCOMPARE(sm->setBpm(0), 95.0);

    // Back down to six: the rows go, the show remembers.
    count->setValue(6);
    QCOMPARE(sm->count(), 6);
    QVERIFY(!panel.row(6).name->isVisible());
}

/* "put the masters in the footer as a set of buttons that show the bpm ..
   and clickable opens the window" */
void SpeedMastersPanel_Test::theFooterShowsEachMastersTempo()
{
    SpeedMasters *sm = m_doc->speedMasters();
    sm->setName(0, "Pars");
    sm->setBpm(0, 128);
    SpeedMastersFooter footer(m_doc);
    footer.show();
    QTRY_COMPARE(footer.button(0)->text(), QString("Pars 128"));
    QVERIFY(footer.button(3)->isVisible());
    QVERIFY(!footer.button(4)->isVisible());          // four in use
    sm->setCount(6);
    QVERIFY(footer.button(5)->isVisible());
    sm->setPaused(1, true);
    QTRY_COMPARE(footer.button(1)->text(), QString("Speed 2 paused"));
    QCOMPARE(SpeedMastersFooter::buttonText("A very long name", 90, false),
             QString("A very lo%1 90").arg(QChar(0x2026)));

    footer.button(2)->click();                       // opens the window
    SpeedMastersPanel *panel = nullptr;
    foreach (QWidget *w, QApplication::topLevelWidgets())
        if (qobject_cast<SpeedMastersPanel *>(w) != nullptr && w->isVisible())
            panel = qobject_cast<SpeedMastersPanel *>(w);
    QVERIFY(panel != nullptr);
    panel->close();
}

/* "in the windows have mapping to midi targets" */
void SpeedMastersPanel_Test::aMastersInputsAreMappedInItsWindow()
{
    SpeedMasters *sm = m_doc->speedMasters();
    SpeedMastersPanel panel(m_doc);
    QCOMPARE(panel.row(0).inputs->text(), SpeedMastersPanel::inputsText(0));

    bool sawDialog = false;
    QTimer::singleShot(0, this, [&sawDialog]() {
        QDialog *dlg = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dlg != nullptr);
        sawDialog = dlg->objectName() == QLatin1String("speedMasterInputsDialog");
        InputSelectionWidget *rate = dlg->findChild<InputSelectionWidget *>("speedMasterInputRate");
        InputSelectionWidget *pause = dlg->findChild<InputSelectionWidget *>("speedMasterInputPause");
        QVERIFY(rate != nullptr && pause != nullptr);
        rate->setInputSource(QSharedPointer<QLCInputSource>(new QLCInputSource(0, 7)));
        pause->setInputSource(QSharedPointer<QLCInputSource>(new QLCInputSource(0, 8)));
        dlg->accept();
    });
    panel.row(0).inputs->click();
    QVERIFY(sawDialog);
    quint32 u = 99, c = 99;
    QVERIFY(sm->input(0, SpeedMasters::RateControl, &u, &c));
    QCOMPARE(u, quint32(0));
    QCOMPARE(c, quint32(7));
    QVERIFY(!sm->input(0, SpeedMasters::TapControl, nullptr, nullptr));
    QCOMPARE(panel.row(0).inputs->text(), SpeedMastersPanel::inputsText(2));

    // And the controller drives it.
    emit m_doc->inputOutputMap()->inputValueChanged(0, 7, 255);
    QCOMPARE(sm->rate(0), 2.0);
    QCOMPARE(panel.row(0).rate->value(), 200);
    emit m_doc->inputOutputMap()->inputValueChanged(0, 8, 127);
    QVERIFY(sm->isPaused(0));

    // Cancel leaves them as they were.
    QTimer::singleShot(0, this, []() {
        QDialog *dlg = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        InputSelectionWidget *rate = dlg->findChild<InputSelectionWidget *>("speedMasterInputRate");
        rate->setInputSource(QSharedPointer<QLCInputSource>(new QLCInputSource(0, 99)));
        dlg->reject();
    });
    panel.row(0).inputs->click();
    QVERIFY(sm->input(0, SpeedMasters::RateControl, &u, &c));
    QCOMPARE(c, quint32(7));
}

QTEST_MAIN(SpeedMastersPanel_Test)
