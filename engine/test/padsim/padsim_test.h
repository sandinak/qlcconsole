/*
  Q Light Controller Plus
  padsim_test.h

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

#ifndef PADSIM_TEST_H
#define PADSIM_TEST_H

#include <QObject>
#include <QList>

class QTemporaryDir;
class QLCIOPlugin;
class FakeHIDBackend;
class Doc;

/* Game pads as their USB input reports, through the real HID plugin
   (docs/design/SURFACE_SIMULATOR.md): the DualSense's R2 fires the hit its
   place arms, the four-together stop is one ALL OFF, and the light the pad
   is sent says each state. */
class PadSim_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    /** The simulator reads each report as the plugin does: every control
     *  of the DualSense, DualShock 4 and Xbox (GIP) report lands on the
     *  channel the plugin gives that role. */
    void eachReportReadsAsThePluginReadsIt_data();
    void eachReportReadsAsThePluginReadsIt();
    /** Armed on the L1 + R1 chord, R2 fires the hit its place arms (the
     *  production's; the cue's is not live), and the light says armed. */
    void dualSenseR2FiresTheHitByPlace();
    /** L1 + R1 + L2 + R2: one ALL OFF, disarmed, the light red. */
    void dualSenseEStopChord();
    /** A left stick that rests off centre does not walk the show. */
    void stickDriftMovesNothing();

private:
    void makeShow();

    QTemporaryDir *m_pluginDir = nullptr;
    Doc *m_doc = nullptr;
    QLCIOPlugin *m_hid = nullptr;
    FakeHIDBackend *m_fake = nullptr;
    QList<int> m_commands;
    quint32 m_list = 0, m_cue1 = 0, m_cue2 = 0;
    quint32 m_breathe = 0, m_strobe = 0;
};

#endif
