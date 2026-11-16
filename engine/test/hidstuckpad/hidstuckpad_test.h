/*
  Q Light Controller Plus
  hidstuckpad_test.h

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

#ifndef HIDSTUCKPAD_TEST_H
#define HIDSTUCKPAD_TEST_H

#include <QObject>
#include <QStringList>

class Doc;
class QLCIOPlugin;
class QTemporaryDir;
class FakeHIDBackend;

/* The bench's hang (0.4.10, dink): a DualSense clone flipped to X-input by
   View + Menu re-enumerated as an Xbox 360 pad that macOS never finished
   opening. With the real HID plugin and the engine: the DMX output ticks on,
   Pad Run disarms as for an unplug, and the other pad's emergency stop
   still stops. */
class HIDStuckPad_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void theShowRunsOnWhileAPadHangs();

private:
    /** @p role's channel on HID line @p line ("lb", "lt"...), or -1. */
    int channelOf(quint32 line, const QString &role) const;
    bool inject(quint32 line, const QString &role, uchar value);
    int stops() const;

    QTemporaryDir *m_pluginDir = nullptr;
    Doc *m_doc = nullptr;
    QLCIOPlugin *m_hid = nullptr;
    FakeHIDBackend *m_fake = nullptr;
    QList<int> m_commands;
};

#endif
