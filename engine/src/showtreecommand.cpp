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
              "include 0.0.3 · record [selected|running|lit] · dup · undo — Tab completes, ↑ recalls");
}

QStringList ShowTreeCommand::keywords()
{
    return { QStringLiteral("cue"), QStringLiteral("look"), QStringLiteral("track"),
             QStringLiteral("block"), QStringLiteral("cueonly"), QStringLiteral("default"),
             QStringLiteral("base"), QStringLiteral("release"), QStringLiteral("include"),
             QStringLiteral("record"), QStringLiteral("dup"), QStringLiteral("undo"),
             QStringLiteral("redo"), QStringLiteral("list"), QStringLiteral("layer") };
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
        const QStringList names = groupNames();
        Target t;
        auto addName = [&](const QString &n) {
            // The first group of that name (by id, as Doc lists them).
            for (const FixtureGroup *g : m_doc->fixtureGroups())
                if (g->name().compare(n, Qt::CaseInsensitive) == 0)
                {
                    if (!t.groups.contains(g->id()))
                        t.groups << g->id();
                    break;
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
        if (nm.isEmpty())
            error = tr("list \"name\"");
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
            error = tr("Don't know “%1”. Start with cue, a group, track/block/cueonly, base, release, "
                       "include, record, dup, undo.").arg(r.peek());
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

QString ShowTreeCommand::complete(const QString &line, const Context &context, QStringList *options) const
{
    const QStringList words = line.split(QLatin1Char(' '));
    const QString first = words.first().toLower();
    for (int n = qMin(3, int(words.count())); n >= 1; --n)
    {
        const QString pre = words.mid(words.count() - n).join(QLatin1Char(' '));
        const QString prev = words.count() - n - 1 >= 0 ? words.at(words.count() - n - 1).toLower() : QString();
        const QString key = attributeKey(prev);

        QStringList cands;
        if (prev == QLatin1String("record") && words.count() - n == 1)
            cands = QStringList({ QStringLiteral("selected"), QStringLiteral("running"), QStringLiteral("lit") });
        else if (prev == QLatin1String("cue") && words.count() - n == 1)
            cands = QStringList({ QStringLiteral("look"), QStringLiteral("+") }) + cueNumbers(context);
        else if (prev == QLatin1String("include"))
            cands = cueNumbers(context);
        else if (!key.isEmpty())
            cands = paletteNames(key) << QStringLiteral("none");
        else if (words.count() - n == 0)
            cands = keywords() + groupNames();
        else if (first == QLatin1String("release"))
            cands = groupNames();
        else
            cands = groupNames() + attributeWords() + QStringList({ "track", "block", "cueonly", "base" });

        if (pre.isEmpty())
        {
            if (n > 1)
                continue;
            if (options != nullptr)
                *options = cands.mid(0, 10);
            return line;
        }
        // "Towers,US" / "Towers+US": complete the name after the ',' or '+'.
        QString lead;
        QString part = pre;
        if (n == 1)
        {
            const int cut = qMax(pre.lastIndexOf(QLatin1Char(',')), pre.lastIndexOf(QLatin1Char('+')));
            if (cut >= 0)
            {
                lead = pre.left(cut + 1);
                part = pre.mid(cut + 1);
                if (part.isEmpty())
                    continue;
                cands = groupNames();
            }
        }
        QStringList match;
        for (const QString &c : cands)
            if (c.startsWith(part, Qt::CaseInsensitive) && !match.contains(c))
                match << c;
        if (match.isEmpty())
            continue;
        std::sort(match.begin(), match.end(), [](const QString &a, const QString &b) {
            return a.compare(b, Qt::CaseInsensitive) < 0;
        });
        if (options != nullptr)
            *options = match.mid(0, 10);
        QString rep;
        if (match.count() == 1)
        {
            rep = match.first() + QLatin1Char(' ');
        }
        else
        {
            rep = match.first();
            for (const QString &m : match)
                while (!m.startsWith(rep, Qt::CaseInsensitive))
                    rep.chop(1);
            if (rep.length() < part.length())
                rep = part;
        }
        rep = lead + rep;
        const QString base = words.mid(0, words.count() - n).join(QLatin1Char(' '));
        return base.isEmpty() ? rep : base + QLatin1Char(' ') + rep;
    }
    if (options != nullptr)
        options->clear();
    return line;
}
