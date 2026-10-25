/*
  Q Light Controller Plus
  venuerigui_test.h

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

#ifndef VENUERIGUI_TEST_H
#define VENUERIGUI_TEST_H

#include <QTemporaryDir>
#include <QObject>

class Doc;

class VenueRigUi_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** A group's editor sets its owner; with a venue applied, its fixtures
     *  go into the venue. */
    void aGroupsOwnerIsSetInItsEditor();
    /** The venue page: the slots, the patch buttons, what is not ours, the
     *  clash list, the house rig. */
    void theVenuePageShowsTheUniverses();
    /** The Focus calls dialog's venue manager opens it. */
    void theVenueManagerOpensIt();
    /** The Connections toolbar's Slot picker sets the universe's slot. */
    void theConnectionsSlotPicker();
    /** Apply now on a venue that clashes asks first: Cancel leaves it off
     *  and the patch alone; Apply anyway goes live, the clash unpatched. */
    void aClashingVenueAsksBeforeItIsApplied();

private:
    Doc *m_doc = nullptr;
    QTemporaryDir *m_dir = nullptr;
};

#endif
