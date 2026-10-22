/*
  Q Light Controller Plus
  workbenchstack.cpp

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

#include <QSet>
#include <functional>

#include "workbenchstack.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "fixturegroup.h"
#include "paletteops.h"
#include "collection.h"
#include "chaserstep.h"
#include "rgbmatrix.h"
#include "usedby.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "show.h"
#include "efx.h"
#include "doc.h"

/****************************************************************************
 * WorkbenchStack: the defaults say "nothing to edit here"
 ****************************************************************************/

WorkbenchStack::WorkbenchStack(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
}

WorkbenchStack::~WorkbenchStack()
{
}

QString WorkbenchStack::addPlan(const QList<quint32> &) const
{
    return QStringLiteral("!") + tr("Nothing is added here.");
}

bool WorkbenchStack::addLayers(const QList<quint32> &, int)
{
    return false;
}

bool WorkbenchStack::removeLayers(const QList<int> &)
{
    return false;
}

bool WorkbenchStack::moveLayer(int, int)
{
    return false;
}

quint32 WorkbenchStack::addNew(Function::Type, int, const QList<GroupHead> &, quint32)
{
    return Function::invalidId();
}

/****************************************************************************
 * FunctionStack
 ****************************************************************************/

FunctionStack::FunctionStack(Doc *doc, quint32 functionId, QObject *parent)
    : WorkbenchStack(doc, parent)
    , m_fid(functionId)
{
}

Function *FunctionStack::function() const
{
    return m_doc != nullptr ? m_doc->function(m_fid) : nullptr;
}

bool FunctionStack::isCollection() const
{
    return qobject_cast<Collection *>(function()) != nullptr;
}

QString FunctionStack::key() const
{
    return QStringLiteral("f:%1").arg(m_fid);
}

WorkbenchStack *FunctionStack::reopen(QObject *parent) const
{
    return function() != nullptr ? new FunctionStack(m_doc, m_fid, parent) : nullptr;
}

QString FunctionStack::title() const
{
    Function *f = function();
    return f != nullptr ? f->name() : tr("(gone)");
}

QString FunctionStack::kindText() const
{
    Function *f = function();
    return f != nullptr ? Function::typeToString(f->type()) : QString();
}

QString FunctionStack::where() const
{
    Function *f = function();
    return f != nullptr ? f->path(true) : QString();
}

static QString usedInText(Doc *doc, quint32 fid, quint32 here)
{
    const QStringList in = FunctionStack::alsoIn(doc, fid, here);
    if (in.isEmpty())
        return QString();
    QStringList shown = in.mid(0, 4);
    if (in.size() > 4)
        shown << FunctionStack::tr("%n more", "", int(in.size() - 4));
    return shown.join(QStringLiteral(", "));
}

QStringList FunctionStack::infoLines() const
{
    QStringList lines;
    Function *f = function();
    if (f == nullptr)
        return lines;

    if (Scene *s = qobject_cast<Scene *>(f))
    {
        QSet<quint32> fixtures;
        for (quint32 gid : s->fixtureGroups())
            if (FixtureGroup *g = m_doc->fixtureGroup(gid))
                for (quint32 fx : g->fixtureList())
                    fixtures.insert(fx);
        for (quint32 fx : s->fixtures())
            fixtures.insert(fx);
        QString l = tr("%n target(s)", "", int(s->fixtureGroups().size() + s->fixtures().size()))
                    + QStringLiteral(" · ") + tr("%n look(s)", "", int(s->palettes().size()))
                    + QStringLiteral(" · ") + tr("%n fixture(s)", "", int(fixtures.size()));
        if (!s->values().isEmpty())
            l += QStringLiteral(" · ") + tr("▣ %n raw value(s)", "", int(s->values().size()));
        lines << l;
    }
    else if (Collection *c = qobject_cast<Collection *>(f))
        lines << tr("%n layer(s), played together", "", int(c->functions().size()));
    else if (Chaser *ch = qobject_cast<Chaser *>(f))
        lines << tr("%n step(s), one after another", "", ch->stepsCount());

    const QString in = usedInText(m_doc, m_fid, Function::invalidId());
    lines << (in.isEmpty() ? tr("Used in: nothing else") : tr("Used in: %1").arg(in));
    const QStringList vc = UsedBy::vcLines({ m_fid });
    lines << (vc.isEmpty() ? tr("Fired by: nothing on the Virtual Console")
                           : tr("Fired by: %1").arg(vc.join(QStringLiteral(", "))));
    return lines;
}

quint32 FunctionStack::previewFunction() const
{
    Function *f = function();
    // A Show owns its transport (Play / Stop in its timeline): never auto-played.
    if (f == nullptr || f->type() == Function::ShowType)
        return Function::invalidId();
    return m_fid;
}

QString FunctionStack::readOnlyReason() const
{
    ShowTreeController *st = m_doc != nullptr ? m_doc->showTree() : nullptr;
    if (st != nullptr && st->isGenerated(m_fid))
        return tr("Made by the Show Tree: change it there.");
    return QString();
}

QStringList FunctionStack::alsoIn(Doc *doc, quint32 fid, quint32 here)
{
    QStringList out;
    if (doc == nullptr || doc->programmer() == nullptr)
        return out;
    ShowTreeController *st = doc->showTree();
    QSet<quint32> seen;
    for (const ProgrammerController::Usage &u : doc->programmer()->functionUsage(fid))
    {
        if (u.ownerId == here)
            continue;
        const bool generated = st != nullptr && st->isGenerated(u.ownerId);
        QString what = u.ownerName;
        if (u.kind == ProgrammerController::Usage::ChaserStep)
            what = tr("%1 (step %2)").arg(u.ownerName).arg(u.position);
        if (generated)
            what = tr("a Show Tree cue (%1)").arg(u.ownerName);
        if (u.kind != ProgrammerController::Usage::ChaserStep)
        {
            if (seen.contains(u.ownerId))
                continue;
            seen.insert(u.ownerId);
        }
        out << what;
    }
    return out;
}

bool FunctionStack::contains(Doc *doc, quint32 container, quint32 fid)
{
    if (doc == nullptr)
        return false;
    QSet<quint32> visited;
    std::function<bool(quint32)> walk = [&](quint32 id) -> bool {
        if (id == fid)
            return true;
        if (visited.contains(id))
            return false;
        visited.insert(id);
        Function *f = doc->function(id);
        if (Collection *c = qobject_cast<Collection *>(f))
        {
            for (quint32 m : c->functions())
                if (walk(m))
                    return true;
        }
        else if (Chaser *ch = qobject_cast<Chaser *>(f))
        {
            for (const ChaserStep &s : ch->steps())
                if (walk(s.fid))
                    return true;
        }
        return false;
    };
    return walk(container);
}

QList<WorkbenchStack::Layer> FunctionStack::layers() const
{
    QList<Layer> out;
    Function *f = function();
    if (f == nullptr)
        return out;

    auto layerOf = [this](quint32 fid, const QString &prefix) {
        Layer l;
        l.functionId = fid;
        Function *m = m_doc->function(fid);
        l.kind = qobject_cast<Scene *>(m) != nullptr ? Layer::SceneLayer : Layer::FunctionLayer;
        l.label = prefix + (m != nullptr ? m->name() : tr("(missing function %1)").arg(fid));
        QStringList badge, tip;
        if (m != nullptr && l.kind == Layer::FunctionLayer)
        {
            badge << tr("%1 ▣").arg(Function::typeToString(m->type()));
            tip << tr("A %1, played as it is. Open it to work on it.")
                       .arg(Function::typeToString(m->type()).toLower());
        }
        // A member shared with other collections or chasers: an edit changes
        // every one of them (open question 7).
        const QStringList also = alsoIn(m_doc, fid, m_fid);
        if (!also.isEmpty())
        {
            badge << tr("also in %1").arg(also.size());
            tip << tr("Also in %1. An edit here changes it there too.")
                       .arg(also.join(QStringLiteral(", ")));
        }
        l.badge = badge.join(QStringLiteral("  "));
        l.tip = tip.join(QLatin1Char('\n'));
        return l;
    };

    if (Collection *c = qobject_cast<Collection *>(f))
    {
        for (quint32 fid : c->functions())
            out << layerOf(fid, QString());
    }
    else if (Chaser *ch = qobject_cast<Chaser *>(f))
    {
        int n = 0;
        for (const ChaserStep &s : ch->steps())
        {
            Layer l = layerOf(s.fid, tr("%1 · ").arg(++n));
            // A step is opened, not opened up (W2 walks a chaser's steps).
            l.kind = Layer::FunctionLayer;
            if (!s.note.isEmpty())
                l.tip = (l.tip.isEmpty() ? QString() : l.tip + QLatin1Char('\n')) + s.note;
            out << l;
        }
    }
    else
    {
        Layer l = layerOf(m_fid, QString());
        if (l.kind == Layer::FunctionLayer)
        {
            // The function itself: its badge is what it is, not "Open".
            l.badge = Function::typeToString(f->type());
            l.tip = tr("Its stock editor is on the right.");
        }
        else
        {
            l.badge.clear();
            l.tip.clear();
        }
        out << l;
    }
    return out;
}

bool FunctionStack::canAddLayers() const
{
    return isCollection() && readOnlyReason().isEmpty();
}

bool FunctionStack::canRemoveLayers() const
{
    return canAddLayers();
}

bool FunctionStack::canReorderLayers() const
{
    return canAddLayers();
}

QString FunctionStack::addPlan(const QList<quint32> &functionIds) const
{
    Collection *c = qobject_cast<Collection *>(function());
    if (c == nullptr)
        return QStringLiteral("!") + tr("A scene is one layer: load a collection to add layers to it.");
    if (!readOnlyReason().isEmpty())
        return QStringLiteral("!") + readOnlyReason();
    QStringList names;
    for (quint32 fid : functionIds)
    {
        Function *f = m_doc->function(fid);
        if (f == nullptr)
            continue;
        if (fid == m_fid || contains(m_doc, fid, m_fid))
            return QStringLiteral("!") + tr("“%1” holds “%2”: it can't go in it.").arg(f->name(), c->name());
        if (c->functions().contains(fid))
            continue;
        names << f->name();
    }
    if (names.isEmpty())
        return QStringLiteral("!") + tr("Already in “%1”.").arg(c->name());
    return names.size() == 1 ? tr("Add “%1” to “%2” as a layer").arg(names.first(), c->name())
                             : tr("Add %n layer(s) to “%1”", "", int(names.size())).arg(c->name());
}

bool FunctionStack::addLayers(const QList<quint32> &functionIds, int at)
{
    Collection *c = qobject_cast<Collection *>(function());
    if (c == nullptr || addPlan(functionIds).startsWith(QLatin1Char('!')))
        return false;
    QStringList names;
    for (quint32 fid : functionIds)
        if (Function *f = m_doc->function(fid))
            if (!c->functions().contains(fid))
                names << f->name();
    const QString label = names.size() == 1 ? tr("add %1 to %2").arg(names.first(), c->name())
                                            : tr("add %n layer(s) to %1", "", int(names.size())).arg(c->name());
    const bool ok = m_doc->paletteOps()->functionStep(label, { m_fid }, [&](QList<quint32> &) {
        bool changed = false;
        int pos = at < 0 || at > c->functions().size() ? -1 : at;
        for (quint32 fid : functionIds)
        {
            if (m_doc->function(fid) == nullptr || c->functions().contains(fid))
                continue;
            if (c->addFunction(fid, pos))
            {
                changed = true;
                if (pos >= 0)
                    ++pos;
            }
        }
        return changed;
    });
    if (ok)
        emit changed();
    return ok;
}

bool FunctionStack::removeLayers(const QList<int> &indexes)
{
    Collection *c = qobject_cast<Collection *>(function());
    if (c == nullptr || !canRemoveLayers())
        return false;
    const QList<quint32> members = c->functions();
    QList<quint32> going;
    for (int i : indexes)
        if (i >= 0 && i < members.size() && !going.contains(members.at(i)))
            going << members.at(i);
    if (going.isEmpty())
        return false;
    Function *first = m_doc->function(going.first());
    const QString label = going.size() == 1 && first != nullptr
                          ? tr("remove %1 from %2").arg(first->name(), c->name())
                          : tr("remove %n layer(s) from %1", "", int(going.size())).arg(c->name());
    const bool ok = m_doc->paletteOps()->functionStep(label, { m_fid }, [&](QList<quint32> &) {
        bool changed = false;
        for (quint32 fid : going)
            changed = c->removeFunction(fid) || changed;
        return changed;
    });
    if (ok)
        emit changed();
    return ok;
}

bool FunctionStack::moveLayer(int from, int to)
{
    Collection *c = qobject_cast<Collection *>(function());
    if (c == nullptr || !canReorderLayers())
        return false;
    const QList<quint32> members = c->functions();
    if (from < 0 || from >= members.size())
        return false;
    to = qBound(0, to, int(members.size()) - 1);
    if (to == from)
        return false;
    const quint32 fid = members.at(from);
    const bool ok = m_doc->paletteOps()->functionStep(tr("reorder layers in %1").arg(c->name()), { m_fid },
                                                     [&](QList<quint32> &) {
        c->removeFunction(fid);
        return c->addFunction(fid, to);
    });
    if (ok)
        emit changed();
    return ok;
}

quint32 FunctionStack::addNew(Function::Type type, int after, const QList<GroupHead> &heads,
                              quint32 group)
{
    Collection *c = qobject_cast<Collection *>(function());
    if (c == nullptr || !canAddLayers())
        return Function::invalidId();
    QString base, what;
    switch (type)
    {
    case Function::SceneType:     base = tr("New Scene");      what = tr("scene"); break;
    case Function::EFXType:       base = tr("New EFX");        what = tr("EFX"); break;
    case Function::RGBMatrixType: base = tr("New RGB Matrix"); what = tr("RGB Matrix"); break;
    default:
        return Function::invalidId();
    }
    quint32 made = Function::invalidId();
    const bool ok = m_doc->paletteOps()->functionStep(tr("add a new %1 to %2").arg(what, c->name()), { m_fid },
                                                     [&](QList<quint32> &news) {
        Function *f = nullptr;
        if (type == Function::SceneType)
            f = new Scene(m_doc);
        else if (type == Function::EFXType)
        {
            EFX *efx = new EFX(m_doc);
            f = efx;
        }
        else
        {
            RGBMatrix *m = new RGBMatrix(m_doc);
            f = m;
        }
        // Filed with the collection, as the Function Manager files a new one
        // in the folder selected.
        f->setPath(c->path(true));
        if (!m_doc->addFunction(f))
        {
            delete f;
            return false;
        }
        f->setName(QStringLiteral("%1 %2").arg(base).arg(f->id()));
        if (EFX *efx = qobject_cast<EFX *>(f))
        {
            for (const GroupHead &h : heads)
                if (m_doc->fixture(h.fxi) != nullptr)
                    efx->addFixture(h.fxi, h.head);
        }
        else if (RGBMatrix *m = qobject_cast<RGBMatrix *>(f))
        {
            if (m_doc->fixtureGroup(group) != nullptr)
                m->setFixtureGroup(group);
        }
        news << f->id();
        made = f->id();
        const int pos = after < 0 || after + 1 >= c->functions().size() ? -1 : after + 1;
        c->addFunction(f->id(), pos);
        return true;
    });
    if (!ok)
        return Function::invalidId();
    emit changed();
    return made;
}
