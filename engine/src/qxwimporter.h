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
 * group, FixtureGroup head assignments, Show/Track function references) so
 * the imported content is internally consistent in its new home.
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
