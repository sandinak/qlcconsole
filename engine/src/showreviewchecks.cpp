/*
  Q Light Controller Plus - qlcconsole
  showreviewchecks.cpp

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

/* Show Review's checks over the patch, fixtures, effects, power, files and
   names (docs/design/SHOW_REVIEW.md section 3). Each is a pure read of the
   Doc. Where the footer's readiness list already finds a problem, the check
   reads the same Doc queries (one source of truth) and the footer entry is
   shown under the check's id (ShowReview::footerCheckId()). */

#include <QCoreApplication>
#include <QRegularExpression>
#include <QSettings>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QMap>
#include <QVector>
#include <algorithm>

#include "showreviewcontext.h"
#include "showtreecontroller.h"
#include "programmercontroller.h"
#include "powerdistribution.h"
#include "monitorproperties.h"
#include "fixturedefcheck.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "focuscalls.h"
#include "collection.h"
#include "fixture.h"
#include "scene.h"
#include "video.h"
#include "audio.h"
#include "doc.h"

static QString tr(const char *text)
{
    return QCoreApplication::translate("ShowReview", text);
}

static QString trn(const char *text, int n)
{
    return QCoreApplication::translate("ShowReview", text, nullptr, n);
}

/** "Spot 1, Spot 2, Spot 3 and 4 more" */
static QString nameList(ReviewContext &ctx, QList<quint32> ids, int max = 4)
{
    QStringList names;
    for (int i = 0; i < ids.count() && i < max; ++i)
        names << ctx.fixtureName(ids.at(i));
    QString s = names.join(QStringLiteral(", "));
    if (ids.count() > max)
        s = trn("%1 and %n more", ids.count() - max).arg(s);
    return s;
}

static QStringList fixtureKeys(const QList<quint32> &ids)
{
    QStringList keys;
    for (quint32 id : ids)
        keys << QStringLiteral("fx:%1").arg(id);
    return keys;
}

static QVariantList variantIds(const QList<quint32> &ids)
{
    QVariantList v;
    for (quint32 id : ids)
        v << id;
    return v;
}

static QByteArray idBytes(QList<quint32> ids)
{
    std::sort(ids.begin(), ids.end());
    QByteArray b;
    for (quint32 id : ids)
        b += QByteArray::number(id) + ',';
    return b;
}

static QString address(const Fixture *fx)
{
    return QStringLiteral("%1/%2–%3").arg(fx->universe() + 1).arg(fx->address() + 1)
            .arg(fx->address() + fx->channels());
}

/** The subject a generated function stands for in words, and its keys:
 *  a Show Tree cue, else the function itself. */
static QString functionSubject(ReviewContext &ctx, quint32 fid, QStringList *keys, QString *jump,
                               QVariantList *jumpData)
{
    ShowTreeController *st = ctx.doc()->showTree();
    quint32 listId = ShowTree::invalidId(), cueId = ShowTree::invalidId();
    if (st->generatedNode(fid, &listId, &cueId) && cueId != ShowTree::invalidId())
    {
        const ShowTree::CueList *list = st->cueList(listId);
        if (list != nullptr)
        {
            *keys << QStringLiteral("list:%1/cue:%2").arg(listId).arg(cueId);
            *jump = QStringLiteral("cue");
            *jumpData = QVariantList() << cueId;
            const int idx = list->indexOf(cueId);
            return list->looks ? tr("hit %1").arg(st->cueLabel(*list, idx)) : ctx.cueText(*list, idx);
        }
    }
    *keys << QStringLiteral("fn:%1").arg(fid);
    *jump = QStringLiteral("function");
    *jumpData = QVariantList() << fid;
    return QStringLiteral("'%1'").arg(ctx.functionName(fid));
}

/****************************************************************************
 * Patch and outputs (PAT)
 ****************************************************************************/

static void pat01(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    Doc *doc = ctx.doc();
    QMap<quint32, QList<Fixture *>> byUniverse;
    for (Fixture *fx : doc->fixtures())
        if (fx != nullptr && fx->isPatched())
            byUniverse[fx->universe()] << fx;
    for (auto it = byUniverse.begin(); it != byUniverse.end(); ++it)
    {
        QList<Fixture *> &fxs = it.value();
        std::sort(fxs.begin(), fxs.end(), [](Fixture *a, Fixture *b) {
            return a->address() != b->address() ? a->address() < b->address() : a->id() < b->id();
        });
        for (int i = 0; i < fxs.count(); ++i)
        {
            for (int j = i + 1; j < fxs.count(); ++j)
            {
                Fixture *a = fxs.at(i), *b = fxs.at(j);
                if (b->address() >= a->address() + a->channels())
                    break;
                const quint32 from = b->address() + 1;
                const quint32 to = qMin(a->address() + a->channels(), b->address() + b->channels());
                QByteArray fp = QByteArray::number(a->id()) + ':' + address(a).toUtf8() + ';'
                                + QByteArray::number(b->id()) + ':' + address(b).toUtf8();
                out << ReviewContext::make(QStringLiteral("PAT-01"), ReviewFinding::Blocker,
                    tr("%1 (%2) and %3 (%4) share channels %5–%6.")
                        .arg(a->name(), address(a), b->name(), address(b)).arg(from).arg(to),
                    fixtureKeys({ a->id(), b->id() }), fp,
                    QStringLiteral("status:fixtures.clash"), variantIds({ a->id(), b->id() }));
            }
        }
    }
    // Fixtures the file put where another already was: loaded unpatched.
    for (quint32 fid : doc->addressClashes())
    {
        Fixture *fx = doc->fixture(fid);
        if (fx == nullptr)
            continue;
        out << ReviewContext::make(QStringLiteral("PAT-01"), ReviewFinding::Blocker,
            tr("%1 wants universe %2, address %3, which another fixture has: it loaded unpatched "
               "and sends nothing.").arg(fx->name()).arg(fx->universe() + 1).arg(fx->address() + 1),
            fixtureKeys({ fid }), "clash:" + address(fx).toUtf8(),
            QStringLiteral("status:fixtures.clash"), variantIds({ fid }));
    }
}

static void pat02(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    for (Fixture *fx : ctx.doc()->fixtures())
    {
        if (fx == nullptr || !fx->isPatched() || fx->address() + fx->channels() <= 512)
            continue;
        out << ReviewContext::make(QStringLiteral("PAT-02"), ReviewFinding::Blocker,
            tr("%1 starts at %2/%3 and needs %4 channels: it runs past the end of the universe.")
                .arg(fx->name()).arg(fx->universe() + 1).arg(fx->address() + 1).arg(fx->channels()),
            fixtureKeys({ fx->id() }), address(fx).toUtf8(),
            QStringLiteral("status:fixtures.clash"), variantIds({ fx->id() }));
    }
}

static void pat03(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    QList<quint32> used, unused;
    const QSet<quint32> &inUse = ctx.usedFixtures();
    for (Fixture *fx : ctx.doc()->fixtures())
    {
        // A clash is PAT-01's, not merely unpatched (as in the footer).
        if (fx == nullptr || fx->isPatched() || fx->addressClash())
            continue;
        (inUse.contains(fx->id()) ? used : unused) << fx->id();
    }
    if (!used.isEmpty())
        out << ReviewContext::make(QStringLiteral("PAT-03"), ReviewFinding::Warning,
            trn("%n fixture(s) the show lights aren't patched: %1. They send nothing.", used.count())
                .arg(nameList(ctx, used)),
            fixtureKeys(used), idBytes(used), QStringLiteral("status:fixtures.unpatched"), variantIds(used));
    if (!unused.isEmpty())
        out << ReviewContext::make(QStringLiteral("PAT-03"), ReviewFinding::Note,
            trn("%n fixture(s) in the inventory, unpatched and unused: %1.", unused.count())
                .arg(nameList(ctx, unused)),
            fixtureKeys(unused), idBytes(unused), QStringLiteral("status:fixtures.unpatched"), variantIds(unused));
}

static void pat04(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    Doc *doc = ctx.doc();
    for (Fixture *fx : doc->fixtures())
    {
        quint32 first = 0;
        if (fx == nullptr || !fx->isPatched()
            || !doc->isOnNotOurs(fx->universe(), fx->address(), fx->channels(), &first))
            continue;
        out << ReviewContext::make(QStringLiteral("PAT-04"), ReviewFinding::Blocker,
            tr("%1 is on %2, which the venue keeps (not ours) from %3/%4.")
                .arg(fx->name(), address(fx)).arg(fx->universe() + 1).arg(first + 1),
            fixtureKeys({ fx->id() }), address(fx).toUtf8() + ':' + QByteArray::number(first),
            QStringLiteral("status:fixtures.clash"), variantIds({ fx->id() }));
    }
}

static void pat06(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    QMap<quint32, int> used;
    for (Fixture *fx : ctx.doc()->fixtures())
        if (fx != nullptr && fx->isPatched())
            used[fx->universe()]++;
    const bool any = ctx.anyOutput();
    QStringList preview;
    QList<quint32> previewUnis;
    for (auto it = used.constBegin(); it != used.constEnd(); ++it)
    {
        if (ctx.universeHasOutput(it.key()))
            continue;
        if (!any)
        {
            preview << QString::number(it.key() + 1);
            previewUnis << it.key();
            continue;
        }
        out << ReviewContext::make(QStringLiteral("PAT-06"), ReviewFinding::Warning,
            trn("Universe %1 has %n fixture(s) and goes nowhere: it has no output.", it.value())
                .arg(it.key() + 1),
            { QStringLiteral("uni:%1").arg(it.key()) }, QByteArray::number(it.value()),
            QStringLiteral("status:universes.nooutput"), QVariantList() << it.key());
    }
    if (!preview.isEmpty())
        out << ReviewContext::make(QStringLiteral("PAT-06"), ReviewFinding::Note,
            tr("The show patches no output anywhere (universes %1): it only previews. Patch "
               "outputs in Connections before it runs a rig.").arg(preview.join(QStringLiteral(", "))),
            { QStringLiteral("uni:none") }, preview.join(QLatin1Char(',')).toUtf8(),
            QStringLiteral("status:universes.nooutput"), QVariantList() << previewUnis.first());
}

/****************************************************************************
 * Fixtures and definitions (FIX)
 ****************************************************************************/

static void fix01(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    QMap<QString, QList<quint32>> byModel;
    for (Fixture *fx : ctx.doc()->fixtures())
        if (fx != nullptr && fx->definitionUnavailable())
            byModel[fx->unavailableManufacturer() + QLatin1Char(' ') + fx->unavailableModel()] << fx->id();
    for (auto it = byModel.constBegin(); it != byModel.constEnd(); ++it)
        out << ReviewContext::make(QStringLiteral("FIX-01"), ReviewFinding::Blocker,
            trn("No definition for '%1' (%n fixture(s): %2). They stand in as dimmers: install the "
                "definition, or open the show where it is and save it as .qlcc to carry it.",
                it.value().count()).arg(it.key(), nameList(ctx, it.value())),
            fixtureKeys(it.value()), it.key().toUtf8() + ':' + idBytes(it.value()),
            QStringLiteral("status:fixtures.nodefinition"), variantIds(it.value()));
}

static void fix05(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    Doc *doc = ctx.doc();
    MonitorProperties *mp = doc->monitorProperties();
    // A show that doesn't use the plot at all isn't asked to.
    if (mp == nullptr || mp->fixtureItemsID().isEmpty())
        return;
    QSet<quint32> needsPlace;
    if (doc->programmer() != nullptr && doc->programmer()->focusCalls() != nullptr)
        for (const FocusCall &call : doc->programmer()->focusCalls()->calls())
            for (quint32 fid : FocusCalls::fixturesOf(doc, call))
                needsPlace.insert(fid);
    QList<quint32> warn, note;
    for (Fixture *fx : doc->fixtures())
    {
        if (fx == nullptr || !fx->isPatched() || doc->isVenueFixture(fx->id()) || mp->containsFixture(fx->id()))
            continue;
        (needsPlace.contains(fx->id()) ? warn : note) << fx->id();
    }
    if (!warn.isEmpty())
        out << ReviewContext::make(QStringLiteral("FIX-05"), ReviewFinding::Warning,
            trn("%n fixture(s) in focus calls aren't on the plot, so focus can't aim them: %1.", warn.count())
                .arg(nameList(ctx, warn)),
            fixtureKeys(warn), idBytes(warn), QStringLiteral("plot"), variantIds(warn));
    if (!note.isEmpty())
        out << ReviewContext::make(QStringLiteral("FIX-05"), ReviewFinding::Note,
            trn("%n patched fixture(s) aren't on the plot: %1.", note.count()).arg(nameList(ctx, note)),
            fixtureKeys(note), idBytes(note), QStringLiteral("plot"), variantIds(note));
}

static void fix07(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    QMap<QString, QList<quint32>> silent;
    QHash<QString, QString> files;
    for (Fixture *fx : ctx.doc()->fixtures())
    {
        if (fx == nullptr)
            continue;
        const QLCFixtureMode *mode = fx->fixtureMode();
        if (mode == nullptr || mode->physical().declaresOutput() || !FixtureDefCheck::expectsLight(mode))
            continue;
        const QLCFixtureDef *def = fx->fixtureDef();
        const QString type = def != nullptr
            ? QStringLiteral("%1 %2 (%3)").arg(def->manufacturer(), def->model(), mode->name())
            : mode->name();
        silent[type] << fx->id();
        files.insert(type, def != nullptr ? def->definitionSourceFile() : QString());
    }
    for (auto it = silent.constBegin(); it != silent.constEnd(); ++it)
        out << ReviewContext::make(QStringLiteral("FIX-07"), ReviewFinding::Note,
            trn("'%1' gives no lumens or power (%n fixture(s)): the plot and the power check guess.",
                it.value().count()).arg(it.key()),
            fixtureKeys(it.value()), it.key().toUtf8(),
            QStringLiteral("status:fixtures.output"), QVariantList() << files.value(it.key()));
}

static void fix14(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    const QMap<QString, QList<quint32>> dups = ctx.doc()->duplicateFixtureNames();
    for (auto it = dups.constBegin(); it != dups.constEnd(); ++it)
        out << ReviewContext::make(QStringLiteral("FIX-14"), ReviewFinding::Note,
            trn("%n fixtures are called '%1': where a fixture is picked by name they can't be told apart.",
                it.value().count()).arg(it.key()),
            fixtureKeys(it.value()), it.key().toUtf8() + ':' + idBytes(it.value()),
            QStringLiteral("status:fixtures.duplicates"), QVariantList() << it.key());
}

/****************************************************************************
 * Effects (FX)
 ****************************************************************************/

QHash<quint32, QString> ShowReviewChecks::designEffectProblems(Doc *doc)
{
    QHash<quint32, QString> why;
    QSet<quint32> inScenes;
    for (Function *f : doc->functionsByType(Function::SceneType))
        if (Scene *sc = qobject_cast<Scene *>(f))
            for (quint32 pid : sc->palettes())
                inScenes.insert(pid);
    for (QLCPalette *p : doc->palettes())
    {
        if (p == nullptr || p->type() != QLCPalette::Effect || !inScenes.contains(p->id()))
            continue;
        if (p->scriptPath().isEmpty())
            why.insert(p->id(), QCoreApplication::translate("AppUtil", "no effect is chosen"));
        else if (!QFile::exists(p->scriptPath()))
            why.insert(p->id(), QCoreApplication::translate("AppUtil", "its script is not on this computer (%1)")
                                    .arg(QFileInfo(p->scriptPath()).fileName()));
    }
    return why;
}

static void fx07(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    const QHash<quint32, QString> why = ShowReviewChecks::designEffectProblems(ctx.doc());
    QList<quint32> ids = why.keys();
    std::sort(ids.begin(), ids.end());
    for (quint32 pid : ids)
        out << ReviewContext::make(QStringLiteral("FX-07"), ReviewFinding::Blocker,
            tr("Effect '%1' can't run: %2.").arg(ctx.paletteName(pid), why.value(pid)),
            { QStringLiteral("pal:%1").arg(pid) }, why.value(pid).toUtf8(),
            QStringLiteral("status:effects.failed"), QVariantList() << pid);
}

/****************************************************************************
 * Power and safety (PWR)
 ****************************************************************************/

static double defaultVoltage()
{
    // The Power window's default (PowerDistributionWidget's KDefaultVoltageKey)
    return QSettings().value(QStringLiteral("power/defaultVoltage"), 120.0).toDouble();
}

static bool powerSetUp(const PowerDistribution *pd)
{
    if (pd == nullptr)
        return false;
    for (const PowerSource &s : pd->sources())
        for (const PowerCircuit &c : s.circuits)
            if (!c.fixtures.isEmpty())
                return true;
    return false;
}

/** A scene's values with its palettes of the kinds in @a types resolved
 *  (as the power estimate resolves a scene, PowerEstimator::
 *  functionFixtureWatts()) -- only those kinds: an aim or a gobo by name is
 *  costly to resolve and moves no intensity and no strobe. */
static QList<SceneValue> sceneValues(Doc *doc, Scene *scene, const QSet<int> &types,
                                     QHash<QString, QList<SceneValue>> *cache = nullptr)
{
    QList<SceneValue> all = scene->values();
    QString targets;
    for (quint32 pid : scene->palettes())
    {
        QLCPalette *pal = doc->palette(pid);
        if (pal == nullptr || !types.contains(int(pal->type())))
            continue;
        if (cache == nullptr)
        {
            all += pal->valuesFromFixtures(doc, scene->fixtures());
            all += pal->valuesFromFixtureGroups(doc, scene->fixtureGroups());
            continue;
        }
        // The tree's scenes play the same palette on the same group cue
        // after cue: each pair resolved once.
        if (targets.isEmpty())
        {
            QStringList t;
            for (quint32 id : scene->fixtures())
                t << QString::number(id);
            t << QStringLiteral("|");
            for (quint32 id : scene->fixtureGroups())
                t << QString::number(id);
            targets = t.join(QLatin1Char(','));
        }
        const QString key = QString::number(pid) + QLatin1Char(':') + targets;
        auto it = cache->constFind(key);
        if (it == cache->constEnd())
            it = cache->insert(key, pal->valuesFromFixtures(doc, scene->fixtures())
                                    + pal->valuesFromFixtureGroups(doc, scene->fixtureGroups()));
        all += it.value();
    }
    return all;
}

/** What @a f puts out, channel by channel (universe * 512 + address):
 *  scenes from their values, collections HTP over their members, each scene
 *  resolved once (@a cache). */
static void functionChannels(Doc *doc, Function *f, QHash<quint32, QHash<quint32, uchar>> &cache,
                             QHash<QString, QList<SceneValue>> &paletteCache,
                             QHash<quint32, uchar> &out, QSet<quint32> &visited)
{
    if (f == nullptr || visited.contains(f->id()))
        return;
    visited.insert(f->id());
    if (Scene *scene = qobject_cast<Scene *>(f))
    {
        auto it = cache.find(scene->id());
        if (it == cache.end())
        {
            QHash<quint32, uchar> ch;
            // decided (can change): colour palettes aren't resolved (most of a
            // run's time on a big show); an unresolved colour counts as white,
            // the most it can draw (see pwr01()).
            static const QSet<int> lit { int(QLCPalette::Dimmer) };
            for (const SceneValue &sv : sceneValues(doc, scene, lit, &paletteCache))
            {
                Fixture *fx = doc->fixture(sv.fxi);
                if (fx == nullptr)
                    continue;
                const quint32 key = fx->universe() * 512u + fx->address() + sv.channel;
                if (uchar(sv.value) > ch.value(key, 0))
                    ch.insert(key, uchar(sv.value));
            }
            it = cache.insert(scene->id(), ch);
        }
        for (auto c = it.value().constBegin(); c != it.value().constEnd(); ++c)
            if (c.value() > out.value(c.key(), 0))
                out.insert(c.key(), c.value());
    }
    else if (Collection *col = qobject_cast<Collection *>(f))
    {
        for (quint32 mid : col->functions())
            functionChannels(doc, doc->function(mid), cache, paletteCache, out, visited);
    }
}

static void pwr01(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    Doc *doc = ctx.doc();
    PowerDistribution *pd = doc->powerDistribution();
    if (!powerSetUp(pd))
        return;
    ShowTreeController *st = doc->showTree();
    const double dflt = defaultVoltage();
    QHash<quint32, QHash<quint32, uchar>> sceneCache;
    QHash<QString, QList<SceneValue>> paletteCache;

    // Each circuit at rest (standby only), once; a look then changes only
    // the fixtures it touches.
    struct Circ { int s; int c; double idle = 0; };
    QList<Circ> circs;
    QHash<quint32, int> circOf;                 // fixture -> index in circs
    QHash<quint32, double> idleOf;
    QHash<quint32, quint32> fixtureAt;          // universe * 512 + address -> fixture
    QSet<quint32> colourAt;                     // the colour-mixing intensity channels
    const PowerEstimator::PreGMReader dark = [](quint32, quint32) -> uchar { return 0; };
    for (int s = 0; s < pd->sources().size(); ++s)
        for (int c = 0; c < pd->sources().at(s).circuits.size(); ++c)
        {
            Circ circ;
            circ.s = s;
            circ.c = c;
            for (quint32 fid : pd->sources().at(s).circuits.at(c).fixtures)
            {
                Fixture *fx = doc->fixture(fid);
                if (fx == nullptr || circOf.contains(fid))
                    continue;
                const double idle = PowerEstimator::fixtureWatts(fx, dark);
                idleOf.insert(fid, idle);
                circ.idle += idle;
                circOf.insert(fid, circs.count());
                if (fx->isPatched())
                    for (quint32 ch = 0; ch < fx->channels(); ++ch)
                    {
                        const quint32 abs = fx->universe() * 512u + fx->address() + ch;
                        fixtureAt.insert(abs, fid);
                        const QLCChannel *qch = fx->channel(ch);
                        if (qch != nullptr && qch->group() == QLCChannel::Intensity
                            && qch->colour() != QLCChannel::NoColour)
                            colourAt.insert(abs);
                    }
            }
            circs << circ;
        }

    for (Function *fn : doc->functions())
    {
        const bool scene = fn->type() == Function::SceneType;
        if (!scene && fn->type() != Function::CollectionType)
            continue;
        QHash<quint32, uchar> channels;
        QSet<quint32> visited;
        functionChannels(doc, fn, sceneCache, paletteCache, channels, visited);
        if (channels.isEmpty())
            continue;                           // lights nothing
        QSet<quint32> touched;
        for (auto it = channels.constBegin(); it != channels.constEnd(); ++it)
        {
            auto at = fixtureAt.constFind(it.key());
            if (at != fixtureAt.constEnd())
                touched.insert(at.value());
        }
        if (touched.isEmpty())
            continue;
        // A cue's own scenes are counted in its collection, together.
        quint32 listId = 0, cueId = ShowTree::invalidId(), layerId = ShowTree::invalidId();
        if (scene && st->generatedNode(fn->id(), &listId, &cueId, &layerId) && layerId != ShowTree::invalidId())
            continue;
        // A colour the scene doesn't set itself (a colour palette, not
        // resolved here) counts as white: the most the look can draw.
        const PowerEstimator::PreGMReader read = [&channels, &colourAt](quint32 uni, quint32 addr) -> uchar {
            const quint32 abs = uni * 512u + addr;
            auto it = channels.constFind(abs);
            if (it != channels.constEnd())
                return it.value();
            return colourAt.contains(abs) ? 255 : 0;
        };
        QVector<double> watts(circs.count());
        for (int i = 0; i < circs.count(); ++i)
            watts[i] = circs.at(i).idle;
        QSet<int> loaded;
        for (quint32 fid : touched)
        {
            const int i = circOf.value(fid, -1);
            if (i < 0)
                continue;
            watts[i] += PowerEstimator::fixtureWatts(doc->fixture(fid), read) - idleOf.value(fid);
            loaded.insert(i);
        }
        QList<int> order = loaded.values();
        std::sort(order.begin(), order.end());
        for (int i : order)
        {
            const PowerSource &src = pd->sources().at(circs.at(i).s);
            const PowerCircuit &cir = src.circuits.at(circs.at(i).c);
            const double srcV = src.voltage > 0.0 ? src.voltage : dflt;
            const double cw = watts.at(i);
            if (cw <= 0.0 || cir.ratedAmps <= 0.0)
                continue;
            const double amps = cw / cir.effectiveVoltage(srcV);
            const double limit = cir.deratedLimit();
            if (amps <= limit)
                continue;
            const bool over = amps > cir.ratedAmps;
            QStringList keys;
            QString jump;
            QVariantList data;
            const QString subject = functionSubject(ctx, fn->id(), &keys, &jump, &data);
            keys << QStringLiteral("circ:%1.%2").arg(circs.at(i).s).arg(circs.at(i).c);
            const QString circuit = src.name.isEmpty() ? cir.name : QStringLiteral("%1 %2").arg(src.name, cir.name);
            const QString text = over
                ? tr("%1 can draw %2 A on circuit %3 — over its %4 A breaker.")
                : tr("%1 can draw %2 A on circuit %3 (%4 A breaker) — over %5 % of it.");
            QString msg = text.arg(subject).arg(amps, 0, 'f', 1).arg(circuit).arg(cir.ratedAmps, 0, 'f', 0);
            if (!over)
                msg = msg.arg(cir.deratePercent);
            out << ReviewContext::make(QStringLiteral("PWR-01"),
                over ? ReviewFinding::Blocker : ReviewFinding::Warning, msg, keys,
                QByteArray::number(amps, 'f', 1) + '/' + QByteArray::number(cir.ratedAmps) + '/'
                    + QByteArray::number(cir.deratePercent),
                jump, data);
        }
    }
}

static void pwr03(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    Doc *doc = ctx.doc();
    PowerDistribution *pd = doc->powerDistribution();
    if (!powerSetUp(pd))
        return;
    QList<quint32> loose;
    for (Fixture *fx : doc->fixtures())
    {
        if (fx == nullptr || !fx->isPatched() || PowerEstimator::basis(fx) == PowerEstimator::NoLoad)
            continue;
        int s = -1, c = -1;
        pd->circuitOf(fx->id(), s, c);
        if (s < 0)
            loose << fx->id();
    }
    if (!loose.isEmpty())
        out << ReviewContext::make(QStringLiteral("PWR-03"), ReviewFinding::Warning,
            trn("%n fixture(s) are on no circuit, so the power check can't count them: %1.", loose.count())
                .arg(nameList(ctx, loose)),
            fixtureKeys(loose), idBytes(loose), QStringLiteral("power"), variantIds(loose));
}

/** How fast a value of @a ch flashes (Hz); 0: not a strobe; -1: a strobe
 *  whose definition gives no rate, set above half its range ("may be"). */
static double strobeHz(const QLCChannel *ch, uchar value)
{
    QLCCapability *cap = ch->searchCapability(value);
    if (cap == nullptr)
        return 0;
    switch (cap->preset())
    {
        case QLCCapability::StrobeFrequency:
            return cap->resource(0).toDouble();
        case QLCCapability::StrobeFreqRange:
        {
            const double lo = cap->resource(0).toDouble(), hi = cap->resource(1).toDouble();
            const int span = int(cap->max()) - int(cap->min());
            const double at = span > 0 ? double(int(value) - int(cap->min())) / span : 1.0;
            return lo + (hi - lo) * at;
        }
        case QLCCapability::StrobeSlowToFast:
        case QLCCapability::StrobeRandomSlowToFast:
            return value > (int(cap->min()) + int(cap->max())) / 2 ? -1 : 0;
        case QLCCapability::StrobeFastToSlow:
        case QLCCapability::StrobeRandomFastToSlow:
            return value < (int(cap->min()) + int(cap->max())) / 2 ? -1 : 0;
        default:
            return 0;
    }
}

static void pwr05(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    Doc *doc = ctx.doc();
    struct Flash { QList<quint32> sure, maybe; double hz = 0; QString subject; QStringList keys;
                   QString jump; QVariantList data; QByteArray fp; };
    QMap<QString, Flash> bySubject;
    for (Function *f : doc->functionsByType(Function::SceneType))
    {
        Scene *scene = qobject_cast<Scene *>(f);
        if (scene == nullptr)
            continue;
        // Strobe comes from raw values or a strobe / shutter palette; whether
        // the light is up, from a dimmer.
        static const QSet<int> kinds { int(QLCPalette::Strobe), int(QLCPalette::Shutter),
                                       int(QLCPalette::Dimmer) };
        const QList<SceneValue> all = sceneValues(doc, scene, kinds);
        QHash<quint32, double> hzOf;
        QSet<quint32> lit, dark;
        for (const SceneValue &sv : all)
        {
            Fixture *fx = doc->fixture(sv.fxi);
            const QLCChannel *ch = fx != nullptr ? fx->channel(sv.channel) : nullptr;
            if (ch == nullptr)
                continue;
            if (ch->group() == QLCChannel::Intensity && ch->colour() == QLCChannel::NoColour)
                (sv.value > 0 ? lit : dark).insert(sv.fxi);
            const double hz = strobeHz(ch, sv.value);
            if (hz != 0)
                hzOf[sv.fxi] = qMax(hzOf.value(sv.fxi), hz);
        }
        if (hzOf.isEmpty())
            continue;
        QStringList keys;
        QString jump;
        QVariantList data;
        const QString subject = functionSubject(ctx, scene->id(), &keys, &jump, &data);
        Flash &fl = bySubject[keys.join(QLatin1Char(','))];
        fl.subject = subject;
        fl.keys = keys;
        fl.jump = jump;
        fl.data = data;
        QList<quint32> ids = hzOf.keys();
        std::sort(ids.begin(), ids.end());
        for (quint32 fid : ids)
        {
            // Dark the whole cue: it doesn't flash.
            if (dark.contains(fid) && !lit.contains(fid))
                continue;
            const double hz = hzOf.value(fid);
            if (hz > 3.0)
            {
                if (!fl.sure.contains(fid))
                    fl.sure << fid;
                fl.hz = qMax(fl.hz, hz);
            }
            else if (hz < 0 && !fl.maybe.contains(fid))
                fl.maybe << fid;
            fl.fp += QByteArray::number(fid) + '@' + QByteArray::number(hz, 'f', 1) + ';';
        }
    }
    for (auto it = bySubject.constBegin(); it != bySubject.constEnd(); ++it)
    {
        const Flash &fl = it.value();
        if (!fl.sure.isEmpty())
        {
            // decided (SHOW_REVIEW.md Q7, can change): four lights or more at
            // once is "a large part of the stage" -- a warning; fewer, a note.
            const bool large = fl.sure.count() >= 4;
            out << ReviewContext::make(QStringLiteral("PWR-05"),
                large ? ReviewFinding::Warning : ReviewFinding::Note,
                tr("%1 strobes %2 at about %3 Hz — over 3 a second. Post a strobe warning, or slow it "
                   "under 3 Hz.").arg(fl.subject, nameList(ctx, fl.sure)).arg(qRound(fl.hz)),
                fl.keys, fl.fp, fl.jump, fl.data);
        }
        else if (!fl.maybe.isEmpty())
        {
            out << ReviewContext::make(QStringLiteral("PWR-05"), ReviewFinding::Note,
                tr("%1 may strobe %2 faster than 3 a second (the definition gives no rate; it's set "
                   "past half way).").arg(fl.subject, nameList(ctx, fl.maybe)),
                fl.keys, fl.fp, fl.jump, fl.data);
        }
    }
}

static void pwr06(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    for (Fixture *fx : ctx.doc()->fixtures())
    {
        if (fx == nullptr)
            continue;
        const Fixture::OutputRules rules = fx->outputRules();
        if (!(rules & (Fixture::ExemptBlackout | Fixture::ExemptGrandMaster)))
            continue;
        // A relay or a house / work light is what the rules are for.
        if (rules & (Fixture::Switch | Fixture::OnAtOpen))
            continue;
        const QLCFixtureDef *def = fx->fixtureDef();
        if (def == nullptr)
            continue;
        switch (def->type())
        {
            case QLCFixtureDef::MovingHead: case QLCFixtureDef::Scanner: case QLCFixtureDef::ColorChanger:
            case QLCFixtureDef::Strobe: case QLCFixtureDef::Flower: case QLCFixtureDef::Effect:
            case QLCFixtureDef::LEDBarBeams: case QLCFixtureDef::LEDBarPixels: case QLCFixtureDef::Laser:
                break;
            default:
                continue;
        }
        QStringList what;
        if (rules & Fixture::ExemptBlackout)
            what << tr("Blackout");
        if (rules & Fixture::ExemptGrandMaster)
            what << tr("the Grand Master");
        out << ReviewContext::make(QStringLiteral("PWR-06"), ReviewFinding::Warning,
            tr("%1 ignores %2. Blackout is a safety control: is it a work light on purpose?")
                .arg(fx->name(), what.join(tr(" and "))),
            fixtureKeys({ fx->id() }), QByteArray::number(int(rules)), QStringLiteral("fixtures"),
            variantIds({ fx->id() }));
    }
}

/****************************************************************************
 * Files (FILE)
 ****************************************************************************/

static QList<QPair<Function *, QString>> mediaFiles(Doc *doc)
{
    QList<QPair<Function *, QString>> files;
    for (Function *f : doc->functions())
    {
        if (Audio *a = qobject_cast<Audio *>(f))
        {
            if (!a->getSourceFileName().isEmpty())
                files << qMakePair(f, a->getSourceFileName());
        }
        else if (Video *v = qobject_cast<Video *>(f))
        {
            const QString url = v->sourceUrl();
            if (!url.isEmpty() && !url.contains(QStringLiteral("://")))
                files << qMakePair(f, url);
        }
    }
    return files;
}

static void file01(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    Doc *doc = ctx.doc();
    if (doc->workspacePath().isEmpty())
        return;                                 // an unsaved show has no folder yet
    for (const auto &m : mediaFiles(doc))
    {
        if (!QFileInfo::exists(m.second))
            continue;                           // FILE-02's
        if (!QDir::isAbsolutePath(doc->normalizeComponentPath(m.second)))
            continue;
        out << ReviewContext::make(QStringLiteral("FILE-01"), ReviewFinding::Warning,
            tr("'%1' plays %2, outside the show's folder: it works on this computer only. "
               "File ▸ Collect for Transfer copies it in.")
                .arg(m.first->name(), QDir::toNativeSeparators(m.second)),
            { QStringLiteral("fn:%1").arg(m.first->id()) }, m.second.toUtf8(),
            QStringLiteral("function"), QVariantList() << m.first->id());
    }
}

static void file02(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    for (const auto &m : mediaFiles(ctx.doc()))
    {
        if (QFileInfo::exists(m.second))
            continue;
        out << ReviewContext::make(QStringLiteral("FILE-02"), ReviewFinding::Blocker,
            tr("'%1' plays %2, which isn't there.")
                .arg(m.first->name(), QFileInfo(m.second).fileName()),
            { QStringLiteral("fn:%1").arg(m.first->id()) }, m.second.toUtf8(),
            QStringLiteral("function"), QVariantList() << m.first->id());
    }
}

/****************************************************************************
 * Naming (NAME)
 ****************************************************************************/

static bool namedCheck(ReviewContext &ctx, Function *f)
{
    if (f == nullptr || !f->isVisible() || ctx.doc()->showTree()->isGenerated(f->id()))
        return false;
    return f->type() == Function::SceneType || f->type() == Function::CollectionType;
}

static void name01(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    // SS-PP.II-Description (memory: show-naming-convention), learned per folder.
    static const QRegularExpression pattern(QStringLiteral("^\\d+-\\d+\\.\\d+-\\S"));
    QMap<QString, QList<Function *>> byFolder;
    for (Function *f : ctx.doc()->functions())
        if (namedCheck(ctx, f))
            byFolder[f->path(true)] << f;
    for (auto it = byFolder.constBegin(); it != byFolder.constEnd(); ++it)
    {
        QList<Function *> on, off;
        for (Function *f : it.value())
            (pattern.match(f->name()).hasMatch() ? on : off) << f;
        // Only where the folder has a pattern: two or more, and most of it.
        if (on.count() < 2 || on.count() <= off.count())
            continue;
        QStringList examples;
        for (int i = 0; i < on.count() && i < 2; ++i)
            examples << on.at(i)->name();
        for (Function *f : off)
            out << ReviewContext::make(QStringLiteral("NAME-01"), ReviewFinding::Note,
                tr("'%1' sits among %2…: name it the same way.")
                    .arg(f->name(), examples.join(QStringLiteral(", "))),
                { QStringLiteral("fn:%1").arg(f->id()) }, f->name().toUtf8(),
                QStringLiteral("function"), QVariantList() << f->id());
    }
}

static void name02(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    static const QRegularExpression defaults(
        QStringLiteral("^(New (Scene|Chaser|Sequence|Collection|EFX|RGB Matrix|Script|Audio|Video|Show))( \\d+)?$"
                       "|^Copy of |\\(Copy\\)$"));
    for (Function *f : ctx.doc()->functions())
    {
        if (f == nullptr || !f->isVisible() || ctx.doc()->showTree()->isGenerated(f->id())
            || !defaults.match(f->name()).hasMatch())
            continue;
        out << ReviewContext::make(QStringLiteral("NAME-02"), ReviewFinding::Note,
            tr("'%1' still has a default name: say what it is.").arg(f->name()),
            { QStringLiteral("fn:%1").arg(f->id()) }, f->name().toUtf8(),
            QStringLiteral("function"), QVariantList() << f->id());
    }
}

/****************************************************************************
 * The list
 ****************************************************************************/

QList<ShowReview::Check> ShowReviewChecks::showChecks()
{
    typedef ShowReview S;
    return {
        { QStringLiteral("PAT-01"), tr("Two fixtures on the same channels"), S::Patch,
          tr("Two fixtures that share DMX channels fight: each one's values move the other. One of "
             "them is on the wrong address. Re-patch it somewhere free."), pat01 },
        { QStringLiteral("PAT-02"), tr("Past the end of the universe"), S::Patch,
          tr("A fixture whose channels run past 512 loses the ones over the end. Only an import or "
             "a hand-edited file can do this. Move it down, or to another universe."), pat02 },
        { QStringLiteral("PAT-03"), tr("Unpatched fixtures"), S::Patch,
          tr("A fixture with no address sends nothing. Unused, that's fine (it waits in the "
             "inventory); lit by cues, the stage will be missing it."), pat03 },
        { QStringLiteral("PAT-04"), tr("On channels the venue keeps"), S::Patch,
          tr("The venue marks some channels as not ours: its own house circuits. A fixture there "
             "drives the venue's lights. Re-patch it."), pat04 },
        { QStringLiteral("PAT-06"), tr("A universe with fixtures and no output"), S::Patch,
          tr("Fixtures on a universe with no output go nowhere. Give each universe an output in "
             "Connections; a show with none at all only previews."), pat06 },
        { QStringLiteral("FIX-01"), tr("Missing definition"), S::Fixtures,
          tr("A fixture whose definition isn't on this computer stands in as a plain dimmer: its "
             "colours, gobos and moves can't be programmed or played. Install the definition, or "
             "save the show as .qlcc where it is so the show carries it."), fix01 },
        { QStringLiteral("FIX-05"), tr("Not on the plot"), S::Fixtures,
          tr("Focus, aiming and the Rig Overview need to know where a light hangs. A fixture in a "
             "focus call that isn't on the plot can't be aimed."), fix05 },
        { QStringLiteral("FIX-07"), tr("No light output data"), S::Fixtures,
          tr("A definition with no lumens or power makes the plot and the power check guess. Set "
             "them on the definition's Physical tab."), fix07 },
        { QStringLiteral("FIX-14"), tr("Duplicate fixture names"), S::Fixtures,
          tr("Where a fixture is picked by name, two with one name can't be told apart. Number "
             "them."), fix14 },
        { QStringLiteral("FX-07"), tr("Effect that won't load"), S::Effects,
          tr("An effect with no script chosen, or whose script isn't on this computer, does "
             "nothing on stage."), fx07 },
        { QStringLiteral("PWR-01"), tr("A circuit over its breaker"), S::Power,
          tr("A cue that draws more than a circuit's breaker trips it mid-show; over 80 % of it "
             "(the continuous-load rule) is too close. Move fixtures to another circuit, or "
             "lower the look."), pwr01 },
        { QStringLiteral("PWR-03"), tr("Lights on no circuit"), S::Power,
          tr("Once the power is set up, a fixture on no circuit isn't counted, so the breaker "
             "check misses its draw."), pwr03 },
        { QStringLiteral("PWR-05"), tr("Flashing faster than 3 a second"), S::Power,
          tr("Light flashing more than 3 times a second can trigger photosensitive epilepsy "
             "(the guidance broadcasters and WCAG use). Many venues require a posted warning. "
             "Slow it, or accept it with the note that the warning is posted."), pwr05 },
        { QStringLiteral("PWR-06"), tr("Lights that ignore Blackout"), S::Power,
          tr("Blackout and the Grand Master are safety controls. A stage light that ignores them "
             "surprises the operator; that rule is for work and house lights."), pwr06 },
        { QStringLiteral("FILE-01"), tr("Media outside the show folder"), S::Files,
          tr("A sound or video file outside the show's folder works on this computer only. "
             "Collect for Transfer copies it in beside the show."), file01 },
        { QStringLiteral("FILE-02"), tr("Missing media"), S::Files,
          tr("A sound or video the show plays that isn't there: the cue plays nothing."), file02 },
        { QStringLiteral("NAME-01"), tr("Names off the folder's pattern"), S::Naming,
          tr("When a folder's scenes are named SS-PP.II-Description, one that isn't is hard to "
             "find and sorts in the wrong place."), name01 },
        { QStringLiteral("NAME-02"), tr("Default and copy names"), S::Naming,
          tr("'New Scene 4' and 'Copy of …' tell the next person nothing. Say what it is."), name02 },
    };
}
