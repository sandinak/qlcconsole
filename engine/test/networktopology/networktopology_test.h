/*
  Q Light Controller Plus - Test Unit
  networktopology_test.h

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

#ifndef NETWORKTOPOLOGY_TEST_H
#define NETWORKTOPOLOGY_TEST_H

#include <QObject>

class Doc;

class NetworkTopology_Test final : public QObject
{
    Q_OBJECT

private:
    Doc *m_doc;

private slots:
    void init();
    void cleanup();
    void defaults();
    void nodesAndPorts();
    void portForUniverse();
    void proximityAnchorWired();
    void proximityAnchorWireless();
    void proximityAnchorUnplaced();
    void transportMatchesPlugin();
    void warningsDuplicateUniverse();
    void warningsWirelessWithoutReceiver();
    void warningsCleanTopologyIsEmpty();
    void xmlRoundTrip();

    void runsFollowAddressOrderFromTheNode();
    void aWirelessRunStartsAtTheReceiverNotTheTransmitter();
    void networkAndStagedFixturesAreNotOnARun();
    void unplottedFixturesCountButHaveNoVertex();
    void anUnanchoredNodeClaimsNoPolylineOrLength();
    void unservedFixturesAreThoseOnNoPort();

    void aNodeWhoseTransportDisagreesWithItsUniversesOutputIsFlagged();
    void matchingOrOpenTransportsAreNotFlagged();
    void everyNodeFieldSurvivesAnXmlRoundTrip();
    void aWrongRootIsRejected();
    void unknownXmlIsSkippedNotFatal();
    void clearContentsResetsTheTopologyAndTheDocPersistsIt();
    void unusedPortsAndANullDocYieldNoRuns();
};

#endif
