/*
  Q Light Controller Plus
  focuscallcue.h

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

#ifndef FOCUSCALLCUE_H
#define FOCUSCALLCUE_H

#include <QPointer>
#include <functional>
#include <QObject>

#include "qlcengine_export.h"
#include "gamepadrole.h"
#include "controllerscheme.h"

class FocusSession;
class FocusCalls;
class Chaser;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * A focus-call cue in a cue list (FOCUS_REFINE_DESIGN.md section 7), for
 * pre-show: GO onto it plays the cue's own looks -- a preset, the work
 * light, the first call's lights parked -- and ARMS the show's focus calls.
 * Nothing steps yet: the voice says "Focus calls ready. Press Start to
 * begin." and the crew, hearing it through the headsets, gets set. The
 * controller's Start (Menu) begins them, in Operate, with the voice and the
 * blink together; leaving them (Menu, or past the last call) goes on with
 * the list: the next cue.
 *
 * Watches the Show Tree's cue lists as they play (their chasers' current
 * step). Owned by ProgrammerController, which routes Start to it while it
 * is armed.
 */
class QLC_ENGINE_EXPORT FocusCallCue final : public QObject
{
    Q_OBJECT

public:
    /** NamingVenue: Start was pressed with no venue applied; the laptop
     *  asks for its name (board Q120) until useVenue() or the time is up. */
    enum State { Idle, Armed, NamingVenue, Running };
    Q_ENUM(State)

    FocusCallCue(Doc *doc, FocusSession *session, FocusCalls *calls, QObject *parent = nullptr);
    ~FocusCallCue() override;

    State state() const { return m_state; }
    bool isArmed() const { return m_state == Armed; }
    /** The cue armed or running, and its list (invalid: none). */
    quint32 cueId() const { return m_cue; }
    quint32 listId() const { return m_list; }

    /** The control that begins the calls when armed: Start (Menu). */
    static constexpr GamepadRole::Role startRole() { return GamepadRole::Start; }

    /** Cue @p cueId of list @p listId is the one playing now (its chaser
     *  stepped to it): arms on a focus-call cue, lets go otherwise. */
    void cueCurrent(quint32 listId, quint32 cueId);
    /** Begin the calls (armed only): Focus mode on the show's calls, in
     *  Operate too. False, with @p why (and said), when it cannot. */
    bool start(QString *why = nullptr);
    /** Let go of an armed cue (the list moved on, or stopped). */
    void disarm();

    /** While NamingVenue: the venue's name (empty: a new one for today's
     *  date; the one applied: still at it), and the calls begin. False
     *  when nothing is being named. */
    bool useVenue(const QString &name);
    /** How long the laptop waits for a name before it uses the date, ms
     *  (8 s; a test shortens it). */
    void setVenueAskTimeout(int ms) { m_askTimeout = ms; }
    /** Someone is typing the name: wait for them, not the clock. */
    void holdVenueAsk();
    int venueAskTimeout() const { return m_askTimeout; }

    /** The cue a list is running (ShowTreeController::runningCue()); a
     *  test stands in for playing lists with @p probe (empty: the tree). */
    void setRunningCueProbe(const std::function<quint32(quint32)> &probe) { m_runningProbe = probe; }

    /** What is said when armed. */
    static QString armedPhrase();

signals:
    void stateChanged(FocusCallCue::State state);
    void announce(const QString &text, int priority = ControllerScheme::ConfirmPriority);
    /** The calls are done: the list goes on (GO on its chaser). */
    void continueRequested(quint32 chaserId);
    /** Start: confirm the venue on the laptop (board Q129) -- "Still at
     *  @p current?" (empty: none applied, "Which venue is this?") -- and,
     *  if nobody answers in @p timeoutMs, a new venue named @p fallback. */
    void venueNeeded(const QString &fallback, int timeoutMs, const QString &current);

private:
    void setState(State state);
    bool beginCalls(QString *why);
    void watch(quint32 functionId);
    void stepped(quint32 chaserId);
    void sessionStopped();
    quint32 chaserOf(quint32 listId) const;

private:
    Doc *m_doc;
    FocusSession *m_session;
    FocusCalls *m_calls;
    State m_state = Idle;
    quint32 m_cue = 0xFFFFFFFF;
    quint32 m_list = 0xFFFFFFFF;
    quint32 m_finished = 0xFFFFFFFF;    ///< the focus-call cue just done
    QList<QPointer<Chaser>> m_watched;
    int m_askTimeout = 8000;
    std::function<quint32(quint32)> m_runningProbe;

    class QTimer *m_askTimer = nullptr;
};

/** @} */

#endif
