/*
  Q Light Controller
  qxwimporter.h

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

#ifndef QXWIMPORTER_H
#define QXWIMPORTER_H

#include <QList>
#include <QStringList>

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/** Outcome of a QxwImporter::import() call, for the UI to summarize. */
class QxwImportResult
{
public:
    int fixturesImported = 0;
    int fixturesRelocated = 0;   //!< address moved due to a DMX conflict
    int fixtureGroupsImported = 0;
    int functionsImported = 0;
    int palettesImported = 0;
    int fixturesPlaced = 0;         //!< brought their 2D-map position along
    int fixturesPowerPatched = 0;   //!< re-patched onto a power circuit
    int powerSourcesCreated = 0;    //!< distros created to hold them
    int layersCreated = 0;          //!< 2D-map layers created in the target
    int mapGroupsCreated = 0;       //!< 2D-map groups created in the target
    int idsRemapped = 0;         //!< fixture/function/group IDs that collided
    QStringList warnings;        //!< items skipped because they couldn't be placed
};

/**
 * Merges a chosen set of fixtures/fixture groups/functions from one Doc
 * (typically a scratch Doc loaded from a second .qxw file) into another,
 * already-open Doc -- unlike Doc::loadXML(), which assumes it's populating
 * an empty Doc and silently drops anything whose ID already exists there.
 *
 * Pulls in each selected item's full dependency closure automatically (a
 * selected Chaser brings its member Scenes; a selected RGBMatrix brings its
 * FixtureGroup and the fixtures in it), remaps any ID that would otherwise
 * collide in the target Doc (fixture IDs, function IDs and fixture group IDs
 * are three separate spaces), relocates a fixture to a free DMX address if
 * its original address is already occupied there, and rewrites every
 * cross-reference (SceneValue::fxi, ChaserStep::fid, RGBMatrix's fixture
 * group, FixtureGroup head assignments, Show/Track function references,
 * Scene palette references and their per-palette fade overrides) so the
 * imported content is internally consistent in its new home.
 *
 * Also carries each fixture's 2D-map placement (position, rotation, scale,
 * gel colour, zoom, flags) and its power feed, matching an existing power
 * source/circuit by name before creating one so importing the same rig twice
 * doesn't produce duplicate distros.
 *
 * 2D layer and group membership is translated, not copied: layers are matched
 * by name and groups by name within the same parent (creating either if
 * absent), so importing the same rig twice reuses them instead of duplicating.
 *
 * NOT carried: a group's truss/platform anchor and a fixture's rig props (both
 * reference structure ids this importer doesn't bring across), and a power
 * source's stage position, which is venue-fixed and belongs to the target's
 * plot. Layer visibility/lock are not copied either -- they're view state, and
 * a fixture arriving on a hidden layer reads as a failed import.
 */
class QxwImporter
{
public:
    /**
     * @param sourceDoc Where the selected items currently live (a scratch
     *        Doc loaded from the file being imported from).
     * @param targetDoc The live Doc to merge into.
     * @param fixtureIds Root fixture selection (from sourceDoc).
     * @param fixtureGroupIds Root fixture group selection (from sourceDoc).
     * @param functionIds Root function selection (from sourceDoc) -- each
     *        one's full dependency closure (member functions + fixtures they
     *        touch) is pulled in automatically.
     */
    static QxwImportResult import(Doc *sourceDoc, Doc *targetDoc,
                                   const QList<quint32> &fixtureIds,
                                   const QList<quint32> &fixtureGroupIds,
                                   const QList<quint32> &functionIds);
};

/** @} */

#endif
