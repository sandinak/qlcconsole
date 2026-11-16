/*
  Q Light Controller
  hiddevice.h

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

#ifndef HIDDEVICE_H
#define HIDDEVICE_H

#include <QAtomicInteger>
#include <QThread>
#include <QFile>

#include <atomic>
#include <functional>

class HIDPlugin;

/*****************************************************************************
 * HIDDevice
 *****************************************************************************/

class HIDDevice : public QThread
{
    Q_OBJECT

public:
    HIDDevice(HIDPlugin* parent, quint32 line, const QString& name, const QString& path);
    virtual ~HIDDevice();

    /*************************************************************************
     * File operations
     *************************************************************************/
public:
    /**
     * Check if the device offers a built-in merger mode.
     *
     * Merger mode means that all DMX data from the device's input port is
     * copied to its output port and - if output from QLC+ is enabled -
     * merged with this output in a HTP manner on the device itself. 
     *
     * @return true if the device offers a merger mode, false otherwise
     */
    virtual bool hasMergerMode() const;

    /**
     * Check if device's built-in merger mode is enabled.
     *
     * @return true if the device's merger mode is enabled, false otherwise
     */
    virtual bool isMergerModeEnabled() const;

    /**
     * Enable or disable the built-in merger mode.
     */
    virtual void enableMergerMode(bool mergerModeEnabled);

    /**
     * Attempt to open the HID device as input in RW mode and fall back
     * to RO if that fails.
     *
     * @return true if the file was opened RW/RO
     */
    virtual bool openInput();

    /**
     * Close the HID device's input
     */
    virtual void closeInput();

     /**
     * Open HID device as output
     *
     */
    virtual bool openOutput();

    /**
     * Close the HID device's output
     */
    virtual void closeOutput();

    /**
     * Get the full path of this HID device
     */
    virtual QString path() const;

    /**
     * Get the device's file descriptor
     */
    virtual int handle() const;

    /**
     * Read one event and emit it.
     */
    virtual bool readEvent() = 0;

protected:
    QString m_filename;
    QFile m_file;

    /*************************************************************************
     * Line
     *************************************************************************/
public:
    quint32 line() const {
        return m_line;
    }
    /** HIDPlugin keeps a device's line equal to its place in the plugin's
     *  list -- the input number a patch opens it by, and which InputPatch
     *  takes its values for. */
    void setLine(quint32 line) { m_line = line; }

    virtual bool hasInput() const { return false; }
    virtual bool hasOutput() const { return false; }

    /** Joystick devices return their VID/PID; DMX devices return 0. */
    virtual unsigned short vendorId()  const { return 0; }
    virtual unsigned short productId() const { return 0; }
    /** Number of axes on joystick devices; -1 for non-joystick. */
    virtual int axisCount() const { return -1; }
    /** Number of buttons on joystick devices; -1 for non-joystick. */
    virtual int buttonCount() const { return -1; }

    /** True when the hardware this object was made for has gone -- the
     *  controller was switched off, or reconnected after the Mac slept, and
     *  is now a different device at the same path. HIDPlugin replaces it. */
    virtual bool isStale() const { return false; }

    /** isStale(), as a check that holds nothing of this object: the plugin's
     *  scan thread runs it while the device stays the GUI thread's. Empty
     *  when the device can't tell (never stale). */
    virtual std::function<bool()> staleProbe() const { return {}; }

    /** Which system entry it was made for, when the listing says (0: it
     *  does not). The same path listed with another one is a device that
     *  came back -- re-enumerated, switched off and on -- and is replaced. */
    quint64 instance() const { return m_instance; }
    void setInstance(quint64 instance) { m_instance = instance; }

    /** Made on a worker thread (HIDPlugin opens devices off the GUI
     *  thread): move it, and its file, to @p thread. Called on the thread
     *  that made it, before anything else uses it. */
    void handTo(QThread *thread);

    /** Ask the reader to stop and wait for it at most @p ms. False when it
     *  is still in a call to the system: it then stops by itself once the
     *  call returns, and the device must not be deleted before. */
    bool stopReading(int ms);

    /** How the device is connected and read, for a person ("Bluetooth LE ·
     *  Xbox Series X|S Controller (045E:0B13) · GameController framework").
     *  Empty when there is nothing to say. */
    virtual QString connectionInfo() const { return QString(); }

    /** Why input from it is not being read, for a person; empty when it
     *  is (or might be). */
    virtual QString inputProblem() const { return QString(); }

    /** Where it is plugged in, to tell two of one model apart
     *  (QLCIOPlugin::inputDeviceLocation()): "USB 14200000" on macOS (the
     *  transport and IOKit location ID of its hidapi path), the sysfs phys
     *  of a Linux joystick ("usb-0000:00:14.0-2/input0"), else the path. */
    virtual QString location() const;

protected:
    quint32 m_line;
    int m_capabilities;
    quint64 m_instance = 0;

    /*************************************************************************
     * Device info
     *************************************************************************/
public:
    /**
     * Get HID device information string to be used in plugin manager
     */
    virtual QString infoText() const;

    /**
     * Get the device's name
     */
    virtual QString name() const;

protected:
    QString m_name;

    /*************************************************************************
     * Input data
     *************************************************************************/
signals:
    /**
     * Signal that is emitted when an input channel's value is changed
     *
     * @param universe The universe where the event happened
     * @param line The input line that received the signal
     * @param channel The channel whose value has changed
     * @param value The changed value
     */
    void valueChanged(quint32 universe, quint32 line, quint32 channel, uchar value);

    /**
     * Diagnostics (setDiagnostics()): one raw input as the device reported
     * it, before any mapping -- @p element names it ("Button 4", "Rz",
     * "USB report byte 4 bit 4", "GameController buttonA"), @p raw is its
     * value as read, @p channel the channel it feeds (-1: none) and @p value
     * what that channel now says (0..255).
     */
    void rawInput(quint32 line, const QString &element, int raw, int channel, int value);

public:
    /** Report every raw input through rawInput() while on (the Controller
     *  test window). Costs nothing while off. */
    void setDiagnostics(bool on) { m_diagnostics.storeRelaxed(on); }
    bool diagnostics() const { return m_diagnostics.loadRelaxed(); }

protected:
    QAtomicInteger<bool> m_diagnostics { false };

public:
    /**
     * Send an input value back the HID device to move motorized sliders
     * and such.
     */
    virtual void feedBack(quint32 channel, uchar value);

protected:
    /** The reader runs while true; set from the GUI thread, read by it. */
    std::atomic<bool> m_running { false };

private:
    /** Input data thread worker method */
    virtual void run() override;

    /*************************************************************************
     * Output data
     *************************************************************************/
public:

    /** Output data, which is a DMX universe */
    virtual void outputDMX(const QByteArray &data, bool forceWrite = false);
};

#endif
