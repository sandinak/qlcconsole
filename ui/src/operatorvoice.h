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

/** @addtogroup ui UI
 * @{
 */

/**
 * The laptop speaking to the operator on stage (board Q107,
 * FOCUS_REFINE_DESIGN.md section 3): Focus calls and Refine say where the
 * controller is -- "AT spots, layer", "fixture 3 of 8" -- so whoever holds it
 * need not look back at the screen.
 *
 * A small interface, so each platform can say it its own way: Qt's
 * TextToSpeech where this Qt has it, else macOS's `say`, else nothing.
 *
 * The laptop feeds the crew's headsets, so the voice is their cue and must
 * land with the blink: a line is never cut off mid-word, and never spoken
 * over. A line that comes while one is being said waits its turn.
 *
 * Two kinds of waiting (gap 9, ControllerScheme::Priority):
 * - a "where am I" line goes stale: only the newest waits, so stepping
 *   quickly through fixtures says where you stopped, not every fixture on
 *   the way;
 * - a confirmation ("Spot 3 kept.") or anything important (a warning, the
 *   controller lost) is queued and never dropped.
 */
class OperatorVoice : public QObject
{
    Q_OBJECT

public:
    explicit OperatorVoice(QObject *parent = nullptr) : QObject(parent) {}
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

    /** The audio outputs this computer's voice can be sent to (macOS
     *  `say -a`), by name; empty where it cannot choose. */
    static QStringList outputDevices();
    /** The output chosen (Settings; empty: the system's). */
    static QString outputDevice();
    static void setOutputDevice(const QString &name);

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
    bool m_speaking = false;
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
    void stopNow() override {}
private:
    QStringList m_said;
    bool m_slow = false;
};

/** macOS's `say`, one process at a time. Linux (spd-say, espeak) and
 *  Windows (PowerShell's SpeechSynthesizer) would be the same with another
 *  command: see FOCUS_REFINE_DESIGN.md. */
class CommandVoice final : public OperatorVoice
{
    Q_OBJECT
public:
    CommandVoice(const QString &program, const QStringList &leadingArgs, QObject *parent = nullptr);
    ~CommandVoice() override;
    QString backend() const override;
    bool isAvailable() const override;
protected:
    void speakNow(const QString &text) override;
    void stopNow() override;
private:
    QString m_program;
    QStringList m_args;
    QPointer<QProcess> m_process;
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
