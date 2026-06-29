/*
  Q Light Controller Plus
  effectinstance.h

  One running instance of an effect script bound to a specific scene.
  Owns an EffectScript (its own QJSEngine), current input/param values,
  a persistent state object, and the last computed DMX write list.

  Threading model:
    runTick()   — must be called from the MAIN thread (QJSEngine constraint)
    dmxWrites() — may be called from the MasterTimer thread (mutex-protected)

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef EFFECTINSTANCE_H
#define EFFECTINSTANCE_H

#include <QMutex>
#include <QJSValue>
#include <QHash>
#include <QList>
#include <QPointF>
#include <QElapsedTimer>
#include <QVariantMap>

#include "effectscript.h"

class Doc;

class EffectInstance
{
public:
    struct DmxWrite {
        int     universeId;
        int     addr;        // universe-local address (0-511)
        uchar   value;
        quint32 fixtureId;   // for GenericFader FadeChannel creation
        quint32 channel;     // fixture-relative channel index
    };

    EffectInstance(Doc *doc, quint32 sceneId, quint32 effectPaletteId);

    bool isValid() const { return m_script.isValid(); }

    quint32 sceneId()         const { return m_sceneId; }
    quint32 effectPaletteId() const { return m_effectPaletteId; }

    // --- Input values (main thread, set by EffectScriptRunner on input events) ---
    void setInputValue(const QString &slotName, float norm01);
    float inputValue(const QString &slotName) const;

    // --- Named data channels (joystick, beat, …) injected before each tick ---
    void setDataChannels(const QHash<QString, QVariantMap> &channels);

    /** Last followspot beam position (pan/tilt degrees) per fixture, supplied by
     *  the runner before each tick.  Exposed to the script as fixture.lastSpot so
     *  followMode = "lastPosition" can seed its setpoint and avoid jumping when a
     *  new scene's followspot takes over. */
    void setLastSpotPositions(const QHash<quint32, QPointF> &deg) { m_lastSpotIn = deg; }

    /** Per-fixture pan/tilt (degrees) the last runTick() produced.  The runner
     *  accumulates these into its persistent last-spot store. */
    QHash<quint32, QPointF> lastIntentDegrees() const { return m_lastIntentDeg; }

    /** Names of data channels this script subscribes to. */
    QStringList dataChannelKeys() const { return m_script.dataChannelKeys(); }

    // --- Param values ---
    void setParamValue(const QString &name, double value);
    void setStringParamValue(const QString &name, const QString &value);

    // --- Run one tick (MAIN THREAD ONLY) ---
    void runTick();

    // --- Read last results (any thread, mutex-protected) ---
    QList<DmxWrite> dmxWrites() const;

    // Returns the scene's fixture list filtered by the script's declared fixtureTypes.
    // If the script declares no types, all scene fixtures are returned.
    QList<quint32> effectiveFixtureIds() const;

private:
    QJSValue buildFixturesArray(const QList<quint32> &fxIds);
    QJSValue buildInputsObject() const;
    QJSValue buildPalettesObject() const;
    QJSValue buildParamsObject() const;
    QJSValue buildDataChannelsObject() const;

    QList<DmxWrite> parseIntents(const QJSValue &intents,
                                 const QList<quint32> &fxIds) const;

    Doc    *m_doc;
    quint32 m_sceneId;
    quint32 m_effectPaletteId;

    EffectScript     m_script;
    QJSValue         m_state;
    QElapsedTimer    m_elapsed;

    QHash<QString, float>      m_inputValues;
    QHash<QString, double>     m_paramValues;
    QHash<QString, QString>    m_stringParamValues;
    QHash<QString, QVariantMap> m_dataChannels;

    // Followspot position handoff (pan/tilt degrees, keyed by fixture id):
    //   m_lastSpotIn   — injected by the runner before runTick() (resume source)
    //   m_lastIntentDeg — captured during parseIntents() (what the script wrote)
    // Both touched on the main thread only.
    QHash<quint32, QPointF>    m_lastSpotIn;
    mutable QHash<quint32, QPointF> m_lastIntentDeg;

    mutable QMutex   m_mutex;
    QList<DmxWrite>  m_lastResults;
};

#endif // EFFECTINSTANCE_H
