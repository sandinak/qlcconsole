/*
  Q Light Controller Plus - qlcconsole
  showstatusdialog.h

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

#ifndef SHOWSTATUSDIALOG_H
#define SHOWSTATUSDIALOG_H

#include <QDialog>
#include <QVariant>

class QTreeWidget;
class QPushButton;

/**
 * The footer's "Not ready" in full: every ShowStatus entry, its detail, and
 * each affected thing as a row -- and, where the entry's source can fix it
 * (ShowStatus::setAction()), the fix. Select rows (shift-click a range,
 * Cmd/Ctrl-click to add one, an entry's own row for all of its items) and
 * press the action button, or double-click one row. The list follows
 * ShowStatus as problems are fixed, and closes itself when none are left.
 */
class ShowStatusDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShowStatusDialog(QWidget *parent = nullptr);

    /** Rebuild from ShowStatus, keeping what was selected. */
    void rebuild();

    /** The entry key the selection belongs to, and its items' data --
     *  empty when nothing actionable is selected or it spans entries. */
    QString selectionKey(QVariantList *items = nullptr) const;

    /** What action button @p index does, on the selection. */
    void runAction(int index = 0);

    QTreeWidget *tree() const { return m_tree; }
    /** The first fix's button. */
    QPushButton *actionButton() const { return m_actions.value(0); }
    /** One per fix the selection's entry offers. */
    QList<QPushButton *> actionButtons() const { return m_actions; }

private:
    void updateActionButton();

    QTreeWidget *m_tree;
    class QDialogButtonBox *m_box;
    QList<QPushButton *> m_actions;
};

#endif
