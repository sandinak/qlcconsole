/*
  Q Light Controller Plus
  padtraces_test.h

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

#ifndef PADTRACES_TEST_H
#define PADTRACES_TEST_H

#include <QObject>
#include <QStringList>

class Doc;
class QLCIOPlugin;
class QTemporaryDir;
class FakeHIDBackend;
class PadTracePlayer;

/* Real pads misbehave in ways synthetic input never does (a DualSense clone
   flipping to an Xbox 360 pad, one pad listed twice, triggers that rest
   above zero). Each trace in traces/ -- recorded on the bench with
   QLC_HID_RECORD (docs/dev/PAD_TRACES.md), or a synthetic copy of what we
   know (traces/README.md) -- is played through the real HID plugin into
   Pad Run, and the operator's rules (engine/src/padrun.h) must hold. */
class PadTraces_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void cloneFlipToXbox();
    void cloneLeavesWhileArmed();
    void twinListing();
    void triggerRest();
    void triggerRestDuringTheChord();
    void aSessionIsRecorded();
    void theIdentitiesAreTheEngines();
    void aLongRecordingStopsAtItsCap();

private:
    /** Patch every HID input named @p name to a universe of its own, from
     *  1; how many there were. */
    int patchPads(const QString &name);
    /** Everything running as the app runs it. */
    void startRunning();
    void teardown();
    /** The clone flip; @p hold: with the Create + Options hold that
     *  causes it, else the pad just leaves while armed. */
    void flip(bool hold);
    int commandCount(int command) const;
    int inputsNamed(const QString &name) const;

    QTemporaryDir *m_pluginDir = nullptr;
    QTemporaryDir *m_recordDir = nullptr;
    Doc *m_doc = nullptr;
    QLCIOPlugin *m_hid = nullptr;
    FakeHIDBackend *m_fake = nullptr;
    PadTracePlayer *m_player = nullptr;
    QList<int> m_commands;
};

#endif
