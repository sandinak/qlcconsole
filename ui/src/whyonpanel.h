/*
  Q Light Controller Plus
  whyonpanel.h

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

#ifndef WHYONPANEL_H
#define WHYONPANEL_H

#include <QFrame>
#include <QKeySequence>

#include "qlcui_export.h"
#include <QPointer>
#include <functional>

#include "whyon.h"

class QListWidgetItem;
class QListWidget;
class QPushButton;
class QComboBox;
class QCheckBox;
class QTimer;
class QLabel;
class Doc;

/**
 * "Why is this on?" (0.5): a small window, opened from a fixture's or a
 * channel's right-click (Rig Overview, the plot, the Fixture Manager, a
 * Simple Desk channel), listing what sets the light now -- WhyOn's answer,
 * top-down, in crew words. Each line goes to what it names (the cue and
 * layer in the Show Tree, the function, the Simple Desk, the VC widget).
 *
 * One window, reused: asking about another light moves it there. While it
 * is shown it asks again four times a second on the UI thread; hidden, it
 * asks nothing (no timer, nothing in the engine).
 *
 * A dark light gets "Why it isn't on" first (WhyOn::Answer::whyNot), then
 * what sets it. A line can offer an action in Design mode: Unpark, Release
 * (a Simple Desk channel), Let go of a move in black. ⌘I opens it on the
 * light last selected in any view; the Show Tree's layer rows and the
 * Workbench's fixture and group rows have it in their right-click menus.
 */
class QLC_UI_EXPORT WhyOnPanel final : public QFrame
{
    Q_OBJECT

public:
    /** Open (or move) the window on channel @a channel of fixture
     *  @a fixtureId (invalid: its intensity), near @a globalPos. */
    static WhyOnPanel *showFor(Doc *doc, quint32 fixtureId, quint32 channel = 0xFFFFFFFF,
                               QWidget *parent = nullptr, const QPoint &globalPos = QPoint());
    /** The same for a bare address (the Simple Desk in universe mode) */
    static WhyOnPanel *showForAddress(Doc *doc, quint32 universe, int address,
                                      QWidget *parent = nullptr, const QPoint &globalPos = QPoint());
    /** The window, if one is open (tests) */
    static WhyOnPanel *current();

    /** The views note what is selected (the plot, the Rig Overview, the
     *  Fixture Manager, the Workbench); ⌘I opens the window on it */
    static void noteSelection(const QList<quint32> &fixtureIds);
    /** ⌘I: open the window on the last selected light; none selected, on a
     *  hint saying to select one */
    static WhyOnPanel *showForSelection(Doc *doc, QWidget *parent = nullptr);
    /** ⌘I, the same everywhere */
    static QKeySequence shortcut();

    /** What a line offers to do from the window, in Design mode */
    enum class Action { NoAction, Unpark, Release, LetGoMark };
    static Action actionFor(const WhyOn::Line &line);
    static QString actionName(Action action);
    /** The line list row @a row's action, if any; false: none, or Operate */
    bool runAction(int row);

    /** The menu entry's words, the same everywhere */
    static QString actionText();

    /** Wording: Simple (the Teaching hook) by default in a Teaching show,
     *  Crew otherwise; the "Plain words" box switches it. */
    WhyOn::Wording wording() const;
    void setWording(WhyOn::Wording wording);

    /** Ask again now */
    void refresh();
    /** What it shows (tests) */
    const WhyOn::Answer &answer() const { return m_answer; }
    QListWidget *lines() const { return m_list; }
    /** Go to what line @a row names (a click does this) */
    void activate(int row);
    /** The line list row @a row is (headers: nullptr) */
    const WhyOn::Line *lineAt(int row) const;
    QPushButton *actionButton() const { return m_action; }

    /** In place of the real jumps (tests): given the line clicked */
    typedef std::function<void(const WhyOn::Line &)> Jump;
    static void setJumpOverride(const Jump &jump);

    ~WhyOnPanel() override;

protected:
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;

private:
    WhyOnPanel(Doc *doc, QWidget *parent);
    void setTarget(quint32 fixtureId, quint32 channel, quint32 universe, int address);
    void fillChannels();
    void jump(const WhyOn::Line &line);
    void place(const QPoint &globalPos);
    void updateAction();

    Doc *m_doc;
    quint32 m_fixtureId = 0xFFFFFFFF;
    quint32 m_channel = 0xFFFFFFFF;
    quint32 m_universe = 0xFFFFFFFF;
    int m_address = -1;
    WhyOn::Answer m_answer;
    /** The why-not lines, then the lines: what each list row points to */
    QList<WhyOn::Line> m_rows;
    QLabel *m_subject;
    QComboBox *m_channels;
    QLabel *m_headline;
    QListWidget *m_list;
    QCheckBox *m_plain;
    QPushButton *m_copy;
    QPushButton *m_action;
    QTimer *m_timer;
};

#endif
