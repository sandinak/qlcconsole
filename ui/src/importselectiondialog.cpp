/*
  Q Light Controller
  importselectiondialog.cpp

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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QTreeWidget>
#include <QDialogButtonBox>
#include <QLabel>

#include "importselectiondialog.h"
#include "doc.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "function.h"

#define KColumnName 0
#define KColumnID   1

ImportSelectionDialog::ImportSelectionDialog(QWidget *parent, Doc *sourceDoc)
    : QDialog(parent)
    , m_sourceDoc(sourceDoc)
{
    setWindowTitle(tr("Import from workspace"));
    resize(500, 400);

    QVBoxLayout *root = new QVBoxLayout(this);

    root->addWidget(new QLabel(tr("Choose what to bring into the current workspace. "
                                   "Selecting a function also brings in whatever it "
                                   "depends on (member functions, the fixtures it targets)."),
                                this));

    QTabWidget *tabs = new QTabWidget(this);
    root->addWidget(tabs, 1);

    auto makeTree = [this]() {
        QTreeWidget *tree = new QTreeWidget(this);
        tree->setColumnCount(2);
        tree->setHeaderLabels(QStringList() << tr("Name") << tr("ID"));
        tree->setRootIsDecorated(false);
        tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
        tree->setSortingEnabled(true);
        tree->sortByColumn(KColumnName, Qt::AscendingOrder);
        return tree;
    };

    m_fixtureTree = makeTree();
    tabs->addTab(m_fixtureTree, tr("Fixtures"));

    m_groupTree = makeTree();
    tabs->addTab(m_groupTree, tr("Fixture Groups"));

    m_functionTree = makeTree();
    tabs->addTab(m_functionTree, tr("Functions"));

    QDialogButtonBox *buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    populate();
}

ImportSelectionDialog::~ImportSelectionDialog()
{
}

void ImportSelectionDialog::populate()
{
    if (m_sourceDoc == NULL)
        return;

    foreach (Fixture *fxi, m_sourceDoc->fixtures())
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_fixtureTree);
        item->setText(KColumnName, fxi->name());
        item->setText(KColumnID, QString::number(fxi->id()));
        item->setData(KColumnID, Qt::UserRole, fxi->id());
    }

    foreach (FixtureGroup *grp, m_sourceDoc->fixtureGroups())
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_groupTree);
        item->setText(KColumnName, grp->name());
        item->setText(KColumnID, QString::number(grp->id()));
        item->setData(KColumnID, Qt::UserRole, grp->id());
    }

    foreach (Function *f, m_sourceDoc->functions())
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_functionTree);
        item->setText(KColumnName, QString("%1 (%2)").arg(f->name(), Function::typeToString(f->type())));
        item->setText(KColumnID, QString::number(f->id()));
        item->setData(KColumnID, Qt::UserRole, f->id());
    }
}

QList<quint32> ImportSelectionDialog::selectedFixtures() const
{
    QList<quint32> ids;
    foreach (QTreeWidgetItem *item, m_fixtureTree->selectedItems())
        ids << item->data(KColumnID, Qt::UserRole).toUInt();
    return ids;
}

QList<quint32> ImportSelectionDialog::selectedFixtureGroups() const
{
    QList<quint32> ids;
    foreach (QTreeWidgetItem *item, m_groupTree->selectedItems())
        ids << item->data(KColumnID, Qt::UserRole).toUInt();
    return ids;
}

QList<quint32> ImportSelectionDialog::selectedFunctions() const
{
    QList<quint32> ids;
    foreach (QTreeWidgetItem *item, m_functionTree->selectedItems())
        ids << item->data(KColumnID, Qt::UserRole).toUInt();
    return ids;
}
