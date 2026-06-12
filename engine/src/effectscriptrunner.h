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
#include <QList>
#include <QMutex>
#include <QTimer>

#include "dmxsource.h"
#include "effectscriptcache.h"

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

    QTimer m_tickTimer;

    mutable QMutex   m_instanceMutex;
    QList<EffectInstance*> m_instances;

    bool m_registered = false;
};

#endif // EFFECTSCRIPTRUNNER_H
