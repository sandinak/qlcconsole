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
    /** Stop, dropping anything not kept. */
    void stop();
    bool isActive() const { return m_active; }
    quint32 cueId() const { return m_cue; }

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

    /** What the controller works on now. */
    QList<quint32> scopeFixtures() const;
    /** The target a Group or Fixture scope edit is kept on (a child layer's). */
    ShowTree::Target scopeTarget() const;

    Page page() const { return m_page; }
    void cyclePage();

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
    void whereAmI();
    void flashSelection();
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
    void setStick(Stick stick, float xNorm, float yNorm);
    void setStickResponse(float deadzone, float sensitivity);
    void setTrigger(int side, float norm);
    void tick(double dt);
    static constexpr float coarseRate() { return 8.0f; }
    static constexpr float fineRate() { return 1.0f; }

    /** What it sends now, (universe, address) -> value. */
    QHash<QPair<int, int>, uchar> pendingWrites() const;

signals:
    void started();
    void stopped();
    void changed();
    void announce(const QString &text);
    void exitRequested();

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
    void say(const QString &text) { emit announce(text); }

private:
    Doc *m_doc;
    bool m_active = false;
    quint32 m_cue = 0xFFFFFFFF;
    QList<quint32> m_layers;
    int m_layerIndex = -1;
    Scope m_scope = LayerScope;
    int m_groupIndex = 0;
    int m_fixtureIndex = 0;
    Page m_page = BeamPage;
    QHash<QString, Edit> m_edits;
    QList<QString> m_editOrder;
    /** Kept in Operate: held on the rig until the next GO plays it. */
    QList<SceneValue> m_kept;
    QHash<QPair<quint32, quint32>, int> m_levelCache;

    float m_stick[2][2] = { { 0.5f, 0.5f }, { 0.5f, 0.5f } };
    float m_deadzone = 0.05f;
    float m_sensitivity = 1.0f;
    ControllerScheme::TriggerButton m_trigger[2];

    QTimer *m_flashTimer = nullptr;
    QElapsedTimer m_flashClock;
    bool m_flashEnabled = true;

    mutable QMutex m_writesMutex;
    QHash<QPair<int, int>, uchar> m_writes;
    OutputLayer m_layer;
};

/** @} */

#endif
