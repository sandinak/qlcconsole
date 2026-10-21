/*
  Q Light Controller Plus
  usedby.h

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

#ifndef USEDBY_H
#define USEDBY_H

#include <QStringList>
#include <QList>
#include <QSet>

class VCButton;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * "Used by" (PROGRAMMING_ROLE_DESIGN.md P-D13), one answer for every tab:
 * PaletteOps::usage() (cues, busking looks, reusables, scenes, effect slots)
 * plus what only the UI knows -- the Virtual Console buttons that fire
 * those. The Show Tree's Library and Look Editor line, its look page, and
 * the delete questions here and in the Programming tab all read it.
 */
namespace UsedBy
{
    /** The VC buttons whose function is one of @a functionIds. */
    QList<VCButton *> vcButtons(const QSet<quint32> &functionIds);
    /** The same, as a person reads them: "VC button “Hit 1”". */
    QStringList vcLines(const QSet<quint32> &functionIds);

    /** Where palette @a paletteId is used: PaletteOps::usage()'s lines, then
     *  the VC buttons firing any of them. */
    QStringList paletteLines(Doc *doc, quint32 paletteId);
    /** "2 cues, 1 look · 1 VC button" (empty: not used). */
    QString paletteSummary(Doc *doc, quint32 paletteId);

    /** The question a palette delete asks: what goes, and for each where it
     *  is used (named by cue and look, never a generated scene). */
    QString deletePalettesQuestion(Doc *doc, const QList<quint32> &paletteIds);
}

/** @} */

#endif
