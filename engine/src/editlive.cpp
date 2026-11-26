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
        connect(st, &ShowTreeController::hitArmingChanged, this, &EditLive::checkCue);
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
    const quint32 prod = st->currentProduction();
    if (prod != kInvalid)
    {
        const quint32 c = st->runningCue(st->mainList(prod));
        if (c != kInvalid)
            return c;
    }
    for (const Production &p : st->productions())
        for (const CueList &l : p.lists)
        {
            if (l.looks || l.isSubCues())
                continue;
            const quint32 c = st->runningCue(l.id);
            if (c != kInvalid)
                return c;
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
        QTimer::singleShot(leftBehindMs(), this, [this, cue = m_left.cue]() {
            if (m_left.cue == cue)
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
    if (m_base.isNull() || m_trim.isNull())
        return;
    m_pending.clear();
    m_pickups.clear();
    m_original.clear();
    m_seeded.clear();
    for (const SceneValue &v : m_base->values())
        m_base->unsetValue(v.fxi, v.channel);
    for (const SceneValue &v : m_trim->values())
        m_trim->unsetValue(v.fxi, v.channel);
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

void EditLive::seed(const QList<quint32> &fixtures)
{
    if (m_base.isNull() || m_trim.isNull())
        return;
    InputOutputMap *iom = m_doc->inputOutputMap();
    QHash<quint32, QByteArray> frames;
    for (quint32 fid : fixtures)
    {
        if (m_seeded.contains(fid))
            continue;
        Fixture *f = m_doc->fixture(fid);
        if (f == nullptr)
            continue;
        // What the stage has on it now, before the Grand Master.
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
            const int addr = int(f->address() + ch);
            const uchar v = addr < frame.size() ? uchar(frame.at(addr)) : 0;
            m_base->setValue(fid, ch, v);
            m_trim->setValue(fid, ch, v);
            m_original.insert(chanKey(fid, ch), v);
        }
        m_seeded.insert(fid);
    }
}

void EditLive::afterEdit(const QList<quint32> &fixtures, int bank, int slot)
{
    for (quint32 fid : fixtures)
    {
        Fixture *f = m_doc->fixture(fid);
        if (f == nullptr)
            continue;
        bool moved = false;
        for (quint32 ch = 0; ch < f->channels() && !moved; ++ch)
            moved = m_trim->value(fid, ch) != m_base->value(fid, ch);
        const quint64 k = slotKey(fid, bank, slot);
        if (!moved)
        {
            // Back where it was: nothing of this fixture is pending.
            m_pending.erase(std::remove_if(m_pending.begin(), m_pending.end(),
                                           [fid](quint64 p) { return keyFixture(p) == fid; }),
                            m_pending.end());
        }
        else if (!m_pending.contains(k))
            m_pending << k;
    }
    push();
    emit changed();
}

void EditLive::push()
{
    QList<OutputLayer::Level> levels;
    for (auto it = m_original.constBegin(); it != m_original.constEnd(); ++it)
    {
        const quint32 fid = keyFixture(it.key());
        const quint32 ch = quint32(it.key() & 0xFFFFFFFF);
        const uchar v = m_trim->value(fid, ch);
        if (v != it.value())
            levels << OutputLayer::Level { fid, ch, v };
    }
    m_layer.set(levels);
}

bool EditLive::adjust(int bank, int slot, int detents, bool fine)
{
    QList<quint32> fixtures;
    if (!begin(bank, slot, &fixtures) || m_trim.isNull())
        return false;
    if (!EncoderAttributes::adjust(m_doc, m_trim, fixtures, bank, slot, detents, fine))
        return false;
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
    // Pick-up (Q299): the fader waits until it meets what is on stage.
    FaderPickup &pick = m_pickups[(quint64(universe) << 32) | channel];
    pick.setSource(universe, channel);
    const int current = qBound(0, int(norm * 255.0f + 0.5f), 255);
    if (!pick.accept(value, current))
    {
        emit message(FaderPickup::hintText(EncoderBank::slotName(bank, slot), int(norm * 100.0f + 0.5f)));
        return false;
    }
    if (!EncoderAttributes::set(m_doc, m_trim, fixtures, bank, slot, float(value) / 255.0f))
        return false;
    afterEdit(fixtures, bank, slot);
    return true;
}

/*****************************************************************************
 * What is pending
 *****************************************************************************/

QList<EditLive::Trim> EditLive::trims() const
{
    QList<Trim> out;
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

void EditLive::drop()
{
    if (m_pending.isEmpty())
        return;
    QSet<quint32> fixtures;
    for (quint64 k : std::as_const(m_pending))
        fixtures.insert(keyFixture(k));
    for (quint32 fid : fixtures)
        for (const SceneValue &v : m_base->values())
            if (v.fxi == fid)
                m_trim->setValue(fid, v.channel, v.value);
    m_pending.clear();
    m_pickups.clear();
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
       rig until then -- held, not pending. */
    for (const SceneValue &v : m_trim->values())
        m_base->setValue(v.fxi, v.channel, v.value);
    m_pending.clear();
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

    auto val = [](const QHash<quint64, uchar> &m, quint32 fid, quint32 ch) -> int {
        return ch == QLCChannel::invalid() ? -1 : int(m.value(chanKey(fid, ch), 0));
    };

    // Per layer and key, the fixtures trimmed there.
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
        int layer = -1;
        for (int i = 0; i < rc.layers.size(); ++i)
            if (!rc.layers.at(i).target.isOpaque()
                && st->targetFixtures(rc.layers.at(i).target).contains(t.fixture))
                layer = i;      // the last is the most specific (a child after its parent)
        if (key.isEmpty() || layer < 0)
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

    /* The look for @p key from what is on the first fixture now; a position
       moves the look that was there by the trim, so a fan stays a fan. */
    auto lookFor = [&](const QString &key, quint32 fid, const Look &was) -> Look {
        Look look;
        look.key = key;
        Fixture *f = m_doc->fixture(fid);
        if (f == nullptr)
            return Look();
        if (key == KeyDimmer)
        {
            const quint32 ch = dimmerChannel(f);
            const QLCChannel *qch = f->channel(ch);
            const int ceiling = qch != nullptr && qch->dimmerCeiling() > 0 ? qch->dimmerCeiling() : 255;
            const int v = qBound(0, val(snap.values, fid, ch) * 255 / ceiling, 255);
            look.palettes << st->createInlinePalette(QLCPalette::Dimmer, { v }, tr("Edit Live level"));
        }
        else if (key == KeyColour)
        {
            auto c = [&](int type) {
                return qMax(0, val(snap.values, fid, f->channelNumber(type, QLCChannel::MSB, 0)));
            };
            const quint32 red = f->channelNumber(QLCChannel::Red, QLCChannel::MSB, 0);
            if (red == QLCChannel::invalid())
                return Look();
            const QColor rgb(c(QLCChannel::Red), c(QLCChannel::Green), c(QLCChannel::Blue));
            const QColor wauv(c(QLCChannel::White), c(QLCChannel::Amber), c(QLCChannel::UV));
            look.palettes << st->createInlinePalette(QLCPalette::Color,
                                                     { QLCPalette::colorToString(rgb, wauv) },
                                                     tr("Edit Live colour"));
        }
        else if (key == KeyPosition)
        {
            auto deg = [&](const QHash<quint64, uchar> &m, int type) {
                const quint32 msb = f->channelNumber(type, QLCChannel::MSB);
                const quint32 lsb = f->channelNumber(type, QLCChannel::LSB);
                const int hi = qMax(0, val(m, fid, msb));
                const int lo = qMax(0, val(m, fid, lsb));
                return RefineSession::dmxToDegrees(f, type, quint16((hi << 8) | lo));
            };
            const float pan = deg(snap.values, QLCChannel::Pan), tilt = deg(snap.values, QLCChannel::Tilt);
            const float dPan = pan - deg(snap.base, QLCChannel::Pan);
            const float dTilt = tilt - deg(snap.base, QLCChannel::Tilt);
            for (quint32 pid : was.palettes)
            {
                const QLCPalette *p = m_doc->palette(pid);
                if (p == nullptr || (p->type() != QLCPalette::PanTilt && p->type() != QLCPalette::Pan
                                     && p->type() != QLCPalette::Tilt))
                    continue;
                QVariantList v = p->values();
                if (p->type() == QLCPalette::PanTilt && v.size() >= 2)
                {
                    v[0] = qRound(v.at(0).toFloat() + dPan);
                    v[1] = qRound(v.at(1).toFloat() + dTilt);
                }
                else if (!v.isEmpty())
                    v[0] = qRound(v.at(0).toFloat() + (p->type() == QLCPalette::Pan ? dPan : dTilt));
                const quint32 np = st->createInlinePalette(p->type(), v, tr("Edit Live position"));
                if (QLCPalette *n = m_doc->palette(np))
                {
                    n->setFanningType(p->fanningType());
                    n->setFanningLayout(p->fanningLayout());
                    n->setFanningAmount(p->fanningAmount());
                    n->setFanningValue(p->fanningValue());
                }
                look.palettes << np;
            }
            if (look.palettes.isEmpty())    // an aim, or nothing: where it points now
                look.palettes << st->createInlinePalette(QLCPalette::PanTilt, { qRound(pan), qRound(tilt) },
                                                         tr("Edit Live position"));
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
                const int v = val(snap.values, fid, ch);
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
                    return Look();
                look.palettes << st->createInlinePalette(QLCPalette::Zoom, { double(qRound(zoom * 10.0f) / 10.0f) },
                                                         tr("Edit Live zoom"));
            }
            else
                look.palettes << st->createInlinePalette(QLCPalette::Beam, beam, tr("Edit Live beam"));
        }
        return look;
    };

    QStringList notes;
    st->beginBatch(tr("Store Edit Live trims (cue %1)").arg(cueText(snap.cue)));
    bool ok = true;
    for (const Group &g : std::as_const(groups))
    {
        const ResolvedLayer &rl = rc.layers.at(g.layer);
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
        const Look look = lookFor(g.key, g.fixtures.first(), rl.values.value(g.key).look);
        if (look.palettes.isEmpty())
        {
            refused << tr("%1 of %2").arg(keyLabel(g.key), st->targetLabel(rl.target));
            continue;
        }
        QList<quint32> all = st->targetFixtures(rl.target);
        QList<quint32> mine = g.fixtures;
        std::sort(all.begin(), all.end());
        std::sort(mine.begin(), mine.end());
        if (mine == all)
        {
            ok = placeLayer != kInvalid ? st->setLayerLook(placeCue, placeLayer, look)
                                        : st->setTargetLook(placeCue, rl.target, look);
        }
        else if (placeLayer != kInvalid)
        {
            // Some of the layer's fixtures: an exception on just those.
            Target t;
            t.fixtures = mine;
            quint32 child = kInvalid;
            for (const ShowTree::Layer &l : pc->layers)
                if (l.parent == placeLayer && l.target == t)
                    child = l.id;
            if (child == kInvalid)
                child = st->addChildLayer(placeCue, placeLayer, t, why);
            ok = child != kInvalid && st->setLayerLook(placeCue, child, look);
        }
        else
        {
            refused << tr("%1 of %2").arg(keyLabel(g.key), st->targetLabel(rl.target));
            continue;
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
