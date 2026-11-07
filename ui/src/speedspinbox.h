/*
  Q Light Controller Plus
  speedspinbox.h

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

#ifndef SPEEDSPINBOX_H
#define SPEEDSPINBOX_H

#include <QCoreApplication>
#include <QDoubleSpinBox>

#include "measurementunits.h"

/** \addtogroup ui UI
 * @{
 */

/**
 * A speed field (the follow spot's Top Speed): value() stays in the unit
 * the caller keeps -- an effect's "cm/s" parameter, centimetres a second --
 * while it shows and takes a length a second in the unit Settings -> Units
 * picks: 6 ft/s with feet or inches, 1.83 m/s with the metric lengths. A
 * speed typed with its own unit is converted (1.8 m/s, 6 ft/s, 180 cm).
 *
 * Header-only and without Q_OBJECT, as LengthSpinBox.
 */
class SpeedSpinBox : public QDoubleSpinBox
{
public:
    /** @a mPerSecondPerValue: metres a second in one unit of value()
     *  (0.01 for cm/s). */
    explicit SpeedSpinBox(double mPerSecondPerValue, QWidget *parent = nullptr,
                          MeasurementUnits::LengthUnit unit = MeasurementUnits::preferredLength())
        : QDoubleSpinBox(parent)
        , m_scale(mPerSecondPerValue)
        , m_unit(unit)
    {
        setDecimals(2);
        /* The arrows step a tenth of a foot (or 5 cm) a second. */
        const double step = MeasurementUnits::isImperial(unit) ? 0.03048 : 0.05;
        setSingleStep(step / m_scale);
        setToolTip(QCoreApplication::translate("SpeedSpinBox", "A speed: %1, or a length a second in any unit (%2, %3)")
                       .arg(MeasurementUnits::formatSpeed(1.8288, unit),
                            MeasurementUnits::formatSpeed(1.8, MeasurementUnits::Metres),
                            MeasurementUnits::formatSpeed(1.8288, MeasurementUnits::Feet)));
    }

    MeasurementUnits::LengthUnit unit() const { return m_unit; }

    QString textFromValue(double value) const override
    {
        return MeasurementUnits::formatSpeed(value * m_scale, m_unit);
    }

    double valueFromText(const QString &text) const override
    {
        bool ok = false;
        const double mps = MeasurementUnits::parseSpeed(text, m_unit, &ok);
        return ok ? mps / m_scale : value();
    }

    QValidator::State validate(QString &input, int &pos) const override
    {
        Q_UNUSED(pos)
        bool ok = false;
        const double v = MeasurementUnits::parseSpeed(input, m_unit, &ok) / m_scale;
        if (!ok)
            return QValidator::Intermediate;
        return v >= minimum() - 1e-9 && v <= maximum() + 1e-9 ? QValidator::Acceptable
                                                               : QValidator::Intermediate;
    }

private:
    double m_scale;
    MeasurementUnits::LengthUnit m_unit;
};

/** @} */

#endif // SPEEDSPINBOX_H
