/*
  Q Light Controller Plus
  whyon.h

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

#ifndef WHYON_H
#define WHYON_H

#include <QCoreApplication>
#include <QString>
#include <QList>
#include <functional>

#include "qlcengine_export.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * "Why is this on?" v1 (0.5): the answer to "this light is at 70 % -- why?",
 * worked out from what the engine did on the last frame.
 *
 * Asked for, never recorded: explain() runs on the main thread when someone
 * asks, copies the universe's fader list (Universe::faders(), under its
 * lock), copies the one FadeChannel of each fader that covers the channel
 * (GenericFader::peekAddress(), under that fader's lock) and the channel's
 * stage values (Universe::channelSnapshot(), under the frame lock), then
 * replays the merge the universe made -- in fader priority order, HTP / LTP /
 * Override / override-hit mix / effect mix / past the Grand Master -- and
 * names each source: the Show Tree cue and layer (and where the value
 * tracked in from), an override hit, an effect and its depth, Park,
 * Highlight, Flash, Focus, the Simple Desk, a VC widget, a preview. Then the
 * stages after the faders: group masters, the Grand Master, a channel
 * modifier, the input passthrough, Blind and Blackout.
 *
 * Nothing in the frame loop changes for it: when nobody asks, it costs
 * nothing. A value read between frames can be a frame old, and the blend
 * modes other than Normal are approximated; the answer says so
 * (Answer::approximate).
 */
class QLC_ENGINE_EXPORT WhyOn final
{
    Q_DECLARE_TR_FUNCTIONS(WhyOn)

public:
    /** What a line is about (and where clicking it goes) */
    enum class Kind
    {
        Cue,            //!< a Show Tree cue's layer (cueId, layerId)
        Hit,            //!< a Show Tree hit (cueId, layerId)
        Preview,        //!< the Show Tree's Design preview (cueId, layerId)
        Function,       //!< a function (functionId)
        Effect,         //!< an effect moving it (functionId: its scene)
        TrackedRestack, //!< a cue list's tracked state put back (functionId)
        Park,
        Highlight,
        Flash,
        Focus,
        LastLook,
        Locate,
        SimpleDesk,
        Programmer,
        VCWidget,       //!< widgetId
        Other,          //!< a source with no owner
        Nothing,        //!< nothing sets it
        GroupMaster,    //!< groupId
        GrandMaster,
        Modifier,
        Passthrough,
        Blind,
        Blackout
    };

    /** How a line bears on the value */
    enum class Verdict
    {
        Wins,       //!< this is what sets it
        Lower,      //!< HTP: asks for less than the winner
        Under,      //!< LTP: a later (higher) source sets it over this
        Replaced,   //!< an override above writes over it
        Scales,     //!< a master or stage after the faders changes it
        Blocks,     //!< nothing of it reaches the rig (Blind, Blackout)
        Info
    };

    /** How the lines are worded. Simple is the hook for Teaching mode: the
     *  same lines without the merge words (HTP, LTP). */
    enum class Wording { Crew, Simple };

    struct Line
    {
        Kind kind = Kind::Other;
        Verdict verdict = Verdict::Info;
        QString text;
        int value = -1;             //!< this source's own DMX value (-1: none)
        quint32 cueId = 0xFFFFFFFF;
        quint32 layerId = 0xFFFFFFFF;
        quint32 functionId = 0xFFFFFFFF;
        quint32 groupId = 0xFFFFFFFF;
        quint32 widgetId = 0xFFFFFFFF;
    };

    struct Answer
    {
        quint32 fixtureId = 0xFFFFFFFF;
        quint32 channel = 0xFFFFFFFF;
        quint32 universe = 0xFFFFFFFF;
        int address = -1;           //!< in the universe, 0-based
        QString subject;            //!< "Dim 1 · Intensity"
        QString headline;           //!< "At 50 %: cue 2 Two · layer A"
        uchar preGM = 0;
        uchar out = 0;              //!< after the masters (what goes out)
        bool sent = true;           //!< false: Blind or Blackout holds it back
        bool approximate = false;   //!< the replay can't be exact (see above)
        QList<Line> lines;          //!< the chain, top-down

        bool isValid() const { return address >= 0; }
        /** The headline and every line, one per line, as plain text */
        QString toText() const;
        /** The lines' texts, in order (for tests) */
        QStringList texts() const;
    };

    /** Why channel @a channel of fixture @a fixtureId is where it is. An
     *  invalid @a channel: the fixture's intensity (its master dimmer, else
     *  its first intensity channel, else its first channel). */
    static Answer explain(Doc *doc, quint32 fixtureId, quint32 channel = 0xFFFFFFFF,
                          Wording wording = Wording::Crew);

    /** The same for a bare address (the Simple Desk in universe mode):
     *  @a universe index, @a address 0-based in it. */
    static Answer explainAddress(Doc *doc, quint32 universe, int address,
                                 Wording wording = Wording::Crew);

    /** The channel explain() picks for a fixture when none is given */
    static quint32 defaultChannel(Doc *doc, quint32 fixtureId);

    /** The UI names Virtual Console widgets (the engine doesn't know them):
     *  widget id -> "VC button Go". Unset: "a Virtual Console widget". */
    static void setWidgetNamer(const std::function<QString(quint32)> &namer);

    /** "50 %" */
    static QString percent(int dmx);
};

/** @} */

#endif
