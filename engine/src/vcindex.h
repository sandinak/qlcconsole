/*
  Q Light Controller Plus - qlcconsole
  vcindex.h

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

#ifndef VCINDEX_H
#define VCINDEX_H

#include <QByteArray>
#include <QString>
#include <QList>
#include <QPair>
#include <QSet>

#include "qlcengine_export.h"

class QXmlStreamReader;

/** @addtogroup engine Engine
 * @{
 */

/**
 * One key or input a Virtual Console widget listens to: a button's key, a
 * Cue List's Next, a frame's page-forward input. @a role names the part of
 * the widget ("" for the widget itself, "Next", "Enable", "Playback"...).
 */
struct QLC_ENGINE_EXPORT VcBinding
{
    QString role;
    QString key;                //!< a QKeySequence in portable text, "" none
    bool hasInput = false;
    quint32 universe = 0;       //!< input universe, as saved (0 based)
    quint32 channel = 0;        //!< input channel, as saved (0 based)

    bool operator==(const VcBinding &o) const
    { return role == o.role && key == o.key && hasInput == o.hasInput
             && universe == o.universe && channel == o.channel; }
};

/** One Virtual Console widget, with only what Show Review reads. */
struct QLC_ENGINE_EXPORT VcWidget
{
    int parent = -1;                //!< index of its frame in VcIndex::widgets, -1 the top
    quint32 id = 0xFFFFFFFF;        //!< the widget's ID (invalid: the top frame)
    QString type;                   //!< the XML tag: "Button", "Slider", "CueList", "CueRunner", "Frame"...
    QString caption;
    int page = 0;                   //!< the page of its frame it sits on
    bool multipage = false;         //!< a frame with pages

    QString action;                 //!< Button: "Toggle", "Flash", "Blackout", "StopAll"...
    QString mode;                   //!< Slider: "Level", "Playback", "GrandMaster"...
    /** The function it runs: a Button's, a Matrix's, a Show Control's, a
     *  Slider's in Playback mode (invalid: none). */
    quint32 function = 0xFFFFFFFF;
    QList<quint32> hits;            //!< Button: the hits (Looks cues) it fires
    quint32 chaser = 0xFFFFFFFF;    //!< Cue List: the chaser it shows
    bool follows = false;           //!< Cue Runner: it follows a given list...
    quint32 follow = 0xFFFFFFFF;    //!< ...this one (not set: the current production)
    QList<quint32> functions;       //!< Speed Dial's functions, Clock's schedules
    QList<QPair<quint32, quint32>> levelChannels;   //!< Level slider: fixture id, channel
    quint32 group = 0xFFFFFFFF;     //!< Group Master slider: the fixture group
    QList<VcBinding> bindings;

    /** Every function the widget runs. */
    QList<quint32> allFunctions() const;
    /** What a person calls it: its caption, else its type. */
    QString label() const;

    bool operator==(const VcWidget &o) const;
};

/**
 * What the Virtual Console references, without the Virtual Console
 * (docs/design/SHOW_REVIEW.md section 5.3): widget -> functions, hits, cue
 * lists, keys, input channels, group masters. Plain structs.
 *
 * decided (can change): it lives in the engine and has one reader, for the
 * <VirtualConsole> element of a workspace. The command line reads the saved
 * file; the app asks the live Virtual Console to save itself into memory and
 * reads that, so the two can't disagree about what a widget saves.
 */
class QLC_ENGINE_EXPORT VcIndex
{
public:
    /** Whether a Virtual Console was read at all. A Doc with no source
     *  has none: a check that needs one stays quiet. */
    bool available = false;
    QList<VcWidget> widgets;        //!< depth first: a frame before its children

    /** Read the <VirtualConsole> element @a reader is on. */
    static VcIndex read(QXmlStreamReader &reader);
    /** The first <VirtualConsole> in @a xml (a workspace, or just it). */
    static VcIndex fromXml(const QByteArray &xml);
    static VcIndex fromFile(const QString &path);

    /** The ID given to the Virtual Console's own settings (the Grand
     *  Master's input) when they hold keys or inputs: type "Properties". */
    static constexpr quint32 propertiesId = 0xFFFFFFFE;

    /** Index in widgets of the widget with ID @a id (-1: none). */
    int indexOf(quint32 id) const;
    const VcWidget *widget(quint32 id) const;

    /** Every function a widget runs. */
    QSet<quint32> functionsUsed() const;
    /** Every hit a button fires. */
    QSet<quint32> hitsFired() const;
    /** Whether widgets @a a and @a b can both be live at once: not on
     *  different pages of one multi-page frame. */
    bool canBeLiveTogether(int a, int b) const;
    /** Where widget @a index is, for a sentence: "in 'Busk'". */
    QString place(int index) const;

    bool operator==(const VcIndex &o) const
    { return available == o.available && widgets == o.widgets; }
};

/** @} */

#endif
