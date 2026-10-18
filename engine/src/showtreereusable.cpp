/*
  Q Light Controller Plus
  showtreereusable.cpp

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

#include <QDebug>
#include <algorithm>

#include "showtreecontroller.h"
#include "collection.h"
#include "qlcpalette.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

using namespace ShowTree;

/* Reusable layers and cues (SHOW_TREE_DESIGN.md §1, P4) and Detach of a
   cue list to a plain chaser. A use of a reusable is an ordinary layer (or
   cue) in the tree with a link (Layer::reusable, Cue::reusable): the
   resolver, the bake and every edit see plain layers. After each edit
   syncReusables() makes the uses the same again -- the one the edit changed
   gives the reusable its content -- so "edit it anywhere, everywhere" holds
   for every operation, by mouse or typed, without each knowing about it. */

/****************************************************************************
 * Helpers
 ****************************************************************************/

static Cue *findCueIn(QList<Production> &productions, quint32 cueId, CueList **listOut = nullptr)
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

static QString useKey(quint32 cueId, quint32 layerId, quint32 reusableId)
{
    return QStringLiteral("%1:%2:%3").arg(cueId).arg(layerId).arg(reusableId);
}

/** A reusable cue as a cue (for its content). */
static Cue asCue(const ReusableCue &r)
{
    Cue c;
    c.name = r.name;
    c.mode = r.mode;
    c.mixOverlap = r.mixOverlap;
    c.looks = r.looks;
    c.layers = r.layers;
    return c;
}

/** @a layers with ids from @a ids by position (new ones from @a newId),
    parents mapped along; what each use keeps of its own (what it carries
    on from) from @a keep by position. */
static QList<Layer> renumbered(const QList<Layer> &layers, const QList<Layer> &keep,
                               const std::function<quint32()> &newId, bool keepTracking)
{
    QList<Layer> out = layers;
    QHash<quint32, quint32> map;
    for (int i = 0; i < out.count(); ++i)
    {
        const quint32 id = i < keep.count() ? keep.at(i).id : newId();
        map.insert(layers.at(i).id, id);
        out[i].id = id;
        if (keepTracking)
        {
            out[i].continues = i < keep.count() ? keep.at(i).continues : Target();
            out[i].continuesAlso = i < keep.count() ? keep.at(i).continuesAlso : QList<Target>();
        }
    }
    for (Layer &l : out)
        if (l.parent != invalidId())
            l.parent = map.value(l.parent, invalidId());
    return out;
}

/****************************************************************************
 * Lookups
 ****************************************************************************/

const ReusableLayer *ShowTreeController::reusableLayer(quint32 id) const
{
    for (const ReusableLayer &r : m_reusableLayers)
        if (r.id == id)
            return &r;
    return nullptr;
}

const ReusableCue *ShowTreeController::reusableCue(quint32 id) const
{
    for (const ReusableCue &r : m_reusableCues)
        if (r.id == id)
            return &r;
    return nullptr;
}

QString ShowTreeController::reusableName(quint32 id) const
{
    if (const ReusableLayer *r = reusableLayer(id))
        return r->name;
    if (const ReusableCue *r = reusableCue(id))
        return r->name;
    return QString();
}

QList<ShowTreeController::ReusableUse> ShowTreeController::reusableUses(quint32 reusableId) const
{
    QList<ReusableUse> out;
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            for (int i = 0; i < l.cues.count(); ++i)
            {
                const Cue &c = l.cues.at(i);
                if (c.reusable == reusableId)
                {
                    ReusableUse u;
                    u.list = l.id;
                    u.cue = c.id;
                    u.label = QStringLiteral("%1 %2").arg(l.name, cueLabel(l, i));
                    out << u;
                }
                for (const Layer &layer : c.layers)
                    if (layer.reusable == reusableId)
                    {
                        ReusableUse u;
                        u.list = l.id;
                        u.cue = c.id;
                        u.layer = layer.id;
                        u.label = QStringLiteral("%1 %2").arg(l.name, cueLabel(l, i));
                        out << u;
                    }
            }
    return out;
}

/****************************************************************************
 * Keeping the uses the same
 ****************************************************************************/

void ShowTreeController::syncReusables(const QList<Production> &before, QList<Production> &after)
{
    if (m_reusableLayers.isEmpty() && m_reusableCues.isEmpty())
        return;

    // What each use was before the edit.
    QHash<QString, QString> wasLayer;
    QHash<quint32, QString> wasCue;
    for (const Production &p : before)
        for (const CueList &l : p.lists)
            for (const Cue &c : l.cues)
            {
                if (c.reusable != invalidId())
                    wasCue.insert(c.id, QString::number(c.reusable) + QLatin1Char('|') + cueContent(c));
                for (const Layer &layer : c.layers)
                    if (layer.reusable != invalidId())
                        wasLayer.insert(useKey(c.id, layer.id, layer.reusable), layerContent(layer));
            }

    auto syncLayers = [&](bool adopt) {
        for (ReusableLayer &r : m_reusableLayers)
        {
            Layer def;
            def.name = r.name;
            def.target = r.target;
            def.looks = r.looks;
            // The first use the edit changed (in tree order) is the new
            // content; a name taken away keeps the reusable's.
            const Layer *changed = nullptr;
            for (const Production &p : after)
                for (const CueList &l : p.lists)
                    for (const Cue &c : l.cues)
                        for (const Layer &layer : c.layers)
                        {
                            if (!adopt || changed != nullptr || layer.reusable != r.id)
                                continue;
                            const QString key = useKey(c.id, layer.id, r.id);
                            if (wasLayer.contains(key) && wasLayer.value(key) != layerContent(layer)
                                && layerContent(layer) != layerContent(def))
                                changed = &layer;
                        }
            if (changed != nullptr)
            {
                if (!changed->name.trimmed().isEmpty())
                    r.name = changed->name;
                r.target = changed->target;
                r.looks = changed->looks;
                def.name = r.name;
                def.target = r.target;
                def.looks = r.looks;
            }
            const QString content = layerContent(def);
            for (Production &p : after)
                for (CueList &l : p.lists)
                    for (Cue &c : l.cues)
                        for (Layer &layer : c.layers)
                            if (layer.reusable == r.id && layerContent(layer) != content)
                            {
                                layer.name = r.name;
                                layer.target = r.target;
                                layer.looks = r.looks;
                            }
            // A reusable cue's layers may use it too.
            for (ReusableCue &rc : m_reusableCues)
                for (Layer &layer : rc.layers)
                    if (layer.reusable == r.id)
                    {
                        layer.name = r.name;
                        layer.target = r.target;
                        layer.looks = r.looks;
                    }
        }
    };

    syncLayers(true);

    for (ReusableCue &r : m_reusableCues)
    {
        const Cue *changed = nullptr;
        for (const Production &p : after)
            for (const CueList &l : p.lists)
                for (const Cue &c : l.cues)
                {
                    if (changed != nullptr || c.reusable != r.id || !wasCue.contains(c.id))
                        continue;
                    const QString now = QString::number(c.reusable) + QLatin1Char('|') + cueContent(c);
                    if (wasCue.value(c.id) != now && cueContent(c) != cueContent(asCue(r)))
                        changed = &c;
                }
        if (changed != nullptr)
        {
            if (!changed->name.trimmed().isEmpty())
                r.name = changed->name;
            r.mode = changed->mode;
            r.mixOverlap = changed->mixOverlap;
            r.looks = changed->looks;
            r.layers = renumbered(changed->layers, r.layers, [this] { return newId(); }, false);
            for (Layer &layer : r.layers)
            {
                layer.continues = Target();
                layer.continuesAlso.clear();
            }
        }
        const Cue def = asCue(r);
        const QString content = cueContent(def);
        for (Production &p : after)
            for (CueList &l : p.lists)
                for (Cue &c : l.cues)
                    if (c.reusable == r.id && cueContent(c) != content)
                    {
                        c.name = r.name;
                        c.mode = r.mode;
                        c.mixOverlap = r.mixOverlap;
                        c.looks = r.looks;
                        c.layers = renumbered(r.layers, c.layers, [this] { return newId(); }, true);
                    }
    }

    // A reusable cue's content may hold a reusable layer: the same again.
    syncLayers(false);
}

/****************************************************************************
 * Make reusable, uses, Unlink here
 ****************************************************************************/

quint32 ShowTreeController::makeLayerReusable(quint32 cueId, quint32 layerId, const QString &name, QString *why)
{
    auto no = [why](const QString &text) {
        if (why != nullptr)
            *why = text;
        return invalidId();
    };
    const Cue *c = cue(cueId);
    const Layer *l = c == nullptr ? nullptr : c->layer(layerId);
    if (l == nullptr)
        return no(tr("No such layer."));
    if (l->reusable != invalidId())
        return no(tr("%1 is reusable already.").arg(reusableName(l->reusable)));
    if (l->parent != invalidId())
        return no(tr("A child layer is part of its parent: make the parent reusable."));
    if (c->reusable != invalidId())
        return no(tr("This cue is a use of the reusable cue %1: its layers are the reusable's.")
                  .arg(reusableName(c->reusable)));
    QString n = name.trimmed();
    if (n.isEmpty())
        n = l->name.isEmpty() ? targetLabel(l->target) : l->name;
    const quint32 id = newId();
    const bool ok = edit(tr("Make %1 reusable").arg(n), [&](QList<Production> &ps) {
        Cue *cc = findCueIn(ps, cueId);
        Layer *layer = cc == nullptr ? nullptr : cc->layer(layerId);
        if (layer == nullptr)
            return false;
        layer->name = n;
        layer->reusable = id;
        ReusableLayer r;
        r.id = id;
        r.name = n;
        r.target = layer->target;
        r.looks = layer->looks;
        m_reusableLayers << r;
        return true;
    });
    return ok ? id : invalidId();
}

quint32 ShowTreeController::makeCueReusable(quint32 cueId, const QString &name, QString *why)
{
    auto no = [why](const QString &text) {
        if (why != nullptr)
            *why = text;
        return invalidId();
    };
    const CueList *list = listOfCue(cueId);
    const Cue *c = cue(cueId);
    if (c == nullptr)
        return no(tr("No such cue."));
    if (c->reusable != invalidId())
        return no(tr("%1 is reusable already.").arg(reusableName(c->reusable)));
    QString n = name.trimmed();
    if (n.isEmpty())
        n = c->name.isEmpty() ? cueLabel(*list, list->indexOf(cueId)) : c->name;
    const quint32 id = newId();
    const bool ok = edit(tr("Make cue %1 reusable").arg(n), [&](QList<Production> &ps) {
        Cue *cc = findCueIn(ps, cueId);
        if (cc == nullptr)
            return false;
        cc->name = n;
        cc->reusable = id;
        ReusableCue r;
        r.id = id;
        r.name = n;
        r.mode = cc->mode;
        r.mixOverlap = cc->mixOverlap;
        r.looks = cc->looks;
        r.layers = renumbered(cc->layers, QList<Layer>(), [this] { return newId(); }, false);
        for (Layer &layer : r.layers)
        {
            layer.continues = Target();
            layer.continuesAlso.clear();
        }
        r.note = cc->note;
        r.fadeIn = cc->fadeIn;
        r.fadeOut = cc->fadeOut;
        r.hold = cc->hold;
        m_reusableCues << r;
        return true;
    });
    return ok ? id : invalidId();
}

quint32 ShowTreeController::insertReusableLayer(quint32 cueId, quint32 reusableId)
{
    const ReusableLayer *r = reusableLayer(reusableId);
    const Cue *c = cue(cueId);
    if (r == nullptr || c == nullptr)
        return invalidId();
    const ReusableLayer def = *r;
    const quint32 id = newId();
    const bool ok = edit(tr("Use %1").arg(def.name), [&](QList<Production> &ps) {
        Cue *cc = findCueIn(ps, cueId);
        if (cc == nullptr)
            return false;
        Layer l;
        l.id = id;
        l.name = def.name;
        l.target = def.target;
        l.looks = def.looks;
        l.reusable = def.id;
        cc->layers << l;
        return true;
    });
    return ok ? id : invalidId();
}

quint32 ShowTreeController::insertReusableCue(quint32 listId, int afterIndex, quint32 reusableId)
{
    const ReusableCue *r = reusableCue(reusableId);
    const CueList *list = cueList(listId);
    if (r == nullptr || list == nullptr)
        return invalidId();
    const ReusableCue def = *r;
    const quint32 id = newId();
    const bool ok = edit(tr("Use cue %1").arg(def.name), [&](QList<Production> &ps) {
        CueList *l = nullptr;
        for (Production &p : ps)
            if (CueList *have = p.list(listId))
                l = have;
        if (l == nullptr)
            return false;
        Cue c;
        c.id = id;
        c.name = def.name;
        c.number = numberForInsert(*l, afterIndex);
        c.note = def.note;
        c.fadeIn = def.fadeIn;
        c.fadeOut = def.fadeOut;
        c.hold = def.hold;
        c.mode = def.mode;
        c.mixOverlap = def.mixOverlap;
        c.looks = def.looks;
        c.layers = renumbered(def.layers, QList<Layer>(), [this] { return newId(); }, true);
        c.reusable = def.id;
        l->cues.insert(qBound(0, afterIndex + 1, int(l->cues.count())), c);
        return true;
    });
    return ok ? id : invalidId();
}

bool ShowTreeController::unlinkLayer(quint32 cueId, quint32 layerId)
{
    const Cue *c = cue(cueId);
    const Layer *l = c == nullptr ? nullptr : c->layer(layerId);
    if (l == nullptr || l->reusable == invalidId())
        return false;
    return edit(tr("Unlink %1 here").arg(reusableName(l->reusable)), [&](QList<Production> &ps) {
        Cue *cc = findCueIn(ps, cueId);
        Layer *layer = cc == nullptr ? nullptr : cc->layer(layerId);
        if (layer == nullptr || layer->reusable == invalidId())
            return false;
        layer->unlinkedFrom = reusableName(layer->reusable);
        layer->reusable = invalidId();
        return true;
    });
}

bool ShowTreeController::unlinkCue(quint32 cueId)
{
    const Cue *c = cue(cueId);
    if (c == nullptr || c->reusable == invalidId())
        return false;
    return edit(tr("Unlink cue %1 here").arg(reusableName(c->reusable)), [&](QList<Production> &ps) {
        Cue *cc = findCueIn(ps, cueId);
        if (cc == nullptr || cc->reusable == invalidId())
            return false;
        cc->unlinkedFrom = reusableName(cc->reusable);
        cc->reusable = invalidId();
        return true;
    });
}

bool ShowTreeController::renameReusable(quint32 reusableId, const QString &name)
{
    const QString n = name.trimmed();
    if (n.isEmpty() || reusableName(reusableId).isEmpty() || reusableName(reusableId) == n)
        return false;
    return edit(tr("Rename %1").arg(reusableName(reusableId)), [&](QList<Production> &) {
        // The uses take it from the reusable (syncReusables).
        for (ReusableLayer &r : m_reusableLayers)
            if (r.id == reusableId)
                r.name = n;
        for (ReusableCue &r : m_reusableCues)
            if (r.id == reusableId)
                r.name = n;
        return true;
    });
}

bool ShowTreeController::removeReusable(quint32 reusableId, bool unlinkUses, QString *why)
{
    const QString name = reusableName(reusableId);
    if (name.isEmpty())
        return false;
    const QList<ReusableUse> uses = reusableUses(reusableId);
    // A reusable cue holding it counts as a use too.
    QStringList inCues;
    for (const ReusableCue &rc : m_reusableCues)
        for (const Layer &l : rc.layers)
            if (l.reusable == reusableId && !inCues.contains(rc.name))
                inCues << rc.name;
    if ((!uses.isEmpty() || !inCues.isEmpty()) && !unlinkUses)
    {
        if (why != nullptr)
        {
            QStringList where;
            for (const ReusableUse &u : uses)
                where << u.label;
            for (const QString &rc : inCues)
                where << tr("reusable cue %1").arg(rc);
            *why = tr("%1 is used in %2: %3.").arg(name).arg(where.count())
                   .arg(where.mid(0, 6).join(QStringLiteral(", ")) + (where.count() > 6 ? QStringLiteral(", …") : QString()));
        }
        return false;
    }
    return edit(tr("Delete %1").arg(name), [&](QList<Production> &ps) {
        for (Production &p : ps)
            for (CueList &l : p.lists)
                for (Cue &c : l.cues)
                {
                    if (c.reusable == reusableId)
                    {
                        c.unlinkedFrom = name;
                        c.reusable = invalidId();
                    }
                    for (Layer &layer : c.layers)
                        if (layer.reusable == reusableId)
                        {
                            layer.unlinkedFrom = name;
                            layer.reusable = invalidId();
                        }
                }
        for (ReusableCue &rc : m_reusableCues)
            for (Layer &layer : rc.layers)
                if (layer.reusable == reusableId)
                    layer.reusable = invalidId();
        for (int i = m_reusableLayers.count() - 1; i >= 0; --i)
            if (m_reusableLayers.at(i).id == reusableId)
                m_reusableLayers.removeAt(i);
        for (int i = m_reusableCues.count() - 1; i >= 0; --i)
            if (m_reusableCues.at(i).id == reusableId)
                m_reusableCues.removeAt(i);
        return true;
    });
}

/****************************************************************************
 * Detach a cue list to a plain chaser
 ****************************************************************************/

bool ShowTreeController::usedOutsideTree(quint32 id)
{
    const QList<quint32> usage = m_doc->getUsage(id);
    for (int i = 0; i + 1 < usage.count(); i += 2)
        if (!isGenerated(usage.at(i)))
            return true;
    return false;
}

void ShowTreeController::handOver(quint32 id)
{
    Function *f = m_doc->function(id);
    m_generatedScenes.remove(id);
    m_generatedCollections.remove(id);
    m_generatedChasers.remove(id);
    m_generatedLabels.remove(id);
    m_detachedLists.insert(id);
    if (f != nullptr && f->type() != Function::ChaserType)
        f->setVisible(true);
    // One-off values it plays are the show's palettes now (they stay where
    // they are, under their names).
    if (const Scene *s = qobject_cast<const Scene *>(f))
        for (quint32 pid : s->palettes())
            if (m_inlinePalettes.remove(pid))
                m_madeReusable.insert(pid);
}

void ShowTreeController::copySharedScenes(quint32 collectionId, QList<quint32> *made)
{
    Collection *coll = qobject_cast<Collection *>(m_doc->function(collectionId));
    if (coll == nullptr)
        return;
    QList<quint32> fids = coll->functions();
    bool changed = false;
    for (int k = 0; k < fids.count(); ++k)
    {
        Function *f = m_doc->function(fids.at(k));
        // Still the tree's: another list plays it (else it went with this one).
        if (f == nullptr || !m_generatedScenes.contains(fids.at(k)))
            continue;
        Function *copy = f->createCopy(m_doc, true);
        if (copy == nullptr)
            continue;
        copy->setName(f->name());
        copy->setPath(f->path(true));
        fids[k] = copy->id();
        changed = true;
        if (made != nullptr)
            *made << copy->id();
    }
    if (!changed)
        return;
    for (quint32 fid : coll->functions())
        coll->removeFunction(fid);
    for (quint32 fid : fids)
        coll->addFunction(fid);
}

void ShowTreeController::applyDetachedLists()
{
    if (!m_snapshotHasDetachedLists)
        return;
    QList<quint32> ids = m_detachedLists.values();
    std::sort(ids.begin(), ids.end());
    for (quint32 id : ids)
    {
        if (m_snapshotDetachedLists.contains(id))
            continue;
        // Undo past the detach: the tree's again, as it was.
        m_detachedLists.remove(id);
        Function *f = m_doc->function(id);
        if (f == nullptr)
            continue;
        if (f->type() == Function::ChaserType)
            m_generatedChasers.insert(id);
        else if (f->type() == Function::CollectionType)
            m_generatedCollections.insert(id);
        else if (f->type() == Function::SceneType)
            m_generatedScenes.insert(id);
        if (f->type() != Function::ChaserType)
            f->setVisible(false);
    }
    QList<quint32> again = m_snapshotDetachedLists.values();
    std::sort(again.begin(), again.end());
    QList<quint32> collections;
    for (quint32 id : again)
        if (!m_detachedLists.contains(id) && m_doc->function(id) != nullptr)
        {
            handOver(id);
            if (m_doc->function(id)->type() == Function::CollectionType)
                collections << id;
        }
    // Redo: what another list plays is copied again (the copies an undo
    // took back were swept as unused).
    for (quint32 id : collections)
    {
        QList<quint32> made;
        copySharedScenes(id, &made);
        for (quint32 c : made)
            handOver(c);
    }
}

quint32 ShowTreeController::detachList(quint32 listId)
{
    const CueList *list = cueList(listId);
    if (list == nullptr || list->looks || !m_generatedChasers.contains(list->chaser)
        || m_doc->function(list->chaser) == nullptr)
        return Function::invalidId();
    const quint32 chaserId = list->chaser;

    // What goes with it: the chaser, its cues' collections, and the scenes
    // only this list plays.
    QSet<quint32> otherScenes;
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
        {
            if (l.id == listId)
                continue;
            for (const Cue &c : l.cues)
                if (const Collection *coll = qobject_cast<const Collection *>(m_doc->function(c.collection)))
                    for (quint32 fid : coll->functions())
                        otherScenes.insert(fid);
        }
    QList<quint32> going { chaserId };
    for (const Cue &c : list->cues)
    {
        const Collection *coll = qobject_cast<const Collection *>(m_doc->function(c.collection));
        if (coll == nullptr || !m_generatedCollections.contains(c.collection))
            continue;
        going << c.collection;
        for (quint32 fid : coll->functions())
            if (m_generatedScenes.contains(fid) && !otherScenes.contains(fid) && !going.contains(fid))
                going << fid;
    }
    const QString name = list->name;

    const bool ok = edit(tr("Detach %1 to a plain chaser").arg(name), [&](QList<Production> &ps) {
        for (Production &p : ps)
            for (int i = 0; i < p.lists.count(); ++i)
                if (p.lists.at(i).id == listId)
                {
                    // Handed over before the bake below would find them unused.
                    for (quint32 id : going)
                        handOver(id);
                    // A scene another list still plays: copied (open 19),
                    // so the chaser is all its own from the start.
                    QList<quint32> made;
                    for (quint32 id : going)
                        if (m_doc->function(id) != nullptr
                            && m_doc->function(id)->type() == Function::CollectionType)
                            copySharedScenes(id, &made);
                    for (quint32 id : made)
                        handOver(id);
                    p.lists.removeAt(i);
                    return true;
                }
        return false;
    });
    return ok ? chaserId : Function::invalidId();
}
