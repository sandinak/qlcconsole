/*
  Q Light Controller Plus
  structureduplicatedialog.cpp

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

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "structureduplicatedialog.h"
#include "patchplanner.h"
#include "doc.h"

typedef StructureDuplicate SD;

StructureDuplicateDialog::StructureDuplicateDialog(Doc *doc, const QString &sourceKey,
                                                   QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_source(sourceKey)
{
    setObjectName(QStringLiteral("structureDuplicateDialog"));
    const QString name = PatchPlanner::structureName(doc, sourceKey);
    setWindowTitle(tr("Duplicate %1's Setup").arg(name));
    resize(760, 640);
    m_keys = SD::targetsFor(doc, sourceKey);

    QVBoxLayout *top = new QVBoxLayout(this);
    QLabel *intro = new QLabel(tr("Copy the %1 fixtures on %2 onto the structures ticked below: the "
                                  "same fixtures, hung the same way, with their own names and "
                                  "addresses.")
                                   .arg(PatchPlanner::fixturesOn(doc, sourceKey).size()).arg(name),
                               this);
    intro->setWordWrap(true);
    top->addWidget(intro);

    m_targets = new QTableWidget(m_keys.size(), 3, this);
    m_targets->setObjectName(QStringLiteral("duplicateTargets"));
    m_targets->setHorizontalHeaderLabels({ tr("Copy to"), tr("Fixtures on it now"),
                                           tr("If it has fixtures") });
    m_targets->verticalHeader()->hide();
    m_targets->horizontalHeader()->setStretchLastSection(true);
    m_targets->setSelectionMode(QAbstractItemView::NoSelection);
    m_targets->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_targets->setMaximumHeight(170);
    for (int i = 0; i < m_keys.size(); ++i)
    {
        QTableWidgetItem *it = new QTableWidgetItem(PatchPlanner::structureName(doc, m_keys.at(i)));
        it->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        it->setCheckState(Qt::Checked);
        m_targets->setItem(i, 0, it);
        const int there = PatchPlanner::fixturesOn(doc, m_keys.at(i)).size();
        QTableWidgetItem *n = new QTableWidgetItem(there > 0 ? QString::number(there) : QString());
        n->setFlags(Qt::ItemIsEnabled);
        m_targets->setItem(i, 1, n);
        QComboBox *how = new QComboBox(m_targets);
        how->addItem(tr("Skip it"), int(SD::Skip));
        how->addItem(tr("Add alongside"), int(SD::Add));
        how->addItem(tr("Replace them"), int(SD::Replace));
        how->setEnabled(there > 0);
        connect(how, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StructureDuplicateDialog::refresh);
        m_targets->setCellWidget(i, 2, how);
    }
    connect(m_targets, &QTableWidget::itemChanged, this, &StructureDuplicateDialog::refresh);
    top->addWidget(m_targets);

    QGroupBox *patch = new QGroupBox(tr("Patch the copies"), this);
    QVBoxLayout *pl = new QVBoxLayout(patch);
    m_nextFree = new QRadioButton(tr("At the next free addresses after the originals"), patch);
    m_offset = new QRadioButton(tr("With an address pattern: each copy this far on from the original"), patch);
    m_unpatched = new QRadioButton(tr("Leave them unpatched (address them later)"), patch);
    m_nextFree->setChecked(true);
    m_addressStep = new QSpinBox(patch);
    m_addressStep->setRange(-511, 511);
    m_addressStep->setPrefix(tr("addresses: "));
    m_addressStep->setValue(0);
    m_universeStep = new QSpinBox(patch);
    m_universeStep->setRange(-100, 100);
    m_universeStep->setPrefix(tr("universes: "));
    m_universeStep->setValue(1);
    QHBoxLayout *ol = new QHBoxLayout;
    ol->addWidget(m_offset);
    ol->addWidget(m_addressStep);
    ol->addWidget(m_universeStep);
    ol->addStretch();
    pl->addWidget(m_nextFree);
    pl->addLayout(ol);
    pl->addWidget(m_unpatched);
    top->addWidget(patch);

    QGroupBox *groups = new QGroupBox(tr("Groups the originals are in"), this);
    QFormLayout *gl = new QFormLayout(groups);
    m_fixtureGroups = new QComboBox(groups);
    m_fixtureGroups->setObjectName(QStringLiteral("duplicateFixtureGroups"));
    m_fixtureGroups->addItem(tr("Add the copies to the same groups"), int(SD::AddToGroups));
    m_fixtureGroups->addItem(tr("Make a parallel group for each structure (T2 Spots)"),
                             int(SD::ParallelGroups));
    m_mapGroups = new QComboBox(groups);
    m_mapGroups->setObjectName(QStringLiteral("duplicateMapGroups"));
    m_mapGroups->addItem(tr("Add the copies to the same groups"), int(SD::AddToGroups));
    m_mapGroups->addItem(tr("Make a parallel group for each structure (T2 Spots)"),
                         int(SD::ParallelGroups));
    m_mapGroups->setCurrentIndex(1);
    gl->addRow(tr("Fixture groups:"), m_fixtureGroups);
    gl->addRow(tr("Plot groups:"), m_mapGroups);
    QLabel *named = new QLabel(tr("A group named for %1 always gets a parallel group.").arg(name), groups);
    named->setWordWrap(true);
    gl->addRow(named);
    top->addWidget(groups);

    m_rows = new QTreeWidget(this);
    m_rows->setObjectName(QStringLiteral("duplicatePreview"));
    m_rows->setRootIsDecorated(false);
    m_rows->setHeaderLabels({ tr("Original"), tr("Copy"), tr("On"), tr("Address"), tr("Note") });
    m_rows->header()->setStretchLastSection(true);
    top->addWidget(m_rows, 1);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    top->addWidget(m_summary);
    m_warning = new QLabel(this);
    m_warning->setWordWrap(true);
    m_warning->setStyleSheet(QStringLiteral("color: #b45309;"));
    top->addWidget(m_warning);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = box->button(QDialogButtonBox::Ok);
    m_ok->setText(tr("Duplicate"));
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    top->addWidget(box);

    connect(m_nextFree, &QRadioButton::toggled, this, &StructureDuplicateDialog::refresh);
    connect(m_offset, &QRadioButton::toggled, this, &StructureDuplicateDialog::refresh);
    connect(m_unpatched, &QRadioButton::toggled, this, &StructureDuplicateDialog::refresh);
    connect(m_addressStep, QOverload<int>::of(&QSpinBox::valueChanged), this, &StructureDuplicateDialog::refresh);
    connect(m_universeStep, QOverload<int>::of(&QSpinBox::valueChanged), this, &StructureDuplicateDialog::refresh);
    connect(m_fixtureGroups, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StructureDuplicateDialog::refresh);
    connect(m_mapGroups, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StructureDuplicateDialog::refresh);
    refresh();
}

QStringList StructureDuplicateDialog::targets() const
{
    QStringList out;
    for (int i = 0; i < m_keys.size(); ++i)
        if (m_targets->item(i, 0)->checkState() == Qt::Checked)
            out << m_keys.at(i);
    return out;
}

SD::Options StructureDuplicateDialog::options() const
{
    SD::Options o;
    o.patch = m_unpatched->isChecked() ? SD::Unpatched : m_offset->isChecked() ? SD::Offset : SD::NextFree;
    o.addressStep = m_addressStep->value();
    o.universeStep = m_universeStep->value();
    o.fixtureGroups = SD::GroupMode(m_fixtureGroups->currentData().toInt());
    o.mapGroups = SD::GroupMode(m_mapGroups->currentData().toInt());
    for (int i = 0; i < m_keys.size(); ++i)
        if (QComboBox *c = qobject_cast<QComboBox *>(m_targets->cellWidget(i, 2)))
            o.existing.insert(m_keys.at(i), SD::Existing(c->currentData().toInt()));
    return o;
}

void StructureDuplicateDialog::setTargets(const QStringList &keys)
{
    for (int i = 0; i < m_keys.size(); ++i)
        m_targets->item(i, 0)->setCheckState(keys.contains(m_keys.at(i)) ? Qt::Checked : Qt::Unchecked);
}

void StructureDuplicateDialog::setPatchMode(SD::PatchMode mode)
{
    (mode == SD::Unpatched ? m_unpatched : mode == SD::Offset ? m_offset : m_nextFree)->setChecked(true);
}

void StructureDuplicateDialog::setOffsets(int addressStep, int universeStep)
{
    m_addressStep->setValue(addressStep);
    m_universeStep->setValue(universeStep);
}

void StructureDuplicateDialog::setFixtureGroupMode(SD::GroupMode mode)
{
    m_fixtureGroups->setCurrentIndex(m_fixtureGroups->findData(int(mode)));
}

void StructureDuplicateDialog::setMapGroupMode(SD::GroupMode mode)
{
    m_mapGroups->setCurrentIndex(m_mapGroups->findData(int(mode)));
}

void StructureDuplicateDialog::setExisting(const QString &key, SD::Existing how)
{
    const int i = m_keys.indexOf(key);
    if (i < 0)
        return;
    if (QComboBox *c = qobject_cast<QComboBox *>(m_targets->cellWidget(i, 2)))
        c->setCurrentIndex(c->findData(int(how)));
}

void StructureDuplicateDialog::refresh()
{
    if (m_rows == nullptr || m_ok == nullptr)
        return;     // still being built
    m_addressStep->setEnabled(m_offset->isChecked());
    m_universeStep->setEnabled(m_offset->isChecked());

    m_preview = SD::preview(m_doc, m_source, targets(), options());
    m_rows->clear();
    foreach (const SD::Row &r, m_preview.rows)
    {
        Fixture *src = m_doc->fixture(r.sourceId);
        const QString where = r.patched
            ? QStringLiteral("%1.%2").arg(r.universe + 1).arg(r.address + 1) : tr("unpatched");
        QTreeWidgetItem *it = new QTreeWidgetItem(m_rows, {
            src != nullptr ? src->name() : QString(), r.name,
            PatchPlanner::structureName(m_doc, r.target), where, r.note });
        if (r.clash)
            it->setForeground(3, QBrush(QColor(QStringLiteral("#b45309"))));
    }
    foreach (const SD::GroupLine &g, m_preview.groups)
    {
        QTreeWidgetItem *it = new QTreeWidgetItem(m_rows, {
            g.source, g.name, PatchPlanner::structureName(m_doc, g.target),
            tr("%1 in a %2").arg(g.members).arg(g.kind),
            g.parallel ? tr("a new group") : tr("added to the group") });
        it->setForeground(0, QBrush(Qt::gray));
    }
    for (int c = 0; c < 4; ++c)
        m_rows->resizeColumnToContents(c);

    int patched = 0;
    foreach (const SD::Row &r, m_preview.rows)
        if (r.patched)
            ++patched;
    m_summary->setText(tr("%1 fixtures: %2 patched, %3 left unpatched. %4 left alone.")
                           .arg(m_preview.rows.size()).arg(patched)
                           .arg(m_preview.rows.size() - patched).arg(m_preview.skipped.size())
                       + (m_preview.newHangers + m_preview.newBars > 0
                              ? tr(" With %1 hangers and %2 child bars.")
                                    .arg(m_preview.newHangers).arg(m_preview.newBars)
                              : QString()));
    QString warn;
    if (m_preview.replacedFixtures > 0)
        warn = tr("Replace removes %1 fixtures already on the target%2. Undo puts the fixtures back.")
                   .arg(m_preview.replacedFixtures)
                   .arg(m_preview.replacedInScenes > 0
                            ? tr(", and %1 scenes lose their values for them (undo does not restore those values)")
                                  .arg(m_preview.replacedInScenes)
                            : QString());
    m_warning->setText(warn);
    m_ok->setEnabled(!m_preview.rows.isEmpty() || m_preview.replacedFixtures > 0);
}
