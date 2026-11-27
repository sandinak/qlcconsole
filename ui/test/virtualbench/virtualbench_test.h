/*
  Q Light Controller Plus
  virtualbench_test.h

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

#ifndef VIRTUALBENCH_TEST_H
#define VIRTUALBENCH_TEST_H

#include <QObject>
#include <QStringList>

class QTemporaryDir;
class QTimer;
class QLCIOPlugin;
class VirtualMidiBench;
class App;

/* The virtual bench (docs/dev/SURFACE_TESTS.md, "Through the real MIDI
   plugin"): his boards as CoreMIDI virtual ports with the names they
   report, found by the app's real MIDI plugin as it finds them on a USB
   cable, in the whole app (offscreen). Every byte the app sends a board is
   read where the board would read it. The stub-plugin tests passed while
   the real boards failed (2026-11-27): no auto-detect, no Ableton mode, the
   FaderPort's motor driving back, Shift+Stop doing nothing, lamps on the
   wrong port. macOS only. */
class VirtualBench_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    /** A port plugged in reaches the plugin by itself (CoreMIDI's
     *  notification): the run loop it needs, and whether it has one. */
    void aPlugReachesThePluginByItself();
    /** The plugin lists the bench's ports by the names his boards report,
     *  as hardware lines; the MaizeDMX's Port 2 as an output only. */
    void theBenchIsListedAsHisBoardsAre();

    /** The APC40 mkII plugged in: patched with its profile, its feedback
     *  its own port, Ableton mode sent to it (twice), a footer chip. */
    void theApc40IsSetUpWhenPluggedIn();
    /** The APC64: its DAW port alone patched, the other three folded under
     *  it; its display taken and named on the DAW port only. */
    void theApc64UsesItsDawPortOnly();
    /** The MaizeDMX's shared port name: asked once ("is this a MaizeDMX?");
     *  Use It patches it and X7's lamp reaches its port. */
    void theMaizeIsAskedOnceThenLit();
    /** The PMJ Black 1 (OpenDeck's port name) beside an RTP session: set up
     *  by itself, its lamps on its own port, none on the session's. */
    void thePmjIsSetUpAndLitOnItsOwnPort();
    /** A port listed before the PMJ's goes (an APC40 unplugged): the PMJ's
     *  lamps stay on the PMJ, never on the port that moved into its place. */
    void aPortGoingKeepsThePmjLampsOnThePmj();
    /** His saved patch (2026-11-27): U01 input the PMJ, feedback the RTP
     *  session. When the PMJ appears its feedback is put on its own port
     *  (fix/053-board-repair; skipped before it). */
    void aPmjFeedbackOnTheRtpLineIsRepaired();

    /** The FaderPort 2: touch, move, let go -- the master follows, and the
     *  motor is sent the master (it stays); Shift+Stop is ALL OFF, Stop
     *  alone refused and said in the footer. */
    void theFaderPortMotorStaysAndShiftStopStops();
    /** dink's preferences (2026-11-27): STOP and the Grand Master already
     *  on the APC64. The FaderPort plugged in beside it. */
    void theFaderPortBesideAnApc64HoldingTheDesk();
    /** The knob in both modes (CC 16 Mackie, CC 60 Studio One): it reaches
     *  the app and its Board Log names it. */
    void theFaderPortKnobInBothModes();

    /** Unplugged: "not connected", greyed; plugged back: on its universe
     *  again, its init sent again. */
    void anUnpluggedBoardIsGreyAndComesBackWithItsInit();
    /** The PMJ and the APC40 together: the PMJ's Back lights, and nothing
     *  of it reaches the APC40. */
    void twoBoardsEachGetOnlyTheirOwnLamps();

private:
    QLCIOPlugin *midi() const;
    quint32 universeOf(const QString &device) const;
    /** Empty when the footer shows at most one message, drawn over no chip. */
    QString footerProblem() const;
    void settle(int ms = 0);

    QTemporaryDir *m_pluginDir = nullptr;
    VirtualMidiBench *m_bench = nullptr;
    QTimer *m_pump = nullptr;
    App *m_app = nullptr;
    int m_lockFd = -1;
    QStringList m_footerMessages;
    QStringList m_footerProblems;
};

#endif
