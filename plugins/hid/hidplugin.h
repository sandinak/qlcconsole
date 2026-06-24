/*
  Q Light Controller
  hidplugin.h

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

#ifndef HIDPLUGIN_H
#define HIDPLUGIN_H

#include <QEvent>
#include <QList>
#include <QDir>

#include "qlcioplugin.h"

class HIDPoller;
class HIDDevice;
class HIDProfile;

/*****************************************************************************
 * HIDPlugin
 *****************************************************************************/

class HIDPlugin final : public QLCIOPlugin
{
    Q_OBJECT
    Q_INTERFACES(QLCIOPlugin)
    Q_PLUGIN_METADATA(IID QLCIOPlugin_iid)

    friend class ConfigureHID;
    friend class HIDPoller;

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    /** @reimp */
    void init() override;

    /** @reimp */
    virtual ~HIDPlugin() override;

    /** @reimp */
    QString name() const override;

    /** @reimp */
    int capabilities() const override;

    /** @reimp */
    QString pluginInfo() const override;

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

    /** @reimp — returns the profile axis/button name for this channel, or empty. */
    QString inputChannelName(quint32 input, quint32 channel) const override;

    /** @reimp — returns names of all loaded HID profiles (input ignored). */
    QStringList inputDeviceProfileNames(quint32 input) const override;

    /** @reimp — returns axis channel index for semantic slot ("pan"/"tilt"), or -1. */
    int inputAxisForSlot(quint32 input, const QString &slot) const override;

    /** @reimp — returns action string for a button channel, or empty. */
    QString inputButtonAction(quint32 input, quint32 channel) const override;

    /** @reimp — opens HIDProfileEditor dialog for the named profile. */
    void editDeviceProfile(quint32 input, const QString &profileName, QWidget *parent) override;

    /** @reimp — returns sensitivity from the device's matched HID profile, or 1.0. */
    float inputProfileSensitivity(quint32 input) const override;

    /** @reimp — returns deadzone from the device's matched HID profile, or 0.05. */
    float inputProfileDeadzone(quint32 input) const override;

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

    /*********************************************************************
     * Configuration
     *********************************************************************/
public:
    /** @reimp */
    void configure() override;

    /** @reimp */
    bool canConfigure() const override;

    /** @reimp — re-enumerate HID devices without opening the configure dialog. */
    void rescan() override { rescanDevices(); }

signals:
    void configurationChanged();

    /*********************************************************************
     * Devices
     *********************************************************************/
public:
    void rescanDevices();

protected:
    HIDDevice* device(const QString& path) const;
    HIDDevice* device(quint32 index) const;
    HIDDevice* deviceOutput(quint32 index) const;

    void addDevice(HIDDevice* device);
    void removeDevice(HIDDevice* device);

signals:
    void deviceAdded(HIDDevice* device);
    void deviceRemoved(HIDDevice* device);

    /*********************************************************************
     * HID device profiles
     *********************************************************************/
public:
    /** Profile matched to @p device by VID:PID, or nullptr. */
    const HIDProfile *profileForDevice(const HIDDevice *device) const;
    /** Profile with the given name, or nullptr. */
    const HIDProfile *profileByName(const QString &name) const;

private:
    void loadProfiles(const QDir &dir);
    QList<HIDProfile*> m_profiles;

protected:
    QList <HIDDevice*> m_devices;

    /********************************************************************
     * Hotplug
     ********************************************************************/
public slots:
    void slotDeviceAdded(uint vid, uint pid);
    void slotDeviceRemoved(uint vid, uint pid);
};

#endif
