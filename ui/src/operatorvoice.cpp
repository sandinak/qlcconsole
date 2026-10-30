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
#include <QDateTime>
#include <QFileInfo>
#include <QTimer>

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
const QString kDeviceIdSetting = QStringLiteral("operatorVoice/deviceId");
const QString kFeedbackSetting = QStringLiteral("controllerTools/feedback");
const QString kVoiceDetailSetting = QStringLiteral("controllerTools/voiceDetail");
const QString kSettleSetting = QStringLiteral("voice/settleMs");

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

OperatorVoice::OperatorVoice(QObject *parent)
    : QObject(parent)
    , m_settleTimer(new QTimer(this))
{
    m_settleTimer->setSingleShot(true);
    connect(m_settleTimer, &QTimer::timeout, this, &OperatorVoice::settleNow);
}

int OperatorVoice::settleMs() const
{
    return m_settleMs >= 0 ? m_settleMs : savedSettleMs();
}

int OperatorVoice::savedSettleMs()
{
    return qBound(0, QSettings().value(kSettleSetting, defaultSettleMs()).toInt(), 5000);
}

void OperatorVoice::saveSettleMs(int ms)
{
    QSettings().setValue(kSettleSetting, qMax(0, ms));
}

void OperatorVoice::say(const QString &text, int priority)
{
    if (text.trimmed().isEmpty())
        return;
    const bool where = priority <= ControllerScheme::WherePriority;
    const int settle = settleMs();
    /* Board Q171: a where line waits until the selection has settled --
       a newer one takes its place and the wait starts again. */
    if (priority == ControllerScheme::WherePriority && settle > 0)
    {
        m_settling = text;
        m_settleTimer->start(settle);
        return;
    }
    // Said at once: what was settling is older than this.
    if (priority == ControllerScheme::WhereNowPriority)
    {
        m_settling.clear();
        m_settleTimer->stop();
    }
    if (m_speaking && settle > 0 && m_speakingPriority <= ControllerScheme::WherePriority
        && (where || priority == ControllerScheme::ImportantPriority))
    {
        /* A where line being said is stale once a newer one is due, and
           waits for nothing important: cut it. A confirmation still waits
           its turn (it is never over, nor cut). */
        cut();
        if (where)
        {
            dropWaitingWhere();
            m_queue.append(qMakePair(text, priority));
            speakNext();
        }
        else
            start(text, priority);
        return;
    }
    if (m_speaking)
    {
        /* Never over the line being said. A where line still waiting is
           stale once a newer one comes; nothing else is ever dropped. */
        if (where)
            dropWaitingWhere();
        m_queue.append(qMakePair(text, priority));
        return;
    }
    start(text, priority);
}

void OperatorVoice::settleNow()
{
    m_settleTimer->stop();
    if (m_settling.isEmpty())
        return;
    const QString text = m_settling;
    m_settling.clear();
    dropWaitingWhere();
    if (!m_speaking)
    {
        start(text, ControllerScheme::WherePriority);
        return;
    }
    m_queue.append(qMakePair(text, ControllerScheme::WherePriority));
    // A stale where line being said: cut, and what waits goes on (in order).
    if (m_speakingPriority <= ControllerScheme::WherePriority)
    {
        cut();
        speakNext();
    }
}

void OperatorVoice::start(const QString &text, int priority)
{
    m_speaking = true;
    m_speakingPriority = priority;
    speakNow(text);
}

void OperatorVoice::cut()
{
    if (!m_speaking)
        return;
    // Its end is not "finished": what waits goes on from here, not twice.
    m_cutting = true;
    stopNow();
    m_cutting = false;
    m_speaking = false;
    ++m_cuts;
}

void OperatorVoice::dropWaitingWhere()
{
    for (int i = m_queue.size() - 1; i >= 0; --i)
        if (m_queue.at(i).second <= ControllerScheme::WherePriority)
            m_queue.removeAt(i);
}

void OperatorVoice::speakNext()
{
    if (m_speaking || m_queue.isEmpty())
        return;
    const QPair<QString, int> next = m_queue.takeFirst();
    start(next.first, next.second);
}

QStringList OperatorVoice::queued() const
{
    QStringList out;
    for (const QPair<QString, int> &line : m_queue)
        out << line.first;
    return out;
}

void OperatorVoice::stop()
{
    m_settleTimer->stop();
    m_settling.clear();
    m_queue.clear();
    if (m_speaking)
        stopNow();
    m_speaking = false;
}

void OperatorVoice::finished()
{
    if (m_cutting)
        return;
    m_speaking = false;
    speakNext();
}

QList<OperatorVoice::OutputDevice> OperatorVoice::parseOutputDevices(const QString &listing)
{
    QList<OutputDevice> out;
    // "   74 MacBook Pro Speakers"
    for (const QString &line : listing.split(QLatin1Char('\n')))
    {
        const QString t = line.trimmed();
        const int sp = t.indexOf(QLatin1Char(' '));
        if (sp > 0)
            out << OutputDevice{ t.left(sp), t.mid(sp + 1).trimmed() };
    }
    return out;
}

QList<OperatorVoice::OutputDevice> OperatorVoice::outputDeviceList(const QString &sayProgram)
{
    QString say = sayProgram;
#if defined(Q_OS_MACOS)
    if (say.isEmpty())
        say = QStandardPaths::findExecutable(QStringLiteral("say"));
#endif
    if (say.isEmpty())
        return QList<OutputDevice>();
    QProcess p;
    p.start(say, { QStringLiteral("-a"), QStringLiteral("?") });
    if (!p.waitForFinished(3000))
    {
        p.kill();
        p.waitForFinished(200);
        return QList<OutputDevice>();
    }
    return parseOutputDevices(QString::fromUtf8(p.readAllStandardOutput()));
}

QStringList OperatorVoice::outputDevices()
{
    QStringList out;
    for (const OutputDevice &d : outputDeviceList())
        out << d.name;
    return out;
}

int OperatorVoice::findOutputDevice(const QList<OutputDevice> &devices, const QString &id,
                                    const QString &name)
{
    if (!id.isEmpty())
        for (int i = 0; i < devices.size(); ++i)
            if (devices.at(i).id == id && (name.isEmpty() || devices.at(i).name == name))
                return i;
    if (!name.isEmpty())
        for (int i = 0; i < devices.size(); ++i)
            if (devices.at(i).name == name)
                return i;
    return -1;
}

QString OperatorVoice::outputDevice()
{
    return QSettings().value(kDeviceSetting).toString();
}

QString OperatorVoice::outputDeviceId()
{
    return QSettings().value(kDeviceIdSetting).toString();
}

void OperatorVoice::setOutputDevice(const QString &name, const QString &id)
{
    QSettings settings;
    if (name.isEmpty())
    {
        settings.remove(kDeviceSetting);
        settings.remove(kDeviceIdSetting);
        return;
    }
    settings.setValue(kDeviceSetting, name);
    if (id.isEmpty())
        settings.remove(kDeviceIdSetting);
    else
        settings.setValue(kDeviceIdSetting, id);
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

QString CommandVoice::missingDeviceNotice(const QString &name)
{
    return tr("The voice output %1 is not there: speaking on this computer.").arg(name);
}

bool CommandVoice::sendsToDevices() const
{
    // `say` takes an output (-a); it is the voice on a Mac.
    return QFileInfo(m_program).fileName() == QLatin1String("say");
}

QString CommandVoice::deviceArgument()
{
    const QString name = outputDevice();
    if (name.isEmpty() || !sendsToDevices())
        return QString();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_listedAt < 0 || now - m_listedAt >= m_checkInterval)
    {
        m_devices = outputDeviceList(m_program);
        m_listedAt = now;
    }
    const int at = findOutputDevice(m_devices, outputDeviceId(), name);
    if (at < 0)
        return QString();
    m_missingSaid = false;      // there (again): say so again if it goes
    return m_devices.at(at).id;
}

void CommandVoice::speakNow(const QString &text)
{
    const QString device = deviceArgument();
    QString line = text;
    if (device.isEmpty() && sendsToDevices() && !outputDevice().isEmpty() && !m_missingSaid)
    {
        // Chosen, and not there: this computer says it, and says why once.
        m_missingSaid = true;
        line = missingDeviceNotice(outputDevice()) + QLatin1Char(' ') + text;
    }
    start(line, device);
}

void CommandVoice::start(const QString &text, const QString &device)
{
    QProcess *p = new QProcess(this);
    // Done (or failed): the next line, if one is waiting.
    connect(p, &QProcess::finished, this, [this, p, text, device](int exitCode, QProcess::ExitStatus status) {
        p->deleteLater();
        if (m_process != p)
            return;
        m_process.clear();
        if (!device.isEmpty() && (exitCode != 0 || status != QProcess::NormalExit))
        {
            /* The output went between the look and the line (`say -a`
               fails, and the line was lost): this line on the system's
               output, said why, and the outputs looked at again next time. */
            m_listedAt = -1;
            QString line = text;
            if (!m_missingSaid)
            {
                m_missingSaid = true;
                line = missingDeviceNotice(outputDevice()) + QLatin1Char(' ') + text;
            }
            start(line, QString());
            return;
        }
        finished();
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
    // To the output chosen: the headsets' feed, say.
    if (!device.isEmpty())
        args << QStringLiteral("-a") << device;
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
    m_last = text;
    if (m_voice != nullptr && OperatorVoice::isEnabled())
        m_voice->say(text, priority);
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
                  "or both. Say the light (%1, or W) always speaks. Hold %1, or Shift+W, to switch.")
               .arg(pc != nullptr ? pc->padText(QStringLiteral("{RS}")) : QStringLiteral("RS")));
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

/*****************************************************************************
 * VoiceDetailCombo
 *****************************************************************************/

VoiceSettleCombo::VoiceSettleCombo(QWidget *parent)
    : QComboBox(parent)
{
    setObjectName(QStringLiteral("voiceSettle"));
    setFocusPolicy(Qt::NoFocus);
    addItem(tr("Off"), 0);
    addItem(tr("250 ms"), 250);
    addItem(tr("400 ms"), 400);
    addItem(tr("700 ms"), 700);
    setToolTip(tr("How long the voice waits after you stop moving before it says where you are, "
                  "so stepping through lights says only where you stopped. Off: it speaks on "
                  "every step. Saves, warnings and Say the light are never held."));
    const int ms = OperatorVoice::savedSettleMs();
    int at = findData(ms);
    if (at < 0)
        at = findData(OperatorVoice::defaultSettleMs());
    setCurrentIndex(at);
    connect(this, &QComboBox::currentIndexChanged, this, [this]() {
        OperatorVoice::saveSettleMs(currentData().toInt());
    });
}

VoiceDetailCombo::VoiceDetailCombo(ProgrammerController *pc, QWidget *parent)
    : QComboBox(parent)
    , m_pc(pc)
{
    setObjectName(QStringLiteral("voiceDetail"));
    setFocusPolicy(Qt::NoFocus);
    for (ControllerScheme::VoiceDetail d : { ControllerScheme::TerseVoice, ControllerScheme::StandardVoice,
                                             ControllerScheme::VerboseVoice })
        addItem(ControllerScheme::voiceDetailName(d), int(d));
    setToolTip(tr("How much the voice says. Terse: \"F1 Spot 3\", \"Saved\". Standard: \"Fixture 1, "
                  "Spot 3\", \"Saved fixture 1\". Verbose: \"Fixture 1 of 8, Spot 3\", \"Saved Spot 3\", "
                  "and each call's target and where it is. Warnings are always said in full."));
    sync();
    connect(this, &QComboBox::currentIndexChanged, this, [this]() {
        m_pc->setVoiceDetail(ControllerScheme::VoiceDetail(currentData().toInt()));
    });
    connect(m_pc, &ProgrammerController::voiceDetailChanged, this, &VoiceDetailCombo::sync);
}

void VoiceDetailCombo::sync()
{
    const bool was = blockSignals(true);
    setCurrentIndex(findData(int(m_pc->voiceDetail())));
    blockSignals(was);
}

int VoiceDetailCombo::savedDetail()
{
    return int(ControllerScheme::voiceDetailFromKey(QSettings().value(kVoiceDetailSetting).toString()));
}

void VoiceDetailCombo::saveDetail(int detail)
{
    QSettings().setValue(kVoiceDetailSetting,
                         ControllerScheme::voiceDetailKey(ControllerScheme::VoiceDetail(detail)));
}
