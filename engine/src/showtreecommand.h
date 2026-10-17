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
#include <functional>

#include "qlcengine_export.h"
#include "showtreecontroller.h"
#include "showtreemodel.h"

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
 *   Towers colour Blue dimmer 45 a layer on Towers, then its looks; a value
 *                                typed reuses the palette that plays exactly
 *                                that, else makes one named as the show's are
 *   Towers Steps colour Red      ONE layer on both groups (as a drop is);
 *   Towers+Steps colour Red      the same
 *   Towers, Steps colour Red     a layer EACH (as an ⌥-drop is)
 *   cue look colour Red          a cue look
 *   Steps effect Sinelon colour Red              an effect with its own colour
 *   Towers gobo none             None: off here
 *   T1-CW colour Red             a fixture by name works as a group does;
 *   Movers/Front/Spots …         a group also by its folder path
 *   function Haze look           a function played as it is (opaque layer)
 *   release Towers               toggle Release on the cue's Towers layer
 *   include 0.0.3                include a cue (again: take it out)
 *   record [selected|running|lit]   Record as new cue: the setting says
 *                                which when the choices differ (or asks)
 *   list "01-Opener"             select that cue list, or make it
 *   set "Towers" SL SR           a group set (Make set), or its members again
 *   US in Steps colour Red       a child layer of the cue's Steps layer (§3.5)
 *   Towers aim DS-Edge spread 0 group pairs cross mirror
 *                                a line aim and its parameters (§3B)
 *   cue look spread +5           a cue's Spread: line aims only (D18)
 *   name "Hero"                  name the selected layer (or cue); "" = automatic
 *   dup · undo · redo
 *
 * Names are the Library's (groups, fixtures, palettes, functions), matched
 * whole and ignoring case; quote one with spaces ("AT W ALL") or let Tab
 * complete it -- Tab completes every word the grammar takes there.
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
        /** The layer selected in the cue (invalid: none): what `name` names. */
        quint32 layer = 0xFFFFFFFF;
        /** The Library's Recent palettes, newest first: a typed value
         *  reuses one of these before any other (after In this cue list). */
        QList<quint32> recentPalettes;
    };

    struct Result
    {
        bool ok = false;
        QString message;
        /** `help`: the text to show (rich text; an example is a link
         *  "ex:<command>"). Empty for other commands. */
        QString help;
        /** Where the command leaves the selection. */
        Context context;
        /** The layer it set last (invalid: none), to select. */
        quint32 layer = 0xFFFFFFFF;
    };

    Result run(const QString &line, const Context &context);

    /** Tab: @a line with its last word completed as far as it is the same
     *  in every word that fits there (keywords, cue numbers, cue lists,
     *  groups and their folder paths, fixtures, palettes of the attribute
     *  before, functions, cue modes, record choices); @a options lists them
     *  (up to ten). A name with spaces completes as it can be typed; one
     *  begun with a quote is closed. */
    QString complete(const QString &line, const Context &context, QStringList *options = nullptr) const;
    /** Tab pressed in the command line: complete() the first time; pressed
     *  again on what it gave, the next of several matches in turn (and
     *  round). @a options: every match; @a current: the one shown (-1: the
     *  common part). */
    QString tab(const QString &line, const Context &context, QStringList *options = nullptr,
                int *current = nullptr);
    /** The words that may come after @a head (a whole number of words) --
     *  for the tests; false when @a head doesn't parse. */
    bool candidates(const QString &head, const Context &context, QStringList *out) const;

    /** The one-line grammar for the hint under the command line. */
    static QString grammar();

    /** One word of the language, as `help` explains it -- the table the
     *  parser's words come from (keywords(), attributeWords()), so help
     *  can't drift from the grammar. */
    struct Word
    {
        QString word;
        QStringList aliases;
        QString group;          //!< navigate, build, looks, tracking, record, undo, help
        bool attribute = false; //!< after a target: an attribute word
        QString syntax;         //!< optional parts in [ ], alternatives with |
        QString what;
        QStringList examples;   //!< each a whole command that parses
        QStringList related;
    };
    static const QList<Word> &words();
    /** The entry for @a word (or an alias); null: none. */
    static const Word *word(const QString &word);
    /** `help`: every word, grouped, a line each; `help <word>`: its syntax,
     *  what it does, examples (links "ex:…") and related words. Rich text. */
    static QString helpText(const QString &word = QString());
    /** The word nearest @a typed (a misspelling, a prefix): empty when none
     *  is near. For "did you mean". */
    static QString nearestWord(const QString &typed, bool attributesOnly = false);

    /** What ⌘C / `copy` holds: cues, or layers (cue, layer). Shared by the
     *  tab's keys and the command line. */
    struct Clipboard
    {
        QList<quint32> cues;
        QList<QPair<quint32, quint32>> layers;
        bool isEmpty() const { return cues.isEmpty() && layers.isEmpty(); }
    };
    Clipboard clipboard;

    /** How a bare "record" picks what to record when the choices differ
     *  (@a choices: ShowTreeController::recordChoices, two or more): the
     *  view's setting, or its dialog. Returns false to cancel. Without one,
     *  the first choice (the selected cue). */
    typedef std::function<bool(quint32 cueId, const QList<ShowTreeController::RecordSource> &choices,
                               ShowTreeController::RecordSource *chosen)> RecordChooser;
    void setRecordChooser(const RecordChooser &chooser) { m_recordChooser = chooser; }

private:
    /** What Tab would do with @a line: the text before the word, the
     *  matches (sorted), and whether the word was begun with a quote. */
    struct Completion
    {
        QString head;
        QString partial;
        QStringList matches;
        bool quoted = false;
    };
    Completion completion(const QString &line, const Context &context) const;
    /** A match as it goes in the line: quoted when it must be. */
    static QString typed(const QString &name, bool quoted);

    QStringList groupNames() const;
    QStringList setNames() const;
    /** Stage targets (points, areas, lines) by name: what `aim` takes. */
    QStringList stageTargetNames() const;
    /** Groups (and, for those in a folder, "Movers/Front/Spots"), then
     *  fixtures: every name a layer can be made on. */
    QStringList targetNames() const;
    QStringList functionNames() const;
    QStringList listNames(const Context &context) const;
    QStringList paletteNames(const QString &key) const;
    QStringList cueNumbers(const Context &context) const;
    static QStringList keywords();
    static QStringList attributeWords();
    /** "colour" -> ShowTree::KeyColour; empty: not an attribute. */
    static QString attributeKey(const QString &word);

    Doc *m_doc;
    ShowTreeController *m_st;
    RecordChooser m_recordChooser;

    // Tab pressed again: the next match.
    QString m_tabLast;
    QString m_tabHead;
    QStringList m_tabMatches;
    bool m_tabQuoted = false;
    int m_tabIndex = -1;
};

/** @} */

#endif
