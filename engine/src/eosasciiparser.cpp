/*
  Q Light Controller
  eosasciiparser.cpp

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
#include <QStringDecoder>
#include <QFile>
#include <QSet>
#include <QtMath>

#include <algorithm>

#include "eosasciiparser.h"

/****************************************************************************
 * Plain data helpers
 ****************************************************************************/

const EosPersChannel *EosPersonality::channel(int param) const
{
    for (const EosPersChannel &c : channels)
        if (c.param == param)
            return &c;
    return nullptr;
}

QString EosShowData::paramName(int id) const
{
    if (paramTypes.contains(id))
        return paramTypes.value(id).name;
    if (id == 1)
        return QStringLiteral("Intens");
    return QStringLiteral("Param %1").arg(id);
}

const EosPalette *EosShowData::findPalette(QChar kind, const QString &number) const
{
    const QList<EosPalette> &list = kind == QLatin1Char('P') ? presets : palettes;
    const double n = number.toDouble();
    for (const EosPalette &p : list)
        if (p.kind == kind && qFuzzyCompare(p.number.toDouble() + 1, n + 1))
            return &p;
    return nullptr;
}

const EosPatch *EosShowData::findPatch(int channel) const
{
    for (const EosPatch &p : patch)
        if (p.channel == channel)
            return &p;
    return nullptr;
}

int EosShowData::cueCount() const
{
    int n = 0;
    for (const EosCueList &l : cueLists)
        n += int(l.cues.count());
    return n;
}

/****************************************************************************
 * Parser
 ****************************************************************************/

EosAsciiParser::EosAsciiParser()
{
}

EosAsciiParser::~EosAsciiParser()
{
}

bool EosAsciiParser::parse(const QString &path, EosShowData &data)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        data.warnings << QObject::tr("Could not open %1: %2").arg(path, file.errorString());
        return false;
    }
    const QByteArray bytes = file.readAll();
    // Eos writes plain ASCII (non-ASCII text goes in $$UText); be lenient
    // about a file someone saved from an editor as UTF-8 or Latin-1.
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString text = utf8.decode(bytes);
    if (utf8.hasError())
        text = QString::fromLatin1(bytes);
    return parseText(text, data);
}

double EosAsciiParser::parseTime(const QString &text)
{
    const QString t = text.trimmed();
    if (t.isEmpty())
        return -1;
    double total = 0;
    const QStringList parts = t.split(QLatin1Char(':'));
    for (const QString &p : parts)
    {
        bool ok = false;
        const double v = p.toDouble(&ok);
        if (!ok || v < 0)
            return -1;
        total = total * 60 + v;
    }
    return total;
}

EosLevel EosAsciiParser::parseLevel(const QString &text, bool percent)
{
    EosLevel l;
    l.raw = text;
    QString t = text.trimmed();
    // Assert / block flags (a b c k) change nothing about the level; null
    // and release (n r) are not moves at all.
    // Eos writes the flags in lower case, the references in upper case
    // ("b127", "kPR2", "CP10").
    if (!t.isEmpty() && (t.at(0) == QLatin1Char('n') || t.at(0) == QLatin1Char('r')))
    {
        l.kind = EosLevel::Release;
        return l;
    }
    if (!t.isEmpty() && QStringLiteral("abck").contains(t.at(0)))
        t = t.mid(1);

    const QString up = t.toUpper();
    static const QRegularExpression palRe(QStringLiteral("^([IFCB])P([0-9.]+)$"));
    static const QRegularExpression preRe(QStringLiteral("^PR([0-9.]+)$"));
    static const QRegularExpression gelRe(QStringLiteral("^[.:]?G[A-Z]*[0-9.]+#([0-9]+)$"));
    QRegularExpressionMatch m;
    if ((m = palRe.match(up)).hasMatch())
    {
        l.kind = EosLevel::PaletteRef;
        l.refKind = m.captured(1).at(0);
        l.ref = m.captured(2);
        return l;
    }
    if ((m = preRe.match(up)).hasMatch())
    {
        l.kind = EosLevel::PresetRef;
        l.refKind = QLatin1Char('P');
        l.ref = m.captured(1);
        return l;
    }
    if ((m = gelRe.match(up)).hasMatch())
    {
        // A gel match: the level after '#' is what it plays.
        l.value = m.captured(1).toInt();
        return l;
    }
    if (up == QLatin1String("FL") || up == QLatin1String("FULL"))
    {
        l.value = 255;
        return l;
    }
    if (up.startsWith(QLatin1Char('H')) && up.length() > 1)
    {
        bool ok = false;
        const int v = up.mid(1).toInt(&ok, 16);
        if (ok)
        {
            l.value = qBound(0, v, 255);
            return l;
        }
    }
    bool ok = false;
    const double v = t.toDouble(&ok);
    if (ok)
    {
        if (percent)
            l.value = qBound(0, int(qRound(v * 255.0 / 100.0)), 255);
        else
            l.value = qBound(0, int(qRound(v)), 65535);
        return l;
    }
    l.kind = EosLevel::Unsupported;
    return l;
}

QList<int> EosAsciiParser::parseChannelList(const QStringList &tokens)
{
    QList<int> out;
    for (int i = 0; i < tokens.count(); ++i)
    {
        const QString &t = tokens.at(i);
        if (t.compare(QLatin1String("thru"), Qt::CaseInsensitive) == 0 && !out.isEmpty() && i + 1 < tokens.count())
        {
            bool ok = false;
            const int to = tokens.at(i + 1).toInt(&ok);
            if (ok)
            {
                for (int c = out.last() + 1; c <= to && c - out.last() < 100000; ++c)
                    out << c;
                ++i;
            }
            continue;
        }
        const QStringList range = t.split(QLatin1Char('>'));
        bool ok = false;
        const int from = range.first().toInt(&ok);
        if (!ok)
            continue;
        if (range.count() == 2)
        {
            const int to = range.at(1).toInt();
            for (int c = from; c <= to && c - from < 100000; ++c)
                out << c;
        }
        else
        {
            out << from;
        }
    }
    return out;
}

namespace
{

enum class Ctx
{
    None,
    Personality,
    Patch,
    Group,
    CueList,
    Cue,
    Palette,
    Sub,
    Skip
};

/** "&#8647;&#8649;" -> the characters. */
QString decodeUText(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("&#(\\d+);"));
    QString out;
    int last = 0;
    QRegularExpressionMatchIterator it = re.globalMatch(s);
    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        out += s.mid(last, m.capturedStart() - last);
        const uint code = m.captured(1).toUInt();
        out += QString::fromUcs4(reinterpret_cast<const char32_t *>(&code), 1);
        last = m.capturedEnd();
    }
    out += s.mid(last);
    return out;
}

double cueValue(const QString &number)
{
    return number.toDouble();
}

} // namespace

bool EosAsciiParser::parseText(const QString &text, EosShowData &data)
{
    static const QRegularExpression splitRe(QStringLiteral("[\\s,]+"));

    Ctx ctx = Ctx::None;
    int currentList = 1;
    EosPersonality *pers = nullptr;
    EosPatch *patch = nullptr;
    EosGroup *group = nullptr;
    EosCue *cue = nullptr;
    EosPalette *pal = nullptr;
    EosSub *sub = nullptr;
    QString *label = nullptr;           // where Text / $$UText go
    int records = 0;
    bool ended = false;
    QHash<QString, QList<int>> cueParts;    // "list/number" -> parts seen
    QStringList unsupportedRefs;

    auto findCue = [&data](int list, const QString &number) -> EosCue * {
        EosCueList &cl = data.cueLists[list];
        cl.number = list;
        const double n = cueValue(number);
        for (EosCue &c : cl.cues)
            if (qFuzzyCompare(cueValue(c.number) + 1, n + 1))
                return &c;
        return nullptr;
    };
    auto addCue = [&data](int list, const QString &number) -> EosCue * {
        EosCueList &cl = data.cueLists[list];
        cl.number = list;
        EosCue c;
        c.number = number;
        c.list = list;
        const double n = cueValue(number);
        int at = 0;
        while (at < cl.cues.count() && cueValue(cl.cues.at(at).number) < n)
            ++at;
        cl.cues.insert(at, c);
        return &cl.cues[at];
    };
    // Pointers into QLists stay valid only until the next insert; every
    // record re-resolves its pointer when it starts, and nothing is inserted
    // into a list while one of its elements is current.
    auto readLevels = [&](const QStringList &tok, int from, EosLevels &into, int param, bool percent) {
        for (int i = from; i < tok.count(); ++i)
        {
            const int at = tok.at(i).indexOf(QLatin1Char('@'));
            if (at <= 0)
                continue;
            bool ok = false;
            const int left = tok.at(i).left(at).toInt(&ok);
            if (!ok)
                continue;
            const EosLevel lv = parseLevel(tok.at(i).mid(at + 1), percent);
            if (lv.kind == EosLevel::Unsupported)
                unsupportedRefs << tok.at(i);
            into[param < 0 ? left : param][param < 0 ? 1 : left] = lv;
        }
    };

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int ln = 0; ln < lines.count() && !ended; ++ln)
    {
        QString line = lines.at(ln);
        line.remove(QLatin1Char('\r'));
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('!')))
            continue;

        // The keyword, and the rest of the line as written (labels keep
        // their spacing).
        const int ws = trimmed.indexOf(QRegularExpression(QStringLiteral("[\\s,]")));
        const QString keyword = ws < 0 ? trimmed : trimmed.left(ws);
        const QString kw = keyword.toLower();
        QString rest = ws < 0 ? QString() : trimmed.mid(ws + 1).trimmed();

        if (kw == QLatin1String("text") || kw == QLatin1String("$$text"))
        {
            if (label != nullptr)
                *label = rest;
            continue;
        }
        if (kw == QLatin1String("$$utext"))
        {
            if (label != nullptr)
                *label = decodeUText(rest);
            continue;
        }

        // Anything else: a trailing "! comment" goes.
        const int bang = rest.indexOf(QLatin1Char('!'));
        if (bang >= 0)
            rest = rest.left(bang).trimmed();
        const QStringList tok = rest.split(splitRe, Qt::SkipEmptyParts);

        /* Primary records */
        if (kw == QLatin1String("ident"))
        {
            ++records;
            ctx = Ctx::None;
            label = nullptr;
            continue;
        }
        if (kw == QLatin1String("manufacturer"))
        {
            data.manufacturer = rest;
            continue;
        }
        if (kw == QLatin1String("console"))
        {
            data.console = rest;
            continue;
        }
        if (kw == QLatin1String("$$format"))
        {
            data.format = rest;
            continue;
        }
        if (kw == QLatin1String("$$software"))
        {
            data.software = rest;
            continue;
        }
        if (kw == QLatin1String("$$title"))
        {
            data.title = rest;
            continue;
        }
        if (kw == QLatin1String("clear") || kw == QLatin1String("set"))
        {
            ctx = Ctx::None;
            label = nullptr;
            continue;
        }
        if (kw == QLatin1String("enddata"))
        {
            ended = true;
            continue;
        }
        if (kw == QLatin1String("$paramtype"))
        {
            ++records;
            ctx = Ctx::None;
            label = nullptr;
            if (tok.count() >= 3)
            {
                EosParamType p;
                p.id = tok.at(0).toInt();
                p.category = tok.at(1).toInt();
                p.name = tok.at(2);
                data.paramTypes.insert(p.id, p);
            }
            continue;
        }
        if (kw == QLatin1String("$personality"))
        {
            ++records;
            ctx = Ctx::Personality;
            label = nullptr;
            EosPersonality p;
            p.id = tok.value(0).toInt();
            data.personalities.insert(p.id, p);
            pers = &data.personalities[p.id];
            continue;
        }
        if (kw == QLatin1String("$patch"))
        {
            ++records;
            ctx = Ctx::Patch;
            // channel, personality, address, address mode, part
            EosPatch p;
            p.channel = tok.value(0).toInt();
            p.personality = tok.value(1).toInt();
            const QString addr = tok.value(2);
            if (addr.contains(QLatin1Char('/')))
                p.address = (addr.section(QLatin1Char('/'), 0, 0).toInt() - 1) * 512
                            + addr.section(QLatin1Char('/'), 1, 1).toInt();
            else
                p.address = addr.toInt();
            p.part = tok.count() > 4 ? qMax(1, tok.at(4).toInt()) : 1;
            if (p.channel <= 0)
            {
                data.warnings << QObject::tr("Line %1: a $Patch record with no channel number, skipped.").arg(ln + 1);
                ctx = Ctx::Skip;
                label = nullptr;
                continue;
            }
            data.patch << p;
            patch = &data.patch.last();
            label = &patch->label;
            continue;
        }
        if (kw == QLatin1String("patch"))
        {
            // Conventional: "Patch <page> <chan><<dimmer>@<level> ..."
            ++records;
            ctx = Ctx::None;
            label = nullptr;
            static const QRegularExpression pr(QStringLiteral("(\\d+)<(\\d+)(?:@(\\S+))?"));
            QRegularExpressionMatchIterator it = pr.globalMatch(rest);
            while (it.hasNext())
            {
                const QRegularExpressionMatch m = it.next();
                EosPatch p;
                p.channel = m.captured(1).toInt();
                p.address = m.captured(2).toInt();
                data.patch << p;
            }
            continue;
        }
        if (kw == QLatin1String("$group") || kw == QLatin1String("group"))
        {
            ++records;
            ctx = Ctx::Group;
            EosGroup g;
            g.number = tok.value(0);
            data.groups << g;
            group = &data.groups.last();
            label = &group->label;
            continue;
        }
        if (kw == QLatin1String("$cuelist"))
        {
            ++records;
            ctx = Ctx::CueList;
            currentList = qMax(1, tok.value(0).toInt());
            EosCueList &cl = data.cueLists[currentList];
            cl.number = currentList;
            label = &cl.label;
            continue;
        }
        if (kw == QLatin1String("cue") || kw == QLatin1String("$cue"))
        {
            ++records;
            ctx = Ctx::Cue;
            QString number = tok.value(0);
            int part = 1;
            if (number.contains(QLatin1Char('/')))
            {
                part = qMax(1, number.section(QLatin1Char('/'), 1, 1).toInt());
                number = number.section(QLatin1Char('/'), 0, 0);
            }
            int list = currentList;
            if (tok.count() > 1)
                list = qMax(1, tok.at(1).toInt());
            bool ok = false;
            number.toDouble(&ok);
            if (!ok)
            {
                data.warnings << QObject::tr("Line %1: a cue with no number (%2), skipped.").arg(ln + 1).arg(trimmed);
                ctx = Ctx::Skip;
                label = nullptr;
                continue;
            }
            cue = findCue(list, number);
            if (cue == nullptr)
                cue = addCue(list, number);
            const QString key = QStringLiteral("%1/%2").arg(list).arg(cueValue(number));
            if (!cueParts[key].contains(part))
                cueParts[key] << part;
            cue->parts = int(cueParts.value(key).count());
            // A part's label is the part's; the cue keeps the first one.
            label = part == 1 || cue->label.isEmpty() ? &cue->label : nullptr;
            continue;
        }
        if (kw == QLatin1String("$intenspalette") || kw == QLatin1String("$focuspalette")
            || kw == QLatin1String("$colorpalette") || kw == QLatin1String("$beampalette")
            || kw == QLatin1String("$preset"))
        {
            ++records;
            ctx = Ctx::Palette;
            EosPalette p;
            p.kind = kw == QLatin1String("$preset") ? QLatin1Char('P') : kw.at(1).toUpper();
            p.number = tok.value(0);
            QList<EosPalette> &list = p.kind == QLatin1Char('P') ? data.presets : data.palettes;
            list << p;
            pal = &list.last();
            label = &pal->label;
            continue;
        }
        if (kw == QLatin1String("sub") || kw == QLatin1String("$sub"))
        {
            ++records;
            ctx = Ctx::Sub;
            EosSub s;
            s.number = tok.value(0);
            data.subs << s;
            sub = &data.subs.last();
            label = &sub->label;
            continue;
        }
        if (keyword.startsWith(QLatin1Char('$')) && !keyword.startsWith(QLatin1String("$$")))
        {
            // $Effect, $Curve, $FaderDef, $MagicSheet, $Macro... and
            // whatever a later Eos adds: counted, skipped.
            ++records;
            data.skipped[keyword]++;
            ctx = Ctx::Skip;
            label = nullptr;
            continue;
        }

        /* Secondary records */
        if (ctx == Ctx::Skip || ctx == Ctx::None)
        {
            if (ctx == Ctx::None && !keyword.startsWith(QLatin1Char('$')))
                data.skipped[keyword]++;
            continue;
        }

        if (ctx == Ctx::Personality && pers != nullptr)
        {
            if (kw == QLatin1String("$$manuf"))
                pers->manufacturer = rest;
            else if (kw == QLatin1String("$$model"))
                pers->model = rest;
            else if (kw == QLatin1String("$$dcid"))
                pers->dcid = rest;
            else if (kw == QLatin1String("$$footprint"))
                pers->footprint = tok.value(0).toInt();
            else if (kw == QLatin1String("$$perschan") && tok.count() >= 3)
            {
                EosPersChannel c;
                c.param = tok.at(0).toInt();
                c.size = qBound(1, tok.at(1).toInt(), 2);
                c.offset = tok.at(2).toInt();
                c.lsbOffset = tok.value(3).toInt();
                c.home = tok.value(4).toInt();
                pers->channels << c;
            }
            continue;
        }
        if (ctx == Ctx::Patch && patch != nullptr)
        {
            if (kw == QLatin1String("$$pers"))
                patch->personalityName = rest;
            else if (kw == QLatin1String("$$options"))
                patch->options += rest.simplified().remove(QLatin1Char(' '));
            continue;
        }
        if (ctx == Ctx::Group && group != nullptr)
        {
            if (kw == QLatin1String("$$chanlist"))
                group->channels << parseChannelList(tok);
            else if (kw == QLatin1String("chan") || kw == QLatin1String("$$chanmove"))
            {
                EosLevels lv;
                readLevels(tok, 0, lv, -1, true);
                group->channels << lv.keys();
            }
            else
            {
                bool ok = false;
                keyword.toInt(&ok);
                if (ok)
                    group->channels << parseChannelList(QStringList() << keyword << tok);
            }
            continue;
        }
        if (ctx == Ctx::Cue && cue != nullptr)
        {
            if (kw == QLatin1String("part"))
            {
                // USITT's own part record: what follows belongs to the part.
                const int part = qMax(1, tok.value(0).toInt());
                const QString key = QStringLiteral("%1/%2").arg(cue->list).arg(cueValue(cue->number));
                if (!cueParts[key].contains(part))
                    cueParts[key] << part;
                cue->parts = int(cueParts.value(key).count());
                label = nullptr;
            }
            else if (kw == QLatin1String("up"))
            {
                const QStringList t = rest.split(QRegularExpression(QStringLiteral("[\\s,/]+")), Qt::SkipEmptyParts);
                if (cue->up < 0)
                {
                    cue->up = parseTime(t.value(0));
                    if (t.count() > 1)
                        cue->upDelay = qMax(0.0, parseTime(t.at(1)));
                }
            }
            else if (kw == QLatin1String("down"))
            {
                const QStringList t = rest.split(QRegularExpression(QStringLiteral("[\\s,/]+")), Qt::SkipEmptyParts);
                if (cue->down < 0)
                {
                    cue->down = parseTime(t.value(0));
                    if (t.count() > 1)
                        cue->downDelay = qMax(0.0, parseTime(t.at(1)));
                }
            }
            else if (kw == QLatin1String("$$timeup") && cue->parts <= 1)
            {
                const double f = parseTime(tok.value(0));
                if (f >= 0)
                    cue->up = f;
                cue->upDelay = qMax(0.0, parseTime(tok.value(1)));
            }
            else if (kw == QLatin1String("$$timedown") && cue->parts <= 1)
            {
                const double f = parseTime(tok.value(0));
                if (f >= 0)
                    cue->down = f;
                cue->downDelay = qMax(0.0, parseTime(tok.value(1)));
            }
            else if (kw == QLatin1String("followon") || kw == QLatin1String("$$follow"))
                cue->follow = parseTime(tok.value(0));
            else if (kw == QLatin1String("$$hang"))
                cue->hang = parseTime(tok.value(0));
            else if (kw == QLatin1String("link") || kw == QLatin1String("$$link"))
                cue->link = tok.join(QLatin1Char('/'));
            else if (kw == QLatin1String("$$block"))
                cue->block = true;
            else if (kw == QLatin1String("$$chanmove"))
                readLevels(tok, 0, cue->moves, -1, true);
            else if (kw == QLatin1String("chan"))
            {
                EosLevels lv;
                readLevels(tok, 0, lv, -1, true);
                for (auto it = lv.constBegin(); it != lv.constEnd(); ++it)
                    cue->tracked.insert(it.key(), it.value().value(1).value);
                cue->hasTracked = true;
            }
            else if (kw == QLatin1String("$$param") && !tok.isEmpty())
                readLevels(tok, 1, cue->moves, tok.at(0).toInt(), false);
            continue;
        }
        if (ctx == Ctx::Palette && pal != nullptr)
        {
            if (kw == QLatin1String("$$param") && !tok.isEmpty())
                readLevels(tok, 1, pal->levels, tok.at(0).toInt(), false);
            else if (kw == QLatin1String("$$chanmove") || kw == QLatin1String("chan"))
                readLevels(tok, 0, pal->levels, -1, true);
            continue;
        }
        if (ctx == Ctx::Sub && sub != nullptr)
        {
            if (kw == QLatin1String("up"))
                sub->up = parseTime(tok.value(0));
            else if (kw == QLatin1String("down"))
                sub->down = parseTime(tok.value(0));
            else if (kw == QLatin1String("$$param") && !tok.isEmpty())
                readLevels(tok, 1, sub->levels, tok.at(0).toInt(), false);
            else if (kw == QLatin1String("$$chanmove") || kw == QLatin1String("chan"))
                readLevels(tok, 0, sub->levels, -1, true);
            else if (kw == QLatin1String("$$ltp") || kw == QLatin1String("$$exclusive")
                     || kw == QLatin1String("$$inhibsubm") || kw == QLatin1String("$$effectsub")
                     || kw == QLatin1String("$$independent") || kw == QLatin1String("$$intensitymaster"))
            {
                // A flag record; a trailing 0 / Off turns it off.
                const QString v = tok.value(0).toLower();
                if (v != QLatin1String("0") && v != QLatin1String("off") && !sub->options.contains(keyword.mid(2)))
                    sub->options << keyword.mid(2);
            }
            continue;
        }
    }

    if (records == 0)
    {
        data.warnings << QObject::tr("This is not a USITT ASCII show file: no Ident line and no records.");
        return false;
    }

    // An Eos export writes the conventional patch twice: a USITT "Patch"
    // record and a $Patch with the personality. The $Patch wins.
    {
        QSet<int> extended;
        for (const EosPatch &p : data.patch)
            if (p.personality != 0 || !p.personalityName.isEmpty() || !p.label.isEmpty())
                extended.insert(p.channel);
        for (int i = int(data.patch.count()) - 1; i >= 0; --i)
        {
            const EosPatch &p = data.patch.at(i);
            if (p.personality == 0 && p.personalityName.isEmpty() && p.label.isEmpty()
                && extended.contains(p.channel))
                data.patch.removeAt(i);
        }
    }

    // Groups dual-emitted as presets (other consoles' imports, some
    // exports): a preset whose label and channels are a group's adds
    // nothing.
    for (int i = int(data.presets.count()) - 1; i >= 0; --i)
    {
        const EosPalette &p = data.presets.at(i);
        for (const EosGroup &g : data.groups)
        {
            QList<int> a = p.levels.keys(), b = g.channels;
            std::sort(b.begin(), b.end());
            if (!p.label.isEmpty() && p.label == g.label && a == b)
            {
                data.presets.removeAt(i);
                break;
            }
        }
    }

    for (EosGroup &g : data.groups)
    {
        QList<int> unique;
        for (int c : g.channels)
            if (!unique.contains(c))
                unique << c;
        g.channels = unique;
    }

    // Warnings the user can act on, in the order they'd read them.
    for (const EosCueList &cl : data.cueLists)
        for (const EosCue &c : cl.cues)
        {
            if (c.parts > 1)
                data.warnings << QObject::tr("Cue %1/%2: %3 parts flattened into one cue; each part's own "
                                             "timing is lost (the cue keeps its first times).")
                                 .arg(cl.number).arg(c.number).arg(c.parts);
        }
    if (!unsupportedRefs.isEmpty())
    {
        unsupportedRefs.removeDuplicates();
        data.warnings << QObject::tr("%n level(s) refer to something that isn't imported (a cue reference "
                                     "or an unknown form) and were left out: %1", "", int(unsupportedRefs.count()))
                         .arg(unsupportedRefs.mid(0, 8).join(QStringLiteral(", ")));
    }
    return true;
}
