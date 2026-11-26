/*
  Q Light Controller Plus - Unit test
  surfacepages_test.h

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

#ifndef SURFACEPAGES_TEST_H
#define SURFACEPAGES_TEST_H

#include <QTemporaryDir>
#include <QObject>

class Doc;
class IOPluginStub;
class SimMidiDevice;

/* Surface pages, slice 1 (docs/design/SURFACE_PAGES.md): the page set
   files and their checker, and the pages driven from the simulated boards
   (APC40 mk2, APC64, PMJ Black 1, MaizeDMX) as the devices send. */
class SurfacePages_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    /* The files */
    void everyBuiltInSetHoldsToItsBoard();
    void theApc40IsAsDecided();
    void theCheckerRefusesTheDesksControls();
    void theCheckerRefusesAJobAControlCantDo();
    void exportImportRoundTrip();
    void importRefusesABrokenSet();

    /* The boards */
    void theApc40SwitchesPages();
    void theApc64EncoderStepsPages();
    void thePmjModeKeysPickThePage();
    void theMaizeDmxFadersAreColour();
    void aKnobTurnsAndItsNeighbourTurnsFine();
    void aFaderIsALevelAndATouchIsNot();
    void operateReleasesTheControlsToTheShow();
    void theShowsEncoderBankOnTheBoardWins();
    void aShowOffersTheSetItWasBuiltWith();

    /* Review fixes (729e6a167007) */
    void aKnobRiddenElsewhereTurnsFromWhereItIs();
    void aKeyHeldIntoOperateIsLetGo();
    void thePageKeysLightsAreThePagesInDesignOnly();
    void aHandEditedSetWithTheDesksControlsIsRefused();
    void thePagesRestWhileALearnListens();
    void anEmptyPageSaysComing();

private:
    SimMidiDevice *plug(const QString &name, const QString &profile, const QString &file, quint32 universe);

    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
    QList<SimMidiDevice *> m_boards;
    QTemporaryDir m_settings;
    QTemporaryDir *m_userSets = nullptr;
};

#endif
