/*
  Q Light Controller Plus
  cuerunner.h

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

#ifndef CUERUNNER_H
#define CUERUNNER_H

#include <QWidget>

#include "showcontrol.h"

class QTreeWidgetItem;
class QTreeWidget;
class QToolButton;
class QComboBox;
class QLabel;
class QTimer;
class Chaser;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The cue runner (SHOW_TREE_DESIGN.md §6B, D26, D27; P7): one component
 * with two homes -- the full-screen Run tab and the Cue Runner Virtual
 * Console widget (compact or list size). It follows a cue list of the Show
 * Tree, or the current production (the show's main cue list), and shows the
 * same state everywhere because it keeps none of its own: what plays is the
 * list's chaser, what is loaded is ShowControl's. Every button goes through
 * ShowControl::execute(), the same path the footer GO, ⌘↩ and QLab / OSC /
 * MSC take.
 *
 * Click a cue = LOAD (it becomes STANDBY); double-click = jump to it now (a
 * sub-cue: its cue first). GO plays the loaded cue, else the next; into a
 * cue with sub-cues it plays the first and holds; Sub GO the next sub-cue;
 * Back the previous main cue; Sub Back the previous sub-cue.
 */
class CueRunner final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(CueRunner)

public:
    enum Size
    {
        Full,       //!< the Run tab: big targets, the list, every button
        Compact,    //!< on now, next, big GO + Sub GO
        List        //!< the whole runner, VC-sized
    };

    CueRunner(Doc *doc, Size size, QWidget *parent = nullptr);
    ~CueRunner();

    Size runnerSize() const { return m_size; }
    void setRunnerSize(Size size);

    /** The Show Tree cue list it follows (invalid: the current production,
     *  i.e. the show's main cue list -- a plain chaser too). */
    quint32 follow() const { return m_follow; }
    void setFollow(quint32 listId);

    /** The chaser it runs now (null: none). */
    Chaser *chaser() const;
    /** The tree list that chaser plays (invalid: a plain chaser, or none). */
    quint32 listId() const;

    /** What the lines say now, for the tests and the VC widget's caption. */
    QString headerText() const;
    QString nowText() const;
    QString nextText() const;
    QString subNextText() const;
    QString timecodeText() const;
    QString modeText() const;
    QTreeWidget *cueTree() const { return m_tree; }

    static QString sizeToString(Size size);
    static Size stringToSize(const QString &str);

public slots:
    void go();
    void back();
    void subGo();
    void subBack();
    void pauseResume();
    void stop();
    /** Click: LOAD the cue (or sub-cue) numbered @a cue. */
    void load(const QString &cue);
    /** Double-click: jump to it now. */
    void jump(const QString &cue);
    /** Bring everything up to date (also on a timer while shown). */
    void refresh();

signals:
    /** It follows another list (the VC widget saves it). */
    void followChanged();

protected:
    void showEvent(QShowEvent *ev) override;
    void hideEvent(QHideEvent *ev) override;

private:
    bool execute(ShowControl::Command command, const QString &cue = QString());
    void buildLayout();
    void rebuildTree();
    void updateStates();
    void fillFollowBox();
    /** "2.1" for the row's cue (or sub-cue). */
    QString rowCue(const QTreeWidgetItem *item) const;

    Doc *m_doc;
    Size m_size;
    quint32 m_follow;
    QTimer *m_timer;
    /** What the tree was built from (list content and chaser), to rebuild
     *  only when it changes. */
    QString m_builtFor;
    bool m_treeDirty = true;

    QLabel *m_header = nullptr;
    QComboBox *m_followBox = nullptr;
    QLabel *m_timecode = nullptr;
    QLabel *m_mode = nullptr;
    QTreeWidget *m_tree = nullptr;
    QLabel *m_now = nullptr;
    QLabel *m_next = nullptr;
    QLabel *m_subNext = nullptr;
    QToolButton *m_go = nullptr;
    QToolButton *m_subGo = nullptr;
    QToolButton *m_subBack = nullptr;
    QToolButton *m_back = nullptr;
    QToolButton *m_pause = nullptr;
    QToolButton *m_stop = nullptr;
};

/** @} */

#endif
