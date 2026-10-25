/*
  Q Light Controller Plus - qlcconsole
  showdifferencesdialog.h

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

#ifndef SHOWDIFFERENCESDIALOG_H
#define SHOWDIFFERENCESDIALOG_H

#include <QDialog>
#include <QStringList>

class QTreeWidget;
class QPushButton;
class QLabel;
class Doc;

/**
 * File → Show Definitions… lists everything the show carries, each with how
 * it compares with this computer's (@p everything); the footer's Review…
 * lists only what differs and is not dealt with.
 *
 * Review… for the footer's "N items in this show differ from this
 * computer's" (SHOW_EMBEDDING_DESIGN.md §4): one row per difference -- what
 * it is, which side looks newer, and whether switching to this computer's
 * copy is safe for what the show uses -- with the three choices: Use this
 * computer's, Keep the show's, Install the show's.
 */
class ShowDifferencesDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ShowDifferencesDialog(Doc *doc, QWidget *parent = nullptr, bool everything = false);

    /** Tests: answer every question with @p yes instead of asking, and say
     *  nothing afterwards; -1 asks again. */
    static void setAutoAnswer(int yes);
    /** The detail shown for the selected row (tests). */
    QString detailText() const;

    /** The rows' keys, in order (tests). */
    QStringList keys() const;
    /** Select the rows of @p keys (tests). */
    void select(const QStringList &keys);

public slots:
    /** Switch the selected rows to this computer's copy (asks first for one
     *  that is not compatible). */
    void useComputers();
    /** Switch every compatible row, without asking. */
    void useComputersForAllCompatible();
    /** Stop warning about the selected rows. */
    void keepShows();
    /** Write the show's copy over this computer's for the selected rows
     *  (asks first). */
    void installShows();
    /** Undo the last Use This Computer's (one or many rows), in one step. */
    void undoChoices();

private slots:
    void rebuild();
    void updateButtons();

private:
    QStringList selectedKeys() const;
    bool ask(const QString &title, const QString &text);
    /** What a bulk action did, when something failed: @p done, and each
     *  error, in a window that scrolls (BulkResultDialog). */
    void tell(const QString &title, int done, const QStringList &failed);

    Doc *m_doc;
    bool m_everything;
    QTreeWidget *m_tree;
    QLabel *m_detail;
    QPushButton *m_undo;
    QPushButton *m_use;
    QPushButton *m_useAll;
    QPushButton *m_keep;
    QPushButton *m_install;
};

#endif
