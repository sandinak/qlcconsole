/*
  Q Light Controller Plus - qlcconsole
  definitionupdatesdialog.h

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

#ifndef DEFINITIONUPDATESDIALOG_H
#define DEFINITIONUPDATESDIALOG_H

#include <QDialog>
#include <QStringList>

#include "fixturedefupdates.h"

class QPlainTextEdit;
class QTreeWidgetItem;
class QTreeWidget;
class QPushButton;
class QComboBox;
class QCheckBox;
class QLabel;
class Doc;

/**
 * Fixtures → Definition Updates… (and the footer's "N of your fixture
 * definitions are older…"): the user folder's definitions that are older
 * than the current version of the same fixture, in two lists -- the ones
 * the open show uses, and the rest -- with what an update would change, and
 * Update from Current / Keep Mine / Ignore This Version / Undo Last Update.
 * Nothing is written without a backup (FixtureDefUpdates).
 */
class DefinitionUpdatesDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit DefinitionUpdatesDialog(Doc *doc, QWidget *parent = nullptr);

    /** Tests: answer every question with @p yes instead of asking, and say
     *  nothing afterwards; -1 asks again. */
    static void setAutoAnswer(int yes);

    /** The rows' files, in order (tests). */
    QStringList files() const;
    /** Select the rows of @p files (tests). */
    void select(const QStringList &files);
    /** The detail shown for the selected row (tests). */
    QString detailText() const;
    /** The last update's or undo's report (tests). */
    QStringList lastReport() const { return m_report; }

public slots:
    void rescan();
    void updateSelected();
    void updateAll();
    void keepMine();
    void ignoreVersion();
    void undoLast();

private slots:
    void updateButtons();

private:
    QList<FixtureDefUpdates::Candidate> selectedCandidates() const;
    void runUpdate(const QList<FixtureDefUpdates::Candidate> &list);
    bool ask(const QString &title, const QString &text);
    void tell(const QString &title, const QStringList &lines);

    Doc *m_doc;
    QList<FixtureDefUpdates::Candidate> m_candidates;
    QStringList m_report;
    QLabel *m_summary;
    QTreeWidget *m_tree;
    QPlainTextEdit *m_detail;
    QCheckBox *m_customised;
    QComboBox *m_linkMode;
    QPushButton *m_update;
    QPushButton *m_updateAll;
    QPushButton *m_keep;
    QPushButton *m_ignore;
    QPushButton *m_undo;
};

#endif
