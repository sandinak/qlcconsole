/*
  Q Light Controller Plus - Test Unit
  outputrules_test.h

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

#ifndef OUTPUTRULES_TEST_H
#define OUTPUTRULES_TEST_H

#include <QObject>
#include <QSharedPointer>

class GenericFader;
class IOPluginStub;
class Universe;
class Fixture;
class Doc;

class OutputRules_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void rulesRoundTripAsText();
    void savedOnlyWhenSetAndSkippedByOlderReaders();
    void grandMasterLeavesExemptAlone();
    void grandMasterAllChannelsMode();
    void blackoutKeepsExemptLive();
    /** Park replaces a running cue's level, higher or not; let go, the
     *  cue's level is back */
    void parkBeatsACueAboveIt();
    /** A parked channel goes out past the Grand Master */
    void parkIgnoresTheGrandMaster();
    /** Blackout still takes a parked dimmer to zero */
    void blackoutTakesAParkOut();
    void switchSnapsAtHalf();
    void switchNeverFades();
    void grandMasterCannotSwitchARelayOff();
    void powerLimitsLeaveARelayAlone();
    void onAtOpenFollowsItsSetting();
    /** A Grand Master move never drops an On-when-not-in-use relay to 0,
     *  not even before the next frame */
    void grandMasterNeverDropsAnOnAtOpenRelay();
    void onAtOpenSettingIsSavedWithTheShow();
    void onAtOpenYieldsToTheShow();
    void blindFollowsItsSetting();
    void unpatchClearsTheBits();
    void movedFixtureTakesItsBits();
    void cloneAndCapabilityKeepTheRules();

private:
    /** One processFaders() frame; the output value at @p address */
    int frame(int address);
    void put(int address, uchar value);
    Fixture *addDimmer(const QString &name, int address, int rulesMask);

    Doc *m_doc = nullptr;
    Universe *m_uni = nullptr;
    IOPluginStub *m_stub = nullptr;
    QSharedPointer<GenericFader> m_fader;
};

#endif
