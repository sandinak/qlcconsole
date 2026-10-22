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
#include <QHash>

class Doc;
class EosShowData;
class EosFixtureMapper;

/** @addtogroup engine Engine
 * @{
 */

/** Outcome of an EosImporter::build() / load() call, for the UI to summarize.
 *  Separate from QxwImportResult: this one covers the *parse* step
 *  (file-shaped-input → populated scratch Doc), not the merge step
 *  (scratch Doc → live Doc), which continues to use QxwImporter. */
class EosImportResult
{
public:
    int fixturesParsed = 0;
    int fixturesUnpatched = 0;       //!< Eos channels with no address: staged in the inventory
    int fixtureGroupsParsed = 0;
    int cueListsParsed = 0;          //!< one Chaser each (the Show Tree imports it)
    int cuesParsed = 0;              //!< one Scene each, the cue's full tracked state
    int submastersParsed = 0;        //!< Scenes created from Sub records
    int presetsParsed = 0;           //!< Scenes created from $Preset records
    int palettesParsed = 0;          //!< IP / CP / FP made into QLC palettes
    int magicSheetsParsed = 0;       //!< magic sheet XMLs successfully read
    int channelObjectsPlaced = 0;    //!< TARGETTYPE=20 placements into layers
    int fixturesUnmapped = 0;        //!< Eos channels patched as a generic dimmer
    QList<quint32> chasers;          //!< the cue lists' chasers, in list order (scratch ids)
    QStringList warnings;            //!< per-record skipped/flattened notes
};

/** Describes one Eos source bundle. The user picks these in the File → Import
 *  wizard before `load()` runs. */
class EosImportBundle
{
public:
    QString asciiPath;                //!< Single USITT ASCII (.asc). REQUIRED.
    QStringList magicSheetPaths;      //!< Zero-or-more magic sheet XMLs (phase 5).
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
 * What the scratch Doc holds (see EOS_IMPORT_DESIGN.md):
 *   - a fixture per patched Eos channel (ID = the channel number), its
 *     definition from EosFixtureMapper; an unaddressed channel is staged
 *     in the inventory;
 *   - a FixtureGroup per Eos group (folder "Eos");
 *   - IP / CP / FP palettes as Dimmer / Color / PanTilt palettes (the
 *     channels' most common value; BP and mixed ones reported);
 *   - per cue list a Chaser (folder "Eos") of one Scene per cue holding
 *     the cue's full tracked state, with the cue number, up / down times
 *     and follow / hang as the step's hold. The UI hands each chaser to
 *     the Show Tree's Import in place, captures the scenes into palettes
 *     and converts the list to tracking -- Eos tracks, the tree tracks;
 *   - a Scene per preset and per sub (folders "Eos/Presets", "Eos/Subs").
 *
 * Not carried, per v1 design: effects, curves, fader / magic-sheet
 * control objects, BP values, per-part cue timing (flattened), cue
 * delays and links (reported), multi-part fixtures' extra parts.
 */
class EosImporter
{
public:
    /**
     * Parse a bundle into a freshly-allocated scratch Doc (definition
     * caches loaded from the system and user folders). Ownership of the
     * returned Doc transfers to the caller. Returns nullptr on unrecoverable
     * parse failure (e.g. no readable ASCII file); recoverable issues go
     * into result.warnings and the Doc is returned populated with whatever
     * did parse. @a mapper: the wizard's (with the user's choices); null
     * reads the bundle's map file.
     */
    static Doc *load(const EosImportBundle &bundle, EosImportResult &result,
                     EosFixtureMapper *mapper = nullptr);

    /** Populate @a doc (empty, its definition cache loaded) from parsed
     *  @a data. @a showName names the cue lists' productions. */
    static bool build(const EosShowData &data, Doc *doc, const EosFixtureMapper &mapper,
                      EosImportResult &result, const QString &showName = QString());

    /** After the merge: bring the imported cue lists' @a chasers (ids in
     *  @a doc, the live show) into the Show Tree -- Import in place, each
     *  cue's scene captured into looks and palettes, the list converted to
     *  tracking -- all one undo step ("Import Eos show"). The new lists'
     *  ids; @a lines gets what happened, a line each. */
    static QList<quint32> toShowTree(Doc *doc, const QList<quint32> &chasers, QStringList *lines = nullptr);
};

/** @} */

#endif
