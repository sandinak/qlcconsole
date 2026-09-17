/*
  Q Light Controller Plus - Test Unit
  monitorproperties_test.h

  Copyright (c) Massimo Callegari

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

#ifndef MONITORPROPERTIES_TEST_H
#define MONITORPROPERTIES_TEST_H

#include <QObject>

class MonitorProperties_Test final : public QObject
{
    Q_OBJECT

private slots:
    void defaults();
    void fixtureItems();
    void genericItems();
    void layers();
    void layersXmlRoundTrip();
    void groupRegistry();
    void groupsXmlRoundTrip();
    void riserMount();
    /** Mounting a fixture must give it a plot item there and then -- without
     *  one it is invisible in the rig view AND the Layers tree. */
    void mountingAFixtureGivesItAPlotItem();
    /** A riser fixture's face and the plane it is drawn on are one fact stored
     *  twice; a file carrying them in disagreement is reconciled on load. */
    void riserFaceAndDrawingPlaneCannotDisagree();
    /** ...and setting the plane carries the face with it, so the repair above
     *  never has a legitimate edit to undo. */
    void settingAFaceTakesTheRiserFaceWithIt();
    void stageOrigin();
    void imagesXmlRoundTrip();
    void childBarFollowsParent();
    /** A tower's offset IS the height: on top of a 3 m tower is 3 m, whatever
     *  side the fixture was attached with. The half-width side nudge belongs
     *  to horizontal runs, where the width is vertical. */
    void aTowerOffsetIsTheHeight();
    void studioFrameDerivation();
    /** A target kind changes where light is aimed -- a drum kit is not a
     *  vocalist, and one global subject height made them the same. */
    void targetKindsAndAimPoint();
    /** A bound target follows its structure rather than remembering where it
     *  used to be. */
    void targetFollowsWhatItIsBoundTo();
    /** Kinds persist by name, so an unknown one degrades instead of becoming
     *  whatever now sits at that index. */
    void targetXmlRoundTrip();
    void studioFrameXmlRoundTrip();
    void studioFrameSlavedToPlatform();
    void reset();

    // Lighting Studio add/remove gate coverage (2026-08-15): Truss add/remove
    // and Platform add/remove/round-trip already existed above; Pipe/Stand/
    // Tower/StageTarget had none at all.
    void stageStructureAddRemove();
    void stageStructuresXmlRoundTrip();

    /** A fixture that has only ever been placed on the plot saves nothing it
     *  was never given: no fixed zoom, and not hidden. */
    void aNewlyPlacedFixtureSavesNoJunk();
};

#endif
