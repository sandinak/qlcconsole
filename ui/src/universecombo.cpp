/*
  Q Light Controller Plus
  universecombo.cpp

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

#include <QCollator>
#include <algorithm>

#include "universecombo.h"
#include "inputoutputmap.h"
#include "doc.h"

UniverseCombo::UniverseCombo(Doc *doc, int newOnes, QWidget *parent)
    : QComboBox(parent)
    , m_existing(doc->inputOutputMap()->universesCount())
{
    struct Entry { QString name; quint32 index; };
    QList<Entry> entries;
    for (quint32 i = 0; i < m_existing; i++)
    {
        QString name = doc->inputOutputMap()->getUniverseNameByIndex(int(i));
        if (name.isEmpty())
            name = tr("Universe %1").arg(i + 1);
        entries << Entry{ name, i };
    }
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::stable_sort(entries.begin(), entries.end(), [&collator](const Entry &a, const Entry &b) {
        return collator.compare(a.name, b.name) < 0;
    });

    foreach (const Entry &e, entries)
    {
        addItem(e.name, e.index);
        setItemData(count() - 1, tr("Universe %1").arg(e.index + 1), Qt::ToolTipRole);
    }
    for (int n = 0; n < newOnes; n++)
        addNew(m_existing + quint32(n));

    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        emit universeChanged(universe());
    });
}

quint32 UniverseCombo::universe() const
{
    return currentData().toUInt();
}

void UniverseCombo::setUniverse(quint32 universe)
{
    int at = findData(universe);
    if (at < 0)
    {
        // Past the show's own: every new one up to it, in order.
        for (quint32 u = m_existing; u <= universe; u++)
            if (findData(u) < 0)
                addNew(u);
        at = findData(universe);
    }
    setCurrentIndex(at);
}

void UniverseCombo::addNew(quint32 universe)
{
    addItem(tr("New universe %1").arg(universe + 1), universe);
    setItemData(count() - 1, tr("Not in the show yet: patching here adds it"), Qt::ToolTipRole);
}
