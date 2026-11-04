/*
  Q Light Controller Plus - qlcconsole
  showreviewpanel.h

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

#ifndef SHOWREVIEWPANEL_H
#define SHOWREVIEWPANEL_H

#include <QDialog>

#include "qlcui_export.h"
#include <QPointer>
#include <QVariantList>
#include <functional>

#include "showreview.h"

class QTreeWidgetItem;
class QTreeWidget;
class QPushButton;
class QComboBox;
class QLineEdit;
class QLabel;
class Doc;

/**
 * The Review panel (docs/design/SHOW_REVIEW.md section 4.1, docs/features/
 * show-review.md): Show Review's findings by severity, each with Fix (the
 * place to fix it), Accept… (a note saved with the show), Reopen and Why?.
 * A window of its own, opened from the footer's Review chip or the Show
 * readiness dialog -- no tab, no menu item. It follows the show: a new run
 * or an acceptance redraws it.
 *
 * The footer's design-time readiness entries that no check reads yet are
 * listed too, under their catalog id, fixed with the footer's own action.
 */
class QLC_UI_EXPORT ShowReviewPanel final : public QDialog
{
    Q_OBJECT

public:
    explicit ShowReviewPanel(Doc *doc, QWidget *parent = nullptr);

    /** Where Fix goes: the app maps a finding's jump kind and ids to its
     *  views (Show Tree cue, Fixtures, Functions...). True when it went. */
    typedef std::function<bool(QWidget *parent, const QString &jump, const QVariantList &data)> Jumper;
    static void setJumper(const Jumper &jumper);
    /** Go where @a jump points (a finding's jump, or "status:<footer key>":
     *  the footer's own fix). True when it went. */
    static bool runJump(QWidget *parent, const QString &jump, const QVariantList &data);
    /** Ask why @a findings are on purpose (the Accept dialog): the note and
     *  who. False when cancelled. */
    static bool askAcceptNote(QWidget *parent, const QList<ReviewFinding> &findings,
                              QString *note, QString *by);

    /** The app's one panel: set once, opened from anywhere (the footer
     *  chip, the Show readiness dialog). */
    static void setShared(Doc *doc, QWidget *parent);
    static bool canOpenShared();
    static ShowReviewPanel *openShared();
    static ShowReviewPanel *shared();

    enum Filter { OpenOnly = 0, AcceptedOnly, All };
    void setFilter(Filter filter);
    void setSearch(const QString &text);

    QTreeWidget *tree() const { return m_tree; }
    QLabel *summary() const { return m_summary; }
    QPushButton *fixButton() const { return m_fix; }
    QPushButton *acceptButton() const { return m_accept; }
    QPushButton *reopenButton() const { return m_reopen; }
    /** PWR-11's "Apply This Cut": the suggested tape on the bottom, put on. */
    QPushButton *cutButton() const { return m_cut; }
    void applySuggestedCut();

    /** The findings selected (rows of findings, not footer rows). */
    QList<ReviewFinding> selectedFindings() const;
    /** Select the first row of check @a checkId; false when none is listed. */
    bool selectCheck(const QString &checkId);

    /** Accept the selection with @a note by @a by (the Accept dialog's
     *  work); false when nothing could be. */
    bool acceptSelected(const QString &note, const QString &by);
    bool reopenSelected();
    /** PWR-12: "I meant it" for the selected rows (@a meant), or "Not
     *  meant" for rows marked so; then re-check. False when none could be. */
    bool markMeantSelected(bool meant = true);
    /** Select the row of finding @a key (ReviewFinding::key()). */
    bool selectFinding(const QString &key);
    /** Whether every one of @a findings is a PWR-12 row "I meant it"
     *  answers (@a meant) or takes back (false). */
    static bool allMeantRows(const QList<ReviewFinding> &findings, bool meant);
    /** Fix the selected row: its jump, or a footer row's action. */
    bool fixSelected();

public slots:
    /** Run every check again, then redraw. */
    void recheck();
    /** Redraw from the last result. */
    void rebuild();

private slots:
    void slotAccept();
    void slotWhy();
    void slotTurnOff();
    void slotContextMenu(const QPoint &pos);
    void updateButtons();

private:
    Doc *m_doc;
    QLabel *m_summary;
    QComboBox *m_filter;
    QLineEdit *m_search;
    QTreeWidget *m_tree;
    QPushButton *m_fix;
    QPushButton *m_accept;
    QPushButton *m_reopen;
    QPushButton *m_cut;
    QPushButton *m_why;
    QPushButton *m_off;
    QList<ReviewFinding> m_findings;

    static Jumper s_jumper;
    static QPointer<ShowReviewPanel> s_shared;
    static Doc *s_doc;
    static QPointer<QWidget> s_parent;
};

#endif
