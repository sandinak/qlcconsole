/*
  Q Light Controller Plus
  shownoteslist.h

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

#ifndef SHOWNOTESLIST_H
#define SHOWNOTESLIST_H

#include <functional>
#include <QDialog>

#include "showtreenotes.h"

class ShowTreeController;
class QTreeWidget;
class QComboBox;
class QLineEdit;
class QLabel;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Every note in the show (Q212, "All notes…" on the notes pane): a table of
 * them, newest first, filtered by open / done, by item (a cue, a list...) and
 * by words; a double-click (or Go to) selects the item in the Show Tree.
 * Print and Export (Markdown or PDF) make the list to work from after
 * rehearsal. Not modal: it stays open beside the tree.
 */
class ShowNotesList final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(ShowNotesList)

public:
    ShowNotesList(Doc *doc, QWidget *parent = nullptr);

    enum StateFilter { AllNotes = 0, OpenNotes, DoneNotes };
    void setStateFilter(StateFilter filter);
    /** Only the notes on @a kind @a target (an invalid target: every item). */
    void setItemFilter(ShowTree::NoteKind kind, quint32 target);
    void setTextFilter(const QString &text);

    /** The notes listed now, in order (tests). */
    QList<quint32> listedNotes() const;
    QTreeWidget *table() const { return m_table; }

    /** Go to the note in row @a row: its item selected in the tree. */
    bool jumpTo(int row);
    /** What a jump does (ShowTreeView: select the item). */
    std::function<void(const ShowTree::Note &note)> jump;

    /** The listed notes as Markdown / as HTML (for print and PDF). */
    QString toMarkdown() const;
    QString toHtml() const;
    /** Write the list to @a path: Markdown for .md, else PDF. */
    bool exportTo(const QString &path, QString *error = nullptr);

public slots:
    void refresh();

private slots:
    void slotExport();
    void slotPrint();

private:
    ShowTreeController *controller() const;
    QList<ShowTree::Note> filtered() const;

private:
    Doc *m_doc;
    QComboBox *m_state = nullptr;
    QComboBox *m_item = nullptr;
    QLineEdit *m_text = nullptr;
    QTreeWidget *m_table = nullptr;
    QLabel *m_count = nullptr;
    bool m_filling = false;
};

/** @} */

#endif
