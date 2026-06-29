/*
  Q Light Controller Plus
  feetinchesspinbox.h

  A QDoubleSpinBox that, in feet mode, accepts and displays imperial
  measurements as feet-and-inches (e.g. 5' 6") in addition to plain decimal
  feet (e.g. 5.5). The stored value is always in display-feet, so callers
  convert to metres exactly as they would with a plain spin box. In metric
  mode it behaves as an ordinary " m" spin box.

  Header-only and deliberately free of the Q_OBJECT macro (it adds no new
  signals/slots), so it needs no moc pass or CMake entry.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef FEETINCHESSPINBOX_H
#define FEETINCHESSPINBOX_H

#include <QDoubleSpinBox>
#include <QRegularExpression>
#include <QStringList>
#include <QtMath>

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class FeetInchesSpinBox : public QDoubleSpinBox
{
public:
    explicit FeetInchesSpinBox(bool feetMode, QWidget *parent = nullptr)
        : QDoubleSpinBox(parent)
        , m_feetMode(feetMode)
    {
        setDecimals(2);
        if (!feetMode)
            setSuffix(QStringLiteral(" m"));
    }

    // Render the stored value. In feet mode show feet-and-inches, rounding the
    // fractional part to the nearest whole inch (e.g. 5.5 -> 5' 6").
    QString textFromValue(double value) const override
    {
        if (!m_feetMode)
            return QDoubleSpinBox::textFromValue(value);

        const bool neg = value < 0;
        const double v = qAbs(value);
        int totalInches = int(qRound(v * 12.0));
        const int feet = totalInches / 12;
        const int inches = totalInches % 12;

        QString s = inches == 0
            ? QStringLiteral("%1'").arg(feet)
            : QStringLiteral("%1' %2\"").arg(feet).arg(inches);
        return neg ? QStringLiteral("-") + s : s;
    }

    double valueFromText(const QString &text) const override
    {
        if (!m_feetMode)
            return QDoubleSpinBox::valueFromText(text);
        return parseFeet(text);
    }

    // Be permissive while editing; valueFromText + the spin box range clamp the
    // final result, so any partially-typed feet/inches string is acceptable.
    QValidator::State validate(QString &input, int &pos) const override
    {
        if (!m_feetMode)
            return QDoubleSpinBox::validate(input, pos);
        Q_UNUSED(input)
        Q_UNUSED(pos)
        return QValidator::Acceptable;
    }

private:
    // Accepts: "5.5", "5", "5'", "5' 6\"", "5'6", "5 6", "6\"",
    // "5ft 6in", and Unicode prime variants (5′ 6″).
    double parseFeet(const QString &raw) const
    {
        QString t = raw.trimmed().toLower();
        t.replace(QChar(0x2019), '\'');   // right single quote
        t.replace(QChar(0x2032), '\'');   // prime
        t.replace(QChar(0x201D), '"');    // right double quote
        t.replace(QChar(0x2033), '"');    // double prime
        if (t.isEmpty())
            return 0.0;

        bool neg = t.startsWith('-');
        if (neg)
            t = t.mid(1).trimmed();

        double feet = 0.0;
        double inches = 0.0;
        const bool hasMark = t.contains('\'') || t.contains('"')
                          || t.contains(QStringLiteral("ft"))
                          || t.contains(QStringLiteral("in"));

        if (hasMark)
        {
            QRegularExpression reFt(QStringLiteral("([0-9]*\\.?[0-9]+)\\s*(?:'|ft|feet)"));
            QRegularExpression reIn(QStringLiteral("([0-9]*\\.?[0-9]+)\\s*(?:\"|in|inch|inches)"));
            QRegularExpressionMatch mFt = reFt.match(t);
            QRegularExpressionMatch mIn = reIn.match(t);
            if (mFt.hasMatch())
                feet = mFt.captured(1).toDouble();
            if (mIn.hasMatch())
                inches = mIn.captured(1).toDouble();
        }
        else
        {
            const QStringList parts = t.split(QRegularExpression(QStringLiteral("\\s+")),
                                              Qt::SkipEmptyParts);
            if (parts.size() >= 2)
            {
                feet = parts.at(0).toDouble();
                inches = parts.at(1).toDouble();
            }
            else
            {
                feet = t.toDouble();   // plain decimal feet
            }
        }

        const double val = feet + inches / 12.0;
        return neg ? -val : val;
    }

    bool m_feetMode;
};

/** @} */

#endif // FEETINCHESSPINBOX_H
