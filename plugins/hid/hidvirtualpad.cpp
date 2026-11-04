/*
  Q Light Controller Plus
  hidvirtualpad.cpp

  Copyright (C) Branson Matheson

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

#include <climits>

#include "hidvirtualpad.h"
#include "hidplugin.h"

HIDVirtualPad::HIDVirtualPad(HIDPlugin *parent, quint32 line, const QString &name,
                             unsigned short vendorId, unsigned short productId)
    : HIDDevice(parent, line, name, QStringLiteral("virtual:%1").arg(name))
    , m_vendorId(vendorId)
    , m_productId(productId)
{
}

bool HIDVirtualPad::openInput()
{
    m_open = true;
    return true;
}

void HIDVirtualPad::closeInput()
{
    m_open = false;
}

QString HIDVirtualPad::connectionInfo() const
{
    return QStringLiteral("virtual (QLC_HID_VIRTUAL_PAD) · %1:%2")
        .arg(m_vendorId, 4, 16, QLatin1Char('0')).arg(m_productId, 4, 16, QLatin1Char('0'));
}

void HIDVirtualPad::inject(quint32 channel, uchar value)
{
    // As HIDOSXJoystick sends what it reads: any universe, this line.
    if (m_open)
        emit valueChanged(UINT_MAX, m_line, channel, value);
}

QList<HIDVirtualPad *> HIDVirtualPad::fromEnvironment(HIDPlugin *parent)
{
    QList<HIDVirtualPad *> pads;
    const QString spec = qEnvironmentVariable("QLC_HID_VIRTUAL_PAD");
    for (const QString &one : spec.split(QLatin1Char(';'), Qt::SkipEmptyParts))
    {
        // vid:pid:name -- the name may hold colons itself (a Bluetooth address).
        const int a = one.indexOf(QLatin1Char(':'));
        const int b = a < 0 ? -1 : one.indexOf(QLatin1Char(':'), a + 1);
        if (b < 0)
            continue;
        bool okV = false, okP = false;
        const unsigned short vid = one.left(a).toUShort(&okV, 16);
        const unsigned short pid = one.mid(a + 1, b - a - 1).toUShort(&okP, 16);
        if (!okV || !okP)
            continue;
        pads << new HIDVirtualPad(parent, 0, one.mid(b + 1), vid, pid);
    }
    return pads;
}
