/*
  Q Light Controller Plus
  measurementedit.cpp

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

#include <QLocale>

#include "measurementedit.h"

typedef MeasurementUnits MU;

MeasurementEdit::MeasurementEdit(QWidget *parent)
    : QLineEdit(parent)
    , m_kind(Length)
    , m_unit(MU::Millimetres)
    , m_value(0.0)
    , m_max(9999.0)
    , m_error(false)
{
    setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    connect(this, &QLineEdit::editingFinished, this, &MeasurementEdit::commit);
    showValue();
}

void MeasurementEdit::setKind(Kind kind)
{
    m_kind = kind;
    showValue();
}

void MeasurementEdit::setUnit(MeasurementUnits::LengthUnit unit)
{
    m_unit = unit;
    showValue();
}

void MeasurementEdit::setMaximum(double max)
{
    m_max = max;
}

void MeasurementEdit::setValue(double value)
{
    m_value = qBound(0.0, value, m_max);
    showValue();
}

void MeasurementEdit::commit()
{
    if (!isModified())
        return;

    bool ok = false;
    const double v = m_kind == Length
            ? MU::parseLength(text(), m_unit, &ok)
            : MU::parseWeight(text(), MU::weightUnitFor(m_unit), &ok);
    if (!ok)
    {
        setError(QString());
        return;
    }
    if (v > m_max + 1e-9)
    {
        setError(tr("At most %1.").arg(m_kind == Length
                    ? MU::formatLength(m_max, m_unit)
                    : MU::formatWeight(m_max, MU::weightUnitFor(m_unit))));
        return;
    }

    const bool changed = qAbs(v - m_value) > 1e-9;
    m_value = v;
    showValue();
    if (changed)
        emit valueChanged(m_value);
}

void MeasurementEdit::showValue()
{
    m_error = false;
    setStyleSheet(QString());
    setProperty("invalid", false);

    QString saved;
    if (m_kind == Length)
    {
        setText(MU::formatLength(m_value, m_unit));
        saved = tr("Saved in the fixture definition as %1 mm (it keeps whole millimetres).")
                .arg(MU::toStoredMm(m_value));
        setToolTip(saved + QStringLiteral("\n") + MU::lengthHelp(m_unit));
    }
    else
    {
        const MU::WeightUnit wu = MU::weightUnitFor(m_unit);
        setText(MU::formatWeight(m_value, wu));
        // As QLCPhysical writes it.
        saved = tr("Saved in the fixture definition as %1 kg.")
                .arg(QLocale::c().toString(m_value));
        setToolTip(saved + QStringLiteral("\n") + MU::weightHelp(wu));
    }
    setModified(false);
}

void MeasurementEdit::setError(const QString &why)
{
    m_error = true;
    setProperty("invalid", true);
    setStyleSheet(QStringLiteral("QLineEdit { color: #d03030; border: 1px solid #d03030; }"));
    const QString help = m_kind == Length ? MU::lengthHelp(m_unit)
                                          : MU::weightHelp(MU::weightUnitFor(m_unit));
    const QString kept = m_kind == Length ? MU::formatLength(m_value, m_unit)
                                          : MU::formatWeight(m_value, MU::weightUnitFor(m_unit));
    setToolTip(tr("Not understood, so it is still %1.").arg(kept)
               + (why.isEmpty() ? QString() : QStringLiteral(" ") + why)
               + QStringLiteral("\n") + help);
    // Keep the text so it can be fixed; editing it again re-reads it.
    setModified(true);
}
