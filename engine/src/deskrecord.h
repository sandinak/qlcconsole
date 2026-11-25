/*
  Q Light Controller Plus
  deskrecord.h

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

#ifndef DESKRECORD_H
#define DESKRECORD_H

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include <QSet>
#include <functional>

#include "grandmasterpickup.h"
#include "qlcengine_export.h"

class QLCIOPlugin;
class QTimer;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * This computer's desk (docs/design/CONTROL_SURFACES_EVAL.md section 6,
 * phase D; decisions Q305-Q309, Q314-Q319, Q322 in CONTROL_SCOPES.md): the
 * static roles STOP, Grand Master, GO and Back belong to THIS computer, not
 * to the show. Each is a control of a device, learned once by pressing it,
 * and kept in this computer's settings (QSettings group settingsGroup(),
 * never in the show file), by the device's identity (ControllerJobs::
 * deskIdentityFor(): a MIDI port by its name, a Windows repeat by its "2- "
 * unit, ALSA by client:port).
 *
 * It reaches the program through the show's own patch (Q322): a role is
 * live where a universe's input has its device. A repatch moves it with
 * the device (rebind()); a show that has not patched the device says so
 * (NotPatched) and patchIt() patches it, named and undoable.
 *
 * Its controls are static: the one dispatch point (InputOutputMap::
 * setStaticControls()) keeps every show listener off them, and a show's
 * Control Map trigger on one is kept but not armed, listed as blocked
 * (Q309). Both stops are live, unless the show's sits on a control the
 * desk uses for something else (Q315). The show's own Grand Master input
 * and its GO of the Run tab's list are fallbacks, armed only while this
 * computer has no live role for them (Q314).
 *
 *  - STOP: ALL OFF on each press (ShowControl::allOff()), always: with no
 *    show, no binding, a Learn open, in Design or Operate, while loading.
 *    Learned press, release, press on a button.
 *  - GO / Back: one press GOes / goes back on the Run tab's list
 *    (ControlMap::runList()), through ShowControl::run() and its
 *    double-GO guard (Q317). Learned on a button; a fader is refused.
 *  - Grand Master: the fader, with GrandMasterPickup after a load, a
 *    replug and a wake (Q316). Learned on a fader; a button is refused.
 *
 * Each role can be turned off on this computer (setOn(), Q317). Every
 * change asks for the show's PIN when it has one (Q319: isLocked()).
 * GUI thread only.
 */
class QLC_ENGINE_EXPORT DeskRecord final : public QObject
{
    Q_OBJECT

public:
    explicit DeskRecord(Doc *doc, QObject *parent = nullptr);
    ~DeskRecord() override;

    enum Role { Stop = 0, GrandMaster, Go, Back };
    Q_ENUM(Role)
    static constexpr int RoleCount = 4;

    /** Where a role stands on this computer in this show. */
    enum State
    {
        NotSet,         //!< nothing learned
        Off,            //!< learned, turned off on this computer (Q317)
        Live,           //!< its device is on a universe's input: it works
        NotPatched,     //!< its device is here, but this show's patch lacks it (Patch It)
        NotConnected    //!< its device is not plugged in
    };

    struct Binding
    {
        QString identity;       //!< ControllerJobs::deskIdentityFor()
        QString device;         //!< the device's name, as a person reads it
        QString plugin;         //!< its plugin ("MIDI")
        quint32 channel = 0xFFFFFFFF;
        QString control;        //!< the control in words, when learned ("Note 82, MIDI ch 1")
        bool on = true;         //!< Q317: turned off on this computer when false

        bool isSet() const { return !identity.isEmpty() && channel != 0xFFFFFFFF; }
        bool operator==(const Binding &o) const
        {
            return identity == o.identity && device == o.device && plugin == o.plugin
                   && channel == o.channel && control == o.control && on == o.on;
        }
    };

    static QList<Role> roles();
    /** "STOP", "Grand Master", "GO", "Back" */
    static QString roleName(Role role);
    /** The settings key: "stop", "gm", "go", "back" */
    static QString roleKey(Role role);
    /** The Grand Master takes a fader; the others a button. */
    static bool takesFader(Role role) { return role == GrandMaster; }
    /** The QSettings group the record is kept in. */
    static QString settingsGroup();

    /*********************************************************************
     * The record
     *********************************************************************/
public:
    Binding binding(Role role) const { return m_bindings[role]; }
    bool isSet(Role role) const { return m_bindings[role].isSet(); }
    bool anySet() const;
    State state(Role role) const;
    bool isLive(Role role) const { return state(role) == Live; }
    /** The universe @p role reaches the program on now; invalid when not live. */
    quint32 universeOf(Role role) const;
    /** Whether @p universe / @p channel is a live role here (and which). */
    bool isDeskControl(quint32 universe, quint32 channel, Role *role = nullptr) const;
    /** "APC MINI · Note 82 · on Universe 3", "not set", "off on this
     *  computer", "APC MINI is not patched in this show". */
    QString stateText(Role role) const;

    /** Q319: a change asks for the show's PIN while one is set. */
    bool isLocked() const;
    /** The PIN: changes allowed until relock() or another show. */
    bool unlock(const QString &pin);
    /** The UI asked for the PIN itself (ShowSettingsDialog::askForPin()). */
    void setUnlocked(bool on) { m_unlocked = on; }
    void relock();

    /** Learned bindings (each change false while locked or unchanged). */
    bool setBinding(Role role, const Binding &binding);
    bool clear(Role role);
    /** Q317: one static role off (or on again) on this computer. */
    bool setOn(Role role, bool on);

    /** Why the last change was refused ("Enter the show's PIN…"). */
    QString lockedText() const;

    /*********************************************************************
     * Learning (four typed learns)
     *********************************************************************/
public:
    /** Listen for @p role's control: STOP a button pressed, released and
     *  pressed again; GO and Back a button; the Grand Master a fader. The
     *  wrong kind is refused (learnRefused()), and a control refused once
     *  stays refused for this learn (a fader pulled to 0 is no press). The
     *  show's emergency stop is refused for GO, Back and the Grand Master.
     *  Another role's control still does its job meanwhile. A learn ends
     *  by itself after learnTimeoutMs(), on a load and on a mode change
     *  (learnCancelled()). False while locked. */
    bool learn(Role role);
    /** Stop listening, nothing learned (learnCancelled() with no words). */
    void cancelLearn();
    bool isLearning() const { return m_learning; }
    /** How long a learn listens before it gives up (ms; 30 s). */
    int learnTimeoutMs() const { return m_learnTimeoutMs; }
    void setLearnTimeoutMs(int ms) { m_learnTimeoutMs = ms; }
    Role learningRole() const { return m_learnRole; }
    /** STOP's two presses must come within this (ms). */
    void setConfirmMs(int ms) { m_confirmMs = ms; }

    /*********************************************************************
     * Devices and the show's patch
     *********************************************************************/
public:
    /** The desk's identity of the device on @p universe's input; empty: none. */
    QString identityOfUniverse(quint32 universe) const;
    /** "Two devices are called APC MINI: …" for each set role whose
     *  device can't be told from another here; empty when none. */
    QString duplicateWarning() const;
    /** The devices set roles need that this show's patch lacks, but are
     *  plugged in (Patch It can patch them). */
    QStringList missingDevices() const;
    /** Patch the first of missingDevices() to a universe whose input is
     *  free (one added when none is). Returns the universe; @p what says
     *  it ("Patched APC MINI to Universe 3's input."). Invalid when there
     *  was nothing to patch. Undo with undoPatchIt(). */
    quint32 patchIt(QString *what = nullptr);
    /** Whether patchIt() can be undone: only in the show it patched (a
     *  load or clearContents() forgets it, and says changed()). */
    bool canUndoPatch() const { return m_patchedUniverse != 0xFFFFFFFF; }
    /** @p role's device sits on a universe whose input passes through to
     *  DMX: the desk can't hear it there (NotPatched, and Patch It leaves
     *  it alone: passthrough is the show's to turn off). */
    bool isOnPassthrough(Role role) const;
    /** The device patchIt() patched off again (and the universe it added
     *  removed). @p what says it. */
    bool undoPatchIt(QString *what = nullptr);

    /*********************************************************************
     * GO, and the Grand Master's pick-up
     *********************************************************************/
public:
    /** What GO plays: "GO → Act 2 main" (the Run tab's list), or that the
     *  Run tab has none. */
    QString goTargetText() const;
    /** The Grand Master waits to pick up (after a load, a replug, a wake):
     *  its fader spoke after the dump and is not at the master yet. */
    bool grandMasterWaiting() const
    { return isLive(GrandMaster) && m_gmPickup.waiting() && m_gmPickup.last() >= 0; }
    /** "GM waiting: bring the fader to 100 %"; empty when not waiting. */
    QString grandMasterHint() const;
    /** Wait to pick up again: a load, a replug, a wake. */
    void rearmGrandMaster();
    /** A clock in ms for the pick-up's dump window (a test's). */
    void setClock(const std::function<qint64()> &clock) { m_clock = clock; }

    /*********************************************************************
     * Show Review's "This computer" section
     *********************************************************************/
public:
    /** One line per role: "STOP: APC MINI · Note 82 · live", … */
    QStringList reviewLines() const;
    /** Whether anything there needs looking at (a role not live). */
    bool reviewNeedsAttention() const;

    /*********************************************************************
     * A small file, to carry the record to another computer
     *********************************************************************/
public:
    bool exportTo(const QString &path, QString *error = nullptr) const;
    /** Replaces the record (false, and nothing changed, while locked or
     *  for a file that is not one). */
    bool importFrom(const QString &path, QString *error = nullptr);

    /*********************************************************************
     * Input
     *********************************************************************/
public:
    /** Desk roles count for "want input" (the eval's risk 4): connected
     *  while a role is set or a learn listens, outside the Control Map's
     *  own cycle, so the desk's STOP is heard in a show with no binding. */
    bool isInputConnected() const { return m_inputConnected; }

public slots:
    /** One input value (InputOutputMap::inputValueChanged), raw. */
    void handleInput(quint32 universe, quint32 channel, uchar value);
    /** Work out again where each role is (a patch, a device, a load). */
    void rebind();

signals:
    /** The record, or where it reaches the program, changed. */
    void changed();
    void learned(DeskRecord::Role role);
    void learnRefused(const QString &why);
    /** A learn ended with nothing learned: @p why says why (it ran out of
     *  time, a show loaded, the mode changed); empty for cancelLearn(). */
    void learnCancelled(const QString &why);
    /** STOP's first press heard: press it again. */
    void learnConfirming(DeskRecord::Role role);
    /** A role did its job ("ALL OFF", "GO Act 2 main"). */
    void fired(const QString &what);
    /** A press did nothing, and why ("GO: the Run tab has no cue list"). */
    void refused(const QString &why);
    void grandMasterWaitingChanged(bool waiting);
    /** The Run tab's list changed: what GO plays now. */
    void goTargetChanged(const QString &text);

private:
    void load();
    void save() const;
    bool changeAllowed();
    void updateConnections();
    void applyStatic();
    qint64 now() const { return m_clock ? m_clock() : m_elapsed.elapsed(); }
    void learnHeard(quint32 universe, quint32 channel, uchar value);
    void finishLearn(quint32 universe, quint32 channel, uchar value);
    void refuseLearn(quint64 k, const QString &why);
    /** End a learn with nothing learned; @p why for learnCancelled(). */
    void endLearn(const QString &why);
    /** Whether @p universe / @p channel holds the show's emergency stop. */
    bool isShowStop(quint32 universe, quint32 channel) const;
    bool isFaderOnProfile(quint32 universe, quint32 channel, bool *known) const;
    QString controlName(quint32 universe, quint32 channel) const;
    void stopPressed(quint64 k, uchar value);
    void goPressed(bool back);
    void grandMaster(uchar value);
    void notifyGrandMasterWaiting();
    /** A learn listens (or no longer): the show's listeners hear no press meanwhile. */
    void setLearning(bool on);
    void endSwallow(quint64 k);
    /** @p universe's device went away or came back: what it held lets go. */
    void forgetHeld(quint32 universe);
    void forgetPatchUndo();
    /** Lines of every input plugin with @p identity: (plugin, line). */
    QList<QPair<QLCIOPlugin *, quint32>> linesOf(const QString &identity) const;

private:
    Doc *m_doc;
    Binding m_bindings[RoleCount];
    /** Where each role is live now (rebind()). */
    quint32 m_universes[RoleCount];
    bool m_inputConnected = false;
    bool m_unlocked = false;

    // Learning
    bool m_learning = false;
    Role m_learnRole = Stop;
    int m_confirmMs = 3000;
    quint64 m_pendingKey = ~quint64(0);
    uchar m_pendingValue = 0;
    bool m_pendingReleased = false;
    int m_pendingPresses = 0;
    QElapsedTimer m_pendingAt;
    /** Controls refused in this learn: never taken by it after. */
    QSet<quint64> m_learnRefused;
    int m_learnTimeoutMs = 30000;
    QTimer *m_learnTimer = nullptr;
    /** Faders: the distinct values a control sent while learning. */
    QHash<quint64, QSet<int>> m_faderSeen;
    /** The press that taught a role, swallowed until its release (or, its
     *  release lost, a second value or a second of quiet). */
    QSet<quint64> m_swallow;
    QHash<quint64, qint64> m_swallowAt;
    QSet<quint64> m_swallowSeen;

    // STOP: a held press resends; ALL OFF again only once a second.
    struct StopHold
    {
        bool released = true;
        QElapsedTimer lastFired;
    };
    QHash<quint64, StopHold> m_stopHold;
    QSet<quint64> m_down;

    // The Grand Master
    GrandMasterPickup m_gmPickup;
    bool m_gmWasWaiting = false;
    /** The live Grand Master's identity|universe|channel, last rebind(). */
    QString m_gmLiveKey;
    QElapsedTimer m_elapsed;
    std::function<qint64()> m_clock;

    // Patch It
    quint32 m_patchedUniverse = 0xFFFFFFFF;
    bool m_patchAddedUniverse = false;
    QString m_patchedDevice;
};

/** @} */

#endif
