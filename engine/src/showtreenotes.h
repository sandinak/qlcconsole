/*
  Q Light Controller Plus
  showtreenotes.h

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

#ifndef SHOWTREENOTES_H
#define SHOWTREENOTES_H

#include <QString>
#include <QList>

#include "qlcengine_export.h"

class QXmlStreamReader;
class QXmlStreamWriter;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Notes beside the Show Tree (Q212): dated entries, like tech notes, kept on
 * a production, a cue list, a cue, a layer or a Library palette. Each may
 * hold a snapshot of the view -- the rig view's camera, a small picture and
 * the cue that was on stage -- so "what I was thinking" comes back with what
 * was being looked at. Plain values; ShowTreeController keeps them, one undo
 * history with the tree.
 */
namespace ShowTree
{
    /** What a note is on. */
    enum class NoteKind { Production, List, Cue, Layer, Palette };
    QLC_ENGINE_EXPORT QString noteKindToString(NoteKind kind);
    QLC_ENGINE_EXPORT NoteKind stringToNoteKind(const QString &str, bool *ok = nullptr);

    /** The rig view as it was when a note was written (StructureStudioView's
     *  camera): its plane, quarter turns, the angled camera, the zoom and pan
     *  (pixels per metre, where the world's zero was on screen, and the
     *  view's size then, so a view of another size keeps the same middle),
     *  the cue on stage and the picture (an id of
     *  ShowTreeController::noteImage()). */
    struct QLC_ENGINE_EXPORT NoteView
    {
        bool valid = false;
        int plane = 3;                  //!< StructureStudioView::Plane (3: Angled)
        int rotation = 0;
        double azimuth = 20.0;
        double elevation = 30.0;
        bool zoomed = false;            //!< zoomed or panned by hand; else fitted to the rig
        double scale = 60.0;
        double originX = 0.0;
        double originY = 0.0;
        int width = 0;
        int height = 0;
        quint32 cue = 0xFFFFFFFF;       //!< on stage then (invalid: nothing)
        QString cueLabel;               //!< "1.0.2 Opener", as it read then
        QString image;                  //!< the picture's id (empty: none)

        bool operator==(const NoteView &o) const;
        bool operator!=(const NoteView &o) const { return !(*this == o); }
    };

    struct QLC_ENGINE_EXPORT Note
    {
        quint32 id = 0xFFFFFFFF;
        NoteKind kind = NoteKind::Cue;
        quint32 target = 0xFFFFFFFF;    //!< the production, list, cue, layer or palette
        quint32 cue = 0xFFFFFFFF;       //!< a layer's cue
        QString text;
        QString author;
        QString when;                   //!< ISO date time; empty: before notes had dates
        bool done = false;
        /** The cue's own note (Cue::note) as an entry: its text is the cue
         *  note's, kept in step both ways. One per cue at most. */
        bool cueNote = false;
        NoteView view;

        bool operator==(const Note &o) const;
        bool operator!=(const Note &o) const { return !(*this == o); }
    };

    QLC_ENGINE_EXPORT void saveXML(QXmlStreamWriter *doc, const Note &note);
    /** Read a <Note> (the reader on its start element). */
    QLC_ENGINE_EXPORT bool loadXML(QXmlStreamReader &root, Note &note);

    /** Newest first; entries without a date (a cue note from before) last. */
    QLC_ENGINE_EXPORT bool noteNewerThan(const Note &a, const Note &b);
}

/** @} */

#endif
