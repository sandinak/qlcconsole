/*
  Q Light Controller Plus - qlcconsole
  operationregistry.cpp

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

/* The Operation Registry's catalog (docs/design/AI_ASSIST.md section 4).
   P0/P1: the Show Tree grammar as one operation, and the read-only
   questions -- each a pure read of the Doc that reuses what already answers
   it (Show Review's checks, the cue sheet, Doc::getUsage, Why is this on?)
   instead of a second copy. */

#include <QJsonArray>
#include <QUrl>
#include <QSet>
#include <algorithm>

#include "operationregistry.h"
#include "showtreecontroller.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "fixturegroup.h"
#include "showreview.h"
#include "collection.h"
#include "qlcpalette.h"
#include "cuesheet.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
/* why_on: Why is this on? (engine/src/whyon.*), landing beside this in the
   same train; the guard can go once both are on main. */
#if __has_include("whyon.h")
#include "whyon.h"
#define QLC_HAVE_WHYON 1
#endif
#include "doc.h"

namespace
{
std::function<bool(quint32)> s_functionUseProbe;

const quint32 kInvalid = 0xFFFFFFFF;

QJsonObject objectSchema(const QJsonObject &properties = QJsonObject(),
                         const QStringList &required = QStringList())
{
    QJsonObject s;
    s.insert(QStringLiteral("type"), QStringLiteral("object"));
    s.insert(QStringLiteral("properties"), properties);
    if (!required.isEmpty())
        s.insert(QStringLiteral("required"), QJsonArray::fromStringList(required));
    return s;
}

QJsonObject stringProperty(const QString &description, const QStringList &choices = QStringList())
{
    QJsonObject p;
    p.insert(QStringLiteral("type"), QStringLiteral("string"));
    p.insert(QStringLiteral("description"), description);
    if (!choices.isEmpty())
        p.insert(QStringLiteral("enum"), QJsonArray::fromStringList(choices));
    return p;
}

QString arg(const QJsonObject &args, const char *key)
{
    return args.value(QLatin1String(key)).toString().trimmed();
}

OperationRegistry::Result answer(const QString &message, const QString &text)
{
    OperationRegistry::Result r;
    r.ok = true;
    r.message = message;
    r.text = text;
    if (r.text.size() > OperationRegistry::MaxText)
    {
        r.text.truncate(OperationRegistry::MaxText);
        r.text += QLatin1Char('\n') + OperationRegistry::tr("(cut: the answer was longer)");
    }
    return r;
}

OperationRegistry::Result refusal(const QString &why)
{
    OperationRegistry::Result r;
    r.message = why;
    return r;
}

QString addressText(const Fixture *fx)
{
    if (fx->address() == kInvalid || !fx->isPatched())
        return OperationRegistry::tr("not patched");
    const quint32 first = fx->address() + 1;
    const quint32 last = fx->address() + fx->channels();
    return OperationRegistry::tr("universe %1, %2-%3")
        .arg(fx->universe() + 1).arg(first, 3, 10, QLatin1Char('0')).arg(last, 3, 10, QLatin1Char('0'));
}

QString fixtureLine(const Fixture *fx)
{
    QString def;
    if (fx->fixtureDef() != nullptr)
        def = QStringLiteral("%1 %2").arg(fx->fixtureDef()->manufacturer(), fx->fixtureDef()->model());
    if (fx->fixtureMode() != nullptr)
        def += QStringLiteral(" (%1)").arg(fx->fixtureMode()->name());
    return QStringLiteral("%1 · %2 · %3").arg(fx->name(), def.trimmed(), addressText(fx));
}

/** The function @a name names (its name, ignoring case, or "#12"); null and
 *  @a why when none or several do. */
Function *findFunction(Doc *doc, const QString &name, QString *why)
{
    const QString key = name.trimmed();
    if (key.startsWith(QLatin1Char('#')))
    {
        bool ok = false;
        Function *f = doc->function(key.mid(1).toUInt(&ok));
        if (ok && f != nullptr)
            return f;
    }
    QList<Function *> hits;
    for (Function *f : doc->functions())
        if (f->name().compare(key, Qt::CaseInsensitive) == 0)
            hits << f;
    if (hits.count() == 1)
        return hits.first();
    if (hits.isEmpty())
        *why = OperationRegistry::tr("No function called \"%1\"").arg(key);
    else
        *why = OperationRegistry::tr("%1 functions are called \"%2\"; give its number (#%3)")
                   .arg(hits.count()).arg(key).arg(hits.first()->id());
    return nullptr;
}

#ifdef QLC_HAVE_WHYON
Fixture *findFixture(Doc *doc, const QString &name, QString *why)
{
    const QString key = name.trimmed();
    if (key.startsWith(QLatin1Char('#')))
    {
        bool ok = false;
        Fixture *fx = doc->fixture(key.mid(1).toUInt(&ok));
        if (ok && fx != nullptr)
            return fx;
    }
    QList<Fixture *> hits;
    for (Fixture *fx : doc->fixtures())
        if (fx->name().compare(key, Qt::CaseInsensitive) == 0)
            hits << fx;
    if (hits.count() == 1)
        return hits.first();
    if (hits.isEmpty())
        *why = OperationRegistry::tr("No fixture called \"%1\"").arg(key);
    else
        *why = OperationRegistry::tr("%1 fixtures are called \"%2\"; give its number (#%3)")
                   .arg(hits.count()).arg(key).arg(hits.first()->id());
    return nullptr;
}
#endif

/** Function types by their words ("scene", "rgbmatrix"). */
QStringList functionTypeWords()
{
    QStringList out;
    for (Function::Type t : { Function::SceneType, Function::ChaserType, Function::EFXType,
                              Function::CollectionType, Function::ScriptType, Function::RGBMatrixType,
                              Function::ShowType, Function::SequenceType, Function::AudioType,
                              Function::VideoType })
        out << Function::typeToString(t).toLower().remove(QLatin1Char(' '));
    return out;
}

QString typeWord(const Function *f)
{
    return Function::typeToString(f->type()).toLower().remove(QLatin1Char(' '));
}

/** Show Review's findings for @a checks (all: empty), open ones only, at
 *  @a min or worse -- run as a query: the panel's result is untouched. */
QString reviewText(Doc *doc, const QStringList &checks, ReviewFinding::Severity min, int *count)
{
    *count = 0;
    if (doc->showReview() == nullptr)
        return QString();
    ShowReview::Options opt;
    opt.checks = checks;
    opt.record = false;
    const ShowReview::Result res = doc->showReview()->run(opt);
    QStringList lines;
    for (const ReviewFinding &f : res.findings)
    {
        if (!f.isOpen() || f.severity < min)
            continue;
        lines << QStringLiteral("%1 %2: %3").arg(ShowReview::severityName(f.severity), f.check, f.message);
        ++*count;
    }
    return lines.join(QLatin1Char('\n'));
}

/** Functions nothing uses: no function holds them (Doc::getUsage), no cue
 *  plays them as a function layer, the tree didn't make them, they aren't
 *  the start-up function, and the UI's probe (VC widgets) doesn't claim
 *  them. */
QList<Function *> unusedFunctions(Doc *doc)
{
    QSet<quint32> inTree;
    if (doc->showTree() != nullptr)
    {
        for (const ShowTree::Production &p : doc->showTree()->productions())
            for (const ShowTree::CueList &l : p.lists)
            {
                if (l.source != kInvalid)
                    inTree.insert(l.source);
                for (const ShowTree::CueList::Original &o : l.originals)
                    inTree.insert(o.function);
                for (const ShowTree::Cue &c : l.cues)
                    for (const ShowTree::Layer &layer : c.layers)
                        if (layer.target.isOpaque())
                            inTree.insert(layer.target.function);
            }
    }
    QList<Function *> out;
    for (Function *f : doc->functions())
    {
        if (f->isVisible() == false || inTree.contains(f->id()) || f->id() == doc->startupFunction())
            continue;
        if (doc->showTree() != nullptr && doc->showTree()->isGenerated(f->id()))
            continue;
        if (!doc->getUsage(f->id()).isEmpty())
            continue;
        if (s_functionUseProbe && s_functionUseProbe(f->id()))
            continue;
        out << f;
    }
    std::sort(out.begin(), out.end(), [](const Function *a, const Function *b) {
        return a->path(true) + a->name() < b->path(true) + b->name();
    });
    return out;
}

QString functionDetail(Doc *doc, Function *f)
{
    QStringList lines;
    lines << OperationRegistry::tr("%1 \"%2\" (#%3)").arg(Function::typeToString(f->type()), f->name()).arg(f->id());
    if (!f->path(true).isEmpty())
        lines << OperationRegistry::tr("Folder: %1").arg(f->path(true));
    if (doc->showTree() != nullptr && doc->showTree()->isGenerated(f->id()))
        lines << OperationRegistry::tr("Made by the Show Tree: %1").arg(doc->showTree()->describeGenerated(f->id()));

    if (Scene *s = qobject_cast<Scene *>(f))
        lines << OperationRegistry::tr("%1 channels on %2 fixtures").arg(s->values().count()).arg(s->fixtures().count());
    else if (Chaser *c = qobject_cast<Chaser *>(f))
    {
        lines << OperationRegistry::tr("%1 steps:").arg(c->stepsCount());
        for (int i = 0; i < c->stepsCount() && i < 64; ++i)
        {
            const ChaserStep *step = c->stepAt(i);
            const Function *sf = doc->function(step->fid);
            lines << OperationRegistry::tr("  %1. %2 (fade in %3, hold %4)")
                         .arg(i + 1)
                         .arg(sf != nullptr ? sf->name() : OperationRegistry::tr("(missing)"))
                         .arg(Function::speedToString(step->fadeIn), Function::speedToString(step->hold));
        }
    }
    else if (Collection *col = qobject_cast<Collection *>(f))
    {
        QStringList names;
        for (quint32 id : col->functions())
            if (const Function *m = doc->function(id))
                names << m->name();
        lines << OperationRegistry::tr("Plays together: %1").arg(names.join(QStringLiteral(", ")));
    }

    QStringList users;
    const QList<quint32> usage = doc->getUsage(f->id());
    for (int i = 0; i + 1 < usage.count(); i += 2)
        if (const Function *u = doc->function(usage.at(i)))
            users << u->name();
    users.removeDuplicates();
    lines << (users.isEmpty() ? OperationRegistry::tr("No other function uses it")
                              : OperationRegistry::tr("Used by: %1").arg(users.join(QStringLiteral(", "))));
    return lines.join(QLatin1Char('\n'));
}

/** A rest-of-line argument: the words joined again, empty when none. */
bool restInto(const char *key, const QStringList &words, QJsonObject *args, QString *)
{
    if (!words.isEmpty())
        args->insert(QLatin1String(key), words.join(QLatin1Char(' ')));
    return true;
}

bool noWords(const QStringList &words, QJsonObject *, QString *why)
{
    if (words.isEmpty())
        return true;
    *why = OperationRegistry::tr("takes nothing after it");
    return false;
}

QString ok(Doc *, const QJsonObject &)
{
    return QString();
}

QList<OperationRegistry::Operation> buildCatalog()
{
    typedef OperationRegistry R;
    typedef OperationRegistry::Operation Op;
    QList<Op> ops;

    /* ---- the Show Tree grammar: a whole command line (P0) ---- */
    {
        Op op;
        op.name = QStringLiteral("command");
        op.title = R::tr("Run a command line");
        op.description = R::tr("Runs one line of the Show Tree command line (cue, layers, looks, "
                               "palettes, record, undo...): one undo step.");
        op.readOnly = false;
        op.schema = objectSchema({ { QStringLiteral("line"), stringProperty(R::tr("The command line text")) } },
                                 { QStringLiteral("line") });
        op.validate = [](Doc *, const QJsonObject &a) {
            return arg(a, "line").isEmpty() ? R::tr("No command") : QString();
        };
        op.execute = [](R::Context &ctx, const QJsonObject &a) {
            R::Result r;
            if (ctx.commander == nullptr)
                return refusal(R::tr("The command line isn't open here"));
            r.command = ctx.commander->run(arg(a, "line"), ctx.selection);
            r.ok = r.command.ok;
            r.message = r.command.message;
            return r;
        };
        op.cliSyntax = [](const QJsonObject &a) { return arg(a, "line"); };
        op.describe = [](const QJsonObject &a) { return R::tr("Run: %1").arg(arg(a, "line")); };
        ops << op;
    }

    /* ---- read-only questions (P1) ---- */
    {
        Op op;
        op.name = QStringLiteral("list_fixtures");
        op.title = R::tr("List fixtures");
        op.description = R::tr("Lists the show's fixtures: name, definition and mode, universe and "
                               "addresses. Optional filter: part of a name or definition.");
        op.schema = objectSchema({ { QStringLiteral("filter"),
                                     stringProperty(R::tr("Only fixtures whose name or definition contains this")) } });
        op.cliWord = QStringLiteral("fixtures");
        op.cliHelp = QStringLiteral("fixtures [<text>]");
        op.validate = ok;
        op.parse = [](const QStringList &w, QJsonObject *a, QString *why) { return restInto("filter", w, a, why); };
        op.execute = [](R::Context &ctx, const QJsonObject &a) {
            const QString filter = arg(a, "filter");
            QStringList lines;
            for (const Fixture *fx : ctx.doc->fixtures())
            {
                const QString line = fixtureLine(fx);
                if (filter.isEmpty() || line.contains(filter, Qt::CaseInsensitive))
                    lines << line;
            }
            return answer(R::tr("%n fixture(s)", "", lines.count()), lines.join(QLatin1Char('\n')));
        };
        op.cliSyntax = [](const QJsonObject &a) {
            const QString f = arg(a, "filter");
            return f.isEmpty() ? QStringLiteral("fixtures") : QStringLiteral("fixtures %1").arg(f);
        };
        op.describe = [](const QJsonObject &a) {
            const QString f = arg(a, "filter");
            return f.isEmpty() ? R::tr("List the fixtures") : R::tr("List the fixtures matching \"%1\"").arg(f);
        };
        ops << op;
    }
    {
        Op op;
        op.name = QStringLiteral("list_groups");
        op.title = R::tr("List groups");
        op.description = R::tr("Lists the fixture groups (with their folder) and how many fixtures each has.");
        op.schema = objectSchema();
        op.cliWord = QStringLiteral("groups");
        op.cliHelp = QStringLiteral("groups");
        op.validate = ok;
        op.parse = noWords;
        op.execute = [](R::Context &ctx, const QJsonObject &) {
            QStringList lines;
            for (const FixtureGroup *g : ctx.doc->fixtureGroups())
            {
                const QString where = g->path().isEmpty() ? QString() : g->path() + QLatin1Char('/');
                lines << R::tr("%1%2 · %n fixture(s)", "", g->fixtureList().count()).arg(where, g->name());
            }
            lines.sort(Qt::CaseInsensitive);
            return answer(R::tr("%n group(s)", "", lines.count()), lines.join(QLatin1Char('\n')));
        };
        op.cliSyntax = [](const QJsonObject &) { return QStringLiteral("groups"); };
        op.describe = [](const QJsonObject &) { return R::tr("List the groups"); };
        ops << op;
    }
    {
        Op op;
        op.name = QStringLiteral("list_functions");
        op.title = R::tr("List functions");
        op.description = R::tr("Lists the show's functions (scenes, chasers, EFX, collections...) "
                               "by folder. Optional type and name filter.");
        op.schema = objectSchema({ { QStringLiteral("type"), stringProperty(R::tr("Only this type"), functionTypeWords()) },
                                   { QStringLiteral("filter"), stringProperty(R::tr("Only names containing this")) } });
        op.cliWord = QStringLiteral("functions");
        op.cliHelp = QStringLiteral("functions [<type>] [<text>]");
        op.validate = [](Doc *, const QJsonObject &a) {
            const QString t = arg(a, "type").toLower();
            return t.isEmpty() || functionTypeWords().contains(t) ? QString()
                                                                    : R::tr("No function type \"%1\"").arg(t);
        };
        op.parse = [](const QStringList &w, QJsonObject *a, QString *) {
            QStringList rest = w;
            if (!rest.isEmpty() && functionTypeWords().contains(rest.first().toLower()))
                a->insert(QStringLiteral("type"), rest.takeFirst().toLower());
            if (!rest.isEmpty())
                a->insert(QStringLiteral("filter"), rest.join(QLatin1Char(' ')));
            return true;
        };
        op.execute = [](R::Context &ctx, const QJsonObject &a) {
            const QString type = arg(a, "type").toLower();
            const QString filter = arg(a, "filter");
            QStringList lines;
            for (const Function *f : ctx.doc->functions())
            {
                if (!f->isVisible() || (!type.isEmpty() && typeWord(f) != type))
                    continue;
                if (!filter.isEmpty() && !f->name().contains(filter, Qt::CaseInsensitive))
                    continue;
                const QString where = f->path(true).isEmpty() ? QString() : f->path(true) + QLatin1Char('/');
                lines << QStringLiteral("%1%2 · %3 · #%4").arg(where, f->name(), Function::typeToString(f->type())).arg(f->id());
            }
            lines.sort(Qt::CaseInsensitive);
            return answer(R::tr("%n function(s)", "", lines.count()), lines.join(QLatin1Char('\n')));
        };
        op.cliSyntax = [](const QJsonObject &a) {
            QStringList w { QStringLiteral("functions") };
            if (!arg(a, "type").isEmpty())
                w << arg(a, "type").toLower();
            if (!arg(a, "filter").isEmpty())
                w << arg(a, "filter");
            return w.join(QLatin1Char(' '));
        };
        op.describe = [](const QJsonObject &a) {
            return arg(a, "type").isEmpty() ? R::tr("List the functions")
                                            : R::tr("List the %1 functions").arg(arg(a, "type"));
        };
        ops << op;
    }
    {
        Op op;
        op.name = QStringLiteral("describe_function");
        op.title = R::tr("Describe a function");
        op.description = R::tr("Describes one function by name (or #id): its type, folder, contents "
                               "(a chaser's steps, a collection's members, a scene's size) and what uses it.");
        op.schema = objectSchema({ { QStringLiteral("function"), stringProperty(R::tr("The function's name, or #id")) } },
                                 { QStringLiteral("function") });
        op.cliWord = QStringLiteral("describe");
        op.cliHelp = QStringLiteral("describe <function>");
        op.validate = [](Doc *doc, const QJsonObject &a) {
            QString why;
            if (arg(a, "function").isEmpty())
                return R::tr("Which function?");
            return findFunction(doc, arg(a, "function"), &why) == nullptr ? why : QString();
        };
        op.parse = [](const QStringList &w, QJsonObject *a, QString *why) {
            if (w.isEmpty())
            {
                *why = R::tr("describe <function>");
                return false;
            }
            return restInto("function", w, a, why);
        };
        op.execute = [](R::Context &ctx, const QJsonObject &a) {
            QString why;
            Function *f = findFunction(ctx.doc, arg(a, "function"), &why);
            return answer(f->name(), functionDetail(ctx.doc, f));
        };
        op.cliSyntax = [](const QJsonObject &a) {
            return QStringLiteral("describe %1").arg(R::quoted(arg(a, "function")));
        };
        op.describe = [](const QJsonObject &a) { return R::tr("Describe \"%1\"").arg(arg(a, "function")); };
        ops << op;
    }
    {
        Op op;
        op.name = QStringLiteral("list_cues");
        op.title = R::tr("List cues");
        op.description = R::tr("The cue sheet of a production (the first one when none is named): every "
                               "cue in running order with number, name, fades, how it goes on and notes.");
        op.schema = objectSchema({ { QStringLiteral("production"), stringProperty(R::tr("The production's name")) } });
        op.cliWord = QStringLiteral("cues");
        op.cliHelp = QStringLiteral("cues [<production>]");
        op.validate = [](Doc *doc, const QJsonObject &a) {
            if (doc->showTree() == nullptr || doc->showTree()->productions().isEmpty())
                return R::tr("The show has no Show Tree cues");
            const QString name = arg(a, "production");
            if (name.isEmpty())
                return QString();
            for (const ShowTree::Production &p : doc->showTree()->productions())
                if (p.name.compare(name, Qt::CaseInsensitive) == 0)
                    return QString();
            return R::tr("No production called \"%1\"").arg(name);
        };
        op.parse = [](const QStringList &w, QJsonObject *a, QString *why) { return restInto("production", w, a, why); };
        op.execute = [](R::Context &ctx, const QJsonObject &a) {
            ShowTreeController *st = ctx.doc->showTree();
            quint32 production = st->productions().first().id;
            const QString name = arg(a, "production");
            for (const ShowTree::Production &p : st->productions())
                if (!name.isEmpty() && p.name.compare(name, Qt::CaseInsensitive) == 0)
                    production = p.id;
            const CueSheet sheet = CueSheet::build(ctx.doc, production);
            QStringList lines;
            for (const CueSheet::ListBlock &block : sheet.lists())
            {
                lines << R::tr("Cue list %1:").arg(block.name);
                for (const CueSheet::Row &row : block.rows)
                {
                    if (row.kind == CueSheet::Row::Section)
                    {
                        lines << QStringLiteral("  -- %1").arg(row.name);
                        continue;
                    }
                    QString line = QStringLiteral("  %1 %2 · in %3 out %4 · %5")
                                       .arg(row.number, row.name, row.fadeIn, row.fadeOut, row.follow);
                    if (!row.note.isEmpty())
                        line += QStringLiteral(" · ") + row.note;
                    if (row.skipped)
                        line += R::tr(" (skipped)");
                    lines << line;
                }
            }
            return answer(R::tr("%n cue(s) in %1", "", sheet.cueCount()).arg(sheet.productionName()),
                          lines.join(QLatin1Char('\n')));
        };
        op.cliSyntax = [](const QJsonObject &a) {
            const QString p = arg(a, "production");
            return p.isEmpty() ? QStringLiteral("cues") : QStringLiteral("cues %1").arg(p);
        };
        op.describe = [](const QJsonObject &) { return R::tr("Show the cue sheet"); };
        ops << op;
    }
#ifdef QLC_HAVE_WHYON
    {
        Op op;
        op.name = QStringLiteral("why_on");
        op.title = R::tr("Why is this on?");
        op.description = R::tr("Explains what sets a fixture's intensity right now, top-down: cue and "
                               "layer, override hit, effect, Park/Highlight, Simple Desk, masters, Blind.");
        op.schema = objectSchema({ { QStringLiteral("fixture"), stringProperty(R::tr("The fixture's name, or #id")) } },
                                 { QStringLiteral("fixture") });
        op.cliWord = QStringLiteral("why");
        op.cliHelp = QStringLiteral("why <fixture>");
        op.validate = [](Doc *doc, const QJsonObject &a) {
            QString why;
            if (arg(a, "fixture").isEmpty())
                return R::tr("Which fixture?");
            return findFixture(doc, arg(a, "fixture"), &why) == nullptr ? why : QString();
        };
        op.parse = [](const QStringList &w, QJsonObject *a, QString *why) {
            if (w.isEmpty())
            {
                *why = R::tr("why <fixture>");
                return false;
            }
            return restInto("fixture", w, a, why);
        };
        op.execute = [](R::Context &ctx, const QJsonObject &a) {
            QString why;
            Fixture *fx = findFixture(ctx.doc, arg(a, "fixture"), &why);
            const WhyOn::Answer ans = WhyOn::explain(ctx.doc, fx->id());
            if (!ans.isValid())
                return refusal(R::tr("%1 isn't patched: nothing sets it").arg(fx->name()));
            return answer(ans.headline, ans.toText());
        };
        op.cliSyntax = [](const QJsonObject &a) { return QStringLiteral("why %1").arg(arg(a, "fixture")); };
        op.describe = [](const QJsonObject &a) { return R::tr("Why is %1 on?").arg(arg(a, "fixture")); };
        ops << op;
    }
#endif
    {
        Op op;
        op.name = QStringLiteral("find_conflicts");
        op.title = R::tr("Find address conflicts");
        op.description = R::tr("Finds fixtures that share DMX channels, and fixtures that run past the end "
                               "of their universe (Show Review's PAT-01 and PAT-02).");
        op.schema = objectSchema();
        op.cliWord = QStringLiteral("conflicts");
        op.cliHelp = QStringLiteral("conflicts");
        op.validate = ok;
        op.parse = noWords;
        op.execute = [](R::Context &ctx, const QJsonObject &) {
            int n = 0;
            const QString text = reviewText(ctx.doc, { QStringLiteral("PAT-01"), QStringLiteral("PAT-02") },
                                            ReviewFinding::Note, &n);
            return answer(n == 0 ? R::tr("No address conflicts") : R::tr("%n address conflict(s)", "", n),
                          n == 0 ? R::tr("No two fixtures share a channel, and none runs past 512.") : text);
        };
        op.cliSyntax = [](const QJsonObject &) { return QStringLiteral("conflicts"); };
        op.describe = [](const QJsonObject &) { return R::tr("Find address conflicts"); };
        ops << op;
    }
    {
        Op op;
        op.name = QStringLiteral("find_unused_functions");
        op.title = R::tr("Find unused functions");
        op.description = R::tr("Finds functions nothing plays: no chaser, collection or sequence holds "
                               "them, no cue plays them, they aren't the start-up function and no "
                               "Virtual Console widget runs them.");
        op.schema = objectSchema();
        op.cliWord = QStringLiteral("unused");
        op.cliHelp = QStringLiteral("unused");
        op.validate = ok;
        op.parse = noWords;
        op.execute = [](R::Context &ctx, const QJsonObject &) {
            QStringList lines;
            for (const Function *f : unusedFunctions(ctx.doc))
            {
                const QString where = f->path(true).isEmpty() ? QString() : f->path(true) + QLatin1Char('/');
                lines << QStringLiteral("%1%2 · %3 · #%4").arg(where, f->name(), Function::typeToString(f->type())).arg(f->id());
            }
            if (!s_functionUseProbe)
                lines << R::tr("(Virtual Console widgets weren't checked.)");
            return answer(R::tr("%n unused function(s)", "", lines.count() - (s_functionUseProbe ? 0 : 1)),
                          lines.join(QLatin1Char('\n')));
        };
        op.cliSyntax = [](const QJsonObject &) { return QStringLiteral("unused"); };
        op.describe = [](const QJsonObject &) { return R::tr("Find unused functions"); };
        ops << op;
    }
    {
        Op op;
        op.name = QStringLiteral("find_problems");
        op.title = R::tr("Review the show");
        op.description = R::tr("Runs Show Review: the open problems in the show's design (patch, "
                               "fixtures, effects, power, media, names), worst first.");
        const QStringList levels { QStringLiteral("note"), QStringLiteral("warning"), QStringLiteral("blocker") };
        op.schema = objectSchema({ { QStringLiteral("min"), stringProperty(R::tr("Only this bad or worse"), levels) } });
        op.cliWord = QStringLiteral("review");
        op.cliHelp = QStringLiteral("review [note|warning|blocker]");
        op.validate = [levels](Doc *, const QJsonObject &a) {
            const QString m = arg(a, "min").toLower();
            return m.isEmpty() || levels.contains(m) ? QString() : R::tr("note, warning or blocker");
        };
        op.parse = [](const QStringList &w, QJsonObject *a, QString *why) {
            if (w.count() > 1)
            {
                *why = R::tr("review [note|warning|blocker]");
                return false;
            }
            if (!w.isEmpty())
                a->insert(QStringLiteral("min"), w.first().toLower());
            return true;
        };
        op.execute = [levels](R::Context &ctx, const QJsonObject &a) {
            const int min = qMax(0, int(levels.indexOf(arg(a, "min").toLower())));
            int n = 0;
            const QString text = reviewText(ctx.doc, QStringList(), ReviewFinding::Severity(min), &n);
            return answer(R::tr("%n open finding(s)", "", n), n == 0 ? R::tr("Nothing open.") : text);
        };
        op.cliSyntax = [](const QJsonObject &a) {
            const QString m = arg(a, "min").toLower();
            return m.isEmpty() ? QStringLiteral("review") : QStringLiteral("review %1").arg(m);
        };
        op.describe = [](const QJsonObject &) { return R::tr("Review the show"); };
        ops << op;
    }
    {
        Op op;
        op.name = QStringLiteral("show_summary");
        op.title = R::tr("Summarise the show");
        op.description = R::tr("A short summary of the show: fixtures and universes, groups, functions "
                               "by type, palettes, Show Tree productions, cue lists and cues, and the mode.");
        op.schema = objectSchema();
        op.cliWord = QStringLiteral("summary");
        op.cliHelp = QStringLiteral("summary");
        op.validate = ok;
        op.parse = noWords;
        op.execute = [](R::Context &ctx, const QJsonObject &) {
            Doc *doc = ctx.doc;
            QStringList lines;
            QSet<quint32> universes;
            int patched = 0;
            for (const Fixture *fx : doc->fixtures())
                if (fx->isPatched() && fx->address() != kInvalid)
                {
                    universes.insert(fx->universe());
                    ++patched;
                }
            lines << R::tr("Fixtures: %1 (%2 patched, on %n universe(s))", "", universes.count())
                         .arg(doc->fixtures().count()).arg(patched);
            lines << R::tr("Groups: %1").arg(doc->fixtureGroups().count());
            QMap<QString, int> byType;
            for (const Function *f : doc->functions())
                if (f->isVisible())
                    byType[Function::typeToString(f->type())]++;
            QStringList types;
            for (auto it = byType.cbegin(); it != byType.cend(); ++it)
                types << QStringLiteral("%1 %2").arg(it.value()).arg(it.key());
            lines << R::tr("Functions: %1").arg(types.isEmpty() ? R::tr("none") : types.join(QStringLiteral(", ")));
            lines << R::tr("Palettes: %1").arg(doc->palettes().count());
            if (ShowTreeController *st = doc->showTree())
            {
                int lists = 0, cues = 0;
                for (const ShowTree::Production &p : st->productions())
                    for (const ShowTree::CueList &l : p.lists)
                    {
                        ++lists;
                        cues += l.cues.count();
                    }
                lines << R::tr("Show Tree: %1 production(s), %2 cue list(s), %3 cue(s)")
                             .arg(st->productions().count()).arg(lists).arg(cues);
                const ShowTree::CueList *list = st->cueList(ctx.selection.list);
                const int at = list != nullptr ? list->indexOf(ctx.selection.cue) : -1;
                if (at >= 0)
                    lines << R::tr("Selected cue: %1 %2").arg(list->cues.at(at).number, list->cues.at(at).name);
            }
            lines << R::tr("Mode: %1").arg(doc->mode() == Doc::Operate ? R::tr("Operate") : R::tr("Design"));
            return answer(R::tr("Show summary"), lines.join(QLatin1Char('\n')));
        };
        op.cliSyntax = [](const QJsonObject &) { return QStringLiteral("summary"); };
        op.describe = [](const QJsonObject &) { return R::tr("Summarise the show"); };
        ops << op;
    }
    return ops;
}
} // namespace

/****************************************************************************
 * The catalog
 ****************************************************************************/

const QList<OperationRegistry::Operation> &OperationRegistry::catalog()
{
    static const QList<Operation> ops = buildCatalog();
    return ops;
}

const OperationRegistry::Operation *OperationRegistry::operation(const QString &name)
{
    for (const Operation &op : catalog())
        if (op.name == name)
            return &op;
    return nullptr;
}

QList<const OperationRegistry::Operation *> OperationRegistry::readOnlyOperations()
{
    QList<const Operation *> out;
    for (const Operation &op : catalog())
        if (op.readOnly)
            out << &op;
    return out;
}

void OperationRegistry::setFunctionUseProbe(const std::function<bool(quint32)> &probe)
{
    s_functionUseProbe = probe;
}

/****************************************************************************
 * Running
 ****************************************************************************/

OperationRegistry::OperationRegistry(Doc *doc, ShowTreeCommand *commander)
    : m_doc(doc)
    , m_commander(commander)
{
}

OperationRegistry::Result OperationRegistry::run(const QString &name, const QJsonObject &args,
                                                 const ShowTreeCommand::Context &selection)
{
    const Operation *op = operation(name);
    if (op == nullptr)
        return refusal(tr("No operation \"%1\"").arg(name));
    const QString why = op->validate ? op->validate(m_doc, args) : QString();
    if (!why.isEmpty())
        return refusal(why);
    Context ctx;
    ctx.doc = m_doc;
    ctx.commander = m_commander;
    ctx.selection = selection;
    return op->execute(ctx, args);
}

QStringList OperationRegistry::splitWords(const QString &line)
{
    QStringList out;
    QString cur;
    bool inQuote = false;
    bool any = false;
    for (const QChar c : line)
    {
        if (c == QLatin1Char('"'))
        {
            inQuote = !inQuote;
            any = true;
            continue;
        }
        if (c.isSpace() && !inQuote)
        {
            if (any)
                out << cur;
            cur.clear();
            any = false;
            continue;
        }
        cur += c;
        any = true;
    }
    if (any)
        out << cur;
    return out;
}

QString OperationRegistry::quoted(const QString &name)
{
    return name.contains(QLatin1Char(' ')) ? QStringLiteral("\"%1\"").arg(name) : name;
}

bool OperationRegistry::nameTaken(const QString &word) const
{
    for (const Fixture *fx : m_doc->fixtures())
        if (fx->name().compare(word, Qt::CaseInsensitive) == 0)
            return true;
    for (const FixtureGroup *g : m_doc->fixtureGroups())
        if (g->name().compare(word, Qt::CaseInsensitive) == 0)
            return true;
    if (m_commander != nullptr && m_commander->groupShortNames().contains(word.toLower()))
        return true;
    return false;
}

QString OperationRegistry::resolveLine(const QString &line, QJsonObject *args, QString *why) const
{
    *args = QJsonObject();
    const QStringList words = splitWords(line.trimmed());
    if (!words.isEmpty())
    {
        const QString w0 = words.first().toLower();
        for (const Operation &op : catalog())
        {
            if (op.cliWord.isEmpty() || op.cliWord != w0 || !op.parse)
                continue;
            if (nameTaken(words.first()))
                break;              // the show's own name: the grammar's
            QString err;
            QJsonObject a;
            if (!op.parse(words.mid(1), &a, &err))
            {
                if (why != nullptr)
                    *why = err;
                return op.name;
            }
            *args = a;
            return op.name;
        }
    }
    args->insert(QStringLiteral("line"), line);
    return QStringLiteral("command");
}

ShowTreeCommand::Result OperationRegistry::runLine(const QString &line, const ShowTreeCommand::Context &selection)
{
    QJsonObject args;
    QString why;
    const QString name = resolveLine(line, &args, &why);
    if (name == QLatin1String("command"))
    {
        Result r = run(name, args, selection);
        ShowTreeCommand::Result res = r.command;
        if (m_commander == nullptr)
        {
            res.context = selection;
            res.message = r.message;
        }
        // help: the registry's words too.
        const QStringList w = splitWords(line.trimmed());
        if (res.ok && w.count() == 1
            && (w.first().compare(QLatin1String("help"), Qt::CaseInsensitive) == 0 || w.first() == QLatin1String("?")))
            res.help += helpText();
        return res;
    }

    ShowTreeCommand::Result res;
    res.context = selection;
    res.keepSelection = true;
    const Operation *op = operation(name);
    if (!why.isEmpty())
    {
        res.message = QStringLiteral("%1: %2").arg(op->cliWord, why);
        return res;
    }
    const Result r = run(name, args, selection);
    res.ok = r.ok;
    res.message = r.message;
    if (r.ok)
        res.help = QStringLiteral("<b>%1</b><pre>%2</pre>").arg(op->cliSyntax(args).toHtmlEscaped(),
                                                               r.text.toHtmlEscaped());
    return res;
}

QString OperationRegistry::helpText()
{
    QString html = QStringLiteral("<h4>%1</h4><table>").arg(tr("Questions (they change nothing)"));
    for (const Operation &op : catalog())
    {
        if (op.cliWord.isEmpty())
            continue;
        html += QStringLiteral("<tr><td><a href=\"ex:%1\"><code>%2</code></a></td><td>%3</td></tr>")
                    .arg(QString::fromLatin1(QUrl::toPercentEncoding(op.cliWord)), op.cliHelp.toHtmlEscaped(),
                         op.title.toHtmlEscaped());
    }
    html += QStringLiteral("</table>");
    return html;
}
