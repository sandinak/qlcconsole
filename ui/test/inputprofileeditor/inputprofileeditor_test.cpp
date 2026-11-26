/*
  Q Light Controller Plus - Unit test
  inputprofileeditor_test.cpp

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
#include <QTemporaryDir>
#include <QMessageBox>
#include <QScopedPointer>
#include <QTreeWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QDir>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "inputprofileeditor.h"
#undef protected
#undef private
#include "inputprofileeditor_test.h"
#include "qlcinputprofile.h"
#include "qlcinputchannel.h"
#include "inputoutputmap.h"
#include "doc.h"

namespace
{
QTemporaryDir *tempDir()
{
    static QTemporaryDir dir;
    return &dir;
}

QTreeWidgetItem *itemFor(InputProfileEditor &e, quint32 channel)
{
    for (int i = 0; i < e.m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *it = e.m_tree->topLevelItem(i);
        if (it->text(0).toUInt() == channel + 1)
            return it;
    }
    return nullptr;
}

/* A person clicks a control in the list */
void pick(InputProfileEditor &e, quint32 channel)
{
    QTreeWidgetItem *it = itemFor(e, channel);
    QVERIFY(it != nullptr);
    e.m_tree->clearSelection();
    e.m_tree->setCurrentItem(it);
    it->setSelected(true);
    e.slotItemClicked(it, 0);
}

/* ...and picks @p data in @p combo, as a click does (activated) */
void choose(QComboBox *combo, const QVariant &data)
{
    const int i = combo->findData(data);
    QVERIFY(i >= 0);
    combo->setCurrentIndex(i);
    emit combo->activated(i);
}
}

void InputProfileEditor_Test::init()
{
    m_doc = new Doc(this);
    // A profile on disk (the editor warns about a file it can't write)
    QLCInputProfile prof;
    prof.setManufacturer(QStringLiteral("S2"));
    prof.setModel(QStringLiteral("Fader box"));
    prof.setSurfaceVersion(1);
    QLCInputChannel *f = new QLCInputChannel();
    f->setType(QLCInputChannel::Slider);
    f->setName(QStringLiteral("Fader 9"));
    prof.insertChannel(8, f);
    QLCInputChannel *b = new QLCInputChannel();
    b->setType(QLCInputChannel::Button);
    b->setName(QStringLiteral("Key 1"));
    prof.insertChannel(20, b);
    m_path = tempDir()->filePath(QStringLiteral("box.qxi"));
    QVERIFY(prof.saveXML(m_path));
}

void InputProfileEditor_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void InputProfileEditor_Test::aSurfaceTypeShowsRoleAndFaderRows()
{
    QScopedPointer<QLCInputProfile> prof(QLCInputProfile::loader(m_path));
    InputProfileEditor e(nullptr, prof.data(), m_doc->inputOutputMap());
    e.show();
    QVERIFY(e.m_surfaceCheck->isChecked());
    e.m_tab->setCurrentIndex(1);
    pick(e, 8);
    QVERIFY(e.m_roleCombo->isVisibleTo(&e));
    QVERIFY(e.m_faderFactCombo->isVisibleTo(&e));        // a fader says what it is
    pick(e, 20);
    QVERIFY(e.m_roleCombo->isVisibleTo(&e));
    QVERIFY(!e.m_faderFactCombo->isVisibleTo(&e));       // a key has no fader fact
    choose(e.m_roleCombo, QStringLiteral("Select"));
    QVERIFY(e.m_roleIndexSpin->isVisibleTo(&e));         // "Select 3": its number
    // Not a surface type: no role rows
    e.m_surfaceCheck->setChecked(false);
    QVERIFY(!e.m_roleCombo->isVisibleTo(&e));
    QVERIFY(!e.m_roleIndexSpin->isVisibleTo(&e));
}

void InputProfileEditor_Test::aRoleAndAFactAreSavedWithTheProfile()
{
    QScopedPointer<QLCInputProfile> prof(QLCInputProfile::loader(m_path));
    InputProfileEditor e(nullptr, prof.data(), m_doc->inputOutputMap());
    e.show();
    e.m_tab->setCurrentIndex(1);
    pick(e, 8);
    choose(e.m_roleCombo, QStringLiteral("GrandMaster"));
    choose(e.m_faderFactCombo, int(FaderFact::Motorised));
    pick(e, 20);
    choose(e.m_roleCombo, QStringLiteral("Page"));
    e.m_roleIndexSpin->setValue(3);
    e.m_surfaceColumnsSpin->setValue(9);
    e.accept();
    QCOMPARE(e.result(), int(QDialog::Accepted));

    // What the caller saves
    QLCInputProfile *edited = e.profile();
    QVERIFY(edited->saveXML(m_path));
    QScopedPointer<QLCInputProfile> back(QLCInputProfile::loader(m_path));
    QVERIFY(back->isSurfaceType());
    QCOMPARE(back->surfaceColumns(), 9);
    QCOMPARE(back->surfaceLayout(), QStringLiteral("grid"));
    QCOMPARE(back->channel(8)->role(), QStringLiteral("GrandMaster"));
    QCOMPARE(back->channel(8)->faderFact(), FaderFact::Motorised);
    QCOMPARE(back->channel(20)->role(), QStringLiteral("Page 3"));

    // The rows show what the control has when it is clicked again
    pick(e, 8);
    QCOMPARE(e.m_roleCombo->currentData().toString(), QStringLiteral("GrandMaster"));
    QCOMPARE(e.m_faderFactCombo->currentData().toInt(), int(FaderFact::Motorised));
}

/* Two Grand Masters, or a stop on a fader: said, and the editor stays open. */
void InputProfileEditor_Test::aBrokenRuleIsSaidAndNothingCloses()
{
    QScopedPointer<QLCInputProfile> prof(QLCInputProfile::loader(m_path));
    InputProfileEditor e(nullptr, prof.data(), m_doc->inputOutputMap());
    e.show();
    e.m_tab->setCurrentIndex(1);
    pick(e, 8);
    choose(e.m_roleCombo, QStringLiteral("EmergencyStop"));
    QString said;
    QTimer::singleShot(0, [&said]() {
        if (QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
        {
            said = box->text();
            box->accept();
        }
    });
    e.accept();
    QVERIFY2(said.contains(QStringLiteral("the stop must be a button")), qPrintable(said));
    QVERIFY(e.isVisible());
    QCOMPARE(e.result(), int(QDialog::Rejected));
}

void InputProfileEditor_Test::shots()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    const QString prefix = qEnvironmentVariable("QLC_SHOT_PREFIX", QStringLiteral("after"));
    QScopedPointer<QLCInputProfile> prof(QLCInputProfile::loader(m_path));
    InputProfileEditor e(nullptr, prof.data(), m_doc->inputOutputMap());
    const QSize sizes[] = { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) };
    for (const QSize &sz : sizes)
    {
        // The dialog at its own size on that screen: at most the screen's
        e.resize(qMin(sz.width(), 760), qMin(sz.height(), 560));
        e.show();
        e.m_tab->setCurrentIndex(0);
        QCoreApplication::processEvents();
        QVERIFY(e.grab().save(QStringLiteral("%1/%2-inputprofileeditor-general-%3x%4.png")
                                  .arg(dir, prefix).arg(sz.width()).arg(sz.height())));
        e.m_tab->setCurrentIndex(1);
        pick(e, 8);
        choose(e.m_roleCombo, QStringLiteral("GrandMaster"));
        choose(e.m_faderFactCombo, int(FaderFact::Absolute));
        QCoreApplication::processEvents();
        QVERIFY(e.grab().save(QStringLiteral("%1/%2-inputprofileeditor-mapping-%3x%4.png")
                                  .arg(dir, prefix).arg(sz.width()).arg(sz.height())));
    }
}

QTEST_MAIN(InputProfileEditor_Test)
