/*
  Q Light Controller Plus
  structureduplicate.cpp

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

#include <climits>
#include <algorithm>
#include <QBitArray>
#include <QRegularExpression>
#include <QSet>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "structureduplicate.h"
#include "naturalcompare.h"
#include "inputoutputmap.h"
#include "monitorproperties.h"
#include "patchplanner.h"
#include "fixturegroup.h"
#include "grouphead.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

#define UNI_SIZE 512

namespace
{

/* A structure's facts, whatever its kind. */
struct StructInfo
{
    bool ok = false;
    QString name;
    quint32 layer = 0;
    quint32 group = 0;
    double originX = 0.0;       // metres; platforms only
    double originY = 0.0;
};

StructInfo infoOf(Doc *doc, const QString &key)
{
    StructInfo s;
    MonitorProperties *props = doc->monitorProperties();
    const QString kind = key.section(':', 0, 0);
    const quint32 id = key.section(':', 1, 1).toUInt();
    if (kind == QLatin1String("truss"))
    {
        if (const Truss *t = props->truss(id))
        { s.ok = true; s.name = t->name(); s.layer = t->layerId(); s.group = t->groupId(); }
    }
    else if (kind == QLatin1String("pipe"))
    {
        if (const Pipe *p = props->pipe(id))
        { s.ok = true; s.name = p->name(); s.layer = p->layerId(); s.group = p->groupId(); }
    }
    else if (kind == QLatin1String("tower"))
    {
        if (const Tower *t = props->tower(id))
        { s.ok = true; s.name = t->name(); s.layer = t->layerId(); s.group = t->groupId(); }
    }
    else if (kind == QLatin1String("platform"))
    {
        if (const StagePlatform *p = props->platform(id))
        {
            s.ok = true; s.name = p->name(); s.layer = p->layerId(); s.group = p->groupId();
            s.originX = double(p->originX());
            s.originY = double(p->originY());
        }
    }
    return s;
}

/* The mount a copy gets: the original's, on the target. */
void retarget(FixtureRigProps &rp, const QString &kind, quint32 id)
{
    if (kind == QLatin1String("truss"))
        rp.trussId = id;
    else if (kind == QLatin1String("pipe"))
        rp.pipeId = id;
    else if (kind == QLatin1String("tower"))
        rp.towerId = id;
    else if (rp.onDeck())
        rp.deckPlatformId = id;
    else
        rp.riserPlatformId = id;
}

int firstFree(const QBitArray &used, quint32 n, quint32 from)
{
    quint32 run = 0;
    for (quint32 i = from; i < UNI_SIZE; i++)
    {
        run = used.testBit(int(i)) ? 0 : run + 1;
        if (run == n)
            return int(i + 1 - n);
    }
    return -1;
}

void mark(QBitArray &used, quint32 address, quint32 n)
{
    for (quint32 i = address; i < address + n && i < UNI_SIZE; i++)
        used.setBit(int(i));
}

bool isFree(const QBitArray &used, quint32 address, quint32 n)
{
    if (address + n > UNI_SIZE)
        return false;
    for (quint32 i = address; i < address + n; i++)
        if (used.testBit(int(i)))
            return false;
    return true;
}

/* What a name says of its structure: the name as written, and without the
   separators ("T-1" is also written "T1"). */
QStringList spellings(const QString &structure)
{
    QStringList out;
    if (structure.isEmpty())
        return out;
    out << structure;
    QString compact = structure;
    compact.remove(QRegularExpression(QStringLiteral("[\\s_\\-]")));
    if (!compact.isEmpty() && compact != structure)
        out << compact;
    return out;
}

/* Whether the whole of @p word is in @p text apart from other letters and
   digits ("T1" is in "T1-Spot" and "T1 Wash", not in "T10" or "AT1"). */
QRegularExpression wholeWord(const QString &word)
{
    QString pattern = QRegularExpression::escape(word);
    pattern.prepend(QStringLiteral("(?<![A-Za-z0-9])"));
    pattern.append(word.at(word.size() - 1).isDigit() ? QStringLiteral("(?![0-9])")
                                                       : QStringLiteral("(?![A-Za-z0-9])"));
    // A name written to end in a letter may carry on in more ("US1t1b"): only
    // digits are held apart from a digit.
    return QRegularExpression(pattern);
}

bool namesStructure(const QString &text, const QString &structure)
{
    foreach (const QString &s, spellings(structure))
        if (text.contains(wholeWord(s)))
            return true;
    return false;
}

QString fixtureXml(Fixture *fx)
{
    QByteArray xml;
    QXmlStreamWriter w(&xml);
    w.writeStartDocument();
    fx->saveXML(&w);
    w.writeEndDocument();
    return QString::fromUtf8(xml);
}

bool groupHas(const QList<quint32> &members, const QSet<quint32> &set)
{
    foreach (quint32 f, members)
        if (set.contains(f))
            return true;
    return false;
}

QByteArray groupXml(FixtureGroup *g)
{
    QByteArray xml;
    QXmlStreamWriter w(&xml);
    w.writeStartDocument();
    g->saveXML(&w);
    w.writeEndDocument();
    return xml;
}

/* One fixture to be made, and how it is to be placed. */
struct Planned
{
    StructureDuplicate::Row row;
    int target = 0;             // index into the targets that are done
};

struct Plan
{
    StructureDuplicate::Preview pv;
    QString sourceKind;
    StructInfo source;
    QList<quint32> sourceIds;               // in their order on it
    QStringList targets;                    // the ones that are done, in order
    QList<StructInfo> targetInfo;           // parallel
    QList<int> targetNumber;                // parallel: the 1-based place in the asked list
    QList<quint32> toRemove;                // fixtures Replace takes off
    QList<Planned> planned;
};

void buildPlan(Doc *doc, const QString &source, const QStringList &asked,
               const StructureDuplicate::Options &opt, Plan &plan)
{
    StructureDuplicate::Preview &pv = plan.pv;
    plan.sourceKind = source.section(':', 0, 0);
    plan.source = infoOf(doc, source);
    plan.sourceIds = PatchPlanner::fixturesOn(doc, source);
    if (!plan.source.ok || plan.sourceIds.isEmpty())
    {
        pv.problems << QObject::tr("%1 carries no fixtures to copy")
                           .arg(PatchPlanner::structureName(doc, source));
        return;
    }

    QSet<quint32> remove;
    int number = 0;
    foreach (const QString &t, asked)
    {
        if (t == source || t.section(':', 0, 0) != plan.sourceKind)
            continue;
        ++number;
        const StructInfo ti = infoOf(doc, t);
        if (!ti.ok)
            continue;
        const QList<quint32> there = PatchPlanner::fixturesOn(doc, t);
        if (!there.isEmpty())
        {
            pv.occupied.insert(t, there.size());
            const StructureDuplicate::Existing how = opt.existingFor(t);
            if (how == StructureDuplicate::Skip)
            {
                pv.skipped << t;
                continue;
            }
            if (how == StructureDuplicate::Replace)
            {
                foreach (quint32 f, there)
                {
                    remove.insert(f);
                    plan.toRemove << f;
                }
            }
        }
        plan.targets << t;
        plan.targetInfo << ti;
        plan.targetNumber << number;
    }

    // What scenes lose when Replace takes lights off.
    pv.replacedFixtures = plan.toRemove.size();
    if (!remove.isEmpty())
    {
        foreach (Function *fn, doc->functions())
        {
            Scene *sc = qobject_cast<Scene *>(fn);
            if (sc == nullptr)
                continue;
            foreach (const SceneValue &v, sc->values())
                if (remove.contains(v.fxi))
                {
                    ++pv.replacedInScenes;
                    break;
                }
        }
    }

    /* Names. The show's own pattern: the structure's name in the original's
       name becomes the target's. Names must stay unique among the show's
       lights and the new ones. */
    QSet<QString> removedNames, chosen;
    foreach (quint32 f, plan.toRemove)
        if (Fixture *fx = doc->fixture(f))
            removedNames.insert(fx->name());
    auto taken = [&](const QString &c) {
        return chosen.contains(c) || (doc->fixtureNameTaken(c) && !removedNames.contains(c));
    };

    /* Addresses. What is there now, less what Replace takes off. */
    QHash<quint32, QBitArray> used;
    auto usedIn = [&used](quint32 uni) -> QBitArray & {
        if (!used.contains(uni))
            used[uni] = QBitArray(UNI_SIZE);
        return used[uni];
    };
    foreach (Fixture *fx, doc->fixtures())
        if (fx != nullptr && fx->isPatched() && !remove.contains(fx->id()))
            mark(usedIn(fx->universe()), fx->address(), fx->channels());

    // NextFree begins just after the originals.
    quint32 cursorUni = 0, cursorFrom = 0;
    {
        bool found = false;
        foreach (quint32 f, plan.sourceIds)
        {
            Fixture *fx = doc->fixture(f);
            if (fx == nullptr || !fx->isPatched())
                continue;
            if (!found || fx->universe() < cursorUni)
            {
                cursorUni = fx->universe();
                found = true;
            }
        }
        if (found)
        {
            foreach (quint32 f, plan.sourceIds)
            {
                Fixture *fx = doc->fixture(f);
                if (fx != nullptr && fx->isPatched() && fx->universe() == cursorUni)
                    cursorFrom = qMax(cursorFrom, fx->address() + fx->channels());
            }
        }
    }

    for (int ti = 0; ti < plan.targets.size(); ++ti)
    {
        const QString &t = plan.targets.at(ti);
        const StructInfo &tinfo = plan.targetInfo.at(ti);
        foreach (quint32 sid, plan.sourceIds)
        {
            Fixture *sfx = doc->fixture(sid);
            if (sfx == nullptr)
                continue;
            Planned p;
            p.target = ti;
            StructureDuplicate::Row &r = p.row;
            r.sourceId = sid;
            r.target = t;
            r.channels = sfx->channels();
            r.name = Doc::nextUniqueModelName(
                StructureDuplicate::renamed(sfx->name(), plan.source.name, tinfo.name), taken);
            chosen.insert(r.name);

            if (opt.patch == StructureDuplicate::Unpatched || !sfx->isPatched())
            {
                r.patched = false;
            }
            else if (opt.patch == StructureDuplicate::Offset)
            {
                const qint64 uni = qint64(sfx->universe()) + qint64(plan.targetNumber.at(ti)) * opt.universeStep;
                const qint64 adr = qint64(sfx->address()) + qint64(plan.targetNumber.at(ti)) * opt.addressStep;
                QString why;
                if (uni < 0 || uni >= qint64(InputOutputMap::maxUniverses()))
                    why = QObject::tr("universe %1 is out of range").arg(uni + 1);
                else if (adr < 0 || adr + r.channels > UNI_SIZE)
                    why = QObject::tr("address %1 does not fit in the universe").arg(adr + 1);
                else if (!isFree(usedIn(quint32(uni)), quint32(adr), r.channels))
                    why = QObject::tr("%1.%2 is taken").arg(uni + 1).arg(adr + 1);
                else if (doc->isOnNotOurs(quint32(uni), quint32(adr), r.channels))
                    why = QObject::tr("%1.%2 is kept by the venue").arg(uni + 1).arg(adr + 1);
                if (why.isEmpty())
                {
                    r.patched = true;
                    r.universe = quint32(uni);
                    r.address = quint32(adr);
                    mark(usedIn(r.universe), r.address, r.channels);
                }
                else
                {
                    r.clash = true;
                    r.note = why;
                    pv.problems << QObject::tr("%1: left unpatched (%2)").arg(r.name, why);
                }
            }
            else    // NextFree
            {
                bool placed = false;
                quint32 uni = cursorUni, from = cursorFrom;
                for (int guard = 0; guard < 4096 && uni < InputOutputMap::maxUniverses()
                                    && r.channels > 0 && r.channels <= UNI_SIZE; guard++)
                {
                    const int at = firstFree(usedIn(uni), r.channels, from);
                    if (at < 0)
                    {
                        ++uni;
                        from = 0;
                        continue;
                    }
                    if (doc->isOnNotOurs(uni, quint32(at), r.channels))
                    {
                        from = quint32(at) + 1;
                        continue;
                    }
                    r.patched = true;
                    r.universe = uni;
                    r.address = quint32(at);
                    mark(usedIn(uni), r.address, r.channels);
                    cursorUni = uni;
                    cursorFrom = r.address + r.channels;
                    placed = true;
                    break;
                }
                if (!placed)
                {
                    r.clash = true;
                    r.note = QObject::tr("no free address");
                    pv.problems << QObject::tr("%1: left unpatched (no free address)").arg(r.name);
                }
            }
            pv.rows << r;
            plan.planned << p;
        }
    }

    // The groups, by line.
    MonitorProperties *props = doc->monitorProperties();
    QSet<quint32> sourceSet(plan.sourceIds.begin(), plan.sourceIds.end());
    foreach (FixtureGroup *g, doc->fixtureGroups())
    {
        if (g == nullptr || !groupHas(g->fixtureList(), sourceSet))
            continue;
        const bool parallel = opt.fixtureGroups == StructureDuplicate::ParallelGroups
                              || namesStructure(g->name(), plan.source.name);
        for (int ti = 0; ti < plan.targets.size(); ++ti)
        {
            StructureDuplicate::GroupLine l;
            l.kind = QStringLiteral("fixture group");
            l.source = g->name();
            l.target = plan.targets.at(ti);
            l.parallel = parallel;
            l.name = parallel ? StructureDuplicate::renamed(g->name(), plan.source.name,
                                                            plan.targetInfo.at(ti).name)
                              : g->name();
            foreach (const Planned &p, plan.planned)
                if (p.target == ti && g->fixtureList().contains(p.row.sourceId))
                    ++l.members;
            if (l.members > 0)
                pv.groups << l;
        }
    }
    QSet<quint32> mapSeen;
    foreach (quint32 sid, plan.sourceIds)
    {
        const quint32 gid = props->fixtureProperties(sid).m_baseItem.m_groupId;
        if (gid == 0 || gid == plan.source.group || mapSeen.contains(gid) || !props->hasGroup(gid))
            continue;
        mapSeen.insert(gid);
        const MonitorProperties::MonitorGroup g = props->group(gid);
        if (g.hasFrame || !g.anchorKind.isEmpty())
            continue;           // a frame or an anchored group stays where it is
        const bool parallel = opt.mapGroups == StructureDuplicate::ParallelGroups
                              || namesStructure(g.name, plan.source.name);
        for (int ti = 0; ti < plan.targets.size(); ++ti)
        {
            StructureDuplicate::GroupLine l;
            l.kind = QStringLiteral("map group");
            l.source = g.name;
            l.target = plan.targets.at(ti);
            l.parallel = parallel;
            l.name = parallel ? StructureDuplicate::renamed(g.name, plan.source.name,
                                                            plan.targetInfo.at(ti).name)
                              : g.name;
            foreach (const Planned &p, plan.planned)
                if (p.target == ti
                    && props->fixtureProperties(p.row.sourceId).m_baseItem.m_groupId == gid)
                    ++l.members;
            if (l.members > 0)
                pv.groups << l;
        }
    }
}

} // namespace

/****************************************************************************
 * Names
 ****************************************************************************/

QString StructureDuplicate::renamed(const QString &name, const QString &sourceStructure,
                                    const QString &targetStructure)
{
    const QStringList from = spellings(sourceStructure);
    const QStringList to = spellings(targetStructure);
    for (int i = 0; i < from.size(); ++i)
    {
        const QRegularExpression re = wholeWord(from.at(i));
        if (name.contains(re))
        {
            QString out = name;
            out.replace(re, to.at(qMin(i, to.size() - 1)));
            return out;
        }
    }
    if (targetStructure.isEmpty())
        return name;
    return targetStructure + QLatin1Char(' ') + name;
}

/****************************************************************************
 * Targets, preview
 ****************************************************************************/

QStringList StructureDuplicate::targetsFor(Doc *doc, const QString &key)
{
    QStringList out;
    if (doc == nullptr || PatchPlanner::fixturesOn(doc, key).isEmpty())
        return out;
    MonitorProperties *props = doc->monitorProperties();
    const QString kind = key.section(':', 0, 0);
    auto add = [&out, &key, &kind](quint32 id) {
        const QString k = kind + QLatin1Char(':') + QString::number(id);
        if (k != key)
            out << k;
    };
    if (kind == QLatin1String("truss"))
    {
        foreach (Truss *t, props->trusses())
            if (t != nullptr && !t->isChildBar())
                add(t->id());
    }
    else if (kind == QLatin1String("pipe"))
    {
        foreach (Pipe *p, props->pipes())
            if (p != nullptr)
                add(p->id());
    }
    else if (kind == QLatin1String("tower"))
    {
        foreach (Tower *t, props->towers())
            if (t != nullptr)
                add(t->id());
    }
    else if (kind == QLatin1String("platform"))
    {
        foreach (StagePlatform *p, props->platforms())
            if (p != nullptr)
                add(p->id());
    }
    std::sort(out.begin(), out.end(), [doc](const QString &a, const QString &b) {
        return naturalCompare(PatchPlanner::structureName(doc, a),
                              PatchPlanner::structureName(doc, b)) < 0;
    });
    return out;
}

StructureDuplicate::Preview StructureDuplicate::preview(Doc *doc, const QString &source,
                                                        const QStringList &targets,
                                                        const Options &options)
{
    Plan plan;
    buildPlan(doc, source, targets, options, plan);
    return plan.pv;
}

/****************************************************************************
 * Apply
 ****************************************************************************/

StructureDuplicate::Result StructureDuplicate::apply(Doc *doc, const QString &source,
                                                     const QStringList &targets,
                                                     const Options &options)
{
    Result res;
    Plan plan;
    buildPlan(doc, source, targets, options, plan);
    res.skipped = plan.pv.skipped;
    res.problems = plan.pv.problems;
    if (plan.planned.isEmpty() && plan.toRemove.isEmpty())
        return res;

    MonitorProperties *props = doc->monitorProperties();
    const QString kind = plan.sourceKind;
    Undo &undo = res.undo;

    // The groups as they were: the ones the originals are in, and any a
    // removed light is in.
    QSet<quint32> touched(plan.sourceIds.begin(), plan.sourceIds.end());
    foreach (quint32 f, plan.toRemove)
        touched.insert(f);
    foreach (FixtureGroup *g, doc->fixtureGroups())
        if (g != nullptr && groupHas(g->fixtureList(), touched))
            undo.groupsBefore.append({ g->id(), groupXml(g) });

    // Replace: what is on the target goes first (its addresses and names
    // are free for the copies).
    foreach (quint32 f, plan.toRemove)
    {
        Fixture *fx = doc->fixture(f);
        if (fx == nullptr)
            continue;
        Removed r;
        r.id = f;
        r.xml = fixtureXml(fx).toUtf8();
        r.rig = props->fixtureRigProps(f);
        r.item = props->fixtureProperties(f);
        undo.removed.append(r);
        doc->deleteFixture(f);
        ++res.replaced;
    }

    // The copies.
    QHash<QString, quint32> newIds;             // "<target index>:<source id>" -> new id
    QHash<QString, quint32> mapGroupFor;        // "<target index>:<group id>" -> group
    QList<PatchPlanner::Assignment> assignments;
    QHash<quint32, QString> xmlOf;
    foreach (const Planned &p, plan.planned)
    {
        const StructInfo &ti = plan.targetInfo.at(p.target);
        const quint32 sid = p.row.sourceId;
        Fixture *sfx = doc->fixture(sid);
        if (sfx == nullptr)
            continue;
        if (!xmlOf.contains(sid))
            xmlOf.insert(sid, fixtureXml(sfx));

        QXmlStreamReader r(xmlOf.value(sid));
        while (!r.atEnd() && !r.isStartElement())
            r.readNext();
        Fixture *fx = new Fixture(doc);
        if (!r.isStartElement() || !fx->loadXML(r, doc, doc->fixtureDefCache()))
        {
            delete fx;
            res.problems << QObject::tr("%1: could not be copied").arg(sfx->name());
            continue;
        }
        fx->setPatched(false);
        fx->setAddressClash(false);
        fx->setName(p.row.name);
        if (!doc->addFixture(fx))
        {
            delete fx;
            res.problems << QObject::tr("%1: could not be added").arg(p.row.name);
            continue;
        }
        const quint32 fid = fx->id();
        undo.createdFixtures << fid;
        res.createdIds << fid;
        newIds.insert(QString::number(p.target) + QLatin1Char(':') + QString::number(sid), fid);

        FixtureRigProps rp = props->fixtureRigProps(sid);
        retarget(rp, kind, plan.targets.at(p.target).section(':', 1, 1).toUInt());
        props->setFixtureRigProps(fid, rp);

        FixturePreviewItem item = props->fixtureProperties(sid);
        item.m_baseItem.m_layerId = ti.layer;
        // A light on the structure's own plot group joins the target's.
        quint32 gid = item.m_baseItem.m_groupId;
        if (gid != 0 && gid != plan.source.group && props->hasGroup(gid))
        {
            const MonitorProperties::MonitorGroup g = props->group(gid);
            const bool parallel = !g.hasFrame && g.anchorKind.isEmpty()
                                  && (options.mapGroups == ParallelGroups
                                      || namesStructure(g.name, plan.source.name));
            if (parallel)
            {
                const QString gk = QString::number(p.target) + QLatin1Char(':') + QString::number(gid);
                if (!mapGroupFor.contains(gk))
                {
                    const quint32 nid = props->nextGroupId();
                    props->createGroup(nid, renamed(g.name, plan.source.name, ti.name),
                                       ti.layer, g.parentGroupId);
                    undo.createdMapGroups << nid;
                    mapGroupFor.insert(gk, nid);
                }
                gid = mapGroupFor.value(gk);
            }
        }
        else if (gid != 0 && gid == plan.source.group)
            gid = ti.group;
        item.m_baseItem.m_groupId = gid;
        const bool deck = (rp.primaryMount() == FixtureRigProps::DeckMount);
        if (deck)
        {
            item.m_baseItem.m_position += QVector3D(float((ti.originX - plan.source.originX) * 1000.0),
                                                    float((ti.originY - plan.source.originY) * 1000.0),
                                                    0.0f);
        }
        props->setFixtureProperties(fid, item);
        if (!deck)
        {
            // Where the mount puts it: the plot reads this cache.
            item.m_baseItem.m_position = props->fixtureRigPosition(fid) * 1000.0f;
            props->setFixtureProperties(fid, item);
        }

        if (p.row.patched)
        {
            PatchPlanner::Assignment a;
            a.fixtureId = fid;
            a.universe = p.row.universe;
            a.address = p.row.address;
            a.newUniverse = a.universe >= doc->inputOutputMap()->universesCount();
            assignments << a;
        }
    }
    res.created = int(undo.createdFixtures.size());

    if (!assignments.isEmpty())
    {
        PatchPlanner planner(doc);
        QStringList problems;
        res.patched = planner.apply(assignments, &problems);
        res.problems << problems;
    }

    // The fixture groups.
    foreach (FixtureGroup *g, doc->fixtureGroups())
    {
        if (g == nullptr)
            continue;
        const QList<quint32> members = g->fixtureList();
        if (!groupHas(members, QSet<quint32>(plan.sourceIds.begin(), plan.sourceIds.end())))
            continue;
        const bool parallel = options.fixtureGroups == ParallelGroups
                              || namesStructure(g->name(), plan.source.name);
        const QMap<QLCPoint, GroupHead> heads = g->headsMap();
        QList<FixtureGroup *> made;
        for (int ti = 0; ti < plan.targets.size(); ++ti)
        {
            QList<QPair<quint32, quint32> > pairs;      // source -> copy, in the group
            foreach (quint32 sid, plan.sourceIds)
            {
                const quint32 nid = newIds.value(QString::number(ti) + QLatin1Char(':')
                                                 + QString::number(sid), UINT_MAX);
                if (nid != UINT_MAX && members.contains(sid))
                    pairs << qMakePair(sid, nid);
            }
            if (pairs.isEmpty())
                continue;
            if (!parallel)
            {
                foreach (const auto &pr, pairs)
                    g->assignFixture(pr.second);
                continue;
            }
            FixtureGroup *ng = new FixtureGroup(doc);
            ng->setName(renamed(g->name(), plan.source.name, plan.targetInfo.at(ti).name));
            ng->setPath(g->path());
            ng->setOwner(g->owner());
            ng->setSize(g->size());
            for (auto it = heads.constBegin(); it != heads.constEnd(); ++it)
                foreach (const auto &pr, pairs)
                    if (it.value().fxi == pr.first)
                        ng->assignHead(it.key(), GroupHead(pr.second, it.value().head));
            if (ng->headsMap().isEmpty() || !doc->addFixtureGroup(ng))
            {
                delete ng;
                continue;
            }
            undo.createdFixtureGroups << ng->id();
        }
    }

    doc->setModified();
    return res;
}

/****************************************************************************
 * Undo
 ****************************************************************************/

void StructureDuplicate::undo(Doc *doc, const Undo &u)
{
    if (doc == nullptr)
        return;
    MonitorProperties *props = doc->monitorProperties();

    foreach (quint32 id, u.createdFixtures)
        if (doc->fixture(id) != nullptr)
            doc->deleteFixture(id);
    foreach (quint32 id, u.createdFixtureGroups)
        if (doc->fixtureGroup(id) != nullptr)
            doc->deleteFixtureGroup(id);
    foreach (quint32 id, u.createdMapGroups)
        props->removeGroup(id);

    foreach (const Removed &r, u.removed)
    {
        QXmlStreamReader rd(r.xml);
        while (!rd.atEnd() && !rd.isStartElement())
            rd.readNext();
        Fixture *fx = new Fixture(doc);
        if (!rd.isStartElement() || !fx->loadXML(rd, doc, doc->fixtureDefCache())
            || !doc->addFixture(fx, r.id))
        {
            delete fx;
            continue;
        }
        props->setFixtureRigProps(r.id, r.rig);
        props->setFixtureProperties(r.id, r.item);
    }

    foreach (const GroupState &s, u.groupsBefore)
    {
        if (doc->fixtureGroup(s.id) != nullptr)
            doc->deleteFixtureGroup(s.id);
        QXmlStreamReader rd(s.xml);
        while (!rd.atEnd() && !rd.isStartElement())
            rd.readNext();
        if (rd.isStartElement())
            FixtureGroup::loader(rd, doc);
    }
    doc->setModified();
}
