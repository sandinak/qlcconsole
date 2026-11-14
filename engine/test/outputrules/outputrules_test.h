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

    /* Show only (Fixture::ShowOnly) */
    void showOnlySavedAndLoaded();
    /** Design: a cue that turns a Show-only relay on leaves its DMX safe
     *  while the other lights preview; the rig views still get the value */
    void designHoldsAShowOnlyRelay();
    /** Operate: a real GO drives it; back to Design it is safe at once */
    void operateGoDrivesItAndDesignTakesItBack();
    /** Operate: a preview's scene (the Workbench) is kept off it, and the
     *  same scene from a GO drives it */
    void operatePreviewIsKeptOffIt();
    /** A preview's chaser step (a scene started by the chaser) too */
    void operatePreviewChildIsKeptOffIt();
    /** Operate: Highlight, Focus and a fixture-test fader don't drive it */
    void highlightFocusAndTestsDontDriveIt();
    /** Blind holds it, Operate too */
    void blindHoldsIt();
    /** What a Design preview left on an LTP channel never comes out live */
    void goingLiveStartsFromSafe();
    /** A laser's every channel 0; a moving light's wheel at its default */
    void safeValuePerKind();
    /** The footer's words */
    void heldTextSaysWhy();
    /** A show with none: no channel held, the frames as before */
    void showWithoutItIsUnchanged();

    /* Show only, safety review (0.4.8) */
    /** 1: Blackout and Blind (with an exempt house light) never send a
     *  preview's LTP value on it: a Blackout frame is raw stored values */
    void blackoutAndBlindNeverSendAPreviewsValue();
    /** 2: every send holds it -- Blackout off, leaving Blind -- not just
     *  the frame; and the next frame after a change is a changed one */
    void everySendHoldsIt();
    /** 3: a preview stopping in Operate does not fade its level onto it */
    void aPreviewsFadeOutNeverReachesIt();
    /** 4: live is read once a frame: a switch to Operate mid-frame does
     *  not send that frame's preview */
    void aSwitchMidFrameSendsNoPreview();
    /** 5: an editor's Test and an editor's own DMX source don't drive it
     *  in Operate; a GO on the same function does */
    void editorTestsAndEditorSourcesDontDriveIt();
    /** 6: made Show only while it is on: a plugin that sends only
     *  changes gets the held frame at once */
    void markingItWhileOnIsSentAtOnce();
    /** 7: its safe value goes out through the channel's modifier */
    void safeValueGoesThroughTheModifier();
    /** 8: a function started by another follows that one's gate (no Doc
     *  lookup on the timer thread); the parent taken by a GO takes them */
    void childrenFollowTheirParentsGate();
    /** 9: Show only wins over On when not in use; made Show only while
     *  live, a stored preview value is not sent */
    void showOnlyWinsOverOnAtOpen();
    void markingItLiveDropsAStoredValue();
    /** What a frame costs with none, with 64 held (Design) and live with a
     *  preview's fader on them: printed, not asserted (hotpath_test is the
     *  guard) */
    void showOnlyFrameCost();

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
    /** A one-step scene of @p values (address, value), started by @p parent */
    class Scene *startScene(const QList<QPair<int, int> > &values, quint32 parentType, quint32 parentId);
    /** Every function stopped and gone from the master timer */
    void stopFunctions();
    /** What went out at @p address, the last frame */
    int sent(int address) const;

    Doc *m_doc = nullptr;
    Universe *m_uni = nullptr;
    IOPluginStub *m_stub = nullptr;
    QSharedPointer<GenericFader> m_fader;
    QLCFixtureDef *m_spotDef = nullptr;
};

#endif
