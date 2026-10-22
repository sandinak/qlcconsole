/*
  Q Light Controller Plus
  palettenaming.cpp

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
#include <QHash>
#include <QPair>
#include <cmath>
#include <climits>
#include <algorithm>

#include "palettenaming.h"

namespace
{

struct NamedColour { const char *name; int r, g, b; };

/* Base hues with a dark and a light variant each, the whites a rig is
   asked for, and black. Nearest by CIE Lab distance, so "dark" and "light"
   fall where the eye puts them, not where RGB distance does. */
const NamedColour kColours[] = {
    { "Red",          255,   0,   0 }, { "Dark Red",      128,   0,   0 }, { "Light Red",     255, 110, 110 },
    { "Orange",       255, 120,   0 }, { "Dark Orange",   170,  70,   0 }, { "Light Orange",  255, 180, 110 },
    { "Amber",        255, 180,   0 },
    { "Yellow",       255, 255,   0 }, { "Dark Yellow",   140, 140,   0 }, { "Light Yellow",  255, 255, 150 },
    { "Lime",         150, 255,   0 },
    { "Green",          0, 255,   0 }, { "Dark Green",      0, 110,   0 }, { "Light Green",   140, 255, 140 },
    { "Teal",           0, 255, 150 },
    { "Cyan",           0, 255, 255 }, { "Dark Cyan",       0, 128, 128 }, { "Light Cyan",    150, 255, 255 },
    { "Sky Blue",       0, 160, 255 },
    { "Blue",           0,   0, 255 }, { "Dark Blue",       0,   0, 128 }, { "Light Blue",    110, 140, 255 },
    { "Purple",       128,   0, 255 }, { "Dark Purple",    70,   0, 140 }, { "Light Purple",  190, 140, 255 },
    { "Magenta",      255,   0, 255 }, { "Dark Magenta",  128,   0, 128 }, { "Light Magenta", 255, 140, 255 },
    { "Pink",         255,   0, 128 }, { "Dark Pink",     140,   0,  70 }, { "Light Pink",    255, 170, 200 },
    { "White",        255, 255, 255 }, { "Warm White",    255, 210, 150 }, { "Cool White",    210, 225, 255 },
    { "Grey",         128, 128, 128 }, { "Dark Grey",      64,  64,  64 }, { "Light Grey",    192, 192, 192 },
    { "Black",          0,   0,   0 },
};

/* Recognised in a name, never given for an RGB value: the extra emitters
   alone, and the spellings an older table or a person may have used. */
const NamedColour kAliases[] = {
    { "UV",            90,   0, 200 },
    { "Violet",       128,   0, 255 },
    { "Gray",         128, 128, 128 },
    { "Dark Gray",     64,  64,  64 },
    { "Light Gray",   192, 192, 192 },
};

/* How far (CIE76) a colour word in a name may be from the old value and
   still be taken as saying it: wide enough for a name an older, coarser
   table gave ("Blue" on a dark blue), narrow enough that a word for some
   other colour is left alone. */
const double kSaysTheColour = 40.0;

struct Lab { double L, a, b; };

double linear(int c)
{
    const double v = c / 255.0;
    return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}

double labF(double t)
{
    return t > 0.008856 ? std::cbrt(t) : 7.787 * t + 16.0 / 116.0;
}

Lab toLab(const QColor &c)
{
    const double r = linear(c.red()), g = linear(c.green()), b = linear(c.blue());
    const double x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047;
    const double y = (0.2126 * r + 0.7152 * g + 0.0722 * b);
    const double z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883;
    const double fx = labF(x), fy = labF(y), fz = labF(z);
    return { 116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz) };
}

double deltaE(const QColor &a, const QColor &b)
{
    const Lab p = toLab(a), q = toLab(b);
    return std::sqrt((p.L - q.L) * (p.L - q.L) + (p.a - q.a) * (p.a - q.a) + (p.b - q.b) * (p.b - q.b));
}

/** @a phrase anywhere in a name as whole words, any case, any spacing. */
QRegularExpression wholeWords(const QString &phrase)
{
    QStringList words = phrase.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (QString &w : words)
        w = QRegularExpression::escape(w);
    return QRegularExpression(QStringLiteral("(?<![\\w])") + words.join(QStringLiteral("\\s+"))
                              + QStringLiteral("(?![\\w])"),
                              QRegularExpression::CaseInsensitiveOption);
}

/** @a replacement written the way @a was is: all lower, all upper, or as
 *  the table has it. */
QString inCaseOf(const QString &was, const QString &replacement)
{
    if (was == was.toLower() && was != was.toUpper())
        return replacement.toLower();
    if (was == was.toUpper() && was != was.toLower() && was.size() > 2)
        return replacement.toUpper();
    return replacement;
}

/** A colour name without its "Dark" / "Light": the hue it is a shade of. */
QString family(const QString &name)
{
    static const QRegularExpression shade(QStringLiteral("^(dark|light)\\s+"),
                                          QRegularExpression::CaseInsensitiveOption);
    QString f = name;
    return f.remove(shade);
}

struct Part
{
    int at = -1;
    int length = 0;
    QString phrase;     //!< the table's spelling (or the macro)
};

/** Every colour word in @a name, longest first where they overlap
 *  ("Dark Red" is one part, not "Red" in it). */
QList<Part> colourParts(const QString &name, const QString &macro = QString())
{
    QStringList phrases;
    if (!macro.isEmpty())
        phrases << macro;
    phrases << PaletteNaming::colourNames();
    QList<Part> parts;
    QVector<bool> taken(name.size(), false);
    for (const QString &ph : phrases)
    {
        QRegularExpressionMatchIterator it = wholeWords(ph).globalMatch(name);
        while (it.hasNext())
        {
            const QRegularExpressionMatch m = it.next();
            bool free = true;
            for (int i = int(m.capturedStart()); i < int(m.capturedEnd()); ++i)
                free = free && !taken[i];
            if (!free)
                continue;
            for (int i = int(m.capturedStart()); i < int(m.capturedEnd()); ++i)
                taken[i] = true;
            Part p;
            p.at = int(m.capturedStart());
            p.length = int(m.capturedLength());
            p.phrase = ph;
            parts << p;
        }
    }
    return parts;
}

struct Level
{
    int at = -1;
    int length = 0;
    int percent = -1;   //!< -1: a word
    QString word;       //!< "Full", "Off", "Max" as written
};

/** Every level in @a name: "45%", "45 %", and the words @a words. */
QList<Level> levelParts(const QString &name, const QStringList &words)
{
    QList<Level> out;
    static const QRegularExpression pct(QStringLiteral("(?<![\\w.])(\\d{1,3})\\s?%"));
    QRegularExpressionMatchIterator it = pct.globalMatch(name);
    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        Level l;
        l.at = int(m.capturedStart());
        l.length = int(m.capturedLength());
        l.percent = m.captured(1).toInt();
        out << l;
    }
    for (const QString &w : words)
    {
        QRegularExpressionMatchIterator wi = wholeWords(w).globalMatch(name);
        while (wi.hasNext())
        {
            const QRegularExpressionMatch m = wi.next();
            Level l;
            l.at = int(m.capturedStart());
            l.length = int(m.capturedLength());
            l.word = m.captured(0);
            out << l;
        }
    }
    return out;
}

/** The level-following both dimmer and strobe use: @a oldPct / @a newPct
 *  with @a low / @a high the words for the ends (0 and 100), @a lead the
 *  word an auto name puts before a percentage ("Intensity"; empty: none). */
QString followLevel(const QString &name, int oldPct, int newPct, const QString &low,
                    const QString &high, const QString &lead)
{
    if (oldPct == newPct)
        return QString();
    const QString oldWord = oldPct <= 0 ? low : (oldPct >= 100 ? high : QString());
    const QString newToken = newPct <= 0 ? low : (newPct >= 100 ? high : QStringLiteral("%1%").arg(newPct));
    const bool newIsWord = newPct <= 0 || newPct >= 100;

    const QList<Level> levels = levelParts(name, { low, high });
    const Level *hit = nullptr;
    for (const Level &l : levels)
    {
        const bool says = l.percent >= 0 ? qAbs(l.percent - oldPct) <= 1
                                         : (!oldWord.isEmpty() && l.word.compare(oldWord, Qt::CaseInsensitive) == 0);
        if (says)
        {
            hit = &l;
            break;
        }
    }
    if (hit == nullptr)
        return QString();

    int at = hit->at, length = hit->length;
    QString replacement = newToken;
    if (hit->percent < 0)
        replacement = inCaseOf(hit->word, newToken);
    if (!lead.isEmpty())
    {
        // "Intensity 45%" -> "Full": the lead word goes with the number...
        static const QRegularExpression before(QStringLiteral("(\\w+)\\s+$"));
        const QRegularExpressionMatch m = before.match(name.left(at));
        if (newIsWord && hit->percent >= 0 && m.hasMatch()
            && m.captured(1).compare(lead, Qt::CaseInsensitive) == 0)
        {
            length += at - int(m.capturedStart());
            at = int(m.capturedStart());
        }
        // ...and "Full" alone -> "Intensity 60%" comes back with it.
        else if (!newIsWord && hit->percent < 0 && name.trimmed().size() == hit->length)
            replacement = lead + QLatin1Char(' ') + newToken;
    }
    QString out = name;
    out.replace(at, length, replacement);
    return out == name ? QString() : out;
}

} // namespace

/****************************************************************************
 * Colour
 ****************************************************************************/

QString PaletteNaming::colourName(const QColor &rgb, const QColor &wauv)
{
    const QColor c = rgb.isValid() ? rgb : QColor(Qt::black);
    if (qMax(c.red(), qMax(c.green(), c.blue())) == 0 && wauv.isValid())
    {
        // Only the extra emitters: the brightest names it.
        const int w = wauv.red(), a = wauv.green(), uv = wauv.blue();
        if (w > 0 && w >= a && w >= uv)
            return QStringLiteral("White");
        if (a > 0 && a >= uv)
            return QStringLiteral("Amber");
        if (uv > 0)
            return QStringLiteral("UV");
    }
    double best = 1e9;
    QString name;
    for (const NamedColour &e : kColours)
    {
        const double d = deltaE(c, QColor(e.r, e.g, e.b));
        if (d < best)
        {
            best = d;
            name = QString::fromLatin1(e.name);
        }
    }
    return name;
}

QStringList PaletteNaming::colourNames()
{
    static QStringList names;
    if (names.isEmpty())
    {
        for (const NamedColour &e : kColours)
            names << QString::fromLatin1(e.name);
        for (const NamedColour &e : kAliases)
            names << QString::fromLatin1(e.name);
        std::stable_sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
            return a.size() > b.size();
        });
    }
    return names;
}

QColor PaletteNaming::namedColour(const QString &name)
{
    for (const NamedColour &e : kColours)
        if (name.compare(QLatin1String(e.name), Qt::CaseInsensitive) == 0)
            return QColor(e.r, e.g, e.b);
    for (const NamedColour &e : kAliases)
        if (name.compare(QLatin1String(e.name), Qt::CaseInsensitive) == 0)
            return QColor(e.r, e.g, e.b);
    return QColor();
}

QString PaletteNaming::followColour(const QString &name, const QColor &oldRgb, const QColor &oldWauv,
                                    const QString &oldMacro, const QString &newColour)
{
    if (name.trimmed().isEmpty() || newColour.isEmpty())
        return QString();
    const QString oldName = colourName(oldRgb, oldWauv);
    const QColor old = oldRgb.isValid() ? oldRgb : QColor(Qt::black);

    const Part *best = nullptr;
    double bestScore = 1e9;
    const QList<Part> parts = colourParts(name, oldMacro);
    for (const Part &p : parts)
    {
        double score;
        if (p.phrase.compare(oldName, Qt::CaseInsensitive) == 0
            || (!oldMacro.isEmpty() && p.phrase == oldMacro))
            score = 0.0;
        else if (family(p.phrase).compare(family(oldName), Qt::CaseInsensitive) == 0)
            score = 1.0;        // "Blue" on what is now called "Dark Blue"
        else
        {
            const QColor said = namedColour(p.phrase);
            if (!said.isValid())
                continue;
            score = deltaE(said, old);
        }
        // Equal: the later one (a show's prefix comes first, "02-Blue Moon Red").
        if (score <= bestScore)
        {
            bestScore = score;
            best = &p;
        }
    }
    if (best == nullptr || bestScore > kSaysTheColour)
        return QString();

    const QString was = name.mid(best->at, best->length);
    if (was.compare(newColour, Qt::CaseInsensitive) == 0)
        return QString();
    QString out = name;
    out.replace(best->at, best->length, inCaseOf(was, newColour));
    return out;
}

/****************************************************************************
 * Levels
 ****************************************************************************/

QString PaletteNaming::dimmerName(int value)
{
    if (value >= 255)
        return QStringLiteral("Full");
    if (value <= 0)
        return QStringLiteral("Off");
    return QStringLiteral("Intensity %1%").arg(qBound(1, qRound(value * 100.0 / 255.0), 99));
}

static int dimmerPercent(int value)
{
    if (value >= 255)
        return 100;
    if (value <= 0)
        return 0;
    return qBound(1, qRound(value * 100.0 / 255.0), 99);
}

QString PaletteNaming::followDimmer(const QString &name, int oldValue, int newValue)
{
    return followLevel(name, dimmerPercent(oldValue), dimmerPercent(newValue),
                       QStringLiteral("Off"), QStringLiteral("Full"), QStringLiteral("Intensity"));
}

QString PaletteNaming::strobeName(int percent)
{
    if (percent <= 0)
        return QStringLiteral("Strobe Off");
    if (percent >= 100)
        return QStringLiteral("Strobe Max");
    return QStringLiteral("Strobe %1%").arg(percent);
}

QString PaletteNaming::followStrobe(const QString &name, int oldPercent, int newPercent)
{
    return followLevel(name, qBound(0, oldPercent, 100), qBound(0, newPercent, 100),
                       QStringLiteral("Off"), QStringLiteral("Max"), QString());
}

/****************************************************************************
 * Named states
 ****************************************************************************/

QString PaletteNaming::followPart(const QString &name, const QString &oldPart, const QString &newPart)
{
    const QString o = oldPart.trimmed(), n = newPart.trimmed();
    if (o.isEmpty() || n.isEmpty() || o.compare(n, Qt::CaseInsensitive) == 0)
        return QString();
    const QRegularExpressionMatch m = wholeWords(o).match(name);
    if (!m.hasMatch())
        return QString();
    QString out = name;
    out.replace(int(m.capturedStart()), int(m.capturedLength()), inCaseOf(m.captured(0), n));
    return out;
}

/****************************************************************************
 * Factory names and conventions
 ****************************************************************************/

bool PaletteNaming::isFactoryName(const QString &name, const QString &typeName)
{
    const QString n = name.trimmed();
    if (n.isEmpty())
        return true;
    const QRegularExpression re(QStringLiteral("^New\\s+%1(\\s+\\d+)?$").arg(QRegularExpression::escape(typeName)),
                                QRegularExpression::CaseInsensitiveOption);
    return re.match(n).hasMatch();
}

QString PaletteNaming::withConvention(const QString &part, const QStringList &siblings, bool colour)
{
    if (part.isEmpty())
        return QString();
    QHash<QString, int> counts;     // "prefix\nsuffix" -> siblings
    int named = 0;
    for (const QString &s : siblings)
    {
        int at = -1, length = 0;
        if (colour)
        {
            const QList<Part> parts = colourParts(s);
            if (parts.size() != 1)
                continue;           // none, or which one is it?
            at = parts.first().at;
            length = parts.first().length;
        }
        else
        {
            const QList<Level> levels = levelParts(s, { QStringLiteral("Off"), QStringLiteral("Full") });
            if (levels.size() != 1)
                continue;
            at = levels.first().at;
            length = levels.first().length;
        }
        ++named;
        counts[s.left(at) + QLatin1Char('\n') + s.mid(at + length)]++;
    }
    QString key;
    int most = 0;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
        if (it.value() > most || (it.value() == most && it.key() < key))
        {
            most = it.value();
            key = it.key();
        }
    // Two at least, and most of those that name it: a convention, not a fluke.
    if (most < 2 || most * 2 <= named || key == QStringLiteral("\n"))
        return QString();
    const int nl = int(key.indexOf(QLatin1Char('\n')));
    return key.left(nl) + part + key.mid(nl + 1);
}
