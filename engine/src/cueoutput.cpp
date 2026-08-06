/*
  Q Light Controller Plus
  cueoutput.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "cueoutput.h"

#include "doc.h"
#include "function.h"
#include "scene.h"
#include "collection.h"
#include "qlcpalette.h"
#include "scenevalue.h"
#include "fixture.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"

// Accumulate @p functionId's would-be output into @p out (fixtureId → channel →
// value). Later writes win, matching palette-paramount (Scene) and run order
// (Collection). @p depth guards against a Collection cycle.
static void computeInto(Doc *doc, quint32 functionId,
                        QHash<quint32, QHash<quint32, uchar>> &out, int depth)
{
    if (depth > 8)
        return;
    Function *f = doc->function(functionId);
    if (f == nullptr)
        return;

    if (f->type() == Function::SceneType)
    {
        Scene *sc = qobject_cast<Scene*>(f);
        if (sc == nullptr)
            return;

        // Baked values first.
        for (const SceneValue &sv : sc->values())
            out[sv.fxi][sv.channel] = sv.value;

        // Non-effect, non-effect-scoped palettes override baked values
        // (palette-paramount, matching Scene::writeDMX / buildSceneBaseValues).
        // Effect palettes + colours that feed an effect are DYNAMIC, so a static
        // cue peek excludes them.
        const QList<quint32> groups   = sc->fixtureGroups();
        const QList<quint32> fixtures = sc->fixtures();
        const QList<quint32> ordered  = sc->palettes();
        for (quint32 pid : ordered)
        {
            QLCPalette *p = doc->palette(pid);
            if (p == nullptr || p->type() == QLCPalette::Effect)
                continue;
            if (QLCPalette::isEffectScoped(doc, ordered, pid))
                continue;
            const int cOff = QLCPalette::colorSetOffset(doc, ordered, pid);
            for (const SceneValue &sv : p->valuesFromFixtureGroups(doc, groups, sc, cOff))
                out[sv.fxi][sv.channel] = sv.value;
            for (const SceneValue &sv : p->valuesFromFixtures(doc, fixtures, sc, cOff))
                out[sv.fxi][sv.channel] = sv.value;
        }
    }
    else if (f->type() == Function::CollectionType)
    {
        Collection *col = qobject_cast<Collection*>(f);
        if (col == nullptr)
            return;
        // Run order: later members win on a shared channel.
        for (quint32 mid : col->functions())
            computeInto(doc, mid, out, depth + 1);
    }
    // Other types (Chaser/EFX/RGBMatrix/Show/…) produce time-varying output, not a
    // single static cue state, so they contribute nothing to a look-ahead peek.
}

QHash<quint32, QHash<quint32, uchar>> CueOutput::compute(Doc *doc, quint32 functionId)
{
    QHash<quint32, QHash<quint32, uchar>> out;
    if (doc != nullptr)
        computeInto(doc, functionId, out, 0);
    return out;
}

QHash<quint32, CueOutput::FixtureCue> CueOutput::computeCues(Doc *doc, quint32 functionId)
{
    QHash<quint32, FixtureCue> result;
    const QHash<quint32, QHash<quint32, uchar>> raw = compute(doc, functionId);

    for (auto it = raw.constBegin(); it != raw.constEnd(); ++it)
    {
        Fixture *fxi = doc ? doc->fixture(it.key()) : nullptr;
        if (fxi == nullptr)
            continue;

        FixtureCue fc;
        const quint32 master = fxi->masterIntensityChannel();
        const QHash<quint32, uchar> &chMap = it.value();
        for (auto cit = chMap.constBegin(); cit != chMap.constEnd(); ++cit)
        {
            if (master != QLCChannel::invalid() && cit.key() == master)
                fc.intensity = cit.value();
            else
                fc.nonIntensity.insert(cit.key(), cit.value());
        }
        fc.lit = (fc.intensity > 0);
        result.insert(it.key(), fc);
    }
    return result;
}
