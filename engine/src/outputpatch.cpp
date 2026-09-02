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
    , m_paused(false)
    , m_blackout(false)
{
}

OutputPatch::OutputPatch(quint32 universe, QObject* parent)
    : QObject(parent)
    , m_plugin(NULL)
    , m_pluginLine(QLCIOPlugin::invalidLine())
    , m_universe(universe)
    , m_paused(false)
    , m_blackout(false)
{
}

OutputPatch::~OutputPatch()
{
    if (m_plugin != NULL)
        m_plugin->closeOutput(m_pluginLine, m_universe);
}

/****************************************************************************
 * Plugin & Output
 ****************************************************************************/

bool OutputPatch::set(QLCIOPlugin* plugin, quint32 output)
{
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
        m_plugin->closeOutput(m_pluginLine, m_universe);

    // A concrete set() always supersedes whatever this patch was pending on.
    m_pendingUID.clear();

    m_plugin = plugin;
    m_pluginLine = output;

    if (m_plugin != NULL)
    {
        emit pluginNameChanged();
        if (m_pluginLine != QLCIOPlugin::invalidLine())
            emit outputNameChanged();
    }

    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
        return m_plugin->openOutput(m_pluginLine, m_universe);

    return false;
}

bool OutputPatch::reconnect()
{
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
    {
        m_plugin->closeOutput(m_pluginLine, m_universe);
#if defined(WIN32) || defined(Q_OS_WIN)
        Sleep(GRACE_MS);
#else
        usleep(GRACE_MS * 1000);
#endif
        bool ret = m_plugin->openOutput(m_pluginLine, m_universe);
        if (ret == true)
        {
            QMap<QString, QVariant>::iterator it = m_parametersCache.begin();
            for (; it != m_parametersCache.end(); it++)
                m_plugin->setParameter(m_universe, m_pluginLine, QLCIOPlugin::Output, it.key(), it.value());
        }
        return ret;
    }
    return false;
}

void OutputPatch::setPending(QLCIOPlugin *plugin, const QString &uid)
{
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
        m_plugin->closeOutput(m_pluginLine, m_universe);

    m_plugin = plugin;
    m_pluginLine = QLCIOPlugin::invalidLine();
    m_pendingUID = uid;

    if (m_plugin != NULL)
        emit pluginNameChanged();
    // outputName() now returns m_pendingUID rather than "None" -- same
    // property, same signal, the identity just isn't open.
    emit outputNameChanged();
}

bool OutputPatch::isPending() const
{
    return m_pluginLine == QLCIOPlugin::invalidLine() && m_pendingUID.isEmpty() == false;
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

void OutputPatch::setPluginParameter(QString prop, QVariant value)
{
    m_parametersCache[prop] = value;
    if (m_plugin != NULL)
        m_plugin->setParameter(m_universe, m_pluginLine, QLCIOPlugin::Output, prop, value);
}

void OutputPatch::unSetPluginParameter(QString prop)
{
    m_parametersCache.remove(prop);
    if (m_plugin != NULL)
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
