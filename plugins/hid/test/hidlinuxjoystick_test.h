/*
  Q Light Controller Plus
  hidlinuxjoystick_test.h

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

#ifndef HIDLINUXJOYSTICK_TEST_H
#define HIDLINUXJOYSTICK_TEST_H

#include <QObject>

/* Linux's joystick reader (/dev/input/jsN) when its pad goes: it waits
   without spinning, reads the pad again when it comes back at the same
   node, and is stopped before the device is taken apart. A FIFO stands in
   for the node. */
class HIDLinuxJoystick_Test final : public QObject
{
    Q_OBJECT

public slots:
    /** The pad's values (channel 0: its first button). */
    void padValue(quint32 universe, quint32 line, quint32 channel, uchar value);

private slots:
    void aPadThatGoesIsWaitedForAndReadAgain();
};

#endif
