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
        bool    replace = false; // true → overwrite the scene value (bypass HTP)
                                 //        so the effect can drive a channel DOWN
    };

    EffectInstance(Doc *doc, quint32 sceneId, quint32 effectPaletteId);

    bool isValid() const { return m_script.isValid(); }

    quint32 sceneId()         const { return m_sceneId; }
    quint32 effectPaletteId() const { return m_effectPaletteId; }

    /** Re-bind a PARKED persistent instance to the scene that is adopting it.
     *  Everything else — the QJSEngine, the script's own state, the phase it had
     *  reached — is deliberately left untouched: that continuity is the whole
     *  point of a persistent effect. Only the scene it draws its fixtures and
     *  base values from changes. */
    void rebindToScene(quint32 sceneId) { m_sceneId = sceneId; }

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

    /** Re-read this look's param values from its palette onto the LIVE instance
     *  (main thread), without resetting JS state. Called when the user edits a
     *  param so the change reaches the running effect on the next tick. */
    void reloadParamsFromPalette();

    // --- Run one tick (MAIN THREAD ONLY) ---
    void runTick();

    // --- Read last results (any thread, mutex-protected) ---
    QList<DmxWrite> dmxWrites() const;

    /** True if the last tick drove nothing (all writes 0 / none) — e.g. a note
     *  effect with no held notes and no live animation. */
    bool lastFrameEmpty() const { return m_lastFrameEmpty; }

    /** True if the script reacts to an input data channel (midi/audio/joystick):
     *  such an effect can safely idle when its frame is empty and wake on input.
     *  A purely time-driven effect (no data channels) returns false and always
     *  ticks. */
    bool canIdle() const { return !m_script.dataChannelKeys().isEmpty(); }

    /** Drop the last tick's writes so writeDMX() stops asserting them. Any
        runTick() bail-out must call this, or the effect's final frame stays
        pinned on the output at Override priority. */
    void clearResults();

    /** Force a V4 garbage-collection pass on this instance's QJSEngine. runTick()
        allocates a fresh fixtures array + per-fixture objects every tick, which
        QJSEngine only reclaims lazily — under continuous ticking the heap grows
        unbounded. The runner calls this periodically to bound RSS. */
    void collectGarbage();

    // Returns the scene's fixture list filtered by the script's declared fixtureTypes.
    // If the script declares no types, all scene fixtures are returned.
    QList<quint32> effectiveFixtureIds() const;

    /** One drawable CELL per fixture HEAD, with its grid position. A multi-head /
     *  pixel fixture yields one cell per head (so effects can address individual
     *  pixels — RGB Matrix parity), a single-head fixture yields one cell. */
    struct EffectCell {
        quint32 fixtureId;
        int     head;
        int     col;
        int     row;
    };
    /** Build the per-head cell list + grid size the effect draws on. Sourced from
     *  the scene's fixture-group head layouts (concatenated left→right), filtered
     *  by the script's fixtureTypes; falls back to a 1-D strip of every head of
     *  the loose fixtures when no group layout applies. */
    QList<EffectCell> effectiveCells(int &gridCols, int &gridRows) const;

    /** Host scene's actuation level [0..1]: ramps 0→1 over the scene's fade-in,
     *  then holds at 1. Applied to the effect's intensity/colour output so the
     *  effect fires in sync with the scene rather than independent of it. */
    float sceneEnvelope() const;

    /** Rebuild m_sceneBaseValues: the DMX value the scene's *non-effect* looks
     *  (Colour/Dimmer palettes + baked values) assign to each fixture channel.
     *  Lets an intensity effect flicker *around* the scene's base rather than
     *  replacing it with an absolute value. Built once per runTick(). */
    void buildSceneBaseValues();

    /** Scene base DMX value for (fixtureId, channel), or @p dflt if the scene's
     *  looks don't drive that channel. */
    uchar sceneBaseValue(quint32 fid, quint32 channel, uchar dflt) const;

private:
    QJSValue buildFixturesArray(const QList<EffectCell> &cells, int gridCols, int gridRows);
    QJSValue buildInputsObject() const;
    QJSValue buildPalettesObject() const;
    QJSValue buildParamsObject() const;
    QJSValue buildDataChannelsObject() const;

    QList<DmxWrite> parseIntents(const QJSValue &intents,
                                 const QList<EffectCell> &cells) const;

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
    bool             m_lastFrameEmpty = false; //!< last tick drove nothing (idle hint)

    float m_envelope = 1.0f; //!< host-scene actuation level, applied per tick

    //!< look's master Dimmer (0..1, 1.0 if none), rebuilt each runTick(). Applied
    //!< host-side to effect colour output on fixtures WITHOUT a master intensity
    //!< channel, so a colour effect respects the look's Dimmer palette. Fixtures
    //!< that HAVE a master dimmer already carry it on that channel (via the scene
    //!< base), so their colour is not additionally scaled (would double-dim).
    float m_lookDimmer = 1.0f;

    //!< scene base DMX per (fixtureId<<32|channel), rebuilt each runTick()
    QHash<quint64, uchar> m_sceneBaseValues;
};

#endif // EFFECTINSTANCE_H
