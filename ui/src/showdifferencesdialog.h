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
    explicit ShowDifferencesDialog(Doc *doc, QWidget *parent = nullptr);

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

private slots:
    void rebuild();
    void updateButtons();

private:
    QStringList selectedKeys() const;

    Doc *m_doc;
    QTreeWidget *m_tree;
    QLabel *m_detail;
    QPushButton *m_use;
    QPushButton *m_useAll;
    QPushButton *m_keep;
    QPushButton *m_install;
};

#endif
