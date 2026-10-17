/*
  Q Light Controller Plus
  showtreecommand.h

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

#ifndef SHOWTREECOMMAND_H
#define SHOWTREECOMMAND_H

#include <QStringList>
#include <QString>

#include "qlcengine_export.h"
#include "showtreemodel.h"

class ShowTreeController;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The Show Tree's optional command line (SHOW_TREE_DESIGN.md §6C, D24):
 * a few words for what the mouse does, calling the same
 * ShowTreeController operations. Each command is one undo step; a command
 * that fails part way leaves the tree as it was.
 *
 *   cue 1.0.1 "Verse"            select cue 1.0.1, or make it
 *   cue + "Verse"                a new cue after the selected one (D23)
 *   track | block | cueonly | default | base     the selected cue's mode
 *   Towers colour Blue dimmer 45 a layer per group named, then its looks
 *   Towers+Steps colour Red      one layer on both groups
 *   cue look colour Red          a cue look
 *   Steps effect Sinelon colour Red              an effect with its own colour
 *   Towers gobo none             None: off here
 *   release Towers               toggle Release on the cue's Towers layer
 *   include 0.0.3                include a cue (again: take it out)
 *   record · dup · undo · redo   list "01-Opener"
 *
 * Names are the Library's (groups, palettes), matched whole and ignoring
 * case; quote one with spaces ("AT W ALL") or let Tab complete it.
 */
class QLC_ENGINE_EXPORT ShowTreeCommand
{
public:
    ShowTreeCommand(Doc *doc, ShowTreeController *controller);

    /** Where the command runs: the selected production, list and cue. */
    struct Context
    {
        quint32 production = 0xFFFFFFFF;
        quint32 list = 0xFFFFFFFF;
        quint32 cue = 0xFFFFFFFF;
    };

    struct Result
    {
        bool ok = false;
        QString message;
        /** Where the command leaves the selection. */
        Context context;
        /** The layer it set last (invalid: none), to select. */
        quint32 layer = 0xFFFFFFFF;
    };

    Result run(const QString &line, const Context &context);

    /** Tab: @a line with its last word completed as far as it is the same
     *  in every name that fits there; @a options lists those names (up to
     *  ten). */
    QString complete(const QString &line, const Context &context, QStringList *options = nullptr) const;

    /** The one-line grammar for the hint under the command line. */
    static QString grammar();

private:
    QStringList groupNames() const;
    QStringList paletteNames(const QString &key) const;
    QStringList cueNumbers(const Context &context) const;
    static QStringList keywords();
    static QStringList attributeWords();
    /** "colour" -> ShowTree::KeyColour; empty: not an attribute. */
    static QString attributeKey(const QString &word);

    Doc *m_doc;
    ShowTreeController *m_st;
};

/** @} */

#endif
