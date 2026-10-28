/*
  Q Light Controller Plus
  artnet_test.cpp

  Copyright (c) Jano Svitok

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

#ifndef ARTNET_TEST_H
#define ARTNET_TEST_H

#include <QObject>

class ArtNetController;

class ArtNet_Test final : public QObject
{
    Q_OBJECT

private slots:
    void setupArtNetDmx();
    void fillArtPollReplyInfo();

    /* Which controller a received packet is attributed to. */
    void packetIsAttributedToTheArrivalInterface();
    void offSegmentPacketIsNotFiledUnderLoopback();

    /* Resolving a saved patch whose exact address no longer matches any
       current line, but whose subnet still does. */
    void lineOnSameSubnetResolvesChangedAddress();

    /* Send failure reporting: SendHealth's hysteresis, driven with
       injected results and times -- no packet leaves the machine. */
    void sendHealthTransientBurstIsNotReported();
    void sendHealthSustainedFailureIsReported();
    void sendHealthHardErrorIsReportedAtOnce();
    void sendHealthRecoveryClears();
    void sendHealthAlternatingResultsDoNotToggle();
    void sendHealthArpFlapIsReportedOnce();
    void sendHealthHardErrorClassification();
    void controllerLogsOnlyOnStateChange();

    /* Node health from ArtPollReplies: ArtNetNodeWatch driven with
       fabricated replies and poll times. */
    void nodeWatchSilentAfterThreeMissedPolls();
    void nodeWatchAnsweringAgainNeedsTwoIntervals();
    void nodeWatchNeverHeardIsNotFlagged();
    void nodeWatchPortShortIsDebouncedBothWays();
    void nodeWatchNotOutputtingOnlyOnceSeenOutputting();
    void nodeWatchBroadcastAndUnicastPickTheRightNodes();
    void nodeWatchBindIndexNumbersPortsPastFour();

    /* The same through the controller and the plugin: real reply bytes in,
       problem text out, announced and cleared. Nothing is sent. */
    void controllerReportsAndClearsProblems();
    void pluginGroupsAProblemsUniverses();

    /* Nodes without an address on our network, and setting a node's IP
       (ArtIpProg). Every send goes to a test sink; nothing leaves. */
    void ipProgQueryChangesNothing();
    void ipProgPacketLayout();
    void ipProgReplyParse();
    void pollReplyDhcpBits();
    void offSubnetReplyIsListedAsNeedingItsIp();
    void sameSubnetNodeIsNotFlagged();
    void ipProgIsNeverSentToAnUnreachableNode();
    void ipProgIsUnicastToAReachableNode();
    void ipProgReplyReachesThePlugin();
    void discoveryPollIsLimitedBroadcastOnTheLine();
    void rescanSendsOneDiscoveryPollPerInterface();
    void periodicPollIsUnchanged();
    void nodeSetupHelpers();
    void nodeSetupOffersTheAliasForAnUnreachableNode();
    void nodeSetupValidatesAndConfirmsBeforeSending();
    void nodeSetupShowsTheNodesAnswer();

    /* The rig network gate: a line not on the show's rig networks sends
       nothing -- DMX, polls, poll replies, discovery, ArtIpProg. Every
       send goes to a test sink; nothing leaves. */
    void rigNetworkGateHoldsEverySend();
    void rigNetworkGateAllowsRigAndLoopback();
    void rigNetworkGateDecisionIsCachedOffTheSendPath();

    /* The RDM flag held steady (ArtNetRdmFlag): a junk Status1 (the
       CR041R's) reads "no RDM" throughout; a real one is followed. */
    void rdmFlagCr041rStaysOff();
    void rdmFlagAnRdmNodeTurnsOnAndHolds();
    void rdmFlagARealChangeIsFollowed();
    void rdmFlagJunkStaysJunkThroughALongRun();
    void rdmFlagTableOfDevicesIsProof();
    void rdmFlagReachesTheNodesList();

    /* Malformed packets from the network (fuzz, matrix 2): dropped quietly,
       never read past their end; valid ones parse as before. */
    void artDmxShorterThanItsHeaderIsDropped();
    void artDmxIsClampedToAUniverse();
    void artTodDataPromisingMoreUidsIsDropped();
    void artTodDataStillParses();
    void rdmValidRepliesStillParse();
    void rdmEveryTruncationIsDropped();
    void rdmFuzzCrashesAreDroppedQuietly();
    void malformedPacketsThroughTheController();

    /* The controller's send and receive paths, byte for byte (coverage
       campaign: the mutation survivors of matrix 2). Every send goes to a
       test sink; nothing leaves. */
    void addUniverseMapsItsOwnUniverse();
    void transmissionModes();
    void standardModeRefreshes();
    void dmxSequenceRunsOneTo255PerUniverse();
    void artDmxInputEmitsChangedChannels();
    void pollTimerFollowsOutputs();
    void pollBytes();
    void pollReplyLayout();
    void pollIsAnsweredOncePerUniverse();
    void rdmCommandsGoToThePatchedNode();
    void rdmRepliesReachTheSignal();
    void pollTickDrivesTheNodeWatchAndOffSubnetPolls();
    void problemsChangedIsNeverEmittedUnderTheLock();
    void steadySendsAnnounceNothing();
    void pollReplyOptionalFields();
    void ipProgReplyBoundaries();
    void legacyPartialOutputIPs();
    void failedWritesAreReported();
    void socketBound();

    /* What the plugin keeps for the show file: only what differs from the
       default, per universe and line; closing a universe drops its own */
    void pluginKeepsOnlyWhatDiffersFromTheDefault();
    void closingAUniverseDropsOnlyItsOwnParameters();

    /* RDM (plugins/interfaces/rdmprotocol.cpp, shared with DMX USB): the
       commands we build, byte for byte, and the replies we read */
    void rdmCommandsAreBuiltToE120();
    void rdmSetCommandsCarryTheirValues();
    void rdmDiscoveryRepliesAreDecoded();
    void rdmMoreRepliesParse();

public slots:
    /* Not a test: connected to problemsChanged() by
       problemsChangedIsNeverEmittedUnderTheLock(). */
    void checkDataMutexIsFree();

private:
    ArtNetController *m_lockCheck = nullptr;
    int m_lockChecks = 0;
    int m_lockHeld = 0;
};

#endif
