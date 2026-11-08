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
#include "fixturegroup.h"
#include "effectinstance.h"
#include "wledfixture.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"

#include <QSet>

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
        QHash<quint32, QHash<quint32, uchar>> pals;
        for (quint32 pid : ordered)
        {
            QLCPalette *p = doc->palette(pid);
            if (p == nullptr || p->type() == QLCPalette::Effect)
                continue;
            if (QLCPalette::isEffectScoped(doc, ordered, pid))
                continue;
            const int cOff = QLCPalette::colorSetOffset(doc, ordered, pid);
            // Fixtures a lower layer sets this on itself (the Show Tree).
            const QList<quint32> left = sc->excludedFixtures(pid);
            for (const SceneValue &sv : p->valuesFromFixtureGroups(doc, groups, sc, cOff))
                if (!left.contains(sv.fxi))
                    pals[sv.fxi][sv.channel] = sv.value;
            for (const SceneValue &sv : p->valuesFromFixtures(doc, fixtures, sc, cOff))
                if (!left.contains(sv.fxi))
                    pals[sv.fxi][sv.channel] = sv.value;
        }
        // A fixture with no dimmer channel shows the look's Dimmer as its
        // colour scaled, as Scene::write lays it out.
        const int lookDim = QLCPalette::lookDimmerValue(doc, ordered);
        for (auto fx = pals.constBegin(); fx != pals.constEnd(); ++fx)
        {
            Fixture *fxi = doc->fixture(fx.key());
            for (auto c = fx.value().constBegin(); c != fx.value().constEnd(); ++c)
                out[fx.key()][c.key()] = QLCPalette::dimScaleWithoutDimmer(fxi, c.key(), c.value(), lookDim);
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

/*********************************************************************
 * WLED looks
 *********************************************************************/

struct WledLookCache::Entry
{
    std::unique_ptr<EffectInstance> instance;
    // What it was built from: another effect or setting builds it again.
    QString wledEffect;
    QString scriptPath;
    QMap<QString, double> params;
    bool asked = false;
};

WledLookCache::WledLookCache() = default;
WledLookCache::~WledLookCache() = default;

void WledLookCache::beginPass()
{
    for (auto &e : m_entries)
        e->asked = false;
}

void WledLookCache::endPass()
{
    for (auto it = m_entries.begin(); it != m_entries.end();)
        it = it.value()->asked ? std::next(it) : m_entries.erase(it);
}

QHash<quint32, QHash<quint32, uchar>> WledLookCache::settings(Doc *doc, quint32 sceneId, quint32 paletteId)
{
    QHash<quint32, QHash<quint32, uchar>> out;
    QLCPalette *pal = doc != nullptr ? doc->palette(paletteId) : nullptr;
    if (pal == nullptr || pal->type() != QLCPalette::Effect || pal->wledEffect().isEmpty())
        return out;
    const QPair<quint32, quint32> key(sceneId, paletteId);
    std::shared_ptr<Entry> &e = m_entries[key];
    if (e == nullptr || e->wledEffect != pal->wledEffect() || e->scriptPath != pal->scriptPath()
        || e->params != pal->effectParamValues())
    {
        e = std::make_shared<Entry>();
        e->wledEffect = pal->wledEffect();
        e->scriptPath = pal->scriptPath();
        e->params = pal->effectParamValues();
        e->instance.reset(new EffectInstance(doc, sceneId, paletteId));
    }
    e->asked = true;
    // As the runner: a look whose script does not load does not play.
    if (!e->instance->isValid())
        return out;
    // Its colours and fixtures are read from the show as it is now.
    for (const EffectInstance::DmxWrite &w : e->instance->wledDeviceWrites())
        out[w.fixtureId][w.channel] = w.value;
    return out;
}

/** The scenes @p functionId plays, in run order. */
static void scenesOf(Doc *doc, quint32 functionId, QList<Scene *> &out, int depth)
{
    if (depth > 8)
        return;
    Function *f = doc->function(functionId);
    if (f == nullptr)
        return;
    if (Scene *sc = qobject_cast<Scene *>(f))
        out << sc;
    else if (Collection *col = qobject_cast<Collection *>(f))
        for (quint32 mid : col->functions())
            scenesOf(doc, mid, out, depth + 1);
}

QHash<quint32, CueOutput::FixtureCue> CueOutput::computeCues(Doc *doc, quint32 functionId,
                                                            WledLookCache *wled)
{
    QHash<quint32, FixtureCue> result = computeCues(doc, functionId);
    if (doc == nullptr || wled == nullptr)
        return result;

    QList<Scene *> scenes;
    scenesOf(doc, functionId, scenes, 0);
    QSet<quint32> touched;
    for (Scene *sc : std::as_const(scenes))
    {
        // A WLED in the scene: its effect settings at rest under the scene's.
        QList<quint32> on = sc->fixtures();
        for (quint32 gid : sc->fixtureGroups())
            if (FixtureGroup *g = doc->fixtureGroup(gid))
                on << g->fixtureList();
        for (quint32 fid : std::as_const(on))
        {
            const QList<QPair<int, int> > rest = WledFixture::settingsAtRest(doc->fixture(fid));
            if (rest.isEmpty())
                continue;
            FixtureCue &fc = result[fid];
            for (const QPair<int, int> &r : rest)
                if (!fc.nonIntensity.contains(quint32(r.first)))
                    fc.nonIntensity.insert(quint32(r.first), uchar(r.second));
            touched.insert(fid);
        }
        // Its WLED looks: their settings replace, their dimmer is a level.
        for (quint32 pid : sc->palettes())
        {
            const QHash<quint32, QHash<quint32, uchar>> sent = wled->settings(doc, sc->id(), pid);
            for (auto fx = sent.constBegin(); fx != sent.constEnd(); ++fx)
            {
                Fixture *fxi = doc->fixture(fx.key());
                if (fxi == nullptr)
                    continue;
                const quint32 master = fxi->masterIntensityChannel();
                FixtureCue &fc = result[fx.key()];
                for (auto c = fx.value().constBegin(); c != fx.value().constEnd(); ++c)
                {
                    if (master != QLCChannel::invalid() && c.key() == master)
                        fc.intensity = qMax(fc.intensity, c.value());
                    else
                        fc.nonIntensity.insert(c.key(), c.value());
                }
                touched.insert(fx.key());
            }
        }
    }
    for (quint32 fid : std::as_const(touched))
    {
        FixtureCue &fc = result[fid];
        fc.lit = fc.intensity > 0;
    }
    return result;
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
