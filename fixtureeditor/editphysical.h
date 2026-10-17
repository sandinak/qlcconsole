/*
  Q Light Controller Plus - Fixture Editor
  editphysical.h

  Copyright (C) Massimo Callegari

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

#ifndef EDITPHYSICAL_H
#define EDITPHYSICAL_H

#include <QWidget>

#include "ui_editphysical.h"
#include "qlcphysical.h"
#include "measurementunits.h"

class EditPhysical final : public QWidget, public Ui_EditPhysical
{
    Q_OBJECT

public:
    explicit EditPhysical(QLCPhysical physical, QWidget *parent = 0);
    ~EditPhysical();

    void pasteFromClipboard(QLCPhysical clipboard);

    QLCPhysical physical();

    /** Show the dimensions and weight in @a unit (weight in kg or lb to
     *  match). Normally the Settings -> Units preference, re-read each time
     *  the page is shown. */
    void setUnit(MeasurementUnits::LengthUnit unit);

signals:
    void copyToClipboard(QLCPhysical physical);
    void requestPasteFromClipboard();

private slots:
    void slotCopyToClipboard();
    void slotPasteFromClipboard();

protected:
    void showEvent(QShowEvent *event) override;

    QLCPhysical m_physical;
    class QLabel *m_unitsNote = nullptr;         ///< "in inches and pounds: ..."
    class QComboBox *m_bodyCombo = nullptr;      ///< Box / Lantern
    class QComboBox *m_emissionCombo = nullptr;  ///< Beam / All round
    class QSpinBox *m_ledsSpin = nullptr;        ///< 0 = one point, else LEDs
    class QComboBox *m_ledLayoutCombo = nullptr; ///< Hanging cluster / Tape
    class QComboBox *m_glassCombo = nullptr;     ///< Diffuse / Clear
};

#endif // EDITPHYSICAL_H
