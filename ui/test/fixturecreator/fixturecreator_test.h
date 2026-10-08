/*
  Q Light Controller Plus - Test Unit
  fixturecreator_test.h

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

#ifndef FIXTURECREATOR_TEST_H
#define FIXTURECREATOR_TEST_H

#include <QObject>

class Doc;

/** Drives the real "Add to inventory" dialog end to end and checks what it
 *  creates and where that shows up. */
class FixtureCreator_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void addToInventoryCreatesAnUnpatchedFixture();
    void aStagedFixtureAppearsInNoUniverseAnywhere();
    void aStagedFixtureNeverOccupiesAnAddress();
    void ordinaryAddStillPatches();
    void networkFixtureKeepsItsIp();
    void aMultiAddStagesEveryFixtureUnpatched();

    /** A run of a model whose name ends in digits numbers with a counter and
     *  never invents models (LM70, LM71, LM72...). */
    void aMultiAddOfANumberedModelAppendsACounter();
    /** An explicit "#N" counter in a typed name still continues (US #4, #5, #6). */
    void anExplicitCounterStillContinues();
    /** A taken default name gets a counter appended. */
    void aTakenDefaultNameGetsACounterNotBumpedDigits();

    void aPreselectedUniverseIsWhereTheFixtureLands();
    void anInvalidAddressIsRefusedWithAMessage();
    void aCancelledDialogCreatesNothing();
    void anEmptyNameFallsBackToTheModelOrGenericDimmer();
    void newFixturesJoinTheGroupTheyWereAddedFrom();
    void aFixtureTheDocRefusesIsNotLeaked();
    void aSingleAddWithATakenNameIsNumbered();

private:
    Doc *m_doc;
};

#endif
