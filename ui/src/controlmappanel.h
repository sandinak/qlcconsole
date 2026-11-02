/*
  Q Light Controller Plus
  controlmappanel.h

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

#ifndef CONTROLMAPPANEL_H
#define CONTROLMAPPANEL_H

#include <QWidget>

#include "qlcui_export.h"

class QTableWidget;
class QVBoxLayout;
class QGroupBox;
class QCheckBox;
class QPushButton;
class QLabel;
class Doc;

/** @addtogroup ui_io
 * @{
 */

/**
 * Every Control Map binding of the show in one table (Connections ▸
 * Devices ▸ Show control & sync ▸ Control Map): the control, what it
 * fires, how a press acts (editable); Remove and Remove all. Bindings are
 * made where the thing is, with Learn trigger… (ControlLearnDialog).
 */
class QLC_UI_EXPORT ControlMapPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit ControlMapPanel(Doc *doc, QWidget *parent = nullptr);

    /** One line for the Devices row: "3 triggers", "None". */
    QString summary() const;

    QTableWidget *table() const { return m_table; }

    /** The Encoder bank section (the scheme of a MIDI controller's
     *  endless encoders; docs/features/encoder-banks.md): the device's
     *  preset, its universe and relative encoding, learn the first
     *  encoder, what it is mapped to. */
    QGroupBox *encoderBox() const { return m_encoderBox; }

public slots:
    void refresh();

signals:
    void summaryChanged();

protected:
    void showEvent(QShowEvent *ev) override;

private slots:
    void slotRemove();
    void slotRemoveAll();
    void slotFired(const QString &what);
    void slotEncoderPreset(int index);
    void slotEncoderEdited();
    void slotEncoderLearn();
    void slotEncoderHeard(quint32 universe, quint32 channel);

private:
    Doc *m_doc;
    QTableWidget *m_table;
    QPushButton *m_remove;
    QPushButton *m_removeAll;
    QLabel *m_empty;
    QLabel *m_last;

    void buildEncoderBox(QVBoxLayout *lay);
    void refreshEncoder();

    QGroupBox *m_encoderBox = nullptr;
    QComboBox *m_encPreset = nullptr;
    QComboBox *m_encUniverse = nullptr;
    QComboBox *m_encEncoding = nullptr;
    QCheckBox *m_encInvert = nullptr;
    QPushButton *m_encLearn = nullptr;
    QLabel *m_encNote = nullptr;
    QLabel *m_encMap = nullptr;
    bool m_encLearning = false;
    bool m_encShowing = false;
};

/** @} */

#endif
