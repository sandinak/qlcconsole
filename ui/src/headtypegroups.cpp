/*
  Q Light Controller Plus
  headtypegroups.cpp

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

#include <QVector3D>
#include <QtMath>
#include <QSet>
#include <algorithm>

#include "createfixturegroup.h"
#include "monitorproperties.h"
#include "headtypesdialog.h"
#include "headtypegroups.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "fixturegroup.h"
#include "headlayout.h"
#include "fixture.h"
#include "doc.h"

HeadTypeGroups::Planned::Planned()
    : sameAs(FixtureGroup::invalidId())
{
}

QString HeadTypeGroups::baseNameFor(Doc *doc, const QList<quint32> &fixtureIds)
{
    const QString base = HeadTypesDialog::suggestName(doc, fixtureIds);
    if (!base.isEmpty())
        return base;
    // Nothing in common: one fixture's own name, else a plain word.
    if (fixtureIds.size() == 1)
    {
        if (const Fixture *fx = doc->fixture(fixtureIds.first()))
            return fx->name();
    }
    return tr("Group");
}

QString HeadTypeGroups::groupName(const QString &base, const QString &type)
{
    const QString t = HeadKinds::typeName(type);
    if (t.isEmpty())
        return base;
    if (base.trimmed().isEmpty())
        return t;
    return QStringLiteral("%1 · %2").arg(base.trimmed(), t);
}

QList<QPair<QString, int> > HeadTypeGroups::typeCounts(Doc *doc, const QList<quint32> &fixtureIds)
{
    QList<QPair<QString, int> > out;
    foreach (quint32 fid, fixtureIds)
    {
        const Fixture *fx = doc->fixture(fid);
        const QLCFixtureMode *mode = fx != nullptr ? fx->fixtureMode() : nullptr;
        if (mode == nullptr)
            continue;
        for (int h = 0; h < mode->heads().size(); ++h)
        {
            const QString t = HeadKinds::typeOf(mode, h);
            int i = 0;
            while (i < out.size() && out.at(i).first != t)
                ++i;
            if (i == out.size())
                out << qMakePair(t, 0);
            out[i].second++;
        }
    }
    return out;
}

/* What a group of @p fixtureIds is named after: what their names share,
   else @p fallback (the whole selection's). */
static QString groupBase(Doc *doc, const QList<quint32> &fixtureIds, const QString &fallback)
{
    const QString b = HeadTypesDialog::suggestName(doc, fixtureIds);
    return b.isEmpty() ? fallback : b;
}

/* The fixtures in the order they are laid out when none of them is on the
   stage: by their place on the 2D plot (top row first, left to right), as
   the one-group creator orders them; the rest as selected, after them. */
static QList<quint32> plotOrder(Doc *doc, const QList<quint32> &fixtureIds)
{
    QList<quint32> ordered = fixtureIds;
    MonitorProperties *mp = doc->monitorProperties();
    if (mp == nullptr)
        return ordered;
    std::stable_sort(ordered.begin(), ordered.end(), [mp](quint32 a, quint32 b) {
        const QVector3D pa = mp->fixturePosition(a, 0, 0);
        const QVector3D pb = mp->fixturePosition(b, 0, 0);
        if (pa.isNull() != pb.isNull()) return !pa.isNull();
        if (pa.isNull()) return false;
        if (qAbs(pa.y() - pb.y()) > 1e-4) return pa.y() > pb.y();
        return pa.x() < pb.x();
    });
    return ordered;
}

/* Lay @p p out: as the fixtures stand on the stage when any of them is
   placed there, else each fixture's heads of the type as a row (its own LED
   grid, for a panel) one under the other -- or, for fixtures of a single head
   each, a near-square of them in plot order. */
static void layOut(Doc *doc, HeadTypeGroups::Planned &p, const HeadTypeGroups::Options &opt)
{
    MonitorProperties *mp = doc->monitorProperties();
    FixtureGroup probe(doc);
    probe.setSize(QSize(1, 1));

    p.onStage = false;
    if (mp != nullptr)
    {
        foreach (quint32 fid, p.fixtures)
            if (!mp->fixtureRigPosition(fid).isNull())
                p.onStage = true;
    }

    if (p.onStage)
    {
        int vertical = -1;
        const QList<CreateFixtureGroup::StageBlock> blocks =
            CreateFixtureGroup::headTypeBlocks(doc, p.fixtures, p.type, &vertical, opt.view);
        CreateFixtureGroup::layoutBlocksByStage(doc, blocks, &probe, vertical, opt.closeGaps);
        if (opt.closeGaps)
            CreateFixtureGroup::closeGaps(&probe);
    }
    else
    {
        /* Seen from the front, as a fixture faces by default: a bar is a row,
           a panel its own grid the right way up. */
        const QList<CreateFixtureGroup::StageBlock> blocks =
            CreateFixtureGroup::headTypeBlocks(doc, plotOrder(doc, p.fixtures), p.type,
                                               nullptr, CreateFixtureGroup::ViewFront);
        bool single = true;
        foreach (const CreateFixtureGroup::StageBlock &b, blocks)
            if (b.heads.size() != 1)
                single = false;
        if (single)
        {
            const int side = qMax(1, int(qCeil(qSqrt(double(blocks.size())))));
            for (int i = 0; i < blocks.size(); ++i)
                probe.assignHead(QLCPoint(i % side, i / side), blocks.at(i).heads.first());
        }
        else
        {
            int row = 0;
            foreach (const CreateFixtureGroup::StageBlock &b, blocks)
            {
                int h = 0;
                QMapIterator<QLCPoint, GroupHead> it(b.heads);
                while (it.hasNext())
                {
                    it.next();
                    probe.assignHead(QLCPoint(it.key().x(), row + it.key().y()), it.value());
                    h = qMax(h, it.key().y() + 1);
                }
                row += h;
            }
        }
        // assignHead() grows the group as it goes; trim it to what is used.
        int w = 1, h = 1;
        foreach (const QLCPoint &pt, probe.headsMap().keys())
        {
            w = qMax(w, pt.x() + 1);
            h = qMax(h, pt.y() + 1);
        }
        probe.restoreState(QSize(w, h), probe.headsMap(), QMap<QLCPoint, quint32>());
    }
    p.size = probe.size();
    p.heads = probe.headsMap();
    if (FixtureGroup *same = CreateFixtureGroup::findSameGroup(doc, p.heads))
        p.sameAs = same->id();
}

QList<HeadTypeGroups::Planned> HeadTypeGroups::plan(Doc *doc, const QList<quint32> &fixtureIds,
                                                    const Options &opt)
{
    QList<Planned> out;
    if (doc == nullptr)
        return out;
    QList<quint32> ids;
    foreach (quint32 fid, fixtureIds)
    {
        const Fixture *fx = doc->fixture(fid);
        if (fx != nullptr && fx->fixtureMode() != nullptr && !ids.contains(fid))
            ids << fid;
    }
    if (ids.isEmpty())
        return out;
    const QString base = opt.baseName.trimmed().isEmpty() ? baseNameFor(doc, ids)
                                                          : opt.baseName.trimmed();

    if (opt.split == OneGroup)
    {
        Planned p;
        p.key = QStringLiteral("all");
        p.name = base;
        p.fixtures = ids;
        out << p;
    }
    else if (opt.split == PerType)
    {
        QStringList types;
        foreach (quint32 fid, ids)
            foreach (const QString &t, HeadKinds::typesOf(doc->fixture(fid)))
                if (!types.contains(t))
                    types << t;
        foreach (const QString &t, types)
        {
            // Each model on its own, in the order they come -- or all together.
            QStringList models;
            QHash<QString, QList<quint32> > byModel;
            foreach (quint32 fid, ids)
            {
                const Fixture *fx = doc->fixture(fid);
                if (!HeadKinds::typesOf(fx).contains(t))
                    continue;
                const QString m = opt.keepModelsApart ? CreateFixtureGroup::fixtureModelKey(fx)
                                                      : QString();
                if (!models.contains(m))
                    models << m;
                byModel[m] << fid;
            }
            foreach (const QString &m, models)
            {
                Planned p;
                p.type = t;
                p.fixtures = byModel.value(m);
                p.key = m.isEmpty() ? t : t + QLatin1Char('|') + m;
                /* Unless a name was given, each is named after its own
                   fixtures: the white heads of bars and strips are "Bar ·
                   White" when only the bars have them. */
                const QString b = opt.baseName.trimmed().isEmpty()
                                      ? groupBase(doc, p.fixtures, base) : base;
                p.name = groupName(b, t);
                out << p;
            }
        }
    }
    else
    {
        foreach (quint32 fid, ids)
        {
            const Fixture *fx = doc->fixture(fid);
            foreach (const QString &t, HeadKinds::typesOf(fx))
            {
                Planned p;
                p.type = t;
                p.fixtures << fid;
                p.key = QStringLiteral("%1|%2").arg(fid).arg(t);
                p.name = groupName(fx->name(), t);
                out << p;
            }
        }
    }

    /* No two the same name: a model's name after the second (two kinds of
       bar both named "Bar"), else a number. */
    QSet<QString> seen;
    for (Planned &p : out)
    {
        if (seen.contains(p.name) && opt.split == PerType && opt.keepModelsApart)
        {
            const Fixture *fx = doc->fixture(p.fixtures.first());
            if (fx != nullptr && fx->fixtureDef() != nullptr)
                p.name = QStringLiteral("%1 (%2)").arg(p.name, fx->fixtureDef()->model());
        }
        const QString stem = p.name;
        for (int n = 2; seen.contains(p.name); ++n)
            p.name = QStringLiteral("%1 %2").arg(stem).arg(n);
        seen.insert(p.name);
        p.path = opt.folder.trimmed();
    }

    for (Planned &p : out)
        layOut(doc, p, opt);
    // A type with no head that could be laid out (none: every head has a type).
    for (int i = out.size() - 1; i >= 0; --i)
        if (out.at(i).heads.isEmpty())
            out.removeAt(i);
    return out;
}

QList<FixtureGroup *> HeadTypeGroups::create(Doc *doc, const QList<Planned> &groups)
{
    QList<FixtureGroup *> made;
    if (doc == nullptr)
        return made;
    foreach (const Planned &p, groups)
    {
        if (p.heads.isEmpty())
            continue;
        FixtureGroup *grp = new FixtureGroup(doc);
        grp->setName(p.name.trimmed().isEmpty() ? HeadKinds::typeName(p.type) : p.name.trimmed());
        grp->setPath(p.path);
        // Complete before Doc announces it, so whoever listens sees it whole.
        grp->restoreState(p.size, p.heads, QMap<QLCPoint, quint32>());
        if (!doc->addFixtureGroup(grp))
        {
            delete grp;
            continue;
        }
        made << grp;
    }
    if (!made.isEmpty())
        doc->setModified();
    return made;
}
