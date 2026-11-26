/*
  Q Light Controller Plus
  editlivechip.h

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

#ifndef EDITLIVECHIP_H
#define EDITLIVECHIP_H

#include <QWidget>
#include <functional>

#include "qlcui_export.h"

class QToolButton;
class QLabel;
class QTimer;
class QMenu;
class EditLive;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The footer's Edit Live chip (docs/features/edit-live.md, Q339): in
 * Operate, with a board that has surface pages, "Edit Live" (a click turns
 * it on, asking the show's PIN if it has one); while on, red "EDIT LIVE",
 * with the number of trims, always shown -- a click opens the popup: each
 * trimmed setting (fixture · attribute · from → to), Store to Cue N, Drop
 * and Turn Edit Live Off. After a GO took trims with the old cue, a
 * "Store to Cue N" button beside it for 30 s (Q338).
 */
class QLC_UI_EXPORT EditLiveChip final : public QWidget
{
    Q_OBJECT

public:
    explicit EditLiveChip(Doc *doc, QWidget *parent = nullptr);

    /** Turn Edit Live on from @p parent: the show's PIN first, if it has
     *  one. False: refused (the why is said on @p parent's status line by
     *  the caller) or the PIN not given. The Run tab's switch uses it too. */
    static bool turnOn(Doc *doc, QWidget *parent, QString *why = nullptr);
    /** Its workspace level key (LevelRegistry): Standard and up, so a
     *  Student show never reaches it (greyed, saying the level). */
    static QString levelKey();
    /** Tests stand in for the PIN dialog (true: given). */
    static void setPinAsker(const std::function<bool(QWidget *, const QString &why)> &asker);

    QToolButton *chip() const { return m_chip; }
    QToolButton *storeLeftButton() const { return m_storeLeft; }
    /** The popup as a click opens it (built anew each time). */
    QMenu *buildMenu();
    void setChipHeight(int h);

public slots:
    void refresh();

signals:
    /** A line for the status bar ("Stored Position of Spot in cue 1"). */
    void said(const QString &text);

private:
    void clicked();
    EditLive *live() const;

private:
    Doc *m_doc;
    QToolButton *m_chip;
    QToolButton *m_storeLeft;
    QTimer *m_leftTimer;
};

/** @} */

#endif
