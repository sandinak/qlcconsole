/*
  Q Light Controller Plus
  operatorvoice.h

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

#ifndef OPERATORVOICE_H
#define OPERATORVOICE_H

#include <QComboBox>
#include <QPointer>
#include <QList>
#include <QPair>
#include <QObject>
#include <QString>

class QProcess;
class QTimer;

/** @addtogroup ui UI
 * @{
 */

/**
 * The laptop speaking to the operator on stage (board Q107,
 * docs/design/FOCUS_REFINE_DESIGN.md section 3): Focus calls and Refine say where the
 * controller is -- "AT spots, layer", "fixture 3 of 8" -- so whoever holds it
 * need not look back at the screen.
 *
 * A small interface, so each platform can say it its own way: Qt's
 * TextToSpeech where this Qt has it, else macOS's `say`, else nothing.
 *
 * The laptop feeds the crew's headsets, so the voice is their cue and must
 * land with the blink. A line is never spoken over: one that comes while
 * another is being said waits its turn.
 *
 * Two kinds of waiting (gap 9, ControllerScheme::Priority):
 * - a "where am I" line goes stale: only the newest waits, so stepping
 *   quickly through fixtures says where you stopped, not every fixture on
 *   the way;
 * - a confirmation ("Spot 3 kept.") or anything important (a warning, the
 *   controller lost) is queued and never dropped.
 *
 * The voice waits for the selection to settle (board Q171, settleMs(),
 * 400 ms by default): a where line is held until no newer one has come for
 * that long, so stepping through eight lights says only where you
 * stopped -- not a stale "Fixture 2..." played to its end first. Then:
 * - a settled where line cuts a where line still being said (it is stale
 *   by definition), and an important line cuts one too -- the only lines
 *   ever cut off mid-word;
 * - a confirmation or important line is never held and never cut;
 * - a WhereNow line (a tool's first line, entering a focus call) and "Say
 *   the light" (a confirmation) are said at once.
 * Off (0) is the voice as it was before: nothing waits, nothing is cut.
 */
class OperatorVoice : public QObject
{
    Q_OBJECT

public:
    explicit OperatorVoice(QObject *parent = nullptr);
    ~OperatorVoice() override {}

    /** What speaks: "Qt TextToSpeech", "macOS say", "none". */
    virtual QString backend() const = 0;
    virtual bool isAvailable() const = 0;

    /** Say @p text: now, or after what is being said and queued.
     *  @p priority: ControllerScheme::Priority -- a where line (0) replaces
     *  a where line still waiting; a confirmation or important line is
     *  never dropped. */
    void say(const QString &text, int priority = 0);
    /** Stop now, and drop what is waiting. */
    void stop();
    bool isSpeaking() const { return m_speaking; }
    /** The last line waiting for the one being said; empty for none. */
    QString waiting() const { return m_queue.isEmpty() ? QString() : m_queue.last().first; }
    /** Every line waiting, in the order they will be said. */
    QStringList queued() const;

    /** How long a where line waits for the selection to settle, in ms (0:
     *  it does not, as before). This voice's own when set (setSettleMs(),
     *  tests), else the setting: savedSettleMs(). */
    int settleMs() const;
    void setSettleMs(int ms) { m_settleMs = ms; }
    /** The setting: Off (0), Short (250), Normal (400, the default), Long
     *  (700); this computer's, beside Voice detail. */
    static constexpr int defaultSettleMs() { return 400; }
    static int savedSettleMs();
    static void saveSettleMs(int ms);
    /** The where line waiting for the selection to settle; empty for none. */
    QString settling() const { return m_settling; }
    /** How many lines were cut off (a stale where line, or one an
     *  important line came over). */
    int cuts() const { return m_cuts; }
    /** The selection has settled (the settle timer, or a test now): the
     *  waiting where line is said. */
    void settleNow();

    /** An audio output the voice can be sent to: its ID (CoreAudio's, as
     *  `say -a ?` lists it) and its name. Two pads are both "Wireless
     *  Controller": the ID tells them apart. */
    struct OutputDevice
    {
        QString id;
        QString name;
        bool operator==(const OutputDevice &o) const { return id == o.id && name == o.name; }
    };
    /** The audio outputs this computer's voice can be sent to (macOS
     *  `say -a`); empty where it cannot choose. @p say: the program to ask
     *  (empty: the system's `say`). */
    static QList<OutputDevice> outputDeviceList(const QString &say = QString());
    /** `say -a ?`'s listing ("   74 MacBook Pro Speakers"), read. */
    static QList<OutputDevice> parseOutputDevices(const QString &listing);
    /** The audio outputs, by name (outputDeviceList()). */
    static QStringList outputDevices();
    /** Which of @p devices the choice is: the one with ID @p id while it
     *  still has @p name, else the first called @p name; -1 when it is not
     *  there (unplugged, asleep, on Bluetooth). */
    static int findOutputDevice(const QList<OutputDevice> &devices, const QString &id,
                                const QString &name);
    /** The output chosen (Settings; empty: the system's): its name, and
     *  its ID when it was chosen from the list. */
    static QString outputDevice();
    static QString outputDeviceId();
    static void setOutputDevice(const QString &name, const QString &id = QString());

    /** The best voice this computer has (none under a test, or with
     *  QLC_NO_VOICE set, so a test run is never heard). */
    static OperatorVoice *create(QObject *parent = nullptr);

    /** The user's choice, kept on this machine (Settings, on by default). */
    static bool isEnabled();
    static void setEnabled(bool on);

protected:
    /** Speak @p text now (nothing is being said). */
    virtual void speakNow(const QString &text) = 0;
    virtual void stopNow() = 0;
    /** The line has been said: the one waiting, if any, now. */
    void finished();

private:
    void start(const QString &text, int priority);
    /** Stop the line being said, mid-word: only a stale where line. */
    void cut();
    void dropWaitingWhere();
    void speakNext();

    bool m_speaking = false;
    int m_speakingPriority = 0;
    bool m_cutting = false;
    int m_cuts = 0;
    int m_settleMs = -1;
    QString m_settling;
    QTimer *m_settleTimer;
    QList<QPair<QString, int>> m_queue;
};

/** Says nothing; records what it was asked to (tests). */
class SilentVoice final : public OperatorVoice
{
    Q_OBJECT
public:
    using OperatorVoice::OperatorVoice;
    QString backend() const override { return QStringLiteral("none"); }
    bool isAvailable() const override { return false; }
    QStringList said() const { return m_said; }
    /** Each line that was cut off, as it was (OperatorVoice::cuts()). */
    QStringList cutLines() const { return m_cut; }
    /** Hold each line "being said" until finishLine() (a test of the
     *  queue); off, a line is done at once. */
    void setSlow(bool slow) { m_slow = slow; }
    void finishLine() { finished(); }
protected:
    void speakNow(const QString &text) override
    {
        m_said << text;
        if (!m_slow)
            finished();
    }
    void stopNow() override
    {
        if (!m_said.isEmpty())
            m_cut << m_said.last();
    }
private:
    QStringList m_said;
    QStringList m_cut;
    bool m_slow = false;
};

/** macOS's `say`, one process at a time. Linux (spd-say, espeak) and
 *  Windows (PowerShell's SpeechSynthesizer) would be the same with another
 *  command: see docs/design/FOCUS_REFINE_DESIGN.md.
 *
 *  `say` speaks to the output chosen in "Voice to:" (`-a`). When that output
 *  is not there -- the pad unplugged, asleep, on Bluetooth -- `say -a` fails
 *  and the line used to be lost, "Controller lost. Lights held." with the
 *  rest. Now the line goes to the system's output instead, and the first one
 *  says so. */
class CommandVoice final : public OperatorVoice
{
    Q_OBJECT
public:
    CommandVoice(const QString &program, const QStringList &leadingArgs, QObject *parent = nullptr);
    ~CommandVoice() override;
    QString backend() const override;
    bool isAvailable() const override;
    /** How long a look at the outputs (`say -a ?`) is trusted, in ms
     *  (5 s; 0 looks before every line). */
    void setDeviceCheckInterval(int ms) { m_checkInterval = ms; }
    /** What the chosen output being gone is said as, before the line. */
    static QString missingDeviceNotice(const QString &name);
protected:
    void speakNow(const QString &text) override;
    void stopNow() override;
private:
    bool sendsToDevices() const;
    /** The `-a` to give: the chosen output's ID, or empty for the system's
     *  (none chosen, or it is not there). */
    QString deviceArgument();
    void start(const QString &text, const QString &device);
private:
    QString m_program;
    QStringList m_args;
    QPointer<QProcess> m_process;
    int m_checkInterval = 5000;
    qint64 m_listedAt = -1;
    QList<OutputDevice> m_devices;
    /** The chosen output was found missing: said once, until it is back. */
    bool m_missingSaid = false;
};

/**
 * Speaks what the programmer announces (ProgrammerController::
 * operatorAnnounce) through the voice, while it is enabled.
 */
class OperatorAnnouncer final : public QObject
{
    Q_OBJECT
public:
    OperatorAnnouncer(QObject *source, OperatorVoice *voice, QObject *parent = nullptr);
    OperatorVoice *voice() const { return m_voice; }
    QString lastSaid() const { return m_last; }

public slots:
    void announce(const QString &text, int priority = 0);

private:
    OperatorVoice *m_voice;
    QString m_last;
};

/**
 * Feedback: Lights / Voice / Both -- one setting for Focus mode, Focus calls
 * and Refine (ProgrammerController::operatorFeedback()), on each panel. The
 * setting is this computer's (Settings), not the show's.
 */
class OperatorFeedbackCombo final : public QComboBox
{
    Q_OBJECT
public:
    OperatorFeedbackCombo(class ProgrammerController *pc, QWidget *parent = nullptr);

    /** Settings: what this computer last chose (Both when nothing). */
    static int savedFeedback();
    static void saveFeedback(int feedback);

private:
    void sync();
    class ProgrammerController *m_pc;
};

/** How long the voice waits for the selection to settle (board Q171): Off,
 *  250, 400 (the default) or 700 ms -- beside Voice detail, this
 *  computer's (OperatorVoice::savedSettleMs()). */
class VoiceSettleCombo final : public QComboBox
{
    Q_OBJECT
public:
    explicit VoiceSettleCombo(QWidget *parent = nullptr);
};

/** Voice detail: Terse, Standard, Verbose -- beside Feedback on every
 *  focus panel, one setting for all three tools, kept with Feedback. */
class VoiceDetailCombo final : public QComboBox
{
    Q_OBJECT
public:
    VoiceDetailCombo(class ProgrammerController *pc, QWidget *parent = nullptr);

    /** Settings: what this computer last chose (Standard when nothing). */
    static int savedDetail();
    static void saveDetail(int detail);

private:
    void sync();
    class ProgrammerController *m_pc;
};

/** @} */

#endif
