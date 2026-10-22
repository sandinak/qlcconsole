/*
  Q Light Controller Plus
  moveinblackbutton.cpp

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

#include <QDoubleSpinBox>
#include <QWidgetAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>

#include "moveinblackbutton.h"
#include "programmercontroller.h"
#include "doc.h"

MoveInBlackButton::MoveInBlackButton(Doc *doc, QWidget *parent)
    : QToolButton(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("moveInBlackButton"));
    setToolButtonStyle(Qt::ToolButtonTextOnly);
    setCheckable(true);
    setPopupMode(QToolButton::MenuButtonPopup);

    QMenu *menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("moveInBlackMenu"));
    m_autoAction = menu->addAction(tr("Auto move-in-black"));
    m_autoAction->setObjectName(QStringLiteral("moveInBlackAuto"));
    m_autoAction->setCheckable(true);

    QWidget *row = new QWidget(menu);
    QHBoxLayout *rl = new QHBoxLayout(row);
    rl->setContentsMargins(24, 2, 12, 2);
    rl->addWidget(new QLabel(tr("Lead time"), row));
    rl->addStretch(1);
    m_leadSpin = new QDoubleSpinBox(row);
    m_leadSpin->setObjectName(QStringLiteral("moveInBlackLead"));
    m_leadSpin->setRange(0.1, 10.0);
    m_leadSpin->setSingleStep(0.1);
    m_leadSpin->setDecimals(1);
    m_leadSpin->setSuffix(tr(" s"));
    m_leadSpin->setToolTip(tr("The next cue must be at least this far away for a dark mover to be "
                              "pre-set. Below it there's no time to move in black, so it's skipped."));
    rl->addWidget(m_leadSpin);
    QWidgetAction *wa = new QWidgetAction(menu);
    wa->setDefaultWidget(row);
    menu->addAction(wa);
    menu->addSeparator();
    QAction *note = menu->addAction(tr("Saved with the show (also in Programming and Settings)"));
    note->setEnabled(false);
    setMenu(menu);

    // Lit while on, in the theme's highlight; the arrow stays plain.
    setStyleSheet(QStringLiteral(
        "QToolButton#moveInBlackButton:checked { background: palette(highlight);"
        "  color: palette(highlighted-text); border-radius: 4px; }"));

    connect(this, &QToolButton::toggled, this, [this](bool on) {
        if (ProgrammerController *pc = controller())
            pc->setAutoMoveInBlack(on);
        sync();     // stays put if the controller said no
    });
    connect(m_autoAction, &QAction::toggled, this, [this](bool on) {
        if (ProgrammerController *pc = controller())
            pc->setAutoMoveInBlack(on);
        sync();
    });
    connect(m_leadSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double s) {
        if (ProgrammerController *pc = controller())
            pc->setMoveInBlackDarkGapMs(int(s * 1000.0 + 0.5));
    });

    if (ProgrammerController *pc = controller())
    {
        connect(pc, &ProgrammerController::autoMoveInBlackChanged, this, &MoveInBlackButton::sync);
        connect(pc, &ProgrammerController::moveInBlackPresetsChanged, this, &MoveInBlackButton::sync);
    }
    sync();
}

ProgrammerController *MoveInBlackButton::controller() const
{
    return m_doc != nullptr ? m_doc->programmer() : nullptr;
}

void MoveInBlackButton::sync()
{
    ProgrammerController *pc = controller();
    const bool on = pc != nullptr && pc->isAutoMoveInBlack();
    const double lead = (pc != nullptr ? pc->moveInBlackDarkGapMs() : 800) / 1000.0;
    const int presets = pc != nullptr ? pc->moveInBlackPresetCount() : 0;
    {
        const QSignalBlocker b1(this), b2(m_autoAction), b3(m_leadSpin);
        setChecked(on);
        m_autoAction->setChecked(on);
        m_leadSpin->setValue(lead);
    }
    // MIDDLE DOT: the count of heads being moved in black right now.
    setText(on && presets > 0 ? tr("MIB \302\267 %1").arg(presets) : tr("MIB"));
    QString tip = tr("Auto move-in-black: while the show runs, look ahead to the next cue and "
                     "pre-position any dark mover it will reveal, if the next cue is at least "
                     "the lead time away (now %1 s). The arrow sets the lead time. Saved with "
                     "the show; the same switch as Programming → Move in black and "
                     "Settings → Show Tree.").arg(lead, 0, 'f', 1);
    if (on)
        tip += QLatin1Char('\n') + (presets > 0
            ? tr("Pre-positioning %n head(s) in black for the next cue.", "", presets)
            : tr("On: nothing to pre-position right now."));
    else
        tip += QLatin1Char('\n') + tr("Off.");
    setToolTip(tip);
}
