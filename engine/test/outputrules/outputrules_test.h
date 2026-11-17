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
#include <functional>
#include "qlcchannel.h"

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

    /* Show only, second review */
    /** 1: going live, a send before the next frame has no preview value */
    void goingLiveSendsNoStoredPreview();
    /** 2: a GO on a list running from a preview drives it */
    void aGoOnAListRunningFromAPreviewDrivesIt();
    /** Epoch: a list left running from Design holds it until a GO in
     *  Operate; a Park set in Design too, until set again */
    void aListLeftRunningFromDesignWaitsForAGo();
    void aParkSetInDesignHoldsItInOperate();
    /** 3a: what a Script under an editor's Test starts is held */
    void whatAScriptUnderATestStartsIsHeld();
    /** 4: a preview's effect on it is held from the first live frame */
    void aPreviewsEffectIsHeldFromTheFirstLiveFrame();
    void aLevelOnAShowOnlyWashStaysHeld();
    /** 5: "not ours" stays zero over a held safe value */
    void notOursStaysZeroUnderTheHold();
    /** 8: made Show only mid-show, only that fixture is reset */
    void markingOneMidShowResetsOnlyIt();

    /* Third safety review (review/048-show-only-3): each is a path the
       rule as set says must hold (or drive) a Show-only fixture. */
    /** R3-1: a Park set in Design stays held when another fixture is parked in Operate */
    void aDesignParkStaysHeldWhenAnotherFixtureIsParkedLive();
    /** R3-2: a preview started and stopped before the timer ticks sends nothing */
    void aPreviewStoppedBeforeItsFirstTickSendsNothing();
    /** R3-3: a Simple Desk cue stack left playing from Design waits for a GO */
    void aSimpleDeskCueStackLeftRunningFromDesignIsHeld();
    /* R3-4 (Identify / Reset refused for a Show-only fixture) is
       Monitor_Test::maintenanceIsRefusedForAShowOnlyFixture */
    /** R3-5: Blackout wins over a Show-only fixture marked Ignore Blackout */
    void blackoutWinsOverShowOnlyIgnoringBlackout();
    /** R3-6: the emergency stop (All Off) takes it out at once, not over a fade */
    void emergencyStopTakesItOutAtOnce();
    /** R3-7: a VC "next step" button on a list left running from Design takes */
    void aVcStepButtonOnADesignListDrivesIt();

    /* Third review: the fixes' own tests */
    /** R3-1: Mark and a held last look are stamped per channel too */
    void aDesignMarkStaysHeldWhenAnotherIsMarkedLive();
    void aDesignLastLookStaysHeldAtTheNextGo();
    /** R3-6: the e-stop holds them, a Simple Desk fader too, until a GO */
    void emergencyStopLatchesUntilTheNextGo();
    /** 9: a Show-only fixture moved mid-show is held at its new address
     *  at once */
    void aShowOnlyFixtureMovedMidShowIsHeldAtOnce();
    void theHeldTextSaysEmergencyStop();
    void testChannelsSitOnTopAndReleaseTogether();
    void aReleasedLtpChannelGoesBackToWhatItWas();
    void aPatchChangeEndsTestMode();
    void theStopAndOperateEndTestMode();
    void aShowOnlyChannelStaysSafeInTestMode();

    /* Fourth safety review (review/048-show-only-4) */
    /** R4-1: All Off on a Show tab show in Operate, then a GO on another
     *  list: the show's last look (captured as it stops, after the stop's
     *  new session) does not fire the laser */
    void emergencyStopThenAGoDoesNotRefireTheShowsLastLook();
    /** R4-2: All Off, then a GO on another list: a stopped cue's LTP level
     *  left stored on a Show-only channel does not come back */
    void emergencyStopThenAGoDoesNotRefireAStoredLtpLevel();
    /** R4-3: the VC step button in Operate steps the last chaser started:
     *  an editor's Test chaser is not made the show by it */
    void aVcStepButtonDoesNotClaimAnEditorTest();

private slots:
    /* Fourth review: the fixes' own tests */
    /** C1, the other order: a look captured before the stop loses its
     *  Show-only channels at the stop */
    void emergencyStopDropsALastLookCapturedBeforeIt();
    /** L2: the step button steps the root of a nested list */
    void aVcStepButtonStepsTheRootOfANestedList();
    /** L3: universe and address changed: marked at once, rebuilt once */
    void aMoveAcrossUniversesMarksAtOnceAndRebuildsOnce();
    /** L3: an emergency stop from a plugin's thread acts at once */
    void anEmergencyStopFromAnotherThreadActsAtOnce();

    /* Fifth review (red until fixed) */
    /** A show's scene fading out after All Off is still the show: a GO on
     *  another list lets the latch go and the fade-out reaches the flame */
    void aShowsFadeOutAfterTheStopStaysOut();
    /* Sixth review (of 1391a70ca1), the engine's part */
    void passthroughAndAResetLeaveAShowOnlyChannelAtSafe();
    void aShowOnlyFixtureMovedOntoAStampedChannelWaits();
    void aChildKeepsItsParentsStampForTheSession();
    void anAutomaticSourceIsHeldUntilArmed();
    /* Eighth review (of 1e5ccc7b62): red until fixed */
    /** R8-1: a held park (set in Design) on a Show-only channel takes the
     *  GO's level past the Grand Master */
    void r8_aHeldParkTakesTheGoPastTheGrandMaster();
    /** R8-2: an On-when-not-in-use fixture moved onto a Show-only one's
     *  address drives it in Operate with no GO */
    void r8_anOnAtOpenFixtureOnItsAddressDrivesIt();
    /** R8-3: a fixture moved by fewer channels than it has keeps the old
     *  marks there: the GO from before the re-patch still drives it */
    void r8_aGoFromBeforeARepatchStillDrivesIt();
    /** R8-4: a send between two frames (Blackout off, Blind off, All Off,
     *  a maintenance send) copies postGMValues() unlocked, mid-frame: a held
     *  writer's value, not yet put back, goes out */
    void r8_aSendBetweenTwoFramesCarriesAHeldWriter();
    /** R8-6: in Blind on a universe with no Blackout-exempt circuit nothing
     *  is sent at all -- not the emergency stop's cut, not Blackout: the
     *  rig keeps the last frame, the laser on */
    void r8_allOffAndBlackoutInBlindNeverReachTheRig();
    /** R8-5: the effect runner's own fader is trusted always: an effect
     *  GO'd before a Show-only fixture is re-patched drives it at its new
     *  address, past the new marks */
    void r8_anEffectFollowsARepatchPastTheMarks();
    /** S0, every send: each send path between two frames, a held writer
     *  present -- the Show-only channel goes out at its S0 value */
    void everySendGoesThroughTheS0Builder();
    /** AllChannels Grand Master: a non-zero safe value goes out the same,
     *  unscaled, held and live */
    void aSafeValueIsNeverScaledHeldOrLive();

    /* Ninth review (of 690eeeab48): red until fixed */
    /** R9-1: an output put on hold (Teaching mode's "hold real output",
     *  the rig-network gate: OutputPatch::setPending()) is closed with no
     *  frame at safe: the line keeps the laser on through All Off */
    void r9_aHeldOutputKeepsTheLaserOnThroughAllOff();
    /** R9-1: a universe re-patched to another output line mid-show: the
     *  old line is closed with no frame at safe, All Off never reaches it */
    void r9_aMovedOutputKeepsTheLaserOnThroughAllOff();
    /** R9-2: Doc::patchFixtures() writes a laser's definition defaults
     *  (applyFixtureToUniverses()) and only marks it Show only after every
     *  fixture is applied: a frame in between sends the default */
    void r9_aLaserBeingPatchedSendsItsDefault();
    /** R9-3: a held relative writer (an EFX in relative mode, the XY pad)
     *  marks a Show-only channel written: Universe::writeRelative() marks
     *  before it checks writeBlocked() */
    void r9_aHeldRelativeWriterSetsTheWrittenFlag();

    /* Ninth review: the fixes' own tests */
    /** Every way a line stops receiving a universe (re-patched, held,
     *  unpatched, the universe removed, the patch destroyed), mid-show with
     *  a Show-only cue live: that line's last frame has the Show-only
     *  channels at safe, the others as they are; nothing after */
    void everyClosingLineGetsShowOnlySafeLast();
    /** A line closed with no Show-only channel gets no extra frame */
    void aClosingLineWithNoShowOnlyGetsNothingExtra();
    void review10_aClosingLineInBlindGetsNoPreview();
    /** Review 11 M-3: a retarget in one step (address and port at once,
     *  PatchPlanner's): the old node gets the closing frame, the new one
     *  never does; OSC's outputPort is a target too */
    void aRetargetInOneStepClosesOnlyTheOldNode();
    /** Review 12 F7: a patch opened with its target (Patch Planner's) sends
     *  nothing to the plugin's default target, not even a closing frame;
     *  the same value again ("1" for 1) sends nothing */
    void aPatchOpenedAimedSendsNothingToTheDefaultTarget();
    /** A held line released: its first frame is the S0 build */
    void aReleasedLineStartsFromTheCurrentBuild();
    /** The patch list changes under the frame lock: frames and sends from
     *  another thread while lines are added, moved, held and removed */
    void patchListChangesUnderTheFrameLock();
    /** A held AutoRemove channel reaching its target lets go of nothing
     *  (no hold, no "Why is this on?" let-go) */
    void aHeldAutoRemoveChannelLetsGoOfNothing();

private slots:
    /* The e-stop rules (OutputRuleKeeper E1-E7), swept: each writer set
       up in Operate, All Off, an unrelated GO -- held -- then its own
       operator action -- driven */
    void sweepFunctionFadeOut();
    void sweepChaserStepFade();
    void sweepEfx();
    void sweepRgbMatrix();
    void sweepPark();
    void sweepMark();
    void sweepLastLook();
    void sweepCueStack();
    void sweepEffectRunner();

    /** The emergency stop (ShowControl AllOff) leaves a fogger at rest, as
     *  Stop all does (E8; UX-70: "a fog machine kept pumping"). */
    void theEmergencyStopPutsAFoggerAtRest();
    /** The emergency stop in FOCUS ("Everything is off.") ends the focus:
     *  no focus light left on (E9). */
    void theEmergencyStopEndsTheFocusLight();

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
    /** A Show-only fixture of its own definition, channels @p chans */
    class Fixture *addShowOnlyDef(const QString &name, int address,
                                  const QList<QPair<QLCChannel::Group, QLCChannel::PrimaryColour> > &chans);
    class Chaser *listOf(class Function *step);
    void goList(class Chaser *c);
    void sweep(int address, const std::function<void()> &tickOnce,
               const std::function<void()> &setUp, const std::function<void()> &again,
               const std::function<void()> &afterStop = std::function<void()>());
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
