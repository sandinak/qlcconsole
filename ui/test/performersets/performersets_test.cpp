/*
  Q Light Controller Plus - Test Unit
  performersets_test.cpp

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
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTimer>

#define protected public
#define private public
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "paletteeditdialog.h"
#undef private
#undef protected

#include "performersets_test.h"
#include "performerfill.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "qlcpalette.h"
#include "doc.h"

namespace
{

/* A fill's three rows: one set, a Performers layer, a costume of its own. */
QList<StageTarget *> choir(MonitorProperties *props, quint32 *group = nullptr)
{
    PerformerFill::Options o;
    o.height = 1.60f;
    o.spread = 0.05f;
    o.costume = QColor(Qt::blue);
    const quint32 layer = PerformerFill::ensureLayer(props);
    const quint32 g = props->nextGroupId();
    props->createGroup(g, QStringLiteral("Choir"), layer, 0);
    QList<StageTarget *> out;
    for (int i = 0; i < 3; ++i)
    {
        StageTarget *t = PerformerFill::addLine(props, QVector3D(0, 1.0f + i, 0),
                                                QVector3D(4, 1.0f + i, 0), 5, o, layer);
        t->setName(QStringLiteral("Choir row %1").arg(i + 1));
        t->setGroupId(g);
        out << t;
    }
    if (group != nullptr)
        *group = g;
    return out;
}

QStringList names(const QList<StageTarget *> &l)
{
    QStringList n;
    foreach (StageTarget *t, l)
        n << t->name();
    return n;
}

} // namespace

void PerformerSets_Test::theSetSharesItsLook()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    const QList<StageTarget *> rows = choir(props);
    QCOMPARE(PerformerFill::setMates(props, rows.at(0)).size(), 2);

    // A line on its own has no set.
    StageTarget *solo = PerformerFill::addLine(props, QVector3D(0, 6, 0), QVector3D(2, 6, 0), 3,
                                               PerformerFill::Options(), 0);
    QVERIFY(PerformerFill::setMates(props, solo).isEmpty());

    rows.at(1)->setSeed(12345);
    rows.at(1)->setPerformerCount(7);
    rows.at(0)->setCostumeColor(QColor(Qt::red));
    rows.at(0)->setPersonHeight(1.20f);
    rows.at(0)->setHeightSpread(0.10f);
    QCOMPARE(PerformerFill::copyLookToSet(props, rows.at(0)), 2);
    foreach (StageTarget *t, rows)
    {
        QCOMPARE(t->costumeColor(), QColor(Qt::red));
        QCOMPARE(t->effectivePersonHeight(), 1.20f);
        QCOMPARE(t->heightSpread(), 0.10f);
    }
    // Their own people stay their own.
    QCOMPARE(rows.at(1)->seed(), quint32(12345));
    QCOMPARE(rows.at(1)->performerCount(), 7);
    QVERIFY(rows.at(0)->seed() != rows.at(2)->seed());
    // The lone line was not in the set.
    QCOMPARE(solo->costumeColor(), QColor());
}

void PerformerSets_Test::oneDialogChangesTheWholeSet()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    QWidget parent;
    Monitor mon(&parent, &doc);
    const QList<StageTarget *> rows = choir(props);

    // Unticked (the default): this line only.
    QTimer::singleShot(0, [&]() {
        QDialog *d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(d != nullptr);
        QCheckBox *whole = d->findChild<QCheckBox *>(QStringLiteral("lineWholeSet"));
        QVERIFY(whole != nullptr && !whole->isHidden() && !whole->isChecked());
        QDoubleSpinBox *h = d->findChild<QDoubleSpinBox *>(QStringLiteral("personHeight"));
        QVERIFY(h != nullptr);
        h->setValue(1.10);
        d->accept();
    });
    mon.slotEditTarget(rows.at(0)->id());
    QVERIFY(qAbs(rows.at(0)->effectivePersonHeight() - 1.10f) < 1e-3f);
    QVERIFY(qAbs(rows.at(1)->effectivePersonHeight() - 1.60f) < 1e-3f);

    // Ticked: all three.
    QTimer::singleShot(0, [&]() {
        QDialog *d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(d != nullptr);
        d->findChild<QCheckBox *>(QStringLiteral("lineWholeSet"))->setChecked(true);
        d->findChild<QDoubleSpinBox *>(QStringLiteral("personHeight"))->setValue(1.30);
        d->accept();
    });
    mon.slotEditTarget(rows.at(2)->id());
    foreach (StageTarget *t, rows)
        QVERIFY2(qAbs(t->effectivePersonHeight() - 1.30f) < 1e-3f, qPrintable(t->name()));
    QCOMPARE(rows.at(1)->costumeColor(), QColor(Qt::blue));

    // A line with no set has no box.
    StageTarget *solo = PerformerFill::addLine(props, QVector3D(0, 6, 0), QVector3D(2, 6, 0), 3,
                                               PerformerFill::Options(), 0);
    QTimer::singleShot(0, [&]() {
        QDialog *d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(d != nullptr);
        QVERIFY(d->findChild<QCheckBox *>(QStringLiteral("lineWholeSet"))->isHidden());
        d->reject();
    });
    mon.slotEditTarget(solo->id());
}

void PerformerSets_Test::aimPickersSkipTheChoir()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StageTarget *mark = props->addStageTarget();
    mark->setName(QStringLiteral("Centre mark"));
    StageTarget *solo = PerformerFill::addLine(props, QVector3D(0, 6, 0), QVector3D(2, 6, 0), 3,
                                               PerformerFill::Options(), 0);
    solo->setName(QStringLiteral("Soloists"));
    const QList<StageTarget *> rows = choir(props);

    QStringList got = names(PerformerFill::pickable(&doc));
    QVERIFY(got.contains(QStringLiteral("Centre mark")));
    QVERIFY(got.contains(QStringLiteral("Soloists")));
    QVERIFY(!got.contains(QStringLiteral("Choir row 1")));
    QCOMPARE(got.size(), 2);

    // What the list holds selected stays.
    got = names(PerformerFill::pickable(&doc, rows.at(1)->id()));
    QCOMPARE(got.size(), 3);
    QVERIFY(got.contains(QStringLiteral("Choir row 2")));

    // A line an aim palette uses stays.
    QLCPalette *pal = new QLCPalette(QLCPalette::PanTilt, &doc);
    pal->setStageTargetId(rows.at(2)->id());
    QVERIFY(doc.addPalette(pal));
    got = names(PerformerFill::pickable(&doc));
    QVERIFY(got.contains(QStringLiteral("Choir row 3")));
    QVERIFY(!got.contains(QStringLiteral("Choir row 1")));
}

void PerformerSets_Test::thePalettePickerSkipsTheChoir()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StageTarget *mark = props->addStageTarget();
    mark->setName(QStringLiteral("Centre mark"));
    const QList<StageTarget *> rows = choir(props);

    PaletteEditDialog dlg(nullptr, &doc);
    QComboBox *combo = dlg.findChild<QComboBox *>();
    QComboBox *tc = dlg.m_targetCombo;
    QVERIFY(combo != nullptr && tc != nullptr);
    QCOMPARE(tc->count(), 2);               // (none) and the mark
    QCOMPARE(tc->itemText(1), QStringLiteral("Centre mark"));
    Q_UNUSED(rows);
}

void PerformerSets_Test::aPlainLineKeepsItsHeights()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    QWidget parent;
    Monitor mon(&parent, &doc);
    StageTarget *t = props->addStageTarget();
    t->makeLine();
    t->setPosition(QVector3D(1, 2, 0));
    t->setLinePoints({ QVector3D(-2, 0, 2.2f), QVector3D(2, 0, 0.9f) });
    const QVector<QVector3D> before = t->linePoints();

    QTimer::singleShot(0, [&]() {
        QDialog *d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(d != nullptr);
        d->findChild<QSpinBox *>(QStringLiteral("linePeople"))->setValue(5);
        d->findChild<QDoubleSpinBox *>(QStringLiteral("personHeight"))->setValue(1.40);
        d->accept();
    });
    mon.slotEditTarget(t->id());
    QVERIFY(t->isPerformerLine());
    QCOMPARE(t->linePoints(), before);              // still where they were, not at chest
    QCOMPARE(props->targetLinePoints(t).first().z(), 2.2f);
    QCOMPARE(props->targetLinePoints(t).last().z(), 0.9f);
}

QTEST_MAIN(PerformerSets_Test)
