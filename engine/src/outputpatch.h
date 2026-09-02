/*
  Q Light Controller
  outputpatch.h

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

#ifndef OUTPUTPATCH_H
#define OUTPUTPATCH_H

#include <QObject>
#include <QMap>

class QLCIOPlugin;

/** @addtogroup engine Engine
 * @{
 */

#define KOutputNone QObject::tr("None")

#define KXMLQLCOutputPatch          QStringLiteral("Patch")
#define KXMLQLCOutputPatchUniverse  QStringLiteral("Universe")
#define KXMLQLCOutputPatchPlugin    QStringLiteral("Plugin")
#define KXMLQLCOutputPatchOutput    QStringLiteral("Output")

class OutputPatch : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(OutputPatch)

    Q_PROPERTY(QString outputName READ outputName NOTIFY outputNameChanged)
    Q_PROPERTY(QString pluginName READ pluginName NOTIFY pluginNameChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(bool blackout READ blackout WRITE setBlackout NOTIFY blackoutChanged)

    /********************************************************************
     * Initialization
     ********************************************************************/
public:
    OutputPatch(QObject* parent = 0);
    OutputPatch(quint32 universe, QObject* parent = 0);
    virtual ~OutputPatch();

    /********************************************************************
     * Plugin & output
     ********************************************************************/
public:
    /**
     * Set the plugin to use and the plugin line number to output data on
     */
    bool set(QLCIOPlugin* plugin, quint32 output);

    /**
     * Record a patch to @p plugin whose saved interface identity (@p uid --
     * an ArtNet interface's IP, for instance) does not currently match any
     * line the plugin offers -- a workspace opened on a machine that isn't
     * on the network it was built for, most commonly. Keeps the plugin
     * association and @p uid so the patch round-trips through saveXML()
     * unchanged and re-resolves correctly back on a machine that DOES have
     * that interface, but opens nothing: isPatched() is false and no
     * hardware line is ever opened while pending. See isPending().
     */
    void setPending(QLCIOPlugin *plugin, const QString &uid);

    /** True while this patch remembers an interface identity that did not
     *  resolve to any line the plugin currently offers -- see setPending().
     *  A pending patch is deliberately inert: nothing is opened and nothing
     *  is sent, but the mapping itself is not lost. */
    bool isPending() const;

    /**
     * If a valid plugin and line have been set, close
     * the output line and re-open it again
     */
    bool reconnect();

    /** The plugin instance that has been assigned to a patch */
    QLCIOPlugin* plugin() const;

    /** Friendly name of the plugin assigned to a patch ("None" if none) */
    QString pluginName() const;

    /** An output line provided by the assigned plugin */
    quint32 output() const;

    /** Friendly name of the assigned output line */
    QString outputName() const;

    /** Returns true if a valid plugin line has been set */
    bool isPatched() const;

    /** Set a parameter specific to the patched plugin */
    void setPluginParameter(QString prop, QVariant value);

    /**
     * Drop a plugin parameter entirely, as opposed to setting it empty.
     *
     * The difference is real: an absent "outputIP" means the plugin's own
     * default (broadcast, for Art-Net), while an empty one is a configured
     * destination of nowhere. Repointing a patch back to broadcast has to
     * remove the parameter, not blank it.
     */
    void unSetPluginParameter(QString prop);

    /** Retrieve the map of custom parameters set to the patched plugin */
    QMap<QString, QVariant> getPluginParameters();

signals:
    void outputNameChanged();
    void pluginNameChanged();

private:
    /** The reference of the plugin associated by this Output patch */
    QLCIOPlugin* m_plugin;
    /** The plugin line open by this Output patch */
    quint32 m_pluginLine;
    /** The universe that this Output patch is attached to */
    quint32 m_universe;
    /** The interface identity this patch is waiting to find again, or
     *  empty when not pending -- see setPending()/isPending(). */
    QString m_pendingUID;
    /** The patch parameters cache */
    QMap<QString, QVariant>m_parametersCache;

    /********************************************************************
     * Value dump
     ********************************************************************/
public:
    /** Get/Set the output patch pause state */
    bool paused() const;
    void setPaused(bool paused);

    /** Get/Set the output patch blackout state */
    bool blackout() const;
    void setBlackout(bool blackout);

    /** Write the contents of a 512 channel value buffer to the plugin.
      * Called periodically by OutputMap. No need to call manually. */
    void dump(quint32 universe, const QByteArray &data, bool dataChanged);

signals:
    void pausedChanged(bool paused);
    void blackoutChanged(bool blackout);

private:
    /** A buffer used when this output patch is paused */
    QByteArray m_pauseBuffer;
    bool m_paused;
    bool m_blackout;
};

/** @} */

#endif
