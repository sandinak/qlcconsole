/*
  Q Light Controller Plus
  editlive.cpp

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

#include <QTimer>
#include <QColor>
#include <QMap>
#include <algorithm>

#include "editlive.h"
#include "encoderattributes.h"
#include "showtreecontroller.h"
#include "outputrulekeeper.h"
#include "inputoutputmap.h"
#include "refinesession.h"
#include "surfacepages.h"
#include "encoderbank.h"
#include "qlcphysical.h"
#include "qlcfixturemode.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "universe.h"
#include "fadechannel.h"
#include "qlcfixturedef.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
const quint32 kInvalid = 0xFFFFFFFF;

quint64 chanKey(quint32 fixture, quint32 channel)
{
    return (quint64(fixture) << 32) | channel;
}

quint64 slotKey(quint32 fixture, int bank, int slot)
{
    return (quint64(fixture) << 32) | quint32((bank << 8) | slot);
}

quint32 keyFixture(quint64 k) { return quint32(k >> 32); }
int keyBank(quint64 k) { return int((k >> 8) & 0xFF); }
int keySlot(quint64 k) { return int(k & 0xFF); }

/** The dimmer a Dimmer look writes on @p f (invalid: none). */
quint32 dimmerChannel(const Fixture *f)
{
    const quint32 master = f->masterIntensityChannel();
    if (master != QLCChannel::invalid())
        return master;
    return f->channelNumber(QLCChannel::Intensity, QLCChannel::MSB, 0);
}

/** A look's value, before it is made a palette: what Store compares
 *  between fixtures (equal: one look for all; else one per fixture). */
struct StoreValue
{
    QLCPalette::PaletteType type = QLCPalette::Undefined;
    QVariantList values;

    bool isValid() const { return type != QLCPalette::Undefined; }
    bool operator==(const StoreValue &o) const { return type == o.type && values == o.values; }
};
}

/*
  Q Light Controller Plus
  editlive.cpp

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

#include <QTimer>
#include <QColor>
#include <QMap>
#include <algorithm>

#include "editlive.h"
#include "encoderattributes.h"
#include "showtreecontroller.h"
#include "outputrulekeeper.h"
#include "inputoutputmap.h"
#include "refinesession.h"
#include "surfacepages.h"
#include "encoderbank.h"
#include "qlcphysical.h"
#include "qlcfixturemode.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "universe.h"
#include "fadechannel.h"
#include "qlcfixturedef.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
const quint32 kInvalid = 0xFFFFFFFF;

quint64 chanKey(quint32 fixture, quint32 channel)
{
    return (quint64(fixture) << 32) | channel;
}

quint64 slotKey(quint32 fixture, int bank, int slot)
{
    return (quint64(fixture) << 32) | quint32((bank << 8) | slot);
}

quint32 keyFixture(quint64 k) { return quint32(k >> 32); }
int keyBank(quint64 k) { return int((k >> 8) & 0xFF); }
int keySlot(quint64 k) { return int(k & 0xFF); }

/** The dimmer a Dimmer look writes on @p f (invalid: none). */
quint32 dimmerChannel(const Fixture *f)
{
    const quint32 master = f->masterIntensityChannel();
    if (master != QLCChannel::invalid())
        return master;
    return f->channelNumber(QLCChannel::Intensity, QLCChannel::MSB, 0);
}

/** A look's value, before it is made a palette: what Store compares
 *  between fixtures (equal: one look for all; else one per fixture). */
struct StoreValue
{
    QLCPalette::PaletteType type = QLCPalette::Undefined;
    QVariantList values;

    bool isValid() const { return type != QLCPalette::Undefined; }
    bool operator==(const StoreValue &o) const { return type == o.type && values == o.values; }
};
}

EditLive::EditLive(Doc *doc, SurfacePages *pages, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_pages(pages)
    , m_cue(kInvalid)
    , m_layer(doc, Universe::SimpleDesk, FadeChannel::Override, QStringLiteral("Edit Live"))
{
    Q_ASSERT(doc != nullptr);
    // Trimming the show is not the show yet: a Show-only fixture stays safe.
    m_layer.setHoldsShowOnly(true);
    m_base = new Scene(doc);
    m_trim = new Scene(doc);

    if (m_pages != nullptr)
    {
        connect(m_pages, &SurfacePages::liveAdjusted, this, &EditLive::adjust);
        connect(m_pages, &SurfacePages::liveLevelSet, this, &EditLive::level);
        connect(m_pages, &SurfacePages::pageChanged, this, [this]() { m_pickups.clear(); });
    }
    // Only in Operate; never left on for the next show.
    connect(m_doc, &Doc::modeChanged, this, [this](Doc::Mode mode) {
        if (mode != Doc::Operate)
            setOn(false);
    });
    connect(m_doc, &Doc::clearing, this, [this]() { setOn(false); });
    connect(m_doc, &Doc::programmerSelectionChanged, this, [this]() { m_pickups.clear(); });
    if (ShowTreeController *st = m_doc->showTree())
    {
        connect(st, &ShowTreeController::hitArmingChanged, this, &EditLive::checkCue);
        connect(st, &ShowTreeController::changed, this, &EditLive::treeChanged);
    }
}

EditLive::~EditLive()
{
    m_layer.clear();
    delete m_base;
    delete m_trim;
}

void EditLive::connectSources()
{
    if (m_sourcesConnected)
        return;
    // The emergency stop (OutputRuleKeeper is made after the Control Map).
    if (OutputRuleKeeper *rules = m_doc->outputRuleKeeper())
    {
        connect(rules, &OutputRuleKeeper::emergencyStopStruck, this, [this](quint32) {
            setOn(false);
        });
        m_sourcesConnected = true;
    }
}

bool EditLive::setOn(bool on, QString *why)
{
    if (on == m_on)
        return true;
    if (on && m_doc->mode() != Doc::Operate)
    {
        if (why != nullptr)
            *why = tr("Edit Live trims the running show: go to Operate first.");
        return false;
    }
    connectSources();
    if (!on)
    {
        clearAll();
        forgetLeftBehind();
    }
    m_on = on;
    m_cue = on ? liveCue() : kInvalid;
    if (m_pages != nullptr)
        m_pages->setLive(on);
    emit onChanged(on);
    emit changed();
    return true;
}

/*****************************************************************************
 * The running cue
 *****************************************************************************/

quint32 EditLive::liveCue() const
{
    ShowTreeController *st = m_doc->showTree();
    if (st == nullptr)
        return kInvalid;
    // A cue with sub-cues: the sub-cue playing is the one on stage.
    auto deepest = [st](quint32 cue) {
        const quint32 sub = st->subCueList(cue);
        if (sub != kInvalid)
        {
            const quint32 s = st->runningCue(sub);
            if (s != kInvalid)
                return s;
        }
        return cue;
    };
    const quint32 prod = st->currentProduction();
    if (prod != kInvalid)
    {
        const quint32 c = st->runningCue(st->mainList(prod));
        if (c != kInvalid)
            return deepest(c);
    }
    for (const Production &p : st->productions())
        for (const CueList &l : p.lists)
        {
            if (l.looks || l.isSubCues())
                continue;
            const quint32 c = st->runningCue(l.id);
            if (c != kInvalid)
                return deepest(c);
        }
    return kInvalid;
}

QString EditLive::cueText(quint32 cueId) const
{
    ShowTreeController *st = m_doc->showTree();
    const CueList *list = st != nullptr ? st->listOfCue(cueId) : nullptr;
    if (list == nullptr)
        return QString();
    return st->cueLabel(*list, list->indexOf(cueId));
}

void EditLive::checkCue()
{
    if (!m_on)
        return;
    const quint32 now = liveCue();
    if (now == m_cue)
        return;
    // GO never waits (Q338 a): the trims go with the old cue, kept a while.
    if (m_cue != kInvalid && !m_pending.isEmpty())
    {
        m_left = snapshot();
        const quint32 generation = ++m_leftGeneration;
        QTimer::singleShot(leftBehindMs(), this, [this, generation]() {
            if (m_leftGeneration == generation)
                forgetLeftBehind();
        });
        emit leftBehind(m_left.cue, m_left.trims.size());
    }
    clearAll();
    m_cue = now;
    emit changed();
}

void EditLive::clearAll()
{
    m_layer.clear();
    for (const QMetaObject::Connection &c : std::as_const(m_masterConnections))
        disconnect(c);
    m_masterConnections.clear();
    m_pending.clear();
    m_slotChannels.clear();
    m_pickups.clear();
    m_original.clear();
    m_lookOriginal.clear();
    m_look.clear();
    m_seedScale.clear();
    m_seeded.clear();
    m_holds.clear();
    if (m_base.isNull() || m_trim.isNull())
        return;
    for (const SceneValue &v : m_base->values())
        m_base->unsetValue(v.fxi, v.channel);
    for (const SceneValue &v : m_trim->values())
        m_trim->unsetValue(v.fxi, v.channel);
}

qreal EditLive::listIntensity() const
{
    ShowTreeController *st = m_doc->showTree();
    if (st == nullptr || m_cue == kInvalid)
        return 1.0;
    // The cue's list (and, for a sub-cue, its cue's list): each one's master.
    qreal scale = 1.0;
    quint32 cue = m_cue;
    for (int depth = 0; depth < 2 && cue != kInvalid; ++depth)
    {
        const CueList *list = st->listOfCue(cue);
        if (list == nullptr)
            break;
        if (const Function *chaser = m_doc->function(list->chaser))
            scale *= chaser->getAttributeValue(Function::Intensity);
        quint32 parent = kInvalid;
        cue = st->isSubCue(cue, &parent) ? parent : kInvalid;
    }
    return qBound(0.0, scale, 1.0);
}

void EditLive::connectMaster()
{
    if (!m_masterConnections.isEmpty())
        return;
    ShowTreeController *st = m_doc->showTree();
    quint32 cue = m_cue;
    for (int depth = 0; depth < 2 && st != nullptr && cue != kInvalid; ++depth)
    {
        const CueList *list = st->listOfCue(cue);
        if (list == nullptr)
            break;
        // A list master moved: the trims' intensities follow it.
        if (Function *chaser = m_doc->function(list->chaser))
            m_masterConnections << connect(chaser, &Function::attributeChanged, this, [this](int, qreal) { push(); });
        quint32 parent = kInvalid;
        cue = st->isSubCue(cue, &parent) ? parent : kInvalid;
    }
}

/*****************************************************************************
 * Trimming
 *****************************************************************************/

bool EditLive::canTrim(int bank, int slot)
{
    switch (bank)
    {
        case EncoderBank::IntensityBank:
        case EncoderBank::ColourBank:
        case EncoderBank::PositionBank:
            return true;
        case EncoderBank::BeamBank:
            return slot >= 0 && slot <= 3;      // zoom, focus, iris, frost
        default:
            return false;
    }
}

QList<quint32> EditLive::selection() const
{
    const QList<quint32> picked = m_selection ? m_selection() : m_doc->programmerSelection();
    QList<quint32> out;
    for (quint32 fid : picked)
    {
        Fixture *f = m_doc->fixture(fid);
        if (f != nullptr && f->isPatched() && !out.contains(fid))
            out << fid;
    }
    return out;
}

bool EditLive::begin(int bank, int slot, QList<quint32> *fixtures)
{
    if (!m_on)
        return false;
    const QString what = EncoderBank::slotName(bank, slot);
    if (!canTrim(bank, slot))
    {
        emit message(tr("Edit Live trims intensity, colour, position and beam; %1 is set in Design.").arg(what));
        return false;
    }
    checkCue();
    if (m_cue == kInvalid)
    {
        emit message(tr("No cue is running: Edit Live trims the running cue."));
        return false;
    }
    *fixtures = selection();
    if (fixtures->isEmpty())
    {
        emit message(tr("Select the fixtures to trim."));
        return false;
    }
    if (!EncoderAttributes::available(m_doc, *fixtures, bank, slot))
    {
        emit message(tr("The selection has no %1.").arg(what.toLower()));
        return false;
    }
    seed(*fixtures);
    connectMaster();
    return true;
}

bool EditLive::isIntensity(quint32 fixture, quint32 channel) const
{
    Fixture *f = m_doc->fixture(fixture);
    const QLCChannel *qch = f != nullptr ? f->channel(channel) : nullptr;
    return qch != nullptr && qch->group() == QLCChannel::Intensity;
}

void EditLive::seed(const QList<quint32> &fixtures)
{
    if (m_base.isNull() || m_trim.isNull())
        return;
    /* Two sides: the stage side starts from what is on stage (an override
       hit's colour, a fade half way, an effect: the first touch never
       jumps), and a Store adds the trim to the look's own level (the cue's
       levels, 0 included: never an effect's or a Locate's value). */
    bool fresh = false;
    for (quint32 fid : fixtures)
        fresh = fresh || !m_seeded.contains(fid);
    if (!fresh)
        return;
    QHash<quint32, QByteArray> levels;
    if (ShowTreeController *st = m_doc->showTree())
        levels = st->cueLevels(m_cue);
    const qreal scale = listIntensity();
    InputOutputMap *iom = m_doc->inputOutputMap();
    QHash<quint32, QByteArray> frames;
    for (quint32 fid : fixtures)
    {
        if (m_seeded.contains(fid))
            continue;
        Fixture *f = m_doc->fixture(fid);
        if (f == nullptr)
            continue;
        if (!frames.contains(f->universe()))
        {
            Universe *u = iom->universe(f->universe());
            frames.insert(f->universe(), u != nullptr ? u->preGMValues() : QByteArray());
        }
        const QByteArray &frame = frames[f->universe()];
        const QByteArray look = levels.value(fid);
        m_base->addFixture(fid);
        m_trim->addFixture(fid);
        for (quint32 ch = 0; ch < f->channels(); ++ch)
        {
            const int addr = int(f->address() + ch);
            const uchar v = addr < frame.size() ? uchar(frame.at(addr)) : 0;
            m_base->setValue(fid, ch, v);
            m_trim->setValue(fid, ch, v);
            m_original.insert(chanKey(fid, ch), v);
            const int lv = int(ch) < look.size() ? int(uchar(look.at(int(ch)))) : 0;
            m_lookOriginal.insert(chanKey(fid, ch), lv);
            m_look.insert(chanKey(fid, ch), lv);
        }
        m_seedScale.insert(fid, scale);
        m_seeded.insert(fid);
    }
}

QHash<quint64, uchar> EditLive::valuesOf(const QList<quint32> &fixtures) const
{
    QHash<quint64, uchar> out;
    if (m_trim.isNull())
        return out;
    for (quint32 fid : fixtures)
    {
        Fixture *f = m_doc->fixture(fid);
        if (f == nullptr)
            continue;
        for (quint32 ch = 0; ch < f->channels(); ++ch)
            out.insert(chanKey(fid, ch), m_trim->value(fid, ch));
    }
    return out;
}

void EditLive::noteMoved(const QHash<quint64, uchar> &before, int bank, int slot)
{
    for (auto it = before.constBegin(); it != before.constEnd(); ++it)
    {
        const quint32 fid = keyFixture(it.key());
        const quint32 ch = quint32(it.key() & 0xFFFFFFFF);
        if (m_trim->value(fid, ch) != it.value())
            m_slotChannels[slotKey(fid, bank, slot)].insert(ch);
    }
}

void EditLive::afterEdit()
{
    recomputePending();
    push();
    emit changed();
}

void EditLive::recomputePending()
{
    if (m_base.isNull() || m_trim.isNull())
        return;
    // Per attribute: pending while any channel it moved is off where it was.
    QList<quint64> now = m_pending;
    for (auto it = m_slotChannels.constBegin(); it != m_slotChannels.constEnd(); ++it)
        if (!now.contains(it.key()))
            now << it.key();
    QList<quint64> out;
    for (quint64 k : std::as_const(now))
    {
        const quint32 fid = keyFixture(k);
        bool moved = false;
        for (quint32 ch : m_slotChannels.value(k))
            moved = moved || m_trim->value(fid, ch) != m_base->value(fid, ch);
        if (moved)
            out << k;
    }
    m_pending = out;
}

void EditLive::push()
{
    if (m_trim.isNull())
        return;
    /* The list master scales what the cue plays; it scales the trims'
       intensities too, from where it was when they started, so the stage
       is never brighter than the cue would play. */
    const qreal scale = listIntensity();
    QList<OutputLayer::Level> levels;
    for (auto it = m_original.constBegin(); it != m_original.constEnd(); ++it)
    {
        const quint32 fid = keyFixture(it.key());
        const quint32 ch = quint32(it.key() & 0xFFFFFFFF);
        const uchar v = m_trim->value(fid, ch);
        if (v == it.value())
            continue;
        uchar out = v;
        if (isIntensity(fid, ch))
        {
            const qreal s0 = m_seedScale.value(fid, 1.0);
            out = uchar(qBound(0, qRound(s0 > 0.0 ? v * scale / s0 : v * scale), 255));
        }
        levels << OutputLayer::Level { fid, ch, out };
    }
    m_layer.set(levels);
}

bool EditLive::adjust(int bank, int slot, int detents, bool fine)
{
    QList<quint32> fixtures;
    if (!begin(bank, slot, &fixtures) || m_trim.isNull())
        return false;
    const QHash<quint64, uchar> before = valuesOf(fixtures);
    if (!EncoderAttributes::adjust(m_doc, m_trim, fixtures, bank, slot, detents, fine))
        return false;
    noteMoved(before, bank, slot);
    afterEdit();
    float norm = 0.0f;
    if (EncoderAttributes::read(m_doc, m_trim, fixtures, bank, slot, &norm))
        emit message(tr("Edit Live: %1 %2%").arg(EncoderBank::slotName(bank, slot)).arg(int(norm * 100.0f + 0.5f)));
    return true;
}

bool EditLive::level(quint32 universe, quint32 channel, int bank, int slot, uchar value)
{
    QList<quint32> fixtures;
    if (!begin(bank, slot, &fixtures) || m_trim.isNull())
        return false;
    float norm = 0.0f;
    if (!EncoderAttributes::read(m_doc, m_trim, fixtures, bank, slot, &norm))
        return false;
    // Pick-up (Q299): the fader waits until it meets what is on stage.
    FaderPickup &pick = m_pickups[(quint64(universe) << 32) | channel];
    pick.setSource(universe, channel);
    const int current = qBound(0, int(norm * 255.0f + 0.5f), 255);
    if (!pick.accept(value, current))
    {
        emit message(FaderPickup::hintText(EncoderBank::slotName(bank, slot), int(norm * 100.0f + 0.5f)));
        return false;
    }
    const QHash<quint64, uchar> before = valuesOf(fixtures);
    if (!EncoderAttributes::set(m_doc, m_trim, fixtures, bank, slot, float(value) / 255.0f))
        return false;
    noteMoved(before, bank, slot);
    afterEdit();
    return true;
}

/*****************************************************************************
 * What is pending
 *****************************************************************************/

bool EditLive::isHolding() const
{
    if (m_base.isNull())
        return false;
    for (auto it = m_original.constBegin(); it != m_original.constEnd(); ++it)
        if (m_base->value(keyFixture(it.key()), quint32(it.key() & 0xFFFFFFFF)) != it.value())
            return true;
    return false;
}

QList<EditLive::Trim> EditLive::trims() const
{
    QList<Trim> out;
    if (m_base.isNull() || m_trim.isNull())
        return out;
    for (quint64 k : m_pending)
    {
        Trim t;
        t.fixture = keyFixture(k);
        t.bank = keyBank(k);
        t.slot = keySlot(k);
        EncoderAttributes::read(m_doc, m_base, { t.fixture }, t.bank, t.slot, &t.from);
        EncoderAttributes::read(m_doc, m_trim, { t.fixture }, t.bank, t.slot, &t.to);
        out << t;
    }
    return out;
}

QString EditLive::trimText(const Trim &t) const
{
    Fixture *f = m_doc->fixture(t.fixture);
    return tr("%1 · %2 · %3% → %4%")
        .arg(f != nullptr ? f->name() : QString::number(t.fixture), EncoderBank::slotName(t.bank, t.slot))
        .arg(int(t.from * 100.0f + 0.5f)).arg(int(t.to * 100.0f + 0.5f));
}

EditLive::Snapshot EditLive::snapshot() const
{
    Snapshot s;
    s.cue = m_cue;
    s.trims = trims();
    QSet<quint32> fixtures;
    for (const Trim &t : s.trims)
        fixtures.insert(t.fixture);
    for (quint32 fid : fixtures)
    {
        Fixture *f = m_doc->fixture(fid);
        if (f == nullptr)
            continue;
        s.scale.insert(fid, m_seedScale.value(fid, 1.0));
        for (quint32 ch = 0; ch < f->channels(); ++ch)
        {
            s.values.insert(chanKey(fid, ch), m_trim->value(fid, ch));
            s.base.insert(chanKey(fid, ch), m_base->value(fid, ch));
            s.look.insert(chanKey(fid, ch), m_look.value(chanKey(fid, ch)));
        }
    }
    return s;
}

/*****************************************************************************
 * Store and Drop
 *****************************************************************************/

void EditLive::applyHolds()
{
    if (m_base.isNull() || m_trim.isNull())
        return;
    /* Each channel: what the latest Store still in the undo history put
       there (unless Drop let go of it on stage), else what the stage had.
       A channel a pending trim moved keeps that trim. */
    ShowTreeController *st = m_doc->showTree();
    QSet<quint64> stamps;
    if (st != nullptr)
        for (const ShowTreeController::StepInfo &i : st->undoSteps())
            stamps.insert(i.stamp);
    QSet<quint64> pendingChannels;
    for (quint64 k : std::as_const(m_pending))
        for (quint32 ch : m_slotChannels.value(k))
            pendingChannels.insert(chanKey(keyFixture(k), ch));
    for (auto it = m_original.constBegin(); it != m_original.constEnd(); ++it)
    {
        uchar stage = it.value();
        int look = m_lookOriginal.value(it.key());
        for (const Hold &h : std::as_const(m_holds))
        {
            if (!stamps.contains(h.stamp))
                continue;
            if (h.look.contains(it.key()))
                look = h.look.value(it.key());
            if (!h.dropped && h.stage.contains(it.key()))
                stage = h.stage.value(it.key());
        }
        m_look.insert(it.key(), look);
        const quint32 fid = keyFixture(it.key());
        const quint32 ch = quint32(it.key() & 0xFFFFFFFF);
        if (m_base->value(fid, ch) == stage)
            continue;
        if (!pendingChannels.contains(it.key()))
            m_trim->setValue(fid, ch, stage);
        m_base->setValue(fid, ch, stage);
    }
}

void EditLive::drop()
{
    if (m_base.isNull() || m_trim.isNull())
        return;
    if (m_pending.isEmpty())
    {
        if (!isHolding())
            return;
        // What was stored goes back to what the cue plays now.
        for (Hold &h : m_holds)
            h.dropped = true;
        applyHolds();
    }
    else
    {
        for (quint64 k : std::as_const(m_pending))
        {
            const quint32 fid = keyFixture(k);
            for (quint32 ch : m_slotChannels.value(k))
                m_trim->setValue(fid, ch, m_base->value(fid, ch));
        }
        m_pending.clear();
    }
    m_slotChannels.clear();
    m_pickups.clear();
    push();
    emit changed();
}

void EditLive::treeChanged()
{
    // A Store undone lets go of the stage; redone, holds it again.
    if (m_holds.isEmpty())
        return;
    applyHolds();
    recomputePending();
    push();
    emit changed();
}

bool EditLive::store(QString *why)
{
    if (!m_on || m_pending.isEmpty())
    {
        if (why != nullptr)
            *why = tr("Nothing to store.");
        return false;
    }
    const Snapshot snap = snapshot();
    if (!storeSnapshot(snap, why))
        return false;
    /* Stored: the running cue plays it from its next GO, so it stays on the
       rig until then -- held, not pending (Drop or an undo lets go). */
    Hold h;
    if (ShowTreeController *st = m_doc->showTree())
        if (!st->undoSteps().isEmpty())
            h.stamp = st->undoSteps().last().stamp;
    QSet<quint32> fixtures;
    for (const Trim &t : snap.trims)
        fixtures.insert(t.fixture);
    for (quint32 fid : fixtures)
    {
        const qreal s0 = snap.scale.value(fid, 1.0);
        for (const SceneValue &v : m_trim->values())
        {
            if (v.fxi != fid)
                continue;
            const quint64 k = chanKey(fid, v.channel);
            const int delta = int(v.value) - int(m_base->value(fid, v.channel));
            if (delta == 0)
                continue;
            h.stage.insert(k, v.value);
            const qreal d = isIntensity(fid, v.channel) && s0 > 0.0 ? delta / s0 : delta;
            h.look.insert(k, qBound(0, qRound(m_look.value(k) + d), 255));
        }
    }
    m_holds << h;
    m_pending.clear();
    m_slotChannels.clear();
    applyHolds();
    push();
    emit changed();
    return true;
}

bool EditLive::storeLeftBehind(QString *why)
{
    if (!hasLeftBehind())
    {
        if (why != nullptr)
            *why = tr("Nothing to store.");
        return false;
    }
    const bool ok = storeSnapshot(m_left, why);
    if (ok)
        forgetLeftBehind();
    return ok;
}

void EditLive::forgetLeftBehind()
{
    const bool had = hasLeftBehind();
    m_left = Snapshot();
    ++m_leftGeneration;
    if (had)
        emit changed();
}

namespace
{
/** The look key a slot's trim is stored as on @p f (empty: none). */
QString lookKey(const Fixture *f, int bank, int slot)
{
    switch (bank)
    {
        case EncoderBank::IntensityBank:
            // No dimmer: intensity scaled the colour.
            return dimmerChannel(f) != QLCChannel::invalid() ? KeyDimmer : KeyColour;
        case EncoderBank::ColourBank: return KeyColour;
        case EncoderBank::PositionBank: return KeyPosition;
        case EncoderBank::BeamBank: return slot == 0 ? KeyZoom : KeyBeam;
        default: return QString();
    }
}

/** Whether a palette of @p type is a look of @p key that a trim can move. */
bool movable(const QString &key, QLCPalette::PaletteType type)
{
    if (key == KeyDimmer)
        return type == QLCPalette::Dimmer;
    if (key == KeyColour)
        return type == QLCPalette::Color;
    if (key == KeyPosition)
        return type == QLCPalette::PanTilt || type == QLCPalette::Pan || type == QLCPalette::Tilt;
    if (key == KeyZoom)
        return type == QLCPalette::Zoom;
    if (key == KeyBeam)
        return type == QLCPalette::Beam;
    return false;
}

QColor shifted(const QColor &c, int dr, int dg, int db)
{
    return QColor(qBound(0, c.red() + dr, 255), qBound(0, c.green() + dg, 255), qBound(0, c.blue() + db, 255));
}

/** A fixture's definition and mode: the heads one DMX delta means the
 *  same move on. */
QString typeOf(const Fixture *f)
{
    const QLCFixtureDef *def = f->fixtureDef();
    const QLCFixtureMode *mode = f->fixtureMode();
    return (def != nullptr ? def->manufacturer() + QLatin1Char('/') + def->model() : QString())
           + QLatin1Char('/') + (mode != nullptr ? mode->name() : QString());
}
}

bool EditLive::storeSnapshot(const Snapshot &snap, QString *why)
{
    ShowTreeController *st = m_doc->showTree();
    const CueList *list = st != nullptr ? st->listOfCue(snap.cue) : nullptr;
    if (list == nullptr || snap.trims.isEmpty())
    {
        if (why != nullptr)
            *why = list == nullptr ? tr("The cue is gone.") : tr("Nothing to store.");
        return false;
    }
    const int at = list->indexOf(snap.cue);
    const QList<ResolvedCue> res = st->resolved(*list);
    if (at < 0 || at >= res.size())
        return false;
    const ResolvedCue rc = res.at(at);

    /* The look's level of a channel before (was) and after (now): the
       trim's delta on stage added to it, in look terms (an intensity's
       delta at a 50% list master is twice that in the look). */
    auto lookWas = [&snap](quint32 fid, quint32 ch) -> int {
        return ch == QLCChannel::invalid() ? -1 : snap.look.value(chanKey(fid, ch));
    };
    auto lookNow = [&](quint32 fid, quint32 ch) -> int {
        if (ch == QLCChannel::invalid())
            return -1;
        const quint64 k = chanKey(fid, ch);
        const int delta = int(snap.values.value(k)) - int(snap.base.value(k));
        const qreal s0 = snap.scale.value(fid, 1.0);
        const qreal d = isIntensity(fid, ch) && s0 > 0.0 ? delta / s0 : delta;
        return qBound(0, qRound(snap.look.value(k) + d), 255);
    };
    auto degrees = [&](Fixture *f, int type, bool now) {
        const quint32 msb = f->channelNumber(type, QLCChannel::MSB);
        const quint32 lsb = f->channelNumber(type, QLCChannel::LSB);
        const int hi = qMax(0, now ? lookNow(f->id(), msb) : lookWas(f->id(), msb));
        const int lo = qMax(0, now ? lookNow(f->id(), lsb) : lookWas(f->id(), lsb));
        return RefineSession::dmxToDegrees(f, type, quint16((hi << 8) | lo));
    };
    auto dmx16 = [&](Fixture *f, int type, bool now) {
        const quint32 msb = f->channelNumber(type, QLCChannel::MSB);
        const quint32 lsb = f->channelNumber(type, QLCChannel::LSB);
        const int hi = qMax(0, now ? lookNow(f->id(), msb) : lookWas(f->id(), msb));
        const int lo = qMax(0, now ? lookNow(f->id(), lsb) : lookWas(f->id(), lsb));
        return (hi << 8) | lo;
    };
    auto zoomDegrees = [&](Fixture *f, bool now) -> float {
        for (quint32 ch = 0; ch < f->channels(); ++ch)
        {
            const QLCChannel *c = f->channel(ch);
            if (c == nullptr || f->fixtureMode() == nullptr || c->controlByte() != QLCChannel::MSB
                || (c->preset() != QLCChannel::BeamZoomSmallBig && c->preset() != QLCChannel::BeamZoomBigSmall))
                continue;
            const int v = now ? lookNow(f->id(), ch) : lookWas(f->id(), ch);
            const QLCPhysical phy = f->fixtureMode()->physical();
            const int d = c->preset() == QLCChannel::BeamZoomBigSmall ? 255 - v : v;
            return float(phy.lensDegreesMin() + (phy.lensDegreesMax() - phy.lensDegreesMin()) * d / 255.0);
        }
        return -1.0f;
    };
    // Focus, frost, iris (as a Beam look holds them).
    auto beamOf = [&](Fixture *f, bool now, QVariantList beam) {
        for (quint32 ch = 0; ch < f->channels(); ++ch)
        {
            const QLCChannel *c = f->channel(ch);
            if (c == nullptr || c->controlByte() != QLCChannel::MSB)
                continue;
            const int v = now ? lookNow(f->id(), ch) : lookWas(f->id(), ch);
            switch (c->preset())
            {
                case QLCChannel::BeamFocusNearFar: beam[0] = v; break;
                case QLCChannel::BeamFocusFarNear: beam[0] = 255 - v; break;
                case QLCChannel::ShutterIrisMinToMax: beam[2] = v; break;
                case QLCChannel::ShutterIrisMaxToMin: beam[2] = 255 - v; break;
                default:
                    if (c->group() == QLCChannel::Beam && c->name().contains(QLatin1String("frost"), Qt::CaseInsensitive))
                        beam[1] = v;
                break;
            }
        }
        return beam;
    };

    /* Per fixture and key, the layer that sets that key on it in this cue:
       a child layer wins (the deepest), else the first top layer that sets
       it (the lower layer wins what two set). None sets it: the most
       specific layer that lights the fixture. */
    auto layerFor = [&](quint32 fid, const QString &key) {
        int best = -1, bestDepth = -1, lit = -1;
        for (int i = 0; i < rc.layers.size(); ++i)
        {
            const ResolvedLayer &l = rc.layers.at(i);
            if (l.target.isOpaque() || !st->targetFixtures(l.target).contains(fid))
                continue;
            lit = i;
            const auto v = l.values.constFind(key);
            if (v == l.values.constEnd() || v->look.none)
                continue;
            if (l.depth > bestDepth)
            {
                best = i;
                bestDepth = l.depth;
            }
        }
        return best >= 0 ? best : lit;
    };

    struct Group
    {
        int layer = -1;
        QString key;
        QList<quint32> fixtures;
    };
    QMap<QString, Group> groups;
    QStringList refused;
    for (const Trim &t : snap.trims)
    {
        Fixture *f = m_doc->fixture(t.fixture);
        if (f == nullptr)
            continue;
        const QString key = lookKey(f, t.bank, t.slot);
        const int layer = key.isEmpty() ? -1 : layerFor(t.fixture, key);
        if (layer < 0)
        {
            refused << tr("%1 is in no layer of this cue").arg(f->name());
            continue;
        }
        Group &g = groups[QStringLiteral("%1|%2").arg(layer).arg(key)];
        g.layer = layer;
        g.key = key;
        if (!g.fixtures.contains(t.fixture))
            g.fixtures << t.fixture;
    }
    if (groups.isEmpty())
    {
        if (why != nullptr)
            *why = refused.isEmpty() ? tr("Nothing to store.") : refused.join(QStringLiteral("; "));
        return false;
    }

    // @p key's value on @p fid, the trim in it: a flat look of it.
    auto valueFor = [&](const QString &key, quint32 fid, const Look &was) -> StoreValue {
        StoreValue out;
        Fixture *f = m_doc->fixture(fid);
        if (f == nullptr)
            return out;
        if (key == KeyDimmer)
        {
            const quint32 ch = dimmerChannel(f);
            const QLCChannel *qch = f->channel(ch);
            const int ceiling = qch != nullptr && qch->dimmerCeiling() > 0 ? qch->dimmerCeiling() : 255;
            out.type = QLCPalette::Dimmer;
            out.values << qBound(0, lookNow(fid, ch) * 255 / ceiling, 255);
        }
        else if (key == KeyColour)
        {
            if (f->channelNumber(QLCChannel::Red, QLCChannel::MSB, 0) == QLCChannel::invalid())
                return out;
            auto c = [&](int type) { return qMax(0, lookNow(fid, f->channelNumber(type, QLCChannel::MSB, 0))); };
            out.type = QLCPalette::Color;
            out.values << QLCPalette::colorToString(QColor(c(QLCChannel::Red), c(QLCChannel::Green), c(QLCChannel::Blue)),
                                                    QColor(c(QLCChannel::White), c(QLCChannel::Amber), c(QLCChannel::UV)));
        }
        else if (key == KeyPosition)
        {
            if (f->channelNumber(QLCChannel::Pan, QLCChannel::MSB) == QLCChannel::invalid()
                && f->channelNumber(QLCChannel::Tilt, QLCChannel::MSB) == QLCChannel::invalid())
                return out;
            out.type = QLCPalette::PanTilt;
            out.values << qRound(degrees(f, QLCChannel::Pan, true)) << qRound(degrees(f, QLCChannel::Tilt, true));
        }
        else if (key == KeyZoom)
        {
            const float z = zoomDegrees(f, true);
            if (z < 0.0f)
                return out;
            out.type = QLCPalette::Zoom;
            out.values << double(qRound(z * 10.0f) / 10.0f);
        }
        else if (key == KeyBeam)
        {
            QVariantList beam { 0, 0, 0 };
            for (quint32 pid : was.palettes)
                if (const QLCPalette *p = m_doc->palette(pid))
                    if (p->type() == QLCPalette::Beam)
                        for (int i = 0; i < qMin(3, int(p->values().size())); ++i)
                            beam[i] = p->values().at(i);
            out.type = QLCPalette::Beam;
            out.values = beamOf(f, true, beam);
        }
        return out;
    };
    /* What the trim moved @p key by on @p fid, in the look's terms: the same
       on every head (of one type, for a position) is one move of the look. */
    auto shiftOf = [&](const QString &key, quint32 fid) -> QList<double> {
        Fixture *f = m_doc->fixture(fid);
        QList<double> d;
        if (f == nullptr)
            return d;
        if (key == KeyDimmer)
        {
            const quint32 ch = dimmerChannel(f);
            const QLCChannel *qch = f->channel(ch);
            const int ceiling = qch != nullptr && qch->dimmerCeiling() > 0 ? qch->dimmerCeiling() : 255;
            d << double(lookNow(fid, ch) - lookWas(fid, ch)) * 255.0 / ceiling;
        }
        else if (key == KeyColour)
        {
            for (int type : { int(QLCChannel::Red), int(QLCChannel::Green), int(QLCChannel::Blue),
                              int(QLCChannel::White), int(QLCChannel::Amber), int(QLCChannel::UV) })
            {
                const quint32 ch = f->channelNumber(type, QLCChannel::MSB, 0);
                d << (ch == QLCChannel::invalid() ? 0.0 : double(lookNow(fid, ch) - lookWas(fid, ch)));
            }
        }
        else if (key == KeyPosition)
        {
            d << double(dmx16(f, QLCChannel::Pan, true) - dmx16(f, QLCChannel::Pan, false))
              << double(dmx16(f, QLCChannel::Tilt, true) - dmx16(f, QLCChannel::Tilt, false));
        }
        else if (key == KeyZoom)
            d << double(zoomDegrees(f, true) - zoomDegrees(f, false));
        else if (key == KeyBeam)
        {
            const QVariantList a = beamOf(f, true, { 0, 0, 0 }), b = beamOf(f, false, { 0, 0, 0 });
            for (int i = 0; i < 3; ++i)
                d << double(a.at(i).toInt() - b.at(i).toInt());
        }
        return d;
    };
    auto labelFor = [this](const QString &key) {
        return key == KeyDimmer ? tr("Edit Live level") : key == KeyColour ? tr("Edit Live colour")
             : key == KeyPosition ? tr("Edit Live position") : key == KeyZoom ? tr("Edit Live zoom")
                                                                              : tr("Edit Live beam");
    };
    auto lookOf = [&](const QString &key, const StoreValue &v) {
        Look look;
        look.key = key;
        look.palettes << st->createInlinePalette(v.type, v.values, labelFor(key));
        return look;
    };
    // @p was moved by @p d (the first head's shift), its fan kept.
    auto shiftedLook = [&](const QString &key, const Look &was, const QList<double> &d, float dPan, float dTilt) {
        Look look = was;
        look.palettes.clear();
        for (quint32 pid : was.palettes)
        {
            const QLCPalette *p = m_doc->palette(pid);
            if (p == nullptr || !movable(key, p->type()))
            {
                look.palettes << pid;
                continue;
            }
            QVariantList v = p->values();
            QVariant fan = p->fanningValue();
            const bool fanned = p->fanningType() != QLCPalette::Flat;
            if (key == KeyDimmer && !v.isEmpty())
            {
                v[0] = qBound(0, qRound(v.at(0).toDouble() + d.value(0)), 255);
                if (fanned && fan.isValid())
                    fan = qBound(0, qRound(fan.toDouble() + d.value(0)), 255);
            }
            else if (key == KeyColour)
            {
                const int dr = qRound(d.value(0)), dg = qRound(d.value(1)), db = qRound(d.value(2));
                const int dw = qRound(d.value(3)), da = qRound(d.value(4)), du = qRound(d.value(5));
                for (int i = 0; i < v.size(); ++i)
                {
                    QColor rgb, wauv;
                    QLCPalette::stringToColor(v.at(i).toString(), rgb, wauv);
                    if (wauv.isValid() || dw != 0 || da != 0 || du != 0)
                        wauv = shifted(wauv.isValid() ? wauv : QColor(Qt::black), dw, da, du);
                    v[i] = QLCPalette::colorToString(shifted(rgb, dr, dg, db), wauv);
                }
                if (fanned && fan.canConvert<QColor>() && fan.value<QColor>().isValid())
                    fan = shifted(fan.value<QColor>(), dr, dg, db);
            }
            else if (key == KeyPosition)
            {
                if (p->type() == QLCPalette::PanTilt && v.size() >= 2)
                {
                    v[0] = qRound(v.at(0).toFloat() + dPan);
                    v[1] = qRound(v.at(1).toFloat() + dTilt);
                }
                else if (!v.isEmpty())
                    v[0] = qRound(v.at(0).toFloat() + (p->type() == QLCPalette::Pan ? dPan : dTilt));
            }
            else if (key == KeyZoom && !v.isEmpty())
            {
                v[0] = double(qRound((v.at(0).toDouble() + d.value(0)) * 10.0) / 10.0);
                if (fanned && fan.isValid())
                    fan = fan.toDouble() + d.value(0);
            }
            else if (key == KeyBeam)
            {
                for (int i = 0; i < qMin(3, int(v.size())); ++i)
                    v[i] = qBound(0, qRound(v.at(i).toDouble() + d.value(i)), 255);
            }
            const quint32 np = st->createInlinePalette(p->type(), v, labelFor(key));
            if (QLCPalette *n = m_doc->palette(np))
            {
                n->setFanningType(p->fanningType());
                n->setFanningLayout(p->fanningLayout());
                n->setFanningAmount(p->fanningAmount());
                n->setFanningValue(fan);
            }
            look.palettes << np;
        }
        return look;
    };

    QStringList notes;
    st->beginBatch(tr("Store Edit Live trims (cue %1)").arg(cueText(snap.cue)));
    bool ok = true;
    for (const Group &g : std::as_const(groups))
    {
        const ResolvedLayer &rl = rc.layers.at(g.layer);
        const Look was = rl.values.value(g.key).look;
        // Where the value is set now: this cue's layer, or the cue it tracks from.
        quint32 placeCue = snap.cue, placeLayer = rl.layerId;
        quint32 srcCue = kInvalid, srcLayer = kInvalid;
        if (st->valueSource(snap.cue, rl.target, g.key, &srcCue, &srcLayer))
        {
            placeCue = srcCue;
            placeLayer = srcLayer;
        }
        else if (!rl.own && rl.fromCue != kInvalid)
            placeCue = rl.fromCue;
        const ShowTree::Cue *pc = st->cue(placeCue);
        if (pc == nullptr)
        {
            ok = false;
            break;
        }
        if (placeLayer == kInvalid || pc->layer(placeLayer) == nullptr)
        {
            // Set by that cue's own look: the layer there on this target.
            placeLayer = kInvalid;
            for (const ShowTree::Layer &l : pc->layers)
                if (l.target == rl.target)
                    placeLayer = l.id;
        }

        QList<quint32> all = st->targetFixtures(rl.target);
        QList<quint32> mine = g.fixtures;
        std::sort(all.begin(), all.end());
        std::sort(mine.begin(), mine.end());
        const bool whole = mine == all;

        QMap<quint32, StoreValue> values;
        bool same = true;
        for (quint32 fid : std::as_const(mine))
        {
            const StoreValue v = valueFor(g.key, fid, was);
            if (!v.isValid())
                continue;
            if (!values.isEmpty() && !(v == values.first()))
                same = false;
            values.insert(fid, v);
        }
        if (values.isEmpty())
        {
            refused << tr("%1 of %2").arg(keyLabel(g.key), st->targetLabel(rl.target));
            continue;
        }

        // One move of the whole layer (one per fixture type for a position)?
        bool even = true;
        QHash<QString, QList<double>> firstOf;
        QList<double> first;
        for (quint32 fid : std::as_const(mine))
        {
            Fixture *f = m_doc->fixture(fid);
            const QList<double> d = shiftOf(g.key, fid);
            const QString group = g.key == KeyPosition && f != nullptr ? typeOf(f) : QString();
            if (first.isEmpty())
                first = d;
            if (!firstOf.contains(group))
            {
                firstOf.insert(group, d);
                continue;
            }
            const QList<double> &ref = firstOf.value(group);
            const double tol = g.key == KeyZoom ? 0.5 : g.key == KeyPosition ? 0.0 : 1.0;
            for (int i = 0; i < d.size() && even; ++i)
                even = qAbs(d.at(i) - ref.value(i)) <= tol;
        }
        bool movableLook = false;
        for (quint32 pid : was.palettes)
            if (const QLCPalette *p = m_doc->palette(pid))
                movableLook = movableLook || movable(g.key, p->type());

        if (whole && even && movableLook && placeLayer != kInvalid)
        {
            /* The whole layer moved as one: its look moved by that, a fan
               or a formation kept (Store = the look + the trim). */
            float dPan = 0.0f, dTilt = 0.0f;
            if (g.key == KeyPosition)
                if (Fixture *f = m_doc->fixture(mine.first()))
                {
                    dPan = degrees(f, QLCChannel::Pan, true) - degrees(f, QLCChannel::Pan, false);
                    dTilt = degrees(f, QLCChannel::Tilt, true) - degrees(f, QLCChannel::Tilt, false);
                }
            ok = st->setLayerLook(placeCue, placeLayer, shiftedLook(g.key, was, first, dPan, dTilt));
        }
        else if (same && whole)
        {
            // Every head the same: one look on the layer.
            const Look look = lookOf(g.key, values.first());
            ok = placeLayer != kInvalid ? st->setLayerLook(placeCue, placeLayer, look)
                                        : st->setTargetLook(placeCue, rl.target, look);
        }
        else if (placeLayer == kInvalid)
        {
            refused << tr("%1 of %2").arg(keyLabel(g.key), st->targetLabel(rl.target));
            continue;
        }
        else
        {
            /* Some of the layer's heads with one result: one exception (a
               child layer) on just those. Heads that end differently (a fan
               moved unevenly, mixed levels): each its own, so the next GO
               plays what was on stage. */
            QList<QPair<QList<quint32>, StoreValue>> parts;
            if (same)
                parts << qMakePair(values.keys(), values.first());
            else
                for (auto it = values.constBegin(); it != values.constEnd(); ++it)
                    parts << qMakePair(QList<quint32>({ it.key() }), it.value());
            for (const auto &part : std::as_const(parts))
            {
                Target t;
                t.fixtures = part.first;
                quint32 child = kInvalid;
                if (rl.target == t)
                    child = placeLayer;     // the layer is those fixtures already
                if (const ShowTree::Cue *c = st->cue(placeCue))
                    for (const ShowTree::Layer &l : c->layers)
                        if (child == kInvalid && l.parent == placeLayer && l.target == t)
                            child = l.id;
                if (child == kInvalid)
                    child = st->addChildLayer(placeCue, placeLayer, t, why);
                ok = child != kInvalid && st->setLayerLook(placeCue, child, lookOf(g.key, part.second));
                if (!ok)
                    break;
            }
        }
        if (!ok)
            break;
        const QString layerName = rl.name.isEmpty() ? st->targetLabel(rl.target) : rl.name;
        notes << (placeCue == snap.cue
                  ? tr("%1 of %2 in cue %3").arg(keyLabel(g.key), layerName, cueText(placeCue))
                  : tr("%1 of %2 in cue %3 (it tracks into cue %4)")
                        .arg(keyLabel(g.key), layerName, cueText(placeCue), cueText(snap.cue)));
    }
    ok = ok && !notes.isEmpty();
    st->endBatch(ok);
    if (!ok)
    {
        if (why != nullptr && why->isEmpty())
            *why = refused.isEmpty() ? tr("The Show Tree did not take the change.")
                                     : tr("Not stored: %1.").arg(refused.join(QStringLiteral(", ")));
        return false;
    }
    m_note = tr("Stored %1.").arg(notes.join(QStringLiteral("; ")));
    if (!refused.isEmpty())
        m_note += QLatin1Char(' ') + tr("Not stored: %1.").arg(refused.join(QStringLiteral(", ")));
    emit stored(m_note);
    return true;
}
