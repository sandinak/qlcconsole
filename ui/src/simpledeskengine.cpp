/*
  Q Light Controller Plus
  simpledeskengine.cpp

  Copyright (c) Heikki Junnila
                Massimo Callegari

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
#include <QMutexLocker>
#include <QVariant>
#include <QDebug>

#include "simpledeskengine.h"
#include "genericfader.h"
#include "mastertimer.h"
#include <utility>
#include "fadechannel.h"
#include "cuestack.h"
#include "universe.h"
#include "doc.h"
#include "inputoutputmap.h"
#include "outputrulekeeper.h"
#include "fixturerest.h"

#define PROP_ID "id"

/****************************************************************************
 * Initialization
 ****************************************************************************/

SimpleDeskEngine::SimpleDeskEngine(Doc* doc)
    : QObject(doc)
    , m_doc(doc)
{
    qDebug() << Q_FUNC_INFO;
    Q_ASSERT(doc != NULL);
    doc->masterTimer()->registerDMXSource(this);
    /* The emergency stop (ALL OFF, Stop All): what the Simple Desk had up,
       on any channel but pan and tilt, and not moved since, is let go of -- a level held
       with nothing running is what Stop All is for (E7: told later if the
       stop came from another thread, so a channel pushed again in the
       meantime keeps its new value). */
    if (OutputRuleKeeper *rules = doc->outputRuleKeeper())
        connect(rules, &OutputRuleKeeper::emergencyStopStruck, this, [this](quint32 session) {
            releaseForStop(session);
        });
    /* A fixture patched, moved or unpatched under a channel the desk holds:
       the desk's channel there was made for what was patched before (its
       fader channel is keyed by fixture and channel), and a second one for
       the new fixture would write the same address -- the output then
       followed hash order. Rebuilt from the desk's values at the next
       write, one channel an address. */
    auto repatched = [this](quint32) {
        QMutexLocker locker(&m_mutex);
        if (m_values.isEmpty())
            return;
        m_repatched = true;
        setChanged(true);
    };
    connect(doc, &Doc::fixtureAdded, this, repatched);
    connect(doc, &Doc::fixtureChanged, this, repatched);
    connect(doc, &Doc::fixtureRemoved, this, repatched);
}

void SimpleDeskEngine::releaseForStop(quint32 session)
{
    QList<uint> drop;
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it)
        {
            if (m_channelEpochs.value(it.key()) >= session)
                continue;           // pushed after the stop: the hand's
            /* Pan and tilt stay where the desk has them, as Stop ALL leaves
               every head (FixtureRest): let go of, the channel would fall
               to its definition's default -- usually 0 -- and the head
               would swing across the stage. */
            if (FixtureRest::holdsPositionAt(m_doc, it.key()))
                continue;
            drop << it.key();
        }
    }
    for (uint ch : std::as_const(drop))
        resetChannel(ch);
}

SimpleDeskEngine::~SimpleDeskEngine()
{
    qDebug() << Q_FUNC_INFO;

    clearContents();
    m_doc->masterTimer()->unregisterDMXSource(this);
}

void SimpleDeskEngine::clearContents()
{
    qDebug() << Q_FUNC_INFO;

    // Stop all cuestacks and wait for each of them to stop
    foreach (CueStack* cs, m_cueStacks)
    {
        cs->stop();
        while (cs->isStarted() == true) { /* NOP */ }
    }

    QMutexLocker locker(&m_mutex);
    foreach (CueStack* cs, m_cueStacks)
        delete cs;
    m_cueStacks.clear();
    m_values.clear();
    locker.unlock();
    emit heldChannelsChanged();
}

/****************************************************************************
 * Universe Values
 ****************************************************************************/

void SimpleDeskEngine::setValue(uint channel, uchar value)
{
    //qDebug() << Q_FUNC_INFO << "channel:" << channel << ", value:" << value;

    QMutexLocker locker(&m_mutex);
    const bool taken = !m_values.contains(channel);
    /* Show only (S3): a channel really moved -- a new value, from the
       desk's fader or its input -- is the show from now on; the same value
       again (an echo, a page reload) stamps nothing. A stamp given before a
       Show-only fixture came onto the address does not count there (S0:
       the channel's own stamp is later). */
    if (taken || m_values.value(channel) != value)
        m_channelEpochs.insert(channel, GenericFader::stampEpoch());
    m_values[channel] = value;
    setChanged(true);
    locker.unlock();
    if (taken)
        emit heldChannelsChanged();
}

QList<uint> SimpleDeskEngine::heldChannels() const
{
    QMutexLocker locker(&m_mutex);
    return m_values.keys();
}

uchar SimpleDeskEngine::value(uint channel) const
{
    QMutexLocker locker(&m_mutex);
    if (m_values.contains(channel) == true)
        return m_values[channel];
    else
        return 0;
}

bool SimpleDeskEngine::hasChannel(uint channel)
{
    QMutexLocker locker(&m_mutex);
    return m_values.contains(channel);
}

void SimpleDeskEngine::setCue(const Cue& cue)
{
    qDebug() << Q_FUNC_INFO;

    QMutexLocker locker(&m_mutex);
    m_values = cue.values();
    setChanged(true);
    locker.unlock();
    emit heldChannelsChanged();
}

Cue SimpleDeskEngine::cue() const
{
    QMutexLocker locker(&m_mutex);
    return Cue(m_values);
}

void SimpleDeskEngine::resetUniverse(int universe)
{
    qDebug() << Q_FUNC_INFO;

    // remove values previously set on universe
    QMutexLocker locker(&m_mutex);
    QMapIterator <uint,uchar> it(m_values);
    while (it.hasNext() == true)
    {
        it.next();
        int uni = it.key() >> 9;
        if (uni == universe)
            m_values.remove(it.key());
    }

    // add command to queue. Will be taken care of at the next writeDMX call
    m_commandQueue.append(QPair<int,quint32>(ResetUniverse, universe));
    setChanged(true);
    locker.unlock();
    emit heldChannelsChanged();
}

void SimpleDeskEngine::resetChannel(uint channel)
{
    QMutexLocker locker(&m_mutex);
    if (m_values.contains(channel))
        m_values.remove(channel);

    // add command to queue. Will be taken care of at the next writeDMX call
    m_commandQueue.append(QPair<int,quint32>(ResetChannel, channel));
    setChanged(true);
    locker.unlock();
    emit heldChannelsChanged();
}

/****************************************************************************
 * Cue Stacks
 ****************************************************************************/

CueStack* SimpleDeskEngine::cueStack(uint stack)
{
    QMutexLocker locker(&m_mutex);

    if (m_cueStacks.contains(stack) == false)
    {
        m_cueStacks[stack] = createCueStack();
        m_cueStacks[stack]->setProperty(PROP_ID, stack);
    }

    return m_cueStacks[stack];
}

CueStack* SimpleDeskEngine::createCueStack()
{
    qDebug() << Q_FUNC_INFO;

    CueStack* cs = new CueStack(m_doc);
    Q_ASSERT(cs != NULL);
    connect(cs, SIGNAL(currentCueChanged(int)), this, SLOT(slotCurrentCueChanged(int)));
    connect(cs, SIGNAL(started()), this, SLOT(slotCueStackStarted()));
    connect(cs, SIGNAL(stopped()), this, SLOT(slotCueStackStopped()));
    return cs;
}

void SimpleDeskEngine::slotCurrentCueChanged(int index)
{
    qDebug() << Q_FUNC_INFO;

    if (sender() == NULL)
        return;

    uint stack = sender()->property(PROP_ID).toUInt();
    emit currentCueChanged(stack, index);
}

void SimpleDeskEngine::slotCueStackStarted()
{
    qDebug() << Q_FUNC_INFO;

    if (sender() == NULL)
        return;

    uint stack = sender()->property(PROP_ID).toUInt();
    emit cueStackStarted(stack);
}

void SimpleDeskEngine::slotCueStackStopped()
{
    qDebug() << Q_FUNC_INFO;

    if (sender() == NULL)
        return;

    uint stack = sender()->property(PROP_ID).toUInt();
    emit cueStackStopped(stack);
}

/************************************************************************
 * Save & Load
 ************************************************************************/

bool SimpleDeskEngine::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCSimpleDeskEngine)
    {
        qWarning() << Q_FUNC_INFO << "Simple Desk Engine node not found";
        return false;
    }

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCCueStack)
        {
            uint id = CueStack::loadXMLID(root);
            if (id != UINT_MAX)
            {
                CueStack* cs = cueStack(id);
                cs->loadXML(root);
            }
            else
            {
                qWarning() << Q_FUNC_INFO << "Missing CueStack ID!";
            }
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unrecognized Simple Desk Engine tag:" << root.name();
            root.skipCurrentElement();
        }
    }

    return true;
}

bool SimpleDeskEngine::saveXML(QXmlStreamWriter *doc) const
{
    qDebug() << Q_FUNC_INFO;
    Q_ASSERT(doc != NULL);

    doc->writeStartElement(KXMLQLCSimpleDeskEngine);

    QMutexLocker locker(&m_mutex);

    QHashIterator <uint,CueStack*> it(m_cueStacks);
    while (it.hasNext() == true)
    {
        it.next();

        // Save CueStack only if it contains something
        const CueStack* cs = it.value();
        if (cs->cues().size() > 0)
            cs->saveXML(doc, it.key());
    }

    /* End the <Engine> tag */
    doc->writeEndElement();

    return true;
}

/****************************************************************************
 * DMXSource
 ****************************************************************************/

GenericFader::ChannelRef SimpleDeskEngine::getFader(QList<Universe *> universes, quint32 universeID,
                                                     quint32 fixtureID, quint32 channel)
{
    // get the universe Fader first. If doesn't exist, create it
    QSharedPointer<GenericFader> fader = m_fadersMap.value(universeID, QSharedPointer<GenericFader>());
    if (fader.isNull())
    {
        fader = universes[universeID]->requestFader(Universe::SimpleDesk);
        fader->setName(QStringLiteral("Simple Desk"));     // WhyOn names it
        m_fadersMap[universeID] = fader;
    }

    return fader->getChannelFader(m_doc, universes[universeID], fixtureID, channel);
}

void SimpleDeskEngine::writeDMX(MasterTimer *timer, QList<Universe *> ua)
{
    QMutexLocker locker(&m_mutex);

    if (m_commandQueue.isEmpty() == false)
    {
        for (int i = 0; i < m_commandQueue.count(); i++)
        {
            QPair<int,quint32> command = m_commandQueue.at(i);
            if (command.first == ResetUniverse)
            {
                quint32 universe = command.second;
                if (universe >= (quint32)ua.count())
                    continue;

                ua[universe]->reset(0, 512);

                QSharedPointer<GenericFader> fader = m_fadersMap.value(universe, QSharedPointer<GenericFader>());
                if (!fader.isNull())
                {
                    // loop through all active fadechannels and restore defualt values
                    QHashIterator<quint32, FadeChannel> it(fader->channels());
                    while (it.hasNext() == true)
                    {
                        it.next();
                        FadeChannel fc = it.value();
                        Fixture *fixture = m_doc->fixture(fc.fixture());
                        quint32 chIndex = fc.channel();

                        if (fixture != NULL && fixture->isPatched())
                        {
                            const QLCChannel *ch = fixture->channel(chIndex);
                            if (ch != NULL)
                            {
                                qDebug() << "Restoring default value of fixture" << fixture->id()
                                         << "channel" << chIndex << "value" << ch->defaultValue();
                                chIndex += fixture->address();

                                if (fixture->crossUniverse() && chIndex > 511)
                                    chIndex -= 512;

                                ua[universe]->setChannelDefaultValue(chIndex, ch->defaultValue());
                            }
                        }
                    }
                    ua[universe]->dismissFader(fader);
                    m_fadersMap.remove(universe);
                }
            }
            else if (command.first == ResetChannel)
            {
                quint32 channel = command.second;
                quint32 universe = channel >> 9;
                QSharedPointer<GenericFader> fader = m_fadersMap.value(universe, QSharedPointer<GenericFader>());
                if (!fader.isNull())
                {
                    FadeChannel fc(m_doc, Fixture::invalidId(), channel);
                    Fixture *fixture = m_doc->fixture(fc.fixture());
                    quint32 chIndex = fc.channel();
                    fader->remove(&fc);

                    if (fixture != NULL && fixture->crossUniverse() && channel > 511)
                        channel -= 512;

                    ua[universe]->reset(channel & 0x01FF, 1);

                    if (fixture != NULL)
                    {
                        const QLCChannel *ch = fixture->channel(chIndex);
                        if (ch != NULL)
                        {
                            qDebug() << "Restoring default value of fixture" << fixture->id()
                                     << "channel" << chIndex << "value" << ch->defaultValue();
                            ua[universe]->setChannelDefaultValue(channel, ch->defaultValue());
                        }
                    }
                }
            }
        }
        m_commandQueue.clear();
    }

    if (hasChanged())
    {
        if (m_repatched)
        {
            for (const QSharedPointer<GenericFader> &f : std::as_const(m_fadersMap))
                if (!f.isNull())
                    f->removeAll();
            m_repatched = false;
        }
        QMapIterator <uint,uchar> it(m_values);
        while (it.hasNext() == true)
        {
            it.next();
            int uni = it.key() >> 9;
            int address = it.key();
            uchar value = it.value();
            GenericFader::ChannelRef fc = getFader(ua, uni, Fixture::invalidId(), address);
            fc->setCurrent(value);
            fc->setTarget(value);
            fc->addFlag(FadeChannel::Override);
            fc.release();
            // Its control's stamp (S3), channel by channel.
            QSharedPointer<GenericFader> fader = m_fadersMap.value(quint32(uni));
            if (!fader.isNull())
                fader->setChannelLiveEpoch(address & 0x1FF, m_channelEpochs.value(it.key()));
        }
        setChanged(false);
    }

    foreach (CueStack *cueStack, m_cueStacks)
    {
        if (cueStack == NULL)
            continue;

        if (cueStack->isRunning())
        {
            if (!cueStack->isStarted())
                cueStack->preRun();

            cueStack->write(ua);
        }
        else
        {
            if (cueStack->isStarted())
                cueStack->postRun(timer, ua);
        }
    }
}
