/*
  Q Light Controller Plus
  controllearndialog.h

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

#ifndef CONTROLLEARNDIALOG_H
#define CONTROLLEARNDIALOG_H

#include <QDialog>

#include "controlmap.h"
#include "qlcui_export.h"

class QPushButton;
class QComboBox;
class QAction;
class QLabel;
class QMenu;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Learn trigger…: listens for the next MIDI note or CC, or OSC message, on
 * any input universe and binds that control to fire a function, a Show
 * Tree cue list, cue or hit (the Control Map, ControlMap). Pressing another
 * control before OK takes that one instead; the mode says how a press acts.
 * Remove trigger takes an existing binding away.
 */
class QLC_UI_EXPORT ControlLearnDialog final : public QDialog
{
    Q_OBJECT

public:
    ControlLearnDialog(Doc *doc, const ControlMap::Target &target, QWidget *parent = nullptr);
    ~ControlLearnDialog();

    /** Adds "Learn trigger…" (or, bound, "Trigger: <control>…") for
     *  @a target to @a menu; choosing it opens the dialog. */
    static QAction *addTo(QMenu *menu, Doc *doc, const ControlMap::Target &target, QWidget *parent);

    /** Whether a control is chosen (heard, or the one already bound). */
    bool hasControl() const { return m_universe != 0xFFFFFFFF; }
    quint32 universe() const { return m_universe; }
    quint32 channel() const { return m_channel; }
    ControlMap::Mode mode() const;
    void setMode(ControlMap::Mode mode);

public slots:
    void accept() override;
    void done(int r) override;

private slots:
    void slotHeard(quint32 universe, quint32 channel);
    void slotRemove();

private:
    void updateText();

private:
    Doc *m_doc;
    ControlMap::Target m_target;
    quint32 m_universe = 0xFFFFFFFF;
    quint32 m_channel = 0xFFFFFFFF;
    QLabel *m_heard;
    QLabel *m_note;
    QComboBox *m_mode;
    QPushButton *m_ok;
    QPushButton *m_remove;
};

/** @} */

#endif
