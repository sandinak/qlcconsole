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

#include "spreadlayer.h"
#include <QMutex>
#include <QJSValue>

#include "beamocclusion.h"
#include <QColor>
#include <QHash>
#include <QList>
#include <QPointF>
#include <QElapsedTimer>
#include <QVariantMap>
#include <QVector3D>
#include <QVector>

#include "effectscript.h"
#include "scenetransition.h"

class Doc;
class Scene;
class Fixture;
class QLCPalette;
class StageTarget;

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
        float   scale = -1.0f;   // >= 0: a dimmer on a head with no dimmer channel --
                                 //       this channel times @scale. @value is then
                                 //       @base * @scale, @base the look's own value;
                                 //       another effect's colour on the channel is
                                 //       scaled instead (EffectScriptRunner composes)
        uchar   base = 0;
        int     layer = 0;       // the effect's place in its look's list: lower wins
        bool    htp = false;     // a colour that only adds to what's there (a sparkle
                                 // over a pulse): combined highest-wins, not by layer
    };

    EffectInstance(Doc *doc, quint32 sceneId, quint32 effectPaletteId);

    bool isValid() const { return m_script.isValid(); }
    /** Why it is not valid, in a few words: the look is gone, no effect is
     *  chosen, or the script's own lastError(). */
    QString error() const;

    quint32 sceneId()         const { return m_sceneId; }
    quint32 effectPaletteId() const { return m_effectPaletteId; }

    /** Re-bind a PARKED persistent instance to the scene that is adopting it.
     *  Everything else — the QJSEngine, the script's own state, the phase it had
     *  reached — is deliberately left untouched: that continuity is the whole
     *  point of a persistent effect. Only the scene it draws its fixtures and
     *  base values from changes. */
    void rebindToScene(quint32 sceneId) { m_sceneId = sceneId; m_alsoScenes.removeAll(sceneId); }

    /** One look, one effect: another running scene that carries this effect
     *  palette (the same palette on two layers of a cue, or a cue-wide effect
     *  over several layers) is played by THIS instance too -- laid out with
     *  the first, each scene's fixtures written with its own base. Main
     *  thread. */
    void addScene(quint32 sceneId)
    { if (sceneId != m_sceneId && !m_alsoScenes.contains(sceneId)) m_alsoScenes.append(sceneId); }
    /** @p sceneId stopped: false when this instance doesn't play it. When it
     *  was the one it is bound to, the next scene it plays takes over (see
     *  hasOtherScenes()). */
    bool removeScene(quint32 sceneId);
    bool hasOtherScenes() const { return !m_alsoScenes.isEmpty(); }
    /** Every scene it plays: the one it is bound to first. */
    QList<quint32> sceneIds() const { return QList<quint32>() << m_sceneId << m_alsoScenes; }

    // --- Release fade-out (main thread) ---------------------------------------
    // When a look stops, instead of snapping off the effect can keep rendering
    // with a linearly decaying envelope for @p ms, then be removed. Dormant
    // unless the look sets a Fade Out time.
    void startFadeOut(uint ms) { m_fadingOut = true; m_fadeOutMs = qMax(1u, ms);
                                 m_fadeOutStartMs = m_elapsed.elapsed(); }
    void cancelFadeOut()       { m_fadingOut = false; }
    bool isFadingOut() const   { return m_fadingOut; }
    bool fadeOutDone() const   { return m_fadingOut &&
                                 (m_elapsed.elapsed() - m_fadeOutStartMs) >= qint64(m_fadeOutMs); }

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

    // --- Live overrides (a VC control; docs/design/VC_EFFECT_CONTROLS.md) ---
    /** The host Spread amount (cycles) and colour 1's hue (0..1), as
     *  override names beside the script's own params. */
    static QString spreadOverrideKey() { return QStringLiteral("__spreadAmount"); }
    static QString hueOverrideKey() { return QStringLiteral("__hue1"); }
    /** Play @p name at @p value while this instance runs, over its palette's:
     *  a script param, spreadOverrideKey() or hueOverrideKey(). Kept across
     *  reloadParamsFromPalette(). Main thread. */
    void setOverride(const QString &name, double value);
    /** Back to the palette's value (the script's default when it pins none). */
    void clearOverride(const QString &name);
    void clearOverrides();
    const QHash<QString, double> &overrides() const { return m_overrides; }
    /** What @p name plays at now (override, palette or default); @p ok false
     *  when the script has no such param. Host keys: their current value. */
    double paramValue(const QString &name, bool *ok = nullptr) const;
    QList<EffectScript::ParamDef> paramDefs() const { return m_script.paramDefs(); }

    // --- Run one tick (MAIN THREAD ONLY) ---
    void runTick();

    /** Off the stage (the Blind view, BlindEffects): its time is @p ms,
     *  set before each runTick(), not its own clock, and it plays as if
     *  its scene were up, running or not. -1 (the default): the stage. */
    void setOffStageClock(qint64 ms) { m_offStageMs = ms; }

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
    /** The fixture kinds the script is for (effect.fixtureTypes). */
    QStringList scriptFixtureTypes() const;

    /** One drawable CELL per fixture HEAD, with its grid position. A multi-head /
     *  pixel fixture yields one cell per head (so effects can address individual
     *  pixels — RGB Matrix parity), a single-head fixture yields one cell. */
    struct EffectCell {
        quint32 fixtureId;
        int     head;
        int     col;
        int     row;
        bool operator==(const EffectCell &o) const
        {
            return fixtureId == o.fixtureId && head == o.head && col == o.col && row == o.row;
        }
    };
    /** Build the per-head cell list + grid size the effect draws on. Sourced from
     *  the scene's fixture-group head layouts (concatenated left→right), filtered
     *  by the script's fixtureTypes; falls back to a 1-D strip of every head of
     *  the loose fixtures when no group layout applies. */
    QList<EffectCell> effectiveCells(int &gridCols, int &gridRows) const;

    /** A head as the spread preview draws it: where it stands (metres, on
     *  the floor plan; its head-layout cell when nothing is placed) and its
     *  phase in cycles. */
    struct SpreadDot
    {
        QPointF plan;
        int col = 0, row = 0;
        double phase = 0;
    };
    /** Each head's spread, as the effect would get it now (the look
     *  editor's preview). @p centre / @p hasCentre: the target it spreads
     *  from, for the target axes. */
    QList<SpreadDot> spreadPreview(QPointF *centre = nullptr, bool *hasCentre = nullptr) const;
    /** The look's Aim target, the centre for spreading out from or round
     *  it. False when the look has none. */
    bool aimCentre(QVector3D *centre) const;

    /** Host scene's actuation level [0..1]: ramps 0→1 over the scene's fade-in,
     *  then holds at 1. Applied to the effect's intensity/colour output so the
     *  effect fires in sync with the scene rather than independent of it. */
    float sceneEnvelope() const;

    /** Rebuild m_sceneBaseValues: the DMX value the scene's *non-effect* looks
     *  (Colour/Dimmer palettes + baked values) assign to each fixture channel.
     *  Lets an intensity effect flicker *around* the scene's base rather than
     *  replacing it with an absolute value. Built once per runTick(). */
    void buildSceneBaseValues();
    void addSceneBaseValues(Scene *scene);

    /** Scene base DMX value for (fixtureId, channel), or @p dflt if the scene's
     *  looks don't drive that channel. */
    uchar sceneBaseValue(quint32 fid, quint32 channel, uchar dflt) const;

private:
    QJSValue buildFixturesArray(const QList<EffectCell> &cells, int gridCols, int gridRows);
    struct FixtureFacts;
    void readFixtureFacts(const Fixture *fxi, FixtureFacts &f) const;
    QJSValue aimAtObject(quint32 fid, const QVector3D &pos, QLCPalette *effectPal,
                         const BeamOcclusion::Model &sight);
    QJSValue sceneTargetObject(quint32 fid, StageTarget *st, const BeamOcclusion::Model &sight);
    /** Line of sight onto a script's aim object: blocked, blockedBy,
     *  blockedKind, blockedAt, clearance (see docs/dev/EFFECTSCRIPT_API.md). */
    void setSight(QJSValue &obj, const BeamOcclusion::Hit &hit) const;
    QJSValue buildInputsObject() const;
    /** What it runs on (a SpeedMasters::Assignment): its own "__speedMaster"
     *  param, else its look's effective speed master. */
    int speedAssignment() const;
    QJSValue buildPalettesObject() const;
    /** The look's colours, in its order (those after this effect, else all),
     *  and its master dimmer (1 when it has none) -- what the effect's colour
     *  slots and palettes.look are filled from. */
    /** The Colour/Dimmer palettes under this effect (its own); @p dimmer
     *  -1 when it has none. */
    void ownLook(QList<QColor> &colours, double &dimmer) const;
    void lookColours(QList<QColor> &colours, double &dimmer, bool *haveDimmer = nullptr) const;
    /** The fixtures and groups the look is on, before any type filter:
     *  what this scene writes (less any it leaves out for this effect). */
    QList<quint32> lookFixtureIds() const;
    /** What the effect lays out over: lookFixtureIds(), or ONE effect's
     *  fixtures across several scenes (Scene::layoutSpan()). */
    QList<quint32> layoutFixtureIds() const;
    /** @p fxIds less WLED fixtures that run it themselves, filtered by the
     *  script's fixtureTypes. */
    QList<quint32> typeFiltered(QList<quint32> fxIds) const;
    /** A WLED look (QLCPalette::wledEffect()): what WLED fixtures in it are
     *  sent -- the effect, its controls and the look's colours, on their own
     *  channels. They run it; the script never sees them. */
    QList<DmxWrite> wledDeviceWrites() const;
    QJSValue buildParamsObject() const;
    QJSValue buildDataChannelsObject() const;

    QList<DmxWrite> parseIntents(const QJSValue &intents,
                                 const QList<EffectCell> &cells) const;

    /* What the script is handed is kept from tick to tick: building a JS
       object for every head of every fixture, every tick, was most of what
       an effect on a big rig cost -- the script itself next to nothing. A
       fixture's facts (its name, heads, what it can do) are read again only
       when it changes, its place on the rig each tick but once a fixture,
       not once a head; the cells are rebuilt when the cells or a fixture
       change. A fresh array holds them each tick, so a script that sorts
       the list it is given does not sort the next one. */
    struct FixtureFacts
    {
        const Fixture *fixture = nullptr;
        const void *mode = nullptr;     //!< its mode, to see it change
        QString name;
        int heads = 0;
        float panRange = 360.0f, tiltRange = 270.0f;
        float lensMin = 0.0f, lensMax = 0.0f;
        bool hasPanTilt = false, hasRGB = false, hasColorWheel = false;
        bool hasDimmer = false, hasGobo = false, hasShutter = false, hasZoom = false;
        QVector3D pos;
        QJSValue posObj;                //!< shared by its cells
        quint64 seen = 0;               //!< the tick it was last looked at
        bool sceneTargetSet = false;    //!< on its cells now
        bool lastSpotSet = false;
    };
    QHash<quint32, FixtureFacts> m_facts;
    QVector<QJSValue> m_cellObjects;
    QList<EffectCell> m_cellsBuilt;
    int m_cellsBuiltCols = -1, m_cellsBuiltRows = -1;
    quint64 m_factsTick = 0;

    Doc    *m_doc;
    quint32 m_sceneId;
    QList<quint32> m_alsoScenes;    //!< addScene(): the other scenes it plays
    quint32 m_effectPaletteId;

    EffectScript     m_script;
    QJSValue         m_state;
    QElapsedTimer    m_elapsed;
    qint64           m_offStageMs = -1;     //!< setOffStageClock()
    /** Its time, ms: m_elapsed, or the off-stage clock. */
    qint64 nowMs() const { return m_offStageMs >= 0 ? m_offStageMs : m_elapsed.elapsed(); }
    /** The clock it reads and the whole beat _beatCount counts from --
     *  set on the first tick, and again when its speed master changes. */
    mutable int      m_clockAssignment = -2;
    SpreadLayer::Settings m_spreadBuilt;    //!< what the cells' "phase" was worked out for
    QVector3D        m_spreadCentre;        //!< ...and round which target
    bool             m_spreadHadCentre = false;
    mutable double   m_startBeat = 0;
    /** Own speed: the effect's time, run at its "__ownRate" (×1 = as
     *  made). Summed tick by tick so a rate change never jumps the
     *  effect's phase; at ×1 it is m_elapsed exactly. */
    mutable double   m_ownMs = 0;
    mutable qint64   m_ownLastMs = 0;

    /** The stage-laid grid of a multi-group look (effectiveCells()): placing
     *  every head on the stage is milliseconds a tick, so it is kept for up to
     *  a second, or until the look's groups or fixtures change. */
    mutable QList<EffectCell> m_stageCells;
    mutable int               m_stageCols = 0, m_stageRows = 0;
    mutable QList<quint32>    m_stageKey;
    mutable QElapsedTimer     m_stageAge;

    QHash<QString, float>      m_inputValues;
    QHash<QString, double>     m_paramValues;
    QHash<QString, double>     m_overrides;     //!< setOverride(): over the palette's
    /** The palette's spread with the override's amount over it. */
    SpreadLayer::Settings spreadSettings(const QLCPalette *pal) const;
    QHash<QString, QString>    m_stringParamValues;
    QHash<QString, QVariantMap> m_dataChannels;

    /** An "image" setting as the script sees it (MatrixImage): its frames,
     *  decoded once and handed over as ArrayBuffers, kept until the
     *  setting changes -- not rebuilt every tick. */
    struct ImageParam
    {
        QString value;
        QJSValue js;
    };
    mutable QHash<QString, ImageParam> m_imageParams;
    QJSValue imageParamObject(const QString &name, const QString &value) const;

    // Followspot position handoff (pan/tilt degrees, keyed by fixture id):
    //   m_lastSpotIn   — injected by the runner before runTick() (resume source)
    //   m_lastIntentDeg — captured during parseIntents() (what the script wrote)
    // Both touched on the main thread only.
    QHash<quint32, QPointF>    m_lastSpotIn;
    mutable QHash<quint32, QPointF> m_lastIntentDeg;

    mutable QMutex   m_mutex;
    QList<DmxWrite>  m_lastResults;
    bool             m_lastFrameEmpty = false; //!< last tick drove nothing (idle hint)
    bool             m_oneShotDone = false;     //!< it has run as many times as it runs (EffectRuns)
    bool             m_scriptRelease = false;   //!< the script's own onFinish is "release"
    int              m_runIndex = 0;            //!< which run a repeating one-shot script is on
    int              m_runLenMs = 0;            //!< this tick's run length (resolvedDurationMs())
    /** The cue / chase step's length, once found (it does not change while
     *  this plays), and when it was last looked for: the look-up walks every
     *  function, so not on every tick. */
    mutable int      m_cueLenMs = 0;
    mutable qint64   m_cueLookedMs = -1;
    /** Where @p el ms is in a run of @p len ms, 0..1, the last run ending on 1. */
    static double runPhase(qint64 el, int len, int runs);

    /** Resolved run length (ms) of one run: Look override → cue/chase
     *  timing (per syncTo) → the script's naturalDurationMs. See
     *  docs/design/EFFECT_LIFECYCLE_DESIGN.md. */
    int resolvedDurationMs() const;

    bool             m_fadingOut = false;   //!< in a release fade after the look stopped
    uint             m_fadeOutMs = 0;
    qint64           m_fadeOutStartMs = 0;  //!< m_elapsed.elapsed() when the fade began

    float m_envelope = 1.0f; //!< host-scene actuation level, applied per tick
    QString m_wledEffect;    //!< QLCPalette::wledEffect(), at construction

    /* The scene's transitions, for each cell: a look arriving by a wipe brings
       its effect in by the same wipe (envelopeFor()). */
    float envelopeFor(quint32 fid, int head) const;
    SceneTransition m_transitionIn, m_transitionOut;   //!< the scene's, this tick
    TransitionMap m_transitionMapIn, m_transitionMapOut;
    bool m_transitionMapInBuilt = false, m_transitionMapOutBuilt = false;
    uint m_sceneElapsed = 0;                           //!< ms since the scene started
    bool m_sceneRunning = false;

    //!< look's master Dimmer (0..1, 1.0 if none), rebuilt each runTick(). Applied
    //!< host-side to effect colour output on fixtures WITHOUT a master intensity
    //!< channel, so a colour effect respects the look's Dimmer palette. Fixtures
    //!< that HAVE a master dimmer already carry it on that channel (via the scene
    //!< base), so their colour is not additionally scaled (would double-dim).
    float m_lookDimmer = 1.0f;
    //!< The same, per fixture, while it plays more than one scene: each
    //!< fixture dimmed by its own scene's Dimmer (empty with one scene).
    QHash<quint32, float> m_lookDimmerOf;

    //!< scene base DMX per (fixtureId<<32|channel), rebuilt each runTick()
    QHash<quint64, uchar> m_sceneBaseValues;
};

#endif // EFFECTINSTANCE_H
