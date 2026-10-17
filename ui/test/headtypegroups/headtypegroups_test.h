/*
  Q Light Controller Plus - Unit test
  headtypegroups_test.h

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

#ifndef HEADTYPEGROUPS_TEST_H
#define HEADTYPEGROUPS_TEST_H

#include <QTemporaryDir>
#include <QObject>
#include <QList>

class QLCFixtureDef;
class Fixture;
class Doc;

class HeadTypeGroups_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    /** A head's type is its colours plus whether it moves; said plainly. */
    void headTypesAreReadFromTheChannels();
    /** One group: every head of every fixture, a row per bar. */
    void oneGroupHoldsEveryHead();
    /** Eight bars of 16 RGB + 4 white: two groups, 128 and 32 heads, a row
     *  per bar. */
    void aGroupPerTypeAcrossTheBars();
    /** The same type on another model shares the group -- unless the models
     *  are kept apart. Names follow each group's own fixtures. */
    void sameTypeOnAnotherModelSharesItsGroup();
    /** A group per type per fixture: 16 groups for 8 bars, in the folder. */
    void aGroupPerTypeForEachFixture();
    /** Placed on the stage, they are laid out where they stand, not in the
     *  order picked. */
    void placedFixturesAreLaidOutByPosition();
    /** Single-head fixtures off the stage fill a near-square. */
    void singleHeadFixturesFillASquare();
    /** A given name names every group; duplicates are numbered. */
    void namesFromAGivenBase();
    /** create() makes the planned groups, with their paths and layouts, and
     *  the next plan knows they exist. */
    void createMakesThePlannedGroups();
    /** The dialog: options re-plan the list, names and ticks are kept,
     *  Create makes the ticked ones. */
    void dialogPreviewsAndCreates();
    /** From the Fixture Manager: groups selected bring their fixtures. */
    void fixtureManagerCreatesFromSelectedGroups();

private:
    QList<Fixture *> addBars(int count, const QString &prefix = QStringLiteral("Bar"));

    Doc *m_doc = nullptr;
    QLCFixtureDef *m_barDef = nullptr;     ///< 16 RGB + 4 white heads
    QLCFixtureDef *m_stripDef = nullptr;   ///< 8 RGB heads
    QLCFixtureDef *m_parDef = nullptr;     ///< one RGB head
    QTemporaryDir m_layouts;
};

#endif
