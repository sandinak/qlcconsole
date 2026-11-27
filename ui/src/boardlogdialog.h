/*
  Q Light Controller Plus - qlcconsole
  boardlogdialog.h

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

#ifndef BOARDLOGDIALOG_H
#define BOARDLOGDIALOG_H

#include <QDialog>
#include "qlcui_export.h"

class QPlainTextEdit;
class QPushButton;
class QLabel;
class Doc;

/**
 * A control surface's Board Log (SurfaceBoardLog): what the board sent and
 * was sent, newest last, and the app's own steps (its init, a profile
 * chosen, the motor fader sent), with Copy for a bug report and a Lamp
 * Test. Opened from the board's footer chip (Board Log…).
 */
class QLC_UI_EXPORT BoardLogDialog final : public QDialog
{
    Q_OBJECT

public:
    BoardLogDialog(Doc *doc, quint32 universe, QWidget *parent = nullptr);
    ~BoardLogDialog() override;

    QString logText() const;
    quint32 universe() const { return m_universe; }

public slots:
    void refresh();
    /** Start the lamp test (its why on the dialog when it can't). */
    void startLampTest();

private:
    Doc *m_doc;
    quint32 m_universe;
    QPlainTextEdit *m_log;
    QPushButton *m_test;
    QLabel *m_step;
};

#endif
