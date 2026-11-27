/*
  Q Light Controller Plus
  timelinefiledrop.h

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

#ifndef TIMELINEFILEDROP_H
#define TIMELINEFILEDROP_H

#include <QStringList>
#include <QList>

class Function;
class Doc;

/** What dropping audio and video files from the desktop onto a timeline
 *  does, shared by the Show Manager tab and the Timeline embedded in Build:
 *  which files are usable, and the Audio / Video functions made for them.
 *  Placing them on a track (and undo, where the host has it) is the host's. */
namespace TimelineFileDrop
{
    struct Result
    {
        /** One new, not-yet-added function per usable file, in the order given. */
        QList<Function *> made;
        /** "Can't use <file>: <why>" for each file that can't be used. */
        QStringList errors;
    };

    /** Make an Audio or Video function (named after its file, the file
     *  referenced where it is) for each of @p paths. */
    Result prepare(Doc *doc, const QStringList &paths);

    /** Lengths (ms) of the first few @p paths where they are known cheaply
     *  (audio); 0 for the rest. For the drop ghost. */
    QList<quint32> probeLengths(Doc *doc, const QStringList &paths);

    /** The status line after a drop that placed @p placed functions. */
    QString summary(const Result &r);
}

#endif
