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
    // A list master pulled while trims are up: the trims follow it.
    m_follow = new QTimer(this);
    m_follow->setInterval(100);
    connect(m_follow, &QTimer::timeout, this, &EditLive::follow);

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
    m_follow->stop();
    m_pending.clear();
    m_slotChannels.clear();
    m_pickups.clear();
    m_original.clear();
    m_seeded.clear();
    m_levels.clear();
    m_levelsRead = false;
    m_heldSteps = -1;
    m_pushedScale = 1.0;
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

void EditLive::follow()
{
    if (!qFuzzyCompare(1.0 + listIntensity(), 1.0 + m_pushedScale))
        push();
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
    return true;
}

uchar EditLive::seedValue(quint32 fixture, quint32 channel, const QByteArray &frame) const
{
    /* The look's level, not the output: the output has the list master,
       the Grand Master's neighbours (Highlight, Park, Focus) and effects
       in it, and a Store must keep what the look plays. A channel the cue
       does not set is what the stage has (a pan at its default); an
       intensity it does not set is dark. */
    const QByteArray look = m_levels.value(fixture);
    const uchar lv = int(channel) < look.size() ? uchar(look.at(int(channel))) : 0;
    if (lv > 0)
        return lv;
    Fixture *f = m_doc->fixture(fixture);
    const QLCChannel *qch = f != nullptr ? f->channel(channel) : nullptr;
    if (qch == nullptr || qch->group() == QLCChannel::Intensity)
        return 0;
    const int addr = int(f->address() + channel);
    return addr < frame.size() ? uchar(frame.at(addr)) : 0;
}

void EditLive::seed(const QList<quint32> &fixtures)
{
    if (m_base.isNull() || m_trim.isNull())
        return;
    if (!m_levelsRead)
    {
        if (ShowTreeController *st = m_doc->showTree())
            m_levels = st->cueLevels(m_cue);
        m_levelsRead = true;
    }
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
        m_base->addFixture(fid);
        m_trim->addFixture(fid);
        for (quint32 ch = 0; ch < f->channels(); ++ch)
        {
            const uchar v = seedValue(fid, ch, frame);
            m_base->setValue(fid, ch, v);
            m_trim->setValue(fid, ch, v);
            m_original.insert(chanKey(fid, ch), v);
        }
        m_seeded.insert(fid);
    }
}

void EditLive::afterEdit(const QList<quint32> &fixtures, int bank, int slot)
{
    Q_UNUSED(fixtures)
    Q_UNUSED(bank)
    Q_UNUSED(slot)
    recomputePending();
    push();
    emit changed();
}

void EditLive::recomputePending()
{
    if (m_base.isNull() || m_trim.isNull())
        return;
    // Per attribute: pending while any channel it moved is off where it was.
    QList<quint64> now;
    for (quint64 k : std::as_const(m_pending))
        now << k;
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
       intensities too, so the stage is never brighter than the cue. */
    const qreal scale = listIntensity();
    QList<OutputLayer::Level> levels;
    for (auto it = m_original.constBegin(); it != m_original.constEnd(); ++it)
    {
        const quint32 fid = keyFixture(it.key());
        const quint32 ch = quint32(it.key() & 0xFFFFFFFF);
        const uchar v = m_trim->value(fid, ch);
        if (v == it.value())
            continue;
        Fixture *f = m_doc->fixture(fid);
        const QLCChannel *qch = f != nullptr ? f->channel(ch) : nullptr;
        const bool intensity = qch != nullptr && qch->group() == QLCChannel::Intensity;
        levels << OutputLayer::Level { fid, ch, intensity ? uchar(qRound(v * scale)) : v };
    }
    m_pushedScale = scale;
    m_layer.set(levels);
    if (levels.isEmpty())
        m_follow->stop();
    else if (!m_follow->isActive())
        m_follow->start();
}

bool EditLive::adjust(int bank, int slot, int detents, bool fine)
{
    QList<quint32> fixtures;
    if (!begin(bank, slot, &fixtures) || m_trim.isNull())
        return false;
    QHash<quint64, uchar> before;
    for (quint32 fid : fixtures)
        for (const SceneValue &v : m_trim->values())
            if (v.fxi == fid)
                before.insert(chanKey(fid, v.channel), v.value);
    if (!EncoderAttributes::adjust(m_doc, m_trim, fixtures, bank, slot, detents, fine))
        return false;
    for (auto it = before.constBegin(); it != before.constEnd(); ++it)
        if (m_trim->value(keyFixture(it.key()), quint32(it.key() & 0xFFFFFFFF)) != it.value())
            m_slotChannels[slotKey(keyFixture(it.key()), bank, slot)].insert(quint32(it.key() & 0xFFFFFFFF));
    afterEdit(fixtures, bank, slot);
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
    // Pick-up (Q299): the fader waits until it meets the look's value.
    FaderPickup &pick = m_pickups[(quint64(universe) << 32) | channel];
    pick.setSource(universe, channel);
    const int current = qBound(0, int(norm * 255.0f + 0.5f), 255);
    if (!pick.accept(value, current))
    {
        emit message(FaderPickup::hintText(EncoderBank::slotName(bank, slot), int(norm * 100.0f + 0.5f)));
        return false;
    }
    QHash<quint64, uchar> before;
    for (quint32 fid : fixtures)
        for (const SceneValue &v : m_trim->values())
            if (v.fxi == fid)
                before.insert(chanKey(fid, v.channel), v.value);
    if (!EncoderAttributes::set(m_doc, m_trim, fixtures, bank, slot, float(value) / 255.0f))
        return false;
    for (auto it = before.constBegin(); it != before.constEnd(); ++it)
        if (m_trim->value(keyFixture(it.key()), quint32(it.key() & 0xFFFFFFFF)) != it.value())
            m_slotChannels[slotKey(keyFixture(it.key()), bank, slot)].insert(quint32(it.key() & 0xFFFFFFFF));
    afterEdit(fixtures, bank, slot);
    return true;
}

/*****************************************************************************
 * What is pending
 *****************************************************************************/

bool EditLive::isHolding() const
{
    if (m_trim.isNull())
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
        for (quint32 ch = 0; ch < f->channels(); ++ch)
        {
            s.values.insert(chanKey(fid, ch), m_trim->value(fid, ch));
            s.base.insert(chanKey(fid, ch), m_base->value(fid, ch));
        }
    }
    return s;
}

/*****************************************************************************
 * Store and Drop
 *****************************************************************************/

void EditLive::releaseHeld(bool keepPending)
{
    if (m_base.isNull() || m_trim.isNull())
        return;
    // What was stored goes back to what the cue plays now.
    QSet<quint64> pendingChannels;
    if (keepPending)
        for (quint64 k : std::as_const(m_pending))
            for (quint32 ch : m_slotChannels.value(k))
                pendingChannels.insert(chanKey(keyFixture(k), ch));
    for (auto it = m_original.constBegin(); it != m_original.constEnd(); ++it)
    {
        const quint32 fid = keyFixture(it.key());
        const quint32 ch = quint32(it.key() & 0xFFFFFFFF);
        if (m_base->value(fid, ch) == it.value())
            continue;
        if (!pendingChannels.contains(it.key()))
            m_trim->setValue(fid, ch, it.value());
        m_base->setValue(fid, ch, it.value());
    }
    m_heldSteps = -1;
}

void EditLive::drop()
{
    if (m_base.isNull() || m_trim.isNull())
        return;
    if (m_pending.isEmpty())
    {
        if (!isHolding())
            return;
        releaseHeld(false);
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
    // The Store undone: what it held goes back to what the cue plays.
    ShowTreeController *st = m_doc->showTree();
    if (m_heldSteps < 0 || st == nullptr || st->undoSteps().size() >= m_heldSteps)
        return;
    releaseHeld(true);
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
    if (!storeSnapshot(snapshot(), why))
        return false;
    /* Stored: the running cue plays it from its next GO, so it stays on the
       rig until then -- held, not pending (Drop or an undo lets go). */
    for (const SceneValue &v : m_trim->values())
        m_base->setValue(v.fxi, v.channel, v.value);
    m_pending.clear();
    m_slotChannels.clear();
    if (ShowTreeController *st = m_doc->showTree())
        m_heldSteps = int(st->undoSteps().size());
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

int valueAt(const QHash<quint64, uchar> &m, quint32 fid, quint32 ch)
{
    return ch == QLCChannel::invalid() ? -1 : int(m.value(chanKey(fid, ch), 0));
}

float degreesAt(Fixture *f, const QHash<quint64, uchar> &m, int type)
{
    const quint32 msb = f->channelNumber(type, QLCChannel::MSB);
    const quint32 lsb = f->channelNumber(type, QLCChannel::LSB);
    const int hi = qMax(0, valueAt(m, f->id(), msb));
    const int lo = qMax(0, valueAt(m, f->id(), lsb));
    return RefineSession::dmxToDegrees(f, type, quint16((hi << 8) | lo));
}

bool isPanTiltType(QLCPalette::PaletteType t)
{
    return t == QLCPalette::PanTilt || t == QLCPalette::Pan || t == QLCPalette::Tilt;
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

    /* The value of @p key on @p fid as it is now: the look levels of the
       trims, so it plays back as it was on stage. */
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
            out.values << qBound(0, valueAt(snap.values, fid, ch) * 255 / ceiling, 255);
        }
        else if (key == KeyColour)
        {
            if (f->channelNumber(QLCChannel::Red, QLCChannel::MSB, 0) == QLCChannel::invalid())
                return out;
            auto c = [&](int type) {
                return qMax(0, valueAt(snap.values, fid, f->channelNumber(type, QLCChannel::MSB, 0)));
            };
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
            out.values << qRound(degreesAt(f, snap.values, QLCChannel::Pan))
                       << qRound(degreesAt(f, snap.values, QLCChannel::Tilt));
        }
        else if (key == KeyZoom || key == KeyBeam)
        {
            QVariantList beam { 0, 0, 0 };
            float zoom = -1.0f;
            if (key == KeyBeam)
                for (quint32 pid : was.palettes)
                    if (const QLCPalette *p = m_doc->palette(pid))
                        if (p->type() == QLCPalette::Beam)
                            for (int i = 0; i < qMin(3, int(p->values().size())); ++i)
                                beam[i] = p->values().at(i);
            for (quint32 ch = 0; ch < f->channels(); ++ch)
            {
                const QLCChannel *c = f->channel(ch);
                if (c == nullptr || c->controlByte() != QLCChannel::MSB)
                    continue;
                const int v = valueAt(snap.values, fid, ch);
                switch (c->preset())
                {
                    case QLCChannel::BeamZoomSmallBig:
                    case QLCChannel::BeamZoomBigSmall:
                    {
                        if (f->fixtureMode() == nullptr)
                            break;
                        const QLCPhysical phy = f->fixtureMode()->physical();
                        const int d = c->preset() == QLCChannel::BeamZoomBigSmall ? 255 - v : v;
                        zoom = float(phy.lensDegreesMin()
                                     + (phy.lensDegreesMax() - phy.lensDegreesMin()) * d / 255.0);
                    }
                    break;
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
            if (key == KeyZoom)
            {
                if (zoom < 0.0f)
                    return out;
                out.type = QLCPalette::Zoom;
                out.values << double(qRound(zoom * 10.0f) / 10.0f);
            }
            else
            {
                out.type = QLCPalette::Beam;
                out.values = beam;
            }
        }
        return out;
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

        QHash<quint32, StoreValue> values;
        bool same = true;
        for (quint32 fid : std::as_const(mine))
        {
            const StoreValue v = valueFor(g.key, fid, was);
            if (!v.isValid())
                continue;
            if (!values.isEmpty() && !(v == values.constBegin().value()))
                same = false;
            values.insert(fid, v);
        }
        if (values.isEmpty())
        {
            refused << tr("%1 of %2").arg(keyLabel(g.key), st->targetLabel(rl.target));
            continue;
        }

        /* A position moved the same on every head of a fanned (or any
           PanTilt) look: the look moved by it, its fan kept. */
        bool done = false;
        if (whole && g.key == KeyPosition && placeLayer != kInvalid)
        {
            QList<quint32> bases;
            for (quint32 pid : was.palettes)
                if (const QLCPalette *p = m_doc->palette(pid))
                    if (isPanTiltType(p->type()))
                        bases << pid;
            float dPan = 0.0f, dTilt = 0.0f;
            bool even = !bases.isEmpty();
            for (int i = 0; i < mine.size() && even; ++i)
            {
                Fixture *f = m_doc->fixture(mine.at(i));
                const float p = degreesAt(f, snap.values, QLCChannel::Pan) - degreesAt(f, snap.base, QLCChannel::Pan);
                const float t = degreesAt(f, snap.values, QLCChannel::Tilt) - degreesAt(f, snap.base, QLCChannel::Tilt);
                if (i == 0)
                {
                    dPan = p;
                    dTilt = t;
                }
                else
                    even = qAbs(p - dPan) <= 0.5f && qAbs(t - dTilt) <= 0.5f;
            }
            if (even)
            {
                Look look;
                look.key = KeyPosition;
                for (quint32 pid : std::as_const(bases))
                {
                    const QLCPalette *p = m_doc->palette(pid);
                    QVariantList v = p->values();
                    if (p->type() == QLCPalette::PanTilt && v.size() >= 2)
                    {
                        v[0] = qRound(v.at(0).toFloat() + dPan);
                        v[1] = qRound(v.at(1).toFloat() + dTilt);
                    }
                    else if (!v.isEmpty())
                        v[0] = qRound(v.at(0).toFloat() + (p->type() == QLCPalette::Pan ? dPan : dTilt));
                    const quint32 np = st->createInlinePalette(p->type(), v, labelFor(KeyPosition));
                    if (QLCPalette *n = m_doc->palette(np))
                    {
                        n->setFanningType(p->fanningType());
                        n->setFanningLayout(p->fanningLayout());
                        n->setFanningAmount(p->fanningAmount());
                        n->setFanningValue(p->fanningValue());
                    }
                    look.palettes << np;
                }
                ok = st->setLayerLook(placeCue, placeLayer, look);
                done = true;
            }
        }
        if (!done && whole && same)
        {
            // Every head the same: one look on the layer.
            const Look look = lookOf(g.key, values.constBegin().value());
            ok = placeLayer != kInvalid ? st->setLayerLook(placeCue, placeLayer, look)
                                        : st->setTargetLook(placeCue, rl.target, look);
            done = true;
        }
        if (!done)
        {
            /* Some of the layer's heads, or heads that now differ (a fan, a
               formation, mixed levels): each its own exception -- a child
               layer on that fixture with its value, so the next GO plays
               what was on stage. */
            if (placeLayer == kInvalid)
            {
                refused << tr("%1 of %2").arg(keyLabel(g.key), st->targetLabel(rl.target));
                continue;
            }
            for (auto it = values.constBegin(); it != values.constEnd() && ok; ++it)
            {
                Target t;
                t.fixtures << it.key();
                quint32 child = kInvalid;
                if (const ShowTree::Cue *c = st->cue(placeCue))
                    for (const ShowTree::Layer &l : c->layers)
                        if (l.parent == placeLayer && l.target == t)
                            child = l.id;
                if (child == kInvalid && rl.target == t)
                    child = placeLayer;     // the layer is that one fixture already
                if (child == kInvalid)
                    child = st->addChildLayer(placeCue, placeLayer, t, why);
                ok = child != kInvalid && st->setLayerLook(placeCue, child, lookOf(g.key, it.value()));
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
