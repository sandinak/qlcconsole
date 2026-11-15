/*
  Q Light Controller Plus - Unit test
  showonlyreview5_test.h

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

#ifndef SHOWONLYREVIEW5_TEST_H
#define SHOWONLYREVIEW5_TEST_H

#include <QObject>

class Doc;
class Fixture;
class Universe;
class IOPluginStub;
class QLCFixtureDef;
class Chaser;
class Function;

/* Fifth Show-only review (of df050e0ce6): after an emergency stop and a GO
   on an unrelated list, nothing that GO does not drive reaches a Show-only
   channel. */
class ShowOnlyReview5_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void simpleDeskReleaseLeavesTheLaserAtSafe();
    void vcSliderLeftUpStaysHeldAcrossDesignAndOperate();
    void vcSliderLeftUpStaysHeldWhenItsInputEchoes();
    void audioTriggerArmedBeforeTheStopStaysHeld();

    /* The e-stop rules, swept over the hand controls and automatic widgets */
    void sweepSimpleDeskChannel();
    void sweepVcSlider();
    void sweepXYPad();
    void sweepAudioFunctionBar();
    void sweepAudioDmxBar();
    void sweepVcClockSchedule();

    /* Sixth review (of 1391a70ca1): automatic writers that come back after
       the stop, and a GO that should drive */
    void r6AudioWidgetFunctionBarAfterStop();
    void r6AudioBarMovesVcSliderAfterStop();
    void r6AudioBarPressesVcButtonAfterStop();
    void r6VcClockFirstScheduleAfterStop();
    void r6InputPassthroughThroughStop();
    void r6PlaybackSliderRestartsAfterStop();
    void r6ScriptLaunchedCueDrives();
    void r6ParkLoadedInOperateDrives();

    /* The sixth review's writers, swept: set up in Operate, held unless its
       own operator action in this session, held after All Off, and an
       unrelated GO wakes only its own list */
    void sweepR6RealAudioWidgetArmsOnlyByTheOperator();
    void sweepR6AudioBarStepsAVcCueList();
    void sweepR6VcClockArmedByInputOrWeb();
    void sweepR6PassthroughNeverDrives();
    void sweepR6PlaybackSliderMovedAfterTheStop();
    void sweepR6ScriptLaunchedCueAfterTheStop();
    void sweepR6ShowLoadedInOperateStartsASession();
    void sweepR6DeskStampIsForWhatIsPatched();

    /* Seventh review (of 66445810d0) */
    void r7SharedSceneLatchesAcrossADesignTrip();
    void r7DesignLeftoverLatchesAtOperate();
    void r7QueuedAllOffChildLatchesInTheGap();
    void r7LtpChannelKeptWhenItsCueLetsGo();
    void r7GoInBlindFiresWhenBlindEnds();
    void r7VcButtonFlashIsHeld();
    void r7CueStackStampSurvivesARepatch();
    void r7TimecodeCueListFiresUnarmed();
    void r7XYPadLoadedInOperateIsStamped();
    void r7AllOffRaceLeavesAnLtpChannelUp();

private:
    Fixture *addDimmer(const QString &name, int address, int rules);
    Fixture *addLaser(const QString &name, int address);
    void tick(int times);
    int sent(int address) const;
    void allOff();
    /** A GO on a new list that only lights the stage dimmer */
    void goUnrelated(int level);
    /** r7: a one-step list playing @p f, and a GO on it */
    Chaser *listOf(Function *f, const QString &name);
    void goList(Chaser *list);
    Fixture *addFogger(const QString &name, int address);

    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
    QLCFixtureDef *m_laserDef = nullptr;
};

#endif
