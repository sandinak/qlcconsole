/*
  Q Light Controller Plus
  universe.cpp

  Copyright (c) Massimo Callegari

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>
#include <QVarLengthArray>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <utility>
#include <math.h>

#if defined(Q_OS_MACOS)
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#include <pthread.h>
#elif defined(Q_OS_LINUX)
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#endif

#include "channelmodifier.h"
#include "inputoutputmap.h"
#include "genericfader.h"
#include "qlcioplugin.h"
#include "outputpatch.h"
#include "iodeferral.h"
#include "grandmaster.h"
#include "mastertimer.h"
#include "inputpatch.h"
#include "qlcmacros.h"
#include "universe.h"
#include "networkroute.h"
#include "function.h"
#include "qlcfile.h"
#include "utils.h"

#define RELATIVE_ZERO_8BIT   0x7F
#define RELATIVE_ZERO_16BIT  0x7F00

#define KXMLUniverseNormalBlend      QStringLiteral("Normal")
#define KXMLUniverseMaskBlend        QStringLiteral("Mask")
#define KXMLUniverseAdditiveBlend    QStringLiteral("Additive")
#define KXMLUniverseSubtractiveBlend QStringLiteral("Subtractive")

Universe::Universe(quint32 id, GrandMaster *gm, QObject *parent)
    : QThread(parent)
    , m_id(id)
    , m_name(QString("Universe %1").arg(id + 1))
    , m_grandMaster(gm)
    , m_passthrough(false)
    , m_monitor(false)
    , m_inputPatch(NULL)
    , m_fbPatch(NULL)
    , m_channelsMask(new QByteArray(UNIVERSE_SIZE, char(0)))
    , m_modifiedZeroValues(new QByteArray(UNIVERSE_SIZE, char(0)))
    , m_running(false)
    , m_usedChannels(0)
    , m_totalChannels(0)
    , m_totalChannelsChanged(false)
    , m_intensityChannelsChanged(false)
    , m_preGMValues(new QByteArray(UNIVERSE_SIZE, char(0)))
    , m_postGMValues(new QByteArray(UNIVERSE_SIZE, char(0)))
    , m_lastPostGMValues(new QByteArray(UNIVERSE_SIZE, char(0)))
    , m_blackoutValues(new QByteArray(UNIVERSE_SIZE, char(0)))
    , m_passthroughValues()
{
    m_modifiers.fill(NULL, UNIVERSE_SIZE);
    m_writtenFrame.reset(new std::atomic<quint32>[UNIVERSE_SIZE]);
    m_held.reset(new std::atomic<quint8>[UNIVERSE_SIZE]);
    m_pastGMFrame.reset(new std::atomic<quint32>[UNIVERSE_SIZE]);
    m_groupMaster.reset(new std::atomic<quint8>[UNIVERSE_SIZE]);
    m_pairRole.reset(new std::atomic<quint8>[UNIVERSE_SIZE]);
    m_sendLatency.reset(new std::atomic<quint32>[kSendLatencyBuckets]);
    resetSendLatency();
    m_letGoBy.reset(new std::atomic<quint32>[UNIVERSE_SIZE]);
    m_letGoFrame.reset(new std::atomic<quint32>[UNIVERSE_SIZE]);
    for (int i = 0; i < UNIVERSE_SIZE; i++)
    {
        m_letGoBy[i].store(0, std::memory_order_relaxed);
        m_letGoFrame[i].store(0, std::memory_order_relaxed);
    {
        m_groupMaster[i].store(UCHAR_MAX, std::memory_order_relaxed);
        m_pairRole[i].store(NotPaired, std::memory_order_relaxed);
    }
        m_writtenFrame[i].store(0, std::memory_order_relaxed);
        m_held[i].store(0, std::memory_order_relaxed);
        m_pastGMFrame[i].store(0, std::memory_order_relaxed);
    }

    // The live post-GM array is written in place, never detached: a reader
    // holding a share of it (a full-size mid(), a copy) never sees its
    // buffer swapped under it mid-frame (R8-4).
    m_post = m_postGMValues->data();

    connect(m_grandMaster, SIGNAL(valueChanged(uchar)),
            this, SLOT(slotGMValueChanged()));
}

Universe::~Universe()
{
    if (isRunning() == true)
    {
        // isRunning is inconsistent with m_running,
        // so double check if the thread is really in the run loop
        while (m_running == false)
            usleep(10000);

        m_running = false;
        // Wake the run loop immediately — it may be blocked on the semaphore
        // (up to ~2 ticks) waiting for the next frame; without this the wait
        // below can time out.
        m_semaphore.release(1);
        // Wait UNCONDITIONALLY for the thread to leave run(). The old wait(1000)
        // gave up after a second and then let ~QThread run on a still-running
        // thread, which Qt turns into a fatal "QThread: Destroyed while thread is
        // still running" abort on shutdown. The loop exits within one frame of
        // m_running=false, so this returns promptly.
        wait();
    }

    // Each line closed after the frame lock (iodeferral.h).
    IODeferral io;
    delete m_inputPatch;
    /* R9-1: each line closed with its closing frame (~OutputPatch), while
       this universe is still whole -- the thread is stopped, nothing else
       sends -- and under the frame lock, as every send is. */
    {
        QMutexLocker frameLocker(&m_frameMutex);
        int opCount = m_outputPatchList.count();
        for (int i = 0; i < opCount; i++)
        {
            OutputPatch *patch = m_outputPatchList.takeLast();
            delete patch;
        }
    }
    delete m_fbPatch;
}

void Universe::setName(QString name)
{
    if (name.isEmpty())
        m_name = QString("Universe %1").arg(m_id + 1);
    else
        m_name = name;
    emit nameChanged();
}

QString Universe::name() const
{
    return m_name;
}

void Universe::setID(quint32 id)
{
    m_id = id;
}

quint32 Universe::id() const
{
    return m_id;
}

ushort Universe::usedChannels()
{
    return m_usedChannels;
}

ushort Universe::totalChannels()
{
    return m_totalChannels;
}

bool Universe::hasChanged()
{
    bool changed =
        memcmp(m_lastPostGMValues->constData(), m_postGMValues->constData(), m_usedChannels) != 0;
    if (changed)
        memcpy(m_lastPostGMValues->data(), m_postGMValues->constData(), m_usedChannels);
    return changed;
}

void Universe::setPassthrough(bool enable)
{
    if (enable == m_passthrough)
        return;

    qDebug() << "Set universe" << id() << "passthrough to" << enable;

    disconnectInputPatch();

    if (enable && m_passthroughValues.isNull())
    {
        // When passthrough is disabled, we don't release the array, since it's only ~512 B and
        // we would have to synchronize with other threads

        // When enabling passthrough, make sure the array is allocated BEFORE m_passthrough is set to
        // true. That way we only have to check for m_passthrough, and do not need to check
        // m_passthroughValues.isNull()
        m_passthroughValues.reset(new QByteArray(UNIVERSE_SIZE, char(0)));
    }

    m_passthrough = enable;

    connectInputPatch();

    emit passthroughChanged();
}

bool Universe::passthrough() const
{
    return m_passthrough;
}

void Universe::setMonitor(bool enable)
{
    m_monitor = enable;
}

bool Universe::monitor() const
{
    return m_monitor;
}

void Universe::setInhibitOutput(bool enable)
{
    if (enable && !m_inhibitOutput.load(std::memory_order_relaxed))
    {
        // Blind: what the rig keeps is the frame from before it (stock);
        // a send that must go in Blind (sendHeld()) sends that frame.
        QMutexLocker frameLocker(&m_frameMutex);
        m_blindFrozen = QByteArray(m_postGMValues->constData(), m_usedChannels);
    }
    m_inhibitOutput.store(enable, std::memory_order_relaxed);
}

bool Universe::inhibitOutput() const
{
    return m_inhibitOutput.load(std::memory_order_relaxed);
}

void Universe::slotGMValueChanged()
{
    QMutexLocker frameLocker(&m_frameMutex);
    {
        for (int i = 0; i < m_intensityChannels.size(); ++i)
        {
            int channel = m_intensityChannels.at(i);
            updatePostGMValue(channel);
        }
    }

    if (m_grandMaster->channelMode() == GrandMaster::AllChannels)
    {
        for (int i = 0; i < m_nonIntensityChannels.size(); ++i)
        {
            int channel = m_nonIntensityChannels.at(i);
            updatePostGMValue(channel);
        }
    }
}

void Universe::setGroupMasters(const QByteArray &scales)
{
    QMutexLocker frameLocker(&m_frameMutex);
    for (int c = 0; c < UNIVERSE_SIZE; ++c)
    {
        const quint8 want = c < scales.size() ? quint8(scales.at(c)) : quint8(UCHAR_MAX);
        if (m_groupMaster[c].load(std::memory_order_relaxed) == want)
            continue;
        m_groupMaster[c].store(want, std::memory_order_relaxed);
        updatePostGMValue(c);
    }
}

uchar Universe::groupMaster(int channel) const
{
    if (channel < 0 || channel >= UNIVERSE_SIZE)
        return UCHAR_MAX;
    return m_groupMaster[channel].load(std::memory_order_relaxed);
}

/************************************************************************
 * Faders
 ************************************************************************/

int Universe::faderInsertPosition(int priority, bool onTop) const
{
    for (int i = m_faders.count() - 1; i >= 0; i--)
    {
        const QSharedPointer<GenericFader> &f = m_faders.at(i);
        if (f.isNull() || f->priority() > priority)
            continue;
        /* One kept on top of this priority stays above a newcomer: the
           effect scripts' fader over a scene started after it. Until
           0.4.0 the later scene went above, and a Show Tree preview
           (Override) hid its effects from the second cue selected on. */
        if (f->priority() == priority && f->onTopOfPriority() && !onTop)
            continue;
        return i + 1;
    }
    return 0;
}

QSharedPointer<GenericFader> Universe::requestTopFader(Universe::FaderPriority priority)
{
    QSharedPointer<GenericFader> fader = requestFader(priority);
    QMutexLocker fadersLocker(&m_fadersMutex);
    fader->setOnTopOfPriority(true);
    const int pos = m_faders.indexOf(fader);
    if (pos >= 0)
    {
        m_faders.removeAt(pos);
        m_faders.insert(faderInsertPosition(int(priority), true), fader);
    }
    return fader;
}

QSharedPointer<GenericFader> Universe::requestFader(Universe::FaderPriority priority)
{
    int insertPos = 0;
    QSharedPointer<GenericFader> fader = QSharedPointer<GenericFader>(new GenericFader());
    fader->setPriority(priority);

    {
        QMutexLocker fadersLocker(&m_fadersMutex);
        // Who it is, for "Why is this on?" (letGo()); never 0
        if (++m_nextFaderToken == 0)
            ++m_nextFaderToken;
        fader->setToken(m_nextFaderToken);
        insertPos = faderInsertPosition(int(priority), false);

        m_faders.insert(insertPos, fader);

        qDebug() << "[Universe]" << id() << ": Generic fader with priority" << fader->priority()
                 << "registered at pos" << insertPos << ", count" << m_faders.count();
    }
    return fader;
}

void Universe::dismissFader(QSharedPointer<GenericFader> fader)
{
    QMutexLocker fadersLocker(&m_fadersMutex);
    int index = m_faders.indexOf(fader);
    if (index >= 0)
    {
        noteDeparted(fader);
        m_faders.takeAt(index);
        fader.clear();
    }
}

void Universe::noteDeparted(const QSharedPointer<GenericFader> &fader)
{
    if (fader.isNull() || fader->token() == 0)
        return;
    fader->noteLetGo(this);
    Departed &d = m_departed[size_t(m_departedNext)];
    d.token = fader->token();
    d.name = fader->name();
    d.functionId = fader->parentFunctionID();
    d.priority = fader->priority();
    m_departedNext = (m_departedNext + 1) % int(m_departed.size());
}

Universe::LetGo Universe::letGo(int address)
{
    LetGo g;
    if (address < 0 || address >= UNIVERSE_SIZE)
        return g;
    QMutexLocker frameLocker(&m_frameMutex);
    const quint32 token = m_letGoBy[address].load(std::memory_order_relaxed);
    if (token == 0)
        return g;
    const quint32 at = m_letGoFrame[address].load(std::memory_order_relaxed);
    const quint32 written = m_writtenFrame[address].load(std::memory_order_relaxed);
    // Written since it let go: someone else has set it.
    if (qint32(written - at) > 0)
        return g;
    g.token = token;
    g.framesAgo = m_frame.load(std::memory_order_relaxed) - at;

    QMutexLocker fadersLocker(&m_fadersMutex);
    for (const QSharedPointer<GenericFader> &f : std::as_const(m_faders))
    {
        if (f.isNull() || f->token() != token)
            continue;
        g.alive = true;
        g.name = f->name();
        g.functionId = f->parentFunctionID();
        g.priority = f->priority();
        return g;
    }
    for (const Departed &d : m_departed)
    {
        if (d.token != token)
            continue;
        g.name = d.name;
        g.functionId = d.functionId;
        g.priority = d.priority;
        break;
    }
    return g;
}

void Universe::requestFaderPriority(QSharedPointer<GenericFader> fader, Universe::FaderPriority priority)
{
    QMutexLocker fadersLocker(&m_fadersMutex);
    int pos = m_faders.indexOf(fader);

    if (pos == -1)
        return;

    /* Out, then back in where requestFader() would put a new one of that
       priority: after the last fader at or below it. (Searched with the
       fader still in the list, a lower priority than its own never found
       a place and was not set.) */
    m_faders.removeAt(pos);
    fader->setPriority(priority);
    const int newPos = faderInsertPosition(int(priority), fader->onTopOfPriority());
    m_faders.insert(newPos, fader);

    if (newPos != pos)
        qDebug() << "[Universe]" << id() << ": Generic fader moved from" << pos
                 << "to" << newPos << ". Count:" << m_faders.count();
}

QList<QSharedPointer<GenericFader> > Universe::faders()
{
    // Return a copy under the lock: the timer thread requests/dismisses faders
    // and the Universe thread's processFaders() removes them concurrently.
    QMutexLocker fadersLocker(&m_fadersMutex);
    return m_faders;
}

void Universe::setFaderPause(quint32 functionID, bool enable)
{
    QMutexLocker fadersLocker(&m_fadersMutex);
    QMutableListIterator<QSharedPointer<GenericFader> > it(m_faders);
    while (it.hasNext())
    {
        QSharedPointer<GenericFader> fader = it.next();
        if (fader.isNull() || fader->parentFunctionID() != functionID)
            continue;

        fader->setPaused(enable);
    }
}

void Universe::setFaderFadeOut(int fadeTime)
{
    QMutexLocker fadersLocker(&m_fadersMutex);
    foreach (QSharedPointer<GenericFader> fader, m_faders)
    {
        if (!fader.isNull() && fader->parentFunctionID() != Function::invalidId())
            fader->setFadeOut(true, uint(fadeTime));
    }
}

void Universe::setFunctionsHeld(const QSet<quint32> &functionIDs)
{
    QMutexLocker fadersLocker(&m_fadersMutex);
    m_heldFunctions = functionIDs;
}

static qint64 sendClockNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

void Universe::tick()
{
    m_tickedNs.store(sendClockNs(), std::memory_order_relaxed);
    m_semaphore.release(1);
}

QVector<quint32> Universe::sendLatencyHistogram() const
{
    QVector<quint32> counts(kSendLatencyBuckets);
    for (int i = 0; i < kSendLatencyBuckets; i++)
        counts[i] = m_sendLatency[i].load(std::memory_order_relaxed);
    return counts;
}

void Universe::resetSendLatency()
{
    for (int i = 0; i < kSendLatencyBuckets; i++)
        m_sendLatency[i].store(0, std::memory_order_relaxed);
}

/* How the universe threads are scheduled. They do the sending, and woken by
   a real-time timer they used to start at ordinary priority: on a busy
   computer the timer then waits on them (docs/performance.md#universe-threads
   has the A/B). The setting is per computer (Settings ▸ Performance);
   QLC_UNIVERSE_SCHED (standard|qos|rt) overrides it, for measuring. Each
   thread applies it as it starts and again at the frame after a change
   (s_priorityGeneration). */
static std::atomic<int> s_universeThreadScheduling { Universe::ThreadsNormal };
static std::atomic<int> s_threadPrioritySet { -1 };    // -1: the default
static std::atomic<int> s_priorityGeneration { 0 };

Universe::ThreadScheduling Universe::threadScheduling()
{
    return ThreadScheduling(s_universeThreadScheduling.load(std::memory_order_relaxed));
}

/* Real-time on macOS and Linux: measured on the show's rig (an M4 Pro at 80
   and 160 universes: the send's p99.9 a third, no more CPU; a Pi 5 at 160:
   the tick's p99 halved for ~10% more CPU). Windows: not measured. */
Universe::ThreadPriority Universe::defaultThreadPriority()
{
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    return PriorityRealtime;
#else
    return PriorityStandard;
#endif
}

#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
/* One warning per run, not one per universe thread */
static void warnOnce(const char *what, int code)
{
    static std::atomic<bool> warned { false };
    if (!warned.exchange(true))
        qWarning() << "Universe threads:" << what << code << "- they run at normal priority";
}
#endif

bool Universe::threadPrioritySupported(ThreadPriority priority)
{
    switch (priority)
    {
        case PriorityStandard:
            return true;
        case PriorityResponsive:
#if defined(Q_OS_MACOS)
            return true;
#else
            return false;
#endif
        case PriorityRealtime:
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
            return true;
#else
            return false;
#endif
    }
    return false;
}

static int envThreadPriority()
{
    const QByteArray mode = qgetenv("QLC_UNIVERSE_SCHED").trimmed().toLower();
    if (mode.isEmpty())
        return -1;
    if (mode == "rt" || mode == "realtime")
        return Universe::PriorityRealtime;
    if (mode == "qos" || mode == "responsive")
        return Universe::PriorityResponsive;
    return Universe::PriorityStandard;   // "standard", "normal", anything else
}

bool Universe::threadPriorityOverridden()
{
    return envThreadPriority() >= 0;
}

void Universe::setThreadPriority(ThreadPriority priority)
{
    if (s_threadPrioritySet.exchange(int(priority), std::memory_order_relaxed) != int(priority))
        s_priorityGeneration.fetch_add(1, std::memory_order_release);
}

Universe::ThreadPriority Universe::threadPriority()
{
    const int env = envThreadPriority();
    if (env >= 0)
        return ThreadPriority(env);
    const int set = s_threadPrioritySet.load(std::memory_order_relaxed);
    ThreadPriority p = set >= 0 ? ThreadPriority(set) : defaultThreadPriority();
    return threadPrioritySupported(p) ? p : PriorityStandard;
}

static void applyUniverseThreadScheduling(Universe::ThreadPriority priority, int previous)
{
    const bool first = previous < 0;
    /* A new thread left Standard is the thread as ever: nothing asked */
    if (first && priority == Universe::PriorityStandard)
    {
        s_universeThreadScheduling.store(Universe::ThreadsNormal, std::memory_order_relaxed);
        return;
    }
#if defined(Q_OS_MACOS)
    const thread_act_t self = pthread_mach_thread_np(pthread_self());
    /* Undo only what was asked before: a thread given an explicit Mach
       policy (even the standard one) can no longer take a QoS class */
    if (previous == Universe::PriorityRealtime)
    {
        thread_standard_policy_data_t standard;
        thread_policy_set(self, THREAD_STANDARD_POLICY, reinterpret_cast<thread_policy_t>(&standard),
                          THREAD_STANDARD_POLICY_COUNT);
    }
    if (previous == Universe::PriorityResponsive && priority != Universe::PriorityResponsive)
        pthread_set_qos_class_self_np(QOS_CLASS_DEFAULT, 0);
    if (priority == Universe::PriorityStandard)
    {
        s_universeThreadScheduling.store(Universe::ThreadsNormal, std::memory_order_relaxed);
    }
    else if (priority == Universe::PriorityResponsive)
    {
        const int rc = pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
        s_universeThreadScheduling.store(rc == 0 ? Universe::ThreadsElevated : Universe::ThreadsRefused,
                                         std::memory_order_relaxed);
        if (rc != 0)
            warnOnce("user-interactive QoS refused:", rc);
    }
    else
    {
        mach_timebase_info_data_t tb;
        if (mach_timebase_info(&tb) != KERN_SUCCESS || tb.numer == 0)
        {
            s_universeThreadScheduling.store(Universe::ThreadsRefused, std::memory_order_relaxed);
            return;
        }
        const double toAbs = double(tb.denom) / double(tb.numer);
        thread_time_constraint_policy_data_t policy;
        policy.period = uint32_t(1.0e9 / MasterTimer::frequency() * toAbs);
        policy.computation = uint32_t(500000.0 * toAbs);   /* 0.5 ms */
        policy.constraint = uint32_t(5000000.0 * toAbs);   /* 5 ms */
        policy.preemptible = 1;
        const kern_return_t kr = thread_policy_set(self, THREAD_TIME_CONSTRAINT_POLICY,
                                                   reinterpret_cast<thread_policy_t>(&policy),
                                                   THREAD_TIME_CONSTRAINT_POLICY_COUNT);
        s_universeThreadScheduling.store(kr == KERN_SUCCESS ? Universe::ThreadsRealtime : Universe::ThreadsRefused,
                                         std::memory_order_relaxed);
        if (kr != KERN_SUCCESS)
            warnOnce("time-constraint policy refused:", int(kr));
    }
#elif defined(Q_OS_LINUX)
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    if (priority != Universe::PriorityRealtime)
    {
        pthread_setschedparam(pthread_self(), SCHED_OTHER, &sp);
        s_universeThreadScheduling.store(Universe::ThreadsNormal, std::memory_order_relaxed);
        return;
    }
    /* One level under the timer's (mastertimer-unix.cpp: the maximum - 10,
       or the user's rtprio limit when that is lower) */
    int prio = sched_get_priority_max(SCHED_FIFO) - 11;
    struct rlimit rl;
    if (getrlimit(RLIMIT_RTPRIO, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY
        && rl.rlim_cur > 0 && rlim_t(prio) >= rl.rlim_cur)
        prio = int(rl.rlim_cur) - 1;
    sp.sched_priority = qMax(1, prio);
    const int rc = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    s_universeThreadScheduling.store(rc == 0 ? Universe::ThreadsRealtime : Universe::ThreadsRefused,
                                     std::memory_order_relaxed);
    if (rc != 0)
        warnOnce("SCHED_FIFO refused (no rtprio limit for this user):", rc);
#else
    Q_UNUSED(priority)
    Q_UNUSED(first)
#endif
}

void Universe::processFaders()
{
    /* One frame at a time, and never while the roles or the value arrays
       are being reallocated (see m_frameMutex). Released before the
       signal: its receivers are free to call back in. */
    QMutexLocker frameLocker(&m_frameMutex);

    flushInput();
    zeroIntensityChannels();
    if (m_overrideWritten.size() != UNIVERSE_SIZE)
        m_overrideWritten = QByteArray(UNIVERSE_SIZE, char(0));
    else
        m_overrideWritten.fill(char(0));

    /* Show only: live or not is read ONCE, for the whole frame -- a mode
       switch landing between the faders and the send must not send one
       frame of a preview unheld. */
    const bool showOnlyLiveNow = showOnlyLive();
    // Live with Show-only channels: S0, each built from scratch this frame.
    const bool keepShowOnly = showOnlyChannelCount() > 0 && showOnlyLiveNow;
    /* S0: every frame, live, each Show-only channel starts at its safe
       value; only the writers that count now (Universe::writeFader())
       put anything on it. Nothing a writer left -- a cue that let go, an
       LTP level, a writer held since -- carries over. Not live, only the
       channels just made Show only (the frame is held as it is sent). */
    if (keepShowOnly || m_showOnlyReset.load(std::memory_order_relaxed))
    {
        QMutexLocker locker(&m_outputRulesMutex);
        const QVector<int> &reset = keepShowOnly ? m_showOnlyChannels : m_showOnlyResetChannels;
        // Q326: nothing held (every frame until a stop) reads the safe values only.
        const bool anyHeld = m_showOnlyHeldCount > 0;
        for (int c : reset)
        {
            const char rest = anyHeld ? char(showOnlyRestLocked(c)) : m_showOnlySafe.at(c);
            (*m_preGMValues)[c] = rest;
            (*m_blackoutValues)[c] = rest;
            updatePostGMValue(c);
        }
        m_showOnlyResetChannels.clear();
        m_showOnlyReset.store(false, std::memory_order_relaxed);
    }

    {
        QMutexLocker fadersLocker(&m_fadersMutex);
        QMutableListIterator<QSharedPointer<GenericFader> > it(m_faders);
        while (it.hasNext())
        {
            QSharedPointer<GenericFader> fader = it.next(); //m_faders.at(i);
            if (fader.isNull())
                continue;

            // destroy a fader if it's been requested
            // and it's not fading out
            if (fader->deleteRequested() && !fader->isFadingOut())
            {
                noteDeparted(fader);
                fader->removeAll();
                it.remove();
                fader.clear();
                continue;
            }

            if (fader->isEnabled() == false)
                continue;
            // Held off stage (setFunctionsHeld): running, unseen.
            if (!m_heldFunctions.isEmpty() && m_heldFunctions.contains(fader->parentFunctionID()))
                continue;

            //qDebug() << "Processing fader" << fader->name() << fader->channelsCount();
            writeFader(fader.data(), keepShowOnly);
        }
    }

    applyAutoOpen();
    applyOnAtOpen();
    // S0: a Show-only channel no writer counted on is at its safe value,
    // never GM-scaled (as a held frame sends it).
    if (keepShowOnly)
        finishShowOnlyFrame();
    // Writes from here on (the DMX sources, before the next frame) belong
    // to the next frame.
    m_frame.fetch_add(1, std::memory_order_relaxed);

    bool dataChanged = hasChanged();
    // A copy of its own, never a share of the live array (whose buffer the
    // frame writes in place, never reallocated: m_post).
    QByteArray postGM(m_postGMValues->constData(), m_usedChannels);
    /* The fixtures held to a maximum draw, on what goes out -- never on the
       stored values, which only the channels written this frame refresh:
       scaled there, an unchanged colour would shrink again every frame. */
    if (applyPowerLimits(postGM) && postGM != m_lastLimited)
    {
        m_lastLimited = postGM;
        dataChanged = true;
    }
    // S0: what this frame built for the Show-only channels is what every
    // send until the next frame has there (buildShowOnly()).
    if (keepShowOnly)
        recordShowOnlyOut(postGM);
    /* Show only, held (Design, Blind): what goes out has the channels at
       their safe value (dumpOutputFrame()); what the rig views get
       (universeWritten) keeps what they would be. */
    dumpOutputFrame(postGM, dataChanged, showOnlyLiveNow);
    const qint64 ticked = m_tickedNs.load(std::memory_order_relaxed);
    if (ticked != 0)
    {
        const qint64 tookUs = (sendClockNs() - ticked) / 1000;
        const int bucket = int(qBound(qint64(0), tookUs / kSendLatencyBucketUs,
                                      qint64(kSendLatencyBuckets - 1)));
        m_sendLatency[bucket].fetch_add(1, std::memory_order_relaxed);
    }
    frameLocker.unlock();

    if (dataChanged)
        emit universeWritten(id(), postGM);
}

void Universe::setPowerLimits(const QVector<PowerLimit> &limits)
{
    QMutexLocker locker(&m_powerMutex);
    m_powerLimits = limits;
    if (limits.isEmpty())
        m_powerHeldBack.clear();
}

QHash<quint32, QPair<double, double> > Universe::powerHeldBack() const
{
    QMutexLocker locker(&m_powerMutex);
    return m_powerHeldBack;
}

bool Universe::applyPowerLimits(QByteArray &out)
{
    QMutexLocker locker(&m_powerMutex);
    if (m_powerLimits.isEmpty())
        return false;
    QHash<quint32, QPair<double, double> > held;
    foreach (const PowerLimit &lim, m_powerLimits)
    {
        auto value = [&out, &lim](int offset) {
            const int a = lim.base + offset;
            return (a >= 0 && a < out.size()) ? uchar(out.at(a)) : uchar(0);
        };
        const double asks = lim.model.amps(value);
        const double k = lim.model.limitFactor(asks);
        if (k >= 1.0)
            continue;
        foreach (const PowerModel::Emitter &e, lim.model.emitters)
        {
            const int a = lim.base + e.offset;
            // A relay is on or off: scaled, it would be switched off (or
            // chatter), not dimmed. It is left as it is.
            if (a >= 0 && a < out.size() && (m_channelsMask->at(a) & SwitchOutput) == 0)
                out[a] = char(int(uchar(out.at(a)) * k));
        }
        held.insert(lim.fixtureId, qMakePair(asks, lim.model.amps(value)));
    }
    m_powerHeldBack = held;
    return true;
}

/************************************************************************
 * Output rules
 ************************************************************************/

void Universe::setOutputRules(const QByteArray &bits)
{
    QVector<int> newlyShowOnly;
    const bool hadShowOnly = showOnlyChannelCount() > 0;
    {
        QMutexLocker locker(&m_outputRulesMutex);
        QVector<int> onAtOpen, showOnly;
        for (int c = 0; c < UNIVERSE_SIZE; c++)
        {
            // uchar: ShowOnlyOutput is the top bit, and a char compare would
            // see it sign-extended on one side only.
            const uchar want = c < bits.size() ? uchar(uchar(bits.at(c)) & OutputRuleBits) : uchar(0);
            const uchar now = uchar(m_channelsMask->at(c));
            if (want & OnAtOpenOutput)
                onAtOpen << c;
            if (want & ShowOnlyOutput)
                showOnly << c;
            if ((now & OutputRuleBits) == want)
                continue;
            // One byte write: the role bits (HTP/LTP/Intensity) never vanish.
            (*m_channelsMask)[c] = char((now & ~OutputRuleBits) | want);
            if ((want & ShowOnlyOutput) && (now & ShowOnlyOutput) == 0)
                newlyShowOnly << c;
            // Made Show only while live: what is stored there (a preview's
            // last LTP value) must not be what goes out; the next frame starts
            // it from safe, as going live does.
            if ((want & ShowOnlyOutput) && (now & ShowOnlyOutput) == 0 && showOnlyLive())
            {
                m_showOnlyResetChannels << c;
                m_showOnlyReset.store(true, std::memory_order_relaxed);
            }
            if ((now & OnAtOpenOutput) && (want & OnAtOpenOutput) == 0)
                m_onAtOpenRestore << c;
            if (want & (GMExempt | SwitchOutput | OnAtOpenOutput | ShowOnlyOutput))
            {
                if (c >= m_totalChannels)
                {
                    m_totalChannels = c + 1;
                    m_totalChannelsChanged = true;
                }
                if (c >= m_usedChannels)
                    m_usedChannels = c + 1;
            }
            updatePostGMValue(c);
        }
        m_onAtOpenChannels = onAtOpen;
        m_showOnlyChannels = showOnly;
        m_showOnlySentDirty = true;     // the next frame is a changed one
        if (m_showOnlySafe.size() != UNIVERSE_SIZE)
            m_showOnlySafe = QByteArray(UNIVERSE_SIZE, char(0));
        m_showOnlyCount.store(int(showOnly.size()), std::memory_order_relaxed);
        // S0: a channel made Show only now takes a stamp of its own; only a
        // writer stamped after it counts there (a cue stack, a fader, a
        // desk channel moved for what was patched before does not).
        if (!newlyShowOnly.isEmpty())
        {
            const quint32 mark = GenericFader::newStamp();
            for (int c : std::as_const(newlyShowOnly))
            {
                m_showOnlyMark[c] = mark;
                setHoldingLocked(c, false);
                setShowOnlyOutSafeLocked(c);
            }
        }
        updateMaxMarkLocked();
    }
    // N2, N4 (outputpatch.h): the I/O map holds or lets go its hardware outputs.
    if (hadShowOnly == false && showOnlyChannelCount() > 0)
        emit showOnlyStarted();
    else if (hadShowOnly && showOnlyChannelCount() == 0)
        emit showOnlyEnded();
}

void Universe::updateMaxMarkLocked()
{
    quint32 maxMark = 0;
    for (int c : std::as_const(m_showOnlyChannels))
        maxMark = qMax(maxMark, m_showOnlyMark.at(c));
    m_showOnlyMaxMark.store(maxMark, std::memory_order_relaxed);
}

void Universe::setShowOnlySafeValues(const QByteArray &safe)
{
    QMutexLocker locker(&m_outputRulesMutex);
    QByteArray full(UNIVERSE_SIZE, char(0));
    memcpy(full.data(), safe.constData(), size_t(qMin(int(safe.size()), UNIVERSE_SIZE)));
    if (full != m_showOnlySafe)
        m_showOnlySentDirty = true;
    m_showOnlySafe = full;
}

void Universe::setShowOnlyLive(bool live)
{
    if (live == false)
    {
        m_showOnlyLive.store(false, std::memory_order_relaxed);
        return;
    }
    if (showOnlyLive())
        return;
    /* Going live: whatever a Design preview, Locate or a test left stored on
       a Show-only channel (an LTP value never sent) must not be what the
       first live send has -- the next frame, or a send between two frames
       (leaving Blind, Blackout). Put back to safe NOW, between two frames,
       and only then live. */
    QMutexLocker frameLocker(&m_frameMutex);
    {
        QMutexLocker locker(&m_outputRulesMutex);
        for (int c : std::as_const(m_showOnlyChannels))
        {
            (*m_preGMValues)[c] = char(showOnlyRestLocked(c));
            (*m_blackoutValues)[c] = char(showOnlyRestLocked(c));
            updatePostGMValue(c);
            setShowOnlyOutSafeLocked(c);
        }
    }
    m_showOnlyLive.store(true, std::memory_order_relaxed);
}

void Universe::cutShowOnly()
{
    /* The emergency stop (E1): what is stored on a Show-only channel goes
       back to safe NOW, between two frames, as going live does -- a pan or
       tilt held by holdShowOnlyPositions() to where it was. */
    QMutexLocker frameLocker(&m_frameMutex);
    QMutexLocker locker(&m_outputRulesMutex);
    for (int c : std::as_const(m_showOnlyChannels))
    {
        (*m_preGMValues)[c] = char(showOnlyRestLocked(c));
        (*m_blackoutValues)[c] = char(showOnlyRestLocked(c));
        updatePostGMValue(c);
        setShowOnlyOutSafeLocked(c);
    }
}

void Universe::setShowOnlyPositions(const QByteArray &mask)
{
    QMutexLocker locker(&m_outputRulesMutex);
    for (int c = 0; c < UNIVERSE_SIZE; c++)
    {
        const bool holds = c < mask.size() && mask.at(c) != 0;
        m_showOnlyHoldsPos[c] = char(holds ? 1 : 0);
        if (!holds)
            setHoldingLocked(c, false);
    }
}

void Universe::setShowOnlyHoldsPosition(int channel, bool holds)
{
    if (channel < 0 || channel >= UNIVERSE_SIZE)
        return;
    QMutexLocker locker(&m_outputRulesMutex);
    m_showOnlyHoldsPos[channel] = char(holds ? 1 : 0);
    if (!holds)
        setHoldingLocked(channel, false);
}

void Universe::holdShowOnlyPositions()
{
    /* Q326: the emergency stop leaves a Show-only head where it is. Each
       pan and tilt channel (FixtureRest::holdsPosition(), marked by
       OutputRuleKeeper) rests from now on at what went out there last --
       what the last live frame built, or, held (Design, Blind), what the
       held frames send already -- until something that counts moves it.
       Taken before the stop's new session, so no frame between the two
       sends a head to its safe value. */
    QMutexLocker frameLocker(&m_frameMutex);
    QMutexLocker locker(&m_outputRulesMutex);
    const bool sent = showOnlyHeld() == false;
    for (int c : std::as_const(m_showOnlyChannels))
    {
        if (m_showOnlyHoldsPos.at(c) == 0)
            continue;
        const char value = sent ? m_showOnlyPre.at(c) : char(showOnlyRestLocked(c));
        m_showOnlyHoldValue[c] = value;
        setHoldingLocked(c, true);
    }
}

void Universe::setHoldingLocked(int c, bool on)
{
    if (bool(m_showOnlyHolding.at(c)) == on)
        return;
    m_showOnlyHolding[c] = char(on ? 1 : 0);
    m_showOnlyHeldCount += on ? 1 : -1;
}

bool Universe::showOnlyHoldingPosition(int channel)
{
    if (channel < 0 || channel >= UNIVERSE_SIZE)
        return false;
    QMutexLocker locker(&m_outputRulesMutex);
    return m_showOnlyHolding.at(channel) != 0;
}

uchar Universe::showOnlyRestLocked(int c) const
{
    // Nothing held (every frame but after a stop): the safe value, one compare.
    if (m_showOnlyHeldCount == 0)
        return uchar(m_showOnlySafe.at(c));
    return uchar(m_showOnlyHolding.at(c) ? m_showOnlyHoldValue.at(c) : m_showOnlySafe.at(c));
}

void Universe::markShowOnly(int channel, bool on, uchar safe, bool remark)
{
    if (channel < 0 || channel >= UNIVERSE_SIZE)
        return;
    bool newlyOn = false;
    const bool hadShowOnly = showOnlyChannelCount() > 0;
    {
        QMutexLocker locker(&m_outputRulesMutex);
        const uchar now = uchar(m_channelsMask->at(channel));
        if (bool(now & ShowOnlyOutput) == on && (!on || uchar(m_showOnlySafe.at(channel)) == safe)
            && !(on && remark))
            return;
        // R8-3: a Show-only fixture moved or re-moded takes a fresh mark on
        // all its channels, those already Show only too.
        newlyOn = on && ((now & ShowOnlyOutput) == 0 || remark);
        if (m_showOnlySafe.size() != UNIVERSE_SIZE)
            m_showOnlySafe = QByteArray(UNIVERSE_SIZE, char(0));
        m_showOnlySafe[channel] = char(on ? safe : 0);
        (*m_channelsMask)[channel] = char(on ? (now | ShowOnlyOutput) : (now & ~ShowOnlyOutput));
        // The channel list in order (buildShowOnly() stops at the frame's end).
        const auto at = std::lower_bound(m_showOnlyChannels.begin(), m_showOnlyChannels.end(), channel);
        const bool listed = at != m_showOnlyChannels.end() && *at == channel;
        if (on && !listed)
            m_showOnlyChannels.insert(at, channel);
        else if (!on && listed)
            m_showOnlyChannels.erase(at);
        m_showOnlyCount.store(int(m_showOnlyChannels.size()), std::memory_order_relaxed);
        m_showOnlySentDirty = true;
        if (on)
        {
            if (channel >= m_totalChannels)
            {
                m_totalChannels = channel + 1;
                m_totalChannelsChanged = true;
            }
            if (channel >= m_usedChannels)
                m_usedChannels = channel + 1;
            // Live: what is stored there is not what the next send has.
            if (showOnlyLive() && (now & ShowOnlyOutput) == 0)
            {
                m_showOnlyResetChannels << channel;
                m_showOnlyReset.store(true, std::memory_order_relaxed);
            }
        }
        if (on)
        {
            // R8-2: Show only wins on a channel shared with another
            // fixture -- never on when not in use, never past Blackout.
            (*m_channelsMask)[channel] = char(uchar(m_channelsMask->at(channel))
                                              & ~(OnAtOpenOutput | BlackoutExempt));
            m_onAtOpenChannels.removeAll(channel);
        }
        // A fresh mark or no longer Show only: no stop's position held.
        if (newlyOn || !on)
            setHoldingLocked(channel, false);
        if (newlyOn)
        {
            m_showOnlyMark[channel] = GenericFader::newStamp();     // S0
            setShowOnlyOutSafeLocked(channel);
        }
        updateMaxMarkLocked();
        updatePostGMValue(channel);
    }
    // N2, N4 (outputpatch.h): the I/O map holds or lets go its hardware outputs.
    if (hadShowOnly == false && showOnlyChannelCount() > 0)
        emit showOnlyStarted();
    else if (hadShowOnly && showOnlyChannelCount() == 0)
        emit showOnlyEnded();
}

bool Universe::showOnlyHeld() const
{
    return showOnlyChannelCount() > 0
           && (showOnlyLive() == false || m_inhibitOutput.load(std::memory_order_relaxed));
}

void Universe::writeFader(GenericFader *fader, bool keepShowOnly)
{
    if (!keepShowOnly)
    {
        fader->write(this);
        return;
    }
    /* S0: a writer counts on a Show-only channel only while its stamp is
       of this session and later than the channel's (made Show only at:
       m_showOnlyMark). One that counts on all of them writes as any fader
       does -- the usual case, one compare. */
    const quint32 session = GenericFader::liveEpoch();
    const bool perChannel = fader->channelStamped() && !fader->holdsShowOnly();
    const quint32 stamp = perChannel ? 0 : fader->showStamp();
    if (!perChannel && stamp >= session && stamp > m_showOnlyMaxMark.load(std::memory_order_relaxed))
    {
        fader->write(this);
        return;
    }
    /* Not on all of them: it contributes nothing where it does not count
       -- no value, no flag (past the Grand Master), not even for a moment
       (writeBlocked(): every write entry point drops it). Nothing to undo. */
    // Only for this write: set, written, cleared again (a channel no longer
    // Show only never stays blocked).
    QVarLengthArray<int, 64> blocked;
    {
        QMutexLocker locker(&m_outputRulesMutex);
        for (int c : std::as_const(m_showOnlyChannels))
        {
            const quint32 st = perChannel ? fader->channelStamp(c) : stamp;
            if (!(st >= session && st > m_showOnlyMark.at(c)))
            {
                m_showOnlyBlocked[c] = char(1);
                blocked.append(c);
            }
        }
    }
    m_writeGuard = !blocked.isEmpty();
    fader->write(this);
    m_writeGuard = false;
    for (int c : blocked)
        m_showOnlyBlocked[c] = char(0);
}

void Universe::buildShowOnly(QByteArray &frame, bool held, bool blackout)
{
    /* S0, for every send: a Show-only channel goes out at its safe value
       (held, Blackout -- which always wins, Ignore Blackout or not) or at
       what the last frame built there from the writers that count
       (m_showOnlyOut) -- never at what the frame being sent happens to
       carry (a copy of the values taken mid-frame, a plugin's own). */
    QMutexLocker locker(&m_outputRulesMutex);
    for (int c : std::as_const(m_showOnlyChannels))
    {
        if (c >= frame.size())
            break;              // in order: the rest are past the frame too
        frame[c] = (held || blackout) ? char(showOnlySafeOut(c)) : m_showOnlyOut.at(c);
    }
}

uchar Universe::showOnlySafeOut(int c)
{
    // Through its modifier, as any value it sends (on an inverted channel a
    // raw 0 would be full on); never scaled by the Grand Master.
    return applyModifiers(c, showOnlyRestLocked(c));
}

void Universe::setShowOnlyOutSafeLocked(int c)
{
    m_showOnlyOut[c] = char(showOnlySafeOut(c));
    m_showOnlyPre[c] = char(showOnlyRestLocked(c));
}

void Universe::finishShowOnlyFrame()
{
    QMutexLocker locker(&m_outputRulesMutex);
    const bool anyHeld = m_showOnlyHeldCount > 0;
    for (int c : std::as_const(m_showOnlyChannels))
    {
        const char pre = m_preGMValues->at(c);
        const char rest = anyHeld ? char(showOnlyRestLocked(c)) : m_showOnlySafe.at(c);
        if (pre == rest)
            m_post[c] = char(applyModifiers(c, uchar(rest)));     // showOnlySafeOut()
        // Q326: a head held by the stop is driven again once something
        // that counts moves it; let go, it rests at its safe value again.
        else if (anyHeld)
            setHoldingLocked(c, false);
        m_showOnlyPre[c] = pre;
    }
}

void Universe::recordShowOnlyOut(const QByteArray &postGM)
{
    QMutexLocker locker(&m_outputRulesMutex);
    for (int c : std::as_const(m_showOnlyChannels))
        m_showOnlyOut[c] = c < postGM.size() ? postGM.at(c) : char(showOnlySafeOut(c));
}

uchar Universe::fadeStartValue(int address)
{
    /* A fade starting on a Show-only channel starts from what the last S0
       frame built there, never from what is stored mid-frame. */
    if (address >= 0 && address < UNIVERSE_SIZE
        && (uchar(m_channelsMask->at(address)) & ShowOnlyOutput) && showOnlyLive())
    {
        QMutexLocker locker(&m_outputRulesMutex);
        return uchar(m_showOnlyPre.at(address));
    }
    return preGMValue(address);
}

void Universe::sendCurrent()
{
    /* A send between two frames (Blackout, leaving Blind, a maintenance
       send): under the frame lock, the stored values as the last frame left
       them; its Show-only channels through the S0 builder. */
    QMutexLocker frameLocker(&m_frameMutex);
    dumpOutputFrame(QByteArray(m_postGMValues->constData(), m_usedChannels), true, showOnlyLive());
}

void Universe::sendHeld()
{
    /* The emergency stop's send, entering Blind, Blackout in Blind: always
       sent, Blind or not. The Show-only channels at safe; the others as
       they are -- in Blind, frozen at the frame from before Blind. */
    QMutexLocker frameLocker(&m_frameMutex);
    if (m_notOursWhole.load(std::memory_order_relaxed) || m_outputPatchList.isEmpty()
        || m_loadHold.load(std::memory_order_relaxed))
        return;
    QByteArray frame = heldBaseFrame();
    if (showOnlyChannelCount() > 0)
    {
        buildShowOnly(frame, true, false);
        showOnlySentChanged(frame);
    }
    QByteArray blackout;
    bool anyBlackout = false;
    foreach (OutputPatch *op, m_outputPatchList)
        anyBlackout = anyBlackout || op->blackout();
    if (anyBlackout)
    {
        blackout = heldBlackoutFrame(frame);
        if (showOnlyChannelCount() > 0)
            buildShowOnly(blackout, true, true);
    }
    {
        QMutexLocker locker(&m_notOursMutex);
        zeroNotOurs(frame);
        if (anyBlackout)
            zeroNotOurs(blackout);
    }
    foreach (OutputPatch *op, m_outputPatchList)
    {
        if (m_totalChannelsChanged == true)
            op->setPluginParameter(PLUGIN_UNIVERSECHANNELS, m_totalChannels);
        op->dump(m_id, op->blackout() ? blackout : frame, true);
    }
    m_totalChannelsChanged = false;
}

bool Universe::closingFrame(QByteArray &frame, bool blackout)
{
    QMutexLocker frameLocker(&m_frameMutex);
    if (showOnlyChannelCount() == 0 || m_notOursWhole.load(std::memory_order_relaxed))
        return false;
    frame = heldBaseFrame();
    buildShowOnly(frame, true, false);
    if (blackout)
    {
        frame = heldBlackoutFrame(frame);
        buildShowOnly(frame, true, true);
    }
    QMutexLocker locker(&m_notOursMutex);
    zeroNotOurs(frame);
    return true;
}

QByteArray Universe::heldBaseFrame() const
{
    /* Blind (review 10 M3/L3): the frame from before it, never the preview.
       A channel past its end (patched or first written in Blind; all of
       them on a universe made in Blind) goes out at 0. */
    if (m_inhibitOutput.load(std::memory_order_relaxed))
    {
        QByteArray frame = m_blindFrozen;
        if (frame.size() < m_usedChannels)
            frame.append(QByteArray(m_usedChannels - frame.size(), char(0)));
        return frame;
    }
    return QByteArray(m_postGMValues->constData(), m_usedChannels);
}

QByteArray Universe::heldBlackoutFrame(const QByteArray &frame) const
{
    if (m_inhibitOutput.load(std::memory_order_relaxed) == false)
        return blackoutFrame(frame);
    /* Blackout in Blind: the frozen frame with its intensity (HTP) at 0 --
       not m_blackoutValues, which carries the preview's LTP values. The
       Blackout-exempt channels keep their frozen value. */
    QByteArray out = frame;
    const char *mask = m_channelsMask->constData();
    const int count = qMin(out.size(), int(m_channelsMask->size()));
    for (int c = 0; c < count; c++)
        if ((mask[c] & HTP) && (mask[c] & BlackoutExempt) == 0)
            out[c] = char(0);
    return out;
}

OutputPatch *Universe::newOutputPatch()
{
    OutputPatch *patch = new OutputPatch(m_id, this);
    patch->setFrameLock(&m_frameMutex);
    patch->setClosingFrame([this](QByteArray &frame, bool blackout) {
        return closingFrame(frame, blackout);
    });
    // Review 19 (A1): the patch list holds only live patches (taken out before a delete).
    patch->setSiblingOnLine([this](const OutputPatch *self, quint32 line) -> OutputPatch * {
        foreach (OutputPatch *op, m_outputPatchList)
            if (op != self && op->plugin() == self->plugin() && op->isPatched() && op->output() == line)
                return op;
        return nullptr;
    });
    return patch;
}

bool Universe::writeBlocked(int address) const
{
    return m_writeGuard && address >= 0 && address < UNIVERSE_SIZE && m_showOnlyBlocked.at(address);
}

bool Universe::showOnlySentChanged(const QByteArray &frame)
{
    QMutexLocker locker(&m_outputRulesMutex);
    bool changed = m_showOnlySentDirty;
    m_showOnlySentDirty = false;
    const int n = int(m_showOnlyChannels.size());
    if (m_showOnlyLastSent.size() != n)
    {
        m_showOnlyLastSent.resize(n);   // only when the rules change
        changed = true;
    }
    for (int i = 0; i < n; i++)
    {
        const int c = m_showOnlyChannels.at(i);
        const uchar v = c < frame.size() ? uchar(frame.at(c)) : uchar(0);
        if (m_showOnlyLastSent.at(i) != v)
        {
            m_showOnlyLastSent[i] = v;
            changed = true;
        }
    }
    return changed;
}

void Universe::setOnAtOpenArmed(bool armed)
{
    QMutexLocker locker(&m_outputRulesMutex);
    if (m_onAtOpenArmed.load(std::memory_order_relaxed) == armed)
        return;
    m_onAtOpenArmed.store(armed, std::memory_order_relaxed);
    if (armed == false)
    {
        foreach (int c, m_onAtOpenChannels)
        {
            if (m_onAtOpenRestore.contains(c) == false)
                m_onAtOpenRestore << c;
        }
    }
}

void Universe::setBlindHoldsExempt(bool hold)
{
    m_blindHoldsExempt.store(hold, std::memory_order_relaxed);
}

void Universe::applyOnAtOpen()
{
    QMutexLocker locker(&m_outputRulesMutex);
    if (m_onAtOpenRestore.isEmpty() == false)
    {
        foreach (int c, m_onAtOpenRestore)
            if (c >= 0 && c < UNIVERSE_SIZE)
                updatePostGMValue(c);
        m_onAtOpenRestore.clear();
    }
    if (m_onAtOpenChannels.isEmpty() || onAtOpenArmed() == false)
        return;

    foreach (int c, m_onAtOpenChannels)
    {
        // Anything in the show that sets it -- even to zero -- wins.
        if (isChannelSet(c))
            continue;

        if (c >= m_usedChannels)
            m_usedChannels = c + 1;
        m_post[c] = char(onAtOpenValue(c));
    }
}

uchar Universe::onAtOpenValue(int channel)
{
    const uchar mask = uchar(m_channelsMask->at(channel));
    uchar value = UCHAR_MAX;
    if ((mask & (GMExempt | SwitchOutput)) == 0)
        value = applyGM(channel, value);
    value = applyModifiers(channel, value);
    if (mask & SwitchOutput)
        value = value >= 128 ? UCHAR_MAX : 0;
    return value;
}

bool Universe::hasBlackoutExempt(int count) const
{
    const char *mask = m_channelsMask->constData();
    count = qMin(count, int(m_channelsMask->size()));
    for (int c = 0; c < count; c++)
        if (mask[c] & BlackoutExempt)
            return true;
    return false;
}

QByteArray Universe::blackoutFrame(const QByteArray &live) const
{
    if (hasBlackoutExempt(live.size()) == false)
        return *m_blackoutValues;

    QByteArray out = *m_blackoutValues;
    const char *mask = m_channelsMask->constData();
    const int count = qMin(live.size(), out.size());
    for (int c = 0; c < count; c++)
        if (mask[c] & BlackoutExempt)
            out[c] = live.at(c);
    return out;
}

/************************************************************************
 * Auto-open shutters
 ************************************************************************/

void Universe::setAutoOpenRules(const QVector<AutoOpenRule> &rules)
{
    QMutexLocker locker(&m_autoOpenMutex);
    // Whatever the old rules may have opened goes back to what is stored,
    // unless the new rules still cover it.
    foreach (const AutoOpenRule &r, m_autoOpenRules)
        if (m_autoOpenRestore.contains(r.shutter) == false)
            m_autoOpenRestore << r.shutter;
    m_autoOpenRules = rules;
}

void Universe::holdChannels(int address, int count)
{
    for (int i = 0; i < count; i++)
        if (address + i >= 0 && address + i < UNIVERSE_SIZE && !writeBlocked(address + i))
            m_held[address + i].store(1, std::memory_order_relaxed);
}

bool Universe::isChannelSet(int address) const
{
    if (address < 0 || address >= UNIVERSE_SIZE)
        return false;
    return writtenRecently(address) || m_held[address].load(std::memory_order_relaxed) != 0;
}

Universe::ChannelSnapshot Universe::channelSnapshot(int address) const
{
    ChannelSnapshot snap;
    if (address < 0 || address >= UNIVERSE_SIZE)
        return snap;
    QMutexLocker frameLocker(&m_frameMutex);
    snap.preGM = uchar(m_preGMValues->at(address));
    snap.postGM = address < m_postGMValues->size() ? uchar(m_postGMValues->at(address)) : 0;
    snap.mask = uchar(m_channelsMask->at(address));
    const quint32 frame = m_frame.load(std::memory_order_relaxed);
    // The frame just finished: m_frame has already moved on past it.
    snap.pastGM = quint32(frame - m_pastGMFrame[address].load(std::memory_order_relaxed)) <= 1;
    snap.framesSinceWrite = quint32(frame - m_writtenFrame[address].load(std::memory_order_relaxed));
    snap.set = isChannelSet(address);
    if (address < m_modifiers.count() && m_modifiers.at(address) != nullptr)
        snap.modifier = m_modifiers.at(address)->name();
    if (m_passthrough && address < m_passthroughValues->size())
        snap.passthrough = uchar(m_passthroughValues->at(address));
    snap.groupMaster = m_groupMaster[address].load(std::memory_order_relaxed);
    return snap;
}

void Universe::applyAutoOpen()
{
    QMutexLocker locker(&m_autoOpenMutex);
    if (m_autoOpenRestore.isEmpty() == false)
    {
        foreach (int a, m_autoOpenRestore)
            if (a >= 0 && a < UNIVERSE_SIZE && isChannelSet(a) == false)
                updatePostGMValue(a);
        m_autoOpenRestore.clear();
    }
    if (m_autoOpenRules.isEmpty())
        return;

    const char *post = m_postGMValues->constData();
    auto lit = [post](int a) {
        return a >= 0 && a < UNIVERSE_SIZE && post[a] != 0;
    };
    // Given a level lately: a resting default (a dimmer that sits at full
    // when nothing drives it) never opens a shutter by itself.
    auto given = [this, post](int a) {
        return a >= 0 && a < UNIVERSE_SIZE && post[a] != 0 && writtenRecently(a);
    };

    foreach (const AutoOpenRule &r, m_autoOpenRules)
    {
        if (r.shutter < 0 || r.shutter >= UNIVERSE_SIZE)
            continue;
        // An explicit shutter value -- closed, strobe, open -- always wins.
        if (isChannelSet(r.shutter))
            continue;

        bool open = false;
        foreach (const AutoOpenHead &h, r.heads)
        {
            bool ok = true, fresh = false;
            foreach (int a, h.gates)
            {
                if (lit(a) == false) { ok = false; break; }
                fresh = fresh || given(a);
            }
            if (ok && h.anyOf.isEmpty() == false)
            {
                bool any = false;
                foreach (int a, h.anyOf)
                    if (lit(a))
                    {
                        any = true;
                        fresh = fresh || given(a);
                    }
                ok = any;
            }
            if (ok && fresh)
            {
                open = true;
                break;
            }
        }

        if (open)
        {
            if (r.shutter >= m_usedChannels)
                m_usedChannels = r.shutter + 1;
            m_post[r.shutter] = char(r.open);
        }
        else
        {
            updatePostGMValue(r.shutter);
        }
    }
}

void Universe::applyThreadPriority()
{
    const int generation = s_priorityGeneration.load(std::memory_order_acquire);
    if (generation == m_priorityApplied)
        return;
    const int previous = m_priorityApplied < 0 ? -1 : m_priorityAsked;
    m_priorityApplied = generation;
    m_priorityAsked = threadPriority();
    applyUniverseThreadScheduling(ThreadPriority(m_priorityAsked), previous);
}

void Universe::run()
{
    m_priorityApplied = -1;
    applyThreadPriority();
    m_running = true;
    int timeout = int(MasterTimer::tick()) * 2;

    qDebug() << "Universe thread started" << id();

    while (m_running)
    {
        if (m_semaphore.tryAcquire(1, timeout) == false)
        {
            //qWarning() << "Semaphore not acquired on universe" << id();
            applyThreadPriority();
            continue;
        }
        applyThreadPriority();
#if 0
        if (m_faders.count())
            qDebug() << "<<<<<<<< UNIVERSE TICK - id" << id() << "faders:" << m_faders.count();
#endif
        processFaders();
    }

    qDebug() << "Universe thread stopped" << id();
}

/************************************************************************
 * Values
 ************************************************************************/

void Universe::reset()
{
    QMutexLocker frameLocker(&m_frameMutex);
    for (int i = 0; i < UNIVERSE_SIZE; i++)
        m_held[i].store(0, std::memory_order_relaxed);
    m_preGMValues->fill(0);
    m_blackoutValues->fill(0);

    if (m_passthrough)
    {
        /* Bytes, not the array: sharing it made the next write reallocate
           the buffer postGMValues() hands out */
        memcpy(m_post, m_passthroughValues->constData(),
               size_t(qMin(m_postGMValues->size(), m_passthroughValues->size())));
    }
    else
        memset(m_post, 0, size_t(m_postGMValues->size()));

    m_modifiers.fill(NULL, UNIVERSE_SIZE);
    m_modifiedZeroValues->fill(0);      // no modifier: every zero is 0
    m_passthrough = false; // not releasing m_passthroughValues, see comment in setPassthrough
    {
        QMutexLocker locker(&m_outputRulesMutex);
        m_showOnlyHolding.fill(char(0));        // Q326: no stop's head held
        m_showOnlyHeldCount = 0;
    }
    restoreShowOnlySafe(0, UNIVERSE_SIZE);
}

void Universe::restoreShowOnlySafe(int address, int range)
{
    /* Show only (E6): a reset leaves a Show-only channel at its safe value
       (a gobo open, a laser's control channel at 0), never 0 or the
       input's -- what "nothing drives it" means there. */
    if (showOnlyChannelCount() == 0)
        return;
    QMutexLocker locker(&m_outputRulesMutex);
    for (int c : std::as_const(m_showOnlyChannels))
    {
        if (c < address || c >= address + range)
            continue;
        (*m_preGMValues)[c] = char(showOnlyRestLocked(c));
        (*m_blackoutValues)[c] = char(showOnlyRestLocked(c));
        updatePostGMValue(c);
        setShowOnlyOutSafeLocked(c);
    }
}

void Universe::reset(int address, int range)
{
    if (address >= UNIVERSE_SIZE)
        return;

    if (address + range > UNIVERSE_SIZE)
       range = UNIVERSE_SIZE - address;

    memset(m_preGMValues->data() + address, 0, range * sizeof(*m_preGMValues->data()));
    memset(m_blackoutValues->data() + address, 0, range * sizeof(*m_blackoutValues->data()));
    memcpy(m_post + address, m_modifiedZeroValues->data() + address, size_t(range));

    applyPassthroughValues(address, range);
    restoreShowOnlySafe(address, range);
}

void Universe::applyPassthroughValues(int address, int range)
{
    if (!m_passthrough)
        return;

    for (int i = address; i < address + range && i < UNIVERSE_SIZE; i++)
    {
        if (uchar(m_channelsMask->at(i)) & ShowOnlyOutput)
            continue;       // Show only: never the input's (R6-4)
        if (static_cast<uchar>(m_postGMValues->at(i)) < static_cast<uchar>(m_passthroughValues->at(i))) // HTP merge
        {
            m_post[i] = (*m_passthroughValues)[i];
        }
    }
}

void Universe::zeroIntensityChannels()
{
    updateIntensityChannelsRanges();
    int const* channels = m_intensityChannelsRanges.constData();
    for (int i = 0; i < m_intensityChannelsRanges.size(); ++i)
    {
        short channel = channels[i] >> 16;
        short size = channels[i] & 0xffff;

        reset(channel, size);
    }
}

QHash<int, uchar> Universe::intensityChannels()
{
    QHash <int, uchar> intensityList;
    for (int i = 0; i < m_intensityChannels.size(); ++i)
    {
        int channel = m_intensityChannels.at(i);
        intensityList[channel] = m_preGMValues->at(channel);
    }
    return intensityList;
}

uchar Universe::postGMValue(int address) const
{
    if (address < 0 || address >= m_postGMValues->size())   // see preGMValue()
        return 0;

    return uchar(m_postGMValues->at(address));
}

const QByteArray* Universe::postGMValues() const
{
    return m_postGMValues.data();
}

Universe::BlendMode Universe::stringToBlendMode(QString mode)
{
    if (mode == KXMLUniverseNormalBlend)
        return NormalBlend;
    else if (mode == KXMLUniverseMaskBlend)
        return MaskBlend;
    else if (mode == KXMLUniverseAdditiveBlend)
        return AdditiveBlend;
    else if (mode == KXMLUniverseSubtractiveBlend)
        return SubtractiveBlend;

    return NormalBlend;
}

QString Universe::blendModeToString(Universe::BlendMode mode)
{
    switch(mode)
    {
        default:
        case NormalBlend:
            return QString(KXMLUniverseNormalBlend);
        break;
        case MaskBlend:
            return QString(KXMLUniverseMaskBlend);
        break;
        case AdditiveBlend:
            return QString(KXMLUniverseAdditiveBlend);
        break;
        case SubtractiveBlend:
            return QString(KXMLUniverseSubtractiveBlend);
        break;
    }
}

const QByteArray Universe::preGMValues() const
{
    /* A shallow copy shares the array: copied while a frame detaches it
       (reset() on the universe thread), the two raced on its header. And
       while the copy lived, the universe thread's next write detached --
       reallocated -- its own buffer, under a per-channel read from the GUI
       (preGMValue()) that could then read the freed one (TSan L2). A copy
       of the bytes leaves the universe's buffer its own, never moved. */
    QMutexLocker frameLocker(&m_frameMutex);
    return QByteArray(m_preGMValues->constData(), m_preGMValues->size());
}

uchar Universe::preGMValue(int address) const
{
    /* Below zero too: an inert channel (a fixture in the inventory, not
       patched) has the INVALID address, all ones, which arrives here as -1.
       A scene with one unpatched fixture in it aborted the engine thread on
       its first tick (QByteArray::at asserts). */
    if (address < 0 || address >= m_preGMValues->size())
        return 0U;

    return static_cast<uchar>(m_preGMValues->at(address));
}

uchar Universe::applyGM(int channel, uchar value)
{
    if ((m_grandMaster->channelMode() == GrandMaster::Intensity && m_channelsMask->at(channel) & Intensity) ||
        (m_grandMaster->channelMode() == GrandMaster::AllChannels))
    {
        if (m_grandMaster->valueMode() == GrandMaster::Limit)
            value = MIN(value, m_grandMaster->value());
        else
            value = uchar(floor((double(value) * m_grandMaster->fraction()) + 0.5));
    }

    return value;
}

bool Universe::updatePostGMPair(int channel, quint8 role)
{
    const int coarse = role == PairFine ? channel - 1 : channel;
    const int fine = coarse + 1;
    const uchar coarseMask = uchar(m_channelsMask->at(coarse));
    const uchar fineMask = uchar(m_channelsMask->at(fine));

    /* No master scaling it (the usual case): byte by byte is exact, and the
       one test every frame of a 16-bit channel pays. */
    const uint group = (coarseMask & Intensity)
                       ? m_groupMaster[coarse].load(std::memory_order_relaxed) : UCHAR_MAX;
    const bool gmApplies = m_grandMaster->channelMode() == GrandMaster::AllChannels
                           || (coarseMask & Intensity);
    if (group == UCHAR_MAX && (gmApplies == false || m_grandMaster->value() == UCHAR_MAX))
        return false;

    if (((coarseMask | fineMask) & (GMExempt | SwitchOutput | OnAtOpenOutput))
        || ((coarseMask ^ fineMask) & Intensity) || pastGM(coarse) || pastGM(fine))
        return false;

    quint32 value = (quint32(preGMValue(coarse)) << 8) | preGMValue(fine);
    if (group != UCHAR_MAX)
        value = (value * group + 127) / 255;
    if (gmApplies)
    {
        if (m_grandMaster->valueMode() == GrandMaster::Limit)
            value = qMin(value, quint32(m_grandMaster->value()) * 257);
        else
            value = quint32(floor(double(value) * m_grandMaster->fraction() + 0.5));
    }

    m_post[coarse] = char(applyPassthrough(coarse, applyModifiers(coarse, uchar(value >> 8))));
    m_post[fine] = char(applyPassthrough(fine, applyModifiers(fine, uchar(value & 0xFF))));
    return true;
}

uchar Universe::applyModifiers(int channel, uchar value)
{
    if (m_modifiers.at(channel) != NULL)
        return m_modifiers.at(channel)->getValue(value);

    return value;
}

uchar Universe::applyPassthrough(int channel, uchar value)
{
    if (m_passthrough)
    {
        // Show only (R6-4): an input merged in never reaches a Show-only
        // channel -- no GO, no stamp, past every fader.
        if (uchar(m_channelsMask->at(channel)) & ShowOnlyOutput)
            return value;
        const uchar passthroughValue = static_cast<uchar>(m_passthroughValues->at(channel));
        if (value < passthroughValue) // HTP merge
        {
            return passthroughValue;
        }
    }

    return value;
}

void Universe::updatePostGMValue(int channel)
{
    const uchar mask = uchar(m_channelsMask->at(channel));

    /* An On-when-not-in-use channel nothing sets stays at its floor on
       every path that works out the output -- a Grand Master move
       (slotGMValueChanged) as much as the frame (applyOnAtOpen): computed
       from the stored 0 here, a relay dropped out until the next frame. */
    if ((mask & OnAtOpenOutput) && onAtOpenArmed() && isChannelSet(channel) == false)
    {
        m_post[channel] = char(onAtOpenValue(channel));
        return;
    }

    const quint8 pairRole = m_pairRole[channel].load(std::memory_order_relaxed);
    if (pairRole != NotPaired && updatePostGMPair(channel, pairRole))
        return;

    uchar value = preGMValue(channel);

    /* Output rules: a GM-exempt channel is never scaled, and neither is a
       relay -- the Grand Master pulled to 30% would otherwise switch it off.
       The relay's snap comes last, so nothing after it (a modifier, the
       input passthrough) can hand it a level between off and on. */
    if (value != 0 && (mask & (GMExempt | SwitchOutput)) == 0 && pastGM(channel) == false)
    {
        /* A group master scales intensity proportionally, before the Grand
           Master (the two multiply). 255 -- no master -- is the one test
           every other channel pays. */
        const uint group = m_groupMaster[channel].load(std::memory_order_relaxed);
        if (group != UCHAR_MAX && (mask & Intensity))
            value = uchar((uint(value) * group + 127) / 255);
        value = applyGM(channel, value);
    }

    value = applyModifiers(channel, value);
    value = applyPassthrough(channel, value);

    if (mask & SwitchOutput)
        value = value >= 128 ? UCHAR_MAX : 0;

    m_post[channel] = static_cast<char>(value);
}

/************************************************************************
 * Patches
 ************************************************************************/

bool Universe::isPatched()
{
    if (m_inputPatch != NULL || m_outputPatchList.count() || m_fbPatch != NULL)
        return true;

    return false;
}

bool Universe::setInputPatch(QLCIOPlugin *plugin,
                             quint32 input, QLCInputProfile *profile)
{
    qDebug() << "[Universe] setInputPatch - ID:" << m_id << ", plugin:" << ((plugin == NULL)?"None":plugin->name())
             << ", input:" << input << ", profile:" << ((profile == NULL)?"None":profile->name());
    /* Opened and closed when the outermost IODeferral ends -- the
       caller's, after it lets the universes go (InputOutputMap). */
    IODeferral io;
    if (m_inputPatch == NULL)
    {
        if (plugin == NULL || input == QLCIOPlugin::invalidLine())
            return true;

        m_inputPatch = new InputPatch(m_id, this);
        connectInputPatch();
    }
    else
    {
        if (input == QLCIOPlugin::invalidLine())
        {
            disconnectInputPatch();
            delete m_inputPatch;
            m_inputPatch = NULL;
            emit inputPatchChanged();
            return true;
        }
    }

    if (m_inputPatch != NULL)
    {
        bool result = m_inputPatch->set(plugin, input, profile);
        result = io.finish() && result;
        emit inputPatchChanged();
        return result;
    }

    return true;
}

bool Universe::setOutputPatch(QLCIOPlugin *plugin, quint32 output, int index,
                              const QMap<QString, QVariant> &params)
{
    if (index < 0)
        return false;

    qDebug() << "[Universe] setOutputPatch - ID:" << m_id
             << ", plugin:" << ((plugin == NULL) ? "None" : plugin->name()) << ", output:" << output;

    /* Under the frame lock: the universe thread's frame (dumpOutputFrame())
       and sendCurrent()/sendHeld() never walk the list, nor dump to a patch,
       while it changes; a line closed sends its closing frame first (R9-1:
       OutputPatch::closeLine()). The signals after, with the lock let go.
       The opens and closes after it too (IODeferral): a device that does
       not answer while opened or closed holds neither this universe's
       thread nor the tick (its frame writes still run under the lock). */
    IODeferral io;
    bool result = false;
    enum { NoChange, Changed, CountChanged } change = NoChange;
    {
        QMutexLocker frameLocker(&m_frameMutex);
        // replace or delete an existing patch
        if (index < m_outputPatchList.count())
        {
            if (plugin == NULL || output == QLCIOPlugin::invalidLine())
            {
                // need to delete an existing patch
                OutputPatch *patch = m_outputPatchList.takeAt(index);
                delete patch;
                result = true;
                change = CountChanged;
            }
            else
            {
                OutputPatch *patch = m_outputPatchList.at(index);
                result = params.isEmpty() ? patch->set(plugin, output) : patch->set(plugin, output, params);
                change = Changed;
            }
        }
        else
        {
            if (plugin == NULL || output == QLCIOPlugin::invalidLine())
                return false;

            // add a new patch
            OutputPatch *patch = newOutputPatch();
            result = params.isEmpty() ? patch->set(plugin, output) : patch->set(plugin, output, params);
            m_outputPatchList.append(patch);
            change = CountChanged;
        }
    }
    result = io.finish() && result;

    if (change == CountChanged)
        emit outputPatchesCountChanged();
    else if (change == Changed)
        emit outputPatchChanged();
    return result;
}

bool Universe::setOutputPatchPending(QLCIOPlugin *plugin, const QString &pendingUID, int index,
                                     quint32 savedLine, const QString &network, bool byNumber)
{
    // An empty name only for a patch held by its number (F3, outputpatch.h).
    if (index < 0 || plugin == NULL || (pendingUID.isEmpty() && (byNumber == false || savedLine == UINT_MAX)))
        return false;

    qDebug() << "[Universe] setOutputPatchPending - ID:" << m_id
             << ", plugin:" << plugin->name() << ", pending UID:" << pendingUID;

    // Under the frame lock, as setOutputPatch(): a held line sends its
    // closing frame first (R9-1). Closed after it (IODeferral).
    IODeferral io;
    bool added = false;
    {
        QMutexLocker frameLocker(&m_frameMutex);
        if (index < m_outputPatchList.count())
        {
            OutputPatch *patch = m_outputPatchList.at(index);
            patch->setPending(plugin, pendingUID, savedLine, network);
            patch->setPendingByNumber(byNumber && pendingUID.isEmpty());
        }
        else
        {
            OutputPatch *patch = newOutputPatch();
            patch->setPending(plugin, pendingUID, savedLine, network);
            patch->setPendingByNumber(byNumber && pendingUID.isEmpty());
            m_outputPatchList.append(patch);
            added = true;
        }
    }
    io.finish();
    if (added)
        emit outputPatchesCountChanged();
    else
        emit outputPatchChanged();
    return true;
}

bool Universe::resolveOutputPatch(int index, quint32 line)
{
    // Under the frame lock, as every patch change: the line opens after
    // it (IODeferral), and its first frame is the current S0 build (sent
    // as changed).
    IODeferral io;
    bool opened = false;
    {
        QMutexLocker frameLocker(&m_frameMutex);
        OutputPatch *patch = outputPatch(index);
        opened = patch != NULL && patch->resolvePending(line);
    }
    return io.finish() && opened;
}

bool Universe::setFeedbackPatch(QLCIOPlugin *plugin, quint32 output)
{
    qDebug() << Q_FUNC_INFO << "plugin:" << plugin << "output:" << output;
    IODeferral io;      // opened and closed after the caller's locks
    if (m_fbPatch == NULL)
    {
        if (plugin == NULL || output == QLCIOPlugin::invalidLine())
            return false;

        m_fbPatch = new OutputPatch(m_id, this);
        m_fbPatch->setFeedback(true);
        /* Review 21 (V3): the plugin keeps one entry per (line, universe),
           the feedback's and the output's alike -- an open output leg on
           the feedback's line is its sibling there. */
        m_fbPatch->setSiblingOnLine([this](const OutputPatch *self, quint32 line) -> OutputPatch * {
            foreach (OutputPatch *op, m_outputPatchList)
                if (op != self && op->plugin() == self->plugin() && op->isPatched() && op->output() == line)
                    return op;
            return nullptr;
        });
    }
    else
    {
        if (plugin == NULL || output == QLCIOPlugin::invalidLine())
        {
            delete m_fbPatch;
            m_fbPatch = NULL;
            emit hasFeedbackChanged();
            return true;
        }
    }
    if (m_fbPatch != NULL)
    {
        bool result = m_fbPatch->set(plugin, output);
        result = io.finish() && result;
        emit hasFeedbackChanged();
        return result;
    }

    return false;
}

bool Universe::hasFeedback() const
{
    return m_fbPatch != NULL ? true : false;
}

InputPatch *Universe::inputPatch() const
{
    return m_inputPatch;
}

OutputPatch *Universe::outputPatch(int index) const
{
    if (index < 0 || index >= m_outputPatchList.count())
        return NULL;

    return m_outputPatchList.at(index);
}

int Universe::outputPatchesCount() const
{
    return m_outputPatchList.count();
}

OutputPatch *Universe::feedbackPatch() const
{
    return m_fbPatch;
}

void Universe::dumpOutput(const QByteArray &data, bool dataChanged)
{
    /* Blackout, leaving Blind and a maintenance send call here between two
       frames: one frame at a time, and Show only held in theirs too. */
    QMutexLocker frameLocker(&m_frameMutex);
    dumpOutputFrame(data, dataChanged, showOnlyLive());
}

void Universe::dumpOutputFrame(const QByteArray &data, bool dataChanged, bool live)
{
    /* A universe the venue's house desk keeps (docs/design/VENUES.md section 5): nothing
       of ours goes out on it at all -- not Blind's, not Blackout's. */
    if (m_notOursWhole.load(std::memory_order_relaxed))
        return;
    // A show being loaded: nothing until its patches are checked (review 13).
    if (m_loadHold.load(std::memory_order_relaxed))
        return;

    /* Show only: held outside a real run (Design) and in Blind; in a
       Blackout frame too, which is built from raw stored values. */
    const bool showOnly = showOnlyChannelCount() > 0;
    const bool inhibit = m_inhibitOutput.load(std::memory_order_relaxed);
    // A reset still to come (a channel just made Show only while live) is
    // held until the frame that does it.
    const bool showOnlyHold = showOnly && (live == false || inhibit
                                           || m_showOnlyReset.load(std::memory_order_relaxed));

    // Fork Blind: hold the physical rig dark while the preview keeps running.
    // processFaders() still emits universeWritten() (the 2D monitor source),
    // so the look is visible on screen but never reaches the output plugins.
    //
    // Output rules: the Blackout-exempt channels (house, work and hall
    // lights, power relays) keep their live value through Blackout and, when
    // blindHoldsExempt(), through Blind -- which then sends what Blackout
    // would rather than nothing, so those circuits are not left to a node's
    // hold-last-look. A universe with none behaves exactly as before.
    if (inhibit)
    {
        if (blindHoldsExempt() == false || hasBlackoutExempt(data.size()) == false)
            return;
        QByteArray frame = blackoutFrame(data);
        if (showOnly)
        {
            buildShowOnly(frame, true, true);
            if (showOnlySentChanged(frame))
                dataChanged = true;
        }
        {
            QMutexLocker locker(&m_notOursMutex);
            zeroNotOurs(frame);
        }
        foreach (OutputPatch *op, m_outputPatchList)
        {
            if (m_totalChannelsChanged == true)
                op->setPluginParameter(PLUGIN_UNIVERSECHANNELS, m_totalChannels);
            op->dump(m_id, frame, dataChanged);
        }
        m_totalChannelsChanged = false;
        return;
    }

    if (m_outputPatchList.count() == 0)
        return;

    /* Show only first, then "not ours" (docs/design/VENUES.md section 5) on
       top: a range the venue's desk keeps is zero in every frame we send,
       whatever asked for it -- a safe value included. */
    const QByteArray *base = &data;
    if (showOnly)
    {
        /* S0 for every send, held or live: its Show-only channels from the
           builder, never from @p data. Copied into a kept buffer: no
           allocation per frame (unless a plugin still holds the last one). */
        m_showOnlyFrame.resize(data.size());
        memcpy(m_showOnlyFrame.data(), data.constData(), size_t(data.size()));
        buildShowOnly(m_showOnlyFrame, showOnlyHold, false);
        base = &m_showOnlyFrame;
    }
    QByteArray masked;
    {
        QMutexLocker locker(&m_notOursMutex);
        if (!m_notOursRanges.isEmpty())
        {
            masked = *base;
            zeroNotOurs(masked);
            base = &masked;
        }
    }
    const QByteArray &frame = *base;

    /* What a Blackout patch sends, made once for every patch in Blackout,
       and whether what goes out on the Show-only channels changed since
       the last frame sent (compared once: it keeps what it saw). */
    QByteArray blackout;
    bool anyBlackout = false;
    foreach (OutputPatch *op, m_outputPatchList)
        anyBlackout = anyBlackout || op->blackout();
    if (anyBlackout)
    {
        blackout = blackoutFrame(frame);
        if (showOnly)
            buildShowOnly(blackout, showOnlyHold, true);
        QMutexLocker locker(&m_notOursMutex);
        zeroNotOurs(blackout);
    }
    if (showOnly && showOnlySentChanged(anyBlackout ? blackout : frame))
        dataChanged = true;

    foreach (OutputPatch *op, m_outputPatchList)
    {
        if (m_totalChannelsChanged == true)
            op->setPluginParameter(PLUGIN_UNIVERSECHANNELS, m_totalChannels);

        if (op->blackout())
            op->dump(m_id, blackout, dataChanged);
        else
            op->dump(m_id, frame, dataChanged);
    }
    m_totalChannelsChanged = false;
}

void Universe::zeroNotOurs(QByteArray &frame) const
{
    for (const QPair<int, int> &r : m_notOursRanges)
        for (int c = qMax(0, r.first); c <= r.second && c < frame.size(); ++c)
            frame[c] = 0;
}

void Universe::flushInput()
{
    if (m_inputPatch == NULL)
        return;

    m_inputPatch->flush(m_id);
}

void Universe::slotInputValueChanged(quint32 universe, quint32 channel, uchar value, const QString &key)
{
    if (m_passthrough)
    {
        if (universe == m_id)
        {
            qDebug() << "write" << channel << value;

            if (channel >= UNIVERSE_SIZE)
                return;

            if (channel >= m_usedChannels)
                m_usedChannels = channel + 1;

            // Show only (R6-4): never kept for a Show-only channel.
            if (uchar(m_channelsMask->at(int(channel))) & ShowOnlyOutput)
                value = 0;
            (*m_passthroughValues)[channel] = value;

            updatePostGMValue(channel);
        }
    }
    else
        emit inputValueChanged(universe, channel, value, key);
}

void Universe::connectInputPatch()
{
    if (m_inputPatch == NULL)
        return;

    if (!m_passthrough)
        connect(m_inputPatch, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)),
                this, SIGNAL(inputValueChanged(quint32,quint32,uchar,QString)));
    else
        connect(m_inputPatch, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)),
                this, SLOT(slotInputValueChanged(quint32,quint32,uchar,const QString&)));
}

void Universe::disconnectInputPatch()
{
    if (m_inputPatch == NULL)
        return;

    if (!m_passthrough)
        disconnect(m_inputPatch, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)),
                this, SIGNAL(inputValueChanged(quint32,quint32,uchar,QString)));
    else
        disconnect(m_inputPatch, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)),
                this, SLOT(slotInputValueChanged(quint32,quint32,uchar,const QString&)));
}

/************************************************************************
 * Channels capabilities
 ************************************************************************/

void Universe::setChannelCapability(ushort channel, QLCChannel::Group group, ChannelType forcedType)
{
    QMutexLocker frameLocker(&m_frameMutex);
    if (channel >= (ushort)m_channelsMask->length())
        return;

    clearChannelPair(channel);

    if (Utils::vectorRemove(m_intensityChannels, channel))
        m_intensityChannelsChanged = true;
    Utils::vectorRemove(m_nonIntensityChannels, channel);

    /* The output rules are the fixture's, kept by OutputRuleKeeper (which
       clears them when the fixture moves or goes): re-applying a channel's
       role must not drop them for a frame. */
    const char rules = char(m_channelsMask->at(channel) & OutputRuleBits);

    if (forcedType != Undefined)
    {
        (*m_channelsMask)[channel] = char(forcedType | rules);
        if ((forcedType & HTP) == HTP)
        {
            //qDebug() << "--- Channel" << channel << "forced type HTP";
            Utils::vectorSortedAddUnique(m_intensityChannels, channel);
            m_intensityChannelsChanged = true;
            if (group == QLCChannel::Intensity)
            {
                //qDebug() << "--- Channel" << channel << "Intensity + HTP";
                (*m_channelsMask)[channel] = char(HTP | Intensity | rules);
            }
        }
        else if ((forcedType & LTP) == LTP)
        {
            //qDebug() << "--- Channel" << channel << "forced type LTP";
            Utils::vectorSortedAddUnique(m_nonIntensityChannels, channel);
        }
    }
    else
    {
        if (group == QLCChannel::Intensity)
        {
            //qDebug() << "--- Channel" << channel << "Intensity + HTP";
            (*m_channelsMask)[channel] = char(HTP | Intensity | rules);
            Utils::vectorSortedAddUnique(m_intensityChannels, channel);
            m_intensityChannelsChanged = true;
        }
        else
        {
            //qDebug() << "--- Channel" << channel << "LTP";
            (*m_channelsMask)[channel] = char(LTP | rules);
            Utils::vectorSortedAddUnique(m_nonIntensityChannels, channel);
        }
    }

    // qDebug() << Q_FUNC_INFO << "Channel:" << channel << "mask:" << QString::number(m_channelsMask->at(channel), 16);
    if (channel >= m_totalChannels)
    {
        m_totalChannels = channel + 1;
        m_totalChannelsChanged = true;
    }
}

void Universe::setChannelPair(ushort coarse)
{
    if (int(coarse) + 1 >= UNIVERSE_SIZE)
        return;
    m_pairRole[coarse].store(PairCoarse, std::memory_order_relaxed);
    m_pairRole[coarse + 1].store(PairFine, std::memory_order_relaxed);
}

void Universe::clearChannelPair(ushort channel)
{
    if (channel >= UNIVERSE_SIZE)
        return;
    const quint8 role = m_pairRole[channel].exchange(NotPaired, std::memory_order_relaxed);
    /* Only a partner that still points back: a patch going down its channels
       has already marked the pair below this one, which is not this one's. */
    if (role == PairCoarse && channel + 1 < UNIVERSE_SIZE
        && m_pairRole[channel + 1].load(std::memory_order_relaxed) == PairFine)
        m_pairRole[channel + 1].store(NotPaired, std::memory_order_relaxed);
    else if (role == PairFine && channel > 0
             && m_pairRole[channel - 1].load(std::memory_order_relaxed) == PairCoarse)
        m_pairRole[channel - 1].store(NotPaired, std::memory_order_relaxed);
}

uchar Universe::channelCapabilities(ushort channel)
{
    if (channel >= (ushort)m_channelsMask->length())
        return Undefined;

    return m_channelsMask->at(channel);
}

void Universe::setChannelDefaultValue(ushort channel, uchar value)
{
    QMutexLocker frameLocker(&m_frameMutex);
    /* Past the universe (a fixture with more channels than it holds), the
       write below landed past the 512-byte buffers in a release build. */
    if (channel >= (ushort)m_preGMValues->length())
        return;

    if (channel >= m_totalChannels)
    {
        m_totalChannels = channel + 1;
        m_totalChannelsChanged = true;
    }

    if (channel >= m_usedChannels)
        m_usedChannels = channel + 1;

    /* E6: a Show-only channel's "nothing drives it" value is its safe
       value, never its definition's default (a laser's control mode). */
    if (uchar(m_channelsMask->at(channel)) & ShowOnlyOutput)
    {
        QMutexLocker locker(&m_outputRulesMutex);
        value = showOnlyRestLocked(channel);
    }

    (*m_preGMValues)[channel] = value;
    updatePostGMValue(channel);
}

void Universe::setChannelModifier(ushort channel, ChannelModifier *modifier)
{
    QMutexLocker frameLocker(&m_frameMutex);
    if (channel >= (ushort)m_modifiers.count())
        return;

    m_modifiers[channel] = modifier;
    // Taken off: the channel's zero is 0 again. Kept, every frame's
    // intensity reset sent the old modifier's zero (an inverted channel's
    // 255) on a channel nothing set.
    if (modifier == NULL)
        (*m_modifiedZeroValues)[channel] = 0;

    if (modifier != NULL)
    {
        (*m_modifiedZeroValues)[channel] = modifier->getValue(0);

        if (channel >= m_totalChannels)
        {
            m_totalChannels = channel + 1;
            m_totalChannelsChanged = true;
        }

        if (channel >= m_usedChannels)
            m_usedChannels = channel + 1;
    }

    updatePostGMValue(channel);
}

ChannelModifier *Universe::channelModifier(ushort channel)
{
    if (channel >= (ushort)m_modifiers.count())
        return NULL;

    return m_modifiers.at(channel);
}

void Universe::updateIntensityChannelsRanges()
{
    if (!m_intensityChannelsChanged)
        return;

    m_intensityChannelsChanged = false;

    m_intensityChannelsRanges.clear();
    short currentPos = -1;
    short currentSize = 0;

    for (int i = 0; i < m_intensityChannels.size(); ++i)
    {
        int channel = m_intensityChannels.at(i);
        if (currentPos + currentSize == channel)
            ++currentSize;
        else
        {
            if (currentPos != -1)
                m_intensityChannelsRanges.append((currentPos << 16) | currentSize);
            currentPos = channel;
            currentSize = 1;
        }
    }
    if (currentPos != -1)
        m_intensityChannelsRanges.append((currentPos << 16) | currentSize);

    qDebug() << Q_FUNC_INFO << ":" << m_intensityChannelsRanges.size() << "ranges";
}

/****************************************************************************
 * Writing
 ****************************************************************************/

/* What of a write of $channelCount channels from $address fits in a universe:
   a 16-bit pair, or a fixture, at its end ran past the buffers (and the
   modifiers) -- unchecked in a release build. Keeps the most significant
   bytes of $value, the ones that land on the channels that exist. */
static bool clipToUniverse(int address, quint32 &value, int &channelCount)
{
    if (address < 0 || address >= UNIVERSE_SIZE || channelCount <= 0)
        return false;
    const int room = UNIVERSE_SIZE - address;
    if (channelCount > room)
    {
        value >>= 8 * (channelCount - room);
        channelCount = room;
    }
    return true;
}

bool Universe::write(int address, uchar value, bool forceLTP)
{
    if (address < 0 || address >= UNIVERSE_SIZE)
        return false;
    // S0: a writer that does not count here contributes nothing.
    if (Q_UNLIKELY(writeBlocked(address)))
        return false;

    //qDebug() << "[Universe]" << id() << ": write channel" << address << ", value:" << value;

    if (address >= m_usedChannels)
        m_usedChannels = address + 1;

    markWritten(address);

    // A lower value on an HTP channel loses: routine, every tick -- not logged
    if (m_channelsMask->at(address) & HTP)
    {
        if (forceLTP == false && value < (uchar)m_preGMValues->at(address))
            return false;
    }
    else
    {
        // preserve non HTP channels for blackout
        (*m_blackoutValues)[address] = char(value);
    }

    (*m_preGMValues)[address] = char(value);

    updatePostGMValue(address);

    return true;
}

bool Universe::writeOverride(int address, uchar value)
{
    if (address < 0 || address >= UNIVERSE_SIZE)
        return false;
    if (Q_UNLIKELY(writeBlocked(address)))
        return false;
    if (m_overrideWritten.size() != UNIVERSE_SIZE)
        m_overrideWritten = QByteArray(UNIVERSE_SIZE, char(0));
    if ((m_channelsMask->at(address) & HTP) && m_overrideWritten.at(address))
        value = qMax(value, uchar(m_preGMValues->at(address)));
    m_overrideWritten[address] = char(1);
    return write(address, value, true);
}

bool Universe::writeMix(int address, uchar value, qreal mix)
{
    if (address < 0 || address >= UNIVERSE_SIZE)
        return false;
    if (mix < 1.0)
    {
        const int under = uchar(m_preGMValues->at(address));
        const int v = under + qRound((int(value) - under) * qMax(qreal(0), mix));
        value = uchar(qBound(0, v, 255));
    }
    return write(address, value, true);
}

bool Universe::writeMix(int address, quint32 value, int channelCount, qreal mix)
{
    if (channelCount == 1)
        return writeMix(address, uchar(value), mix);
    if (!clipToUniverse(address, value, channelCount))
        return false;
    if (mix < 1.0)
    {
        qint64 under = 0;
        for (int i = 0; i < channelCount; i++)
            under = (under << 8) | uchar(m_preGMValues->at(address + i));
        value = quint32(under + qRound64((qint64(value) - under) * qMax(qreal(0), mix)));
    }
    for (int i = 0; i < channelCount; i++)
        write(address + i, uchar(value >> (8 * (channelCount - 1 - i))), true);
    return true;
}

bool Universe::writePastGM(int address, uchar value)
{
    if (address < 0 || address >= UNIVERSE_SIZE)
        return false;
    // R8-1: not even its "past the Grand Master" flag, where it does not count.
    if (Q_UNLIKELY(writeBlocked(address)))
        return false;
    // Stamped before the write: write() works out the post-GM value.
    m_pastGMFrame[address].store(m_frame.load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
    return write(address, value, true);
}

bool Universe::writeMultiple(int address, quint32 value, int channelCount)
{
    if (!clipToUniverse(address, value, channelCount))
        return false;

    for (int i = 0; i < channelCount; i++)
    {
        //qDebug() << "[Universe]" << id() << ": write channel" << (address + i) << ", value:" << QString::number(((uchar *)&value)[channelCount - 1 - i]);
        if (Q_UNLIKELY(writeBlocked(address + i)))
            continue;

        markWritten(address + i);

        // preserve non HTP channels for blackout
        if ((m_channelsMask->at(address + i) & HTP) == 0)
            (*m_blackoutValues)[address + i] = ((uchar *)&value)[channelCount - 1 - i];

        (*m_preGMValues)[address + i] = ((uchar *)&value)[channelCount - 1 - i];

        updatePostGMValue(address + i);
    }

    return true;
}

bool Universe::writeRelative(int address, quint32 value, int channelCount)
{
    if (!clipToUniverse(address, value, channelCount))
        return false;

    //qDebug() << "Write relative channel" << address << "value" << value;

    // S0 (R9-3): a relative write on a channel it does not count on: none --
    // no value, no "written" flag, no growth. Checked before anything.
    if (Q_UNLIKELY(m_writeGuard))
        for (int i = 0; i < channelCount; i++)
            if (writeBlocked(address + i))
                return false;

    if (address + channelCount >= m_usedChannels)
        m_usedChannels = address + channelCount;

    for (int i = 0; i < channelCount; i++)
        markWritten(address + i);

    if (channelCount == 1)
    {
        short newVal = uchar((*m_preGMValues)[address]);
        newVal += short(value) - RELATIVE_ZERO_8BIT;
        (*m_preGMValues)[address] = char(CLAMP(newVal, 0, UCHAR_MAX));
        (*m_blackoutValues)[address] = char(CLAMP(newVal, 0, UCHAR_MAX));
        updatePostGMValue(address);
    }
    else
    {
        quint32 currentValue = 0;
        for (int i = 0; i < channelCount; i++)
            currentValue = (currentValue << 8) + uchar(m_preGMValues->at(address + i));

        currentValue = qint32(CLAMP((qint32)currentValue + (qint32)value - RELATIVE_ZERO_16BIT, 0, 0xFFFF));

        for (int i = 0; i < channelCount; i++)
        {
            (*m_preGMValues)[address + i] = ((uchar *)&currentValue)[channelCount - 1 - i];
            (*m_blackoutValues)[address + i] = ((uchar *)&currentValue)[channelCount - 1 - i];
            updatePostGMValue(address + i);
        }
    }

    return true;
}

bool Universe::writeBlended(int address, quint32 value, int channelCount, Universe::BlendMode blend)
{
    if (!clipToUniverse(address, value, channelCount))
        return false;

    // S0 (R9-3): where its writer does not count, nothing -- checked first
    // (writeMultiple() drops each blocked byte of a write it lets through).
    if (Q_UNLIKELY(m_writeGuard))
    {
        int blocked = 0;
        for (int i = 0; i < channelCount; i++)
            if (writeBlocked(address + i))
                ++blocked;
        if (blocked == channelCount)
            return false;
    }

    if (address + channelCount >= m_usedChannels)
        m_usedChannels = address + channelCount;

    quint32 currentValue = 0;
    for (int i = 0; i < channelCount; i++)
        currentValue = (currentValue << 8) + uchar(m_preGMValues->at(address + i));

    switch (blend)
    {
        case NormalBlend:
        {
            // routine, every tick -- not logged
            if ((m_channelsMask->at(address) & HTP) && value < currentValue)
                return false;
        }
        break;
        case MaskBlend:
        {
            if (value)
            {
                if (currentValue)
                    value = float(currentValue) * (float(value) / pow(255.0, channelCount));
                else
                    value = 0;
            }
        }
        break;
        case AdditiveBlend:
        {
            //qDebug() << "Universe write additive channel" << channel << ", value:" << currVal << "+" << value;
            value = fmin(float(currentValue + value), pow(255.0, channelCount));
        }
        break;
        case SubtractiveBlend:
        {
            if (value >= currentValue)
                value = 0;
            else
                value = currentValue - value;
        }
        break;
        default:
            qDebug() << "[Universe] Blend mode not handled. Implement me!" << blend;
            return false;
        break;
    }

    writeMultiple(address, value, channelCount);

    return true;
}

/*********************************************************************
 * Load & Save
 *********************************************************************/

bool Universe::loadXML(QXmlStreamReader &root, int index, InputOutputMap *ioMap)
{
    if (root.name() != KXMLQLCUniverse)
    {
        qWarning() << Q_FUNC_INFO << "Universe node not found";
        return false;
    }

    QXmlStreamAttributes attrs = root.attributes();

    if (attrs.hasAttribute(KXMLQLCUniverseName))
        setName(attrs.value(KXMLQLCUniverseName).toString());

    setSlotOwner(RigOwner::fromXmlName(attrs.value(KXMLQLCUniverseOwner).toString()));

    if (attrs.hasAttribute(KXMLQLCUniversePassthrough))
    {
        if (attrs.value(KXMLQLCUniversePassthrough).toString() == KXMLQLCTrue ||
            attrs.value(KXMLQLCUniversePassthrough).toString() == "1")
            setPassthrough(true);
        else
            setPassthrough(false);
    }
    else
    {
        setPassthrough(false);
    }

    QList<InputSlotDevice> slotDevices;
    QStringList slotRoles;

    while (root.readNextStartElement())
    {
        QXmlStreamAttributes pAttrs = root.attributes();
        OfflinePatch patch;

        if (root.name() == KXMLQLCUniverseInputSlot)
        {
            /* A controller slot (older builds skip it and open the first
               device, written as the <Input> above it). */
            slotRoles = pAttrs.value(KXMLQLCUniverseInputSlotRoles).toString()
                            .split(QLatin1Char(','));
            while (slotRoles.isEmpty() == false && slotRoles.last().isEmpty())
                slotRoles.removeLast();
            while (root.readNextStartElement())
            {
                if (root.name() != KXMLQLCUniverseInputSlotDevice)
                {
                    root.skipCurrentElement();
                    continue;
                }
                const QXmlStreamAttributes dAttrs = root.attributes();
                InputSlotDevice dev;
                dev.plugin = dAttrs.value(KXMLQLCUniversePlugin).toString();
                dev.uid = dAttrs.value(KXMLQLCUniverseLineUID).toString();
                dev.anyGamepad = dAttrs.value(KXMLQLCUniverseInputSlotAny).toString() == KXMLQLCTrue;
                dev.line = dAttrs.value(KXMLQLCUniverseLine).toString().toUInt();
                dev.profile = dAttrs.value(KXMLQLCUniverseProfileName).toString();
                dev.hidProfile = dAttrs.value(KXMLQLCUniverseHIDProfileName).toString();
                while (root.readNextStartElement())
                {
                    if (root.name() == KXMLQLCUniversePluginParameters)
                    {
                        const QXmlStreamAttributes pa = root.attributes();
                        for (int i = 0; i < pa.count(); i++)
                            dev.params[pa.at(i).name().toString()] = pa.at(i).value().toString();
                    }
                    root.skipCurrentElement();
                }
                if (dev.plugin.isEmpty() == false && (dev.anyGamepad || dev.uid.isEmpty() == false))
                    slotDevices << dev;
            }
            continue;
        }

        if (root.name() == KXMLQLCUniverseInputPatch)
            patch.tag = InputPatchTag;
        else if (root.name() == KXMLQLCUniverseOutputPatch)
            patch.tag = OutputPatchTag;
        else if (root.name() == KXMLQLCUniverseFeedbackPatch)
            patch.tag = FeedbackPatchTag;
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown Universe tag:" << root.name();
            root.skipCurrentElement();
            continue;
        }

        patch.plugin = patch.tag == InputPatchTag ? KInputNone : KOutputNone;
        patch.profile = KInputNone;
        if (pAttrs.hasAttribute(KXMLQLCUniversePlugin))
            patch.plugin = pAttrs.value(KXMLQLCUniversePlugin).toString();
        if (pAttrs.hasAttribute(KXMLQLCUniverseLineUID))
            patch.uid = pAttrs.value(KXMLQLCUniverseLineUID).toString();
        if (pAttrs.hasAttribute(KXMLQLCUniverseLine))
            patch.line = pAttrs.value(KXMLQLCUniverseLine).toString().toUInt();
        if (pAttrs.hasAttribute(KXMLQLCUniverseNetwork))
            patch.network = pAttrs.value(KXMLQLCUniverseNetwork).toString();
        if (pAttrs.hasAttribute(KXMLQLCUniverseProfileName))
            patch.profile = pAttrs.value(KXMLQLCUniverseProfileName).toString();
        if (pAttrs.hasAttribute(KXMLQLCUniverseHIDProfileName))
            patch.hidProfile = pAttrs.value(KXMLQLCUniverseHIDProfileName).toString();

        /* The parameters are read BEFORE the patch is applied: a unicast
           target (outputIP) is what decides which local interface reaches
           the patch's network -- see NetworkRoute::resolve(). */
        while (root.readNextStartElement())
        {
            if (root.name() == KXMLQLCUniversePluginParameters)
            {
                QXmlStreamAttributes pluginAttrs = root.attributes();
                for (int i = 0; i < pluginAttrs.count(); i++)
                    patch.params[pluginAttrs.at(i).name().toString()] =
                            pluginAttrs.at(i).value().toString();
            }
            root.skipCurrentElement();
        }

        ioMap->loadPatch(index, patch);
    }

    if (slotDevices.isEmpty() == false)
        ioMap->setInputSlot(index, slotDevices, slotRoles);

    return true;
}

void Universe::setSlotOwner(RigOwner::Owner owner)
{
    m_slotOwner = owner == RigOwner::House ? RigOwner::House : RigOwner::Tour;
}

QList<Universe::OfflinePatch> Universe::outputPatchDescriptions() const
{
    QList<OfflinePatch> out;
    foreach (OutputPatch *op, m_outputPatchList)
    {
        OfflinePatch p;
        p.tag = OutputPatchTag;
        p.plugin = op->pluginName();
        p.uid = op->outputName();
        p.profile = QString();
        p.params = op->getPluginParameters();
        if (op->isPending())
        {
            // Pending: the same UID, line and network it was loaded with.
            p.line = op->pendingLine();
            p.network = op->pendingNetwork();
        }
        else
        {
            p.line = op->output();
            p.network = NetworkRoute::networkForLine(op->outputName());
        }
        out << p;
    }
    foreach (const OfflinePatch &p, m_offlinePatches)
        if (p.tag == OutputPatchTag)
            out << p;
    return out;
}

void Universe::setShowOutputBase(const QList<OfflinePatch> &patches)
{
    m_showOutputBase = patches;
    m_hasShowOutputBase = true;
}

void Universe::clearShowOutputBase()
{
    m_showOutputBase.clear();
    m_hasShowOutputBase = false;
}

void Universe::setNotOurs(bool whole, const QList<QPair<int, int>> &ranges)
{
    {
        QMutexLocker locker(&m_notOursMutex);
        m_notOursRanges = ranges;
    }
    m_notOursWhole.store(whole, std::memory_order_relaxed);
}

QList<QPair<int, int>> Universe::notOursRanges() const
{
    QMutexLocker locker(&m_notOursMutex);
    return m_notOursRanges;
}

bool Universe::isNotOurs(int channel) const
{
    if (notOursWhole())
        return true;
    QMutexLocker locker(&m_notOursMutex);
    for (const QPair<int, int> &r : m_notOursRanges)
        if (channel >= r.first && channel <= r.second)
            return true;
    return false;
}

QList<Universe::OfflinePatch> Universe::offlinePatches() const
{
    return m_offlinePatches;
}

void Universe::addOfflinePatch(const OfflinePatch &patch)
{
    m_offlinePatches.append(patch);
}

void Universe::removeOfflinePatch(int index)
{
    if (index >= 0 && index < m_offlinePatches.count())
        m_offlinePatches.removeAt(index);
}

void Universe::clearOfflinePatches(PatchTagType tag)
{
    for (int i = m_offlinePatches.count() - 1; i >= 0; i--)
        if (m_offlinePatches.at(i).tag == tag)
            m_offlinePatches.removeAt(i);
}

bool Universe::loadXMLPluginParameters(QXmlStreamReader &root, PatchTagType currentTag, int patchIndex)
{
    if (root.name() != KXMLQLCUniversePluginParameters)
    {
        qWarning() << Q_FUNC_INFO << "PluginParameters node not found";
        return false;
    }

    QXmlStreamAttributes pluginAttrs = root.attributes();
    for (int i = 0; i < pluginAttrs.count(); i++)
    {
        QXmlStreamAttribute attr = pluginAttrs.at(i);
        if (currentTag == InputPatchTag)
        {
            InputPatch *ip = inputPatch();
            if (ip != NULL)
                ip->setPluginParameter(attr.name().toString(), attr.value().toString());
        }
        else if (currentTag == OutputPatchTag)
        {
            OutputPatch *op = outputPatch(patchIndex);
            if (op != NULL)
                op->setPluginParameter(attr.name().toString(), attr.value().toString());
        }
        else if (currentTag == FeedbackPatchTag)
        {
            OutputPatch *fbp = feedbackPatch();
            if (fbp != NULL)
                fbp->setPluginParameter(attr.name().toString(), attr.value().toString());
        }
    }
    root.skipCurrentElement();

    return true;
}

bool Universe::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != NULL);

    doc->writeStartElement(KXMLQLCUniverse);
    doc->writeAttribute(KXMLQLCUniverseName, name());
    doc->writeAttribute(KXMLQLCUniverseID, QString::number(id()));

    if (passthrough() == true)
        doc->writeAttribute(KXMLQLCUniversePassthrough, KXMLQLCTrue);
    // Only a House slot says so: a show that never uses slots is unchanged.
    if (m_slotOwner == RigOwner::House)
        doc->writeAttribute(KXMLQLCUniverseOwner, RigOwner::xmlName(m_slotOwner));

    /* A kept (offline) patch is written back exactly as it was loaded --
       a show saved on a machine away from its network must not lose a
       patch it merely could not open here. */
    auto saveOffline = [&](PatchTagType tag, const QString &xmlTag)
    {
        foreach (const OfflinePatch &p, m_offlinePatches)
        {
            if (p.tag != tag)
                continue;
            savePatchXML(doc, xmlTag, p.plugin, p.uid, p.line, p.profile,
                         p.params, p.hidProfile, p.network);
        }
    };

    /* A controller slot: <Input> is its first device, as older builds
       open it, whichever is live here; then the whole list. */
    QList<InputSlotDevice> slotDevices = m_inputSlot;
    if (m_inputSlotLive >= 0 && m_inputSlotLive < slotDevices.count()
        && inputPatch() != NULL && inputPatch()->isPatched())
    {
        InputSlotDevice &live = slotDevices[m_inputSlotLive];
        live.profile = inputPatch()->profileName() == KInputNone ? QString()
                                                                 : inputPatch()->profileName();
        live.hidProfile = inputPatch()->hidProfileName();
        live.params = inputPatch()->getPluginParameters();
        live.line = inputPatch()->input();
    }
    int slotFirst = -1;
    for (int i = 0; i < slotDevices.count() && slotFirst < 0; i++)
        if (slotDevices.at(i).anyGamepad == false)
            slotFirst = i;

    if (slotFirst >= 0 && slotFirst != m_inputSlotLive)
    {
        const InputSlotDevice &d = slotDevices.at(slotFirst);
        savePatchXML(doc, KXMLQLCUniverseInputPatch, d.plugin, d.uid, d.line, d.profile,
                     d.params, d.hidProfile, QString());
    }
    else if (inputPatch() != NULL)
    {
        savePatchXML(doc, KXMLQLCUniverseInputPatch, inputPatch()->pluginName(), inputPatch()->inputName(),
            inputPatch()->input(), inputPatch()->profileName(), inputPatch()->getPluginParameters(),
            inputPatch()->hidProfileName(), NetworkRoute::networkForLine(inputPatch()->inputName()));
    }
    else
    {
        saveOffline(InputPatchTag, KXMLQLCUniverseInputPatch);
    }
    if (slotDevices.isEmpty() == false)
    {
        doc->writeStartElement(KXMLQLCUniverseInputSlot);
        if (m_inputSlotRoles.isEmpty() == false)
            doc->writeAttribute(KXMLQLCUniverseInputSlotRoles, m_inputSlotRoles.join(QLatin1Char(',')));
        foreach (const InputSlotDevice &d, slotDevices)
        {
            doc->writeStartElement(KXMLQLCUniverseInputSlotDevice);
            doc->writeAttribute(KXMLQLCUniversePlugin, d.plugin);
            if (d.anyGamepad)
                doc->writeAttribute(KXMLQLCUniverseInputSlotAny, KXMLQLCTrue);
            else
            {
                doc->writeAttribute(KXMLQLCUniverseLineUID, d.uid);
                doc->writeAttribute(KXMLQLCUniverseLine, QString::number(d.line));
            }
            if (d.profile.isEmpty() == false && d.profile != KInputNone)
                doc->writeAttribute(KXMLQLCUniverseProfileName, d.profile);
            if (d.hidProfile.isEmpty() == false)
                doc->writeAttribute(KXMLQLCUniverseHIDProfileName, d.hidProfile);
            savePluginParametersXML(doc, d.params);
            doc->writeEndElement();
        }
        doc->writeEndElement();
    }
    /* A venue's patch is on: the show keeps its own (docs/design/VENUES.md section 5). */
    const QList<OfflinePatch> outputs = m_hasShowOutputBase ? m_showOutputBase
                                                            : outputPatchDescriptions();
    foreach (const OfflinePatch &p, outputs)
        savePatchXML(doc, KXMLQLCUniverseOutputPatch, p.plugin, p.uid, p.line, p.profile,
                     p.params, p.hidProfile, p.network);
    if (feedbackPatch() != NULL)
    {
        savePatchXML(doc, KXMLQLCUniverseFeedbackPatch, feedbackPatch()->pluginName(), feedbackPatch()->outputName(),
            feedbackPatch()->output(), "", feedbackPatch()->getPluginParameters(), QString(),
            NetworkRoute::networkForLine(feedbackPatch()->outputName()));
    }
    else
    {
        saveOffline(FeedbackPatchTag, KXMLQLCUniverseFeedbackPatch);
    }

    /* End the <Universe> tag */
    doc->writeEndElement();

    return true;
}

void Universe::savePatchXML(
    QXmlStreamWriter *doc,
    const QString &tag,
    const QString &pluginName,
    const QString &lineName,
    quint32 line,
    QString profileName,
    QMap<QString, QVariant> parameters,
    const QString &hidProfileName,
    const QString &network) const
{
    // sanity check: don't save invalid data
    if (pluginName.isEmpty() || pluginName == KInputNone || line == QLCIOPlugin::invalidLine())
        return;

    doc->writeStartElement(tag);
    doc->writeAttribute(KXMLQLCUniversePlugin, pluginName);
    doc->writeAttribute(KXMLQLCUniverseLineUID, lineName);
    doc->writeAttribute(KXMLQLCUniverseLine, QString::number(line));
    if (!profileName.isEmpty() && profileName != KInputNone)
        doc->writeAttribute(KXMLQLCUniverseProfileName, profileName);
    if (!hidProfileName.isEmpty())
        doc->writeAttribute(KXMLQLCUniverseHIDProfileName, hidProfileName);
    /* The network the line is on ("172.18.2.0/24") -- what loading resolves
       by, so the show follows the network to another machine or address.
       UID/Line stay for older builds, which ignore this attribute. */
    if (!network.isEmpty())
        doc->writeAttribute(KXMLQLCUniverseNetwork, network);

    savePluginParametersXML(doc, parameters);
    doc->writeEndElement();
}

bool Universe::savePluginParametersXML(QXmlStreamWriter *doc,
                                       QMap<QString, QVariant> parameters) const
{
    Q_ASSERT(doc != NULL);

    if (parameters.isEmpty())
        return false;

    doc->writeStartElement(KXMLQLCUniversePluginParameters);
    QMapIterator<QString, QVariant> it(parameters);
    while (it.hasNext())
    {
        it.next();
        QString pName = it.key();
        QVariant pValue = it.value();
        doc->writeAttribute(pName, pValue.toString());
    }
    doc->writeEndElement();

    return true;
}

