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
}

void CaptureManager::slotFunctionStopped(quint32 id)
{
    m_running.remove(id);
    m_runningOrder.removeAll(id);
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
    bool anyApplied = false;
    for (const ScenePlan& p : plan)
    {
        if (p.scene == nullptr)
            continue;
        for (const Override& o : p.overrides)
        {
            QPair<quint32, quint32> key = qMakePair(o.fxi, o.channel);
            if (p.chaserDriven.contains(key))
                continue; // skip chaser-animated channels
            p.scene->setValue(SceneValue(o.fxi, o.channel, o.value));
            anyApplied = true;
        }
    }
    if (anyApplied)
    {
        m_doc->setModified();
        emit changesApplied();
    }
    clear();
}

void CaptureManager::saveAsNew(const QList<ScenePlan>& plan,
                               const QHash<quint32, QString>& newNamesById)
{
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

        for (const Override& o : p.overrides)
        {
            QPair<quint32, quint32> key = qMakePair(o.fxi, o.channel);
            if (p.chaserDriven.contains(key))
                continue;
            clone->setValue(SceneValue(o.fxi, o.channel, o.value));
        }

        for (Collection* c : runningCollections)
        {
            QList<quint32> children = c->functions();
            int idx = children.indexOf(p.scene->id());
            if (idx >= 0)
            {
                c->removeFunction(p.scene->id());
                c->addFunction(clone->id(), idx);
            }
        }

        anyApplied = true;
    }

    if (anyApplied)
    {
        m_doc->setModified();
        emit changesApplied();
    }
    clear();
}
