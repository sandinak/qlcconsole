/*
  Q Light Controller Plus
  treekeepview.h

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

#ifndef TREEKEEPVIEW_H
#define TREEKEEPVIEW_H

#include <QAbstractItemView>
#include <QTreeWidget>
#include <QTreeWidgetItem>

/** Keeps a tree on what is selected when its search / filter text changes
 *  or is cleared: call it after the rows are shown and hidden again. The
 *  selected rows that are still shown get their parents opened, and the
 *  current one (else the first selected) is scrolled to the middle of the
 *  view, so clearing a search leaves the person on the row they found.
 *  A selected row the filter hides is left to the tree, as before. */
namespace TreeKeepView
{
    inline bool shown(const QTreeWidgetItem *item)
    {
        for (; item != nullptr; item = item->parent())
            if (item->isHidden())
                return false;
        return true;
    }

    inline void reveal(QTreeWidget *tree)
    {
        if (tree == nullptr)
            return;
        const QList<QTreeWidgetItem *> selected = tree->selectedItems();
        QTreeWidgetItem *target = nullptr;
        for (QTreeWidgetItem *it : selected)
        {
            if (!shown(it))
                continue;
            for (QTreeWidgetItem *p = it->parent(); p != nullptr; p = p->parent())
                if (!p->isExpanded())
                    p->setExpanded(true);
            if (target == nullptr)
                target = it;
        }
        QTreeWidgetItem *cur = tree->currentItem();
        if (cur != nullptr && cur->isSelected() && shown(cur))
            target = cur;
        if (target != nullptr)
            tree->scrollTo(tree->indexFromItem(target), QAbstractItemView::PositionAtCenter);
    }
}

#endif
