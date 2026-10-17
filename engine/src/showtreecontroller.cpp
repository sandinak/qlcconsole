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
#include <QDebug>
#include <algorithm>

#include "showtreecontroller.h"
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "qlcfixturemode.h"
#include "speedmasters.h"
#include "fixturegroup.h"
#include "collection.h"
#include "qlcchannel.h"
#include "chaserstep.h"
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
        c->includes.removeAll(cue.id);
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
    m_generatedScenes.remove(fid);
    m_generatedCollections.remove(fid);
    m_generatedChasers.remove(fid);
    m_generatedLabels.remove(fid);
}

void ShowTreeController::slotPaletteRemoved(quint32 pid)
{
    m_inlinePalettes.remove(pid);
}
