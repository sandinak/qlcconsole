/*
  Q Light Controller Plus
  showtreecontroller.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QBuffer>
#include <QColor>
#include <QRegularExpression>
#include <climits>
#include <QTimer>
#include <QSet>
#include <QDebug>
#include <algorithm>

#include "showtreecontroller.h"
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "speedmasters.h"
#include "fixturegroup.h"
#include "collection.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "chaserstep.h"
#include "mastertimer.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

using namespace ShowTree;

#define KXMLShowTreeNextId          QStringLiteral("NextId")
#define KXMLShowTreeGenerated       QStringLiteral("Generated")
#define KXMLShowTreeInline          QStringLiteral("InlinePalettes")
#define KXMLShowTreeChasers         QStringLiteral("Chasers")

/** Where the tree files what it makes (§4): out of the way of the user's
    own folders. */
static const QString kProductionsFolder = QStringLiteral("Productions");
static const QString kInlinePaletteFolder = QStringLiteral("Show Tree");

ShowTreeController::ShowTreeController(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
    connect(m_doc, &Doc::functionRemoved, this, &ShowTreeController::slotFunctionRemoved);
    connect(m_doc, &Doc::paletteRemoved, this, &ShowTreeController::slotPaletteRemoved);
    connect(m_doc, &Doc::modeChanged, this, &ShowTreeController::slotModeChanged);
    m_sweepTimer = new QTimer(this);
    m_sweepTimer->setSingleShot(true);
    m_sweepTimer->setInterval(250);
    connect(m_sweepTimer, &QTimer::timeout, this, &ShowTreeController::bake);
    // From the DMX thread: queued, so the preview is only ever touched here.
    connect(m_doc->masterTimer(), &MasterTimer::functionStarted,
            this, &ShowTreeController::slotTimerFunctionStarted, Qt::QueuedConnection);
    connect(m_doc->masterTimer(), &MasterTimer::functionStopped,
            this, &ShowTreeController::slotTimerFunctionStopped, Qt::QueuedConnection);
}

ShowTreeController::~ShowTreeController()
{
}

/****************************************************************************
 * The tree
 ****************************************************************************/

const Production *ShowTreeController::production(quint32 id) const
{
    for (const Production &p : m_productions)
        if (p.id == id)
            return &p;
    return nullptr;
}

const CueList *ShowTreeController::cueList(quint32 listId, quint32 *productionId) const
{
    for (const Production &p : m_productions)
    {
        if (const CueList *l = p.list(listId))
        {
            if (productionId != nullptr)
                *productionId = p.id;
            return l;
        }
    }
    return nullptr;
}

const CueList *ShowTreeController::listOfCue(quint32 cueId) const
{
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            if (l.indexOf(cueId) >= 0)
                return &l;
    return nullptr;
}

const Cue *ShowTreeController::cue(quint32 cueId) const
{
    const CueList *l = listOfCue(cueId);
    return l == nullptr ? nullptr : l->cue(cueId);
}

quint32 ShowTreeController::newId()
{
    return m_nextId++;
}

static Cue *cueIn(QList<Production> &productions, quint32 cueId, CueList **listOut = nullptr)
{
    for (Production &p : productions)
        for (CueList &l : p.lists)
            if (Cue *c = l.cue(cueId))
            {
                if (listOut != nullptr)
                    *listOut = &l;
                return c;
            }
    return nullptr;
}

static CueList *listIn(QList<Production> &productions, quint32 listId)
{
    for (Production &p : productions)
        if (CueList *l = p.list(listId))
            return l;
    return nullptr;
}

ShowTree::Cue *ShowTreeController::findCue(quint32 cueId, CueList **list)
{
    return cueIn(m_productions, cueId, list);
}

/****************************************************************************
 * Editing
 ****************************************************************************/

bool ShowTreeController::edit(const QString &label,
                              const std::function<bool(QList<Production> &)> &change)
{
    if (m_batchDepth > 0)
    {
        // Part of a command: applied now, baked and recorded once at the end.
        QList<Production> work = m_productions;
        if (!change(work))
            return false;
        m_productions = work;
        m_batchChanged = true;
        return true;
    }

    const QByteArray before = snapshotXml();
    QList<Production> work = m_productions;
    if (!change(work))
        return false;

    m_productions = work;
    m_undo.append(Snapshot { label, before });
    while (m_undo.count() > kUndoLimit)
        m_undo.removeFirst();
    m_redo.clear();

    bake();
    m_doc->setModified();
    emit changed();
    return true;
}

void ShowTreeController::beginBatch(const QString &label)
{
    if (m_batchDepth++ > 0)
        return;
    m_batchNamed.clear();
    m_batchLabel = label;
    m_batchBefore = snapshotXml();
    m_batchProductions = m_productions;
    m_batchChanged = false;
}

void ShowTreeController::endBatch(bool keep)
{
    if (m_batchDepth == 0 || --m_batchDepth > 0)
        return;
    // Named palettes the command made: reusable now it has worked, else
    // one-offs nothing uses, dropped below with the rest.
    const QList<quint32> named = m_batchNamed;
    m_batchNamed.clear();
    if (keep)
    {
        for (quint32 pid : named)
            if (m_inlinePalettes.remove(pid) && !m_batchChanged)
                m_doc->setModified();
    }
    if (!m_batchChanged)
    {
        // Nothing in the tree changed, but a failed command may have made
        // a one-off value on the way.
        if (!keep)
            dropUnusedInlinePalettes();
        return;
    }
    if (!keep)
    {
        // Back as it was; a bake drops a one-off value made on the way.
        m_productions = m_batchProductions;
        bake();
        emit changed();
        return;
    }
    m_undo.append(Snapshot { m_batchLabel, m_batchBefore });
    while (m_undo.count() > kUndoLimit)
        m_undo.removeFirst();
    m_redo.clear();
    bake();
    m_doc->setModified();
    emit changed();
}

quint32 ShowTreeController::addProduction(const QString &name)
{
    const quint32 id = newId();
    edit(tr("New production"), [&](QList<Production> &ps) {
        Production p;
        p.id = id;
        p.name = name;
        ps << p;
        return true;
    });
    return id;
}

quint32 ShowTreeController::addCueList(quint32 productionId, const QString &name, ListMode mode)
{
    const quint32 id = newId();
    const bool ok = edit(tr("New cue list"), [&](QList<Production> &ps) {
        for (Production &p : ps)
        {
            if (p.id != productionId)
                continue;
            CueList l;
            l.id = id;
            l.name = name;
            l.mode = mode;
            p.lists << l;
            return true;
        }
        return false;
    });
    return ok ? id : invalidId();
}

quint32 ShowTreeController::addCue(quint32 listId, int afterIndex, const QString &name)
{
    const quint32 id = newId();
    const bool ok = edit(tr("New cue"), [&](QList<Production> &ps) {
        CueList *l = listIn(ps, listId);
        if (l == nullptr)
            return false;
        Cue c;
        c.id = id;
        c.name = name;
        c.number = numberForInsert(*l, afterIndex);
        const int at = qBound(0, afterIndex + 1, int(l->cues.count()));
        l->cues.insert(at, c);
        return true;
    });
    return ok ? id : invalidId();
}

bool ShowTreeController::removeCue(quint32 cueId)
{
    return edit(tr("Delete cue"), [&](QList<Production> &ps) {
        CueList *l = nullptr;
        if (cueIn(ps, cueId, &l) == nullptr)
            return false;
        l->cues.removeAt(l->indexOf(cueId));
        for (Cue &c : l->cues)
            c.includes.removeAll(cueId);
        if (l->baseCue == cueId)
            l->baseCue = invalidId();
        return true;
    });
}

bool ShowTreeController::moveCue(quint32 cueId, int toIndex)
{
    return edit(tr("Move cue"), [&](QList<Production> &ps) {
        CueList *l = nullptr;
        if (cueIn(ps, cueId, &l) == nullptr)
            return false;
        const int from = l->indexOf(cueId);
        toIndex = qBound(0, toIndex, int(l->cues.count()) - 1);
        if (from == toIndex)
            return false;
        l->cues.move(from, toIndex);
        return true;
    });
}

bool ShowTreeController::setCue(const Cue &cue)
{
    return edit(tr("Edit cue"), [&](QList<Production> &ps) {
        Cue *c = cueIn(ps, cue.id);
        if (c == nullptr)
            return false;
        const quint32 coll = c->collection;
        *c = cue;
        c->collection = coll;
        // Includes as given, less any that are gone or would loop (D8).
        const QList<quint32> wanted = c->includes;
        c->includes.clear();
        CueList *l = nullptr;
        cueIn(ps, cue.id, &l);
        for (quint32 inc : wanted)
            if (canInclude(*l, cue.id, inc))
                c->includes << inc;
        return true;
    });
}

quint32 ShowTreeController::addLayer(quint32 cueId, const Target &target, const QString &name)
{
    if (target.isEmpty())
        return invalidId();
    const quint32 id = newId();
    const bool ok = edit(tr("Add layer"), [&](QList<Production> &ps) {
        Cue *c = cueIn(ps, cueId);
        if (c == nullptr)
            return false;
        Layer l;
        l.id = id;
        l.name = name;
        l.target = target;
        c->layers << l;
        return true;
    });
    return ok ? id : invalidId();
}

bool ShowTreeController::removeLayer(quint32 cueId, quint32 layerId)
{
    return edit(tr("Remove layer"), [&](QList<Production> &ps) {
        Cue *c = cueIn(ps, cueId);
        if (c == nullptr)
            return false;
        for (int i = 0; i < c->layers.count(); ++i)
            if (c->layers.at(i).id == layerId)
            {
                c->layers.removeAt(i);
                return true;
            }
        return false;
    });
}

bool ShowTreeController::setLayerLook(quint32 cueId, quint32 layerId, const Look &look)
{
    return edit(tr("Set %1").arg(keyLabel(look.key)), [&](QList<Production> &ps) {
        Cue *c = cueIn(ps, cueId);
        Layer *l = c == nullptr ? nullptr : c->layer(layerId);
        if (l == nullptr || l->target.isOpaque())
            return false;
        const Look *was = l->look(look.key);
        if (was != nullptr && *was == look)
            return false;
        l->setLook(look);
        return true;
    });
}

bool ShowTreeController::setCueLook(quint32 cueId, const Look &look)
{
    return edit(tr("Set cue %1").arg(keyLabel(look.key)), [&](QList<Production> &ps) {
        Cue *c = cueIn(ps, cueId);
        if (c == nullptr)
            return false;
        const Look *was = c->look(look.key);
        if (was != nullptr && *was == look)
            return false;
        c->setLook(look);
        return true;
    });
}

bool ShowTreeController::setTargetLook(quint32 cueId, const Target &target, const Look &look)
{
    if (target.isEmpty() || target.isOpaque())
        return false;
    const quint32 newLayer = newId();
    return edit(tr("Set %1").arg(keyLabel(look.key)), [&](QList<Production> &ps) {
        Cue *c = cueIn(ps, cueId);
        if (c == nullptr)
            return false;
        Layer *l = nullptr;
        for (Layer &have : c->layers)
            if (have.target == target)
                l = &have;
        if (l == nullptr)
        {
            Layer fresh;
            fresh.id = newLayer;
            fresh.target = target;
            c->layers << fresh;
            l = &c->layers.last();
        }
        const Look *was = l->look(look.key);
        if (was != nullptr && *was == look)
            return false;
        l->setLook(look);
        return true;
    });
}

bool ShowTreeController::setLayerRelease(quint32 cueId, quint32 layerId, bool release)
{
    return edit(release ? tr("Release layer") : tr("Carry layer"), [&](QList<Production> &ps) {
        Cue *c = cueIn(ps, cueId);
        Layer *l = c == nullptr ? nullptr : c->layer(layerId);
        if (l == nullptr || l->release == release)
            return false;
        l->release = release;
        return true;
    });
}

bool ShowTreeController::setLayerTarget(quint32 cueId, quint32 layerId, const Target &target,
                                        const QString &label)
{
    Target to;
    for (quint32 g : target.groups)
        if (!to.groups.contains(g))
            to.groups << g;
    for (quint32 f : target.fixtures)
        if (!to.fixtures.contains(f))
            to.fixtures << f;
    if (to.isEmpty())
        return false;

    return edit(label.isEmpty() ? tr("Change layer") : label, [&](QList<Production> &ps) {
        CueList *list = nullptr;
        Cue *c = cueIn(ps, cueId, &list);
        Layer *l = c == nullptr ? nullptr : c->layer(layerId);
        if (l == nullptr || l->target.isOpaque() || l->target == to)
            return false;

        const Target from = l->target;
        const int at = list->indexOf(cueId);
        const QList<ResolvedCue> before = resolve(*list);

        // In this cue: if it carried on from a layer (tracked, included),
        // it still does, under its new target.
        const ResolvedLayer *rl = before.at(at).layer(from);
        const Target carried = rl == nullptr ? Target() : rl->tookOver;
        l->target = to;
        l->continues = (!carried.isEmpty() && !(carried == to)) ? carried : Target();

        // Onwards, while the layer tracks: the same layer set again in a
        // later cue (it took the tracked one's place) follows the change.
        for (int j = at + 1; j < list->cues.count(); ++j)
        {
            const ResolvedLayer *next = before.at(j).layer(from);
            if (next == nullptr)
            {
                // A layer there that carried on from it under a target of
                // its own carries on from the new one.
                for (Layer &other : list->cues[j].layers)
                    if (!other.continues.isEmpty() && other.continues == from)
                        other.continues = to;
                break;                      // released, a Block cue, or taken over
            }
            if (!next->own)
                continue;                   // carried through: follows by itself
            if (!(next->tookOver == from))
                break;                      // a fresh layer on that target
            Layer *again = list->cues[j].layer(next->layerId);
            if (again == nullptr)
                break;
            again->target = to;
            again->continues = Target();
        }
        return true;
    });
}

bool ShowTreeController::addToLayer(quint32 cueId, quint32 layerId, const QList<quint32> &groups,
                                    const QList<quint32> &fixtures)
{
    const Cue *c = cue(cueId);
    const Layer *l = c == nullptr ? nullptr : c->layer(layerId);
    if (l == nullptr || l->target.isOpaque())
        return false;
    Target t = l->target;
    QStringList names;
    for (quint32 g : groups)
        if (!t.groups.contains(g))
        {
            t.groups << g;
            Target one;
            one.groups << g;
            names << targetLabel(one);
        }
    for (quint32 f : fixtures)
        if (!t.fixtures.contains(f))
        {
            t.fixtures << f;
            Target one;
            one.fixtures << f;
            names << targetLabel(one);
        }
    if (names.isEmpty())
        return false;
    return setLayerTarget(cueId, layerId, t, tr("Add %1 to layer").arg(names.join(QStringLiteral(", "))));
}

bool ShowTreeController::removeGroupFromLayer(quint32 cueId, quint32 layerId, quint32 groupId)
{
    const Cue *c = cue(cueId);
    const Layer *l = c == nullptr ? nullptr : c->layer(layerId);
    if (l == nullptr || !l->target.groups.contains(groupId))
        return false;
    Target t = l->target;
    t.groups.removeAll(groupId);
    Target one;
    one.groups << groupId;
    return setLayerTarget(cueId, layerId, t, tr("Remove %1 from layer").arg(targetLabel(one)));
}

bool ShowTreeController::removeFixtureFromLayer(quint32 cueId, quint32 layerId, quint32 fixtureId)
{
    const Cue *c = cue(cueId);
    const Layer *l = c == nullptr ? nullptr : c->layer(layerId);
    if (l == nullptr || !l->target.fixtures.contains(fixtureId))
        return false;
    Target t = l->target;
    t.fixtures.removeAll(fixtureId);
    Target one;
    one.fixtures << fixtureId;
    return setLayerTarget(cueId, layerId, t, tr("Remove %1 from layer").arg(targetLabel(one)));
}

bool ShowTreeController::setListMode(quint32 listId, ListMode mode)
{
    return edit(tr("Cue list mode"), [&](QList<Production> &ps) {
        CueList *l = listIn(ps, listId);
        if (l == nullptr || l->mode == mode)
            return false;
        l->mode = mode;
        return true;
    });
}

bool ShowTreeController::setBaseCue(quint32 listId, quint32 cueId)
{
    return edit(tr("Base cue"), [&](QList<Production> &ps) {
        CueList *l = listIn(ps, listId);
        if (l == nullptr || l->baseCue == cueId)
            return false;
        if (cueId != invalidId() && l->indexOf(cueId) < 0)
            return false;
        l->baseCue = cueId;
        return true;
    });
}

bool ShowTreeController::setCueMode(quint32 cueId, CueMode mode)
{
    return edit(tr("Cue mode"), [&](QList<Production> &ps) {
        Cue *c = cueIn(ps, cueId);
        if (c == nullptr || c->mode == mode)
            return false;
        c->mode = mode;
        return true;
    });
}

bool ShowTreeController::setTargetRelease(quint32 cueId, const Target &target, bool release)
{
    if (target.isEmpty())
        return false;
    const quint32 newLayer = newId();
    return edit(release ? tr("Release layer") : tr("Carry layer"), [&](QList<Production> &ps) {
        Cue *c = cueIn(ps, cueId);
        if (c == nullptr)
            return false;
        Layer *l = nullptr;
        for (Layer &have : c->layers)
            if (have.target == target)
                l = &have;
        if (l == nullptr)
        {
            if (!release)
                return false;           // carried in, and carried on: nothing to do
            Layer fresh;
            fresh.id = newLayer;
            fresh.target = target;      // takes the carried-in layer's place and values
            c->layers << fresh;
            l = &c->layers.last();
        }
        if (l->release == release)
            return false;
        l->release = release;
        return true;
    });
}

bool ShowTreeController::addInclude(quint32 cueId, quint32 includedId, QString *why)
{
    const CueList *list = listOfCue(cueId);
    if (list == nullptr)
        return false;
    if (!canInclude(*list, cueId, includedId, why))
        return false;
    const Cue *inc = list->cue(includedId);
    return edit(tr("Include %1").arg(cueLabel(*list, list->indexOf(inc->id))), [&](QList<Production> &ps) {
        Cue *c = cueIn(ps, cueId);
        if (c == nullptr)
            return false;
        c->includes << includedId;
        return true;
    });
}

bool ShowTreeController::removeInclude(quint32 cueId, quint32 includedId)
{
    return edit(tr("Remove include"), [&](QList<Production> &ps) {
        Cue *c = cueIn(ps, cueId);
        return c != nullptr && c->includes.removeAll(includedId) > 0;
    });
}

bool ShowTreeController::moveInclude(quint32 cueId, quint32 includedId, int toIndex)
{
    return edit(tr("Move include"), [&](QList<Production> &ps) {
        Cue *c = cueIn(ps, cueId);
        if (c == nullptr)
            return false;
        const int from = c->includes.indexOf(includedId);
        toIndex = qBound(0, toIndex, int(c->includes.count()) - 1);
        if (from < 0 || from == toIndex)
            return false;
        c->includes.move(from, toIndex);
        return true;
    });
}

quint32 ShowTreeController::addCueNumbered(quint32 listId, const QString &number, const QString &name)
{
    const QString num = number.trimmed();
    const CueList *have = cueList(listId);
    if (have == nullptr || num.isEmpty() || indexOfNumber(*have, num) >= 0)
        return invalidId();
    const quint32 id = newId();
    const bool ok = edit(tr("New cue"), [&](QList<Production> &ps) {
        CueList *l = listIn(ps, listId);
        if (l == nullptr)
            return false;
        Cue c;
        c.id = id;
        c.number = num;
        c.name = name;
        int at = 0;
        while (at < l->cues.count())
        {
            const QString n = l->cues.at(at).number.isEmpty() ? QString::number(at + 1)
                                                               : l->cues.at(at).number;
            if (compareCueNumbers(n, num) > 0)
                break;
            ++at;
        }
        l->cues.insert(at, c);
        return true;
    });
    return ok ? id : invalidId();
}

quint32 ShowTreeController::duplicateCue(quint32 cueId)
{
    const CueList *have = listOfCue(cueId);
    if (have == nullptr)
        return invalidId();
    const int at = have->indexOf(cueId);
    Cue copy = have->cues.at(at);
    copy.id = newId();
    copy.collection = invalidId();
    copy.number = numberForInsert(*have, at);
    for (Layer &l : copy.layers)
        l.id = newId();
    const quint32 listId = have->id;
    const bool ok = edit(tr("Duplicate cue"), [&](QList<Production> &ps) {
        CueList *l = listIn(ps, listId);
        if (l == nullptr)
            return false;
        l->cues.insert(at + 1, copy);
        return true;
    });
    return ok ? copy.id : invalidId();
}

quint32 ShowTreeController::recordCue(quint32 cueId)
{
    return recordCue(cueId, RecordSource::Selected);
}

QString ShowTreeController::recordSourceToString(RecordSource source)
{
    switch (source)
    {
        case RecordSource::Running: return QStringLiteral("running");
        case RecordSource::Lit: return QStringLiteral("lit");
        case RecordSource::Selected: break;
    }
    return QStringLiteral("selected");
}

bool ShowTreeController::stringToRecordSource(const QString &str, RecordSource *source)
{
    const QString s = str.trimmed().toLower();
    RecordSource src;
    if (s == QLatin1String("selected") || s == QLatin1String("sel"))
        src = RecordSource::Selected;
    else if (s == QLatin1String("running") || s == QLatin1String("run"))
        src = RecordSource::Running;
    else if (s == QLatin1String("lit") || s == QLatin1String("everything") || s == QLatin1String("all"))
        src = RecordSource::Lit;
    else
        return false;
    if (source != nullptr)
        *source = src;
    return true;
}

quint32 ShowTreeController::runningCue(quint32 listId) const
{
    const CueList *list = cueList(listId);
    if (list == nullptr || list->chaser == invalidId())
        return invalidId();
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(list->chaser));
    if (chaser == nullptr || !chaser->isRunning())
        return invalidId();
    const int idx = chaser->currentStepIndex();
    ChaserStep *step = idx < 0 ? nullptr : chaser->stepAt(idx);
    if (step == nullptr)
        return invalidId();
    for (const Cue &c : list->cues)
        if (c.collection != invalidId() && c.collection == step->fid)
            return c.id;
    return invalidId();
}

/** A cue's stage as layers of its own: every layer that plays something,
    with the looks it plays (tracked, included and its own, resolved). */
static void stageLayers(const ResolvedCue &rc, QList<Layer> &into, const std::function<quint32()> &newId)
{
    for (const ResolvedLayer &rl : rc.layers)
    {
        // What is on stage: a layer that plays nothing (only None) isn't.
        if (!rl.target.isOpaque() && rl.isDark())
            continue;
        Layer *have = nullptr;
        for (Layer &l : into)
            if (l.target == rl.target)
                have = &l;
        if (have != nullptr)
        {
            // On stage twice (two lists): the later one wins, attribute by
            // attribute, as the later layer does.
            if (!rl.target.isOpaque())
                for (const Look &look : rl.playedLooks())
                    have->setLook(look);
            continue;
        }
        Layer l;
        l.id = newId();
        l.name = rl.name;
        l.target = rl.target;
        if (!rl.target.isOpaque())
            l.looks = rl.playedLooks();
        into << l;
    }
}

QList<Layer> ShowTreeController::recordLayers(quint32 cueId, RecordSource source, bool *available) const
{
    QList<Layer> out;
    if (available != nullptr)
        *available = false;
    const CueList *list = listOfCue(cueId);
    if (list == nullptr)
        return out;
    // Ids are given when the cue is made; 0 here, as a placeholder.
    const std::function<quint32()> noId = []() { return quint32(0); };

    auto cueStage = [&](const CueList &l, quint32 cid) {
        const int at = l.indexOf(cid);
        if (at >= 0)
            stageLayers(resolve(l).at(at), out, noId);
    };

    switch (source)
    {
        case RecordSource::Selected:
            cueStage(*list, cueId);
            if (available != nullptr)
                *available = true;
        break;
        case RecordSource::Running:
        {
            const quint32 running = runningCue(list->id);
            if (running == invalidId())
                return out;
            cueStage(*list, running);
            if (available != nullptr)
                *available = true;
        }
        break;
        case RecordSource::Lit:
        {
            // Every list running for real, in tree order; the Design preview
            // on top (it wins there); then anything else lit, as it is.
            QSet<quint32> functions;
            for (const Production &p : m_productions)
                for (const CueList &l : p.lists)
                {
                    const quint32 running = runningCue(l.id);
                    if (running != invalidId())
                        cueStage(l, running);
                    for (const Cue &c : l.cues)
                        for (const Layer &layer : c.layers)
                            if (layer.target.isOpaque())
                                functions.insert(layer.target.function);
                }
            if (m_previewState == PreviewState::Playing)
                if (const CueList *pl = listOfCue(m_previewCue))
                    cueStage(*pl, m_previewCue);
            for (const Layer &l : out)
                if (l.target.isOpaque())
                    functions.insert(l.target.function);
            QList<quint32> others;
            for (Function *f : m_doc->functions())
            {
                if (!f->isRunning() || isGenerated(f->id()) || functions.contains(f->id()))
                    continue;
                if (f->type() != Function::SceneType && f->type() != Function::EFXType
                    && f->type() != Function::RGBMatrixType)
                    continue;           // a chaser / collection / show: its steps are what's lit
                others << f->id();
            }
            std::sort(others.begin(), others.end());
            for (quint32 fid : others)
            {
                Layer l;
                l.target.function = fid;
                out << l;
            }
            if (available != nullptr)
                *available = true;
        }
        break;
    }
    return out;
}

/** Two recordings are the same when they put the same layers on stage. */
static QString layersSignature(const QList<Layer> &layers)
{
    QStringList parts;
    for (const Layer &l : layers)
    {
        QStringList looks;
        for (const Look &look : l.looks)
            looks << QStringLiteral("%1=%2/%3").arg(look.key, idsToString(look.palettes), idsToString(look.own));
        looks.sort();
        parts << l.target.signature() + QLatin1Char('{') + looks.join(QLatin1Char(';')) + QLatin1Char('}');
    }
    parts.sort();
    return parts.join(QLatin1Char(' '));
}

bool ShowTreeController::canRecord(quint32 cueId, RecordSource source) const
{
    bool available = false;
    const QList<Layer> layers = recordLayers(cueId, source, &available);
    return available && !layers.isEmpty();
}

QList<ShowTreeController::RecordSource> ShowTreeController::recordChoices(quint32 cueId) const
{
    QList<RecordSource> out;
    QStringList seen;
    for (RecordSource src : { RecordSource::Selected, RecordSource::Running, RecordSource::Lit })
    {
        bool available = false;
        const QList<Layer> layers = recordLayers(cueId, src, &available);
        if (!available || layers.isEmpty())
            continue;
        const QString sig = layersSignature(layers);
        if (seen.contains(sig))
            continue;
        seen << sig;
        out << src;
    }
    return out;
}

quint32 ShowTreeController::recordCue(quint32 cueId, RecordSource source)
{
    const CueList *have = listOfCue(cueId);
    if (have == nullptr)
        return invalidId();
    bool available = false;
    QList<Layer> layers = recordLayers(cueId, source, &available);
    if (!available)
        return invalidId();
    const int at = have->indexOf(cueId);
    // Named and timed after the cue it records (the running one), or the
    // selected one for everything lit.
    const quint32 from = source == RecordSource::Running ? runningCue(have->id) : cueId;
    const Cue &src = *have->cue(from);

    Cue rec;
    rec.id = newId();
    rec.number = numberForInsert(*have, at);
    if (source == RecordSource::Lit)
        rec.name = tr("on stage (recorded)");
    else
        rec.name = src.name.isEmpty() ? tr("recorded") : tr("%1 (recorded)").arg(src.name);
    rec.mode = CueMode::Block;
    rec.fadeIn = src.fadeIn;
    rec.fadeOut = src.fadeOut;
    rec.hold = src.hold;
    for (Layer &l : layers)
        l.id = newId();
    rec.layers = layers;

    QString label = tr("Record as new cue");
    if (source == RecordSource::Running)
        label = tr("Record as new cue: running %1").arg(cueLabel(*have, have->indexOf(from)));
    else if (source == RecordSource::Lit)
        label = tr("Record as new cue: everything lit");
    const quint32 listId = have->id;
    const bool ok = edit(label, [&](QList<Production> &ps) {
        CueList *l = listIn(ps, listId);
        if (l == nullptr)
            return false;
        l->cues.insert(at + 1, rec);
        return true;
    });
    return ok ? rec.id : invalidId();
}

/****************************************************************************
 * Split and merge
 ****************************************************************************/

/** Whether two targets light a unit in common (a group or a fixture). */
static bool overlaps(const Target &a, const Target &b)
{
    for (quint32 g : a.groups)
        if (b.groups.contains(g))
            return true;
    for (quint32 f : a.fixtures)
        if (b.fixtures.contains(f))
            return true;
    return false;
}

/** The carried-in layers an own layer stands for in its cue. */
static QList<Target> carriedFrom(const Layer &layer, const ResolvedLayer *rl)
{
    QList<Target> out;
    auto add = [&out](const Target &t) {
        if (!t.isEmpty() && !out.contains(t))
            out << t;
    };
    if (rl == nullptr || rl->tookOver.isEmpty())
        return out;
    add(rl->tookOver);
    add(layer.continues);
    for (const Target &t : layer.continuesAlso)
        add(t);
    return out;
}

/** In layers carrying on from @a from, carry on from @a to instead. */
static void replaceContinues(Cue &cue, const Target &from, const QList<Target> &to)
{
    for (Layer &other : cue.layers)
    {
        QList<Target> all;
        if (!other.continues.isEmpty())
            all << other.continues;
        all << other.continuesAlso;
        const int at = all.indexOf(from);
        if (at < 0)
            continue;
        all.removeAt(at);
        for (int i = 0; i < to.count(); ++i)
            if (!all.contains(to.at(i)) && !(to.at(i) == other.target))
                all.insert(at + i, to.at(i));
        other.continues = all.isEmpty() ? Target() : all.takeFirst();
        other.continuesAlso = all;
    }
}

QString ShowTreeController::unitLabel(const QString &unit) const
{
    const quint32 id = unit.mid(2).toUInt();
    Target t;
    if (unit.startsWith(QLatin1String("g:")))
        t.groups << id;
    else if (unit.startsWith(QLatin1String("f:")))
        t.fixtures << id;
    else
        t.function = id;
    return targetLabel(t);
}

bool ShowTreeController::splitLayer(quint32 cueId, quint32 layerId, QList<quint32> *made, QString *why)
{
    const Cue *c = cue(cueId);
    const Layer *l = c == nullptr ? nullptr : c->layer(layerId);
    if (l == nullptr || l->target.isOpaque())
    {
        if (why != nullptr)
            *why = tr("Only a layer on groups or fixtures can be split.");
        return false;
    }
    const QList<Target> pieces = l->pieces();
    if (pieces.count() < 2)
    {
        if (why != nullptr)
            *why = tr("%1 is one group already.").arg(targetLabel(l->target));
        return false;
    }
    const QString label = tr("Split layer %1").arg(l->name.isEmpty() ? targetLabel(l->target) : l->name);
    QString refusal;
    QList<quint32> ids;

    const bool ok = edit(label, [&](QList<Production> &ps) {
        CueList *list = nullptr;
        if (cueIn(ps, cueId, &list) == nullptr)
            return false;
        const CueList beforeList = *list;
        const QList<ResolvedCue> before = resolve(beforeList);
        const int at = list->indexOf(cueId);
        const Target from = l->target;

        // Cut the layer in @a cue into pieces, each with its looks, release
        // and name; each carries on from what it overlaps of @a carried.
        auto cut = [&](Cue &cue, quint32 lid, const QList<Target> &carried) {
            int idx = -1;
            for (int i = 0; i < cue.layers.count(); ++i)
                if (cue.layers.at(i).id == lid)
                    idx = i;
            if (idx < 0)
                return;
            const Layer orig = cue.layers.takeAt(idx);
            for (int p = 0; p < pieces.count(); ++p)
            {
                Layer piece = orig;
                piece.id = p == 0 ? orig.id : newId();
                piece.target = pieces.at(p);
                QList<Target> cont;
                for (const Target &t : carried)
                    if (overlaps(t, piece.target) && !(t == piece.target))
                        cont << t;
                piece.continues = cont.isEmpty() ? Target() : cont.takeFirst();
                piece.continuesAlso = cont;
                cue.layers.insert(idx + p, piece);
                if (cue.id == cueId)
                    ids << piece.id;
            }
        };

        cut(list->cues[at], layerId, carriedFrom(*l, before.at(at).layer(from)));

        // Onwards, while it tracks: the same layer set again is split too; a
        // layer carrying on from it under another target carries on from
        // the pieces.
        for (int j = at + 1; j < list->cues.count(); ++j)
        {
            const ResolvedLayer *next = before.at(j).layer(from);
            if (next == nullptr)
            {
                replaceContinues(list->cues[j], from, pieces);
                break;
            }
            if (!next->own)
                continue;
            if (!(next->tookOver == from))
                break;                      // a fresh layer on that target
            const Layer *again = list->cues.at(j).layer(next->layerId);
            if (again == nullptr)
                break;
            if (again->target == from)
            {
                cut(list->cues[j], again->id, QList<Target>());
                continue;
            }
            replaceContinues(list->cues[j], from, pieces);
            break;
        }

        quint32 cid = invalidId();
        QString unit, key;
        if (stageDiffers(beforeList, *list, &cid, &unit, &key))
        {
            refusal = tr("Splitting would change what cue %1 plays on %2 (%3), so it was left as it is.")
                      .arg(cueLabel(beforeList, beforeList.indexOf(cid)), unitLabel(unit), keyLabel(key));
            return false;
        }
        return true;
    });
    if (why != nullptr)
        *why = refusal;
    if (ok && made != nullptr)
        *made = ids;
    return ok;
}

quint32 ShowTreeController::mergeLayers(quint32 cueId, const QList<quint32> &layerIds, QString *why)
{
    auto refuse = [why](const QString &text) {
        if (why != nullptr)
            *why = text;
        return invalidId();
    };
    const Cue *c = cue(cueId);
    if (c == nullptr)
        return refuse(tr("No such cue."));
    QList<const Layer *> layers;
    for (quint32 lid : layerIds)
    {
        const Layer *l = c->layer(lid);
        if (l == nullptr)
            return refuse(tr("Only layers of the cue itself can be merged."));
        if (l->target.isOpaque())
            return refuse(tr("%1 plays a function as it is: it can't be merged.").arg(targetLabel(l->target)));
        if (!layers.contains(l))
            layers << l;
    }
    if (layers.count() < 2)
        return refuse(tr("Select two or more layers of one cue to merge."));

    auto layerName = [this](const Layer &l) { return l.name.isEmpty() ? targetLabel(l.target) : l.name; };
    auto lookName = [this](const Look &l) {
        if (l.none)
            return tr("None");
        QStringList n;
        for (quint32 pid : l.palettes)
            if (const QLCPalette *p = m_doc->palette(pid))
                n << p->name();
        return n.isEmpty() ? tr("nothing") : n.join(QStringLiteral(" + "));
    };

    // One layer from several: they must agree on what they set (the safe
    // rule -- nothing is guessed), and on Release.
    auto combine = [&](const QList<Layer> &from, Layer *into, QString *conflict) -> bool {
        Layer m = from.first();
        m.continues = Target();
        m.continuesAlso.clear();
        for (int i = 1; i < from.count(); ++i)
        {
            const Layer &o = from.at(i);
            for (quint32 g : o.target.groups)
                if (!m.target.groups.contains(g))
                    m.target.groups << g;
            for (quint32 f : o.target.fixtures)
                if (!m.target.fixtures.contains(f))
                    m.target.fixtures << f;
            if (o.release != from.first().release)
            {
                *conflict = tr("%1 is released and %2 isn't.")
                            .arg(layerName(from.first().release ? from.first() : o),
                                 layerName(from.first().release ? o : from.first()));
                return false;
            }
            if (o.name != m.name)
                m.name.clear();
            for (const Look &look : o.looks)
            {
                for (int k = 0; k < i; ++k)
                {
                    const Look *was = from.at(k).look(look.key);
                    if (was != nullptr && *was != look)
                    {
                        *conflict = tr("%1 sets %2 to %3, %4 to %5.")
                                    .arg(layerName(from.at(k)), keyLabel(look.key), lookName(*was),
                                         layerName(o), lookName(look));
                        return false;
                    }
                }
                if (m.look(look.key) == nullptr)
                    m.setLook(look);
            }
        }
        *into = m;
        return true;
    };

    QList<Layer> chosen;
    for (const Layer *l : layers)
        chosen << *l;
    Layer merged;
    QString conflict;
    if (!combine(chosen, &merged, &conflict))
        return refuse(tr("Can't merge: %1 Make them the same first.").arg(conflict));

    QStringList names;
    for (const Layer *l : layers)
        names << layerName(*l);
    const QString label = tr("Merge layers %1").arg(names.join(QStringLiteral(", ")));
    QString refusal;
    QList<Target> targets;
    for (const Layer *l : layers)
        targets << l->target;
    const quint32 mergedId = layers.first()->id;

    const bool ok = edit(label, [&](QList<Production> &ps) {
        CueList *list = nullptr;
        if (cueIn(ps, cueId, &list) == nullptr)
            return false;
        const CueList beforeList = *list;
        const QList<ResolvedCue> before = resolve(beforeList);
        const int at = list->indexOf(cueId);

        // Put @a m in place of layers @a ids of @a cue (at the first one).
        auto replace = [](Cue &cue, const QList<quint32> &ids, const Layer &m) {
            int first = -1;
            for (int i = cue.layers.count() - 1; i >= 0; --i)
                if (ids.contains(cue.layers.at(i).id))
                {
                    first = i;
                    cue.layers.removeAt(i);
                }
            cue.layers.insert(qMax(0, first), m);
        };

        // In this cue: it carries on from everything they carried on from.
        QList<Target> carried;
        for (const Layer *l : layers)
            for (const Target &t : carriedFrom(*l, before.at(at).layer(l->target)))
                if (!carried.contains(t) && !(t == merged.target))
                    carried << t;
        Layer m = merged;
        m.continues = carried.isEmpty() ? Target() : carried.takeFirst();
        m.continuesAlso = carried;
        replace(list->cues[at], layerIds, m);

        // Onwards, while they track together: set again together, merged
        // there too; carried on from under another target, from the merge.
        for (int j = at + 1; j < list->cues.count(); ++j)
        {
            QList<const ResolvedLayer *> next;
            int missing = 0, own = 0;
            for (const Target &t : targets)
            {
                const ResolvedLayer *rl = before.at(j).layer(t);
                next << rl;
                if (rl == nullptr)
                    ++missing;
                else if (rl->own)
                    ++own;
            }
            if (missing == targets.count())
            {
                for (const Target &t : targets)
                    replaceContinues(list->cues[j], t, { merged.target });
                break;
            }
            if (missing > 0)
                break;                      // they part here (the check says if that matters)
            if (own == 0)
                continue;                   // carried through together
            if (own < targets.count())
                break;
            QList<Layer> again;
            QList<quint32> againIds;
            bool same = true;
            for (int k = 0; k < next.count(); ++k)
            {
                const Layer *l = list->cues.at(j).layer(next.at(k)->layerId);
                if (l == nullptr || !(next.at(k)->tookOver == targets.at(k)) || !(l->target == targets.at(k)))
                {
                    same = false;
                    break;
                }
                again << *l;
                againIds << l->id;
            }
            if (!same)
                break;
            Layer mj;
            QString clash;
            if (!combine(again, &mj, &clash))
            {
                refusal = tr("Can't merge: in cue %1 %2").arg(cueLabel(beforeList, j), clash);
                return false;
            }
            replace(list->cues[j], againIds, mj);
        }

        quint32 cid = invalidId();
        QString unit, key;
        if (stageDiffers(beforeList, *list, &cid, &unit, &key))
        {
            refusal = tr("Merging would change what cue %1 plays on %2 (%3), so it was left as it is.")
                      .arg(cueLabel(beforeList, beforeList.indexOf(cid)), unitLabel(unit), keyLabel(key));
            return false;
        }
        return true;
    });
    if (!ok)
        return refuse(refusal.isEmpty() ? tr("Nothing to merge.") : refusal);
    return mergedId;
}

bool ShowTreeController::valueSource(quint32 cueId, const Target &target, const QString &key,
                                     quint32 *sourceCue, quint32 *sourceLayer) const
{
    const CueList *list = listOfCue(cueId);
    if (list == nullptr || target.isEmpty())
        return false;
    const QList<ResolvedCue> resolved = resolve(*list);

    quint32 cur = cueId;
    Target t = target;
    quint32 layerId = invalidId();
    // Back along where it came from: tracked, included, base. Every step
    // goes to another cue, so a list's length of steps is enough.
    for (int guard = 0; guard <= list->cues.count(); ++guard)
    {
        const int idx = list->indexOf(cur);
        if (idx < 0)
            return false;
        const ResolvedLayer *rl = resolved.at(idx).layer(t);
        if (rl == nullptr && layerId != invalidId())
        {
            // The layer is on another target there (a group added later on).
            for (const ResolvedLayer &l : resolved.at(idx).layers)
                if (l.layerId == layerId)
                    rl = &l;
        }
        if (rl == nullptr || !rl->values.contains(key))
            return false;
        const ResolvedValue &v = rl->values.value(key);
        if (v.origin == Origin::Own || v.origin == Origin::CueLook)
        {
            if (cur == cueId)
                return false;           // set in this cue: nothing to update elsewhere
            if (sourceCue != nullptr)
                *sourceCue = cur;
            if (sourceLayer != nullptr)
                *sourceLayer = v.origin == Origin::Own ? rl->layerId : invalidId();
            return true;
        }
        if (v.fromCue == cur || v.fromCue == invalidId())
            return false;
        t = rl->target;
        layerId = rl->layerId;
        cur = v.fromCue;
    }
    return false;
}

bool ShowTreeController::updateSource(quint32 cueId, const Target &target, const Look &look)
{
    quint32 srcCue = invalidId(), srcLayer = invalidId();
    if (!valueSource(cueId, target, look.key, &srcCue, &srcLayer))
        return false;
    const CueList *list = listOfCue(srcCue);
    const QString label = tr("Update cue %1: %2").arg(cueLabel(*list, list->indexOf(srcCue)),
                                                      keyLabel(look.key));
    return edit(label, [&](QList<Production> &ps) {
        Cue *c = cueIn(ps, srcCue);
        if (c == nullptr)
            return false;
        if (srcLayer != invalidId())
        {
            Layer *l = c->layer(srcLayer);
            if (l == nullptr || l->target.isOpaque())
                return false;
            const Look *was = l->look(look.key);
            if (was != nullptr && *was == look)
                return false;
            l->setLook(look);
            return true;
        }
        // A cue look there: the cue's look changes (it reaches that cue's
        // own layers that don't set it themselves).
        const Look *was = c->look(look.key);
        if (was != nullptr && *was == look)
            return false;
        c->setLook(look);
        return true;
    });
}

ShowTreeController::Usage ShowTreeController::usageInList(quint32 listId) const
{
    Usage u;
    const CueList *list = cueList(listId);
    if (list == nullptr)
        return u;
    auto looks = [&u](const QList<Look> &ls) {
        for (const Look &l : ls)
        {
            for (quint32 p : l.palettes)
                u.palettes[p]++;
            for (quint32 p : l.own)
                u.palettes[p]++;
        }
    };
    for (const Cue &c : list->cues)
    {
        looks(c.looks);
        for (const Layer &l : c.layers)
        {
            looks(l.looks);
            if (l.target.isOpaque())
                u.functions[l.target.function]++;
            for (quint32 g : l.target.groups)
                u.groups[g]++;
            for (quint32 f : l.target.fixtures)
                u.fixtures[f]++;
        }
    }
    return u;
}

bool ShowTreeController::renameNode(quint32 nodeId, const QString &name)
{
    return edit(tr("Rename"), [&](QList<Production> &ps) {
        for (Production &p : ps)
        {
            if (p.id == nodeId)
            {
                p.name = name;
                return true;
            }
            for (CueList &l : p.lists)
            {
                if (l.id == nodeId)
                {
                    l.name = name;
                    return true;
                }
                for (Cue &c : l.cues)
                {
                    if (c.id == nodeId)
                    {
                        c.name = name;
                        return true;
                    }
                    if (Layer *layer = c.layer(nodeId))
                    {
                        layer->name = name;
                        return true;
                    }
                }
            }
        }
        return false;
    });
}

bool ShowTreeController::removeCueList(quint32 listId)
{
    return edit(tr("Delete cue list"), [&](QList<Production> &ps) {
        for (Production &p : ps)
            for (int i = 0; i < p.lists.count(); ++i)
                if (p.lists.at(i).id == listId)
                {
                    p.lists.removeAt(i);
                    return true;
                }
        return false;
    });
}

bool ShowTreeController::removeProduction(quint32 productionId)
{
    return edit(tr("Delete production"), [&](QList<Production> &ps) {
        for (int i = 0; i < ps.count(); ++i)
            if (ps.at(i).id == productionId)
            {
                ps.removeAt(i);
                return true;
            }
        return false;
    });
}

/****************************************************************************
 * Looks and palettes
 ****************************************************************************/

QString ShowTreeController::effectKind(const QLCPalette *palette) const
{
    if (palette == nullptr || palette->type() != QLCPalette::Effect)
        return QString();
    EffectScriptRunner *runner = m_doc->effectScriptRunner();
    if (runner == nullptr || palette->scriptPath().isEmpty())
        return QStringLiteral("Other");
    const EffectScriptCache::ScriptMeta meta =
        runner->cache()->scriptMeta(EffectScriptCache::nameFromPath(palette->scriptPath()));
    const QString kind = EffectScriptCache::categoryForTypes(meta.fixtureTypes);
    return kind.isEmpty() ? QStringLiteral("Other") : kind;
}

QString ShowTreeController::keyForPalette(quint32 paletteId) const
{
    const QLCPalette *p = m_doc->palette(paletteId);
    if (p == nullptr)
        return QString();
    switch (p->type())
    {
        case QLCPalette::Color: return KeyColour;
        case QLCPalette::Dimmer: return KeyDimmer;
        case QLCPalette::Pan:
        case QLCPalette::Tilt:
        case QLCPalette::PanTilt:
        case QLCPalette::Aim: return KeyPosition;
        case QLCPalette::Shutter:
        case QLCPalette::Strobe: return KeyShutter;
        case QLCPalette::Gobo: return KeyGobo;
        case QLCPalette::Zoom: return KeyZoom;
        case QLCPalette::Beam: return KeyBeam;
        case QLCPalette::Effect: return effectKey(effectKind(p));
        case QLCPalette::Undefined: break;
    }
    return QStringLiteral("Other");
}

Look ShowTreeController::lookForPalette(quint32 paletteId) const
{
    Look l;
    l.key = keyForPalette(paletteId);
    if (!l.key.isEmpty())
        l.palettes << paletteId;
    return l;
}

quint32 ShowTreeController::createInlinePalette(QLCPalette::PaletteType type,
                                                const QVariantList &values,
                                                const QString &label)
{
    QLCPalette *p = new QLCPalette(type, m_doc);
    p->setName(label);
    p->setPath(QStringLiteral("Palettes/%1").arg(kInlinePaletteFolder));
    p->setValues(values);
    // Known as inline before Doc announces it, so the palette trees can
    // leave it out from the start.
    quint32 id = 0;
    for (const QLCPalette *other : m_doc->palettes())
        id = qMax(id, other->id() + 1);
    m_inlinePalettes.insert(id);
    if (!m_doc->addPalette(p, id))
    {
        m_inlinePalettes.remove(id);
        delete p;
        return QLCPalette::invalidId();
    }
    return id;
}

bool ShowTreeController::makeReusable(quint32 paletteId, const QString &name, const QString &path)
{
    QLCPalette *p = m_doc->palette(paletteId);
    if (p == nullptr || !m_inlinePalettes.contains(paletteId))
        return false;
    m_inlinePalettes.remove(paletteId);
    p->setName(name);
    p->setPath(path);
    m_doc->setModified();
    emit changed();
    return true;
}

bool ShowTreeController::makeReusable(quint32 paletteId)
{
    const QLCPalette *p = m_doc->palette(paletteId);
    if (p == nullptr || !m_inlinePalettes.contains(paletteId))
        return false;
    // Named as if it were new: its own one-off name is not "taken".
    m_inlinePalettes.remove(paletteId);
    const QString name = suggestPaletteName(p->type(), p->values());
    m_inlinePalettes.insert(paletteId);
    return makeReusable(paletteId, name, suggestPaletteFolder(p->type()));
}

/** The level a Dimmer value is, as the picker and "dimmer 45" say it. */
static int levelPercent(int value)
{
    return qRound(value * 100.0 / 255.0);
}

/** Whether a palette of @a type plays exactly @a values. */
static bool playsExactly(const QLCPalette *p, QLCPalette::PaletteType type, const QVariantList &values)
{
    if (p->type() != type || values.isEmpty() || p->values().isEmpty()
        || p->fanningType() != QLCPalette::Flat)
        return false;
    if (type == QLCPalette::Dimmer)
    {
        bool ok1 = false, ok2 = false;
        const int a = p->values().first().toInt(&ok1);
        const int b = values.first().toInt(&ok2);
        return ok1 && ok2 && a == b;
    }
    if (type == QLCPalette::Color)
    {
        QColor ra, wa, rb, wb;
        if (!QLCPalette::stringToColor(p->values().first().toString(), ra, wa)
            || !QLCPalette::stringToColor(values.first().toString(), rb, wb))
            return false;
        const QColor black(Qt::black);
        return ra.rgb() == rb.rgb()
               && (wa.isValid() ? wa : black).rgb() == (wb.isValid() ? wb : black).rgb();
    }
    return false;
}

quint32 ShowTreeController::findPalette(QLCPalette::PaletteType type, const QVariantList &values,
                                        quint32 listId, const QList<quint32> &recent) const
{
    QList<const QLCPalette *> match;
    for (const QLCPalette *p : m_doc->palettes())
    {
        const bool madeNow = m_batchNamed.contains(p->id());
        if ((m_inlinePalettes.contains(p->id()) && !madeNow) || p->name().isEmpty())
            continue;
        if (playsExactly(p, type, values))
            match << p;
    }
    if (match.isEmpty())
        return QLCPalette::invalidId();

    // 1. Already in this cue list: the most used, then the lowest id.
    const Usage usage = usageInList(listId);
    const QLCPalette *best = nullptr;
    for (const QLCPalette *p : match)
    {
        const int n = usage.palettes.value(p->id());
        if (n == 0)
            continue;
        if (best == nullptr || n > usage.palettes.value(best->id())
            || (n == usage.palettes.value(best->id()) && p->id() < best->id()))
            best = p;
    }
    if (best != nullptr)
        return best->id();
    // 2. Recent, newest first.
    for (quint32 pid : recent)
        for (const QLCPalette *p : match)
            if (p->id() == pid)
                return pid;
    // 3. Any: by name, then id.
    std::sort(match.begin(), match.end(), [](const QLCPalette *a, const QLCPalette *b) {
        const int c = a->name().compare(b->name(), Qt::CaseInsensitive);
        return c != 0 ? c < 0 : a->id() < b->id();
    });
    return match.first()->id();
}

/** "Palettes/Color/" and "Palettes/Color" are the same folder. */
static QString cleanFolder(QString path)
{
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    return path;
}

QString ShowTreeController::suggestPaletteFolder(QLCPalette::PaletteType type) const
{
    QMap<QString, int> count;
    for (const QLCPalette *p : m_doc->palettes())
        if (p->type() == type && !m_inlinePalettes.contains(p->id()) && !p->name().isEmpty())
            count[cleanFolder(p->path())]++;
    QString best;
    for (auto it = count.constBegin(); it != count.constEnd(); ++it)
    {
        // The most used; on a tie the shorter (the type's own folder over a
        // song's sub-folder), then by name (QMap order).
        if (best.isNull() || it.value() > count.value(best)
            || (it.value() == count.value(best) && it.key().length() < best.length()))
            best = it.key();
    }
    if (best.isEmpty())
        return QStringLiteral("Palettes/%1").arg(QLCPalette::typeToString(type));
    return best;
}

/** A colour's name as a person says it: "Medium Sea Green", and whether it
    is exactly that colour (else the nearest). */
static QString colourName(const QColor &c, bool *exact)
{
    QString best;
    int bestDist = INT_MAX;
    // Sorted, so the same colour always gets the same name ("aqua" before
    // "cyan").
    QStringList names = QColor::colorNames();
    names.sort();
    for (const QString &n : names)
    {
        const QColor k = QColor::fromString(n);
        if (k.alpha() != 255)
            continue;           // "transparent"
        const int dr = k.red() - c.red(), dg = k.green() - c.green(), db = k.blue() - c.blue();
        const int dist = dr * dr + dg * dg + db * db;
        if (dist < bestDist)
        {
            bestDist = dist;
            best = n;
        }
    }
    if (exact != nullptr)
        *exact = bestDist == 0;
    // "mediumseagreen" -> "Medium Sea Green": split before the known words.
    static const QStringList words = {
        "alice", "antique", "aqua", "marine", "azure", "beige", "bisque", "black", "blanched", "almond",
        "blue", "violet", "brown", "burly", "wood", "cadet", "chartreuse", "chocolate", "coral",
        "cornflower", "cornsilk", "crimson", "cyan", "dark", "deep", "dim", "dodger", "fire", "brick",
        "floral", "white", "forest", "green", "fuchsia", "gainsboro", "ghost", "gold", "rod", "gray",
        "grey", "yellow", "honeydew", "hot", "pink", "indian", "red", "indigo", "ivory", "khaki",
        "lavender", "blush", "lawn", "lemon", "chiffon", "light", "lime", "linen", "magenta", "maroon",
        "medium", "orchid", "purple", "sea", "slate", "spring", "turquoise", "midnight", "mint", "cream",
        "misty", "rose", "moccasin", "navajo", "navy", "old", "lace", "olive", "drab", "orange", "pale",
        "papaya", "whip", "peach", "puff", "peru", "plum", "powder", "rebecca", "rosy", "royal", "saddle",
        "salmon", "sandy", "shell", "sienna", "silver", "sky", "snow", "steel", "tan", "teal", "thistle",
        "tomato", "wheat", "smoke", "goldenrod" };
    QStringList parts;
    QString rest = best;
    while (!rest.isEmpty())
    {
        QString w;
        for (const QString &k : words)
            if (rest.startsWith(k) && k.length() > w.length())
                w = k;
        if (w.isEmpty())
            w = rest;
        parts << w.left(1).toUpper() + w.mid(1);
        rest = rest.mid(w.length());
    }
    return parts.join(QLatin1Char(' '));
}

QString ShowTreeController::suggestPaletteName(QLCPalette::PaletteType type, const QVariantList &values) const
{
    QList<const QLCPalette *> siblings;
    QStringList taken;
    for (const QLCPalette *p : m_doc->palettes())
    {
        if (p->type() != type || p->name().isEmpty())
            continue;
        if (!m_inlinePalettes.contains(p->id()) || m_batchNamed.contains(p->id()))
            taken << p->name();
        if (!m_inlinePalettes.contains(p->id()) && p->fanningType() == QLCPalette::Flat && !p->values().isEmpty())
            siblings << p;
    }
    std::sort(siblings.begin(), siblings.end(), [](const QLCPalette *a, const QLCPalette *b) {
        return a->id() < b->id();
    });

    // A sibling whose name holds its own value gives the pattern, "{}" where
    // the value goes; the most common pattern wins (ties: the oldest).
    auto pattern = [&](const std::function<QStringList(const QLCPalette *)> &spellings) {
        QMap<QString, int> count;
        QStringList order;
        for (const QLCPalette *p : siblings)
        {
            const QStringList spelt = spellings(p);
            for (int si = 0; si < spelt.count(); ++si)
            {
                const QString &v = spelt.at(si);
                if (v.isEmpty())
                    continue;
                const QRegularExpression re(QStringLiteral("(?<![0-9A-Za-z])%1(?![0-9A-Za-z])")
                                            .arg(QRegularExpression::escape(v)),
                                            QRegularExpression::CaseInsensitiveOption);
                const QRegularExpressionMatch m = re.match(p->name());
                if (!m.hasMatch())
                    continue;
                QString t = p->name();
                t.replace(m.capturedStart(), m.capturedLength(), QStringLiteral("{%1}").arg(si));
                if (!order.contains(t))
                    order << t;
                count[t]++;
                break;
            }
        }
        QString best;
        for (const QString &t : order)
            if (best.isNull() || count.value(t) > count.value(best))
                best = t;
        return best;
    };

    QString name;
    if (type == QLCPalette::Dimmer)
    {
        const int pc = levelPercent(values.value(0).toInt());
        const QString t = pattern([](const QLCPalette *p) {
            return QStringList { QString::number(levelPercent(p->values().first().toInt())) };
        });
        name = t.isEmpty() ? QStringLiteral("Dimmer %1%").arg(pc)
                           : QString(t).replace(QStringLiteral("{0}"), QString::number(pc));
    }
    else if (type == QLCPalette::Color)
    {
        QColor rgb, wauv;
        QLCPalette::stringToColor(values.value(0).toString(), rgb, wauv);
        const QString hex = rgb.name();                   // "#33aa55"
        // Siblings write the hex with its '#' ({0}) or without ({1}).
        const QString t = pattern([](const QLCPalette *p) {
            QColor r, w;
            if (!QLCPalette::stringToColor(p->values().first().toString(), r, w))
                return QStringList();
            return QStringList { r.name(), r.name().mid(1) };
        });
        if (!t.isEmpty())
        {
            name = QString(t).replace(QStringLiteral("{0}"), hex).replace(QStringLiteral("{1}"), hex.mid(1));
        }
        else
        {
            // Palettes named by colour ("Red", "Candle Orange"): its name, or
            // the nearest with its hex when it is only near.
            bool exact = false;
            const QString nm = colourName(rgb, &exact);
            name = exact ? nm : QStringLiteral("%1 %2").arg(nm, hex);
        }
    }
    else
    {
        name = QLCPalette::typeToString(type);
    }

    // Never a name that type already has.
    QString unique = name;
    for (int n = 2; taken.contains(unique, Qt::CaseInsensitive); ++n)
        unique = QStringLiteral("%1 (%2)").arg(name).arg(n);
    return unique;
}

quint32 ShowTreeController::createNamedPalette(QLCPalette::PaletteType type, const QVariantList &values)
{
    const QString name = suggestPaletteName(type, values);
    const QString folder = suggestPaletteFolder(type);
    // Made the way a one-off is (so a failed command drops it), then made
    // reusable: at once, or when the command succeeds.
    const quint32 pid = createInlinePalette(type, values, name);
    if (pid == QLCPalette::invalidId())
        return pid;
    if (QLCPalette *p = m_doc->palette(pid))
        p->setPath(folder);
    if (m_batchDepth > 0)
    {
        m_batchNamed << pid;
        return pid;
    }
    m_inlinePalettes.remove(pid);
    m_doc->setModified();
    emit changed();
    return pid;
}

static QList<quint32> targetFixtures(const Doc *doc, const Target &target)
{
    QList<quint32> ids;
    for (quint32 g : target.groups)
    {
        const FixtureGroup *grp = doc->fixtureGroup(g);
        if (grp == nullptr)
            continue;
        for (quint32 f : grp->fixtureList())
            if (!ids.contains(f))
                ids << f;
    }
    for (quint32 f : target.fixtures)
        if (!ids.contains(f))
            ids << f;
    return ids;
}

static bool fixtureHasGroup(const Fixture *fxi, QLCChannel::Group group)
{
    for (quint32 i = 0; i < fxi->channels(); ++i)
    {
        const QLCChannel *ch = fxi->channel(i);
        if (ch != nullptr && ch->group() == group)
            return true;
    }
    return false;
}

static bool fixtureReaches(const Fixture *fxi, const QString &key)
{
    if (fxi == nullptr)
        return false;
    if (key == KeyGobo)
        return fixtureHasGroup(fxi, QLCChannel::Gobo);
    if (key == KeyPosition)
        return fixtureHasGroup(fxi, QLCChannel::Pan) || fixtureHasGroup(fxi, QLCChannel::Tilt);
    if (key == KeyZoom || key == KeyBeam)
        return fixtureHasGroup(fxi, QLCChannel::Beam);
    if (key == KeyShutter)
        return fixtureHasGroup(fxi, QLCChannel::Shutter) || fixtureHasGroup(fxi, QLCChannel::Intensity);
    if (key == KeyColour)
    {
        if (fixtureHasGroup(fxi, QLCChannel::Colour))
            return true;
        for (quint32 i = 0; i < fxi->channels(); ++i)
        {
            const QLCChannel *ch = fxi->channel(i);
            if (ch != nullptr && ch->group() == QLCChannel::Intensity
                && ch->colour() != QLCChannel::NoColour)
                return true;
        }
        return false;
    }
    return true;
}

void ShowTreeController::lookReach(const Target &target, const QString &key, int *reach, int *total) const
{
    const QList<quint32> ids = targetFixtures(m_doc, target);
    int n = 0;
    for (quint32 id : ids)
        if (fixtureReaches(m_doc->fixture(id), key))
            ++n;
    if (reach != nullptr)
        *reach = n;
    if (total != nullptr)
        *total = int(ids.count());
}

QList<quint32> ShowTreeController::lookMisses(const Target &target, const QString &key) const
{
    QList<quint32> out;
    for (quint32 id : targetFixtures(m_doc, target))
        if (!fixtureReaches(m_doc->fixture(id), key))
            out << id;
    return out;
}

/** Whether @a look itself reaches @a fxi: its attribute's channels, and
    for a Gobo look a slot on that fixture's wheel. */
static bool lookReachesFixture(const Doc *doc, const Fixture *fxi, const Look &look)
{
    if (!fixtureReaches(fxi, look.key))
        return false;
    if (look.key != KeyGobo || look.none || look.palettes.isEmpty())
        return true;
    for (quint32 pid : look.palettes)
    {
        const QLCPalette *p = const_cast<Doc *>(doc)->palette(pid);
        if (p == nullptr || p->type() != QLCPalette::Gobo || p->goboValueFor(fxi) >= 0)
            return true;
    }
    return false;
}

void ShowTreeController::lookReach(const Target &target, const Look &look, int *reach, int *total) const
{
    const QList<quint32> ids = targetFixtures(m_doc, target);
    int n = 0;
    for (quint32 id : ids)
        if (lookReachesFixture(m_doc, m_doc->fixture(id), look))
            ++n;
    if (reach != nullptr)
        *reach = n;
    if (total != nullptr)
        *total = int(ids.count());
}

QList<quint32> ShowTreeController::lookMisses(const Target &target, const Look &look) const
{
    QList<quint32> out;
    for (quint32 id : targetFixtures(m_doc, target))
        if (!lookReachesFixture(m_doc, m_doc->fixture(id), look))
            out << id;
    return out;
}

QStringList ShowTreeController::lookAmbiguities(const Target &target, const Look &look) const
{
    QStringList out;
    if (look.key != KeyGobo || look.none || look.palettes.isEmpty())
        return out;
    QSet<QString> seen;
    for (quint32 id : targetFixtures(m_doc, target))
    {
        const Fixture *fxi = m_doc->fixture(id);
        const QString def = QLCPalette::definitionKey(fxi);
        if (fxi == nullptr || seen.contains(def) || lookReachesFixture(m_doc, fxi, look))
            continue;
        for (quint32 pid : look.palettes)
        {
            const QLCPalette *p = m_doc->palette(pid);
            if (p == nullptr)
                continue;
            const QLCPalette::GoboResolution r = p->resolveGobo(fxi);
            if (r.how != QLCPalette::GoboAmbiguous)
                continue;
            seen.insert(def);
            const QString model = fxi->fixtureDef() != nullptr ? fxi->fixtureDef()->model() : fxi->name();
            out << tr("%1: %2").arg(model, r.candidates.join(QStringLiteral(" / ")));
            break;
        }
    }
    return out;
}

QString ShowTreeController::targetLabel(const Target &target) const
{
    if (target.isOpaque())
    {
        const Function *f = m_doc->function(target.function);
        return f == nullptr ? tr("(missing function)") : f->name();
    }
    QStringList names;
    for (quint32 g : target.groups)
    {
        const FixtureGroup *grp = m_doc->fixtureGroup(g);
        names << (grp == nullptr ? tr("(missing group)") : grp->name());
    }
    for (quint32 f : target.fixtures)
    {
        const Fixture *fxi = m_doc->fixture(f);
        names << (fxi == nullptr ? tr("(missing fixture)") : fxi->name());
    }
    return names.join(QStringLiteral(" + "));
}

QString ShowTreeController::cueLabel(const CueList &list, int index) const
{
    if (index < 0 || index >= list.cues.count())
        return QString();
    const Cue &c = list.cues.at(index);
    const QString number = c.number.isEmpty() ? QString::number(index + 1) : c.number;
    return c.name.isEmpty() ? number : QStringLiteral("%1 %2").arg(number, c.name);
}

/****************************************************************************
 * Undo
 ****************************************************************************/

QString ShowTreeController::undoText() const
{
    return m_undo.isEmpty() ? QString() : m_undo.last().label;
}

QString ShowTreeController::redoText() const
{
    return m_redo.isEmpty() ? QString() : m_redo.last().label;
}

bool ShowTreeController::undo()
{
    if (m_undo.isEmpty())
        return false;
    const Snapshot s = m_undo.takeLast();
    m_redo.append(Snapshot { s.label, snapshotXml() });
    restoreSnapshot(s.xml);
    return true;
}

bool ShowTreeController::redo()
{
    if (m_redo.isEmpty())
        return false;
    const Snapshot s = m_redo.takeLast();
    m_undo.append(Snapshot { s.label, snapshotXml() });
    restoreSnapshot(s.xml);
    return true;
}

void ShowTreeController::clearUndo()
{
    m_undo.clear();
    m_redo.clear();
}

QByteArray ShowTreeController::snapshotXml() const
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.writeStartDocument();
    saveState(&w, true);
    w.writeEndDocument();
    return xml;
}

void ShowTreeController::restoreSnapshot(const QByteArray &xml)
{
    QXmlStreamReader r(xml);
    while (!r.atEnd() && !r.isStartElement())
        r.readNext();
    if (r.name() == KXMLQLCShowTree)
        loadState(r, true);
    bake();
    m_doc->setModified();
    emit changed();
}

/****************************************************************************
 * Baking
 ****************************************************************************/

QList<quint32> ShowTreeController::scenePalettes(const ResolvedLayer &layer)
{
    QList<quint32> out;
    for (const Look &l : layer.playedLooks())
    {
        out << l.palettes;
        if (isEffectKey(l.key))
            out << l.own;
    }
    return out;
}

QString ShowTreeController::sceneSignature(const Target &target, const QList<quint32> &palettes)
{
    return target.signature() + QStringLiteral("|p:") + idsToString(palettes);
}

QString ShowTreeController::sceneSignature(const Scene *scene) const
{
    Target t;
    t.groups = scene->fixtureGroups();
    t.fixtures = scene->fixtures();
    return sceneSignature(t, scene->palettes());
}

bool ShowTreeController::isGenerated(quint32 functionId) const
{
    return m_generatedScenes.contains(functionId) || m_generatedCollections.contains(functionId)
           || m_generatedChasers.contains(functionId);
}

QString ShowTreeController::describeGenerated(quint32 functionId) const
{
    return m_generatedLabels.value(functionId);
}

static bool sameSteps(const QList<ChaserStep> &a, const QList<ChaserStep> &b)
{
    if (a.count() != b.count())
        return false;
    for (int i = 0; i < a.count(); ++i)
    {
        const ChaserStep &x = a.at(i), &y = b.at(i);
        if (x.fid != y.fid || x.fadeIn != y.fadeIn || x.hold != y.hold || x.fadeOut != y.fadeOut
            || x.duration != y.duration || x.note != y.note || x.cueNumber != y.cueNumber)
            return false;
    }
    return true;
}

void ShowTreeController::bake()
{
    if (m_baking)
        return;
    m_baking = true;

    QSet<quint32> keepScenes, keepCollections, keepChasers;
    m_generatedLabels.clear();
    m_lastChanged.clear();

    // Generated scenes are found again by what they play.
    QHash<QString, quint32> bySignature;
    QList<quint32> existing = m_generatedScenes.values();
    std::sort(existing.begin(), existing.end());
    for (quint32 id : existing)
    {
        const Scene *s = qobject_cast<const Scene *>(m_doc->function(id));
        if (s != nullptr && !bySignature.contains(sceneSignature(s)))
            bySignature.insert(sceneSignature(s), id);
    }

    for (Production &prod : m_productions)
    {
        for (CueList &list : prod.lists)
        {
            const QString folder = QStringLiteral("%1/%2/%3").arg(kProductionsFolder, prod.name, list.name);
            const QList<ResolvedCue> resolved = resolve(list);

            Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(list.chaser));
            if (chaser == nullptr || !m_generatedChasers.contains(list.chaser))
            {
                chaser = new Chaser(m_doc);
                chaser->setFadeInMode(Chaser::PerStep);
                chaser->setFadeOutMode(Chaser::PerStep);
                chaser->setDurationMode(Chaser::PerStep);
                chaser->setRunOrder(Function::SingleShot);
                if (!m_doc->addFunction(chaser))
                {
                    delete chaser;
                    continue;
                }
                list.chaser = chaser->id();
                m_generatedChasers.insert(chaser->id());
            }
            if (chaser->name() != list.name)
                chaser->setName(list.name);
            const QString chaserPath = QStringLiteral("%1/%2").arg(kProductionsFolder, prod.name);
            if (chaser->path(true) != chaserPath)
                chaser->setPath(chaserPath);
            keepChasers.insert(chaser->id());
            m_generatedLabels.insert(chaser->id(), tr("Cue list %1").arg(list.name));

            QList<ChaserStep> steps;
            for (int i = 0; i < list.cues.count(); ++i)
            {
                Cue &cue = list.cues[i];
                const QString label = cueLabel(list, i);
                const QString collName = cue.name.isEmpty()
                                         ? (cue.number.isEmpty() ? QString::number(i + 1) : cue.number)
                                         : QStringLiteral("%1-%2").arg(cue.number.isEmpty()
                                                                       ? QString::number(i + 1) : cue.number,
                                                                       cue.name);

                QList<quint32> members;
                for (const ResolvedLayer &rl : resolved.at(i).layers)
                {
                    if (rl.target.isOpaque())
                    {
                        const quint32 fid = rl.target.function;
                        if (m_doc->function(fid) != nullptr && fid != chaser->id()
                            && !members.contains(fid))
                            members << fid;
                        continue;
                    }
                    if (rl.isDark())
                        continue;

                    QList<quint32> pals;
                    for (quint32 pid : scenePalettes(rl))
                        if (m_doc->palette(pid) != nullptr)
                            pals << pid;
                    if (pals.isEmpty())
                        continue;

                    const QString sig = sceneSignature(rl.target, pals);
                    const QString layerName = rl.name.isEmpty() ? targetLabel(rl.target) : rl.name;
                    quint32 sid = bySignature.value(sig, Function::invalidId());
                    Scene *scene = qobject_cast<Scene *>(m_doc->function(sid));
                    if (scene == nullptr)
                    {
                        scene = new Scene(m_doc);
                        for (quint32 g : rl.target.groups)
                            scene->addFixtureGroup(g);
                        for (quint32 f : rl.target.fixtures)
                            scene->addFixture(f);
                        for (quint32 p : pals)
                            scene->addPalette(p);
                        scene->setVisible(false);
                        if (!m_doc->addFunction(scene))
                        {
                            delete scene;
                            continue;
                        }
                        sid = scene->id();
                        bySignature.insert(sig, sid);
                        m_generatedScenes.insert(sid);
                    }
                    if (!keepScenes.contains(sid))
                    {
                        // Named after the first cue that plays it.
                        const QString name = QStringLiteral("%1 · %2").arg(collName, layerName);
                        if (scene->name() != name)
                            scene->setName(name);
                        if (scene->path(true) != folder)
                            scene->setPath(folder);
                        keepScenes.insert(sid);
                        m_generatedLabels.insert(sid, tr("Layer %1 in cue %2 of %3")
                                                      .arg(layerName, label, list.name));
                    }
                    if (!members.contains(sid))
                        members << sid;
                }

                Collection *coll = qobject_cast<Collection *>(m_doc->function(cue.collection));
                if (coll == nullptr || !m_generatedCollections.contains(cue.collection))
                {
                    coll = new Collection(m_doc);
                    coll->setVisible(false);
                    if (!m_doc->addFunction(coll))
                    {
                        delete coll;
                        continue;
                    }
                    cue.collection = coll->id();
                    m_generatedCollections.insert(coll->id());
                }
                if (coll->name() != collName)
                    coll->setName(collName);
                if (coll->path(true) != folder)
                    coll->setPath(folder);
                if (coll->functions() != members)
                {
                    m_lastChanged << cue.id;
                    for (quint32 fid : coll->functions())
                        coll->removeFunction(fid);
                    for (quint32 fid : members)
                        coll->addFunction(fid);
                }
                keepCollections.insert(coll->id());
                m_generatedLabels.insert(coll->id(), tr("Cue %1 in %2").arg(label, list.name));

                ChaserStep step(coll->id(), cue.fadeIn, cue.hold, cue.fadeOut);
                // As ChaserStep::loadXML: an infinite hold is the duration
                // (adding the fade would wrap it round to a few seconds).
                step.duration = int(cue.hold) < 0 ? cue.hold : cue.fadeIn + cue.hold;
                step.note = cue.note;
                step.cueNumber = cue.number;
                steps << step;
            }

            const QList<ChaserStep> have = chaser->steps();
            if (!sameSteps(have, steps))
            {
                // In place, so a running list keeps its current step.
                for (int i = 0; i < steps.count(); ++i)
                {
                    if (i < have.count())
                        chaser->replaceStep(steps.at(i), i);
                    else
                        chaser->addStep(steps.at(i));
                }
                for (int i = chaser->stepsCount() - 1; i >= steps.count(); --i)
                    chaser->removeStep(i);
            }
        }
    }

    deleteUnused(keepChasers, m_generatedChasers);
    deleteUnused(keepCollections, m_generatedCollections);
    deleteUnused(keepScenes, m_generatedScenes);
    dropUnusedInlinePalettes();

    m_baking = false;

    // The previewed cue may play something else now: re-take it, so an
    // edit shows on the rig at once.
    syncPreview();
}

void ShowTreeController::deleteUnused(const QSet<quint32> &keep, QSet<quint32> &generated)
{
    QList<quint32> ids = generated.values();
    std::sort(ids.begin(), ids.end());
    for (quint32 id : ids)
    {
        if (keep.contains(id))
            continue;
        Function *f = m_doc->function(id);
        if (f == nullptr)
        {
            generated.remove(id);
            continue;
        }
        // Still playing (its cue is on stage): left for a later bake, so
        // an edit never pulls a running look out from under the show.
        if (f->isRunning())
            continue;
        generated.remove(id);
        m_doc->deleteFunction(id);
    }
}

void ShowTreeController::dropUnusedInlinePalettes()
{
    QSet<quint32> used;
    auto addLooks = [&used](const QList<Look> &looks) {
        for (const Look &l : looks)
        {
            for (quint32 p : l.palettes)
                used.insert(p);
            for (quint32 p : l.own)
                used.insert(p);
        }
    };
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            for (const Cue &c : l.cues)
            {
                addLooks(c.looks);
                for (const Layer &layer : c.layers)
                    addLooks(layer.looks);
            }

    QList<quint32> ids = m_inlinePalettes.values();
    std::sort(ids.begin(), ids.end());
    for (quint32 id : ids)
    {
        if (used.contains(id))
            continue;
        bool running = false;
        for (quint32 sid : m_generatedScenes)
        {
            const Scene *s = qobject_cast<const Scene *>(m_doc->function(sid));
            if (s != nullptr && s->palettes().contains(id))
                running = true;
        }
        if (running)
            continue;   // a scene still waiting to be let go of plays it
        m_inlinePalettes.remove(id);
        if (m_doc->palette(id) != nullptr)
            m_doc->deletePalette(id);
    }
}

quint32 ShowTreeController::detachLayer(quint32 cueId, quint32 layerId)
{
    const CueList *list = listOfCue(cueId);
    if (list == nullptr)
        return Function::invalidId();
    const int index = list->indexOf(cueId);
    const QList<ResolvedCue> resolved = resolve(*list);
    const Cue *c = list->cue(cueId);
    const Layer *layer = c == nullptr ? nullptr : c->layer(layerId);
    if (layer == nullptr || layer->target.isOpaque())
        return Function::invalidId();
    const ResolvedLayer *rl = resolved.at(index).layer(layer->target);
    if (rl == nullptr)
        return Function::invalidId();

    Scene *scene = new Scene(m_doc);
    for (quint32 g : rl->target.groups)
        scene->addFixtureGroup(g);
    for (quint32 f : rl->target.fixtures)
        scene->addFixture(f);
    for (quint32 p : scenePalettes(*rl))
        if (m_doc->palette(p) != nullptr)
            scene->addPalette(p);
    scene->setName(QStringLiteral("%1 · %2").arg(cueLabel(*list, index),
                                                      layer->name.isEmpty() ? targetLabel(layer->target)
                                                                            : layer->name));
    if (!m_doc->addFunction(scene))
    {
        delete scene;
        return Function::invalidId();
    }
    const quint32 sid = scene->id();
    const bool ok = edit(tr("Detach layer"), [&](QList<Production> &ps) {
        Cue *cc = cueIn(ps, cueId);
        Layer *l = cc == nullptr ? nullptr : cc->layer(layerId);
        if (l == nullptr)
            return false;
        l->target = Target();
        l->target.function = sid;
        l->looks.clear();
        return true;
    });
    if (!ok)
    {
        m_doc->deleteFunction(sid);
        return Function::invalidId();
    }
    return sid;
}

/****************************************************************************
 * Import
 ****************************************************************************/

bool ShowTreeController::playsTheSame(const QList<quint32> &a, const QList<quint32> &b) const
{
    struct Stack
    {
        QList<quint32> colours;
        QSet<quint32> others;
        QList<QList<quint32>> effects;
    };
    auto read = [this](const QList<quint32> &pals) {
        Stack st;
        for (quint32 pid : pals)
        {
            const QLCPalette *p = m_doc->palette(pid);
            if (p == nullptr)
                continue;
            if (p->type() == QLCPalette::Effect)
                st.effects << QList<quint32>({ pid });
            else if ((p->type() == QLCPalette::Color || p->type() == QLCPalette::Dimmer)
                     && QLCPalette::isEffectScoped(m_doc, pals, pid) && !st.effects.isEmpty())
                st.effects.last() << pid;
            else if (p->type() == QLCPalette::Color)
                st.colours << pid;
            else
                st.others.insert(pid);
        }
        return st;
    };
    const Stack x = read(a), y = read(b);
    return x.colours == y.colours && x.others == y.others && x.effects == y.effects;
}

bool ShowTreeController::sceneToLayer(const Scene *scene, Layer *layer, QString *why) const
{
    auto refuse = [why](const QString &reason) {
        if (why != nullptr)
            *why = reason;
        return false;
    };

    if (scene == nullptr)
        return refuse(tr("not a scene"));
    if (!scene->values().isEmpty())
        return refuse(tr("has channel values of its own"));
    if (scene->fixtureGroups().isEmpty() && scene->fixtures().isEmpty())
        return refuse(tr("lights no group or fixture"));
    if (scene->palettes().isEmpty())
        return refuse(tr("has no looks"));
    if (scene->lookScope() == Scene::ScopeGroup)
        return refuse(tr("its looks are scoped to one group"));
    if (scene->fadeInSpeed() != 0 || scene->fadeOutSpeed() != 0)
        return refuse(tr("has fade times of its own"));
    if (!scene->paletteFades().isEmpty())
        return refuse(tr("has fade times per look"));
    if (scene->transitionIn().isActive() || scene->transitionOut().isActive())
        return refuse(tr("has a transition"));
    if (scene->blendMode() != Universe::NormalBlend)
        return refuse(tr("has a blend mode"));
    if (scene->speedMaster() != SpeedMasters::Inherit)
        return refuse(tr("is on a speed master"));

    QList<Look> looks;
    int currentEffect = -1;
    const QList<quint32> pals = scene->palettes();
    for (quint32 pid : pals)
    {
        const QLCPalette *p = m_doc->palette(pid);
        if (p == nullptr)
            return refuse(tr("a look it uses is missing"));

        if (p->type() == QLCPalette::Effect)
        {
            const QString key = effectKey(effectKind(p));
            for (const Look &l : looks)
                if (l.key == key)
                    return refuse(tr("has two %1s").arg(keyLabel(key).toLower()));
            Look l;
            l.key = key;
            l.palettes << pid;
            looks << l;
            currentEffect = int(looks.count()) - 1;
            continue;
        }
        if ((p->type() == QLCPalette::Color || p->type() == QLCPalette::Dimmer)
            && QLCPalette::isEffectScoped(m_doc, pals, pid))
        {
            if (currentEffect < 0)
                return refuse(tr("its looks are not in the usual order"));
            looks[currentEffect].own << pid;
            continue;
        }

        const QString key = keyForPalette(pid);
        int at = -1;
        for (int i = 0; i < looks.count(); ++i)
            if (looks.at(i).key == key)
                at = i;
        if (at < 0)
        {
            Look l;
            l.key = key;
            l.palettes << pid;
            looks << l;
        }
        else if (key == KeyColour)
        {
            looks[at].palettes << pid;      // colour 2, 3...
        }
        else
        {
            return refuse(tr("has two %1 looks (the lower always wins)").arg(keyLabel(key).toLower()));
        }
    }

    Layer l;
    l.target.groups = scene->fixtureGroups();
    l.target.fixtures = scene->fixtures();
    l.looks = looks;

    // It must bake back to a stack that plays the same: the static looks
    // may move (each is the only one of its kind, and an Aim or a Gobo is
    // the look's wherever it sits), but the colours keep their numbering
    // and the effects their order, each with its own looks.
    ResolvedLayer rl;
    rl.target = l.target;
    for (const Look &look : looks)
        rl.values.insert(look.key, ResolvedValue { look, Origin::Own, invalidId() });
    if (!playsTheSame(scenePalettes(rl), pals))
        return refuse(tr("its effects are in an order the tree doesn't keep"));

    if (layer != nullptr)
        *layer = l;
    return true;
}

ShowTreeController::ImportReport ShowTreeController::importChaser(quint32 chaserId)
{
    ImportReport report;
    const Chaser *chaser = qobject_cast<const Chaser *>(m_doc->function(chaserId));
    if (chaser == nullptr || chaser->type() != Function::ChaserType)
    {
        report.notes << tr("Not a chaser.");
        return report;
    }

    const QList<ChaserStep> steps = chaser->steps();
    QHash<quint32, quint32> cueOfFunction;      // a step's function -> its cue
    Production prod;
    prod.id = newId();
    prod.name = chaser->name();
    CueList list;
    list.id = newId();
    list.name = chaser->name();
    list.mode = ListMode::NonTracking;

    for (int i = 0; i < steps.count(); ++i)
    {
        const quint32 fid = steps.at(i).fid;
        if (!cueOfFunction.contains(fid))
            cueOfFunction.insert(fid, newId());
    }

    QSet<quint32> usedCueIds;
    for (int i = 0; i < steps.count(); ++i)
    {
        const ChaserStep &step = steps.at(i);
        const Function *f = m_doc->function(step.fid);
        Cue cue;
        // The same function twice in a chaser is two cues.
        cue.id = usedCueIds.contains(cueOfFunction.value(step.fid)) ? newId()
                                                                     : cueOfFunction.value(step.fid);
        usedCueIds.insert(cue.id);
        cue.mode = CueMode::ListDefault;
        cue.fadeIn = step.fadeIn;
        cue.fadeOut = step.fadeOut;
        cue.hold = step.hold;
        cue.note = step.note;
        const QString fname = f == nullptr ? QString() : f->name();
        QString number, name;
        if (!step.cueNumber.isEmpty())
        {
            cue.number = step.cueNumber;
            if (!splitNumberedName(fname, nullptr, &name))
                name = fname;
            cue.name = name;
        }
        else if (splitNumberedName(fname, &number, &name))
        {
            cue.number = number;
            cue.name = name;
        }
        else
        {
            cue.name = fname;
        }
        if (step.skip)
            report.notes << tr("Cue %1 was skipped in the chaser; it plays in the cue list.")
                            .arg(cueLabel(list, i).isEmpty() ? QString::number(i + 1) : fname);
        if (step.speedMaster != SpeedMasters::Inherit)
            report.notes << tr("%1: its own speed master is not carried over.").arg(fname);
        if (step.subCues != ChaserStep::Auto)
            report.notes << tr("%1: sub-cues (Sub GO) are not carried over yet.").arg(fname);

        auto addOpaque = [&](const Function *fn, const QString &why) {
            Layer l;
            l.id = newId();
            l.target.function = fn->id();
            cue.layers << l;
            report.opaqueLayers++;
            report.opaqueReasons << QStringLiteral("%1: %2").arg(fn->name(), why);
        };
        auto addFunction = [&](const Function *fn) {
            if (fn == nullptr)
                return;
            if (fn->type() == Function::SceneType)
            {
                Layer l;
                QString why;
                if (sceneToLayer(qobject_cast<const Scene *>(fn), &l, &why))
                {
                    l.id = newId();
                    cue.layers << l;
                    report.layers++;
                }
                else
                {
                    addOpaque(fn, why);
                }
                return;
            }
            if (fn->type() == Function::CollectionType && cueOfFunction.contains(fn->id())
                && fn->id() != step.fid)
            {
                cue.includes << cueOfFunction.value(fn->id());
                report.includes++;
                return;
            }
            addOpaque(fn, tr("played as it is (%1)").arg(Function::typeToString(fn->type())));
        };

        if (f == nullptr)
        {
            report.notes << tr("Step %1 plays a function that no longer exists.").arg(i + 1);
        }
        else if (f->type() == Function::CollectionType)
        {
            const Collection *coll = qobject_cast<const Collection *>(f);
            for (quint32 member : coll->functions())
                addFunction(m_doc->function(member));
        }
        else
        {
            addFunction(f);
        }
        list.cues << cue;
    }
    report.cues = int(list.cues.count());

    // Two scenes on the same target in one cue: the chaser ran both, the
    // tree keeps one value per attribute (the later one).
    for (int i = 0; i < list.cues.count(); ++i)
    {
        const Cue &cue = list.cues.at(i);
        QHash<QString, QStringList> sources;   // target -> where it is set
        QHash<QString, Target> targets;
        for (quint32 inc : cue.includes)
        {
            CueList single = list;
            const QList<ResolvedCue> r = resolve(single);
            const int j = list.indexOf(inc);
            if (j < 0)
                continue;
            for (const ResolvedLayer &rl : r.at(j).layers)
            {
                if (rl.target.isOpaque())
                    continue;
                sources[rl.target.signature()] << tr("cue %1").arg(cueLabel(list, j));
                targets.insert(rl.target.signature(), rl.target);
            }
        }
        for (const Layer &l : cue.layers)
        {
            if (l.target.isOpaque())
                continue;
            sources[l.target.signature()] << tr("its own scene");
            targets.insert(l.target.signature(), l.target);
        }
        QStringList sigs = sources.keys();
        std::sort(sigs.begin(), sigs.end());
        for (const QString &sig : sigs)
            if (sources.value(sig).count() > 1)
                report.htpConflicts << tr("Cue %1: %2 is set by %3; the chaser ran them all "
                                          "(the brighter intensity won), the cue list keeps "
                                          "the last one's value for each attribute.")
                                       .arg(cueLabel(list, i), targetLabel(targets.value(sig)),
                                            sources.value(sig).join(tr(" and ")));
    }

    // The generated chaser starts as a copy, so it plays the way the
    // original does: run order, speed modes, tempo, GO on the beat.
    Function *copy = const_cast<Chaser *>(chaser)->createCopy(m_doc, true);
    Chaser *generated = qobject_cast<Chaser *>(copy);
    if (generated == nullptr)
    {
        report.notes << tr("Could not copy the chaser.");
        return report;
    }
    while (generated->stepsCount() > 0)
        generated->removeStep(generated->stepsCount() - 1);
    m_generatedChasers.insert(generated->id());
    list.chaser = generated->id();
    prod.lists << list;

    const QSet<quint32> scenesBefore = m_generatedScenes;
    edit(tr("Import %1").arg(chaser->name()), [&](QList<Production> &ps) {
        ps << prod;
        return true;
    });

    report.production = prod.id;
    report.list = list.id;
    QSet<quint32> scenes;
    if (const CueList *l = cueList(list.id))
        for (const Cue &c : l->cues)
            if (const Collection *coll = qobject_cast<const Collection *>(m_doc->function(c.collection)))
                for (quint32 m : coll->functions())
                    if (m_generatedScenes.contains(m))
                        scenes.insert(m);
    report.generatedScenes = int(scenes.count());
    Q_UNUSED(scenesBefore)
    return report;
}

/****************************************************************************
 * Load & save
 ****************************************************************************/

void ShowTreeController::saveState(QXmlStreamWriter *doc, bool snapshot) const
{
    doc->writeStartElement(KXMLQLCShowTree);
    doc->writeAttribute(KXMLShowTreeNextId, QString::number(m_nextId));

    for (const Production &p : m_productions)
        ShowTree::saveXML(doc, p);

    QList<quint32> inl = m_inlinePalettes.values();
    std::sort(inl.begin(), inl.end());

    if (!snapshot)
    {
        QList<quint32> gen = m_generatedScenes.values() + m_generatedCollections.values()
                             + m_generatedChasers.values();
        std::sort(gen.begin(), gen.end());
        doc->writeStartElement(KXMLShowTreeGenerated);
        doc->writeAttribute(QStringLiteral("Scenes"), idsToString(QList<quint32>(
            [this] { QList<quint32> l = m_generatedScenes.values(); std::sort(l.begin(), l.end()); return l; }())));
        doc->writeAttribute(QStringLiteral("Collections"), idsToString(QList<quint32>(
            [this] { QList<quint32> l = m_generatedCollections.values(); std::sort(l.begin(), l.end()); return l; }())));
        doc->writeAttribute(QStringLiteral("Chasers"), idsToString(QList<quint32>(
            [this] { QList<quint32> l = m_generatedChasers.values(); std::sort(l.begin(), l.end()); return l; }())));
        doc->writeEndElement();

        doc->writeStartElement(KXMLShowTreeInline);
        doc->writeAttribute(QStringLiteral("IDs"), idsToString(inl));
        doc->writeEndElement();
    }
    else
    {
        // An undo step must bring back a one-off value it dropped, as it was.
        doc->writeStartElement(KXMLShowTreeInline);
        for (quint32 id : inl)
            if (QLCPalette *p = m_doc->palette(id))
                p->saveXML(doc);
        doc->writeEndElement();

        // ...and a cue list's chaser with its playback settings (run order,
        // speed modes, GO on the beat): a bake would make a plain new one.
        doc->writeStartElement(KXMLShowTreeChasers);
        for (const Production &p : m_productions)
            for (const CueList &l : p.lists)
                if (Function *f = m_doc->function(l.chaser))
                    f->saveXML(doc);
        doc->writeEndElement();
    }

    doc->writeEndElement();
}

void ShowTreeController::loadState(QXmlStreamReader &root, bool snapshot)
{
    m_productions.clear();
    const quint32 next = root.attributes().value(KXMLShowTreeNextId).toString().toUInt();
    if (next > m_nextId || !snapshot)
        m_nextId = qMax<quint32>(next, 1);

    while (root.readNextStartElement())
    {
        if (root.name() == QLatin1String("Production"))
        {
            Production p;
            if (ShowTree::loadXML(root, p))
                m_productions << p;
        }
        else if (root.name() == KXMLShowTreeGenerated && !snapshot)
        {
            const QXmlStreamAttributes a = root.attributes();
            for (quint32 id : stringToIds(a.value(QStringLiteral("Scenes")).toString()))
                m_generatedScenes.insert(id);
            for (quint32 id : stringToIds(a.value(QStringLiteral("Collections")).toString()))
                m_generatedCollections.insert(id);
            for (quint32 id : stringToIds(a.value(QStringLiteral("Chasers")).toString()))
                m_generatedChasers.insert(id);
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLShowTreeInline)
        {
            if (!snapshot)
            {
                for (quint32 id : stringToIds(root.attributes().value(QStringLiteral("IDs")).toString()))
                    m_inlinePalettes.insert(id);
                root.skipCurrentElement();
                continue;
            }
            while (root.readNextStartElement())
            {
                if (root.name() != QLatin1String("Palette"))
                {
                    root.skipCurrentElement();
                    continue;
                }
                QLCPalette *saved = new QLCPalette(QLCPalette::Undefined, m_doc);
                if (!saved->loadXML(root))
                {
                    delete saved;
                    continue;
                }
                m_inlinePalettes.insert(saved->id());
                QLCPalette *have = m_doc->palette(saved->id());
                if (have == nullptr)
                {
                    if (!m_doc->addPalette(saved, saved->id()))
                        delete saved;
                }
                else
                {
                    if (have->type() == saved->type())
                    {
                        have->setName(saved->name());
                        have->setValues(saved->values());
                    }
                    delete saved;
                }
            }
        }
        else if (root.name() == KXMLShowTreeChasers && snapshot)
        {
            while (root.readNextStartElement())
            {
                const quint32 id = root.attributes().value(QStringLiteral("ID")).toString().toUInt();
                if (root.name() != QLatin1String("Function") || m_doc->function(id) != nullptr)
                {
                    root.skipCurrentElement();
                    continue;
                }
                if (Function::loader(root, m_doc) && m_doc->function(id) != nullptr)
                    m_generatedChasers.insert(id);
            }
        }
        else
        {
            root.skipCurrentElement();
        }
    }

    // Ids above anything in the tree, whatever the file said.
    for (const Production &p : m_productions)
    {
        m_nextId = qMax(m_nextId, p.id + 1);
        for (const CueList &l : p.lists)
        {
            m_nextId = qMax(m_nextId, l.id + 1);
            for (const Cue &c : l.cues)
            {
                m_nextId = qMax(m_nextId, c.id + 1);
                for (const Layer &layer : c.layers)
                    m_nextId = qMax(m_nextId, layer.id + 1);
            }
        }
    }
}

bool ShowTreeController::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCShowTree)
    {
        root.skipCurrentElement();
        return false;
    }
    reset();
    loadState(root, false);
    emit changed();
    return true;
}

void ShowTreeController::saveXML(QXmlStreamWriter *doc) const
{
    if (m_productions.isEmpty() && m_generatedScenes.isEmpty() && m_generatedCollections.isEmpty()
        && m_generatedChasers.isEmpty() && m_inlinePalettes.isEmpty())
        return;     // a show without a tree saves as it always did
    saveState(doc, false);
}

void ShowTreeController::reset()
{
    // The show is going: let go of what the preview plays (the functions
    // are still there; Doc deletes them after this).
    stopPreview();
    m_sweepTimer->stop();     // nothing of the old show is left to sweep
    m_productions.clear();
    m_generatedScenes.clear();
    m_generatedCollections.clear();
    m_generatedChasers.clear();
    m_inlinePalettes.clear();
    m_generatedLabels.clear();
    m_undo.clear();
    m_redo.clear();
    m_nextId = 1;
    emit changed();
}

void ShowTreeController::slotFunctionRemoved(quint32 fid)
{
    m_previewRunning.removeAll(fid);
    m_generatedScenes.remove(fid);
    m_generatedCollections.remove(fid);
    m_generatedChasers.remove(fid);
    m_generatedLabels.remove(fid);
}

void ShowTreeController::slotPaletteRemoved(quint32 pid)
{
    m_inlinePalettes.remove(pid);
}

/****************************************************************************
 * Preview
 ****************************************************************************/

FunctionParent ShowTreeController::previewParent()
{
    // Its own parent: a chaser's collection, a VC button or the Programming
    // tab holding the same scene keep it when the preview lets go, and the
    // preview never stops what they started (a Master parent would).
    return FunctionParent(FunctionParent::AutoVCWidget, 0xFFFFFF53);
}

void ShowTreeController::previewCue(quint32 cueId)
{
    if (cue(cueId) == nullptr)
    {
        stopPreview();
        return;
    }
    // Again for the same cue is fine: it brings back what something else
    // stopped (Stop all), and changes nothing otherwise.
    m_previewCue = cueId;
    syncPreview();
}

void ShowTreeController::stopPreview()
{
    m_previewCue = invalidId();
    syncPreview();
}

QStringList ShowTreeController::listsRunning() const
{
    QStringList names;
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            if (const Function *f = m_doc->function(l.chaser))
                if (f->isRunning())
                    names << l.name;
    return names;
}

static void setEditOverride(Function *f, bool on)
{
    if (Scene *s = qobject_cast<Scene *>(f))
        s->setEditOverride(on);
}

void ShowTreeController::syncPreview()
{
    const PreviewState before = m_previewState;
    const QList<quint32> runningBefore = m_previewRunning;

    QList<quint32> want;
    PreviewState state = PreviewState::Off;
    const CueList *list = m_previewCue == invalidId() ? nullptr : listOfCue(m_previewCue);
    const Cue *c = list == nullptr ? nullptr : list->cue(m_previewCue);
    if (c == nullptr)
    {
        m_previewCue = invalidId();
    }
    else if (m_doc->mode() != Doc::Design)
    {
        // Live: the running list owns the stage; edits land on GO.
        state = PreviewState::NotDesign;
    }
    else
    {
        // Design: what is in edit is what shows -- also over a list running
        // for real (its own or another), which keeps going underneath.
        if (const Collection *coll = qobject_cast<const Collection *>(m_doc->function(c->collection)))
            for (quint32 fid : coll->functions())
                if (m_doc->function(fid) != nullptr && !want.contains(fid))
                    want << fid;
        state = want.isEmpty() ? PreviewState::Dark : PreviewState::Playing;
    }

    // Start first, stop after: a scene both cues play is never let go of,
    // so it runs on (no re-fade, effects keep their phase). The preview's
    // scenes win on stage (Scene::setEditOverride: above the Auto faders,
    // LTP on every channel), so a running cue at full can't hide a dimmer
    // edited down; an opaque function (EFX, a matrix) mixes as it would.
    MasterTimer *timer = m_doc->masterTimer();
    for (quint32 fid : want)
    {
        // start() is a no-op for a parent the function already has, and
        // brings back one something else stopped (Stop all, a mode change).
        if (Function *f = m_doc->function(fid))
        {
            setEditOverride(f, true);
            f->start(timer, previewParent());
        }
    }
    bool stoppedAny = false;
    for (quint32 fid : runningBefore)
    {
        if (want.contains(fid))
            continue;
        if (Function *f = m_doc->function(fid))
        {
            // Back to plain HTP/LTP first: if the real list holds it too, it
            // plays on as the list's; if not, it fades into what's under it.
            setEditOverride(f, false);
            f->stop(previewParent());
            stoppedAny = true;
        }
    }
    m_previewRunning = want;
    m_previewState = state;

    // A generated scene the preview let go of, that nothing plays any more,
    // goes at the next bake; it is still running for a tick, so sweep then.
    if (stoppedAny)
        m_sweepTimer->start();

    if (state != before || want != runningBefore)
        emit previewChanged();
}

void ShowTreeController::slotModeChanged()
{
    // Into Design the app has stopped every function (MasterTimer
    // stopAllFunctions): what the preview ran is gone, start it afresh.
    // Into Operate the preview stops -- Live edits are blind until GO.
    if (m_doc->mode() == Doc::Design)
    {
        for (quint32 fid : m_previewRunning)
            setEditOverride(m_doc->function(fid), false);
        m_previewRunning.clear();
    }
    syncPreview();
}

void ShowTreeController::slotTimerFunctionStarted(quint32 fid)
{
    // A list started or stopped for real under a preview: the preview stays
    // (Design: the edit wins), only what the header says changes.
    if (m_previewCue == invalidId() || m_previewState != PreviewState::Playing)
        return;
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            if (l.chaser == fid)
            {
                emit previewChanged();
                return;
            }
}

void ShowTreeController::slotTimerFunctionStopped(quint32 fid)
{
    slotTimerFunctionStarted(fid);
}
