/*
  Q Light Controller Plus
  callingscript.h

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

#ifndef CALLINGSCRIPT_H
#define CALLINGSCRIPT_H

#include <QString>
#include <QList>

#include "qlcengine_export.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The stage manager's calling script (0.5), laid out line by line from a
 * production's script text (ShowTree::CallingScript) and its cue lists, on
 * demand -- nothing here is stored. A line carries the markers in its
 * margin (a cue's standby or GO, with its number, name and note, read from
 * the cue as it is now) and whether it is a heading: a Markdown "#" line,
 * or a line that is a song section's title or name under the production's
 * numbering scheme. It prints through CueSheet's PDF writer.
 */
class QLC_ENGINE_EXPORT CallingScriptSheet
{
public:
    /** A cue called at a line. */
    struct Marker
    {
        bool go = true;                 //!< GO; false: standby
        quint32 cueId = 0xFFFFFFFF;
        QString number;                 //!< "1.0.2", a sub-cue's "4.1"
        QString name;
        QString note;
        int offset = 0;                 //!< where in the script text
    };

    struct Line
    {
        int start = 0;                  //!< offset of its first character
        QString text;
        bool heading = false;
        QList<Marker> markers;          //!< standby before GO
    };

    static CallingScriptSheet build(Doc *doc, quint32 productionId);

    const QString &title() const { return m_title; }
    const QList<Line> &lines() const { return m_lines; }
    bool isEmpty() const { return m_lines.isEmpty(); }
    /** The number of cues placed on the script. */
    int markerCount() const;
    /** The line holding character @a offset (-1: none). */
    int lineOfOffset(int offset) const;
    /** The line where cue @a cueId's GO is called (standby with @a go
     *  false); -1: not placed. */
    int lineOfCue(quint32 cueId, bool go = true) const;

    /** The printable page: the script with its margin. */
    QString toHtml(const QString &heading) const;
    /** Write it as a PDF (CueSheet::writePdf). */
    bool writePdf(const QString &path, const QString &heading, QString *error = nullptr) const;

private:
    QString m_title;
    QList<Line> m_lines;
};

/** @} */

#endif
