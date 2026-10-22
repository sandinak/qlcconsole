/*
  Q Light Controller Plus
  moveinblackbutton.h

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

#ifndef MOVEINBLACKBUTTON_H
#define MOVEINBLACKBUTTON_H

#include <QToolButton>
#include <QPointer>

class ProgrammerController;
class QDoubleSpinBox;
class QAction;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * "MIB": the show's automatic move-in-black, for the place an operator runs
 * cues from (the Show Tree's bar). A click turns it on and off; the arrow
 * holds the same switch and the lead time. While it is pre-positioning dark
 * heads for the next cue it says how many ("MIB · 3").
 *
 * A view only: the setting is ProgrammerController's, one per show and saved
 * with it, and this follows its autoMoveInBlackChanged() like the
 * Programming tab's "Move in black" menu and Settings → Show Tree do.
 */
class MoveInBlackButton final : public QToolButton
{
    Q_OBJECT

public:
    MoveInBlackButton(Doc *doc, QWidget *parent = nullptr);

    QAction *autoAction() const { return m_autoAction; }
    QDoubleSpinBox *leadSpin() const { return m_leadSpin; }

private slots:
    /** Switch, lead time and indicator from the controller */
    void sync();

private:
    ProgrammerController *controller() const;

    Doc *m_doc;
    QAction *m_autoAction;
    QDoubleSpinBox *m_leadSpin;
};

/** @} */

#endif
