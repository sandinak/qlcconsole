/*
  Q Light Controller Plus
  showtreeview_test.h

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

#ifndef SHOWTREEVIEW_TEST_H
#define SHOWTREEVIEW_TEST_H

#include <QObject>

class FixtureGroup;
class Doc;

class ShowTreeView_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void theTreeShowsProductionsListsCuesAndLayers();
    void trackedLayersShowGreyWithWhereTheyCameFrom();
    void groupsDroppedOnACueBecomeLayers();
    void groupsDroppedOnALayerJoinIt();
    void aPaletteOnTheCueIsACueLookAndTheHintSaysWhatItReaches();
    void layerRowsShowOwnAndInheritedValues();
    void beamRowsSayWhatTheyApplyTo();
    void goboLooksMatchByNamePerType();
    void oneOffValuesGoToRecentNotAll();
    void noneAndClear();
    void cueFieldsEditTheCue();
    void undoAndRedoFromTheView();
    void theLibraryFilters();
    void theLibraryShowsGroupsInTheirFolders();
    void importingAChaserSelectsTheNewList();
    void inlinePalettesStayOutOfThePaletteTree();
    void dropsOnTheTreeWidget();
    void anEffectGetsLooksOfItsOwn();
    void theLookEditorEditsARowsPalette();
    void selectingACuePreviewsItOnTheRig();
    void inDesignTheEditShowsOverItsRunningList();
    // P2: tracking in the tab
    void trackingControlsOnTheListAndCuePages();
    void includesAddMoveRemoveAndNoLoops();
    void nextCueReceivesSaysWhatPassesOn();
    void aCarriedInLayerCanBeReleased();
    void updateCueWhereATrackedValueWasSet();
    void recordAsNewCueFromTheHeader();
    void theLibraryShowsWhatTheListUses();
    void theCommandLine();
    // P2 answers (2026-10-17)
    void recordAsksWhenTheChoicesDiffer();
    void severalGroupsOneLayerOrEach();
    void aPickedValueReusesAPalette();
    void pictureOfARealShow();
    void aDimmedColourOnPixelBarsReachesTheUniverse();

private:
    quint32 dimmer(int level, const QString &name);
    quint32 colour(const QColor &c, const QString &name);
    FixtureGroup *group(const QString &name, int from, int count);

    Doc *m_doc = nullptr;
    QList<quint32> m_fixtures;
};

#endif
