/*
  Q Light Controller Plus - Unit test
  beamocclusion_test.h

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

#ifndef BEAMOCCLUSION_TEST_H
#define BEAMOCCLUSION_TEST_H

#include <QObject>

class BeamOcclusion_Test final : public QObject
{
    Q_OBJECT

private slots:
    void aDrapeBlocksClosedNotOpenOrFlown();
    void aWallBlocksButNotOverTheTop();
    void aPlatformEdgeBlocksALowBeam();
    void aTowerBlocksAndThinPipesDoNot();
    void clearanceSaysHowCloseItPasses();
    void aHeadIsNotBlockedByItsOwnMount();
    void theStageSaysWhichKindsCount();
    void theModelIsRebuiltOnlyWhenTheStageChanges();
    void theSettingsAndTheOptionAreSaved();
    void aSceneAimSaysWhatIsInTheWay();
    void anAimLookDimsABlockedHeadOnlyWhenAsked();
    /** A line target: each head's own point on the line is the one checked,
     *  not the line's centre. */
    void aLineTargetChecksEachHeadsOwnPoint();
    /** A curtain driven by a fixture blocks as that fixture has it, live --
     *  the query, the Aim look's dimming and the model's rebuild. */
    void aDrivenCurtainBlocksAsItsFixtureHasIt();
    void aHundredHeadsAndFiftyFeaturesFitATick();
};

#endif
