/*
  Q Light Controller Plus
  measurementedit.h

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

#ifndef MEASUREMENTEDIT_H
#define MEASUREMENTEDIT_H

#include <QLineEdit>

#include "measurementunits.h"

/** @addtogroup ui UI
 * @{
 */

/**
 * A length or a weight, shown in the person's unit (Settings -> Units) and
 * typed in any unit: 7.5", 0.625 ft, 6' 3", 19.05 cm, 26.5 lbs, 400 g ...
 * (MeasurementUnits says what is accepted). value() is always millimetres or
 * kilograms, exactly as typed -- the caller rounds to what its file keeps.
 * Text that can't be read is marked red with what would be accepted, and
 * the value stays as it was.
 */
class MeasurementEdit : public QLineEdit
{
    Q_OBJECT

public:
    enum Kind { Length, Weight };

    explicit MeasurementEdit(QWidget *parent = nullptr);

    /** Length (mm) or weight (kg); reformats the text */
    void setKind(Kind kind);
    Kind kind() const { return m_kind; }

    /** The length unit shown (weight follows: kg or lb); reformats */
    void setUnit(MeasurementUnits::LengthUnit unit);
    MeasurementUnits::LengthUnit unit() const { return m_unit; }

    /** Largest accepted value, in mm or kg */
    void setMaximum(double max);
    double maximum() const { return m_max; }

    /** The value in mm or kg. Setting it clears any error. */
    void setValue(double value);
    double value() const { return m_value; }

    /** True while the text couldn't be read (and the value is the last good one) */
    bool hasError() const { return m_error; }

public slots:
    /** Read what was typed, if it changed: on Return, on leaving the field,
     *  and before a caller takes value() */
    void commit();

signals:
    void valueChanged(double value);

private:
    void showValue();
    void setError(const QString &why);

private:
    Kind m_kind;
    MeasurementUnits::LengthUnit m_unit;
    double m_value;
    double m_max;
    bool m_error;
};

/** @} */

#endif
