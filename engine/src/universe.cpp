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
#include <math.h>

#include "channelmodifier.h"
#include "inputoutputmap.h"
#include "genericfader.h"
#include "qlcioplugin.h"
#include "outputpatch.h"
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
    for (int i = 0; i < UNIVERSE_SIZE; i++)
    {
        m_writtenFrame[i].store(0, std::memory_order_relaxed);
        m_held[i].store(0, std::memory_order_relaxed);
        m_pastGMFrame[i].store(0, std::memory_order_relaxed);
    }

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

    delete m_inputPatch;
    int opCount = m_outputPatchList.count();
    for (int i = 0; i < opCount; i++)
    {
        OutputPatch *patch = m_outputPatchList.takeLast();
        delete patch;
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
    m_inhibitOutput = enable;
}

bool Universe::inhibitOutput() const
{
    return m_inhibitOutput;
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

/************************************************************************
 * Faders
 ************************************************************************/

QSharedPointer<GenericFader> Universe::requestFader(Universe::FaderPriority priority)
{
    int insertPos = 0;
    QSharedPointer<GenericFader> fader = QSharedPointer<GenericFader>(new GenericFader());
    fader->setPriority(priority);

    {
        QMutexLocker fadersLocker(&m_fadersMutex);
        {
            for (int i = m_faders.count() - 1; i >= 0; i--)
            {
                const QSharedPointer<GenericFader>& f = m_faders.at(i);
                if (!f.isNull() && f->priority() <= fader->priority())
                {
                    insertPos = i + 1;
                    break;
                }
            }
        }

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
        m_faders.takeAt(index);
        fader.clear();
    }
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
    int newPos = 0;
    for (int i = m_faders.count() - 1; i >= 0; i--)
    {
        const QSharedPointer<GenericFader> &f = m_faders.at(i);
        if (!f.isNull() && f->priority() <= priority)
        {
            newPos = i + 1;
            break;
        }
    }
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

void Universe::tick()
{
    m_semaphore.release(1);
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
                fader->removeAll();
                it.remove();
                fader.clear();
                continue;
            }

            if (fader->isEnabled() == false)
                continue;

            //qDebug() << "Processing fader" << fader->name() << fader->channelsCount();
            fader->write(this);
        }
    }

    applyAutoOpen();
    applyOnAtOpen();
    // Writes from here on (the DMX sources, before the next frame) belong
    // to the next frame.
    m_frame.fetch_add(1, std::memory_order_relaxed);

    bool dataChanged = hasChanged();
    QByteArray postGM = m_postGMValues->mid(0, m_usedChannels);
    /* The fixtures held to a maximum draw, on what goes out -- never on the
       stored values, which only the channels written this frame refresh:
       scaled there, an unchanged colour would shrink again every frame. */
    if (applyPowerLimits(postGM) && postGM != m_lastLimited)
    {
        m_lastLimited = postGM;
        dataChanged = true;
    }
    dumpOutput(postGM, dataChanged);
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
    QMutexLocker locker(&m_outputRulesMutex);
    QVector<int> onAtOpen;
    for (int c = 0; c < UNIVERSE_SIZE; c++)
    {
        const char want = c < bits.size() ? char(bits.at(c) & OutputRuleBits) : char(0);
        const char now = m_channelsMask->at(c);
        if (want & OnAtOpenOutput)
            onAtOpen << c;
        if ((now & OutputRuleBits) == want)
            continue;
        // One byte write: the role bits (HTP/LTP/Intensity) never vanish.
        (*m_channelsMask)[c] = char((now & ~OutputRuleBits) | want);
        if ((now & OnAtOpenOutput) && (want & OnAtOpenOutput) == 0)
            m_onAtOpenRestore << c;
        if (want & (GMExempt | SwitchOutput | OnAtOpenOutput))
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
        (*m_postGMValues)[c] = char(onAtOpenValue(c));
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
        if (address + i >= 0 && address + i < UNIVERSE_SIZE)
            m_held[address + i].store(1, std::memory_order_relaxed);
}

bool Universe::isChannelSet(int address) const
{
    if (address < 0 || address >= UNIVERSE_SIZE)
        return false;
    return writtenRecently(address) || m_held[address].load(std::memory_order_relaxed) != 0;
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
            (*m_postGMValues)[r.shutter] = char(r.open);
        }
        else
        {
            updatePostGMValue(r.shutter);
        }
    }
}

void Universe::run()
{
    m_running = true;
    int timeout = int(MasterTimer::tick()) * 2;

    qDebug() << "Universe thread started" << id();

    while (m_running)
    {
        if (m_semaphore.tryAcquire(1, timeout) == false)
        {
            //qWarning() << "Semaphore not acquired on universe" << id();
            continue;
        }
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
        (*m_postGMValues) = (*m_passthroughValues);
    else
        m_postGMValues->fill(0);

    m_modifiers.fill(NULL, UNIVERSE_SIZE);
    m_passthrough = false; // not releasing m_passthroughValues, see comment in setPassthrough
}

void Universe::reset(int address, int range)
{
    if (address >= UNIVERSE_SIZE)
        return;

    if (address + range > UNIVERSE_SIZE)
       range = UNIVERSE_SIZE - address;

    memset(m_preGMValues->data() + address, 0, range * sizeof(*m_preGMValues->data()));
    memset(m_blackoutValues->data() + address, 0, range * sizeof(*m_blackoutValues->data()));
    memcpy(m_postGMValues->data() + address, m_modifiedZeroValues->data() + address, range * sizeof(*m_postGMValues->data()));

    applyPassthroughValues(address, range);
}

void Universe::applyPassthroughValues(int address, int range)
{
    if (!m_passthrough)
        return;

    for (int i = address; i < address + range && i < UNIVERSE_SIZE; i++)
    {
        if (static_cast<uchar>(m_postGMValues->at(i)) < static_cast<uchar>(m_passthroughValues->at(i))) // HTP merge
        {
            (*m_postGMValues)[i] = (*m_passthroughValues)[i];
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
       (reset() on the universe thread), the two raced on its header. */
    QMutexLocker frameLocker(&m_frameMutex);
    return *m_preGMValues;
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
        (*m_postGMValues)[channel] = char(onAtOpenValue(channel));
        return;
    }

    uchar value = preGMValue(channel);

    /* Output rules: a GM-exempt channel is never scaled, and neither is a
       relay -- the Grand Master pulled to 30% would otherwise switch it off.
       The relay's snap comes last, so nothing after it (a modifier, the
       input passthrough) can hand it a level between off and on. */
    if (value != 0 && (mask & (GMExempt | SwitchOutput)) == 0 && pastGM(channel) == false)
        value = applyGM(channel, value);

    value = applyModifiers(channel, value);
    value = applyPassthrough(channel, value);

    if (mask & SwitchOutput)
        value = value >= 128 ? UCHAR_MAX : 0;

    (*m_postGMValues)[channel] = static_cast<char>(value);
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
        emit inputPatchChanged();
        return result;
    }

    return true;
}

bool Universe::setOutputPatch(QLCIOPlugin *plugin, quint32 output, int index)
{
    if (index < 0)
        return false;

    qDebug() << "[Universe] setOutputPatch - ID:" << m_id
             << ", plugin:" << ((plugin == NULL) ? "None" : plugin->name()) << ", output:" << output;

    // replace or delete an existing patch
    if (index < m_outputPatchList.count())
    {
        if (plugin == NULL || output == QLCIOPlugin::invalidLine())
        {
            // need to delete an existing patch
            OutputPatch *patch = m_outputPatchList.takeAt(index);
            delete patch;
            emit outputPatchesCountChanged();
            return true;
        }

        OutputPatch *patch = m_outputPatchList.at(index);
        bool result = patch->set(plugin, output);
        emit outputPatchChanged();
        return result;
    }
    else
    {
        if (plugin == NULL || output == QLCIOPlugin::invalidLine())
            return false;

        // add a new patch
        OutputPatch *patch = new OutputPatch(m_id, this);
        bool result = patch->set(plugin, output);
        m_outputPatchList.append(patch);
        emit outputPatchesCountChanged();
        return result;
    }

    return false;
}

bool Universe::setOutputPatchPending(QLCIOPlugin *plugin, const QString &pendingUID, int index,
                                     quint32 savedLine, const QString &network)
{
    if (index < 0 || plugin == NULL || pendingUID.isEmpty())
        return false;

    qDebug() << "[Universe] setOutputPatchPending - ID:" << m_id
             << ", plugin:" << plugin->name() << ", pending UID:" << pendingUID;

    if (index < m_outputPatchList.count())
    {
        OutputPatch *patch = m_outputPatchList.at(index);
        patch->setPending(plugin, pendingUID, savedLine, network);
        emit outputPatchChanged();
        return true;
    }
    else
    {
        OutputPatch *patch = new OutputPatch(m_id, this);
        patch->setPending(plugin, pendingUID, savedLine, network);
        m_outputPatchList.append(patch);
        emit outputPatchesCountChanged();
        return true;
    }
}

bool Universe::setFeedbackPatch(QLCIOPlugin *plugin, quint32 output)
{
    qDebug() << Q_FUNC_INFO << "plugin:" << plugin << "output:" << output;
    if (m_fbPatch == NULL)
    {
        if (plugin == NULL || output == QLCIOPlugin::invalidLine())
            return false;

        m_fbPatch = new OutputPatch(m_id, this);
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
    // Fork Blind: hold the physical rig dark while the preview keeps running.
    // processFaders() still emits universeWritten() (the 2D monitor source),
    // so the look is visible on screen but never reaches the output plugins.
    //
    // Output rules: the Blackout-exempt channels (house, work and hall
    // lights, power relays) keep their live value through Blackout and, when
    // blindHoldsExempt(), through Blind -- which then sends what Blackout
    // would rather than nothing, so those circuits are not left to a node's
    // hold-last-look. A universe with none behaves exactly as before.
    if (m_inhibitOutput)
    {
        if (blindHoldsExempt() == false || hasBlackoutExempt(data.size()) == false)
            return;
        const QByteArray frame = blackoutFrame(data);
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

    foreach (OutputPatch *op, m_outputPatchList)
    {
        if (m_totalChannelsChanged == true)
            op->setPluginParameter(PLUGIN_UNIVERSECHANNELS, m_totalChannels);

        if (op->blackout())
            op->dump(m_id, blackoutFrame(data), dataChanged);
        else
            op->dump(m_id, data, dataChanged);
    }
    m_totalChannelsChanged = false;
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

uchar Universe::channelCapabilities(ushort channel)
{
    if (channel >= (ushort)m_channelsMask->length())
        return Undefined;

    return m_channelsMask->at(channel);
}

void Universe::setChannelDefaultValue(ushort channel, uchar value)
{
    QMutexLocker frameLocker(&m_frameMutex);
    if (channel >= m_totalChannels)
    {
        m_totalChannels = channel + 1;
        m_totalChannelsChanged = true;
    }

    if (channel >= m_usedChannels)
        m_usedChannels = channel + 1;

    (*m_preGMValues)[channel] = value;
    updatePostGMValue(channel);
}

void Universe::setChannelModifier(ushort channel, ChannelModifier *modifier)
{
    QMutexLocker frameLocker(&m_frameMutex);
    if (channel >= (ushort)m_modifiers.count())
        return;

    m_modifiers[channel] = modifier;

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
    if (m_overrideWritten.size() != UNIVERSE_SIZE)
        m_overrideWritten = QByteArray(UNIVERSE_SIZE, char(0));
    if ((m_channelsMask->at(address) & HTP) && m_overrideWritten.at(address))
        value = qMax(value, uchar(m_preGMValues->at(address)));
    m_overrideWritten[address] = char(1);
    return write(address, value, true);
}

bool Universe::writePastGM(int address, uchar value)
{
    if (address < 0 || address >= UNIVERSE_SIZE)
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

    while (root.readNextStartElement())
    {
        QXmlStreamAttributes pAttrs = root.attributes();
        OfflinePatch patch;

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

    return true;
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

    if (inputPatch() != NULL)
    {
        savePatchXML(doc, KXMLQLCUniverseInputPatch, inputPatch()->pluginName(), inputPatch()->inputName(),
            inputPatch()->input(), inputPatch()->profileName(), inputPatch()->getPluginParameters(),
            inputPatch()->hidProfileName(), NetworkRoute::networkForLine(inputPatch()->inputName()));
    }
    else
    {
        saveOffline(InputPatchTag, KXMLQLCUniverseInputPatch);
    }
    foreach (OutputPatch *op, m_outputPatchList)
    {
        if (op->isPending())
        {
            // Pending: the same UID, line and network it was loaded with.
            savePatchXML(doc, KXMLQLCUniverseOutputPatch, op->pluginName(), op->outputName(),
                op->pendingLine(), "", op->getPluginParameters(), QString(), op->pendingNetwork());
            continue;
        }
        savePatchXML(doc, KXMLQLCUniverseOutputPatch, op->pluginName(), op->outputName(),
            op->output(), "", op->getPluginParameters(), QString(),
            NetworkRoute::networkForLine(op->outputName()));
    }
    saveOffline(OutputPatchTag, KXMLQLCUniverseOutputPatch);
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

