/*
  Q Light Controller Plus
  focuscallsui_test.h

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

#ifndef FOCUSCALLS_UI_TEST_H
#define FOCUSCALLS_UI_TEST_H

#include <QTemporaryDir>
#include <QObject>
#include <QList>

class Doc;

class FocusCallsUi_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    /** The dialog edits the show's calls; with no venue, nothing is beside
     *  the show; with a new one, it is made and applied. */
    void theDialogEditsCallsAndPicksTheVenue();
    /** The venue manager: switch, rename and delete, the rig and the file
     *  following. */
    void theVenueManager();
    /** A venue needs the show saved first. */
    void aVenueNeedsASavedShow();
    /** Running calls, the panel says the call, the scope and where saves go,
     *  and the map uses Focus calls' words. */
    void thePanelShowsTheCall();
    /** Refine's panel: the cue and layer, the map in Refine's words, the
     *  keys doing what the controller does. */
    void theRefinePanel();
    /** A focus-call cue: the Run tab says it waits for the controller; its
     *  button starts the calls; leaving goes on. */
    void aFocusCallCueWaitsForTheController();
    /** Build's Workbench: Refine from its header and from a row. */
    void refineFromTheWorkbench();
    /** Feedback is one setting, on the panels, with Shift+W; W says the light. */
    void feedbackIsOneSettingOnEveryPanel();
    /** The voice: silent under a test, and what is announced reaches it
     *  while it is on. */
    void theVoiceHearsTheAnnouncements();

private:
    Doc *m_doc = nullptr;
    QTemporaryDir *m_dir = nullptr;
    QList<quint32> m_spots;
    quint32 m_group = 0;
    quint32 m_target = 0;
};

#endif
