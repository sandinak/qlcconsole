/*
  Q Light Controller Plus
  effectscriptrunner.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "effectscriptrunner.h"
#include "effectinstance.h"

#include "doc.h"
#include "scene.h"
#include "qlcpalette.h"
#include "mastertimer.h"
#include "inputoutputmap.h"
#include "universe.h"
#include "fadechannel.h"

#include <QCoreApplication>
#include <QDebug>

static const int TICK_INTERVAL_MS = 20; // ~50 Hz, matches MasterTimer default

EffectScriptRunner::EffectScriptRunner(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
    m_cache.rescan();
    m_presetCache.rescan();

    // Drive tick() on the main thread. Use a precise timer so the effect
    // actuates promptly and doesn't stutter/lag when the GUI event loop is busy
    // (a coarse timer can drift by tens of ms under load, which reads as the
    // flicker "starting late" or freezing while the window has focus).
    connect(&m_tickTimer, &QTimer::timeout,
            this, &EffectScriptRunner::slotTick);
    m_tickTimer.setTimerType(Qt::PreciseTimer);
    m_tickTimer.setInterval(TICK_INTERVAL_MS);
    m_tickTimer.start();

    // Destroy all JS engines before Qt's object tree tears down — QJSEngine
    // V4 GC teardown is expensive and blocks the main thread for seconds.
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
            this, &EffectScriptRunner::slotPrepareQuit);

    // Subscribe to all input value changes
    if (m_doc->inputOutputMap())
    {
        connect(m_doc->inputOutputMap(),
                SIGNAL(inputValueChanged(quint32, quint32, uchar, const QString&)),
                this,
                SLOT(slotInputValueChanged(quint32, quint32, uchar, const QString&)));
    }
}

EffectScriptRunner::~EffectScriptRunner()
{
    // slotPrepareQuit() should have already cleaned up via aboutToQuit.
    // This is a safety net for cases where the runner is destroyed without
    // the application quitting (unit tests, etc.).
    slotPrepareQuit();
}

void EffectScriptRunner::slotPrepareQuit()
{
    m_tickTimer.stop();

    if (m_registered && m_doc && m_doc->masterTimer())
    {
        m_doc->masterTimer()->unregisterDMXSource(this);
        m_registered = false;
    }

    m_faders.clear(); // faders dismissed via universe destructor during shutdown

    {
        QMutexLocker locker(&m_instanceMutex);
        qDeleteAll(m_instances);
        m_instances.clear();
    }

    QMutexLocker locker(&m_writesMutex);
    m_publishedWrites.clear();
}

// ---------------------------------------------------------------------------
// Scene lifecycle
// ---------------------------------------------------------------------------

void EffectScriptRunner::slotFunctionStarted(quint32 fid)
{
    Function *f = m_doc->function(fid);
    if (!f || f->type() != Function::SceneType)
        return;
    createInstancesForScene(fid);
}

void EffectScriptRunner::slotFunctionStopped(quint32 fid)
{
    // Deliberately NOT gated on Doc::function(fid) still resolving. This slot
    // is queued, so by the time it runs the function may already be gone from
    // the Doc (workspace cleared, or the scene deleted while running) — and an
    // early return there would strand the instance forever: it keeps ticking,
    // keeps re-asserting its last frame into an Override-priority fader, and
    // leaks a whole QJSEngine. destroyInstancesForScene() is keyed on the
    // instance's own sceneId, so it needs no Doc lookup and is a no-op for ids
    // that own no instances.
    destroyInstancesForScene(fid);
}

void EffectScriptRunner::slotFunctionRemoved(quint32 fid)
{
    // Doc::deleteFunction() removes the function from the MasterTimer's list
    // WITHOUT emitting functionStopped, so deleting a running scene would
    // otherwise never tear its effect instances down.
    destroyInstancesForScene(fid);
}

void EffectScriptRunner::slotDocCleared()
{
    // File > New / Open: every scene is about to be deleted. Drop all
    // instances now rather than waiting for stop signals that won't arrive.
    QList<quint32> sceneIds;
    {
        QMutexLocker locker(&m_instanceMutex);
        for (const EffectInstance *inst : m_instances)
            if (!sceneIds.contains(inst->sceneId()))
                sceneIds.append(inst->sceneId());
    }
    for (quint32 sid : sceneIds)
        destroyInstancesForScene(sid);
}

void EffectScriptRunner::createInstancesForScene(quint32 sceneId)
{
    Scene *scene = qobject_cast<Scene*>(m_doc->function(sceneId));
    if (!scene)
        return;

    for (quint32 pid : scene->palettes())
    {
        QLCPalette *pal = m_doc->palette(pid);
        if (!pal || pal->type() != QLCPalette::Effect)
            continue;

        EffectInstance *inst = new EffectInstance(m_doc, sceneId, pid);
        if (!inst->isValid())
        {
            qWarning() << "[EffectScriptRunner] instance invalid for palette" << pid;
            delete inst;
            continue;
        }

        {
            QMutexLocker locker(&m_instanceMutex);
            m_instances.append(inst);
        }

        // Register DMXSource on first instance
        if (!m_registered && m_doc->masterTimer())
        {
            m_doc->masterTimer()->registerDMXSource(this);
            m_registered = true;
        }

    }
}

void EffectScriptRunner::syncScene(quint32 sceneId)
{
    // Used when a running scene's palette list changes (no stop/start fired).
    destroyInstancesForScene(sceneId);
    createInstancesForScene(sceneId);
}

void EffectScriptRunner::destroyInstancesForScene(quint32 sceneId)
{
    // Deadlock prevention: unregisterDMXSource acquires m_dmxSourceListMutex,
    // but the timer thread holds m_dmxSourceListMutex while calling writeDMX.
    // Collect cleanup work under the lock, then release before touching
    // MasterTimer or universes.
    bool shouldUnregister = false;

    {
        QMutexLocker locker(&m_instanceMutex);
        QMutableListIterator<EffectInstance*> it(m_instances);
        while (it.hasNext())
        {
            EffectInstance *inst = it.next();
            if (inst->sceneId() == sceneId)
            {
                delete inst;
                it.remove();
            }
        }

        if (m_instances.isEmpty() && m_registered)
        {
            shouldUnregister = true;
            m_registered = false;
        }
    } // release m_instanceMutex before touching MasterTimer

    // Republish immediately: the destroyed instances' writes are still sitting in
    // m_publishedWrites, and writeDMX() would keep asserting that dead frame until
    // the next tick republished without them.
    publishWrites();

    if (shouldUnregister)
    {
        if (m_doc->masterTimer())
            m_doc->masterTimer()->unregisterDMXSource(this);

        // Dismiss all per-universe faders so no stale values linger.
        QList<Universe*> ua = m_doc->inputOutputMap()->claimUniverses();
        for (auto it2 = m_faders.begin(); it2 != m_faders.end(); ++it2)
        {
            int idx = it2.key();
            if (!it2.value().isNull() && idx >= 0 && idx < ua.size())
                ua.at(idx)->dismissFader(it2.value());
        }
        m_doc->inputOutputMap()->releaseUniverses(false);
        m_faders.clear();
    }
}

// ---------------------------------------------------------------------------
// Main-thread tick
// ---------------------------------------------------------------------------

void EffectScriptRunner::slotTick()
{
    // Effect scripts run in BOTH modes: live in Operate, and on the previewed
    // scene in Edit so the operator can fly the beam (joystick) while building.
    // The previewed scene is started via Function::start() in Design too, so it
    // has live instances here.  The beam position carries continuously across an
    // Edit↔Run switch via m_lastSpotDeg (followMode = "lastPosition").
    // NOTE: no lock is held across runTick(). m_instances is main-thread-only
    // (see the header) and writeDMX() no longer reads it, so the MasterTimer
    // thread cannot be blocked by script evaluation here.
    QMutexLocker locker(&m_instanceMutex);
    for (EffectInstance *inst : m_instances)
    {
        inst->setLastSpotPositions(m_lastSpotDeg);
        inst->setDataChannels(m_dataChannels);
        inst->runTick();

        // Capture the pan/tilt the script just produced so a later scene (whose
        // followspot uses followMode = "lastPosition") can resume from where the
        // beam left off — even after this instance is destroyed on scene stop.
        const QHash<quint32, QPointF> &deg = inst->lastIntentDegrees();
        for (auto it = deg.constBegin(); it != deg.constEnd(); ++it)
            m_lastSpotDeg[it.key()] = it.value();
    }
    locker.unlock();

    publishWrites();
}

void EffectScriptRunner::publishWrites()
{
    QList<EffectInstance::DmxWrite> combined;
    {
        QMutexLocker locker(&m_instanceMutex);
        for (const EffectInstance *inst : m_instances)
            combined += inst->dmxWrites();
    }

    QMutexLocker locker(&m_writesMutex);
    m_publishedWrites = combined;
}

void EffectScriptRunner::setDataChannel(const QString &name, const QVariantMap &data)
{
    m_dataChannels[name] = data;
}

// ---------------------------------------------------------------------------
// Input routing
// ---------------------------------------------------------------------------

void EffectScriptRunner::slotInputValueChanged(quint32 universe, quint32 channel,
                                                uchar value, const QString &key)
{
    Q_UNUSED(key)

    const float norm = value / 255.0f;

    QMutexLocker locker(&m_instanceMutex);
    for (EffectInstance *inst : m_instances)
    {
        QLCPalette *pal = m_doc->palette(inst->effectPaletteId());
        if (!pal) continue;

        const QMap<QString, QPair<quint32,quint32>> &binds = pal->effectInputBindings();
        for (auto it = binds.constBegin(); it != binds.constEnd(); ++it)
        {
            if (it.value().first == universe && it.value().second == channel)
                inst->setInputValue(it.key(), norm);
        }
    }
}

// ---------------------------------------------------------------------------
// DMXSource — timer thread
// ---------------------------------------------------------------------------

void EffectScriptRunner::writeDMX(MasterTimer *timer, QList<Universe*> universes)
{
    Q_UNUSED(timer)

    // Effect scripts write in both modes — live in Operate, and on the previewed
    // scene in Edit (so the joystick can fly the beam while building).

    // Clear all faders so channels from the previous tick don't linger when
    // the effect stops writing them (e.g. fixture removed from scene).
    for (auto &fader : m_faders)
        if (!fader.isNull()) fader->removeAll();

    // Take a copy of the last published tick and release immediately. This mutex
    // is NEVER held across script evaluation (that is the whole reason it exists
    // separately from m_instanceMutex), so this can't stall the DMX thread.
    QList<EffectInstance::DmxWrite> writes;
    {
        QMutexLocker locker(&m_writesMutex);
        writes = m_publishedWrites;
    }

    for (const auto &w : writes)
    {
        if (w.universeId < 0 || w.universeId >= universes.size())
            continue;
        Universe *uni = universes.at(w.universeId);
        if (!uni)
            continue;

        // Get or create a GenericFader for this universe. Using a fader
        // instead of uni->write() ensures that processFaders() writes our
        // values AFTER zeroIntensityChannels() has run — without this,
        // the RGB writes are wiped before they reach the DMX output.
        //
        // Priority is Override (not Auto): an effect palette is meant to
        // layer ON TOP of the scene it's attached to.  At equal (Auto)
        // priority the winner is decided by fader insertion order, which is
        // fragile across scene transitions — the scene's Aim/PanTilt palette
        // could win and the effect's pan/tilt would be silently overridden
        // (followspot couldn't move the beam).  Override guarantees the
        // effect fader is written last, so it reliably wins LTP channels
        // (pan/tilt).  HTP channels (intensity) still max regardless of
        // order, so this doesn't change colour/dimmer behaviour.
        QSharedPointer<GenericFader> &fader = m_faders[w.universeId];
        if (fader.isNull())
        {
            fader = uni->requestFader(Universe::Override);
            fader->setName(QStringLiteral("EffectScript"));
        }

        FadeChannel fc(m_doc, w.fixtureId, w.channel);
        fc.setTarget(w.value);
        fc.setCurrent(w.value);
        fc.setFadeTime(0);
        // replace=true → the effect OWNS this channel: write it verbatim
        // (bypassing the HTP merge) so it can drive the value DOWN below the
        // scene's level (e.g. a candle dimmer flicker under a lit base).
        if (w.replace)
            fc.addFlag(FadeChannel::Override);
        // replace(), not add(): removeAll() above has already emptied the
        // fader, so add()'s HTP-merge branch could never fire anyway — but
        // its insert branch logs a qDebug() line per channel. At 50 Hz over
        // a rig's worth of channels that is thousands of formatted stderr
        // writes per second on the MasterTimer thread, which on its own can
        // blow the tick budget. replace() is the same insert without the log.
        fader->replace(fc);
    }
}
