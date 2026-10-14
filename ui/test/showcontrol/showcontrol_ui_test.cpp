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
#include <QCheckBox>
#include <QLineEdit>
#include <QSettings>
#include <QtTest>

#define private public
#include "chasereditor.h"
#include "mastertimer.h"
#undef private
#include "showcontrollistener.h"
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
const char *const kKeys[] = { "showcontrol/oscEnabled", "showcontrol/oscPort",
                              "showcontrol/mscEnabled", "showcontrol/mscDeviceId" };

QByteArray osc(const QByteArray &address)
{
    QByteArray b = address;
    do b.append('\0'); while (b.size() % 4);
    b.append(',');
    do b.append('\0'); while (b.size() % 4);
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
    // The test's own settings, put back after.
    QSettings s;
    for (const char *k : kKeys)
        m_saved << s.value(QLatin1String(k));
}

void ShowControlUi_Test::cleanupTestCase()
{
    QSettings s;
    for (int i = 0; i < m_saved.count(); ++i)
    {
        if (m_saved.at(i).isValid())
            s.setValue(QLatin1String(kKeys[i]), m_saved.at(i));
        else
            s.remove(QLatin1String(kKeys[i]));
    }
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

QTEST_MAIN(ShowControlUi_Test)
