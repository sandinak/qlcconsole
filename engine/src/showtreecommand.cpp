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
#include "monitorproperties.h"
#include "stagetarget.h"
#include "paletteops.h"
#include "doc.h"
#include <QUrl>
#include <QVector>

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
              "US in Steps colour Red (a child layer) · set \"Towers\" SL SR · "
              "Towers aim DS-Edge spread 0 group pairs cross mirror · cue look spread +5 · "
              "cue look colour Red · Steps effect Sinelon colour Red · gobo none · release Towers · "
              "name \"Hero\" · T1-CW … (a fixture) · function Haze · include 0.0.3 · "
              "record [selected|running|lit] · list \"name\" · dup · reusable · use BackStage · unlink · "
              "detach [list] · convert [apply] · undo — Tab completes every word "
              "(again: the next), ↑ recalls");
}

/****************************************************************************
 * The language: one table, for the parser's words and for `help`
 ****************************************************************************/

const QList<ShowTreeCommand::Word> &ShowTreeCommand::words()
{
    // Each word with its syntax, what it does and examples: `help` reads
    // this, keywords() and attributeWords() come from it, and a test runs
    // every example through the parser.
    static const QList<Word> table = [] {
        QList<Word> t;
        auto add = [&t](const char *word, const char *group, const char *syntax, const char *what,
                        QStringList examples, QStringList related = {}, QStringList aliases = {},
                        bool attribute = false) {
            Word w;
            w.word = QString::fromUtf8(word);
            w.group = QString::fromUtf8(group);
            w.syntax = QString::fromUtf8(syntax);
            w.what = tr(what);
            w.examples = examples;
            w.related = related;
            w.aliases = aliases;
            w.attribute = attribute;
            t << w;
        };
        // navigate
        add("list", "navigate", "list \"name\"",
            "Selects that cue list, or makes a new Tracking one in the selected production.",
            { "list \"01-Opener\"" }, { "cue" });
        add("cue", "navigate", "cue <number> [\"name\"] [track|block|cueonly|default] [base] | cue + [\"name\"] … | "
                                "cue look <attribute> <value>…",
            "Selects cue <number>, or makes it in number order (renamed if the name differs); "
            "cue + makes a new cue after the selected one, numbered as + Cue does; cue look sets a look "
            "for the whole cue (it reaches the cue's own layers that don't set it).",
            { "cue 1.0.1 \"Verse\"", "cue + \"Chorus\" block", "cue look colour Blue", "cue look spread +5" },
            { "list", "track", "copy" }, { "new" });
        // build
        add("layer", "build", "[layer] <groups> [, <groups>…] [in <parent>] <attribute> <value>…",
            "The selected cue's layer on those groups (made if needed; a grey carried-in one is brought "
            "in), then its looks. Names one after another (or joined with +) are ONE layer; a comma "
            "starts another. A group, a set, a fixture, a named layer or a folder path "
            "(Movers/Front/Spots). With layers selected in the tree, start with the attribute "
            "(colour Blue, dimmer 45, release on) and it acts on all of them.",
            { "Towers colour Blue dimmer 45", "Towers USTs dimmer 100", "Towers, USTs dimmer 100",
              "layer Steps colour Red" }, { "in", "set", "colour" });
        add("in", "build", "<groups> in <parent groups> <attribute> <value>…",
            "A child layer: on part of the cue's layer on <parent>. It sets only what it sets; its parent "
            "lights the rest of it, and doesn't light what it sets.",
            { "US in Steps colour Red", "US in Steps dimmer 0" }, { "layer" });
        add("set", "build", "set \"name\" <groups>…",
            "Makes a group set -- one name for several groups, reusable in any cue (e.g. Towers = SL + "
            "SR) -- or gives an existing set its members again. Spread across a set, lights are "
            "numbered by where they stand, not by group.",
            { "set \"Towers\" SL SR" }, { "layer" });
        add("function", "build", "function <name> [, <name>…]",
            "Functions played as they are (an EFX, an RGB Matrix, a scene with channel values): a layer "
            "each.", { "function Haze" }, { "layer" }, { "fn" });
        add("name", "build", "name \"text\"",
            "Names the selected layer (else the cue). \"\" goes back to the automatic name (the layer's "
            "groups). The layer stays the same layer: tracking is unchanged.",
            { "name \"Hero\"", "name \"\"" }, { "layer", "cue" });
        // looks (attributes)
        add("colour", "looks", "<groups> colour <palette>[ <palette>…] | #rrggbb | none | clear",
            "The Colour look: palettes by name (colours 1, 2…), or a hex colour (the palette playing "
            "exactly it is reused, else one is made, named as yours are).",
            { "Towers colour Blue", "Steps colour #33aa55" }, { "dimmer", "none" }, { "color" }, true);
        add("dimmer", "looks", "<groups> dimmer <palette> | <0-100>[%] | none | clear",
            "The Dimmer look: a palette by name, or a level (reused, or a palette made for it).",
            { "Towers dimmer 45", "Towers dimmer Full" }, { "colour" }, { "dim", "level" }, true);
        add("position", "looks", "<groups> position <palette> | none | clear",
            "The Position look: a Pan/Tilt or Aim palette by name.",
            { "Towers position Aim DS-Edge" }, { "aim" }, { "pos" }, true);
        add("aim", "looks", "<groups> aim <target>|<Aim palette>",
            "Position aimed at a stage target (a point, an area or a line) by name: the Aim palette "
            "pointing there is reused, else one is made. On a line, the lights spread along it.",
            { "Towers aim DS-Edge", "cue look aim DS-Edge" }, { "spread", "group", "cross" }, {}, true);
        add("spread", "looks", "<groups> spread <-5…+5> | none | clear",
            "A line aim's Spread: +5 end to end, 0 all on the centre, below 0 crossed. On the cue it "
            "reaches only layers aiming at a line, as one formation.",
            { "Towers aim DS-Edge spread 0", "cue look spread +5", "Towers spread -2.5" },
            { "aim", "group", "cross" }, {}, true);
        add("group", "looks", "<groups> group each|pairs|threes|fours|halves | none | clear",
            "A line aim's Grouping: which lights move together across the centre.",
            { "Towers group pairs" }, { "spread", "cross" }, { "grouping" }, true);
        add("cross", "looks", "<groups> cross swap|mirror | none | clear",
            "A line aim's Cross, below spread 0: groups swap sides keeping their order, or each group "
            "flips in place.", { "Towers cross mirror" }, { "spread", "group" }, {}, true);
        add("gobo", "looks", "<groups> gobo <palette> | none | clear",
            "The Gobo look (by name per fixture type).", { "Towers gobo Breakup", "Towers gobo none" },
            { "zoom", "beam" }, {}, true);
        add("zoom", "looks", "<groups> zoom <palette> | none | clear", "The Zoom look.",
            { "Towers zoom none" }, { "gobo", "beam" }, {}, true);
        add("beam", "looks", "<groups> beam <palette> | none | clear", "The Focus / Frost / Iris look.",
            { "Towers beam clear" }, { "gobo", "zoom" }, { "focus", "frost", "iris" }, true);
        add("shutter", "looks", "<groups> shutter <palette> | none | clear", "The Shutter (and strobe) look.",
            { "Towers shutter none" }, { "dimmer" }, { "strobe" }, true);
        add("effect", "looks", "<groups> effect <palette> [colour <palette>] [dimmer <palette>] | none | clear",
            "An effect; a colour or dimmer right after it is the effect's own. On the cue: one effect "
            "across its layers (those setting that effect themselves are left out).",
            { "Steps effect Sinelon colour Red", "cue look effect Sinelon", "Steps effect none" },
            { "colour" }, { "fx" }, true);
        add("none", "looks", "<groups> <attribute> none | off | clear",
            "none (or off): the attribute is off here, whatever it would inherit. clear: takes this "
            "layer's value away, so it inherits again.",
            { "Towers gobo none", "Towers dimmer clear" }, { "colour" }, { "off", "clear" });
        // tracking
        add("track", "tracking", "track | block | cueonly | default",
            "The selected cue's mode: Track (starts from the cue before), Block (from nothing), "
            "Cue-only (its changes don't pass on), default (the list's).",
            { "block", "track", "cue + \"Top\" block" }, { "base", "release" },
            { "block", "cueonly", "cue-only", "default" });
        add("base", "tracking", "base", "Makes the selected cue the list's base cue: released layers go "
            "back to it.", { "base" }, { "release" });
        add("release", "tracking", "release <groups> [, <groups>…]",
            "Turns Release on the cue's layer on, or off again: leaving the cue it goes back to the "
            "base cue, else dark.", { "release Towers" }, { "base" });
        add("include", "tracking", "include <cue number>",
            "Includes that cue's output (as if standing in it); again, takes it out.",
            { "include 0.0.3" }, { "track" });
        // record & copy
        add("record", "record", "record [selected|running|lit]",
            "Record as new cue: what is on stage frozen into a new Block cue after the selected one -- "
            "the selected cue, the list's running cue, or everything lit (your setting, or it asks).",
            { "record", "record running" }, { "dup", "copy" });
        add("dup", "record", "dup", "Duplicates the selected cue right after it (numbered for you).",
            { "dup" }, { "copy", "paste" });
        add("copy", "record", "copy [cue <number>…] | copy layer <groups>",
            "Copies the selected cue (or cues by number, or the selected cue's layer on those groups) "
            "for paste. In the tree, ⌘C does the same.",
            { "copy", "copy cue 1.0.1", "copy layer Towers" }, { "paste", "dup" });
        add("paste", "record", "paste",
            "Pastes what copy took: cues right after the selected cue (numbered in turn, never "
            "renumbering), or layers into the selected cue. One undo step. ⌘V in the tree.",
            { "paste" }, { "copy" });
        // reuse (P4)
        add("reusable", "reuse", "reusable [\"name\"]",
            "Make reusable: the selected layer (else the selected cue) goes into the Library as a "
            "reusable layer (cue), named as given (else its own name), and stays its first use. Every use "
            "is live-linked: change one, they all change.",
            { "reusable", "reusable \"BackStage\"" }, { "use", "unlink" });
        add("use", "reuse", "use <reusable>",
            "Puts a reusable in: a reusable layer as a layer of the selected cue, a reusable cue as a new "
            "cue after it. Live-linked to the others.",
            { "use BackStage", "use \"Walk-in\"" }, { "reusable", "unlink" });
        add("unlink", "reuse", "unlink",
            "Unlink here: the selected layer (else cue) stops being a use of its reusable and keeps what "
            "it is now as a plain copy of its own. The other uses stay linked.",
            { "unlink" }, { "reusable", "detach" });
        add("detach", "reuse", "detach | detach list",
            "Detach the selected layer to a plain scene (the cue then plays that scene as it is), or the "
            "selected cue list to a plain chaser: its chaser and collections become ordinary functions "
            "and the list leaves the tree.",
            { "detach", "detach list" }, { "unlink" });
        add("convert", "reuse", "convert | convert apply",
            "Convert to tracking: shows what would change in the selected (imported) cue list -- values "
            "that only repeat what tracks in taken out, includes of the cue before made tracking -- then "
            "convert apply does it, one undo step. Every cue plays exactly as before.",
            { "convert", "convert apply" }, { "track" });
        // palettes (R2): the Library's housekeeping, typed
        add("palette", "palettes", "palette <type> [\"name\"] [#rrggbb | <0-100>] [in <folder>] | "
                                   "palette <name> rename \"new\" | move <folder>|none | dup | delete [anyway] | edit | "
                                   "palette folder <folder> [rename \"new\" | delete]",
            "Palettes as the Library keeps them: a new one of a type (colour, dimmer, position, aim, beam, gobo, "
            "zoom, shutter, strobe, effect), named, valued and filed if you say so, opened in the Look Editor; a "
            "palette renamed, moved to a folder (folders hold any type), duplicated, deleted (refused while "
            "something uses it, unless anyway) or edited; folders made, renamed, deleted (what is in one moves "
            "up). One undo step each.",
            { "palette colour \"Amber 2\" #ffb000", "palette dimmer \"Half\" 50 in \"Levels\"",
              "palette Blue rename \"Navy\"", "palette Blue move \"01-Song\"", "palette Blue dup",
              "palette Blue edit", "palette folder \"01-Song\"", "palette folder \"01-Song\" rename \"02-Song\"" },
            { "colour", "look" }, { "pal" });
        // looks (R3): busking looks beside the cue lists
        add("look", "busking", "look \"name\" [in <folder>]",
            "Selects that busking look of the selected production, or makes it in its Looks section (made too, "
            "the first time); in files it in a folder. Then set its layers as on a cue: Towers colour Blue. A "
            "look never tracks; a VC button fires it.",
            { "look \"Hit 1\"", "look \"Blackout Hit\" in \"Hits\"" }, { "vcpage", "adopt" });
        add("vcpage", "busking", "vcpage",
            "Make VC page: a frame on the Virtual Console with one button per look of the selected look's "
            "folder (else every look), captioned with its name, coloured by its first colour.",
            { "vcpage" }, { "look" });
        add("adopt", "busking", "adopt <scene>",
            "Adopt as look: a scene (say a Programming one) becomes a look of the selected production -- its "
            "groups and palettes a layer, or else the scene played as it is. The scene stays.",
            { "adopt Haze" }, { "look" });
        // undo
        add("undo", "undo", "undo | redo", "Undoes (redoes) the last change: to the tree, or to a palette "
            "(made, renamed, filed, deleted) -- whichever came last.",
            { "undo", "redo" }, {}, { "redo" });
        add("help", "help", "help [<word>] | ?", "This: every word, or one word's syntax and examples.",
            { "help", "help spread", "?" }, {}, { "?" });
        return t;
    }();
    return table;
}

const ShowTreeCommand::Word *ShowTreeCommand::word(const QString &w)
{
    const QString lw = w.trimmed().toLower();
    for (const Word &e : words())
        if (e.word == lw || e.aliases.contains(lw))
            return &e;
    return nullptr;
}

static int editDistance(const QString &a, const QString &b)
{
    QVector<int> prev(b.length() + 1), cur(b.length() + 1);
    for (int j = 0; j <= b.length(); ++j)
        prev[j] = j;
    for (int i = 1; i <= a.length(); ++i)
    {
        cur[0] = i;
        for (int j = 1; j <= b.length(); ++j)
            cur[j] = qMin(qMin(prev[j] + 1, cur[j - 1] + 1), prev[j - 1] + (a.at(i - 1) == b.at(j - 1) ? 0 : 1));
        prev = cur;
    }
    return prev[b.length()];
}

QString ShowTreeCommand::nearestWord(const QString &typed, bool attributesOnly)
{
    const QString t = typed.trimmed().toLower();
    if (t.length() < 2)
        return QString();
    QString best;
    int bestD = 3;
    for (const Word &e : words())
    {
        if (attributesOnly && !e.attribute)
            continue;
        for (const QString &cand : QStringList(e.word) + e.aliases)
        {
            if (cand.length() < 2)
                continue;
            int d = editDistance(t, cand);
            if (cand.startsWith(t) && t.length() >= 3)
                d = qMin(d, 1);
            if (d < bestD)
            {
                bestD = d;
                best = e.word;
            }
        }
    }
    return best;
}

QString ShowTreeCommand::helpText(const QString &w)
{
    auto esc = [](const QString &x) { return x.toHtmlEscaped(); };
    auto example = [&esc](const QString &x) {
        return QStringLiteral("<a href=\"ex:%1\"><code>%2</code></a>").arg(QString::fromUtf8(QUrl::toPercentEncoding(x)),
                                                                           esc(x));
    };
    if (!w.trimmed().isEmpty())
    {
        const Word *e = word(w);
        if (e == nullptr)
        {
            const QString near = nearestWord(w);
            return near.isEmpty() ? tr("No word “%1”. Type <code>help</code> for them all.").arg(esc(w.trimmed()))
                                  : tr("No word “%1”. Did you mean <b>%2</b>? %3")
                                    .arg(esc(w.trimmed()), near, example(QStringLiteral("help ") + near));
        }
        QString out = QStringLiteral("<b>%1</b>").arg(esc(e->word));
        if (!e->aliases.isEmpty())
            out += tr(" (also %1)").arg(esc(e->aliases.join(QStringLiteral(", "))));
        out += QStringLiteral("<br><code>%1</code><br>%2").arg(esc(e->syntax), esc(e->what));
        QStringList ex;
        for (const QString &x : e->examples)
            ex << example(x);
        out += tr("<br>Examples: %1").arg(ex.join(QStringLiteral(" · ")));
        if (!e->related.isEmpty())
        {
            QStringList rel;
            for (const QString &r : e->related)
                rel << example(QStringLiteral("help ") + r).replace(QStringLiteral("<code>help "), QStringLiteral("<code>"));
            out += tr("<br>See also: %1").arg(rel.join(QStringLiteral(", ")));
        }
        return out;
    }
    const QList<QPair<QString, QString>> groups = {
        { "navigate", tr("Navigate") }, { "build", tr("Build") }, { "looks", tr("Looks") },
        { "tracking", tr("Tracking") }, { "record", tr("Record and copy") },
        { "reuse", tr("Reuse, detach, convert") }, { "palettes", tr("Palettes") },
        { "busking", tr("Looks (busking)") }, { "undo", tr("Undo") },
        { "help", tr("Help") } };
    QString out = tr("Every word (<code>help &lt;word&gt;</code> for one, with examples; Tab completes):");
    for (const auto &g : groups)
    {
        out += QStringLiteral("<br><b>%1</b>").arg(g.second);
        for (const Word &e : words())
            if (e.group == g.first)
                out += QStringLiteral("<br>&nbsp;&nbsp;<a href=\"ex:help%20%1\"><code>%1</code></a> — %2")
                       .arg(esc(e.word), esc(e.what.section(QLatin1Char('.'), 0, 0)));
    }
    return out;
}

QStringList ShowTreeCommand::keywords()
{
    // The words a command starts with: the table's (less the attributes
    // and "in", which come after a group), "look" after cue, "block" and
    // the other modes.
    QStringList out;
    for (const Word &w : words())
    {
        if (w.attribute || w.word == QLatin1String("in") || w.word == QLatin1String("none"))
            continue;
        out << w.word;
        for (const QString &a : w.aliases)
            if (a != QLatin1String("cue-only") && a != QLatin1String("new") && a != QLatin1String("fn")
                && a != QLatin1String("pal"))
                out << a;
    }
    return out;
}

QStringList ShowTreeCommand::attributeWords()
{
    // What Tab offers; attributeKey() also takes the aliases (color, dim,
    // level, pos, focus, frost, iris, strobe, fx).
    QStringList out;
    for (const Word &w : words())
        if (w.attribute)
            out << w.word;
    return out;
}

QString ShowTreeCommand::attributeKey(const QString &word)
{
    const QString w = word.toLower();
    if (w == QLatin1String("colour") || w == QLatin1String("color"))
        return KeyColour;
    if (w == QLatin1String("dimmer") || w == QLatin1String("dim") || w == QLatin1String("level"))
        return KeyDimmer;
    if (w == QLatin1String("position") || w == QLatin1String("pos") || w == QLatin1String("aim"))
        return KeyPosition;
    if (w == QLatin1String("spread"))
        return KeySpread;
    if (w == QLatin1String("group") || w == QLatin1String("grouping"))
        return KeyGrouping;
    if (w == QLatin1String("cross"))
        return KeyCross;
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

QStringList ShowTreeCommand::setNames() const
{
    QStringList out;
    for (const GroupSet &gs : m_st->groupSets())
        if (!gs.name.isEmpty() && !out.contains(gs.name, Qt::CaseInsensitive))
            out << gs.name;
    return out;
}

QStringList ShowTreeCommand::stageTargetNames() const
{
    QStringList out;
    const MonitorProperties *mp = m_doc->monitorProperties();
    if (mp == nullptr)
        return out;
    for (const StageTarget *t : mp->stageTargets())
        if (!t->name().isEmpty() && !out.contains(t->name(), Qt::CaseInsensitive))
            out << t->name();
    return out;
}

QStringList ShowTreeCommand::targetNames() const
{
    QStringList out = groupNames();
    for (const QString &n : setNames())
        if (!out.contains(n, Qt::CaseInsensitive))
            out << n;
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

QStringList ShowTreeCommand::reusableNames() const
{
    QStringList out;
    for (const ReusableLayer &r : m_st->reusableLayers())
        if (!r.name.isEmpty() && !out.contains(r.name, Qt::CaseInsensitive))
            out << r.name;
    for (const ReusableCue &r : m_st->reusableCues())
        if (!r.name.isEmpty() && !out.contains(r.name, Qt::CaseInsensitive))
            out << r.name;
    return out;
}

QStringList ShowTreeCommand::allPaletteNames() const
{
    QStringList out;
    const PaletteOps *ops = m_doc->paletteOps();
    for (const QLCPalette *p : m_doc->palettes())
        if (!p->name().isEmpty() && ops->isManaged(p->id()) && !out.contains(p->name(), Qt::CaseInsensitive))
            out << p->name();
    return out;
}

QStringList ShowTreeCommand::paletteFolders() const
{
    return m_doc->paletteOps()->folders();
}

QStringList ShowTreeCommand::lookNames(const Context &context) const
{
    QStringList out;
    const CueList *l = m_st->cueList(m_st->looksSection(context.production));
    if (l != nullptr)
        for (const Cue &c : l->cues)
            if (!c.name.isEmpty() && !out.contains(c.name, Qt::CaseInsensitive))
                out << c.name;
    return out;
}

QStringList ShowTreeCommand::lookFolderNames(const Context &context) const
{
    return m_st->lookFolders(m_st->looksSection(context.production));
}

int ShowTreeCommand::paletteTypeWord(const QString &word)
{
    const QString w = word.toLower();
    if (w == QLatin1String("colour") || w == QLatin1String("color"))
        return QLCPalette::Color;
    if (w == QLatin1String("dimmer") || w == QLatin1String("intensity"))
        return QLCPalette::Dimmer;
    if (w == QLatin1String("position") || w == QLatin1String("pantilt"))
        return QLCPalette::PanTilt;
    if (w == QLatin1String("aim"))
        return QLCPalette::Aim;
    if (w == QLatin1String("beam"))
        return QLCPalette::Beam;
    if (w == QLatin1String("gobo"))
        return QLCPalette::Gobo;
    if (w == QLatin1String("zoom"))
        return QLCPalette::Zoom;
    if (w == QLatin1String("shutter"))
        return QLCPalette::Shutter;
    if (w == QLatin1String("strobe"))
        return QLCPalette::Strobe;
    if (w == QLatin1String("effect"))
        return QLCPalette::Effect;
    return QLCPalette::Undefined;
}

QStringList ShowTreeCommand::paletteTypeWords()
{
    return { "colour", "dimmer", "position", "aim", "beam", "gobo", "zoom", "shutter", "strobe", "effect" };
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
    if (w0 == QLatin1String("help") || w0 == QLatin1String("?"))
    {
        // The language, from the table the parser's words come from.
        r.word();
        const QString what = r.src.mid(r.i).trimmed();
        res.ok = true;
        res.help = helpText(what);
        res.message = what.isEmpty() ? tr("help: every word (Esc closes)") : tr("help %1").arg(what);
        return res;
    }
    if (w0 == QLatin1String("undo") || w0 == QLatin1String("redo"))
    {
        // One history: the tree's steps and the palettes' (P-D14).
        const bool undo = w0 == QLatin1String("undo");
        const QString what = undo ? m_st->sharedUndoText() : m_st->sharedRedoText();
        res.ok = undo ? m_st->undoShared() : m_st->redoShared();
        res.message = res.ok ? (undo ? tr("Undid: %1") : tr("Redid: %1")).arg(what)
                             : (undo ? tr("Nothing to undo") : tr("Nothing to redo"));
        return res;
    }

    if (w0 == QLatin1String("palette") || w0 == QLatin1String("pal"))
        return runPalette(r.src, context);
    if (w0 == QLatin1String("look") || w0 == QLatin1String("vcpage") || w0 == QLatin1String("adopt"))
        return runLook(r.src, context);

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
        QStringList names = targetNames();
        // A named layer of the selected cue, by its name: its groups.
        QHash<QString, Target> layerNames;
        if (const Cue *c = m_st->cue(ctx.cue))
            for (const Layer &l : c->layers)
                if (!l.name.isEmpty() && !l.target.isOpaque() && !names.contains(l.name, Qt::CaseInsensitive))
                {
                    names << l.name;
                    layerNames.insert(l.name.toLower(), l.target);
                }
        Target t;
        auto addName = [&](const QString &n) {
            if (layerNames.contains(n.toLower()))
            {
                const Target lt = layerNames.value(n.toLower());
                for (quint32 g : lt.groups)
                    if (!t.groups.contains(g))
                        t.groups << g;
                for (quint32 f : lt.fixtures)
                    if (!t.fixtures.contains(f))
                        t.fixtures << f;
                for (quint32 set : lt.sets)
                    if (!t.sets.contains(set))
                        t.sets << set;
                return;
            }
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
            for (const GroupSet &gs : m_st->groupSets())
                if (gs.name.compare(n, Qt::CaseInsensitive) == 0)
                {
                    if (!t.sets.contains(gs.id))
                        t.sets << gs.id;
                    return;
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
            {
                const QString near = nearestWord(kw, true);
                return tr("Expected an attribute (colour, dimmer, position, aim, spread, group, cross, gobo, "
                          "zoom, beam, shutter, effect), got “%1”.%2").arg(kw)
                       .arg(near.isEmpty() ? QString() : tr(" Did you mean “%1”? Type help %1.").arg(near));
            }
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
                    const QList<ResolvedCue> rc = now == nullptr ? QList<ResolvedCue>() : m_st->resolved(*now);
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

            if (isLineParamKey(key))
            {
                // A line aim's Spread / Grouping / Cross (§3B, D16): a value,
                // set on its own; a cue's reaches only line aims (D18).
                const QString w = r.word();
                const Look l = ShowTreeController::lineParamLook(key, w);
                if (l.value.isEmpty())
                    return key == KeySpread ? tr("spread -5 … +5 (0: all on the line's centre), got “%1”.").arg(w)
                           : key == KeyGrouping ? tr("group each | pairs | threes | fours | halves, got “%1”.").arg(w)
                           : tr("cross swap | mirror, got “%1”.").arg(w);
                setLook(l);
                effect.clear();
                ++n;
                continue;
            }
            if (kw.compare(QLatin1String("aim"), Qt::CaseInsensitive) == 0)
            {
                // Aim at a target (a line, a point, an area) by its name, or
                // an Aim palette by name: the palette pointing there is
                // reused, else one is made.
                quint32 pid = QLCPalette::invalidId();
                const QString pal = r.name(paletteNames(KeyPosition));
                if (!pal.isEmpty())
                {
                    for (const QLCPalette *have : m_doc->palettes())
                        if (have->name().compare(pal, Qt::CaseInsensitive) == 0
                            && m_st->keyForPalette(have->id()) == KeyPosition && !m_st->isInlinePalette(have->id()))
                            pid = have->id();
                }
                else
                {
                    const QString tn = r.name(stageTargetNames());
                    if (tn.isEmpty())
                        return tr("aim <target>: no target or Aim palette “%1”. Tab lists them.").arg(r.peek());
                    if (const MonitorProperties *mp = m_doc->monitorProperties())
                        for (const StageTarget *t : mp->stageTargets())
                            if (t->name().compare(tn, Qt::CaseInsensitive) == 0)
                            {
                                pid = m_st->aimPaletteFor(t->id());
                                break;
                            }
                }
                if (pid == QLCPalette::invalidId())
                    return tr("Couldn't aim there.");
                setLook(m_st->lookForPalette(pid));
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

    if (w0 == QLatin1String("set"))
    {
        // Make set (§1): set "Towers" SL SR -- or, a set of that name
        // already there, its members again. Usable by name from then on.
        r.word();
        QString nm = r.quoted();
        if (nm.isNull())
            nm = r.word();
        const QList<Target> ts = targets();
        Target all;
        for (const Target &t : ts)
        {
            Target ex = m_st->expandTarget(t);
            for (quint32 g : ex.groups)
                if (!all.groups.contains(g))
                    all.groups << g;
            for (quint32 f : ex.fixtures)
                if (!all.fixtures.contains(f))
                    all.fixtures << f;
        }
        quint32 have = invalidId();
        for (const GroupSet &gs : m_st->groupSets())
            if (gs.name.compare(nm, Qt::CaseInsensitive) == 0)
                have = gs.id;
        if (nm.trimmed().isEmpty() || (all.groups.isEmpty() && all.fixtures.isEmpty()))
            error = tr("set \"name\" <groups> -- e.g. set \"Towers\" SL SR");
        else if (!r.atEnd())
            error = tr("Don't know “%1”.").arg(r.peek());
        else if (have != invalidId())
        {
            if (!m_st->setGroupSetMembers(have, all.groups, all.fixtures))
                error = tr("%1 already has those members.").arg(nm);
        }
        else if (m_st->addGroupSet(nm, all.groups, all.fixtures) == invalidId())
            error = tr("Couldn't make the set.");
    }
    else if (w0 == QLatin1String("name"))
    {
        // name "Hero": the selected layer (or else the cue); "" goes back
        // to the automatic name (the layer's groups). Identity is kept.
        r.word();
        QString nm = r.quoted();
        if (nm.isNull())
            nm = r.src.mid(r.i).trimmed();
        else if (!r.atEnd())
            error = tr("name \"…\" -- then nothing more");
        const quint32 node = ctx.layer != invalidId() ? ctx.layer : ctx.cue;
        if (error.isEmpty() && node == invalidId())
            error = tr("Select a layer or a cue to name.");
        else if (error.isEmpty() && !m_st->renameNode(node, nm.trimmed()))
            error = tr("It is called that already.");
        if (error.isEmpty() && ctx.layer != invalidId())
            res.layer = ctx.layer;
    }
    else if (w0 == QLatin1String("list"))
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
    else if (w0 == QLatin1String("convert"))
    {
        // Convert to tracking (§7): the plan first, `convert apply` does it.
        r.word();
        const QString what = r.word().toLower();
        if (!what.isEmpty() && what != QLatin1String("apply"))
            error = tr("convert · convert apply");
        else
        {
            ShowTreeController::ConvertPlan plan = m_st->planConvertToTracking(ctx.list);
            if (!plan.ok)
                error = plan.why;
            else if (what.isEmpty())
            {
                QString html = tr("<b>Convert %1 to tracking</b> — every cue plays as it does now:").arg(list->name.toHtmlEscaped());
                for (const QString &c : plan.changes)
                    html += QStringLiteral("<br>&nbsp;&nbsp;") + c.toHtmlEscaped();
                html += tr("<br><a href=\"ex:convert apply\"><code>convert apply</code></a> does it (one undo step).");
                res.help = html;
                res.message = tr("%1 cue(s) track, %2 value(s) and %3 layer(s) only repeated what tracks in; "
                                 "convert apply does it").arg(plan.tracking).arg(plan.looksRemoved).arg(plan.layersRemoved);
                res.keepSelection = true;
            }
            else if (!m_st->convertToTracking(ctx.list, &plan))
                error = plan.why.isEmpty() ? tr("Couldn't convert.") : plan.why;
            else
                res.message = tr("converted: %1 cue(s) track, %2 value(s) and %3 layer(s) taken out, %4 include(s) "
                                 "made tracking").arg(plan.tracking).arg(plan.looksRemoved).arg(plan.layersRemoved)
                              .arg(plan.includesRemoved);
        }
    }
    else if (w0 == QLatin1String("detach") && r.src.mid(r.i).trimmed().toLower() == QLatin1String("detach list"))
    {
        r.word();
        r.word();
        const QString nm = list->name;
        if (m_st->detachList(ctx.list) == invalidId())
            error = tr("Couldn't detach %1.").arg(nm);
        else
        {
            res.message = tr("%1 is a plain chaser now (Functions → %2)").arg(nm, nm);
            ctx.list = invalidId();
            ctx.cue = invalidId();
        }
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
    else if (w0 == QLatin1String("reusable"))
    {
        // Make reusable: the selected layer, else the cue.
        r.word();
        QString nm = r.quoted();
        if (nm.isNull())
            nm = r.src.mid(r.i).trimmed();
        QString why;
        const quint32 id = ctx.layer != invalidId() ? m_st->makeLayerReusable(ctx.cue, ctx.layer, nm, &why)
                                                    : m_st->makeCueReusable(ctx.cue, nm, &why);
        if (id == invalidId())
            error = why;
        else
        {
            res.message = tr("%1 is reusable (Library → All → Reusable)").arg(m_st->reusableName(id));
            res.layer = ctx.layer;
        }
    }
    else if (w0 == QLatin1String("use"))
    {
        r.word();
        const QString nm = r.name(reusableNames());
        quint32 rid = invalidId();
        for (const ReusableLayer &x : m_st->reusableLayers())
            if (rid == invalidId() && x.name.compare(nm, Qt::CaseInsensitive) == 0)
                rid = x.id;
        bool isCue = false;
        for (const ReusableCue &x : m_st->reusableCues())
            if (rid == invalidId() && x.name.compare(nm, Qt::CaseInsensitive) == 0)
            {
                rid = x.id;
                isCue = true;
            }
        if (nm.isEmpty() || rid == invalidId())
            error = tr("use <reusable>: no reusable “%1”. Tab lists them.").arg(r.peek());
        else if (!r.atEnd())
            error = tr("Don't know “%1”.").arg(r.peek());
        else if (isCue)
        {
            const quint32 id = m_st->insertReusableCue(ctx.list, list->indexOf(ctx.cue), rid);
            if (id == invalidId())
                error = tr("Couldn't put %1 in.").arg(nm);
            else
                ctx.cue = id;
        }
        else
        {
            const quint32 id = m_st->insertReusableLayer(ctx.cue, rid);
            if (id == invalidId())
                error = tr("Couldn't put %1 in.").arg(nm);
            else
                res.layer = id;
        }
    }
    else if (w0 == QLatin1String("unlink"))
    {
        r.word();
        const Cue *c = list->cue(ctx.cue);
        const Layer *l = ctx.layer == invalidId() ? nullptr : c->layer(ctx.layer);
        if (!r.atEnd())
            error = tr("unlink -- then nothing more");
        else if (l != nullptr && l->reusable != invalidId())
        {
            m_st->unlinkLayer(ctx.cue, ctx.layer);
            res.layer = ctx.layer;
        }
        else if (l == nullptr && c->reusable != invalidId())
            m_st->unlinkCue(ctx.cue);
        else
            error = tr("The selected %1 isn't a use of a reusable.").arg(l != nullptr ? tr("layer") : tr("cue"));
    }
    else if (w0 == QLatin1String("detach"))
    {
        r.word();
        if (!r.atEnd())
            error = tr("detach · detach list");
        else if (ctx.layer == invalidId())
            error = tr("Select a layer to detach (or: detach list).");
        else if (m_st->detachLayer(ctx.cue, ctx.layer) == invalidId())
            error = tr("Couldn't detach that layer.");
        else
            res.layer = ctx.layer;
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
    else if (w0 == QLatin1String("copy"))
    {
        // copy | copy cue <n>… | copy layer <groups>: for paste (and ⌘V).
        r.word();
        const QString what = r.peek().toLower();
        Clipboard clip;
        if (what == QLatin1String("cue"))
        {
            r.word();
            while (!r.atEnd() && error.isEmpty())
            {
                const QString num = r.word();
                const int idx = indexOfNumber(*list, num);
                if (idx < 0)
                    error = tr("copy cue <number>…: no cue %1").arg(num);
                else
                    clip.cues << list->cues.at(idx).id;
            }
            if (clip.cues.isEmpty() && error.isEmpty())
                clip.cues << ctx.cue;
        }
        else if (what == QLatin1String("layer"))
        {
            r.word();
            for (const Target &t : targets())
                if (const Cue *c = m_st->cue(ctx.cue))
                    for (const Layer &l : c->layers)
                        if (l.target == t)
                            clip.layers << qMakePair(ctx.cue, l.id);
            if (clip.layers.isEmpty())
                error = tr("copy layer <groups>: the selected cue has no layer on “%1”.").arg(r.peek());
        }
        else if (what.isEmpty())
        {
            if (ctx.layer != invalidId())
                clip.layers << qMakePair(ctx.cue, ctx.layer);
            else
                clip.cues << ctx.cue;
        }
        else
            error = tr("copy · copy cue <number>… · copy layer <groups>");
        if (error.isEmpty() && !r.atEnd())
            error = tr("Don't know “%1”.").arg(r.peek());
        if (error.isEmpty())
        {
            clipboard = clip;
            res.message = clip.cues.isEmpty() ? tr("copied %1 layer(s); paste puts them in the selected cue")
                                                .arg(clip.layers.count())
                                              : tr("copied %1 cue(s); paste puts them after the selected cue")
                                                .arg(clip.cues.count());
        }
    }
    else if (w0 == QLatin1String("paste"))
    {
        r.word();
        if (!r.atEnd())
            error = tr("paste -- then nothing more");
        else if (clipboard.isEmpty())
            error = tr("Nothing copied yet (copy, or ⌘C in the tree).");
        else if (!clipboard.cues.isEmpty())
        {
            const QList<quint32> made = m_st->pasteCues(clipboard.cues, ctx.cue);
            if (made.isEmpty())
                error = tr("Couldn't paste here.");
            else
                ctx.cue = made.last();
        }
        else
        {
            const QList<quint32> made = m_st->pasteLayers(clipboard.layers, ctx.cue);
            if (made.isEmpty())
                error = tr("Couldn't paste here.");
            else
                res.layer = made.last();
        }
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
    else if (w0 == QLatin1String("release") && !ctx.layers.isEmpty()
             && QStringList({ "on", "off", "" }).contains(r.src.mid(r.i + 7).trimmed().toLower()))
    {
        // Several layers selected: release on (or off) on all of them.
        r.word();
        const bool on = r.word().toLower() != QLatin1String("off");
        for (const QPair<quint32, quint32> &ref : ctx.layers)
            m_st->setLayerRelease(ref.first, ref.second, on);
        res.keepSelection = true;
    }
    else if (!attributeKey(w0).isEmpty() && !ctx.layers.isEmpty() && targetNames().indexOf(QRegularExpression(
                 QStringLiteral("^%1$").arg(QRegularExpression::escape(w0)),
                 QRegularExpression::CaseInsensitiveOption)) < 0)
    {
        // `colour Blue`, `dimmer 45`… with several layers selected: on all
        // of them, cue by cue, one undo step.
        QList<quint32> cues;
        for (const QPair<quint32, quint32> &ref : ctx.layers)
            if (!cues.contains(ref.first))
                cues << ref.first;
        const int start = r.i;
        const quint32 keepCue = ctx.cue;
        for (quint32 cid : cues)
        {
            QList<quint32> ls;
            for (const QPair<quint32, quint32> &ref : ctx.layers)
                if (ref.first == cid)
                    ls << ref.second;
            r.i = start;
            ctx.cue = cid;
            error = assigns(ls);
            if (!error.isEmpty())
                break;
        }
        ctx.cue = keepCue;
        res.keepSelection = true;
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
            const QString near = nearestWord(r.peek());
            error = tr("Don't know “%1”.%2 Start with cue, a group or fixture, a mode, base, release, include, "
                       "function, record, list, set, name, copy, paste, dup, reusable, use, unlink, detach, "
                       "convert, undo — type help.")
                    .arg(r.peek(), near.isEmpty() ? QString() : tr(" Did you mean “%1”? (help %1)").arg(near));
        }
        else
        {
            QList<quint32> layers;
            if (r.peek().toLower() == QLatin1String("in"))
            {
                // US in Steps …: child layers of the cue's Steps layer (§3.5).
                r.word();
                const QList<Target> up = targets();
                if (up.count() != 1)
                    error = tr("… in <one layer's groups>: the parent layer");
                quint32 parent = invalidId();
                if (error.isEmpty())
                {
                    if (const Cue *c = m_st->cue(ctx.cue))
                        for (const Layer &l : c->layers)
                            if (l.target == up.first())
                                parent = l.id;
                    if (parent == invalidId())
                        parent = m_st->addLayer(ctx.cue, up.first());
                }
                for (const Target &t : ts)
                {
                    if (!error.isEmpty())
                        break;
                    quint32 lid = invalidId();
                    if (const Cue *c = m_st->cue(ctx.cue))
                        for (const Layer &l : c->layers)
                            if (l.parent == parent && l.target == t)
                                lid = l.id;
                    QString why;
                    if (lid == invalidId())
                        lid = m_st->addChildLayer(ctx.cue, parent, t, &why);
                    if (lid == invalidId())
                        error = why;
                    else
                        layers << lid;
                }
            }
            else
                for (const Target &t : ts)
                {
                    const quint32 lid = layerOn(t);
                    if (lid != invalidId())
                        layers << lid;
                }
            if (!error.isEmpty())
            {
                m_st->endBatch(false);
                res.message = error;
                return res;
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
    res.message = res.message.isEmpty() ? QStringLiteral("✓ %1").arg(r.src)
                                        : QStringLiteral("✓ %1").arg(res.message);
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
 * Palettes (R2) and looks (R3)
 ****************************************************************************/

ShowTreeCommand::Result ShowTreeCommand::runPalette(const QString &line, const Context &context)
{
    // Each one PaletteOps step: one undo step, on the history the tab's
    // Undo shares with the tree.
    Result res;
    res.context = context;
    Reader r;
    r.src = line.trimmed();
    r.word();
    PaletteOps *ops = m_doc->paletteOps();

    // A folder: quoted, one the show has (spaces and all), or a word.
    auto folderArg = [&]() -> QString {
        const QString q = r.quoted();
        if (!q.isNull())
            return q;
        const QString known = r.name(paletteFolders());
        return known.isEmpty() ? r.word() : known;
    };
    auto fail = [&res](const QString &why) {
        res.message = why;
        return res;
    };

    const QString w1 = r.peek().toLower();
    if (w1.isEmpty())
        return fail(tr("palette <type> \"name\" · palette <name> rename|move|dup|delete|edit · "
                       "palette folder \"name\""));

    if (w1 == QLatin1String("folder"))
    {
        r.word();
        const QString folder = PaletteOps::cleanFolder(folderArg());
        if (folder.isEmpty())
            return fail(tr("palette folder \"name\" [rename \"new\" | delete]"));
        const QString op = r.word().toLower();
        if (op.isEmpty())
        {
            if (!ops->addFolder(folder))
                return fail(tr("There is a folder “%1” already.").arg(folder));
            res.message = tr("new folder “%1”").arg(folder);
        }
        else if (op == QLatin1String("rename") || op == QLatin1String("name"))
        {
            QString to = r.quoted();
            if (to.isNull())
                to = r.src.mid(r.i).trimmed();
            if (!ops->renameFolder(folder, to))
                return fail(tr("Couldn't rename “%1” to “%2”.").arg(folder, to));
            res.message = tr("folder “%1” is “%2”").arg(folder, to);
        }
        else if (op == QLatin1String("delete"))
        {
            const int n = ops->folderCount(folder);
            if (!ops->removeFolder(folder))
                return fail(tr("No folder “%1”.").arg(folder));
            res.message = tr("folder “%1” deleted; %2 palette(s) moved up").arg(folder).arg(n);
        }
        else
            return fail(tr("palette folder \"name\" [rename \"new\" | delete]"));
        if (!r.atEnd())
            return fail(tr("Don't know “%1”.").arg(r.peek()));
        res.ok = true;
        res.message = QStringLiteral("✓ %1").arg(res.message);
        res.keepSelection = true;
        return res;
    }

    const QLCPalette::PaletteType type = QLCPalette::PaletteType(paletteTypeWord(w1));
    if (type != QLCPalette::Undefined)
    {
        // A new palette: palette <type> ["name"] [value] [in <folder>].
        r.word();
        const QString name = r.quoted();
        QVariantList values;
        static const QRegularExpression level(QStringLiteral("^(\\d{1,3})%?$"));
        static const QRegularExpression hex(QStringLiteral("^#[0-9a-fA-F]{6}$"));
        const QString v = r.peek();
        if (type == QLCPalette::Dimmer && level.match(v).hasMatch() && level.match(v).captured(1).toInt() <= 100)
        {
            r.word();
            values << qRound(level.match(v).captured(1).toInt() * 255.0 / 100.0);
        }
        else if (type == QLCPalette::Color && hex.match(v).hasMatch())
        {
            r.word();
            values << QLCPalette::colorToString(QColor(v), QColor(Qt::black));
        }
        QString path = m_st->suggestPaletteFolder(type);
        if (r.peek().toLower() == QLatin1String("in"))
        {
            r.word();
            const QString folder = PaletteOps::cleanFolder(folderArg());
            if (folder.isEmpty())
                return fail(tr("… in \"folder\""));
            path = PaletteOps::pathForFolder(folder);
        }
        if (!r.atEnd())
            return fail(tr("palette %1 \"name\" %2 [in \"folder\"] -- don't know “%3”")
                        .arg(w1, type == QLCPalette::Dimmer ? QStringLiteral("[0-100]")
                                 : type == QLCPalette::Color ? QStringLiteral("[#rrggbb]") : QString(), r.peek()));
        QString nm = name.isNull() ? QString() : name.trimmed();
        if (nm.isEmpty())
            nm = values.isEmpty() ? ops->newName(type) : m_st->suggestPaletteName(type, values);
        for (const QLCPalette *have : m_doc->palettes())
            if (have->type() == type && have->name().compare(nm, Qt::CaseInsensitive) == 0)
                return fail(tr("There is a %1 palette “%2” already.").arg(w1, have->name()));
        const quint32 id = ops->create(type, path, nm, values);
        if (id == QLCPalette::invalidId())
            return fail(tr("Couldn't make the palette."));
        const QString folder = PaletteOps::folderOf(m_doc->palette(id));
        res.ok = true;
        res.editPalette = id;
        res.keepSelection = true;
        res.message = folder.isEmpty() ? tr("✓ new %1 palette “%2”").arg(w1, nm)
                                       : tr("✓ new %1 palette “%2” in %3").arg(w1, nm, folder);
        return res;
    }

    // An existing palette by name, then what to do with it.
    const QString name = r.name(allPaletteNames());
    if (name.isEmpty())
        return fail(tr("No palette “%1”; palette <type> \"name\" makes one. Tab lists them.").arg(r.peek()));
    quint32 pid = QLCPalette::invalidId();
    QList<QLCPalette *> pl = m_doc->palettes();
    std::sort(pl.begin(), pl.end(), [](const QLCPalette *a, const QLCPalette *b) { return a->id() < b->id(); });
    for (const QLCPalette *have : pl)
        if (pid == QLCPalette::invalidId() && ops->isManaged(have->id())
            && have->name().compare(name, Qt::CaseInsensitive) == 0)
            pid = have->id();
    const QString op = r.word().toLower();
    res.keepSelection = true;
    if (op.isEmpty() || op == QLatin1String("edit"))
    {
        if (!r.atEnd())
            return fail(tr("Don't know “%1”.").arg(r.peek()));
        res.ok = true;
        res.editPalette = pid;
        res.message = tr("✓ editing “%1”").arg(name);
        return res;
    }
    if (op == QLatin1String("rename") || op == QLatin1String("name"))
    {
        QString to = r.quoted();
        if (to.isNull())
            to = r.src.mid(r.i).trimmed();
        else if (!r.atEnd())
            return fail(tr("palette <name> rename \"new\" -- then nothing more"));
        if (!ops->rename(pid, to))
            return fail(to.trimmed().isEmpty() ? tr("rename \"new name\"") : tr("It is called that already."));
        res.message = tr("“%1” is “%2”").arg(name, to.trimmed());
    }
    else if (op == QLatin1String("move") || op == QLatin1String("in"))
    {
        QString folder = folderArg();
        if (folder.toLower() == QLatin1String("none"))
            folder.clear();
        if (!r.atEnd())
            return fail(tr("palette <name> move \"folder\" | none -- then nothing more"));
        if (!ops->move({ pid }, folder))
            return fail(tr("“%1” is there already.").arg(name));
        res.message = folder.isEmpty() ? tr("“%1” in no folder").arg(name)
                                       : tr("“%1” in %2").arg(name, PaletteOps::cleanFolder(folder));
    }
    else if (op == QLatin1String("dup") || op == QLatin1String("duplicate"))
    {
        const quint32 id = ops->duplicate(pid);
        if (id == QLCPalette::invalidId())
            return fail(tr("Couldn't duplicate “%1”.").arg(name));
        res.editPalette = id;
        res.message = tr("“%1”").arg(m_doc->palette(id)->name());
    }
    else if (op == QLatin1String("delete"))
    {
        const bool anyway = r.word().toLower() == QLatin1String("anyway");
        const QList<PaletteOps::Use> uses = ops->usage(pid);
        if (!uses.isEmpty() && !anyway)
            return fail(tr("“%1” is used by %2. To delete it all the same: palette %3 delete anyway")
                        .arg(name, PaletteOps::usageSummary(uses), typed(name, name.contains(QLatin1Char(' ')))));
        if (!ops->remove({ pid }))
            return fail(tr("Couldn't delete “%1”.").arg(name));
        res.message = tr("deleted “%1”").arg(name);
    }
    else
        return fail(tr("palette <name> rename \"new\" | move <folder> | dup | delete | edit"));
    res.ok = true;
    res.message = QStringLiteral("✓ %1").arg(res.message);
    return res;
}

ShowTreeCommand::Result ShowTreeCommand::runLook(const QString &line, const Context &context)
{
    Result res;
    res.context = context;
    Context &ctx = res.context;
    Reader r;
    r.src = line.trimmed();
    const QString w0 = r.word().toLower();
    auto fail = [&res, &context](const QString &why) {
        res.message = why;
        res.context = context;
        return res;
    };
    if (m_st->production(ctx.production) == nullptr)
        return fail(tr("Select a production first (+ Production)."));
    const quint32 section = m_st->looksSection(ctx.production);

    if (w0 == QLatin1String("look"))
    {
        QString name = r.quoted();
        if (name.isNull())
            name = r.name(lookNames(ctx));
        if (name.isEmpty())
            name = r.word();
        if (name.trimmed().isEmpty())
            return fail(tr("look \"name\" [in \"folder\"]"));
        QString folder;
        bool folderGiven = false;
        if (r.peek().toLower() == QLatin1String("in"))
        {
            r.word();
            folder = r.quoted();
            if (folder.isNull())
                folder = r.name(lookFolderNames(ctx));
            if (folder.isEmpty())
                folder = r.word();
            folderGiven = true;
        }
        if (!r.atEnd())
            return fail(tr("look \"name\" [in \"folder\"] -- don't know “%1”").arg(r.peek()));
        quint32 have = invalidId();
        if (const CueList *l = m_st->cueList(section))
            for (const Cue &c : l->cues)
                if (have == invalidId() && c.name.compare(name.trimmed(), Qt::CaseInsensitive) == 0)
                    have = c.id;
        if (have != invalidId())
        {
            if (folderGiven)
                m_st->setLookFolder({ have }, folder);
            ctx.list = section;
            ctx.cue = have;
            ctx.layer = invalidId();
            res.message = tr("look “%1”").arg(name.trimmed());
        }
        else
        {
            // After the selected look, else at the end.
            const quint32 after = m_st->isLook(ctx.cue) ? ctx.cue : invalidId();
            if (!folderGiven && after != invalidId())
                if (const Cue *c = m_st->cue(after))
                    folder = c->folder;
            const quint32 id = m_st->addLook(ctx.production, name.trimmed(), folder, after);
            if (id == invalidId())
                return fail(tr("Couldn't make the look."));
            ctx.list = m_st->looksSection(ctx.production);
            ctx.cue = id;
            ctx.layer = invalidId();
            res.message = tr("new look “%1”").arg(name.trimmed());
        }
    }
    else if (w0 == QLatin1String("vcpage"))
    {
        if (!r.atEnd())
            return fail(tr("vcpage -- then nothing more"));
        if (section == invalidId() || m_st->looksIn(section, QString()).isEmpty())
            return fail(tr("No looks yet: look \"name\" makes one."));
        QString folder;
        if (m_st->isLook(ctx.cue))
            if (const Cue *c = m_st->cue(ctx.cue))
                folder = c->folder;
        if (!m_vcPageMaker)
            return fail(tr("No Virtual Console here."));
        QString msg;
        if (!m_vcPageMaker(section, folder, &msg))
            return fail(msg.isEmpty() ? tr("Couldn't make the VC page.") : msg);
        res.message = msg;
        res.keepSelection = true;
    }
    else
    {
        // adopt <scene>
        const QString name = r.name(functionNames());
        if (name.isEmpty())
            return fail(tr("adopt <scene>: no function “%1”. Tab lists them.").arg(r.peek()));
        if (!r.atEnd())
            return fail(tr("adopt <scene> -- then nothing more"));
        quint32 fid = invalidId();
        for (const Function *f : m_doc->functions())
            if (fid == invalidId() && !m_st->isGenerated(f->id()) && f->name().compare(name, Qt::CaseInsensitive) == 0)
                fid = f->id();
        QString why;
        const quint32 id = m_st->adoptAsLook(ctx.production, fid, &why);
        if (id == invalidId())
            return fail(why);
        ctx.list = m_st->looksSection(ctx.production);
        ctx.cue = id;
        ctx.layer = invalidId();
        res.message = why.isEmpty() ? tr("“%1” is a look").arg(name)
                                    : tr("“%1” is a look, played as it is (%2)").arg(name, why);
    }
    res.ok = true;
    res.message = QStringLiteral("✓ %1").arg(res.message);
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
    QStringList targets = targetNames();
    // Named layers of the selected cue complete too.
    if (const Cue *c = m_st->cue(context.cue))
        for (const Layer &l : c->layers)
            if (!l.name.isEmpty() && !l.target.isOpaque() && !targets.contains(l.name, Qt::CaseInsensitive))
                targets << l.name;

    // attribute value, attribute value… to the end.
    auto assigns = [&]() -> bool {
        for (;;)
        {
            if (r.atEnd())
            {
                *out = attributeWords();
                return true;
            }
            const QString kw = r.word();
            const QString key = attributeKey(kw);
            if (key.isEmpty())
                return false;
            const bool aim = kw.compare(QLatin1String("aim"), Qt::CaseInsensitive) == 0;
            if (r.atEnd())
            {
                if (key == KeySpread)
                    *out = QStringList({ "-5", "-2.5", "0", "+2.5", "+5" }) + none;
                else if (key == KeyGrouping)
                    *out = QStringList({ "each", "pairs", "threes", "fours", "halves" }) + none;
                else if (key == KeyCross)
                    *out = QStringList({ "swap", "mirror" }) + none;
                else if (aim)
                    *out = stageTargetNames() + paletteNames(key) + none;
                else
                    *out = paletteNames(key) + none;
                return true;
            }
            const QString next = r.peek().toLower();
            if (none.contains(next))
            {
                r.word();
                continue;
            }
            if (isLineParamKey(key))
            {
                if (ShowTreeController::lineParamValue(key, r.word()).isEmpty())
                    return false;
                continue;
            }
            if (aim && !r.name(stageTargetNames()).isEmpty())
                continue;
            if (!r.name(paletteNames(key)).isEmpty())
                continue;
            static const QRegularExpression value(QStringLiteral("^(\\d{1,3}%?|#[0-9a-fA-F]{6})$"));
            if (key == KeyEffectPrefix || !value.match(next).hasMatch())
                return false;
            r.word();
        }
    };
    // Names one after another (or joined by '+', or ',' for another layer).
    std::function<bool(const QStringList &, bool)> names;
    names = [&](const QStringList &all, bool thenAssign) -> bool {
        bool joined = false;
        int n = 0;
        for (;;)
        {
            if (r.atEnd())
            {
                *out = all;
                if (thenAssign && !joined && n > 0)
                    *out += attributeWords() + QStringList({ "in" });
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
        if (n > 0 && thenAssign && r.peek().toLower() == QLatin1String("in"))
        {
            // … in <parent>: a child layer, then its looks.
            r.word();
            return names(all, true);
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
    if (w0 == QLatin1String("undo") || w0 == QLatin1String("redo") || w0 == QLatin1String("dup")
        || w0 == QLatin1String("name") || w0 == QLatin1String("paste") || w0 == QLatin1String("reusable")
        || w0 == QLatin1String("unlink"))
        return true;
    if (w0 == QLatin1String("use"))
    {
        r.word();
        if (r.atEnd())
        {
            *out = reusableNames();
            return true;
        }
        return !r.name(reusableNames()).isEmpty() && r.atEnd();
    }
    // A name: quoted (any), or one of @a known; false: none there.
    auto nameOr = [&r](const QStringList &known) {
        if (r.src.mid(r.i).trimmed().startsWith(QLatin1Char('"')))
            return !r.quoted().isNull();
        return !r.name(known).isEmpty() || !r.word().isEmpty();
    };
    if (w0 == QLatin1String("palette") || w0 == QLatin1String("pal"))
    {
        r.word();
        if (r.atEnd())
        {
            *out = paletteTypeWords() + allPaletteNames() + QStringList(QStringLiteral("folder"));
            return true;
        }
        const QString w1 = r.peek().toLower();
        if (w1 == QLatin1String("folder"))
        {
            r.word();
            if (r.atEnd())
            {
                *out = paletteFolders();
                return true;
            }
            if (!nameOr(paletteFolders()))
                return false;
            if (r.atEnd())
            {
                *out = QStringList({ "rename", "delete" });
                return true;
            }
            const QString op = r.word().toLower();
            if (op == QLatin1String("delete"))
                return r.atEnd();
            if (op != QLatin1String("rename"))
                return false;
            if (!r.atEnd())
                r.quoted();
            return r.atEnd();
        }
        if (paletteTypeWord(w1) != QLCPalette::Undefined)
        {
            // palette <type> ["name"] [value] [in <folder>]
            r.word();
            if (r.src.mid(r.i).trimmed().startsWith(QLatin1Char('"')))
                r.quoted();
            static const QRegularExpression value(QStringLiteral("^(\\d{1,3}%?|#[0-9a-fA-F]{6})$"));
            if (value.match(r.peek()).hasMatch())
                r.word();
            if (r.atEnd())
            {
                *out = QStringList({ "in" });
                return true;
            }
            if (r.word().toLower() != QLatin1String("in"))
                return false;
            if (r.atEnd())
            {
                *out = paletteFolders();
                return true;
            }
            return nameOr(paletteFolders()) && r.atEnd();
        }
        if (r.name(allPaletteNames()).isEmpty())
            return false;
        if (r.atEnd())
        {
            *out = QStringList({ "rename", "move", "dup", "delete", "edit" });
            return true;
        }
        const QString op = r.word().toLower();
        if (op == QLatin1String("move") || op == QLatin1String("in"))
        {
            if (r.atEnd())
            {
                *out = paletteFolders() + QStringList(QStringLiteral("none"));
                return true;
            }
            return nameOr(paletteFolders()) && r.atEnd();
        }
        if (op == QLatin1String("delete"))
        {
            if (r.atEnd())
            {
                *out = QStringList({ "anyway" });
                return true;
            }
            return r.word().toLower() == QLatin1String("anyway") && r.atEnd();
        }
        if (op == QLatin1String("rename") || op == QLatin1String("name"))
        {
            if (!r.atEnd())
                r.quoted();
            return r.atEnd();
        }
        return (op == QLatin1String("dup") || op == QLatin1String("edit")) && r.atEnd();
    }
    if (w0 == QLatin1String("look"))
    {
        r.word();
        if (r.atEnd())
        {
            *out = lookNames(context);
            return true;
        }
        if (!nameOr(lookNames(context)))
            return false;
        if (r.atEnd())
        {
            *out = QStringList({ "in" });
            return true;
        }
        if (r.word().toLower() != QLatin1String("in"))
            return false;
        if (r.atEnd())
        {
            *out = lookFolderNames(context);
            return true;
        }
        return nameOr(lookFolderNames(context)) && r.atEnd();
    }
    if (w0 == QLatin1String("vcpage"))
    {
        r.word();
        return r.atEnd();
    }
    if (w0 == QLatin1String("adopt"))
    {
        r.word();
        if (r.atEnd())
        {
            *out = functionNames();
            return true;
        }
        return !r.name(functionNames()).isEmpty() && r.atEnd();
    }
    if (w0 == QLatin1String("convert") || w0 == QLatin1String("detach"))
    {
        r.word();
        if (r.atEnd())
        {
            *out = QStringList(w0 == QLatin1String("convert") ? QStringLiteral("apply") : QStringLiteral("list"));
            return true;
        }
        r.word();
        return r.atEnd();
    }
    if (w0 == QLatin1String("help") || w0 == QLatin1String("?"))
    {
        r.word();
        if (r.atEnd())
        {
            for (const Word &w : words())
                *out << w.word;
            return true;
        }
        r.word();
        return r.atEnd();
    }
    if (w0 == QLatin1String("copy"))
    {
        r.word();
        if (r.atEnd())
        {
            *out = QStringList({ "cue", "layer" });
            return true;
        }
        const QString what = r.word().toLower();
        if (what == QLatin1String("cue"))
        {
            while (!r.atEnd())
                r.word();
            *out = cueNumbers(context);
            return true;
        }
        if (what == QLatin1String("layer"))
            return names(targets, false);
        return false;
    }
    if (w0 == QLatin1String("set"))
    {
        // set "Towers" SL SR: a name, then groups.
        r.word();
        if (r.atEnd())
            return true;
        if (r.src.mid(r.i).trimmed().startsWith(QLatin1Char('"')))
            r.quoted();
        else
            r.word();
        return names(targets, false);
    }
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
