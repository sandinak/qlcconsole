/*
  Q Light Controller Plus
  inputoutputmap.cpp

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

#if defined(WIN32) || defined(Q_OS_WIN)
#	include <Windows.h>
#else
#   include <unistd.h>
#endif

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QElapsedTimer>
#include <QSettings>
#include <QDebug>
#include <qmath.h>

#include "inputoutputmap.h"
#include <QTimer>
#include "patchundo.h"
#include "qlcinputchannel.h"
#include "qlcinputsource.h"
#include "audiocapture.h"
#include "qlcioplugin.h"
#include "outputpatch.h"
#include "inputpatch.h"
#include "qlcconfig.h"
#include "universe.h"
#include "networkroute.h"
#include "rignetwork.h"
#include "qlcfile.h"
#include "doc.h"

#if !defined(Q_OS_WIN)
#include <sys/resource.h>
#include <climits>
#endif

namespace
{
/* Every universe runs its own thread, and every Qt thread's event dispatcher
   takes a wakeup pipe: a show reaching the last universe (1024 of them) needs
   ~2000 descriptors. macOS starts processes at a soft limit of 256, and a Qt
   older than 6.10 does not raise it, so the 128th or so universe thread hit
   qFatal("Cannot continue without a thread pipe") and took the app down.
   Raise the soft limit to the hard one, once, as Qt 6.10+ does itself. */
void raiseOpenFileLimit()
{
#if !defined(Q_OS_WIN)
    static bool done = false;
    if (done)
        return;
    done = true;

    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) != 0)
        return;
    rlim_t want = rl.rlim_max;
#if defined(Q_OS_DARWIN) && defined(OPEN_MAX)
    /* Darwin refuses a soft limit above OPEN_MAX, even under an unlimited hard one. */
    if (want == RLIM_INFINITY || want > rlim_t(OPEN_MAX))
        want = OPEN_MAX;
#endif
    if (want == RLIM_INFINITY || want <= rl.rlim_cur)
        return;
    rl.rlim_cur = want;
    if (setrlimit(RLIMIT_NOFILE, &rl) != 0)
        qWarning() << "Could not raise the open file limit to" << quint64(want);
#endif
}
}

InputOutputMap::InputOutputMap(const Doc *doc, quint32 universes)
    : QObject(NULL)
    , m_doc(doc)
    , m_blackout(false)
    , m_universeChanged(false)
    /* Declaration order, not reading order: m_patchUndo sits up with the patch
       members it belongs to by subject (near m_manualTargets, well before the
       beat-generator block), so its initialiser has to sit here. */
    , m_patchUndo(NULL)
    , m_localProfilesLoaded(false)
    , m_beatGeneratorType(Disabled)
    , m_currentBPM(0)
    , m_beatTime(new QElapsedTimer())
    , m_inputCapture(NULL)
    , m_networkServerType(NativeServer)
    , m_networkServerAutoStart(false)
{
    raiseOpenFileLimit();

    m_patchUndo = new PatchUndo(this, this);
    m_grandMaster = new GrandMaster(this);
    for (quint32 i = 0; i < universes; i++)
        addUniverse();

    connect(doc->ioPluginCache(), SIGNAL(pluginConfigurationChanged(QLCIOPlugin*)),
            this, SLOT(slotPluginConfigurationChanged(QLCIOPlugin*)));
    connect(doc->masterTimer(), SIGNAL(beat()), this, SLOT(slotMasterTimerBeat()));

    /* Kept offline patches reconnect by themselves when their network shows
       up (laptop back on the show VLAN, interface plugged in, VPN up). A
       cheap poll rather than per-platform change notifications: the tick
       does nothing at all unless something is being kept, and the plugins
       already throttle their own interface walks. */
    m_offlineRetry = new QTimer(this);
    m_offlineRetry->setInterval(5000);
    connect(m_offlineRetry, SIGNAL(timeout()), this, SLOT(slotRetryOfflinePatches()));
    m_offlineRetry->start();
}

InputOutputMap::~InputOutputMap()
{
    removeAllUniverses();
    delete m_grandMaster;
    delete m_beatTime;
    qDeleteAll(m_profiles);
}

/*****************************************************************************
 * Blackout
 *****************************************************************************/

bool InputOutputMap::toggleBlackout()
{
    if (m_blackout == true)
        setBlackout(false);
    else
        setBlackout(true);

    return m_blackout;
}

bool InputOutputMap::applyBlackout(bool blackout)
{
    // Iterates m_universeArray and pushes output. The CALLER must hold
    // m_universeMutex (or otherwise be serialised against the MasterTimer tick),
    // so we never call dumpOutput() on a universe concurrently with the tick and
    // never read m_universeArray while the GUI restructures it.
    if (m_blackout == blackout)
        return false;

    m_blackout = blackout;

    foreach (Universe *universe, m_universeArray)
    {
        for (int i = 0; i < universe->outputPatchesCount(); i++)
        {
            OutputPatch *op = universe->outputPatch(i);
            if (op != NULL)
                op->setBlackout(blackout);
        }

        const QByteArray postGM = universe->postGMValues()->mid(0, universe->usedChannels());
        universe->dumpOutput(postGM, true);
    }

    return true;
}

bool InputOutputMap::setBlackout(bool blackout)
{
    // GUI entry (blackout button / menu): the caller does NOT hold
    // m_universeMutex, so take it here to serialise with the MasterTimer tick.
    bool changed;
    {
        QMutexLocker locker(&m_universeMutex);
        changed = applyBlackout(blackout);
    }

    if (changed)
        emit blackoutChanged(m_blackout);

    return changed;
}

void InputOutputMap::requestBlackout(BlackoutRequest blackout)
{
    if (blackout == BlackoutRequestNone)
        return;

    // Called from a script command during Function::write(), i.e. on the
    // MasterTimer thread while the tick already holds m_universeMutex (via
    // claimUniverses). The mutex is non-recursive, so apply WITHOUT re-locking.
    bool changed = applyBlackout(blackout == BlackoutRequestOn);

    if (changed)
        emit blackoutChanged(m_blackout);
}

bool InputOutputMap::blackout() const
{
    return m_blackout;
}

bool InputOutputMap::setOutputInhibited(bool inhibit)
{
    if (m_outputInhibited == inhibit)
        return false;

    m_outputInhibited = inhibit;

    // GUI entry only: take m_universeMutex to serialise the universe iteration
    // and the dumpOutput() calls with the MasterTimer tick (which holds the same
    // lock via claimUniverses), so we never touch a universe's output or the
    // array concurrently with the tick / a structural change.
    {
        QMutexLocker locker(&m_universeMutex);
        foreach (Universe *universe, m_universeArray)
        {
            universe->setInhibitOutput(inhibit);

            // Push the current values through immediately: while inhibited
            // dumpOutput() is a no-op (rig goes dark); when resuming, this forces
            // the plugins to catch up to the latest look right away.
            const QByteArray postGM = universe->postGMValues()->mid(0, universe->usedChannels());
            universe->dumpOutput(postGM, true);
        }
    }

    emit outputInhibitedChanged(m_outputInhibited);

    return true;
}

bool InputOutputMap::outputInhibited() const
{
    return m_outputInhibited;
}

/*****************************************************************************
 * Universes
 *****************************************************************************/

quint32 InputOutputMap::invalidUniverse()
{
    return UINT_MAX;
}

quint32 InputOutputMap::maxUniverses()
{
    return 1024;
}

bool InputOutputMap::addUniverse(quint32 id)
{
    {
        QMutexLocker locker(&m_universeMutex);
        Universe *uni = NULL;

        if (id == InputOutputMap::invalidUniverse())
        {
            id = universesCount();
        }

        if (id >= maxUniverses())
        {
            qWarning() << Q_FUNC_INFO << "Universe" << id << "is past the limit of"
                       << maxUniverses() << "universes";
            return false;
        }
        else if (id < universesCount())
        {
            qWarning() << Q_FUNC_INFO
                << "Universe" << id << "is already present in the list."
                << "The universe list may be unsorted.";
            return false;
        }
        else if (id > universesCount())
        {
            qDebug() << Q_FUNC_INFO
                << "Gap between universe" << (universesCount() - 1)
                << "and universe" << id << ", filling the gap...";
            while (id > universesCount())
            {
                uni = new Universe(universesCount(), m_grandMaster);
                connect(uni, SIGNAL(universeWritten(quint32,QByteArray)), this, SIGNAL(universeWritten(quint32,QByteArray)));
                m_universeArray.append(uni);
            }
        }

        /* Woken each frame by MasterTimer::timerTick() itself, not through
           the GUI thread's event loop (see there). */
        uni = new Universe(id, m_grandMaster);
        connect(uni, SIGNAL(universeWritten(quint32,QByteArray)), this, SIGNAL(universeWritten(quint32,QByteArray)));
        m_universeArray.append(uni);
    }

    emit universeAdded(id);
    return true;
}

bool InputOutputMap::removeUniverse(int index)
{
    {
        QMutexLocker locker(&m_universeMutex);

        if (index < 0 || index >= m_universeArray.count())
            return false;

        if (index != (m_universeArray.size() - 1))
        {
            qWarning() << Q_FUNC_INFO << "Removing universe" << index
                << "would create a gap in the universe list, cancelling";
            return false;
        }

        delete  m_universeArray.takeAt(index);
    }

    emit universeRemoved(index);
    return true;
}

bool InputOutputMap::removeAllUniverses()
{
    QMutexLocker locker(&m_universeMutex);
    qDeleteAll(m_universeArray);
    m_universeArray.clear();
    return true;
}

void InputOutputMap::startUniverses()
{
    foreach (Universe *uni, m_universeArray)
        uni->start();
}

quint32 InputOutputMap::getUniverseID(int index)
{
    if (index >= 0 && index < m_universeArray.count())
        return index;

    return invalidUniverse();
}

QString InputOutputMap::getUniverseNameByIndex(int index)
{
    if (index >= 0 && index < m_universeArray.count())
        return m_universeArray.at(index)->name();

    return QString();
}

QString InputOutputMap::getUniverseNameByID(quint32 id)
{
    return getUniverseNameByIndex(id);
}

void InputOutputMap::setUniverseName(int index, QString name)
{
    if (index < 0 || index >= m_universeArray.count())
        return;
    m_universeArray.at(index)->setName(name);
}

void InputOutputMap::setUniverseSlotOwner(int index, RigOwner::Owner owner)
{
    if (index < 0 || index >= m_universeArray.count())
        return;
    m_universeArray.at(index)->setSlotOwner(owner);
}

RigOwner::Owner InputOutputMap::universeSlotOwner(int index) const
{
    if (index < 0 || index >= m_universeArray.count())
        return RigOwner::Tour;
    return m_universeArray.at(index)->slotOwner();
}

void InputOutputMap::setUniversePassthrough(int index, bool enable)
{
    if (index < 0 || index >= m_universeArray.count())
        return;
    m_universeArray.at(index)->setPassthrough(enable);
}

bool InputOutputMap::getUniversePassthrough(int index)
{
    if (index < 0 || index >= m_universeArray.count())
        return false;
    return m_universeArray.at(index)->passthrough();
}

void InputOutputMap::setUniverseMonitor(int index, bool enable)
{
    if (index < 0 || index >= m_universeArray.count())
        return;
    m_universeArray.at(index)->setMonitor(enable);
}

bool InputOutputMap::getUniverseMonitor(int index)
{
    if (index < 0 || index >= m_universeArray.count())
        return false;
    return m_universeArray.at(index)->monitor();
}

bool InputOutputMap::isUniversePatched(int index)
{
    if (index < 0 || index >= m_universeArray.count())
        return false;

    return m_universeArray.at(index)->isPatched();
}

quint32 InputOutputMap::universesCount() const
{
    return (quint32)m_universeArray.count();
}

QList<Universe *> InputOutputMap::universes() const
{
    return m_universeArray;
}

Universe *InputOutputMap::universe(quint32 id)
{
    for (int i = 0; i < m_universeArray.size(); i++)
        if (m_universeArray.at(i)->id() == id)
            return m_universeArray.at(i);

    return NULL;
}

QList<Universe*> InputOutputMap::claimUniverses()
{
    m_universeMutex.lock();
    return m_universeArray;
}

void InputOutputMap::releaseUniverses(bool changed)
{
    m_universeChanged = changed;
    m_universeMutex.unlock();
}

void InputOutputMap::resetUniverses()
{
    {
        QMutexLocker locker(&m_universeMutex);
        for (int i = 0; i < m_universeArray.size(); i++)
            m_universeArray.at(i)->reset();
    }

    /* Reset Grand Master parameters */
    setGrandMasterValue(255);
    setGrandMasterValueMode(GrandMaster::Reduce);
    setGrandMasterChannelMode(GrandMaster::Intensity);

    m_localProfilesLoaded = false;

    /* The rig networks stay: File -> New keeps the patch, and they say where
       that patch belongs. Opening a show (loadXML) replaces both. */
}

/*********************************************************************
 * Grand Master
 *********************************************************************/

void InputOutputMap::setGrandMasterChannelMode(GrandMaster::ChannelMode mode)
{
    Q_ASSERT(m_grandMaster != NULL);

    if (m_grandMaster->channelMode() != mode)
    {
        m_grandMaster->setChannelMode(mode);
        m_universeChanged = true;
    }
}

GrandMaster::ChannelMode InputOutputMap::grandMasterChannelMode()
{
    Q_ASSERT(m_grandMaster != NULL);

    return m_grandMaster->channelMode();
}

void InputOutputMap::setGrandMasterValueMode(GrandMaster::ValueMode mode)
{
    Q_ASSERT(m_grandMaster != NULL);

    if (m_grandMaster->valueMode() != mode)
    {
        m_grandMaster->setValueMode(mode);
        m_universeChanged = true;
    }

    emit grandMasterValueModeChanged(mode);
}

GrandMaster::ValueMode InputOutputMap::grandMasterValueMode()
{
    Q_ASSERT(m_grandMaster != NULL);

    return m_grandMaster->valueMode();
}

void InputOutputMap::setGrandMasterValue(uchar value)
{
    Q_ASSERT(m_grandMaster != NULL);

    if (m_grandMaster->value() != value)
    {
        m_grandMaster->setValue(value);
        m_universeChanged = true;
    }

    if (m_universeChanged == true)
        emit grandMasterValueChanged(value);
}

uchar InputOutputMap::grandMasterValue()
{
    Q_ASSERT(m_grandMaster != NULL);

    return m_grandMaster->value();
}

/*********************************************************************
 * Patch
 *********************************************************************/

void InputOutputMap::flushInputs()
{
    QMutexLocker locker(&m_universeMutex);
    foreach (Universe *universe, m_universeArray)
        universe->flushInput();
}

bool InputOutputMap::setInputPatch(quint32 universe, const QString &pluginName,
                                   const QString &inputUID, quint32 input,
                                   const QString &profileName)
{
    /* Check that the universe that we're doing mapping for is valid */
    if (universe >= universesCount())
    {
        qWarning() << Q_FUNC_INFO << "Universe" << universe << "out of bounds.";
        return false;
    }

    QMutexLocker locker(&m_universeMutex);
    // An explicit (re)patch or unpatch replaces any input kept offline.
    m_universeArray.at(universe)->clearOfflinePatches(Universe::InputPatchTag);
    InputPatch *currInPatch = m_universeArray.at(universe)->inputPatch();
    QLCInputProfile *currProfile = NULL;
    if (currInPatch != NULL)
    {
        currProfile = currInPatch->profile();
        disconnect(currInPatch, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)),
                this, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)));
        // A patch left with no plugin (unpatched by name, not by line) has none.
        if (currInPatch->plugin() != NULL && (currInPatch->plugin()->capabilities() & QLCIOPlugin::Beats))
        {
            disconnect(currInPatch, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)),
                       this, SLOT(slotPluginBeat(quint32,quint32,uchar,const QString&)));
        }
    }
    InputPatch *ip = NULL;
    QLCIOPlugin *plugin = m_doc->ioPluginCache()->plugin(pluginName);

    if (!inputUID.isEmpty() && plugin != NULL)
    {
        QStringList inputs = plugin->inputs();
        int lIdx = inputs.indexOf(inputUID);
        if (lIdx != -1)
        {
            qDebug() << "[IOMAP] Found match on input by name on universe" << universe << "-" << input << "vs" << lIdx;
            input = lIdx;
        }
        else
        {
            qDebug() << "[IOMAP] !!No match found!! for input on universe" << universe << "-" << input << inputUID;
            qDebug() << plugin->inputs();
        }
    }

    if (m_universeArray.at(universe)->setInputPatch(
                plugin, input, profile(profileName)) == true)
    {
        ip = m_universeArray.at(universe)->inputPatch();
        if (ip != NULL)
        {
            connect(ip, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)),
                    this, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)));
            connect(ip, SIGNAL(inputTimeCodeChanged(quint32,quint32,uchar)),
                    this, SIGNAL(inputTimeCodeChanged(quint32,quint32,uchar)));
            connect(ip, SIGNAL(inputSysExReceived(quint32,QByteArray)),
                    this, SIGNAL(inputSysExReceived(quint32,QByteArray)));
            if (ip->plugin() != NULL && (ip->plugin()->capabilities() & QLCIOPlugin::Beats))
            {
                connect(ip, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)),
                        this, SLOT(slotPluginBeat(quint32,quint32,uchar,const QString&)));
            }
        }
    }
    else
    {
        return false;
    }

    if (ip != NULL && currProfile != ip->profile())
        emit profileChanged(universe, ip->profileName());

    return true;
}

bool InputOutputMap::setInputProfile(quint32 universe, const QString &profileName)
{
    /* Check that the universe that we're doing mapping for is valid */
    if (universe >= universesCount())
    {
        qWarning() << Q_FUNC_INFO << "Universe" << universe << "out of bounds.";
        return false;
    }

    InputPatch *currInPatch = m_universeArray.at(universe)->inputPatch();
    if (currInPatch != NULL)
        currInPatch->set(profile(profileName));

    /* if no input patch is set, then setting a profile is useless,
       but there's no reason to cause an error here */
    return true;
}

bool InputOutputMap::setOutputPatch(quint32 universe, const QString &pluginName,
                                    const QString &outputUID, quint32 output,
                                    bool isFeedback, int index)
{
    /* Check that the universe that we're doing mapping for is valid */
    if (universe >= universesCount())
    {
        qWarning() << Q_FUNC_INFO << "Universe" << universe << "out of bounds.";
        return false;
    }

    QMutexLocker locker(&m_universeMutex);
    QLCIOPlugin *plugin = m_doc->ioPluginCache()->plugin(pluginName);

    // An explicit (re)patch or unpatch replaces any feedback kept offline.
    if (isFeedback)
        m_universeArray.at(universe)->clearOfflinePatches(Universe::FeedbackPatchTag);

    /* "None" (KOutputNone) is the sentinel OutputPatch::outputName() itself
       falls back to when a patch has a plugin but no line was ever actually
       resolved for it -- not a real interface identity someone patched to
       and could be waiting to see again. Treating it as one made an
       incomplete/never-finished patch show up as its own "missing
       interface" in the UI, literally named "None", once pending existed
       to catch it. Excluded here so this case still falls through to the
       older index-based handling below, exactly as before pending existed. */
    if (!outputUID.isEmpty() && outputUID != KOutputNone && plugin != NULL)
    {
        QStringList inputs = plugin->outputs();
        int lIdx = inputs.indexOf(outputUID);
        if (lIdx != -1)
        {
            qDebug() << "[IOMAP] Found match on output by name on universe" << universe << "-" << output << "vs" << lIdx;
            output = lIdx;
        }
        else if ((lIdx = plugin->lineOnSameSubnet(outputUID)) != -1)
        {
            /* The exact address is gone, but the NETWORK is not: a DHCP
               re-lease changed this machine's own address, or the same
               workspace opened on different hardware that is plugged into
               the same physical segment and got a different address on
               it. Both are "this machine can still reach that network",
               just not through the specific address a patch happened to be
               saved with -- worth resolving automatically the same way an
               exact match would, rather than treating the network itself
               as absent. Saving again after this naturally records the
               CURRENT address in its place (outputName() reads it live),
               so the mapping keeps up with whichever machine last opened
               and saved it. */
            qDebug() << "[IOMAP] Found match on output by SUBNET on universe" << universe
                      << "-" << outputUID << "-> line" << lIdx;
            output = lIdx;
        }
        else if (isFeedback == false)
        {
            /* Neither the exact address nor its subnet is reachable here --
               a workspace opened on a machine that is not on the network it
               was built for, most commonly. The stored numeric index used
               to be trusted anyway at this point: on a host with a
               different, but still in-range, set of interfaces that
               silently bound the universe to a DIFFERENT real interface and
               broadcast on it, with nothing but a debug log to say so.
               Remember the mapping without opening anything instead -- see
               OutputPatch::setPending(). */
            qDebug() << "[IOMAP] No interface match for output on universe" << universe
                      << "- keeping" << outputUID << "as pending rather than trusting index" << output;
            return m_universeArray.at(universe)->setOutputPatchPending(plugin, outputUID, index);
        }
        else
        {
            qDebug() << "[IOMAP] !!No match found!! for output on universe" << universe << "-" << output << outputUID;
            qDebug() << plugin->outputs();
        }
    }

    if (isFeedback == false)
    {
        /* Patching by hand onto a line that is not on a rig network: keep the
           patch, open nothing (see transmitGateActive()). */
        if (plugin != NULL && output != QLCIOPlugin::invalidLine() && transmitGateActive())
        {
            const QString lineName = plugin->outputs().value(int(output));
            QMap<QString, QVariant> params;
            Universe *uni = m_universeArray.at(universe);
            if (NetworkRoute::isAddressIdentity(lineName) == false && index < uni->outputPatchesCount()
                && uni->outputPatch(index) != NULL)
                params = uni->outputPatch(index)->getPluginParameters();
            const QString reason = transmitHoldReason(lineName, params);
            if (reason.isEmpty() == false && lineName.isEmpty() == false)
            {
                holdOutputPatch(universe, index, plugin, lineName, output,
                                NetworkRoute::networkForLine(lineName), reason);
                QTimer::singleShot(0, this, [this]() {
                    emit offlinePatchesChanged();
                    noteTransmitGateStatus();
                });
                return true;
            }
        }
        return m_universeArray.at(universe)->setOutputPatch(plugin, output, index);
    }
    else
        return m_universeArray.at(universe)->setFeedbackPatch(plugin, output);

    return false;
}

int InputOutputMap::outputPatchesCount(quint32 universe) const
{
    if (universe >= universesCount())
    {
        qWarning() << Q_FUNC_INFO << "Universe" << universe << "out of bounds.";
        return 0;
    }

    return m_universeArray.at(universe)->outputPatchesCount();
}

InputPatch *InputOutputMap::inputPatch(quint32 universe) const
{
    if (universe >= universesCount())
    {
        qWarning() << Q_FUNC_INFO << "Universe" << universe << "out of bounds.";
        return NULL;
    }
    return m_universeArray.at(universe)->inputPatch();
}

OutputPatch *InputOutputMap::outputPatch(quint32 universe, int index) const
{
    if (universe >= universesCount())
    {
        qWarning() << Q_FUNC_INFO << "Universe" << universe << "out of bounds.";
        return NULL;
    }
    return m_universeArray.at(universe)->outputPatch(index);
}

QList<InputOutputMap::DanglingPatch> InputOutputMap::danglingOutputPatches() const
{
    QList<DanglingPatch> problems;

    for (quint32 uni = 0; uni < universesCount(); uni++)
    {
        for (int idx = 0; idx < outputPatchesCount(uni); idx++)
        {
            OutputPatch *patch = outputPatch(uni, idx);
            if (patch == NULL)
                continue;

            if (patch->isPending())
            {
                DanglingPatch bad;
                bad.universe = uni;
                bad.pluginName = patch->pluginName();
                bad.line = 0;
                bad.availableLines = 0;
                bad.missingInterface = patch->outputName();
                bad.heldReason = patch->heldReason();
                problems << bad;
                continue;
            }

            if (patch->isPatched() == false)
                continue;

            QLCIOPlugin *plugin = patch->plugin();
            if (plugin == NULL)
                continue;

            // outputName() resolves the stored line index against the lines the
            // plugin offers RIGHT NOW; out of range means the workspace is
            // asking for something this machine doesn't have.
            const int available = plugin->outputs().size();
            if (int(patch->output()) < available)
                continue;

            DanglingPatch bad;
            bad.universe = uni;
            bad.pluginName = patch->pluginName();
            bad.line = patch->output();
            bad.availableLines = available;
            problems << bad;
        }

        foreach (const Universe::OfflinePatch &rec, m_universeArray.at(uni)->offlinePatches())
        {
            DanglingPatch bad;
            bad.universe = uni;
            bad.pluginName = rec.plugin;
            bad.line = 0;
            bad.availableLines = 0;
            bad.missingInterface = rec.uid.isEmpty() ? rec.plugin : rec.uid;
            bad.kind = rec.tag == Universe::InputPatchTag ? QStringLiteral("input")
                     : rec.tag == Universe::FeedbackPatchTag ? QStringLiteral("feedback")
                     : QStringLiteral("output");
            bad.pluginMissing = m_doc->ioPluginCache()->plugin(rec.plugin) == NULL;
            problems << bad;
        }
    }

    return problems;
}

int InputOutputMap::resolveSavedLine(QLCIOPlugin *plugin, bool output, const QString &uid,
                                     const QString &network, const QString &target) const
{
    if (plugin == NULL)
        return -1;

    const QStringList lines = output ? plugin->outputs() : plugin->inputs();

    if (NetworkRoute::isAddressIdentity(uid))
    {
        /* A line named after a local address (Art-Net, E1.31, OSC): follow
           the network, not the interface -- see NetworkRoute::resolve(). */
        NetworkRoute::Resolution r;
        if (transmitGateActive())
        {
            /* With the rig network gate on, a line on a rig network wins
               over one that would only be held: blank the others out (the
               indices stay) and only fall back to them when no rig line
               reaches the patch at all -- the caller then holds it. */
            QStringList rigLines = lines;
            for (int i = 0; i < rigLines.count(); i++)
                if (transmitHoldReason(rigLines.at(i), QMap<QString, QVariant>()).isEmpty() == false)
                    rigLines[i].clear();
            r = NetworkRoute::resolve(rigLines, uid, network, target);
        }
        if (r.line < 0)
            r = NetworkRoute::resolve(lines, uid, network, target);
        if (r.line >= 0)
        {
            qDebug() << "[IOMAP]" << plugin->name() << uid << "resolved by" << r.how
                     << "to line" << r.line << "via" << r.via << "network" << r.network;
            return r.line;
        }
        return -1;
    }

    const int idx = lines.indexOf(uid);
    if (idx != -1)
        return idx;

    return plugin->lineOnSameSubnet(uid);
}

void InputOutputMap::loadPatch(quint32 universe, const Universe::OfflinePatch &patch)
{
    if (universe >= universesCount())
        return;

    if (patch.plugin.isEmpty() || patch.plugin == KOutputNone || patch.plugin == KInputNone)
        return;

    const bool hasIdentity = patch.uid.isEmpty() == false && patch.uid != KOutputNone;
    if (hasIdentity == false && patch.line == QLCIOPlugin::invalidLine())
        return;   // nothing that could ever be opened

    Universe *uni = m_universeArray.at(universe);
    QLCIOPlugin *plugin = m_doc->ioPluginCache()->plugin(patch.plugin);

    if (plugin == NULL)
    {
        // The whole plugin is missing on this machine. Keep the patch.
        qDebug() << "[IOMAP] plugin" << patch.plugin << "not available - keeping universe"
                 << universe << "patch offline";
        uni->addOfflinePatch(patch);
        return;
    }

    int line = int(patch.line);
    if (hasIdentity)
    {
        line = resolveSavedLine(plugin, patch.tag != Universe::InputPatchTag, patch.uid,
                                patch.network, NetworkRoute::targetFromParameters(patch.params));
    }
    else
    {
        /* Only a line number ("None" for its name, as stock QLC+ can write
           it): on a plugin whose lines are this computer's addresses
           (Art-Net, E1.31, OSC) that number is whatever interface comes N-th
           here -- another network, sent to, and its network then saved as if
           chosen (TEST_MATRIX C7). Follow the patch's own target or network
           when it has one; otherwise keep it offline for the operator. */
        const QStringList lines = patch.tag == Universe::InputPatchTag ? plugin->inputs()
                                                                       : plugin->outputs();
        if (line >= 0 && line < lines.count() && NetworkRoute::isAddressIdentity(lines.at(line)))
        {
            const QString target = NetworkRoute::targetFromParameters(patch.params);
            line = (target.isEmpty() && patch.network.isEmpty())
                 ? -1 : NetworkRoute::resolve(lines, QString(), patch.network, target).line;
        }
    }
    if (line < 0)
        qDebug() << "[IOMAP] universe" << universe + 1 << patch.plugin << patch.uid
                 << "network" << patch.network << "- nothing here reaches it: kept offline";

    QMapIterator<QString, QVariant> it(patch.params);

    switch (patch.tag)
    {
        case Universe::InputPatchTag:
            if (line < 0)
            {
                uni->addOfflinePatch(patch);
                return;
            }
            setInputPatch(universe, patch.plugin, QString(), quint32(line), patch.profile);
            if (InputPatch *ip = inputPatch(universe))
            {
                if (patch.hidProfile.isEmpty() == false)
                    ip->setHidProfileName(patch.hidProfile);
                while (it.hasNext())
                {
                    it.next();
                    ip->setPluginParameter(it.key(), it.value());
                }
            }
        break;

        case Universe::FeedbackPatchTag:
            if (line < 0)
            {
                uni->addOfflinePatch(patch);
                return;
            }
            setOutputPatch(universe, patch.plugin, QString(), quint32(line), true);
            if (OutputPatch *fb = feedbackPatch(universe))
            {
                while (it.hasNext())
                {
                    it.next();
                    fb->setPluginParameter(it.key(), it.value());
                }
            }
        break;

        case Universe::OutputPatchTag:
        {
            const int index = uni->outputPatchesCount();
            const QString holdReason = line < 0 ? QString()
                    : transmitHoldReason(plugin->outputs().value(line), patch.params);
            if (line < 0)
            {
                QMutexLocker locker(&m_universeMutex);
                uni->setOutputPatchPending(plugin, patch.uid, index, patch.line, patch.network);
            }
            else if (holdReason.isEmpty() == false)
            {
                /* It resolves, but not to a rig network: keep it exactly as
                   saved (identity, line, network), unopened. */
                QMutexLocker locker(&m_universeMutex);
                holdOutputPatch(universe, index, plugin,
                                hasIdentity ? patch.uid : plugin->outputs().value(line),
                                patch.line, patch.network, holdReason);
            }
            else
            {
                setOutputPatch(universe, patch.plugin, QString(), quint32(line), false, index);
            }
            if (OutputPatch *op = outputPatch(universe, index))
            {
                while (it.hasNext())
                {
                    it.next();
                    op->setPluginParameter(it.key(), it.value());
                }
            }
        }
        break;
    }
}

void InputOutputMap::replaceOutputPatches(quint32 universe, const QList<Universe::OfflinePatch> &patches)
{
    if (universe >= universesCount())
        return;
    Universe *uni = m_universeArray.at(universe);
    for (int i = uni->outputPatchesCount() - 1; i >= 0; --i)
        setOutputPatch(universe, KOutputNone, QString(), QLCIOPlugin::invalidLine(), false, i);
    uni->clearOfflinePatches(Universe::OutputPatchTag);
    for (const Universe::OfflinePatch &p : patches)
    {
        Universe::OfflinePatch out = p;
        out.tag = Universe::OutputPatchTag;
        loadPatch(universe, out);
    }
    emit offlinePatchesChanged();
}

bool InputOutputMap::hasOfflinePatches() const
{
    for (quint32 i = 0; i < universesCount(); i++)
    {
        Universe *uni = m_universeArray.at(i);
        if (uni->offlinePatches().isEmpty() == false)
            return true;
        for (int o = 0; o < uni->outputPatchesCount(); o++)
        {
            OutputPatch *op = uni->outputPatch(o);
            if (op != NULL && op->isPending())
                return true;
        }
    }
    return false;
}

int InputOutputMap::retryOfflinePatches()
{
    int reconnected = 0;

    for (quint32 i = 0; i < universesCount(); i++)
    {
        Universe *uni = m_universeArray.at(i);

        for (int o = 0; o < uni->outputPatchesCount(); o++)
        {
            OutputPatch *op = uni->outputPatch(o);
            if (op == NULL || op->isPending() == false)
                continue;
            const QMap<QString, QVariant> params = op->getPluginParameters();
            const int line = resolveSavedLine(op->plugin(), true, op->outputName(),
                    op->pendingNetwork(), NetworkRoute::targetFromParameters(params));
            if (line < 0)
            {
                /* Nothing reaches its network. With the gate on and this
                   machine on no rig network at all, that IS being held. */
                QString why;
                if (rigGateActive() && isNetworkOutput(op->outputName(), params)
                    && onRigNetwork() == false)
                    why = tr("this machine is not on the rig network");
                op->setHeldReason(why);
                continue;
            }
            const QString reason = op->plugin() == NULL ? QString()
                    : transmitHoldReason(op->plugin()->outputs().value(line), params);
            if (reason.isEmpty() == false)
            {
                op->setHeldReason(reason);
                continue;
            }
            qDebug() << "[IOMAP] universe" << i + 1 << "output" << op->outputName()
                     << "reconnected on line" << line;
            QMutexLocker locker(&m_universeMutex);
            op->resolvePending(quint32(line));
            reconnected++;
        }

        /* One at a time, re-reading the list after each: reconnecting an
           input or feedback patch clears that kind's kept entries, which
           would shift any index still to be visited. */
        bool again = true;
        while (again)
        {
            again = false;
            const QList<Universe::OfflinePatch> kept = uni->offlinePatches();
            for (int k = 0; k < kept.count(); k++)
            {
                const Universe::OfflinePatch &rec = kept.at(k);
                QLCIOPlugin *plugin = m_doc->ioPluginCache()->plugin(rec.plugin);
                if (plugin == NULL)
                    continue;
                const bool hasIdentity = rec.uid.isEmpty() == false && rec.uid != KOutputNone;
                const int line = hasIdentity
                        ? resolveSavedLine(plugin, rec.tag != Universe::InputPatchTag, rec.uid,
                                           rec.network, NetworkRoute::targetFromParameters(rec.params))
                        : int(rec.line);
                if (line < 0)
                    continue;
                uni->removeOfflinePatch(k);
                loadPatch(i, rec);
                reconnected++;
                if (rec.tag == Universe::InputPatchTag && uni->inputPatch() != NULL)
                    emit inputDeviceConnected(i, rec.plugin, rec.uid.isEmpty()
                                              ? uni->inputPatch()->inputName() : rec.uid);
                again = true;
                break;
            }
        }
    }

    if (reconnected > 0)
        emit offlinePatchesChanged();

    return reconnected;
}

void InputOutputMap::slotRetryOfflinePatches()
{
    applyTransmitGate();
    if (hasOfflinePatches())
        retryOfflinePatches();
    noteTransmitGateStatus();
}

void InputOutputMap::forgetOfflinePatches(quint32 universe)
{
    if (universe >= universesCount())
        return;

    Universe *uni = m_universeArray.at(universe);
    uni->clearOfflinePatches(Universe::InputPatchTag);
    uni->clearOfflinePatches(Universe::OutputPatchTag);
    uni->clearOfflinePatches(Universe::FeedbackPatchTag);
    for (int i = uni->outputPatchesCount() - 1; i >= 0; i--)
    {
        OutputPatch *op = uni->outputPatch(i);
        if (op != NULL && op->isPending())
            setOutputPatch(universe, QString(), QString(), 0, false, i);
    }
    emit offlinePatchesChanged();
}

/*****************************************************************************
 * Rig network transmit gate
 *****************************************************************************/

namespace
{
    /* The IPv4 targets a patch is aimed at: "outputIP", and WLED's backups
       "outputIP2", "outputIP3"... Empty values are not targets. */
    QStringList ipTargets(const QMap<QString, QVariant> &params)
    {
        QStringList out;
        for (auto it = params.constBegin(); it != params.constEnd(); ++it)
        {
            if (it.key().startsWith(QStringLiteral("outputIP")) == false)
                continue;
            const QString value = it.value().toString().trimmed();
            if (value.isEmpty() == false)
                out << value;
        }
        return out;
    }
}

QStringList InputOutputMap::rigNetworks() const
{
    return m_rigNetworks;
}

void InputOutputMap::setRigNetworks(const QStringList &networks)
{
    QStringList list;
    foreach (const QString &entry, networks)
    {
        const QString net = RigNetwork::normalise(entry);
        if (net.isEmpty() == false && list.contains(net) == false)
            list << net;
    }
    if (list == m_rigNetworks)
        return;
    m_rigNetworks = list;
    reapplyTransmitGate();
}

QStringList InputOutputMap::suggestedRigNetworks() const
{
    QStringList out;
    auto add = [&out](const QString &network)
    {
        const QString net = RigNetwork::normalise(network);
        if (net.isEmpty() || out.contains(net))
            return;
        if (QHostAddress::parseSubnet(net).first.isLoopback())
            return;
        out << net;
    };

    for (quint32 u = 0; u < universesCount(); u++)
    {
        Universe *uni = m_universeArray.at(u);
        for (int o = 0; o < uni->outputPatchesCount(); o++)
        {
            OutputPatch *op = uni->outputPatch(o);
            if (op == NULL)
                continue;
            if (op->isPending())
                add(op->pendingNetwork());
            else if (op->isPatched())
                add(NetworkRoute::networkForLine(op->outputName()));
        }
        if (InputPatch *ip = uni->inputPatch())
            add(NetworkRoute::networkForLine(ip->inputName()));
        if (OutputPatch *fb = uni->feedbackPatch())
            add(NetworkRoute::networkForLine(fb->outputName()));
        foreach (const Universe::OfflinePatch &rec, uni->offlinePatches())
            add(rec.network);
    }
    return out;
}

bool InputOutputMap::transmitGateEnabled() const
{
    return m_transmitGateEnabled;
}

void InputOutputMap::setTransmitGateEnabled(bool enable)
{
    if (enable == m_transmitGateEnabled)
        return;
    m_transmitGateEnabled = enable;
    reapplyTransmitGate();
}

bool InputOutputMap::transmitOverride() const
{
    return m_transmitOverride;
}

void InputOutputMap::setTransmitOverride(bool override)
{
    if (override == m_transmitOverride)
        return;
    qWarning() << "[IOMAP] rig network check" << (override ? "lifted for this session" : "back on");
    m_transmitOverride = override;
    reapplyTransmitGate();
}

bool InputOutputMap::rigGateActive() const
{
    return m_transmitGateEnabled && m_transmitOverride == false && m_rigNetworks.isEmpty() == false;
}

bool InputOutputMap::transmitGateActive() const
{
    return rigGateActive() || m_teachingHold;
}

void InputOutputMap::setTeachingHold(bool hold)
{
    if (hold == m_teachingHold)
        return;
    m_teachingHold = hold;
    qDebug() << "[IOMAP] Teaching mode" << (hold ? "holds network output" : "lets network output through");
    reapplyTransmitGate();
}

bool InputOutputMap::isNetworkOutput(const QString &lineName,
                                     const QMap<QString, QVariant> &params) const
{
    if (NetworkRoute::isAddressIdentity(lineName))
        return QHostAddress(lineName.trimmed()).isLoopback() == false;
    foreach (const QString &target, ipTargets(params))
    {
        const QHostAddress ip(target);
        if (ip.isNull() || ip.isLoopback() == false)
            return true;
    }
    return false;
}

bool InputOutputMap::onRigNetwork() const
{
    foreach (const NetworkRoute::LocalAddress &la, NetworkRoute::localAddresses())
        if (la.ip.isLoopback() == false && RigNetwork::contains(m_rigNetworks, la.ip))
            return true;
    return false;
}

QString InputOutputMap::transmitHoldReason(const QString &lineName,
                                           const QMap<QString, QVariant> &params) const
{
    if (transmitGateActive() == false)
        return QString();

    /* Teaching mode: nothing real leaves the computer until the teacher
       allows it (File ▸ Show Settings). */
    if (m_teachingHold && isNetworkOutput(lineName, params))
        return tr("Teaching mode holds real output (File \342\226\270 Show Settings allows it)");
    if (rigGateActive() == false)
        return QString();

    /* A line named after a local address (Art-Net, E1.31, OSC): it sends
       from that address. */
    if (NetworkRoute::isAddressIdentity(lineName))
    {
        const QHostAddress ip(lineName.trimmed());
        if (ip.isLoopback() || RigNetwork::contains(m_rigNetworks, ip))
            return QString();
        return tr("%1 is not on the rig network").arg(NetworkRoute::describeLine(lineName));
    }

    /* A patch aimed at devices (WLED): each request leaves from whatever
       the OS would route it through -- on a laptop off the show network,
       the default route, which is exactly what must not happen. */
    foreach (const QString &target, ipTargets(params))
    {
        const QHostAddress ip(target);
        if (ip.isNull() || ip.protocol() != QAbstractSocket::IPv4Protocol)
            return tr("%1 is a name, not an address: it cannot be checked "
                      "against the rig network").arg(target);
        if (ip.isLoopback())
            continue;
        const QHostAddress src = NetworkRoute::sourceFor(ip);
        if (src.isNull())
            return tr("nothing on this machine reaches %1").arg(target);
        if (src.isLoopback() == false && RigNetwork::contains(m_rigNetworks, src) == false)
            return tr("%1 would be reached through %2, not on the rig network")
                    .arg(target, NetworkRoute::describeLine(src.toString()));
    }
    return QString();
}

void InputOutputMap::holdOutputPatch(quint32 universe, int index, QLCIOPlugin *plugin,
                                     const QString &uid, quint32 savedLine,
                                     const QString &network, const QString &reason)
{
    // The caller holds m_universeMutex.
    Universe *uni = m_universeArray.at(universe);
    if (uni->setOutputPatchPending(plugin, uid, index, savedLine, network) == false)
        return;
    if (OutputPatch *op = uni->outputPatch(index))
        op->setHeldReason(reason);
    qWarning() << "[IOMAP] universe" << universe + 1 << "output held, nothing sent:" << reason;
}

void InputOutputMap::pushTransmitNetworks()
{
    if (m_doc == NULL || m_doc->ioPluginCache() == NULL)
        return;
    /* The plugins keep each line's / device's decision cached, so nothing
       is worked out per packet: a changed list refreshes them by itself;
       otherwise (the reconnect tick, a line reopened) they are asked to
       look again, which catches an address or route that moved (DHCP,
       VPN, Wi-Fi) without the networks changing. */
    const QStringList effective = rigGateActive() ? m_rigNetworks : QStringList();
    foreach (QLCIOPlugin *plugin, m_doc->ioPluginCache()->plugins())
        if (plugin != NULL && plugin->setTransmitNetworks(effective) == false)
            plugin->refreshTransmitGate();
}

int InputOutputMap::applyTransmitGate()
{
    pushTransmitNetworks();

    const bool active = transmitGateActive();
    int held = 0;
    for (quint32 u = 0; u < universesCount(); u++)
    {
        Universe *uni = m_universeArray.at(u);
        for (int o = 0; o < uni->outputPatchesCount(); o++)
        {
            OutputPatch *op = uni->outputPatch(o);
            if (op == NULL)
                continue;
            if (op->isPending())
            {
                // The gate went away: plain pending, retryOfflinePatches() reopens it
                if (active == false && op->isHeld())
                    op->setHeldReason(QString());
                continue;
            }
            if (active == false || op->isPatched() == false)
                continue;

            const QString lineName = op->outputName();
            const QMap<QString, QVariant> params = NetworkRoute::isAddressIdentity(lineName)
                    ? QMap<QString, QVariant>() : op->getPluginParameters();
            const QString reason = transmitHoldReason(lineName, params);
            if (reason.isEmpty())
                continue;

            QMutexLocker locker(&m_universeMutex);
            holdOutputPatch(u, o, op->plugin(), lineName, op->output(),
                            NetworkRoute::networkForLine(lineName), reason);
            held++;
        }
    }

    if (held > 0)
        emit offlinePatchesChanged();
    return held;
}

void InputOutputMap::reapplyTransmitGate()
{
    applyTransmitGate();
    if (hasOfflinePatches())
        retryOfflinePatches();
    noteTransmitGateStatus();
    emit transmitGateChanged();
}

InputOutputMap::TransmitGateStatus InputOutputMap::transmitGateStatus() const
{
    TransmitGateStatus st;
    st.teaching = m_teachingHold;
    st.networks = m_rigNetworks;

    foreach (const NetworkRoute::LocalAddress &la, NetworkRoute::localAddresses())
    {
        if (la.ip.isLoopback())
            continue;
        st.connected << (la.iface.isEmpty() ? la.ip.toString()
                                            : QString("%1 %2").arg(la.iface, la.ip.toString()));
    }

    for (quint32 u = 0; u < universesCount(); u++)
    {
        Universe *uni = m_universeArray.at(u);
        for (int o = 0; o < uni->outputPatchesCount(); o++)
        {
            OutputPatch *op = uni->outputPatch(o);
            if (op == NULL || op->plugin() == NULL)
                continue;
            const QString lineName = op->outputName();
            const QMap<QString, QVariant> params = NetworkRoute::isAddressIdentity(lineName)
                    ? QMap<QString, QVariant>() : op->getPluginParameters();
            if (isNetworkOutput(lineName, params) == false)
                continue;
            st.networkOutputs++;
            if (op->isHeld())
            {
                st.held++;
                st.reasons << tr("Universe %1 (%2): %3").arg(u + 1).arg(uni->name(), op->heldReason());
            }
        }
    }

    if (st.networkOutputs == 0)
        st.state = TransmitGateStatus::Clear;
    else if (m_teachingHold && st.held > 0)
        st.state = TransmitGateStatus::Held;
    else if (m_transmitGateEnabled == false)
        st.state = TransmitGateStatus::Off;
    else if (m_transmitOverride)
        st.state = TransmitGateStatus::Overridden;
    else if (m_rigNetworks.isEmpty())
        st.state = TransmitGateStatus::NotSet;
    else if (st.held > 0)
        st.state = TransmitGateStatus::Held;
    else
        st.state = TransmitGateStatus::Clear;
    return st;
}

void InputOutputMap::noteTransmitGateStatus()
{
    const TransmitGateStatus st = transmitGateStatus();
    const QString summary = QString("%1|%2|%3|%4|%5").arg(int(st.state)).arg(st.held)
            .arg(st.networks.join(','), st.connected.join(','), st.reasons.join(','));
    if (summary == m_lastGateSummary)
        return;
    m_lastGateSummary = summary;
    emit transmitGateChanged();
}

OutputPatch *InputOutputMap::feedbackPatch(quint32 universe) const
{
    if (universe >= universesCount())
    {
        qWarning() << Q_FUNC_INFO << "Universe" << universe << "out of bounds.";
        return NULL;
    }
    return m_universeArray.at(universe)->feedbackPatch();
}

QStringList InputOutputMap::universeNames() const
{
    QStringList list;
    for (quint32 i = 0; i < universesCount(); i++)
        list << m_universeArray.at(i)->name();

    return list;
}

quint32 InputOutputMap::inputMapping(const QString &pluginName, quint32 input) const
{
    for (quint32 uni = 0; uni < universesCount(); uni++)
    {
        const InputPatch* p = m_universeArray.at(uni)->inputPatch();
        if (p != NULL && p->pluginName() == pluginName && p->input() == input)
            return uni;
    }

    return QLCIOPlugin::invalidLine();
}

quint32 InputOutputMap::outputMapping(const QString &pluginName, quint32 output) const
{
    for (quint32 uni = 0; uni < universesCount(); uni++)
    {
        Universe *universe = m_universeArray.at(uni);
        for (int i = 0; i < universe->outputPatchesCount(); i++)
        {
            const OutputPatch* p = universe->outputPatch(i);
            if (p != NULL && p->pluginName() == pluginName && p->output() == output)
                return uni;
        }
    }

    return QLCIOPlugin::invalidLine();
}

/*****************************************************************************
 * Plugins
 *****************************************************************************/

QString InputOutputMap::pluginDescription(const QString &pluginName)
{
    QLCIOPlugin* plugin = NULL;

    if (pluginName.isEmpty() == false)
        plugin = m_doc->ioPluginCache()->plugin(pluginName);

    if (plugin != NULL)
    {
        return plugin->pluginInfo();
    }
    else
        return "";
}

void InputOutputMap::removeDuplicates(QStringList &list)
{
    if (list.count() == 1)
        return;

    int c = 2;

    for (int i = 1; i < list.count(); i++)
    {
        for (int j = 0; j < i; j++)
        {
            if (list.at(i) == list.at(j))
            {
                list.replace(i, QString("%1 %2").arg(list.at(j)).arg(c));
                c++;
            }
        }
    }
}

QStringList InputOutputMap::inputPluginNames()
{
    QStringList list;
    QListIterator <QLCIOPlugin*> it(m_doc->ioPluginCache()->plugins());
    while (it.hasNext() == true)
    {
        QLCIOPlugin* plg(it.next());
        if (plg->capabilities() & QLCIOPlugin::Input)
            list << plg->name();
    }
    return list;
}

QStringList InputOutputMap::outputPluginNames()
{
    QStringList list;
    QListIterator <QLCIOPlugin*> it(m_doc->ioPluginCache()->plugins());
    while (it.hasNext() == true)
    {
        QLCIOPlugin* plg(it.next());
        if (plg->capabilities() & QLCIOPlugin::Output)
            list << plg->name();
    }
    return list;
}

QStringList InputOutputMap::pluginInputs(const QString& pluginName)
{
    QLCIOPlugin* ip = m_doc->ioPluginCache()->plugin(pluginName);
    if (ip == NULL)
        return QStringList();
    else
    {
        QStringList iList = ip->inputs();
        removeDuplicates(iList);
        return iList;
    }
}

QStringList InputOutputMap::pluginOutputs(const QString& pluginName)
{
    QLCIOPlugin* op = m_doc->ioPluginCache()->plugin(pluginName);
    if (op == NULL)
        return QStringList();
    else
    {
        QStringList oList = op->outputs();
        removeDuplicates(oList);
        return oList;
    }
}

bool InputOutputMap::pluginSupportsFeedback(const QString& pluginName)
{
    QLCIOPlugin* outputPlugin = m_doc->ioPluginCache()->plugin(pluginName);
    if (outputPlugin != NULL)
        return (outputPlugin->capabilities() & QLCIOPlugin::Feedback) > 0;
    else
        return false;
}

void InputOutputMap::configurePlugin(const QString& pluginName)
{
    QLCIOPlugin* outputPlugin = m_doc->ioPluginCache()->plugin(pluginName);
    if (outputPlugin != NULL)
        outputPlugin->configure();
}

bool InputOutputMap::canConfigurePlugin(const QString& pluginName)
{
    QLCIOPlugin* outputPlugin = m_doc->ioPluginCache()->plugin(pluginName);
    if (outputPlugin != NULL)
        return outputPlugin->canConfigure();
    else
        return false;
}

QString InputOutputMap::inputPluginStatus(const QString& pluginName, quint32 input)
{
    QLCIOPlugin* inputPlugin = NULL;
    QString info;

    if (pluginName.isEmpty() == false)
        inputPlugin = m_doc->ioPluginCache()->plugin(pluginName);

    if (inputPlugin != NULL)
    {
        info = inputPlugin->inputInfo(input);
    }
    else
    {
        /* Nothing selected */
        info += QString("<HTML><HEAD></HEAD><BODY>");
        info += QString("<H3>%1</H3>").arg(tr("Nothing selected"));
        info += QString("</BODY></HTML>");
    }

    return info;
}

QString InputOutputMap::outputPluginStatus(const QString& pluginName, quint32 output)
{
    QLCIOPlugin* outputPlugin = m_doc->ioPluginCache()->plugin(pluginName);
    if (outputPlugin != NULL)
    {
        return outputPlugin->outputInfo(output);
    }
    else
    {
        QString info;
        info += QString("<HTML><HEAD></HEAD><BODY>");
        info += QString("<H3>%1</H3>").arg(tr("Nothing selected"));
        info += QString("</BODY></HTML>");
        return info;
    }
}

bool InputOutputMap::sendFeedBack(quint32 universe, quint32 channel, uchar value, const QVariant &params)
{
    if (universe >= universesCount())
        return false;

    OutputPatch* patch = m_universeArray.at(universe)->feedbackPatch();

    if (patch != NULL && patch->isPatched())
    {
        patch->plugin()->sendFeedBack(universe, patch->output(), channel, value, params);
        return true;
    }
    else
    {
        return false;
    }
}

void InputOutputMap::followInputDevices(QLCIOPlugin *plugin)
{
    const QStringList lines = plugin->inputs();
    bool kept = false;

    for (quint32 i = 0; i < universesCount(); i++)
    {
        Universe *uni = m_universeArray.at(i);
        InputPatch *ip = uni->inputPatch();
        if (ip == NULL || ip->plugin() != plugin || ip->inputUID().isEmpty())
            continue;

        const QString device = ip->inputUID();
        const int line = lines.indexOf(device);
        if (line == int(ip->input()))
            continue;   // where it was: reconnect() below is enough

        if (line >= 0)
        {
            /* Still here, on another line: a device before it in the
               plugin's list went away. Reopened on its own line, not on
               whatever now sits at the old one. */
            qDebug() << "[IOMAP] universe" << i + 1 << "input" << device
                     << "moved from line" << ip->input() << "to" << line;
            QMutexLocker locker(&m_universeMutex);
            uni->setInputPatch(plugin, quint32(line), ip->profile());
            continue;
        }

        /* Gone. Keep the patch as a workspace would describe it, so it is
           saved unchanged and reopens by itself when the device is back. */
        qDebug() << "[IOMAP] universe" << i + 1 << "input" << device << "disconnected: kept offline";
        Universe::OfflinePatch rec;
        rec.tag = Universe::InputPatchTag;
        rec.plugin = plugin->name();
        rec.uid = device;
        rec.line = ip->input();
        rec.profile = ip->profileName();
        rec.hidProfile = ip->hidProfileName();
        rec.params = ip->getPluginParameters();
        setInputPatch(i, KInputNone, QString(), QLCIOPlugin::invalidLine());
        uni->addOfflinePatch(rec);
        kept = true;
        emit inputDeviceDisconnected(i, rec.plugin, device);
    }

    if (kept)
        emit offlinePatchesChanged();
}

void InputOutputMap::slotPluginConfigurationChanged(QLCIOPlugin* plugin)
{
    /* A device input (game controller, MIDI surface) is a line that comes
       and goes: follow it by name before reopening anything by number. */
    if (plugin->linesAreHardware())
        followInputDevices(plugin);

    QMutexLocker locker(&m_universeMutex);
    bool success = true;
    for (quint32 i = 0; i < universesCount(); i++)
    {
        Universe *universe = m_universeArray.at(i);
        for (int oi = 0; oi < universe->outputPatchesCount(); oi++)
        {
            OutputPatch* op = universe->outputPatch(oi);

            if (op != NULL && op->plugin() == plugin)
            {
                /*success = */ op->reconnect();
            }
        }

        InputPatch* ip = m_universeArray.at(i)->inputPatch();

        if (ip != NULL && ip->plugin() == plugin)
        {
            /*success = */ ip->reconnect();
        }

        OutputPatch* fp = m_universeArray.at(i)->feedbackPatch();
        if (fp != NULL && fp->plugin() == plugin)
        {
            /*success = */ fp->reconnect();
        }
    }
    locker.unlock();

    /* The device a kept patch waits for may be what just appeared: reopen
       it now rather than at the next retry tick. */
    if (plugin->linesAreHardware() && hasOfflinePatches())
        retryOfflinePatches();

    emit pluginConfigurationChanged(plugin->name(), success);
}

/*****************************************************************************
 * Profiles
 *****************************************************************************/

void InputOutputMap::loadProfiles(const QDir& dir)
{
    if (dir.exists() == false || dir.isReadable() == false)
        return;

    /* Go thru all found file entries and attempt to load an input
       profile from each of them. */
    QStringListIterator it(dir.entryList());
    while (it.hasNext() == true)
    {
        QLCInputProfile* prof;
        QString path;

        path = dir.absoluteFilePath(it.next());
        if (QLCFile::isOnlineOnly(path))   // in cloud storage, not on this machine: do not wait for it
        {
            QLCFile::noteSkippedOnlineOnly(path);
            continue;
        }

        prof = QLCInputProfile::loader(path);
        if (prof != NULL)
        {
            /* Check for duplicates */
            if (profile(prof->name()) == NULL)
                addProfile(prof);
            else
                delete prof;
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unable to find an input profile from" << path;
        }
    }
}

QStringList InputOutputMap::profileNames()
{
    QStringList list;
    QListIterator <QLCInputProfile*> it(m_profiles);
    while (it.hasNext() == true)
        list << it.next()->name();
    return list;
}

QLCInputProfile* InputOutputMap::profile(const QString& name)
{
    QListIterator <QLCInputProfile*> it(m_profiles);
    while (it.hasNext() == true)
    {
        QLCInputProfile *profile = it.next();
        if (profile->name() == name)
            return profile;
    }

    // Attempt to load input profile
    // from the workspace folder
    if (m_localProfilesLoaded == false)
    {
        if (m_doc->workspacePath().isEmpty())
            return NULL;

        m_localProfilesLoaded = true;

        qDebug() << "Input profile" << name << "not found. Attempt to load it from" << m_doc->workspacePath();
        QDir localDir(m_doc->workspacePath());
        localDir.setFilter(QDir::Files);
        localDir.setNameFilters(QStringList() << QString("*%1").arg(KExtInputProfile));
        loadProfiles(localDir);

        QListIterator <QLCInputProfile*> it(m_profiles);
        while (it.hasNext() == true)
        {
            QLCInputProfile *profile = it.next();
            if (profile->name() == name)
                return profile;
        }
    }

    return NULL;
}

bool InputOutputMap::addProfile(QLCInputProfile* profile)
{
    Q_ASSERT(profile != NULL);

    /* Don't add the same profile twice */
    if (m_profiles.contains(profile) == false)
    {
        m_profiles.append(profile);
        return true;
    }
    else
    {
        return false;
    }
}

bool InputOutputMap::removeProfile(const QString& name)
{
    QMutableListIterator <QLCInputProfile*> it(m_profiles);
    while (it.hasNext() == true)
    {
        QLCInputProfile *profile = it.next();
        if (profile->name() == name)
        {
            it.remove();
            delete profile;
            return true;
        }
    }

    return false;
}

bool InputOutputMap::inputSourceNames(const QLCInputSource *src,
                                QString& uniName, QString& chName) const
{
    if (src == NULL || src->isValid() == false)
        return false;

    if (src->universe() >= universesCount())
        return false;

    InputPatch* pat = m_universeArray.at(src->universe())->inputPatch();
    if (pat == NULL)
    {
        /* There is no patch for the given universe */
        uniName = QString("%1 -UNPATCHED-").arg(src->universe() + 1);

        ushort page = src->page();
        ushort channel = (src->channel() & 0x0000FFFF) + 1;

        if (page != 0)
            chName = QString("%1: ? (Page %2)").arg(channel).arg(page + 1);
        else
            chName = QString("%1: ?").arg(channel);
        return true;
    }

    QLCInputProfile* profile = pat->profile();
    if (profile == NULL)
    {
        /* There is no profile. Display plugin name and channel number. */
        if (pat->plugin() != NULL)
            uniName = QString("%1: %2").arg(src->universe() + 1).arg(pat->plugin()->name());
        else
            uniName = QString("%1: ??").arg(src->universe() + 1);

        ushort page = src->page();
        ushort channel = (src->channel() & 0x0000FFFF) + 1;

        if (page != 0)
            chName = QString("%1: ? (Page %2)").arg(channel).arg(page + 1);
        else
            chName = QString("%1: ?").arg(channel);
    }
    else
    {
        QLCInputChannel* ich;
        QString name;

        /* Display profile name for universe */
        uniName = QString("%1: %2").arg(src->universe() + 1).arg(profile->name());

        /* User can input the channel number by hand, so put something
           rational to the channel name in those cases as well. */
        ushort page = src->page();
        ushort channel = (src->channel() & 0x0000FFFF);

        ich = profile->channel(channel);
        if (ich != NULL)
            name = ich->name();
        else
            name = QString("?");

        /* Display channel name */
        if (page != 0)
            chName = QString("%1: %2 (Page %3)").arg(channel + 1).arg(name).arg(page + 1);
        else
            chName = QString("%1: %2").arg(channel + 1).arg(name);
    }

    return true;
}

bool InputOutputMap::inputSourceNames(QSharedPointer<QLCInputSource> const& src,
                                QString& uniName, QString& chName) const
{
    return inputSourceNames(src.data(), uniName, chName);
}

QDir InputOutputMap::systemProfileDirectory()
{
    return QLCFile::systemDirectory(QString(INPUTPROFILEDIR), QString(KExtInputProfile));
}

QDir InputOutputMap::userProfileDirectory()
{
    return QLCFile::userDirectory(QString(USERINPUTPROFILEDIR), QString(INPUTPROFILEDIR),
                                  QStringList() << QString("*%1").arg(KExtInputProfile));
}

/*********************************************************************
 * Beats
 *********************************************************************/

void InputOutputMap::setBeatGeneratorType(InputOutputMap::BeatGeneratorType type)
{
    if (type == m_beatGeneratorType)
        return;

    if (m_beatGeneratorType == Audio)
    {
        m_inputCapture->unregisterBandsNumber(4);
        disconnect(m_inputCapture, SIGNAL(beatDetected()), this, SLOT(slotProcessBeat()));
    }

    m_beatGeneratorType = type;
    qDebug() << "[InputOutputMap] setting beat type:" << m_beatGeneratorType;

    switch (m_beatGeneratorType)
    {
        case Internal:
        {
            m_doc->masterTimer()->setBeatSourceType(MasterTimer::Internal);
            setBpmNumber(m_doc->masterTimer()->bpmNumber());
        }
        break;
        case Plugin:
        {
            m_doc->masterTimer()->setBeatSourceType(MasterTimer::External);
            // reset the current BPM number and detect it from the MIDI beats
            setBpmNumber(0);
            m_beatTime->restart();
        }
        break;
        case Audio:
        {
            m_doc->masterTimer()->setBeatSourceType(MasterTimer::External);
            // reset the current BPM number and detect it from the audio input
            setBpmNumber(0);
            m_beatTime->restart();
            QSharedPointer<AudioCapture> capture(m_doc->audioInputCapture());
            m_inputCapture = capture.data();
            connect(m_inputCapture, SIGNAL(beatDetected()), this, SLOT(slotProcessBeat()));
            m_inputCapture->registerBandsNumber(4);
        }
        break;
        case Disabled:
        default:
            m_doc->masterTimer()->setBeatSourceType(MasterTimer::None);
            setBpmNumber(0);
        break;
    }

    emit beatGeneratorTypeChanged();
}

InputOutputMap::BeatGeneratorType InputOutputMap::beatGeneratorType() const
{
    return m_beatGeneratorType;
}

int InputOutputMap::audioInputBlockMs() const
{
    if (m_beatGeneratorType != Audio || m_inputCapture == NULL
            || m_inputCapture->sampleRate() == 0)
        return -1;
    return qRound(1000.0 * double(m_inputCapture->captureSize())
                  / double(m_inputCapture->sampleRate()));
}

QString InputOutputMap::beatTypeToString(BeatGeneratorType type) const
{
    switch (type)
    {
        case Internal:  return "Internal";
        case Plugin:    return "Plugin";
        case Audio:     return "Audio";
        default:        return "Disabled";
    }
}

InputOutputMap::BeatGeneratorType InputOutputMap::stringToBeatType(QString str)
{
    if (str == "Internal")
        return Internal;
    else if (str == "Plugin")
        return Plugin;
    else if (str == "Audio")
        return Audio;

    return Disabled;
}

void InputOutputMap::setBpmNumber(int bpm)
{
    if (m_beatGeneratorType == Disabled || bpm == m_currentBPM)
        return;

    //qDebug() << "[InputOutputMap] set BPM to" << bpm;
    m_currentBPM = bpm;

    if (bpm != 0)
        m_doc->masterTimer()->requestBpmNumber(bpm);

    emit bpmNumberChanged(m_currentBPM);
}

double InputOutputMap::tapBeat()
{
    MasterTimer *mt = m_doc->masterTimer();
    if (m_beatGeneratorType == Disabled)
        return mt->bpm();
    const double bpm = mt->tap();
    if (qRound(bpm) != m_currentBPM)
    {
        m_currentBPM = qRound(bpm);
        emit bpmNumberChanged(m_currentBPM);
    }
    return bpm;
}

int InputOutputMap::bpmNumber() const
{
    if (m_beatGeneratorType == Disabled)
        return 0;

    return m_currentBPM;
}

void InputOutputMap::slotProcessBeat()
{
    // process the timer as first thing, to avoid wasting time
    // with the operations below
    qint64 elapsed = m_beatTime->elapsed();
    m_beatTime->restart();

    int bpm = qRound(60000.0 / (float)elapsed);
    float currBpmTime = 60000.0 / (float)m_currentBPM;
    // here we check if the difference between the current BPM duration
    // and the current time elapsed is within a range of +/-1ms.
    // If it isn't, then the BPM number has really changed, otherwise
    // it's just a tiny time drift
    if (qAbs((float)elapsed - currBpmTime) > 1)
        setBpmNumber(bpm);

    m_doc->masterTimer()->requestBeat();
    emit beat();
}

void InputOutputMap::slotMasterTimerBeat()
{
    if (m_beatGeneratorType != Internal)
        return;

    emit beat();
}

void InputOutputMap::slotPluginBeat(quint32 universe, quint32 channel, uchar value, const QString &key)
{
    Q_UNUSED(universe)

    // not interested in synthetic release or non-beat event
    if (m_beatGeneratorType != Plugin || value == 0 || key != "beat")
        return;

    qDebug() << "Plugin beat:" << channel << m_beatTime->elapsed();

    slotProcessBeat();
}

/*********************************************************************
 * Network server
 *********************************************************************/

void InputOutputMap::setNetworkServerType(InputOutputMap::NetworkServerType type)
{
    m_networkServerType = type;
}

InputOutputMap::NetworkServerType InputOutputMap::networkServerType() const
{
    return m_networkServerType;
}

QString InputOutputMap::networkServerTypeToString(InputOutputMap::NetworkServerType type) const
{
    if (type == WebServer)
        return "Web";
    return "Native";
}

InputOutputMap::NetworkServerType InputOutputMap::stringToNetworkServerType(const QString &str) const
{
    if (str.compare("Web", Qt::CaseInsensitive) == 0)
        return WebServer;
    return NativeServer;
}

void InputOutputMap::setNetworkServerAutoStart(bool enable)
{
    m_networkServerAutoStart = enable;
}

bool InputOutputMap::networkServerAutoStart() const
{
    return m_networkServerAutoStart;
}

void InputOutputMap::setNetworkServerName(QString name)
{
    m_networkServerName = name;
}

QString InputOutputMap::networkServerName() const
{
    return m_networkServerName;
}

void InputOutputMap::setNetworkServerPassword(QString password)
{
    m_networkServerPassword = password;
}

QString InputOutputMap::networkServerPassword() const
{
    return m_networkServerPassword;
}

/*********************************************************************
 * Defaults - !! FALLBACK !!
 *********************************************************************/

void InputOutputMap::loadDefaults()
{
    /* ************************ INPUT *********************************** */
    QSettings settings;
    QString plugin;
    QString input;
    QString key;

    for (quint32 i = 0; i < universesCount(); i++)
    {
        QString profileName;
        bool passthrough;

        /* Plugin name */
        key = QString("/inputmap/universe%1/plugin/").arg(i);
        plugin = settings.value(key).toString();

        /* Plugin input */
        key = QString("/inputmap/universe%1/input/").arg(i);
        input = settings.value(key).toString();

        /* Input profile */
        key = QString("/inputmap/universe%1/profile/").arg(i);
        profileName = settings.value(key).toString();

        key = QString("/inputmap/universe%1/passthrough/").arg(i);
        passthrough = settings.value(key).toBool();
        if (passthrough == true)
            m_universeArray.at(i)->setPassthrough(passthrough);

        /* Do the mapping */
        if (plugin != KInputNone && input != KInputNone)
            setInputPatch(i, plugin, "", input.toUInt(), profileName);
    }

    /* ************************ OUTPUT *********************************** */
    QString output;
    QString fb_plugin;
    QString feedback;

    for (quint32 i = 0; i < universesCount(); i++)
    {
        /* Plugin name */
        key = QString("/outputmap/universe%1/plugin/").arg(i);
        plugin = settings.value(key).toString();

        /* Plugin output */
        key = QString("/outputmap/universe%1/output/").arg(i);
        output = settings.value(key).toString();

        /* Feedback plugin name */
        key = QString("/outputmap/universe%1/feedbackplugin/").arg(i);
        fb_plugin = settings.value(key).toString();

        /* Feedback line */
        key = QString("/outputmap/universe%1/feedback/").arg(i);
        feedback = settings.value(key).toString();

        if (plugin != KOutputNone && output != KOutputNone)
            setOutputPatch(i, plugin, "", output.toUInt());

        if (fb_plugin != KOutputNone && feedback != KOutputNone)
            setOutputPatch(i, fb_plugin, "", feedback.toUInt(), true);
    }
}

void InputOutputMap::saveDefaults()
{
    /* ************************ INPUT *********************************** */
    QSettings settings;
    QString key;

    for (quint32 i = 0; i < universesCount(); i++)
    {
        InputPatch* inPatch = inputPatch(i);

        /* Plugin name */
        key = QString("/inputmap/universe%1/plugin/").arg(i);
        if (inPatch != NULL)
            settings.setValue(key, inPatch->pluginName());
        else
            settings.setValue(key, KInputNone);

        /* Plugin input */
        key = QString("/inputmap/universe%1/input/").arg(i);
        if (inPatch != NULL)
            settings.setValue(key, QString::number(inPatch->input()));
        else
            settings.setValue(key, KInputNone);

        /* Input profile */
        key = QString("/inputmap/universe%1/profile/").arg(i);
        if (inPatch != NULL)
            settings.setValue(key, inPatch->profileName());
        else
            settings.setValue(key, KInputNone);

        /* Passthrough */
        key = QString("/inputmap/universe%1/passthrough/").arg(i);
        bool passthrough = m_universeArray.at(i)->passthrough();
        if (passthrough == true)
            settings.setValue(key, passthrough);
        else
            settings.remove(key);
    }

    /* ************************ OUTPUT *********************************** */

    for (quint32 i = 0; i < universesCount(); i++)
    {
        OutputPatch* outPatch = outputPatch(i);
        OutputPatch* fbPatch = feedbackPatch(i);

        key = QString("/outputmap/universe%1/plugin/").arg(i);

        /* Plugin name */
        if (outPatch != NULL)
            settings.setValue(key, outPatch->pluginName());
        else
            settings.setValue(key, KOutputNone);

        /* Plugin output */
        key = QString("/outputmap/universe%1/output/").arg(i);
        if (outPatch != NULL)
            settings.setValue(key, outPatch->output());
        else
            settings.setValue(key, KOutputNone);

        key = QString("/outputmap/universe%1/feedbackplugin/").arg(i);

        /* Feedback plugin name */
        if (fbPatch != NULL)
            settings.setValue(key, fbPatch->pluginName());
        else
            settings.setValue(key, KOutputNone);

        /* Feedback plugin output */
        key = QString("/outputmap/universe%1/feedback/").arg(i);
        if (fbPatch != NULL)
            settings.setValue(key, QString::number(fbPatch->output()));
        else
            settings.setValue(key, KOutputNone);
    }
}

/*********************************************************************
 * Load & Save
 *********************************************************************/

bool InputOutputMap::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLIOMap)
    {
        qWarning() << Q_FUNC_INFO << "InputOutputMap node not found";
        return false;
    }

    /** Reset the current universe list and read the new one */
    removeAllUniverses();
    /* A held step describes the patch of the workspace being replaced.
       Restoring it onto this one would write one show's wiring into another. */
    if (m_patchUndo != NULL)
        m_patchUndo->clear();
    m_targetAliases.clear();
    m_manualTargets.clear();
    m_lineAliases.clear();
    /* An older show has none, and behaves as it always did. "Transmit
       anyway" was said about the show being replaced. */
    m_rigNetworks.clear();
    m_transmitOverride = false;
    /* Stock 4.14.4's network-server settings belong to the show they came
       with: an older show has none, and gets the defaults. */
    m_networkServerType = NativeServer;
    m_networkServerAutoStart = false;
    m_networkServerName.clear();
    m_networkServerPassword.clear();

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLIORigNetwork)
        {
            /* Before the universes in the file, so the patches below are
               loaded knowing where they may transmit. */
            const QString net = RigNetwork::normalise(root.readElementText());
            if (net.isEmpty() == false && m_rigNetworks.contains(net) == false)
                m_rigNetworks << net;
            continue;   // readElementText already consumed the end element
        }
        else if (root.name() == KXMLQLCUniverse)
        {
            quint32 id = InputOutputMap::invalidUniverse();
            if (root.attributes().hasAttribute(KXMLQLCUniverseID))
                id = root.attributes().value(KXMLQLCUniverseID).toString().toUInt();
            if (addUniverse(id))
            {
                Universe *uni = m_universeArray.last();
                uni->loadXML(root, m_universeArray.count() - 1, this);
            }
            else
            {
                /* Out of range or a duplicate: read past it, or its
                   children would be taken for the map's own. */
                root.skipCurrentElement();
            }
        }
        else if (root.name() == QStringLiteral("TargetAlias"))
        {
            const QString addr = root.attributes()
                                 .value(QStringLiteral("Address")).toString();
            const QString name = root.readElementText();
            if (addr.isEmpty() == false && name.isEmpty() == false)
                m_targetAliases[addr] = name;
            continue;   // readElementText already consumed the end element
        }
        else if (root.name() == QStringLiteral("LineAlias"))
        {
            const QString plug = root.attributes()
                                 .value(QStringLiteral("Plugin")).toString();
            const QString ln = root.attributes()
                               .value(QStringLiteral("Line")).toString();
            const QString name = root.readElementText();
            if (plug.isEmpty() == false && name.isEmpty() == false)
                m_lineAliases[plug + "|" + ln] = name;
            continue;   // readElementText already consumed the end element
        }
        else if (root.name() == QStringLiteral("ManualTarget"))
        {
            ManualTarget t;
            t.plugin = root.attributes().value(QStringLiteral("Plugin")).toString();
            t.line = root.attributes().value(QStringLiteral("Line")).toString().toUInt();
            t.address = root.attributes().value(QStringLiteral("Address")).toString();
            while (root.readNextStartElement())
            {
                if (root.name() == QStringLiteral("Port"))
                {
                    bool ok = false;
                    const quint32 pa = root.attributes()
                                       .value(QStringLiteral("Address"))
                                       .toString().toUInt(&ok);
                    if (ok)
                        t.ports.append(pa);
                }
                root.skipCurrentElement();
            }
            if (t.address.isEmpty() == false)
                m_manualTargets.append(t);
            continue;   // the child loop already consumed our end element
        }
        else if (root.name() == KXMLIOBeatGenerator)
        {
            QXmlStreamAttributes attrs = root.attributes();

            if (attrs.hasAttribute(KXMLIOBeatType))
                setBeatGeneratorType(stringToBeatType(attrs.value(KXMLIOBeatType).toString()));

            if (attrs.hasAttribute(KXMLIOBeatsPerMinute))
                setBpmNumber(attrs.value(KXMLIOBeatsPerMinute).toInt());

            root.skipCurrentElement();
        }
        else if (root.name() == KXMLIONetworkServer)
        {
            QXmlStreamAttributes attrs = root.attributes();
            NetworkServerType type = NativeServer;

            if (attrs.hasAttribute(KXMLIONetworkType))
                type = stringToNetworkServerType(attrs.value(KXMLIONetworkType).toString());
            setNetworkServerType(type);

            if (attrs.hasAttribute(KXMLIONetworkAutoStart))
                setNetworkServerAutoStart(attrs.value(KXMLIONetworkAutoStart) == KXMLQLCTrue);

            if (type == NativeServer)
            {
                setNetworkServerName(attrs.value(KXMLIONetworkName).toString());
                setNetworkServerPassword(attrs.value(KXMLIONetworkPassword).toString());
            }
            else
            {
                setNetworkServerName(QString());
                setNetworkServerPassword(QString());
            }

            root.skipCurrentElement();
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown IO Map tag:" << root.name();
            root.skipCurrentElement();
        }
    }

    /* A hand-edited file may list them after the universes: hold whatever
       was opened meanwhile, and set every kept patch's held reason now
       rather than on the next tick. */
    applyTransmitGate();
    if (hasOfflinePatches())
        retryOfflinePatches();
    noteTransmitGateStatus();

    return true;
}

QString InputOutputMap::targetAlias(const QString &address) const
{
    return m_targetAliases.value(address);
}

void InputOutputMap::setTargetAlias(const QString &address, const QString &name)
{
    if (address.isEmpty())
        return;
    if (name.isEmpty())
        m_targetAliases.remove(address);
    else
        m_targetAliases[address] = name;
}

QMap<QString, QString> InputOutputMap::targetAliases() const
{
    return m_targetAliases;
}

QString InputOutputMap::lineAlias(const QString &plugin, const QString &lineName) const
{
    return m_lineAliases.value(plugin + "|" + lineName);
}

void InputOutputMap::setLineAlias(const QString &plugin, const QString &lineName,
                                  const QString &alias)
{
    const QString key = plugin + "|" + lineName;
    if (alias.isEmpty())
        m_lineAliases.remove(key);
    else
        m_lineAliases[key] = alias;
}

QList<quint32> InputOutputMap::universesWithInputOn(const QString &plugin,
                                                    quint32 line) const
{
    QList<quint32> list;
    foreach (Universe *uni, m_universeArray)
    {
        InputPatch *ip = uni->inputPatch();
        if (ip != NULL && ip->pluginName() == plugin && ip->input() == line)
            list << uni->id();
    }
    return list;
}

PatchUndo *InputOutputMap::patchUndo() const
{
    return m_patchUndo;
}

QList<InputOutputMap::ManualTarget> InputOutputMap::manualTargets() const
{
    return m_manualTargets;
}

bool InputOutputMap::isManualTarget(const QString &address) const
{
    foreach (const ManualTarget &t, m_manualTargets)
    {
        if (t.address == address)
            return true;
    }
    return false;
}

void InputOutputMap::addManualTarget(const QString &plugin, quint32 line,
                                     const QString &address)
{
    if (address.isEmpty() || isManualTarget(address))
        return;

    ManualTarget t;
    t.plugin = plugin;
    t.line = line;
    t.address = address;
    m_manualTargets.append(t);
}

void InputOutputMap::removeManualTarget(const QString &address)
{
    for (int i = 0; i < m_manualTargets.count(); i++)
    {
        if (m_manualTargets.at(i).address == address)
        {
            m_manualTargets.removeAt(i);
            return;
        }
    }
}

void InputOutputMap::addManualPort(const QString &address, quint32 portAddress)
{
    for (int i = 0; i < m_manualTargets.count(); i++)
    {
        if (m_manualTargets.at(i).address != address)
            continue;
        /* Two ports on one node CAN legitimately share an address -- see the
           OLA case in the connections tree -- but a duplicate here would be
           the same row twice, not two ports. */
        if (m_manualTargets.at(i).ports.contains(portAddress) == false)
            m_manualTargets[i].ports.append(portAddress);
        return;
    }
}

void InputOutputMap::removeManualPort(const QString &address, quint32 portAddress)
{
    for (int i = 0; i < m_manualTargets.count(); i++)
    {
        if (m_manualTargets.at(i).address == address)
        {
            m_manualTargets[i].ports.removeAll(portAddress);
            return;
        }
    }
}

bool InputOutputMap::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != NULL);

    /* IO Map Instance entry */
    doc->writeStartElement(KXMLIOMap);

    doc->writeStartElement(KXMLIOBeatGenerator);
    doc->writeAttribute(KXMLIOBeatType, beatTypeToString(m_beatGeneratorType));
    doc->writeAttribute(KXMLIOBeatsPerMinute, QString::number(m_currentBPM));
    doc->writeEndElement();

    /* The show's rig networks, ahead of the universes so a load knows them
       before it opens any patch. An older build skips them (with a warning)
       and transmits as it always did. */
    foreach (const QString &net, m_rigNetworks)
        doc->writeTextElement(KXMLIORigNetwork, net);

    doc->writeStartElement(KXMLIONetworkServer);
    doc->writeAttribute(KXMLIONetworkType, networkServerTypeToString(m_networkServerType));
    doc->writeAttribute(KXMLIONetworkAutoStart, m_networkServerAutoStart ? KXMLQLCTrue : KXMLQLCFalse);
    if (m_networkServerType == NativeServer)
    {
        doc->writeAttribute(KXMLIONetworkName, m_networkServerName);
        doc->writeAttribute(KXMLIONetworkPassword, m_networkServerPassword);
    }
    doc->writeEndElement();

    foreach (Universe *uni, m_universeArray)
        uni->saveXML(doc);

    /* Operator-supplied names for output targets. Written after the universes
       so an older build, which skips unknown elements, still reads the patch
       itself correctly and merely loses the labels. */
    QMapIterator<QString, QString> ait(m_targetAliases);
    while (ait.hasNext())
    {
        ait.next();
        doc->writeStartElement(QStringLiteral("TargetAlias"));
        doc->writeAttribute(QStringLiteral("Address"), ait.key());
        doc->writeCharacters(ait.value());
        doc->writeEndElement();
    }

    QMapIterator<QString, QString> lit(m_lineAliases);
    while (lit.hasNext())
    {
        lit.next();
        const int sep = lit.key().indexOf('|');
        if (sep < 0)
            continue;
        doc->writeStartElement(QStringLiteral("LineAlias"));
        doc->writeAttribute(QStringLiteral("Plugin"), lit.key().left(sep));
        doc->writeAttribute(QStringLiteral("Line"), lit.key().mid(sep + 1));
        doc->writeCharacters(lit.value());
        doc->writeEndElement();
    }

    /* Hand-declared targets, written last for the same reason as the aliases:
       an older build skips what it does not recognise and still loads the
       patch correctly. */
    foreach (const ManualTarget &t, m_manualTargets)
    {
        doc->writeStartElement(QStringLiteral("ManualTarget"));
        doc->writeAttribute(QStringLiteral("Plugin"), t.plugin);
        doc->writeAttribute(QStringLiteral("Line"), QString::number(t.line));
        doc->writeAttribute(QStringLiteral("Address"), t.address);
        foreach (quint32 pa, t.ports)
        {
            doc->writeStartElement(QStringLiteral("Port"));
            doc->writeAttribute(QStringLiteral("Address"), QString::number(pa));
            doc->writeEndElement();
        }
        doc->writeEndElement();
    }

    doc->writeEndElement();

    return true;
}
