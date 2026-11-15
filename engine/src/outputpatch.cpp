/*
  Q Light Controller
  outputpatch.cpp

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

#if defined(WIN32) || defined(Q_OS_WIN)
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#else
#   include <unistd.h>
#endif

#include <QRecursiveMutex>
#include <memory>
#include <QMutexLocker>
#include <QDebug>

#include "qlcioplugin.h"
#include "outputpatch.h"

#define GRACE_MS 1

/*****************************************************************************
 * Initialization
 *****************************************************************************/

OutputPatch::OutputPatch(QObject* parent)
    : QObject(parent)
    , m_plugin(NULL)
    , m_pluginLine(QLCIOPlugin::invalidLine())
    , m_universe(UINT_MAX)
    , m_pendingLine(UINT_MAX)
    , m_paused(false)
    , m_blackout(false)
{
}

OutputPatch::OutputPatch(quint32 universe, QObject* parent)
    : QObject(parent)
    , m_plugin(NULL)
    , m_pluginLine(QLCIOPlugin::invalidLine())
    , m_universe(universe)
    , m_pendingLine(UINT_MAX)
    , m_paused(false)
    , m_blackout(false)
{
}

OutputPatch::~OutputPatch()
{
    /* Review 20: another open patch of this universe on this line goes on
       sending it there -- (line, universe) is not closed under it. */
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
    {
        if (siblingOnLine(m_pluginLine) == false)
            closeLine();
    }
    else if (m_plugin != NULL)
    {
        m_plugin->closeOutput(m_pluginLine, m_universe);
        /* Held by N2/N3 and now unpatched (review 12 F6): the plugin
           forgets this universe on the line it had, whoever else is open
           there. */
        if (m_holdKind != NotHeld && m_pendingLine != UINT_MAX && siblingOnLine(m_pendingLine) == false)
            m_plugin->forgetOutput(m_pendingLine, m_universe);
    }
}

void OutputPatch::setClosingFrame(const std::function<bool(QByteArray &, bool)> &source)
{
    m_closingFrame = source;
}

void OutputPatch::sendClosingFrame()
{
    if (m_plugin == NULL || m_pluginLine == QLCIOPlugin::invalidLine() || !m_closingFrame)
        return;
    /* Show only (S0): a node or an interface keeps the last frame it was
       given. The line's last one has every Show-only channel at its safe
       value -- paused or not -- or a laser stays on where it was, and All
       Off, Blackout, a cue letting go never reach it again. On the wire,
       not only handed over: a plugin that sends from its own thread or
       timer sends it now (review 10). */
    QByteArray frame;
    if (m_closingFrame(frame, m_blackout))
    {
        m_plugin->writeUniverse(m_universe, m_pluginLine, frame, true);
        m_plugin->flushOutput(m_pluginLine, m_universe);
    }
}

void OutputPatch::closeLine()
{
    if (m_plugin == NULL || m_pluginLine == QLCIOPlugin::invalidLine())
        return;
    std::unique_ptr<QMutexLocker<QRecursiveMutex> > locker;
    if (m_frameLock != nullptr)
        locker.reset(new QMutexLocker<QRecursiveMutex>(m_frameLock));
    sendClosingFrame();
    m_plugin->closeOutput(m_pluginLine, m_universe);
}

/****************************************************************************
 * Plugin & Output
 ****************************************************************************/

bool OutputPatch::set(QLCIOPlugin* plugin, quint32 output)
{
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
    {
        // The same line again (a re-set of what is open): no closing frame.
        if (m_plugin == plugin && m_pluginLine == output)
            m_plugin->closeOutput(m_pluginLine, m_universe);
        /* Review 20: moved away from a line another open patch of this
           universe still uses -- left open for it. */
        else if (siblingOnLine(m_pluginLine) == false)
            closeLine();
    }

    // A concrete set() always supersedes whatever this patch was pending on.
    m_pendingUID.clear();
    m_pendingLine = UINT_MAX;
    m_pendingNetwork.clear();
    m_heldReason.clear();

    m_plugin = plugin;
    m_pluginLine = output;

    if (m_plugin != NULL)
    {
        emit pluginNameChanged();
        if (m_pluginLine != QLCIOPlugin::invalidLine())
            emit outputNameChanged();
    }

    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
    {
        const bool opened = m_plugin->openOutput(m_pluginLine, m_universe);
        m_freshLine.store(opened, std::memory_order_relaxed);
        // The device it was patched to: a hold keeps it pending on that name.
        m_lineName = m_plugin->outputs().value(int(m_pluginLine));
        m_reconnectHeld.clear();
        m_holdKind = NotHeld;
        m_pendingByNumber = false;
        m_fromShowOnlyHold = false;
        m_sharedLine = false;
        m_loadedLine = UINT_MAX;
        return opened;
    }

    return false;
}

bool OutputPatch::set(QLCIOPlugin *plugin, quint32 output, const QMap<QString, QVariant> &params)
{
    /* Review 12 F7: opened with its target in one step -- the parameters
       given to the plugin before any frame (the caller holds the frame
       lock), never to its default target first, and no closing frame: it
       was sending nowhere. */
    for (auto it = params.constBegin(); it != params.constEnd(); ++it)
        m_parametersCache[it.key()] = it.value();
    const bool opened = set(plugin, output);
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
        for (auto it = params.constBegin(); it != params.constEnd(); ++it)
            m_plugin->setParameter(m_universe, m_pluginLine, QLCIOPlugin::Output, it.key(), it.value());
    return opened;
}

bool OutputPatch::reconnect()
{
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
    {
        // Closed and opened between two frames (L1): no frame to a line half-way.
        std::unique_ptr<QMutexLocker<QRecursiveMutex> > locker;
        if (m_frameLock != nullptr)
            locker.reset(new QMutexLocker<QRecursiveMutex>(m_frameLock));

        /* By number, as it always was (review 11: following a name broke an
           ordinary unplug and replug). A universe with Show-only channels
           is never on a hardware line (N1, outputpatch.h). */
        m_plugin->closeOutput(m_pluginLine, m_universe);
#if defined(WIN32) || defined(Q_OS_WIN)
        Sleep(GRACE_MS);
#else
        usleep(GRACE_MS * 1000);
#endif
        bool ret = m_plugin->openOutput(m_pluginLine, m_universe);
        if (ret == true)
        {
            m_freshLine.store(true, std::memory_order_relaxed);
            QMap<QString, QVariant>::iterator it = m_parametersCache.begin();
            for (; it != m_parametersCache.end(); it++)
                m_plugin->setParameter(m_universe, m_pluginLine, QLCIOPlugin::Output, it.key(), it.value());
        }
        return ret;
    }
    return false;
}

void OutputPatch::hold(HoldKind kind, const QString &why, bool closeTheLine, bool sendClosing,
                       quint32 waitLine, bool sharedLine)
{
    if (m_plugin == NULL || m_pluginLine == QLCIOPlugin::invalidLine())
        return;
    std::unique_ptr<QMutexLocker<QRecursiveMutex> > locker;
    if (m_frameLock != nullptr)
        locker.reset(new QMutexLocker<QRecursiveMutex>(m_frameLock));
    m_reconnectHeld = why;
    m_holdKind = kind;
    m_sharedLine = sharedLine;
    m_fromShowOnlyHold = false;
    m_heldOnLine = m_pluginLine;
    qWarning() << "[OutputPatch]" << why;
    /* N2: its frame, the whole Show-only mask at safe, on the wire before
       the line stops receiving this universe. @p closeTheLine false: the
       plugin only forgets this universe there (review 12 F6) -- the patch
       that holds the line closes it (review 17). */
    /* Review 19 (A1): another open patch of this universe on this line
       goes on sending it there -- neither closed nor forgotten. */
    if (siblingOnLine(m_pluginLine) == false)
    {
        if (closeTheLine)
        {
            if (sendClosing)
                sendClosingFrame();
            m_plugin->closeOutput(m_pluginLine, m_universe);
        }
        else
            m_plugin->forgetOutput(m_pluginLine, m_universe);
    }
    // F3: no name (a dangling line): pending on its number only.
    m_pendingUID = m_lineName;
    m_pendingByNumber = m_lineName.isEmpty();
    m_pendingLine = waitLine != UINT_MAX ? waitLine : m_pluginLine;
    m_pendingNetwork.clear();
    m_pluginLine = QLCIOPlugin::invalidLine();
    emit outputNameChanged();
}

QString OutputPatch::deviceText(const QLCIOPlugin *plugin, quint32 line, const QString &name)
{
    if (name.isEmpty() == false && name != KOutputNone)
        return name;
    const QString of = plugin != NULL ? plugin->name() : QString();
    if (line == UINT_MAX || line == QLCIOPlugin::invalidLine())
        return tr("an unknown line of %1").arg(of);
    return tr("line %1 of %2").arg(line + 1).arg(of);
}

QString OutputPatch::deviceText() const
{
    if (isPatched())
        return deviceText(m_plugin, m_pluginLine, m_plugin->outputs().value(int(m_pluginLine)));
    return deviceText(m_plugin, m_pendingLine, m_pendingUID);
}

void OutputPatch::setPending(QLCIOPlugin *plugin, const QString &uid,
                             quint32 savedLine, const QString &network)
{
    /* Held: the line stops receiving this universe -- its closing frame
       first; unless another open patch of this universe uses it (review 20). */
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine()
        && siblingOnLine(m_pluginLine) == false)
        closeLine();

    m_plugin = plugin;
    m_pluginLine = QLCIOPlugin::invalidLine();
    m_reconnectHeld.clear();
    m_holdKind = NotHeld;
    m_pendingByNumber = false;
    m_fromShowOnlyHold = false;
    m_sharedLine = false;
    m_pendingUID = uid;
    m_pendingLine = savedLine;
    m_pendingNetwork = network;

    if (m_plugin != NULL)
        emit pluginNameChanged();
    // outputName() now returns m_pendingUID rather than "None" -- same
    // property, same signal, the identity just isn't open.
    emit outputNameChanged();
}

bool OutputPatch::resolvePending(quint32 line)
{
    if (isPending() == false || m_plugin == NULL)
        return false;

    const QMap<QString, QVariant> params = m_parametersCache;
    const bool opened = set(m_plugin, line);
    QMapIterator<QString, QVariant> it(params);
    while (it.hasNext())
    {
        it.next();
        setPluginParameter(it.key(), it.value());
    }
    return opened;
}

bool OutputPatch::isPending() const
{
    if (m_pluginLine != QLCIOPlugin::invalidLine())
        return false;
    // F3: a held patch with no name waits on its line number.
    return m_pendingUID.isEmpty() == false || (m_pendingByNumber && m_pendingLine != UINT_MAX);
}

QString OutputPatch::pluginName() const
{
    if (m_plugin != NULL)
        return m_plugin->name();
    else
        return KOutputNone;
}

QLCIOPlugin* OutputPatch::plugin() const
{
    return m_plugin;
}

QString OutputPatch::outputName() const
{
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine() &&
        m_pluginLine < quint32(m_plugin->outputs().size()))
    {
        return m_plugin->outputs()[m_pluginLine];
    }
    else if (m_pendingUID.isEmpty() == false)
    {
        /* Unresolved, not unpatched -- saveXML() reads this to write
           LineUID back out, so a pending patch round-trips to the same
           identity rather than being written out as "None" and losing the
           mapping for good the next time this file is saved. */
        return m_pendingUID;
    }
    else
    {
        return KOutputNone;
    }
}

quint32 OutputPatch::output() const
{
    return m_pluginLine;
}

bool OutputPatch::isPatched() const
{
    return output() != QLCIOPlugin::invalidLine() && m_plugin != NULL;
}

bool OutputPatch::sameParameterValue(const QVariant &a, const QVariant &b)
{
    // "1" and 1 are the same address (review 12 F7: wledAddress).
    if (a == b)
        return true;
    return a.isValid() && b.isValid() && a.toString() == b.toString();
}

bool OutputPatch::isTargetParameter(const QString &prop)
{
    static const QStringList exact = {
        QStringLiteral("outputUni"),                                    // Art-Net
        QStringLiteral("universe"), QStringLiteral("multicast"),       // E1.31
        QStringLiteral("mcastIP"), QStringLiteral("mcastFullIP"),
        QStringLiteral("ucastIP"), QStringLiteral("ucastPort"),
        QStringLiteral("outputPort"),                                   // OSC
    };
    // Art-Net / E1.31 / WLED "outputIP", and a WLED backup's "outputIP2".. and window
    return exact.contains(prop) || prop.startsWith(QLatin1String("outputIP"))
           || prop.startsWith(QLatin1String("wledAddress")) || prop.startsWith(QLatin1String("wledChannels"));
}

void OutputPatch::setPluginParameters(const QMap<QString, QVariant> &values, const QStringList &unset)
{
    std::unique_ptr<QMutexLocker<QRecursiveMutex> > locker;
    if (m_frameLock != nullptr)
        locker.reset(new QMutexLocker<QRecursiveMutex>(m_frameLock));
    bool retarget = false;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it)
        if (isTargetParameter(it.key())
            && sameParameterValue(m_parametersCache.value(it.key()), it.value()) == false)
            retarget = true;
    foreach (const QString &prop, unset)
        if (isTargetParameter(prop) && m_parametersCache.contains(prop))
            retarget = true;
    // One closing frame, to the old target only: then every change at once.
    if (retarget)
        sendClosingFrame();
    for (auto it = values.constBegin(); it != values.constEnd(); ++it)
    {
        m_parametersCache[it.key()] = it.value();
        if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
            m_plugin->setParameter(m_universe, m_pluginLine, QLCIOPlugin::Output, it.key(), it.value());
    }
    foreach (const QString &prop, unset)
    {
        if (m_parametersCache.remove(prop) == 0)
            continue;
        if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
            m_plugin->unSetParameter(m_universe, m_pluginLine, QLCIOPlugin::Output, prop);
    }
}

void OutputPatch::setPluginParameter(QString prop, QVariant value)
{
    /* Aimed elsewhere (review 10 M1): the old node, port or universe stops
       receiving this universe -- its closing frame first, then the move,
       with no frame between (the frame lock). */
    if (isTargetParameter(prop) && m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine()
        && (m_parametersCache.contains(prop) == false
            || sameParameterValue(m_parametersCache.value(prop), value) == false))
    {
        setPluginParameters({ { prop, value } });
        return;
    }
    m_parametersCache[prop] = value;
    // Nothing to tell the plugin while pending: there is no line yet.
    // resolvePending() replays the cache once there is.
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
        m_plugin->setParameter(m_universe, m_pluginLine, QLCIOPlugin::Output, prop, value);
}

void OutputPatch::unSetPluginParameter(QString prop)
{
    // Back to the plugin's default target (broadcast): a retarget too.
    std::unique_ptr<QMutexLocker<QRecursiveMutex> > locker;
    if (isTargetParameter(prop) && m_parametersCache.contains(prop)
        && m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
    {
        if (m_frameLock != nullptr)
            locker.reset(new QMutexLocker<QRecursiveMutex>(m_frameLock));
        sendClosingFrame();
    }
    m_parametersCache.remove(prop);
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
        m_plugin->unSetParameter(m_universe, m_pluginLine, QLCIOPlugin::Output, prop);
}

QMap<QString, QVariant> OutputPatch::getPluginParameters()
{
    /* A pending patch (see setPending()) has no real line yet, so there is
       nothing for the PLUGIN to report -- it only knows about lines that
       actually exist. What this patch would send once it resolves is
       exactly what setPluginParameter() already recorded locally: the same
       cache reconnect() replays onto the plugin once a real line exists.
       Reading that instead, rather than asking a plugin that has nothing
       to say yet, is what keeps a pending ArtNet patch's target IP and port
       visible instead of reading back as broadcast/untargeted. */
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
        return m_plugin->getParameters(m_universe, m_pluginLine, QLCIOPlugin::Output);

    return m_parametersCache;
}

/*****************************************************************************
 * Value dump
 *****************************************************************************/
bool OutputPatch::paused() const
{
    return m_paused;
}

void OutputPatch::setPaused(bool paused)
{
    if (m_paused == paused)
        return;

    m_paused = paused;

    if (m_pauseBuffer.length())
        m_pauseBuffer.clear();

    emit pausedChanged(m_paused);
}

bool OutputPatch::blackout() const
{
    return m_blackout;
}

void OutputPatch::setBlackout(bool blackout)
{
    if (m_blackout == blackout)
        return;

    m_blackout = blackout;
    emit blackoutChanged(m_blackout);
}

void OutputPatch::dump(quint32 universe, const QByteArray& data, bool dataChanged)
{
    /* Don't do anything if there is no plugin and/or output line. */
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
    {
        // A line just opened (released from a hold, re-patched): its first
        // frame is sent as changed, so it carries the current build.
        if (Q_UNLIKELY(m_freshLine.load(std::memory_order_relaxed)))
        {
            dataChanged = true;
            m_freshLine.store(false, std::memory_order_relaxed);
        }
        if (m_paused)
        {
            if (m_pauseBuffer.isNull())
                m_pauseBuffer.append(data);

            m_plugin->writeUniverse(universe, m_pluginLine, m_pauseBuffer, dataChanged);
        }
        else
        {
            m_plugin->writeUniverse(universe, m_pluginLine, data, dataChanged);
        }
    }
}
