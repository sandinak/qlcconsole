/*
  Q Light Controller Plus - qlcconsole
  deskchip.h

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

#ifndef DESKCHIP_H
#define DESKCHIP_H

#include <QToolButton>

#include "qlcui_export.h"

class QTimer;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The footer's chip for this computer's desk (DeskRecord): hidden while no
 * role is learned, else
 *  - green "Desk": every role that is on reaches the program;
 *  - amber: a role is off, its device is not plugged in, two devices of one
 *    name can't be told apart, or the Grand Master waits to pick up;
 *  - red: a role's device is here but this show's patch lacks it -- a click
 *    is Patch It (named; the chip then offers Undo for a while).
 * Its tooltip says each role and what GO plays now (the Run tab's list,
 * named). A click on green or amber opens Connections ▸ Control surfaces.
 */
class QLC_UI_EXPORT DeskChip final : public QToolButton
{
    Q_OBJECT

public:
    enum Colour { Hidden, Green, Amber, Red };
    Q_ENUM(Colour)

    explicit DeskChip(Doc *doc, QWidget *parent = nullptr);

    Colour colour() const { return m_colour; }
    /** How long Undo stays offered after Patch It (ms). */
    void setUndoMs(int ms);

public slots:
    void refresh();

signals:
    /** Open Connections ▸ Control surfaces. */
    void setUpRequested();
    /** What Patch It (or its Undo) did, for the status line. */
    void said(const QString &text);

private:
    void slotClicked();

private:
    Doc *m_doc;
    Colour m_colour = Hidden;
    QTimer *m_undoTimer;
    QString m_patched;
};

/** @} */

#endif
