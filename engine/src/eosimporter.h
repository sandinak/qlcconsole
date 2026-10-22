/*
  Q Light Controller
  eosimporter.h

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

#ifndef EOSIMPORTER_H
#define EOSIMPORTER_H

#include <QString>
#include <QStringList>
#include <QList>

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/** Outcome of an EosImporter::load() call, for the UI to summarize.
 *  Separate from QxwImportResult: this one covers the *parse* step
 *  (file-shaped-input → populated scratch Doc), not the merge step
 *  (scratch Doc → live Doc), which continues to use QxwImporter. */
class EosImportResult
{
public:
    int fixturesParsed = 0;
    int fixtureGroupsParsed = 0;
    int cuesParsed = 0;              //!< loose Scenes created from Cue records
    int submastersParsed = 0;        //!< Scenes created from Sub records
    int palettesParsed = 0;          //!< palette shells; values usually empty
    int magicSheetsParsed = 0;       //!< magic sheet XMLs successfully read
    int channelObjectsPlaced = 0;    //!< TARGETTYPE=20 placements into layers
    int fixturesUnmapped = 0;        //!< Eos personalities with no QLC def match
    QStringList warnings;            //!< per-record skipped/flattened notes
};

/** Describes one Eos source bundle. The user picks these in the File → Import
 *  wizard before `load()` runs. */
class EosImportBundle
{
public:
    QString asciiPath;                //!< Single USITT ASCII (.asc). REQUIRED.
    QStringList magicSheetPaths;      //!< Zero-or-more magic sheet XMLs.
    QString augment3dEsfPath;         //!< Optional .esf3d for Augment3D. v2.
    QString fixtureMapPath;           //!< ~/.qlcconsole/eos-fixture-map.json
};

/**
 * Load an Eos show bundle (USITT ASCII + optional magic sheet XMLs +
 * optional Augment3D) into a scratch Doc, which the caller then passes to
 * the existing ImportSelectionDialog / QxwImporter pipeline for merging
 * into the live Doc. EosImporter DOES NOT touch the live Doc -- it only
 * produces the scratch Doc.
 *
 * See EOS_IMPORT_DESIGN.md for the architecture and format reality check.
 *
 * Not-carried, per v1 design:
 *   - Palette values beyond intensity (ASCII dialect doesn't emit them).
 *   - Effects (no clean 1:1 to QLC chaser/EFX).
 *   - Multi-part cue per-group timing (flattened to base time).
 *   - Magic sheet background images (not round-tripped by Eos itself).
 *   - Pixel maps.
 *   - Augment3D (v2).
 */
class EosImporter
{
public:
    /**
     * Parse a bundle into a freshly-allocated scratch Doc. Ownership of the
     * returned Doc transfers to the caller. Returns nullptr on unrecoverable
     * parse failure (e.g. no readable ASCII file); recoverable issues go
     * into result.warnings and the Doc is returned populated with whatever
     * did parse.
     *
     * @param bundle Input files + per-user fixture map path.
     * @param result Out-param: counters + warning list for UI reporting.
     */
    static Doc *load(const EosImportBundle &bundle, EosImportResult &result);
};

/** @} */

#endif
