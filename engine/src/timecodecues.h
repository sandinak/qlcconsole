/*
  Q Light Controller Plus
  timecodecues.h

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

#ifndef TIMECODECUES_H
#define TIMECODECUES_H

#include <QObject>
#include <QString>
#include <QList>
#include "qlcengine_export.h"

class Doc;
class TimecodeSource;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Cues at a timecode ("cue 12 at 00:01:23:10"): a Show Tree cue list set to
 * follow timecode (ShowTree::CueList::followTimecode) GOes to each cue that
 * has a time (ShowTree::Cue::timecode) as the incoming timecode reaches it,
 * through ShowControl::run() with RouteTimecode -- the one GO path.
 *
 * decided (can change):
 *  - a jump forward fires only the latest cue it passed, not every one;
 *  - a jump back re-arms: cues after the new place fire again as it plays
 *    on, and none fires for the jump itself; a step back of under
 *    kJitterMs is jitter and ignored;
 *  - when timecode drops out nothing fires and nothing is forgotten: it
 *    carries on from where it was;
 *  - the first position seen fires only a cue exactly there: joining a
 *    show part-way does not replay what is already past;
 *  - a cue with no time, or skipped, is left to GO by hand. A GO by hand
 *    between timecode cues changes nothing here: the next cue still
 *    fires at its time, and it goes to the cue by number.
 */
class QLC_ENGINE_EXPORT TimecodeCues final : public QObject
{
    Q_OBJECT

public:
    /** A step back under this is jitter, not a jump back. */
    static const int kJitterMs = 250;

    TimecodeCues(Doc *doc, TimecodeSource *source);

    /** "hh:mm:ss:ff" at @a fps (30 when unknown). */
    static QString format(qint32 ms, int fps);
    /** Reads "hh:mm:ss:ff", "mm:ss:ff" or "ss:ff" (":", ";" or "." between);
     *  false for anything else. @a ms is rounded up to a whole frame so it
     *  reads back the same. */
    static bool parse(const QString &text, int fps, qint32 *ms);

    /** The next cue of the cue list played by chaser @a chaserId that has a
     *  time after the timecode now -- only while that list follows
     *  timecode. Its number, its time; false when there is none. */
    bool next(quint32 chaserId, QString *cue, qint32 *ms) const;
    /** Whether any cue list follows timecode (the Run tab shows nothing
     *  when none does). */
    bool anyFollowing() const;

signals:
    /** A cue was fired (after run() returned). */
    void fired(quint32 chaserId, const QString &cue);

private slots:
    void slotTime(quint32 ms);
    void slotTreeChanged() { m_dirty = true; }

private:
    struct Entry
    {
        qint32 ms;
        quint32 chaser;
        QString cue;
        int order;          //!< list, then cue: later wins a tie
    };
    void rebuild() const;

    Doc *m_doc;
    TimecodeSource *m_source;
    qint64 m_last = -1;     //!< the position before; -1: none yet
    mutable bool m_dirty = true;
    mutable QList<Entry> m_entries;
    mutable bool m_any = false;
};

/** @} */

#endif
