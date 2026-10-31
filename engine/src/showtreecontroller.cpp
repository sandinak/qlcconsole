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
#include <QDataStream>
#include <QBuffer>
#include <QColor>
#include <QRegularExpression>
#include <climits>
#include <QTimer>
#include <QSet>
#include <QDebug>
#include <algorithm>

#include "showtreecontroller.h"
#include "showcontrol.h"
#include "paletteops.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "linespread.h"
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
#include "programmercontroller.h"

using namespace ShowTree;

#define KXMLShowTreeNextId          QStringLiteral("NextId")
#define KXMLShowTreeGenerated       QStringLiteral("Generated")
#define KXMLShowTreeInline          QStringLiteral("InlinePalettes")
#define KXMLShowTreeChasers         QStringLiteral("Chasers")
#define KXMLShowTreeReusable        QStringLiteral("Reusable")
#define KXMLShowTreeDetached        QStringLiteral("Detached")

/** Where the tree files what it makes (§4): out of the way of the user's
    own folders. */
static const QString kProductionsFolder = QStringLiteral("Productions");
static const QString kInlinePaletteFolder = QStringLiteral("Show Tree");

static QList<quint32> fixturesOfTarget(const Doc *doc, const Target &target)
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

/* The heads of each fixture @p target lights, as units "fixture|head": a
   group lights the heads it holds -- a mover's Ring group only its ring
   head -- and a fixture named alone all of its own. Two layers overlap
   where they share a unit, so a Movers layer and a Ring layer on the same
   fixtures are not one over the other. */
static QHash<quint32, QList<qint64>> unitsOfTarget(const Doc *doc, const Target &target)
{
    auto unit = [](quint32 f, int h) { return (qint64(f) << 16) | qint64(h & 0xFFFF); };
    auto allHeads = [doc, &unit](quint32 f) {
        QList<qint64> out;
        const Fixture *fx = const_cast<Doc *>(doc)->fixture(f);
        const int n = fx != nullptr ? qMax(1, fx->heads()) : 1;
        for (int h = 0; h < n; ++h)
            out << unit(f, h);
        return out;
    };
    QHash<quint32, QList<qint64>> out;
    for (quint32 g : target.groups)
    {
        const FixtureGroup *grp = doc->fixtureGroup(g);
        if (grp == nullptr)
            continue;
        for (const GroupHead &gh : grp->headList())
        {
            if (!gh.isValid())
                continue;
            QList<qint64> &u = out[gh.fxi];
            const QList<qint64> add = gh.head < 0 ? allHeads(gh.fxi) : QList<qint64>{ unit(gh.fxi, gh.head) };
            for (qint64 x : add)
                if (!u.contains(x))
                    u << x;
        }
        for (quint32 f : grp->fixtureList())
            if (!out.contains(f))
                out.insert(f, allHeads(f));
    }
    for (quint32 f : target.fixtures)
        out.insert(f, allHeads(f));
    return out;
}

ShowTreeController::ShowTreeController(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
    connect(m_doc, &Doc::functionRemoved, this, &ShowTreeController::slotFunctionRemoved);
    connect(m_doc, &Doc::paletteRemoved, this, &ShowTreeController::slotPaletteRemoved);
    connect(m_doc, &Doc::modeChanged, this, &ShowTreeController::slotModeChanged);
    // originalFor() asks again once the show's functions change.
    auto stale = [this]() { m_originalsStale = true; };
    connect(m_doc, &Doc::functionAdded, this, stale);
    connect(m_doc, &Doc::functionRemoved, this, stale);
    connect(m_doc, &Doc::functionChanged, this, stale);
    connect(this, &ShowTreeController::changed, this, stale);
    // What bake() planned for each cue stands on the rig: a change to it
    // plans every cue again.
    auto rigChanged = [this]() { ++m_planWorld; };
    connect(m_doc, &Doc::fixtureAdded, this, rigChanged);
    connect(m_doc, &Doc::fixtureRemoved, this, rigChanged);
    connect(m_doc, &Doc::fixtureChanged, this, rigChanged);
    connect(m_doc, &Doc::fixtureGroupAdded, this, rigChanged);
    connect(m_doc, &Doc::fixtureGroupRemoved, this, rigChanged);
    connect(m_doc, &Doc::fixtureGroupChanged, this, rigChanged);
    connect(m_doc, &Doc::paletteAdded, this, rigChanged);
    connect(m_doc, &Doc::paletteRemoved, this, rigChanged);
    m_sweepTimer = new QTimer(this);
    m_sweepTimer->setSingleShot(true);
    m_sweepTimer->setInterval(250);
    connect(m_sweepTimer, &QTimer::timeout, this, &ShowTreeController::bake);
    // From the DMX thread: queued, so the preview is only ever touched here.
    connect(m_doc->masterTimer(), &MasterTimer::functionStarted,
            this, &ShowTreeController::slotTimerFunctionStarted, Qt::QueuedConnection);
    connect(m_doc->masterTimer(), &MasterTimer::functionStopped,
            this, &ShowTreeController::slotTimerFunctionStopped, Qt::QueuedConnection);
    // GO from anywhere (OSC, MSC, the VC, a key) holds the preview (RC8).
    // Auto: an MSC command arrives on its plugin's thread and is queued.
    if (m_doc->showControl() != nullptr)
        connect(m_doc->showControl(), &ShowControl::aboutToTakeStage, this,
                [this]() { takeStageForRun(); });
    // The main cue list set elsewhere (a chaser editor) moves the current
    // production with it (P5): one source of truth for what GO plays.
    if (m_doc->showControl() != nullptr)
        connect(m_doc->showControl(), &ShowControl::mainCueListChanged,
                this, &ShowTreeController::slotMainCueListChanged);
    // A production made current arms its hits (Q154).
    connect(this, &ShowTreeController::currentProductionChanged, this, &ShowTreeController::hitArmingChanged);
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

bool ShowTreeController::isLineAim(const Look &look) const
{
    if (look.key != KeyPosition || look.none || look.palettes.isEmpty())
        return false;
    const QLCPalette *p = m_doc->palette(look.palettes.first());
    if (p == nullptr || p->type() != QLCPalette::Aim)
        return false;
    const MonitorProperties *mp = m_doc->monitorProperties();
    const StageTarget *t = mp == nullptr ? nullptr : mp->stageTarget(p->stageTargetId());
    return t != nullptr && t->isLine();
}

LineAimTest ShowTreeController::lineAimTest() const
{
    return [this](const Look &look) { return isLineAim(look); };
}

QList<ResolvedCue> ShowTreeController::resolved(const CueList &list) const
{
    // Sub-cues (P6): the first starts from what its cue puts on stage.
    if (list.isSubCues())
    {
        const CueList *parent = nullptr;
        for (const Production &p : m_productions)
            for (const CueList &l : p.lists)
                if (!l.isSubCues() && l.indexOf(list.subOf) >= 0)
                    parent = &l;
        if (parent != nullptr)
        {
            const QList<ResolvedCue> up = resolve(*parent, lineAimTest());
            const QList<ResolvedLayer> start = up.at(parent->indexOf(list.subOf)).layers;
            return resolve(list, lineAimTest(), &start);
        }
    }
    return resolve(list, lineAimTest());
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

/* A Looks section's rules (R3), whatever an edit did: a look plays only what
   it has -- no mode, includes or base cue -- and only looks are filed in
   folders (a look pasted into a cue list leaves its folder behind). */
static void normalizeLooks(QList<Production> &ps)
{
    /* Sub-cue lists (P6), whatever an edit did: always tracking, no mode,
       includes or base; in the production of their cue, right after its
       list; gone with their cue, or empty -- and never of a look or of a
       sub-cue (one level, SUBCUES §8). */
    {
        QHash<quint32, QPair<int, quint32>> owner;      // cue -> (production index, list id)
        QHash<quint32, bool> ownerOk;
        for (int pi = 0; pi < ps.count(); ++pi)
            for (const CueList &l : ps.at(pi).lists)
                for (const Cue &c : l.cues)
                {
                    owner.insert(c.id, qMakePair(pi, l.id));
                    ownerOk.insert(c.id, !l.looks && !l.isSubCues());
                }
        QList<CueList> subs;
        QSet<quint32> seenParents;
        for (Production &p : ps)
            for (int i = int(p.lists.count()) - 1; i >= 0; --i)
                if (p.lists.at(i).isSubCues())
                    subs.prepend(p.lists.takeAt(i));
        for (CueList &s : subs)
        {
            if (s.cues.isEmpty() || !ownerOk.value(s.subOf, false) || seenParents.contains(s.subOf))
                continue;
            seenParents.insert(s.subOf);
            s.mode = ListMode::Tracking;
            s.baseCue = invalidId();
            s.looks = false;
            s.folders.clear();
            for (Cue &c : s.cues)
            {
                c.mode = CueMode::ListDefault;
                c.includes.clear();
                c.folder.clear();
            }
            Production &p = ps[owner.value(s.subOf).first];
            int at = int(p.lists.count());
            for (int i = 0; i < p.lists.count(); ++i)
                if (p.lists.at(i).id == owner.value(s.subOf).second)
                    at = i + 1;
            while (at < p.lists.count() && p.lists.at(at).isSubCues())
                ++at;
            p.lists.insert(at, s);
        }
    }
    for (Production &p : ps)
        for (CueList &l : p.lists)
        {
            if (l.isSubCues())
                continue;
            for (Cue &c : l.cues)
            {
                c.follow = false;
                c.wait = 0;
            }
            if (!l.looks)
            {
                for (Cue &c : l.cues)
                {
                    c.folder.clear();
                    c.scope = HitScope::Production;     // a hit pasted into a list is a cue
                    c.scopeOf = invalidId();
                }
                continue;
            }
            l.mode = ListMode::NonTracking;
            l.baseCue = invalidId();
            for (Cue &c : l.cues)
            {
                c.mode = CueMode::ListDefault;
                c.includes.clear();
                c.skip = false;                 // a look is fired, not GOne through
                c.noMoveInBlack = false;        // ... nor looked ahead to
                c.focusCall = false;            // ... nor a pre-show step
            }
            l.moveInBlack = MoveInBlack::Show;
        }
    // Scoped hits (Q154): a hit whose cue or list is gone (deleted, or in
    // another production now) moves up to its production's Hits section;
    // only there does it keep a folder.
    for (Production &p : ps)
    {
        QSet<quint32> lists, cues;
        for (const CueList &l : p.lists)
        {
            if (l.looks)
                continue;
            if (!l.isSubCues())
                lists.insert(l.id);
            for (const Cue &c : l.cues)
                cues.insert(c.id);
        }
        for (CueList &l : p.lists)
            if (l.looks)
                for (Cue &c : l.cues)
                {
                    if ((c.scope == HitScope::List && !lists.contains(c.scopeOf))
                        || (c.scope == HitScope::Cue && !cues.contains(c.scopeOf)))
                        c.scope = HitScope::Production;
                    if (c.scope == HitScope::Production || c.scope == HitScope::Global)
                        c.scopeOf = invalidId();
                    if (c.scope != HitScope::Production)
                        c.folder.clear();
                }
    }
}

bool ShowTreeController::edit(const QString &label,
                              const std::function<bool(QList<Production> &)> &change)
{
    if (m_batchDepth > 0)
    {
        // Part of a command: applied now, baked and recorded once at the end.
        QList<Production> work = m_productions;
        if (!change(work))
            return false;
        normalizeLooks(work);
        syncReusables(m_productions, work);
        m_productions = work;
        m_batchChanged = true;
        return true;
    }

    const QByteArray before = snapshotXml();
    QList<Production> work = m_productions;
    if (!change(work))
        return false;
    normalizeLooks(work);

    // A reusable's use edited: every use (P4).
    syncReusables(m_productions, work);
    m_productions = work;
    pushUndo(label, before);

    bake();
    m_doc->setModified();
    emit changed();
    return true;
}

void ShowTreeController::pushUndo(const QString &label, const QByteArray &before)
{
    Snapshot step;
    step.label = label;
    step.xml = before;
    step.stamp = PaletteOps::nextUndoStamp();
    m_undo.append(step);
    while (m_undo.count() > kUndoLimit)
        m_undo.removeFirst();
    m_redo.clear();
    // One history with the palette steps: a new step makes their redo stale.
    if (PaletteOps *ops = m_doc->paletteOps())
        ops->clearRedo();
}

void ShowTreeController::beginBatch(const QString &label)
{
    if (m_batchDepth++ > 0)
        return;
    m_batchNamed.clear();
    m_batchLabel = label;
    m_batchBefore = snapshotXml();
    m_batchProductions = m_productions;
    m_batchSets = m_groupSets;
    m_batchReusableLayers = m_reusableLayers;
    m_batchReusableCues = m_reusableCues;
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
        {
            if (m_inlinePalettes.remove(pid) && !m_batchChanged)
                m_doc->setModified();
            m_madeReusable.insert(pid);
        }
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
        m_groupSets = m_batchSets;
        m_reusableLayers = m_batchReusableLayers;
        m_reusableCues = m_batchReusableCues;
        bake();
        emit changed();
        return;
    }
    pushUndo(m_batchLabel, m_batchBefore);
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
    if (production(id) != nullptr && m_current == invalidId())
    {
        // Nothing else has GO: this one does. A main cue list that is still
        // a chaser (a plain one, or a list kept after "production none")
        // is the operator's choice and stays.
        const ShowControl *sc = m_doc->showControl();
        const quint32 main = sc != nullptr ? sc->mainCueList() : Function::invalidId();
        if (main == Function::invalidId() || m_doc->function(main) == nullptr)
            setCurrentProduction(id);
    }
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
        // Q190: numbered to the show's scheme, in the section it is added in.
        Numbering scheme = Numbering::None;
        for (const Production &p : ps)
            if (p.list(listId) != nullptr)
                scheme = p.numbering;
        c.number = numberForInsertIn(*l, scheme, afterIndex, takenNumbers(ps, *l));
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

/** Layer @a id of @a cue and every layer under it (its child layers). */
static QList<quint32> withChildren(const Cue &cue, quint32 id)
{
    QList<quint32> out { id };
    for (int k = 0; k < out.count(); ++k)
        for (const Layer &l : cue.layers)
            if (l.parent == out.at(k) && !out.contains(l.id))
                out << l.id;
    return out;
}

bool ShowTreeController::removeLayer(quint32 cueId, quint32 layerId)
{
    return edit(tr("Remove layer"), [&](QList<Production> &ps) {
        Cue *c = cueIn(ps, cueId);
        if (c == nullptr || c->layer(layerId) == nullptr)
            return false;
        // Its child layers go with it: they are part of it.
        const QList<quint32> gone = withChildren(*c, layerId);
        for (int i = c->layers.count() - 1; i >= 0; --i)
            if (gone.contains(c->layers.at(i).id))
                c->layers.removeAt(i);
        return true;
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
    for (quint32 set : target.sets)
        if (!to.sets.contains(set))
            to.sets << set;
    for (quint32 f : target.fixtures)
        if (!to.fixtures.contains(f))
            to.fixtures << f;
    if (to.isEmpty())
        return false;
    // A child layer stays part of its parent, and a parent keeps its
    // children inside it (§3.5).
    if (const Cue *have = cue(cueId))
        if (const Layer *l = have->layer(layerId))
        {
            if (const Layer *up = l->parent == invalidId() ? nullptr : have->layer(l->parent))
                if (!targetContains(up->target, to))
                    return false;
            for (const Layer &child : have->layers)
                if (child.parent == layerId && !targetContains(to, child.target))
                    return false;
        }

    return edit(label.isEmpty() ? tr("Change layer") : label, [&](QList<Production> &ps) {
        CueList *list = nullptr;
        Cue *c = cueIn(ps, cueId, &list);
        Layer *l = c == nullptr ? nullptr : c->layer(layerId);
        if (l == nullptr || l->target.isOpaque() || l->target == to)
            return false;

        const Target from = l->target;
        const int at = list->indexOf(cueId);
        const QList<ResolvedCue> before = resolved(*list);

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
                                    const QList<quint32> &fixtures, const QList<quint32> &sets)
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
    for (quint32 set : sets)
        if (!t.sets.contains(set))
        {
            t.sets << set;
            Target one;
            one.sets << set;
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
        if (l == nullptr || l->looks || l->isSubCues() || l->mode == mode)
            return false;
        l->mode = mode;
        return true;
    });
}

bool ShowTreeController::setBaseCue(quint32 listId, quint32 cueId)
{
    return edit(tr("Base cue"), [&](QList<Production> &ps) {
        CueList *l = listIn(ps, listId);
        if (l == nullptr || l->looks || l->isSubCues() || l->baseCue == cueId)
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
        CueList *l = nullptr;
        Cue *c = cueIn(ps, cueId, &l);
        if (c == nullptr || l->looks || l->isSubCues() || c->mode == mode)
            return false;
        c->mode = mode;
        return true;
    });
}

bool ShowTreeController::setCuesSkipped(const QList<quint32> &cueIds, bool skip)
{
    const QString label = skip ? (cueIds.count() == 1 ? tr("Skip cue") : tr("Skip %1 cues").arg(cueIds.count()))
                               : (cueIds.count() == 1 ? tr("Unskip cue") : tr("Unskip %1 cues").arg(cueIds.count()));
    return edit(label, [&](QList<Production> &ps) {
        bool changed = false;
        for (quint32 cueId : cueIds)
        {
            CueList *l = nullptr;
            Cue *c = cueIn(ps, cueId, &l);
            if (c == nullptr || l->looks || c->skip == skip)
                continue;
            c->skip = skip;
            changed = true;
        }
        return changed;
    });
}

bool ShowTreeController::isCueSkipped(quint32 cueId) const
{
    const Cue *c = cue(cueId);
    return c != nullptr && c->skip;
}

bool ShowTreeController::setCuesColour(const QList<quint32> &cueIds, const QString &colour)
{
    QString hex;
    if (!colour.isEmpty() && !parseTextColour(colour, &hex))
        return false;
    const QString label = cueIds.count() == 1 ? tr("Cue colour") : tr("Colour of %1 cues").arg(cueIds.count());
    return edit(label, [&](QList<Production> &ps) {
        bool changed = false;
        for (quint32 cueId : cueIds)
        {
            Cue *c = cueIn(ps, cueId);
            if (c == nullptr || c->colour == hex)
                continue;
            c->colour = hex;
            changed = true;
        }
        return changed;
    });
}

bool ShowTreeController::setListColour(quint32 listId, const QString &colour)
{
    QString hex;
    if (!colour.isEmpty() && !parseTextColour(colour, &hex))
        return false;
    return edit(tr("Cue list colour"), [&](QList<Production> &ps) {
        CueList *l = listIn(ps, listId);
        if (l == nullptr || l->colour == hex)
            return false;
        l->colour = hex;
        return true;
    });
}

QString ShowTreeController::cueColour(quint32 cueId) const
{
    const Cue *c = cue(cueId);
    if (c == nullptr)
        return QString();
    if (!c->colour.isEmpty())
        return c->colour;
    const CueList *l = listOfCue(cueId);
    if (l == nullptr)
        return QString();
    if (!l->colour.isEmpty())
        return l->colour;
    // A sub-cue: its cue's.
    return l->isSubCues() && l->subOf != cueId ? cueColour(l->subOf) : QString();
}

QString ShowTreeController::listColour(quint32 listId) const
{
    const CueList *l = cueList(listId);
    return l == nullptr ? QString() : l->colour;
}

/* Q38: move in black per list and per cue */

bool ShowTreeController::setListMoveInBlack(quint32 listId, MoveInBlack mode)
{
    const QString label = mode == MoveInBlack::On ? tr("Move in black: On")
                        : mode == MoveInBlack::Off ? tr("Move in black: Off")
                        : tr("Move in black: the show's");
    return edit(label, [&](QList<Production> &ps) {
        CueList *l = listIn(ps, listId);
        if (l != nullptr && l->isSubCues())
        {
            CueList *owner = nullptr;           // a sub-cue list: its cue's list
            l = cueIn(ps, l->subOf, &owner) != nullptr ? owner : nullptr;
        }
        if (l == nullptr || l->looks || l->isSubCues() || l->moveInBlack == mode)
            return false;
        l->moveInBlack = mode;
        return true;
    });
}

MoveInBlack ShowTreeController::listMoveInBlack(quint32 listId) const
{
    const CueList *l = cueList(listId);
    if (l != nullptr && l->isSubCues())
        l = listOfCue(l->subOf);
    return l == nullptr ? MoveInBlack::Show : l->moveInBlack;
}

bool ShowTreeController::setCuesNoMoveInBlack(const QList<quint32> &cueIds, bool no)
{
    const QString label = no ? (cueIds.count() == 1 ? tr("Don't move in black into cue")
                                                    : tr("Don't move in black into %1 cues").arg(cueIds.count()))
                             : (cueIds.count() == 1 ? tr("Move in black into cue")
                                                    : tr("Move in black into %1 cues").arg(cueIds.count()));
    return edit(label, [&](QList<Production> &ps) {
        bool changed = false;
        for (quint32 cueId : cueIds)
        {
            CueList *l = nullptr;
            Cue *c = cueIn(ps, cueId, &l);
            if (c == nullptr || l->looks || c->noMoveInBlack == no)
                continue;
            c->noMoveInBlack = no;
            changed = true;
        }
        return changed;
    });
}

bool ShowTreeController::setCueTimecode(quint32 cueId, qint32 ms)
{
    ms = qMax<qint32>(-1, ms);
    return edit(ms < 0 ? tr("Clear cue timecode") : tr("Set cue timecode"), [&](QList<Production> &ps) {
        CueList *l = nullptr;
        Cue *c = cueIn(ps, cueId, &l);
        if (c == nullptr || l->looks || l->isSubCues() || c->timecode == ms)
            return false;
        c->timecode = ms;
        return true;
    });
}

bool ShowTreeController::setListFollowTimecode(quint32 listId, bool follow)
{
    return edit(follow ? tr("Follow timecode") : tr("Stop following timecode"), [&](QList<Production> &ps) {
        CueList *l = listIn(ps, listId);
        if (l == nullptr || l->looks || l->isSubCues() || l->followTimecode == follow)
            return false;
        l->followTimecode = follow;
        return true;
    });
}

bool ShowTreeController::isCueNoMoveInBlack(quint32 cueId) const
{
    const Cue *c = cue(cueId);
    return c != nullptr && c->noMoveInBlack;
}

bool ShowTreeController::setCuesFocusCall(const QList<quint32> &cueIds, bool on)
{
    const QString label = on ? (cueIds.count() == 1 ? tr("Make cue a venue focus cue")
                                                    : tr("Make %1 cues venue focus cues").arg(cueIds.count()))
                             : (cueIds.count() == 1 ? tr("Cue no longer a venue focus cue")
                                                    : tr("%1 cues no longer venue focus cues").arg(cueIds.count()));
    return edit(label, [&](QList<Production> &ps) {
        bool changed = false;
        for (quint32 cueId : cueIds)
        {
            CueList *l = nullptr;
            Cue *c = cueIn(ps, cueId, &l);
            if (c == nullptr || l->looks || c->focusCall == on)
                continue;
            c->focusCall = on;
            changed = true;
        }
        return changed;
    });
}

bool ShowTreeController::isCueFocusCall(quint32 cueId) const
{
    const Cue *c = cue(cueId);
    return c != nullptr && c->focusCall;
}

bool ShowTreeController::setCuesNeedsWork(const QList<quint32> &cueIds, bool on)
{
    const QString label = on ? (cueIds.count() == 1 ? tr("Mark cue Needs work")
                                                    : tr("Mark %1 cues Needs work").arg(cueIds.count()))
                             : (cueIds.count() == 1 ? tr("Clear Needs work on cue")
                                                    : tr("Clear Needs work on %1 cues").arg(cueIds.count()));
    return edit(label, [&](QList<Production> &ps) {
        bool changed = false;
        for (quint32 cueId : cueIds)
        {
            Cue *c = cueIn(ps, cueId, nullptr);
            if (c == nullptr || c->needsWork == on)
                continue;
            c->needsWork = on;
            changed = true;
        }
        return changed;
    });
}

bool ShowTreeController::isCueNeedsWork(quint32 cueId) const
{
    const Cue *c = cue(cueId);
    return c != nullptr && c->needsWork;
}

QList<quint32> ShowTreeController::needsWorkCues() const
{
    QList<quint32> out;
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            for (const Cue &c : l.cues)
                if (c.needsWork)
                    out << c.id;
    return out;
}

bool ShowTreeController::showMoveInBlack() const
{
    return m_doc != nullptr && m_doc->programmer() != nullptr && m_doc->programmer()->isAutoMoveInBlack();
}

bool ShowTreeController::moveInBlackInto(quint32 cueId, bool showDefault) const
{
    const Cue *c = cue(cueId);
    const CueList *l = listOfCue(cueId);
    if (c == nullptr || l == nullptr || l->looks)
        return showDefault;
    // A sub-cue: its own flag, its cue's, and its cue's list's setting.
    bool off = c->noMoveInBlack;
    if (l->isSubCues())
    {
        if (const Cue *parent = cue(l->subOf))
            off = off || parent->noMoveInBlack;
        l = listOfCue(l->subOf);
        if (l == nullptr)
            return !off && showDefault;
    }
    return effectiveMoveInBlack(showDefault, l->moveInBlack, off);
}

bool ShowTreeController::listMovesInBlack(quint32 listId, bool showDefault) const
{
    return effectiveMoveInBlack(showDefault, listMoveInBlack(listId), false);
}

bool ShowTreeController::hasMoveInBlackOverrides(int *lists, int *cues) const
{
    int nl = 0, nc = 0;
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
        {
            if (l.looks)
                continue;
            if (!l.isSubCues() && l.moveInBlack != MoveInBlack::Show)
                nl++;
            for (const Cue &c : l.cues)
                if (c.noMoveInBlack)
                    nc++;
        }
    if (lists != nullptr)
        *lists = nl;
    if (cues != nullptr)
        *cues = nc;
    return nl + nc > 0;
}

bool ShowTreeController::anyListMoveInBlackOn() const
{
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            if (!l.looks && !l.isSubCues() && l.moveInBlack == MoveInBlack::On)
                return true;
    return false;
}

bool ShowTreeController::moveInBlackForNext(quint32 chaserId, int step, quint32 functionId,
                                            bool showDefault) const
{
    // A running list (or a cue's sub-cues): the cue its next step plays.
    if (chaserId != invalidId())
        for (const Production &p : m_productions)
            for (const CueList &l : p.lists)
            {
                if (l.looks || l.chaser != chaserId)
                    continue;
                quint32 cueId = invalidId();
                if (!l.isSubCues())
                    cueId = step >= 0 ? cueOfStep(l.id, step) : invalidId();
                else if (step >= 0 && step < l.cues.count())
                    cueId = l.cues.at(step).id;
                return cueId != invalidId() ? moveInBlackInto(cueId, showDefault)
                                            : listMovesInBlack(l.id, showDefault);
            }
    if (functionId == invalidId())
        return showDefault;
    // A Show's next item: a cue's collection, or a whole list's chaser.
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
        {
            if (l.looks)
                continue;
            if (!l.isSubCues() && l.chaser == functionId)
            {
                // The list starts at its first cue.
                return l.cues.isEmpty() ? listMovesInBlack(l.id, showDefault)
                                        : moveInBlackInto(l.cues.first().id, showDefault);
            }
            for (const Cue &c : l.cues)
                if (c.collection == functionId)
                    return moveInBlackInto(c.id, showDefault);
        }
    return showDefault;
}

bool ShowTreeController::setCueMixOverlap(quint32 cueId, bool mix)
{
    return edit(mix ? tr("Overlap: mix") : tr("Overlap: lower wins"), [&](QList<Production> &ps) {
        Cue *c = cueIn(ps, cueId);
        if (c == nullptr || c->mixOverlap == mix)
            return false;
        c->mixOverlap = mix;
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
    copy.number = numberForInsert(*have, at, takenNumbers(m_productions, *have));
    QHash<quint32, quint32> ids;
    for (Layer &l : copy.layers)
    {
        const quint32 nid = newId();
        ids.insert(l.id, nid);
        l.id = nid;
    }
    for (Layer &l : copy.layers)
        if (l.parent != invalidId())
            l.parent = ids.value(l.parent, invalidId());     // the copy's own parent
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

QList<quint32> ShowTreeController::pasteCues(const QList<quint32> &cueIds, quint32 afterCueId)
{
    const CueList *dest = listOfCue(afterCueId);
    if (dest == nullptr || cueIds.isEmpty())
        return {};
    const quint32 destId = dest->id;
    // Copies made up front (ids from the counter); numbered in the edit,
    // one after the other.
    QList<Cue> copies;
    for (quint32 cid : cueIds)
    {
        const CueList *from = listOfCue(cid);
        const Cue *src = from == nullptr ? nullptr : from->cue(cid);
        if (src == nullptr)
            continue;
        Cue copy = *src;
        copy.id = newId();
        copy.collection = invalidId();
        if (from->id != destId)
            copy.includes.clear();          // a cue of another list
        QHash<quint32, quint32> ids;
        for (Layer &l : copy.layers)
        {
            const quint32 nid = newId();
            ids.insert(l.id, nid);
            l.id = nid;
        }
        for (Layer &l : copy.layers)
            if (l.parent != invalidId())
                l.parent = ids.value(l.parent, invalidId());
        copies << copy;
    }
    if (copies.isEmpty())
        return {};
    QList<quint32> made;
    const bool ok = edit(copies.count() == 1 ? tr("Paste cue") : tr("Paste %1 cues").arg(copies.count()),
                         [&](QList<Production> &ps) {
        CueList *l = listIn(ps, destId);
        if (l == nullptr)
            return false;
        int at = l->indexOf(afterCueId);
        if (at < 0)
            return false;
        for (Cue c : copies)
        {
            c.number = numberForInsert(*l, at, takenNumbers(ps, *l));
            if (c.number.isEmpty())
                return false;
            // An include of a cue that isn't there, or would loop: dropped.
            const QList<quint32> wanted = c.includes;
            c.includes.clear();
            l->cues.insert(at + 1, c);
            for (quint32 inc : wanted)
                if (canInclude(*l, c.id, inc))
                    l->cues[at + 1].includes << inc;
            made << c.id;
            ++at;
        }
        return true;
    });
    return ok ? made : QList<quint32>();
}

QList<quint32> ShowTreeController::pasteLayers(const QList<QPair<quint32, quint32>> &layers, quint32 toCueId,
                                               quint32 afterLayerId)
{
    if (cue(toCueId) == nullptr)
        return {};
    QList<Layer> copies;
    QList<Target> parentTargets;            // per copy: its parent's target, if any
    QHash<quint32, int> copyOf;             // original layer id -> copy index
    for (const QPair<quint32, quint32> &ref : layers)
    {
        const Cue *c = cue(ref.first);
        const Layer *l = c == nullptr ? nullptr : c->layer(ref.second);
        if (l == nullptr || copyOf.contains(l->id))
            continue;
        Layer copy = *l;
        copy.continues = Target();
        copy.continuesAlso.clear();
        const Layer *up = l->parent == invalidId() ? nullptr : c->layer(l->parent);
        parentTargets << (up == nullptr ? Target() : up->target);
        copyOf.insert(l->id, int(copies.count()));
        copy.id = newId();
        copies << copy;
    }
    if (copies.isEmpty())
        return {};
    // A parent copied too: the copy's parent.
    for (int i = 0; i < copies.count(); ++i)
    {
        const quint32 orig = copies.at(i).parent;
        copies[i].parent = copyOf.contains(orig) ? copies.at(copyOf.value(orig)).id : invalidId();
    }
    QList<quint32> made;
    const bool ok = edit(copies.count() == 1 ? tr("Paste layer") : tr("Paste %1 layers").arg(copies.count()),
                         [&](QList<Production> &ps) {
        CueList *toList = nullptr;
        Cue *c = cueIn(ps, toCueId, &toList);
        if (c == nullptr)
            return false;
        // After the layer given (and its children): a top layer's place.
        int place = afterLayerId == layerFirst ? 0 : int(c->layers.count());
        if (const Layer *after = afterLayerId == invalidId() ? nullptr : c->layer(afterLayerId))
        {
            const QList<quint32> block = after->parent == invalidId() ? withChildren(*c, afterLayerId)
                                                                       : QList<quint32> { afterLayerId };
            place = 0;
            for (int k = 0; k < c->layers.count(); ++k)
                if (block.contains(c->layers.at(k).id))
                    place = k + 1;
        }
        for (int i = 0; i < copies.count(); ++i)
        {
            Layer l = copies.at(i);
            if (toList != nullptr && toList->looks)
                l.release = false;              // a hit never tracks: no Release
            if (l.parent == invalidId() && !parentTargets.at(i).isEmpty())
                for (const Layer &have : c->layers)
                    if (have.target == parentTargets.at(i))
                        l.parent = have.id;
            Layer *same = nullptr;
            for (Layer &have : c->layers)
                if (have.target == l.target && have.parent == l.parent)
                    same = &have;
            if (same != nullptr)
            {
                // One layer per target: the pasted looks set there.
                for (const Look &look : l.looks)
                    same->setLook(look);
                same->release = l.release;
                made << same->id;
                continue;
            }
            if (l.parent != invalidId())
            {
                // After its parent's children.
                const QList<quint32> under = withChildren(*c, l.parent);
                int at = 0;
                for (int k = 0; k < c->layers.count(); ++k)
                    if (under.contains(c->layers.at(k).id))
                        at = k + 1;
                c->layers.insert(at, l);
            }
            else
                c->layers.insert(qMin(place++, int(c->layers.count())), l);
            made << l.id;
        }
        return true;
    });
    return ok ? made : QList<quint32>();
}

QList<quint32> ShowTreeController::moveLayers(const QList<QPair<quint32, quint32>> &layers, quint32 toCueId,
                                              quint32 afterLayerId, QString *why)
{
    QList<quint32> made;
    QString reason;
    const bool ok = edit(layers.count() == 1 ? tr("Move layer") : tr("Move %1 layers").arg(layers.count()),
                         [&](QList<Production> &ps) {
        CueList *toList = nullptr;
        Cue *dest = cueIn(ps, toCueId, &toList);
        if (dest == nullptr)
            return false;
        // The layers asked for, once each; a layer whose parent comes too
        // is carried by it (children go with their parent).
        struct Item { quint32 cue; Layer layer; Target parentTarget; bool root; };
        QList<Item> moving;
        QList<QPair<quint32, quint32>> refs;
        for (const QPair<quint32, quint32> &ref : layers)
        {
            Cue *src = cueIn(ps, ref.first);
            if (src == nullptr || src->layer(ref.second) == nullptr || refs.contains(ref))
                continue;
            refs << ref;
        }
        QList<quint32> ids;
        for (const QPair<quint32, quint32> &ref : refs)
            ids << ref.second;
        for (const QPair<quint32, quint32> &ref : refs)
        {
            Cue *src = cueIn(ps, ref.first);
            const Layer *l = src->layer(ref.second);
            const bool top = l->parent == invalidId() || !ids.contains(l->parent);
            if (!top)
                continue;
            if (ref.first == toCueId && l->parent != invalidId())
            {
                reason = tr("A child layer stays under its parent: move the parent instead.");
                return false;
            }
            const Layer *up = l->parent == invalidId() ? nullptr : src->layer(l->parent);
            // The layer, then its children in the order they have.
            for (const Layer &have : src->layers)
                if (withChildren(*src, l->id).contains(have.id))
                    moving << Item { ref.first, have, up == nullptr ? Target() : up->target, have.id == l->id };
        }
        if (moving.isEmpty())
            return false;
        QList<quint32> movingIds;
        for (const Item &it : moving)
            movingIds << it.layer.id;
        if (afterLayerId != invalidId() && movingIds.contains(afterLayerId))
        {
            reason = tr("It is already there.");
            return false;
        }
        // Out of where they are.
        for (const Item &it : moving)
        {
            Cue *src = cueIn(ps, it.cue);
            for (int i = src->layers.count() - 1; i >= 0; --i)
                if (src->layers.at(i).id == it.layer.id)
                    src->layers.removeAt(i);
        }
        int place = afterLayerId == layerFirst ? 0 : int(dest->layers.count());
        if (const Layer *after = afterLayerId == invalidId() ? nullptr : dest->layer(afterLayerId))
        {
            const QList<quint32> block = after->parent == invalidId() ? withChildren(*dest, afterLayerId)
                                                                       : QList<quint32> { afterLayerId };
            place = 0;
            for (int k = 0; k < dest->layers.count(); ++k)
                if (block.contains(dest->layers.at(k).id))
                    place = k + 1;
        }
        QHash<quint32, quint32> merged;         // a moved layer -> the layer it joined there
        for (Item &it : moving)
        {
            Layer l = it.layer;
            if (it.cue != toCueId)
            {
                l.continues = Target();
                l.continuesAlso.clear();
            }
            if (toList != nullptr && toList->looks)
                l.release = false;              // a hit never tracks: no Release
            if (merged.contains(l.parent))
                l.parent = merged.value(l.parent);
            else if (it.root && it.cue != toCueId)
            {
                l.parent = invalidId();
                if (!it.parentTarget.isEmpty())
                    for (const Layer &have : dest->layers)
                        if (have.target == it.parentTarget)
                            l.parent = have.id;
            }
            Layer *same = nullptr;
            if (it.cue != toCueId)
                for (Layer &have : dest->layers)
                    if (have.target == l.target && have.parent == l.parent)
                        same = &have;
            if (same != nullptr)
            {
                // One layer per target: the moved looks set there.
                for (const Look &look : l.looks)
                    same->setLook(look);
                same->release = l.release;
                merged.insert(l.id, same->id);
                made << same->id;
                continue;
            }
            int at = qMin(place, int(dest->layers.count()));
            if (l.parent != invalidId() && it.cue != toCueId && it.root)
            {
                // After its parent's children.
                const QList<quint32> under = withChildren(*dest, l.parent);
                at = 0;
                for (int k = 0; k < dest->layers.count(); ++k)
                    if (under.contains(dest->layers.at(k).id))
                        at = k + 1;
                dest->layers.insert(at, l);
            }
            else
            {
                dest->layers.insert(at, l);
                place = at + 1;
            }
            made << l.id;
        }
        return true;
    });
    if (why != nullptr)
        *why = reason;
    return ok ? made : QList<quint32>();
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
            stageLayers(resolved(l).at(at), out, noId);
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
                    // A busking look fired from VC (R3): its layers, not
                    // its generated scenes (those are the tree's).
                    if (l.looks)
                        for (const Cue &c : l.cues)
                        {
                            const Function *f = m_doc->function(c.collection);
                            if (f != nullptr && f->isRunning())
                                cueStage(l, c.id);
                        }
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
    rec.number = numberForInsert(*have, at, takenNumbers(m_productions, *have));
    if (source == RecordSource::Lit)
        rec.name = tr("on stage (recorded)");
    else
        rec.name = src.name.isEmpty() ? tr("recorded") : tr("%1 (recorded)").arg(src.name);
    rec.mode = CueMode::Block;
    // What mixed there mixes here, so it plays the same.
    rec.mixOverlap = src.mixOverlap;
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
    else if (unit.startsWith(QLatin1String("s:")))
        t.sets << id;
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
    if (!childLayers(cueId, layerId).isEmpty())
    {
        if (why != nullptr)
            *why = tr("%1 has child layers: take them out first.").arg(targetLabel(l->target));
        return false;
    }
    if (l->reusable != invalidId())
    {
        if (why != nullptr)
            *why = tr("%1 is reusable: Unlink here first (every use would split).").arg(reusableName(l->reusable));
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
        const QList<ResolvedCue> before = resolved(beforeList);
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
        if (stageDiffers(beforeList, *list, &cid, &unit, &key, lineAimTest()))
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
        if (!childLayers(cueId, lid).isEmpty())
            return refuse(tr("%1 has child layers: take them out first.").arg(targetLabel(l->target)));
        if (l->reusable != invalidId())
            return refuse(tr("%1 is reusable: Unlink here first.").arg(reusableName(l->reusable)));
        if (!layers.isEmpty() && l->parent != layers.first()->parent)
            return refuse(tr("Only layers under the same parent can be merged."));
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
            for (quint32 set : o.target.sets)
                if (!m.target.sets.contains(set))
                    m.target.sets << set;
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
        const QList<ResolvedCue> before = resolved(beforeList);
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
        if (stageDiffers(beforeList, *list, &cid, &unit, &key, lineAimTest()))
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
    const QList<ResolvedCue> resolved = this->resolved(*list);

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
    // Its cues' sub-cues are part of the song too (P6).
    QList<const Cue *> cues;
    for (const Cue &c : list->cues)
        cues << &c;
    for (const Production &p : m_productions)
        for (const CueList &s : p.lists)
            if (s.isSubCues() && list->indexOf(s.subOf) >= 0)
                for (const Cue &c : s.cues)
                    cues << &c;
    for (const Cue *cp : cues)
    {
        const Cue &c = *cp;
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
            for (quint32 set : l.target.sets)
                u.sets[set]++;
        }
    }
    return u;
}

bool ShowTreeController::renameNode(quint32 nodeId, const QString &name)
{
    // A name only: a layer's identity is its target (an empty name goes
    // back to the automatic one, its groups'), a cue's is its number.
    return edit(tr("Rename"), [&](QList<Production> &ps) {
        auto set = [&name](QString &have) {
            if (have == name)
                return false;
            have = name;
            return true;
        };
        for (Production &p : ps)
        {
            if (p.id == nodeId)
                return !name.isEmpty() && set(p.name);
            for (CueList &l : p.lists)
            {
                if (l.id == nodeId)
                    return !name.isEmpty() && !l.looks && set(l.name);
                for (Cue &c : l.cues)
                {
                    if (c.id == nodeId)
                        return set(c.name);
                    if (Layer *layer = c.layer(nodeId))
                        return set(layer->name);
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
 * Group sets (§1)
 ****************************************************************************/

const GroupSet *ShowTreeController::groupSet(quint32 id) const
{
    for (const GroupSet &gs : m_groupSets)
        if (gs.id == id)
            return &gs;
    return nullptr;
}

bool ShowTreeController::editSets(const QString &label, const std::function<bool(QList<GroupSet> &)> &change)
{
    if (m_batchDepth > 0)
    {
        QList<GroupSet> work = m_groupSets;
        if (!change(work))
            return false;
        m_groupSets = work;
        m_batchChanged = true;
        return true;
    }
    const QByteArray before = snapshotXml();
    QList<GroupSet> work = m_groupSets;
    if (!change(work))
        return false;
    m_groupSets = work;
    pushUndo(label, before);
    bake();
    m_doc->setModified();
    emit changed();
    return true;
}

/** @a ids once each, in order. */
static QList<quint32> unique(const QList<quint32> &ids)
{
    QList<quint32> out;
    for (quint32 id : ids)
        if (!out.contains(id))
            out << id;
    return out;
}

quint32 ShowTreeController::addGroupSet(const QString &name, const QList<quint32> &groups,
                                        const QList<quint32> &fixtures)
{
    const QList<quint32> g = unique(groups), f = unique(fixtures);
    if (g.isEmpty() && f.isEmpty())
        return invalidId();
    QString n = name.trimmed().isEmpty() ? tr("Set") : name.trimmed();
    auto taken = [this](const QString &x) {
        for (const GroupSet &gs : m_groupSets)
            if (gs.name.compare(x, Qt::CaseInsensitive) == 0)
                return true;
        return false;
    };
    if (taken(n))
    {
        int k = 2;
        while (taken(QStringLiteral("%1 (%2)").arg(n).arg(k)))
            ++k;
        n = QStringLiteral("%1 (%2)").arg(n).arg(k);
    }
    const quint32 id = newId();
    const bool ok = editSets(tr("Make set %1").arg(n), [&](QList<GroupSet> &sets) {
        GroupSet gs;
        gs.id = id;
        gs.name = n;
        gs.groups = g;
        gs.fixtures = f;
        sets << gs;
        return true;
    });
    return ok ? id : invalidId();
}

bool ShowTreeController::setGroupSetMembers(quint32 setId, const QList<quint32> &groups,
                                            const QList<quint32> &fixtures)
{
    const QList<quint32> g = unique(groups), f = unique(fixtures);
    if (g.isEmpty() && f.isEmpty())
        return false;
    const GroupSet *have = groupSet(setId);
    if (have == nullptr)
        return false;
    // Child layers inside a layer on the set must stay inside it.
    GroupSet after = *have;
    after.groups = g;
    after.fixtures = f;
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            for (const Cue &c : l.cues)
                for (const Layer &child : c.layers)
                {
                    const Layer *up = child.parent == invalidId() ? nullptr : c.layer(child.parent);
                    if (up == nullptr || !up->target.sets.contains(setId))
                        continue;
                    Target outer = expandTarget(up->target);
                    for (quint32 gid : have->groups)
                        outer.groups.removeAll(gid);
                    for (quint32 fid : have->fixtures)
                        outer.fixtures.removeAll(fid);
                    outer.groups << g;
                    outer.fixtures << f;
                    if (!targetContains(outer, child.target))
                        return false;
                }
    return editSets(tr("Set %1's members").arg(have->name), [&](QList<GroupSet> &sets) {
        for (GroupSet &gs : sets)
            if (gs.id == setId)
            {
                if (gs.groups == g && gs.fixtures == f)
                    return false;
                gs.groups = g;
                gs.fixtures = f;
                return true;
            }
        return false;
    });
}

bool ShowTreeController::renameGroupSet(quint32 setId, const QString &name)
{
    if (name.trimmed().isEmpty())
        return false;
    return editSets(tr("Rename set"), [&](QList<GroupSet> &sets) {
        for (GroupSet &gs : sets)
            if (gs.id == setId)
            {
                if (gs.name == name.trimmed())
                    return false;
                gs.name = name.trimmed();
                return true;
            }
        return false;
    });
}

bool ShowTreeController::removeGroupSet(quint32 setId, QString *why)
{
    const GroupSet *have = groupSet(setId);
    if (have == nullptr)
        return false;
    QStringList used;
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            for (int i = 0; i < l.cues.count(); ++i)
                for (const Layer &layer : l.cues.at(i).layers)
                    if (layer.target.sets.contains(setId) || layer.continues.sets.contains(setId))
                    {
                        const QString label = cueLabel(l, i);
                        if (!used.contains(label))
                            used << label;
                    }
    for (const ReusableLayer &r : m_reusableLayers)
        if (r.target.sets.contains(setId))
            used << tr("reusable %1").arg(r.name);
    for (const ReusableCue &r : m_reusableCues)
        for (const Layer &layer : r.layers)
            if (layer.target.sets.contains(setId) && !used.contains(tr("reusable %1").arg(r.name)))
                used << tr("reusable %1").arg(r.name);
    if (!used.isEmpty())
    {
        if (why != nullptr)
            *why = tr("%1 is used in cue %2: take it out of those layers first.")
                   .arg(have->name, used.mid(0, 5).join(QStringLiteral(", ")));
        return false;
    }
    const QString name = have->name;
    return editSets(tr("Delete set %1").arg(name), [&](QList<GroupSet> &sets) {
        for (int i = 0; i < sets.count(); ++i)
            if (sets.at(i).id == setId)
            {
                sets.removeAt(i);
                return true;
            }
        return false;
    });
}

Target ShowTreeController::expandTarget(const Target &target) const
{
    if (target.sets.isEmpty())
        return target;
    Target out;
    out.function = target.function;
    for (quint32 set : target.sets)
        if (const GroupSet *gs = groupSet(set))
        {
            for (quint32 g : gs->groups)
                if (!out.groups.contains(g))
                    out.groups << g;
            for (quint32 f : gs->fixtures)
                if (!out.fixtures.contains(f))
                    out.fixtures << f;
        }
    for (quint32 g : target.groups)
        if (!out.groups.contains(g))
            out.groups << g;
    for (quint32 f : target.fixtures)
        if (!out.fixtures.contains(f))
            out.fixtures << f;
    return out;
}

QList<quint32> ShowTreeController::targetFixtures(const Target &target) const
{
    return fixturesOfTarget(m_doc, expandTarget(target));
}

/****************************************************************************
 * Child layers (§3.5)
 ****************************************************************************/

bool ShowTreeController::targetContains(const Target &outer, const Target &inner) const
{
    if (outer.isOpaque() || inner.isOpaque())
        return false;
    const QList<quint32> out = targetFixtures(outer), in = targetFixtures(inner);
    if (in.isEmpty())
        return false;
    for (quint32 f : in)
        if (!out.contains(f))
            return false;
    return true;
}

QList<quint32> ShowTreeController::childLayers(quint32 cueId, quint32 layerId) const
{
    const Cue *c = cue(cueId);
    if (c == nullptr || c->layer(layerId) == nullptr)
        return {};
    QList<quint32> all = withChildren(*c, layerId);
    all.removeFirst();
    return all;
}

quint32 ShowTreeController::addChildLayer(quint32 cueId, quint32 parentId, const Target &target, QString *why)
{
    auto refuse = [why](const QString &text) {
        if (why != nullptr)
            *why = text;
        return invalidId();
    };
    const Cue *c = cue(cueId);
    const Layer *up = c == nullptr ? nullptr : c->layer(parentId);
    if (up == nullptr || up->target.isOpaque())
        return refuse(tr("A child layer goes under a layer of groups or fixtures of this cue."));
    if (target.isEmpty() || target.isOpaque())
        return refuse(tr("A child layer is on groups or fixtures."));
    if (target == up->target)
        return refuse(tr("A child layer is on part of %1, not all of it.").arg(targetLabel(up->target)));
    if (!targetContains(up->target, target))
        return refuse(tr("%1 isn't part of %2: a child layer is on part of its parent's fixtures.")
                      .arg(targetLabel(target), targetLabel(up->target)));
    for (const Layer &l : c->layers)
        if (l.parent == parentId && l.target == target)
            return refuse(tr("%1 already has a child layer on %2.").arg(targetLabel(up->target),
                                                                         targetLabel(target)));
    const quint32 id = newId();
    const bool ok = edit(tr("Add child layer %1").arg(targetLabel(target)), [&](QList<Production> &ps) {
        Cue *cc = cueIn(ps, cueId);
        if (cc == nullptr || cc->layer(parentId) == nullptr)
            return false;
        // After the parent and its children: lower wins, so a child is under
        // (after) its parent.
        const QList<quint32> under = withChildren(*cc, parentId);
        int at = 0;
        for (int i = 0; i < cc->layers.count(); ++i)
            if (under.contains(cc->layers.at(i).id))
                at = i + 1;
        Layer l;
        l.id = id;
        l.target = target;
        l.parent = parentId;
        cc->layers.insert(at, l);
        return true;
    });
    return ok ? id : refuse(tr("Couldn't add the child layer."));
}

/****************************************************************************
 * Line aims (§3B)
 ****************************************************************************/

QString ShowTreeController::lineParamValue(const QString &key, const QString &word)
{
    const QString w = word.trimmed().toLower();
    if (key == KeySpread)
    {
        bool ok = false;
        const double v = QString(w).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
        if (!ok || v < -LineSpread::maxSpread() - 1e-9 || v > LineSpread::maxSpread() + 1e-9)
            return QString();
        // One decimal, as the slider steps.
        return QString::number(qRound(v * 10.0) / 10.0);
    }
    if (key == KeyGrouping)
    {
        static const QStringList words { "each", "pairs", "threes", "fours", "halves" };
        for (const QString &x : words)
            if (w == x || (w.length() >= 3 && x.startsWith(w)) || w + QLatin1Char('s') == x)
                return x;
        if (w == QLatin1String("1") || w == QLatin1String("single") || w == QLatin1String("one"))
            return QStringLiteral("each");
        if (w == QLatin1String("2"))
            return QStringLiteral("pairs");
        if (w == QLatin1String("3"))
            return QStringLiteral("threes");
        if (w == QLatin1String("4"))
            return QStringLiteral("fours");
        if (w == QLatin1String("half"))
            return QStringLiteral("halves");
        return QString();
    }
    if (key == KeyCross)
    {
        if (w == QLatin1String("swap") || w == QLatin1String("swaps") || w == QLatin1String("sides"))
            return QStringLiteral("swap");
        if (w == QLatin1String("mirror") || w == QLatin1String("flip"))
            return QStringLiteral("mirror");
    }
    return QString();
}

Look ShowTreeController::lineParamLook(const QString &key, const QString &value)
{
    Look l;
    l.key = key;
    l.value = lineParamValue(key, value);
    return l;
}

QString ShowTreeController::lineParamText(const QString &key, const QString &value)
{
    if (key == KeySpread)
    {
        const double v = value.toDouble();
        return v > 0 ? QStringLiteral("+%1").arg(value) : value;
    }
    if (key == KeyGrouping)
    {
        if (value == QLatin1String("each"))
            return tr("Each light");
        if (value.isEmpty())
            return value;
        return value.left(1).toUpper() + value.mid(1);
    }
    if (key == KeyCross)
        return value == QLatin1String("mirror") ? tr("Each group flips (mirror)") : tr("Groups swap sides");
    return value;
}

/** A line parameter look applied to @a s. */
static void applyLineParam(LineSpread::Settings &s, const QString &key, const QString &value)
{
    if (value.isEmpty())
        return;
    if (key == KeySpread)
        s.spread = qBound(-LineSpread::maxSpread(), value.toDouble(), LineSpread::maxSpread());
    else if (key == KeyGrouping)
    {
        if (value == QLatin1String("each"))
            s.group = 1;
        else if (value == QLatin1String("pairs"))
            s.group = 2;
        else if (value == QLatin1String("threes"))
            s.group = 3;
        else if (value == QLatin1String("fours"))
            s.group = 4;
        else if (value == QLatin1String("halves"))
            s.group = LineSpread::Halves;
    }
    else if (key == KeyCross)
        s.cross = value == QLatin1String("mirror") ? LineSpread::Mirror : LineSpread::SwapSides;
}

quint32 ShowTreeController::lineAimPalette(quint32 basePid, const ResolvedLayer &layer, bool create)
{
    QLCPalette *base = m_doc->palette(basePid);
    const MonitorProperties *mp = m_doc->monitorProperties();
    const StageTarget *t = (base == nullptr || mp == nullptr || base->type() != QLCPalette::Aim)
                           ? nullptr : mp->stageTarget(base->stageTargetId());
    if (t == nullptr || !t->isLine())
        return basePid;
    const LineSpread::Settings own = LineSpread::fromParams(base->effectParamValues());
    LineSpread::Settings want = own;
    QStringList what;
    for (const QString &k : { KeySpread, KeyGrouping, KeyCross })
    {
        const auto v = layer.values.constFind(k);
        if (v == layer.values.constEnd() || v.value().look.none || v.value().look.value.isEmpty())
            continue;
        applyLineParam(want, k, v.value().look.value);
    }
    if (want == own)
        return basePid;
    QMap<QString, double> params = base->effectParamValues();
    LineSpread::toParams(want, params);

    // The same line, the same settings: one palette, found again by them.
    QList<quint32> inl = m_inlinePalettes.values();
    std::sort(inl.begin(), inl.end());
    for (quint32 pid : inl)
    {
        const QLCPalette *p = m_doc->palette(pid);
        if (p != nullptr && p->type() == QLCPalette::Aim && p->stageTargetId() == base->stageTargetId()
            && p->effectParamValues() == params)
            return pid;
    }
    if (!create)
        return QLCPalette::invalidId();

    QLCPalette *p = base->createCopy();
    p->setEffectParamValues(params);
    QStringList parts { tr("spread %1").arg(lineParamText(KeySpread, QString::number(want.spread))) };
    if (want.group != 1)
        parts << (want.group == LineSpread::Halves ? tr("halves")
                  : want.group == 2 ? tr("pairs") : want.group == 3 ? tr("threes")
                  : want.group == 4 ? tr("fours") : tr("groups of %1").arg(want.group));
    if (want.cross == LineSpread::Mirror)
        parts << tr("mirror");
    p->setName(QStringLiteral("%1 · %2").arg(base->name(), parts.join(QStringLiteral(", "))));
    p->setPath(QStringLiteral("Palettes/%1").arg(kInlinePaletteFolder));
    quint32 id = 0;
    for (const QLCPalette *other : m_doc->palettes())
        id = qMax(id, other->id() + 1);
    m_inlinePalettes.insert(id);
    if (!m_doc->addPalette(p, id))
    {
        m_inlinePalettes.remove(id);
        delete p;
        return basePid;
    }
    return id;
}

quint32 ShowTreeController::aimPaletteFor(quint32 targetId)
{
    MonitorProperties *mp = m_doc->monitorProperties();
    const StageTarget *t = mp == nullptr ? nullptr : mp->stageTarget(targetId);
    if (t == nullptr)
        return QLCPalette::invalidId();
    // Reused: a named Aim palette pointing there with nothing of its own
    // (no spread, group or cross), the lowest id.
    QList<QLCPalette *> pl = m_doc->palettes();
    std::sort(pl.begin(), pl.end(), [](const QLCPalette *a, const QLCPalette *b) { return a->id() < b->id(); });
    for (const QLCPalette *p : pl)
        if (p->type() == QLCPalette::Aim && p->stageTargetId() == targetId && !isInlinePalette(p->id())
            && !p->name().isEmpty() && LineSpread::fromParams(p->effectParamValues()) == LineSpread::Settings())
            return p->id();
    // Else one named after the target, where the show keeps its Aim looks.
    QLCPalette *p = new QLCPalette(QLCPalette::Aim, m_doc);
    p->setStageTargetId(targetId);
    QString name = tr("Aim %1").arg(t->name());
    auto taken = [&pl](const QString &n) {
        for (const QLCPalette *o : pl)
            if (o->name().compare(n, Qt::CaseInsensitive) == 0)
                return true;
        return false;
    };
    if (taken(name))
    {
        int k = 2;
        while (taken(QStringLiteral("%1 (%2)").arg(name).arg(k)))
            ++k;
        name = QStringLiteral("%1 (%2)").arg(name).arg(k);
    }
    p->setName(name);
    p->setPath(suggestPaletteFolder(QLCPalette::Aim));
    if (!m_doc->addPalette(p))
    {
        delete p;
        return QLCPalette::invalidId();
    }
    return p->id();
}

static QString layerKey(quint32 cueId, const Target &target)
{
    return QStringLiteral("%1|%2").arg(cueId).arg(target.signature());
}

quint32 ShowTreeController::layerScene(quint32 cueId, const Target &target) const
{
    return m_layerScenes.value(layerKey(cueId, target), Function::invalidId());
}

QList<Target> ShowTreeController::spanMembers(quint32 cueId, const Target &target, const QString &key) const
{
    return m_spanMembers.value(layerKey(cueId, target)).value(key);
}

/****************************************************************************
 * What a cue plays: a scene per layer, with what it leaves out and what it
 * lays out over (§3.5, D10, D17)
 ****************************************************************************/

QString ShowTreeController::ScenePlan::signature() const
{
    QString sig = ShowTreeController::sceneSignature(target, palettes);
    QList<quint32> keys = excluded.keys();
    std::sort(keys.begin(), keys.end());
    for (quint32 pid : keys)
        if (!excluded.value(pid).isEmpty())
            sig += QStringLiteral("|x%1:%2").arg(pid).arg(idsToString(excluded.value(pid)));
    keys = spanGroups.keys() + spanFixtures.keys();
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    for (quint32 pid : keys)
        sig += QStringLiteral("|w%1:%2/%3").arg(pid).arg(idsToString(spanGroups.value(pid)),
                                                         idsToString(spanFixtures.value(pid)));
    keys = fades.keys();
    std::sort(keys.begin(), keys.end());
    for (quint32 pid : keys)
        sig += QStringLiteral("|f%1:%2/%3").arg(pid).arg(fades.value(pid).first).arg(fades.value(pid).second);
    return sig;
}

QList<ShowTreeController::ScenePlan> ShowTreeController::planCue(const CueList &list,
                                                                 const QList<ResolvedCue> &resolved,
                                                                 int index) const
{
    return const_cast<ShowTreeController *>(this)->planCue(list, resolved, index, false);
}

QList<ShowTreeController::ScenePlan> ShowTreeController::planCue(const CueList &list,
                                                                 const QList<ResolvedCue> &resolved,
                                                                 int index, bool create)
{
    QList<ScenePlan> out;
    if (index < 0 || index >= resolved.count())
        return out;
    const ResolvedCue &rc = resolved.at(index);
    const int n = int(rc.layers.count());
    // Overlapping layers that mix (an imported cue): only a deeper layer --
    // a child -- leaves its parent out.
    const bool mix = index < list.cues.count() && list.cues.at(index).mixOverlap;

    // What each layer lights.
    QVector<Target> targets(n);
    QVector<QList<quint32>> fixtures(n);
    QVector<QHash<quint32, QList<qint64>>> units(n);   // per fixture: the heads it lights
    for (int i = 0; i < n; ++i)
    {
        const ResolvedLayer &rl = rc.layers.at(i);
        if (rl.target.isOpaque())
            continue;
        targets[i] = expandTarget(rl.target);
        const TargetUnits &tu = targetUnits(targets.at(i));
        fixtures[i] = tu.fixtures;
        units[i] = tu.units;
    }

    /* Who sets each attribute on each fixture (§2, "Overlap"; §3.5): this
       cue beats earlier ones (a layer's own over a cue look; an include
       over what tracked in; the later cue's tracked value); then the
       deeper layer (a child over its parent); then the lower layer. */
    auto rank = [&](int i, const ResolvedValue &v) {
        int cls = 0, recency = 0;
        if (v.fromCue == rc.cueId && v.origin == Origin::Own)
            cls = 3;
        else if (v.fromCue == rc.cueId && v.origin == Origin::CueLook)
            cls = 2;
        else if (v.origin == Origin::Included)
            cls = 1;
        else
            recency = list.indexOf(v.fromCue);
        return std::make_tuple(cls, recency, rc.layers.at(i).depth, i);
    };
    // The layer that wins, and by what rank (-1: none).
    struct Win
    {
        int layer = -1;
        std::tuple<int, int, int, int> rank;
    };
    QHash<QString, QHash<quint32, Win>> winner;
    // The same per head ("fixture|head"): what leaves a layer out.
    QHash<QString, QHash<qint64, Win>> unitWinner;
    auto contest = [](auto &w, auto key, int i, const std::tuple<int, int, int, int> &r) {
        auto at = w.find(key);
        if (at == w.end())
            w.insert(key, Win{ i, r });
        else if (at->rank < r)
            *at = Win{ i, r };
    };
    for (int i = 0; i < n; ++i)
    {
        const ResolvedLayer &rl = rc.layers.at(i);
        if (rl.target.isOpaque())
            continue;
        for (auto it = rl.values.constBegin(); it != rl.values.constEnd(); ++it)
        {
            if (isLineParamKey(it.key()))
                continue;                       // they go with Position
            const auto r = rank(i, it.value());
            QHash<quint32, Win> &w = winner[it.key()];
            QHash<qint64, Win> &wu = unitWinner[it.key()];
            const QHash<quint32, QList<qint64>> &us = units.at(i);
            for (quint32 f : fixtures.at(i))
            {
                contest(w, f, i, r);
                const auto uf = us.constFind(f);
                if (uf != us.constEnd())
                    for (qint64 u : uf.value())
                        contest(wu, u, i, r);
            }
        }
    }

    /* ONE effect / ONE formation across layers (D10, D17): the layers
       carrying the same value set at cue level (the same span), a line aim
       also with the same Spread / Grouping / Cross none of them set itself. */
    auto spanKey = [&](int i, const QString &key) -> QString {
        const ResolvedLayer &rl = rc.layers.at(i);
        const auto v = rl.values.constFind(key);
        if (v == rl.values.constEnd() || v.value().look.none || v.value().span == invalidId())
            return QString();
        QString k = QStringLiteral("%1|%2/%3|%4").arg(key, idsToString(v.value().look.palettes),
                                                      idsToString(v.value().look.own))
                    .arg(v.value().span);
        if (key == KeyPosition)
        {
            for (const QString &pk : { KeySpread, KeyGrouping, KeyCross })
            {
                const auto p = rl.values.constFind(pk);
                if (p == rl.values.constEnd())
                    continue;
                if (p.value().span == invalidId())
                    return QString();           // a parameter of its own: on its own
                k += QStringLiteral("|%1=%2@%3").arg(pk, p.value().look.value).arg(p.value().span);
            }
        }
        return k;
    };

    for (int i = 0; i < n; ++i)
    {
        const ResolvedLayer &rl = rc.layers.at(i);
        if (rl.target.isOpaque() || rl.isDark())
            continue;
        ScenePlan plan;
        plan.layer = i;
        plan.target = targets.at(i);
        plan.target.sets.clear();
        bool writes = false;
        for (const Look &look : rl.playedLooks())
        {
            QList<quint32> pals;
            for (quint32 pid : look.palettes)
            {
                quint32 use = pid;
                if (look.key == KeyPosition)
                    use = lineAimPalette(pid, rl, create);
                if (use != QLCPalette::invalidId() && m_doc->palette(use) != nullptr)
                    pals << use;
            }
            if (isEffectKey(look.key))
                for (quint32 pid : look.own)
                    if (m_doc->palette(pid) != nullptr)
                        pals << pid;
            if (pals.isEmpty())
                continue;

            // Its span: the others it is one with, if any.
            QList<int> members { i };
            const QString sk = (isEffectKey(look.key) || look.key == KeyPosition) ? spanKey(i, look.key) : QString();
            if (!sk.isEmpty())
                for (int j = 0; j < n; ++j)
                    if (j != i && !rc.layers.at(j).target.isOpaque() && !rc.layers.at(j).isDark()
                        && spanKey(j, look.key) == sk)
                        members << j;
            std::sort(members.begin(), members.end());

            // Left out: what it would light where it doesn't win -- a lower
            // layer (or a child) sets that attribute there.
            const QHash<qint64, Win> wu = unitWinner.value(look.key);
            /* Left out of fixture f when layer m wins none of the heads it
               lights there: @p lost says whether a winner beats it. */
            auto losesAll = [&](int m, quint32 f, const std::function<bool(int)> &lost) {
                const QList<qint64> us = units.at(m).value(f);
                if (us.isEmpty())
                    return false;
                for (qint64 u : us)
                    if (!lost(wu.value(u).layer))
                        return false;
                return true;
            };
            QList<quint32> excluded;
            QList<quint32> spanG, spanF;
            if (members.count() > 1)
            {
                for (int m : members)
                {
                    for (quint32 g : targets.at(m).groups)
                        if (!spanG.contains(g))
                            spanG << g;
                    for (quint32 f : targets.at(m).fixtures)
                        if (!spanF.contains(f))
                            spanF << f;
                    for (quint32 f : fixtures.at(m))
                    {
                        if (!excluded.contains(f) && losesAll(m, f, [&](int by) {
                                return !members.contains(by)
                                       && (!mix || (by >= 0 && rc.layers.at(by).depth > rc.layers.at(m).depth));
                            }))
                            excluded << f;
                    }
                }
            }
            else
            {
                for (quint32 f : fixtures.at(i))
                {
                    if (losesAll(i, f, [&](int by) {
                            return by != i && (!mix || (by >= 0 && rc.layers.at(by).depth > rl.depth));
                        }))
                        excluded << f;
                }
            }
            std::sort(excluded.begin(), excluded.end());
            for (quint32 f : fixtures.at(i))
                if (!excluded.contains(f))
                    writes = true;
            // An effect's fades (ms, or the cue's): its depth comes up
            // and goes on them. FadeSnap both ways: none, as before.
            if (isEffectKey(look.key) && (look.fadeIn != Look::FadeSnap || look.fadeOut != Look::FadeSnap))
            {
                // One way snapping, the other not: 0 (at once), so the
                // scene carries a fade in and the effect a depth fade.
                auto fade = [](int ms) {
                    return ms == Look::FadeFollowCue ? Scene::FadeFollow : qMax(0, ms);
                };
                for (quint32 pid : look.palettes)
                    if (pals.contains(pid))
                        plan.fades.insert(pid, qMakePair(fade(look.fadeIn), fade(look.fadeOut)));
            }
            for (quint32 pid : pals)
            {
                plan.palettes << pid;
                if (!excluded.isEmpty())
                    plan.excluded.insert(pid, excluded);
                if (members.count() > 1)
                {
                    plan.spanGroups.insert(pid, spanG);
                    plan.spanFixtures.insert(pid, spanF);
                }
            }
            if (members.count() > 1)
                plan.spanMembers.insert(look.key, members);
        }
        // Every look left out everywhere (a lower layer sets all of it):
        // nothing to play.
        if (plan.palettes.isEmpty() || !writes)
            continue;
        out << plan;
    }

    /* A fixture with no dimmer channel shows its Dimmer only as the colour
       scaled (QLCPalette::dimScaleWithoutDimmer, per scene). Where the
       layer that wins Dimmer there isn't the one that wins Colour (a child
       dimming what its parent colours, open 16; a lower sibling's level),
       the Dimmer's layer plays the winning colour there itself -- the
       colour's layer leaves that fixture out -- so the Dimmer that wins is
       the one that dims it, as the single rule says. Only into a layer
       with no Colour of its own (its own colours' set offsets stay put),
       one colour look per layer. */
    {
        const QHash<quint32, Win> dimW = winner.value(KeyDimmer);
        const QHash<quint32, Win> colW = winner.value(KeyColour);
        QHash<int, QList<quint32>> fillAt;              // dimmer layer -> fixtures
        QHash<int, int> fillFrom;                       // dimmer layer -> colour layer
        QHash<int, QList<quint32>> fillOut;             // colour layer -> fixtures left out
        QList<quint32> fs = dimW.keys();
        std::sort(fs.begin(), fs.end());
        for (quint32 f : fs)
        {
            const int di = dimW.value(f).layer;
            const int cj = colW.value(f).layer;
            if (cj < 0 || cj == di)
                continue;
            if (mix && rc.layers.at(di).depth <= rc.layers.at(cj).depth)
                continue;                               // a mixing cue: as a collection plays
            if (QLCPalette::fixtureHasDimmer(m_doc->fixture(f)))
                continue;
            const ResolvedLayer &dl = rc.layers.at(di), &cl = rc.layers.at(cj);
            const ResolvedValue dv = dl.values.value(KeyDimmer), cv = cl.values.value(KeyColour);
            if (dv.look.none || cv.look.none || cv.look.palettes.isEmpty() || dl.values.contains(KeyColour))
                continue;
            if (fillFrom.contains(di) && rc.layers.at(fillFrom.value(di)).values.value(KeyColour).look.palettes
                                         != cv.look.palettes)
                continue;                               // one colour look per layer
            fillFrom.insert(di, cj);
            fillAt[di] << f;
            fillOut[cj] << f;
        }
        if (!fillAt.isEmpty())
        {
            QSet<int> planned;
            for (const ScenePlan &p : out)
                planned.insert(p.layer);
            // A Dimmer layer all of whose looks were left out has no plan
            // yet: one with only its Dimmer, where it wins.
            for (auto it = fillAt.constBegin(); it != fillAt.constEnd(); ++it)
            {
                if (planned.contains(it.key()))
                    continue;
                ScenePlan p;
                p.layer = it.key();
                p.target = targets.at(it.key());
                p.target.sets.clear();
                QList<quint32> ex;
                for (quint32 f : fixtures.at(it.key()))
                    if (dimW.value(f).layer != it.key())
                        ex << f;
                std::sort(ex.begin(), ex.end());
                for (quint32 pid : rc.layers.at(it.key()).values.value(KeyDimmer).look.palettes)
                    if (m_doc->palette(pid) != nullptr)
                    {
                        p.palettes << pid;
                        if (!ex.isEmpty())
                            p.excluded.insert(pid, ex);
                    }
                out << p;
            }
            QList<ScenePlan> kept;
            for (ScenePlan p : out)
            {
                if (fillAt.contains(p.layer))
                {
                    const QList<quint32> fill = fillAt.value(p.layer);
                    const QList<quint32> cols = rc.layers.at(fillFrom.value(p.layer))
                                                .values.value(KeyColour).look.palettes;
                    QList<quint32> ex;
                    for (quint32 f : fixtures.at(p.layer))
                        if (!fill.contains(f))
                            ex << f;
                    std::sort(ex.begin(), ex.end());
                    QList<quint32> front;
                    for (quint32 pid : cols)
                        if (m_doc->palette(pid) != nullptr && !p.palettes.contains(pid))
                        {
                            front << pid;
                            if (!ex.isEmpty())
                                p.excluded.insert(pid, ex);
                        }
                    p.palettes = front + p.palettes;
                }
                if (fillOut.contains(p.layer))
                {
                    const QList<quint32> outF = fillOut.value(p.layer);
                    for (quint32 pid : rc.layers.at(p.layer).values.value(KeyColour).look.palettes)
                    {
                        if (!p.palettes.contains(pid))
                            continue;
                        QList<quint32> ex = p.excluded.value(pid);
                        for (quint32 f : outF)
                            if (!ex.contains(f))
                                ex << f;
                        std::sort(ex.begin(), ex.end());
                        p.excluded.insert(pid, ex);
                    }
                }
                bool writes = false;
                for (quint32 pid : p.palettes)
                    for (quint32 f : fixtures.at(p.layer))
                        if (!p.excluded.value(pid).contains(f))
                            writes = true;
                if (writes)
                    kept << p;
            }
            std::sort(kept.begin(), kept.end(), [](const ScenePlan &a, const ScenePlan &b) {
                return a.layer < b.layer;
            });
            out = kept;
        }
    }
    return out;
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

bool ShowTreeController::applyReusable(quint32 paletteId, const QString &name, const QString &path)
{
    QLCPalette *p = m_doc->palette(paletteId);
    if (p == nullptr || !m_inlinePalettes.contains(paletteId))
        return false;
    m_inlinePalettes.remove(paletteId);
    m_madeReusable.insert(paletteId);
    p->setName(name);
    p->setPath(path);
    return true;
}

bool ShowTreeController::makeReusable(quint32 paletteId, const QString &name, const QString &path)
{
    if (m_doc->palette(paletteId) == nullptr || !m_inlinePalettes.contains(paletteId))
        return false;
    // One undo step (D22): the snapshot holds it as the one-off it was.
    const QByteArray before = snapshotXml();
    if (!applyReusable(paletteId, name, path))
        return false;
    pushUndo(tr("Make reusable %1").arg(name), before);
    m_doc->setModified();
    emit changed();
    return true;
}

void ShowTreeController::reusableName(quint32 paletteId, QString *name, QString *path)
{
    const QLCPalette *p = m_doc->palette(paletteId);
    if (p == nullptr)
        return;
    // Named as if it were new: its own one-off name is not "taken".
    const bool wasInline = m_inlinePalettes.remove(paletteId);
    *name = suggestPaletteName(p->type(), p->values());
    *path = suggestPaletteFolder(p->type());
    if (wasInline)
        m_inlinePalettes.insert(paletteId);
}

bool ShowTreeController::makeReusable(quint32 paletteId)
{
    return makeReusable(QList<quint32>({ paletteId }));
}

bool ShowTreeController::makeReusable(const QList<quint32> &paletteIds)
{
    QList<quint32> ids;
    for (quint32 pid : paletteIds)
        if (m_doc->palette(pid) != nullptr && m_inlinePalettes.contains(pid) && !ids.contains(pid))
            ids << pid;
    if (ids.isEmpty())
        return false;
    const QByteArray before = snapshotXml();
    QStringList names;
    for (quint32 pid : ids)
    {
        // One at a time: the second of two equal names gets " (2)".
        QString name, path;
        reusableName(pid, &name, &path);
        if (applyReusable(pid, name, path))
            names << name;
    }
    pushUndo(tr("Make reusable %1").arg(names.join(QStringLiteral(", "))), before);
    m_doc->setModified();
    emit changed();
    return true;
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
        // A colour macro by name plays the macro, not its colour.
        if (!p->colourMacro().isEmpty())
            return false;
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
    // None yet: unfiled (folders hold any type; a type-named folder would
    // be a folder of its own now).
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
    m_madeReusable.insert(pid);
    m_doc->setModified();
    emit changed();
    return pid;
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
    const QList<quint32> ids = targetFixtures(target);
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
    for (quint32 id : targetFixtures(target))
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
    const QList<quint32> ids = targetFixtures(target);
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
    for (quint32 id : targetFixtures(target))
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
    for (quint32 id : targetFixtures(target))
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
    for (quint32 set : target.sets)
    {
        const GroupSet *gs = groupSet(set);
        names << (gs == nullptr ? tr("(missing set)") : gs->name);
    }
    for (quint32 g : target.groups)
    {
        const FixtureGroup *grp = m_doc->fixtureGroup(g);
        names << (grp == nullptr ? tr("(missing group)") : grp->name());
    }
    /* Fixtures named one by one read as a wall ("can we check to see if
       there are groups that match fixtures vs printing every fixture?"):
       where they make up whole fixture groups, the groups, largest first;
       a long remainder as its first few and a count. */
    QList<quint32> left = target.fixtures;
    if (left.size() > 1)
    {
        QList<const FixtureGroup *> groups;
        for (const FixtureGroup *grp : m_doc->fixtureGroups())
            if (grp != nullptr && grp->fixtureList().size() > 1)
                groups << grp;
        std::stable_sort(groups.begin(), groups.end(), [](const FixtureGroup *a, const FixtureGroup *b) {
            return a->fixtureList().size() > b->fixtureList().size();
        });
        for (const FixtureGroup *grp : groups)
        {
            const QList<quint32> members = grp->fixtureList();
            bool all = true;
            for (quint32 f : members)
                all = all && left.contains(f);
            if (!all)
                continue;
            names << grp->name();
            for (quint32 f : members)
                left.removeAll(f);
        }
    }
    const int shown = left.size() > 4 ? 3 : left.size();
    for (int i = 0; i < shown; ++i)
    {
        const Fixture *fxi = m_doc->fixture(left.at(i));
        names << (fxi == nullptr ? tr("(missing fixture)") : fxi->name());
    }
    if (shown < left.size())
        names << tr("%n more fixture(s)", "", int(left.size() - shown));
    return names.join(QStringLiteral(" + "));
}

QString ShowTreeController::targetContents(const Target &target) const
{
    if (target.isOpaque())
        return tr("plays %1").arg(targetLabel(target));
    QStringList parts;
    auto group = [this](quint32 g) {
        const FixtureGroup *grp = m_doc->fixtureGroup(g);
        return grp == nullptr ? tr("(missing group)")
                              : QStringLiteral("%1 (%2)").arg(grp->name()).arg(grp->fixtureList().count());
    };
    for (quint32 set : target.sets)
    {
        const GroupSet *gs = groupSet(set);
        if (gs == nullptr)
        {
            parts << tr("(missing set)");
            continue;
        }
        QStringList members;
        for (quint32 g : gs->groups)
            members << group(g);
        if (!gs->fixtures.isEmpty())
            members << tr("%1 fixture(s)").arg(gs->fixtures.count());
        parts << QStringLiteral("%1: %2").arg(gs->name, members.join(QStringLiteral(" · ")));
    }
    for (quint32 g : target.groups)
        parts << group(g);
    for (quint32 f : target.fixtures)
    {
        const Fixture *fxi = m_doc->fixture(f);
        parts << (fxi == nullptr ? tr("(missing fixture)") : fxi->name());
    }
    const int n = int(targetFixtures(target).count());
    return tr("%1 — %2 fixture(s)").arg(parts.join(QStringLiteral(" · "))).arg(n);
}

QString ShowTreeController::cueLabel(const CueList &list, int index) const
{
    if (index < 0 || index >= list.cues.count())
        return QString();
    const Cue &c = list.cues.at(index);
    // A look goes by its name alone: its number is only the tree's.
    if (list.looks)
        return c.name.isEmpty() ? tr("Hit %1").arg(index + 1) : c.name;
    const QString number = cueNumber(list, index);
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
    m_redo.append(undoStep(s));
    return true;
}

bool ShowTreeController::redo()
{
    if (m_redo.isEmpty())
        return false;
    const Snapshot s = m_redo.takeLast();
    m_undo.append(redoStep(s));
    return true;
}

ShowTreeController::Snapshot ShowTreeController::undoStep(const Snapshot &s)
{
    Snapshot back { s.label, QByteArray(), s.stamp, s.outside, s.outsideBefore, {} };
    if (s.parts.empty())
        back.xml = snapshotXml();
    // Undone in the reverse order of doing: what the step changed outside
    // after its tree change first (so the bake sees the show as it was: a
    // timeline no longer playing a list's chaser lets the bake delete it),
    // what it changed before its tree change last (a VC widget can point
    // at the chaser again once the tree has made it again).
    if (s.outside)
        s.outside(true);
    if (s.parts.empty())
    {
        restoreSnapshot(s.xml);
    }
    else
    {
        // A group: its parts last first, each as its own step would be.
        back.parts.resize(s.parts.size());
        for (size_t i = s.parts.size(); i-- > 0;)
            back.parts[i] = undoStep(s.parts[i]);
    }
    if (s.outsideBefore)
        s.outsideBefore(true);
    return back;
}

ShowTreeController::Snapshot ShowTreeController::redoStep(const Snapshot &s)
{
    Snapshot back { s.label, QByteArray(), s.stamp, s.outside, s.outsideBefore, {} };
    if (s.parts.empty())
        back.xml = snapshotXml();
    if (s.outsideBefore)
        s.outsideBefore(false);
    if (s.parts.empty())
    {
        restoreSnapshot(s.xml);
    }
    else
    {
        back.parts.resize(s.parts.size());
        for (size_t i = 0; i < s.parts.size(); ++i)
            back.parts[i] = redoStep(s.parts[i]);
    }
    if (s.outside)
        s.outside(false);
    return back;
}

bool ShowTreeController::groupStepsSince(quint64 stamp, const QString &label)
{
    int n = 0;
    while (n < m_undo.count() && m_undo.at(m_undo.count() - 1 - n).stamp > stamp)
        ++n;
    if (n == 0)
        return false;
    if (n == 1)
    {
        m_undo.last().label = label;
        return true;
    }
    Snapshot group { label, QByteArray(), m_undo.last().stamp, {}, {}, {} };
    group.parts.reserve(size_t(n));
    for (int i = m_undo.count() - n; i < m_undo.count(); ++i)
        group.parts.push_back(m_undo.at(i));
    m_undo.erase(m_undo.end() - n, m_undo.end());
    m_undo.append(group);
    return true;
}

/* The tab's one history (P-D14): the step taken last is undone first; the
   step undone last is redone first (its stamp is the lowest of the redo
   tops, as undo walks the stamps down). */
static bool paletteFirst(quint64 tree, quint64 palette, bool undo)
{
    if (palette == 0)
        return false;
    if (tree == 0)
        return true;
    return undo ? palette > tree : palette < tree;
}

bool ShowTreeController::undoShared()
{
    PaletteOps *ops = m_doc->paletteOps();
    if (ops != nullptr && paletteFirst(undoStamp(), ops->undoStamp(), true))
        return ops->undo();
    return undo();
}

bool ShowTreeController::redoShared()
{
    PaletteOps *ops = m_doc->paletteOps();
    if (ops != nullptr && paletteFirst(redoStamp(), ops->redoStamp(), false))
        return ops->redo();
    return redo();
}

bool ShowTreeController::canUndoShared() const
{
    return canUndo() || (m_doc->paletteOps() != nullptr && m_doc->paletteOps()->canUndo());
}

bool ShowTreeController::canRedoShared() const
{
    return canRedo() || (m_doc->paletteOps() != nullptr && m_doc->paletteOps()->canRedo());
}

QString ShowTreeController::sharedUndoText() const
{
    const PaletteOps *ops = m_doc->paletteOps();
    if (ops != nullptr && paletteFirst(undoStamp(), ops->undoStamp(), true))
        return ops->undoText();
    return undoText();
}

QString ShowTreeController::sharedRedoText() const
{
    const PaletteOps *ops = m_doc->paletteOps();
    if (ops != nullptr && paletteFirst(redoStamp(), ops->redoStamp(), false))
        return ops->redoText();
    return redoText();
}

bool ShowTreeController::attachToLastStep(const std::function<void(bool)> &apply, bool beforeTree)
{
    if (m_undo.isEmpty())
        return false;
    // Several things outside on one step (an import's filing, then its VC
    // re-point): undone last first, redone in the order they were done.
    std::function<void(bool)> &slot = beforeTree ? m_undo.last().outsideBefore : m_undo.last().outside;
    const std::function<void(bool)> before = slot;
    if (!before)
        slot = apply;
    else
        slot = [before, apply](bool undone) {
            if (undone)
            {
                apply(true);
                before(true);
            }
            else
            {
                before(false);
                apply(false);
            }
        };
    return true;
}

void ShowTreeController::clearUndo()
{
    m_undo.clear();
    m_redo.clear();
    m_snapshotRemap.clear();
}

/** The functions the tree's opaque layers play. */
static QSet<quint32> opaqueFunctions(const QList<Production> &productions)
{
    QSet<quint32> out;
    for (const Production &p : productions)
        for (const CueList &l : p.lists)
            for (const Cue &c : l.cues)
                for (const Layer &layer : c.layers)
                    if (layer.target.isOpaque())
                        out.insert(layer.target.function);
    return out;
}

/* Take @a pids off @a looks: a look left with no palette goes (with an
   effect's own looks); one keeping others (colour 2 of 3, Tilt without
   Pan) keeps them in their order. */
static bool stripPalettes(QList<Look> &looks, const QSet<quint32> &pids)
{
    bool changed = false;
    for (int i = looks.count() - 1; i >= 0; --i)
    {
        Look &l = looks[i];
        const int had = int(l.palettes.count());
        const int hadOwn = int(l.own.count());
        l.palettes.erase(std::remove_if(l.palettes.begin(), l.palettes.end(),
                                        [&pids](quint32 p) { return pids.contains(p); }),
                         l.palettes.end());
        l.own.erase(std::remove_if(l.own.begin(), l.own.end(),
                                   [&pids](quint32 p) { return pids.contains(p); }),
                    l.own.end());
        if (l.palettes.count() == had && l.own.count() == hadOwn)
            continue;
        changed = true;
        if (had > 0 && l.palettes.isEmpty())
            looks.removeAt(i);
    }
    return changed;
}

bool ShowTreeController::removePaletteUses(const QList<quint32> &paletteIds,
                                           const std::function<void()> &deleteThem)
{
    const QSet<quint32> pids(paletteIds.begin(), paletteIds.end());
    bool any = false;
    auto strip = [&](QList<Layer> &layers) {
        for (Layer &layer : layers)
            any |= stripPalettes(layer.looks, pids);
    };
    for (Production &p : m_productions)
        for (CueList &l : p.lists)
            for (Cue &c : l.cues)
            {
                any |= stripPalettes(c.looks, pids);
                strip(c.layers);
            }
    for (ReusableLayer &r : m_reusableLayers)
        any |= stripPalettes(r.looks, pids);
    for (ReusableCue &r : m_reusableCues)
    {
        any |= stripPalettes(r.looks, pids);
        strip(r.layers);
    }
    if (deleteThem)
        deleteThem();
    if (!any)
        return false;
    // One bake for the lot, after the palettes have gone.
    bake();
    m_doc->setModified();
    emit changed();
    return true;
}

QByteArray ShowTreeController::outsideUndoState() const
{
    // Two parts: the generated functions as they are, and the tree's own
    // undo snapshot.
    QByteArray functions;
    QBuffer buffer(&functions);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.writeStartDocument();
    w.writeStartElement(QStringLiteral("Generated"));
    QList<quint32> gen = m_generatedScenes.values() + m_generatedCollections.values();
    std::sort(gen.begin(), gen.end());
    for (quint32 id : gen)
        if (Function *f = m_doc->function(id))
            f->saveXML(&w);
    w.writeEndElement();
    w.writeEndDocument();

    QByteArray state;
    QDataStream out(&state, QIODevice::WriteOnly);
    out << functions << snapshotXml();
    return state;
}

void ShowTreeController::restoreOutsideUndoState(const QByteArray &state)
{
    QByteArray functions, tree;
    QDataStream in(state);
    in >> functions >> tree;
    if (tree.isEmpty())
        return;

    // The generated functions the bake let go of since: back with their
    // ids, so the bake of the restored tree finds them again by what they
    // play and every cue's Collection holds the same functions as before.
    QXmlStreamReader r(functions);
    while (!r.atEnd() && !r.isStartElement())
        r.readNext();
    while (r.readNextStartElement())
    {
        const quint32 id = r.attributes().value(QStringLiteral("ID")).toString().toUInt();
        if (r.name() != QLatin1String("Function") || m_doc->function(id) != nullptr)
        {
            r.skipCurrentElement();
            continue;
        }
        if (!Function::loader(r, m_doc))
            continue;
        if (Function *f = m_doc->function(id))
        {
            if (f->type() == Function::SceneType)
                m_generatedScenes.insert(id);
            else
                m_generatedCollections.insert(id);
        }
    }
    restoreSnapshot(tree);
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

void ShowTreeController::remapSnapshotFunction(quint32 from, quint32 to)
{
    for (auto it = m_snapshotRemap.begin(); it != m_snapshotRemap.end(); ++it)
        if (it.value() == from)
            it.value() = to;
    m_snapshotRemap.insert(from, to);
}

void ShowTreeController::restoreSnapshot(const QByteArray &xml)
{
    QXmlStreamReader r(xml);
    while (!r.atEnd() && !r.isStartElement())
        r.readNext();
    m_snapshotDetached.clear();
    if (r.name() == KXMLQLCShowTree)
        loadState(r, true);
    if (!m_snapshotRemap.isEmpty())
        for (Production &p : m_productions)
            for (CueList &l : p.lists)
                for (Cue &c : l.cues)
                    for (Layer &layer : c.layers)
                    {
                        const quint32 fid = layer.target.function;
                        if (m_snapshotRemap.contains(fid) && (m_doc->function(fid) == nullptr || isGenerated(fid)))
                            layer.target.function = m_snapshotRemap.value(fid);
                    }
    applyDetachedLists();
    bake();
    // A scene Detach made after this snapshot (the detach is undone) goes,
    // once the bake has let go of it; the redo snapshot holds it as it was.
    const QSet<quint32> played = opaqueFunctions(m_productions);
    QList<quint32> detached = m_detached.values();
    std::sort(detached.begin(), detached.end());
    for (quint32 id : detached)
    {
        if (m_snapshotDetached.contains(id) || played.contains(id))
            continue;
        Function *f = m_doc->function(id);
        if (f != nullptr && f->isRunning())
            continue;   // never pulled from under the show
        m_detached.remove(id);
        if (f != nullptr && m_doc->getUsage(id).isEmpty())
            m_doc->deleteFunction(id);
    }
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
    ScenePlan plan;
    plan.target.groups = scene->fixtureGroups();
    plan.target.fixtures = scene->fixtures();
    plan.palettes = scene->palettes();
    plan.excluded = scene->excludedFixtures();
    const QHash<quint32, Scene::LayoutSpan> spans = scene->layoutSpans();
    for (auto it = spans.constBegin(); it != spans.constEnd(); ++it)
    {
        plan.spanGroups.insert(it.key(), it.value().groups);
        plan.spanFixtures.insert(it.key(), it.value().fixtures);
    }
    const QHash<quint32, Scene::PaletteFade> fades = scene->paletteFades();
    for (auto it = fades.constBegin(); it != fades.constEnd(); ++it)
        plan.fades.insert(it.key(), qMakePair(it.value().fadeInMs, it.value().fadeOutMs));
    return plan.signature();
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
            || x.duration != y.duration || x.note != y.note || x.cueNumber != y.cueNumber
            || x.subCues != y.subCues || x.skip != y.skip)
            return false;
    }
    return true;
}

static bool sameTarget(const Target &a, const Target &b)
{
    return a.function == b.function && a.groups == b.groups && a.fixtures == b.fixtures && a.sets == b.sets;
}

/* What planCue() reads of a resolved layer: its target, depth and values. */
static bool samePlannedLayers(const QList<ResolvedLayer> &a, const QList<ResolvedLayer> &b)
{
    if (a.count() != b.count())
        return false;
    for (int i = 0; i < a.count(); ++i)
    {
        const ResolvedLayer &x = a.at(i), &y = b.at(i);
        if (x.depth != y.depth || !sameTarget(x.target, y.target) || x.values.count() != y.values.count())
            return false;
        for (auto it = x.values.constBegin(), jt = y.values.constBegin(); it != x.values.constEnd(); ++it, ++jt)
        {
            const ResolvedValue &v = it.value(), &w = jt.value();
            if (it.key() != jt.key() || v.origin != w.origin || v.fromCue != w.fromCue || v.span != w.span
                || v.look != w.look)
                return false;
        }
    }
    return true;
}

void ShowTreeController::forgetPlans()
{
    m_planCache.clear();
    m_targetUnits.clear();
}

const ShowTreeController::TargetUnits &ShowTreeController::targetUnits(const Target &expanded)
{
    if (m_targetUnitsWorld != m_planWorld)
    {
        m_targetUnits.clear();
        m_targetUnitsWorld = m_planWorld;
    }
    QByteArray key;
    key.reserve(int(sizeof(quint32)) * int(expanded.groups.count() + expanded.fixtures.count() + 1));
    auto put = [&key](quint32 v) { key.append(reinterpret_cast<const char *>(&v), sizeof(v)); };
    for (quint32 g : expanded.groups)
        put(g);
    put(0xFFFFFFFF);
    for (quint32 f : expanded.fixtures)
        put(f);
    auto it = m_targetUnits.find(key);
    if (it == m_targetUnits.end())
    {
        TargetUnits tu;
        tu.fixtures = fixturesOfTarget(m_doc, expanded);
        tu.units = unitsOfTarget(m_doc, expanded);
        it = m_targetUnits.insert(key, tu);
    }
    return it.value();
}

QList<ShowTreeController::ScenePlan> ShowTreeController::bakePlan(const CueList &list,
                                                                  const QList<ResolvedCue> &resolved,
                                                                  int index, const QHash<quint32, int> &order)
{
    const ResolvedCue &rc = resolved.at(index);
    const bool mix = index < list.cues.count() && list.cues.at(index).mixOverlap;
    // The order of the cues its values come from (planCue ranks a later
    // cue's tracked value over an earlier one's): as ranks, so a cue added
    // or taken away elsewhere in the list changes nothing here.
    QVector<int> recency;
    bool aim = false;
    for (const ResolvedLayer &rl : rc.layers)
        for (auto it = rl.values.constBegin(); it != rl.values.constEnd(); ++it)
        {
            recency << order.value(it.value().fromCue, -1);
            // A line aim plays a palette made for its settings, found by
            // the palettes' values: always planned afresh.
            if (it.key() == KeyPosition)
                for (quint32 pid : it.value().look.palettes)
                    if (const QLCPalette *p = m_doc->palette(pid))
                        aim = aim || p->type() == QLCPalette::Aim;
        }
    QVector<int> ranks = recency;
    std::sort(ranks.begin(), ranks.end());
    ranks.erase(std::unique(ranks.begin(), ranks.end()), ranks.end());
    for (int &r : recency)
        r = int(std::lower_bound(ranks.begin(), ranks.end(), r) - ranks.begin());

    auto it = m_planCache.find(rc.cueId);
    if (!aim && it != m_planCache.end() && it->world == m_planWorld && it->mix == mix
        && it->recency == recency && samePlannedLayers(it->layers, rc.layers))
    {
        bool there = true;
        for (const ScenePlan &p : it->plans)
            for (quint32 pid : p.palettes)
                there = there && m_doc->palette(pid) != nullptr;
        if (there)
            return it->plans;
    }

    const quint64 world = m_planWorld;
    const QList<ScenePlan> plans = planCue(list, resolved, index, true);
    ++m_lastBakePlanned;
    if (aim)
    {
        m_planCache.remove(rc.cueId);
        return plans;
    }
    PlanCacheEntry e;
    e.layers = rc.layers;
    e.recency = recency;
    e.mix = mix;
    // A palette planning made (a line aim's) changes the world after this
    // plan was made from it: kept as of before, so planned again next time.
    e.world = world;
    e.plans = plans;
    m_planCache.insert(rc.cueId, e);
    return plans;
}

void ShowTreeController::bake()
{
    if (m_baking)
        return;
    m_baking = true;
    m_lastBakePlanned = 0;
    // Group sets live here, not in Doc: a change to them is a new world.
    {
        bool same = m_groupSets.count() == m_planGroupSets.count();
        for (int i = 0; same && i < m_groupSets.count(); ++i)
        {
            const GroupSet &a = m_groupSets.at(i), &b = m_planGroupSets.at(i);
            same = a.id == b.id && a.groups == b.groups && a.fixtures == b.fixtures;
        }
        if (!same)
        {
            ++m_planWorld;
            m_planGroupSets = m_groupSets;
        }
    }
    QSet<quint32> plannedCues;

    QSet<quint32> keepScenes, keepCollections, keepChasers;
    m_generatedLabels.clear();
    m_lastChanged.clear();
    m_layerScenes.clear();
    m_spanMembers.clear();

    // Generated scenes are found again by what they play. An override
    // hit's are its own (they play over the show, so never share one with
    // a cue): found by what they play and the mark.
    const QString overrideMark = QStringLiteral("|override");
    QSet<quint32> overrideScenes;
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            if (l.looks)
                for (const Cue &c : l.cues)
                    if (c.override)
                        if (const Collection *coll = qobject_cast<const Collection *>(m_doc->function(c.collection)))
                            for (quint32 fid : coll->functions())
                                overrideScenes.insert(fid);
    QHash<QString, quint32> bySignature;
    QList<quint32> existing = m_generatedScenes.values();
    std::sort(existing.begin(), existing.end());
    for (quint32 id : existing)
    {
        const Scene *s = qobject_cast<const Scene *>(m_doc->function(id));
        if (s == nullptr)
            continue;
        const QString sig = sceneSignature(s) + (overrideScenes.contains(id) ? overrideMark : QString());
        if (!bySignature.contains(sig))
            bySignature.insert(sig, id);
    }

    for (Production &prod : m_productions)
    {
        // Sub-cue lists first (P6): their chasers are their cues' steps.
        QList<CueList *> order;
        for (CueList &list : prod.lists)
            if (list.isSubCues())
                order << &list;
        for (CueList &list : prod.lists)
            if (!list.isSubCues())
                order << &list;
        for (CueList *lp : order)
        {
            CueList &list = *lp;
            // A sub-cue list is filed with its cue's list, and named after it.
            const CueList *parentList = nullptr;
            int parentIndex = -1;
            if (list.isSubCues())
                for (const CueList &l : prod.lists)
                    if (!l.isSubCues() && l.indexOf(list.subOf) >= 0)
                    {
                        parentList = &l;
                        parentIndex = l.indexOf(list.subOf);
                    }
            if (list.isSubCues() && parentList == nullptr)
                continue;
            const QString folder = QStringLiteral("%1/%2/%3").arg(kProductionsFolder, prod.name,
                                                                  parentList != nullptr ? parentList->name : list.name);
            const QList<ResolvedCue> resolved = this->resolved(list);
            QHash<quint32, int> order;
            order.reserve(list.cues.count());
            for (int i = 0; i < list.cues.count(); ++i)
                order.insert(list.cues.at(i).id, i);

            // A Looks section (R3) has no chaser: each look is a Collection
            // of its own, fired from VC like any function.
            Chaser *chaser = list.looks ? nullptr : qobject_cast<Chaser *>(m_doc->function(list.chaser));
            if (!list.looks && (chaser == nullptr || !m_generatedChasers.contains(list.chaser)))
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
            if (chaser != nullptr && parentList != nullptr)
            {
                // Sub-cues: a hidden chaser, the parent cue's step.
                const QString name = tr("%1 · sub-cues").arg(cueLabel(*parentList, parentIndex));
                if (chaser->name() != name)
                    chaser->setName(name);
                if (chaser->path(true) != folder)
                    chaser->setPath(folder);
                if (chaser->isVisible())
                    chaser->setVisible(false);
                keepChasers.insert(chaser->id());
                m_generatedLabels.insert(chaser->id(), tr("Sub-cues of cue %1 in %2")
                                                       .arg(cueLabel(*parentList, parentIndex), parentList->name));
            }
            else if (chaser != nullptr)
            {
                if (chaser->name() != list.name)
                    chaser->setName(list.name);
                const QString chaserPath = QStringLiteral("%1/%2").arg(kProductionsFolder, prod.name);
                if (chaser->path(true) != chaserPath)
                    chaser->setPath(chaserPath);
                keepChasers.insert(chaser->id());
                m_generatedLabels.insert(chaser->id(), tr("Cue list %1").arg(list.name));
            }
            else
            {
                list.chaser = invalidId();
            }

            QList<ChaserStep> steps;
            for (int i = 0; i < list.cues.count(); ++i)
            {
                Cue &cue = list.cues[i];
                const QString label = cueLabel(list, i);
                const QString number = cueNumber(list, i);
                const QString collName = list.looks ? label : cue.name.isEmpty()
                                         ? number : QStringLiteral("%1-%2").arg(number, cue.name);

                QList<quint32> members;
                // Opaque functions as they are, in layer order.
                QHash<int, quint32> opaqueAt;
                for (int k = 0; k < resolved.at(i).layers.count(); ++k)
                {
                    const ResolvedLayer &rl = resolved.at(i).layers.at(k);
                    if (!rl.target.isOpaque())
                        continue;
                    const quint32 fid = rl.target.function;
                    if (m_doc->function(fid) != nullptr && (chaser == nullptr || fid != chaser->id()))
                        opaqueAt.insert(k, fid);
                }
                const QList<ScenePlan> plans = bakePlan(list, resolved, i, order);
                plannedCues.insert(cue.id);
                QHash<int, quint32> sceneAt;
                for (const ScenePlan &plan : plans)
                {
                    const ResolvedLayer &rl = resolved.at(i).layers.at(plan.layer);
                    const QString sig = plan.signature() + (list.looks && cue.override ? overrideMark : QString());
                    const QString layerName = rl.name.isEmpty() ? targetLabel(rl.target) : rl.name;
                    quint32 sid = bySignature.value(sig, Function::invalidId());
                    Scene *scene = qobject_cast<Scene *>(m_doc->function(sid));
                    if (scene == nullptr)
                    {
                        scene = new Scene(m_doc);
                        for (quint32 g : plan.target.groups)
                            scene->addFixtureGroup(g);
                        for (quint32 f : plan.target.fixtures)
                            scene->addFixture(f);
                        for (quint32 p : plan.palettes)
                            scene->addPalette(p);
                        // What it leaves out for a lower layer, and what it
                        // lays out over with others: part of what it plays,
                        // so set before it runs and never changed after.
                        for (auto it = plan.excluded.constBegin(); it != plan.excluded.constEnd(); ++it)
                            scene->setExcludedFixtures(it.key(), it.value());
                        for (quint32 p : plan.palettes)
                            if (plan.spanGroups.contains(p) || plan.spanFixtures.contains(p))
                            {
                                Scene::LayoutSpan span;
                                span.groups = plan.spanGroups.value(p);
                                span.fixtures = plan.spanFixtures.value(p);
                                scene->setLayoutSpan(p, span);
                            }
                        for (auto it = plan.fades.constBegin(); it != plan.fades.constEnd(); ++it)
                            scene->setPaletteFade(it.key(), it.value().first, it.value().second);
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
                        m_generatedLabels.insert(sid, list.looks
                                                      ? tr("Layer %1 in hit %2 of %3")
                                                        .arg(layerName, label, prod.name)
                                                      : tr("Layer %1 in cue %2 of %3")
                                                        .arg(layerName, label, list.name));
                    }
                    sceneAt.insert(plan.layer, sid);
                    const QString lk = layerKey(cue.id, rl.target);
                    m_layerScenes.insert(lk, sid);
                    for (auto it = plan.spanMembers.constBegin(); it != plan.spanMembers.constEnd(); ++it)
                    {
                        QList<Target> ts;
                        for (int m : it.value())
                            ts << resolved.at(i).layers.at(m).target;
                        m_spanMembers[lk].insert(it.key(), ts);
                    }
                }
                // In layer order, once each.
                for (int k = 0; k < resolved.at(i).layers.count(); ++k)
                {
                    const quint32 fid = opaqueAt.contains(k) ? opaqueAt.value(k)
                                                             : sceneAt.value(k, Function::invalidId());
                    if (fid != Function::invalidId() && !members.contains(fid))
                        members << fid;
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
                // A look's Collection is what a VC button fires: shown (the
                // function pickers list it by the look's name), filed in its
                // folder, still the tree's (locked, D13).
                const QString collPath = list.looks && !cue.folder.isEmpty()
                                         ? folder + QLatin1Char('/') + cue.folder : folder;
                if (coll->path(true) != collPath)
                    coll->setPath(collPath);
                if (coll->isVisible() != list.looks)
                    coll->setVisible(list.looks);
                // A hit on a VC Flash button plays while the button is held,
                // and comes and goes over its own fades (Q180).
                coll->setFlashPlays(list.looks);
                if (list.looks)
                    coll->setHitFades(cue.fadeIn, cue.fadeOut);
                else
                    coll->setHitFades(0, 0);
                coll->setHitOverride(list.looks && cue.override);
                if (coll->functions() != members)
                {
                    m_lastChanged << cue.id;
                    for (quint32 fid : coll->functions())
                        coll->removeFunction(fid);
                    for (quint32 fid : members)
                        coll->addFunction(fid);
                }
                keepCollections.insert(coll->id());
                m_generatedLabels.insert(coll->id(), list.looks ? tr("Hit %1 in %2").arg(label, prod.name)
                                                                : tr("Cue %1 in %2").arg(label, list.name));
                if (list.looks)
                    continue;

                if (list.isSubCues())
                {
                    // A sub-cue holds until Sub GO, unless the next one
                    // auto-follows: its wait is this one's hold.
                    const bool nextFollows = i + 1 < list.cues.count() && list.cues.at(i + 1).follow;
                    const uint hold = nextFollows ? list.cues.at(i + 1).wait : Function::infiniteSpeed();
                    ChaserStep step(coll->id(), cue.fadeIn, hold, cue.fadeOut);
                    step.duration = nextFollows ? cue.fadeIn + hold : hold;
                    step.note = cue.note;
                    step.cueNumber = number;
                    step.skip = cue.skip;
                    steps << step;
                    continue;
                }

                ChaserStep step(coll->id(), cue.fadeIn, cue.hold, cue.fadeOut);
                // As ChaserStep::loadXML: an infinite hold is the duration
                // (adding the fade would wrap it round to a few seconds).
                step.duration = int(cue.hold) < 0 ? cue.hold : cue.fadeIn + cue.hold;
                step.note = cue.note;
                step.cueNumber = cue.number;
                // Skipped: GO, auto-follow and Back pass over it (the
                // runner), a jump still plays it.
                step.skip = cue.skip;
                // With sub-cues (P6): the step plays their chaser, held on
                // Sub GO; the first sub-cue plays with GO.
                for (const CueList &s : prod.lists)
                    if (s.subOf == cue.id && m_doc->function(s.chaser) != nullptr)
                    {
                        step.fid = s.chaser;
                        bool follows = false;
                        for (const Cue &sc : s.cues)
                            follows = follows || sc.follow;
                        step.subCues = follows ? ChaserStep::SubGoFollow : ChaserStep::SubGo;
                    }
                steps << step;
            }

            if (chaser == nullptr)
                continue;
            // Tonight's running order: the songs in that order, a song left
            // out skipped; the cues themselves stay as they are.
            if (!list.looks && !list.isSubCues() && !prod.runningOrder.isEmpty())
            {
                QList<bool> out;
                const QList<int> order = playOrder(list, prod.numbering, prod.runningOrder, &out);
                if (order.count() == steps.count())
                {
                    QList<ChaserStep> played;
                    for (int at : order)
                    {
                        ChaserStep s = steps.at(at);
                        s.skip = s.skip || out.at(at);
                        played << s;
                    }
                    steps = played;
                }
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
    // Plans of cues that are gone: forgotten (an undo bringing one back
    // plans it again).
    for (auto it = m_planCache.begin(); it != m_planCache.end();)
        it = plannedCues.contains(it.key()) ? std::next(it) : m_planCache.erase(it);

    m_baking = false;

    // The current production's main list may have a (new) chaser now.
    syncCurrentProduction();

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
        // Played by a function of the show's own (a detached list's cue):
        // handed over to it, never deleted from under it.
        if (usedOutsideTree(id))
        {
            handOver(id);
            continue;
        }
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
    // A reusable no cue uses (yet) keeps its one-off values.
    for (const ReusableLayer &r : m_reusableLayers)
        addLooks(r.looks);
    for (const ReusableCue &r : m_reusableCues)
    {
        addLooks(r.looks);
        for (const Layer &layer : r.layers)
            addLooks(layer.looks);
    }

    // A scene step in Build's history can put one back in its scene
    // (Make a layer's one-off, undone): it stays while that step does.
    if (const PaletteOps *ops = m_doc->paletteOps())
        used.unite(ops->palettesInHistory());

    QList<quint32> ids = m_inlinePalettes.values();
    std::sort(ids.begin(), ids.end());
    for (quint32 id : ids)
    {
        if (used.contains(id))
            continue;
        // A generated scene still waiting to be let go of plays it, or a
        // scene of the show's own does (one made reusable, used, and then
        // made a one-off again by an undo): it stays.
        bool running = false;
        for (Function *f : m_doc->functions())
        {
            const Scene *s = qobject_cast<const Scene *>(f);
            if (s != nullptr && s->palettes().contains(id))
                running = true;
        }
        if (running)
            continue;
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
    const QList<ResolvedCue> resolved = this->resolved(*list);
    const Cue *c = list->cue(cueId);
    const Layer *layer = c == nullptr ? nullptr : c->layer(layerId);
    if (layer == nullptr || layer->target.isOpaque())
        return Function::invalidId();
    const ResolvedLayer *rl = resolved.at(index).layer(layer->target);
    if (rl == nullptr)
        return Function::invalidId();

    // A plain copy of what it plays now (its groups, its looks -- and what
    // it leaves out for a layer under it, so it plays the same).
    Scene *scene = new Scene(m_doc);
    bool planned = false;
    for (const ScenePlan &plan : planCue(*list, resolved, index))
    {
        if (!(resolved.at(index).layers.at(plan.layer).target == layer->target))
            continue;
        for (quint32 g : plan.target.groups)
            scene->addFixtureGroup(g);
        for (quint32 f : plan.target.fixtures)
            scene->addFixture(f);
        for (quint32 p : plan.palettes)
            scene->addPalette(p);
        for (auto it = plan.excluded.constBegin(); it != plan.excluded.constEnd(); ++it)
            scene->setExcludedFixtures(it.key(), it.value());
        for (auto it = plan.fades.constBegin(); it != plan.fades.constEnd(); ++it)
            scene->setPaletteFade(it.key(), it.value().first, it.value().second);
        planned = true;
        break;
    }
    if (!planned)
    {
        const Target t = expandTarget(rl->target);
        for (quint32 g : t.groups)
            scene->addFixtureGroup(g);
        for (quint32 f : t.fixtures)
            scene->addFixture(f);
        for (quint32 p : scenePalettes(*rl))
            if (m_doc->palette(p) != nullptr)
                scene->addPalette(p);
    }
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
        l->reusable = invalidId();      // this use only: the reusable stays
        return true;
    });
    if (!ok)
    {
        m_doc->deleteFunction(sid);
        return Function::invalidId();
    }
    // Known as detached after the step's snapshot: undo takes it away.
    m_detached.insert(sid);
    return sid;
}

QString ShowTreeController::lockedText()
{
    return tr("Made by the Show Tree — edit it there.");
}

bool ShowTreeController::effectOnlyTracked(quint32 sceneId, quint32 paletteId, quint32 viaFunction) const
{
    QList<quint32> cues;
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            for (const Cue &c : l.cues)
            {
                if (c.collection == invalidId())
                    continue;
                if (viaFunction != invalidId() ? c.collection == viaFunction : true)
                {
                    const Collection *coll = qobject_cast<const Collection *>(m_doc->function(c.collection));
                    if (coll != nullptr && (viaFunction != invalidId()
                                            || (coll->isRunning() && coll->functions().contains(sceneId))))
                        cues << c.id;
                }
            }
    bool tracked = false;
    for (quint32 cueId : cues)
    {
        ResolvedCue rc;
        if (!resolvedCue(cueId, &rc))
            continue;
        for (const ResolvedLayer &layer : rc.layers)
        {
            if (layerScene(cueId, layer.target) != sceneId)
                continue;
            for (const ResolvedValue &v : layer.values)
            {
                if (!v.look.palettes.contains(paletteId))
                    continue;
                if (v.origin != Origin::Tracked)
                    return false;       // set in this cue: a new start
                tracked = true;
            }
        }
    }
    return tracked;
}

bool ShowTreeController::generatedNode(quint32 functionId, quint32 *listId, quint32 *cueId,
                                       quint32 *layerId) const
{
    if (cueId != nullptr)
        *cueId = invalidId();
    if (layerId != nullptr)
        *layerId = invalidId();
    if (!isGenerated(functionId))
        return false;
    const Scene *scene = qobject_cast<const Scene *>(m_doc->function(functionId));
    const QString sig = scene == nullptr ? QString() : sceneSignature(scene);
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
        {
            if (l.chaser == functionId)
            {
                *listId = l.id;
                // A cue's sub-cues: shown as that cue.
                if (l.isSubCues())
                    if (const CueList *up = listOfCue(l.subOf))
                    {
                        *listId = up->id;
                        if (cueId != nullptr)
                            *cueId = l.subOf;
                    }
                return true;
            }
            for (const Cue &c : l.cues)
                if (c.collection == functionId)
                {
                    *listId = l.id;
                    if (cueId != nullptr)
                        *cueId = c.id;
                    return true;
                }
            if (scene == nullptr)
                continue;
            // A layer's scene: where it is a layer of the cue's own first.
            const QList<ResolvedCue> rcs = resolved(l);
            quint32 firstCue = invalidId();
            for (int i = 0; i < rcs.count() && i < l.cues.count(); ++i)
                for (const ScenePlan &plan : planCue(l, rcs, i))
                {
                    if (plan.signature() != sig)
                        continue;
                    const ResolvedLayer &rl = rcs.at(i).layers.at(plan.layer);
                    if (firstCue == invalidId())
                        firstCue = l.cues.at(i).id;
                    if (rl.own && l.cues.at(i).layer(rl.layerId) != nullptr)
                    {
                        *listId = l.id;
                        if (cueId != nullptr)
                            *cueId = l.cues.at(i).id;
                        if (layerId != nullptr)
                            *layerId = rl.layerId;
                        return true;
                    }
                }
            if (firstCue != invalidId())
            {
                *listId = l.id;
                if (cueId != nullptr)
                    *cueId = firstCue;
                return true;
            }
        }
    return false;
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
            // It snapped on and off in the scene: so it does in the tree.
            l.fadeIn = l.fadeOut = Look::FadeSnap;
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

bool ShowTreeController::buildImport(quint32 chaserId, CueList *listOut, ImportReport *reportOut,
                                     const std::function<quint32()> &newId) const
{
    ImportReport &report = *reportOut;
    const Chaser *chaser = qobject_cast<const Chaser *>(m_doc->function(chaserId));
    if (chaser == nullptr || chaser->type() != Function::ChaserType)
    {
        report.notes << tr("Not a chaser.");
        return false;
    }

    const QList<ChaserStep> steps = chaser->steps();
    QHash<quint32, quint32> cueOfFunction;      // a step's function -> its cue
    CueList &list = *listOut;
    list.id = newId();
    list.name = chaser->name();
    list.mode = ListMode::NonTracking;
    list.source = chaserId;

    for (int i = 0; i < steps.count(); ++i)
    {
        const quint32 fid = steps.at(i).fid;
        if (!cueOfFunction.contains(fid))
            cueOfFunction.insert(fid, newId());
    }

    QSet<quint32> usedCueIds;
    QHash<quint32, quint32> origin;             // layer -> the function it was read from
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
        // Skipped in the chaser: skipped in the list too (put back with
        // Include cue).
        cue.skip = step.skip;
        if (step.speedMaster != SpeedMasters::Inherit)
            report.notes << tr("%1: its own speed master is not carried over.").arg(fname);
        if (step.subCues != ChaserStep::Auto)
            report.notes << tr("%1: sub-cues (Sub GO) are not carried over yet.").arg(fname);

        auto cueTitle = [&cue]() {
            return cue.number.isEmpty() ? cue.name : QStringLiteral("%1 %2").arg(cue.number, cue.name);
        };
        auto addOpaque = [&](const Function *fn, const QString &why) {
            Layer l;
            l.id = newId();
            l.target.function = fn->id();
            origin.insert(l.id, fn->id());
            cue.layers << l;
            report.opaqueLayers++;
            report.opaqueReasons << QStringLiteral("%1: %2").arg(fn->name(), why);
        };
        std::function<void(const Function *, int)> addFunction = [&](const Function *fn, int depth) {
            if (fn == nullptr)
                return;
            if (fn->type() == Function::SceneType)
            {
                Layer l;
                QString why;
                if (sceneToLayer(qobject_cast<const Scene *>(fn), &l, &why))
                {
                    l.id = newId();
                    origin.insert(l.id, fn->id());
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
                if (!cue.includes.contains(cueOfFunction.value(fn->id())))
                {
                    cue.includes << cueOfFunction.value(fn->id());
                    report.includes++;
                }
                return;
            }
            // W5: a collection inside the step that is no step itself (DH's
            // "0.0.0-base") played its members together with the step's
            // own, as one: they are read into the cue as the step's own are.
            // One that plays nothing is left out. Not one with a blend mode
            // or a speed master of its own: that plays as it is.
            if (fn->type() == Function::CollectionType && fn->id() != step.fid && depth < 8
                && fn->blendMode() == Universe::NormalBlend && fn->speedMaster() == SpeedMasters::Inherit)
            {
                const Collection *inner = qobject_cast<const Collection *>(fn);
                const QList<quint32> members = inner->functions();
                bool plays = false;
                for (quint32 m : members)
                    plays |= m_doc->function(m) != nullptr;
                if (!plays)
                {
                    report.dropped++;
                    report.flattenNotes << tr("Cue %1: %2 plays nothing, so it is left out.")
                                           .arg(cueTitle(), fn->name());
                    return;
                }
                report.flattened++;
                report.flattenNotes << tr("Cue %1: %2 is read into the cue's layers (it is no step of its own).")
                                       .arg(cueTitle(), fn->name());
                for (quint32 m : members)
                    addFunction(m_doc->function(m), depth + 1);
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
                addFunction(m_doc->function(member), 1);
        }
        else
        {
            addFunction(f, 1);
        }
        list.cues << cue;
    }
    report.cues = int(list.cues.count());

    // Two scenes on the same target in one cue: the chaser ran both, the
    // tree keeps one value per attribute (the later one) -- unless the cue
    // ends up mixing (below), when it plays as the chaser did.
    QList<QPair<int, QString>> htp;     // cue index (-1 - it: own scenes only), text
    for (int i = 0; i < list.cues.count(); ++i)
    {
        const Cue &cue = list.cues.at(i);
        QHash<QString, QStringList> sources;   // target -> where it is set
        QHash<QString, Target> targets;
        for (quint32 inc : cue.includes)
        {
            CueList single = list;
            const QList<ResolvedCue> r = resolved(single);
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
                htp << qMakePair(sources.value(sig).count(tr("its own scene")) > 1 ? -1 - i : i, tr("Cue %1: %2 is set by %3; the chaser ran them all "
                                          "(the brighter intensity won), the cue list keeps "
                                          "the last one's value for each attribute.")
                                       .arg(cueLabel(list, i), targetLabel(targets.value(sig)),
                                            sources.value(sig).join(tr(" and "))));
    }

    // Two scenes on fixtures in common (different targets) setting the same
    // attribute: the chaser ran both, highest-wins on intensity and RGB.
    // The tree would let the lower one win (§2); the imported cue mixes
    // instead, so it plays as it did (the cue page's Overlap says so).
    {
        const QList<ResolvedCue> r = resolved(list);
        for (int i = 0; i < list.cues.count() && i < r.count(); ++i)
        {
            const QList<ResolvedLayer> &ls = r.at(i).layers;
            QStringList clash;
            for (int a = 0; a < ls.count() && clash.isEmpty(); ++a)
                for (int b = a + 1; b < ls.count() && clash.isEmpty(); ++b)
                {
                    const ResolvedLayer &la = ls.at(a), &lb = ls.at(b);
                    if (la.target.isOpaque() || lb.target.isOpaque() || la.target == lb.target)
                        continue;
                    const QList<quint32> fa = targetFixtures(la.target), fb = targetFixtures(lb.target);
                    bool common = false, dimmerless = false;
                    for (quint32 f : fa)
                        if (fb.contains(f))
                        {
                            common = true;
                            dimmerless |= !QLCPalette::fixtureHasDimmer(m_doc->fixture(f));
                        }
                    if (!common)
                        continue;
                    QStringList keys;
                    for (const Look &l : la.playedLooks())
                        if (lb.values.contains(l.key) && !lb.values.value(l.key).look.none)
                            keys << keyLabel(l.key);
                    // No dimmer channel: a scene's Dimmer is its colour
                    // scaled, so one's Colour and the other's Dimmer meet.
                    auto sets = [](const ResolvedLayer &x, const QString &k) {
                        return x.values.contains(k) && !x.values.value(k).look.none;
                    };
                    if (keys.isEmpty() && dimmerless
                        && ((sets(la, KeyColour) && sets(lb, KeyDimmer)) || (sets(la, KeyDimmer) && sets(lb, KeyColour))))
                        keys << tr("%1 on fixtures with no dimmer").arg(keyLabel(KeyColour) + QLatin1Char('/')
                                                                         + keyLabel(KeyDimmer));
                    if (!keys.isEmpty())
                        clash << tr("Cue %1: %2 and %3 share fixtures and both set %4; they mix "
                                    "as the chaser did (Overlap: Mix on the cue page).")
                                 .arg(cueLabel(list, i), targetLabel(la.target), targetLabel(lb.target),
                                      keys.join(QStringLiteral(", ")));
                }
            if (!clash.isEmpty())
            {
                list.cues[i].mixOverlap = true;
                report.notes << clash;
            }
        }
    }

    // An include at full beside the cue's own scenes: the chaser ran the
    // included collection and the cue's scenes together, highest-wins on
    // intensity, so an included level above the cue's own showed; the tree's
    // rule plays the cue's own (own > included). Only where that changes the
    // output -- the cue's static values or where an effect plays, as Convert
    // compares them (staticCueOutput) -- the cue mixes instead (Overlap:
    // Mix), so it plays as it did. Again until nothing changes: a cue that
    // includes one that now mixes may change in turn.
    {
        const LineAimTest aimTest = lineAimTest();
        bool changed = true;
        for (int round = 0; changed && round <= list.cues.count(); ++round)
        {
            changed = false;
            for (int i = 0; i < list.cues.count(); ++i)
            {
                if (list.cues.at(i).mixOverlap || list.cues.at(i).includes.isEmpty())
                    continue;
                CueList cand = list;
                cand.cues[i].mixOverlap = true;
                QStringList e0, e1;
                const QHash<quint64, uchar> o0 = staticCueOutput(list, resolve(list, aimTest), i, &e0);
                const QHash<quint64, uchar> o1 = staticCueOutput(cand, resolve(cand, aimTest), i, &e1);
                if (o0 == o1 && e0 == e1)
                    continue;
                int n = 0;
                QSet<quint32> fixtures;
                QSet<quint64> keys;
                for (auto it = o0.constBegin(); it != o0.constEnd(); ++it)
                    keys.insert(it.key());
                for (auto it = o1.constBegin(); it != o1.constEnd(); ++it)
                    keys.insert(it.key());
                for (quint64 k : keys)
                    if (o0.value(k) != o1.value(k))
                    {
                        ++n;
                        fixtures.insert(quint32(k >> 32));
                    }
                list.cues[i].mixOverlap = true;
                changed = true;
                QStringList inc;
                for (quint32 id : list.cues.at(i).includes)
                    inc << cueLabel(list, list.indexOf(id));
                const QString line = n > 0
                        ? tr("Cue %1: the cue it includes (%2) plays beside its own scenes, as in the chaser; "
                             "the one rule would change %n channel(s)", "", n)
                          .arg(cueLabel(list, i), inc.join(QStringLiteral(", ")))
                          + tr(" on %n fixture(s), so it mixes (Overlap: Mix on the cue page).", "",
                               int(fixtures.count()))
                        : tr("Cue %1: the cue it includes (%2) plays beside its own scenes, as in the chaser; "
                             "the one rule would change where an effect plays, so it mixes (Overlap: Mix "
                             "on the cue page).").arg(cueLabel(list, i), inc.join(QStringLiteral(", ")));
                report.mixedIncludes << line;
                report.notes << line;
            }
        }
    }
    // A cue that mixes plays an include beside its own scene; two of its
    // own scenes on one target still merge (-1 - index: those).
    for (const auto &h : htp)
        if (h.first < 0 || !list.cues.at(h.first).mixOverlap)
            report.htpConflicts << h.second;

    // What repeats from step to step is set once and tracks.
    trackRepeats(list, origin, report, newId);
    return true;
}

ShowTreeController::ImportReport ShowTreeController::importChaser(quint32 chaserId)
{
    ImportReport report;
    CueList list;
    if (!buildImport(chaserId, &list, &report, [this]() { return newId(); }))
        return report;
    const Chaser *chaser = qobject_cast<const Chaser *>(m_doc->function(chaserId));
    Production prod;
    prod.id = newId();
    prod.name = chaser->name();

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
    // One chaser answers to a cue-list number: it stays with the original
    // until what fires that is moved to the list (the import's re-point).
    generated->setCueListNumber(QString());
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
    // Plays the same? Statically, cue by cue against the chaser's steps.
    report.differChannels = importDifferences(chaserId, list.id, &report.differCues, &report.checkedCues);
    return report;
}

/****************************************************************************
 * Load & save
 ****************************************************************************/

void ShowTreeController::saveState(QXmlStreamWriter *doc, bool snapshot) const
{
    doc->writeStartElement(KXMLQLCShowTree);
    doc->writeAttribute(KXMLShowTreeNextId, QString::number(m_nextId));
    // What GO plays is the operator's, not an edit: never on undo.
    if (!snapshot && m_current != invalidId())
        doc->writeAttribute(QStringLiteral("Current"), QString::number(m_current));

    for (const GroupSet &gs : m_groupSets)
        ShowTree::saveXML(doc, gs);
    for (const ReusableLayer &r : m_reusableLayers)
        ShowTree::saveXML(doc, r);
    for (const ReusableCue &r : m_reusableCues)
        ShowTree::saveXML(doc, r);
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

        QStringList pages = m_lookVcFrames.keys();
        std::sort(pages.begin(), pages.end());
        for (const QString &k : pages)
        {
            doc->writeStartElement(QStringLiteral("LookVCPage"));
            doc->writeAttribute(QStringLiteral("List"), k.section(QLatin1Char('\n'), 0, 0));
            doc->writeAttribute(QStringLiteral("Folder"), k.section(QLatin1Char('\n'), 1));
            doc->writeAttribute(QStringLiteral("Frame"), QString::number(m_lookVcFrames.value(k)));
            doc->writeAttribute(QStringLiteral("Buttons"), idsToString(m_lookVcButtons.value(k)));
            doc->writeEndElement();
        }
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

        // ...a one-off made reusable, named as it is (redo of Make
        // reusable names it again)...
        QList<quint32> reusable = m_madeReusable.values();
        std::sort(reusable.begin(), reusable.end());
        doc->writeStartElement(KXMLShowTreeReusable);
        for (quint32 id : reusable)
            if (QLCPalette *p = m_doc->palette(id))
                if (!m_inlinePalettes.contains(id))
                    p->saveXML(doc);
        doc->writeEndElement();

        // ...and the scenes Detach made, as they are now: an undo past a
        // detach takes its scene away, a redo brings it back.
        QList<quint32> detached = m_detached.values();
        std::sort(detached.begin(), detached.end());
        doc->writeStartElement(KXMLShowTreeDetached);
        for (quint32 id : detached)
            if (Function *f = m_doc->function(id))
                f->saveXML(doc);
        doc->writeEndElement();

        // ...and the functions a list Detach handed over (an undo past it
        // takes them back, a redo hands them over again).
        QList<quint32> lists = m_detachedLists.values();
        std::sort(lists.begin(), lists.end());
        doc->writeStartElement(QStringLiteral("DetachedLists"));
        doc->writeAttribute(QStringLiteral("IDs"), idsToString(lists));
        doc->writeEndElement();
    }

    doc->writeEndElement();
}

void ShowTreeController::loadState(QXmlStreamReader &root, bool snapshot)
{
    m_productions.clear();
    m_groupSets.clear();
    m_reusableLayers.clear();
    m_reusableCues.clear();
    m_snapshotHasDetachedLists = false;
    m_snapshotDetachedLists.clear();
    const quint32 next = root.attributes().value(KXMLShowTreeNextId).toString().toUInt();
    if (!snapshot)
    {
        bool ok = false;
        const quint32 cur = root.attributes().value(QStringLiteral("Current")).toString().toUInt(&ok);
        m_current = ok ? cur : invalidId();
    }
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
        else if (root.name() == QLatin1String("GroupSet"))
        {
            GroupSet gs;
            if (ShowTree::loadXML(root, gs))
                m_groupSets << gs;
        }
        else if (root.name() == QLatin1String("ReusableLayer"))
        {
            ReusableLayer r;
            if (ShowTree::loadXML(root, r))
                m_reusableLayers << r;
        }
        else if (root.name() == QLatin1String("ReusableCue"))
        {
            ReusableCue r;
            if (ShowTree::loadXML(root, r))
                m_reusableCues << r;
        }
        else if (root.name() == QLatin1String("DetachedLists") && snapshot)
        {
            m_snapshotHasDetachedLists = true;
            for (quint32 id : stringToIds(root.attributes().value(QStringLiteral("IDs")).toString()))
                m_snapshotDetachedLists.insert(id);
            root.skipCurrentElement();
        }
        else if (root.name() == QLatin1String("LookVCPage"))
        {
            if (!snapshot)
            {
                const QXmlStreamAttributes a = root.attributes();
                const QString k = a.value(QStringLiteral("List")).toString() + QLatin1Char('\n')
                                  + a.value(QStringLiteral("Folder")).toString();
                m_lookVcFrames.insert(k, a.value(QStringLiteral("Frame")).toString().toUInt());
                m_lookVcButtons.insert(k, stringToIds(a.value(QStringLiteral("Buttons")).toString()));
            }
            root.skipCurrentElement();
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
                        have->setPath(saved->path());
                        have->setValues(saved->values());
                    }
                    delete saved;
                }
            }
        }
        else if (root.name() == KXMLShowTreeReusable && snapshot)
        {
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
                const quint32 id = saved->id();
                m_inlinePalettes.remove(id);
                m_madeReusable.insert(id);
                QLCPalette *have = m_doc->palette(id);
                if (have == nullptr)
                {
                    if (!m_doc->addPalette(saved, id))
                        delete saved;
                }
                else
                {
                    if (have->type() == saved->type())
                    {
                        have->setName(saved->name());
                        have->setPath(saved->path());
                        have->setValues(saved->values());
                    }
                    delete saved;
                }
            }
        }
        else if (root.name() == KXMLShowTreeDetached && snapshot)
        {
            while (root.readNextStartElement())
            {
                const quint32 id = root.attributes().value(QStringLiteral("ID")).toString().toUInt();
                if (root.name() == QLatin1String("Function"))
                    m_snapshotDetached.insert(id);
                if (root.name() != QLatin1String("Function") || m_doc->function(id) != nullptr)
                {
                    if (root.name() == QLatin1String("Function") && m_doc->function(id) != nullptr)
                        m_detached.insert(id);
                    root.skipCurrentElement();
                    continue;
                }
                if (Function::loader(root, m_doc) && m_doc->function(id) != nullptr)
                    m_detached.insert(id);
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
    for (const GroupSet &gs : m_groupSets)
        m_nextId = qMax(m_nextId, gs.id + 1);
    for (const ReusableLayer &r : m_reusableLayers)
        m_nextId = qMax(m_nextId, r.id + 1);
    for (const ReusableCue &r : m_reusableCues)
    {
        m_nextId = qMax(m_nextId, r.id + 1);
        for (const Layer &layer : r.layers)
            m_nextId = qMax(m_nextId, layer.id + 1);
    }
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
                // A hit's Collection, as the file left it: flashes as a hit,
                // with its fades.
                if (l.looks)
                    if (Collection *coll = qobject_cast<Collection *>(m_doc->function(c.collection)))
                    {
                        coll->setFlashPlays(true);
                        coll->setHitFades(c.fadeIn, c.fadeOut);
                        coll->setHitOverride(c.override);
                    }
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
    if (production(m_current) == nullptr)
        m_current = invalidId();
    applyProductionTimecode();
    emit changed();
    return true;
}

void ShowTreeController::saveXML(QXmlStreamWriter *doc) const
{
    if (m_productions.isEmpty() && m_generatedScenes.isEmpty() && m_generatedCollections.isEmpty()
        && m_generatedChasers.isEmpty() && m_inlinePalettes.isEmpty() && m_groupSets.isEmpty()
        && m_reusableLayers.isEmpty() && m_reusableCues.isEmpty())
        return;     // a show without a tree saves as it always did
    saveState(doc, false);
}

quint32 ShowTreeController::lookVcFrame(quint32 listId, const QString &folder) const
{
    return m_lookVcFrames.value(QString::number(listId) + QLatin1Char('\n') + folder, invalidId());
}

QList<quint32> ShowTreeController::lookVcButtons(quint32 listId, const QString &folder) const
{
    return m_lookVcButtons.value(QString::number(listId) + QLatin1Char('\n') + folder);
}

void ShowTreeController::setLookVcFrame(quint32 listId, const QString &folder, quint32 frameId,
                                        const QList<quint32> &buttons)
{
    const QString k = QString::number(listId) + QLatin1Char('\n') + folder;
    if (frameId == invalidId())
    {
        m_lookVcFrames.remove(k);
        m_lookVcButtons.remove(k);
    }
    else
    {
        m_lookVcFrames.insert(k, frameId);
        m_lookVcButtons.insert(k, buttons);
    }
    m_doc->setModified();
}

void ShowTreeController::reset()
{
    // The show is going: let go of what the preview plays (the functions
    // are still there; Doc deletes them after this).
    stopPreview();
    m_sweepTimer->stop();     // nothing of the old show is left to sweep
    m_productions.clear();
    m_groupSets.clear();
    m_reusableLayers.clear();
    m_reusableCues.clear();
    m_detachedLists.clear();
    m_generatedScenes.clear();
    m_generatedCollections.clear();
    m_generatedChasers.clear();
    m_lookVcFrames.clear();
    m_lookVcButtons.clear();
    m_inlinePalettes.clear();
    m_layerScenes.clear();
    m_spanMembers.clear();
    m_madeReusable.clear();
    m_detached.clear();
    m_generatedLabels.clear();
    m_planCache.clear();
    m_planGroupSets.clear();
    m_targetUnits.clear();
    m_undo.clear();
    m_redo.clear();
    m_snapshotRemap.clear();
    m_nextId = 1;
    m_current = invalidId();
    applyProductionTimecode();
    emit changed();
}

void ShowTreeController::slotFunctionRemoved(quint32 fid)
{
    m_previewRunning.removeAll(fid);
    m_generatedScenes.remove(fid);
    m_generatedCollections.remove(fid);
    m_generatedChasers.remove(fid);
    m_generatedLabels.remove(fid);
    m_detached.remove(fid);
    m_detachedLists.remove(fid);
}

void ShowTreeController::slotPaletteRemoved(quint32 pid)
{
    m_inlinePalettes.remove(pid);
    m_madeReusable.remove(pid);
}

/****************************************************************************
 * Preview
 ****************************************************************************/

FunctionParent ShowTreeController::previewParent()
{
    // Its own parent: a chaser's collection, a VC button or the Programming
    // tab holding the same scene keep it when the preview lets go, and the
    // preview never stops what they started (a Master parent would).
    return FunctionParent::preview(0x3);
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
    setEditedCue(cueId);
    // Held by a run: selecting stays off stage while a list runs; with
    // none running any more, a cue selected plays again.
    if (m_previewHeld && listsRunning().isEmpty())
        m_previewHeld = false;
    syncPreview();
}

void ShowTreeController::pickCue(quint32 cueId)
{
    // The latest action wins: a pick after GO takes the stage back (over
    // the run, which keeps going underneath) until the next GO.
    if (cue(cueId) != nullptr)
        m_previewHeld = false;
    previewCue(cueId);
}

void ShowTreeController::setEditedCue(quint32 cueId)
{
    if (m_editedCue == cueId)
        return;
    m_editedCue = cueId;
    emit editedCueChanged();
}

quint32 ShowTreeController::editedCue() const
{
    return cue(m_editedCue) != nullptr ? m_editedCue : invalidId();
}

/* One collection's levels into @a levels: HTP on intensity, else the last
   wins; @a over: every channel it sets replaces what is there (an override
   hit). */
static void addCollectionLevels(Doc *doc, const Collection *coll, QHash<quint32, QByteArray> &levels, bool over)
{
    // One scene's levels, as Scene::write() lays them at its start: its own
    // values, then its looks in order (effect looks and the colours that
    // feed an effect left out, a look's left-out fixtures skipped), a
    // fixture with no dimmer dimmed by the look's Dimmer.
    auto put = [&](const SceneValue &scv, bool htp) {
        Fixture *fx = doc->fixture(scv.fxi);
        if (fx == nullptr || scv.channel >= fx->channels())
            return;
        QByteArray &bytes = levels[scv.fxi];
        if (bytes.size() != int(fx->channels()))
            bytes = QByteArray(int(fx->channels()), 0);
        const uchar now = uchar(bytes.at(int(scv.channel)));
        bytes[int(scv.channel)] = char(htp ? qMax(now, scv.value) : scv.value);
    };
    for (quint32 fid : coll->functions())
    {
        Scene *scene = qobject_cast<Scene *>(doc->function(fid));
        if (scene == nullptr)
            continue;
        QHash<QPair<quint32, quint32>, uchar> mine;
        QList<QPair<quint32, quint32> > order;
        auto set = [&](const SceneValue &scv) {
            const QPair<quint32, quint32> k(scv.fxi, scv.channel);
            if (!mine.contains(k))
                order << k;
            mine.insert(k, scv.value);
        };
        for (const SceneValue &scv : scene->values())
            set(scv);
        const QList<quint32> pals = scene->palettes();
        const QHash<quint32, QList<quint32> > excluded = scene->excludedFixtures();
        for (quint32 pid : pals)
        {
            QLCPalette *palette = doc->palette(pid);
            if (palette == nullptr || palette->type() == QLCPalette::Effect
                || QLCPalette::isEffectScoped(doc, pals, pid))
                continue;
            const int cOff = QLCPalette::colorSetOffset(doc, pals, pid);
            const QList<quint32> outList = excluded.value(pid);
            QList<SceneValue> vals = palette->valuesFromFixtureGroups(doc, scene->fixtureGroups(), scene, cOff);
            vals += palette->valuesFromFixtures(doc, scene->fixtures(), scene, cOff);
            for (const SceneValue &scv : vals)
                if (!outList.contains(scv.fxi))
                    set(scv);
        }
        const int lookDim = QLCPalette::lookDimmerValue(doc, pals);
        for (const QPair<quint32, quint32> &k : order)
        {
            Fixture *fx = doc->fixture(k.first);
            if (fx == nullptr)
                continue;
            SceneValue scv(k.first, k.second, mine.value(k));
            scv.value = QLCPalette::dimScaleWithoutDimmer(fx, scv.channel, scv.value, lookDim);
            const QLCChannel *ch = fx->channel(scv.channel);
            put(scv, !over && ch != nullptr && ch->group() == QLCChannel::Intensity);
        }
    }
}

QHash<quint32, QByteArray> ShowTreeController::cueLevels(quint32 cueId) const
{
    QHash<quint32, QByteArray> levels;
    const Cue *c = cue(cueId);
    const Collection *coll = c == nullptr ? nullptr
                           : qobject_cast<const Collection *>(m_doc->function(c->collection));
    if (coll != nullptr)
        addCollectionLevels(m_doc, coll, levels, false);
    return levels;
}

QHash<quint32, QByteArray> ShowTreeController::blindLevels(quint32 cueId) const
{
    QHash<quint32, QByteArray> levels = cueLevels(cueId);
    // decided (can change): an override hit live on stage is drawn over the
    // cue in edit -- it would win over that cue too; one not fired isn't.
    for (const Production &p : m_productions)
        if (const CueList *l = p.looks())
            for (const Cue &h : l->cues)
                if (h.override && h.id != cueId)
                    if (const Collection *coll = qobject_cast<const Collection *>(m_doc->function(h.collection)))
                        if (coll->isRunning())
                            addCollectionLevels(m_doc, coll, levels, true);
    return levels;
}

QStringList ShowTreeController::takeStageForRun()
{
    QStringList held;
    if (m_previewState == PreviewState::Playing)
        held << tr("the Show Tree preview");
    m_previewHeld = true;
    syncPreview();
    emit stageTakenForRun(&held);
    return held;
}

void ShowTreeController::resumePreview()
{
    if (!m_previewHeld)
        return;
    m_previewHeld = false;
    syncPreview();
}

void ShowTreeController::setPreviewSolo(quint32 cueId, quint32 layerId)
{
    if (cue(cueId) == nullptr)
        layerId = invalidId();
    const quint32 soloCue = layerId == invalidId() ? invalidId() : cueId;
    if (soloCue == m_soloCue && layerId == m_soloLayer)
        return;
    m_soloCue = soloCue;
    m_soloLayer = layerId;
    syncPreview();
}

QList<quint32> ShowTreeController::soloFunctions(quint32 cueId, const QList<quint32> &want) const
{
    ResolvedCue rc;
    if (!resolvedCue(cueId, &rc))
        return want;
    // The layer, then its child layers (and theirs), by target.
    QList<Target> targets;
    for (const ResolvedLayer &rl : rc.layers)
        if (rl.layerId == m_soloLayer && rl.depth == 0)
            targets << rl.target;
    if (targets.isEmpty())
        return want;        // not a layer of this cue (any more): the whole cue
    for (bool more = true; more; )
    {
        more = false;
        for (const ResolvedLayer &rl : rc.layers)
            if (rl.depth > 0 && targets.contains(rl.parent) && !targets.contains(rl.target))
            {
                targets << rl.target;
                more = true;
            }
    }
    QSet<quint32> keep;
    for (const Target &t : targets)
    {
        const quint32 sid = t.isOpaque() ? t.function : layerScene(cueId, t);
        if (sid != Function::invalidId())
            keep.insert(sid);
    }
    QList<quint32> out;
    for (quint32 fid : want)
        if (keep.contains(fid))
            out << fid;
    return out;
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
            if (const Function *f = l.isSubCues() ? nullptr : m_doc->function(l.chaser))
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
    else if (m_previewHeld)
    {
        // A run took the stage (GO): nothing previews until Play.
        state = PreviewState::Held;
    }
    else
    {
        // Design: what is in edit is what shows -- also over a list running
        // for real once Play asked for it, which keeps going underneath.
        if (const Collection *coll = qobject_cast<const Collection *>(m_doc->function(c->collection)))
            for (quint32 fid : coll->functions())
                if (m_doc->function(fid) != nullptr && !want.contains(fid))
                    want << fid;
        // Solo (walking the show): one layer of the cue.
        if (m_soloLayer != invalidId() && m_soloCue == c->id)
            want = soloFunctions(c->id, want);
        state = want.isEmpty() ? PreviewState::Dark : PreviewState::Playing;
    }

    // Start first, stop after: a scene both cues play is never let go of,
    // so it runs on (no re-fade, effects keep their phase). The preview's
    // scenes win on stage (Scene::setEditOverride: above the Auto faders,
    // LTP on every channel), so a running cue at full can't hide a dimmer
    // edited down; an opaque function (EFX, a matrix) mixes as it would.
    MasterTimer *timer = m_doc->masterTimer();
    if (timer == nullptr)
    {
        // The Doc is being torn down (~Doc frees its timer first): there is
        // nothing left to start or stop a preview on.
        m_previewRunning.clear();
        m_previewState = PreviewState::Off;
        return;
    }
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
        // Into Design the app stopped every list too: nothing holds now.
        if (listsRunning().isEmpty())
            m_previewHeld = false;
    }
    syncPreview();
}

void ShowTreeController::slotTimerFunctionStarted(quint32 fid)
{
    noteListRuntime(fid);       // scoped hits (Q154): what is armed
    // A cue list started for real -- from anywhere, a VC cue list too --
    // takes the stage: the preview holds (RC8). Not one a preview itself
    // plays (the Workbench on a list's chaser).
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            if (l.chaser == fid && !l.isSubCues())
            {
                const Function *f = m_doc->function(fid);
                if (f != nullptr && f->isRunning() && !f->startedOnlyByPreview()
                    && m_doc->mode() == Doc::Design && !m_previewHeld)
                    takeStageForRun();
                else
                    emit previewChanged();   // what the header says changes
                return;
            }
}

void ShowTreeController::slotTimerFunctionStopped(quint32 fid)
{
    noteListRuntime(fid);
    // A list stopping changes what the header says; the hold stays until
    // the next cue selected or Play (a Stop all never brings a preview
    // back on stage by itself).
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            if (l.chaser == fid)
            {
                emit previewChanged();
                return;
            }
}
