/*
  Q Light Controller Plus
  measurementunits.cpp

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

#include <QRegularExpression>
#include <QSettings>
#include <QtMath>
#include <cmath>

#include "measurementunits.h"
#include "qlcfile.h"

namespace
{
    /* Where the units were kept before they joined the rest (SA3): on
       macOS, "qlcplus" made it com.qlcplus.qlcconsole, beside every other
       setting in org.qlcplus.qlcconsole. Moved over once, then gone from
       there. Elsewhere it is the same place. */
    void moveOldUnits(QSettings &to, bool always = false)
    {
#if defined(Q_OS_DARWIN)
        static bool s_done = false;     // once a run: it is read often
        if (s_done && !always)
            return;
        s_done = true;
        QSettings from(to.format(), QSettings::UserScope,
                       QStringLiteral("qlcplus"), QStringLiteral("qlcconsole"));
        if (from.fileName() == to.fileName())
            return;
        from.beginGroup(QStringLiteral("units"));
        const QStringList keys = from.childKeys();
        from.endGroup();
        if (keys.isEmpty())
            return;
        for (const QString &k : keys)
        {
            const QString key = QStringLiteral("units/") + k;
            if (!to.contains(key))
                to.setValue(key, from.value(key));
        }
        from.remove(QStringLiteral("units"));
#else
        Q_UNUSED(to)
        Q_UNUSED(always)
#endif
    }

    /* qlcconsole's own settings, whichever app is asking: the fixture editor
       names itself differently, so a plain QSettings() there would read an
       unrelated file. The default format, not Native, so a test that points
       QSettings at a temporary INI gets this one too. */
#define APP_SETTINGS(var) QSettings var(QSettings::defaultFormat(), QSettings::UserScope, \
                                        QLCFile::settingsOrganisation(), QStringLiteral("qlcconsole")); \
                          moveOldUnits(var)

    double roundTo(double v, int decimals)
    {
        const double p = std::pow(10.0, decimals);
        return std::round(v * p) / p;
    }

    QString number(double v, int decimals)
    {
        QString s = QString::number(v, 'f', decimals);
        if (s.contains(QLatin1Char('.')))
        {
            while (s.endsWith(QLatin1Char('0')))
                s.chop(1);
            if (s.endsWith(QLatin1Char('.')))
                s.chop(1);
        }
        if (s == QLatin1String("-0"))
            s = QStringLiteral("0");
        return s;
    }

    bool isWhole(double v)
    {
        return qAbs(v - std::round(v)) < 1e-6;
    }

    /* The fewest decimals (up to maxDecimals) of value/per that still mean
       the same whole millimetre (or the same gram for weights) when it
       comes back. For a value that isn't whole, just its precision. */
    double shortest(double base, double per, int maxDecimals, int *decimals)
    {
        const double v = base / per;
        if (isWhole(base))
        {
            for (int d = 0; d < maxDecimals; d++)
            {
                const double c = roundTo(v, d);
                if (MeasurementUnits::toStoredMm(c * per) == qRound64(base))
                {
                    *decimals = d;
                    return c;
                }
            }
        }
        *decimals = maxDecimals;
        return roundTo(v, maxDecimals);
    }

    QString normalise(const QString &text)
    {
        QString t = text.trimmed().toLower();
        t.replace(QChar(0x2019), QLatin1Char('\''));   // right single quote
        t.replace(QChar(0x2032), QLatin1Char('\''));   // prime
        t.replace(QChar(0x201D), QLatin1Char('"'));    // right double quote
        t.replace(QChar(0x201C), QLatin1Char('"'));    // left double quote
        t.replace(QChar(0x2033), QLatin1Char('"'));    // double prime
        t.replace(QStringLiteral("''"), QStringLiteral("\""));
        // A tape measure's fractions: ½ ¼ ¾ ⅛ ⅜ ⅝ ⅞, then 11 1/4, 11-1/4, 3/8.
        static const QList<QPair<QChar, QString>> vulgar = {
            { QChar(0x00BD), QStringLiteral(" 1/2") }, { QChar(0x00BC), QStringLiteral(" 1/4") },
            { QChar(0x00BE), QStringLiteral(" 3/4") }, { QChar(0x215B), QStringLiteral(" 1/8") },
            { QChar(0x215C), QStringLiteral(" 3/8") }, { QChar(0x215D), QStringLiteral(" 5/8") },
            { QChar(0x215E), QStringLiteral(" 7/8") } };
        for (const auto &v : vulgar)
            t.replace(v.first, v.second);
        static const QRegularExpression mixed(QStringLiteral("(\\d+(?:\\.\\d+)?)(?:\\s+|\\s*-\\s*)(\\d+)\\s*/\\s*(\\d+)"));
        static const QRegularExpression frac(QStringLiteral("(\\d+)\\s*/\\s*(\\d+)"));
        auto expand = [&t](const QRegularExpression &re, bool whole) {
            QRegularExpressionMatch m;
            int from = 0;
            while ((m = re.match(t, from)).hasMatch())
            {
                const int k = whole ? 1 : 0;
                const double num = m.captured(1 + k).toDouble(), den = m.captured(2 + k).toDouble();
                if (den == 0.0)
                    return;     // left as it is: it reads as malformed
                const double v = (whole ? m.captured(1).toDouble() : 0.0) + num / den;
                const QString rep = QString::number(v, 'g', 12);
                t.replace(m.capturedStart(), m.capturedLength(), rep);
                from = m.capturedStart() + rep.size();
            }
        };
        expand(mixed, true);
        expand(frac, false);
        return t.trimmed();
    }

    /* Inches to the nearest eighth, the way a tape measure reads: 15 3/4,
       7 1/2, 3/8, 12. Fractions reduced. */
    QString eighths(qint64 total)
    {
        const qint64 whole = total / 8;
        int e = int(total % 8);
        if (e == 0)
            return QString::number(whole);
        int den = 8;
        while (e % 2 == 0) { e /= 2; den /= 2; }
        const QString f = QStringLiteral("%1/%2").arg(e).arg(den);
        return whole == 0 ? f : QStringLiteral("%1 %2").arg(whole).arg(f);
    }

    /* Splits "6' 3\"", "12kg 400 g", "7,5 in" into (number, unit) terms.
       A comma is a decimal point only before one or two digits, so "1,200"
       is refused rather than read as 1.2. False on anything else. */
    bool terms(const QString &t, QList<QPair<double, QString>> &out)
    {
        static const QRegularExpression re(QStringLiteral(
            "\\s*(\\d+(?:\\.\\d*|,\\d{1,2}(?!\\d))?|\\.\\d+)\\s*([a-z]+\\.?|\"|')?"));
        int pos = 0;
        while (pos < t.length())
        {
            const QRegularExpressionMatch m = re.match(t, pos, QRegularExpression::NormalMatch,
                                                        QRegularExpression::AnchorAtOffsetMatchOption);
            if (!m.hasMatch() || m.capturedLength() == 0)
                return false;
            QString num = m.captured(1);
            num.replace(QLatin1Char(','), QLatin1Char('.'));
            bool ok = false;
            const double v = num.toDouble(&ok);
            if (!ok)
                return false;
            QString unit = m.captured(2);
            if (unit.endsWith(QLatin1Char('.')))
                unit.chop(1);
            out.append(qMakePair(v, unit));
            pos = m.capturedEnd();
            while (pos < t.length() && t.at(pos).isSpace())
                pos++;
        }
        return !out.isEmpty();
    }

    /* Millimetres per named length unit; 0 for an unknown one */
    double lengthFactor(const QString &u)
    {
        static const QStringList mm = { "mm", "millimeter", "millimeters", "millimetre", "millimetres" };
        static const QStringList cm = { "cm", "centimeter", "centimeters", "centimetre", "centimetres" };
        static const QStringList m  = { "m", "meter", "meters", "metre", "metres" };
        static const QStringList in = { "in", "inch", "inches", "\"" };
        static const QStringList ft = { "ft", "foot", "feet", "'" };
        if (mm.contains(u)) return 1.0;
        if (cm.contains(u)) return 10.0;
        if (m.contains(u))  return 1000.0;
        if (in.contains(u)) return 25.4;
        if (ft.contains(u)) return 304.8;
        return 0.0;
    }

    /* Kilograms per named weight unit; 0 for an unknown one */
    double weightFactor(const QString &u)
    {
        static const QStringList kg = { "kg", "kgs", "kilo", "kilos", "kilogram", "kilograms" };
        static const QStringList g  = { "g", "gram", "grams" };
        static const QStringList lb = { "lb", "lbs", "pound", "pounds" };
        static const QStringList oz = { "oz", "ounce", "ounces" };
        if (kg.contains(u)) return 1.0;
        if (g.contains(u))  return 0.001;
        if (lb.contains(u)) return 0.45359237;
        if (oz.contains(u)) return 0.45359237 / 16.0;
        return 0.0;
    }

    /* Sum the terms. A bare number is in bareFactor when it's all there is,
       or in followFactor (inches, ounces) straight after a term in
       bigFactor (feet, pounds): 6' 3, 5 lb 4. */
    double parse(const QString &text, double bareFactor, double (*factor)(const QString &),
                 double bigFactor, double followFactor, bool *ok)
    {
        *ok = false;
        const QString t = normalise(text);
        if (t.isEmpty())
        {
            *ok = true;
            return 0.0;
        }
        QList<QPair<double, QString>> list;
        if (!terms(t, list))
            return 0.0;

        double total = 0.0;
        double prev = 0.0;
        for (int i = 0; i < list.size(); i++)
        {
            double f;
            if (list.at(i).second.isEmpty())
            {
                if (list.size() == 1)
                    f = bareFactor;
                else if (i == list.size() - 1 && prev == bigFactor)
                    f = followFactor;
                else
                    return 0.0;
            }
            else
            {
                f = factor(list.at(i).second);
                if (f == 0.0)
                    return 0.0;
            }
            total += list.at(i).first * f;
            prev = f;
        }
        *ok = true;
        return total;
    }
}

/*****************************************************************************
 * Preference
 *****************************************************************************/

void MeasurementUnits::moveOldSettings()
{
    QSettings s(QSettings::defaultFormat(), QSettings::UserScope,
                QLCFile::settingsOrganisation(), QStringLiteral("qlcconsole"));
    moveOldUnits(s, true);
}

QString MeasurementUnits::settingsKey()
{
    return QStringLiteral("units/length");
}

MeasurementUnits::LengthUnit MeasurementUnits::preferredLength()
{
    APP_SETTINGS(s);
    return fromKey(s.value(settingsKey()).toString(), Feet);
}

void MeasurementUnits::setPreferredLength(LengthUnit unit)
{
    APP_SETTINGS(s);
    s.setValue(settingsKey(), key(unit));
}

bool MeasurementUnits::isPreferenceSet()
{
    APP_SETTINGS(s);
    return s.contains(settingsKey());
}

bool MeasurementUnits::seedFromFirstShow(bool showInFeet)
{
    APP_SETTINGS(s);
    const QString seededKey = QStringLiteral("units/seededFromShow");
    if (s.value(seededKey, false).toBool())
        return false;
    s.setValue(seededKey, true);
    if (s.contains(settingsKey()) || !showInFeet)
        return false;
    s.setValue(settingsKey(), key(Feet));
    return true;
}

bool MeasurementUnits::isImperial(LengthUnit unit)
{
    return unit == Inches || unit == Feet;
}

MeasurementUnits::WeightUnit MeasurementUnits::weightUnitFor(LengthUnit unit)
{
    return isImperial(unit) ? Pounds : Kilograms;
}

QString MeasurementUnits::symbol(LengthUnit unit)
{
    switch (unit)
    {
        case Centimetres: return QStringLiteral("cm");
        case Metres:      return QStringLiteral("m");
        case Inches:      return QStringLiteral("in");
        case Feet:        return QStringLiteral("ft");
        default:          return QStringLiteral("mm");
    }
}

QString MeasurementUnits::symbol(WeightUnit unit)
{
    return unit == Pounds ? QStringLiteral("lb") : QStringLiteral("kg");
}

QString MeasurementUnits::name(LengthUnit unit)
{
    switch (unit)
    {
        case Centimetres: return tr("Centimetres (cm, kg)");
        case Metres:      return tr("Metres (m, kg)");
        case Inches:      return tr("Inches (in, lb)");
        case Feet:        return tr("Feet and inches (ft, lb)");
        default:          return tr("Millimetres (mm, kg)");
    }
}

QString MeasurementUnits::key(LengthUnit unit)
{
    return symbol(unit);
}

MeasurementUnits::LengthUnit MeasurementUnits::fromKey(const QString &key, LengthUnit fallback)
{
    for (LengthUnit u : { Millimetres, Centimetres, Metres, Inches, Feet })
        if (key == symbol(u))
            return u;
    return fallback;
}

double MeasurementUnits::mmPer(LengthUnit unit)
{
    switch (unit)
    {
        case Centimetres: return 10.0;
        case Metres:      return 1000.0;
        case Inches:      return 25.4;
        case Feet:        return 304.8;
        default:          return 1.0;
    }
}

double MeasurementUnits::kgPer(WeightUnit unit)
{
    return unit == Pounds ? 0.45359237 : 1.0;
}

int MeasurementUnits::toStoredMm(double mm)
{
    // Half up, and past the float noise of adding feet and inches back up
    // (26' 10.5" is 8191.4999... or 8191.5000...1 depending on the sum).
    return qRound(mm + 1e-6);
}

/*****************************************************************************
 * Parsing
 *****************************************************************************/

double MeasurementUnits::parseLength(const QString &text, LengthUnit bareUnit, bool *ok)
{
    bool dummy;
    if (ok == nullptr)
        ok = &dummy;
    return parse(text, mmPer(bareUnit), lengthFactor, 304.8, 25.4, ok);
}

double MeasurementUnits::parseWeight(const QString &text, WeightUnit bareUnit, bool *ok)
{
    bool dummy;
    if (ok == nullptr)
        ok = &dummy;
    return parse(text, kgPer(bareUnit), weightFactor, 0.45359237, 0.45359237 / 16.0, ok);
}

/*****************************************************************************
 * Formatting
 *****************************************************************************/

QString MeasurementUnits::formatLength(double mm)
{
    return formatLength(mm, preferredLength());
}

QString MeasurementUnits::formatMetres(double metres)
{
    return formatLength(metres * 1000.0, preferredLength());
}

MeasurementUnits::LengthUnit MeasurementUnits::speedUnitFor(LengthUnit unit)
{
    return isImperial(unit) ? Feet : Metres;
}

QString MeasurementUnits::speedSymbol(LengthUnit unit)
{
    return symbol(speedUnitFor(unit)) + QStringLiteral("/s");
}

QString MeasurementUnits::formatSpeed(double metresPerSecond, LengthUnit unit)
{
    const LengthUnit u = speedUnitFor(unit);
    const int d = u == Feet ? 1 : 2;
    const double v = roundTo(metresPerSecond * 1000.0 / mmPer(u), d);
    return QStringLiteral("%1 %2").arg(number(v, d), speedSymbol(unit));
}

QString MeasurementUnits::formatSpeed(double metresPerSecond)
{
    return formatSpeed(metresPerSecond, preferredLength());
}

double MeasurementUnits::parseSpeed(const QString &text, LengthUnit unit, bool *ok)
{
    QString t = text.trimmed();
    // A length a second: "/s", "/sec", " per second" off the end.
    static const QRegularExpression perSecond(QStringLiteral("\\s*(?:/\\s*s(?:ec(?:ond)?)?|per\\s+second)$"),
                                              QRegularExpression::CaseInsensitiveOption);
    t.remove(perSecond);
    const double mm = parseLength(t, speedUnitFor(unit), ok);
    return mm / 1000.0;
}

QString MeasurementUnits::formatLength(double mm, LengthUnit unit)
{
    // An offset (stage left of centre, below the deck) keeps its sign.
    if (mm < 0.0)
    {
        const QString pos = formatLength(-mm, unit);
        return pos.startsWith(QLatin1Char('0')) && !pos.contains(QRegularExpression(QStringLiteral("[1-9]")))
               ? pos : QStringLiteral("-") + pos;
    }
    int d = 0;
    if (unit == Feet || unit == Inches)
    {
        // To the nearest 1/8", as a tape measure reads: 3' 11 1/4", 15 3/4".
        const qint64 total = qRound64(mm / 25.4 * 8.0);
        if (unit == Inches)
            return QStringLiteral("%1\"").arg(eighths(total));
        const qint64 feet = total / 96;
        const qint64 rest = total % 96;
        if (feet == 0)
            return QStringLiteral("%1\"").arg(eighths(rest));
        if (rest == 0)
            return QStringLiteral("%1'").arg(feet);
        return QStringLiteral("%1' %2\"").arg(feet).arg(eighths(rest));
    }

    int maxD = 1;
    switch (unit)
    {
        case Centimetres: maxD = 2; break;
        case Metres:      maxD = 4; break;
        default:          maxD = 1; break;
    }
    const double v = shortest(mm, mmPer(unit), maxD, &d);
    return QStringLiteral("%1 %2").arg(number(v, d), symbol(unit));
}

QString MeasurementUnits::formatWeight(double kg, WeightUnit unit)
{
    const int d = unit == Pounds ? 2 : 3;
    return QStringLiteral("%1 %2").arg(number(roundTo(kg / kgPer(unit), d), d), symbol(unit));
}

QString MeasurementUnits::lengthHelp(LengthUnit unit)
{
    return tr("A number in %1, or a length with its unit, converted for you: "
              "7 1/2\" or 7.5 in, 0.625' or 0.625 ft, 6' 3 1/4\", 190.5 mm, 19.05 cm, 0.19 m.")
            .arg(symbol(unit));
}

QString MeasurementUnits::weightHelp(WeightUnit unit)
{
    return tr("A number in %1, or a weight with its unit, converted for you: "
              "12 kg, 400 g, 26.5 lb (or lbs), 5 lb 4 oz.")
            .arg(symbol(unit));
}
