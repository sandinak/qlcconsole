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
   instead of a second copy. P2: the grammar's change words as operations
   with schemas, and runPlan() -- a confirmed plan as one undo step. */

#include <QRegularExpression>
#include <QFileInfo>
#include <QJsonArray>
#include <QUrl>
#include <QSet>
#include <algorithm>
#include <climits>

#include "operationregistry.h"
#include "configimport.h"
#include "groupmasters.h"
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
#include "whyon.h"
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

/** Whether the show has a Virtual Console to ask (its VcIndex), or the UI
 *  gave a probe. */
bool vcChecked(Doc *doc)
{
    if (s_functionUseProbe)
        return true;
    return doc->showReview() != nullptr && doc->showReview()->vcIndex().available;
}

/** Functions nothing uses: no function holds them (Doc::getUsage), no cue
 *  plays them as a function layer, the tree didn't make them, they aren't
 *  the start-up function, and no Virtual Console widget runs them (the
 *  VcIndex; the UI's probe, if one is set, too). */
QList<Function *> unusedFunctions(Doc *doc)
{
    QSet<quint32> onConsole;
    if (doc->showReview() != nullptr)
    {
        const VcIndex vc = doc->showReview()->vcIndex();
        if (vc.available)
            onConsole = vc.functionsUsed();
    }
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
        if (onConsole.contains(f->id()))
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

/* ---- the change words (P2) ----
   Each is one word of the Show Tree grammar with a schema: cliSyntax()
   renders the one line that does it and execute() runs that line, so the
   grammar stays the only thing that edits the tree from text, and a
   model's call reads back as what the operator could have typed. Strict
   schemas (additionalProperties: false): a model's call that doesn't fit is
   refused before anything runs. */

QJsonObject strictSchema(const QJsonObject &properties, const QStringList &required = QStringList())
{
    QJsonObject s = objectSchema(properties, required);
    s.insert(QStringLiteral("additionalProperties"), false);
    return s;
}

QJsonObject namesProperty(const QString &description, int minItems = 1)
{
    QJsonObject item { { QStringLiteral("type"), QStringLiteral("string") }, { QStringLiteral("minLength"), 1 } };
    QJsonObject p { { QStringLiteral("type"), QStringLiteral("array") }, { QStringLiteral("items"), item },
                    { QStringLiteral("description"), description } };
    if (minItems > 0)
        p.insert(QStringLiteral("minItems"), minItems);
    return p;
}

const QString kCuePattern = QStringLiteral("^[0-9]+(\\.[0-9]+)*$");

QJsonObject cueNumberProperty(const QString &description)
{
    QJsonObject p = stringProperty(description);
    p.insert(QStringLiteral("pattern"), kCuePattern);
    return p;
}

QStringList cueModes()
{
    return { QStringLiteral("track"), QStringLiteral("block"), QStringLiteral("cueonly"), QStringLiteral("default") };
}

QStringList lookAttributes()
{
    return { QStringLiteral("colour"), QStringLiteral("dimmer"), QStringLiteral("position"), QStringLiteral("aim"),
             QStringLiteral("spread"), QStringLiteral("group"), QStringLiteral("cross"), QStringLiteral("gobo"),
             QStringLiteral("zoom"), QStringLiteral("beam"), QStringLiteral("shutter"), QStringLiteral("effect") };
}

QJsonObject looksProperty()
{
    QJsonObject value { { QStringLiteral("type"), QJsonArray { QStringLiteral("string"), QStringLiteral("number") } },
                        { QStringLiteral("description"),
                          OperationRegistry::tr("A palette by name, a level 0-100 (dimmer), a hex colour "
                                                "#rrggbb (colour), a stage target (aim), -5..+5 (spread), "
                                                "none (off here) or clear (inherit again)") } };
    QJsonObject item = strictSchema({ { QStringLiteral("attribute"),
                                        stringProperty(OperationRegistry::tr("Which look"), lookAttributes()) },
                                      { QStringLiteral("value"), value } },
                                    { QStringLiteral("attribute"), QStringLiteral("value") });
    return QJsonObject { { QStringLiteral("type"), QStringLiteral("array") }, { QStringLiteral("items"), item },
                         { QStringLiteral("minItems"), 1 },
                         { QStringLiteral("description"), OperationRegistry::tr("The looks, in order") } };
}

/** A name as the grammar reads it: quoted when it has a space, a '+' or a
 *  ',' (they would end it), or is empty. */
QString gq(const QString &name)
{
    if (name.isEmpty() || name.contains(QLatin1Char(' ')) || name.contains(QLatin1Char('+'))
        || name.contains(QLatin1Char(',')))
        return QStringLiteral("\"%1\"").arg(name);
    return name;
}

QStringList strings(const QJsonObject &a, const char *key)
{
    QStringList out;
    for (const QJsonValue &v : a.value(QLatin1String(key)).toArray())
        out << v.toString().trimmed();
    return out;
}

QString valueText(const QJsonValue &v)
{
    if (v.isDouble())
        return QString::number(v.toDouble());
    return v.toString().trimmed();
}

QString looksText(const QJsonObject &a)
{
    QStringList w;
    for (const QJsonValue &v : a.value(QLatin1String("looks")).toArray())
    {
        // A value ends at a space or a comma only ("+5" is a spread).
        const QJsonObject l = v.toObject();
        const QString value = valueText(l.value(QLatin1String("value")));
        w << l.value(QLatin1String("attribute")).toString()
          << (value.isEmpty() || value.contains(QLatin1Char(' ')) || value.contains(QLatin1Char(','))
                  ? QStringLiteral("\"%1\"").arg(value) : value);
    }
    return w.join(QLatin1Char(' '));
}

QString looksSentence(const QJsonObject &a)
{
    QStringList w;
    for (const QJsonValue &v : a.value(QLatin1String("looks")).toArray())
    {
        const QJsonObject l = v.toObject();
        w << OperationRegistry::tr("%1 %2").arg(l.value(QLatin1String("attribute")).toString(),
                                                valueText(l.value(QLatin1String("value"))));
    }
    return w.join(QStringLiteral(", "));
}

QString targetsText(const QJsonObject &a, const char *key = "targets", bool each = false)
{
    QStringList w;
    for (const QString &t : strings(a, key))
        w << gq(t);
    return w.join(each ? QStringLiteral(", ") : QStringLiteral(" "));
}

/** No name may hold a double quote: the grammar couldn't read it back. */
QString noQuotes(const QJsonObject &a)
{
    for (auto it = a.constBegin(); it != a.constEnd(); ++it)
    {
        QStringList values;
        if (it.value().isString())
            values << it.value().toString();
        else if (it.value().isArray())
            for (const QJsonValue &v : it.value().toArray())
            {
                if (v.isString())
                    values << v.toString();
                else if (v.isObject())
                    values << v.toObject().value(QLatin1String("value")).toString();
            }
        for (const QString &s : values)
            if (s.contains(QLatin1Char('"')))
                return OperationRegistry::tr("%1: a name can't hold a double quote").arg(it.key());
    }
    return QString();
}

/** Run the grammar line the operation renders: one undo step on its own,
 *  part of the plan's one inside runPlan(). */
OperationRegistry::Result runGrammar(OperationRegistry::Context &ctx, const QString &line)
{
    if (ctx.commander == nullptr)
        return refusal(OperationRegistry::tr("The command line isn't open here"));
    OperationRegistry::Result r;
    r.command = ctx.commander->run(line, ctx.selection);
    r.ok = r.command.ok;
    r.message = r.command.message;
    return r;
}

void appendChangeOperations(QList<OperationRegistry::Operation> &ops)
{
    typedef OperationRegistry R;
    typedef OperationRegistry::Operation Op;

    // Every change op: strict schema, no quotes in names, runs its line.
    auto add = [&ops](Op op) {
        op.readOnly = false;
        const OperationRegistry::Validator extra = op.validate;
        op.validate = [extra](Doc *doc, const QJsonObject &a) {
            if (doc->showTree() == nullptr)
                return R::tr("The show has no Show Tree");
            const QString why = noQuotes(a);
            if (!why.isEmpty())
                return why;
            return extra ? extra(doc, a) : QString();
        };
        const OperationRegistry::Renderer render = op.cliSyntax;
        op.execute = [render](R::Context &ctx, const QJsonObject &a) { return runGrammar(ctx, render(a)); };
        ops << op;
    };

    {
        Op op;
        op.name = QStringLiteral("make_cue");
        op.title = R::tr("Select or make a cue");
        op.description = R::tr("Selects cue <number> of the selected cue list, or makes it in number order "
                               "(renamed if the name differs), optionally setting its mode. Later steps act "
                               "on this cue.");
        op.schema = strictSchema({ { QStringLiteral("number"), cueNumberProperty(R::tr("The cue number, e.g. 1.0.1")) },
                                   { QStringLiteral("name"), stringProperty(R::tr("The cue's name")) },
                                   { QStringLiteral("mode"), stringProperty(R::tr("Its tracking mode"), cueModes()) } },
                                 { QStringLiteral("number") });
        op.cliSyntax = [](const QJsonObject &a) {
            QStringList w { QStringLiteral("cue"), arg(a, "number") };
            if (a.contains(QLatin1String("name")) && a.value(QLatin1String("name")).isString())
                w << QStringLiteral("\"%1\"").arg(arg(a, "name"));
            if (!arg(a, "mode").isEmpty())
                w << arg(a, "mode");
            return w.join(QLatin1Char(' '));
        };
        op.describe = [](const QJsonObject &a) {
            return arg(a, "name").isEmpty() ? R::tr("Go to cue %1 (made if it isn't there)").arg(arg(a, "number"))
                                            : R::tr("Go to cue %1 \"%2\" (made if it isn't there)")
                                                  .arg(arg(a, "number"), arg(a, "name"));
        };
        add(op);
    }
    {
        Op op;
        op.name = QStringLiteral("add_cue");
        op.title = R::tr("Add a cue after the selected one");
        op.description = R::tr("Makes a new cue right after the selected cue, numbered as + Cue does, "
                               "optionally named and with a mode. Later steps act on the new cue.");
        op.schema = strictSchema({ { QStringLiteral("name"), stringProperty(R::tr("The new cue's name")) },
                                   { QStringLiteral("mode"), stringProperty(R::tr("Its tracking mode"), cueModes()) } });
        op.cliSyntax = [](const QJsonObject &a) {
            QStringList w { QStringLiteral("cue"), QStringLiteral("+") };
            if (!arg(a, "name").isEmpty())
                w << QStringLiteral("\"%1\"").arg(arg(a, "name"));
            if (!arg(a, "mode").isEmpty())
                w << arg(a, "mode");
            return w.join(QLatin1Char(' '));
        };
        op.describe = [](const QJsonObject &a) {
            return arg(a, "name").isEmpty() ? R::tr("Add a cue after the selected one")
                                            : R::tr("Add cue \"%1\" after the selected one").arg(arg(a, "name"));
        };
        add(op);
    }
    {
        Op op;
        op.name = QStringLiteral("make_cue_list");
        op.title = R::tr("Select or make a cue list");
        op.description = R::tr("Selects that cue list of the selected production, or makes a new Tracking one.");
        QJsonObject name = stringProperty(R::tr("The cue list's name"));
        name.insert(QStringLiteral("minLength"), 1);
        op.schema = strictSchema({ { QStringLiteral("name"), name } }, { QStringLiteral("name") });
        op.cliSyntax = [](const QJsonObject &a) { return QStringLiteral("list \"%1\"").arg(arg(a, "name")); };
        op.describe = [](const QJsonObject &a) { return R::tr("Go to cue list \"%1\" (made if it isn't there)").arg(arg(a, "name")); };
        add(op);
    }
    {
        Op op;
        op.name = QStringLiteral("set_look");
        op.title = R::tr("Set looks on groups");
        op.description = R::tr("In the selected cue: the layer on these groups or fixtures (made if needed), "
                               "then its looks -- colour, dimmer, position, gobo... by palette name, a "
                               "dimmer level 0-100 or a #rrggbb colour (a palette playing exactly it is "
                               "reused, else one is made). Several targets are ONE layer, unless each is true.");
        op.schema = strictSchema({ { QStringLiteral("targets"), namesProperty(R::tr("Groups, sets or fixtures by name")) },
                                   { QStringLiteral("each"), QJsonObject { { QStringLiteral("type"), QStringLiteral("boolean") },
                                                                           { QStringLiteral("description"),
                                                                             R::tr("A layer each instead of one on all") } } },
                                   { QStringLiteral("looks"), looksProperty() } },
                                 { QStringLiteral("targets"), QStringLiteral("looks") });
        op.cliSyntax = [](const QJsonObject &a) {
            return QStringLiteral("%1 %2").arg(targetsText(a, "targets", a.value(QLatin1String("each")).toBool()), looksText(a));
        };
        op.describe = [](const QJsonObject &a) {
            return R::tr("%1: %2").arg(strings(a, "targets").join(QStringLiteral(", ")), looksSentence(a));
        };
        add(op);
    }
    {
        Op op;
        op.name = QStringLiteral("set_cue_look");
        op.title = R::tr("Set a look on the whole cue");
        op.description = R::tr("A look for the whole selected cue: it reaches the cue's own layers that don't "
                               "set it themselves (spread: line aims only; effect: one effect across them).");
        op.schema = strictSchema({ { QStringLiteral("looks"), looksProperty() } }, { QStringLiteral("looks") });
        op.cliSyntax = [](const QJsonObject &a) { return QStringLiteral("cue look %1").arg(looksText(a)); };
        op.describe = [](const QJsonObject &a) { return R::tr("The whole cue: %1").arg(looksSentence(a)); };
        add(op);
    }
    {
        Op op;
        op.name = QStringLiteral("set_intensity");
        op.title = R::tr("Set intensity");
        op.description = R::tr("In the selected cue: the dimmer of these groups or fixtures, 0-100 percent "
                               "(the Dimmer palette at that level is reused, else one is made).");
        QJsonObject level { { QStringLiteral("type"), QStringLiteral("integer") }, { QStringLiteral("minimum"), 0 },
                            { QStringLiteral("maximum"), 100 }, { QStringLiteral("description"), R::tr("Percent") } };
        op.schema = strictSchema({ { QStringLiteral("targets"), namesProperty(R::tr("Groups, sets or fixtures by name")) },
                                   { QStringLiteral("level"), level } },
                                 { QStringLiteral("targets"), QStringLiteral("level") });
        op.cliSyntax = [](const QJsonObject &a) {
            return QStringLiteral("%1 dimmer %2").arg(targetsText(a)).arg(a.value(QLatin1String("level")).toInt());
        };
        op.describe = [](const QJsonObject &a) {
            return R::tr("%1 at %2%").arg(strings(a, "targets").join(QStringLiteral(", ")))
                .arg(a.value(QLatin1String("level")).toInt());
        };
        add(op);
    }
    {
        Op op;
        op.name = QStringLiteral("set_cue_mode");
        op.title = R::tr("Set the cue's mode");
        op.description = R::tr("The selected cue's tracking mode: track (starts from the cue before), block "
                               "(from nothing), cueonly (its changes don't pass on), default (the list's).");
        op.schema = strictSchema({ { QStringLiteral("mode"), stringProperty(R::tr("The mode"), cueModes()) } },
                                 { QStringLiteral("mode") });
        op.cliSyntax = [](const QJsonObject &a) { return arg(a, "mode"); };
        op.describe = [](const QJsonObject &a) { return R::tr("Make the selected cue %1").arg(arg(a, "mode")); };
        add(op);
    }
    {
        Op op;
        op.name = QStringLiteral("release_layer");
        op.title = R::tr("Toggle Release on a layer");
        op.description = R::tr("Turns Release on the selected cue's layer on these groups on (or off again): "
                               "leaving the cue it goes back to the base cue, else dark.");
        op.schema = strictSchema({ { QStringLiteral("targets"), namesProperty(R::tr("The layer's groups, or its function")) } },
                                 { QStringLiteral("targets") });
        op.cliSyntax = [](const QJsonObject &a) { return QStringLiteral("release %1").arg(targetsText(a)); };
        op.describe = [](const QJsonObject &a) {
            return R::tr("Toggle Release on the %1 layer").arg(strings(a, "targets").join(QStringLiteral(" + ")));
        };
        add(op);
    }
    for (const bool skip : { true, false })
    {
        Op op;
        op.name = skip ? QStringLiteral("skip_cues") : QStringLiteral("unskip_cues");
        op.title = skip ? R::tr("Skip cues") : R::tr("Put skipped cues back");
        op.description = skip ? R::tr("Skips cues by number (the selected one when none is named): GO passes "
                                      "over them; they track as if they weren't there.")
                              : R::tr("Puts skipped cues back (the selected one when none is named).");
        QJsonObject item = cueNumberProperty(R::tr("A cue number"));
        QJsonObject cues { { QStringLiteral("type"), QStringLiteral("array") }, { QStringLiteral("items"), item },
                           { QStringLiteral("description"), R::tr("Cue numbers; empty: the selected cue") } };
        op.schema = strictSchema({ { QStringLiteral("cues"), cues } });
        const QString word = skip ? QStringLiteral("skip") : QStringLiteral("unskip");
        op.cliSyntax = [word](const QJsonObject &a) {
            const QStringList c = strings(a, "cues");
            return c.isEmpty() ? word : QStringLiteral("%1 %2").arg(word, c.join(QStringLiteral(", ")));
        };
        op.describe = [skip](const QJsonObject &a) {
            const QStringList c = strings(a, "cues");
            const QString which = c.isEmpty() ? R::tr("the selected cue") : R::tr("cue %1").arg(c.join(QStringLiteral(", ")));
            return skip ? R::tr("Skip %1").arg(which) : R::tr("Put %1 back").arg(which);
        };
        add(op);
    }
    {
        Op op;
        op.name = QStringLiteral("record_cue");
        op.title = R::tr("Record as new cue");
        op.description = R::tr("Freezes what is on stage into a new Block cue after the selected one: from "
                               "the selected cue, the list's running cue, or everything lit.");
        op.schema = strictSchema({ { QStringLiteral("source"),
                                     stringProperty(R::tr("What to record"),
                                                    { QStringLiteral("selected"), QStringLiteral("running"), QStringLiteral("lit") }) } },
                                 { QStringLiteral("source") });
        op.cliSyntax = [](const QJsonObject &a) { return QStringLiteral("record %1").arg(arg(a, "source")); };
        op.describe = [](const QJsonObject &a) { return R::tr("Record a new cue from %1").arg(arg(a, "source")); };
        add(op);
    }
    {
        Op op;
        op.name = QStringLiteral("rename_selected");
        op.title = R::tr("Name the selected layer or cue");
        op.description = R::tr("Names the selected layer (else the selected cue); an empty name goes back to "
                               "the automatic one. Tracking is unchanged.");
        op.schema = strictSchema({ { QStringLiteral("name"), stringProperty(R::tr("The new name; \"\": automatic")) } },
                                 { QStringLiteral("name") });
        op.cliSyntax = [](const QJsonObject &a) { return QStringLiteral("name \"%1\"").arg(arg(a, "name")); };
        op.describe = [](const QJsonObject &a) {
            return arg(a, "name").isEmpty() ? R::tr("Give the selection its automatic name back")
                                            : R::tr("Name the selection \"%1\"").arg(arg(a, "name"));
        };
        add(op);
    }
    {
        Op op;
        op.name = QStringLiteral("duplicate_cue");
        op.title = R::tr("Duplicate the selected cue");
        op.description = R::tr("Duplicates the selected cue right after it (numbered for you).");
        op.schema = strictSchema(QJsonObject());
        op.cliSyntax = [](const QJsonObject &) { return QStringLiteral("dup"); };
        op.describe = [](const QJsonObject &) { return R::tr("Duplicate the selected cue"); };
        add(op);
    }
    {
        Op op;
        op.name = QStringLiteral("include_cue");
        op.title = R::tr("Include a cue");
        op.description = R::tr("Includes that cue's output in the selected cue (as if standing in it); again "
                               "takes it out.");
        op.schema = strictSchema({ { QStringLiteral("number"), cueNumberProperty(R::tr("The cue to include")) } },
                                 { QStringLiteral("number") });
        op.cliSyntax = [](const QJsonObject &a) { return QStringLiteral("include %1").arg(arg(a, "number")); };
        op.describe = [](const QJsonObject &a) { return R::tr("Include cue %1 in the selected cue").arg(arg(a, "number")); };
        add(op);
    }
    {
        Op op;
        op.name = QStringLiteral("make_group_set");
        op.title = R::tr("Make a group set");
        op.description = R::tr("One name for several groups, reusable in any cue (Towers = SL + SR); an "
                               "existing set gets these members again.");
        QJsonObject name = stringProperty(R::tr("The set's name"));
        name.insert(QStringLiteral("minLength"), 1);
        op.schema = strictSchema({ { QStringLiteral("name"), name },
                                   { QStringLiteral("groups"), namesProperty(R::tr("Its groups")) } },
                                 { QStringLiteral("name"), QStringLiteral("groups") });
        op.cliSyntax = [](const QJsonObject &a) {
            return QStringLiteral("set \"%1\" %2").arg(arg(a, "name"), targetsText(a, "groups"));
        };
        op.describe = [](const QJsonObject &a) {
            return R::tr("Set \"%1\" = %2").arg(arg(a, "name"), strings(a, "groups").join(QStringLiteral(" + ")));
        };
        add(op);
    }
    {
        Op op;
        op.name = QStringLiteral("play_function");
        op.title = R::tr("Play functions as layers");
        op.description = R::tr("In the selected cue: each function (an EFX, an RGB Matrix, a scene) played as "
                               "it is, a layer each.");
        op.schema = strictSchema({ { QStringLiteral("functions"), namesProperty(R::tr("Functions by name")) } },
                                 { QStringLiteral("functions") });
        op.validate = [](Doc *doc, const QJsonObject &a) {
            for (const QString &name : strings(a, "functions"))
            {
                QString why;
                if (findFunction(doc, name, &why) == nullptr)
                    return why;
            }
            return QString();
        };
        op.cliSyntax = [](const QJsonObject &a) {
            return QStringLiteral("function %1").arg(targetsText(a, "functions", true));
        };
        op.describe = [](const QJsonObject &a) {
            return R::tr("Play %1 in the selected cue").arg(strings(a, "functions").join(QStringLiteral(", ")));
        };
        add(op);
    }
}

/* ---- configuration import (P2b, AI_ASSIST.md section 5b) ---- */

QJsonObject configProperties()
{
    QJsonObject sections { { QStringLiteral("type"), QStringLiteral("array") },
                           { QStringLiteral("items"), stringProperty(OperationRegistry::tr("A section"), ConfigImport::allSections()) },
                           { QStringLiteral("description"),
                             OperationRegistry::tr("What to take: universes (names), outputs, inputs (with their "
                                                   "input profile), feedback. Left out: all of them.") } };
    QJsonObject universes { { QStringLiteral("type"), QStringLiteral("array") },
                            { QStringLiteral("items"), QJsonObject { { QStringLiteral("type"), QStringLiteral("integer") },
                                                                     { QStringLiteral("minimum"), 1 } } },
                            { QStringLiteral("description"),
                              OperationRegistry::tr("Universe numbers in the file (1 is the first). Left out: all.") } };
    auto map = [](const QString &d) {
        return QJsonObject { { QStringLiteral("type"), QStringLiteral("object") }, { QStringLiteral("description"), d } };
    };
    QJsonObject props;
    props.insert(QStringLiteral("file"), stringProperty(OperationRegistry::tr("Path of the show file (.qxw) to take the setup from")));
    props.insert(QStringLiteral("sections"), sections);
    props.insert(QStringLiteral("universes"), universes);
    props.insert(QStringLiteral("universeMap"),
                 map(OperationRegistry::tr("File universe number to universe number here, e.g. {\"3\": \"1\"}")));
    props.insert(QStringLiteral("addressMap"),
                 map(OperationRegistry::tr("Network address in the file to the address here, e.g. {\"2.0.0.50\": \"2.0.0.60\"}")));
    props.insert(QStringLiteral("pluginMap"),
                 map(OperationRegistry::tr("Plugin in the file to the plugin here, for one that isn't installed")));
    props.insert(QStringLiteral("onCollision"),
                 stringProperty(OperationRegistry::tr("When a universe here already has a setup: skip it (keep ours), "
                                                      "replace it, or renumber (put the file's on a new universe)"),
                                { QStringLiteral("skip"), QStringLiteral("replace"), QStringLiteral("renumber") }));
    props.insert(QStringLiteral("onMissingPlugin"),
                 stringProperty(OperationRegistry::tr("When a plugin isn't available here: skip those patches, or "
                                                      "keep them offline"),
                                { QStringLiteral("skip"), QStringLiteral("keep") }));
    return props;
}

QString configValidate(Doc *doc, const QJsonObject &a)
{
    ConfigImport::Options o;
    QString why;
    if (!ConfigImport::optionsFrom(a, &o, &why))
        return why;
    const ConfigImport::Plan p = ConfigImport::plan(doc, o);
    return p.ok() ? QString() : p.problems.join(QStringLiteral("; "));
}

QString intText(const QJsonValue &v)
{
    return v.isDouble() ? QString::number(int(v.toDouble())) : v.toString().trimmed();
}

QString mapText(const QJsonObject &o, const QString &key)
{
    QStringList parts;
    const QJsonObject m = o.value(key).toObject();
    for (auto it = m.constBegin(); it != m.constEnd(); ++it)
        parts << QStringLiteral("%1=%2").arg(it.key(), intText(it.value()));
    return parts.join(QLatin1Char(','));
}

QString joinInts(const QJsonArray &arr)
{
    QStringList parts;
    for (const QJsonValue &v : arr)
        parts << QString::number(int(v.toDouble()));
    return parts.join(QLatin1Char(','));
}

/** The command-line text of an import; "apply" ends it when it applies. */
QString configLine(const QJsonObject &a, bool withApply)
{
    QStringList w { QStringLiteral("import"), QStringLiteral("config"),
                    OperationRegistry::quoted(arg(a, "file")) };
    const QJsonArray secs = a.value(QLatin1String("sections")).toArray();
    if (!secs.isEmpty())
    {
        QStringList names;
        for (const QJsonValue &v : secs)
            names << v.toString();
        w << QStringLiteral("sections") << names.join(QLatin1Char(','));
    }
    if (!a.value(QLatin1String("universes")).toArray().isEmpty())
        w << QStringLiteral("universes") << joinInts(a.value(QLatin1String("universes")).toArray());
    if (!mapText(a, QStringLiteral("universeMap")).isEmpty())
        w << QStringLiteral("map") << mapText(a, QStringLiteral("universeMap"));
    if (!mapText(a, QStringLiteral("addressMap")).isEmpty())
        w << QStringLiteral("address") << mapText(a, QStringLiteral("addressMap"));
    if (!mapText(a, QStringLiteral("pluginMap")).isEmpty())
        w << QStringLiteral("plugin") << OperationRegistry::quoted(mapText(a, QStringLiteral("pluginMap")));
    if (!arg(a, "onCollision").isEmpty())
        w << QStringLiteral("collision") << arg(a, "onCollision");
    if (!arg(a, "onMissingPlugin").isEmpty())
        w << QStringLiteral("missing") << arg(a, "onMissingPlugin");
    if (withApply)
        w << QStringLiteral("apply");
    return w.join(QLatin1Char(' '));
}

/** "a=b,c=d" -> {a: b, c: d}. */
bool parseMap(const QString &text, QJsonObject *into, QString *why)
{
    for (const QString &pair : text.split(QLatin1Char(','), Qt::SkipEmptyParts))
    {
        const int eq = pair.indexOf(QLatin1Char('='));
        if (eq <= 0 || eq == pair.length() - 1)
        {
            *why = OperationRegistry::tr("\"%1\" isn't from=to").arg(pair);
            return false;
        }
        into->insert(pair.left(eq).trimmed(), pair.mid(eq + 1).trimmed());
    }
    return true;
}

bool parseConfigWords(const QStringList &words, QJsonObject *out, QString *why)
{
    if (words.count() < 2 || words.first().compare(QLatin1String("config"), Qt::CaseInsensitive) != 0)
    {
        *why = OperationRegistry::tr("import config <file> [sections a,b] [universes 1,3] [map 3=1] "
                                     "[address 2.0.0.50=2.0.0.60] [plugin a=b] [collision skip|replace|renumber] "
                                     "[missing skip|keep] [apply]");
        return false;
    }
    QJsonObject a;
    a.insert(QStringLiteral("file"), words.at(1));
    bool apply = false;
    for (int i = 2; i < words.count(); ++i)
    {
        const QString k = words.at(i).toLower();
        if (k == QLatin1String("apply"))
        {
            apply = true;
            continue;
        }
        if (i + 1 >= words.count())
        {
            *why = OperationRegistry::tr("\"%1\" needs a value").arg(k);
            return false;
        }
        const QString v = words.at(++i);
        if (k == QLatin1String("sections"))
            a.insert(QStringLiteral("sections"), QJsonArray::fromStringList(v.toLower().split(QLatin1Char(','), Qt::SkipEmptyParts)));
        else if (k == QLatin1String("universes"))
        {
            QJsonArray u;
            for (const QString &n : v.split(QLatin1Char(','), Qt::SkipEmptyParts))
            {
                bool ok = false;
                const int num = n.toInt(&ok);
                if (!ok)
                {
                    *why = OperationRegistry::tr("\"%1\" isn't a universe number").arg(n);
                    return false;
                }
                u << num;
            }
            a.insert(QStringLiteral("universes"), u);
        }
        else if (k == QLatin1String("map") || k == QLatin1String("address") || k == QLatin1String("plugin"))
        {
            const QString key = k == QLatin1String("map") ? QStringLiteral("universeMap")
                              : k == QLatin1String("address") ? QStringLiteral("addressMap") : QStringLiteral("pluginMap");
            QJsonObject m = a.value(key).toObject();
            if (!parseMap(v, &m, why))
                return false;
            a.insert(key, m);
        }
        else if (k == QLatin1String("collision"))
            a.insert(QStringLiteral("onCollision"), v.toLower());
        else if (k == QLatin1String("missing"))
            a.insert(QStringLiteral("onMissingPlugin"), v.toLower());
        else
        {
            *why = OperationRegistry::tr("\"%1\" isn't part of import config").arg(k);
            return false;
        }
    }
    // Typed without "apply": the command line shows what it would do.
    a.insert(QStringLiteral("apply"), apply);
    *out = a;
    return true;
}

QString configDescribe(const QJsonObject &a)
{
    QStringList bits;
    const QString file = QFileInfo(arg(a, "file")).fileName();
    const QJsonObject um = a.value(QLatin1String("universeMap")).toObject();
    for (auto it = um.constBegin(); it != um.constEnd(); ++it)
        bits << OperationRegistry::tr("universe %1 to %2").arg(it.key(), intText(it.value()));
    const QJsonObject am = a.value(QLatin1String("addressMap")).toObject();
    for (auto it = am.constBegin(); it != am.constEnd(); ++it)
        bits << QStringLiteral("%1 → %2").arg(it.key(), intText(it.value()));
    if (!arg(a, "onCollision").isEmpty())
        bits << OperationRegistry::tr("collisions: %1").arg(arg(a, "onCollision"));
    return bits.isEmpty() ? OperationRegistry::tr("Import the universes and I/O setup from %1").arg(file)
                          : OperationRegistry::tr("Import the universes and I/O setup from %1 (%2)")
                                .arg(file, bits.join(QStringLiteral(", ")));
}

void appendConfigOperations(QList<OperationRegistry::Operation> &ops, bool readOnly)
{
    typedef OperationRegistry R;
    typedef OperationRegistry::Operation Op;
    if (readOnly)
    {
        {
            Op op;
            op.name = QStringLiteral("read_config");
            op.title = R::tr("Read a show file's universes and I/O setup");
            op.description = R::tr("Reads another show file (.qxw) and lists its universes and their output, "
                                   "input and feedback patches (plugin, line, target address, network, input "
                                   "profile), and how many fixtures, groups and palettes it has. Changes nothing.");
            op.schema = strictSchema({ { QStringLiteral("file"), stringProperty(R::tr("Path of the show file (.qxw)")) } },
                                     { QStringLiteral("file") });
            op.cliWord = QStringLiteral("config");
            op.cliHelp = QStringLiteral("config <file>");
            op.validate = [](Doc *, const QJsonObject &a) {
                return arg(a, "file").isEmpty() ? R::tr("No file") : QString();
            };
            op.parse = [](const QStringList &w, QJsonObject *a, QString *why) {
                if (w.count() != 1)
                {
                    *why = R::tr("config <file>");
                    return false;
                }
                a->insert(QStringLiteral("file"), w.first());
                return true;
            };
            op.execute = [](R::Context &, const QJsonObject &a) {
                const ConfigImport::Source src = ConfigImport::readSource(arg(a, "file"));
                if (!src.ok)
                    return refusal(src.error);
                return answer(R::tr("%n universe(s) in %1", "", src.universes.count()).arg(QFileInfo(arg(a, "file")).fileName()),
                              ConfigImport::sourceText(src));
            };
            op.cliSyntax = [](const QJsonObject &a) { return QStringLiteral("config %1").arg(R::quoted(arg(a, "file"))); };
            op.describe = [](const QJsonObject &a) { return R::tr("Read the setup in %1").arg(QFileInfo(arg(a, "file")).fileName()); };
            ops << op;
        }
        {
            Op op;
            op.name = QStringLiteral("preview_config_import");
            op.title = R::tr("Preview a configuration import");
            op.description = R::tr("What import_config would do with the same arguments: the problems that would "
                                   "stop it (a universe collision or a missing plugin that needs a choice), the "
                                   "route warnings, and the current-to-resulting changes. Changes nothing.");
            op.schema = strictSchema(configProperties(), { QStringLiteral("file") });
            op.validate = [](Doc *, const QJsonObject &a) {
                return arg(a, "file").isEmpty() ? R::tr("No file") : QString();
            };
            op.execute = [](R::Context &ctx, const QJsonObject &a) {
                ConfigImport::Options o;
                QString why;
                if (!ConfigImport::optionsFrom(a, &o, &why))
                    return refusal(why);
                const ConfigImport::Plan p = ConfigImport::plan(ctx.doc, o);
                return answer(p.ok() ? R::tr("Preview: nothing is changed") : R::tr("Preview: it needs choices"), p.text());
            };
            op.cliSyntax = [](const QJsonObject &a) { return configLine(a, false); };
            op.describe = [](const QJsonObject &a) { return R::tr("Preview: %1").arg(configDescribe(a)); };
            ops << op;
        }
        return;
    }

    Op op;
    op.name = QStringLiteral("import_config");
    op.title = R::tr("Import universes and I/O setup from a show file");
    op.description = R::tr("Takes universes (names), output, input (with input profile) and feedback patches "
                           "from another show file (.qxw), with a mapping (universes, network addresses, "
                           "plugins). Patches keep their route requirement. Check with preview_config_import "
                           "first: a universe that already has a setup needs onCollision, a plugin that isn't "
                           "installed needs onMissingPlugin. Not fixtures, groups or palettes.");
    QJsonObject props = configProperties();
    props.insert(QStringLiteral("apply"),
                 QJsonObject { { QStringLiteral("type"), QStringLiteral("boolean") },
                               { QStringLiteral("description"), R::tr("false: only show what it would do") } });
    op.schema = strictSchema(props, { QStringLiteral("file") });
    op.readOnly = false;
    op.cliWord = QStringLiteral("import");
    op.cliHelp = QStringLiteral("import config <file> ... apply");
    op.validate = [](Doc *doc, const QJsonObject &a) {
        if (doc->showTree() == nullptr)
            return R::tr("The show has no Show Tree");
        if (a.contains(QLatin1String("apply")) && !a.value(QLatin1String("apply")).toBool(true))
            return QString();       // a preview: nothing is refused for its choices
        return configValidate(doc, a);
    };
    op.parse = parseConfigWords;
    op.execute = [](R::Context &ctx, const QJsonObject &a) {
        ConfigImport::Options o;
        QString why;
        if (!ConfigImport::optionsFrom(a, &o, &why))
            return refusal(why);
        if (a.contains(QLatin1String("apply")) && !a.value(QLatin1String("apply")).toBool(true))
        {
            const ConfigImport::Plan p = ConfigImport::plan(ctx.doc, o);
            R::Result r = answer(p.ok() ? R::tr("Preview (add \"apply\" to make it)") : R::tr("Preview: it needs choices"),
                                 p.text());
            r.ok = true;
            return r;
        }
        ShowTreeController *st = ctx.doc->showTree();
        if (st == nullptr)
            return refusal(R::tr("The show has no Show Tree"));
        const bool own = !st->inBatch();
        const QString label = R::tr("Import configuration from %1").arg(QFileInfo(o.file).fileName());
        if (own)
            st->beginBatch(label);
        const ConfigImport::Applied ap = ConfigImport::apply(ctx.doc, o);
        if (!ap.ok)
        {
            if (own)
                st->endBatch(false);
            return refusal(ap.message);
        }
        st->noteChangeOutsideTree();
        if (own)
        {
            st->endBatch(true);
            st->attachToLastStep(ap.undo);
        }
        else if (ctx.outside != nullptr)
        {
            ctx.outside->append(ap.undo);
        }
        R::Result r = answer(R::tr("✓ %1 (one undo step)").arg(ap.message), ap.report.join(QLatin1Char('\n')));
        r.ok = true;
        r.command.ok = true;
        r.command.context = ctx.selection;
        r.command.keepSelection = true;
        return r;
    };
    op.cliSyntax = [](const QJsonObject &a) {
        return configLine(a, !a.contains(QLatin1String("apply")) || a.value(QLatin1String("apply")).toBool(true));
    };
    op.describe = configDescribe;
    ops << op;
}


/* ---- playing words (0.4 leftovers): a group master, an override hit ---- */

/** The group @a name names that has a master; null and @a why when none or
 *  several do. */
FixtureGroup *findMasteredGroup(Doc *doc, const QString &name, QString *why)
{
    const QString key = name.trimmed();
    QList<FixtureGroup *> hits;
    FixtureGroup *plain = nullptr;
    for (FixtureGroup *g : doc->fixtureGroups())
        if (g->name().compare(key, Qt::CaseInsensitive) == 0)
        {
            if (g->hasMaster())
                hits << g;
            else
                plain = g;
        }
    if (hits.count() == 1)
        return hits.first();
    if (hits.count() > 1)
        *why = OperationRegistry::tr("Several groups called \"%1\" have a master").arg(key);
    else if (plain != nullptr)
        *why = OperationRegistry::tr("\"%1\" has no master (Fixture Manager > the group > Group master)").arg(key);
    else
        *why = OperationRegistry::tr("No group called \"%1\"").arg(key);
    return nullptr;
}

bool parseMasterWords(const QStringList &words, QJsonObject *out, QString *why)
{
    if (words.count() < 2)
    {
        *why = OperationRegistry::tr("master <group> <0-100>[%] | full | off");
        return false;
    }
    QString level = words.last().toLower();
    int percent = 0;
    if (level == QLatin1String("full"))
        percent = 100;
    else if (level == QLatin1String("off"))
        percent = 0;
    else
    {
        if (level.endsWith(QLatin1Char('%')))
            level.chop(1);
        bool ok = false;
        percent = level.toInt(&ok);
        if (!ok)
        {
            *why = OperationRegistry::tr("\"%1\" isn't a level (0-100, full or off)").arg(words.last());
            return false;
        }
    }
    out->insert(QStringLiteral("group"), words.mid(0, words.count() - 1).join(QLatin1Char(' ')));
    out->insert(QStringLiteral("level"), percent);
    return true;
}

void appendLiveOperations(QList<OperationRegistry::Operation> &ops)
{
    typedef OperationRegistry R;
    typedef OperationRegistry::Operation Op;
    {
        Op op;
        op.name = QStringLiteral("set_group_master");
        op.title = R::tr("Set a group master");
        op.description = R::tr("Moves a fixture group's master (a fader that scales the intensity of its heads) "
                               "to a percentage. Playing, not editing: it works in Operate and has no undo step.");
        QJsonObject level { { QStringLiteral("type"), QStringLiteral("integer") },
                            { QStringLiteral("minimum"), 0 }, { QStringLiteral("maximum"), 100 },
                            { QStringLiteral("description"), R::tr("Percent, 0 to 100") } };
        op.schema = strictSchema({ { QStringLiteral("group"), stringProperty(R::tr("The fixture group's name")) },
                                   { QStringLiteral("level"), level } },
                                 { QStringLiteral("group"), QStringLiteral("level") });
        op.readOnly = false;
        op.live = true;
        op.cliWord = QStringLiteral("master");
        op.cliHelp = QStringLiteral("master <group> <0-100>|full|off");
        op.parse = parseMasterWords;
        op.validate = [](Doc *doc, const QJsonObject &a) {
            QString why;
            findMasteredGroup(doc, arg(a, "group"), &why);
            return why;
        };
        op.execute = [](R::Context &ctx, const QJsonObject &a) {
            QString why;
            FixtureGroup *g = findMasteredGroup(ctx.doc, arg(a, "group"), &why);
            if (g == nullptr)
                return refusal(why);
            const int percent = a.value(QLatin1String("level")).toInt();
            g->setMasterLevel(uchar(qRound(percent * double(UCHAR_MAX) / 100.0)));
            R::Result r;
            r.ok = true;
            r.message = R::tr("%1 master at %2%").arg(g->name()).arg(percent);
            r.command.ok = true;
            r.command.context = ctx.selection;
            r.command.keepSelection = true;
            return r;
        };
        op.cliSyntax = [](const QJsonObject &a) {
            return QStringLiteral("master %1 %2").arg(R::quoted(arg(a, "group"))).arg(a.value(QLatin1String("level")).toInt());
        };
        op.describe = [](const QJsonObject &a) {
            return R::tr("Move the %1 master to %2%").arg(arg(a, "group")).arg(a.value(QLatin1String("level")).toInt());
        };
        ops << op;
    }
    {
        Op op;
        op.name = QStringLiteral("set_hit_override");
        op.title = R::tr("Make the selected hit an override");
        op.description = R::tr("Makes the selected hit an override hit (it wins over everything the show plays "
                               "on its channels, intensity too), or a plain hit again.");
        op.schema = strictSchema({ { QStringLiteral("on"), QJsonObject { { QStringLiteral("type"), QStringLiteral("boolean") },
                                                                          { QStringLiteral("description"), R::tr("false: a plain hit again") } } } });
        op.readOnly = false;
        op.cliWord = QStringLiteral("override");
        op.cliHelp = QStringLiteral("override [on|off]");
        op.parse = [](const QStringList &words, QJsonObject *out, QString *why) {
            if (words.count() > 1)
            {
                *why = R::tr("override [on|off]");
                return false;
            }
            if (words.isEmpty() || words.first().compare(QLatin1String("on"), Qt::CaseInsensitive) == 0)
                return true;
            if (words.first().compare(QLatin1String("off"), Qt::CaseInsensitive) == 0)
            {
                out->insert(QStringLiteral("on"), false);
                return true;
            }
            *why = R::tr("override [on|off]");
            return false;
        };
        op.validate = [](Doc *doc, const QJsonObject &) {
            return doc->showTree() == nullptr ? R::tr("The show has no Show Tree") : QString();
        };
        op.execute = [](R::Context &ctx, const QJsonObject &a) {
            ShowTreeController *st = ctx.doc->showTree();
            if (st == nullptr)
                return refusal(R::tr("The show has no Show Tree"));
            if (ctx.doc->mode() == Doc::Operate)
                return refusal(R::tr("Changes can't be made in Operate mode"));
            const quint32 id = ctx.selection.cue;
            if (id == kInvalid || st->cue(id) == nullptr || !st->isLook(id))
                return refusal(R::tr("Select a hit first (a cue of a hits list)"));
            const bool on = a.value(QLatin1String("on")).toBool(true);
            R::Result r;
            r.command.context = ctx.selection;
            r.command.keepSelection = true;
            if (st->isOverrideHit(id) == on)
            {
                r.ok = r.command.ok = true;
                r.message = on ? R::tr("already an override hit") : R::tr("already a plain hit");
                return r;
            }
            if (!st->setHitsOverride({ id }, on))
                return refusal(R::tr("Nothing changed"));
            r.ok = r.command.ok = true;
            r.message = on ? R::tr("✓ override hit: wins over everything the show plays")
                           : R::tr("✓ plain hit again");
            r.command.message = r.message;
            return r;
        };
        op.cliSyntax = [](const QJsonObject &a) {
            return a.value(QLatin1String("on")).toBool(true) ? QStringLiteral("override") : QStringLiteral("override off");
        };
        op.describe = [](const QJsonObject &a) {
            return a.value(QLatin1String("on")).toBool(true) ? R::tr("Make the selected hit an override hit")
                                                              : R::tr("Make the selected hit a plain hit");
        };
        ops << op;
    }
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
            const bool checked = vcChecked(ctx.doc);
            if (!checked)
                lines << R::tr("(Virtual Console widgets weren't checked.)");
            return answer(R::tr("%n unused function(s)", "", lines.count() - (checked ? 0 : 1)),
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

    appendConfigOperations(ops, true);
    appendChangeOperations(ops);
    appendConfigOperations(ops, false);
    appendLiveOperations(ops);
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

QList<const OperationRegistry::Operation *> OperationRegistry::changeOperations()
{
    QList<const Operation *> out;
    for (const Operation &op : catalog())
        if (!op.readOnly && !op.live && op.name != QLatin1String("command"))
            out << &op;
    return out;
}

namespace
{
QString typeName(const QJsonValue &v)
{
    switch (v.type())
    {
        case QJsonValue::Bool: return QStringLiteral("boolean");
        case QJsonValue::Double: return QStringLiteral("number");
        case QJsonValue::String: return QStringLiteral("string");
        case QJsonValue::Array: return QStringLiteral("array");
        case QJsonValue::Object: return QStringLiteral("object");
        default: return QStringLiteral("null");
    }
}

bool typeFits(const QString &type, const QJsonValue &v)
{
    if (type == QLatin1String("integer"))
        return v.isDouble() && v.toDouble() == double(qint64(v.toDouble()));
    if (type == QLatin1String("number"))
        return v.isDouble();
    return typeName(v) == type;
}

QString checkValue(const QString &where, const QJsonObject &schema, const QJsonValue &v)
{
    typedef OperationRegistry R;
    const QJsonValue type = schema.value(QLatin1String("type"));
    if (!type.isUndefined())
    {
        QStringList types;
        if (type.isArray())
            for (const QJsonValue &t : type.toArray())
                types << t.toString();
        else
            types << type.toString();
        bool fits = false;
        for (const QString &t : types)
            fits = fits || typeFits(t, v);
        if (!fits)
            return R::tr("%1: wants %2, not %3").arg(where, types.join(QStringLiteral(" or ")), typeName(v));
    }
    if (schema.contains(QLatin1String("enum")))
    {
        const QJsonArray choices = schema.value(QLatin1String("enum")).toArray();
        if (!choices.contains(v))
        {
            QStringList c;
            for (const QJsonValue &e : choices)
                c << e.toString();
            return R::tr("%1: one of %2").arg(where, c.join(QStringLiteral(", ")));
        }
    }
    if (v.isDouble())
    {
        if (schema.contains(QLatin1String("minimum")) && v.toDouble() < schema.value(QLatin1String("minimum")).toDouble())
            return R::tr("%1: %2 is below %3").arg(where).arg(v.toDouble()).arg(schema.value(QLatin1String("minimum")).toDouble());
        if (schema.contains(QLatin1String("maximum")) && v.toDouble() > schema.value(QLatin1String("maximum")).toDouble())
            return R::tr("%1: %2 is above %3").arg(where).arg(v.toDouble()).arg(schema.value(QLatin1String("maximum")).toDouble());
    }
    if (v.isString())
    {
        const QString s = v.toString();
        if (schema.contains(QLatin1String("minLength")) && s.trimmed().length() < schema.value(QLatin1String("minLength")).toInt())
            return R::tr("%1: can't be empty").arg(where);
        if (schema.contains(QLatin1String("pattern"))
            && !QRegularExpression(schema.value(QLatin1String("pattern")).toString()).match(s).hasMatch())
            return R::tr("%1: \"%2\" isn't in the right form").arg(where, s);
    }
    if (v.isArray())
    {
        const QJsonArray a = v.toArray();
        if (schema.contains(QLatin1String("minItems")) && a.count() < schema.value(QLatin1String("minItems")).toInt())
            return R::tr("%1: needs at least %2").arg(where).arg(schema.value(QLatin1String("minItems")).toInt());
        if (schema.contains(QLatin1String("maxItems")) && a.count() > schema.value(QLatin1String("maxItems")).toInt())
            return R::tr("%1: at most %2").arg(where).arg(schema.value(QLatin1String("maxItems")).toInt());
        const QJsonObject items = schema.value(QLatin1String("items")).toObject();
        if (!items.isEmpty())
            for (int i = 0; i < a.count(); ++i)
            {
                const QString why = checkValue(QStringLiteral("%1[%2]").arg(where).arg(i), items, a.at(i));
                if (!why.isEmpty())
                    return why;
            }
    }
    if (v.isObject())
    {
        const QJsonObject o = v.toObject();
        const QJsonObject props = schema.value(QLatin1String("properties")).toObject();
        const QString prefix = where.isEmpty() ? QString() : where + QLatin1Char('.');
        for (const QJsonValue &r : schema.value(QLatin1String("required")).toArray())
        {
            const QJsonValue got = o.value(r.toString());
            if (got.isUndefined() || got.isNull())
                return R::tr("%1%2: missing").arg(prefix, r.toString());
        }
        const bool strict = schema.value(QLatin1String("additionalProperties")) == QJsonValue(false);
        for (auto it = o.constBegin(); it != o.constEnd(); ++it)
        {
            if (it.value().isNull())
                continue;
            if (!props.contains(it.key()))
            {
                if (strict)
                    return R::tr("%1%2: not an argument of this operation").arg(prefix, it.key());
                continue;
            }
            const QString why = checkValue(prefix + it.key(), props.value(it.key()).toObject(), it.value());
            if (!why.isEmpty())
                return why;
        }
    }
    return QString();
}
} // namespace

QString OperationRegistry::checkArgs(const QJsonObject &schema, const QJsonObject &args)
{
    return checkValue(QString(), schema, args);
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
    const QString why = check(name, args);
    if (!why.isEmpty())
        return refusal(why);
    const Operation *op = operation(name);
    Context ctx;
    ctx.doc = m_doc;
    ctx.commander = m_commander;
    ctx.selection = selection;
    ctx.outside = &m_outside;
    return op->execute(ctx, args);
}

QString OperationRegistry::check(const QString &name, const QJsonObject &args) const
{
    const Operation *op = operation(name);
    if (op == nullptr)
        return tr("No operation \"%1\"").arg(name);
    const QString bad = checkArgs(op->schema, args);
    if (!bad.isEmpty())
        return bad;
    return op->validate ? op->validate(m_doc, args) : QString();
}

OperationRegistry::Result OperationRegistry::runPlan(const QList<Call> &calls,
                                                     const ShowTreeCommand::Context &selection,
                                                     const QString &label)
{
    if (calls.isEmpty())
        return refusal(tr("Nothing to do"));
    if (m_doc->mode() == Doc::Operate)
        return refusal(tr("Changes can't be made in Operate mode"));
    if (m_commander == nullptr || m_doc->showTree() == nullptr)
        return refusal(tr("The command line isn't open here"));
    // Everything checked before anything runs.
    for (int i = 0; i < calls.count(); ++i)
    {
        const Operation *op = operation(calls.at(i).name);
        QString why = check(calls.at(i).name, calls.at(i).args);
        if (why.isEmpty() && (op->readOnly || op->live || op->name == QLatin1String("command")))
            why = tr("not a change operation");
        if (!why.isEmpty())
            return refusal(tr("Step %1 (%2): %3. Nothing changed.").arg(i + 1).arg(calls.at(i).name, why));
    }

    ShowTreeController *st = m_doc->showTree();
    m_outside.clear();
    // Undo what steps did outside the Show Tree (a configuration import).
    auto putOutsideBack = [this]() {
        for (int i = m_outside.count() - 1; i >= 0; --i)
            m_outside.at(i)(true);
        m_outside.clear();
    };
    st->beginBatch(label);
    ShowTreeCommand::Context at = selection;
    Result last;
    for (int i = 0; i < calls.count(); ++i)
    {
        last = run(calls.at(i).name, calls.at(i).args, at);
        if (!last.ok)
        {
            st->endBatch(false);
            putOutsideBack();
            return refusal(tr("Step %1 (%2): %3. Nothing changed.")
                               .arg(i + 1).arg(operation(calls.at(i).name)->cliSyntax(calls.at(i).args), last.message));
        }
        // Where the step leaves the selection, as the view would: its cue,
        // and the layer it set (what a following `rename_selected` names).
        const quint32 cueBefore = at.cue;
        at = last.command.context;
        if (last.command.layer != 0xFFFFFFFF)
            at.layer = last.command.layer;
        else if (at.cue != cueBefore)
            at.layer = 0xFFFFFFFF;
    }
    st->endBatch(true);
    for (const std::function<void(bool)> &undo : m_outside)
        st->attachToLastStep(undo);
    m_outside.clear();
    last.ok = true;
    last.command.ok = true;
    last.command.context = at;
    last.message = tr("✓ %1 (one undo step)").arg(label);
    last.command.message = last.message;
    return last;
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
