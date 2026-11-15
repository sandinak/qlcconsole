/*
  Q Light Controller Plus
  dmxusb.h

  Copyright (C) Heikki Junnila
  Copyright (C) Massimo Callegari

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

#ifndef DMXUSB_H
#define DMXUSB_H

#include <QMutex>
#include <atomic>

#include "qlcioplugin.h"

class DMXUSBWidget;

class DMXUSB final : public QLCIOPlugin
{
    Q_OBJECT
    Q_INTERFACES(QLCIOPlugin)
    Q_PLUGIN_METADATA(IID QLCIOPlugin_iid)

    /************************************************************************
     * Initialization
     ************************************************************************/
public:
    /** @reimp */
    virtual ~DMXUSB();

    /** @reimp */
    void init() override;

    /** @reimp */
    QString name() const override;

    /** @reimp */
    int capabilities() const override;

    /** @reimp */
    QString pluginInfo() const override;

    /** Find out what kinds of widgets there are currently connected */
    bool rescanWidgets();

    /** @reimp The Rescan action in the connections view. Without this the
        base-class no-op runs and freshly plugged hardware never appears until
        the app is restarted or the plugin's own dialog is opened. */
    void rescan() override { rescanWidgets(); }

    /** @reimp A line here means a device is plugged in. */
    bool linesAreHardware() const override { return true; }


    /** Get currently connected widgets (input & output) */
    QList <DMXUSBWidget*> widgets() const;

private:
    /** List of references to the discovered USB widgets */
    QList <DMXUSBWidget*> m_widgets;

    /** Review 10 M2: the widgets (and m_outputs) are never replaced while a
     *  universe thread is inside writeUniverse(): it takes this, rescan
     *  takes it to swap them. Nothing else is done under it that waits on
     *  the engine (a closing frame is fetched before it is taken). */
    QMutex m_widgetsMutex;
    /** A rescan in progress: frames of the show no longer reach the old
     *  widgets, so their last one stays the closing frame */
    std::atomic<bool> m_rescanning { false };
    /** What the engine has opened since the last rescan, per (line,
     *  universe) -- a universe mirrored on two lines, two universes on one
     *  (review 12 F6): a frame for any other pair is dropped, never kept in
     *  a widget that may be another device now (under m_widgetsMutex) */
    QList<QPair<quint32, quint32> > m_openPairs;
    /** True when some pair other than (@p line, @p universe) is open on
     *  @p line (m_widgetsMutex held) */
    bool lineOpenForAnother(quint32 line, quint32 universe) const;

    /************************************************************************
     * Outputs
     ************************************************************************/
public:
    /** @reimp */
    bool openOutput(quint32 output, quint32 universe) override;

    /** @reimp */
    void closeOutput(quint32 output, quint32 universe) override;

    /** @reimp */
    QStringList outputs() override;

    /** @reimp */
    QString outputInfo(quint32 output) override;

    /** @reimp */
    void writeUniverse(quint32 universe, quint32 output, const QByteArray& data, bool dataChanged) override;

    /** @reimp the frame given last is on the wire (DMXUSBWidget::flush()) */
    void flushOutput(quint32 output, quint32 universe) override;

    /** @reimp this universe forgotten on @p output, the line left open for
     *  the others (review 12 F6) */
    void forgetOutput(quint32 output, quint32 universe) override;

    /** @reimp: a widget that stopped taking data (unplugged mid-session,
     *  driver gave up) -- WriteHealth, fed by each widget's write loop. */
    QStringList outputProblems(quint32 output, quint32 universe) const override;

private:
    /**
     *  List of references to USB widgets ordered by output lines.
     *  If a widget has multiple outputs, it will appear in this
     *  list multiple times
     */
    QList <DMXUSBWidget*> m_outputs;

    /*************************************************************************
     * Inputs
     *************************************************************************/
public:
    /** @reimp */
    bool openInput(quint32 input, quint32 universe) override;

    /** @reimp */
    void closeInput(quint32 input, quint32 universe) override;

    /** @reimp */
    QStringList inputs() override;

    /** @reimp */
    QString inputInfo(quint32 input) override;

private:
    /**
     *  List of references to USB widgets ordered by input lines.
     *  If a widget has multiple inputs, it will appear in this
     *  list multiple times
     */
    QList <DMXUSBWidget*> m_inputs;

    /********************************************************************
     * Configuration
     ********************************************************************/
public:
    /** @reimp */
    void configure() override;

    /** @reimp */
    bool canConfigure() const override;

    /********************************************************************
     * RDM
     ********************************************************************/
public:
    /** @reimp */
    bool sendRDMCommand(quint32 universe, quint32 line, uchar command, QVariantList params) override;

signals:
    void rdmValueChanged(quint32 universe, quint32 line, QVariantMap data);

    /********************************************************************
     * Hotplug
     ********************************************************************/
public slots:
    void slotDeviceAdded(uint vid, uint pid);
    void slotDeviceRemoved(uint vid, uint pid);
};

#endif
