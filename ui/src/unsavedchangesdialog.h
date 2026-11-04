/*
  Q Light Controller Plus
  unsavedchangesdialog.h

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

#ifndef UNSAVEDCHANGESDIALOG_H
#define UNSAVEDCHANGESDIALOG_H

#include <QDialog>

#include "changejournal.h"

class QTreeWidget;
class QPushButton;
class QLabel;
class Doc;

/**
 * The footer's "● Unsaved changes", opened: what changed since the last
 * save, newest first -- when, what sort of thing, and what was done, every
 * edit of one thing joined to the others ("1.0.2 Chorus -- Colour, Dimmer
 * (3 edits)"). It is the undo stacks' own steps (what Undo shows) plus a
 * short list of changes that go through no undo stack; the rest of what
 * marks the show modified is one "Other changes" line, counted only.
 *
 * A row takes you to its thing (activated: double click or Go to). The
 * dialog stays up beside the work and follows it; Save and Undo last act
 * on the show, Close puts it away.
 */
class UnsavedChangesDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit UnsavedChangesDialog(Doc *doc, QWidget *parent = nullptr);

    /** The row texts, newest first: "when|kind|title|what|edits" (tests). */
    QStringList rows() const;
    /** The line over the list ("3 changes"). */
    QString summaryText() const;
    /** Select row @a row and take its jump (tests; what a double click does). */
    void activateRow(int row);

signals:
    /** The row's thing, to be shown. */
    void jumpRequested(const ChangeJournal::Subject &subject);
    void saveRequested();

public slots:
    void refresh();

private slots:
    void slotGo();
    void slotUndoLast();
    void slotSelectionChanged();

private:
    Doc *m_doc;
    QLabel *m_summary;
    QTreeWidget *m_list;
    QPushButton *m_go;
    QPushButton *m_undo;
    QPushButton *m_save;
    QList<ChangeJournal::Entry> m_entries;
};

#endif
