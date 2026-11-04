/*
  Q Light Controller Plus
  lengthspinbox.h

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

#ifndef LENGTHSPINBOX_H
#define LENGTHSPINBOX_H

#include <QDoubleSpinBox>

#include "measurementunits.h"

/** \addtogroup ui UI
 * @{
 */

/**
 * A length field whose value() stays in the unit the caller keeps (an
 * effect's "cm" parameter: centimetres) while it shows and takes the unit
 * Settings -> Units picks: 5' 6" with feet, 167.6 cm with centimetres. A
 * length typed with its own unit is converted (5' 6", 5.5 ft, 66 in, 1.676 m).
 *
 * Header-only and without Q_OBJECT (no new signals), so no moc pass.
 */
class LengthSpinBox : public QDoubleSpinBox
{
public:
    /** @a mmPerValue: millimetres in one unit of value() (10 for cm). */
    explicit LengthSpinBox(double mmPerValue, QWidget *parent = nullptr,
                           MeasurementUnits::LengthUnit unit = MeasurementUnits::preferredLength())
        : QDoubleSpinBox(parent)
        , m_mmPerValue(mmPerValue)
        , m_unit(unit)
    {
        setDecimals(2);
        setToolTip(MeasurementUnits::lengthHelp(m_unit));
    }

    MeasurementUnits::LengthUnit unit() const { return m_unit; }

    QString textFromValue(double value) const override
    {
        return MeasurementUnits::formatLength(value * m_mmPerValue, m_unit);
    }

    double valueFromText(const QString &text) const override
    {
        bool ok = false;
        const double mm = MeasurementUnits::parseLength(clean(text), m_unit, &ok);
        return ok ? mm / m_mmPerValue : value();
    }

    QValidator::State validate(QString &input, int &pos) const override
    {
        Q_UNUSED(pos)
        bool ok = false;
        const double mm = MeasurementUnits::parseLength(clean(input), m_unit, &ok);
        if (!ok)
            return QValidator::Intermediate;
        const double v = mm / m_mmPerValue;
        return v >= minimum() - 1e-9 && v <= maximum() + 1e-9 ? QValidator::Acceptable
                                                               : QValidator::Intermediate;
    }

private:
    static QString clean(QString text)
    {
        text.replace(QChar(0x2032), QLatin1Char('\''));   // prime
        text.replace(QChar(0x2033), QLatin1Char('"'));    // double prime
        text.replace(QChar(0x2019), QLatin1Char('\''));
        text.replace(QChar(0x201D), QLatin1Char('"'));
        return text.trimmed();
    }

    double m_mmPerValue;
    MeasurementUnits::LengthUnit m_unit;
};

/** @} */

#endif // LENGTHSPINBOX_H
