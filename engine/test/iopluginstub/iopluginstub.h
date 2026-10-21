/*
  Q Light Controller
  iopluginstub.h

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

#ifndef IOPLUGINSTUB_H
#define IOPLUGINSTUB_H

#include <QStringList>
#include <QString>
#include <QList>
#include <QMap>

#include "qlcioplugin.h"

class IOPluginStub : public QLCIOPlugin
{
    Q_OBJECT
    Q_INTERFACES(QLCIOPlugin)
    Q_PLUGIN_METADATA(IID QLCIOPlugin_iid)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    /** @reimp */
    virtual ~IOPluginStub() override;

    /** @reimp */
    void init() override;

    /** @reimp */
    QString name() const override;

    /** @reimp */
    int capabilities() const override;

    /** @reimp */
    QString pluginInfo() const override;

    /*********************************************************************
     * Discovery / description hooks
     *
     * All driven by public members so a test can build a scenario -- a node
     * heard on line 2, a plugin whose lines are hardware, a protocol that
     * supports targets -- without needing real hardware or a network.
     *********************************************************************/
public:
    /** @reimp */
    QList<Device> discoveredDevices() const override { return m_devices; }

    /** @reimp */
    QString lineDescription(quint32 line, bool output) const override
    { Q_UNUSED(output) return m_lineDescriptions.value(int(line)); }

    /** @reimp */
    bool linesAreHardware() const override { return m_linesAreHardware; }

    /** @reimp */
    bool supportsOutputTargets() const override { return m_supportsTargets; }

    /** @reimp */
    void rescan() override { m_rescanCalled++; }

    /** @reimp Records what was probed, so a test can assert on WHICH addresses
        were asked -- the interesting part is that already-heard ones are not. */
    bool probeTarget(const QString &address) override
    { m_probed << address; return true; }

    /** @reimp m_outputProblems for @p universe, whatever the line */
    QStringList outputProblems(quint32 output, quint32 universe) const override
    { Q_UNUSED(output) return m_outputProblems.value(universe); }

    /** Say the problems changed, as a real plugin would */
    void emitTargetProblemsChanged() { emit targetProblemsChanged(); }

    /** @reimp m_channelRoles for @p channel, whatever the line */
    QString inputChannelRole(quint32 input, quint32 channel) const override
    { Q_UNUSED(input) return m_channelRoles.value(channel); }

    /** @reimp m_channelNames for @p channel, whatever the line */
    QString inputChannelName(quint32 input, quint32 channel) const override
    { Q_UNUSED(input) return m_channelNames.value(channel); }

    /** @reimp m_connectionInfo for @p input */
    QString inputConnectionInfo(quint32 input) const override
    { return m_connectionInfo.value(input); }

    /** @reimp m_inputProblems for @p input */
    QString inputProblem(quint32 input) const override
    { return m_inputProblems.value(input); }

    /** @reimp Counted, like a real plugin's. */
    bool setInputDiagnostics(bool enable) override
    { m_diagnostics += enable ? 1 : -1; return true; }

    /** Report a raw input, as a plugin with diagnostics on would. */
    void emitDiagnostic(quint32 input, const QString &device, const QString &connection,
                        const QString &element, int raw, int channel, int value)
    { emit inputDiagnostic(input, device, connection, element, raw, channel, value); }

public:
    /** What outputProblems() reports, by universe */
    QMap<quint32, QStringList> m_outputProblems;
    /** What inputChannelRole() reports, by channel ("a", "lb", "rx"...) */
    QMap<quint32, QString> m_channelRoles;
    /** What inputChannelName() reports, by channel ("Cross", "L1"...) */
    QMap<quint32, QString> m_channelNames;
    /** What inputConnectionInfo() / inputProblem() report, by input line */
    QMap<quint32, QString> m_connectionInfo;
    QMap<quint32, QString> m_inputProblems;
    /** setInputDiagnostics() calls: +1 on, -1 off */
    int m_diagnostics = 0;
    QList<Device> m_devices;
    QMap<int, QString> m_lineDescriptions;
    bool m_linesAreHardware;
    bool m_supportsTargets;
    int m_rescanCalled;
    QStringList m_probed;
    /** Number of lines outputs()/inputs() advertise; 4 unless a test says otherwise. */
    int m_lineCount;
    /** When set, outputs()/inputs() advertise exactly these line names
     *  (e.g. interface addresses, like Art-Net) instead of m_lineCount stubs. */
    QStringList m_lineNames;
    /** When set, name() returns this (e.g. "ArtNet", to load real show XML). */
    QString m_nameOverride;

    /** @reimp Like a network plugin: each line's rig network gate decision,
     *  cached by line name (the line's address) and counted. */
    void refreshTransmitGate() override
    {
        m_gateRefreshes++;
        m_gateAllowed.clear();
        for (const QString &line : std::as_const(m_lineNames))
            m_gateAllowed.insert(line, transmitAllowedFrom(line));
    }
    int m_gateRefreshes = 0;
    QMap<QString, bool> m_gateAllowed;

    /*********************************************************************
     * Outputs
     *********************************************************************/
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

public:
    /** List of outputs that have been opened */
    QList <quint32> m_openOutputs;

    /** Fake universe buffer */
    QByteArray m_universe;

    /*********************************************************************
     * Inputs
     *********************************************************************/
public:
    /** @reimp */
    bool openInput(quint32 input, quint32 universe) override;

    /** @reimp */
    void closeInput(quint32 input, quint32 universe) override;

    /** @reimp */
    QStringList inputs() override;

    /** @reimp */
    QString inputInfo(quint32 input) override;

    /** Tell the plugin to emit valueChanged signal */
    void emitValueChanged(quint32 universe, quint32 input, quint32 channel, uchar value)
    {
        emit valueChanged(universe, input, channel, value);
    }

public:
    /** List of inputs that have been opened */
    QList <quint32> m_openInputs;

    /*********************************************************************
     * Configuration
     *********************************************************************/
public:
    /** @reimp */
    void configure() override;

    /** @reimp */
    bool canConfigure() const override;

public:
    int m_configureCalled;
    bool m_canConfigure;
};

#endif
