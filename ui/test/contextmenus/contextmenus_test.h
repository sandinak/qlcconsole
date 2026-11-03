/*
  Q Light Controller Plus
  contextmenus_test.h

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

#ifndef CONTEXTMENUS_TEST_H
#define CONTEXTMENUS_TEST_H

#include <QObject>
#include <QList>

class Doc;

/* The right-click canon (docs/design/CONTEXT_MENUS.md): the builder's
   sections and checks, and every migrated menu built in every place it
   opens -- a fixture group and a fixture give the same items, in the same
   order, wherever they are right-clicked. Drift fails here. Offscreen. */
class ContextMenus_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void sectionsLayOutInOrderWithOneSeparatorBetween();
    void theCanonCatchesDrift();
    void titleCase();

    void aFixtureGroupHasOneMenuEverywhere();
    void aFixtureHasOneMenuEverywhere();
    void theFixtureGroupItemsWork();
    void buildMenusFollowTheCanon();
    void theLibraryMenusFollowTheCanon();
    void thePlotMenusFollowTheCanon();
    void theFunctionsTabFollowsTheCanon();

    void aPaletteHasOneMenuEverywhere();
    void aLookRowPutsItsOwnItemsOnTop();
    void aFunctionHasOneMenuEverywhere();
    void aLayerHasOneMenuEverywhere();
    void theShowTreeMenusFollowTheCanon();
    void theRigViewFollowsTheCanon();

private:
    Doc *m_doc = nullptr;
    QList<quint32> m_fixtures;
    quint32 m_groupA = 0;
    quint32 m_groupB = 0;
};

#endif
