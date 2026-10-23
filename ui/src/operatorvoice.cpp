/*
  Q Light Controller Plus
  operatorvoice.cpp

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

#include <QStandardPaths>
#include <QGuiApplication>
#include <QSettings>
#include <QProcess>

#ifdef QLC_HAVE_TEXTTOSPEECH
#include <QTextToSpeech>
#endif

#include "operatorvoice.h"

namespace
{
const QString kEnabledSetting = QStringLiteral("operatorVoice/enabled");

#ifdef QLC_HAVE_TEXTTOSPEECH
/** Qt's own: whatever engine this platform's Qt TextToSpeech has. */
class QtVoice final : public OperatorVoice
{
public:
    explicit QtVoice(QObject *parent)
        : OperatorVoice(parent)
        , m_tts(new QTextToSpeech(this))
    {
    }
    QString backend() const override { return QStringLiteral("Qt TextToSpeech"); }
    bool isAvailable() const override { return m_tts->state() != QTextToSpeech::Error; }
    void say(const QString &text) override
    {
        m_tts->stop();
        m_tts->say(text);
    }
    void stop() override { m_tts->stop(); }

private:
    QTextToSpeech *m_tts;
};
#endif
}

/*****************************************************************************
 * OperatorVoice
 *****************************************************************************/

OperatorVoice *OperatorVoice::create(QObject *parent)
{
    // Never heard from a test run, or when asked not to be.
    if (qEnvironmentVariableIsSet("QLC_NO_VOICE")
        || QGuiApplication::platformName() == QLatin1String("offscreen")
        || QGuiApplication::platformName() == QLatin1String("minimal"))
        return new SilentVoice(parent);

#if defined(Q_OS_MACOS)
    // `say` is on every Mac and needs nothing deployed with the app.
    const QString say = QStandardPaths::findExecutable(QStringLiteral("say"));
    if (!say.isEmpty())
        return new CommandVoice(say, { QStringLiteral("-r"), QStringLiteral("210") }, parent);
#elif defined(Q_OS_LINUX)
    const QString spd = QStandardPaths::findExecutable(QStringLiteral("spd-say"));
    if (!spd.isEmpty())
        return new CommandVoice(spd, { QStringLiteral("--wait") }, parent);
#endif
#ifdef QLC_HAVE_TEXTTOSPEECH
    QtVoice *qt = new QtVoice(parent);
    if (qt->isAvailable())
        return qt;
    delete qt;
#endif
    return new SilentVoice(parent);
}

bool OperatorVoice::isEnabled()
{
    return QSettings().value(kEnabledSetting, true).toBool();
}

void OperatorVoice::setEnabled(bool on)
{
    QSettings().setValue(kEnabledSetting, on);
}

/*****************************************************************************
 * CommandVoice
 *****************************************************************************/

CommandVoice::CommandVoice(const QString &program, const QStringList &leadingArgs, QObject *parent)
    : OperatorVoice(parent)
    , m_program(program)
    , m_args(leadingArgs)
{
}

CommandVoice::~CommandVoice()
{
    stop();
}

QString CommandVoice::backend() const
{
    return m_program.section(QLatin1Char('/'), -1);
}

bool CommandVoice::isAvailable() const
{
    return !m_program.isEmpty();
}

void CommandVoice::say(const QString &text)
{
    stop();
    if (text.trimmed().isEmpty())
        return;
    QProcess *p = new QProcess(this);
    connect(p, &QProcess::finished, p, &QObject::deleteLater);
    connect(p, &QProcess::errorOccurred, p, &QObject::deleteLater);
    m_process = p;
    p->start(m_program, QStringList(m_args) << text);
}

void CommandVoice::stop()
{
    if (m_process.isNull())
        return;
    QProcess *p = m_process.data();
    m_process.clear();
    if (p->state() != QProcess::NotRunning)
    {
        p->kill();
        p->waitForFinished(200);
    }
    p->deleteLater();
}

/*****************************************************************************
 * OperatorAnnouncer
 *****************************************************************************/

OperatorAnnouncer::OperatorAnnouncer(QObject *source, OperatorVoice *voice, QObject *parent)
    : QObject(parent)
    , m_voice(voice)
{
    if (source != nullptr)
        connect(source, SIGNAL(operatorAnnounce(QString)), this, SLOT(announce(QString)));
}

void OperatorAnnouncer::announce(const QString &text)
{
    m_last = text;
    if (m_voice != nullptr && OperatorVoice::isEnabled())
        m_voice->say(text);
}
