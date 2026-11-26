/*
  Q Light Controller Plus - Unit test
  deskrecord_test.h

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

#ifndef DESKRECORD_TEST_H
#define DESKRECORD_TEST_H

#include <QTemporaryDir>
#include <QObject>

class IOPluginStub;
class Scene;
class Doc;

/* This computer's desk (docs/design/CONTROL_SURFACES_EVAL.md §7, phase D):
   STOP, Grand Master, GO and Back kept with this computer by the device's
   identity, reached through the show's patch, never written into the show;
   the show's own triggers on its controls kept but not armed. */
class DeskRecord_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void stopFiresInEmptyShow();
    void stopMatrix_data();
    void stopMatrix();
    void neverWrittenToShow();
    void rebindsByIdentityOnRepatch();
    void twoSameNameDevicesWarn();
    void goTargetsRunTabListAndNamesIt();
    void learnRefusesWrongControlKind();
    void bothStopsLive();
    void sameControlDeskWins();
    void showFallbackOnlyForARoleThisComputerLacks();
    void perComputerTickTurnsOneRoleOff();
    void teacherLockFreezesRecord();
    void onePinUnlocksOneChange();
    void exportImportRoundTrip();
    void patchItIsNamedAndUndoable();
    void grandMasterPicksUpAfterLoadReplugWake();
    void reviewLinesSayWhatIsReachable();
    void removingThePatchedUniverseUnbinds();
    void learnNeverTakesAFaderPulledToZero();
    void anOpenLearnEndsAndTheDeskStillWorks();
    void learnRefusesTheShowsEmergencyStop();
    void reLearnedGrandMasterPicksUp();
    void patchUndoIsForgottenWithTheShow();
    void grandMasterRearmsOnlyForItsOwnDevice();
    void plugInAndOutSayChanged();
    void aLostStopReleaseNeverSwallowsTheNextPress();
    void learnPressesFireNothingOfTheShows();
    void aGrandMasterMadeLiveWaitsToPickUp();
    void passthroughUniverseIsNotLive();
    /** Phase R: a replug's state dump fires no desk STOP, GO or Back. */
    void aReplugDumpPressesNothingOfTheDesk();
    /** A key really held across a replug, resending 127 past the window:
     *  no GO until a release then a press. */
    void aKeyHeldAcrossAReplugGoesOnlyAfterARelease();
    void aPortDumpAsAShowLoadsNeverMovesTheGrandMaster();
    void aPortDumpOnPatchItNeverMovesTheGrandMaster();

private:
    /** One input value, through the input map (the dispatch point). */
    void send(quint32 universe, quint32 channel, uchar value);
    void tick(int n = 2);
    /** Learn @a role on @a channel of @a universe as a person does. */
    void learn(int role, quint32 universe, quint32 channel);
    Scene *scene(const QString &name);
    /** A production with lists "Act 1" and "Act 2", one cue each; @a act2
     *  gets the second list's id. Returns the first's. */
    quint32 twoLists(quint32 *act2);

    Doc *m_doc = nullptr;
    /** This test's own settings (never the person's, nor another leg's
     *  copy of this test running at the same time in the gate). */
    QTemporaryDir m_settings;
    IOPluginStub *m_stub = nullptr;
};

#endif
