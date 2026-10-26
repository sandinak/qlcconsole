/*
  Q Light Controller Plus - qlcconsole
  inputdevicenotices.h

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

#ifndef INPUTDEVICENOTICES_H
#define INPUTDEVICENOTICES_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include "qlcui_export.h"

class QTimer;
class Doc;

/**
 * Says when a device a universe's input is patched to -- a game controller,
 * a MIDI surface -- comes or goes.
 *
 * The engine keeps such a patch offline while its device is away and puts
 * it back when the device reappears (InputOutputMap::followInputDevices()).
 * This turns that into words: a notice ("Controller 'X' disconnected —
 * universe 2", amber; "... connected", plain) and a footer readiness entry
 * ("input.devices", ShowStatus) for as long as one is missing.
 *
 * A Bluetooth pad going to sleep and waking again, or reconnecting, can go
 * and come back within a second or two: nothing is said until the state has
 * held for settleTime(), and a device that came back by then is not
 * mentioned at all.
 */
class QLC_UI_EXPORT InputDeviceNotices : public QObject
{
    Q_OBJECT

public:
    explicit InputDeviceNotices(Doc *doc, QObject *parent = nullptr);

    /** One device input patch, as it stands. */
    struct Device
    {
        quint32 universe = 0;
        QString plugin;
        QString name;
        bool connected = false;
    };

    /** Every input patched to a device (a plugin whose lines are hardware):
     *  the open ones, and the ones kept offline whose plugin is here. */
    QList<Device> devices() const;

    /** A show was opened or cleared: forget what was said. The next notice
     *  names what is missing, not what is there. */
    void reset();

    /** How long a change has to hold before it is said (ms). */
    int settleTime() const;
    void setSettleTime(int ms);

    /** Say what changed now instead of after the settle time. */
    void settleNow();

    /** "Controller", "MIDI device" or "Input device", for @p plugin. */
    static QString kindName(const QString &plugin);

signals:
    /** Show @p text; @p warning (a device went): amber, and it stays. */
    void notice(const QString &text, bool warning);

private:
    void schedule();
    void updateStatus(const QList<Device> &now) const;

    Doc *m_doc;
    QTimer *m_settle;
    /** What was last said, by universe. */
    QMap<quint32, Device> m_said;
};

#endif
