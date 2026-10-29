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
 *  the 2D map; everything else is counted for the report. */
class EosMagicSheetObject
{
public:
    enum Kind {
        Channel = 20,        //!< TARGETID = channel number
        Cue = 2,             //!< TARGETLISTID = cue list
        Group = 3,
        Macro = 4,
        Palette = 6,         //!< TARGETLISTID 1-4: IP / FP / CP / BP
        Preset = 9,
        Sub = 10,
        Effect = 13,
        NestedSheet = 33,
        Fader = 45,
        Unknown = 0
    };

    Kind kind = Unknown;
    int targetType = 0;      //!< as written (an unknown code stays here)
    QString targetId;        //!< the target's number: channel, group, cue...
    int targetListId = 0;    //!< palette type (1=IP, 2=FP, 3=CP, 4=BP) or cue list
    QString key;             //!< the object's KEY: ChannelButton, Button, Truss...
    QPointF position;        //!< sheet-local (x, y), the object's centre
    QString label;
};

/** One parsed magic sheet. */
class EosMagicSheet
{
public:
    QString name;            //!< the file's base name (the sheet's label isn't in the XML)
    QString uid;             //!< SHOWFILE_VERSION + name, for calibration keying
    QList<EosMagicSheetObject> objects;     //!< objects with a target
    int imagesDropped = 0;   //!< images, shapes and lines: no target (not round-tripped by Eos either)
    int unknownKindsDropped = 0;

    QList<EosMagicSheetObject> channels() const;
};

/**
 * Parser for Magic Sheet XML exports from the Eos Magic Sheet Editor
 * (background settings tab → export icon). The XML schema is undocumented by
 * ETC; the element / attribute set and TARGETTYPE enum values are per
 * community reverse-engineering in Codys-Wright/eos-toolkit (ms_gen.py):
 *
 *   MAGICSHEET SHOWFILE_VERSION / ETCGRAPHICSSCENE / ITEMLIST /
 *     ITEM KEY POSX POSY / ITEMDATA / RECT X Y W H,
 *       MAGICSHEET / TARGET TARGETTYPE TARGETID TARGETLISTID, TEXT STR
 *
 * See docs/design/EOS_IMPORT_DESIGN.md § "Magic sheet XML — what it buys you" for how
 * the output feeds 2D-map placement (+ the per-sheet calibration step).
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
     * not fail the parse. Items with no target (images, shapes, truss) are
     * counted in `imagesDropped`.
     */
    EosMagicSheet parse(const QString &path, QStringList &warnings);
    /** The same from XML already read; @a name names the sheet. */
    EosMagicSheet parseXml(const QByteArray &xml, const QString &name, QStringList &warnings);
};

/** @} */

#endif
