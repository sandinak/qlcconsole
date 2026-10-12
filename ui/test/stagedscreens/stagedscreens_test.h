/*
  Q Light Controller Plus - Test Unit
  stagedscreens_test.h

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

#ifndef STAGEDSCREENS_TEST_H
#define STAGEDSCREENS_TEST_H

#include <QObject>

class Doc;
class Fixture;

/**
 * The screens that list fixtures by universe and address. A staged
 * (inventory) fixture reports universe 1 / address 1 but owns neither, so
 * each must leave it out of address-based lists -- or, where it is designed
 * against, say it is unpatched -- rather than file it under universe 1.
 */
class StagedScreens_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void channelPickersListOnlyPatchedFixtures();
    void thePatchGridCountsOnlyPatchedFixtures();
    void theProgrammingSourceTreeFilesStagedFixturesUnderInventory();
    void aLookTargetSaysItIsUnpatched();
    void aGroupLayoutSaysAHeadIsUnpatched();

private:
    Doc *m_doc;
    Fixture *m_real;
    Fixture *m_staged;
};

#endif
