/*
  Q Light Controller Plus
  capturemanager.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QDebug>

#include "capturemanager.h"
#include "chaser.h"
#include "chaserstep.h"
#include "collection.h"
#include "doc.h"
#include "function.h"
#include "mastertimer.h"
#include "scene.h"
#include "scenevalue.h"

CaptureManager::CaptureManager(Doc* doc, QObject* parent)
    : QObject(parent)
    , m_doc(doc)
    , m_capturing(false)
{
    Q_ASSERT(doc != nullptr);

    connect(m_doc->masterTimer(), &MasterTimer::functionStarted,
            this, &CaptureManager::slotFunctionStarted);
    connect(m_doc->masterTimer(), &MasterTimer::functionStopped,
            this, &CaptureManager::slotFunctionStopped);
}

bool CaptureManager::isCapturing() const
{
    return m_capturing;
}

void CaptureManager::setCapturing(bool on)
{
    if (on == m_capturing)
        return;
    m_capturing = on;
    if (!on)
        clear();
    emit capturingChanged(m_capturing);
}

void CaptureManager::recordOverride(quint32 fxi, quint32 channel, uchar value,
                                    const QString& sourceName)
{
    if (!m_capturing)
        return;

    Override o;
    o.fxi = fxi;
    o.channel = channel;
    o.value = value;
    o.sourceName = sourceName;
    m_overrides.insert(qMakePair(fxi, channel), o);

    emit overrideRecorded(fxi, channel, value);
}

void CaptureManager::clear()
{
    m_overrides.clear();
}

int CaptureManager::overrideCount() const
{
    return m_overrides.size();
}

void CaptureManager::slotFunctionStarted(quint32 id)
{
    if (m_running.contains(id))
        return;
    m_running.insert(id);
    m_runningOrder.append(id);

    Function* f = m_doc->function(id);
    if (f != nullptr && f->type() == Function::ChaserType)
    {
        Chaser* chaser = static_cast<Chaser*>(f);
        // Cache initial step scene so we know what was active when the
        // chaser fires currentStepChanged for the first transition.
        m_chaserStepScene.insert(id, chaserCurrentStepSceneId(chaser));
        connect(chaser, &Chaser::currentStepChanged,
                this, &CaptureManager::slotChaserStepChanged,
                Qt::UniqueConnection);
    }
}

void CaptureManager::slotFunctionStopped(quint32 id)
{
    m_running.remove(id);
    m_runningOrder.removeAll(id);

    Function* f = m_doc->function(id);
    if (f != nullptr && f->type() == Function::ChaserType)
    {
        Chaser* chaser = static_cast<Chaser*>(f);
        // Stopping a chaser is also a natural commit point: capture
        // anything tied to its last-active step before the chaser leaves.
        if (m_capturing && !m_overrides.isEmpty())
        {
            quint32 lastSceneId = m_chaserStepScene.value(id, Function::invalidId());
            Function* sf = m_doc->function(lastSceneId);
            if (sf != nullptr && sf->type() == Function::SceneType)
            {
                autoStoreToScene(static_cast<Scene*>(sf),
                                 tr("chaser '%1' stopped").arg(chaser->name()));
            }
        }
        disconnect(chaser, &Chaser::currentStepChanged,
                   this, &CaptureManager::slotChaserStepChanged);
        m_chaserStepScene.remove(id);
    }
}

quint32 CaptureManager::chaserCurrentStepSceneId(Chaser* chaser) const
{
    if (chaser == nullptr)
        return Function::invalidId();
    int idx = chaser->currentStepIndex();
    QList<ChaserStep> steps = chaser->steps();
    if (idx < 0 || idx >= steps.size())
        return Function::invalidId();
    Function* f = m_doc->function(steps.at(idx).fid);
    if (f == nullptr || f->type() != Function::SceneType)
        return Function::invalidId();
    return f->id();
}

void CaptureManager::slotChaserStepChanged(int stepNumber)
{
    Q_UNUSED(stepNumber);
    Chaser* chaser = qobject_cast<Chaser*>(sender());
    if (chaser == nullptr)
        return;

    quint32 oldSceneId = m_chaserStepScene.value(chaser->id(),
                                                 Function::invalidId());
    quint32 newSceneId = chaserCurrentStepSceneId(chaser);
    m_chaserStepScene[chaser->id()] = newSceneId;

    if (!m_capturing || m_overrides.isEmpty())
        return;
    if (oldSceneId == Function::invalidId() || oldSceneId == newSceneId)
        return;

    Function* f = m_doc->function(oldSceneId);
    if (f == nullptr || f->type() != Function::SceneType)
        return;

    autoStoreToScene(static_cast<Scene*>(f),
                     tr("chaser '%1' advanced").arg(chaser->name()));
}

void CaptureManager::autoStoreToScene(Scene* target, const QString& reason)
{
    if (target == nullptr)
        return;

    // Build the (fxi, channel) set the target Scene asserts.
    QSet<QPair<quint32, quint32> > targetChannels;
    for (const SceneValue& sv : target->values())
        targetChannels.insert(qMakePair(sv.fxi, sv.channel));

    // Collect overrides that match channels owned by the target.
    QList<Override> matched;
    QList<QPair<quint32, quint32> > matchedKeys;
    for (auto it = m_overrides.constBegin(); it != m_overrides.constEnd(); ++it)
    {
        if (targetChannels.contains(it.key()))
        {
            matched.append(it.value());
            matchedKeys.append(it.key());
        }
    }
    if (matched.isEmpty())
        return;

    // Snapshot for undo before mutating.
    UndoEntry undo;
    undo.label = tr("Auto-store");
    undo.sceneCount = 1;
    undo.channelCount = matched.size();
    undo.priorSceneValues.insert(target->id(), target->values());

    for (const Override& o : matched)
        target->setValue(SceneValue(o.fxi, o.channel, o.value));

    m_undoStack.append(undo);
    while (m_undoStack.size() > MAX_UNDO_DEPTH)
        m_undoStack.removeFirst();

    // Remove only the keys we just consumed; leave other in-flight
    // captures alone so a later manual Store still sees them.
    for (const auto& k : matchedKeys)
        m_overrides.remove(k);

    m_doc->setModified();
    emit changesApplied();
    emit undoStackChanged();

    QString summary = tr("Auto-stored %1 channel(s) into '%2' (%3)")
                          .arg(matched.size()).arg(target->name()).arg(reason);
    emit autoStored(summary);
}

QList<CaptureManager::ScenePlan> CaptureManager::buildPlan() const
{
    QList<ScenePlan> result;
    if (m_overrides.isEmpty())
        return result;

    // Walk running scenes in start order so the latest start is processed
    // last and wins LTP for any (fxi, channel) it owns.
    QList<Scene*> runningScenes;
    QSet<quint32> chaserDrivenSceneIds;

    for (quint32 fid : m_runningOrder)
    {
        Function* f = m_doc->function(fid);
        if (f == nullptr)
            continue;

        if (f->type() == Function::SceneType)
        {
            runningScenes.append(static_cast<Scene*>(f));
        }
        else if (f->type() == Function::ChaserType)
        {
            Chaser* chaser = static_cast<Chaser*>(f);
            int idx = chaser->currentStepIndex();
            QList<ChaserStep> steps = chaser->steps();
            if (idx >= 0 && idx < steps.size())
            {
                Function* stepFn = m_doc->function(steps.at(idx).fid);
                if (stepFn != nullptr && stepFn->type() == Function::SceneType)
                    chaserDrivenSceneIds.insert(stepFn->id());
            }
        }
    }

    // Build (fxi, channel) -> list of running scenes that contain it
    QHash<QPair<quint32, quint32>, QList<Scene*> > containing;
    for (Scene* s : runningScenes)
    {
        for (const SceneValue& sv : s->values())
            containing[qMakePair(sv.fxi, sv.channel)].append(s);
    }

    // Channels currently driven by a chaser step (excluded from apply)
    QSet<QPair<quint32, quint32> > chaserChannels;
    for (quint32 sceneId : chaserDrivenSceneIds)
    {
        Function* f = m_doc->function(sceneId);
        if (f == nullptr)
            continue;
        Scene* s = static_cast<Scene*>(f);
        for (const SceneValue& sv : s->values())
            chaserChannels.insert(qMakePair(sv.fxi, sv.channel));
    }

    QHash<Scene*, ScenePlan> bySceneTmp;

    for (auto it = m_overrides.constBegin(); it != m_overrides.constEnd(); ++it)
    {
        const QPair<quint32, quint32>& key = it.key();
        const Override& o = it.value();

        const QList<Scene*>& candidates = containing.value(key);
        if (candidates.isEmpty())
            continue; // no running scene asserts this channel; ignore

        // LTP winner: the candidate that started most recently. Since
        // runningScenes is in start order and we appended candidates in
        // the same iteration, candidates.last() is the LTP owner.
        Scene* owner = candidates.last();

        ScenePlan& plan = bySceneTmp[owner];
        plan.scene = owner;
        plan.overrides.append(o);
        if (candidates.size() > 1)
            plan.conflicts.insert(key);
        if (chaserChannels.contains(key))
            plan.chaserDriven.insert(key);
    }

    // Order plans by Scene name for deterministic UI
    QList<Scene*> orderedScenes = bySceneTmp.keys();
    std::sort(orderedScenes.begin(), orderedScenes.end(),
              [](Scene* a, Scene* b) { return a->name() < b->name(); });
    for (Scene* s : orderedScenes)
        result.append(bySceneTmp.value(s));

    return result;
}

void CaptureManager::applyInPlace(const QList<ScenePlan>& plan)
{
    UndoEntry undo;
    undo.label = tr("Apply");
    undo.sceneCount = 0;
    undo.channelCount = 0;

    bool anyApplied = false;
    for (const ScenePlan& p : plan)
    {
        if (p.scene == nullptr)
            continue;

        // Snapshot prior values before mutating.
        if (!undo.priorSceneValues.contains(p.scene->id()))
        {
            undo.priorSceneValues.insert(p.scene->id(), p.scene->values());
            undo.sceneCount++;
        }

        for (const Override& o : p.overrides)
        {
            QPair<quint32, quint32> key = qMakePair(o.fxi, o.channel);
            if (p.chaserDriven.contains(key))
                continue; // skip chaser-animated channels
            p.scene->setValue(SceneValue(o.fxi, o.channel, o.value));
            undo.channelCount++;
            anyApplied = true;
        }
    }
    if (anyApplied)
    {
        // Push undo entry, capping depth.
        m_undoStack.append(undo);
        while (m_undoStack.size() > MAX_UNDO_DEPTH)
            m_undoStack.removeFirst();

        m_doc->setModified();
        emit changesApplied();
        emit undoStackChanged();
    }
    clear();
}

void CaptureManager::saveAsNew(const QList<ScenePlan>& plan,
                               const QHash<quint32, QString>& newNamesById)
{
    UndoEntry undo;
    undo.label = tr("Save as new");
    undo.sceneCount = 0;
    undo.channelCount = 0;

    bool anyApplied = false;

    // Collect running Collections so we can rewire their child lists if
    // they referenced the original Scene ids.
    QList<Collection*> runningCollections;
    for (quint32 fid : m_running)
    {
        Function* f = m_doc->function(fid);
        if (f != nullptr && f->type() == Function::CollectionType)
            runningCollections.append(static_cast<Collection*>(f));
    }

    for (const ScenePlan& p : plan)
    {
        if (p.scene == nullptr)
            continue;

        QString newName = newNamesById.value(p.scene->id(),
                                             p.scene->name() + tr(" (Edit)"));

        Function* copy = p.scene->createCopy(m_doc, true);
        if (copy == nullptr)
            continue;
        copy->setName(newName);
        copy->setPath(p.scene->path(true));
        Scene* clone = static_cast<Scene*>(copy);
        undo.createdSceneIds.append(clone->id());
        undo.sceneCount++;

        for (const Override& o : p.overrides)
        {
            QPair<quint32, quint32> key = qMakePair(o.fxi, o.channel);
            if (p.chaserDriven.contains(key))
                continue;
            clone->setValue(SceneValue(o.fxi, o.channel, o.value));
            undo.channelCount++;
        }

        for (Collection* c : runningCollections)
        {
            QList<quint32> children = c->functions();
            int idx = children.indexOf(p.scene->id());
            if (idx >= 0)
            {
                if (!undo.priorCollectionChildren.contains(c->id()))
                    undo.priorCollectionChildren.insert(c->id(), children);
                c->removeFunction(p.scene->id());
                c->addFunction(clone->id(), idx);
            }
        }

        anyApplied = true;
    }

    if (anyApplied)
    {
        m_undoStack.append(undo);
        while (m_undoStack.size() > MAX_UNDO_DEPTH)
            m_undoStack.removeFirst();

        m_doc->setModified();
        emit changesApplied();
        emit undoStackChanged();
    }
    clear();
}

int CaptureManager::impactedSceneCount() const
{
    if (m_overrides.isEmpty())
        return 0;
    QList<ScenePlan> plan = buildPlan();
    return plan.size();
}

bool CaptureManager::canUndo() const
{
    return !m_undoStack.isEmpty();
}

QString CaptureManager::lastUndoLabel() const
{
    if (m_undoStack.isEmpty())
        return QString();
    return m_undoStack.last().label;
}

void CaptureManager::undoLast()
{
    if (m_undoStack.isEmpty())
        return;

    UndoEntry e = m_undoStack.takeLast();

    // Restore Scene values: clear current values, set the snapshot back.
    for (auto it = e.priorSceneValues.constBegin();
         it != e.priorSceneValues.constEnd(); ++it)
    {
        Function* f = m_doc->function(it.key());
        if (f == nullptr || f->type() != Function::SceneType)
            continue;
        Scene* s = static_cast<Scene*>(f);

        // Wipe current values, then re-set from the snapshot. There's no
        // bulk replace API on Scene, so iterate.
        QList<SceneValue> current = s->values();
        for (const SceneValue& sv : current)
            s->unsetValue(sv.fxi, sv.channel);
        for (const SceneValue& sv : it.value())
            s->setValue(sv);
    }

    // Restore Collection child lists (save-as-new path).
    for (auto it = e.priorCollectionChildren.constBegin();
         it != e.priorCollectionChildren.constEnd(); ++it)
    {
        Function* f = m_doc->function(it.key());
        if (f == nullptr || f->type() != Function::CollectionType)
            continue;
        Collection* c = static_cast<Collection*>(f);
        QList<quint32> currentChildren = c->functions();
        for (quint32 child : currentChildren)
            c->removeFunction(child);
        const QList<quint32>& priorChildren = it.value();
        for (int i = 0; i < priorChildren.size(); ++i)
            c->addFunction(priorChildren.at(i), i);
    }

    // Delete clones created by save-as-new.
    for (quint32 cloneId : e.createdSceneIds)
        m_doc->deleteFunction(cloneId);

    // In-flight captures may be from before the undo and would re-assert
    // values we just reverted. Drop them; user can re-edit if they meant
    // to keep the look.
    clear();

    m_doc->setModified();

    QString summary = tr("Undid %1 (%2 scene(s), %3 channel(s) restored)")
                          .arg(e.label).arg(e.sceneCount).arg(e.channelCount);
    emit undoPerformed(summary);
    emit undoStackChanged();
}
