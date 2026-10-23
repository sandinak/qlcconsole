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

#include <QPointer>
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
 * A new phrase cuts off the one before (stepping quickly through fixtures
 * says only where you stopped).
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
    /** Say @p text, cutting off anything still being said. */
    virtual void say(const QString &text) = 0;
    virtual void stop() = 0;

    /** The best voice this computer has (none under a test, or with
     *  QLC_NO_VOICE set, so a test run is never heard). */
    static OperatorVoice *create(QObject *parent = nullptr);

    /** The user's choice, kept on this machine (Settings, on by default). */
    static bool isEnabled();
    static void setEnabled(bool on);
};

/** Says nothing; records what it was asked to (tests). */
class SilentVoice final : public OperatorVoice
{
    Q_OBJECT
public:
    using OperatorVoice::OperatorVoice;
    QString backend() const override { return QStringLiteral("none"); }
    bool isAvailable() const override { return false; }
    void say(const QString &text) override { m_said << text; }
    void stop() override {}
    QStringList said() const { return m_said; }
private:
    QStringList m_said;
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
    void say(const QString &text) override;
    void stop() override;
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
    void announce(const QString &text);

private:
    OperatorVoice *m_voice;
    QString m_last;
};

/** @} */

#endif
