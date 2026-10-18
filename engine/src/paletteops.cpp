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
    // As feat/palette-folders' QLCPalette::migrateLegacyPath, read only: a
    // first folder named like a palette type was the old per-type layout,
    // and an effect auto-filed under Effect/<category>/<engine> is unfiled.
    QStringList segs = cleanFolder(path).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segs.isEmpty())
        return QString();
    if (segs.first() == QLatin1String("Palettes"))
        segs.removeFirst();
    if (segs.isEmpty())
        return QString();
    static const QLCPalette::PaletteType kTypes[] = {
        QLCPalette::Dimmer, QLCPalette::Color, QLCPalette::Pan, QLCPalette::Tilt, QLCPalette::PanTilt,
        QLCPalette::Aim, QLCPalette::Shutter, QLCPalette::Strobe, QLCPalette::Gobo, QLCPalette::Zoom,
        QLCPalette::Beam, QLCPalette::Effect };
    bool typeFolder = false;
    for (QLCPalette::PaletteType t : kTypes)
        if (segs.first() == QLCPalette::typeToString(t))
            typeFolder = true;
    if (!typeFolder)
        return segs.join(QLatin1Char('/'));
    static const QStringList kEffectCategories = { "Color", "Dimmer", "Position", "Beam", "Other", "WLED" };
    if (type == QLCPalette::Effect && segs.first() == QLatin1String("Effect") && segs.count() == 3
        && kEffectCategories.contains(segs.at(1)))
        return QString();
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
    return step(label, ids, [&](QList<quint32> &) {
        for (quint32 id : ids)
            m_doc->deletePalette(id);
        return true;
    });
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
    apply(st.before, st.foldersBefore);
    m_redo << st;
    emit changed();
    return true;
}

bool PaletteOps::redo()
{
    if (m_redo.isEmpty())
        return false;
    Step st = m_redo.takeLast();
    apply(st.after, st.foldersAfter);
    m_undo << st;
    emit changed();
    return true;
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
