/*
  Q Light Controller Plus - Test Unit
  structureduplicateui_test.cpp

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

#include <climits>
#include <QMenu>
#include <QPointer>
#include <QScopedPointer>
#include <QScopeGuard>
#include <QTableWidget>
#include <QTreeWidget>
#include <QComboBox>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define protected public
#define private public
#include "structureduplicateui_test.h"
#include "structureduplicatedialog.h"
#include "monitorgraphicsview.h"
#include "showsettingsdialog.h"
#include "teachingsettings.h"
#include "monitorproperties.h"
#include "patchplanner.h"
#include "fixture.h"
#include "doc.h"
#undef protected
#undef private

typedef StructureDuplicate SD;

namespace
{

struct Rig
{
    QScopedPointer<Doc> doc;
    Truss *t1 = nullptr, *t2 = nullptr, *t3 = nullptr;
    QString k1, k2, k3;
    QList<Fixture *> lights;
};

void hang(Rig &r, Truss *t, const QString &name, quint32 address, float offset, bool patched = true)
{
    Fixture *fx = new Fixture(r.doc.data());
    fx->setChannels(10);
    fx->setUniverse(0);
    fx->setAddress(address);
    fx->setPatched(patched);
    fx->setName(name);
    QVERIFY(r.doc->addFixture(fx));
    FixtureRigProps rp;
    rp.trussId = t->id();
    rp.trussOffset = offset;
    r.doc->monitorProperties()->setFixtureRigProps(fx->id(), rp);
    FixturePreviewItem item;
    item.m_baseItem.m_position = r.doc->monitorProperties()->fixtureRigPosition(fx->id()) * 1000.0f;
    r.doc->monitorProperties()->setFixtureProperties(fx->id(), item);
    if (t == r.t1)
        r.lights << fx;
}

void build(Rig &r, Doc *parent)
{
    r.doc.reset(new Doc(parent));
    MonitorProperties *props = r.doc->monitorProperties();
    auto truss = [props](const QString &n, float x) {
        Truss *t = props->addTruss();
        t->setName(n);
        t->setOrigin(QVector3D(x, 1.0f, 4.0f));
        t->setLength(4.0f);
        return t;
    };
    r.t1 = truss("T-1", 0.0f);
    r.t2 = truss("T-2", 5.0f);
    r.t3 = truss("T-3", 10.0f);
    hang(r, r.t1, "T1-Spot 1", 0, 0.5f);
    hang(r, r.t1, "T1-Spot 2", 10, 1.5f);
    hang(r, r.t1, "T1-Wash", 20, 2.5f);
    r.k1 = QString("truss:%1").arg(r.t1->id());
    r.k2 = QString("truss:%1").arg(r.t2->id());
    r.k3 = QString("truss:%1").arg(r.t3->id());
}

void setUpView(MonitorGraphicsView &view)
{
    view.resize(800, 600);
    view.setGridMetrics(1000.0);
    view.setGridSize(QSize(12, 8));
    view.setViewPOV(MonitorGraphicsView::PovTop);
}

int onStructure(Doc *doc, const QString &key)
{
    return PatchPlanner::fixturesOn(doc, key).size();
}

}

void StructureDuplicateUi_Test::theMenuOffersItWhereThereIsSomewhereToCopyTo()
{
    QScopedPointer<QObject> owner(new QObject);
    Rig r;
    build(r, nullptr);
    MonitorGraphicsView view(r.doc.data());
    setUpView(view);

    auto has = [&](const QString &key) {
        QMenu menu;
        view.populateItemMenu(&menu, key.section(':', 0, 0), key.section(':', 1, 1).toUInt(), QPointF(), false);
        foreach (QAction *a, menu.actions())
            if (a->text().startsWith(QStringLiteral("Duplicate Setup to")))
                return true;
        return false;
    };
    QVERIFY(has(r.k1));            // it carries fixtures, and there are other trusses
    QVERIFY(!has(r.k2));           // an empty one has nothing to copy
}

void StructureDuplicateUi_Test::theDialogPreviewsBeforeAnythingIsMade()
{
    Rig r;
    build(r, nullptr);
    StructureDuplicateDialog dlg(r.doc.data(), r.k1);
    QCOMPARE(dlg.targets(), QStringList({ r.k2, r.k3 }));
    // Previewed: six copies, named, addressed after the originals.
    QCOMPARE(dlg.preview().rows.size(), 6);
    QCOMPARE(dlg.preview().rows.first().name, QString("T2-Spot 1"));
    QCOMPARE(dlg.preview().rows.first().address, quint32(30));
    QTreeWidget *rows = dlg.findChild<QTreeWidget *>("duplicatePreview");
    QVERIFY(rows != nullptr);
    QVERIFY(rows->topLevelItemCount() >= 6);
    QCOMPARE(rows->topLevelItem(0)->text(1), QString("T2-Spot 1"));
    QCOMPARE(rows->topLevelItem(0)->text(3), QString("1.31"));
    // Nothing was made by looking.
    QCOMPARE(r.doc->fixtures().size(), 3);

    dlg.setTargets({ r.k3 });
    QCOMPARE(dlg.preview().rows.size(), 3);
    QCOMPARE(dlg.preview().rows.first().name, QString("T3-Spot 1"));
}

void StructureDuplicateUi_Test::theDialogCarriesItsChoices()
{
    Rig r;
    build(r, nullptr);
    StructureDuplicateDialog dlg(r.doc.data(), r.k1);
    QCOMPARE(dlg.options().patch, SD::NextFree);                    // the defaults
    QCOMPARE(dlg.options().fixtureGroups, SD::AddToGroups);
    QCOMPARE(dlg.options().mapGroups, SD::ParallelGroups);
    QCOMPARE(dlg.options().defaultExisting, SD::Skip);

    dlg.setPatchMode(SD::Offset);
    dlg.setOffsets(100, 1);
    QCOMPARE(dlg.options().patch, SD::Offset);
    QCOMPARE(dlg.preview().rows.first().universe, quint32(1));      // T-2: one universe on
    QCOMPARE(dlg.preview().rows.first().address, quint32(100));
    QCOMPARE(dlg.preview().rows.last().universe, quint32(2));       // T-3: two on
    dlg.setFixtureGroupMode(SD::ParallelGroups);
    QCOMPARE(dlg.options().fixtureGroups, SD::ParallelGroups);
    dlg.setPatchMode(SD::Unpatched);
    QVERIFY(!dlg.preview().rows.first().patched);
}

void StructureDuplicateUi_Test::twoTowersAreMadeAndOneUndoTakesThemBack()
{
    Rig r;
    build(r, nullptr);
    MonitorGraphicsView view(r.doc.data());
    setUpView(view);
    foreach (Fixture *f, r.lights)
        view.addFixture(f->id(), QPointF(0, 0));
    const int items = view.m_fixtures.size();
    QVERIFY(!view.canUndo());

    QVERIFY(view.duplicateStructureSetup(r.k1, { r.k2, r.k3 }, SD::Options()));
    QCOMPARE(onStructure(r.doc.data(), r.k2), 3);
    QCOMPARE(onStructure(r.doc.data(), r.k3), 3);
    QCOMPARE(view.m_fixtures.size(), items + 6);            // on the plot too
    QVERIFY(view.canUndo());

    view.undoLastMove();                                    // one step
    QCOMPARE(r.doc->fixtures().size(), 3);
    QCOMPARE(view.m_fixtures.size(), items);
    QVERIFY(!view.canUndo());
}

void StructureDuplicateUi_Test::anOccupiedTargetIsAskedAbout()
{
    Rig r;
    build(r, nullptr);
    hang(r, r.t2, "Old", 200, 1.0f);
    const quint32 oldId = PatchPlanner::fixturesOn(r.doc.data(), r.k2).first();
    MonitorGraphicsView view(r.doc.data());
    setUpView(view);

    StructureDuplicateDialog dlg(r.doc.data(), r.k1);
    QCOMPARE(dlg.preview().occupied.value(r.k2), 1);
    QCOMPARE(dlg.preview().skipped, QStringList({ r.k2 }));         // skip is the default
    QCOMPARE(dlg.preview().rows.size(), 3);

    dlg.setExisting(r.k2, SD::Add);
    QCOMPARE(dlg.preview().rows.size(), 6);
    QVERIFY(view.duplicateStructureSetup(r.k1, dlg.targets(), dlg.options()));
    QCOMPARE(onStructure(r.doc.data(), r.k2), 4);
    view.undoLastMove();
    QCOMPARE(onStructure(r.doc.data(), r.k2), 1);

    dlg.setExisting(r.k2, SD::Replace);
    QVERIFY(view.duplicateStructureSetup(r.k1, dlg.targets(), dlg.options()));
    QCOMPARE(onStructure(r.doc.data(), r.k2), 3);
    QVERIFY(r.doc->fixture(oldId) == nullptr || r.doc->fixture(oldId)->name() != "Old");
    view.undoLastMove();
    QCOMPARE(onStructure(r.doc.data(), r.k2), 1);
    QVERIFY(r.doc->fixture(oldId) != nullptr);
    QCOMPARE(r.doc->fixture(oldId)->name(), QString("Old"));
    QVERIFY(view.m_fixtures.contains(oldId));                       // back on the plot
}

void StructureDuplicateUi_Test::aTeachingShowAsksBeforeThePatchChanges()
{
    Rig r;
    build(r, nullptr);
    MonitorGraphicsView view(r.doc.data());
    setUpView(view);
    r.doc->teachingSettings()->setEnabled(true);
    ShowSettingsDialog::setTestMode(true);
    auto back = qScopeGuard([]() { ShowSettingsDialog::setTestMode(false); ShowSettingsDialog::testAnswers().clear(); });
    int &asked = ShowSettingsDialog::patchQuestions();
    asked = 0;

    // Declined: nothing is made.
    ShowSettingsDialog::testAnswers() = { "no" };
    QVERIFY(!view.duplicateStructureSetup(r.k1, { r.k2 }, SD::Options()));
    QCOMPARE(asked, 1);
    QCOMPARE(r.doc->fixtures().size(), 3);
    QVERIFY(!view.canUndo());

    // Accepted: made, patched.
    ShowSettingsDialog::testAnswers() = { "yes" };
    QVERIFY(view.duplicateStructureSetup(r.k1, { r.k2 }, SD::Options()));
    QCOMPARE(asked, 2);
    QCOMPARE(r.doc->fixtures().size(), 6);
    QVERIFY(r.doc->fixture(PatchPlanner::fixturesOn(r.doc.data(), r.k2).first())->isPatched());
}

void StructureDuplicateUi_Test::anUnpatchedCopyNeedsNoQuestion()
{
    Rig r;
    build(r, nullptr);
    MonitorGraphicsView view(r.doc.data());
    setUpView(view);
    r.doc->teachingSettings()->setEnabled(true);
    ShowSettingsDialog::setTestMode(true);
    auto back = qScopeGuard([]() { ShowSettingsDialog::setTestMode(false); ShowSettingsDialog::testAnswers().clear(); });
    int &asked = ShowSettingsDialog::patchQuestions();
    asked = 0;
    SD::Options opt;
    opt.patch = SD::Unpatched;
    QVERIFY(view.duplicateStructureSetup(r.k1, { r.k2 }, opt));
    QCOMPARE(asked, 0);                     // the patch did not change
    QCOMPARE(r.doc->fixtures().size(), 6);
}

QTEST_MAIN(StructureDuplicateUi_Test)
