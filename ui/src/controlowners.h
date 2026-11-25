/*
  Q Light Controller Plus
  controlowners.h

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

#ifndef CONTROLOWNERS_H
#define CONTROLOWNERS_H

#include <QCoreApplication>
#include <QSharedPointer>
#include <QPointer>
#include <QString>
#include <QList>
#include <QSet>

#include "controlmap.h"
#include "qlcui_export.h"

class QLCInputSource;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Who listens to a control (docs/design/CONTROL_SCOPES.md §1, §8 phase 1,
 * board Q297): the one answer every Learn asks before it takes a MIDI note
 * or CC, an OSC message or a computer key. The listeners are the Control
 * Map's triggers and its emergency stop, the encoder bank, every Virtual
 * Console widget's external inputs, the speed masters' and the Grand
 * Master's inputs, the channel groups' inputs and the PMJ Black 1 overlay.
 *
 * One job per control: a control another listener has is refused at Learn,
 * naming it (refusal()), with Move It Here (release()) -- except where the
 * owner can't let go: the PMJ's controls come with its profile, the encoder
 * bank's runs (its encoders, pushes and bank buttons) are set up together,
 * and nothing takes the emergency stop's control (its own Learn can take a
 * control from anything except those). In a window that applies on OK, Move
 * It Here takes the control only when it is accepted.
 *
 * Every owner has a place. A Control Map trigger has its own (a cue, a cue
 * list, a production or everywhere: phase 3, Q295, Q296); every other
 * listener is everywhere. Two triggers at different places share a control
 * (the narrowest armed one fires), so only two at the same place collide --
 * and those may step through instead (Q303). A trigger and any other
 * listener always collide.
 *
 * Reads the models live and keeps nothing (GUI thread only).
 */
class QLC_UI_EXPORT ControlOwners final
{
    Q_DECLARE_TR_FUNCTIONS(ControlOwners)

public:
    /** The listeners. */
    enum Kind
    {
        Trigger,        //!< a Control Map binding
        EmergencyStop,  //!< the Control Map's emergency stop
        Encoder,        //!< a control of the encoder bank's device
        VCInput,        //!< one external input of a Virtual Console widget
        SpeedMaster,    //!< a speed master's Tap, Rate or Pause input
        GrandMaster,    //!< the Grand Master's input
        ChannelGroup,   //!< a channel group's input
        Pmj             //!< the PMJ Black 1 overlay (fixed by its profile)
    };

    /** The encoder bank's parts that listen (its rings and lamps are
     *  feedback lines and hear nothing). */
    enum EncoderPart { EncoderTurn, EncoderPush, BankNext, BankPrevious, BankDirect };

    struct Owner
    {
        Kind kind = Trigger;
        /** Where it is live (Q295): a trigger's own; everywhere for the rest. */
        ControlMap::Place place;
        /** What it does, as a person reads it ("Scene: Warm wash",
         *  "Speed master 1 Tap"). */
        QString thing;
        /** Where it is set up ("Control Map", "Virtual Console"). */
        QString where;
        /** Where to go to change it, for the "Who has it now" list. */
        QString fix;
        /** false: it can't give the control up (refusal() says why). */
        bool movable = true;
        QString fixedWhy;

        /* Which one: per kind */
        ControlMap::Target target;      //!< Trigger, EmergencyStop
        int part = -1;                  //!< Encoder: EncoderPart; SpeedMaster: its Control
        int index = -1;                 //!< Encoder: slot; SpeedMaster: master; ChannelGroup: id
        QPointer<QObject> widget;       //!< VCInput: the VCWidget
        quint8 inputId = 0;             //!< VCInput
        QSharedPointer<QLCInputSource> source;   //!< VCInput: the widget's source itself

        /** The same listener (the same trigger, widget input, master...). */
        bool sameAs(const Owner &o) const;
    };

    /** The place in words ("everywhere", "in cue 2.3"). */
    static QString placeName(Doc *doc, const ControlMap::Place &place);

    /** Everything that listens to @a channel of @a universe now
     *  (KeyboardUniverse: a computer key). A Virtual Console channel's
     *  page bits are left out: a widget on any page is the owner. */
    static QList<Owner> owners(Doc *doc, quint32 universe, quint32 channel);
    /** The channels of @a universe that a listener other than the PMJ
     *  overlay has now (a trigger, the stop, a widget...): the PMJ leaves
     *  their lamps to that listener (one lamp writer per control). One
     *  walk of the show, however many channels are asked about. */
    static QSet<quint32> showControls(Doc *doc, quint32 universe);
    /** owners(), without @a self (the listener that is learning). */
    static QList<Owner> others(Doc *doc, quint32 universe, quint32 channel, const Owner &self);

    /** Whether @a a and @a b on one control is two jobs on it: two
     *  triggers at the same place (Q296: at different places they share
     *  it), a trigger and another listener, or two other listeners -- but
     *  not two Virtual Console widgets (Show Review's VC-06 has their page
     *  and job rules) and not two parts of the encoder bank's own device. */
    static bool collides(const Owner &a, const Owner &b);
    /** Whether every one of @a owners is a trigger (not the stop): a
     *  button may then step through them (Q303). */
    static bool allTriggers(const QList<Owner> &owners);

    /** "<control> already does <thing> (<where>). One job per control.",
     *  one line per owner, and the reason when one of them can't let go. */
    static QString refusal(Doc *doc, quint32 universe, quint32 channel, const QList<Owner> &owners);
    /** refusal() for a run of controls (the encoder bank's encoders,
     *  pushes, bank buttons): each owner named with its own control
     *  ("CC 10 already does A (where); CC 12 already does B (where)."),
     *  owners of one control together. @a taken: (channel, owner). */
    static QString runRefusal(Doc *doc, quint32 universe, const QList<QPair<quint32, Owner>> &taken);
    /** Whether Move It Here may take the control from all of @a owners. */
    static bool canMove(const QList<Owner> &owners);
    /** What Move It Here does on the undo side, in words: the app's undo
     *  doesn't cover inputs, so it says how to put it back. */
    static QString moveNote();
    /** moveNote() in a window that applies on OK (a widget's Properties,
     *  a speed master's Inputs…, the Virtual Console's Properties): the
     *  control is taken only when it is accepted. */
    static QString moveOnOkNote();

    /** Takes the control away from @a owner (it no longer reacts to it).
     *  false: it can't let go, or it is gone. */
    static bool release(Doc *doc, const Owner &owner);
    /** Whether release() of @a owner would work now: it can let go and it
     *  is still there (the trigger, the widget, the channel group...). */
    static bool canRelease(Doc *doc, const Owner &owner);
    /** release() for each of @a owners, all or nothing: false when any
     *  can't let go, and then every one keeps its control (none is
     *  released, or those released before one failed are put back). */
    static bool releaseAll(Doc *doc, const QList<Owner> &owners);

    /** The control in words ("MIDI · CC 7, MIDI ch 1", "Key · F5"). */
    static QString controlName(Doc *doc, quint32 universe, quint32 channel);

    /** A control two or more listeners have now (a show saved before the
     *  refusal, or set up in files). */
    struct Collision
    {
        quint32 universe = 0;
        quint32 channel = 0;
        QList<Owner> owners;
    };
    /** Every control with two jobs, in universe and channel order. Two
     *  triggers at one place are not: they are there only because someone
     *  chose Step Through Them (Q303). */
    static QList<Collision> collisions(Doc *doc);
    /** One collision in words, with how to fix it. */
    static QString collisionText(Doc *doc, const Collision &c);

    /** Test hook: the PMJ channel names, so owners() can be checked
     *  without an overlay. Empty when @a channel isn't the PMJ's. */
    static QString pmjControlName(quint32 channel);

private:
    /** Every listener of the show with the control it hears, as
     *  (universe << 32 | channel): the one walk behind owners() and
     *  collisions(). */
    static QList<QPair<quint64, Owner>> owned(Doc *doc);
};

/** @} */

#endif
