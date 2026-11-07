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
#include <QSet>
#include <QHash>
#include <QElapsedTimer>

#include "qlcioplugin.h"
#include "stickfilter.h"

class HIDPoller;
class HIDDevice;
class HIDVirtualPad;
class HIDProfile;
class HIDOSXHotplug;
class QTimer;

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

    /** @reimp — returns the gamepad role of an axis/button channel, or empty. */
    QString inputChannelRole(quint32 input, quint32 channel) const override;

    /** @reimp — opens HIDProfileEditor dialog for the named profile. */
    void editDeviceProfile(quint32 input, const QString &profileName, QWidget *parent) override;

    /** @reimp — returns sensitivity from the device's matched HID profile, or 1.0. */
    float inputProfileSensitivity(quint32 input) const override;

    /** @reimp — the device's HID profile's deadzone; with none, 0.05 for a
     *  game pad this plugin knows and 0.08 for any other joystick. */
    float inputProfileDeadzone(quint32 input) const override;

    /** @reimp */
    QString inputConnectionInfo(quint32 input) const override;

    /** @reimp */
    QString inputProblem(quint32 input) const override;
    /** @reimp */
    QString inputDeviceKind(quint32 input) const override;

    /** @reimp — starts every joystick (patched or not) reporting raw input. */
    bool setInputDiagnostics(bool enable) override;

    /** @reimp — each stick axis: its reading, rest, output and deadzone. */
    QVariantList inputStickAxes(quint32 input) const override;

    /** @reimp — Centre Sticks: learn the rests again once let go. */
    bool centreInputSticks(quint32 input) override;

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

    /** @reimp -- a gamepad's own output (Q126): params {"gamepad": "light",
     *  "rgb": "#rrggbb" (empty: its own colour)} or {"gamepad": "rumble",
     *  "pulses": n}, to the pad on @p inputLine. macOS through the
     *  GameController framework only; elsewhere, and for anything else,
     *  nothing. */
    void sendFeedBack(quint32 universe, quint32 inputLine, quint32 channel, uchar value,
                      const QVariant &params) override;

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

    /** @reimp A line here means a device is plugged in. */
    bool linesAreHardware() const override { return true; }


signals:
    void configurationChanged();

    /*********************************************************************
     * Devices
     *********************************************************************/
public:
    void rescanDevices();

    /** A value from the virtual pad (QLC_HID_VIRTUAL_PAD) on @p input, sent
     *  on as the pad's own reading would be. False if it is not one. */
    Q_INVOKABLE bool injectVirtualInput(quint32 input, quint32 channel, uint value);

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

private slots:
    void slotRawInput(quint32 line, const QString &element, int raw, int channel, int value);
    /** Every reading of every device: its sticks' rest taken out
     *  (StickFilter), then sent on if the device is patched. */
    void slotDeviceValue(quint32 universe, quint32 line, quint32 channel, uchar value);
    void slotStickTick();

private:
    /** Rescan a moment from now, once however often it is asked. */
    void scheduleRescan(int ms = 400);
    /** Wire @p dev up for diagnostics, starting it if they are on. */
    void setupDiagnostics(HIDDevice *dev);
    /** True if @p line is patched as an input. */
    bool isPatchedInput(quint32 line) const;

    /*********************************************************************
     * Stick rest (a worn stick that does not come back to the middle)
     *********************************************************************/
    /** Who @p dev is, across rescans and runs: vendor, product and name
     *  (a Bluetooth pad's name carries its address). */
    static QString stickKey(const HIDDevice *dev);
    /** @p dev's StickFilter, made (its sticks, deadzone and remembered
     *  rests) the first time; nullptr for a device with no sticks. */
    StickFilter *stickFilter(const HIDDevice *dev);
    /** The stick axes of @p dev: by its profile's roles, else the pad
     *  layout's, else a joystick's first two axes (X and Y). */
    QList<int> stickAxes(const HIDDevice *dev) const;
    /** The deadzone the tools apply for @p dev: its profile's, else 0.05
     *  for a known game pad and 0.08 for any other joystick. */
    float deviceDeadzone(const HIDDevice *dev) const;
    void rememberRests(const QString &key, const StickFilter &f) const;
    void ensureStickTimer();

    QHash<QString, StickFilter> m_sticks;
    QTimer *m_stickTimer = nullptr;
    QElapsedTimer m_stickClock;

    QTimer *m_rescanTimer = nullptr;
    /** Asks every few seconds whether a device has gone unannounced. */
    QTimer *m_staleTimer = nullptr;
    HIDOSXHotplug *m_osxHotplug = nullptr;
    int m_diagnostics = 0;
    /** Devices started only for diagnostics. */
    QSet<HIDDevice *> m_diagnosticsStarted;
    /** QLC_HID_VIRTUAL_PAD's pads: kept whatever a rescan finds. */
    QList<HIDVirtualPad *> m_virtualPads;
    /** QLC_HID_VIRTUAL_ONLY: no system devices at all. */
    bool m_virtualOnly = false;
};

#endif
