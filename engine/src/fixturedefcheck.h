/*
  Q Light Controller Plus
  fixturedefcheck.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef FIXTUREDEFCHECK_H
#define FIXTUREDEFCHECK_H

#include <QByteArray>
#include <QString>
#include <QList>

class QLCFixtureDef;

/** What is wrong with a fixture definition, and how to put it right.
 *
 *  Two kinds of trouble. A FILE that cannot be read at all -- broken XML, a
 *  `\>` typed for `/>` -- is diagnosed where it breaks (diagnose()) and
 *  offered repairs that are tried before they are offered (repairs()). A
 *  definition that DOES load is checked for what is clearly wrong (errors:
 *  a head on a channel its mode does not have, capability ranges that
 *  overlap) and for what is missing (warnings: no size, no light output, no
 *  beam angle) -- the data the rig view and the effects draw from (check()).
 *
 *  The fixture editor shows all of it as you edit; the app's footer uses
 *  the file diagnosis for definitions that failed to load. */
namespace FixtureDefCheck
{
    /** Where a file stops being a definition. line/column are 1-based; 0
     *  when the problem is not at a place (an empty file, no DOCTYPE). */
    struct ParseProblem
    {
        QString message;
        int line = 0;
        int column = 0;
        /** It does load -- but parts of it are lost when it does: a mode
         *  naming a channel that does not exist, a mode or channel with no
         *  name, two channels with one name. Openable as it is. */
        bool loads = false;
        /** A few words, for a list: "line 12: expected '>' or '/'", "no modes". */
        QString brief() const;
    };

    /** False when @p xml loads as a fixture definition, whole; else true,
     *  with the first problem in @p problem -- including one that loads
     *  but loses parts of itself (ParseProblem::loads). */
    bool diagnose(const QByteArray &xml, ParseProblem *problem);

    /** A proposed repair: what it does, the first line it changes (before
     *  and after), and the whole repaired file. @a complete when the result
     *  loads as a definition; otherwise it only gets past this problem. */
    struct Repair
    {
        QString description;
        int line = 0;
        QString before;
        QString after;
        QByteArray fixed;
        bool complete = false;
        /** It cannot be wrong -- "\\>" is never anything but "/>" -- so it is
         *  applied without asking (autoRepair()). */
        bool certain = false;
    };

    /** Repairs for @p xml's first problem, each tried first: only those that
     *  get the file past where it broke are offered, best first. */
    QList<Repair> repairs(const QByteArray &xml);

    /** Apply every certain repair to @p xml, over and over while one still
     *  applies; @p applied gets what each did. Returns the result -- @p xml
     *  itself when there was nothing certain to do. */
    QByteArray autoRepair(const QByteArray &xml, QStringList *applied = nullptr);

    /** One thing wrong with, or missing from, a loaded definition. */
    struct Issue
    {
        enum Level { Warning, Error };
        enum Section { General, Physical, Channels, Modes };
        Level level = Warning;
        Section section = General;
        QString target;         ///< the channel or mode it is about, if any
        QString message;
    };

    /** Everything wrong with or missing from @p def, errors first. */
    QList<Issue> check(const QLCFixtureDef *def);
}

#endif
