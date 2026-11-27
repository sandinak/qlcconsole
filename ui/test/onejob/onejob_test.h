/*
  Q Light Controller Plus - Unit test
  onejob_test.h

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

#ifndef ONEJOB_TEST_H
#define ONEJOB_TEST_H

#include <QObject>
#include <QTemporaryDir>

class IOPluginStub;
class Doc;

/* One job per control, and a control says what it does (ControlOwners::
   jobsFor()): the speed masters' Inputs… dialog, Choose… and Auto Detect
   name a control's current jobs -- this computer's desk roles and the
   surface pages' controls too -- and Move It Here takes a desk role. On the
   simulated APC40 mk2 (its Grand Master fader is seeded). */
class OneJob_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    /** jobsFor(): the desk's Grand Master, a surface page's control, free. */
    void aControlSaysEveryJobItHas();
    /** The desk's Grand Master fader given to a speed master's Tap: the box
     *  names "Grand Master", Auto Detect says it under the fields, Move It
     *  Here (on OK) clears the role. */
    void aDeskRoleIsNamedAndMovedFromTheSpeedMastersDialog();
    /** Choose… lists each control's job: "free", or what it does. */
    void chooseListsEachControlsJob();
    /** Move It Here's undo puts the desk role back as it was. */
    void movingADeskRoleCanBePutBack();
    /** The dialog opens on what is assigned: Tap, Rate and Pause filled in. */
    void theDialogOpensOnItsSavedInputs();

private:
    void plug();

    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
    QTemporaryDir m_settings;
    QTemporaryDir *m_userSets = nullptr;
    class SimMidiDevice *m_apc = nullptr;
};

#endif
