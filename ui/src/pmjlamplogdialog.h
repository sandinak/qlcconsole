/*
  Q Light Controller Plus - qlcconsole
  pmjlamplogdialog.h

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

#ifndef PMJLAMPLOGDIALOG_H
#define PMJLAMPLOGDIALOG_H

#include <QDialog>
#include <QPointer>

#include "qlcui_export.h"

class QPlainTextEdit;
class QComboBox;
class QPushButton;
class QLabel;
class PMJOverlay;

/**
 * The PMJ Black 1's lamp log (Connections ▸ Control surfaces ▸ PMJ Black 1
 * ▸ Lamp Log…): the last lamps the app sent the board, byte for byte, so a
 * test at the board shows what was sent, not what was meant; and a Lamp
 * Test that walks one button's lamp through every level and blink the
 * board has (PMJOverlay::startLampTest()).
 */
class QLC_UI_EXPORT PmjLampLogDialog final : public QDialog
{
    Q_OBJECT

public:
    PmjLampLogDialog(PMJOverlay *overlay, QWidget *parent = nullptr);
    ~PmjLampLogDialog() override;

    /** The log as shown. */
    QString logText() const;
    QPushButton *testButton() const { return m_test; }
    QComboBox *buttonBox() const { return m_control; }

private:
    void refresh();

    QPointer<PMJOverlay> m_overlay;
    QPlainTextEdit *m_log = nullptr;
    QComboBox *m_control = nullptr;
    QPushButton *m_test = nullptr;
    QLabel *m_step = nullptr;
};

#endif
