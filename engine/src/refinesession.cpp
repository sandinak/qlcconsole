/*
  Q Light Controller Plus
  refinesession.cpp

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
#include <algorithm>

#include "showtreecontroller.h"
#include "inputoutputmap.h"
#include "paletteops.h"
#include "function.h"
#include "refinesession.h"
#include "focussession.h"
#include "qlcfixturemode.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "fadechannel.h"
#include "scenevalue.h"
#include "righeads.h"
#include "universe.h"
#include "fixture.h"
#include "doc.h"

namespace
{
const quint32 kInvalid = 0xFFFFFFFF;

bool isPositionType(QLCPalette::PaletteType t)
{
    return t == QLCPalette::PanTilt || t == QLCPalette::Pan || t == QLCPalette::Tilt;
}

int focusChannel(const Fixture *fx, bool *reversed)
{
    const QLCFixtureMode *mode = fx != nullptr ? fx->fixtureMode() : nullptr;
    if (mode == nullptr)
        return -1;
    for (quint32 c = 0; c < quint32(mode->channels().size()); ++c)
        if (const QLCChannel *ch = mode->channel(c))
            if (ch->preset() == QLCChannel::BeamFocusNearFar || ch->preset() == QLCChannel::BeamFocusFarNear)
            {
                *reversed = ch->preset() == QLCChannel::BeamFocusFarNear;
                return int(c);
            }
    return -1;
}
}

RefineSession::RefineSession(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_layer(doc, Universe::Focus, FadeChannel::Override, QStringLiteral("Refine"))
{
    m_flashTimer = new QTimer(this);
    m_flashTimer->setInterval(30);
    connect(m_flashTimer, &QTimer::timeout, this, &RefineSession::onFlashTick);
    /* Board Q112a: Refine works in Operate too. Back in Design the cue is
       previewed again and shows what was kept; nothing need be held. */
    if (m_doc != nullptr)
        connect(m_doc, &Doc::modeChanged, this, [this](Doc::Mode mode) {
            if (!m_active)
                return;
            m_levelCache.clear();
            if (mode == Doc::Design && !m_sceneMode)
            {
                m_kept.clear();
                m_doc->showTree()->previewCue(m_cue);
            }
            rebuildWrites();
        });
}

RefineSession::~RefineSession()
{
}

/*****************************************************************************
 * Starting and stopping
 *****************************************************************************/

bool RefineSession::start(quint32 cueId, quint32 layerId, QString *why)
{
    auto refuse = [why](const QString &reason) {
        if (why != nullptr)
            *why = reason;
        return false;
    };
    ShowTreeController *st = m_doc != nullptr ? m_doc->showTree() : nullptr;
    if (st == nullptr || st->cue(cueId) == nullptr)
        return refuse(tr("Choose a cue to refine."));
    if (m_active)
        stop();
    m_sceneMode = false;
    m_cue = cueId;
    const QList<quint32> layers = findLayers(why);
    if (layers.isEmpty())
    {
        m_cue = kInvalid;
        m_tracked.clear();
        return false;
    }
    m_keepToSource = false;
    m_layers = layers;
    m_layerIndex = qMax(0, layers.indexOf(layerId));
    m_scope = LayerScope;
    m_groupIndex = m_fixtureIndex = 0;
    m_page = BeamPage;
    m_edits.clear();
    m_editOrder.clear();
    m_active = true;
    m_kept.clear();
    m_levelCache.clear();
    /* In Design, the cue up as the Show Tree previews it: a kept edit is
       seen at once. In Operate the show is running and stays as it is. */
    if (m_doc->mode() != Doc::Operate && st->previewedCue() != cueId)
        st->previewCue(cueId);
    rebuildWrites();
    emit started();
    emit changed();
    flashSelection();
    const ShowTree::Cue *c = cue();
    say(tr("Refine, cue %1. %2").arg(c->number.isEmpty() ? c->name : c->number, wherePhrase()));
    return true;
}

void RefineSession::stop()
{
    if (!m_active)
        return;
    m_active = false;
    m_edits.clear();
    m_editOrder.clear();
    m_layers.clear();
    m_tracked.clear();
    m_sharedConfirmed = false;
    m_sharedWarning.clear();
    m_sharedUsers.clear();
    m_kept.clear();
    m_levelCache.clear();
    m_layerIndex = -1;
    m_cue = kInvalid;
    m_sceneMode = false;
    for (int s = 0; s < 2; ++s)
        m_stick[s][0] = m_stick[s][1] = 0.5f;
    m_trigger[0].reset();
    m_trigger[1].reset();
    m_hold.reset();
    m_flashTimer->stop();
    m_flashClock.invalidate();
    {
        QMutexLocker locker(&m_writesMutex);
        m_writes.clear();
    }
    m_layer.clear();
    emit stopped();
}

/*****************************************************************************
 * Where it is
 *****************************************************************************/

QList<quint32> RefineSession::findLayers(QString *why) const
{
    ShowTreeController *st = m_doc->showTree();
    QList<quint32> layers;
    const ShowTree::Cue *c = st->cue(m_cue);
    if (c == nullptr)
        return layers;
    for (const ShowTree::Layer &l : c->layers)
        if (l.parent == kInvalid && !l.target.isOpaque() && !st->targetFixtures(l.target).isEmpty())
            layers << l.id;
    /* What only tracks in (section 6C): a stand-in for each top layer
       carried in from an earlier cue, with its looks as they play here. */
    RefineSession *self = const_cast<RefineSession *>(this);
    self->m_tracked.clear();
    const ShowTree::CueList *list = st->listOfCue(m_cue);
    if (list != nullptr)
    {
        const QList<ShowTree::ResolvedCue> rc = st->resolved(*list);
        const int at = list->indexOf(m_cue);
        if (at >= 0 && at < rc.size())
            for (const ShowTree::ResolvedLayer &rl : rc.at(at).layers)
            {
                if (rl.own || rl.depth > 0 || rl.target.isOpaque() || rl.isDark()
                        || st->targetFixtures(rl.target).isEmpty())
                    continue;
                Tracked t;
                t.layer.id = 0xF0000000u + quint32(self->m_tracked.size());
                t.layer.target = rl.target;
                t.layer.name = rl.name;
                t.layer.looks = rl.playedLooks();
                quint32 src = kInvalid;
                for (const QString &key : rl.values.keys())
                    if (src == kInvalid)
                        st->valueSource(m_cue, rl.target, key, &src);
                t.source = src != kInvalid ? src : rl.fromCue;
                self->m_tracked.insert(t.layer.id, t);
                layers << t.layer.id;
            }
    }
    if (layers.isEmpty() && why != nullptr)
        *why = tr("This cue has no layer on fixtures to refine.");
    return layers;
}

bool RefineSession::isTrackedLayer() const
{
    return !m_sceneMode && m_tracked.contains(currentLayer());
}

quint32 RefineSession::trackedSource() const
{
    return m_tracked.value(currentLayer()).source;
}

void RefineSession::setKeepToSource(bool source)
{
    m_keepToSource = source;
    emit changed();
    if (isTrackedLayer())
    {
        const ShowTree::Cue *src = m_doc->showTree()->cue(trackedSource());
        say(source && src != nullptr ? tr("Keeps go into cue %1.").arg(src->number.isEmpty() ? src->name : src->number)
                                     : tr("Keeps go into this cue."));
    }
}

const ShowTree::Cue *RefineSession::cue() const
{
    return m_doc->showTree()->cue(m_cue);
}

const ShowTree::Layer *RefineSession::layer(quint32 layerId) const
{
    const auto t = m_tracked.constFind(layerId);
    if (t != m_tracked.constEnd())
        return &t->layer;
    const ShowTree::Cue *c = cue();
    if (c == nullptr)
        return nullptr;
    for (const ShowTree::Layer &l : c->layers)
        if (l.id == layerId)
            return &l;
    return nullptr;
}

const ShowTree::Layer *RefineSession::childOn(quint32 layerId, const ShowTree::Target &target) const
{
    const ShowTree::Cue *c = cue();
    if (c == nullptr)
        return nullptr;
    for (const ShowTree::Layer &l : c->layers)
        if (l.parent == layerId && l.target == target)
            return &l;
    return nullptr;
}

quint32 RefineSession::currentLayer() const
{
    return m_layers.value(m_layerIndex, kInvalid);
}

void RefineSession::setLayerIndex(int index)
{
    if (!m_active || m_layers.isEmpty())
        return;
    const int n = m_layers.size();
    m_layerIndex = ((index % n) + n) % n;
    m_groupIndex = m_fixtureIndex = 0;
    m_levelCache.clear();
    // A new layer starts as a whole: the layer is what a trigger steps to.
    m_scope = LayerScope;
    rebuildWrites();
    emit changed();
    flashSelection();
    say(wherePhrase());
}

void RefineSession::nextLayer()
{
    setLayerIndex(m_layerIndex + 1);
}

void RefineSession::previousLayer()
{
    setLayerIndex(m_layerIndex - 1);
}

QList<ShowTree::Target> RefineSession::groups() const
{
    if (m_sceneMode)
        return sceneGroups();
    const ShowTree::Layer *l = layer(currentLayer());
    if (l == nullptr)
        return QList<ShowTree::Target>();
    const QList<ShowTree::Target> pieces = l->pieces();
    // One piece is the layer itself: no Group scope.
    return pieces.size() > 1 ? pieces : QList<ShowTree::Target>();
}

QList<quint32> RefineSession::layerFixtures() const
{
    if (m_sceneMode)
        return sceneFixtures();
    const ShowTree::Layer *l = layer(currentLayer());
    return l != nullptr ? m_doc->showTree()->targetFixtures(l->target) : QList<quint32>();
}

quint32 RefineSession::currentFixture() const
{
    return layerFixtures().value(m_fixtureIndex, kInvalid);
}

void RefineSession::setScope(Scope scope)
{
    if (!m_active)
        return;
    if (scope == GroupScope && groups().isEmpty())
        scope = FixtureScope;
    if (scope != LayerScope && isTrackedLayer())
    {
        // An exception needs the layer to be this cue's own first.
        say(tr("Tracked layer: keep it in this cue first, then pick a fixture."));
        return;
    }
    if (scope == m_scope)
        return;
    m_scope = scope;
    rebuildWrites();
    emit changed();
    flashSelection();
    say(wherePhrase());
}

void RefineSession::cycleScope()
{
    switch (m_scope)
    {
        case LayerScope:   setScope(groups().isEmpty() ? FixtureScope : GroupScope); break;
        case GroupScope:   setScope(FixtureScope); break;
        case FixtureScope: setScope(LayerScope); break;
    }
}

void RefineSession::nextItem()
{
    if (!m_active)
        return;
    if (m_scope == LayerScope)
    {
        setScope(FixtureScope);
        return;
    }
    if (m_scope == GroupScope)
        m_groupIndex = (m_groupIndex + 1) % qMax(1, groups().size());
    else
        m_fixtureIndex = (m_fixtureIndex + 1) % qMax(1, layerFixtures().size());
    rebuildWrites();
    emit changed();
    flashSelection();
    say(wherePhrase());
}

void RefineSession::previousItem()
{
    if (!m_active)
        return;
    if (m_scope == LayerScope)
    {
        setScope(FixtureScope);
        return;
    }
    if (m_scope == GroupScope)
    {
        const int n = qMax(1, groups().size());
        m_groupIndex = (m_groupIndex - 1 + n) % n;
    }
    else
    {
        const int n = qMax(1, layerFixtures().size());
        m_fixtureIndex = (m_fixtureIndex - 1 + n) % n;
    }
    rebuildWrites();
    emit changed();
    flashSelection();
    say(wherePhrase());
}

bool RefineSession::pickFixture(quint32 fid)
{
    const int at = layerFixtures().indexOf(fid);
    if (!m_active || at < 0)
        return false;
    m_fixtureIndex = at;
    m_scope = LayerScope;           // so setScope() says and flashes it
    setScope(FixtureScope);
    return true;
}

bool RefineSession::pickGroup(quint32 groupId)
{
    const QList<ShowTree::Target> gs = groups();
    for (int i = 0; i < gs.size(); ++i)
        if (gs.at(i).groups.contains(groupId))
        {
            m_groupIndex = i;
            m_scope = LayerScope;
            setScope(GroupScope);
            return true;
        }
    return false;
}

ShowTree::Target RefineSession::scopeTarget() const
{
    if (m_scope == GroupScope)
        return groups().value(m_groupIndex);
    ShowTree::Target t;
    if (m_scope == FixtureScope && currentFixture() != kInvalid)
        t.fixtures << currentFixture();
    else if (const ShowTree::Layer *l = layer(currentLayer()))
        t = l->target;
    return t;
}

QList<quint32> RefineSession::scopeFixtures() const
{
    if (!m_active)
        return QList<quint32>();
    switch (m_scope)
    {
        case LayerScope:   return layerFixtures();
        case GroupScope:   return m_doc->showTree()->targetFixtures(scopeTarget());
        case FixtureScope:
            return currentFixture() != kInvalid ? QList<quint32>{ currentFixture() } : QList<quint32>();
    }
    return QList<quint32>();
}

void RefineSession::cyclePage()
{
    m_page = Page((int(m_page) + 1) % 3);
    emit changed();
    switch (m_page)
    {
        case BeamPage:   say(tr("Beam page: zoom and focus.")); break;
        case ColourPage: say(tr("Colour page.")); break;
        case GoboPage:   say(tr("Gobo page.")); break;
    }
}

/*****************************************************************************
 * Values in the cue
 *****************************************************************************/

const ShowTree::Look *RefineSession::lookFor(quint32 fid, const QString &key, QList<quint32> *lit) const
{
    if (m_sceneMode)
        return sceneLookFor(fid, key, lit);
    const ShowTree::Cue *c = cue();
    const ShowTree::Layer *l = layer(currentLayer());
    if (c == nullptr || l == nullptr)
        return nullptr;
    ShowTreeController *st = m_doc->showTree();
    // Its own exception first: the last child of the layer on it that sets it.
    const ShowTree::Look *found = nullptr;
    for (const ShowTree::Layer &ch : c->layers)
        if (ch.parent == l->id && ch.look(key) != nullptr && !ch.look(key)->none
                && st->targetFixtures(ch.target).contains(fid))
        {
            found = ch.look(key);
            if (lit != nullptr)
                *lit = st->targetFixtures(ch.target);
        }
    if (found != nullptr)
        return found;
    if (lit != nullptr)
        *lit = st->targetFixtures(l->target);
    if (const ShowTree::Look *own = l->look(key))
        return own->none ? nullptr : own;
    // A cue look reaches its own top layers.
    for (const ShowTree::Look &cl : c->looks)
        if (cl.key == key && !cl.none)
            return &cl;
    return nullptr;
}

float RefineSession::dmxToDegrees(Fixture *fx, int type, quint16 dmx)
{
    // Fixture::positionToValues() inverted, so a fixture's own mapping
    // (its range, an indexed band) is honoured.
    if (fx == nullptr || fx->fixtureMode() == nullptr)
        return 0.0f;
    const quint32 msb = fx->channelNumber(type, QLCChannel::MSB);
    const quint32 lsb = fx->channelNumber(type, QLCChannel::LSB);
    if (msb == QLCChannel::invalid())
        return 0.0f;
    QLCPhysical phy = fx->fixtureMode()->physical();
    double maxDeg = (type == QLCChannel::Pan) ? phy.focusPanMax() : phy.focusTiltMax();
    if (maxDeg <= 0.0)
        maxDeg = (type == QLCChannel::Pan) ? 360.0 : 270.0;
    auto dmxAt = [&](double deg) {
        quint16 v = 0;
        for (const SceneValue &sv : fx->positionToValues(type, float(deg)))
        {
            if (sv.channel == msb)
                v = quint16((v & 0x00FF) | (sv.value << 8));
            else if (sv.channel == lsb)
                v = quint16((v & 0xFF00) | sv.value);
        }
        if (lsb == QLCChannel::invalid())
            v = quint16(v & 0xFF00);
        return v;
    };
    // Its band: what 0 and the full range are sent as (linear between).
    const double lo16 = dmxAt(0.0), hi16 = dmxAt(maxDeg);
    if (hi16 <= lo16)
        return 0.0f;
    const double want = (lsb == QLCChannel::invalid()) ? double(dmx & 0xFF00) : double(dmx);
    const double deg = maxDeg * (qBound(lo16, want, hi16) - lo16) / (hi16 - lo16);
    return float(qRound(deg * 100.0) / 100.0);
}

bool RefineSession::cuePosition(quint32 fid, float &pan, float &tilt) const
{
    Fixture *fx = m_doc->fixture(fid);
    if (fx == nullptr || fx->fixtureMode() == nullptr)
        return false;
    const quint32 pMsb = fx->channelNumber(QLCChannel::Pan, QLCChannel::MSB);
    const quint32 pLsb = fx->channelNumber(QLCChannel::Pan, QLCChannel::LSB);
    const quint32 tMsb = fx->channelNumber(QLCChannel::Tilt, QLCChannel::MSB);
    const quint32 tLsb = fx->channelNumber(QLCChannel::Tilt, QLCChannel::LSB);
    if (pMsb == QLCChannel::invalid() && tMsb == QLCChannel::invalid())
        return false;

    QHash<quint32, uchar> ch;
    QList<quint32> lit;
    const ShowTree::Look *look = lookFor(fid, ShowTree::KeyPosition, &lit);
    if (look != nullptr)
    {
        /* Degrees the palettes hold, as they are, when they hold them for
           every fixture alike (no fanning): exact, where the DMX read back
           is only as fine as the fixture's channels. */
        bool exact = !look->palettes.isEmpty();
        float p = 0.0f, t = 0.0f;
        bool hasP = false, hasT = false;
        for (quint32 pid : look->palettes)
        {
            const QLCPalette *pal = m_doc->palette(pid);
            if (pal == nullptr || pal->fanningType() != QLCPalette::Flat || pal->values().isEmpty())
                exact = false;
            else if (pal->type() == QLCPalette::PanTilt && pal->values().size() >= 2)
            {
                p = pal->values().at(0).toFloat();
                t = pal->values().at(1).toFloat();
                hasP = hasT = true;
            }
            else if (pal->type() == QLCPalette::Pan)
            {
                p = pal->values().at(0).toFloat();
                hasP = true;
            }
            else if (pal->type() == QLCPalette::Tilt)
            {
                t = pal->values().at(0).toFloat();
                hasT = true;
            }
            else
                exact = false;
        }
        if (exact && hasP && hasT)
        {
            pan = p;
            tilt = t;
            return true;
        }
    }
    if (look != nullptr)
    {
        for (quint32 pid : look->palettes)
            if (QLCPalette *p = m_doc->palette(pid))
                for (const SceneValue &sv : p->valuesFromFixtures(m_doc, lit))
                    if (sv.fxi == fid)
                        ch.insert(sv.channel, sv.value);
    }
    if (ch.isEmpty() && m_sceneMode)
        ch = sceneRawValues(fid);
    if (ch.isEmpty())
    {
        // Nothing the cue says here (tracked in, say): where it is now.
        QList<Universe *> unis = m_doc->inputOutputMap()->claimUniverses();
        const int u = int(fx->universe());
        if (u >= 0 && u < unis.size() && unis.at(u) != nullptr)
            for (quint32 c : { pMsb, pLsb, tMsb, tLsb })
                if (c != QLCChannel::invalid() && int(fx->address() + c) < UNIVERSE_SIZE)
                    ch.insert(c, unis.at(u)->preGMValue(int(fx->address() + c)));
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
    auto read16 = [&](quint32 msb, quint32 lsb) {
        return quint16((ch.value(msb) << 8) | (lsb != QLCChannel::invalid() ? ch.value(lsb) : 0));
    };
    pan = pMsb != QLCChannel::invalid() ? dmxToDegrees(fx, QLCChannel::Pan, read16(pMsb, pLsb)) : 0.0f;
    tilt = tMsb != QLCChannel::invalid() ? dmxToDegrees(fx, QLCChannel::Tilt, read16(tMsb, tLsb)) : 0.0f;
    return true;
}

QString RefineSession::editKey() const
{
    return QStringLiteral("%1/%2/%3").arg(int(m_scope)).arg(currentLayer())
            .arg(m_scope == LayerScope ? QString() : scopeTarget().signature());
}

RefineSession::Edit &RefineSession::edit()
{
    const QString key = editKey();
    if (!m_edits.contains(key))
    {
        Edit e;
        e.scope = m_scope;
        e.layer = currentLayer();
        e.target = scopeTarget();
        e.fixtures = scopeFixtures();
        m_edits.insert(key, e);
        m_editOrder << key;
    }
    return m_edits[key];
}

bool RefineSession::nudgePosition(float dPan, float dTilt)
{
    if (!m_active || (dPan == 0.0f && dTilt == 0.0f))
        return false;
    const QList<quint32> fixtures = scopeFixtures();
    if (fixtures.isEmpty())
        return false;
    if (m_scope == LayerScope)
    {
        // An aimed layer follows its target: the whole layer cannot be moved
        // by degrees without losing that.
        const ShowTree::Layer *l = layer(currentLayer());
        const ShowTree::Look *look = m_sceneMode ? lookFor(fixtures.first(), ShowTree::KeyPosition)
                                   : l != nullptr ? l->look(ShowTree::KeyPosition) : nullptr;
        if (look != nullptr)
            for (quint32 pid : look->palettes)
                if (const QLCPalette *p = m_doc->palette(pid))
                    if (!isPositionType(p->type()))
                    {
                        if (!m_edits.contains(editKey()))
                            say(tr("This layer is aimed at a target. Refine a fixture, or move the target."));
                        return false;
                    }
        Edit &e = edit();
        e.hasPos = true;
        e.pan += dPan;
        e.tilt += dTilt;
    }
    else if (m_scope == GroupScope)
    {
        /* Board Q112c: a group keeps its fan. The nudge is an offset from
           each fixture's own position, not one position for all. */
        Edit &e = edit();
        e.hasPos = true;
        e.pan += dPan;
        e.tilt += dTilt;
    }
    else
    {
        Edit &e = edit();
        if (!e.hasPos)
        {
            if (!cuePosition(fixtures.first(), e.pan, e.tilt))
                return false;
            e.hasPos = true;
        }
        e.pan += dPan;
        e.tilt += dTilt;
    }
    rebuildWrites();
    emit changed();
    return true;
}

bool RefineSession::nudgeZoom(float dDeg)
{
    const QList<quint32> fixtures = scopeFixtures();
    if (!m_active || fixtures.isEmpty() || dDeg == 0.0f)
        return false;
    double lo = 0.0, hi = 0.0;
    bool ranged = false;
    for (quint32 fid : fixtures)
        if (RigHeads::hasZoomChannel(m_doc->fixture(fid)) && RigHeads::lensRange(m_doc->fixture(fid), lo, hi))
        {
            ranged = true;
            break;
        }
    if (!ranged)
    {
        say(tr("No zoom here."));
        return false;
    }
    Edit &e = edit();
    if (!e.hasZoom)
    {
        e.zoom = float(hi);
        if (const ShowTree::Look *look = lookFor(fixtures.first(), ShowTree::KeyZoom))
            if (const QLCPalette *p = m_doc->palette(look->palettes.value(0, kInvalid)))
                e.zoom = p->value().toFloat();
        e.hasZoom = true;
    }
    e.zoom = float(qBound(lo, double(qRound((e.zoom + dDeg) * 10.0f)) / 10.0, hi));
    rebuildWrites();
    emit changed();
    return true;
}

bool RefineSession::nudgeFocus(int dValue)
{
    const QList<quint32> fixtures = scopeFixtures();
    if (!m_active || fixtures.isEmpty() || dValue == 0)
        return false;
    bool rev = false;
    bool any = false;
    for (quint32 fid : fixtures)
        any = any || focusChannel(m_doc->fixture(fid), &rev) >= 0;
    if (!any)
    {
        say(tr("No focus here."));
        return false;
    }
    Edit &e = edit();
    if (!e.hasFocus)
    {
        e.focus = 128;
        if (const ShowTree::Look *look = lookFor(fixtures.first(), ShowTree::KeyBeam))
            if (const QLCPalette *p = m_doc->palette(look->palettes.value(0, kInvalid)))
                if (!p->values().isEmpty())
                    e.focus = p->values().first().toInt();
        e.hasFocus = true;
    }
    e.focus = qBound(0, e.focus + dValue, 255);
    rebuildWrites();
    emit changed();
    return true;
}

bool RefineSession::stepPalette(int dir)
{
    const QList<quint32> fixtures = scopeFixtures();
    if (!m_active || fixtures.isEmpty() || m_page == BeamPage)
        return false;
    const QLCPalette::PaletteType type = m_page == ColourPage ? QLCPalette::Color : QLCPalette::Gobo;
    const QString key = m_page == ColourPage ? ShowTree::KeyColour : ShowTree::KeyGobo;
    QList<QLCPalette *> list;
    for (QLCPalette *p : m_doc->palettes())
        if (p->type() == type && !m_doc->showTree()->isInlinePalette(p->id()))
            list << p;
    if (list.isEmpty())
    {
        say(m_page == ColourPage ? tr("No colour palettes.") : tr("No gobo palettes."));
        return false;
    }
    std::sort(list.begin(), list.end(), [](const QLCPalette *a, const QLCPalette *b) {
        return a->name().localeAwareCompare(b->name()) < 0;
    });
    Edit &e = edit();
    quint32 &slot = (m_page == ColourPage) ? e.colour : e.gobo;
    quint32 now = slot;
    if (now == kInvalid)
        if (const ShowTree::Look *look = lookFor(fixtures.first(), key))
            now = look->palettes.value(0, kInvalid);
    int at = -1;
    for (int i = 0; i < list.size(); ++i)
        if (list.at(i)->id() == now)
            at = i;
    const int n = list.size();
    at = (at < 0) ? (dir > 0 ? 0 : n - 1) : (((at + dir) % n) + n) % n;
    slot = list.at(at)->id();
    rebuildWrites();
    emit changed();
    say(list.at(at)->name());
    return true;
}

bool RefineSession::pendingPosition(quint32 fid, float &pan, float &tilt) const
{
    bool found = false;
    for (const QString &k : m_editOrder)
    {
        const Edit &e = m_edits[k];
        if (!e.hasPos || !e.fixtures.contains(fid))
            continue;
        if (e.scope != FixtureScope)
        {
            float p = 0.0f, t = 0.0f;
            if (!cuePosition(fid, p, t))
                continue;
            pan = p + e.pan;
            tilt = t + e.tilt;
        }
        else
        {
            pan = e.pan;
            tilt = e.tilt;
        }
        found = true;
    }
    return found;
}

bool RefineSession::pendingZoom(quint32 fid, float &deg) const
{
    bool found = false;
    for (const QString &k : m_editOrder)
        if (m_edits[k].hasZoom && m_edits[k].fixtures.contains(fid))
        {
            deg = m_edits[k].zoom;
            found = true;
        }
    return found;
}

bool RefineSession::pendingFocus(quint32 fid, int &value) const
{
    bool found = false;
    for (const QString &k : m_editOrder)
        if (m_edits[k].hasFocus && m_edits[k].fixtures.contains(fid))
        {
            value = m_edits[k].focus;
            found = true;
        }
    return found;
}

bool RefineSession::keep(QString *why)
{
    if (!m_active || m_edits.isEmpty())
        return false;
    if (m_sceneMode)
        return sceneKeep(why);
    ShowTreeController *st = m_doc->showTree();
    // Layer edits first, then groups, then fixtures: the most specific last.
    QList<QString> order = m_editOrder;
    std::stable_sort(order.begin(), order.end(), [this](const QString &a, const QString &b) {
        return int(m_edits[a].scope) < int(m_edits[b].scope);
    });

    /* A group's positions, each fixture's own plus the nudge: read before
       anything in the cue changes. */
    QHash<QString, QList<QPair<quint32, QPair<float, float>>>> groupPositions;
    for (const QString &k : order)
    {
        const Edit &e = m_edits[k];
        if (e.scope != GroupScope || !e.hasPos)
            continue;
        for (quint32 fid : e.fixtures)
        {
            float p = 0.0f, t = 0.0f;
            if (cuePosition(fid, p, t))
                groupPositions[k] << qMakePair(fid, qMakePair(p + e.pan, t + e.tilt));
        }
    }
    // In Operate, what is kept lands on the next GO: hold it on the rig.
    if (m_doc->mode() == Doc::Operate)
        for (const QString &k : order)
            m_kept << previewValues(m_edits[k]);

    /* A look onto its layer -- or, for a layer only tracked in, into this
       cue (it becomes its own here) or the cue its values come from. */
    bool toSource = false;
    auto setLook = [&](quint32 layerId, const ShowTree::Look &look) {
        const auto t = m_tracked.constFind(layerId);
        if (t == m_tracked.constEnd())
            return st->setLayerLook(m_cue, layerId, look);
        if (m_keepToSource && st->updateSource(m_cue, t->layer.target, look))
        {
            toSource = true;
            return true;
        }
        return st->setTargetLook(m_cue, t->layer.target, look);
    };
    const bool wasTracked = isTrackedLayer();
    const QString light = keptLight();
    const ShowTree::Target trackedTarget = wasTracked ? layer(currentLayer())->target : ShowTree::Target();

    st->beginBatch(tr("Refine"));
    bool ok = true;
    for (const QString &k : order)
    {
        const Edit e = m_edits.value(k);
        quint32 onLayer = e.layer;
        // A group's positions: an exception on each fixture, its fan kept.
        for (const auto &fp : groupPositions.value(k))
        {
            ShowTree::Target one;
            one.fixtures << fp.first;
            const ShowTree::Layer *child = childOn(e.layer, one);
            const quint32 cid = child != nullptr ? child->id : st->addChildLayer(m_cue, e.layer, one, why);
            if (cid == kInvalid)
            {
                ok = false;
                break;
            }
            ShowTree::Look look;
            look.key = ShowTree::KeyPosition;
            look.palettes << st->createInlinePalette(QLCPalette::PanTilt,
                                                     { qRound(fp.second.first), qRound(fp.second.second) },
                                                     tr("Refined position"));
            ok = ok && st->setLayerLook(m_cue, cid, look);
        }
        if (!ok)
            break;
        const bool rest = e.hasZoom || e.hasFocus || e.colour != kInvalid || e.gobo != kInvalid;
        if (e.scope == GroupScope && !rest)
            continue;
        if (e.scope != LayerScope)
        {
            const ShowTree::Layer *child = childOn(e.layer, e.target);
            onLayer = child != nullptr ? child->id : st->addChildLayer(m_cue, e.layer, e.target, why);
            if (onLayer == kInvalid)
            {
                ok = false;
                break;
            }
        }
        if (e.hasPos && e.scope != GroupScope)
        {
            ShowTree::Look look;
            look.key = ShowTree::KeyPosition;
            const ShowTree::Layer *l = layer(onLayer);
            const ShowTree::Look *base = (e.scope == LayerScope && l != nullptr)
                                         ? l->look(ShowTree::KeyPosition) : nullptr;
            if (base != nullptr && !base->palettes.isEmpty())
            {
                // The layer's own value moved by the nudge, fanning kept.
                for (quint32 pid : base->palettes)
                {
                    const QLCPalette *p = m_doc->palette(pid);
                    if (p == nullptr || !isPositionType(p->type()))
                        continue;
                    QVariantList v = p->values();
                    if (p->type() == QLCPalette::PanTilt && v.size() >= 2)
                    {
                        v[0] = qRound(v.at(0).toFloat() + e.pan);
                        v[1] = qRound(v.at(1).toFloat() + e.tilt);
                    }
                    else if (!v.isEmpty())
                        v[0] = qRound(v.at(0).toFloat() + (p->type() == QLCPalette::Pan ? e.pan : e.tilt));
                    const quint32 np = st->createInlinePalette(p->type(), v, tr("Refined position"));
                    if (QLCPalette *n = m_doc->palette(np))
                    {
                        n->setFanningType(p->fanningType());
                        n->setFanningLayout(p->fanningLayout());
                        n->setFanningAmount(p->fanningAmount());
                        n->setFanningValue(p->fanningValue());
                    }
                    look.palettes << np;
                }
            }
            else
            {
                float pan = e.pan, tilt = e.tilt;
                if (e.scope == LayerScope)
                {
                    float p0 = 0.0f, t0 = 0.0f;
                    cuePosition(e.fixtures.value(0, kInvalid), p0, t0);
                    pan += p0;
                    tilt += t0;
                }
                look.palettes << st->createInlinePalette(QLCPalette::PanTilt,
                                                         { qRound(pan), qRound(tilt) },
                                                         tr("Refined position"));
            }
            ok = ok && setLook(onLayer, look);
        }
        if (e.hasZoom)
        {
            ShowTree::Look look;
            look.key = ShowTree::KeyZoom;
            look.palettes << st->createInlinePalette(QLCPalette::Zoom, { double(e.zoom) }, tr("Refined zoom"));
            ok = ok && setLook(onLayer, look);
        }
        if (e.hasFocus)
        {
            // Focus moved; frost and iris as they were.
            QVariantList v { e.focus, 0, 0 };
            if (const ShowTree::Look *was = lookFor(e.fixtures.value(0, kInvalid), ShowTree::KeyBeam))
                if (const QLCPalette *p = m_doc->palette(was->palettes.value(0, kInvalid)))
                    for (int i = 1; i < qMin(3, p->values().size()); ++i)
                        v[i] = p->values().at(i);
            ShowTree::Look look;
            look.key = ShowTree::KeyBeam;
            look.palettes << st->createInlinePalette(QLCPalette::Beam, v, tr("Refined focus"));
            ok = ok && setLook(onLayer, look);
        }
        if (e.colour != kInvalid)
        {
            ShowTree::Look look;
            look.key = ShowTree::KeyColour;
            look.palettes << e.colour;
            ok = ok && setLook(onLayer, look);
        }
        if (e.gobo != kInvalid)
        {
            ShowTree::Look look;
            look.key = ShowTree::KeyGobo;
            look.palettes << e.gobo;
            ok = ok && setLook(onLayer, look);
        }
        if (!ok)
            break;
    }
    st->endBatch(ok);
    if (!ok)
    {
        m_kept.clear();
        if (why != nullptr && why->isEmpty())
            *why = tr("The Show Tree did not take the change.");
        say(tr("Not kept."));
        return false;
    }
    m_edits.clear();
    m_editOrder.clear();
    m_levelCache.clear();
    if (wasTracked && !toSource)
    {
        // Its own here now: step on from that layer.
        m_layers = findLayers(nullptr);
        for (int i = 0; i < m_layers.size(); ++i)
            if (const ShowTree::Layer *l = layer(m_layers.at(i)))
                if (!m_tracked.contains(l->id) && l->target == trackedTarget)
                    m_layerIndex = i;
    }
    else if (wasTracked)
        m_layers = findLayers(nullptr);
    rebuildWrites();
    emit changed();
    emit kept();
    if (wasTracked)
        say(toSource ? tr("Kept in the cue it tracks from.") : tr("Kept in this cue."));
    else
        say(m_doc->mode() == Doc::Operate ? tr("%1 kept. It plays from the next GO.").arg(light)
                                          : tr("%1 kept.").arg(light));
    return true;
}

bool RefineSession::nextPending()
{
    if (!m_active)
        return false;
    if (m_editOrder.isEmpty())
    {
        say(tr("All kept."));
        return false;
    }
    // After where we are, round to the start.
    const QString here = editKey();
    int at = m_editOrder.indexOf(here);
    const Edit e = m_edits.value(m_editOrder.at((at + 1) % m_editOrder.size()));
    m_layerIndex = qMax(0, m_layers.indexOf(e.layer));
    m_scope = e.scope;
    if (e.scope == FixtureScope)
        m_fixtureIndex = qMax(0, layerFixtures().indexOf(e.fixtures.value(0)));
    else if (e.scope == GroupScope)
        m_groupIndex = qMax(0, groups().indexOf(e.target));
    rebuildWrites();
    emit changed();
    flashSelection();
    say(tr("%1, not kept.").arg(keptLight()));
    return true;
}

bool RefineSession::keepAtSource(QString *why)
{
    if (!isTrackedLayer())
        return keep(why);
    const bool was = m_keepToSource;
    m_keepToSource = true;
    const bool ok = keep(why);
    m_keepToSource = was;
    emit changed();
    return ok;
}

bool RefineSession::confirmShared(QString *why)
{
    if (m_sharedWarning.isEmpty())
        return false;
    m_sharedConfirmed = true;
    return keep(why);
}

QString RefineSession::keptLight() const
{
    if (m_scope == FixtureScope)
        if (const Fixture *fx = m_doc->fixture(currentFixture()))
            return fx->name();
    if (m_scope == GroupScope)
        return targetName(scopeTarget());
    return layerName(currentLayer());
}

void RefineSession::revert()
{
    m_sharedConfirmed = false;
    m_sharedWarning.clear();
    m_sharedUsers.clear();
    if (m_edits.isEmpty())
        return;
    m_edits.clear();
    m_editOrder.clear();
    rebuildWrites();
    emit changed();
    say(tr("Back to the cue."));
}

bool RefineSession::clearException()
{
    if (!m_active)
        return false;
    if (m_scope == LayerScope)
    {
        revert();
        return true;
    }
    if (m_sceneMode)
        return sceneClearException();
    // Pending at this scope goes first.
    const QString key = editKey();
    if (m_edits.remove(key) > 0)
        m_editOrder.removeAll(key);
    // A group's positions are kept on each of its fixtures (Q112c).
    QList<quint32> fixtureChildren;
    if (m_scope == GroupScope && m_page == BeamPage)
        for (quint32 fid : scopeFixtures())
        {
            ShowTree::Target one;
            one.fixtures << fid;
            if (const ShowTree::Layer *fc = childOn(currentLayer(), one))
                if (fc->look(ShowTree::KeyPosition) != nullptr)
                    fixtureChildren << fc->id;
        }
    const ShowTree::Layer *child = childOn(currentLayer(), scopeTarget());
    if (child == nullptr && fixtureChildren.isEmpty())
    {
        rebuildWrites();
        emit changed();
        say(tr("No exception here."));
        return false;
    }
    if (child == nullptr)
    {
        ShowTreeController *st = m_doc->showTree();
        st->beginBatch(tr("Refine: clear exception"));
        for (quint32 id : fixtureChildren)
        {
            ShowTree::Look empty;
            empty.key = ShowTree::KeyPosition;
            st->setLayerLook(m_cue, id, empty);
            const ShowTree::Layer *fc = layer(id);
            if (fc != nullptr && fc->looks.isEmpty())
                st->removeLayer(m_cue, id);
        }
        st->endBatch(true);
        rebuildWrites();
        emit changed();
        say(tr("Exception cleared."));
        return true;
    }
    const quint32 childId = child->id;
    QStringList keys;
    if (m_page == BeamPage)
        keys << ShowTree::KeyPosition << ShowTree::KeyZoom << ShowTree::KeyBeam;
    else
        keys << (m_page == ColourPage ? ShowTree::KeyColour : ShowTree::KeyGobo);
    ShowTreeController *st = m_doc->showTree();
    st->beginBatch(tr("Refine: clear exception"));
    for (const QString &k : keys)
        if (child != nullptr && child->look(k) != nullptr)
        {
            ShowTree::Look empty;
            empty.key = k;
            st->setLayerLook(m_cue, childId, empty);
            child = layer(childId);
        }
    child = layer(childId);
    if (child != nullptr && child->looks.isEmpty())
        st->removeLayer(m_cue, childId);
    for (quint32 id : fixtureChildren)
    {
        ShowTree::Look empty;
        empty.key = ShowTree::KeyPosition;
        st->setLayerLook(m_cue, id, empty);
        const ShowTree::Layer *fc = layer(id);
        if (fc != nullptr && fc->looks.isEmpty())
            st->removeLayer(m_cue, id);
    }
    st->endBatch(true);
    rebuildWrites();
    emit changed();
    say(tr("Exception cleared."));
    return true;
}

bool RefineSession::undo()
{
    const bool done = m_sceneMode ? (m_doc->paletteOps() != nullptr && m_doc->paletteOps()->undo())
                                  : m_doc->showTree()->undo();
    rebuildWrites();
    emit changed();
    say(done ? tr("Undone.") : tr("Nothing to undo."));
    return done;
}

/*****************************************************************************
 * Telling the operator
 *****************************************************************************/

QString RefineSession::targetName(const ShowTree::Target &t) const
{
    return m_doc->showTree()->targetLabel(t);
}

QString RefineSession::layerName(quint32 layerId) const
{
    if (m_sceneMode)
    {
        const Function *f = m_doc->function(layerId);
        return f != nullptr ? f->name() : QString();
    }
    const ShowTree::Layer *l = layer(layerId);
    if (l == nullptr)
        return QString();
    return l->name.isEmpty() ? targetName(l->target) : l->name;
}

QString RefineSession::wherePhrase() const
{
    if (!m_active)
        return QString();
    // "AT spots, layer." / "DS wash, group." / "AT spots, fixture 3 of 8."
    const QString name = layerName(currentLayer());
    switch (m_scope)
    {
        case LayerScope:
            if (isTrackedLayer())
            {
                const ShowTree::Cue *src = m_doc->showTree()->cue(trackedSource());
                const QString from = src != nullptr ? (src->number.isEmpty() ? src->name : src->number) : QString();
                // The default said: a keep goes into this cue.
                return m_keepToSource ? tr("%1, tracked from %2. Keeps go into %2.").arg(name, from)
                                      : tr("%1, tracked from %2. Keeps go into this cue.").arg(name, from);
            }
            return tr("%1, layer.").arg(name);
        case GroupScope:
            return tr("%1, group %2 of %3.").arg(targetName(scopeTarget()))
                    .arg(m_groupIndex + 1).arg(groups().size());
        case FixtureScope:
            return tr("%1, fixture %2 of %3.").arg(name).arg(m_fixtureIndex + 1)
                    .arg(layerFixtures().size());
    }
    return QString();
}

QString RefineSession::lightPhrase() const
{
    if (!m_active)
        return QString();
    QString where;
    if (m_sceneMode)
        where = tr("Scene %1, %2").arg(layerName(currentLayer()), wherePhrase());
    else
    {
        const ShowTree::Cue *c = cue();
        where = tr("Cue %1, %2").arg(c != nullptr ? (c->number.isEmpty() ? c->name : c->number) : QString(),
                                     wherePhrase());
    }
    // The light first and briefly: the fixture, else what is picked.
    QString light;
    if (m_scope == FixtureScope)
    {
        if (const Fixture *fx = m_doc->fixture(currentFixture()))
            light = fx->name();
    }
    else if (m_scope == GroupScope)
        light = targetName(scopeTarget());
    else
        light = layerName(currentLayer());
    return light.isEmpty() ? where : tr("%1. %2").arg(light, where);
}

void RefineSession::whereAmI()
{
    if (!m_active)
        return;
    flashSelection();
    emit sayLight(lightPhrase());
}

void RefineSession::setLightsFeedback(bool on)
{
    if (on == m_lightsFeedback)
        return;
    m_lightsFeedback = on;
    if (!on)
        m_flashClock.invalidate();
    rebuildWrites();
}

void RefineSession::flashSelection()
{
    if (!m_active || !m_flashEnabled || !m_lightsFeedback)
        return;
    m_flashClock.start();
    m_flashTimer->start();
    rebuildWrites();
}

bool RefineSession::isFlashing() const
{
    return m_active && m_flashClock.isValid()
        && m_flashClock.elapsed() < FocusSession::flashDurationMs();
}

void RefineSession::onFlashTick()
{
    if (!isFlashing())
    {
        m_flashTimer->stop();
        m_flashClock.invalidate();
    }
    rebuildWrites();
}

/*****************************************************************************
 * Controller
 *****************************************************************************/

void RefineSession::apply(ControllerScheme::Verb verb)
{
    if (!m_active)
        return;
    switch (verb)
    {
        case ControllerScheme::PreviousGroup: previousLayer(); break;
        case ControllerScheme::NextGroup:     nextLayer(); break;
        case ControllerScheme::PreviousItem:  previousItem(); break;
        case ControllerScheme::NextItem:      nextItem(); break;
        case ControllerScheme::CycleScope:    cycleScope(); break;
        case ControllerScheme::Keep:          keep(); break;
        case ControllerScheme::Revert:        revert(); break;
        case ControllerScheme::Reset:         clearException(); break;
        case ControllerScheme::Undo:          undo(); break;
        case ControllerScheme::Exit:
            if (hasPending())
                say(m_editOrder.size() == 1 ? tr("1 change not kept.")
                                            : tr("%1 changes not kept.").arg(m_editOrder.size()));
            emit exitRequested();
            break;
        case ControllerScheme::ZoomWider:
            if (m_page == BeamPage)
                nudgeZoom(1.0f);
            break;
        case ControllerScheme::ZoomNarrower:
            if (m_page == BeamPage)
                nudgeZoom(-1.0f);
            break;
        case ControllerScheme::FocusNearer:
            if (m_page == BeamPage)
                nudgeFocus(-5);
            else
                stepPalette(-1);
            break;
        case ControllerScheme::FocusFarther:
            if (m_page == BeamPage)
                nudgeFocus(5);
            else
                stepPalette(1);
            break;
        case ControllerScheme::Alternate:     cyclePage(); break;
        case ControllerScheme::WhereAmI:      whereAmI(); break;
        case ControllerScheme::NoVerb:        break;
    }
}

bool RefineSession::repeats(ControllerScheme::Verb verb) const
{
    // On the colour and gobo pages left and right step palettes: once each.
    return ControllerScheme::repeats(verb) && m_page == BeamPage;
}

void RefineSession::press(ControllerScheme::Verb verb, bool down)
{
    if (!down)
    {
        m_hold.release(int(verb));
        return;
    }
    m_hold.press(int(verb), repeats(verb));
    apply(verb);
}

void RefineSession::setStick(Stick stick, float xNorm, float yNorm)
{
    m_stick[stick][0] = qBound(0.0f, xNorm, 1.0f);
    m_stick[stick][1] = qBound(0.0f, yNorm, 1.0f);
}

void RefineSession::setStickResponse(float deadzone, float sensitivity)
{
    m_deadzone = deadzone;
    m_sensitivity = sensitivity > 0.0f ? sensitivity : 1.0f;
}

void RefineSession::setTrigger(int side, float norm)
{
    if (side < 0 || side > 1)
        return;
    if (m_trigger[side].update(qBound(0.0f, norm, 1.0f)))
        apply(side == 0 ? ControllerScheme::PreviousGroup : ControllerScheme::NextGroup);
}

void RefineSession::tick(double dt)
{
    if (!m_active)
        return;
    // A held D-pad arm repeats (press()); a page change stops it.
    if (m_hold.held() >= 0 && !repeats(ControllerScheme::Verb(m_hold.held())))
        m_hold.reset();
    for (int n = m_hold.due(dt); n > 0 && m_hold.held() >= 0; --n)
        apply(ControllerScheme::Verb(m_hold.held()));
    float dPan = 0.0f, dTilt = 0.0f;
    for (int s = 0; s < 2; ++s)
    {
        const float rate = (s == Coarse ? coarseRate() : fineRate()) * m_sensitivity * float(dt);
        dPan += FocusSession::stickVelocity(m_stick[s][0], m_deadzone) * rate;
        dTilt -= FocusSession::stickVelocity(m_stick[s][1], m_deadzone) * rate;
    }
    if (dPan != 0.0f || dTilt != 0.0f)
        nudgePosition(dPan, dTilt);
}

/*****************************************************************************
 * DMX
 *****************************************************************************/

QList<SceneValue> RefineSession::previewValues(const Edit &e) const
{
    QList<SceneValue> out;
    const ShowTree::Cue *c = cue();
    for (quint32 fid : e.fixtures)
    {
        Fixture *fx = m_doc->fixture(fid);
        if (fx == nullptr || fx->fixtureMode() == nullptr)
            continue;
        if (e.hasPos)
        {
            bool own = false;
            // A layer's nudge leaves a fixture with its own position alone.
            if (e.scope == LayerScope && m_sceneMode)
                own = sceneHasOwn(fid, ShowTree::KeyPosition);
            else if (e.scope == LayerScope && c != nullptr)
                for (const ShowTree::Layer &ch : c->layers)
                    if (ch.parent == e.layer && ch.look(ShowTree::KeyPosition) != nullptr
                            && m_doc->showTree()->targetFixtures(ch.target).contains(fid))
                        own = true;
            float pan = e.pan, tilt = e.tilt;
            bool have = !own;
            if (have && e.scope != FixtureScope)
            {
                float p0 = 0.0f, t0 = 0.0f;
                have = cuePosition(fid, p0, t0);
                pan += p0;
                tilt += t0;
            }
            if (have)
            {
                // Whole degrees, as the palette will keep them.
                out << fx->positionToValues(QLCChannel::Pan, float(qRound(pan)));
                out << fx->positionToValues(QLCChannel::Tilt, float(qRound(tilt)));
            }
        }
        if (e.hasZoom && RigHeads::hasZoomChannel(fx))
            out << fx->zoomToValues(e.zoom, false);
        if (e.hasFocus)
        {
            bool rev = false;
            const int fc = focusChannel(fx, &rev);
            if (fc >= 0)
                out << SceneValue(fid, quint32(fc), uchar(rev ? 255 - e.focus : e.focus));
        }
        for (quint32 pid : { e.colour, e.gobo })
            if (pid != kInvalid)
                if (QLCPalette *p = m_doc->palette(pid))
                    out << p->valuesFromFixtures(m_doc, { fid });
    }
    return out;
}

int RefineSession::cueLevel(quint32 fid, quint32 channel)
{
    const QPair<quint32, quint32> key(fid, channel);
    const auto cached = m_levelCache.constFind(key);
    if (cached != m_levelCache.constEnd())
        return cached.value();
    int level = -1;
    // What the cue's Dimmer look gives it...
    QList<quint32> lit;
    if (const ShowTree::Look *look = lookFor(fid, ShowTree::KeyDimmer, &lit))
        for (quint32 pid : look->palettes)
            if (QLCPalette *p = m_doc->palette(pid))
                for (const SceneValue &sv : p->valuesFromFixtures(m_doc, lit))
                    if (sv.fxi == fid && sv.channel == channel)
                        level = sv.value;
    // ...else what it is at now (before anything here dims it: cached).
    if (level < 0)
    {
        level = 0;
        Fixture *fx = m_doc->fixture(fid);
        QList<Universe *> unis = m_doc->inputOutputMap()->claimUniverses();
        const int u = fx != nullptr ? int(fx->universe()) : -1;
        if (u >= 0 && u < unis.size() && unis.at(u) != nullptr
                && int(fx->address() + channel) < UNIVERSE_SIZE)
            level = unis.at(u)->preGMValue(int(fx->address() + channel));
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
    m_levelCache.insert(key, level);
    return level;
}

void RefineSession::rebuildWrites()
{
    QHash<QPair<int, int>, uchar> writes;
    if (m_active)
    {
        auto put = [&](Fixture *fx, quint32 ch, uchar value) {
            const int addr = int(fx->address()) + int(ch);
            if (addr < UNIVERSE_SIZE)
                writes.insert(qMakePair(int(fx->universe()), addr), value);
        };
        // What was kept in Operate, until the next GO plays it.
        for (const SceneValue &sv : m_kept)
            if (Fixture *fx = m_doc->fixture(sv.fxi))
                put(fx, sv.channel, sv.value);
        // The pending values, the most specific last.
        QList<QString> order = m_editOrder;
        std::stable_sort(order.begin(), order.end(), [this](const QString &a, const QString &b) {
            return int(m_edits[a].scope) < int(m_edits[b].scope);
        });
        for (const QString &k : order)
            for (const SceneValue &sv : previewValues(m_edits[k]))
                if (Fixture *fx = m_doc->fixture(sv.fxi))
                    put(fx, sv.channel, sv.value);

        /* Where the operator is: the rest of the layer dimmed while a group
           or fixture is picked, and what is picked blinking on a change. */
        const QList<quint32> selected = scopeFixtures();
        const bool dark = m_flashClock.isValid()
                          && !FocusSession::flashLit(m_flashClock.elapsed());
        for (quint32 fid : layerFixtures())
        {
            Fixture *fx = m_doc->fixture(fid);
            if (fx == nullptr || fx->fixtureMode() == nullptr || !fx->isPatched())
                continue;
            const bool picked = selected.contains(fid);
            if (picked && !dark)
                continue;   // as the cue has it
            if (!picked && (m_scope == LayerScope || !m_lightsFeedback))
                continue;
            const uchar dim = uchar(qRound(dimLevel() * 255.0f));
            for (quint32 c = 0; c < fx->channels(); ++c)
                if (const QLCChannel *ch = fx->fixtureMode()->channel(c))
                    if (ch->group() == QLCChannel::Intensity && ch->colour() == QLCChannel::NoColour)
                    {
                        if (picked)
                            put(fx, c, 0);
                        // Board Q112b: down to the dim level, never up to it.
                        else if (cueLevel(fid, c) > dim)
                            put(fx, c, dim);
                    }
        }
    }
    QList<OutputLayer::Level> levels;
    levels.reserve(writes.size());
    for (auto it = writes.constBegin(); it != writes.constEnd(); ++it)
        levels.append({ Fixture::invalidId(),
                        quint32(it.key().first) * UNIVERSE_SIZE + quint32(it.key().second),
                        it.value() });
    {
        QMutexLocker locker(&m_writesMutex);
        m_writes = writes;
    }
    m_layer.set(levels);
}

QHash<QPair<int, int>, uchar> RefineSession::pendingWrites() const
{
    QMutexLocker locker(&m_writesMutex);
    return m_writes;
}
