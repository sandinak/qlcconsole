/*
  Q Light Controller Plus - qlcconsole
  webaccessrun.h

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

#ifndef WEBACCESSRUN_H
#define WEBACCESSRUN_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

class Doc;

/**
 * The phone / tablet Run page (docs/features/other-improvements.md, "Run
 * page"): the show's main cue list as a phone sees it -- production and
 * list, the cue on now and the next one, the song section -- and the
 * commands it sends, GO, Back, Stop, Sub GO, Sub Back.
 *
 * It keeps no state of its own: the state is read off ShowControl, the
 * Show Tree and the list's chaser, and every command goes through
 * ShowControl::run() with ShowControl::RouteWeb (docs/design/RUN_COMMANDS.md),
 * never by stepping the chaser. "Allow remote GO" (Show Settings) and the
 * show's PIN are checked here as well as in App::runCommand(), so the page
 * is just as safe without the App (a test, the engine alone).
 *
 * The websocket side (WebAccess) tells it how many Run pages are open; the
 * state is polled four times a second only then, and a change is announced
 * by stateChanged() as one JSON text.
 */
class WebAccessRun final : public QObject
{
    Q_OBJECT

public:
    explicit WebAccessRun(Doc *doc, QObject *parent = nullptr);

    /** The page itself: one self-contained HTML text (dark, phone first). */
    static QString html();

    /** The state now, as JSON text:
     *  {list, production, now:{num,name,sub}, next:{...}, subNext, section,
     *   running, paused, hasSub, mode, remote, pin, teaching}. */
    QByteArray stateJson() const;

    /** Remote GO is on in Show Settings. */
    bool remoteAllowed() const;
    /** The show has a PIN the page asks for once per session. */
    bool pinRequired() const;

    /** Does @a pin open the page for the session? Wrong tries are counted:
     *  after kMaxTries in a row, none is taken for kLockoutMs (any PIN,
     *  right or wrong), so a PIN can't be walked through from a phone. */
    enum PinResult { PinOk, PinBad, PinLocked };
    PinResult checkPin(const QString &pin);

    /** The result of one command from a page. */
    enum Result { Done, NothingToDo, Refused, NeedPin, Unknown };
    /** Carry out @a command ("go", "back", "stop", "subgo", "subback")
     *  through ShowControl::run() with RouteWeb. @a unlocked: this page
     *  gave the PIN (or the show has none). */
    Result command(const QString &command, bool unlocked);

    /** How many Run pages are open: the state is watched while there are some. */
    void setClients(int count);

    static const int kMaxTries = 5;
    static const int kLockoutMs = 30000;

signals:
    /** The state changed: its JSON text. */
    void stateChanged(const QByteArray &json);

private slots:
    void poll();

private:
    Doc *m_doc;
    QTimer m_timer;
    QByteArray m_last;
    int m_clients = 0;
    int m_failures = 0;
    QElapsedTimer m_lockout;
    bool m_locked = false;
};

#endif
