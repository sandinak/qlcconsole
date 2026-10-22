/*
  Q Light Controller Plus
  chaserwalker.h

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

#ifndef CHASERWALKER_H
#define CHASERWALKER_H

#include <QPointer>
#include <QWidget>

class FunctionStack;
class QTreeWidgetItem;
class QTreeWidget;
class QToolButton;
class QPushButton;
class QLineEdit;
class QLabel;
class NoteEdit;

/** @addtogroup ui_functions
 * @{
 */

/**
 * A chaser in the Workbench (WORKFLOW_DESIGN.md §3.4, W2): walk it the way
 * the Chaser editor lists it, each step opened up.
 *
 * - ◀ prev · step N · next ▶;
 * - the step's header: fade in, hold, fade out, cue no. and note, edited in
 *   place (the Chaser editor's columns, its timing modes);
 * - "also in N" when the step's function plays elsewhere too, with *Make
 *   this step's own copy*;
 * - the step table: click a step to walk to it; double-click its note to
 *   type in it there; drag scenes onto it to add
 *   steps; + Step, ▲ ▼, Remove.
 *
 * It sits under the Workbench's header; the step's layers are the rows
 * under it. Every edit is one step on the shared history.
 */
class ChaserWalker final : public QWidget
{
    Q_OBJECT

public:
    ChaserWalker(FunctionStack *stack, QWidget *parent = nullptr);

    /** Show the chaser and the step walked to as they now are. */
    void refresh();

    QTreeWidget *stepTable() const { return m_table; }

signals:
    /** Open function @a functionId in the Workbench (↩ comes back). */
    void openRequested(quint32 functionId);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildTable();
    void commitTime(int which, const QString &text);
    /** Step @a step's (-1: the one walked to) cue number or note. */
    void commitText(int which, const QString &text, int step = -1);
    void addStepsPicked();
    void scheduleRefresh();

    QPointer<FunctionStack> m_stack;
    QToolButton *m_prev = nullptr;
    QToolButton *m_next = nullptr;
    QLabel *m_where = nullptr;
    QLineEdit *m_fadeIn = nullptr;
    QLineEdit *m_hold = nullptr;
    QLineEdit *m_fadeOut = nullptr;
    QLineEdit *m_cue = nullptr;
    NoteEdit *m_note = nullptr;
    QLabel *m_shared = nullptr;
    QPushButton *m_ownCopy = nullptr;
    QPushButton *m_openStep = nullptr;
    QTreeWidget *m_table = nullptr;
    QToolButton *m_addStep = nullptr;
    QToolButton *m_upStep = nullptr;
    QToolButton *m_downStep = nullptr;
    QToolButton *m_removeStep = nullptr;
    QLabel *m_dropHint = nullptr;
    bool m_refreshing = false;
    bool m_refreshQueued = false;
};

/** @} */

#endif
