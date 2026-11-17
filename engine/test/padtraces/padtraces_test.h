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
    /** Two same-named pads with no location: A arms, holds LT and goes; B
     *  moves onto A's line: pad A lost -- disarmed, said; B's LT + A plays
     *  nothing (board Q253's fourth review). */
    void aShiftedLineIsNoSafety();
    /** Another pad's drop leaves the runner as it was (macOS-like and
     *  Linux-like): armed, its held LT a safety, three of four of a stop
     *  completed by the fourth. */
    void anotherPadsDropLeavesTheRunnerAsItWas_data();
    void anotherPadsDropLeavesTheRunnerAsItWas();
    /** A pad re-enumerated at its path keeps its own universe (its stop
     *  live); the other pad keeps its arm. */
    void aPadReenumeratedKeepsItsUniverse_data();
    void aPadReenumeratedKeepsItsUniverse();
    /** A pad unplugged and back: its universe reopens on it, never on the
     *  pad of the same name that took its line meanwhile. */
    void aPadUnpluggedAndBackTakesNoOtherPad();
    /** A spare of the same name on charge never takes an unplugged pad's
     *  universe; the pad plugged back does (both backends). */
    void aSpareOnChargeTakesNoUniverse_data();
    void aSpareOnChargeTakesNoUniverse();
    /** An "any" slot moved off a line another universe was patched to is
     *  a pad lost: disarmed, the spare's A plays nothing. */
    void anAnySlotTakesNoPadSilently();
    /** An "any" slot follows its pad's device to another line, never the
     *  spare that moved onto its old line. */
    void anAnySlotFollowsItsPadNotTheLine();
    /** A spare that was here re-enumerates where it is: still not the pad
     *  a universe waits for. */
    void aSpareReenumeratedTakesNoUniverse_data();
    void aSpareReenumeratedTakesNoUniverse();
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
    /** Rescan until @p count pads named @p name are listed. */
    void q253Scan(const QString &name, int count);
    int q253Line(const QString &path) const;
    /** A reading on the pad at @p path, as it would send it. */
    bool q253In(const QString &path, quint32 ch, uint value);

    QTemporaryDir *m_pluginDir = nullptr;
    QTemporaryDir *m_recordDir = nullptr;
    Doc *m_doc = nullptr;
    QLCIOPlugin *m_hid = nullptr;
    FakeHIDBackend *m_fake = nullptr;
    PadTracePlayer *m_player = nullptr;
    QList<int> m_commands;
};

#endif
