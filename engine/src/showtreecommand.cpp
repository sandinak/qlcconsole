/*
  Q Light Controller Plus
  showtreecommand.cpp

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
#include <QCoreApplication>
#include <QColor>
#include <algorithm>

#include "showtreecontroller.h"
#include "showtreecommand.h"
#include "fixturegroup.h"
#include "function.h"
#include "fixture.h"
#include "qlcpalette.h"
#include "doc.h"

using namespace ShowTree;

static QString tr(const char *text)
{
    return QCoreApplication::translate("ShowTreeCommand", text);
}

ShowTreeCommand::ShowTreeCommand(Doc *doc, ShowTreeController *controller)
    : m_doc(doc)
    , m_st(controller)
{
}

QString ShowTreeCommand::grammar()
{
    return tr("cue 1.0.1 \"name\" · cue + \"name\" · track | block | cueonly | base · "
              "Towers colour Blue dimmer 45 · Towers USTs … = one layer · Towers, USTs … = a layer each · "
              "cue look colour Red · Steps effect Sinelon colour Red · gobo none · release Towers · "
              "T1-CW … (a fixture) · function Haze · include 0.0.3 · record [selected|running|lit] · "
              "list \"name\" · dup · undo — Tab completes every word (again: the next), ↑ recalls");
}

QStringList ShowTreeCommand::keywords()
{
    return { QStringLiteral("cue"), QStringLiteral("look"), QStringLiteral("track"),
             QStringLiteral("block"), QStringLiteral("cueonly"), QStringLiteral("default"),
             QStringLiteral("base"), QStringLiteral("release"), QStringLiteral("include"),
             QStringLiteral("record"), QStringLiteral("dup"), QStringLiteral("undo"),
             QStringLiteral("redo"), QStringLiteral("list"), QStringLiteral("layer"),
             QStringLiteral("function") };
}

QStringList ShowTreeCommand::attributeWords()
{
    // What Tab offers; attributeKey() also takes color, dim, level, pos,
    // focus, frost, iris, strobe and fx.
    return { QStringLiteral("colour"), QStringLiteral("dimmer"), QStringLiteral("position"),
             QStringLiteral("gobo"), QStringLiteral("zoom"), QStringLiteral("beam"),
             QStringLiteral("shutter"), QStringLiteral("effect") };
}

QString ShowTreeCommand::attributeKey(const QString &word)
{
    const QString w = word.toLower();
    if (w == QLatin1String("colour") || w == QLatin1String("color"))
        return KeyColour;
    if (w == QLatin1String("dimmer") || w == QLatin1String("dim") || w == QLatin1String("level"))
        return KeyDimmer;
    if (w == QLatin1String("position") || w == QLatin1String("pos"))
        return KeyPosition;
    if (w == QLatin1String("gobo"))
        return KeyGobo;
    if (w == QLatin1String("zoom"))
        return KeyZoom;
    if (w == QLatin1String("beam") || w == QLatin1String("focus") || w == QLatin1String("frost")
        || w == QLatin1String("iris"))
        return KeyBeam;
    if (w == QLatin1String("shutter") || w == QLatin1String("strobe"))
        return KeyShutter;
    if (w == QLatin1String("effect") || w == QLatin1String("fx"))
        return KeyEffectPrefix;
    return QString();
}

QStringList ShowTreeCommand::groupNames() const
{
    QStringList out;
    for (const FixtureGroup *g : m_doc->fixtureGroups())
        if (!g->name().isEmpty() && !out.contains(g->name(), Qt::CaseInsensitive))
            out << g->name();
    return out;
}

QStringList ShowTreeCommand::targetNames() const
{
    QStringList out = groupNames();
    for (const FixtureGroup *g : m_doc->fixtureGroups())
    {
        const QString path = g->path();
        if (g->name().isEmpty() || path.isEmpty())
            continue;
        QString full = path;
        if (!full.endsWith(QLatin1Char('/')))
            full += QLatin1Char('/');
        full += g->name();
        if (!out.contains(full, Qt::CaseInsensitive))
            out << full;
    }
    for (const Fixture *f : m_doc->fixtures())
        if (!f->name().isEmpty() && !out.contains(f->name(), Qt::CaseInsensitive))
            out << f->name();
    return out;
}

QStringList ShowTreeCommand::functionNames() const
{
    // What the Library lists under Functions: never the tree's own.
    QStringList out;
    for (const Function *f : m_doc->functions())
        if (!f->name().isEmpty() && !m_st->isGenerated(f->id()) && !out.contains(f->name(), Qt::CaseInsensitive))
            out << f->name();
    return out;
}

QStringList ShowTreeCommand::listNames(const Context &context) const
{
    // The selected production's lists first.
    QStringList out;
    for (const Production &p : m_st->productions())
        if (p.id == context.production)
            for (const CueList &l : p.lists)
                if (!l.name.isEmpty() && !out.contains(l.name, Qt::CaseInsensitive))
                    out << l.name;
    for (const Production &p : m_st->productions())
        for (const CueList &l : p.lists)
            if (!l.name.isEmpty() && !out.contains(l.name, Qt::CaseInsensitive))
                out << l.name;
    return out;
}

QStringList ShowTreeCommand::paletteNames(const QString &key) const
{
    // Named palettes only: a one-off value is never in All (D21).
    QStringList out;
    for (const QLCPalette *p : m_doc->palettes())
    {
        if (m_st->isInlinePalette(p->id()) || p->name().isEmpty())
            continue;
        const QString pk = m_st->keyForPalette(p->id());
        if ((key == KeyEffectPrefix ? isEffectKey(pk) : pk == key)
            && !out.contains(p->name(), Qt::CaseInsensitive))
            out << p->name();
    }
    return out;
}

QStringList ShowTreeCommand::cueNumbers(const Context &context) const
{
    QStringList out;
    const CueList *list = m_st->cueList(context.list);
    if (list == nullptr)
        return out;
    for (int i = 0; i < list->cues.count(); ++i)
        out << (list->cues.at(i).number.isEmpty() ? QString::number(i + 1) : list->cues.at(i).number);
    return out;
}

/****************************************************************************
 * Running a command
 ****************************************************************************/

namespace
{

/** Reads a command word by word; names may hold spaces. */
struct Reader
{
    QString src;
    int i = 0;

    void ws()
    {
        while (i < src.length() && src.at(i) == QLatin1Char(' '))
            ++i;
    }
    bool atEnd()
    {
        ws();
        return i >= src.length();
    }
    QString peek()
    {
        ws();
        int j = i;
        while (j < src.length() && src.at(j) != QLatin1Char(' '))
            ++j;
        return src.mid(i, j - i);
    }
    QString word()
    {
        const QString w = peek();
        i += w.length();
        return w;
    }
    /** A "quoted" string, or a null QString. */
    QString quoted()
    {
        ws();
        if (i >= src.length() || src.at(i) != QLatin1Char('"'))
            return QString();
        const int j = src.indexOf(QLatin1Char('"'), i + 1);
        const QString s = src.mid(i + 1, (j < 0 ? src.length() : j) - i - 1);
        i = j < 0 ? src.length() : j + 1;
        return s.isNull() ? QStringLiteral("") : s;
    }
    /** The longest of @a names the text goes on with (whole words; a '+'
     *  or ',' also ends one), or quoted; empty: none. */
    QString name(const QStringList &names)
    {
        ws();
        if (i < src.length() && src.at(i) == QLatin1Char('"'))
        {
            const int save = i;
            const QString q = quoted();
            for (const QString &n : names)
                if (n.compare(q, Qt::CaseInsensitive) == 0)
                    return n;
            i = save;
            return QString();
        }
        const QString rest = src.mid(i);
        QString best;
        for (const QString &n : names)
        {
            if (n.isEmpty() || !rest.startsWith(n, Qt::CaseInsensitive))
                continue;
            if (rest.length() > n.length() && rest.at(n.length()) != QLatin1Char(' ')
                && rest.at(n.length()) != QLatin1Char('+') && rest.at(n.length()) != QLatin1Char(','))
                continue;
            if (n.length() > best.length())
                best = n;
        }
        i += best.length();
        return best;
    }
    /** A '+' joining two names into one layer (as a space does). */
    bool plus()
    {
        return symbol(QLatin1Char('+'));
    }
    /** A ',' ending one layer's names: the next name starts another. */
    bool comma()
    {
        return symbol(QLatin1Char(','));
    }
    bool symbol(QChar c)
    {
        ws();
        if (i < src.length() && src.at(i) == c)
        {
            ++i;
            return true;
        }
        return false;
    }
};

} // namespace

ShowTreeCommand::Result ShowTreeCommand::run(const QString &line, const Context &context)
{
    Result res;
    res.context = context;
    Reader r;
    r.src = line.trimmed();
    if (r.src.isEmpty())
    {
        res.message = tr("Type a command; Tab completes names.");
        return res;
    }

    const QString w0 = r.peek().toLower();
    if (w0 == QLatin1String("undo") || w0 == QLatin1String("redo"))
    {
        const bool undo = w0 == QLatin1String("undo");
        const QString what = undo ? m_st->undoText() : m_st->redoText();
        res.ok = undo ? m_st->undo() : m_st->redo();
        res.message = res.ok ? (undo ? tr("Undid: %1") : tr("Redid: %1")).arg(what)
                             : (undo ? tr("Nothing to undo") : tr("Nothing to redo"));
        return res;
    }

    Context ctx = context;
    QString error;

    if (w0 == QLatin1String("record"))
    {
        // One edit of its own (Record is one undo step); what it records is
        // said, or the setting / a dialog picks when the choices differ.
        r.word();
        const QString what = r.word();
        const CueList *l = m_st->cueList(ctx.list);
        if (l == nullptr || l->indexOf(ctx.cue) < 0)
        {
            res.message = tr("Select a cue first (or: cue 1.0.0 \"name\").");
            return res;
        }
        using RS = ShowTreeController::RecordSource;
        RS src = RS::Selected;
        if (!what.isEmpty())
        {
            if (!ShowTreeController::stringToRecordSource(what, &src) || !r.atEnd())
            {
                res.message = tr("record · record selected · record running · record lit");
                return res;
            }
            if (src != RS::Selected && !m_st->canRecord(ctx.cue, src))
            {
                res.message = src == RS::Running ? tr("Nothing of this cue list is running.")
                                                 : tr("Nothing is lit.");
                return res;
            }
        }
        else
        {
            const QList<RS> choices = m_st->recordChoices(ctx.cue);
            if (choices.count() == 1)
                src = choices.first();
            else if (choices.count() > 1 && m_recordChooser)
            {
                if (!m_recordChooser(ctx.cue, choices, &src))
                {
                    res.message = tr("Not recorded.");
                    return res;
                }
            }
            else if (choices.count() > 1)
                src = choices.first();
        }
        const quint32 id = m_st->recordCue(ctx.cue, src);
        if (id == invalidId())
        {
            res.message = tr("Couldn't record.");
            return res;
        }
        ctx.cue = id;
        res.ok = true;
        res.message = QStringLiteral("✓ record %1").arg(ShowTreeController::recordSourceToString(src));
        res.context = ctx;
        return res;
    }

    // Values typed as a level or a hex: the palettes they came to (reused
    // or new), to say so.
    QList<quint32> valuePalettes, madeNew;
    m_st->beginBatch(r.src);

    // The selected cue's mode words, after "cue …" or on their own.
    auto modeWords = [&]() -> QString {
        while (!r.atEnd())
        {
            const QString w = r.peek().toLower();
            if (w == QLatin1String("track") || w == QLatin1String("block")
                || w == QLatin1String("cueonly") || w == QLatin1String("cue-only")
                || w == QLatin1String("default"))
            {
                r.word();
                const CueMode mode = w == QLatin1String("track") ? CueMode::Track
                                     : w == QLatin1String("block") ? CueMode::Block
                                     : w == QLatin1String("default") ? CueMode::ListDefault
                                     : CueMode::CueOnly;
                m_st->setCueMode(ctx.cue, mode);
            }
            else if (w == QLatin1String("base"))
            {
                r.word();
                m_st->setBaseCue(ctx.list, ctx.cue);
            }
            else
            {
                return tr("Unknown word “%1” (track, block, cueonly, default, base)").arg(w);
            }
        }
        return QString();
    };

    // Groups named one after another (or joined with '+') make ONE layer,
    // as a drop of several does; a ',' starts another layer, as an ⌥-drop
    // makes one per group.
    auto targets = [&]() {
        QList<Target> out;
        const QStringList names = targetNames();
        Target t;
        auto addName = [&](const QString &n) {
            // The first group of that name (by id, as Doc lists them), then
            // one by its folder path, then a fixture.
            for (const FixtureGroup *g : m_doc->fixtureGroups())
                if (g->name().compare(n, Qt::CaseInsensitive) == 0)
                {
                    if (!t.groups.contains(g->id()))
                        t.groups << g->id();
                    return;
                }
            for (const FixtureGroup *g : m_doc->fixtureGroups())
            {
                QString full = g->path();
                if (full.isEmpty())
                    continue;
                if (!full.endsWith(QLatin1Char('/')))
                    full += QLatin1Char('/');
                if ((full + g->name()).compare(n, Qt::CaseInsensitive) == 0)
                {
                    if (!t.groups.contains(g->id()))
                        t.groups << g->id();
                    return;
                }
            }
            for (const Fixture *f : m_doc->fixtures())
                if (f->name().compare(n, Qt::CaseInsensitive) == 0)
                {
                    if (!t.fixtures.contains(f->id()))
                        t.fixtures << f->id();
                    return;
                }
        };
        for (;;)
        {
            const int save = r.i;
            const bool comma = r.comma();
            if (comma && !t.isEmpty())
            {
                out << t;
                t = Target();
            }
            if (!comma)
                r.plus();
            const QString n = r.name(names);
            if (n.isEmpty())
            {
                r.i = save;
                break;
            }
            addName(n);
        }
        if (!t.isEmpty())
            out << t;
        return out;
    };

    // The selected cue's own layer on @a t: there, or made (a carried-in
    // one is brought into the cue).
    auto layerOn = [&](const Target &t) -> quint32 {
        if (const Cue *c = m_st->cue(ctx.cue))
            for (const Layer &l : c->layers)
                if (l.target == t)
                    return l.id;
        return m_st->addLayer(ctx.cue, t);
    };

    // attribute value, attribute value… on @a layers (none: the cue's looks)
    auto assigns = [&](const QList<quint32> &layers) -> QString {
        QString effect;         // the effect just set: a colour/dimmer after it is its own
        int n = 0;
        auto setLook = [&](const Look &look) {
            if (layers.isEmpty())
                m_st->setCueLook(ctx.cue, look);
            for (quint32 lid : layers)
                m_st->setLayerLook(ctx.cue, lid, look);
        };
        while (!r.atEnd())
        {
            const QString kw = r.word();
            const QString key = attributeKey(kw);
            if (key.isEmpty())
                return tr("Expected an attribute (colour, dimmer, position, gobo, zoom, beam, shutter, "
                          "effect), got “%1”.").arg(kw);
            const QString next = r.peek().toLower();

            if (next == QLatin1String("none") || next == QLatin1String("off")
                || next == QLatin1String("clear"))
            {
                r.word();
                const bool clear = next == QLatin1String("clear");
                QStringList keys { key };
                if (key == KeyEffectPrefix)
                {
                    // Every effect that plays there (each kind is a key).
                    keys.clear();
                    // Read afresh: an edit before this one replaced the tree.
                    const CueList *now = m_st->cueList(ctx.list);
                    const QList<ResolvedCue> rc = now == nullptr ? QList<ResolvedCue>() : resolve(*now);
                    const int at = now == nullptr ? -1 : now->indexOf(ctx.cue);
                    const Cue *c = m_st->cue(ctx.cue);
                    if (at >= 0 && c != nullptr)
                    {
                        if (layers.isEmpty())
                        {
                            for (const Look &l : c->looks)
                                if (isEffectKey(l.key))
                                    keys << l.key;
                        }
                        for (quint32 lid : layers)
                            if (const Layer *l = c->layer(lid))
                                if (const ResolvedLayer *rl = rc.at(at).layer(l->target))
                                    for (const QString &k : rl->values.keys())
                                        if (isEffectKey(k) && !keys.contains(k))
                                            keys << k;
                    }
                }
                for (const QString &k : keys)
                {
                    Look l;
                    l.key = k;
                    l.none = !clear;
                    setLook(l);
                }
                effect.clear();
                ++n;
                continue;
            }

            if (key == KeyEffectPrefix)
            {
                const QString name = r.name(paletteNames(KeyEffectPrefix));
                if (name.isEmpty())
                    return tr("No effect “%1”. Tab lists them.").arg(r.peek());
                const QLCPalette *p = nullptr;
                for (const QLCPalette *have : m_doc->palettes())
                    if (have->name().compare(name, Qt::CaseInsensitive) == 0
                        && isEffectKey(m_st->keyForPalette(have->id())) && !m_st->isInlinePalette(have->id()))
                        p = have;
                const Look look = m_st->lookForPalette(p->id());
                setLook(look);
                effect = look.key;
                ++n;
                continue;
            }

            // A value: a palette by name, a level (45, 45%) or a colour (#33aa55).
            quint32 pid = QLCPalette::invalidId();
            const QString name = r.name(paletteNames(key));
            if (!name.isEmpty())
            {
                for (const QLCPalette *have : m_doc->palettes())
                    if (have->name().compare(name, Qt::CaseInsensitive) == 0
                        && m_st->keyForPalette(have->id()) == key && !m_st->isInlinePalette(have->id()))
                        pid = have->id();
            }
            else
            {
                static const QRegularExpression level(QStringLiteral("^(\\d{1,3})%?$"));
                static const QRegularExpression hex(QStringLiteral("^#[0-9a-fA-F]{6}$"));
                const QString w = r.peek();
                const QRegularExpressionMatch m = level.match(w);
                if (key == KeyDimmer && m.hasMatch() && m.captured(1).toInt() <= 100)
                {
                    r.word();
                    // The palette that plays exactly that, else a new named
                    // one (D11/D21 as changed 2026-10-17): adjusting it later
                    // adjusts every cue that uses it.
                    const QVariantList v { qRound(m.captured(1).toInt() * 255.0 / 100.0) };
                    pid = m_st->findPalette(QLCPalette::Dimmer, v, ctx.list, ctx.recentPalettes);
                    if (pid == QLCPalette::invalidId())
                    {
                        pid = m_st->createNamedPalette(QLCPalette::Dimmer, v);
                        madeNew << pid;
                    }
                    valuePalettes << pid;
                }
                else if (key == KeyColour && hex.match(w).hasMatch())
                {
                    r.word();
                    const QVariantList v { QLCPalette::colorToString(QColor(w), QColor(Qt::black)) };
                    pid = m_st->findPalette(QLCPalette::Color, v, ctx.list, ctx.recentPalettes);
                    if (pid == QLCPalette::invalidId())
                    {
                        pid = m_st->createNamedPalette(QLCPalette::Color, v);
                        madeNew << pid;
                    }
                    valuePalettes << pid;
                }
                else
                {
                    return tr("No %1 palette “%2”. Tab lists them.").arg(keyLabel(key).toLower(), w);
                }
            }
            if (pid == QLCPalette::invalidId())
                return tr("Couldn't make that value.");

            if (!effect.isEmpty() && (key == KeyColour || key == KeyDimmer))
            {
                // After an effect: that effect's own look (inside it only).
                const Cue *c = m_st->cue(ctx.cue);
                if (layers.isEmpty())
                {
                    const Look *have = c == nullptr ? nullptr : c->look(effect);
                    if (have != nullptr)
                    {
                        Look l = *have;
                        l.own << pid;
                        m_st->setCueLook(ctx.cue, l);
                    }
                }
                for (quint32 lid : layers)
                {
                    c = m_st->cue(ctx.cue);
                    const Layer *layer = c == nullptr ? nullptr : c->layer(lid);
                    const Look *have = layer == nullptr ? nullptr : layer->look(effect);
                    if (have == nullptr)
                        continue;
                    Look l = *have;
                    l.own << pid;
                    m_st->setLayerLook(ctx.cue, lid, l);
                }
            }
            else
            {
                Look l = m_st->lookForPalette(pid);
                if (l.key.isEmpty())
                    return tr("That palette sets nothing the tree knows.");
                setLook(l);
                effect.clear();
            }
            ++n;
        }
        return n == 0 ? tr("Nothing to set.") : QString();
    };

    const CueList *list = m_st->cueList(ctx.list);

    if (w0 == QLatin1String("list"))
    {
        r.word();
        QString nm = r.quoted();
        if (nm.isNull())
            nm = r.src.mid(r.i).trimmed();
        quint32 have = invalidId();
        for (const Production &p : m_st->productions())
            for (const CueList &l : p.lists)
                if (l.name.compare(nm, Qt::CaseInsensitive) == 0
                    && (have == invalidId() || p.id == ctx.production))
                {
                    if (have == invalidId() || p.id == ctx.production)
                        have = l.id;
                }
        if (nm.isEmpty())
            error = tr("list \"name\"");
        else if (have != invalidId())
        {
            // There already: select it.
            quint32 prod = invalidId();
            m_st->cueList(have, &prod);
            ctx.production = prod;
            ctx.list = have;
            ctx.cue = invalidId();
        }
        else if (m_st->production(ctx.production) == nullptr)
            error = tr("Select a production first (+ Production).");
        else
        {
            ctx.list = m_st->addCueList(ctx.production, nm, ListMode::Tracking);
            ctx.cue = invalidId();
        }
    }
    else if (list == nullptr)
    {
        error = tr("Select a cue list first (or: list \"name\").");
    }
    else if (w0 == QLatin1String("cue"))
    {
        r.word();
        const QString w1 = r.peek().toLower();
        static const QRegularExpression number(QStringLiteral("^\\d+(\\.\\d+)*$"));
        if (w1 == QLatin1String("look"))
        {
            r.word();
            error = ctx.cue == invalidId() ? tr("Select a cue first.") : assigns({});
        }
        else if (w1 == QLatin1String("+") || w1 == QLatin1String("new"))
        {
            r.word();
            const QString nm = r.quoted();
            const int at = ctx.cue == invalidId() ? int(list->cues.count()) - 1 : list->indexOf(ctx.cue);
            ctx.cue = m_st->addCue(ctx.list, at, nm);
            error = ctx.cue == invalidId() ? tr("Couldn't add a cue.") : modeWords();
        }
        else if (number.match(w1).hasMatch())
        {
            r.word();
            const QString nm = r.quoted();
            const int idx = indexOfNumber(*list, w1);
            if (idx < 0)
                ctx.cue = m_st->addCueNumbered(ctx.list, w1, nm);
            else
            {
                ctx.cue = list->cues.at(idx).id;
                if (!nm.isNull() && nm != list->cues.at(idx).name)
                    m_st->renameNode(ctx.cue, nm);
            }
            error = ctx.cue == invalidId() ? tr("Couldn't add cue %1.").arg(w1) : modeWords();
        }
        else
        {
            error = tr("cue <number> \"name\" · cue + \"name\" · cue look <attribute> <value>");
        }
    }
    else if (QStringList({ "track", "block", "cueonly", "cue-only", "default", "base" }).contains(w0))
    {
        error = ctx.cue == invalidId() ? tr("Select a cue first.") : modeWords();
    }
    else if (ctx.cue == invalidId() || list->indexOf(ctx.cue) < 0)
    {
        error = tr("Select a cue first (or: cue 1.0.0 \"name\").");
    }
    else if (w0 == QLatin1String("dup"))
    {
        r.word();
        const quint32 id = m_st->duplicateCue(ctx.cue);
        if (id == invalidId())
            error = tr("Couldn't duplicate.");
        else
            ctx.cue = id;
    }
    else if (w0 == QLatin1String("include"))
    {
        r.word();
        const QString num = r.word();
        const int idx = indexOfNumber(*list, num);
        if (idx < 0)
            error = tr("include <cue number>: no cue %1").arg(num);
        else
        {
            const quint32 inc = list->cues.at(idx).id;
            const Cue *c = list->cue(ctx.cue);
            QString why;
            if (c->includes.contains(inc))
                m_st->removeInclude(ctx.cue, inc);
            else if (!m_st->addInclude(ctx.cue, inc, &why))
                error = tr("Can't include %1: %2").arg(num, why);
        }
    }
    else if (w0 == QLatin1String("function") || w0 == QLatin1String("fn"))
    {
        // Functions played as they are: an opaque layer each.
        r.word();
        const QStringList names = functionNames();
        int n = 0;
        for (;;)
        {
            const int save = r.i;
            if (!r.comma())
                r.plus();
            const QString nm = r.name(names);
            if (nm.isEmpty())
            {
                r.i = save;
                break;
            }
            for (const Function *f : m_doc->functions())
                if (f->name().compare(nm, Qt::CaseInsensitive) == 0 && !m_st->isGenerated(f->id()))
                {
                    Target t;
                    t.function = f->id();
                    const quint32 lid = layerOn(t);
                    if (lid != invalidId())
                        res.layer = lid;
                    ++n;
                    break;
                }
        }
        if (n == 0)
            error = tr("function <name>: no function “%1”. Tab lists them.").arg(r.peek());
        else if (!r.atEnd())
            error = tr("function <name>, <name>… -- then nothing more (“%1”)").arg(r.peek());
    }
    else if (w0 == QLatin1String("release"))
    {
        r.word();
        const QList<Target> ts = targets();
        if (ts.isEmpty())
            error = tr("release <group>");
        for (const Target &t : ts)
        {
            bool now = false;
            if (const Cue *c = m_st->cue(ctx.cue))
                for (const Layer &l : c->layers)
                    if (l.target == t)
                        now = l.release;
            m_st->setTargetRelease(ctx.cue, t, !now);
        }
    }
    else
    {
        if (w0 == QLatin1String("layer"))
            r.word();
        const QList<Target> ts = targets();
        if (ts.isEmpty())
        {
            error = tr("Don't know “%1”. Start with cue, a group or fixture, track/block/cueonly, base, "
                       "release, include, function, record, list, dup, undo.").arg(r.peek());
        }
        else
        {
            QList<quint32> layers;
            for (const Target &t : ts)
            {
                const quint32 lid = layerOn(t);
                if (lid != invalidId())
                    layers << lid;
            }
            if (!r.atEnd())
                error = assigns(layers);
            if (!layers.isEmpty())
                res.layer = layers.last();
        }
    }

    m_st->endBatch(error.isEmpty());
    if (!error.isEmpty())
    {
        res.message = error;
        res.layer = invalidId();
        return res;
    }
    res.ok = true;
    res.message = QStringLiteral("✓ %1").arg(r.src);
    QStringList said;
    for (quint32 pid : valuePalettes)
        if (const QLCPalette *p = m_doc->palette(pid))
            said << (madeNew.contains(pid) ? tr("new palette “%1” in %2").arg(p->name(), p->path())
                                           : tr("reused “%1”").arg(p->name()));
    if (!said.isEmpty())
        res.message += QStringLiteral(" — ") + said.join(QStringLiteral(", "));
    res.context = ctx;
    return res;
}

/****************************************************************************
 * Tab completion
 ****************************************************************************/

bool ShowTreeCommand::candidates(const QString &head, const Context &context, QStringList *out) const
{
    // Read @a head as run() would, word by word, and say what may come next.
    // A head that doesn't read (a misspelt name) offers nothing.
    out->clear();
    Reader r;
    r.src = head;
    const QStringList modes({ "track", "block", "cueonly", "default", "base" });
    const QStringList none({ "none", "off", "clear" });
    const QStringList targets = targetNames();

    // attribute value, attribute value… to the end.
    auto assigns = [&]() -> bool {
        for (;;)
        {
            if (r.atEnd())
            {
                *out = attributeWords();
                return true;
            }
            const QString key = attributeKey(r.word());
            if (key.isEmpty())
                return false;
            if (r.atEnd())
            {
                *out = paletteNames(key) + none;
                return true;
            }
            const QString next = r.peek().toLower();
            if (none.contains(next))
            {
                r.word();
                continue;
            }
            if (!r.name(paletteNames(key)).isEmpty())
                continue;
            static const QRegularExpression value(QStringLiteral("^(\\d{1,3}%?|#[0-9a-fA-F]{6})$"));
            if (key == KeyEffectPrefix || !value.match(next).hasMatch())
                return false;
            r.word();
        }
    };
    // Names one after another (or joined by '+', or ',' for another layer).
    auto names = [&](const QStringList &all, bool thenAssign) -> bool {
        bool joined = false;
        int n = 0;
        for (;;)
        {
            if (r.atEnd())
            {
                *out = all;
                if (thenAssign && !joined && n > 0)
                    *out += attributeWords();
                return true;
            }
            const int save = r.i;
            joined = r.comma() || r.plus();
            if (joined && r.atEnd())
                continue;
            if (r.name(all).isEmpty())
            {
                r.i = save;
                break;
            }
            joined = false;
            ++n;
        }
        return n > 0 && thenAssign && assigns();
    };
    auto modeWords = [&]() -> bool {
        while (!r.atEnd())
        {
            const QString w = r.word().toLower();
            if (!modes.contains(w) && w != QLatin1String("cue-only"))
                return false;
        }
        *out = modes;
        return true;
    };

    if (r.atEnd())
    {
        *out = keywords() + targets;
        return true;
    }
    const QString w0 = r.peek().toLower();
    if (w0 == QLatin1String("record"))
    {
        r.word();
        if (!r.atEnd())
            return r.word(), r.atEnd();       // one word, then nothing
        *out = QStringList({ "selected", "running", "lit" });
        return true;
    }
    if (w0 == QLatin1String("undo") || w0 == QLatin1String("redo") || w0 == QLatin1String("dup"))
        return true;
    if (w0 == QLatin1String("list"))
    {
        r.word();
        if (r.atEnd())
            *out = listNames(context);
        return true;
    }
    if (w0 == QLatin1String("include"))
    {
        r.word();
        if (r.atEnd())
            *out = cueNumbers(context);
        return true;
    }
    if (w0 == QLatin1String("function") || w0 == QLatin1String("fn"))
    {
        r.word();
        return names(functionNames(), false);
    }
    if (w0 == QLatin1String("release"))
    {
        r.word();
        return names(targets, false);
    }
    if (w0 == QLatin1String("cue"))
    {
        r.word();
        if (r.atEnd())
        {
            *out = QStringList({ "look", "+" }) + cueNumbers(context);
            return true;
        }
        const QString w1 = r.word().toLower();
        if (w1 == QLatin1String("look"))
            return assigns();
        static const QRegularExpression number(QStringLiteral("^\\d+(\\.\\d+)*$"));
        if (w1 != QLatin1String("+") && w1 != QLatin1String("new") && !number.match(w1).hasMatch())
            return false;
        if (r.src.mid(r.i).trimmed().startsWith(QLatin1Char('"')))
            r.quoted();
        return modeWords();
    }
    if (modes.contains(w0) || w0 == QLatin1String("cue-only"))
        return modeWords();
    if (w0 == QLatin1String("layer"))
        r.word();
    return names(targets, true);
}

QString ShowTreeCommand::typed(const QString &name, bool quoted)
{
    // As the reader takes it: whole, unless a ',' or '+' in it would split it.
    if (quoted || name.contains(QLatin1Char(',')) || name.contains(QLatin1Char('+')))
        return QStringLiteral("\"%1\"").arg(name);
    return name;
}

ShowTreeCommand::Completion ShowTreeCommand::completion(const QString &line, const Context &context) const
{
    Completion c;
    auto matching = [&](const QString &head, const QString &part, QStringList *out) -> bool {
        QStringList cands;
        if (!candidates(head, context, &cands))
            return false;
        out->clear();
        for (const QString &n : cands)
            if (n.startsWith(part, Qt::CaseInsensitive) && !out->contains(n, Qt::CaseInsensitive))
                *out << n;
        std::sort(out->begin(), out->end(), [](const QString &a, const QString &b) {
            return a.compare(b, Qt::CaseInsensitive) < 0;
        });
        return true;
    };

    // A name begun with a quote: everything after it is the word.
    if (line.count(QLatin1Char('"')) % 2 == 1)
    {
        const int q = line.lastIndexOf(QLatin1Char('"'));
        c.head = line.left(q);
        c.partial = line.mid(q + 1);
        c.quoted = true;
        matching(c.head, c.partial, &c.matches);
        return c;
    }

    // The word may hold spaces ("AT W" of "AT W ALL"): try it from each
    // word start back from the end, the longest first; the first whose
    // head reads and that something matches wins.
    QList<int> starts;
    for (int i = line.length(); i >= 0 && starts.count() < 6; --i)
    {
        if (i == 0 || line.at(i - 1) == QLatin1Char(' ') || line.at(i - 1) == QLatin1Char(',')
            || line.at(i - 1) == QLatin1Char('+'))
            starts.prepend(i);
    }
    for (int start : starts)
    {
        const QString part = line.mid(start);
        if (part.contains(QLatin1Char('"')) || part.startsWith(QLatin1Char(' ')))
            continue;
        QStringList m;
        if (!matching(line.left(start), part, &m) || m.isEmpty())
            continue;
        if (part.isEmpty() && start != line.length())
            continue;
        c.head = line.left(start);
        c.partial = part;
        c.matches = m;
        return c;
    }
    c.head = line;
    return c;
}

QString ShowTreeCommand::complete(const QString &line, const Context &context, QStringList *options) const
{
    const Completion c = completion(line, context);
    if (options != nullptr)
        *options = c.matches.mid(0, 10);
    if (c.matches.isEmpty() || c.partial.isEmpty())
        return line;                // nothing, or nothing typed yet: listed only
    if (c.matches.count() == 1)
        return c.head + typed(c.matches.first(), c.quoted) + QLatin1Char(' ');
    // The part every match shares (spelt as the first has it).
    QString common = c.matches.first();
    for (const QString &m : c.matches)
        while (!m.startsWith(common, Qt::CaseInsensitive))
            common.chop(1);
    if (common.length() <= c.partial.length())
        return line;
    return c.head + (c.quoted ? QStringLiteral("\"") : QString()) + common;
}

QString ShowTreeCommand::tab(const QString &line, const Context &context, QStringList *options, int *current)
{
    if (line == m_tabLast && m_tabMatches.count() > 1)
    {
        // Again on what Tab gave: the next match, round and round.
        m_tabIndex = (m_tabIndex + 1) % int(m_tabMatches.count());
        m_tabLast = m_tabHead + typed(m_tabMatches.at(m_tabIndex), m_tabQuoted);
        if (options != nullptr)
            *options = m_tabMatches;
        if (current != nullptr)
            *current = m_tabIndex;
        return m_tabLast;
    }
    const Completion c = completion(line, context);
    QString out = complete(line, context);
    m_tabHead = c.head;
    m_tabQuoted = c.quoted;
    m_tabMatches = c.matches.count() > 1 ? c.matches : QStringList();
    m_tabIndex = -1;
    m_tabLast = out;
    if (options != nullptr)
        *options = c.matches;
    if (current != nullptr)
        *current = -1;
    return out;
}
