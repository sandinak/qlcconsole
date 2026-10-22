/*
  Q Light Controller Plus
  paletteops.cpp

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
#include <QDateTime>
#include <QSet>
#include <algorithm>

#include "showtreecontroller.h"
#include "paletteops.h"
#include "function.h"
#include "scene.h"
#include "doc.h"

using namespace ShowTree;

PaletteOps::PaletteOps(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
}

PaletteOps::~PaletteOps()
{
}

/****************************************************************************
 * Folders
 ****************************************************************************/

QString PaletteOps::cleanFolder(const QString &folder)
{
    QStringList parts = folder.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (QString &p : parts)
        p = p.trimmed();
    parts.removeAll(QString());
    return parts.join(QLatin1Char('/'));
}

QString PaletteOps::folderOfPath(const QString &path, QLCPalette::PaletteType type)
{
    // One folder model (feat/palette-folders): stored paths are
    // type-independent ("Palettes/01-Song/"); an older show's per-type
    // layout was migrated when it loaded (QLCPalette::migrateLegacyPath),
    // so a folder is simply the path under "Palettes" -- as the
    // Programming tab's tree reads it.
    Q_UNUSED(type)
    QStringList segs = cleanFolder(path).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (!segs.isEmpty() && segs.first() == QLatin1String("Palettes"))
        segs.removeFirst();
    return segs.join(QLatin1Char('/'));
}

QString PaletteOps::folderOf(const QLCPalette *p)
{
    return p == nullptr ? QString() : folderOfPath(p->path(), p->type());
}

QString PaletteOps::pathForFolder(const QString &folder)
{
    const QString f = cleanFolder(folder);
    return f.isEmpty() ? QString() : QStringLiteral("Palettes/%1/").arg(f);
}

/** Whether folder path @a path is @a folder or under it. */
static bool inFolder(const QString &path, const QString &folder)
{
    return !path.isEmpty() && (path == folder || path.startsWith(folder + QLatin1Char('/')));
}

bool PaletteOps::isManaged(quint32 paletteId) const
{
    const ShowTreeController *st = m_doc->showTree();
    return m_doc->palette(paletteId) != nullptr && (st == nullptr || !st->isInlinePalette(paletteId));
}

QStringList PaletteOps::folders() const
{
    QStringList out;
    auto add = [&out](const QString &path) {
        const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        for (int i = 1; i <= parts.count(); ++i)
        {
            const QString p = parts.mid(0, i).join(QLatin1Char('/'));
            if (!out.contains(p))
                out << p;
        }
    };
    for (const QLCPalette *p : m_doc->palettes())
        if (isManaged(p->id()))
            add(folderOf(p));
    for (const QString &f : m_emptyFolders)
        add(f);
    std::sort(out.begin(), out.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return out;
}

int PaletteOps::folderCount(const QString &folder) const
{
    const QString f = cleanFolder(folder);
    int n = 0;
    for (const QLCPalette *p : m_doc->palettes())
        if (isManaged(p->id()) && inFolder(folderOf(p), f))
            ++n;
    return n;
}

bool PaletteOps::addFolder(const QString &folder)
{
    const QString f = cleanFolder(folder);
    if (f.isEmpty() || folders().contains(f))
        return false;
    return step(tr("New folder %1").arg(f), {}, [&](QList<quint32> &) {
        m_emptyFolders << f;
        return true;
    });
}

bool PaletteOps::renameFolder(const QString &folder, const QString &name)
{
    const QString from = cleanFolder(folder);
    const QString leaf = cleanFolder(name);
    if (from.isEmpty() || leaf.isEmpty() || leaf.contains(QLatin1Char('/')))
        return false;
    const int slash = from.lastIndexOf(QLatin1Char('/'));
    const QString to = slash < 0 ? leaf : from.left(slash + 1) + leaf;
    if (to == from)
        return false;
    auto moved = [&](const QString &path) { return path == from ? to : to + path.mid(from.length()); };
    QList<quint32> ids;
    for (const QLCPalette *p : m_doc->palettes())
        if (isManaged(p->id()) && inFolder(folderOf(p), from))
            ids << p->id();
    bool emptyToo = false;
    for (const QString &f : m_emptyFolders)
        emptyToo |= inFolder(f, from);
    if (ids.isEmpty() && !emptyToo)
        return false;
    return step(tr("Rename folder %1").arg(from), ids, [&](QList<quint32> &) {
        for (quint32 id : ids)
            if (QLCPalette *p = m_doc->palette(id))
                p->setPath(pathForFolder(moved(folderOf(p))));
        for (QString &f : m_emptyFolders)
            if (inFolder(f, from))
                f = moved(f);
        m_emptyFolders.removeDuplicates();
        return true;
    });
}

bool PaletteOps::removeFolder(const QString &folder)
{
    const QString gone = cleanFolder(folder);
    if (gone.isEmpty())
        return false;
    const int slash = gone.lastIndexOf(QLatin1Char('/'));
    const QString up = slash < 0 ? QString() : gone.left(slash);
    auto moved = [&](const QString &path) {
        if (path == gone)
            return up;
        const QString rest = path.mid(gone.length() + 1);
        return up.isEmpty() ? rest : up + QLatin1Char('/') + rest;
    };
    QList<quint32> ids;
    for (const QLCPalette *p : m_doc->palettes())
        if (isManaged(p->id()) && inFolder(folderOf(p), gone))
            ids << p->id();
    bool emptyToo = false;
    for (const QString &f : m_emptyFolders)
        emptyToo |= inFolder(f, gone);
    if (ids.isEmpty() && !emptyToo)
        return false;
    return step(tr("Delete folder %1").arg(gone), ids, [&](QList<quint32> &) {
        for (quint32 id : ids)
            if (QLCPalette *p = m_doc->palette(id))
                p->setPath(pathForFolder(moved(folderOf(p))));
        QStringList keep;
        for (const QString &f : m_emptyFolders)
        {
            if (!inFolder(f, gone))
                keep << f;
            else if (f != gone && !moved(f).isEmpty())
                keep << moved(f);
        }
        keep.removeDuplicates();
        m_emptyFolders = keep;
        return true;
    });
}

/****************************************************************************
 * Palettes
 ****************************************************************************/

void PaletteOps::setStartValue(QLCPalette *p)
{
    // As the Programming tab always started one: valid, and it saves.
    switch (p->type())
    {
        case QLCPalette::Color:   p->setValue(QColor(Qt::white).name()); break;
        case QLCPalette::Dimmer:  p->setValue(255); break;
        case QLCPalette::PanTilt: p->setValue(270, 135); break;
        case QLCPalette::Aim:     break;    // no values: the target is set in the Look Editor
        case QLCPalette::Effect:  break;    // the script is picked in the Look Editor
        case QLCPalette::Strobe:  p->setValue(0.25f); break;
        default:                  p->setValue(0); break;
    }
}

QString PaletteOps::newName(QLCPalette::PaletteType type, const QString &base) const
{
    const QString stem = base.isEmpty() ? tr("New %1").arg(QLCPalette::typeToString(type)) : base;
    QSet<QString> taken;
    for (const QLCPalette *p : m_doc->palettes())
        if (p->type() == type)
            taken.insert(p->name().toLower());
    if (!taken.contains(stem.toLower()))
        return stem;
    for (int n = 2;; ++n)
    {
        const QString name = QStringLiteral("%1 %2").arg(stem).arg(n);
        if (!taken.contains(name.toLower()))
            return name;
    }
}

quint32 PaletteOps::create(QLCPalette::PaletteType type, const QString &path, const QString &name,
                           const QVariantList &values)
{
    if (type == QLCPalette::Undefined)
        return QLCPalette::invalidId();
    quint32 made = QLCPalette::invalidId();
    const QString nm = name.trimmed().isEmpty() ? newName(type) : name.trimmed();
    step(tr("New %1 palette").arg(QLCPalette::typeToString(type)), {}, [&](QList<quint32> &ids) {
        QLCPalette *p = new QLCPalette(type);
        if (values.isEmpty())
            setStartValue(p);
        else
            p->setValues(values);
        p->setName(nm);
        p->setPath(path);
        if (!m_doc->addPalette(p))
        {
            delete p;
            return false;
        }
        made = p->id();
        ids << made;
        m_emptyFolders.removeAll(folderOf(p));
        return true;
    });
    return made;
}

quint32 PaletteOps::duplicate(quint32 paletteId)
{
    QLCPalette *src = m_doc->palette(paletteId);
    if (src == nullptr || !isManaged(paletteId))
        return QLCPalette::invalidId();
    quint32 made = QLCPalette::invalidId();
    step(tr("Duplicate %1").arg(src->name()), {}, [&](QList<quint32> &ids) {
        QLCPalette *copy = src->createCopy();
        if (copy == nullptr)
            return false;
        copy->setName(src->name() + tr(" (copy)"));
        if (!m_doc->addPalette(copy))
        {
            delete copy;
            return false;
        }
        made = copy->id();
        ids << made;
        return true;
    });
    return made;
}

bool PaletteOps::rename(quint32 paletteId, const QString &name)
{
    QLCPalette *p = m_doc->palette(paletteId);
    const QString nm = name.trimmed();
    if (p == nullptr || !isManaged(paletteId) || nm.isEmpty() || nm == p->name())
        return false;
    return step(tr("Rename %1").arg(p->name()), { paletteId }, [&](QList<quint32> &) {
        p->setName(nm);
        return true;
    });
}

bool PaletteOps::setPath(const QList<quint32> &paletteIds, const QString &path)
{
    QList<quint32> ids;
    for (quint32 id : paletteIds)
        if (QLCPalette *p = m_doc->palette(id))
            if (isManaged(id) && p->path() != path && !ids.contains(id))
                ids << id;
    if (ids.isEmpty())
        return false;
    const QString folder = folderOfPath(path, QLCPalette::Undefined);
    return step(folder.isEmpty() ? tr("Out of folder") : tr("Move to %1").arg(folder), ids,
                [&](QList<quint32> &) {
        for (quint32 id : ids)
            m_doc->palette(id)->setPath(path);
        m_emptyFolders.removeAll(folder);
        return true;
    });
}

bool PaletteOps::move(const QList<quint32> &paletteIds, const QString &folder)
{
    // Only those not in that folder already: a palette stored the older
    // way is left as it is when it is there.
    const QString f = cleanFolder(folder);
    QList<quint32> ids;
    for (quint32 id : paletteIds)
        if (const QLCPalette *p = m_doc->palette(id))
            if (folderOf(p) != f)
                ids << id;
    return setPath(ids, pathForFolder(f));
}

bool PaletteOps::remove(const QList<quint32> &paletteIds)
{
    QList<quint32> ids;
    for (quint32 id : paletteIds)
        if (isManaged(id) && !ids.contains(id))
            ids << id;
    if (ids.isEmpty())
        return false;
    const QString label = ids.count() == 1 ? tr("Delete %1").arg(m_doc->palette(ids.first())->name())
                                           : tr("Delete %1 palettes").arg(ids.count());
    // The tree's uses go with it (open 26), in the same step: the tree on
    // either side rides on it, so one Undo puts back the palette and every
    // look that named it.
    ShowTreeController *tree = m_doc->showTree();
    const QByteArray treeBefore = tree != nullptr ? tree->outsideUndoState() : QByteArray();
    bool treeChanged = false;
    const bool ok = step(label, ids, [&](QList<quint32> &) {
        auto deleteThem = [&]() {
            for (quint32 id : ids)
                m_doc->deletePalette(id);
        };
        if (tree != nullptr)
            treeChanged = tree->removePaletteUses(ids, deleteThem);
        else
            deleteThem();
        return true;
    });
    if (ok && treeChanged)
    {
        m_undo.last().treeBefore = treeBefore;
        m_undo.last().treeAfter = tree->outsideUndoState();
    }
    return ok;
}

/****************************************************************************
 * Usage
 ****************************************************************************/

QList<PaletteOps::Use> PaletteOps::usage(quint32 paletteId) const
{
    QList<Use> out;
    const ShowTreeController *st = m_doc->showTree();
    auto inLooks = [paletteId](const QList<Look> &looks) {
        for (const Look &l : looks)
            if (l.palettes.contains(paletteId) || l.own.contains(paletteId))
                return true;
        return false;
    };
    if (st != nullptr)
    {
        for (const Production &p : st->productions())
            for (const CueList &l : p.lists)
                for (int i = 0; i < l.cues.count(); ++i)
                {
                    const Cue &c = l.cues.at(i);
                    const QString where = l.looks ? tr("Look %1").arg(st->cueLabel(l, i))
                                                  : tr("Cue %1 in %2").arg(st->cueLabel(l, i), l.name);
                    auto use = [&](quint32 layer, const QString &what) {
                        Use u;
                        u.kind = l.looks ? Use::Look : Use::Cue;
                        u.owner = c.id;
                        u.layer = layer;
                        u.function = c.collection;
                        u.label = QStringLiteral("%1 — %2").arg(where, what);
                        out << u;
                    };
                    if (inLooks(c.looks))
                        use(invalidId(), l.looks ? tr("look-wide") : tr("cue look"));
                    for (const Layer &layer : c.layers)
                        if (inLooks(layer.looks))
                            use(layer.id, layer.name.isEmpty() ? st->targetLabel(layer.target) : layer.name);
                }
        for (const ReusableLayer &r : st->reusableLayers())
            if (inLooks(r.looks))
            {
                Use u;
                u.kind = Use::Reusable;
                u.owner = r.id;
                u.label = tr("Reusable layer %1").arg(r.name);
                out << u;
            }
        for (const ReusableCue &r : st->reusableCues())
        {
            bool used = inLooks(r.looks);
            for (const Layer &layer : r.layers)
                used |= inLooks(layer.looks);
            if (used)
            {
                Use u;
                u.kind = Use::Reusable;
                u.owner = r.id;
                u.label = tr("Reusable cue %1").arg(r.name);
                out << u;
            }
        }
    }
    QList<Function *> functions = m_doc->functions();
    std::sort(functions.begin(), functions.end(), [](const Function *a, const Function *b) {
        return a->id() < b->id();
    });
    for (const Function *f : functions)
    {
        const Scene *s = qobject_cast<const Scene *>(f);
        if (s == nullptr || (st != nullptr && st->isGenerated(s->id())))
            continue;
        const int at = int(s->palettes().indexOf(paletteId));
        if (at < 0)
            continue;
        Use u;
        u.kind = Use::Scene;
        u.owner = s->id();
        u.function = s->id();
        u.label = tr("Scene “%1” — look %2").arg(s->name()).arg(at + 1);
        out << u;
    }
    for (const QLCPalette *p : m_doc->palettes())
    {
        if (p->id() == paletteId)
            continue;
        const QMap<QString, quint32> binds = p->effectPaletteBindings();
        for (auto it = binds.constBegin(); it != binds.constEnd(); ++it)
            if (it.value() == paletteId)
            {
                Use u;
                u.kind = Use::EffectSlot;
                u.owner = p->id();
                u.label = tr("Effect look “%1” — its “%2” slot").arg(p->name(), it.key());
                out << u;
            }
    }
    return out;
}

QString PaletteOps::usageSummary(const QList<Use> &uses)
{
    QSet<quint32> cues, looks, reusables, scenes;
    int slotCount = 0;
    for (const Use &u : uses)
    {
        switch (u.kind)
        {
            case Use::Cue: cues.insert(u.owner); break;
            case Use::Look: looks.insert(u.owner); break;
            case Use::Reusable: reusables.insert(u.owner); break;
            case Use::Scene: scenes.insert(u.owner); break;
            case Use::EffectSlot: ++slotCount; break;
        }
    }
    QStringList parts;
    if (!cues.isEmpty())
        parts << tr("%n cue(s)", "", int(cues.count()));
    if (!looks.isEmpty())
        parts << tr("%n look(s)", "", int(looks.count()));
    if (!reusables.isEmpty())
        parts << tr("%n reusable(s)", "", int(reusables.count()));
    if (!scenes.isEmpty())
        parts << tr("%n scene(s)", "", int(scenes.count()));
    if (slotCount > 0)
        parts << tr("%n effect slot(s)", "", slotCount);
    return parts.join(QStringLiteral(", "));
}

/****************************************************************************
 * Undo
 ****************************************************************************/

quint64 PaletteOps::nextUndoStamp()
{
    static quint64 s_stamp = 0;
    return ++s_stamp;
}

quint64 PaletteOps::undoStamp() const
{
    return m_undo.isEmpty() ? 0 : m_undo.last().stamp;
}

quint64 PaletteOps::redoStamp() const
{
    return m_redo.isEmpty() ? 0 : m_redo.last().stamp;
}

QString PaletteOps::undoText() const
{
    return m_undo.isEmpty() ? QString() : m_undo.last().label;
}

QString PaletteOps::redoText() const
{
    return m_redo.isEmpty() ? QString() : m_redo.last().label;
}

PaletteOps::State PaletteOps::capture(quint32 id) const
{
    State s;
    s.id = id;
    QLCPalette *p = m_doc->palette(id);
    if (p == nullptr)
        return s;
    s.exists = true;
    s.name = p->name();
    s.path = p->path();
    s.values = p->values();
    s.colourMacro = p->colourMacro();
    s.goboName = p->goboName();
    s.goboValues = p->goboValues();
    QBuffer buffer(&s.xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.writeStartDocument();
    p->saveXML(&w);
    w.writeEndDocument();
    // Where it is used, to put it back there if it goes.
    for (const Function *f : m_doc->functions())
        if (const Scene *sc = qobject_cast<const Scene *>(f))
        {
            const int at = int(sc->palettes().indexOf(id));
            if (at >= 0)
                s.scenes << qMakePair(sc->id(), at);
        }
    for (const QLCPalette *other : m_doc->palettes())
    {
        const QMap<QString, quint32> binds = other->effectPaletteBindings();
        for (auto it = binds.constBegin(); it != binds.constEnd(); ++it)
            if (it.value() == id)
                s.bindings << qMakePair(other->id(), it.key());
    }
    return s;
}

void PaletteOps::apply(const QList<State> &states, const QStringList &folders)
{
    for (const State &s : states)
    {
        QLCPalette *have = m_doc->palette(s.id);
        if (!s.exists)
        {
            if (have != nullptr)
                m_doc->deletePalette(s.id);
            continue;
        }
        if (have == nullptr)
        {
            QXmlStreamReader r(s.xml);
            while (!r.atEnd() && !r.isStartElement())
                r.readNext();
            QLCPalette *p = new QLCPalette(QLCPalette::Undefined, m_doc);
            if (!p->loadXML(r) || !m_doc->addPalette(p, s.id))
            {
                delete p;
                continue;
            }
            p->setPath(s.path);     // as it was stored, whatever loadXML makes of it
            // Back in every scene, at its place, and every slot.
            for (const QPair<quint32, int> &at : s.scenes)
            {
                Scene *sc = qobject_cast<Scene *>(m_doc->function(at.first));
                if (sc == nullptr || sc->palettes().contains(s.id))
                    continue;
                QList<quint32> order = sc->palettes();
                order.insert(qBound(0, at.second, int(order.count())), s.id);
                sc->addPalette(s.id);
                sc->reorderPalettes(order);
            }
            for (const QPair<quint32, QString> &slot : s.bindings)
                if (QLCPalette *other = m_doc->palette(slot.first))
                    other->setEffectPaletteBinding(slot.second, s.id);
            continue;
        }
        if (have->name() != s.name)
            have->setName(s.name);
        if (have->path() != s.path)
            have->setPath(s.path);
        if (s.value)
        {
            have->setValues(s.values);
            have->setColourMacro(s.colourMacro);
            have->setGoboName(s.goboName);
            have->setGoboValues(s.goboValues);
            // Scenes read a palette as they run: the ones playing it, again.
            for (Function *f : m_doc->functions())
                if (Scene *sc = qobject_cast<Scene *>(f))
                    if (sc->isRunning() && sc->palettes().contains(s.id))
                        sc->requestPaletteRefresh();
        }
    }
    m_emptyFolders = folders;
    m_doc->setModified();
}

bool PaletteOps::step(const QString &label, QList<quint32> ids,
                      const std::function<bool(QList<quint32> &)> &change)
{
    Step st;
    st.label = label;
    for (quint32 id : ids)
        st.before << capture(id);
    st.foldersBefore = m_emptyFolders;
    QList<quint32> touched = ids;
    if (!change(touched))
    {
        m_emptyFolders = st.foldersBefore;
        return false;
    }
    for (quint32 id : touched)
    {
        if (!ids.contains(id))
        {
            State none;
            none.id = id;
            st.before << none;
        }
        st.after << capture(id);
    }
    st.foldersAfter = m_emptyFolders;
    st.stamp = nextUndoStamp();
    m_undo << st;
    while (m_undo.count() > kUndoLimit)
        m_undo.removeFirst();
    m_redo.clear();
    // One action, one Undo: a step here makes the tree's redo stale too.
    if (ShowTreeController *tree = m_doc->showTree())
        tree->clearRedo();
    m_doc->setModified();
    emit changed();
    return true;
}

bool PaletteOps::undo()
{
    if (m_undo.isEmpty())
        return false;
    Step st = m_undo.takeLast();
    if (st.functions)
    {
        applyFunctions(st.functionsBefore);
        m_redo << st;
        emit changed();
        return true;
    }
    if (!st.scenesBefore.isEmpty())
    {
        applyScenes(st.scenesBefore, st.scenesAfter);
        m_redo << st;
        emit changed();
        return true;
    }
    // The palettes first (back in their scenes and slots), then the tree
    // that names them, baked once.
    apply(st.before, st.foldersBefore);
    if (!st.treeBefore.isEmpty())
        if (ShowTreeController *tree = m_doc->showTree())
            tree->restoreOutsideUndoState(st.treeBefore);
    m_redo << st;
    emit changed();
    return true;
}

bool PaletteOps::redo()
{
    if (m_redo.isEmpty())
        return false;
    Step st = m_redo.takeLast();
    if (st.functions)
    {
        applyFunctions(st.functionsAfter);
        m_undo << st;
        emit changed();
        return true;
    }
    if (!st.scenesAfter.isEmpty())
    {
        applyScenes(st.scenesAfter, st.scenesBefore);
        m_undo << st;
        emit changed();
        return true;
    }
    apply(st.after, st.foldersAfter);
    if (!st.treeAfter.isEmpty())
        if (ShowTreeController *tree = m_doc->showTree())
            tree->restoreOutsideUndoState(st.treeAfter);
    m_undo << st;
    emit changed();
    return true;
}

/****************************************************************************
 * Value steps
 ****************************************************************************/

bool PaletteOps::valueStep(const QString &label, quint32 paletteId, const std::function<void()> &change,
                           const QString &mergeKey)
{
    if (m_doc->palette(paletteId) == nullptr)
        return false;
    if (!isManaged(paletteId))
    {
        change();
        return false;
    }
    State before = capture(paletteId);
    before.value = true;
    change();
    State after = capture(paletteId);
    after.value = true;
    if (!after.exists || (after.values == before.values && after.name == before.name
                          && after.colourMacro == before.colourMacro && after.goboName == before.goboName
                          && after.goboValues == before.goboValues))
        return false;

    const qint64 at = QDateTime::currentMSecsSinceEpoch();
    if (!mergeKey.isEmpty() && !m_undo.isEmpty() && m_undo.last().mergeKey == mergeKey
        && at - m_undo.last().mergeAt < 1500 && m_redo.isEmpty()
        && m_undo.last().after.size() == 1 && m_undo.last().after.first().id == paletteId)
    {
        // The same palette, further on: keep where it started.
        Step &top = m_undo.last();
        top.after[0] = after;
        top.mergeAt = at;
        m_doc->setModified();
        emit changed();
        return true;
    }

    Step st;
    st.label = label;
    st.before << before;
    st.after << after;
    st.foldersBefore = st.foldersAfter = m_emptyFolders;
    st.mergeKey = mergeKey;
    st.mergeAt = at;
    st.stamp = nextUndoStamp();
    m_undo << st;
    while (m_undo.count() > kUndoLimit)
        m_undo.removeFirst();
    m_redo.clear();
    if (ShowTreeController *tree = m_doc->showTree())
        tree->clearRedo();
    m_doc->setModified();
    emit changed();
    return true;
}

/****************************************************************************
 * Scene steps
 ****************************************************************************/

/** What a scene saves: its whole state, to tell whether an edit changed it. */
static QByteArray sceneSignature(const Scene *scene)
{
    QByteArray xml;
    if (scene == nullptr)
        return xml;
    QBuffer buf(&xml);
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    scene->saveXML(&w);
    return xml;
}

QSharedPointer<Scene> PaletteOps::sceneSnapshot(quint32 sceneId) const
{
    Scene *scene = qobject_cast<Scene *>(m_doc->function(sceneId));
    if (scene == nullptr)
        return QSharedPointer<Scene>();
    Scene *snap = new Scene(m_doc);
    // Owned by the pointer alone: the Doc would delete it a second time.
    snap->setParent(nullptr);
    snap->copyFrom(scene);
    /* copyFrom() takes the id-less snapshot's name and path too; the id is
       the scene's, so a later step can tell which scene it is. */
    snap->setID(sceneId);
    return QSharedPointer<Scene>(snap);
}

bool PaletteOps::recordSceneStep(const QString &label, const QList<QSharedPointer<Scene>> &before,
                                 const QString &mergeKey)
{
    QList<QPair<quint32, QSharedPointer<Scene>>> was, now;
    for (const QSharedPointer<Scene> &b : before)
    {
        if (b.isNull())
            continue;
        const quint32 id = b->id();
        QSharedPointer<Scene> after = sceneSnapshot(id);
        if (after.isNull() || sceneSignature(b.data()) == sceneSignature(after.data()))
            continue;
        was << qMakePair(id, b);
        now << qMakePair(id, after);
    }
    if (was.isEmpty())
        return false;

    const qint64 at = QDateTime::currentMSecsSinceEpoch();
    if (!mergeKey.isEmpty() && !m_undo.isEmpty() && m_undo.last().mergeKey == mergeKey
        && at - m_undo.last().mergeAt < 1500 && m_redo.isEmpty())
    {
        Step &top = m_undo.last();
        // The same scenes, further on: keep where they started, take where
        // they are now.
        for (const QPair<quint32, QSharedPointer<Scene>> &n : now)
        {
            bool found = false;
            for (QPair<quint32, QSharedPointer<Scene>> &a : top.scenesAfter)
                if (a.first == n.first)
                {
                    a.second = n.second;
                    found = true;
                }
            if (!found)
            {
                for (const QPair<quint32, QSharedPointer<Scene>> &w : was)
                    if (w.first == n.first)
                        top.scenesBefore << w;
                top.scenesAfter << n;
            }
        }
        top.mergeAt = at;
        m_doc->setModified();
        emit changed();
        return true;
    }

    Step st;
    st.label = label;
    st.scenesBefore = was;
    st.scenesAfter = now;
    st.foldersBefore = st.foldersAfter = m_emptyFolders;
    st.mergeKey = mergeKey;
    st.mergeAt = at;
    st.stamp = nextUndoStamp();
    m_undo << st;
    while (m_undo.count() > kUndoLimit)
        m_undo.removeFirst();
    m_redo.clear();
    if (ShowTreeController *tree = m_doc->showTree())
        tree->clearRedo();
    m_doc->setModified();
    emit changed();
    return true;
}

bool PaletteOps::sceneStep(const QString &label, const QList<quint32> &sceneIds,
                           const std::function<bool()> &change)
{
    QList<QSharedPointer<Scene>> before;
    for (quint32 id : sceneIds)
    {
        bool dup = false;
        for (const QSharedPointer<Scene> &b : before)
            dup = dup || b->id() == id;
        if (dup)
            continue;
        QSharedPointer<Scene> s = sceneSnapshot(id);
        if (!s.isNull())
            before << s;
    }
    if (!change())
        return false;
    recordSceneStep(label, before);
    return true;
}

void PaletteOps::applyScenes(const QList<QPair<quint32, QSharedPointer<Scene>>> &states,
                             const QList<QPair<quint32, QSharedPointer<Scene>>> &other)
{
    for (const QPair<quint32, QSharedPointer<Scene>> &s : states)
    {
        Scene *scene = qobject_cast<Scene *>(m_doc->function(s.first));
        if (scene == nullptr || s.second.isNull())
            continue;
        /* The name and folder are put back only when this step changed them
           (a rename): a rename made since, outside this history (the
           Functions tab), stays. */
        QString otherName = s.second->name(), otherPath = s.second->path(true);
        for (const QPair<quint32, QSharedPointer<Scene>> &o : other)
            if (o.first == s.first && !o.second.isNull())
            {
                otherName = o.second->name();
                otherPath = o.second->path(true);
            }
        const QString keepName = scene->name(), keepPath = scene->path(true);
        scene->copyFrom(s.second.data());
        if (otherName == s.second->name())
            scene->setName(keepName);
        if (otherPath == s.second->path(true))
            scene->setPath(keepPath);
        if (scene->isRunning())
            scene->resetRuntime();
    }
    m_doc->setModified();
}

/****************************************************************************
 * Function steps
 ****************************************************************************/

/** What a function saves: its whole state, to tell whether an edit changed it. */
static QByteArray functionSignature(const Function *f)
{
    QByteArray xml;
    if (f == nullptr)
        return xml;
    QBuffer buf(&xml);
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    f->saveXML(&w);
    return xml;
}

QSharedPointer<Function> PaletteOps::functionSnapshot(quint32 id) const
{
    Function *f = m_doc->function(id);
    if (f == nullptr)
        return QSharedPointer<Function>();
    Function *snap = f->createCopy(m_doc, false);
    if (snap == nullptr)
        return QSharedPointer<Function>();
    // Owned by the pointer alone: the Doc would delete it a second time.
    snap->setParent(nullptr);
    snap->setID(id);
    /* Deaf to the show: a collection drops a member the Doc removes, and a
       snapshot that did would forget it -- an Undo deleting a new EFX would
       take it out of the Redo side too. */
    QObject::disconnect(m_doc, nullptr, snap, nullptr);
    return QSharedPointer<Function>(snap);
}

bool PaletteOps::functionStep(const QString &label, const QList<quint32> &functionIds,
                              const std::function<bool(QList<quint32> &made)> &change)
{
    QList<quint32> ids;
    for (quint32 id : functionIds)
        if (!ids.contains(id))
            ids << id;
    QList<QPair<quint32, QSharedPointer<Function>>> was;
    for (quint32 id : ids)
        was << qMakePair(id, functionSnapshot(id));
    QList<quint32> made;
    if (!change(made))
        return false;
    for (quint32 id : made)
        if (!ids.contains(id))
        {
            ids << id;
            was << qMakePair(id, QSharedPointer<Function>());
        }

    Step st;
    st.label = label;
    st.functions = true;
    bool any = false;
    for (const QPair<quint32, QSharedPointer<Function>> &w : was)
    {
        QSharedPointer<Function> now = functionSnapshot(w.first);
        if (w.second.isNull() && now.isNull())
            continue;
        if (!w.second.isNull() && !now.isNull()
            && functionSignature(w.second.data()) == functionSignature(now.data()))
            continue;
        any = true;
        st.functionsBefore << w;
        st.functionsAfter << qMakePair(w.first, now);
    }
    if (!any)
        return false;
    st.foldersBefore = st.foldersAfter = m_emptyFolders;
    st.stamp = nextUndoStamp();
    m_undo << st;
    while (m_undo.count() > kUndoLimit)
        m_undo.removeFirst();
    m_redo.clear();
    if (ShowTreeController *tree = m_doc->showTree())
        tree->clearRedo();
    m_doc->setModified();
    emit changed();
    return true;
}

void PaletteOps::applyFunctions(const QList<QPair<quint32, QSharedPointer<Function>>> &states)
{
    // Made first (what the others name exists), then changed, then deleted
    // (nothing names them any more).
    for (const QPair<quint32, QSharedPointer<Function>> &s : states)
    {
        if (s.second.isNull() || m_doc->function(s.first) != nullptr)
            continue;
        Function *f = s.second->createCopy(m_doc, false);
        if (f == nullptr)
            continue;
        if (!m_doc->addFunction(f, s.first))
            delete f;
        else
            f->setName(s.second->name());       // the copy's own, not a "copy of"
    }
    for (const QPair<quint32, QSharedPointer<Function>> &s : states)
    {
        Function *f = m_doc->function(s.first);
        if (s.second.isNull() || f == nullptr)
            continue;
        if (functionSignature(f) == functionSignature(s.second.data()))
            continue;
        f->copyFrom(s.second.data());
        if (Scene *scene = qobject_cast<Scene *>(f))
            if (scene->isRunning())
                scene->resetRuntime();
        m_doc->notifyFunctionChanged(f->id());
    }
    for (const QPair<quint32, QSharedPointer<Function>> &s : states)
        if (s.second.isNull() && m_doc->function(s.first) != nullptr)
            m_doc->deleteFunction(s.first);
    m_doc->setModified();
}

void PaletteOps::clearUndo()
{
    m_undo.clear();
    m_redo.clear();
}

void PaletteOps::reset()
{
    m_emptyFolders.clear();
    clearUndo();
    emit changed();
}
