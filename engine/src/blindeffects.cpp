/*
  Q Light Controller Plus
  blindeffects.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "blindeffects.h"

#include "collection.h"
#include "doc.h"
#include "effectinstance.h"
#include "effectscriptrunner.h"
#include "fixture.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "scene.h"
#include "showtreecontroller.h"

BlindEffects::BlindEffects(Doc *doc)
    : m_doc(doc)
{
}

BlindEffects::~BlindEffects()
{
    clear();
}

void BlindEffects::clear()
{
    qDeleteAll(m_instances);
    m_instances.clear();
    m_signature.clear();
    m_clockMs = 0;
}

void BlindEffects::setCue(quint32 cueId)
{
    QList<quint32> scenes;
    ShowTreeController *st = m_doc != nullptr ? m_doc->showTree() : nullptr;
    const ShowTree::Cue *c = st != nullptr ? st->cue(cueId) : nullptr;
    if (c != nullptr)
        if (const Collection *coll = qobject_cast<const Collection *>(m_doc->function(c->collection)))
            scenes = coll->functions();
    setScenes(scenes);
}

QString BlindEffects::signatureOf(const QList<quint32> &sceneIds) const
{
    QString sig;
    for (quint32 sid : sceneIds)
    {
        Scene *scene = qobject_cast<Scene *>(m_doc->function(sid));
        if (scene == nullptr)
            continue;
        for (quint32 pid : scene->palettes())
        {
            QLCPalette *pal = m_doc->palette(pid);
            if (pal == nullptr || pal->type() != QLCPalette::Effect)
                continue;
            sig += QStringLiteral("%1:%2:%3|").arg(sid).arg(pid).arg(pal->scriptPath());
        }
    }
    return sig;
}

void BlindEffects::setScenes(const QList<quint32> &sceneIds)
{
    const QString sig = m_doc != nullptr ? signatureOf(sceneIds) : QString();
    if (sig == m_signature)
    {
        // The same effects: an edit to their settings shows at once.
        for (EffectInstance *inst : m_instances)
            inst->reloadParamsFromPalette();
        return;
    }
    clear();
    m_signature = sig;
    if (sig.isEmpty())
        return;

    // As EffectScriptRunner::createInstancesForScene() makes them: one
    // instance an effect look, playing every scene of the cue that has it.
    QHash<quint32, EffectInstance *> byPalette;
    for (quint32 sid : sceneIds)
    {
        Scene *scene = qobject_cast<Scene *>(m_doc->function(sid));
        if (scene == nullptr)
            continue;
        for (quint32 pid : scene->palettes())
        {
            QLCPalette *pal = m_doc->palette(pid);
            if (pal == nullptr || pal->type() != QLCPalette::Effect)
                continue;
            if (EffectInstance *existing = byPalette.value(pid))
            {
                existing->addScene(sid);
                continue;
            }
            EffectInstance *inst = new EffectInstance(m_doc, sid, pid);
            if (!inst->isValid())
            {
                delete inst;
                continue;
            }
            inst->setOffStageClock(0);
            byPalette.insert(pid, inst);
            m_instances.append(inst);
        }
    }
    advance(0);
}

void BlindEffects::advance(qint64 ms)
{
    m_clockMs += qMax<qint64>(0, ms);
    for (EffectInstance *inst : m_instances)
    {
        inst->setOffStageClock(m_clockMs);
        inst->runTick();
    }
}

QHash<quint32, QByteArray> BlindEffects::apply(QHash<quint32, QByteArray> levels) const
{
    if (m_instances.isEmpty())
        return levels;
    QList<EffectInstance::DmxWrite> writes;
    for (EffectInstance *inst : m_instances)
        writes += inst->dmxWrites();
    for (const EffectInstance::DmxWrite &w : EffectScriptRunner::composeWrites(writes))
    {
        Fixture *fx = m_doc->fixture(w.fixtureId);
        if (fx == nullptr || w.channel >= fx->channels())
            continue;
        QByteArray &bytes = levels[w.fixtureId];
        if (bytes.size() != int(fx->channels()))
            bytes = QByteArray(int(fx->channels()), 0);
        // As the effect's Override fader lays it on the scene: an
        // intensity the higher, unless the effect owns the channel.
        const QLCChannel *ch = fx->channel(w.channel);
        const bool htp = !w.replace && ch != nullptr && ch->group() == QLCChannel::Intensity;
        const uchar now = uchar(bytes.at(int(w.channel)));
        bytes[int(w.channel)] = char(htp ? qMax(now, w.value) : w.value);
    }
    return levels;
}
