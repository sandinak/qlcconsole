/*
  Q Light Controller - Fixture Definition Editor
  fixturebrowser.cpp

  Copyright (c) Branson Matheson

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

#include <QTreeWidget>
#include <QVBoxLayout>
#include <QStringList>
#include <QLineEdit>
#include <QVariant>

#include "fixturebrowser.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"

#define KColumnName 0
#define PROP_MANUFACTURER (Qt::UserRole)
#define PROP_MODEL        (Qt::UserRole + 1)

FixtureBrowser::FixtureBrowser(QWidget* parent)
    : QWidget(parent)
    , m_cache(new QLCFixtureDefCache)
    , m_search(NULL)
    , m_tree(NULL)
{
    // Same split this app's own "working directory" default already uses
    // (App::App(), app.cpp) -- full parse for the (small) user directory,
    // the lighter manufacturer/model-only scan for the (large) system
    // library. Either way definitionSourceFile() ends up populated, which
    // is all fillTree()/slotItemActivated() actually need.
    m_cache->load(QLCFixtureDefCache::userDefinitionDirectory());
    m_cache->loadMap(QLCFixtureDefCache::systemDefinitionDirectory());

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search manufacturer or model..."));
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);
    connect(m_search, SIGNAL(textChanged(QString)),
            this, SLOT(slotSearchChanged(QString)));

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    layout->addWidget(m_tree);
    connect(m_tree, SIGNAL(itemActivated(QTreeWidgetItem*,int)),
            this, SLOT(slotItemActivated(QTreeWidgetItem*,int)));

    fillTree();
}

FixtureBrowser::~FixtureBrowser()
{
    delete m_cache;
}

void FixtureBrowser::slotSearchChanged(const QString&)
{
    fillTree();
}

void FixtureBrowser::fillTree()
{
    m_tree->clear();

    const QString filter = m_search->text().toLower();

    foreach (const QString& manuf, m_cache->manufacturers())
    {
        QTreeWidgetItem* parent = NULL;

        foreach (const QString& model, m_cache->models(manuf))
        {
            if (filter.isEmpty() == false &&
                manuf.toLower().contains(filter) == false &&
                model.toLower().contains(filter) == false)
            {
                continue;
            }

            if (parent == NULL)
            {
                parent = new QTreeWidgetItem(m_tree);
                parent->setText(KColumnName, manuf);
                // A filter match narrow enough to be worth expanding for —
                // an unfiltered full list stays collapsed so it isn't a wall
                // of every manufacturer's models at once.
                if (filter.isEmpty() == false)
                    parent->setExpanded(true);
            }

            QTreeWidgetItem* child = new QTreeWidgetItem(parent);
            child->setText(KColumnName, model);
            child->setData(KColumnName, PROP_MANUFACTURER, manuf);
            child->setData(KColumnName, PROP_MODEL, model);
        }
    }

    m_tree->sortItems(KColumnName, Qt::AscendingOrder);
}

void FixtureBrowser::slotItemActivated(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)

    if (item == NULL)
        return;

    const QVariant manufVar = item->data(KColumnName, PROP_MANUFACTURER);
    const QVariant modelVar = item->data(KColumnName, PROP_MODEL);
    if (manufVar.isValid() == false || modelVar.isValid() == false)
        return; // A manufacturer row, not a model leaf -- nothing to open.

    QLCFixtureDef* def = m_cache->fixtureDef(manufVar.toString(), modelVar.toString());
    if (def == NULL || def->definitionSourceFile().isEmpty())
        return;

    emit definitionActivated(def->definitionSourceFile());
}
