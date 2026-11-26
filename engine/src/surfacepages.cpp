/*
  Q Light Controller Plus
  surfacepages.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QSettings>
#include <QUrl>

#include "surfacepages.h"
#include "inputoutputmap.h"
#include "qlcinputprofile.h"
#include "qlcinputchannel.h"
#include "encoderbank.h"
#include "deskrecord.h"
#include "controlmap.h"
#include "inputpatch.h"
#include "doc.h"

#define KXMLProfile QStringLiteral("Profile")
#define KXMLName QStringLiteral("Name")

namespace
{
const quint32 kNone = 0xFFFFFFFF;

quint64 keyOf(quint32 universe, quint32 channel)
{
    return (quint64(universe) << 32) | channel;
}
}

SurfacePages::SurfacePages(Doc *doc)
    : QObject(nullptr)
    , m_doc(doc)
{
    Q_ASSERT(doc != nullptr);
    if (InputOutputMap *iom = m_doc->inputOutputMap())
    {
        connect(iom, &InputOutputMap::inputValueChanged, this,
                [this](quint32 u, quint32 ch, uchar v, const QString &) { handleInput(u, ch, v); });
        connect(iom, &InputOutputMap::inputPatchChanged, this, &SurfacePages::rescan);
        connect(iom, &InputOutputMap::profileChanged, this, &SurfacePages::rescan);
        connect(iom, &InputOutputMap::universeRemoved, this, &SurfacePages::rescan);
        connect(iom, &InputOutputMap::inputDeviceConnected, this, [this](quint32 universe) {
            if (board(universe) != nullptr)
                sendLamps(universe);
        });
    }
    connect(m_doc, &Doc::loaded, this, &SurfacePages::rescan);

    connect(m_doc, &Doc::modeChanged, this, [this]() {
        for (Board &b : m_boards)
            b.last.clear();
        applyStatic();
        for (quint32 u : m_boards.keys())
            sendLamps(u);
        emit setsChanged();
    });
}

/*****************************************************************************
 * Boards
 *****************************************************************************/

void SurfacePages::rescan()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    if (iom == nullptr)
        return;
    QMap<quint32, Board> was = m_boards;
    m_boards.clear();
    DeskRecord *desk = m_doc->deskRecord();
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        InputPatch *ip = iom->inputPatch(u);
        if (ip == nullptr || ip->profileName().isEmpty())
            continue;
        Board b;
        b.profile = ip->profileName();
        b.layout = SurfaceLayout::forProfile(b.profile);
        if (!b.layout.isValid())
            continue;
        b.identity = desk != nullptr ? desk->identityOfUniverse(u) : QString();
        b.set = setFor(b);
        if (!b.set.isValid())
            continue;
        // The same board with the same set: on the page it was on.
        const auto old = was.constFind(u);
        if (old != was.constEnd() && old->profile == b.profile && old->set.name() == b.set.name())
        {
            b.page = qBound(0, old->page, int(b.set.pages().size()) - 1);
            b.lampsOurs = old->lampsOurs;
        }
        m_boards.insert(u, b);
    }
    applyStatic();
    for (quint32 u : m_boards.keys())
        sendLamps(u);
    emit setsChanged();
}

SurfacePages::Board *SurfacePages::board(quint32 universe)
{
    auto it = m_boards.find(universe);
    return it == m_boards.end() ? nullptr : &it.value();
}

const SurfacePages::Board *SurfacePages::board(quint32 universe) const
{
    auto it = m_boards.constFind(universe);
    return it == m_boards.constEnd() ? nullptr : &it.value();
}

QList<quint32> SurfacePages::universes() const
{
    return m_boards.keys();
}

PageSet SurfacePages::pageSet(quint32 universe) const
{
    const Board *b = board(universe);
    return b != nullptr ? b->set : PageSet();
}

int SurfacePages::page(quint32 universe) const
{
    const Board *b = board(universe);
    return b != nullptr ? b->page : 0;
}

QString SurfacePages::pageName(quint32 universe) const
{
    const Board *b = board(universe);
    if (b == nullptr || b->page < 0 || b->page >= b->set.pages().size())
        return QString();
    return b->set.pages().at(b->page).name;
}

bool SurfacePages::pageIsEmpty(quint32 universe) const
{
    const Board *b = board(universe);
    if (b == nullptr || b->page < 0 || b->page >= b->set.pages().size())
        return false;
    for (const PageSet::Binding &x : b->set.pages().at(b->page).controls)
        if (x.param != QLatin1String("none"))
            return false;
    return true;
}

QString SurfacePages::pageText(quint32 universe) const
{
    const QString name = pageName(universe);
    return pageIsEmpty(universe) ? tr("%1: coming").arg(name) : name;
}

QString SurfacePages::chipText(quint32 universe) const
{
    const Board *b = board(universe);
    if (b == nullptr)
        return QString();
    return QStringLiteral("%1 · %2").arg(b->layout.shortName(), pageText(universe));
}

void SurfacePages::setPage(quint32 universe, int page)
{
    Board *b = board(universe);
    if (b == nullptr || page < 0 || page >= b->set.pages().size() || page == b->page)
        return;
    b->page = page;
    b->last.clear();    // a knob's first move on the new page is read from there
    applyStatic();
    sendLamps(universe);
    emit pageChanged(universe, pageName(universe));
    /* TODO(feat/053-surface-setup-init): the APC64's screen line 3 shows the
       page ("the page or role", its layout's display) once device setup's
       display writer is on main; pageChanged() is what it will listen to. */
}

bool SurfacePages::pressKey(quint32 universe, quint32 key)
{
    Board *b = board(universe);
    if (b == nullptr || !b->set.keys().contains(key))
        return false;
    if (key == b->set.nextKey())
        stepPage(universe, 1);
    else if (key == b->set.previousKey())
        stepPage(universe, -1);
    else
        setPage(universe, b->set.pageForKey(key, b->page));
    return true;
}

void SurfacePages::stepPage(quint32 universe, int direction)
{
    Board *b = board(universe);
    if (b == nullptr || b->set.pages().isEmpty() || direction == 0)
        return;
    const int n = int(b->set.pages().size());
    setPage(universe, ((b->page + (direction > 0 ? 1 : -1)) % n + n) % n);
}

QHash<quint32, QString> SurfacePages::labels(quint32 universe) const
{
    const Board *b = board(universe);
    return b != nullptr ? b->set.labels(b->page) : QHash<quint32, QString>();
}

const PageSet::Binding *SurfacePages::bindingFor(quint32 universe, quint32 channel) const
{
    const Board *b = board(universe);
    if (b == nullptr)
        return nullptr;
    const int i = b->layout.indexOf(channel);
    if (i < 0)
        return nullptr;
    return b->set.binding(b->page, b->layout.controls().at(i).channel);
}

bool SurfacePages::owns(quint32 universe, quint32 channel) const
{
    const Board *b = board(universe);
    if (b == nullptr || !isActive(universe))
        return false;
    const int i = b->layout.indexOf(channel);
    return i >= 0 && b->set.controlsOn(b->page).contains(b->layout.controls().at(i).channel);
}

bool SurfacePages::encoderBankOn(quint32 universe) const
{
    ControlMap *cm = m_doc->controlMap();
    EncoderBank *enc = cm != nullptr ? cm->encoderBank() : nullptr;
    return enc != nullptr && enc->isEnabled() && enc->device().universe == universe;
}

bool SurfacePages::isActive(quint32 universe) const
{
    return board(universe) != nullptr && m_doc->mode() == Doc::Design && !encoderBankOn(universe);
}

bool SurfacePages::isOverlaid(quint32 universe) const
{
    const Board *b = board(universe);
    return b != nullptr && m_overlaid.contains(b->profile);
}

void SurfacePages::setOverlaid(const QString &profileName)
{
    m_overlaid.insert(profileName);
    applyStatic();
}

void SurfacePages::applyStatic()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    if (iom == nullptr)
        return;
    /* In Design the page has its controls: no show listener (a trigger, a
       VC widget, a channel group) hears them. In Operate they are the
       show's again (Q332 a). A board whose overlay reads its controls (the
       PMJ) keeps hearing them; the overlay knows the page. */
    QSet<quint64> controls;
    for (auto it = m_boards.constBegin(); it != m_boards.constEnd(); ++it)
    {
        if (!isActive(it.key()) || m_overlaid.contains(it->profile))
            continue;
        for (quint32 ch : it->set.controlsOn(it->page))
        {
            // Never the desk's GO, Back, STOP or GM, whatever a set says.
            if (!it->layout.roleOn(ch).isEmpty())
                continue;
            controls.insert(keyOf(it.key(), ch));
            const int i = it->layout.indexOf(ch);
            if (i >= 0)
                for (quint32 a : it->layout.controls().at(i).also)
                    controls.insert(keyOf(it.key(), a));
        }
    }
    iom->setPageControls(controls);
}

void SurfacePages::sendLamps(quint32 universe)
{
    Board *b = board(universe);
    InputOutputMap *iom = m_doc->inputOutputMap();
    // The PMJ's page keys are its overlay's to light.
    if (b == nullptr || iom == nullptr || m_overlaid.contains(b->profile))
        return;
    /* One owner per light: in Design the page has its keys' lights (the
       Control Map leaves a page control alone); out of Design they go
       dark and the show's own lights come back. */
    if (!isActive(universe))
    {
        if (!b->lampsOurs)
            return;
        b->lampsOurs = false;
        for (const PageSet::Page &p : b->set.pages())
            if (p.key != kNone)
                iom->sendFeedBack(universe, p.key, 0, QVariant());
        if (ControlMap *cm = m_doc->controlMap())
            cm->resendFeedback();
        return;
    }
    b->lampsOurs = true;
    const quint32 on = b->page >= 0 && b->page < b->set.pages().size() ? b->set.pages().at(b->page).key : kNone;
    for (const PageSet::Page &p : b->set.pages())
        if (p.key != kNone)
            iom->sendFeedBack(universe, p.key, p.key == on ? 255 : 0, QVariant());
}

/*****************************************************************************
 * Input
 *****************************************************************************/

int SurfacePages::turnDetents(Board &b, quint32 channel, uchar value, int knobDelta)
{
    int detents = knobDelta;
    QLCInputProfile *profile = m_doc->inputOutputMap()->profile(b.profile);
    const QLCInputChannel *ich = profile != nullptr ? profile->channel(channel) : nullptr;
    if (ich != nullptr && ich->movementType() == QLCInputChannel::Relative)
        detents = EncoderBank::decode(EncoderBank::Encoding(int(ich->relativeEncoding())), value);
    else if (knobDelta == kNotAKnob)
        detents = EncoderBank::decode(EncoderBank::TwosComplement, value);
    return b.set.invertTurns() ? -detents : detents;
}

bool SurfacePages::standsDown() const
{
    // A learn of any kind hears the board, not a page: the desk's, the show's.
    InputOutputMap *iom = m_doc->inputOutputMap();
    ControlMap *cm = m_doc->controlMap();
    return (iom != nullptr && iom->deskLearning()) || (cm != nullptr && cm->isListening());
}

void SurfacePages::handleInput(quint32 universe, quint32 channel, uchar value)
{
    Board *b = board(universe);
    if (b == nullptr)
        return;
    InputOutputMap *iom = m_doc->inputOutputMap();
    const int ci = b->layout.indexOf(channel);
    if (ci < 0)
        return;
    const SurfaceLayout::Control &c = b->layout.controls().at(ci);
    const quint32 own = c.channel;
    QLCInputProfile *profile = iom->profile(b->profile);
    if (channel != own)
    {
        // One control on another channel (the APC40's device knob on the
        // track's MIDI channel) is the same control; a strip's touch is not
        // its value.
        const QLCInputChannel *a = profile != nullptr ? profile->channel(channel) : nullptr;
        const QLCInputChannel *o = profile != nullptr ? profile->channel(own) : nullptr;
        if (a == nullptr || o == nullptr || a->type() != o->type())
            return;
    }

    /* A knob that sends where it is (the APC40's) is followed on every
       value, whoever has it now (Operate, a learn, a page where it has no
       job): its first turn on a page is then the turn, never the whole
       way from where it was last heard. */
    int knobDelta = kNotAKnob;
    const QLCInputChannel *ich = profile != nullptr ? profile->channel(channel) : nullptr;
    if (c.kind == SurfaceLayout::Knob && (ich == nullptr || ich->movementType() != QLCInputChannel::Relative))
    {
        const int midi = value == 255 ? 127 : value >> 1;
        const int last = b->last.value(own, -1);
        b->last.insert(own, midi);
        knobDelta = last < 0 ? 0 : midi - last;
    }

    // A key's release always lets go, in any mode (held across a switch).
    const quint64 k = keyOf(universe, own);
    if (value == 0 && m_down.remove(k))
        return;

    // The desk's own (a role learned on any control), a learn, the show's encoder bank.
    if (standsDown() || iom->isStaticControl(universe, channel) || encoderBankOn(universe))
        return;
    // Never the desk's GO, Back, STOP or GM, whatever a set says.
    if (!b->layout.roleOn(own).isEmpty())
        return;
    const bool design = m_doc->mode() == Doc::Design;
    const bool overlaid = m_overlaid.contains(b->profile);
    // Operate: the controls are the show's (the PMJ's keys are its overlay's).
    if (!design)
        return;

    if (own == b->set.stepper())
    {
        const int d = turnDetents(*b, channel, value, knobDelta);
        if (d != 0)
            stepPage(universe, d);
        return;
    }
    if (b->set.keys().contains(own))
    {
        // An overlay's keys are the overlay's to press (pressKey()).
        if (overlaid || value == 0)
            return;
        if (m_down.contains(k))
            return;     // a held key resending is the one press
        m_down.insert(k);
        pressKey(universe, own);
        return;
    }
    const PageSet::Binding *bind = b->set.binding(b->page, own);
    int bank = 0, slot = 0;
    if (bind == nullptr || !PageSet::bankSlot(bind->param, &bank, &slot))
        return;     // nothing here, or one of the overlay's rules
    if (c.isFader())
        emit levelSet(universe, own, bank, slot, value);
    else if (c.turns())
    {
        const int d = turnDetents(*b, channel, value, knobDelta);
        if (d != 0)
            emit adjusted(bank, slot, d, bind->fine);
    }
}

/*****************************************************************************
 * The set, per computer
 *****************************************************************************/

QString SurfacePages::settingsGroup()
{
    return QStringLiteral("SurfacePages");
}

QString SurfacePages::settingsKey(const Board &b) const
{
    // By the device's identity (DeskRecord's), else by its type.
    const QString who = b.identity.isEmpty() ? QStringLiteral("type:") + b.profile : b.identity;
    return settingsGroup() + QLatin1Char('/')
           + QString::fromLatin1(QUrl::toPercentEncoding(who + QLatin1Char('|') + b.profile));
}

PageSet SurfacePages::setFor(const Board &b) const
{
    const QString name = QSettings().value(settingsKey(b)).toString();
    if (!name.isEmpty())
    {
        const PageSet s = PageSet::find(b.profile, name);
        if (s.isValid())
            return s;
    }
    return PageSet::defaultFor(b.profile);
}

QString SurfacePages::chosenSet(quint32 universe) const
{
    const Board *b = board(universe);
    return b != nullptr ? b->set.name() : QString();
}

bool SurfacePages::chooseSet(quint32 universe, const QString &name)
{
    Board *b = board(universe);
    if (b == nullptr)
        return false;
    const PageSet s = PageSet::find(b->profile, name);
    if (!s.isValid())
        return false;
    QSettings().setValue(settingsKey(*b), name);
    b->set = s;
    b->page = 0;
    applyStatic();
    sendLamps(universe);
    emit pageChanged(universe, pageName(universe));
    emit setsChanged();
    return true;
}

/*****************************************************************************
 * The show's sets
 *****************************************************************************/

QString SurfacePages::offeredSet(quint32 universe) const
{
    const Board *b = board(universe);
    if (b == nullptr || m_declined.contains(universe))
        return QString();
    const QString name = m_showSets.value(b->profile);
    if (name.isEmpty() || name == b->set.name() || !PageSet::find(b->profile, name).isValid())
        return QString();
    return name;
}

void SurfacePages::useShowSet(quint32 universe)
{
    const QString name = offeredSet(universe);
    if (!name.isEmpty())
        chooseSet(universe, name);
}

void SurfacePages::declineShowSet(quint32 universe)
{
    m_declined.insert(universe);
    emit setsChanged();
}

bool SurfacePages::hasShowSets() const
{
    if (!m_showSets.isEmpty())
        return true;
    for (const Board &b : m_boards)
        if (b.set.name() != PageSet::defaultFor(b.profile).name())
            return true;
    return false;
}

bool SurfacePages::saveXML(QXmlStreamWriter *doc) const
{
    if (doc == nullptr)
        return false;
    /* The set each board was built with here, when it is not the board's
       own default; a board not patched now keeps what the show said. */
    QHash<QString, QString> sets = m_showSets;
    for (const Board &b : m_boards)
    {
        if (b.set.name() != PageSet::defaultFor(b.profile).name())
            sets.insert(b.profile, b.set.name());
        else
            sets.remove(b.profile);
    }
    QStringList profiles = sets.keys();
    profiles.sort();
    for (const QString &p : profiles)
    {
        doc->writeStartElement(KXMLQLCSurfacePageSet);
        doc->writeAttribute(KXMLProfile, p);
        doc->writeAttribute(KXMLName, sets.value(p));
        doc->writeEndElement();
    }
    return true;
}

void SurfacePages::loadXML(QXmlStreamReader &root)
{
    const QXmlStreamAttributes attrs = root.attributes();
    const QString profile = attrs.value(KXMLProfile).toString();
    const QString name = attrs.value(KXMLName).toString();
    if (!profile.isEmpty() && !name.isEmpty())
        m_showSets.insert(profile, name);
    root.skipCurrentElement();
    m_declined.clear();
    emit setsChanged();
}

void SurfacePages::reset()
{
    m_showSets.clear();
    m_declined.clear();
    emit setsChanged();
}
