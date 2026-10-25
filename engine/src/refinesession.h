/*
  Q Light Controller Plus
  refinesession.h

  Copyright (C) Branson Matheson

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef REFINESESSION_H
#define REFINESESSION_H

#include <QElapsedTimer>
#include <QObject>
#include <QMutex>
#include <QHash>
#include <QList>
#include <functional>

#include "qlcengine_export.h"
#include "controllerscheme.h"
#include "showtreemodel.h"
#include "outputlayer.h"

#include "scenevalue.h"

class QLCPalette;
class Fixture;
class QTimer;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Refine (board Q106-Q108, FOCUS_REFINE_DESIGN.md section 6): with a Show
 * Tree cue up, the controller edits that cue's OWN values -- position, zoom,
 * focus, colour, gobo -- from the stage, where the light can be seen. Not
 * calibration (that is Focus calls): what the cue does.
 *
 * The same controller as Focus mode (ControllerScheme): the triggers step the
 * cue's layers, the bumpers its groups or fixtures, Y changes the scope
 * (Layer -> Group -> Fixture), A keeps, B goes back to the cue, X clears an
 * exception, the sticks move, the D-pad sets the beam (or, on its other
 * pages, steps colour and gobo palettes).
 *
 * What it changes stays pending -- shown on the rig through an OutputLayer
 * over the cue, so the operator on stage sees it -- until A keeps it. Keeping
 * is ONE Show Tree step:
 *  - at Layer scope, the layer's own look: a new one-off (inline) palette
 *    for the value, fanning kept, set on the layer;
 *  - at Group or Fixture scope, a per-fixture EXCEPTION: a child layer of
 *    that layer on just that group or fixture (made the first time) with
 *    the value as its own look. The parent lights the rest; X takes the
 *    exception off again.
 *
 * In Design the cue is the Show Tree's preview, so a kept edit is seen at
 * once. In Operate (board Q112a) the show runs on; a kept edit is held on
 * the rig until Refine ends, and plays from the cue's next GO.
 *
 * At Group scope a position is an offset from each fixture's own (Q112c):
 * it is kept on each fixture, so a fan inside the group stays a fan.
 * Out-of-scope fixtures of the layer are dimmed only where the cue has
 * them brighter than the dim level (Q112b).
 */
class QLC_ENGINE_EXPORT RefineSession final : public QObject
{
    Q_OBJECT

public:
    enum Scope { LayerScope, GroupScope, FixtureScope };
    /** What the D-pad sets (LS click changes it). */
    enum Page { BeamPage, ColourPage, GoboPage };
    enum Stick { Coarse, Fine };

    RefineSession(Doc *doc, QObject *parent = nullptr);
    ~RefineSession() override;

    /*********************************************************************
     * Starting and stopping
     *********************************************************************/
public:
    /** Refine cue @p cueId, starting on its layer @p layerId (invalid: the
     *  first). Refuses (false, @p why) in Operate, for a cue with no layer
     *  it can refine (own, top-level, on fixtures), or no cue. Previews the
     *  cue. */
    bool start(quint32 cueId, quint32 layerId = 0xFFFFFFFF, QString *why = nullptr);
    /** Refine Build scenes (the Workbench's stack): each scene is a layer,
     *  its group rows the groups, an exception a fixture left out of the
     *  attribute's palettes with the scene's own values on it (section 6B).
     *  Starts on @p first (invalid: the first). */
    bool startScene(const QList<quint32> &sceneIds, quint32 first = 0xFFFFFFFF,
                    QString *why = nullptr);
    /** Refining Build scenes, not a Show Tree cue. */
    bool isSceneMode() const { return m_active && m_sceneMode; }
    /** Stop, dropping anything not kept. */
    void stop();
    bool isActive() const { return m_active; }
    quint32 cueId() const { return m_cue; }

    /** Board Q130, quiet Refine in Operate: only the cue its list is
     *  running can be refined there (start() refuses any other), and it
     *  is refined quietly -- no blink, no dim. When the list moves off it
     *  (GO), what was kept stops being held and Refine goes blind (no
     *  DMX) until the cue runs again. True in Design, and for a scene. */
    bool cueIsRunning() const;
    /** True while Refine sends nothing: Operate, and its cue not running. */
    bool isBlind() const;
    /** The cue a list is running (ShowTreeController::runningCue()); a
     *  test stands in for playing lists with @p probe (empty: the tree). */
    void setRunningCueProbe(const std::function<quint32(quint32)> &probe) { m_runningProbe = probe; }

    /*********************************************************************
     * Where it is
     *********************************************************************/
public:
    /** The layers it steps through (the cue's own top layers on fixtures). */
    QList<quint32> layers() const { return m_layers; }
    int layerIndex() const { return m_layerIndex; }
    quint32 currentLayer() const;
    void nextLayer();
    void previousLayer();
    void setLayerIndex(int index);

    Scope scope() const { return m_scope; }
    void setScope(Scope scope);
    /** Layer -> Group -> Fixture -> Layer (Group only where the layer has
     *  more than one group). */
    void cycleScope();
    /** The groups (pieces) of the current layer, Group scope's steps. */
    QList<ShowTree::Target> groups() const;
    int groupIndex() const { return m_groupIndex; }
    /** The fixtures of the current layer, Fixture scope's steps. */
    QList<quint32> layerFixtures() const;
    int fixtureIndex() const { return m_fixtureIndex; }
    quint32 currentFixture() const;
    /** Next / previous group or fixture (by scope); at Layer scope it picks
     *  the first fixture. */
    void nextItem();
    void previousItem();

    /** Start on @p fid at Fixture scope (an entry from a fixture's row). */
    bool pickFixture(quint32 fid);
    /** Start on the group (piece) with group @p groupId at Group scope. */
    bool pickGroup(quint32 groupId);

    /** What the controller works on now. */
    QList<quint32> scopeFixtures() const;
    /** The target a Group or Fixture scope edit is kept on (a child layer's). */
    ShowTree::Target scopeTarget() const;

    Page page() const { return m_page; }
    void cyclePage();

    /** The current layer is only carried into the cue (tracked from an
     *  earlier one, section 6C): Layer scope only, and A keeps either in
     *  this cue (the default: the layer becomes its own here) or in the cue
     *  the value comes from (it then tracks on from there). */
    bool isTrackedLayer() const;
    /** The cue the current tracked layer's values come from (invalid: none). */
    quint32 trackedSource() const;
    bool keepsToSource() const { return m_keepToSource; }
    /** Where A keeps a tracked layer's change (said). */
    void setKeepToSource(bool source);
    /** Both bumpers (Q126): to the next pending change -- its layer, its
     *  scope, its fixture -- said; "All kept." when nothing is pending. */
    bool nextPending();

    /** View + A (board Q118): keep this once at the source cue, whatever
     *  setKeepToSource() says; on a layer of the cue's own, an ordinary keep. */
    bool keepAtSource(QString *why = nullptr);

    /*********************************************************************
     * Editing
     *********************************************************************/
public:
    /** Move the position by @p dPan, @p dTilt degrees. False (and said why)
     *  when it cannot: a Layer aimed at a stage target. */
    bool nudgePosition(float dPan, float dTilt);
    /** Widen (+) / narrow (-) the zoom, degrees. */
    bool nudgeZoom(float dDeg);
    /** Focus (Beam look's focus), -1..1 of its range per call scaled by 255. */
    bool nudgeFocus(int dValue);
    /** Step to the next (+1) or previous (-1) colour / gobo palette. */
    bool stepPalette(int dir);

    bool hasPending() const { return !m_edits.isEmpty(); }
    int pendingCount() const { return m_editOrder.size(); }
    /** A change pending where the controller is now (this layer, group or
     *  fixture). */
    bool hasPendingHere() const { return m_active && m_edits.contains(editKey()); }
    /** View + RS: "2 not kept." / "All kept." */
    QString pendingCountPhrase() const
    { return pendingCount() == 0 ? tr("All kept.") : tr("%1 not kept.").arg(pendingCount()); }
    /** Asked before a start (ProgrammerController's): false, with the
     *  reason, while another controller tool -- or this one -- has work
     *  that a start would throw away (gap 5: never silently discarded). */
    void setStartGuard(const std::function<bool(QString *)> &guard) { m_startGuard = guard; }
    /** Board Q116 (Build): a keep that would change a palette other scenes
     *  use is held until A is pressed again; this says which, empty when
     *  nothing waits. confirmShared() is that second A (or the panel's
     *  button). */
    QString sharedWarning() const { return m_sharedWarning; }
    /** A warning waits for a fresh press of A (board Q132): a held A
     *  never answers it. */
    bool hasWarningPending() const { return !m_sharedWarning.isEmpty(); }
    QStringList sharedUsers() const { return m_sharedUsers; }
    bool confirmShared(QString *why = nullptr);
    /** Keep everything pending in the cue: one Show Tree step. */
    bool keep(QString *why = nullptr);
    /** Drop what is pending. */
    void revert();
    /** At Group / Fixture scope, take the exception off (the child layer's
     *  looks for this page's keys; the child goes when it is empty). At
     *  Layer scope the same as revert(). */
    bool clearException();
    /** The Show Tree's undo. */
    bool undo();

    /** What @p fid is pending to (for the panel and tests): pan / tilt
     *  degrees, zoom degrees, focus 0..255; false where nothing is. */
    bool pendingPosition(quint32 fid, float &pan, float &tilt) const;
    bool pendingZoom(quint32 fid, float &deg) const;
    bool pendingFocus(quint32 fid, int &value) const;

    /** The position @p fid has in the cue now (child layer, else layer),
     *  degrees; false when the cue sets none. */
    bool cuePosition(quint32 fid, float &pan, float &tilt) const;

    /** Degrees for a channel's 16-bit DMX on @p fx (the inverse of
     *  Fixture::positionToValues()). */
    static float dmxToDegrees(Fixture *fx, int type, quint16 dmx);

    /*********************************************************************
     * Telling the operator (the same as Focus calls)
     *********************************************************************/
public:
    QString wherePhrase() const;
    /** "Say the light" (RS): the light's name first -- "Mover 3. Cue 4,
     *  Movers, fixture 3 of 8." -- said whatever the feedback setting. */
    QString lightPhrase() const;
    void whereAmI();
    /** The rig blinks and dims (Feedback: Lights or Both); off: Voice only. */
    void setLightsFeedback(bool on);
    void flashSelection();
    /** Something important: the selection's slow long blink (Q131). */
    void flashWarning();
    void setFlashEnabled(bool on) { m_flashEnabled = on; }
    bool isFlashing() const;
    /** Out-of-scope fixtures of the layer are dimmed to this, 0..1. */
    static constexpr float dimLevel() { return 0.15f; }

    /*********************************************************************
     * Controller
     *********************************************************************/
public:
    /** Do what @p verb means here. Exit only asks (exitRequested()). */
    void apply(ControllerScheme::Verb verb);
    /** A control went down (@p down) or up: applied once on the press; the
     *  D-pad's zoom and focus arms repeat while held (tick() does them) --
     *  not on the colour and gobo pages, where they step palettes. */
    void press(ControllerScheme::Verb verb, bool down);
    /** Whether @p verb repeats on the page now. */
    bool repeats(ControllerScheme::Verb verb) const;
    void setStick(Stick stick, float xNorm, float yNorm);
    void setStickResponse(float deadzone, float sensitivity);
    void setTrigger(int side, float norm);
    /** The controller went away: both sticks centred, a held D-pad arm
     *  and the triggers released, so nothing moves. */
    void releaseControls();
    void tick(double dt);
    static constexpr float coarseRate() { return 8.0f; }
    static constexpr float fineRate() { return 1.0f; }

    /** What it sends now, (universe, address) -> value. */
    QHash<QPair<int, int>, uchar> pendingWrites() const;

signals:
    void started();
    void stopped();
    void changed();
    void announce(const QString &text, int priority = ControllerScheme::WherePriority);
    /** Said because the operator asked (Say the light): always spoken. */
    void sayLight(const QString &text);
    void exitRequested();
    /** A keep went in (the pad buzzes). */
    void kept();

private:
    struct Edit
    {
        Scope scope = LayerScope;
        quint32 layer = 0xFFFFFFFF;
        ShowTree::Target target;      ///< Group / Fixture scope
        QList<quint32> fixtures;      ///< what it lights
        bool hasPos = false;
        float pan = 0.0f, tilt = 0.0f;   ///< Layer: a delta; else absolute
        bool hasZoom = false;
        float zoom = 0.0f;
        bool hasFocus = false;
        int focus = 0;
        quint32 colour = 0xFFFFFFFF;
        quint32 gobo = 0xFFFFFFFF;
    };

    const ShowTree::Cue *cue() const;
    /* Build scenes (refinescene.cpp). */
    class Scene *sceneAt(quint32 sceneId) const;
    QList<quint32> sceneFixtures() const;
    QList<ShowTree::Target> sceneGroups() const;
    /** The scene's palettes that set @p key on @p fid, as a look. */
    const ShowTree::Look *sceneLookFor(quint32 fid, const QString &key, QList<quint32> *lit = nullptr) const;
    /** @p fid has its own (raw) @p key values, out of the palettes. */
    bool sceneHasOwn(quint32 fid, const QString &key) const;
    QHash<quint32, uchar> sceneRawValues(quint32 fid) const;
    bool sceneKeep(QString *why);
    /** The scenes besides @p exceptScene that use palette @p paletteId, by
     *  name (the Show Tree's cues bake to scenes named for them). */
    QStringList paletteUsers(quint32 paletteId, quint32 exceptScene) const;
    /** What a keep is said for: the light picked, or the layer. */
    QString keptLight() const;
    /** "4" (its number, else its name). */
    QString cueNumber() const;
    bool sceneClearException();
    const ShowTree::Layer *layer(quint32 layerId) const;
    /** The child layer of @p layerId exactly on @p target, if any. */
    const ShowTree::Layer *childOn(quint32 layerId, const ShowTree::Target &target) const;
    /** The look @p key that lights @p fid in the cue: its own child's, else
     *  the layer's. */
    const ShowTree::Look *lookFor(quint32 fid, const QString &key, QList<quint32> *lit = nullptr) const;
    QString editKey() const;
    Edit &edit();
    QString layerName(quint32 layerId) const;
    QString targetName(const ShowTree::Target &t) const;
    QList<SceneValue> previewValues(const Edit &e) const;
    void rebuildWrites();
    /** The intensity the cue gives @p fid's @p channel (its Dimmer look,
     *  else the output as it was before Refine dimmed it), 0..255. */
    int cueLevel(quint32 fid, quint32 channel);
    void onFlashTick();
    void say(const QString &text, int priority = ControllerScheme::WherePriority)
    { emit announce(text, priority); }
    void confirm(const QString &text) { say(text, ControllerScheme::ConfirmPriority); }
    void warn(const QString &text) { say(text, ControllerScheme::ImportantPriority); }

private:
    Doc *m_doc;
    bool m_active = false;
    bool m_sceneMode = false;
    bool m_sharedConfirmed = false;
    QString m_sharedWarning;
    QStringList m_sharedUsers;
    mutable ShowTree::Look m_sceneLook;
    quint32 m_cue = 0xFFFFFFFF;
    QList<quint32> m_layers;
    int m_layerIndex = -1;
    Scope m_scope = LayerScope;
    int m_groupIndex = 0;
    int m_fixtureIndex = 0;
    Page m_page = BeamPage;
    /** A layer carried into the cue: a stand-in Layer (its target, its
     *  played looks) and where its values come from. */
    struct Tracked
    {
        ShowTree::Layer layer;
        quint32 source = 0xFFFFFFFF;
    };
    QHash<quint32, Tracked> m_tracked;
    bool m_keepToSource = false;
    /** The layers of the cue to step through: own top layers, then tracked. */
    QList<quint32> findLayers(QString *why) const;
    QHash<QString, Edit> m_edits;
    QList<QString> m_editOrder;
    /** Kept in Operate: held on the rig until the next GO plays it. */
    QList<SceneValue> m_kept;
    QHash<QPair<quint32, quint32>, int> m_levelCache;

    float m_stick[2][2] = { { 0.5f, 0.5f }, { 0.5f, 0.5f } };
    float m_deadzone = 0.05f;
    float m_sensitivity = 1.0f;
    ControllerScheme::TriggerButton m_trigger[2];
    ControllerScheme::HoldRepeat m_hold;

    QTimer *m_flashTimer = nullptr;
    QElapsedTimer m_flashClock;
    bool m_flashEnabled = true;
    bool m_warnFlash = false;
    std::function<quint32(quint32)> m_runningProbe;
    std::function<bool(QString *)> m_startGuard;
    bool m_wasRunning = true;
    /** Operate: Refine is quiet (no blink, no dim). */
    bool isQuiet() const;
    /** The list moved on (or back): release the hold, go blind. */
    void checkRunning();
    bool m_lightsFeedback = true;

    mutable QMutex m_writesMutex;
    QHash<QPair<int, int>, uchar> m_writes;
    OutputLayer m_layer;
};

/** @} */

#endif
