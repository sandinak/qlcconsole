/*
  Q Light Controller Plus - Unit test
  onejob_test.cpp

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
#include <QScopeGuard>
#include <QSettings>
#include <QTreeWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QDialog>
#include <QTimer>
#include <QDir>
#include <QtTest>

#include "onejob_test.h"
#include "simmididevice.h"
#include "inputselectionwidget.h"
#include "controlcollisionbox.h"
#include "selectinputchannel.h"
#include "speedmasterspanel.h"
#include "controlowners.h"
#include "surfacepages.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "qlcfile.h"
#include "speedmasters.h"
#include "deskrecord.h"
#include "controlmap.h"
#include "pageset.h"
#include "doc.h"

namespace
{
const QString kApc = QStringLiteral("Akai APC40 mkII");
const quint32 kMasterFader = 14;        // the APC's seeded Grand Master
const quint32 kKnob1 = 16;              // Pan on the Move page
const quint32 kFreeFader = 128 + 34;   // a clip pad nothing uses

void settle()
{
    for (int i = 0; i < 3; ++i)
    {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    }
}
}

void OneJob_Test::initTestCase()
{
    QVERIFY(m_settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
}

void OneJob_Test::init()
{
    QSettings().remove(DeskRecord::settingsGroup());
    QSettings().remove(SurfacePages::settingsGroup());
    m_userSets = new QTemporaryDir();
    PageSet::setUserDirectory(m_userSets->path());
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
    plug();
}

void OneJob_Test::cleanup()
{
    delete m_apc;
    m_apc = nullptr;
    m_stub->m_lineNames.clear();
    m_stub->m_linesAreHardware = false;
    m_stub->m_networkDmx = true;
    delete m_doc;
    m_doc = nullptr;
    m_stub = nullptr;
    PageSet::setUserDirectory(QString());
    delete m_userSets;
    m_userSets = nullptr;
    QSettings().remove(DeskRecord::settingsGroup());
    QSettings().remove(SurfacePages::settingsGroup());
}

void OneJob_Test::plug()
{
    m_apc = new SimMidiDevice(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc,
                              QStringLiteral(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("inputprofiles/Akai-APC40-mkII.qxi"));
    QVERIFY2(m_apc->plug(), qPrintable(m_apc->lastError()));
    settle();
    QVERIFY2(m_doc->deskRecord()->isDeskControl(0, kMasterFader), "the APC's Grand Master was not seeded");
}

void OneJob_Test::aControlSaysEveryJobItHas()
{
    // The desk's Grand Master.
    QList<ControlOwners::Job> jobs = ControlOwners::jobsFor(m_doc, 0, kMasterFader);
    QVERIFY(!jobs.isEmpty());
    QVERIFY2(ControlOwners::jobsText(jobs).contains(QStringLiteral("Grand Master")),
             qPrintable(ControlOwners::jobsText(jobs)));
    QCOMPARE(jobs.first().owner.kind, ControlOwners::Desk);

    // A speed master's Rate on it too: both are named.
    m_doc->speedMasters()->setInput(0, SpeedMasters::RateControl, 0, kMasterFader);
    const QString both = ControlOwners::jobsText(ControlOwners::jobsFor(m_doc, 0, kMasterFader));
    QVERIFY2(both.contains(QStringLiteral("Grand Master")) && both.contains(QStringLiteral("Rate")),
             qPrintable(both));
    m_doc->speedMasters()->clearInput(0, SpeedMasters::RateControl);

    // A surface page's control: its job, and the page.
    jobs = ControlOwners::jobsFor(m_doc, 0, kKnob1);
    bool page = false;
    for (const ControlOwners::Job &j : jobs)
        page = page || (j.isPage && j.name.contains(QStringLiteral("Pan")));
    QVERIFY2(page, qPrintable(ControlOwners::jobsText(jobs)));
    QVERIFY(!ControlOwners::pageJobsText(jobs).isEmpty());

    // Free.
    QVERIFY(ControlOwners::jobsFor(m_doc, 0, 200).isEmpty());
    QCOMPARE(ControlOwners::jobsText(QList<ControlOwners::Job>()), QStringLiteral("free"));
}

void OneJob_Test::aDeskRoleIsNamedAndMovedFromTheSpeedMastersDialog()
{
    SpeedMasters *sm = m_doc->speedMasters();
    DeskRecord *desk = m_doc->deskRecord();
    InputOutputMap *iom = m_doc->inputOutputMap();
    SpeedMastersPanel panel(m_doc);

    QString freeLabel, label, boxText, moved, changed;
    bool refused = false, moveShown = false;
    QTimer::singleShot(0, this, [&]() {
        QDialog *dlg = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dlg != nullptr);
        auto done = qScopeGuard([dlg]() { dlg->accept(); });      // a failed check never hangs it
        InputSelectionWidget *tap = dlg->findChild<InputSelectionWidget *>("speedMasterInputTap");
        QVERIFY(tap != nullptr);
        tap->m_autoDetectInputButton->setChecked(true);
        // A free control: its job line says so, before anything is accepted.
        emit iom->inputValueChanged(0, kFreeFader, 127);
        freeLabel = tap->findChild<QLabel *>("currentJob")->text();
        tap->stopAutoDetection();
        tap->setInputSource(QSharedPointer<QLCInputSource>());
        tap->m_autoDetectInputButton->setChecked(true);
        // The desk's Grand Master fader.
        emit iom->inputValueChanged(0, kMasterFader, 127);
        label = tap->findChild<QLabel *>("currentJob")->text();
        refused = tap->collisionBox()->isRefusing();
        boxText = tap->collisionBox()->text();
        moveShown = tap->collisionBox()->moveButton()->isVisibleTo(dlg);
        // The window at three sizes, when the pool asks for shots.
        const QString shotDir = qEnvironmentVariable("QLC_SHOT_DIR");
        if (!shotDir.isEmpty())
        {
            QDir().mkpath(shotDir);
            const QString prefix = qEnvironmentVariable("QLC_SHOT_PREFIX", QStringLiteral("after"));
            for (const QSize &sz : { QSize(640, 640), QSize(900, 800), QSize(1200, 900) })
            {
                dlg->resize(sz);
                settle();
                dlg->grab().save(QStringLiteral("%1/%2-onejob-speedmaster-inputs-%3x%4.png")
                                     .arg(shotDir, prefix).arg(sz.width()).arg(sz.height()));
            }
        }
        tap->collisionBox()->moveButton()->click();
        tap->stopAutoDetection();
        QVERIFY(!tap->inputSource().isNull());
        QCOMPARE(tap->inputSource()->channel(), kMasterFader);
        // Nothing is taken until OK; the line says so, and is read afresh.
        QVERIFY(desk->isSet(DeskRecord::GrandMaster));
        moved = tap->findChild<QLabel *>("currentJob")->text();
        // A job given to that control by someone else shows at the next move of it.
        sm->setInput(1, SpeedMasters::TapControl, 0, kMasterFader);
        emit iom->inputValueChanged(0, kMasterFader, 127);
        tap->m_autoDetectInputButton->setChecked(true);
        emit iom->inputValueChanged(0, kMasterFader, 127);
        tap->stopAutoDetection();
        changed = tap->findChild<QLabel *>("currentJob")->text();
        sm->clearInput(1, SpeedMasters::TapControl);
    });
    panel.row(0).inputs->click();
    QVERIFY2(freeLabel.contains(QStringLiteral("free")), qPrintable(freeLabel));
    QVERIFY2(label.contains(QStringLiteral("Grand Master")), qPrintable(label));
    QVERIFY(refused);
    QVERIFY2(boxText.contains(QStringLiteral("Grand Master")) && boxText.contains(QStringLiteral("One job per control")),
             qPrintable(boxText));
    QVERIFY(moveShown);
    QVERIFY2(moved.contains(QStringLiteral("taken from Grand Master when you press OK")), qPrintable(moved));
    QVERIFY2(changed.contains(QStringLiteral("Speed 2 Tap")) || changed.contains(QStringLiteral("Tap")), qPrintable(changed));
    // OK: the Tap is on the fader and the desk's Grand Master is cleared.
    quint32 u = 99, c = 99;
    QVERIFY(sm->input(0, SpeedMasters::TapControl, &u, &c));
    QCOMPARE(c, kMasterFader);
    QVERIFY2(!desk->isSet(DeskRecord::GrandMaster), "the desk still has the Grand Master");
}

void OneJob_Test::chooseListsEachControlsJob()
{
    SelectInputChannel sic(nullptr, m_doc->inputOutputMap(), m_doc);
    QString master, free;
    std::function<void(QTreeWidgetItem *)> walk = [&](QTreeWidgetItem *it) {
        for (int i = 0; i < it->childCount(); ++i)
        {
            QTreeWidgetItem *ch = it->child(i);
            if (ch->text(1) == QLatin1String("0") && ch->text(2) == QString::number(kMasterFader))
                master = ch->text(3);
            if (ch->text(1) == QLatin1String("0") && ch->text(2) == QString::number(kFreeFader))
                free = ch->text(3);
            walk(ch);
        }
    };
    for (int i = 0; i < sic.m_tree->topLevelItemCount(); ++i)
        walk(sic.m_tree->topLevelItem(i));
    QVERIFY2(master.contains(QStringLiteral("Grand Master")), qPrintable(master));
    QCOMPARE(free, QStringLiteral("free"));
    QVERIFY(!sic.m_tree->isColumnHidden(3));
    QCOMPARE(sic.m_tree->headerItem()->text(3), QStringLiteral("Does now"));
}

void OneJob_Test::movingADeskRoleCanBePutBack()
{
    DeskRecord *desk = m_doc->deskRecord();
    const DeskRecord::Binding before = desk->binding(DeskRecord::GrandMaster);
    const QList<ControlOwners::Owner> owners =
        ControlOwners::listenersOf(m_doc, ControlOwners::jobsFor(m_doc, 0, kMasterFader));
    QVERIFY(ControlOwners::canMove(owners));
    const std::function<void()> undo = ControlOwners::releaseAllUndoable(m_doc, owners);
    QVERIFY(undo);
    QVERIFY(!desk->isSet(DeskRecord::GrandMaster));
    QVERIFY(ControlOwners::jobsFor(m_doc, 0, kMasterFader).isEmpty()
            || !ControlOwners::jobsText(ControlOwners::jobsFor(m_doc, 0, kMasterFader)).contains("Grand Master"));
    undo();
    QVERIFY(desk->isSet(DeskRecord::GrandMaster));
    QVERIFY(desk->binding(DeskRecord::GrandMaster) == before);
    QVERIFY(desk->isDeskControl(0, kMasterFader));

    // The other places still refuse a desk role (Learn: never moved).
    const QList<ControlOwners::Owner> plain = ControlOwners::owners(m_doc, 0, kMasterFader);
    QVERIFY(!ControlOwners::canMove(plain));
}

void OneJob_Test::theDialogOpensOnItsSavedInputs()
{
    SpeedMasters *sm = m_doc->speedMasters();
    sm->setInput(0, SpeedMasters::TapControl, 0, 41);
    sm->setInput(0, SpeedMasters::RateControl, 0, 42);
    sm->setInput(0, SpeedMasters::PauseControl, 0, 43);
    SpeedMastersPanel panel(m_doc);
    QList<quint32> got;
    QStringList shown;
    QTimer::singleShot(0, this, [&]() {
        QDialog *dlg = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dlg != nullptr);
        auto done = qScopeGuard([dlg]() { dlg->accept(); });
        for (const char *n : { "speedMasterInputTap", "speedMasterInputRate", "speedMasterInputPause" })
        {
            InputSelectionWidget *w = dlg->findChild<InputSelectionWidget *>(n);
            QVERIFY(w != nullptr);
            got << (w->inputSource().isNull() ? 0xFFFFu : w->inputSource()->channel());
            shown << w->m_inputChannelEdit->text();
        }
    });
    panel.row(0).inputs->click();
    QCOMPARE(got, (QList<quint32>{ 41, 42, 43 }));
    for (const QString &s : std::as_const(shown))
        QVERIFY2(!s.isEmpty() && s != QLatin1String("None") && s != KInputNone, qPrintable(s));
    // Untouched, OK keeps them.
    quint32 c = 0;
    QVERIFY(sm->input(0, SpeedMasters::PauseControl, nullptr, &c));
    QCOMPARE(c, 43u);
}

QTEST_MAIN(OneJob_Test)
