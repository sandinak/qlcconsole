/*
  Q Light Controller
  fixtureselection.cpp

  Copyright (c) Heikki Junnila

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

#include <QTreeWidgetItem>
#include <QTreeWidget>
#include <QHeaderView>
#include <QLabel>
#include <QAction>
#include <QSettings>

#include "fixturetreewidget.h"
#include <QLineEdit>
#include <QKeyEvent>
#include <QTreeWidgetItemIterator>
#include "fixtureselection.h"
#include "doc.h"

#define KColumnName 0

#define SETTINGS_GEOMETRY "fixtureselection/geometry"

FixtureSelection::FixtureSelection(QWidget* parent, Doc* doc)
    : QDialog(parent)
    , m_doc(doc)
    , m_selectionMode(Fixtures)
{
    Q_ASSERT(doc != NULL);

    setupUi(this);

    QAction* action = new QAction(this);
    action->setShortcut(QKeySequence(QKeySequence::Close));
    connect(action, SIGNAL(triggered(bool)), this, SLOT(reject()));
    addAction(action);

    m_treeFlags = FixtureTreeWidget::UniverseNumber |
                  FixtureTreeWidget::HeadsNumber |
                  FixtureTreeWidget::Manufacturer |
                  FixtureTreeWidget::Model |
                  FixtureTreeWidget::AddressRange |
                  FixtureTreeWidget::ShowGroups;

    // Type to find: name, manufacturer or model.
    m_search = new QLineEdit(this);
    m_search->setObjectName("fixtureSearch");
    m_search->setPlaceholderText(tr("Search fixtures (name, manufacturer, model)..."));
    m_search->setClearButtonEnabled(true);
    m_search->installEventFilter(this);
    m_mainLayout->addWidget(m_search);
    connect(m_search, &QLineEdit::textChanged, this, &FixtureSelection::slotSearchChanged);
    // Return adds what is picked (the single match, say).
    connect(m_search, &QLineEdit::returnPressed, this, [this]() {
        if (!m_tree->selectedItems().isEmpty())
            accept();
    });

    m_tree = new FixtureTreeWidget(m_doc, m_treeFlags, this);
    m_mainLayout->addWidget(m_tree);

    QSettings settings;
    QVariant geometrySettings = settings.value(SETTINGS_GEOMETRY);
    if (geometrySettings.isValid() == true)
        restoreGeometry(geometrySettings.toByteArray());

    connect(m_tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(slotItemDoubleClicked()));

    connect(m_tree, SIGNAL(itemSelectionChanged()),
            this, SLOT(slotSelectionChanged()));
}

FixtureSelection::~FixtureSelection()
{
    QSettings settings;
    settings.setValue(SETTINGS_GEOMETRY, saveGeometry());
}

int FixtureSelection::exec()
{
    //fillTree();
    m_tree->updateTree();
    if (m_tree->topLevelItemCount() == 0)
    {
        m_tree->setHeaderLabel(tr("No fixtures available"));
        QTreeWidgetItem* item = new QTreeWidgetItem(m_tree);
        item->setText(0, tr("Go to the Fixture Manager and add some fixtures first."));
        m_tree->resizeColumnToContents(0);
        m_tree->setEnabled(false);
        m_search->setEnabled(false);
        m_buttonBox->setStandardButtons(QDialogButtonBox::Cancel);
    }
    else
    {
        m_tree->setTextFilter(m_search->text());
        m_search->setFocus();
    }
    return QDialog::exec();
}

void FixtureSelection::slotSearchChanged(const QString &text)
{
    m_tree->setTextFilter(text);

    // Nothing hidden stays selected; one selectable match is picked for you.
    QList<QTreeWidgetItem *> matches;
    for (QTreeWidgetItemIterator it(m_tree); *it != NULL; ++it)
    {
        QTreeWidgetItem *item = *it;
        if (item->isHidden() && item->isSelected())
            item->setSelected(false);
        bool shown = !item->isHidden();
        for (QTreeWidgetItem *up = item->parent(); shown && up != NULL; up = up->parent())
            shown = !up->isHidden();
        const bool fixtureRow = item->data(KColumnName, PROP_ID).isValid()
                                && !item->data(KColumnName, PROP_HEAD).isValid();
        if (shown && fixtureRow && !text.trimmed().isEmpty()
            && (item->flags() & Qt::ItemIsSelectable) && (item->flags() & Qt::ItemIsEnabled))
            matches << item;
    }
    if (matches.size() == 1)
    {
        m_tree->clearSelection();
        matches.first()->setSelected(true);
        m_tree->setCurrentItem(matches.first());
        m_tree->scrollToItem(matches.first());
    }
}

bool FixtureSelection::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_search && event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Down)
    {
        // Into the list, on the first row still showing.
        for (QTreeWidgetItemIterator it(m_tree); *it != NULL; ++it)
        {
            QTreeWidgetItem *item = *it;
            if (item->isHidden() || !(item->flags() & Qt::ItemIsSelectable))
                continue;
            m_tree->setCurrentItem(item);
            break;
        }
        m_tree->setFocus();
        return true;
    }
    return QDialog::eventFilter(watched, event);
}

/****************************************************************************
 * Selected fixtures
 ****************************************************************************/

QList <quint32> FixtureSelection::selection() const
{
    return m_selectedFixtures;
}

QList <GroupHead> FixtureSelection::selectedHeads() const
{
    return m_selectedHeads;
}

/****************************************************************************
 * Multi-selection
 ****************************************************************************/

void FixtureSelection::setMultiSelection(bool multi)
{
    if (multi == true)
        m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    else
        m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
}

/****************************************************************************
 * Selection mode
 ****************************************************************************/

void FixtureSelection::setSelectionMode(SelectionMode mode)
{
    m_selectionMode = mode;
    if (mode == Fixtures)
    {
        m_tree->setRootIsDecorated(false);
        m_tree->setItemsExpandable(false);
        m_treeFlags = m_treeFlags & (~FixtureTreeWidget::ShowHeads);
    }
    else
    {
        m_tree->setRootIsDecorated(true);
        m_tree->setItemsExpandable(true);
        m_treeFlags = m_treeFlags | FixtureTreeWidget::ShowHeads;
    }
    m_tree->setFlags(m_treeFlags);
}

/****************************************************************************
 * Disabled items
 ****************************************************************************/

void FixtureSelection::setDisabledFixtures(const QList <quint32>& disabled)
{
    m_tree->setDisabledFixtures(disabled);
}

void FixtureSelection::setDisabledHeads(const QList <GroupHead>& disabled)
{
    m_tree->setDisabledHeads(disabled);
}

/****************************************************************************
 * Tree
 ****************************************************************************/

void FixtureSelection::slotItemDoubleClicked()
{
    if (m_tree->selectedItems().isEmpty() == false)
        accept();
}

void FixtureSelection::slotSelectionChanged()
{
    if (m_tree->selectedItems().size() > 0)
        m_buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    else
        m_buttonBox->setStandardButtons(QDialogButtonBox::Cancel);
}

void FixtureSelection::accept()
{
    m_selectedFixtures = m_tree->selectedFixtures();
    m_selectedHeads = m_tree->selectedHeads();

    QDialog::accept();
}
