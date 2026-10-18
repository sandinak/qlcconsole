/*
  Q Light Controller Plus
  focussession.h

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

#ifndef FOCUSSESSION_H
#define FOCUSSESSION_H

#include <QObject>
#include <QMutex>
#include <QSet>
#include <QHash>
#include <QList>
#include <QPair>
#include <QVector3D>

#include "qlcengine_export.h"
#include "gamepadrole.h"
#include "dmxsource.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Focus mode: aligning the rig before a show.
 *
 * Steps through a list of moving heads one at a time. The current head is lit
 * and aimed at its REFERENCE -- the point it should hit, from a scene's Aim
 * look or a stage target -- through the aim solver, with its calibration
 * offsets as they stand right now. Nudging changes those offsets live, so the
 * real light moves while the rig views (which take calibration out, see
 * AimSolver::beamDirection()) keep drawing the intended aim. Saving makes a
 * head's offsets the saved ones; reverting puts them back.
 *
 * It is a TOOL, not an effect (FOCUS_MODE_DESIGN.md): it runs on the main
 * thread and its writes to the rig are ordinary edits there. It takes its
 * stick from the same path the follow spot does -- ProgrammerController's
 * bound axes, the HID profile's deadzone and sensitivity, its 50 Hz joystick
 * tick -- and owns the controller while it is on.
 *
 * Its light is a DMXSource, like Highlight: it overrides, for the heads it
 * uses, whatever else is running, and lets go when it stops.
 */
class QLC_ENGINE_EXPORT FocusSession final : public QObject, public DMXSource
{
    Q_OBJECT

public:
    /** What a head is aimed at. */
    struct Reference
    {
        enum Kind { None, SceneAim, Target };
        Kind kind = None;
        quint32 id = 0xFFFFFFFF;   ///< the scene, or the stage target
        bool isValid() const { return kind != None; }
    };

    /** What the heads in the list that are NOT being focused do. */
    enum Others { OthersDark, OthersDim, OthersAsTheyAre };

    /** Which way round the list is stepped through. */
    enum Order { ByStagePosition, AsGiven };

    struct Options
    {
        QList<quint32> fixtures;
        Order order = ByStagePosition;
        Reference first;
        Reference second;          ///< optional: Y toggles to it
        Others others = OthersDark;
        float level = 0.8f;        ///< the focused head's intensity, 0..1
        float dimLevel = 0.15f;    ///< the others', with OthersDim
    };

    /** A head's calibration offsets, degrees. */
    struct Offset
    {
        float pan = 0.0f;
        float tilt = 0.0f;
        bool operator==(const Offset &o) const { return pan == o.pan && tilt == o.tilt; }
        bool operator!=(const Offset &o) const { return !(*this == o); }
    };

    /** What a press does. */
    enum Control
    {
        NoControl,
        Next, Previous,
        Save, Revert, Zero,
        ToggleReference,
        UndoSave,
        Exit,
        StepPanUp, StepPanDown, StepTiltUp, StepTiltDown
    };

    /** The stick a reading came from. */
    enum Stick { Coarse, Fine };

    FocusSession(Doc *doc, QObject *parent = nullptr);
    ~FocusSession() override;

    /*********************************************************************
     * Starting and stopping
     *********************************************************************/
public:
    /** Start on @p options. Refuses (false, with @p why) in Operate mode,
     *  when nothing in the list can be focused, or when none of it has a
     *  reference to aim at. Restarting replaces a running session after
     *  discarding its unsaved offsets. */
    bool start(const Options &options, QString *why = nullptr);

    /** Stop, discarding any unsaved offsets (saveAll() first to keep them).
     *  Lets go of the heads. */
    void stop();

    bool isActive() const { return m_active; }
    const Options &options() const { return m_options; }

    /** The heads @p fixtures holds that Focus mode can work: they can pan or
     *  tilt and they are placed on the plot. In their order, once each. */
    static QList<quint32> focusable(Doc *doc, const QList<quint32> &fixtures);

    /** @p fixtures front to back (downstage first), then house left to right
     *  within a row -- rows are heads within 0.75 m of the row's first. */
    static QList<quint32> stageOrder(Doc *doc, const QList<quint32> &fixtures);

    /*********************************************************************
     * Where it is
     *********************************************************************/
public:
    QList<quint32> order() const { return m_order; }
    int count() const { return m_order.size(); }
    int index() const { return m_index; }
    /** The head being focused, or 0xFFFFFFFF when not active. */
    quint32 currentFixture() const;

    void next();
    void previous();
    void setIndex(int index);

    /** True while the second reference is the one aimed at. */
    bool usingSecondReference() const { return m_useSecond; }
    /** Switch between the two references; false (and no change) when there
     *  is no second one for the current head. */
    bool toggleReference();
    /** The point head @p fid is aimed at now (current reference). */
    bool referencePoint(quint32 fid, QVector3D &point) const;
    /** A short name for the reference in use, for the HUD. */
    QString referenceName() const;

    /*********************************************************************
     * Offsets
     *********************************************************************/
public:
    Offset liveOffset(quint32 fid) const;
    Offset savedOffset(quint32 fid) const;
    bool isDirty(quint32 fid) const { return liveOffset(fid) != savedOffset(fid); }
    bool hasUnsaved() const;
    QList<quint32> unsavedFixtures() const;
    /** Saved at least once this session. */
    bool isChecked(quint32 fid) const { return m_checked.contains(fid); }
    int checkedCount() const { return m_checked.size(); }

    /** Add to the current head's live offsets (degrees). */
    void nudge(float dPan, float dTilt);
    /** The step the D-pad and arrow keys take, degrees. */
    float stepDegrees() const { return m_step; }
    void setStepDegrees(float step);

    /** The current head: live becomes saved -- one undo step. Marks the show
     *  modified. */
    void save();
    /** The current head back to its saved offsets. */
    void revert();
    /** The current head's live offsets to zero (unsaved until save()). */
    void zero();
    /** Undo the last save(): that head's saved and live offsets go back to
     *  what they were before it. False when there is nothing to undo. */
    bool undoSave();
    bool canUndo() const { return !m_undo.isEmpty(); }

    /** Save every head with unsaved offsets (one undo step each). */
    void saveAll();
    /** Put every head back to its saved offsets. */
    void discardAll();

    /*********************************************************************
     * Controller
     *********************************************************************/
public:
    /** What a gamepad button does here (None for anything Focus mode does not
     *  use). Sticks are not controls: see setStick(). */
    static Control controlFor(GamepadRole::Role role);
    /** Do @p control. Exit only asks (exitRequested()), since leaving with
     *  unsaved offsets needs an answer. */
    void apply(Control control);

    /** The follow spot's stick response (followspot.js axisVelocity()): a
     *  normalised axis reading @p norm (0..1, 0.5 centred) as a velocity,
     *  -1..1, nothing inside @p deadzone and linear past it. */
    static float stickVelocity(float norm, float deadzone);

    /** The latest reading of a stick, normalised as the controller sends it
     *  (0..1, 0.5 centred; Y 0 = pushed up). */
    void setStick(Stick stick, float xNorm, float yNorm);
    /** The profile's deadzone and sensitivity, as the follow spot gets them. */
    void setStickResponse(float deadzone, float sensitivity);
    /** Integrate the sticks over @p dt seconds: full deflection is
     *  coarseRate() (left) or fineRate() (right) degrees per second, times
     *  the sensitivity. Called from the programmer's joystick tick. */
    void tick(double dt);

    static constexpr float coarseRate() { return 8.0f; }
    static constexpr float fineRate() { return 1.0f; }

    /*********************************************************************
     * DMX
     *********************************************************************/
public:
    /** @reimp -- the focused head's aim and light, the others' level. */
    void writeDMX(MasterTimer *timer, QList<Universe*> universes) override;

    /** The writes writeDMX() makes, (universe, absolute address) -> value,
     *  for a test or a readout. */
    QHash<QPair<int, int>, uchar> pendingWrites() const;

signals:
    void started();
    void stopped();
    void currentChanged(quint32 fid);
    /** A head's live or saved offsets changed. */
    void offsetChanged(quint32 fid);
    void referenceChanged();
    /** Exit was pressed: the UI asks about unsaved offsets, then stop()s. */
    void exitRequested();

private:
    struct Head
    {
        QVector3D first;
        bool hasFirst = false;
        QVector3D second;
        bool hasSecond = false;
        Offset saved;
        Offset live;
    };
    struct UndoEntry
    {
        quint32 fid;
        Offset before;
        bool wasChecked;
    };

    void resolveReferences();
    QHash<quint32, QVector3D> resolve(const Reference &ref) const;
    /** Write @p offset into the rig for @p fid (the solver and the views
     *  read it from there). */
    void applyOffset(quint32 fid, const Offset &offset);
    void rebuildWrites();
    void markModified();

private:
    Doc *m_doc;
    bool m_active = false;
    Options m_options;
    QList<quint32> m_order;
    int m_index = -1;
    bool m_useSecond = false;
    QHash<quint32, Head> m_heads;
    QSet<quint32> m_checked;
    QList<UndoEntry> m_undo;
    float m_step = 0.1f;

    float m_stick[2][2] = { { 0.5f, 0.5f }, { 0.5f, 0.5f } };
    float m_deadzone = 0.05f;
    float m_sensitivity = 1.0f;
    float m_carryPan = 0.0f;    ///< stick travel below a hundredth, kept
    float m_carryTilt = 0.0f;

    mutable QMutex m_writesMutex;
    QHash<QPair<int, int>, uchar> m_writes;
    bool m_registered = false;
};

/** @} */

#endif
