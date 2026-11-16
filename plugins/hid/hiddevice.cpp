/*
  Q Light Controller
  hiddevice.cpp

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

#include <QString>
#include <QFile>
#include <QDeadlineTimer>

#include "hiddevice.h"
#include "gamepadlayout.h"
#include "hidplugin.h"

HIDDevice::HIDDevice(HIDPlugin* parent, quint32 line, const QString &name, const QString& path)
    : QThread(parent)
{
    m_name = name;
    m_filename = path;
    m_file.setFileName(path);
    m_line = line;
}

HIDDevice::~HIDDevice()
{
    if (isRunning() == true)
    {
        m_running = false;
        wait();
    }
    closeInput();
}

void HIDDevice::handTo(QThread *thread)
{
    m_file.moveToThread(thread);
    moveToThread(thread);
}

bool HIDDevice::stopReading(int ms)
{
    m_running = false;
    if (isRunning() == false)
        return true;
    return wait(QDeadlineTimer(ms));
}

/*****************************************************************************
 * File operations
 *****************************************************************************/
bool HIDDevice::hasMergerMode() const
{
    return false; //usual HIDDevices don't offer a merger mode
}

bool HIDDevice::isMergerModeEnabled() const
{
    return false; //never enabled when not offered
}

void HIDDevice::enableMergerMode(bool mergerModeEnabled)
{
    Q_UNUSED(mergerModeEnabled);
}


bool HIDDevice::openInput()
{
    return false;
}

void HIDDevice::closeInput()
{
}

bool HIDDevice::openOutput()
{
    return false;
}

void HIDDevice::closeOutput()
{
}

QString HIDDevice::path() const
{
    return m_filename;
}

QString HIDDevice::location() const
{
    const QString p = path();
    /* macOS hidapi: "<transport>_<vid>_<pid>_<location>" -- the USB port
       (or the Bluetooth link) it is on. */
    const QString mac = GamepadLayout::locationFromPath(p);
    if (!mac.isEmpty())
        return mac;
#if defined(Q_OS_LINUX)
    /* A joystick node (/dev/input/js0): the number moves as pads come and
       go; its phys is the port. */
    if (p.startsWith(QLatin1String("/dev/input/")))
    {
        QFile phys(QStringLiteral("/sys/class/input/%1/device/phys").arg(p.mid(11)));
        if (phys.open(QIODevice::ReadOnly))
        {
            const QString text = QString::fromLatin1(phys.readAll()).trimmed();
            if (!text.isEmpty())
                return text.startsWith(QLatin1String("usb-")) ? QStringLiteral("USB ") + text : text;
        }
    }
#endif
    return p;
}

int HIDDevice::handle() const
{
    return m_file.handle();
}

/*****************************************************************************
 * Device info
 *****************************************************************************/

QString HIDDevice::infoText() const
{
    return QString();
}

QString HIDDevice::name() const
{
    return m_name;
}

/*****************************************************************************
 * Input data
 *****************************************************************************/

void HIDDevice::feedBack(quint32 channel, uchar value)
{
    Q_UNUSED(channel);
    Q_UNUSED(value);
}

void HIDDevice::run()
{
}

void HIDDevice::outputDMX(const QByteArray &data, bool forceWrite)
{
    Q_UNUSED(data);
    Q_UNUSED(forceWrite);
}
