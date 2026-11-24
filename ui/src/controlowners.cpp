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
#include "doc.h"

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
            for (quint8 id : w->inputSourceIds())
            {
                const QSharedPointer<QLCInputSource> src = w->inputSource(id);
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
    QList<Owner> out;
    const quint64 k = universe == ControlMap::KeyboardUniverse ? controlKey(universe, channel)
                                                              : controlKey(universe, channel & kPageMask);
    const QList<QPair<quint64, Owner>> all = owned(doc);
    for (const QPair<quint64, Owner> &p : all)
        if (p.first == k)
            out << p.second;
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

QString ControlOwners::moveNote()
{
    return tr("Move It Here is not on Undo: to put it back, learn the control there again.");
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
            return false;
        case Encoder:
        {
            EncoderBank *enc = doc->controlMap()->encoderBank();
            EncoderBank::Device d = enc->device();
            switch (owner.part)
            {
                case EncoderTurn:
                    // The encoders are a run from the first: it ends before this one.
                    if (owner.index > 0)
                        d.encoderCount = owner.index;
                    else
                        d.encoders = -1;
                    break;
                case EncoderPush: d.pushes = -1; break;
                case BankNext: d.bankNext = -1; break;
                case BankPrevious: d.bankPrevious = -1; break;
                case BankDirect:
                    if (d.bankLamps == d.bankDirect)
                        d.bankLamps = -1;
                    d.bankDirect = -1;
                    break;
                default:
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
            w->setInputSource(QSharedPointer<QLCInputSource>(), owner.inputId);
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

bool ControlOwners::releaseAll(Doc *doc, const QList<Owner> &owners)
{
    if (!canMove(owners))
        return false;
    bool ok = true;
    for (const Owner &o : owners)
        ok = release(doc, o) && ok;
    return ok;
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
    return out;
}

QString ControlOwners::collisionText(Doc *doc, const Collision &c)
{
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
