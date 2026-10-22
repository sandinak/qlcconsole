/*
  Q Light Controller
  eosmagicsheetparser.h

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

#ifndef EOSMAGICSHEETPARSER_H
#define EOSMAGICSHEETPARSER_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QPointF>

/** @addtogroup engine Engine
 * @{
 */

/** One object on a magic sheet, decoded to the subset we care about.
 *  Only channel objects (TARGETTYPE=20) carry placement info usable for
 *  Lighting Studio; everything else is for cross-referencing. */
class EosMagicSheetObject
{
public:
    enum Kind {
        Channel = 20,        //!< TARGETLISTID = channel number
        Cue = 2,
        Group = 3,
        Macro = 4,
        Palette = 6,         //!< sub 1-4: IP / FP / CP / BP
        Preset = 9,
        NestedSheet = 33,
        Fader = 45,
        Unknown = 0
    };

    Kind kind = Unknown;
    int targetListId = 0;    //!< channel number for Channel; palette index etc. for Palette
    int paletteSubType = 0;  //!< only meaningful when kind == Palette (1=IP, 2=FP, 3=CP, 4=BP)
    QPointF position;        //!< sheet-local (x, y)
    QString label;
};

/** One parsed magic sheet. */
class EosMagicSheet
{
public:
    QString name;
    QString uid;             //!< used for calibration-persistence keying
    QList<EosMagicSheetObject> objects;
    int imagesDropped = 0;   //!< background images are not round-tripped by Eos
    int unknownKindsDropped = 0;
};

/**
 * Parser for Magic Sheet XML exports from the Eos Magic Sheet Editor
 * (settings tab → export icon). The XML schema is undocumented by ETC;
 * attribute set and TARGETTYPE enum values are per community reverse-
 * engineering in Codys-Wright/eos-toolkit.
 *
 * See EOS_IMPORT_DESIGN.md § "Magic sheet XML — what it buys you" for how
 * the output feeds Lighting Studio layer placement + the per-sheet
 * calibration step.
 */
class EosMagicSheetParser final
{
public:
    EosMagicSheetParser();
    ~EosMagicSheetParser();

    /**
     * Parse one magic sheet XML file. Returns a populated sheet on success,
     * or a default-constructed sheet with name empty on failure (caller
     * checks `sheet.name.isEmpty()`).
     *
     * Unknown TARGETTYPE codes are counted in `unknownKindsDropped` but do
     * not fail the parse. Image elements and arbitrary shapes are counted
     * in `imagesDropped`.
     */
    EosMagicSheet parse(const QString &path, QStringList &warnings);
};

/** @} */

#endif
