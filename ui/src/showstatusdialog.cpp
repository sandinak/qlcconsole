/*
  Q Light Controller Plus - qlcconsole
  showstatusdialog.cpp

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
#include <QVBoxLayout>
#include <QLabel>
#include <QSet>
#include <QMap>

#include "showstatusdialog.h"
#include "showstatus.h"

namespace
{
    enum Roles
    {
        KeyRole = Qt::UserRole,     ///< the entry a row belongs to
        DataRole,                   ///< an item row's ShowStatus::Entry::itemData
        IsItemRole                  ///< true on an item row (not summary or detail)
    };
}

ShowStatusDialog::ShowStatusDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Show readiness"));
    QVBoxLayout *lay = new QVBoxLayout(this);

    // An actual list, not a wall of prose: one top-level row per registered
    // source (its summary, bold), the one-sentence detail as an italic
    // child, then each individually affected thing (a universe, say) as its
    // own row underneath -- so "which ones, specifically" is a glance
    // rather than a re-read of a paragraph.
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    lay->addWidget(m_tree);

    QLabel *hint = new QLabel(tr("Select rows (shift-click for a range) and fix them, "
                                 "or double-click one."), this);
    hint->setStyleSheet("QLabel { color: #909090; }");
    lay->addWidget(hint);

    m_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(m_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(m_box);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &ShowStatusDialog::updateActionButton);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
        if (item == nullptr || !item->data(0, IsItemRole).toBool())
            return;
        m_tree->clearSelection();
        item->setSelected(true);
        runAction(0);
    });
    // Fixing one thing changes the list: follow it.
    connect(ShowStatus::instance(), &ShowStatus::changed, this, &ShowStatusDialog::rebuild);

    rebuild();
    resize(560, 400);
}

void ShowStatusDialog::rebuild()
{
    // What was selected, to select again where it is still listed.
    QSet<QString> wasSelected;
    foreach (QTreeWidgetItem *it, m_tree->selectedItems())
        wasSelected.insert(it->data(0, KeyRole).toString() + QLatin1Char('\n')
                           + it->data(0, DataRole).toString() + QLatin1Char('\n')
                           + it->text(0));

    const QList<ShowStatus::Entry> entries = ShowStatus::instance()->allEntries();
    if (entries.isEmpty() && isVisible())
    {
        accept();                   // nothing left to fix
        return;
    }

    m_tree->blockSignals(true);
    m_tree->clear();
    foreach (const ShowStatus::Entry &e, entries)
    {
        const bool actionable = !ShowStatus::instance()->actionLabel(e.key).isEmpty();
        QTreeWidgetItem *top = new QTreeWidgetItem(m_tree);
        top->setText(0, e.summary.isEmpty() ? e.key : e.summary);
        top->setData(0, KeyRole, e.key);
        QFont boldFont = top->font(0);
        boldFont.setBold(true);
        top->setFont(0, boldFont);
        if (!actionable)
            top->setFlags(top->flags() & ~Qt::ItemIsSelectable);

        if (e.detail.isEmpty() == false)
        {
            QTreeWidgetItem *d = new QTreeWidgetItem(top);
            d->setText(0, e.detail);
            QFont italicFont = d->font(0);
            italicFont.setItalic(true);
            d->setFont(0, italicFont);
            d->setFlags(d->flags() & ~Qt::ItemIsSelectable);
        }
        QMap<QString, QTreeWidgetItem *> groups;
        for (int i = 0; i < e.items.size(); ++i)
        {
            // Under its heading, if it has one ("Local", "Included").
            QTreeWidgetItem *under = top;
            const QString group = e.itemGroups.value(i);
            if (!group.isEmpty())
            {
                under = groups.value(group);
                if (under == nullptr)
                {
                    under = new QTreeWidgetItem(top, QStringList(group));
                    under->setData(0, KeyRole, e.key);
                    if (!actionable)
                        under->setFlags(under->flags() & ~Qt::ItemIsSelectable);
                    under->setExpanded(true);
                    groups.insert(group, under);
                }
            }
            QTreeWidgetItem *row = new QTreeWidgetItem(under, QStringList(e.items.at(i)));
            row->setToolTip(0, e.items.at(i));      // the whole of a long line
            row->setData(0, KeyRole, e.key);
            row->setData(0, DataRole, e.itemData.value(i));
            row->setData(0, IsItemRole, true);
            if (!actionable)
                row->setFlags(row->flags() & ~Qt::ItemIsSelectable);
        }
        top->setExpanded(true);
        foreach (QTreeWidgetItem *g, groups)
            g->setExpanded(true);
    }

    QTreeWidgetItemIterator it(m_tree);
    while (*it)
    {
        const QString id = (*it)->data(0, KeyRole).toString() + QLatin1Char('\n')
                           + (*it)->data(0, DataRole).toString() + QLatin1Char('\n')
                           + (*it)->text(0);
        if (wasSelected.contains(id))
            (*it)->setSelected(true);
        ++it;
    }
    m_tree->blockSignals(false);
    updateActionButton();
}

QString ShowStatusDialog::selectionKey(QVariantList *items) const
{
    QString key;
    QVariantList data;
    QSet<QTreeWidgetItem *> taken;
    foreach (QTreeWidgetItem *it, m_tree->selectedItems())
    {
        const QString k = it->data(0, KeyRole).toString();
        if (k.isEmpty())
            continue;
        if (!key.isEmpty() && k != key)
            return QString();           // spans two problems: no one fix
        key = k;
        // An entry's own row, or a heading, stands for all the items under it.
        QList<QTreeWidgetItem *> rows;
        if (it->data(0, IsItemRole).toBool())
            rows << it;
        else
        {
            QList<QTreeWidgetItem *> todo;
            for (int c = 0; c < it->childCount(); ++c)
                todo << it->child(c);
            while (!todo.isEmpty())
            {
                QTreeWidgetItem *x = todo.takeFirst();
                if (x->data(0, IsItemRole).toBool())
                    rows << x;
                for (int c = 0; c < x->childCount(); ++c)
                    todo << x->child(c);
            }
        }
        foreach (QTreeWidgetItem *row, rows)
            if (!taken.contains(row))
            {
                taken.insert(row);
                data << row->data(0, DataRole);
            }
    }
    if (key.isEmpty() || data.isEmpty() || ShowStatus::instance()->actionLabel(key).isEmpty())
        return QString();
    if (items != nullptr)
        *items = data;
    return key;
}

void ShowStatusDialog::updateActionButton()
{
    QVariantList items;
    const QString key = selectionKey(&items);
    QStringList labels = key.isEmpty() ? QStringList() : ShowStatus::instance()->actionLabels(key);
    bool offered = !labels.isEmpty();
    if (labels.isEmpty())
    {
        // Still shows that fixes are on offer, greyed until something is picked.
        foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
            if (!ShowStatus::instance()->actionLabel(e.key).isEmpty())
            {
                labels << tr("Fix…");
                break;
            }
    }
    while (m_actions.size() < labels.size())
    {
        const int index = m_actions.size();
        QPushButton *b = m_box->addButton(QString(), QDialogButtonBox::ActionRole);
        connect(b, &QPushButton::clicked, this, [this, index]() { runAction(index); });
        m_actions << b;
    }
    for (int i = 0; i < m_actions.size(); ++i)
    {
        QPushButton *b = m_actions.at(i);
        if (i >= labels.size())
        {
            b->setVisible(false);
            continue;
        }
        QString label = labels.at(i);
        if (offered && items.size() > 1)
            label += QString(" (%1)").arg(items.size());
        b->setText(label);
        b->setVisible(true);
        b->setEnabled(offered);
    }
}

void ShowStatusDialog::runAction(int index)
{
    QVariantList items;
    const QString key = selectionKey(&items);
    if (key.isEmpty())
        return;
    ShowStatus::instance()->runAction(key, this, items, index);
}
