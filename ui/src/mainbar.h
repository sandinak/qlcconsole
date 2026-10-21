/*
  Q Light Controller Plus
  mainbar.h

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

#ifndef MAINBAR_H
#define MAINBAR_H

#include <QPointer>
#include <QToolBar>
#include <QWidget>

class QHBoxLayout;
class QToolButton;
class QLayout;
class QAction;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The show controls: Stop all, Blackout, Blind and the Design | Operate
 * switch. Small icon-and-word buttons, so they fit beside a tab's own tools
 * in the one top bar, or in the status bar. Blackout lights red and Blind
 * blue while they are on, so their state reads at this size.
 *
 * Built on the App's own actions: the same shortcuts, the same slots, the
 * same Control menu entries. Only the buttons are new.
 */
class ShowControls final : public QToolBar
{
    Q_OBJECT

public:
    ShowControls(Doc *doc, QAction *stopAll, QAction *blackout, QAction *blind,
                 QAction *mode, QWidget *parent = nullptr);
    ~ShowControls();

    QToolButton *stopAllButton() const { return m_stopAll; }
    QToolButton *blackoutButton() const { return m_blackout; }
    QToolButton *blindButton() const { return m_blind; }
    QToolButton *designButton() const { return m_design; }
    QToolButton *operateButton() const { return m_operate; }
    /** The Design | Operate switch: a widget of its own, placed beside this
     *  bar (by the top bar, or the status bar) */
    QWidget *modeSwitch() const { return m_modeSwitch; }

    /** App::TabLabelMode: icons and words, icons only or words only.
     *  The Design | Operate switch is words either way. */
    void setLabelMode(int mode);

    /** For the status bar: smaller icons, and no words beside them (the
     *  bar is full of chips already) unless the mode is words only */
    void setInStatusBar(bool on);
    bool isInStatusBar() const { return m_inStatusBar; }

private slots:
    /** The switch follows the mode, whoever changed it (and stays put when
     *  a change is refused) */
    void syncMode();
    void slotModeClicked();

private:
    QToolButton *buttonFor(QAction *action, const char *name);
    void applyStyle();

    Doc *m_doc;
    QAction *m_modeAction;
    QToolButton *m_stopAll;
    QToolButton *m_blackout;
    QToolButton *m_blind;
    QToolButton *m_design;
    QToolButton *m_operate;
    QPointer<QWidget> m_modeSwitch;
    int m_labelMode;
    bool m_inStatusBar = false;
};

/**
 * The one bar at the top of the main window: the current tab's own tools on
 * the left, the show controls pinned on the right.
 *
 * A tab says which of its widgets are its tools (registerTabTools()), and
 * keeps building, enabling and hiding them as it always has. While that tab
 * is the one on show here, the tools are BORROWED: lifted out of the tab's
 * layout (a hidden placeholder keeps their place) into this bar, then put
 * back when another tab comes up, when the tab is moved to a window of its
 * own, or when the show controls go to the status bar. So nothing in a tab
 * changes, and a tab in its own window has its tools at its own top, where
 * they always were.
 *
 * A QToolBar's own overflow (») is the narrow-window behaviour: the tools
 * shrink first, the show controls keep their room.
 */
class MainBar final : public QWidget
{
    Q_OBJECT

public:
    explicit MainBar(QWidget *parent = nullptr);
    ~MainBar();

    /** @a tools (inside @a page) are @a page's own tools, for the top bar */
    static void registerTabTools(QWidget *page, QWidget *tools);
    /** What registerTabTools() said for @a page, or nullptr */
    static QWidget *tabTools(QWidget *page);

    /** The show controls and the Design | Operate switch, at the right end */
    void setShowControls(QWidget *controls, QWidget *modeSwitch);

    /** Borrow @a page's tools, giving back whatever was borrowed before.
     *  nullptr just gives back. */
    void hostTabTools(QWidget *page);
    /** Whether tools are borrowed at all. Off gives back what is here. */
    void setHosting(bool on);
    bool isHosting() const { return m_hosting; }

    /** The tools on show here, and whose they are */
    QWidget *hostedTools() const { return m_tools; }
    QWidget *hostedPage() const { return m_page; }

    /** Again after a tab's own label-mode pass: tools here keep their
     *  words beside the icon, not under it (a bar's worth of height) */
    void restyleHostedTools();

private:
    void giveBack();

    QHBoxLayout *m_layout;
    QHBoxLayout *m_slot;
    bool m_hosting;

    QPointer<QWidget> m_page;
    QPointer<QWidget> m_tools;
    /* Where the tools came from: the menu bar of a layout, or a placeholder
       standing in their place in one */
    QPointer<QWidget> m_home;
    QPointer<QLayout> m_homeLayout;
    QPointer<QWidget> m_placeholder;
    bool m_homeIsMenuBar;
    /* Buttons switched to words-beside-icon here, to switch back */
    QList<QPointer<QToolButton>> m_restyledButtons;
    QList<QPointer<QToolBar>> m_restyledBars;
    QMetaObject::Connection m_pageGone;
};

/** @} */

#endif // MAINBAR_H
