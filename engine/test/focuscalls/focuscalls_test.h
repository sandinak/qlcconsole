/*
  Q Light Controller Plus
  focuscalls_test.h

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

#ifndef FOCUSCALLS_TEST_H
#define FOCUSCALLS_TEST_H

#include <QTemporaryDir>
#include <QObject>

class Doc;

class FocusCalls_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** One layout for every tool: the map by role, and back again. */
    void theControllerSchemeIsOneMap();
    /** A trigger is a button with hysteresis. */
    void aTriggerPressesOnce();
    /** The show's calls survive a save and load, in order. */
    void callsAreSavedWithTheShow();
    /** A call's fixtures are its groups' then its own, once each; its name
     *  is made from them and the target when it has none. */
    void aCallNamesItself();
    /** Triggers step calls, bumpers fixtures; each call aims at its own
     *  target; Y switches to the whole call and back. */
    void runningCallsStepsCallsAndFixtures();
    /** Group scope nudges, saves and undoes the whole call as one. */
    void groupScopeWorksTheWholeCall();
    /** What is said, and the flash's pattern. */
    void theOperatorIsToldWhereTheyAre();
    /** The venue sidecar: named after the show, written on every save,
     *  read back on open and applied -- and the show file never takes a
     *  venue's corrections. */
    void correctionsGoToTheVenueNotTheShow();
    /** Switching venues puts one venue's corrections in place of the
     *  other's; none is the show as built. */
    void switchingVenues();
    /** A venue nobody named gets the date; names stay unique. */
    void venueNames();
    /** Q111: a calibration changed elsewhere (the fixture dialog) while a
     *  venue is applied goes into the venue and its file, not the show; a
     *  Focus mode nudge only when saved. */
    void anEditElsewhereGoesToTheVenue();
    /** A focus-call cue: GO arms it (said once), Start begins the calls in
     *  Operate, triggers step, past the last call it asks to leave, and
     *  leaving goes on with the list. */
    void aFocusCallCueArmsThenStartsOnTheController();

private:
    Doc *m_doc = nullptr;
    QTemporaryDir *m_dir = nullptr;
};

#endif
