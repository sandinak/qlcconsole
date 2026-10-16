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
};

#endif
