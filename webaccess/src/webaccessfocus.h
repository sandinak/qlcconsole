/*
  Q Light Controller Plus - qlcconsole
  webaccessfocus.h

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

#ifndef WEBACCESSFOCUS_H
#define WEBACCESSFOCUS_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

#include "webaccessrun.h"

class FocusSession;
class Doc;

/**
 * The focus remote (docs/features/gamepad-remote.md): a phone, tablet or
 * Steam Deck works the Focus mode or Venue focus that is running on the
 * console, from the stage, with its own controller or the page's buttons.
 *
 * It starts nothing: the operator starts Focus mode or Venue focus on the
 * console (the venue is the console's question), and the page works the
 * session that runs. It adds no path of its own: a press is the same
 * gamepad role the console's controller sends, done through
 * FocusSession::controlFor() and apply(); a stick move is a nudge, as the
 * arrow keys make (FocusSession::nudge()).
 *
 * The same rules as the Run page (WebAccessRun): "Allow remote GO" in Show
 * Settings, then the show's PIN once per page. Exit (Menu) is not taken:
 * leaving with unsaved corrections is asked at the console.
 *
 * Moves come as deltas, in degrees, never as a stick's position, so a page
 * that drops off the Wi-Fi leaves nothing held: the light stops. They are
 * added up and applied at most every kApplyMs (a burst from a slow network
 * is one move), each held to kMaxStep.
 */
class WebAccessFocus final : public QObject
{
    Q_OBJECT

public:
    explicit WebAccessFocus(Doc *doc, WebAccessRun *run, QObject *parent = nullptr);

    /** The page: one self-contained HTML text, served at /focus. */
    static QString html();

    /** The state now, as JSON text: {api, active, remote, pin, mode,
     *  teaching, holding, venue, calls:[{name,note}], call, callName,
     *  target, note, scope ("fixture"/"group"), targetPage, fixture:{id,
     *  name, index, count, pan, tilt, delta, unsaved, checked, beam, focus},
     *  fixtures:[{id,name,unsaved,checked}], unsaved, checked, total,
     *  coarse, fine, repeatDelay, repeatInterval, labels:{role: label}}. */
    QByteArray stateJson() const;

    /** Add @a dPan, @a dTilt degrees (each held to kMaxStep) to the move
     *  waiting; applied now, or kApplyMs after the last one. */
    WebAccessRun::Result nudge(double dPan, double dTilt, bool unlocked);
    /** A controller button, by its role key ("a", "rb", "lt", "dpad_up"...):
     *  what the console's controller scheme makes of it. */
    WebAccessRun::Result press(const QString &roleKey, bool unlocked);
    /** Go to focus call @a index (0-based). */
    WebAccessRun::Result selectCall(int index, bool unlocked);
    /** Go to fixture @a fid (in its call), flashing it: Locate. */
    WebAccessRun::Result selectFixture(quint32 fid, bool unlocked);

    /** How many focus pages are open: the state is watched while there are. */
    void setClients(int count);

    /** Moves applied so far (each may be many messages). For a test. */
    int appliedMoves() const { return m_applied; }

    static constexpr int kApplyMs = 20;
    static constexpr double kMaxStep = 5.0;
    /** The state goes to the pages at most this often (ms): 20 a second. */
    static constexpr int kStateMs = 50;

signals:
    void stateChanged(const QByteArray &json);

private slots:
    void poll();
    void schedule();
    void applyMove();

private:
    FocusSession *session() const;
    /** Remote GO on, the PIN given, a session running. */
    WebAccessRun::Result allowed(bool unlocked) const;

private:
    Doc *m_doc;
    WebAccessRun *m_run;
    QTimer m_poll;
    QTimer m_soon;
    QTimer m_apply;
    QElapsedTimer m_lastApply;
    QElapsedTimer m_lastEmit;
    QByteArray m_last;
    int m_clients = 0;
    double m_pendPan = 0.0;
    double m_pendTilt = 0.0;
    int m_applied = 0;
};

#endif
