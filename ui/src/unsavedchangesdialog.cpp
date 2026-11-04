/*
  Q Light Controller Plus
  unsavedchangesdialog.cpp

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
#include <QTreeWidget>
#include <QPushButton>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QDateTime>
#include <QLabel>

#include "unsavedchangesdialog.h"
#include "showtreecontroller.h"
#include "paletteops.h"
#include "doc.h"

UnsavedChangesDialog::UnsavedChangesDialog(Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("unsavedChangesDialog"));
    setWindowTitle(tr("Unsaved changes"));
    setMinimumSize(420, 260);
    resize(560, 360);

    QVBoxLayout *lay = new QVBoxLayout(this);
    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("unsavedSummary"));
    m_summary->setWordWrap(true);
    lay->addWidget(m_summary);

    m_list = new QTreeWidget(this);
    m_list->setObjectName(QStringLiteral("unsavedList"));
    m_list->setColumnCount(3);
    m_list->setHeaderLabels({ tr("Time"), tr("Kind"), tr("Change") });
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setAlternatingRowColors(true);
    m_list->header()->setStretchLastSection(true);
    m_list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    lay->addWidget(m_list, 1);

    QLabel *note = new QLabel(tr("Newest first. Double-click a row to go to it."), this);
    note->setEnabled(false);
    lay->addWidget(note);

    QDialogButtonBox *box = new QDialogButtonBox(this);
    m_go = box->addButton(tr("Go to"), QDialogButtonBox::ActionRole);
    m_go->setObjectName(QStringLiteral("unsavedGo"));
    m_undo = box->addButton(tr("Undo last"), QDialogButtonBox::ActionRole);
    m_undo->setObjectName(QStringLiteral("unsavedUndo"));
    m_save = box->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    m_save->setObjectName(QStringLiteral("unsavedSave"));
    QPushButton *close = box->addButton(QDialogButtonBox::Close);
    close->setObjectName(QStringLiteral("unsavedClose"));
    lay->addWidget(box);

    connect(m_go, &QPushButton::clicked, this, &UnsavedChangesDialog::slotGo);
    connect(m_undo, &QPushButton::clicked, this, &UnsavedChangesDialog::slotUndoLast);
    connect(m_save, &QPushButton::clicked, this, &UnsavedChangesDialog::saveRequested);
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    connect(m_list, &QTreeWidget::itemDoubleClicked, this, [this]() { slotGo(); });
    connect(m_list, &QTreeWidget::itemSelectionChanged, this, &UnsavedChangesDialog::slotSelectionChanged);

    connect(doc, &Doc::modified, this, &UnsavedChangesDialog::refresh);
    if (doc->showTree() != nullptr)
        connect(doc->showTree(), &ShowTreeController::changed, this, &UnsavedChangesDialog::refresh);
    if (doc->paletteOps() != nullptr)
        connect(doc->paletteOps(), &PaletteOps::changed, this, &UnsavedChangesDialog::refresh);

    refresh();
}

void UnsavedChangesDialog::refresh()
{
    ChangeJournal *journal = m_doc->changeJournal();
    m_entries = journal->entries();
    m_list->clear();
    for (const ChangeJournal::Entry &e : m_entries)
    {
        QString text = e.title;
        if (!e.what.isEmpty())
            text += QString::fromUtf8(" \xe2\x80\x94 ") + e.what;
        if (e.edits > 1)
            text += tr(" (%1 edits)").arg(e.edits);
        QTreeWidgetItem *item = new QTreeWidgetItem(m_list);
        item->setText(0, e.itemised && e.when > 0 ? QDateTime::fromMSecsSinceEpoch(e.when).toString(QStringLiteral("HH:mm:ss"))
                                                  : QString());
        item->setText(1, ChangeJournal::kindName(e.kind));
        item->setText(2, text);
        item->setToolTip(2, text);
        if (!e.itemised)
        {
            QFont f = item->font(2);
            f.setItalic(true);
            item->setFont(2, f);
        }
    }
    if (m_entries.isEmpty())
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_list);
        item->setText(2, m_doc->isModified()
                              ? tr("Nothing is listed: the changes were undone, or are not recorded.")
                              : tr("No changes since the last save."));
        item->setFlags(Qt::NoItemFlags);
    }
    m_summary->setText(journal->summary());
    m_undo->setEnabled(m_doc->showTree() != nullptr && journal->stepCount() > 0
                       && m_doc->showTree()->canUndoShared());
    slotSelectionChanged();
}

void UnsavedChangesDialog::slotSelectionChanged()
{
    const int row = m_list->indexOfTopLevelItem(m_list->currentItem());
    m_go->setEnabled(row >= 0 && row < m_entries.count()
                     && m_entries.at(row).subject.jump != ChangeJournal::NoJump);
}

void UnsavedChangesDialog::slotGo()
{
    const int row = m_list->indexOfTopLevelItem(m_list->currentItem());
    if (row >= 0 && row < m_entries.count() && m_entries.at(row).subject.jump != ChangeJournal::NoJump)
        emit jumpRequested(m_entries.at(row).subject);
}

void UnsavedChangesDialog::slotUndoLast()
{
    if (m_doc->showTree() != nullptr)
        m_doc->showTree()->undoShared();
    refresh();
}

QStringList UnsavedChangesDialog::rows() const
{
    QStringList out;
    for (int i = 0; i < m_list->topLevelItemCount() && i < m_entries.count(); ++i)
    {
        const ChangeJournal::Entry &e = m_entries.at(i);
        out << QStringLiteral("%1|%2|%3|%4|%5").arg(m_list->topLevelItem(i)->text(0),
                ChangeJournal::kindName(e.kind), e.title, e.what).arg(e.edits);
    }
    return out;
}

QString UnsavedChangesDialog::summaryText() const
{
    return m_summary->text();
}

void UnsavedChangesDialog::activateRow(int row)
{
    m_list->setCurrentItem(m_list->topLevelItem(row));
    slotGo();
}
