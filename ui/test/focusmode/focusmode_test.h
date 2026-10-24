/*
  Q Light Controller Plus
  focusmode_test.h

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

#ifndef FOCUSMODE_TEST_H
#define FOCUSMODE_TEST_H

#include <QObject>
#include <QList>

class IOPluginStub;
class Doc;

/* The Focus mode panel's controls map and its choice of device, with the
   engine's test plugin standing in for a gamepad. */
class FocusMode_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void cleanupTestCase();

    /** The controller map labels each control by what its role does here,
     *  with the profile's own name, and greys what the profile lacks. */
    void theMapShowsTheProfilesBindings();
    /** Controller when one is connected (and the keyboard was not the last
     *  choice), else the keyboard -- in the setup and the panel. */
    void theDefaultFollowsTheDevice();
    /** On the keyboard the controller moves nothing. */
    void theKeyboardIgnoresTheController();
    /** A button, a stick, a key: each lights its place on the map. */
    void pressingLightsTheControl();
    /** The controller unplugged mid-session: the keyboard takes over, and
     *  it is offered back when it returns. */
    void aDisconnectFallsBackToTheKeyboard();
    void aPadThatSleepsIsPickedUpAgain();
    /** A paused controller is said, with why -- not silence -- and the
     *  panel stays up when another app is in front. */
    void aPausedControllerSaysWhy();
    /** The setup's Controller test lists each raw input once, with what it
     *  maps to, and the log has it all. */
    void theControllerTestListsEachInput();
    /** The save-status lamp and its words: green all saved, yellow this
     *  light, red another, a red ring round yellow for both; a save flashes
     *  it green. */
    void theLampSaysWhatIsUnsaved();
    /** Leaving with unsaved lights names them. */
    void exitNamesTheUnsavedLights();
    /** With FOCUSUI_SHOTS set: the panel, both maps, in each theme, saved
     *  there (for the docs and a look). */
    void shots();

private:
    void plugInPad();
    void unplugPad();

    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
    QList<quint32> m_heads;
    quint32 m_target = 0;
};

#endif
