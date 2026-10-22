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
#include "chaserwalker.h"
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

QString WorkbenchStack::dropPlanOn(int, const QMimeData *, bool) const
{
    return QString();
}

bool WorkbenchStack::dropOn(int, const QMimeData *, bool)
{
    return false;
}

bool WorkbenchStack::showMenu(int, int, const QPoint &)
{
    return false;
}

bool WorkbenchStack::clearAttribute(int, int)
{
    return false;
}

QWidget *WorkbenchStack::lendPane(int, int)
{
    return nullptr;
}

void WorkbenchStack::returnPane(QWidget *)
{
}

QList<quint32> WorkbenchStack::layerFixtures(int) const
{
    return QList<quint32>();
}

int WorkbenchStack::layerOfFixture(quint32) const
{
    return -1;
}

quint32 WorkbenchStack::layerScene(int) const
{
    return Function::invalidId();
}

QWidget *WorkbenchStack::createHeaderWidget(QWidget *)
{
    return nullptr;
}

quint32 WorkbenchStack::makeOwnCopy(int)
{
    return Function::invalidId();
}

bool WorkbenchStack::canMakeOwnCopy(int) const
{
    return false;
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
    if (function() == nullptr)
        return nullptr;
    FunctionStack *s = new FunctionStack(m_doc, m_fid, parent);
    // ↩ to a chaser: the step it was on.
    s->m_step = qBound(0, m_step, qMax(0, stepCount() - 1));
    return s;
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
    {
        lines << tr("%n step(s), one after another", "", ch->stepsCount());
        if (Function *sf = m_doc->function(stepFunction()))
        {
            QString l = tr("Step %1: %2 (%3)").arg(m_step + 1).arg(sf->name(),
                                                                Function::typeToString(sf->type()).toLower());
            if (Collection *c = qobject_cast<Collection *>(sf))
                l += QStringLiteral(" · ") + tr("%n layer(s)", "", int(c->functions().size()));
            lines << l;
        }
    }

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
    // A chaser walked: the step being worked on plays, on its own.
    if (f->type() == Function::ChaserType)
        return stepFunction();
    return m_fid;
}

QString FunctionStack::readOnlyReason() const
{
    ShowTreeController *st = m_doc != nullptr ? m_doc->showTree() : nullptr;
    if (st != nullptr && (st->isGenerated(m_fid)
                          || (chaser() != nullptr && st->isGenerated(stepFunction()))))
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
    // A chaser: the layers of the step walked to (W2).
    Function *f = layerSource();
    if (f == nullptr || f->type() == Function::SequenceType)   // its steps: the overview's editor
        return out;
    const quint32 here = f->id();

    auto layerOf = [this, here](quint32 fid, const QString &prefix) {
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
        const QStringList also = alsoIn(m_doc, fid, here);
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
        Layer l = layerOf(here, QString());
        if (l.kind == Layer::FunctionLayer && here != m_fid)
        {
            // A chaser step that plays as it is: a ▣ row that opens.
            out << l;
            return out;
        }
        if (l.kind == Layer::FunctionLayer)
        {
            // The function itself: its badge is what it is, not "Open".
            l.badge = Function::typeToString(f->type());
            l.tip = tr("Its stock editor is on the right.");
        }
        else if (here == m_fid)
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
    return editCollection() != nullptr && readOnlyReason().isEmpty();
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
    Collection *c = editCollection();
    if (c == nullptr && chaser() != nullptr)
        return QStringLiteral("!") + tr("Step %1 plays one function, not a collection: drop it on the step "
                                        "table to add steps.").arg(m_step + 1);
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
        if (fid == c->id() || contains(m_doc, fid, c->id()))
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
    Collection *c = editCollection();
    if (c == nullptr || addPlan(functionIds).startsWith(QLatin1Char('!')))
        return false;
    QStringList names;
    for (quint32 fid : functionIds)
        if (Function *f = m_doc->function(fid))
            if (!c->functions().contains(fid))
                names << f->name();
    const QString label = names.size() == 1 ? tr("add %1 to %2").arg(names.first(), c->name())
                                            : tr("add %n layer(s) to %1", "", int(names.size())).arg(c->name());
    const bool ok = m_doc->paletteOps()->functionStep(label, { c->id() }, [&](QList<quint32> &) {
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
    Collection *c = editCollection();
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
    const bool ok = m_doc->paletteOps()->functionStep(label, { c->id() }, [&](QList<quint32> &) {
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
    Collection *c = editCollection();
    if (c == nullptr || !canReorderLayers())
        return false;
    const QList<quint32> members = c->functions();
    if (from < 0 || from >= members.size())
        return false;
    to = qBound(0, to, int(members.size()) - 1);
    if (to == from)
        return false;
    const quint32 fid = members.at(from);
    const bool ok = m_doc->paletteOps()->functionStep(tr("reorder layers in %1").arg(c->name()), { c->id() },
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
    Collection *c = editCollection();
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
    const bool ok = m_doc->paletteOps()->functionStep(tr("add a new %1 to %2").arg(what, c->name()), { c->id() },
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

/****************************************************************************
 * FunctionStack, W2: a chaser walked step by step; shared members split off
 ****************************************************************************/

/* A new step of @a ch, timed as the Chaser editor times one it adds: the
   chaser's own times where they are common to every step, nothing where each
   step has its own. */
static ChaserStep newStep(Chaser *ch, quint32 fid)
{
    ChaserStep step(fid);
    if (ch->fadeInMode() == Chaser::Common)
        step.fadeIn = ch->fadeInSpeed();
    if (ch->fadeOutMode() == Chaser::Common)
        step.fadeOut = ch->fadeOutSpeed();
    if (ch->durationMode() == Chaser::Common)
    {
        step.duration = ch->duration();
        step.hold = Function::speedSubtract(step.duration, step.fadeIn);
    }
    return step;
}

Chaser *FunctionStack::chaser() const
{
    // A sequence is a chaser of one scene's values: no steps to walk or
    // add (its editor is on the overview).
    Function *f = function();
    return f != nullptr && f->type() == Function::ChaserType ? qobject_cast<Chaser *>(f) : nullptr;
}

int FunctionStack::stepCount() const
{
    Chaser *ch = chaser();
    return ch != nullptr ? ch->stepsCount() : 0;
}

quint32 FunctionStack::stepFunction() const
{
    Chaser *ch = chaser();
    if (ch == nullptr)
        return Function::invalidId();
    const QList<ChaserStep> steps = ch->steps();
    if (steps.isEmpty())
        return Function::invalidId();
    return steps.at(qBound(0, m_step, int(steps.size()) - 1)).fid;
}

bool FunctionStack::setStep(int index)
{
    if (chaser() == nullptr)
        return false;
    index = qBound(0, index, qMax(0, stepCount() - 1));
    if (index == m_step)
        return false;
    m_step = index;
    emit layersReplaced();
    emit changed();
    return true;
}

Function *FunctionStack::layerSource() const
{
    Function *f = function();
    if (qobject_cast<Chaser *>(f) != nullptr)
        return m_doc->function(stepFunction());
    return f;
}

Collection *FunctionStack::editCollection() const
{
    return qobject_cast<Collection *>(layerSource());
}

QWidget *FunctionStack::createHeaderWidget(QWidget *parent)
{
    if (chaser() == nullptr)
        return nullptr;
    return new ChaserWalker(this, parent);
}

bool FunctionStack::editStep(int index, const QString &label, const std::function<void(ChaserStep &)> &edit)
{
    Chaser *ch = chaser();
    if (ch == nullptr || !readOnlyReason().isEmpty() || index < 0 || index >= ch->stepsCount())
        return false;
    const bool ok = m_doc->paletteOps()->functionStep(label, { m_fid }, [&](QList<quint32> &) {
        ChaserStep st = ch->steps().at(index);
        edit(st);
        return ch->replaceStep(st, index);
    });
    if (ok)
        emit changed();
    return ok;
}

QString FunctionStack::stepsPlan(Doc *doc, quint32 chaserId, const QList<quint32> &functionIds)
{
    Chaser *ch = qobject_cast<Chaser *>(doc != nullptr ? doc->function(chaserId) : nullptr);
    if (ch == nullptr)
        return QStringLiteral("!") + tr("Not a chaser.");
    ShowTreeController *st = doc->showTree();
    if (st != nullptr && st->isGenerated(chaserId))
        return QStringLiteral("!") + tr("Made by the Show Tree: change it there.");
    QStringList names;
    for (quint32 fid : functionIds)
    {
        Function *f = doc->function(fid);
        if (f == nullptr)
            continue;
        if (fid == chaserId || contains(doc, fid, chaserId))
            return QStringLiteral("!") + tr("“%1” holds “%2”: it can't be a step of it.").arg(f->name(), ch->name());
        names << f->name();
    }
    if (names.isEmpty())
        return QStringLiteral("!") + tr("Nothing to add.");
    return names.size() == 1 ? tr("Add “%1” to “%2” as a step").arg(names.first(), ch->name())
                             : tr("Add %n step(s) to “%1”", "", int(names.size())).arg(ch->name());
}

bool FunctionStack::addSteps(Doc *doc, quint32 chaserId, const QList<quint32> &functionIds, int at)
{
    if (stepsPlan(doc, chaserId, functionIds).startsWith(QLatin1Char('!')) || doc->paletteOps() == nullptr)
        return false;
    Chaser *ch = qobject_cast<Chaser *>(doc->function(chaserId));
    QStringList names;
    for (quint32 fid : functionIds)
        if (Function *f = doc->function(fid))
            names << f->name();
    const QString label = names.size() == 1 ? tr("add %1 to %2 as a step").arg(names.first(), ch->name())
                                            : tr("add %n step(s) to %1", "", int(names.size())).arg(ch->name());
    return doc->paletteOps()->functionStep(label, { chaserId }, [&](QList<quint32> &) {
        bool changed = false;
        int pos = at < 0 || at > ch->stepsCount() ? -1 : at;
        for (quint32 fid : functionIds)
        {
            if (doc->function(fid) == nullptr)
                continue;
            if (ch->addStep(newStep(ch, fid), pos))
            {
                changed = true;
                if (pos >= 0)
                    ++pos;
            }
        }
        return changed;
    });
}

bool FunctionStack::insertSteps(const QList<quint32> &functionIds, int at)
{
    const bool ok = addSteps(m_doc, m_fid, functionIds, at);
    if (ok)
        emit changed();
    return ok;
}

bool FunctionStack::removeStep(int index)
{
    Chaser *ch = chaser();
    if (ch == nullptr || !readOnlyReason().isEmpty() || index < 0 || index >= ch->stepsCount())
        return false;
    Function *f = m_doc->function(ch->steps().at(index).fid);
    const bool ok = m_doc->paletteOps()->functionStep(
        tr("remove step %1 (%2) from %3").arg(index + 1).arg(f != nullptr ? f->name() : QString(), ch->name()),
        { m_fid }, [&](QList<quint32> &) { return ch->removeStep(index); });
    if (!ok)
        return false;
    const int was = m_step;
    m_step = qBound(0, m_step, qMax(0, ch->stepsCount() - 1));
    if (index <= was)
        emit layersReplaced();
    emit changed();
    return true;
}

bool FunctionStack::moveStep(int from, int to)
{
    Chaser *ch = chaser();
    if (ch == nullptr || !readOnlyReason().isEmpty())
        return false;
    to = qBound(0, to, ch->stepsCount() - 1);
    if (from < 0 || from >= ch->stepsCount() || from == to)
        return false;
    const bool ok = m_doc->paletteOps()->functionStep(tr("reorder steps in %1").arg(ch->name()), { m_fid },
                                                     [&](QList<quint32> &) { return ch->moveStep(from, to); });
    if (!ok)
        return false;
    // The step walked to goes with it.
    if (m_step == from)
        m_step = to;
    else if (from < m_step && to >= m_step)
        --m_step;
    else if (from > m_step && to <= m_step)
        ++m_step;
    emit changed();
    return true;
}

quint32 FunctionStack::addNewStep()
{
    Chaser *ch = chaser();
    if (ch == nullptr || !readOnlyReason().isEmpty() || m_doc->paletteOps() == nullptr)
        return Function::invalidId();
    const int at = ch->stepsCount() == 0 ? 0 : qMin(m_step + 1, ch->stepsCount());
    quint32 made = Function::invalidId();
    const bool ok = m_doc->paletteOps()->functionStep(tr("add a new scene to %1 as step %2").arg(ch->name()).arg(at + 1),
                                                     { m_fid }, [&](QList<quint32> &news) {
        Scene *s = new Scene(m_doc);
        // Filed with the chaser, as + Scene files one with a collection.
        s->setPath(ch->path(true));
        if (!m_doc->addFunction(s))
        {
            delete s;
            return false;
        }
        s->setName(QStringLiteral("%1 %2").arg(tr("New Scene")).arg(s->id()));
        news << s->id();
        made = s->id();
        ch->addStep(newStep(ch, s->id()), at >= ch->stepsCount() ? -1 : at);
        return true;
    });
    if (!ok)
        return Function::invalidId();
    m_step = at;
    emit layersReplaced();
    emit changed();
    return made;
}

quint32 FunctionStack::makeChaser(Doc *doc, const QList<quint32> &functionIds, const QString &folder)
{
    if (doc == nullptr || doc->paletteOps() == nullptr)
        return Function::invalidId();
    QList<quint32> ids;
    for (quint32 fid : functionIds)
        if (doc->function(fid) != nullptr)
            ids << fid;
    if (ids.isEmpty())
        return Function::invalidId();
    quint32 made = Function::invalidId();
    doc->paletteOps()->functionStep(tr("chaser from %n function(s)", "", int(ids.size())), {},
                                    [&](QList<quint32> &news) {
        Chaser *ch = new Chaser(doc);
        if (!doc->addFunction(ch))
        {
            delete ch;
            return false;
        }
        // As the Function Manager names a new one.
        ch->setName(QStringLiteral("%1 %2").arg(tr("New Chaser")).arg(ch->id()));
        ch->setPath(folder);
        for (quint32 fid : ids)
        {
            ch->addStep(newStep(ch, fid));
        }
        news << ch->id();
        made = ch->id();
        return true;
    });
    return made;
}

quint32 FunctionStack::makeOwnCopy(Doc *doc, quint32 containerId, int index)
{
    if (doc == nullptr || doc->paletteOps() == nullptr)
        return Function::invalidId();
    Function *container = doc->function(containerId);
    Collection *c = qobject_cast<Collection *>(container);
    Chaser *ch = qobject_cast<Chaser *>(container);
    quint32 fid = Function::invalidId();
    if (c != nullptr && index >= 0 && index < c->functions().size())
        fid = c->functions().at(index);
    else if (ch != nullptr && index >= 0 && index < ch->stepsCount())
        fid = ch->steps().at(index).fid;
    Function *src = doc->function(fid);
    ShowTreeController *st = doc->showTree();
    if (src == nullptr || (st != nullptr && st->isGenerated(containerId)))
        return Function::invalidId();

    quint32 made = Function::invalidId();
    doc->paletteOps()->functionStep(tr("make %1's own copy of %2").arg(container->name(), src->name()),
                                    { containerId }, [&](QList<quint32> &news) {
        Function *copy = src->createCopy(doc, true);
        if (copy == nullptr)
            return false;
        copy->setName(doc->nextDuplicateName(src));
        news << copy->id();
        made = copy->id();
        if (c != nullptr)
        {
            // In its place: out, and the copy in at the same index.
            c->removeFunction(fid);
            return c->addFunction(copy->id(), index >= c->functions().size() ? -1 : index);
        }
        ChaserStep step = ch->steps().at(index);
        step.fid = copy->id();
        return ch->replaceStep(step, index);
    });
    return made;
}

bool FunctionStack::canMakeOwnCopy(int index) const
{
    if (!readOnlyReason().isEmpty())
        return false;
    const QList<Layer> ls = layers();
    if (index < 0 || index >= ls.size() || editCollection() == nullptr)
        return false;
    return !alsoIn(m_doc, ls.at(index).functionId, editCollection()->id()).isEmpty();
}

quint32 FunctionStack::makeOwnCopy(int index)
{
    if (!canMakeOwnCopy(index))
        return Function::invalidId();
    const quint32 made = makeOwnCopy(m_doc, editCollection()->id(), index);
    if (made != Function::invalidId())
        emit changed();
    return made;
}

quint32 FunctionStack::makeStepOwnCopy()
{
    if (chaser() == nullptr || !readOnlyReason().isEmpty())
        return Function::invalidId();
    if (alsoIn(m_doc, stepFunction(), Function::invalidId()).size() < 2)
        return Function::invalidId();     // only this step uses it
    const quint32 made = makeOwnCopy(m_doc, m_fid, m_step);
    if (made != Function::invalidId())
    {
        emit layersReplaced();
        emit changed();
    }
    return made;
}
