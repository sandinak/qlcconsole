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
class QLCFixtureDef;
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
    /** Highlight, Flash and Focus are over what runs, not over the Grand
     *  Master: it scales them, and Blackout takes them to zero */
    void theGrandMasterAndBlackoutApplyToAHighlight();
    void theGrandMasterAndBlackoutApplyToAFlash();
    void theGrandMasterAndBlackoutApplyToFocus();
    /** The ladder, two at a time on one light; let go of the top one and
     *  the next one shows: Park over Focus and Flash */
    void parkBeatsFocusAndFlash();
    /** Focus over Flash, and both over Highlight */
    void focusAndFlashBeatHighlight();
    /** Highlight over a running cue, intensity and colour */
    void highlightBeatsARunningCue();
    /** A held last look is under everything: a cue takes its colour, and
     *  Highlight, Flash, Focus and Park each take it over */
    void aHeldLastLookIsUnderEverything();
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
    /** A two-channel light (dimmer, cyan) at @p address, on the plot */
    Fixture *addSpot(const QString &name, int address);
    /** A master timer tick (the flasher's) and one frame */
    int tickFrame(int address);
    /** Focus mode on @p fid alone, its light at @p level */
    void startFocus(quint32 fid, float level);
    /** What goes out at @p address: GM 255 at @p full, half at half GM,
     *  0 under Blackout (with the house light still on), back after */
    void checkGrandMasterAndBlackout(int address, int full);

    Doc *m_doc = nullptr;
    Universe *m_uni = nullptr;
    IOPluginStub *m_stub = nullptr;
    QSharedPointer<GenericFader> m_fader;
    QLCFixtureDef *m_spotDef = nullptr;
};

#endif
