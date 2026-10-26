/*
  Q Light Controller Plus - Test Unit
  fixtureuniversepatch_test.h

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

#ifndef FIXTUREUNIVERSEPATCH_TEST_H
#define FIXTUREUNIVERSEPATCH_TEST_H

#include <QObject>

class Doc;

/* "right click on universe in fixtures list should let me patch": the
   Fixture Manager's universe rows patch and change their output in the
   Connections tab's connection window (board Q153), and unpatch through
   its own code (UniversePatcher). */
class FixtureUniversePatch_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void anUnpatchedUniverseOffersPatchOutput();
    void patchingFromTheMenuSetsTheOutputAndCanBeUndone();
    void aPatchedUniverseSaysWhereItGoesAndCanBeUnpatched();
    void severalSelectedArePatchedInTheOrderTheyWerePicked();
    void severalSelectedAreUnpatchedInOneStep();
    void aFixtureRowsMenuIsUnchanged();
    void showInConnectionsHandsOverTheUniverse();

private:
    Doc *m_doc;
};

#endif
