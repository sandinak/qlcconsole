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

    qDebug() << "[EffectScriptRunner] createInstancesForScene" << sceneId
             << "palettes:" << scene->palettes().size()
             << "groups:" << scene->fixtureGroups().size()
             << "fixtures:" << scene->fixtures().size();

    for (quint32 pid : scene->palettes())
    {
        QLCPalette *pal = m_doc->palette(pid);
        if (!pal || pal->type() != QLCPalette::Effect)
            continue;

        qDebug() << "[EffectScriptRunner] creating instance for effect palette" << pid
                 << "script:" << pal->scriptPath();

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

        qDebug() << "[EffectScriptRunner] started instance for scene"
                 << sceneId << "palette" << pid;
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

    if (m_instances.isEmpty() && m_registered && m_doc->masterTimer())
    {
        m_doc->masterTimer()->unregisterDMXSource(this);
        m_registered = false;
    }
}

// ---------------------------------------------------------------------------
// Main-thread tick
// ---------------------------------------------------------------------------

void EffectScriptRunner::slotTick()
{
    QMutexLocker locker(&m_instanceMutex);
    for (EffectInstance *inst : m_instances)
        inst->runTick();
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

    QMutexLocker locker(&m_instanceMutex);
    for (const EffectInstance *inst : m_instances)
    {
        const QList<EffectInstance::DmxWrite> &writes = inst->dmxWrites();
        static int s_logThrottle = 0;
        if (++s_logThrottle % 50 == 1) // log roughly every second
            qDebug() << "[EffectScriptRunner] writeDMX instances:" << m_instances.size()
                     << "writes from this inst:" << writes.size();
        for (const auto &w : writes)
        {
            if (w.universeId >= 0 && w.universeId < universes.size())
            {
                Universe *uni = universes.at(w.universeId);
                if (uni)
                    uni->write(w.addr, w.value, /*forceLTP=*/true);
            }
        }
    }
}
