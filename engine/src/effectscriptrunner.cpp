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

    // Drive tick() on the main thread
    connect(&m_tickTimer, &QTimer::timeout,
            this, &EffectScriptRunner::slotTick);
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

    QMutexLocker locker(&m_instanceMutex);
    qDeleteAll(m_instances);
    m_instances.clear();
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
    Function *f = m_doc->function(fid);
    if (!f || f->type() != Function::SceneType)
        return;
    destroyInstancesForScene(fid);
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
    // but the timer thread holds m_dmxSourceListMutex while calling writeDMX,
    // which acquires m_instanceMutex.  Calling unregisterDMXSource while
    // holding m_instanceMutex causes an ABBA deadlock.  Collect cleanup work
    // under the lock, then release before touching MasterTimer or universes.
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
    QMutexLocker locker(&m_instanceMutex);
    for (EffectInstance *inst : m_instances)
    {
        inst->setDataChannels(m_dataChannels);
        inst->runTick();
    }
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

    // Effect scripts are for live show operation; suppress output in Design mode.
    if (m_doc->mode() == Doc::Design)
    {
        static int suppressCount = 0;
        if (++suppressCount % 250 == 1)
            qCritical() << "[ESR] suppressed in Design mode, call#" << suppressCount
                        << "instances=" << m_instances.size();
        for (auto &fader : m_faders)
            if (!fader.isNull()) fader->removeAll();
        return;
    }

    // Clear all faders so channels from the previous tick don't linger when
    // the effect stops writing them (e.g. fixture removed from scene).
    for (auto &fader : m_faders)
        if (!fader.isNull()) fader->removeAll();

    QMutexLocker locker(&m_instanceMutex);
    for (const EffectInstance *inst : m_instances)
    {
        const QList<EffectInstance::DmxWrite> &writes = inst->dmxWrites();
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
            QSharedPointer<GenericFader> &fader = m_faders[w.universeId];
            if (fader.isNull())
            {
                fader = uni->requestFader(Universe::Auto);
                fader->setName(QStringLiteral("EffectScript"));
            }

            FadeChannel fc(m_doc, w.fixtureId, w.channel);
            fc.setTarget(w.value);
            fc.setCurrent(w.value);
            fc.setFadeTime(0);
            fader->add(fc);
        }
    }
}
