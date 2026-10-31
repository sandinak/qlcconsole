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
#include "qlcui_export.h"

class QTreeWidgetItem;
class CueRunnerHandle;
class QProgressBar;
class QTreeWidget;
class QToolButton;
class QPushButton;
class QAction;
class QComboBox;
class QLabel;
class QTimer;
class Chaser;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The cue runner (docs/design/SHOW_TREE_DESIGN.md §6B, D26, D27; P7): one component
 * with two homes -- the full-screen Run tab and the Cue Runner Virtual
 * Console widget (compact or list size). It follows a cue list of the Show
 * Tree, or the current production (the show's main cue list), and shows the
 * same state everywhere because it keeps none of its own: what plays is the
 * list's chaser, what is loaded is ShowControl's. Every button goes through
 * ShowControl::run() (docs/design/RUN_COMMANDS.md), the same path ⌘↩, the
 * pad, the Control Map and QLab / OSC / MSC take.
 *
 * Click a cue = LOAD (it becomes STANDBY); double-click = jump to it now (a
 * sub-cue: its cue first). GO plays the loaded cue, else the next; into a
 * cue with sub-cues it plays the first and holds; Sub GO the next sub-cue;
 * Back the previous main cue; Sub Back the previous sub-cue.
 *
 * The VC sizes (compact, list) are an info area over a button area: the
 * info takes what the widget grows by (a larger cue name, more upcoming
 * cues), the buttons keep the height set for them. Two handles size them
 * -- between the info and the buttons, and between GO and Sub GO -- while
 * unlocked (the VC widget unlocks them in Design mode only).
 */
class QLC_UI_EXPORT CueRunner final : public QWidget
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

    /** "Focus calls: waiting for the controller" while a focus-call cue is
     *  armed (running: that it runs); empty otherwise. */
    QString focusArmedText() const;

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
    /** The colour a line's text is drawn in now ("now", "next", "subnext",
     *  "upcoming0".. ): the cue's text colour made readable, else the
     *  theme's. For the tests. */
    QColor textColour(const QString &part) const;

    /** Q62: the Run tab with no main cue list says so and offers to
     *  upgrade the show: its button triggers @a action (File ▸ Upgrade
     *  This Show…, set by the App; a test's own). Null: no button. */
    static void setUpgradeAction(QAction *action);
    static QAction *upgradeAction();
    /** Carry out @a request on @a doc by the one run path
     *  (ShowControl::run(), docs/design/RUN_COMMANDS.md): in the app,
     *  App::runCommand() -- the guards, the preview hold (RC8), then
     *  ShowControl::execute(). */
    static bool runCommand(Doc *doc, const ShowControl::Request &request, ShowControl::Route route);
    /** The Run tab's empty state (no main cue list) shown now. */
    bool isEmptyStateShown() const;
    QPushButton *upgradeButton() const { return m_upgrade; }

    static QString sizeToString(Size size);
    static Size stringToSize(const QString &str);

    /*********************************************************************
     * The VC sizes' layout (compact, list; the Run tab ignores these)
     *********************************************************************/
    /** How many cues after NEXT the compact runner lists, room allowing
     *  (0..kMaxUpcoming). */
    int upcomingCount() const { return m_upcomingCount; }
    void setUpcomingCount(int count);
    /** The timecode chip (shown only while there is a source). */
    bool showTimecode() const { return m_showTimecode; }
    void setShowTimecode(bool show);
    /** The small secondary buttons: Back and Stop (list: Sub Back and
     *  Pause too). */
    bool showSecondary() const { return m_showSecondary; }
    void setShowSecondary(bool show);
    bool showSubGo() const { return m_showSubGo; }
    void setShowSubGo(bool show);
    /** The button area's height in pixels (it shrinks below this only
     *  when the widget is too small for it and the info). */
    int buttonHeight() const { return m_buttonHeight; }
    void setButtonHeight(int px);
    /** GO's share of the GO | Sub GO row, in percent. */
    int goShare() const { return m_goShare; }
    void setGoShare(int percent);
    /** Locked: the handles are hidden and do nothing. */
    bool handlesLocked() const { return m_handlesLocked; }
    void setHandlesLocked(bool locked);
    /** Not interactive: the buttons and the list let the mouse through
     *  (to the VC widget under them, in Design mode). */
    void setControlsInteractive(bool interactive);
    bool controlsInteractive() const { return m_interactive; }

    /** The parts, for the VC widget and the tests (null in the Run tab). */
    QWidget *infoArea() const { return m_info; }
    QWidget *buttonArea() const { return m_buttons; }
    QWidget *infoHandle() const;
    QWidget *goHandle() const;
    QToolButton *goButton() const { return m_go; }
    QToolButton *subGoButton() const { return m_subGo; }
    /** The upcoming lines shown now. */
    QStringList upcomingTexts() const;

    static const int kMaxUpcoming = 5;
    static const int kMinButtonHeight = 32;
    static const int kMaxButtonHeight = 240;
    static const int kMinGoShare = 40;
    static const int kMaxGoShare = 90;

    /*********************************************************************
     * What each action can do now (the VC widget's feedback)
     *********************************************************************/
    bool canGo() const { return m_flags & FlagGo; }
    bool canSubGo() const { return m_flags & FlagSubGo; }
    bool canBack() const { return m_flags & FlagBack; }
    bool canSubBack() const { return m_flags & FlagSubBack; }
    bool isPaused() const { return m_flags & FlagPaused; }
    bool canStop() const { return m_flags & FlagStop; }
    bool canLoadNext() const { return m_flags & FlagLoadNext; }
    bool canLoadPrevious() const { return m_flags & FlagLoadPrevious; }

public slots:
    void go();
    void back();
    void subGo();
    void subBack();
    void pauseResume();
    void stop();
    /** LOAD the main cue after (before) the one on standby. */
    void loadNext();
    void loadPrevious();
    /** Click: LOAD the cue (or sub-cue) numbered @a cue. */
    void load(const QString &cue);
    /** Double-click: jump to it now. */
    void jump(const QString &cue);
    /** Bring everything up to date (also on a timer while shown). */
    void refresh();

signals:
    /** It follows another list (the VC widget saves it). */
    void followChanged();
    /** A handle was dragged (the VC widget saves the new sizes). */
    void layoutEdited();
    /** What the actions can do changed (canGo() ...). */
    void statesChanged();

protected:
    void showEvent(QShowEvent *ev) override;
    void hideEvent(QHideEvent *ev) override;
    void resizeEvent(QResizeEvent *ev) override;

private:
    enum Flag
    {
        FlagGo = 1 << 0, FlagSubGo = 1 << 1, FlagBack = 1 << 2, FlagSubBack = 1 << 3,
        FlagPaused = 1 << 4, FlagStop = 1 << 5, FlagLoadNext = 1 << 6, FlagLoadPrevious = 1 << 7
    };

    bool execute(ShowControl::Command command, const QString &cue = QString());
    void buildLayout();
    void buildFullLayout();
    QToolButton *makeButton(const QString &text, const QString &tip, const char *name);
    void makeButtons();
    void makeFollowBox();
    void makeTree();
    void fitStateColumn();
    void buildPanelLayout();
    /** Sizes the button area, the GO row and the info's fonts and lines to
     *  the room there is (VC sizes). */
    void layoutPanel();
    void applyVisibility();
    void applyInteractive();
    /** The cue number LOAD takes for main cue @a step. */
    QString stepCue(int step) const;
    void rebuildTree();
    void updateStates();
    void fillFollowBox();
    /** "2.1" for the row's cue (or sub-cue). */
    QString rowCue(const QTreeWidgetItem *item) const;
    /** Draw @a label in cue @a cueId's text colour (invalid, or none: the
     *  theme's; @a dim: the theme's dimmed). */
    void tint(QLabel *label, quint32 cueId, bool dim);

    Doc *m_doc;
    Size m_size;
    /** Design: the cue clicked in the list plays (the latest action wins,
     *  docs/design/LIVE_BLIND.md) until GO, another pick or leaving. */
    void pickRowCue(const QString &cue);
    quint32 m_pickedCue = 0xFFFFFFFF;
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
    /** Q62: no main cue list -- what that means, and Upgrade (Run tab). */
    QWidget *m_empty = nullptr;
    QPushButton *m_upgrade = nullptr;

    // The VC sizes.
    QWidget *m_info = nullptr;
    QWidget *m_buttons = nullptr;
    QWidget *m_secondary = nullptr;
    QProgressBar *m_fade = nullptr;
    CueRunnerHandle *m_infoHandle = nullptr;
    CueRunnerHandle *m_goHandle = nullptr;
    QList<QLabel *> m_upcoming;
    QStringList m_upcomingTexts;
    int m_upcomingCount = 2;
    bool m_showTimecode = true;
    bool m_showSecondary = true;
    bool m_showSubGo = true;
    int m_buttonHeight = 64;
    int m_goShare = 70;
    bool m_handlesLocked = false;
    bool m_interactive = true;
    int m_flags = 0;
    /** A handle's drag: the height or share it started from. */
    int m_dragFrom = 0;
private:
    void refreshFocusArmed();
    /** The Venue focus button: greyed, with why, with nothing to focus. */
    void refreshFocusCalls();
    QToolButton *m_focusCallsBtn = nullptr;
    QWidget *m_focusArmed = nullptr;
    QLabel *m_focusArmedLabel = nullptr;
    /** Scoped hits (Q157): the VC buttons whose armed hit overrides a
     *  wider one now -- "HIT 1: Strobe ↳ overrides Blackout" (Run tab). */
    void refreshHitsNow();
    QLabel *m_hitsNow = nullptr;
    QToolButton *m_focusStart = nullptr;
};

/** @} */

#endif
