/*
  Q Light Controller Plus
  fixturedefcheck.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QStringList>
#include <QSet>
#include <QSize>
#include <QMap>
#include <QPair>

#include "fixturedefcheck.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcfixturedef.h"
#include "qlccapability.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "qlcfile.h"

using namespace FixtureDefCheck;

namespace
{
    /** Read @p xml to the end: the first well-formedness error, and the
     *  elements still open when it happened (outermost first). */
    bool scan(const QByteArray &xml, ParseProblem *problem, QStringList *open, bool *sawDoctype)
    {
        QXmlStreamReader r(xml);
        QStringList stack;
        bool doctype = false;
        while (!r.atEnd())
        {
            const QXmlStreamReader::TokenType t = r.readNext();
            if (t == QXmlStreamReader::DTD)
                doctype = (r.dtdName() == KXMLQLCFixtureDefDocument);
            else if (t == QXmlStreamReader::StartElement)
                stack << r.name().toString();
            else if (t == QXmlStreamReader::EndElement && !stack.isEmpty())
                stack.removeLast();
        }
        if (open != nullptr)
            *open = stack;
        if (sawDoctype != nullptr)
            *sawDoctype = doctype;
        if (r.hasError())
        {
            if (problem != nullptr)
            {
                problem->message = r.errorString();
                problem->line = int(r.lineNumber());
                problem->column = int(r.columnNumber());
            }
            return true;
        }
        return false;
    }

    /** The definition's own <Channel>s, in order: name and preset. */
    QList<QPair<QString, QString> > topLevelChannels(const QByteArray &xml)
    {
        QList<QPair<QString, QString> > out;
        QXmlStreamReader r(xml);
        int depth = 0;
        while (!r.atEnd())
        {
            const QXmlStreamReader::TokenType t = r.readNext();
            if (t == QXmlStreamReader::StartElement)
            {
                ++depth;
                if (depth == 2 && r.name() == QLatin1String("Channel"))
                    out << qMakePair(r.attributes().value(QStringLiteral("Name")).toString(),
                                     r.attributes().value(QStringLiteral("Preset")).toString());
            }
            else if (t == QXmlStreamReader::EndElement)
                --depth;
        }
        return out;
    }

    /** Does @p xml have a <Mode> element (not <Model>)? */
    bool hasMode(const QByteArray &xml)
    {
        static const QRegularExpression re(QStringLiteral("<Mode[\\s>/]"));
        return QString::fromUtf8(xml).contains(re);
    }

    /** What the loader would make of a well-formed file, with line numbers. */
    struct Outline
    {
        struct Named { QString name; int line = 0; };
        struct Ref { QString mode; QString channel; int line = 0; };
        QList<Named> channels;          ///< top-level <Channel>s (name may be empty)
        QList<Named> modes;             ///< <Mode>s (name may be empty)
        QList<Ref> refs;                ///< <Mode><Channel>name</Channel>
    };

    Outline outline(const QByteArray &xml)
    {
        Outline o;
        QXmlStreamReader r(xml);
        int depth = 0;
        QString mode;
        bool inMode = false;
        while (!r.atEnd())
        {
            const QXmlStreamReader::TokenType t = r.readNext();
            if (t == QXmlStreamReader::StartElement)
            {
                ++depth;
                const int line = int(r.lineNumber());
                if (depth == 2 && r.name() == QLatin1String("Channel"))
                    o.channels << Outline::Named{ r.attributes().value(QStringLiteral("Name")).toString(), line };
                else if (depth == 2 && r.name() == QLatin1String("Mode"))
                {
                    mode = r.attributes().value(QStringLiteral("Name")).toString();
                    o.modes << Outline::Named{ mode, line };
                    inMode = true;
                }
                else if (depth == 3 && inMode && r.name() == QLatin1String("Channel"))
                {
                    const QString text = r.readElementText();
                    --depth;            // readElementText() consumed the end
                    o.refs << Outline::Ref{ mode, text, line };
                }
            }
            else if (t == QXmlStreamReader::EndElement)
            {
                if (depth == 2 && r.name() == QLatin1String("Mode"))
                    inMode = false;
                --depth;
            }
        }
        return o;
    }

    int editDistance(const QString &a, const QString &b)
    {
        QVector<int> prev(b.size() + 1), cur(b.size() + 1);
        for (int j = 0; j <= b.size(); ++j)
            prev[j] = j;
        for (int i = 1; i <= a.size(); ++i)
        {
            cur[0] = i;
            for (int j = 1; j <= b.size(); ++j)
                cur[j] = qMin(qMin(prev[j] + 1, cur[j - 1] + 1),
                              prev[j - 1] + (a.at(i - 1) == b.at(j - 1) ? 0 : 1));
            prev.swap(cur);
        }
        return prev[b.size()];
    }

    /** The channel @p name most likely meant: the same but for case or
     *  spacing, else one or two typos away -- or empty. */
    QString nearestName(const QString &name, const QStringList &names)
    {
        const QString key = name.simplified().toLower();
        foreach (const QString &n, names)
            if (n.simplified().toLower() == key)
                return n;
        QString best;
        int bestD = 3;
        foreach (const QString &n, names)
        {
            const int d = editDistance(key, n.simplified().toLower());
            if (d < bestD) { bestD = d; best = n; }
            else if (d == bestD) best.clear();      // two as likely: no guess
        }
        return best;
    }

    /** What the loader quietly drops from @p o, first first. */
    bool lossy(const Outline &o, ParseProblem *p)
    {
        QStringList names;
        foreach (const Outline::Named &c, o.channels)
        {
            if (c.name.trimmed().isEmpty())
            {
                p->message = QStringLiteral("A channel has no name: it is left out when the file loads.");
                p->line = c.line;
                return true;
            }
            if (names.contains(c.name))
            {
                p->message = QString("Two channels are called \"%1\": the second is left out when the file loads.").arg(c.name);
                p->line = c.line;
                return true;
            }
            names << c.name;
        }
        foreach (const Outline::Named &m, o.modes)
            if (m.name.trimmed().isEmpty())
            {
                p->message = QStringLiteral("A mode has no name: it is left out when the file loads.");
                p->line = m.line;
                return true;
            }
        foreach (const Outline::Ref &ref, o.refs)
            if (!names.contains(ref.channel))
            {
                p->message = QString("Mode \"%1\" uses channel \"%2\", which the definition does not have: "
                                     "the mode loses it when the file loads.").arg(ref.mode, ref.channel);
                p->line = ref.line;
                return true;
            }
        return false;
    }

    QStringList linesOf(const QByteArray &xml)
    {
        return QString::fromUtf8(xml).split(QLatin1Char('\n'));
    }

    QByteArray joinLines(const QStringList &lines)
    {
        return lines.join(QLatin1Char('\n')).toUtf8();
    }

    /** A repair of @p xml into @p fixed, with its first changed line. */
    Repair makeRepair(const QString &description, const QByteArray &xml, const QByteArray &fixed)
    {
        Repair rp;
        rp.description = description;
        rp.fixed = fixed;
        const QStringList a = linesOf(xml), b = linesOf(fixed);
        int i = 0;
        while (i < a.size() && i < b.size() && a.at(i) == b.at(i))
            ++i;
        rp.line = i + 1;
        rp.before = a.value(i).trimmed();
        rp.after = b.value(i).trimmed();
        return rp;
    }
}

QString ParseProblem::brief() const
{
    if (message.contains(QStringLiteral("It is empty")))
        return QStringLiteral("empty: no channels or modes");
    if (message.contains(QStringLiteral("no modes")))
        return QStringLiteral("no modes");
    if (message.contains(QStringLiteral("not a fixture definition the library")))
        return QStringLiteral("not a definition the library can read");
    if (message.contains(QStringLiteral("DOCTYPE")))
        return QStringLiteral("not marked as a fixture definition");
    if (message.contains(QStringLiteral("empty")))
        return QStringLiteral("empty file");
    QString m = message.section(QStringLiteral(", but"), 0, 0).trimmed();
    if (m.endsWith(QLatin1Char('.')))
        m.chop(1);
    if (!m.isEmpty())
        m[0] = m.at(0).toLower();
    return line > 0 ? QString("line %1: %2").arg(line).arg(m) : m;
}

bool FixtureDefCheck::diagnose(const QByteArray &xml, ParseProblem *problem)
{
    ParseProblem p;
    if (xml.trimmed().isEmpty())
    {
        p.message = QStringLiteral("The file is empty.");
        if (problem != nullptr)
            *problem = p;
        return true;
    }
    bool doctype = false;
    if (scan(xml, &p, nullptr, &doctype))
    {
        if (problem != nullptr)
            *problem = p;
        return true;
    }
    if (!doctype)
    {
        p.message = QStringLiteral("No <!DOCTYPE FixtureDefinition>: it is not marked as a "
                                   "fixture definition.");
        if (problem != nullptr)
            *problem = p;
        return true;
    }
    QLCFixtureDef def;
    const bool loads = def.loadXMLData(xml);
    if (loads)
    {
        if (lossy(outline(xml), &p))
        {
            p.loads = true;
            if (problem != nullptr)
                *problem = p;
            return true;
        }
        return false;
    }
    else if (lossy(outline(xml), &p))
    {
        // What the loader dropped is why nothing is left (every mode unnamed, say).
        if (problem != nullptr)
            *problem = p;
        return true;
    }
    else
    {
        const bool channels = !topLevelChannels(xml).isEmpty(), modes = hasMode(xml);
        p.message = !channels && !modes
            ? QStringLiteral("It is empty: no channels and no modes, nothing to patch.")
            : !modes
            ? QStringLiteral("It has no modes: a definition needs at least one to be patched.")
            : QStringLiteral("It is well-formed XML, but not a fixture definition the "
                             "library can read.");
        if (problem != nullptr)
            *problem = p;
        return true;
    }
    return false;
}

QList<Repair> FixtureDefCheck::repairs(const QByteArray &xml)
{
    QList<Repair> out;
    ParseProblem first;
    if (!diagnose(xml, &first))
        return out;
    QStringList open;
    bool doctype = false;
    const bool malformed = scan(xml, nullptr, &open, &doctype);
    const QStringList lines = linesOf(xml);
    const int at = first.line - 1;              // 0-based, -1 when not at a line

    QList<Repair> candidates, lastResort;

    // "\>" typed for "/>" -- also "\ >". Never anything else, so certain.
    {
        static const QRegularExpression backslash(QStringLiteral("\\\\\\s*>"));
        const QString t0 = QString::fromUtf8(xml);
        if (t0.contains(backslash))
        {
            QString t = t0;
            const int n = t.count(backslash);
            t.replace(backslash, QStringLiteral("/>"));
            Repair rp = makeRepair(n == 1 ? QStringLiteral("Replace \"\\>\" with \"/>\"")
                                          : QString("Replace all %1 \"\\>\" with \"/>\"").arg(n),
                                   xml, t.toUtf8());
            rp.certain = true;
            candidates << rp;
        }
    }

    // <Channel ...> written with ">" for "/>": a channel with nothing in it,
    // the next line already the next channel (or the mode). One that does
    // have capabilities is followed by them, and is left alone.
    {
        static const QRegularExpression openTag(QStringLiteral("^(\\s*<Channel\\b[^>]*[^/\\\\\\s])\\s*>\\s*$"));
        static const QRegularExpression nextSibling(QStringLiteral("^\\s*(<Channel\\b|<Mode\\b|<Physical\\b|</FixtureDefinition>)"));
        QStringList l = lines;
        int n = 0;
        for (int i = 0; i + 1 < l.size(); ++i)
        {
            const QRegularExpressionMatch m = openTag.match(l.at(i));
            if (!m.hasMatch())
                continue;
            int j = i + 1;
            while (j < l.size() && l.at(j).trimmed().isEmpty())
                ++j;
            if (j < l.size() && nextSibling.match(l.at(j)).hasMatch())
            {
                l[i] = m.captured(1) + QStringLiteral("/>");
                ++n;
            }
        }
        if (n > 0)
        {
            Repair rp = makeRepair(n == 1 ? QStringLiteral("Close the channel left open (\">\" for \"/>\")")
                                          : QString("Close the %1 channels left open (\">\" for \"/>\")").arg(n),
                                   xml, joinLines(l));
            rp.certain = true;
            candidates << rp;
        }
    }

    // A bare "&" that is not an entity.
    {
        static const QRegularExpression bareAmp(
            QStringLiteral("&(?!(amp|lt|gt|quot|apos|#[0-9]+|#x[0-9a-fA-F]+);)"));
        QString text = QString::fromUtf8(xml);
        if (text.contains(bareAmp))
        {
            text.replace(bareAmp, QStringLiteral("&amp;"));
            candidates << makeRepair(QStringLiteral("Write every bare \"&\" as \"&amp;\""), xml,
                                     text.toUtf8());
        }
    }

    // No DOCTYPE: add one after the XML declaration.
    if (!malformed && !doctype)
    {
        QStringList l = lines;
        const int after = (!l.isEmpty() && l.first().trimmed().startsWith(QStringLiteral("<?xml"))) ? 1 : 0;
        l.insert(after, QStringLiteral("<!DOCTYPE FixtureDefinition>"));
        candidates << makeRepair(QStringLiteral("Add the <!DOCTYPE FixtureDefinition> line"),
                                 xml, joinLines(l));
    }

    // Channels but no mode: one with all of them, in order -- and a head
    // for each run of colour channels (Red, Green, Blue[, White...]).
    const QList<QPair<QString, QString> > chans = malformed ? QList<QPair<QString, QString> >()
                                                            : topLevelChannels(xml);
    if (!malformed && doctype && !chans.isEmpty() && !hasMode(xml))
    {
        auto colourOf = [](const QString &preset) -> QString {
            for (const char *c : { "Red", "Green", "Blue", "White", "Amber", "UV", "Lime", "Cyan", "Magenta", "Yellow" })
                if (preset.startsWith(QLatin1String("Intensity")) && preset.endsWith(QLatin1String(c)))
                    return QLatin1String(c);
            return QString();
        };
        QStringList mode;
        mode << QStringLiteral(" <Mode Name=\"All channels\">");
        for (int i = 0; i < chans.size(); ++i)
            mode << QString("  <Channel Number=\"%1\">%2</Channel>").arg(i)
                        .arg(chans.at(i).first.toHtmlEscaped());
        int heads = 0;
        for (int i = 0; i < chans.size(); )
        {
            if (colourOf(chans.at(i).second) == QLatin1String("Red")
                && i + 2 < chans.size()
                && colourOf(chans.at(i + 1).second) == QLatin1String("Green")
                && colourOf(chans.at(i + 2).second) == QLatin1String("Blue"))
            {
                int j = i + 3;
                while (j < chans.size() && !colourOf(chans.at(j).second).isEmpty()
                       && colourOf(chans.at(j).second) != QLatin1String("Red"))
                    ++j;
                mode << QStringLiteral("  <Head>");
                for (int k = i; k < j; ++k)
                    mode << QString("   <Channel>%1</Channel>").arg(k);
                mode << QStringLiteral("  </Head>");
                ++heads;
                i = j;
            }
            else
                ++i;
        }
        mode << QStringLiteral(" </Mode>");
        QStringList l = lines;
        int insertAt = l.size();
        for (int i = 0; i < l.size(); ++i)
            if (l.at(i).trimmed().startsWith(QStringLiteral("<Physical"))
                || l.at(i).trimmed().startsWith(QStringLiteral("</FixtureDefinition")))
            {
                insertAt = i;
                break;
            }
        for (int k = mode.size() - 1; k >= 0; --k)
            l.insert(insertAt, mode.at(k));
        candidates << makeRepair(heads > 1
                ? QString("Add a mode with all %1 channels in order, as %2 heads").arg(chans.size()).arg(heads)
                : QString("Add a mode with all %1 channels in order").arg(chans.size()),
            xml, joinLines(l));
    }

    // An empty stub -- a make and model, nothing else: the honest guess is a
    // placeholder with one dimmer channel, so it can at least be patched.
    if (!malformed && doctype && chans.isEmpty() && !hasMode(xml))
    {
        QStringList l = lines;
        int insertAt = l.size();
        for (int i = 0; i < l.size(); ++i)
            if (l.at(i).trimmed().startsWith(QStringLiteral("<Physical"))
                || l.at(i).trimmed().startsWith(QStringLiteral("</FixtureDefinition")))
            {
                insertAt = i;
                break;
            }
        const QStringList add = {
            QStringLiteral(" <Channel Name=\"Dimmer\" Preset=\"IntensityDimmer\"/>"),
            QStringLiteral(" <Mode Name=\"Placeholder\">"),
            QStringLiteral("  <Channel Number=\"0\">Dimmer</Channel>"),
            QStringLiteral(" </Mode>") };
        for (int k = add.size() - 1; k >= 0; --k)
            l.insert(insertAt, add.at(k));
        candidates << makeRepair(QStringLiteral("Make it a placeholder: one dimmer channel, one mode"),
                                 xml, joinLines(l));
    }

    if (malformed && at >= 0 && at < lines.size())
    {
        const QString line = lines.at(at);

        // A closing tag that does not match what is open: close the right one.
        if (!open.isEmpty())
        {
            static const QRegularExpression closing(QStringLiteral("</\\s*([A-Za-z_][\\w.-]*)\\s*>"));
            QRegularExpressionMatchIterator it = closing.globalMatch(line);
            while (it.hasNext())
            {
                const QRegularExpressionMatch m = it.next();
                if (m.captured(1) == open.last())
                    continue;
                QStringList l = lines;
                l[at] = QString(line).replace(m.capturedStart(), m.capturedLength(),
                                              QString("</%1>").arg(open.last()));
                candidates << makeRepair(QString("Close <%1> here instead of <%2>")
                                             .arg(open.last(), m.captured(1)), xml, joinLines(l));
                QStringList l2 = lines;
                l2[at] = QString(line).insert(m.capturedStart(), QString("</%1>").arg(open.last()));
                candidates << makeRepair(QString("Close <%1> before </%2>")
                                             .arg(open.last(), m.captured(1)), xml, joinLines(l2));
                break;
            }
        }

        // An attribute value without quotes: Name=Red -> Name="Red".
        {
            static const QRegularExpression unquoted(QStringLiteral("(\\s[A-Za-z_][\\w.-]*)=([^\"'\\s>/]+)"));
            if (line.contains(unquoted))
            {
                QStringList l = lines;
                l[at] = QString(line).replace(unquoted, QStringLiteral("\\1=\"\\2\""));
                candidates << makeRepair(QString("Quote the attribute values on line %1").arg(first.line),
                                         xml, joinLines(l));
            }
        }

        // The file stops with elements still open: close them.
        if (!open.isEmpty() && first.message.contains(QStringLiteral("Premature end"), Qt::CaseInsensitive))
        {
            QStringList l = lines;
            for (int i = open.size() - 1; i >= 0; --i)
                l << QString("%1</%2>").arg(QString(i, QLatin1Char(' ')), open.at(i));
            candidates << makeRepair(QStringLiteral("Close the elements left open at the end"),
                                     xml, joinLines(l));
        }

        // Last resort: the broken line itself goes (offered after every guess).
        {
            QStringList l = lines;
            l.removeAt(at);
            lastResort << makeRepair(QString("Delete line %1").arg(first.line), xml, joinLines(l));
        }
    }

    /* ---- Educated guesses at what was meant ---- */
    const QString text = QString::fromUtf8(xml);

    // Typographic quotes from a word processor: “Red” for "Red".
    if (text.contains(QChar(0x201C)) || text.contains(QChar(0x201D))
        || text.contains(QChar(0x2018)) || text.contains(QChar(0x2019)))
    {
        QString t = text;
        t.replace(QChar(0x201C), QLatin1Char('"')).replace(QChar(0x201D), QLatin1Char('"'))
         .replace(QChar(0x2018), QLatin1Char('\'')).replace(QChar(0x2019), QLatin1Char('\''));
        candidates << makeRepair(QStringLiteral("Replace curly quotes with straight ones"), xml, t.toUtf8());
    }

    // Not UTF-8 at all: most likely Latin-1 from an old editor.
    if (QString::fromUtf8(xml).toUtf8() != xml)
    {
        QString t = QString::fromLatin1(xml);
        t.replace(QRegularExpression(QStringLiteral("encoding=\"[^\"]*\"")), QStringLiteral("encoding=\"UTF-8\""));
        candidates << makeRepair(QStringLiteral("Read it as Latin-1 and save it as UTF-8"), xml, t.toUtf8());
    }

    // Junk before the first tag, or after the definition ends.
    {
        const int start = text.indexOf(QLatin1Char('<'));
        if (start > 0 && !text.left(start).trimmed().isEmpty())
            candidates << makeRepair(QStringLiteral("Remove what comes before the first tag"),
                                     xml, text.mid(start).toUtf8());
        const int end = text.lastIndexOf(QStringLiteral("</FixtureDefinition>"));
        if (end >= 0)
        {
            const int after = end + int(qstrlen("</FixtureDefinition>"));
            if (!text.mid(after).trimmed().isEmpty())
                candidates << makeRepair(QStringLiteral("Remove what comes after </FixtureDefinition>"),
                                         xml, (text.left(after) + QLatin1Char('\n')).toUtf8());
        }
    }

    // <fixturedefinition>, <!DOCTYPE fixturedefinition>: the names are case sensitive.
    {
        QString t = text;
        t.replace(QRegularExpression(QStringLiteral("<!DOCTYPE\\s+fixturedefinition\\s*>"),
                                     QRegularExpression::CaseInsensitiveOption),
                  QStringLiteral("<!DOCTYPE FixtureDefinition>"));
        t.replace(QRegularExpression(QStringLiteral("<(/?)fixturedefinition\\b"),
                                     QRegularExpression::CaseInsensitiveOption),
                  QStringLiteral("<\\1FixtureDefinition"));
        if (t != text)
            candidates << makeRepair(QStringLiteral("Spell FixtureDefinition the way the format does"),
                                     xml, t.toUtf8());
    }

    // A bare "<" in text ("A < B"): written as "&lt;".
    {
        static const QRegularExpression bareLt(QStringLiteral("<(?=[\\s\\d=<])"));
        if (text.contains(bareLt))
        {
            QString t = text;
            t.replace(bareLt, QStringLiteral("&lt;"));
            candidates << makeRepair(QStringLiteral("Write every bare \"<\" in text as \"&lt;\""), xml, t.toUtf8());
        }
    }

    // An attribute value that never closes its quote -- the parser only
    // notices a line or more later, so look back for the line it opened on.
    if (malformed && at >= 0 && at < lines.size())
    {
        for (int k = at; k >= qMax(0, at - 50); --k)
        {
            if (lines.at(k).count(QLatin1Char('"')) % 2 == 0)
                continue;
            QString line = lines.at(k);
            static const QRegularExpression tail(QStringLiteral("\\s*(/?>)\\s*$"));
            const QRegularExpressionMatch m = tail.match(line);
            if (m.hasMatch())
            {
                line.insert(m.capturedStart(), QLatin1Char('"'));
                QStringList l = lines;
                l[k] = line;
                candidates << makeRepair(QString("Close the quote left open on line %1").arg(k + 1),
                                         xml, joinLines(l));
            }
            break;
        }
    }

    // What the loader would quietly drop: name it, number it, or find what was meant.
    if (!malformed)
    {
        const Outline o = outline(xml);
        QStringList names;
        foreach (const Outline::Named &c, o.channels)
            if (!c.name.trimmed().isEmpty())
                names << c.name;

        // Unnamed modes and channels get names.
        {
            QStringList l = lines;
            int modeNo = 0, chNo = 0;
            bool changed = false;
            foreach (const Outline::Named &m, o.modes)
            {
                ++modeNo;
                if (!m.name.trimmed().isEmpty())
                    continue;
                QString &ln = l[m.line - 1];
                ln.replace(QRegularExpression(QStringLiteral("\\sName=\"\\s*\"")), QString());
                ln.replace(QRegularExpression(QStringLiteral("<Mode\\b")), QString("<Mode Name=\"Mode %1\"").arg(modeNo));
                changed = true;
            }
            foreach (const Outline::Named &c, o.channels)
            {
                ++chNo;
                if (!c.name.trimmed().isEmpty())
                    continue;
                QString &ln = l[c.line - 1];
                ln.replace(QRegularExpression(QStringLiteral("\\sName=\"\\s*\"")), QString());
                ln.replace(QRegularExpression(QStringLiteral("<Channel\\b")), QString("<Channel Name=\"Channel %1\"").arg(chNo));
                changed = true;
            }
            if (changed)
                candidates << makeRepair(QStringLiteral("Name the modes and channels that have no name"),
                                         xml, joinLines(l));
        }

        // Two channels with one name: number the later ones.
        {
            QStringList l = lines;
            QMap<QString, int> seenNames;
            bool changed = false;
            foreach (const Outline::Named &c, o.channels)
            {
                if (c.name.trimmed().isEmpty())
                    continue;
                const int n = ++seenNames[c.name];
                if (n == 1)
                    continue;
                QString &ln = l[c.line - 1];
                const QString renamed = QString("%1 %2").arg(c.name).arg(n);
                ln.replace(QString("Name=\"%1\"").arg(c.name.toHtmlEscaped()),
                           QString("Name=\"%1\"").arg(renamed.toHtmlEscaped()));
                changed = true;
            }
            if (changed)
                candidates << makeRepair(QStringLiteral("Number the channels that share a name"),
                                         xml, joinLines(l));
        }

        // A mode naming a channel that is not there.
        QMap<QString, QString> guess;           // missing -> what it most likely meant
        QStringList missing;
        foreach (const Outline::Ref &ref, o.refs)
            if (!names.contains(ref.channel) && !missing.contains(ref.channel))
            {
                missing << ref.channel;
                const QString g = nearestName(ref.channel, names);
                if (!g.isEmpty())
                    guess.insert(ref.channel, g);
            }
        if (!guess.isEmpty())
        {
            QStringList l = lines;
            foreach (const Outline::Ref &ref, o.refs)
                if (guess.contains(ref.channel))
                    l[ref.line - 1].replace(QString(">%1</Channel>").arg(ref.channel.toHtmlEscaped()),
                                            QString(">%1</Channel>").arg(guess.value(ref.channel).toHtmlEscaped()));
            const QString first = guess.firstKey();
            candidates << makeRepair(guess.size() == 1
                    ? QString("Read \"%1\" as \"%2\"").arg(first, guess.value(first))
                    : QString("Read \"%1\" as \"%2\", and %3 more like it")
                          .arg(first, guess.value(first)).arg(guess.size() - 1),
                xml, joinLines(l));
        }
        if (!missing.isEmpty())
        {
            QStringList l = lines;
            int insertAt = l.size();
            for (int i = 0; i < l.size(); ++i)
                if (l.at(i).trimmed().startsWith(QStringLiteral("<Mode")))
                {
                    insertAt = i;
                    break;
                }
            for (int k = missing.size() - 1; k >= 0; --k)
                l.insert(insertAt, QString(" <Channel Name=\"%1\"/>").arg(missing.at(k).toHtmlEscaped()));
            candidates << makeRepair(missing.size() == 1
                    ? QString("Add the missing channel \"%1\"").arg(missing.first())
                    : QString("Add the %1 missing channels").arg(missing.size()),
                xml, joinLines(l));
        }
    }

    candidates << lastResort;

    // Offer only what gets past where it broke -- best first.
    QSet<QByteArray> seen;
    QMap<int, QList<Repair> > ranked;            // lower key = better
    for (Repair rp : candidates)
    {
        if (rp.fixed == xml || seen.contains(rp.fixed))
            continue;
        seen.insert(rp.fixed);
        ParseProblem next;
        if (!diagnose(rp.fixed, &next))
        {
            rp.complete = true;
            ranked[0] << rp;
            continue;
        }
        const bool progress = (first.line > 0 && next.line > first.line)
                           || (first.line > 0 && next.line == 0);
        if (progress)
            ranked[1 + qMax(0, 100000 - next.line)] << rp;
    }
    for (auto it = ranked.constBegin(); it != ranked.constEnd(); ++it)
        out << it.value();
    return out;
}

QList<Issue> FixtureDefCheck::check(const QLCFixtureDef *def)
{
    QList<Issue> errors, warnings;
    if (def == nullptr)
        return errors;
    auto add = [&](Issue::Level level, Issue::Section section, const QString &target, const QString &message) {
        Issue i;
        i.level = level;
        i.section = section;
        i.target = target;
        i.message = message;
        (level == Issue::Error ? errors : warnings) << i;
    };

    // General
    if (def->manufacturer().trimmed().isEmpty())
        add(Issue::Error, Issue::General, QString(), QStringLiteral("No manufacturer."));
    if (def->model().trimmed().isEmpty())
        add(Issue::Error, Issue::General, QString(), QStringLiteral("No model."));

    // Channels
    const QList<QLCChannel *> channels = def->channels();
    if (channels.isEmpty())
        add(Issue::Error, Issue::Channels, QString(), QStringLiteral("No channels."));
    QSet<QString> names;
    foreach (QLCChannel *ch, channels)
    {
        if (ch == nullptr)
            continue;
        if (names.contains(ch->name()))
            add(Issue::Error, Issue::Channels, ch->name(),
                QString("Two channels are called \"%1\".").arg(ch->name()));
        names.insert(ch->name());

        const QList<QLCCapability *> caps = ch->capabilities();
        if (caps.isEmpty())
        {
            add(Issue::Warning, Issue::Channels, ch->name(),
                QString("\"%1\" has no capabilities: nothing says what its values do.").arg(ch->name()));
            continue;
        }
        int unnamed = 0;
        foreach (const QLCCapability *cap, caps)
            if (cap->name().trimmed().isEmpty())
                ++unnamed;
        if (unnamed > 0)
            add(Issue::Warning, Issue::Channels, ch->name(),
                QString("\"%1\": %2 of its capabilities have no description.").arg(ch->name()).arg(unnamed));
        QVector<int> owner(256, -1);
        bool overlap = false;
        for (int c = 0; c < caps.size(); ++c)
        {
            const QLCCapability *cap = caps.at(c);
            if (cap->min() > cap->max())
            {
                add(Issue::Error, Issue::Channels, ch->name(),
                    QString("\"%1\": capability \"%2\" runs backwards (%3 to %4).")
                        .arg(ch->name(), cap->name()).arg(cap->min()).arg(cap->max()));
                continue;
            }
            for (int v = cap->min(); v <= cap->max(); ++v)
            {
                if (owner[v] >= 0 && !overlap)
                {
                    add(Issue::Error, Issue::Channels, ch->name(),
                        QString("\"%1\": capabilities \"%2\" and \"%3\" overlap at %4.")
                            .arg(ch->name(), caps.at(owner[v])->name(), cap->name()).arg(v));
                    overlap = true;
                }
                owner[v] = c;
            }
        }
        int gaps = 0;
        for (int v = 0; v < 256; ++v)
            if (owner[v] < 0)
                ++gaps;
        if (gaps > 0)
            add(Issue::Warning, Issue::Channels, ch->name(),
                QString("\"%1\": %2 of its 256 values belong to no capability.").arg(ch->name()).arg(gaps));
    }

    // Modes
    QList<QLCFixtureMode *> modes = const_cast<QLCFixtureDef *>(def)->modes();
    if (modes.isEmpty())
        add(Issue::Error, Issue::Modes, QString(), QStringLiteral("No modes: it cannot be patched."));
    QSet<QString> modeNames;
    QMap<QString, QStringList> physicalGaps;     // message -> modes it applies to
    foreach (QLCFixtureMode *mode, modes)
    {
        if (mode == nullptr)
            continue;
        if (modeNames.contains(mode->name()))
            add(Issue::Error, Issue::Modes, mode->name(),
                QString("Two modes are called \"%1\".").arg(mode->name()));
        modeNames.insert(mode->name());

        const QVector<QLCChannel *> mch = mode->channels();
        if (mch.isEmpty())
        {
            add(Issue::Error, Issue::Modes, mode->name(),
                QString("Mode \"%1\" has no channels.").arg(mode->name()));
            continue;
        }
        const QVector<QLCFixtureHead> &heads = mode->heads();
        for (int h = 0; h < heads.size(); ++h)
            foreach (quint32 c, heads.at(h).channels())
                if (int(c) >= mch.size())
                    add(Issue::Error, Issue::Modes, mode->name(),
                        QString("Mode \"%1\": head %2 uses channel %3, which the mode does not have.")
                            .arg(mode->name()).arg(h + 1).arg(c + 1));

        int reds = 0;
        bool panTilt = false;
        foreach (QLCChannel *ch, mch)
        {
            if (ch == nullptr)
                continue;
            if (ch->colour() == QLCChannel::Red)
                ++reds;
            if (ch->group() == QLCChannel::Pan || ch->group() == QLCChannel::Tilt)
                panTilt = true;
        }
        if (reds > 1 && heads.isEmpty())
            add(Issue::Warning, Issue::Modes, mode->name(),
                QString("Mode \"%1\" has %2 sets of colour channels but no heads: effects and "
                        "the rig view treat it as one light.").arg(mode->name()).arg(reds));

        const QLCPhysical phys = mode->physical();
        const QSize layout = phys.layoutSize();
        if (heads.size() > 1 && layout.width() * layout.height() > 1
            && layout.width() * layout.height() != heads.size())
            add(Issue::Warning, Issue::Physical, mode->name(),
                QString("Mode \"%1\": the pixel layout %2 x %3 does not hold its %4 heads; it is "
                        "drawn as one row.").arg(mode->name()).arg(layout.width())
                        .arg(layout.height()).arg(heads.size()));

        if (phys.width() <= 0 || phys.height() <= 0 || phys.depth() <= 0)
            physicalGaps[QStringLiteral("No size (width, height, depth): the rig view draws it at a guess.")] << mode->name();
        if (!expectsLight(mode))
            continue;                   // a curtain, a hazer: nothing about light applies
        if (!phys.declaresOutput())
            physicalGaps[QStringLiteral("No light output (lumens or power): the rig view draws its light at a guess.")] << mode->name();
        if (phys.lensDegreesMax() <= 0.0 && phys.emission() != QLCPhysical::OmniEmission)
            physicalGaps[QStringLiteral("No beam angle (lens degrees): its beam is drawn at a guess.")] << mode->name();
        if (panTilt && (phys.focusPanMax() <= 0 || phys.focusTiltMax() <= 0))
            physicalGaps[QStringLiteral("No pan/tilt range: aiming guesses how far it turns.")] << mode->name();
    }
    for (auto it = physicalGaps.constBegin(); it != physicalGaps.constEnd(); ++it)
        add(Issue::Warning, Issue::Physical,
            it.value().size() == modes.size() ? QString() : it.value().join(QStringLiteral(", ")),
            it.key());

    return errors + warnings;
}

bool FixtureDefCheck::expectsLight(const QLCFixtureMode *mode)
{
    if (mode == nullptr)
        return true;
    if (!mode->physical().emitsLight())
        return false;
    const QLCFixtureDef *def = mode->fixtureDef();
    if (def == nullptr)
        return true;
    switch (def->type())
    {
        case QLCFixtureDef::Fan:
        case QLCFixtureDef::Hazer:
        case QLCFixtureDef::Smoke:
            // An effects machine with LEDs on it is still a light.
            foreach (QLCChannel *ch, mode->channels())
                if (ch != nullptr && ch->group() == QLCChannel::Intensity)
                    return true;
            return false;
        default:
            return true;
    }
}

QByteArray FixtureDefCheck::autoRepair(const QByteArray &xml, QStringList *applied)
{
    QByteArray current = xml;
    for (int round = 0; round < 8; ++round)
    {
        bool did = false;
        foreach (const Repair &rp, repairs(current))
            if (rp.certain)
            {
                if (applied != nullptr)
                    applied->append(rp.description);
                current = rp.fixed;
                did = true;
                break;
            }
        if (!did)
            break;
    }
    return current;
}
