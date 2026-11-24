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
#include "iodeferral.h"
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
    // The closing-frame source handed to the plugins points here.
    if (m_doc != NULL && m_doc->ioPluginCache() != NULL)
    {
        foreach (QLCIOPlugin *plugin, m_doc->ioPluginCache()->plugins())
        {
            if (plugin != NULL)
                plugin->clearClosingFrameSource(this);
        }
    }
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

        /* S0, every send: under the frame lock, the Show-only channels
           through the builder. In Blind, Blackout still sends (on and off):
           the frame from before Blind, the Show-only channels at safe. */
        if (universe->inhibitOutput())
            universe->sendHeld();
        else
            universe->sendCurrent();
    }

    return true;
}

bool InputOutputMap::setBlackout(bool blackout)
{
    // GUI entry (blackout button / menu): the caller does NOT hold
    // m_universeMutex, so take it here to serialise with the MasterTimer tick.
    // The frames Blackout sends (sendCurrent/sendHeld -> writeUniverse) are
    // written here, under the lock: IODeferral defers opens and closes only.
    bool changed;
    {
        IODeferral io;
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
    /* Show only (R7-4): Blind ending starts a new session (OutputRuleKeeper)
       before anything is sent -- a GO taken in Blind was checked on
       screen; it needs a GO again to fire. */
    if (inhibit == false)
        emit outputResuming();

    // GUI entry only: take m_universeMutex to serialise the universe iteration
    // and the dumpOutput() calls with the MasterTimer tick (which holds the same
    // lock via claimUniverses), so we never touch a universe's output or the
    // array concurrently with the tick / a structural change. The frames it
    // sends are written under the lock (IODeferral defers opens and closes,
    // not frame writes).
    {
        IODeferral io;
        QMutexLocker locker(&m_universeMutex);
        foreach (Universe *universe, m_universeArray)
        {
            universe->setInhibitOutput(inhibit);
            // ...and its Show-only channels at safe in what goes out now.
            if (inhibit == false && universe->showOnlyLive() && universe->showOnlyChannelCount() > 0)
                universe->cutShowOnly();

            // Push the current values through immediately: while inhibited
            // dumpOutput() is a no-op (the rig keeps the frame from before);
            // when resuming, this forces the plugins to catch up to the
            // latest look right away. R8-6: entering Blind, a universe with
            // Show-only channels sends one frame with them at safe -- what
            // the rig keeps through Blind.
            if (inhibit && universe->showOnlyChannelCount() > 0)
                universe->sendHeld();
            else
                universe->sendCurrent();
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
        IODeferral io;
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
                // In Blind: inhibited from its first frame, as the others are.
                uni->setInhibitOutput(m_outputInhibited);
                uni->setLoadHold(m_loadHold);
                connect(uni, SIGNAL(universeWritten(quint32,QByteArray)), this, SIGNAL(universeWritten(quint32,QByteArray)));
                watchShowOnly(uni);
                m_universeArray.append(uni);
            }
        }

        /* Woken each frame by MasterTimer::timerTick() itself, not through
           the GUI thread's event loop (see there). Created while Blind is on
           (added, or recreated by a show loaded in Blind): inhibited from
           its first frame, matching the others. */
        uni = new Universe(id, m_grandMaster);
        uni->setInhibitOutput(m_outputInhibited);
        uni->setLoadHold(m_loadHold);
        connect(uni, SIGNAL(universeWritten(quint32,QByteArray)), this, SIGNAL(universeWritten(quint32,QByteArray)));
        watchShowOnly(uni);
        m_universeArray.append(uni);
    }

    emit universeAdded(id);
    return true;
}

bool InputOutputMap::removeUniverse(int index)
{
    {
        IODeferral io;      // its patches closed after the claim
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
    IODeferral io;      // their patches closed after the claim
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

QList<Universe *> InputOutputMap::universesSnapshot()
{
    // tryLock, not lock: the claim is not recursive, and the tick that
    // holds it may be the caller. Briefly waited for otherwise.
    if (m_universeMutex.tryLock(200))
    {
        const QList<Universe *> copy = m_universeArray;
        m_universeMutex.unlock();
        return copy;
    }
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
    /* A patch change made under the claim (a Show-only hold, said
       directly) reaches its device once the claim is let go. */
    const bool outer = IODeferral::begin();
    m_universeMutex.lock();
    m_claimDeferral = outer;
    return m_universeArray;
}

void InputOutputMap::releaseUniverses(bool changed)
{
    m_universeChanged = changed;
    const bool outer = m_claimDeferral;
    m_claimDeferral = false;
    m_universeMutex.unlock();
    IODeferral::end(outer);
}

void InputOutputMap::resetUniverses()
{
    {
        IODeferral io;
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
    IODeferral io;
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

    /* What the patch there read and has not delivered yet goes out first,
       with the patch as it was -- its device's roles -- before it is
       replaced or let go: a stop press in flight still counts (board
       Q253, requirement D). Here, with no lock held: the receivers take
       m_universeMutex (see the note before inputPatchChanged below). And
       before the IODeferral below: whatever those values make the
       receivers do reaches the old patch's device as it is, not after the
       change. */
    if (InputPatch *before = m_universeArray.at(universe)->inputPatch())
        before->deliverNow();

    /* The device closed and opened after the universes are let go: a
       controller whose reader stopped answering waits there (up to
       HIDDevice::kReaderStopMs), and a device the system never answers
       for -- never with every universe's DMX output held. */
    IODeferral io;

    /* Patched by hand to one device (or unpatched): a controller slot gives
       way, unless that device is one of its own. */
    if (m_applyingSlot == false && m_universeArray.at(universe)->hasInputSlot())
        keepOrDropInputSlot(universe, pluginName, inputUID, input);

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
            ip->setChannelMap(QVector<quint32>());  // a slot sets its own after

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

    /* Said with the universes let go: profileChanged reaches the pad's
       tools (ProgrammerController::focusControllerChanged) on this thread,
       and anything there that takes m_universeMutex (Blackout, a re-patch)
       would lock up the GUI. */
    locker.unlock();
    const bool opened = io.finish();
    if (ip != NULL && currProfile != ip->profile())
        emit profileChanged(universe, ip->profileName());
    emit inputPatchChanged(universe);

    return opened;
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
                                    bool isFeedback, int index,
                                    const QMap<QString, QVariant> &params)
{
    /* The bookkeeping under the universe claim, the device calls (the old
       line's closing frame and close, the new one's open and target) after
       it: a DMX interface that does not answer while it is opened or
       closed (a USB hub wedged by a device re-enumerating) never stops the
       other universes' output. The patch sends nothing until its calls
       have run. (A frame write that hangs is not covered: iodeferral.h.) */
    IODeferral io;
    const bool done = patchOutput(universe, pluginName, outputUID, output, isFeedback, index, params);
    return io.finish() && done;
}

bool InputOutputMap::patchOutput(quint32 universe, const QString &pluginName,
                                 const QString &outputUID, quint32 output,
                                 bool isFeedback, int index,
                                 const QMap<QString, QVariant> &params)
{
    m_lastPatchRefusal.clear();
    /* Check that the universe that we're doing mapping for is valid */
    if (universe >= universesCount())
    {
        qWarning() << Q_FUNC_INFO << "Universe" << universe << "out of bounds.";
        return false;
    }

    QMutexLocker locker(&m_universeMutex);
    QLCIOPlugin *plugin = m_doc->ioPluginCache()->plugin(pluginName);
    /* N1 (outputpatch.h): a universe with Show-only fixtures has no output
       but a network DMX one -- open or waiting for its interface. */
    const bool hasUID = outputUID.isEmpty() == false && outputUID != KOutputNone;
    if (isFeedback == false && plugin != NULL && (output != QLCIOPlugin::invalidLine() || hasUID)
        && m_universeArray.at(universe)->showOnlyChannelCount() > 0)
    {
        const QString refusal = showOnlyRefusal(plugin, hasUID ? outputUID : plugin->outputs().value(int(output)),
                                                output);
        if (refusal.isEmpty() == false)
        {
            m_lastPatchRefusal = refusal;
            qWarning() << "[IOMAP] universe" << universe + 1 << "not patched:" << m_lastPatchRefusal;
            return false;
        }
    }
    /* Review 10 M2: a plugin whose lines go away under it (a USB device
       unplugged, a rescan) asks this for each open output's closing frame
       before it lets go of it. */
    if (plugin != NULL && isFeedback == false)
        plugin->setClosingFrameSource([this](quint32 uni, QByteArray &frame) {
            const QList<Universe *> ua = universesSnapshot();
            return uni < quint32(ua.size()) && ua.at(int(uni)) != NULL
                   && ua.at(int(uni))->closingFrame(frame, false);
        }, this);

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
        {
            Universe *pu = m_universeArray.at(universe);
            // The line it asked for kept, written back as it was.
            if (pu->setOutputPatchPending(plugin, outputUID, index, output) == false)
                return false;
            // Its target kept with it, for when it opens (review 12 F7).
            for (auto it = params.constBegin(); it != params.constEnd(); ++it)
                if (OutputPatch *pop = pu->outputPatch(index))
                    pop->setPluginParameter(it.key(), it.value());
            return true;
        }
        }
        else
        {
            qDebug() << "[IOMAP] !!No match found!! for output on universe" << universe << "-" << output << outputUID;
            qDebug() << plugin->outputs();
        }
    }

    if (isFeedback == false)
    {
        /* F4 (outputpatch.h): not by hand onto an interface a held laser
           universe is cabled to. */
        if (plugin != NULL && output != QLCIOPlugin::invalidLine())
        {
            Universe *ru = m_universeArray.at(universe);
            const QString why = reservedWhy(plugin, output, index >= 0 && index < ru->outputPatchesCount()
                                                            ? ru->outputPatch(index) : NULL);
            if (why.isEmpty() == false)
            {
                m_lastPatchRefusal = why;
                qWarning() << "[IOMAP] universe" << universe + 1 << "not patched:" << why;
                return false;
            }
        }
        /* Patching by hand onto a line that is not on a rig network: keep the
           patch, open nothing (see transmitGateActive()). */
        if (plugin != NULL && output != QLCIOPlugin::invalidLine() && transmitGateActive())
        {
            const QString lineName = plugin->outputs().value(int(output));
            QMap<QString, QVariant> lineParams;
            Universe *uni = m_universeArray.at(universe);
            if (NetworkRoute::isAddressIdentity(lineName) == false && index < uni->outputPatchesCount()
                && uni->outputPatch(index) != NULL)
                lineParams = uni->outputPatch(index)->getPluginParameters();
            const QString reason = transmitHoldReason(lineName, lineParams, plugin);
            if (reason.isEmpty() == false && lineName.isEmpty() == false)
            {
                holdOutputPatch(universe, index, plugin, lineName, output,
                                NetworkRoute::networkForLine(lineName), reason);
                for (auto it = params.constBegin(); it != params.constEnd(); ++it)
                    if (OutputPatch *hop = m_universeArray.at(universe)->outputPatch(index))
                        hop->setPluginParameter(it.key(), it.value());
                QTimer::singleShot(0, this, [this]() {
                    emit offlinePatchesChanged();
                    noteTransmitGateStatus();
                });
                return true;
            }
        }
        return m_universeArray.at(universe)->setOutputPatch(plugin, output, index, params);
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
                // F3: "line N of <plugin>", never "None"; a hold says why.
                bad.missingInterface = patch->deviceText();
                bad.heldReason = patch->heldReason().isEmpty() && patch->holdKind() == OutputPatch::HeldReserved
                               ? patch->reconnectHeld() : patch->heldReason();
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
                                     const QString &network, const QString &target,
                                     quint32 savedLine) const
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

    /* H5 (outputpatch.h): a name more than one line carries (two of one
       model with no serial) resolves to the saved line number when that
       line carries it, else to the first, as stock does. */
    if (lines.count(uid) > 1 && savedLine != UINT_MAX && savedLine < quint32(lines.count())
        && lines.at(int(savedLine)) == uid)
        return int(savedLine);
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

    IODeferral io;      // opened after the claims below (iodeferral.h)

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
    const bool showOnly = uni->showOnlyChannelCount() > 0;
    QString keptWhy;
    if (hasIdentity)
    {
        line = resolveSavedLine(plugin, patch.tag != Universe::InputPatchTag, patch.uid,
                                patch.network, NetworkRoute::targetFromParameters(patch.params),
                                patch.line);
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
        /* A patch kept by where it goes, not by this computer's interface (a
           venue's room output, "Art-Net node 10.0.0.20",
           docs/design/VENUE_FROM_SHOW.md): with nothing here reaching it now
           (no cable yet, a late address) it is kept offline and tried again
           by retryOfflinePatches(), never dropped. */
        if (patch.tag == Universe::OutputPatchTag
            && NetworkRoute::targetFromParameters(patch.params).isEmpty() == false
            && (line < 0 || line >= lines.count()))
        {
            qDebug() << "[IOMAP] universe" << universe + 1 << patch.plugin << "to"
                     << NetworkRoute::targetFromParameters(patch.params) << "- nothing here reaches it: kept offline";
            uni->addOfflinePatch(patch);
            return;
        }
    }
    /* N1/N3 on load (outputpatch.h), as far as the Show-only fixtures are
       known by now (recheckPatchesAfterLoad() does the rest): never an
       output but a network DMX one -- kept pending, said. F4: nothing else
       onto an interface a held laser universe is cabled to. */
    OutputPatch::HoldKind keptKind = OutputPatch::NotHeld;
    const QString showOnlyWhy = patch.tag == Universe::OutputPatchTag && showOnly
            ? showOnlyRefusal(plugin, hasIdentity ? patch.uid : plugin->outputs().value(line), patch.line)
            : QString();
    if (showOnlyWhy.isEmpty() == false)
    {
        keptWhy = showOnlyWhy;
        keptKind = OutputPatch::HeldShowOnly;
        line = -1;
    }
    else if (patch.tag == Universe::OutputPatchTag && line >= 0)
    {
        keptWhy = reservedWhy(plugin, quint32(line), NULL);
        if (keptWhy.isEmpty() == false)
        {
            // By name later, as any pending patch; by number with none.
            keptKind = hasIdentity ? OutputPatch::NotHeld : OutputPatch::HeldReserved;
            line = -1;
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
                    : transmitHoldReason(plugin->outputs().value(line), patch.params, plugin);
            if (line < 0)
            {
                QMutexLocker locker(&m_universeMutex);
                // F3: held with only a line number saved -- by that number.
                const bool byNumber = hasIdentity == false && keptKind != OutputPatch::NotHeld;
                uni->setOutputPatchPending(plugin, byNumber ? QString() : patch.uid, index, patch.line,
                                           patch.network, byNumber);
                if (OutputPatch *kop = uni->outputPatch(index))
                {
                    if (keptKind != OutputPatch::NotHeld)
                        kop->markHold(keptKind, keptWhy);
                    else if (keptWhy.isEmpty() == false)
                        kop->setReconnectHeld(keptWhy);
                }
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
                /* Opened aimed, in one step (review 21, V1: a venue switch,
                   a config import): never on the plugin's default target
                   first, whose closing frame the first parameter's
                   retarget sent there. */
                setOutputPatch(universe, patch.plugin, QString(), quint32(line), false, index, patch.params);
                // Its saved number, for the check once the show is read.
                if (OutputPatch *lop = uni->outputPatch(index))
                    if (lop->isPatched())
                        lop->setLoadedLine(patch.line);
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
    IODeferral io;      // each reopened after the claim (iodeferral.h)
    int reconnected = 0;

    /* Review 17: the patches held off a reserved interface first (F4) --
       one that shared its line before comes back there, as stock -- then
       everything else, which then sees them where they are. */
    for (int phase = 0; phase < 2; phase++)
    {
        for (quint32 i = 0; i < universesCount(); i++)
        {
            Universe *uni = m_universeArray.at(i);

            for (int o = 0; o < uni->outputPatchesCount(); o++)
            {
                OutputPatch *op = uni->outputPatch(o);
                if (op == NULL || op->isPending() == false
                    || (phase == 0) != (op->holdKind() == OutputPatch::HeldReserved))
                    continue;
                QLCIOPlugin *plugin = op->plugin();
                const QMap<QString, QVariant> params = op->getPluginParameters();
                // N1: only a network DMX output while the universe has Show-only fixtures.
                if (uni->showOnlyChannelCount() > 0)
                {
                    const QString refusal = showOnlyRefusal(plugin, op->outputName(), op->pendingLine());
                    if (refusal.isEmpty() == false)
                    {
                        if (op->showOnlyHold() == false)
                            op->markHold(OutputPatch::HeldShowOnly, refusal);
                        continue;
                    }
                }
                QString note;
                int line = -1;
                if (op->pendingByNumber() || op->holdKind() == OutputPatch::HeldReserved)
                {
                    // F3, F4: back by its number.
                    if (plugin != NULL && op->pendingLine() < quint32(plugin->outputs().count()))
                        line = int(op->pendingLine());
                }
                else if (op->fromShowOnlyHold() && op->sharedLine() && plugin != NULL
                         && op->heldOnLine() < quint32(plugin->outputs().count())
                         && plugin->outputs().value(int(op->heldOnLine())) == op->outputName())
                {
                    /* Review 18-19: a former laser universe that shared its
                       line comes back on the very line it was open on, like
                       the others there -- only while that line still names
                       its device; else by name, as stock. */
                    line = int(op->heldOnLine());
                }
                else
                    line = resolveSavedLine(plugin, true, op->outputName(), op->pendingNetwork(),
                                            NetworkRoute::targetFromParameters(params), op->pendingLine());
                // F4: nothing onto an interface a held laser universe is cabled to.
                if (line >= 0)
                {
                    note = reservedWhy(plugin, quint32(line), op);
                    if (note.isEmpty() == false)
                        line = -1;
                }
                /* F4: back by number, never onto a line another patch has --
                   unless it shared that line before it was held (stock). */
                if (line >= 0 && op->holdKind() == OutputPatch::HeldReserved && op->sharedLine() == false
                    && lineUsedByAnother(plugin, quint32(line), op))
                {
                    note = tr("Waiting for %1").arg(op->deviceText());
                    line = -1;
                }
                /* Its lasers gone (N4): as stock, by name -- but a name other
                   lines carry too is not taken while another patch is on it
                   (nobody can tell those interfaces apart). */
                if (line >= 0 && op->fromShowOnlyHold() && op->sharedLine() == false && plugin != NULL
                    && plugin->outputs().count(plugin->outputs().value(line)) > 1
                    && lineUsedByAnother(plugin, quint32(line), op))
                    line = -1;
                if (line < 0)
                {
                    if (note.isEmpty() && op->fromShowOnlyHold())
                        note = tr("Waiting for %1").arg(op->deviceText());
                    // A stale reason goes with the reservation that gave it.
                    op->setReconnectHeld(note);
                    /* Nothing reaches its network. With the gate on and this
                       machine on no rig network at all, that IS being held. */
                    QString why;
                    if (rigGateActive() && isNetworkOutput(op->outputName(), params)
                        && onRigNetwork() == false)
                        why = tr("this machine is not on the rig network");
                    op->setHeldReason(why);
                    continue;
                }
                const QString reason = plugin == NULL ? QString()
                        : transmitHoldReason(plugin->outputs().value(line), params, plugin);
                if (reason.isEmpty() == false)
                {
                    op->setHeldReason(reason);
                    continue;
                }
                qDebug() << "[IOMAP] universe" << i + 1 << "output" << op->outputName()
                         << "reconnected on line" << line;
                QMutexLocker locker(&m_universeMutex);
                uni->resolveOutputPatch(o, quint32(line));
                reconnected++;
            }
        }
    }

    for (quint32 i = 0; i < universesCount(); i++)
    {
        Universe *uni = m_universeArray.at(i);

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
                if (rec.tag == Universe::InputPatchTag && uni->hasInputSlot())
                    continue;   // a controller slot's: applyInputSlots() picks
                QLCIOPlugin *plugin = m_doc->ioPluginCache()->plugin(rec.plugin);
                if (plugin == NULL)
                    continue;
                const bool hasIdentity = rec.uid.isEmpty() == false && rec.uid != KOutputNone;
                int line = hasIdentity
                        ? resolveSavedLine(plugin, rec.tag != Universe::InputPatchTag, rec.uid,
                                           rec.network, NetworkRoute::targetFromParameters(rec.params),
                                           rec.line)
                        : int(rec.line);
                /* Kept by where it goes (loadPatch()): back only once a line
                   here reaches it. */
                const QString target = NetworkRoute::targetFromParameters(rec.params);
                if (hasIdentity == false && rec.tag == Universe::OutputPatchTag && target.isEmpty() == false)
                {
                    const QStringList outs = plugin->outputs();
                    if (line >= 0 && line < outs.count() && NetworkRoute::isAddressIdentity(outs.at(line)))
                        line = NetworkRoute::resolve(outs, QString(), rec.network, target).line;
                    if (line >= outs.count())
                        line = -1;
                }
                /* An input back by its name: never onto a device another
                   patch follows (two pads of one name: that pad stays its
                   universe's); the next line of the name, if one is free
                   (round-5 review). */
                auto usable = [&](int l) {
                    const quint64 id = plugin->inputDeviceId(quint32(l));
                    if (inputDeviceFollowed(plugin, id, NULL) || (id != 0 && rec.notTheDevice.contains(id)))
                        return false;
                    /* Nor one of those re-enumerated where it was since (a
                       sleep, a port's power cycle): still not the pad that
                       went (round-7 review). */
                    for (quint64 here : rec.notTheDevice)
                        if (id != 0 && plugin->inputDeviceReplacement(here) == id)
                            return false;
                    return true;
                };
                if (line >= 0 && rec.tag == Universe::InputPatchTag && usable(line) == false)
                {
                    /* Not a device that was here when it went (a spare of
                       that name, on charge), nor one another patch follows:
                       the pad plugged back (round-6 review). */
                    const QStringList names = plugin->inputs();
                    line = -1;
                    for (int l = 0; l < names.size() && line < 0; ++l)
                        if (names.at(l) == rec.uid && usable(l))
                            line = l;
                }
                if (line < 0)
                    continue;
                /* Reopened on the line chosen here (two lines of one name
                   resolve to the saved line number when it carries it). */
                Universe::OfflinePatch chosen = rec;
                chosen.line = quint32(line);
                uni->removeOfflinePatch(k);
                loadPatch(i, chosen);
                reconnected++;
                if (rec.tag == Universe::InputPatchTag && uni->inputPatch() != NULL)
                    emit inputDeviceConnected(i, rec.plugin, rec.uid.isEmpty()
                                              ? uni->inputPatch()->inputName() : rec.uid);
                again = true;
                break;
            }
        }
    }

    io.finish();
    if (reconnected > 0)
        emit offlinePatchesChanged();

    return reconnected;
}

void InputOutputMap::slotRetryOfflinePatches()
{
    applyInputSlots();
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
    if (uni->hasInputSlot())
        setInputSlot(universe, QList<Universe::InputSlotDevice>());
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

bool InputOutputMap::isLocalHardwareOutput(const QString &lineName,
                                           const QMap<QString, QVariant> &params,
                                           const QLCIOPlugin *plugin) const
{
    if (plugin == NULL)
        return false;
    /* Nothing leaves the computer on these */
    const QString name = plugin->name();
    if (name == QLatin1String("Loopback") || name == QLatin1String("Dummy"))
        return false;
    /* A network plugin (Art-Net, E1.31, OSC, WLED) is judged by where it
       sends: a loopback address stays inside the computer. */
    if (NetworkRoute::isAddressIdentity(lineName) || ipTargets(params).isEmpty() == false)
        return false;
    return true;
}

QString InputOutputMap::transmitHoldReason(const QString &lineName,
                                           const QMap<QString, QVariant> &params,
                                           const QLCIOPlugin *plugin) const
{
    if (transmitGateActive() == false)
        return QString();

    /* Teaching mode: nothing real leaves the computer until the teacher
       allows it (File ▸ Show Settings). */
    if (m_teachingHold && (isNetworkOutput(lineName, params)
                           || isLocalHardwareOutput(lineName, params, plugin)))
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
    /* Review 10 H2: the patches are held first -- each held line is sent
       its closing frame (OutputPatch::closeLine()) -- and only then are
       the plugins told the networks: a plugin's own transmit hold (Art-Net,
       E1.31, WLED) would drop that frame, and the node keep the laser on. */
    const bool active = transmitGateActive();
    int held = 0;
    IODeferral io;      // closing frames and closes after the claim

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
            const QString reason = transmitHoldReason(lineName, params, op->plugin());
            if (reason.isEmpty())
                continue;

            QMutexLocker locker(&m_universeMutex);
            holdOutputPatch(u, o, op->plugin(), lineName, op->output(),
                            NetworkRoute::networkForLine(lineName), reason);
            held++;
        }
    }
    // Each held line's closing frame on the wire before the plugins hold it.
    io.finish();

    pushTransmitNetworks();

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
            const bool network = isNetworkOutput(lineName, params);
            /* Teaching also holds USB and other hardware outputs: counted
               as held (not as network output, which says "looks like a
               real show"). */
            if (network == false
                && (m_teachingHold == false || isLocalHardwareOutput(lineName, params, op->plugin()) == false))
                continue;
            if (network)
                st.networkOutputs++;
            if (op->isHeld())
            {
                st.held++;
                st.reasons << tr("Universe %1 (%2): %3").arg(u + 1).arg(uni->name(), op->heldReason());
            }
        }
    }

    if (st.networkOutputs == 0 && st.held == 0)
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
    /* Moved or gone, each closed and opened after the claim: a reader that
       stopped answering waits up to HIDDevice::kReaderStopMs there. */
    IODeferral io;

    for (quint32 i = 0; i < universesCount(); i++)
    {
        Universe *uni = m_universeArray.at(i);
        InputPatch *ip = uni->inputPatch();
        if (ip == NULL || ip->plugin() != plugin || ip->inputUID().isEmpty())
            continue;
        if (uni->hasInputSlot())
            continue;   // a controller slot: applyInputSlots() follows it

        const QString device = ip->inputUID();
        /* The device it opened, by its id where the plugin gives one: two
           pads of one name are told apart (a Linux joystick has no
           location), so a pad renumbered when one before it went is
           followed, not the first of that name (board Q253's fourth
           review). Gone, or no id: by name, as ever. */
        int line = -1;
        if (const quint64 id = ip->deviceId())
        {
            for (int l = 0; l < lines.size() && line < 0; ++l)
                if (plugin->inputDeviceId(quint32(l)) == id)
                    line = l;
            /* Its device is gone. Re-enumerated where it was (the plugin
               says which device replaced it): follow that one -- a pad
               lost and a new one, on its own universe (the engine sees the
               id change). Never by name: another pad of that name on the
               line, a spare on charge, is not the pad that went (round-6
               review). Else gone: kept offline, and the pad plugged back
               reopens it. */
            if (line < 0)
                if (const quint64 again = plugin->inputDeviceReplacement(id))
                    for (int l = 0; l < lines.size() && line < 0; ++l)
                        if (plugin->inputDeviceId(quint32(l)) == again)
                            line = l;
        }
        else
            line = lines.indexOf(device);
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
        // The devices here as it went: none of them is the one that went.
        for (int l = 0; l < lines.size(); ++l)
            if (const quint64 here = plugin->inputDeviceId(quint32(l)))
                rec.notTheDevice.insert(here);
        rec.profile = ip->profileName();
        rec.hidProfile = ip->hidProfileName();
        rec.params = ip->getPluginParameters();
        setInputPatch(i, KInputNone, QString(), QLCIOPlugin::invalidLine());
        uni->addOfflinePatch(rec);
        kept = true;
        emit inputDeviceDisconnected(i, rec.plugin, device);
    }
    io.finish();

    if (kept)
        emit offlinePatchesChanged();
}

bool InputOutputMap::hasFreeInputDevice(QLCIOPlugin *plugin, const QString &name) const
{
    if (plugin == NULL)
        return false;
    const QStringList lines = plugin->inputs();
    for (int l = 0; l < lines.size(); ++l)
    {
        if (lines.at(l) != name)
            continue;
        const quint64 id = plugin->inputDeviceId(quint32(l));
        bool taken = false;
        for (Universe *uni : m_universeArray)
        {
            const InputPatch *p = uni->inputPatch();
            if (p != NULL && p->plugin() == plugin && (id != 0 ? p->deviceId() == id : p->input() == quint32(l)))
                taken = true;
        }
        if (taken == false)
            return true;
    }
    return false;
}

bool InputOutputMap::inputDeviceFollowed(const QLCIOPlugin *plugin, quint64 id, const InputPatch *except) const
{
    if (id == 0)
        return false;
    for (Universe *uni : m_universeArray)
    {
        const InputPatch *p = uni->inputPatch();
        if (p != NULL && p != except && p->plugin() == plugin && p->deviceId() == id)
            return true;
    }
    return false;
}

QString InputOutputMap::networkOnlyText(const QString &device)
{
    // N1's words, everywhere it refuses or holds.
    return tr("Lasers, foggers and flame need a network output in this version — Art-Net, sACN "
              "(E1.31) or WLED: %1 can't send to them. Patch this universe to one of those, or take "
              "the Show-only rule off.").arg(device);
}

QString InputOutputMap::reservedText(const QString &device, const QString &universe, const QString &output)
{
    // F4's words, everywhere it refuses or holds.
    return tr("Kept off %1: it is cabled to %2's lasers, which need a network output. Remove the %3 "
              "output from %2 to free it.").arg(device, universe, output);
}

QString InputOutputMap::showOnlyRefusal(QLCIOPlugin *plugin, const QString &lineName, quint32 line)
{
    /* N1: a universe with Show-only fixtures sends only over Art-Net, sACN
       or WLED (review 16) -- to any target, this computer's own (a
       visualiser) too. */
    if (plugin == NULL || plugin->isNetworkDmxOutput())
        return QString();
    return networkOnlyText(OutputPatch::deviceText(plugin, line, lineName));
}

QString InputOutputMap::reservedWhy(const QLCIOPlugin *plugin, quint32 line, const OutputPatch *except) const
{
    /* F4: the interface a universe held by N2/N3 was held off -- by name,
       every line of that name; by number when it had none (F3). */
    if (plugin == NULL || plugin->isNetworkDmxOutput() || line == QLCIOPlugin::invalidLine())
        return QString();
    const QString name = const_cast<QLCIOPlugin *>(plugin)->outputs().value(int(line));
    for (quint32 u = 0; u < universesCount(); u++)
    {
        const Universe *uni = m_universeArray.at(u);
        for (int o = 0; o < uni->outputPatchesCount(); o++)
        {
            const OutputPatch *op = uni->outputPatch(o);
            /* Review 17: only while its universe still has Show-only
               fixtures -- never one with none left. */
            if (op == NULL || op == except || op->plugin() != plugin || op->showOnlyHold() == false
                || op->isPending() == false || uni->showOnlyChannelCount() == 0)
                continue;
            const bool mine = op->pendingByNumber() ? op->pendingLine() == line
                                                    : (name.isEmpty() == false && name == op->outputName());
            if (mine)
                return reservedText(op->deviceText(), uni->name(),
                                    plugin->linesAreHardware() ? tr("USB") : plugin->name());
        }
    }
    return QString();
}

QString InputOutputMap::outputPatchRefusal(quint32 universe, const QString &pluginName, quint32 line,
                                           int index) const
{
    if (universe >= universesCount() || m_doc == NULL || m_doc->ioPluginCache() == NULL)
        return QString();
    QLCIOPlugin *plugin = m_doc->ioPluginCache()->plugin(pluginName);
    if (plugin == NULL || line == QLCIOPlugin::invalidLine())
        return QString();
    const Universe *uni = m_universeArray.at(universe);
    if (uni->showOnlyChannelCount() > 0)
    {
        const QString refusal = showOnlyRefusal(plugin, plugin->outputs().value(int(line)), line);
        if (refusal.isEmpty() == false)
            return refusal;
    }
    return reservedWhy(plugin, line, index >= 0 && index < uni->outputPatchesCount()
                                     ? uni->outputPatch(index) : NULL);
}

void InputOutputMap::watchShowOnly(Universe *uni)
{
    /* N2: the first Show-only fixture on a universe holds its hardware
       patches -- after every mark of that change (beginShowOnlyMarks()),
       and not during a show load (recheckPatchesAfterLoad(), N3). Direct,
       unclaimed: a mark can come from code that holds the universe claim;
       the hold takes the frame lock only. */
    connect(uni, &Universe::showOnlyStarted, this, [this, uni]() {
        if (m_showOnlyMarks > 0)
            m_showOnlyMarked.insert(uni->id());
        else if (m_loadHold == false)
            holdShowOnlyOutputs(uni, false);
    }, Qt::DirectConnection);
    /* N4: none left on it -- at once, its holds are plain pending (by
       name, H5) and reserve nothing any more (review 17); they come back on
       the retry, queued: the rules can be rebuilt under the universe claim,
       which reopening takes. */
    connect(uni, &Universe::showOnlyEnded, this, [this, uni]() {
        for (int o = 0; o < uni->outputPatchesCount(); o++)
            if (OutputPatch *op = uni->outputPatch(o))
                if (op->showOnlyHold())
                    op->releaseShowOnlyHold();
        QMetaObject::invokeMethod(this, [this]() {
            if (hasOfflinePatches())
                retryOfflinePatches();
        }, Qt::QueuedConnection);
    }, Qt::DirectConnection);
}

void InputOutputMap::beginShowOnlyMarks()
{
    m_showOnlyMarks++;
}

void InputOutputMap::endShowOnlyMarks()
{
    if (m_showOnlyMarks == 0 || --m_showOnlyMarks > 0)
        return;
    const QSet<quint32> marked = m_showOnlyMarked;
    m_showOnlyMarked.clear();
    if (m_loadHold)
        return;     // N3 checks them once the show is read
    for (quint32 u : marked)
        if (u < universesCount() && m_universeArray.at(u)->showOnlyChannelCount() > 0)
            holdShowOnlyOutputs(m_universeArray.at(u), false);
}

void InputOutputMap::holdShowOnlyOutputs(Universe *universe, bool fromLoad)
{
    /* N2/N3: each output of a universe with Show-only fixtures that is not
       a network DMX output is held. N2 waits on the line it was open on;
       N3 on its saved one. On a line other patches share too (review 17,
       at runtime as on load): they are held first (F4, the plugin only
       forgets them), then this universe's closing frame -- its whole
       frame, the Show-only channels at safe -- goes out and is flushed,
       and only then is the line closed. */
    bool held = false;
    for (int o = 0; o < universe->outputPatchesCount(); o++)
    {
        OutputPatch *op = universe->outputPatch(o);
        if (op == NULL || op->plugin() == NULL)
            continue;
        QLCIOPlugin *plugin = op->plugin();
        if (op->isPatched())
        {
            const quint32 line = op->output();
            const QString why = showOnlyRefusal(plugin, plugin->outputs().value(int(line)), line);
            if (why.isEmpty())
                continue;
            /* Only a line that is not a network DMX output gets here (N1:
               showOnlyRefusal() is the plugin allow-list). Every other
               universe on it is held first (F4, only forgotten there). */
            bool shared = false;
            const QString kept = reservedText(op->deviceText(), universe->name(),
                                              plugin->linesAreHardware() ? tr("USB") : plugin->name());
            for (quint32 u = 0; u < universesCount(); u++)
            {
                Universe *other = m_universeArray.at(u);
                for (int k = 0; k < other->outputPatchesCount(); k++)
                {
                    OutputPatch *q = other->outputPatch(k);
                    if (q == NULL || q == op || q->plugin() != plugin || q->isPatched() == false
                        || q->output() != line)
                        continue;
                    q->hold(OutputPatch::HeldReserved, kept, false, false, UINT_MAX, true);
                    shared = true;
                }
            }
            /* Then this universe: its closing frame, flushed, then closed.
               Shared before: it comes back there by number once its lasers
               are gone. */
            op->hold(OutputPatch::HeldShowOnly, why, true, true, fromLoad ? op->loadedLine() : UINT_MAX, shared);
            held = true;
        }
        else if (op->isPending() && op->showOnlyHold() == false)
        {
            const QString why = showOnlyRefusal(plugin, op->outputName(), op->pendingLine());
            if (why.isEmpty())
                continue;
            op->markHold(OutputPatch::HeldShowOnly, why);
            held = true;
        }
    }
    if (held)
        emit offlinePatchesChanged();
}

void InputOutputMap::setLoadHold(bool hold)
{
    m_loadHold = hold;
    IODeferral io;
    QMutexLocker locker(&m_universeMutex);
    foreach (Universe *uni, m_universeArray)
        uni->setLoadHold(hold);
}

int InputOutputMap::recheckPatchesAfterLoad()
{
    /* N3: the I/O map is read before the fixtures, so while a show loads
       its patches resolve without knowing which universes have Show-only
       fixtures. Once it is read -- before the first frame goes out
       (setLoadHold()) -- each of their outputs that is not a network DMX
       one is held, waiting on its saved line; then (F4) any other output
       the load opened onto an interface those reserve. */
    int held = 0;
    for (quint32 u = 0; u < universesCount(); u++)
    {
        Universe *uni = m_universeArray.at(u);
        if (uni->showOnlyChannelCount() == 0)
            continue;
        for (int o = 0; o < uni->outputPatchesCount(); o++)
        {
            const OutputPatch *op = uni->outputPatch(o);
            if (op != NULL && op->plugin() != NULL && op->plugin()->isNetworkDmxOutput() == false
                && op->showOnlyHold() == false)
                held++;
        }
        holdShowOnlyOutputs(uni, true);
    }
    for (quint32 u = 0; u < universesCount(); u++)
    {
        Universe *uni = m_universeArray.at(u);
        if (uni->showOnlyChannelCount() > 0)
            continue;
        for (int o = 0; o < uni->outputPatchesCount(); o++)
        {
            OutputPatch *op = uni->outputPatch(o);
            if (op == NULL || op->isPatched() == false)
                continue;
            const QString why = reservedWhy(op->plugin(), op->output(), op);
            if (why.isEmpty())
                continue;
            const bool shared = lineUsedByAnother(op->plugin(), op->output(), op);
            op->hold(OutputPatch::HeldReserved, why, shared == false, false, UINT_MAX, shared);
            held++;
        }
    }
    if (held > 0)
        emit offlinePatchesChanged();
    return held;
}

bool InputOutputMap::lineUsedByAnother(const QLCIOPlugin *plugin, quint32 line,
                                       const OutputPatch *except) const
{
    for (quint32 u = 0; u < universesCount(); u++)
    {
        const Universe *uni = m_universeArray.at(u);
        for (int o = 0; o < uni->outputPatchesCount(); o++)
        {
            const OutputPatch *op = uni->outputPatch(o);
            if (op != NULL && op != except && op->plugin() == plugin
                && op->isPatched() && op->output() == line)
                return true;
        }
    }
    return false;
}

void InputOutputMap::slotPluginConfigurationChanged(QLCIOPlugin* plugin)
{
    /* A device input (game controller, MIDI surface) is a line that comes
       and goes: follow it by name before reopening anything by number. */
    if (plugin->linesAreHardware())
    {
        followInputDevices(plugin);
        applyInputSlots();
    }

    /* Every patch reconnects by line number, as stock does: a universe
       with Show-only fixtures has none on a hardware line (N1), and nothing
       else opens onto the interface one was held off (F4). The bookkeeping
       under the claim, every close and open after it (IODeferral): a
       device that does not answer while closed or opened -- a reader
       stuck in a call, a DMX interface on a wedged USB hub -- never stops
       the other universes' DMX output. */
    IODeferral io;
    QMutexLocker locker(&m_universeMutex);
    bool success = true;
    bool keptOff = false;
    QList<InputPatch *> inputs;
    for (quint32 i = 0; i < universesCount(); i++)
    {
        Universe *universe = m_universeArray.at(i);
        for (int oi = 0; oi < universe->outputPatchesCount(); oi++)
        {
            OutputPatch* op = universe->outputPatch(oi);
            if (op == NULL || op->plugin() != plugin)
                continue;
            /* F4: not by number onto an interface a held laser universe is
               cabled to -- held, no frame of it there; back by number once
               that line is not reserved (retryOfflinePatches()). */
            const QString why = op->isPatched() ? reservedWhy(plugin, op->output(), op) : QString();
            if (why.isEmpty() == false)
            {
                const bool shared = lineUsedByAnother(plugin, op->output(), op);
                op->hold(OutputPatch::HeldReserved, why, shared == false, false, UINT_MAX, shared);
                keptOff = true;
                continue;
            }
            /*success = */ op->reconnect();
        }

        InputPatch* ip = universe->inputPatch();
        if (ip != NULL && ip->plugin() == plugin)
            inputs << ip;

        OutputPatch* fp = universe->feedbackPatch();
        if (fp != NULL && fp->plugin() == plugin)
            /*success = */ fp->reconnect();
    }
    locker.unlock();

    /* Inputs after the lock: nothing the MasterTimer tick reads, and a
       controller's close can wait on its reader (a pad that stopped
       answering) -- never with every universe's DMX output held. Only
       this thread changes patches, so each is still there. */
    for (InputPatch *ip : std::as_const(inputs))
        /*success = */ ip->reconnect();
    io.finish();

    /* The device a kept patch waits for may be what just appeared: reopen
       it now rather than at the next retry tick. */
    if (plugin->linesAreHardware() && hasOfflinePatches())
        retryOfflinePatches();
    if (keptOff)
        emit offlinePatchesChanged();

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
