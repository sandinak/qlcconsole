/*
  Q Light Controller Plus
  controlowners.cpp

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

#include <QStringList>
#include <algorithm>
#include <QHash>
#include <QMap>

#include "controlowners.h"
#include "virtualconsole.h"
#include "vcproperties.h"
#include "vcwidget.h"
#include "vcframe.h"
#include "qlcinputsource.h"
#include "inputoutputmap.h"
#include "channelsgroup.h"
#include "speedmasters.h"
#include "encoderbank.h"
#include "pmjoverlay.h"
#include "inputpatch.h"
#include "programmercontroller.h"
#include "keytriggers.h"
#include "padrun.h"
#include "deskrecord.h"
#include "showtreecontroller.h"
#include "doc.h"
#include "controlmap.h"
#include "surfacepages.h"
#include "pageset.h"

namespace
{
quint64 controlKey(quint32 universe, quint32 channel)
{
    return (quint64(universe) << 32) | channel;
}

/* A Virtual Console channel carries its frame page in the bits above 16
   (InputSelectionWidget); the control itself is the low bits. */
const quint32 kPageMask = 0xFFFF;
}

bool ControlOwners::Owner::sameAs(const Owner &o) const
{
    if (kind != o.kind)
        return false;
    switch (kind)
    {
        case Trigger:
        case EmergencyStop:
            return target == o.target;
        case Encoder:
        case SpeedMaster:
            return part == o.part && index == o.index;
        case VCInput:
            return widget == o.widget && inputId == o.inputId;
        case GrandMaster:
            return true;
        case ChannelGroup:
        case Pmj:
        case Pad:
            return index == o.index;
        case Keyboard:
            return true;
        case Desk:
            return index == o.index;
    }
    return false;
}

QString ControlOwners::placeName(Doc *doc, const ControlMap::Place &place)
{
    if (doc == nullptr || doc->controlMap() == nullptr)
        return QString();
    return doc->controlMap()->placeName(place);
}

QString ControlOwners::controlName(Doc *doc, quint32 universe, quint32 channel)
{
    if (doc == nullptr || doc->controlMap() == nullptr)
        return QString();
    // A computer key is the whole channel (key and modifiers), not a page.
    return doc->controlMap()->controlName(universe, universe == ControlMap::KeyboardUniverse
                                                        ? channel : channel & kPageMask);
}

QString ControlOwners::pmjControlName(quint32 channel)
{
    return PMJOverlay::controlName(channel);
}

QString ControlOwners::padJob(int role, int family)
{
    const GamepadRole::Role r = GamepadRole::Role(role);
    QString job;
    switch (PadRun::actionFor(r))
    {
        case PadRun::Arm:
            job = r == GamepadRole::LB ? tr("the arm, with {RB}") : tr("the arm, with {LB}");
            break;
        case PadRun::Safety:  job = tr("the safety for GO, Back, Sub GO and Sub Back"); break;
        case PadRun::Go:      job = tr("GO with {LT} held"); break;
        case PadRun::Back:    job = tr("Back with {LT} held"); break;
        case PadRun::SubGo:   job = tr("Sub GO with {LT} held"); break;
        case PadRun::SubBack: job = tr("Sub Back with {LT} held"); break;
        case PadRun::FireHit: job = tr("the armed hit (hits fire by their place)"); break;
        case PadRun::WhereAmI: job = tr("saying the list and what is next"); break;
        case PadRun::Walk:    job = tr("walking the show"); break;
        case PadRun::NoAction:
        case PadRun::EmergencyStop:
            job = tr("its tools (Focus, walking the show)");
            break;
    }
    return PadNames::expand(job, PadNames::Family(family));
}

QList<QPair<quint64, ControlOwners::Owner>> ControlOwners::owned(Doc *doc)
{
    QList<QPair<quint64, Owner>> out;
    if (doc == nullptr)
        return out;
    ControlMap *cm = doc->controlMap();
    const QString map = tr("Control Map");

    // The Control Map: its triggers and its emergency stop.
    for (const ControlMap::Binding &b : cm->bindings())
    {
        Owner o;
        o.target = b.target;
        o.where = map;
        if (b.target.type == ControlMap::EmergencyStopTarget)
        {
            o.kind = EmergencyStop;
            o.thing = tr("the emergency stop");
            o.movable = false;
            o.fixedWhy = tr("Nothing takes the emergency stop's control: learn the stop on another "
                            "button first.");
            o.fix = tr("keep it on the stop; clear the other one");
        }
        else
        {
            o.kind = Trigger;
            o.thing = cm->targetName(b.target);
            o.place = b.place;
            if (b.place.scope != ControlMap::Everywhere)
                o.where = tr("%1, %2").arg(map, cm->placeName(b.place));
            o.fix = tr("Control Map table ▸ Remove, or another place in Works");
        }
        out << qMakePair(controlKey(b.universe, b.channel), o);
    }

    // The encoder bank's device, while it is on.
    const EncoderBank *enc = cm->encoderBank();
    if (enc != nullptr && enc->isEnabled())
    {
        const EncoderBank::Device d = enc->device();
        auto add = [&](qint32 first, int count, EncoderPart part) {
            if (first < 0)
                return;
            for (int i = 0; i < count; ++i)
            {
                Owner o;
                o.kind = Encoder;
                o.part = part;
                o.index = i;
                o.where = tr("encoder bank");
                o.fix = tr("Control Map ▸ Encoder bank ▸ Learn More…");
                /* The encoders, their pushes and the bank buttons are runs
                   (a first control and a count): one of them can't be
                   given up without the rest of its run, so none is moved.
                   Next and Previous are single controls and can be. */
                if (part == EncoderTurn || part == EncoderPush || part == BankDirect)
                {
                    o.movable = false;
                    o.fixedWhy = tr("The encoder bank's encoders, pushes and bank buttons are set up as a "
                                    "run and can't be moved one by one: change the bank's controls in "
                                    "Control Map ▸ Encoder bank first.");
                }
                switch (part)
                {
                    case EncoderTurn: o.thing = tr("Encoder %1").arg(i + 1); break;
                    case EncoderPush: o.thing = tr("the push of encoder %1").arg(i + 1); break;
                    case BankNext: o.thing = tr("Next bank"); break;
                    case BankPrevious: o.thing = tr("Previous bank"); break;
                    case BankDirect: o.thing = tr("Bank %1 (%2)").arg(i + 1).arg(EncoderBank::bankName(i)); break;
                }
                out << qMakePair(controlKey(d.universe, quint32(first) + quint32(i)), o);
            }
        };
        add(d.encoders, d.encoderCount, EncoderTurn);
        add(d.pushes, d.encoderCount, EncoderPush);
        add(d.bankNext, 1, BankNext);
        add(d.bankPrevious, 1, BankPrevious);
        add(d.bankDirect, EncoderBank::bankCount(), BankDirect);
    }

    // Every Virtual Console widget's external inputs.
    VirtualConsole *vc = VirtualConsole::instance();
    if (vc != nullptr && vc->contents() != nullptr)
    {
        QList<VCWidget *> widgets = vc->contents()->findChildren<VCWidget *>();
        widgets.prepend(vc->contents());
        for (VCWidget *w : std::as_const(widgets))
        {
            /* Every input id: the widget's own and its sub-controls' (a
               preset, a custom control, a page shortcut), which fire through
               their own source. */
            QList<quint8> ids = w->inputSourceIds();
            QHash<quint8, QSharedPointer<QLCInputSource>> sub;
            for (const auto &p : w->subInputSources())
            {
                sub.insert(p.first, p.second);
                if (!ids.contains(p.first))
                    ids << p.first;
            }
            for (quint8 id : std::as_const(ids))
            {
                const QSharedPointer<QLCInputSource> src = sub.contains(id) ? sub.value(id) : w->inputSource(id);
                if (src.isNull() || !src->isValid())
                    continue;
                Owner o;
                o.kind = VCInput;
                o.widget = w;
                o.inputId = id;
                o.source = src;
                o.thing = w->caption().isEmpty() ? VCWidget::typeToString(w->type())
                                                  : tr("%1 \"%2\"").arg(VCWidget::typeToString(w->type()),
                                                                        w->caption());
                o.where = tr("Virtual Console");
                o.fix = tr("the widget's Properties ▸ External Input");
                out << qMakePair(controlKey(src->universe(), src->channel() & kPageMask), o);
            }
        }

        // The Grand Master's input (the Virtual Console's properties).
        const VCProperties props = vc->properties();
        if (props.grandMasterInputUniverse() != InputOutputMap::invalidUniverse())
        {
            Owner o;
            o.kind = GrandMaster;
            o.thing = tr("the Grand Master");
            o.where = tr("Virtual Console ▸ Properties");
            o.fix = tr("Virtual Console ▸ Properties ▸ Grand Master");
            out << qMakePair(controlKey(props.grandMasterInputUniverse(),
                                        props.grandMasterInputChannel() & kPageMask), o);
        }
    }

    // The speed masters' Tap, Rate and Pause.
    if (SpeedMasters *sm = doc->speedMasters())
    {
        for (int i = 0; i < SpeedMasters::MaxCount; ++i)
        {
            for (int c = 0; c < SpeedMasters::ControlCount; ++c)
            {
                quint32 u = 0, ch = 0;
                if (!sm->input(i, SpeedMasters::Control(c), &u, &ch))
                    continue;
                Owner o;
                o.kind = SpeedMaster;
                o.index = i;
                o.part = c;
                const QString what = c == SpeedMasters::TapControl ? tr("Tap")
                                   : c == SpeedMasters::RateControl ? tr("Rate") : tr("Pause");
                o.thing = tr("%1 %2").arg(sm->name(i), what);
                o.where = tr("Speed Masters");
                o.fix = tr("Speed Masters ▸ Inputs…");
                out << qMakePair(controlKey(u, ch), o);
            }
        }
    }

    // The channel groups' inputs (Simple Desk).
    for (ChannelsGroup *g : doc->channelsGroups())
    {
        const QSharedPointer<QLCInputSource> src = g->inputSource();
        if (src.isNull() || !src->isValid())
            continue;
        Owner o;
        o.kind = ChannelGroup;
        o.index = int(g->id());
        o.thing = tr("channel group \"%1\"").arg(g->name());
        o.where = tr("Simple Desk");
        o.fix = tr("Simple Desk ▸ the channel group ▸ Edit");
        out << qMakePair(controlKey(src->universe(), src->channel() & kPageMask), o);
    }

    // The PMJ overlay: every control of a universe patched with its profile.
    InputOutputMap *iom = doc->inputOutputMap();
    for (quint32 u = 0; iom != nullptr && u < iom->universesCount(); ++u)
    {
        InputPatch *ip = iom->inputPatch(u);
        if (ip == nullptr || ip->profileName().compare(PMJOverlay::deviceId, Qt::CaseInsensitive) != 0)
            continue;
        for (const quint32 ch : PMJOverlay::controlChannels())
        {
            const QString name = PMJOverlay::controlName(ch);
            Owner o;
            o.kind = Pmj;
            o.index = int(ch);
            o.thing = tr("the PMJ Black 1's %1").arg(name);
            o.where = tr("PMJ overlay");
            o.movable = false;
            o.fixedWhy = tr("The PMJ Black 1's controls come with its profile and can't be moved: "
                            "patch the board with another profile to use them for something else.");
            o.fix = tr("fixed by the PMJ profile; clear the other one");
            out << qMakePair(controlKey(u, ch), o);
        }
    }
    return out;
}

QList<ControlOwners::Owner> ControlOwners::owners(Doc *doc, quint32 universe, quint32 channel)
{
    if (doc == nullptr)
        return QList<Owner>();
    return ownersFrom(doc, universe, channel, owned(doc));
}

QHash<quint32, QList<ControlOwners::Owner>> ControlOwners::ownersOn(Doc *doc, quint32 universe,
                                                                    const QList<quint32> &channels)
{
    QHash<quint32, QList<Owner>> out;
    if (doc == nullptr)
        return out;
    const QList<QPair<quint64, Owner>> all = owned(doc);
    for (quint32 ch : channels)
        out.insert(ch, ownersFrom(doc, universe, ch, all));
    return out;
}

QList<ControlOwners::Owner> ControlOwners::ownersFrom(Doc *doc, quint32 universe, quint32 channel,
                                                      const QList<QPair<quint64, Owner>> &all)
{
    QList<Owner> out;
    // A key the app already uses (this computer's; never the show's).
    if (universe == ControlMap::KeyboardUniverse)
    {
        QString where;
        const QString use = KeyTriggers::appUse(channel, &where);
        if (!use.isEmpty())
        {
            Owner o;
            o.kind = Keyboard;
            o.thing = use;
            o.where = where;
            o.movable = false;
            o.fixedWhy = tr("The app's own keys can't be moved: choose another key.");
            o.fix = tr("choose another key");
            out << o;
        }
    }
    /* A game pad's button or trigger: its Run map (Q324). A trigger there
       would fire a cue or a hit on one press, with no arm and no safety.
       Where the pad runs no cues (Windows) there is no Run map: the pad's
       buttons stay free for a Control Map trigger. */
    else if (ProgrammerController *pc = PadRun::padRunSupported() ? doc->programmer() : nullptr)
    {
        const int role = pc->padRole(universe, channel & kPageMask);
        if (role != GamepadRole::None)
        {
            const int family = int(pc->padFamilyOf(universe));
            Owner o;
            o.kind = Pad;
            o.index = role;
            o.thing = tr("%1 on the pad: %2")
                          .arg(PadNames::name(GamepadRole::Role(role), PadNames::Family(family)),
                               padJob(role, family));
            o.where = tr("the pad's Run map");
            o.movable = false;
            o.fixedWhy = PadNames::expand(
                tr("A game pad's buttons are its Run map: a show trigger there would fire on one press, "
                   "without the arm and the safety. Hits fire from {RT} by their place; use a MIDI or "
                   "OSC control or a computer key for this."), PadNames::Family(family));
            o.fix = tr("use another controller's control");
            out << o;
        }
    }
    // This computer's desk (Q309): static here, never the show's to take.
    if (universe != ControlMap::KeyboardUniverse)
    {
        Owner o;
        if (deskOwner(doc, universe, channel & kPageMask, &o))
            out << o;
    }
    const quint64 k = universe == ControlMap::KeyboardUniverse ? controlKey(universe, channel)
                                                              : controlKey(universe, channel & kPageMask);
    for (const QPair<quint64, Owner> &p : all)
        if (p.first == k)
            out << p.second;
    return out;
}

QList<quint64> ControlOwners::controls(Doc *doc)
{
    QSet<quint64> keys;
    const QList<QPair<quint64, Owner>> all = owned(doc);
    for (const QPair<quint64, Owner> &p : all)
        keys.insert(p.first);
    // This computer's desk roles (static here, Q309).
    if (doc != nullptr && doc->inputOutputMap() != nullptr)
        for (quint64 k : doc->inputOutputMap()->staticControls())
            keys.insert(k);
    QList<quint64> out(keys.cbegin(), keys.cend());
    std::sort(out.begin(), out.end());
    return out;
}

QString ControlOwners::ownersText(const QList<Owner> &owners)
{
    if (owners.isEmpty())
        return tr("Free");
    QStringList jobs;
    for (const Owner &o : owners)
    {
        const QString job = o.where.isEmpty() ? o.thing : tr("%1 (%2)").arg(o.thing, o.where);
        if (!jobs.contains(job))
            jobs << job;
    }
    return jobs.join(QStringLiteral("; "));
}

QSet<quint32> ControlOwners::showControls(Doc *doc, quint32 universe)
{
    QSet<quint32> out;
    const QList<QPair<quint64, Owner>> all = owned(doc);
    for (const QPair<quint64, Owner> &p : all)
        if (p.second.kind != Pmj && quint32(p.first >> 32) == universe)
            out.insert(quint32(p.first & 0xFFFFFFFFu));
    return out;
}

QList<ControlOwners::Owner> ControlOwners::others(Doc *doc, quint32 universe, quint32 channel, const Owner &self)
{
    QList<Owner> out;
    for (const Owner &o : owners(doc, universe, channel))
        if (collides(self, o))
            out << o;
    return out;
}

bool ControlOwners::collides(const Owner &a, const Owner &b)
{
    if (a.sameAs(b))
        return false;
    /* The pad's Run map and the app's keys refuse a show trigger and the
       stop; what else listens to them is not asked here. */
    auto isShowTrigger = [](const Owner &o) { return o.kind == Trigger || o.kind == EmergencyStop; };
    // This computer's desk: nothing of the show shares its controls (Q309).
    if (a.kind == Desk || b.kind == Desk)
        return true;
    if (a.kind == Pad || a.kind == Keyboard)
        return isShowTrigger(b);
    if (b.kind == Pad || b.kind == Keyboard)
        return isShowTrigger(a);
    // Two widgets on one input: Show Review's VC-06, with its page and job rules.
    if (a.kind == VCInput && b.kind == VCInput)
        return false;
    // The encoder bank's own device: one mapping, set up together.
    if (a.kind == Encoder && b.kind == Encoder)
        return false;
    // Q296: two triggers at different places share a control (the narrowest
    // armed one fires); every other listener is everywhere, always.
    if (a.kind == Trigger && b.kind == Trigger)
        return a.place == b.place;
    return true;
}

bool ControlOwners::allTriggers(const QList<Owner> &owners)
{
    for (const Owner &o : owners)
        if (o.kind != Trigger)
            return false;
    return !owners.isEmpty();
}

bool ControlOwners::canMove(const QList<Owner> &owners)
{
    for (const Owner &o : owners)
        if (!o.movable)
            return false;
    return !owners.isEmpty();
}

QString ControlOwners::refusal(Doc *doc, quint32 universe, quint32 channel, const QList<Owner> &owners)
{
    if (owners.isEmpty())
        return QString();
    QStringList jobs;
    for (const Owner &o : owners)
    {
        const QString job = tr("%1 (%2)").arg(o.thing, o.where);
        if (!jobs.contains(job))
            jobs << job;
    }
    QString list = jobs.first();
    if (jobs.count() == 2)
        list = tr("%1 and %2").arg(jobs.at(0), jobs.at(1));
    else if (jobs.count() > 2)
        list = tr("%1 and %2").arg(jobs.mid(0, jobs.count() - 1).join(QStringLiteral(", ")), jobs.last());
    // "<control> already does <thing> (<where>). One job per control."
    QString text = tr("%1 already does %2. One job per control.")
            .arg(controlName(doc, universe, channel), list);
    for (const Owner &o : owners)
    {
        if (!o.movable)
        {
            text += QStringLiteral("\n") + o.fixedWhy;
            break;
        }
    }
    return text;
}

QString ControlOwners::runRefusal(Doc *doc, quint32 universe, const QList<QPair<quint32, Owner>> &taken)
{
    if (taken.isEmpty())
        return QString();
    // The controls in the order first seen, each with its own owners.
    QList<quint32> channels;
    QHash<quint32, QList<Owner>> byChannel;
    QList<Owner> all;
    for (const QPair<quint32, Owner> &t : taken)
    {
        if (!channels.contains(t.first))
            channels << t.first;
        byChannel[t.first] << t.second;
        all << t.second;
    }
    if (channels.count() == 1)
        return refusal(doc, universe, channels.first(), all);
    QStringList lines;
    for (quint32 ch : std::as_const(channels))
    {
        QStringList jobs;
        for (const Owner &o : byChannel.value(ch))
        {
            const QString job = tr("%1 (%2)").arg(o.thing, o.where);
            if (!jobs.contains(job))
                jobs << job;
        }
        lines << tr("%1 already does %2").arg(controlName(doc, universe, ch), jobs.join(tr(" and ")));
    }
    // "CC 10 already does A (where); CC 12 already does B (where). One job per control."
    QString text = tr("%1. One job per control.").arg(lines.join(QStringLiteral("; ")));
    for (const Owner &o : std::as_const(all))
    {
        if (!o.movable)
        {
            text += QStringLiteral("\n") + o.fixedWhy;
            break;
        }
    }
    return text;
}

QString ControlOwners::moveNote()
{
    return tr("Move It Here is not on Undo: to put it back, learn the control there again.");
}

QString ControlOwners::moveOnOkNote()
{
    return tr("Move It Here takes it when you press OK; Cancel leaves it where it is. "
              "It is not on Undo: to put it back, learn the control there again.");
}

bool ControlOwners::release(Doc *doc, const Owner &owner)
{
    if (doc == nullptr || !owner.movable)
        return false;
    switch (owner.kind)
    {
        case Trigger:
            return doc->controlMap()->removeBinding(owner.target);
        case EmergencyStop:
        case Pmj:
        case Pad:
        case Keyboard:
            return false;
        case Desk:
        {
            // Only a desk role a picker made movable (listenersOf()).
            DeskRecord *d = doc->deskRecord();
            return d != nullptr && d->clear(DeskRecord::Role(owner.index));
        }
        case Encoder:
        {
            EncoderBank *enc = doc->controlMap()->encoderBank();
            EncoderBank::Device d = enc->device();
            switch (owner.part)
            {
                case BankNext: d.bankNext = -1; break;
                case BankPrevious: d.bankPrevious = -1; break;
                default:
                    // A run (the encoders, pushes, bank buttons): never one
                    // control of it -- the rest of the run would go with it.
                    return false;
            }
            enc->setDevice(d, QStringLiteral("custom"));
            return true;
        }
        case VCInput:
        {
            VCWidget *w = qobject_cast<VCWidget *>(owner.widget.data());
            if (w == nullptr)
                return false;
            w->setSubInputSource(QSharedPointer<QLCInputSource>(), owner.inputId);
            doc->setModified();
            return true;
        }
        case SpeedMaster:
            doc->speedMasters()->clearInput(owner.index, SpeedMasters::Control(owner.part));
            doc->setModified();
            return true;
        case GrandMaster:
            if (VirtualConsole *vc = VirtualConsole::instance())
            {
                vc->clearGrandMasterInput();
                return true;
            }
            return false;
        case ChannelGroup:
        {
            ChannelsGroup *g = doc->channelsGroup(quint32(owner.index));
            if (g == nullptr)
                return false;
            g->setInputSource(QSharedPointer<QLCInputSource>());
            doc->setModified();
            return true;
        }
    }
    return false;
}

bool ControlOwners::canRelease(Doc *doc, const Owner &owner)
{
    if (doc == nullptr || !owner.movable)
        return false;
    switch (owner.kind)
    {
        case Trigger:
            return doc->controlMap()->binding(owner.target) != nullptr;
        case EmergencyStop:
        case Pmj:
        case Pad:
        case Keyboard:
            return false;
        case Desk:
            return doc->deskRecord() != nullptr && !doc->deskRecord()->isLocked()
                   && doc->deskRecord()->isSet(DeskRecord::Role(owner.index));
        case Encoder:
            return doc->controlMap()->encoderBank()->isEnabled()
                   && (owner.part == BankNext || owner.part == BankPrevious);
        case VCInput:
            return qobject_cast<VCWidget *>(owner.widget.data()) != nullptr;
        case SpeedMaster:
        {
            quint32 u = 0, ch = 0;
            return doc->speedMasters() != nullptr && owner.part >= 0 && owner.part < SpeedMasters::ControlCount
                   && doc->speedMasters()->input(owner.index, SpeedMasters::Control(owner.part), &u, &ch);
        }
        case GrandMaster:
            return VirtualConsole::instance() != nullptr;
        case ChannelGroup:
            return doc->channelsGroup(quint32(owner.index)) != nullptr;
    }
    return false;
}

namespace
{
/* What one owner had before release() took it: enough to put it back. */
struct Held
{
    ControlOwners::Owner owner;
    ControlMap::Binding binding;
    EncoderBank::Device device;
    QString presetKey;
    bool enabled = false;
    QSharedPointer<QLCInputSource> source;
    quint32 universe = 0;
    quint32 channel = 0;
    DeskRecord::Binding desk;
};

Held holdOf(Doc *doc, const ControlOwners::Owner &o)
{
    Held h;
    h.owner = o;
    switch (o.kind)
    {
        case ControlOwners::Trigger:
            if (const ControlMap::Binding *b = doc->controlMap()->binding(o.target))
                h.binding = *b;
            break;
        case ControlOwners::Encoder:
        {
            const EncoderBank *enc = doc->controlMap()->encoderBank();
            h.device = enc->device();
            h.presetKey = enc->presetKey();
            h.enabled = enc->isEnabled();
            break;
        }
        case ControlOwners::VCInput:
            h.source = o.source;
            break;
        case ControlOwners::SpeedMaster:
            doc->speedMasters()->input(o.index, SpeedMasters::Control(o.part), &h.universe, &h.channel);
            break;
        case ControlOwners::GrandMaster:
            if (VirtualConsole *vc = VirtualConsole::instance())
            {
                h.universe = vc->properties().grandMasterInputUniverse();
                h.channel = vc->properties().grandMasterInputChannel();
            }
            break;
        case ControlOwners::ChannelGroup:
            if (ChannelsGroup *g = doc->channelsGroup(quint32(o.index)))
                h.source = g->inputSource();
            break;
        case ControlOwners::EmergencyStop:
        case ControlOwners::Pmj:
        case ControlOwners::Pad:
        case ControlOwners::Keyboard:
            break;
        case ControlOwners::Desk:
            if (doc->deskRecord() != nullptr)
                h.desk = doc->deskRecord()->binding(DeskRecord::Role(o.index));
            break;
    }
    return h;
}

void putBack(Doc *doc, const Held &h)
{
    switch (h.owner.kind)
    {
        case ControlOwners::Trigger:
            // Beside whatever shares its place: as it was (stepping through, if it did).
            doc->controlMap()->setBinding(h.binding, true);
            break;
        case ControlOwners::Encoder:
        {
            EncoderBank *enc = doc->controlMap()->encoderBank();
            enc->setDevice(h.device, h.presetKey);
            enc->setEnabled(h.enabled);
            break;
        }
        case ControlOwners::VCInput:
            if (VCWidget *w = qobject_cast<VCWidget *>(h.owner.widget.data()))
                w->setSubInputSource(h.source, h.owner.inputId);
            break;
        case ControlOwners::SpeedMaster:
            doc->speedMasters()->setInput(h.owner.index, SpeedMasters::Control(h.owner.part), h.universe, h.channel);
            break;
        case ControlOwners::GrandMaster:
            if (VirtualConsole *vc = VirtualConsole::instance())
                vc->restoreGrandMasterInput(h.universe, h.channel);
            break;
        case ControlOwners::ChannelGroup:
            if (ChannelsGroup *g = doc->channelsGroup(quint32(h.owner.index)))
                g->setInputSource(h.source);
            break;
        case ControlOwners::EmergencyStop:
        case ControlOwners::Pmj:
        case ControlOwners::Pad:
        case ControlOwners::Keyboard:
            break;
        case ControlOwners::Desk:
            if (doc->deskRecord() != nullptr)
                doc->deskRecord()->setBinding(DeskRecord::Role(h.owner.index), h.desk);
            break;
    }
}
}

bool ControlOwners::releaseAll(Doc *doc, const QList<Owner> &owners)
{
    if (!canMove(owners))
        return false;
    /* All or nothing: a control taken from some of its owners and not the
       others would end with fewer jobs than it had and not the one asked
       for -- or with none. Every one is asked first ... */
    for (const Owner &o : owners)
        if (!canRelease(doc, o))
            return false;
    // ... and if one still fails, those already released get it back.
    QList<Held> released;
    for (const Owner &o : owners)
    {
        const Held h = holdOf(doc, o);
        if (!release(doc, o))
        {
            for (int i = released.count() - 1; i >= 0; --i)
                putBack(doc, released.at(i));
            return false;
        }
        released << h;
    }
    return true;
}

std::function<void()> ControlOwners::releaseAllUndoable(Doc *doc, const QList<Owner> &owners)
{
    if (!canMove(owners))
        return std::function<void()>();
    QSharedPointer<QList<Held>> held(new QList<Held>());
    for (const Owner &o : owners)
        *held << holdOf(doc, o);
    if (!releaseAll(doc, owners))
        return std::function<void()>();
    QPointer<Doc> d(doc);
    return [d, held]() {
        if (d.isNull())
            return;
        for (int i = held->count() - 1; i >= 0; --i)
            putBack(d.data(), held->at(i));
    };
}

QList<ControlOwners::Job> ControlOwners::jobsFor(Doc *doc, quint32 universe, quint32 channel)
{
    QList<Job> out;
    if (doc == nullptr)
        return out;
    for (const Owner &o : owners(doc, universe, channel))
    {
        Job j;
        j.owner = o;
        if (o.kind == Desk)
        {
            j.name = DeskRecord::roleName(DeskRecord::Role(o.index));
            j.note = tr("this computer's desk");
        }
        else if (o.kind == EmergencyStop)
        {
            j.name = tr("Emergency Stop");
            j.note = o.where;
        }
        else if (o.kind == GrandMaster)
        {
            j.name = tr("Grand Master");
            j.note = o.where;
        }
        else
        {
            j.name = o.thing;
            j.note = o.where;
        }
        out << j;
    }
    // The surface pages' controls: a page job is the page's, in Design.
    ControlMap *cm = doc->controlMap();
    SurfacePages *sp = cm != nullptr ? cm->surfacePages() : nullptr;
    if (sp != nullptr && universe != ControlMap::KeyboardUniverse && sp->universes().contains(universe))
    {
        const PageSet set = sp->pageSet(universe);
        const quint32 ch = channel & 0xFFFF;
        QStringList pages;
        QString text;
        for (int p = 0; p < set.pages().count(); ++p)
            if (const PageSet::Binding *b = set.binding(p, ch))
            {
                if (b->param == QLatin1String("none"))
                    continue;
                pages << set.pages().at(p).name;
                if (text.isEmpty())
                    text = b->text();
            }
        if (!pages.isEmpty())
        {
            Job j;
            j.isPage = true;
            if (pages.count() == set.pages().count())
                j.name = tr("every page: %1").arg(text);
            else
            {
                j.name = tr("page %1: %2").arg(pages.join(QStringLiteral(", ")), text);
                j.note = tr("on page %1 only").arg(pages.join(QStringLiteral(", ")));
            }
            out << j;
        }
    }
    return out;
}

QString ControlOwners::jobsText(const QList<Job> &jobs)
{
    if (jobs.isEmpty())
        return tr("free");
    QStringList lines;
    for (const Job &j : jobs)
    {
        const QString l = j.note.isEmpty() ? j.name : tr("%1 (%2)").arg(j.name, j.note);
        if (!lines.contains(l))
            lines << l;
    }
    return lines.join(QStringLiteral("; "));
}

QString ControlOwners::pageJobsText(const QList<Job> &jobs)
{
    QStringList lines;
    for (const Job &j : jobs)
        if (j.isPage)
            lines << (j.note.isEmpty() ? j.name : tr("%1 (%2)").arg(j.name, j.note));
    return lines.join(QStringLiteral("; "));
}

QList<ControlOwners::Owner> ControlOwners::listenersOf(Doc *doc, const QList<Job> &jobs)
{
    QList<Owner> out;
    DeskRecord *desk = doc != nullptr ? doc->deskRecord() : nullptr;
    for (const Job &j : jobs)
    {
        if (j.isPage)
            continue;
        Owner o = j.owner;
        if (o.kind == Desk && desk != nullptr && !desk->isLocked())
        {
            o.movable = true;
            o.fixedWhy.clear();
        }
        out << o;
    }
    return out;
}

QList<ControlOwners::Collision> ControlOwners::collisions(Doc *doc)
{
    QMap<quint64, QList<Owner>> byControl;
    const QList<QPair<quint64, Owner>> all = owned(doc);
    for (const QPair<quint64, Owner> &p : all)
        byControl[p.first] << p.second;

    QList<Collision> out;
    for (auto it = byControl.constBegin(); it != byControl.constEnd(); ++it)
    {
        const QList<Owner> &list = it.value();
        if (list.count() < 2)
            continue;
        Collision c;
        c.universe = quint32(it.key() >> 32);
        c.channel = quint32(it.key() & 0xFFFFFFFF);
        for (int a = 0; a < list.count(); ++a)
        {
            bool clash = false;
            // Two triggers at one place stay only by Step Through Them
            // (Q303; Learn refuses them otherwise): chosen, not two jobs.
            for (int b = 0; b < list.count() && !clash; ++b)
                clash = a != b && collides(list.at(a), list.at(b))
                        && !(list.at(a).kind == Trigger && list.at(b).kind == Trigger);
            if (clash)
                c.owners << list.at(a);
        }
        if (c.owners.count() >= 2)
            out << c;
    }
    /* A show's trigger on a control this computer's desk has (Q309): kept in
       the show, never armed here, listed as blocked. */
    if (doc != nullptr && doc->controlMap() != nullptr)
    {
        for (const QPair<ControlMap::Binding, QString> &p : doc->controlMap()->deskBlocked())
        {
            const ControlMap::Binding &b = p.first;
            Owner desk;
            if (!deskOwner(doc, b.universe, b.channel, &desk))
                continue;
            Collision c;
            c.universe = b.universe;
            c.channel = b.channel;
            c.owners << desk;
            for (const QPair<quint64, Owner> &o : all)
                if (o.second.kind == Trigger || o.second.kind == EmergencyStop)
                    if (o.second.target == b.target)
                        c.owners << o.second;
            if (c.owners.count() >= 2)
                out << c;
        }
    }
    return out;
}

bool ControlOwners::deskOwner(Doc *doc, quint32 universe, quint32 channel, Owner *out)
{
    DeskRecord *desk = doc != nullptr ? doc->deskRecord() : nullptr;
    DeskRecord::Role role = DeskRecord::Stop;
    if (desk == nullptr || !desk->isDeskControl(universe, channel, &role))
        return false;
    Owner o;
    o.kind = Desk;
    o.index = int(role);
    o.thing = tr("this computer's %1").arg(DeskRecord::roleName(role));
    o.where = tr("Connections ▸ Control surfaces");
    o.movable = false;
    o.fixedWhy = tr("That is this computer's %1: a show can't take it. Pick a free control, or change "
                    "this computer's desk in Connections ▸ Control surfaces.").arg(DeskRecord::roleName(role));
    o.fix = tr("learn the show's trigger on another control");
    *out = o;
    return true;
}

QString ControlOwners::collisionText(Doc *doc, const Collision &c)
{
    for (const Owner &d : c.owners)
    {
        if (d.kind != Desk)
            continue;
        QStringList kept;
        for (const Owner &o : c.owners)
            if (o.kind != Desk)
                kept << tr("%1 (%2)").arg(o.thing, o.where);
        return tr("%1: %2 is blocked on this computer: it is %3. It stays in the show and works on "
                  "a computer whose desk doesn't use it; learn it on another control to use it here.")
                .arg(controlName(doc, c.universe, c.channel), kept.join(tr(" and ")), d.thing);
    }
    QStringList jobs;
    QStringList fixes;
    for (const Owner &o : c.owners)
    {
        jobs << tr("%1 (%2)").arg(o.thing, o.where);
        if (!fixes.contains(o.fix))
            fixes << o.fix;
    }
    return tr("%1: %2. Keep one: learn it again on the one that should have it and choose Move It "
              "Here, or clear the other (%3).")
            .arg(controlName(doc, c.universe, c.channel), jobs.join(tr(" and ")), fixes.join(QStringLiteral("; ")));
}

QStringList ControlOwners::deskJobsOn(Doc *doc, quint32 universe)
{
    QStringList out;
    DeskRecord *dr = doc != nullptr ? doc->deskRecord() : nullptr;
    if (dr == nullptr)
        return out;
    for (DeskRecord::Role role : dr->rolesOn(universe))
        out << tr("this computer's %1").arg(DeskRecord::roleName(role));
    return out;
}

QStringList ControlOwners::jobsOn(Doc *doc, quint32 universe, bool withDesk)
{
    QStringList stop, gm, triggers, vc, rest;
    bool encoder = false;
    ShowTreeController *st = doc != nullptr ? doc->showTree() : nullptr;

    /* This computer's desk roles on that universe come first: losing the
       desk's STOP or Grand Master is what the operator must hear about.
       Matched by the device, never by where the role is live now: an
       unplug has made it not live before the notice is said. */
    const QStringList desk = withDesk ? deskJobsOn(doc, universe) : QStringList();
    for (const QPair<quint64, Owner> &p : owned(doc))
    {
        if (quint32(p.first >> 32) != universe)
            continue;
        const Owner &o = p.second;
        switch (o.kind)
        {
            case EmergencyStop:
                if (stop.isEmpty())
                    stop << tr("Emergency Stop");
                break;
            case GrandMaster:
                if (gm.isEmpty())
                    gm << tr("Grand Master");
                break;
            case Trigger:
            {
                QString what = o.thing;
                const ShowTree::CueList *l = st != nullptr ? st->cueList(o.target.id) : nullptr;
                if (o.target.type == ControlMap::CueListTarget && l != nullptr)
                    what = tr("GO of %1").arg(l->name);
                else if (o.target.type == ControlMap::CueListBackTarget && l != nullptr)
                    what = tr("Back of %1").arg(l->name);
                if (!what.isEmpty() && !triggers.contains(what))
                    triggers << what;
                break;
            }
            case VCInput:
                vc << o.thing;
                break;
            case Encoder:
                encoder = true;
                break;
            case SpeedMaster:
            case ChannelGroup:
                if (!rest.contains(o.thing))
                    rest << o.thing;
                break;
            case Pmj:
            case Pad:
            case Keyboard:
            case Desk:
                break;
        }
    }
    QStringList out = desk + stop + gm + triggers;
    if (vc.count() > 2)
        out << tr("%1 Virtual Console controls").arg(vc.count());
    else
        out << vc;
    if (encoder)
        out << tr("the encoder bank");
    out << rest;
    return out;
}
