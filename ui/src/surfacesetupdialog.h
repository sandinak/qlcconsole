/*
  Q Light Controller Plus - qlcconsole
  surfacesetupdialog.h

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

#ifndef SURFACESETUPDIALOG_H
#define SURFACESETUPDIALOG_H

#include <QDialog>
#include <QList>

#include "surfacesetup.h"
#include "qlcui_export.h"

class QLabel;
class QComboBox;
class QPushButton;
class QTimer;
class QWidget;
class Doc;

/** @addtogroup ui_io
 * @{
 */

/**
 * Connections ▸ Control surfaces ▸ Controller setup ▸ Set Up…: what each
 * patched controller needs set on the device itself (SurfaceSetup). It
 * checks on opening (a read); every write is a button here: Align Lamps
 * (a backup first, its path shown), Undo Align, Identify Lamps… for a
 * board with no shipped pairing, Send Again for an init. In Operate a
 * write asks first. docs/features/control-surfaces.md, "Setting up a
 * controller".
 */
class QLC_UI_EXPORT SurfaceSetupDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SurfaceSetupDialog(Doc *doc, QWidget *parent = nullptr);
    ~SurfaceSetupDialog();

    /** Opened as the hub opens it (modal). */
    static void open(Doc *doc, QWidget *parent);

    /** The universe shown (UINT_MAX: none). */
    quint32 universe() const;
    QString statusText() const;
    /** During Identify: the lamp flashing now (-1: not identifying). */
    int identifyLamp() const { return m_identifying ? m_lamp : -1; }

    /** For a test: confirm the Operate question without asking. */
    void setConfirmInOperate(bool yes) { m_autoConfirm = yes ? 1 : 0; }

public slots:
    void checkNow();
    void alignLamps();
    void undoAlign();
    void sendAgain();
    void checkMode();
    void startIdentify();
    void skipLamp();
    void finishIdentify();
    /** Pair the board's own output as its feedback line, or say where to. */
    void patchFeedback();

private:
    /** Whether a write may go ahead (in Operate: asked). */
    bool confirmWrite(bool *confirmed);
    void updateState();
    void nextLamp();
    void showError(const QString &error);

    Doc *m_doc;
    SurfaceSetup *m_setup;
    QLabel *m_intro = nullptr;
    QComboBox *m_device = nullptr;
    QLabel *m_deviceLabel = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_backup = nullptr;
    QLabel *m_error = nullptr;
    QWidget *m_buttons = nullptr;
    QPushButton *m_check = nullptr;
    QPushButton *m_align = nullptr;
    QPushButton *m_undo = nullptr;
    QPushButton *m_identify = nullptr;
    QPushButton *m_send = nullptr;
    QPushButton *m_mode = nullptr;
    QPushButton *m_patchFeedback = nullptr;
    QWidget *m_identifyBox = nullptr;
    QLabel *m_identifyText = nullptr;
    QPushButton *m_skip = nullptr;
    QPushButton *m_finish = nullptr;
    QTimer *m_flash = nullptr;
    bool m_flashOn = false;
    bool m_identifying = false;
    int m_lamp = 0;
    QList<SurfaceSetup::LampPair> m_found;
    int m_autoConfirm = -1;
};

/** @} */

#endif
