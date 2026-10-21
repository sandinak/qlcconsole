/*
  Q Light Controller
  genericfader.cpp

  Copyright (c) Heikki Junnila

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

#include <QDebug>

#include "genericfader.h"
#include "fadechannel.h"
#include "doc.h"
#include "fixture.h"
#include "mastertimer.h"

GenericFader::GenericFader(QObject *parent)
    : QObject(parent)
    , m_fid(Function::invalidId())
    , m_priority(Universe::Auto)
    , m_handleSecondary(false)
    , m_intensity(1.0)
    , m_parentIntensity(1.0)
    , m_paused(false)
    , m_enabled(true)
    , m_fadeOut(false)
    , m_deleteRequest(false)
    , m_blendMode(Universe::NormalBlend)
    , m_monitoring(false)
{
}

GenericFader::~GenericFader()
{
    // What it was showing on unpatched fixtures goes out with it.
    foreach (const FadeChannel &fc, m_channels)
        if (fc.isInert())
            showOnFixture(fc, 0);
}

void GenericFader::showOnFixture(const FadeChannel &fc, quint32 value) const
{
    if (m_doc == nullptr)
        return;
    Fixture *fx = m_doc->fixture(fc.fixture());
    if (fx == nullptr || fx->isPatched())
        return;
    const int n = qMax(1, fc.channelCount());
    for (int i = 0; i < n; ++i)
        fx->setPreviewValue(fc.channel() + quint32(i), ((const uchar *)&value)[n - 1 - i]);
}

QString GenericFader::name() const
{
    return m_name;
}

void GenericFader::setName(QString name)
{
    m_name = name;
}

quint32 GenericFader::parentFunctionID() const
{
    return m_fid;
}

void GenericFader::setParentFunctionID(quint32 fid)
{
    m_fid = fid;
}

int GenericFader::priority() const
{
    return m_priority;
}

void GenericFader::setPriority(int priority)
{
    m_priority = priority;
}

bool GenericFader::handleSecondary()
{
    return m_handleSecondary;
}

void GenericFader::setHandleSecondary(bool enable)
{
    m_handleSecondary = enable;
}

quint32 GenericFader::channelHash(quint32 fixtureID, quint32 channel)
{
    return ((fixtureID & 0x0000FFFF) << 16) | (channel & 0x0000FFFF);
}

void GenericFader::add(const FadeChannel& ch)
{
    quint32 hash = channelHash(ch.fixture(), ch.channel());

    std::lock_guard<ChannelsLock> l(m_channelsLock);
    QHash<quint32,FadeChannel>::iterator channelIterator = m_channels.find(hash);
    if (channelIterator != m_channels.end())
    {
        // perform a HTP check
        if (channelIterator.value().current() <= ch.current())
            channelIterator.value() = ch;
    }
    else
    {
        m_channels.insert(hash, ch);
        qDebug() << "Added new fader with hash" << hash;
    }
}

void GenericFader::replace(const FadeChannel &ch)
{
    quint32 hash = channelHash(ch.fixture(), ch.channel());
    std::lock_guard<ChannelsLock> l(m_channelsLock);
    m_channels.insert(hash, ch);
}

void GenericFader::remove(FadeChannel *ch)
{
    if (ch == NULL)
        return;

    quint32 hash = channelHash(ch->fixture(), ch->channel());
    std::lock_guard<ChannelsLock> l(m_channelsLock);
    if (m_channels.remove(hash) == 0)
        qDebug() << "No FadeChannel found with hash" << hash;
}

void GenericFader::removeAll()
{
    std::lock_guard<ChannelsLock> l(m_channelsLock);
    foreach (const FadeChannel &fc, m_channels)
        if (fc.isInert())
            showOnFixture(fc, 0);
    m_channels.clear();
}

void GenericFader::replaceAll(const QList<FadeChannel> &channels)
{
    QHash<quint32, FadeChannel> next;
    next.reserve(channels.size());
    for (const FadeChannel &fc : channels)
        next.insert(channelHash(fc.fixture(), fc.channel()), fc);

    std::lock_guard<ChannelsLock> l(m_channelsLock);
    // Unpatched channels dropped from the set go dark on the fixture.
    for (auto it = m_channels.constBegin(); it != m_channels.constEnd(); ++it)
        if (it.value().isInert() && !next.contains(it.key()))
            showOnFixture(it.value(), 0);
    m_channels.swap(next);
}

void GenericFader::setChannelsFlag(int flag, bool on)
{
    std::lock_guard<ChannelsLock> l(m_channelsLock);
    for (auto it = m_channels.begin(); it != m_channels.end(); ++it)
    {
        if (on)
            it.value().addFlag(flag);
        else
            it.value().removeFlag(flag);
    }
}

bool GenericFader::deleteRequested()
{
    return m_deleteRequest;
}

void GenericFader::requestDelete()
{
    m_deleteRequest = true;
}

GenericFader::ChannelRef GenericFader::getChannelFader(const Doc *doc, Universe *universe, quint32 fixtureID, quint32 channel)
{
    FadeChannel fc(doc, fixtureID, channel);
    quint32 primary = fc.primaryChannel();
    quint32 hash;

    // calculate hash depending on primary channel presence
    if (handleSecondary() && primary != QLCChannel::invalid())
        hash = channelHash(fc.fixture(), primary);
    else
        hash = channelHash(fc.fixture(), fc.channel());

    // Held from the lookup until the caller is done with the channel: the
    // universe thread's write() steps and removes channels under this lock.
    std::unique_lock<ChannelsLock> lock(m_channelsLock);
    if (doc != nullptr)
        m_doc = doc;

    // search for existing FadeChannel
    QHash<quint32,FadeChannel>::iterator channelIterator = m_channels.find(hash);
    if (channelIterator != m_channels.end())
    {
        FadeChannel *fcFound = &channelIterator.value();

        if (handleSecondary() &&
            fcFound->channelCount() == 1 &&
            primary != QLCChannel::invalid())
        {
            //qDebug() << "Adding channel to primary" << channel;
            fcFound->addChannel(channel);
            // An inert channel (unpatched fixture) has no address to read.
            if (universe && fcFound->address() != QLCChannel::invalid())
                fcFound->setCurrent(universe->preGMValue(fcFound->address() + 1), 1);
        }
        return ChannelRef(std::move(lock), fcFound);
    }

    // set current universe value -- none for an inert channel (a fixture in
    // the inventory): it has no address, and sends nothing.
    if (universe && fc.address() != QLCChannel::invalid())
        fc.setCurrent(universe->preGMValue(fc.address()));

    // new channel. Add to GenericFader
    channelIterator = m_channels.insert(hash, fc);
    //qDebug() << "Added new fader with hash" << hash;

    return ChannelRef(std::move(lock), &channelIterator.value());
}

QHash<quint32, FadeChannel> GenericFader::channels() const
{
    std::lock_guard<ChannelsLock> l(m_channelsLock);
    return m_channels;
}

int GenericFader::channelsCount() const
{
    std::lock_guard<ChannelsLock> l(m_channelsLock);
    return m_channels.count();
}

void GenericFader::write(Universe *universe)
{
    if (m_zeroAll)
    {
        for (int c = 0; c < 512; ++c)
            universe->write(c, 0, true);
    }

    if (m_monitoring)
        emit preWriteData(universe->id(), universe->preGMValues());

    qreal compIntensity = intensity() * parentIntensity();

    //qDebug() << "[GenericFader] writing channels: " << this << m_channels.count();

    // iterate through all the channels handled by this fader
    std::lock_guard<ChannelsLock> l(m_channelsLock);
    QMutableHashIterator <quint32,FadeChannel> it(m_channels);
    while (it.hasNext() == true)
    {
        FadeChannel& fc(it.next().value());

        /* An unpatched (inventory) fixture's channel has no real address: it
           must not fall through to the bare-channel-number address below. It
           still fades and follows intensity -- and what it would be goes to
           the fixture, for the rig view, instead of to a universe. Removed
           when done like any other channel: kept forever, they held a fading
           fader open so it was never deleted. */
        if (fc.isInert())
        {
            if (m_paused == false)
                fc.nextStep(MasterTimer::tick());
            quint32 value = fc.current();
            const int iflags = fc.flags();
            if (fc.canFade() && (iflags & FadeChannel::Intensity))
                value = fc.current(compIntensity);
            showOnFixture(fc, value);
            const bool zeroDone = fc.current() == 0 && fc.target() == 0 && fc.isReady();
            if (((iflags & FadeChannel::Intensity) && (iflags & FadeChannel::HTP)
                 && m_blendMode == Universe::NormalBlend) || m_fadeOut)
            {
                if (zeroDone)
                {
                    showOnFixture(fc, 0);
                    it.remove();
                    continue;
                }
            }
            if ((iflags & FadeChannel::AutoRemove) && value == fc.target())
                it.remove();
            continue;
        }

        int flags = fc.flags();
        quint32 address = fc.addressInUniverse();
        int channelCount = fc.channelCount();

        if (address == QLCChannel::invalid())
        {
            qWarning() << "Invalid channel found";
            continue;
        }

        if (flags & FadeChannel::SetTarget)
        {
            fc.removeFlag(FadeChannel::SetTarget);
            fc.addFlag(FadeChannel::AutoRemove);
            for (int i = 0; i < channelCount; i++)
                fc.setTarget(universe->preGMValue(address + i), i);
        }

        // Calculate the next step
        if (m_paused == false)
            fc.nextStep(MasterTimer::tick());

        quint32 value = fc.current();

        // Apply intensity to channels that can fade
        if (fc.canFade())
        {
            if ((flags & FadeChannel::CrossFade) && fc.fadeTime() == 0)
            {
                // morph start <-> target depending on intensities
                bool rampUp = fc.target() > fc.start() ? true : false;
                value = rampUp ? fc.target() - fc.start() : fc.start() - fc.target();
                value = qreal(value) * intensity();
                value = qreal(rampUp ? fc.start() + value : fc.start() - value) * parentIntensity();
            }
            else if (flags & FadeChannel::Intensity)
            {
                value = fc.current(compIntensity);
            }
        }

        //qDebug() << "[GenericFader] >>> uni:" << universe->id() << ", address:" << address << ", value:" << value << "int:" << compIntensity;
        if (flags & FadeChannel::Override)
        {
            // Every byte of a 16-bit channel (a fader handling secondary
            // channels holds both in one FadeChannel), MSB first.
            // An edit preview's scenes: HTP among themselves (as at Auto),
            // over everything else. Other overrides (an effect owning a
            // channel) write verbatim.
            const bool edit = flags & FadeChannel::EditOverride;
            for (int i = 0; i < channelCount; i++)
            {
                const uchar b = channelCount == 1 ? uchar(value) : ((uchar *)&value)[channelCount - 1 - i];
                if (edit)
                    universe->writeOverride(address + i, b);
                else
                    universe->write(address + i, b, true);
            }
            continue;
        }
        else if (flags & FadeChannel::Relative)
        {
            universe->writeRelative(address, value, channelCount);
        }
        else if (flags & FadeChannel::Flashing)
        {
            for (int i = 0; i < channelCount; i++)
                universe->write(address + i, ((uchar *)&value)[channelCount - 1 - i],
                                flags & FadeChannel::ForceLTP ? true : false);
            continue;
        }
        else
        {
            // treat value as a whole, so do this just once per FadeChannel
            universe->writeBlended(address, value, channelCount, m_blendMode);
        }

        if (((flags & FadeChannel::Intensity) &&
            (flags & FadeChannel::HTP) &&
            m_blendMode == Universe::NormalBlend) || m_fadeOut)
        {
            // Remove all channels that reach their target _zero_ value.
            // They have no effect either way so removing them saves a bit of CPU.
            if (fc.current() == 0 && fc.target() == 0 && fc.isReady())
                it.remove();
        }

        if (flags & FadeChannel::AutoRemove && value == fc.target())
        {
            // A one-shot writer (slider, programmer edit) leaves the value
            // where it put it: still set, for auto-open shutters. A fade-out
            // (SetTarget) is a function letting go, not setting anything.
            if (m_fadeOut == false)
                universe->holdChannels(int(address), channelCount);
            it.remove();
        }
    }

    // self-request deletion when fadeout is complete
    if (m_fadeOut && m_channels.isEmpty())
    {
        m_fadeOut = false;
        requestDelete();
    }
}

qreal GenericFader::intensity() const
{
    return m_intensity;
}

void GenericFader::adjustIntensity(qreal fraction)
{
    //qDebug() << name() << "I FADER intensity" << fraction << ", PARENT:" << m_parentIntensity;
    m_intensity = fraction;
}

qreal GenericFader::parentIntensity() const
{
    return m_parentIntensity;
}

void GenericFader::setParentIntensity(qreal fraction)
{
    //qDebug() << name() << "P FADER intensity" << m_intensity << ", PARENT:" << fraction;
    m_parentIntensity = fraction;
}

bool GenericFader::isPaused() const
{
    return m_paused;
}

void GenericFader::setPaused(bool paused)
{
    m_paused = paused;
}

bool GenericFader::isEnabled() const
{
    return m_enabled;
}

void GenericFader::setEnabled(bool enable)
{
    m_enabled = enable;
}

bool GenericFader::isFadingOut() const
{
    return m_fadeOut;
}

void GenericFader::setFadeOut(bool enable, uint fadeTime)
{
    m_fadeOut = enable;

    if (fadeTime == 0)
        return;

    std::lock_guard<ChannelsLock> l(m_channelsLock);
    QMutableHashIterator <quint32,FadeChannel> it(m_channels);
    while (it.hasNext() == true)
    {
        FadeChannel& fc(it.next().value());

        fc.setStart(fc.current());
        // if not HTP and/or flashing, request channels
        // to target the current universe value
        // (will be handled in the write method)
        if (((fc.flags() & FadeChannel::Flashing) == 0) &&
            ((fc.flags() & FadeChannel::Intensity) == 0))
            fc.addFlag(FadeChannel::SetTarget);
        fc.setTarget(0);
        fc.setElapsed(0);
        fc.setReady(false);
        fc.setFadeTime(fc.canFade() ? fadeTime : 0);
        // if flashing, remove the flag and treat
        // it like a regular fade out to target
        fc.removeFlag(FadeChannel::Flashing);
    }
}

void GenericFader::setFadeOut(bool enable, uint fadeTime, const QHash<quint32, uint> &channelFadeTimes)
{
    m_fadeOut = enable;

    std::lock_guard<ChannelsLock> l(m_channelsLock);
    QMutableHashIterator <quint32,FadeChannel> it(m_channels);
    while (it.hasNext() == true)
    {
        FadeChannel& fc(it.next().value());

        fc.setStart(fc.current());
        if (((fc.flags() & FadeChannel::Flashing) == 0) &&
            ((fc.flags() & FadeChannel::Intensity) == 0))
            fc.addFlag(FadeChannel::SetTarget);
        fc.setTarget(0);
        fc.setElapsed(0);
        fc.setReady(false);
        // Per-channel override (keyed by channelHash) wins over the default.
        const uint t = channelFadeTimes.value(it.key(), fadeTime);
        fc.setFadeTime(fc.canFade() ? t : 0);
        fc.removeFlag(FadeChannel::Flashing);
    }
}

void GenericFader::setChannelTimings(const std::function<bool(const FadeChannel &, uint &, uint &)> &timing)
{
    std::lock_guard<ChannelsLock> l(m_channelsLock);
    QMutableHashIterator <quint32,FadeChannel> it(m_channels);
    while (it.hasNext() == true)
    {
        FadeChannel& fc(it.next().value());
        uint fade = fc.fadeTime(), delay = 0;
        if (!timing(fc, fade, delay))
            continue;
        fc.setFadeTime(fc.canFade() ? fade : 0);
        fc.setDelay(delay);
    }
}

void GenericFader::setBlendMode(Universe::BlendMode mode)
{
    m_blendMode = mode;
}

void GenericFader::setMonitoring(bool enable)
{
    m_monitoring = enable;
}

void GenericFader::resetCrossfade()
{
    qDebug() << name() << "resetting crossfade channels";
    std::lock_guard<ChannelsLock> l(m_channelsLock);
    QMutableHashIterator <quint32,FadeChannel> it(m_channels);
    while (it.hasNext() == true)
    {
        FadeChannel& fc(it.next().value());
        fc.removeFlag(FadeChannel::CrossFade);
    }
}
