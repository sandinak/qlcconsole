/*
  Q Light Controller Plus - qlcconsole
  osc_test.h

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

#ifndef OSC_TEST_H
#define OSC_TEST_H

#include <QObject>

/* Hostile network input to the OSC plugin: what one UDP datagram from
   anyone on the show network can do. Loopback only. */
class OSC_Test final : public QObject
{
    Q_OBJECT

private slots:
    void validMessagesStillParse();
    void validBundlesStillParse();

    void aBundleWithANegativeSizeEnds();
    void aBundleWithAHugeSizeEnds();
    void aBundleCutInsideASizeIsDropped();
    void tagsWithNoEndAreDropped();
    void aStringWithNoEndIsDropped();
    void outOfRangeNumbersAreClamped();
    void randomDatagramsNeverCrash();

    void theInputListensOnlyOnItsInterface();
    void manyPathsDoNotGrowMemoryWithoutBound();
    void aLoopbackSenderDrivesTheInput();

    /* What it sends and where (coverage campaign): to sockets of the test's
       own on 127.0.0.1, nothing else. */
    void dmxAndGenericMessageLayout();
    void universeDefaultsAndTheirReturns();
    void onlyChangedChannelsAreSent();
    void feedbackKeepsMultiValuePaths();
    void multiValueInputIsSplitPerValue();
};

#endif
