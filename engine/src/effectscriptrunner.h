/*
  Q Light Controller Plus
  effectscriptrunner.h

  Owns all active EffectInstances and drives the effect engine:
    - registers as a DMXSource (writes pre-computed results every tick)
    - creates/destroys instances as scenes start/stop
    - subscribes to InputOutputMap to feed joystick/MIDI/OSC values into
      the correct input slots
    - runs a ~50 Hz QTimer on the main thread to call instance.runTick()
      (QJSEngine must stay on the main thread)

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef EFFECTSCRIPTRUNNER_H
#define EFFECTSCRIPTRUNNER_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QPointF>
#include <QTimer>
#include <QSharedPointer>
#include <QVariantMap>

#include "dmxsource.h"
#include "effectscriptcache.h"
#include "effectpresetcache.h"
#include "genericfader.h"

class EffectInstance;
class Doc;

class EffectScriptRunner final : public QObject, public DMXSource
{
    Q_OBJECT

public:
    explicit EffectScriptRunner(Doc *doc);
    ~EffectScriptRunner() override;

    /** Script discovery cache (also used by the UI for listing scripts). */
    EffectScriptCache *cache() { return &m_cache; }

    /** Preset discovery cache (named configs over the scripts; UI listing). */
    EffectPresetCache *presetCache() { return &m_presetCache; }

    // DMXSource
    void writeDMX(MasterTimer *timer, QList<Universe*> universes) override;

public slots:
    void slotFunctionStarted(quint32 fid);
    void slotFunctionStopped(quint32 fid);

    /** Re-scan a running scene's palettes and recreate Effect instances.
     *  Call this whenever a scene's palette list changes without a stop/start. */
    void syncScene(quint32 sceneId);

    void slotInputValueChanged(quint32 universe, quint32 channel,
                               uchar value, const QString &key);

    /** Update a named real-time data channel (e.g. "joystick") injected into
     *  every subscribing effect script before each tick.  Call from the main thread. */
    void setDataChannel(const QString &name, const QVariantMap &data);

private slots:
    void slotTick();

    /** Stop the tick timer and destroy all EffectInstances early, before
     *  Qt's object-destruction cascade.  Connected to QCoreApplication::
     *  aboutToQuit; prevents a multi-second hang from QJSEngine teardown. */
    void slotPrepareQuit();

private:
    void createInstancesForScene(quint32 sceneId);
    void destroyInstancesForScene(quint32 sceneId);

    Doc *m_doc;
    EffectScriptCache m_cache;
    EffectPresetCache m_presetCache;

    QTimer m_tickTimer;

    mutable QMutex   m_instanceMutex;
    QList<EffectInstance*> m_instances;

    bool m_registered = false;

    /** Named real-time data channels updated by external sources (joystick, beat…).
     *  Accessed only from the main thread; no mutex needed. */
    QHash<QString, QVariantMap> m_dataChannels;

    /** Per-universe GenericFaders used to write effect DMX through the fader
     *  pipeline instead of directly to preGMValues (which zeroIntensityChannels
     *  would wipe before processFaders re-writes colour faders). */
    QHash<int, QSharedPointer<GenericFader>> m_faders; // key = universe index

    /** Last followspot beam position (pan/tilt degrees) per fixture, captured
     *  from the most recent Operate-mode tick.  Persists across scene
     *  transitions and instance teardown so a followspot effect configured for
     *  followMode = "lastPosition" resumes where the beam left off instead of
     *  jumping.  Main-thread only (written/read in slotTick). */
    QHash<quint32, QPointF> m_lastSpotDeg;
};

#endif // EFFECTSCRIPTRUNNER_H
