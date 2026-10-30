/*
  Q Light Controller Plus
  mainbar.cpp

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

#include <QHBoxLayout>
#include <QToolButton>
#include <QAction>
#include <QLayout>

#include "mainbar.h"
#include "blindstyle.h"
#include "doc.h"

#define KTabToolsProperty "mainBarTabTools"

/*****************************************************************************
 * ShowControls
 *****************************************************************************/

ShowControls::ShowControls(Doc *doc, QAction *stopAll, QAction *blackout, QAction *blind,
                           QAction *mode, QWidget *parent)
    : QToolBar(tr("Show controls"), parent)
    , m_doc(doc)
    , m_modeAction(mode)
    , m_labelMode(0)
{
    setObjectName(QStringLiteral("ShowControls"));
    setMovable(false);
    setFloatable(false);
    setIconSize(QSize(18, 18));
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    m_stopAll = buttonFor(stopAll, "showStopAll");
    if (m_stopAll != nullptr)
        m_stopAll->setPopupMode(QToolButton::DelayedPopup);  // click = stop, hold = fade menu
    m_blackout = buttonFor(blackout, "showBlackout");
    m_blind = buttonFor(blind, "showBlind");

    /* Design | Operate: two states, both named, the one you are in lit.
       The toolbar's old single button said where a click would TAKE you,
       which read as the mode you were in about half the time.

       A widget of its own beside this bar, not in it: in a status bar too
       full for everything, the layout squeezes its widest items first, and
       the bar and the switch apart are each narrower than the chips that
       give way before them. */
    QWidget *modeSwitch = new QWidget(parent);
    modeSwitch->setObjectName(QStringLiteral("modeSwitch"));
    m_modeSwitch = modeSwitch;
    QHBoxLayout *ml = new QHBoxLayout(modeSwitch);
    ml->setContentsMargins(0, 0, 0, 0);
    ml->setSpacing(0);
    const QString keys = mode != nullptr ? mode->shortcut().toString(QKeySequence::NativeText) : QString();
    auto half = [&](const QString &text, const QString &tip, const char *name) {
        QToolButton *b = new QToolButton(modeSwitch);
        b->setObjectName(QLatin1String(name));
        b->setText(text);
        b->setToolTip(keys.isEmpty() ? tip : tr("%1 (%2)").arg(tip, keys));
        b->setCheckable(true);
        b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        b->setFocusPolicy(Qt::NoFocus);
        connect(b, &QToolButton::clicked, this, &ShowControls::slotModeClicked);
        ml->addWidget(b);
        return b;
    };
    m_design = half(tr("Design"), tr("Design mode: build and edit the show"), "modeDesign");
    m_operate = half(tr("Operate"), tr("Operate mode: run the show"), "modeOperate");
    modeSwitch->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    /* Lit while on: red for Blackout, blue for Blind -- the colours the
       footer and the Blind/Park docs already use. Colours of their own with
       white words, so they read the same on all six themes; the switch's
       Design half uses the theme's own highlight pair. */
    setStyleSheet(QString(
        // Its own background, so it reads the same in the status bar when
        // Blind turns that blue.
        "QToolBar#ShowControls { background: palette(window); border: none; }"
        "QToolButton#showBlackout:checked { background-color: #c62828; color: #ffffff;"
        "  border: 1px solid #8e1b1b; border-radius: 4px; }"
        "QToolButton#showBlind:checked { background-color: " + BlindStyle::colour() + "; color: #ffffff;"
        "  border: 1px solid " + BlindStyle::edge() + "; border-radius: 4px; }"));
    modeSwitch->setStyleSheet(QStringLiteral(
        "QWidget#modeSwitch { background: palette(window); }"
        "QToolButton#modeDesign, QToolButton#modeOperate { padding: 2px 9px;"
        "  border: 1px solid palette(mid); background: transparent; color: palette(window-text); }"
        "QToolButton#modeDesign { border-top-left-radius: 5px; border-bottom-left-radius: 5px; }"
        "QToolButton#modeOperate { border-left: none;"
        "  border-top-right-radius: 5px; border-bottom-right-radius: 5px; }"
        "QToolButton#modeDesign:hover:!checked, QToolButton#modeOperate:hover:!checked {"
        "  background: rgba(128, 128, 128, 60); }"
        "QToolButton#modeDesign:checked { background: palette(highlight); color: palette(highlighted-text); }"
        "QToolButton#modeOperate:checked { background: #2e7d32; color: #ffffff; }"
        "QToolButton#modeDesign:disabled, QToolButton#modeOperate:disabled { color: palette(mid); }"
        // Tighter in the status bar, which is short of room.
        "QWidget#modeSwitch[inStatusBar=\"true\"] QToolButton { padding: 1px 5px; }"));

    if (m_doc != nullptr)
        connect(m_doc, &Doc::modeChanged, this, &ShowControls::syncMode);
    if (m_modeAction != nullptr)
        connect(m_modeAction, &QAction::changed, this, &ShowControls::syncMode);
    syncMode();
}

ShowControls::~ShowControls()
{
    // Placed beside this bar, so not a child of it: it goes with it.
    delete m_modeSwitch;
}

QToolButton *ShowControls::buttonFor(QAction *action, const char *name)
{
    if (action == nullptr)
        return nullptr;
    addAction(action);
    QToolButton *b = qobject_cast<QToolButton *>(widgetForAction(action));
    if (b != nullptr)
        b->setObjectName(QLatin1String(name));
    return b;
}

void ShowControls::setLabelMode(int mode)
{
    m_labelMode = mode;
    applyStyle();
}

void ShowControls::setInStatusBar(bool on)
{
    m_inStatusBar = on;
    setIconSize(on ? QSize(16, 16) : QSize(18, 18));
    applyStyle();
    if (m_modeSwitch != nullptr)
    {
        m_modeSwitch->setProperty("inStatusBar", on);
        // The [inStatusBar] rule: a property change alone doesn't re-polish.
        const QString sheet = m_modeSwitch->styleSheet();
        m_modeSwitch->setStyleSheet(QString());
        m_modeSwitch->setStyleSheet(sheet);
    }
}

void ShowControls::applyStyle()
{
    // App::TabLabelMode: 0 icons and words, 1 icons only, 2 words only.
    // The status bar is already full of chips: there, icons and words is
    // icons only -- the tooltips name them, and Blackout and Blind light
    // up all the same. Words only stays words, by request.
    if (m_labelMode == 2)
        setToolButtonStyle(Qt::ToolButtonTextOnly);
    else if (m_labelMode == 1 || m_inStatusBar)
        setToolButtonStyle(Qt::ToolButtonIconOnly);
    else
        setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
}

void ShowControls::syncMode()
{
    const bool operate = m_doc != nullptr && m_doc->mode() == Doc::Operate;
    m_design->setChecked(!operate);
    m_operate->setChecked(operate);
    const bool enabled = m_modeAction != nullptr && m_modeAction->isEnabled();
    m_design->setEnabled(enabled);
    m_operate->setEnabled(enabled);
}

void ShowControls::slotModeClicked()
{
    const bool wantOperate = (sender() == m_operate);
    const bool inOperate = m_doc != nullptr && m_doc->mode() == Doc::Operate;
    // The action, not Doc::setMode(): its checks (unsaved programmer values,
    // a running show) come with it, and it may say no.
    if (wantOperate != inOperate && m_modeAction != nullptr)
        m_modeAction->trigger();
    syncMode();
}

/*****************************************************************************
 * MainBar
 *****************************************************************************/

MainBar::MainBar(QWidget *parent)
    : QWidget(parent)
    , m_hosting(true)
    , m_homeIsMenuBar(false)
{
    setObjectName(QStringLiteral("MainBar"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(6);

    QWidget *slot = new QWidget(this);
    slot->setObjectName(QStringLiteral("MainBarTabTools"));
    slot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_slot = new QHBoxLayout(slot);
    m_slot->setContentsMargins(0, 0, 0, 0);
    m_slot->setSpacing(0);
    m_layout->addWidget(slot, 1);
}

MainBar::~MainBar()
{
    giveBack();
}

void MainBar::registerTabTools(QWidget *page, QWidget *tools)
{
    if (page == nullptr)
        return;
    page->setProperty(KTabToolsProperty, QVariant::fromValue(static_cast<QObject *>(tools)));
}

QWidget *MainBar::tabTools(QWidget *page)
{
    if (page == nullptr)
        return nullptr;
    return qobject_cast<QWidget *>(page->property(KTabToolsProperty).value<QObject *>());
}

void MainBar::setShowControls(QWidget *controls, QWidget *modeSwitch)
{
    if (controls == nullptr)
        return;
    // Fixed: never folded into an overflow menu. The tab's tools give way
    // first.
    controls->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_layout->addWidget(controls, 0);
    controls->show();
    if (modeSwitch != nullptr)
    {
        m_layout->addWidget(modeSwitch, 0);
        modeSwitch->show();
    }
}

void MainBar::setHosting(bool on)
{
    m_hosting = on;
    if (on == false)
        giveBack();
}

static QLayout *layoutHolding(QLayout *layout, QWidget *w)
{
    if (layout == nullptr)
        return nullptr;
    if (layout->indexOf(w) >= 0)
        return layout;
    for (int i = 0; i < layout->count(); ++i)
        if (QLayout *found = layoutHolding(layout->itemAt(i)->layout(), w))
            return found;
    return nullptr;
}

void MainBar::hostTabTools(QWidget *page)
{
    if (page != nullptr && page == m_page && m_tools != nullptr)
        return;     // already here

    giveBack();
    if (m_hosting == false || page == nullptr)
        return;

    QWidget *tools = tabTools(page);
    if (tools == nullptr || tools->parentWidget() == nullptr
            || tools->parentWidget()->layout() == nullptr)
        return;

    QWidget *home = tools->parentWidget();
    QLayout *homeLayout = nullptr;
    bool menuBar = false;
    if (home->layout()->menuBar() == tools)
    {
        homeLayout = home->layout();
        menuBar = true;
    }
    else
    {
        homeLayout = layoutHolding(home->layout(), tools);
    }
    if (homeLayout == nullptr)
        return;

    const bool hidden = tools->isHidden();
    if (menuBar)
    {
        homeLayout->setMenuBar(nullptr);
    }
    else
    {
        // A hidden stand-in keeps the tools' place (and every index after
        // it) in the tab's layout until they come back.
        QWidget *placeholder = new QWidget(home);
        placeholder->setObjectName(QStringLiteral("MainBarPlaceholder"));
        delete homeLayout->replaceWidget(tools, placeholder);
        placeholder->setVisible(false);
        m_placeholder = placeholder;
    }

    m_page = page;
    m_tools = tools;
    m_home = home;
    m_homeLayout = homeLayout;
    m_homeIsMenuBar = menuBar;
    m_slot->addWidget(tools, 1);
    tools->setVisible(hidden == false);
    restyleHostedTools();

    // The tab going (quit, a view rebuilt) takes its tools with it.
    m_pageGone = connect(page, &QObject::destroyed, this, [this]() {
        QWidget *tools = m_tools;
        m_tools = nullptr;
        m_page = nullptr;
        delete m_placeholder;
        delete tools;
    });
}

void MainBar::giveBack()
{
    disconnect(m_pageGone);

    QWidget *tools = m_tools;
    QWidget *page = m_page;
    if (tools != nullptr)
    {
        // Words back under the icons where the tab had them so.
        for (const QPointer<QToolBar> &bar : std::as_const(m_restyledBars))
            if (bar != nullptr && bar->toolButtonStyle() == Qt::ToolButtonTextBesideIcon)
                bar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        for (const QPointer<QToolButton> &b : std::as_const(m_restyledButtons))
            if (b != nullptr && b->toolButtonStyle() == Qt::ToolButtonTextBesideIcon)
                b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        const bool hidden = tools->isHidden();
        m_slot->removeWidget(tools);
        // Its own parent first: a layout added with addItem() (the Virtual
        // Console's) has no widget of its own to adopt it into.
        if (m_home != nullptr)
            tools->setParent(m_home);
        else if (page != nullptr)
            tools->setParent(page);
        if (m_homeLayout != nullptr && m_homeIsMenuBar)
            m_homeLayout->setMenuBar(tools);
        else if (m_homeLayout != nullptr && m_placeholder != nullptr)
            delete m_homeLayout->replaceWidget(m_placeholder, tools);
        tools->setVisible(hidden == false);
    }
    delete m_placeholder;

    m_restyledBars.clear();
    m_restyledButtons.clear();
    m_tools = nullptr;
    m_page = nullptr;
    m_home = nullptr;
    m_homeLayout = nullptr;
    m_homeIsMenuBar = false;
}

void MainBar::restyleHostedTools()
{
    if (m_tools == nullptr)
        return;

    QList<QToolBar *> bars = m_tools->findChildren<QToolBar *>();
    if (QToolBar *own = qobject_cast<QToolBar *>(m_tools))
        bars.prepend(own);
    for (QToolBar *bar : std::as_const(bars))
        if (bar->toolButtonStyle() == Qt::ToolButtonTextUnderIcon)
        {
            bar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            m_restyledBars << bar;
        }

    const QList<QToolButton *> buttons = m_tools->findChildren<QToolButton *>();
    for (QToolButton *b : buttons)
        if (b->toolButtonStyle() == Qt::ToolButtonTextUnderIcon)
        {
            b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            m_restyledButtons << b;
        }
}
