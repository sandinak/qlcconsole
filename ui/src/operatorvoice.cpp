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

#include "programmercontroller.h"
#include "controllerscheme.h"
#include "operatorvoice.h"

namespace
{
const QString kEnabledSetting = QStringLiteral("operatorVoice/enabled");
const QString kDeviceSetting = QStringLiteral("operatorVoice/device");
const QString kFeedbackSetting = QStringLiteral("controllerTools/feedback");

#ifdef QLC_HAVE_TEXTTOSPEECH
/** Qt's own: whatever engine this platform's Qt TextToSpeech has. */
class QtVoice final : public OperatorVoice
{
public:
    explicit QtVoice(QObject *parent)
        : OperatorVoice(parent)
        , m_tts(new QTextToSpeech(this))
    {
        QObject::connect(m_tts, &QTextToSpeech::stateChanged, this, [this](QTextToSpeech::State s) {
            if (s == QTextToSpeech::Ready || s == QTextToSpeech::Error)
                finished();
        });
    }
    QString backend() const override { return QStringLiteral("Qt TextToSpeech"); }
    bool isAvailable() const override { return m_tts->state() != QTextToSpeech::Error; }
protected:
    void speakNow(const QString &text) override { m_tts->say(text); }
    void stopNow() override { m_tts->stop(); }

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

void OperatorVoice::say(const QString &text)
{
    if (text.trimmed().isEmpty())
        return;
    if (m_speaking)
    {
        // Never over the line being said; the newest waits, stale ones go.
        m_waiting = text;
        return;
    }
    m_speaking = true;
    speakNow(text);
}

void OperatorVoice::stop()
{
    m_waiting.clear();
    if (m_speaking)
        stopNow();
    m_speaking = false;
}

void OperatorVoice::finished()
{
    m_speaking = false;
    if (m_waiting.isEmpty())
        return;
    const QString next = m_waiting;
    m_waiting.clear();
    m_speaking = true;
    speakNow(next);
}

QStringList OperatorVoice::outputDevices()
{
    QStringList out;
#if defined(Q_OS_MACOS)
    const QString say = QStandardPaths::findExecutable(QStringLiteral("say"));
    if (say.isEmpty())
        return out;
    QProcess p;
    p.start(say, { QStringLiteral("-a"), QStringLiteral("?") });
    if (!p.waitForFinished(3000))
        return out;
    // "   74 MacBook Pro Speakers"
    for (const QString &line : QString::fromUtf8(p.readAllStandardOutput()).split(QLatin1Char('\n')))
    {
        const QString t = line.trimmed();
        const int sp = t.indexOf(QLatin1Char(' '));
        if (sp > 0)
            out << t.mid(sp + 1).trimmed();
    }
#endif
    return out;
}

QString OperatorVoice::outputDevice()
{
    return QSettings().value(kDeviceSetting).toString();
}

void OperatorVoice::setOutputDevice(const QString &name)
{
    if (name.isEmpty())
        QSettings().remove(kDeviceSetting);
    else
        QSettings().setValue(kDeviceSetting, name);
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
    stopNow();
}

QString CommandVoice::backend() const
{
    return m_program.section(QLatin1Char('/'), -1);
}

bool CommandVoice::isAvailable() const
{
    return !m_program.isEmpty();
}

void CommandVoice::speakNow(const QString &text)
{
    QProcess *p = new QProcess(this);
    // Done (or failed): the next line, if one is waiting.
    connect(p, &QProcess::finished, this, [this, p]() {
        p->deleteLater();
        if (m_process == p)
        {
            m_process.clear();
            finished();
        }
    });
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError) {
        p->deleteLater();
        if (m_process == p)
        {
            m_process.clear();
            finished();
        }
    });
    m_process = p;
    QStringList args = m_args;
#if defined(Q_OS_MACOS)
    // To the output chosen: the headsets' feed, say.
    const QString device = outputDevice();
    if (!device.isEmpty() && m_program.endsWith(QLatin1String("/say")))
        args << QStringLiteral("-a") << device;
#endif
    p->start(m_program, args << text);
}

void CommandVoice::stopNow()
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
        connect(source, SIGNAL(operatorAnnounce(QString,int)), this, SLOT(announce(QString,int)));
}

void OperatorAnnouncer::announce(const QString &text, int priority)
{
    Q_UNUSED(priority)
    m_last = text;
    if (m_voice != nullptr && OperatorVoice::isEnabled())
        m_voice->say(text);
}

/*****************************************************************************
 * OperatorFeedbackCombo
 *****************************************************************************/

OperatorFeedbackCombo::OperatorFeedbackCombo(ProgrammerController *pc, QWidget *parent)
    : QComboBox(parent)
    , m_pc(pc)
{
    setObjectName(QStringLiteral("operatorFeedback"));
    setFocusPolicy(Qt::NoFocus);
    addItem(tr("Lights and voice"), int(ControllerScheme::BothFeedback));
    addItem(tr("Lights only"), int(ControllerScheme::LightsFeedback));
    addItem(tr("Voice only"), int(ControllerScheme::VoiceFeedback));
    setToolTip(tr("How you are told where you are: the rig blinks what is picked, the laptop says it, "
                  "or both. Say the light (RS, or W) always speaks. Hold RS, or Shift+W, to switch."));
    sync();
    connect(this, &QComboBox::currentIndexChanged, this, [this]() {
        m_pc->setOperatorFeedback(ControllerScheme::Feedback(currentData().toInt()));
    });
    connect(m_pc, &ProgrammerController::operatorFeedbackChanged, this, &OperatorFeedbackCombo::sync);
}

void OperatorFeedbackCombo::sync()
{
    const bool was = blockSignals(true);
    setCurrentIndex(findData(int(m_pc->operatorFeedback())));
    blockSignals(was);
}

int OperatorFeedbackCombo::savedFeedback()
{
    return int(ControllerScheme::feedbackFromKey(QSettings().value(kFeedbackSetting).toString()));
}

void OperatorFeedbackCombo::saveFeedback(int feedback)
{
    QSettings().setValue(kFeedbackSetting, ControllerScheme::feedbackKey(ControllerScheme::Feedback(feedback)));
}
