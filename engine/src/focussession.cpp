/*
  Q Light Controller Plus
  focussession.cpp

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

#include <QtMath>
#include <algorithm>

#include "focussession.h"
#include "monitorproperties.h"
#include "qlcfixturemode.h"
#include "qlccapability.h"
#include "stagetarget.h"
#include "mastertimer.h"
#include "linespread.h"
#include "qlcchannel.h"
#include "aimsolver.h"
#include "righeads.h"
#include "qlcphysical.h"
#include "universe.h"
#include "fadechannel.h"
#include "fixture.h"
#include "scene.h"
#include "truss.h"
#include "doc.h"

namespace
{
// The offset fields' own range (the fixture dialog's spin boxes).
const float kPanOffsetLimit = 720.0f;
const float kTiltOffsetLimit = 360.0f;
const float kRowDepth = 0.75f;   // metres: heads this close front-to-back share a row

float roundOffset(float v)
{
    // Offsets are saved to two places; hold them there so "saved" and "live"
    // compare as the file will.
    return float(qRound(double(v) * 100.0) / 100.0);
}
}

FocusSession::FocusSession(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_layer(doc, Universe::Focus, FadeChannel::Override, QStringLiteral("Focus"))
{
    /* Into Perform, the rig belongs to the show: Focus mode ends, and a head
       still being nudged goes back to what was saved. */
    if (m_doc != nullptr)
        connect(m_doc, &Doc::modeChanged, this, [this](Doc::Mode mode) {
            if (mode == Doc::Operate)
                stop();
        });
}

FocusSession::~FocusSession()
{
}

/*****************************************************************************
 * Starting and stopping
 *****************************************************************************/

QList<quint32> FocusSession::focusable(Doc *doc, const QList<quint32> &fixtures)
{
    const QList<quint32> movers = movingHeads(doc, fixtures);
    const QList<quint32> fixed = fixedLights(doc, fixtures);
    QList<quint32> out;
    for (quint32 fid : fixtures)
        if ((movers.contains(fid) || fixed.contains(fid)) && !out.contains(fid))
            out << fid;
    return out;
}

QList<quint32> FocusSession::movingHeads(Doc *doc, const QList<quint32> &fixtures)
{
    QList<quint32> out;
    if (doc == nullptr)
        return out;
    for (quint32 fid : AimSolver::aimable(doc, fixtures))
    {
        AimSolver::Frame frame;
        if (AimSolver::frameFor(doc, fid, frame))
            out << fid;
    }
    return out;
}

QList<quint32> FocusSession::fixedLights(Doc *doc, const QList<quint32> &fixtures)
{
    QList<quint32> out;
    MonitorProperties *props = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (props == nullptr)
        return out;
    for (quint32 fid : fixtures)
    {
        Fixture *fx = doc->fixture(fid);
        if (fx == nullptr || fx->fixtureMode() == nullptr || out.contains(fid)
                || !props->containsFixture(fid) || fx->heads() > 1)
            continue;
        if (fx->channelNumber(QLCChannel::Pan, QLCChannel::MSB) != QLCChannel::invalid()
                || fx->channelNumber(QLCChannel::Tilt, QLCChannel::MSB) != QLCChannel::invalid())
            continue;
        // A light: something that makes it brighter.
        bool light = false;
        for (quint32 c = 0; c < fx->channels() && !light; ++c)
            if (const QLCChannel *ch = fx->fixtureMode()->channel(c))
                light = ch->group() == QLCChannel::Intensity;
        if (light)
            out << fid;
    }
    return out;
}

QVector3D FocusSession::fixedLanding(quint32 fid) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    const QVector3D pos = props->fixtureRigPosition(fid);
    QVector3D dir;
    if (!rp.fixedAimDirection(dir))
        dir = (rp.mountingType == Truss::TopHung) ? QVector3D(0, 0, -1) : QVector3D(0, 0, 1);
    // Where the beam crosses head height -- or, if it never does, in front.
    const float headZ = props->platformHeightAt(pos.x(), pos.y()) + subjectHeight();
    QVector3D at = pos + QVector3D(0.0f, 2.0f, 0.0f);
    if (qAbs(dir.z()) > 1e-3f)
    {
        const float t = (headZ - pos.z()) / dir.z();
        if (t > 0.0f)
            at = pos + dir * t;
    }
    at.setZ(props->platformHeightAt(at.x(), at.y()) + subjectHeight());
    return at;
}

void FocusSession::applyLanding(quint32 fid, const Head &h, bool original)
{
    MonitorProperties *props = m_doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(fid);
    if (original)
    {
        rp.fixedAim = h.origAim;
        rp.panZeroDir = h.origPan;
        rp.aimDown = h.origDown;
    }
    else
    {
        const QVector3D d = h.first - props->fixtureRigPosition(fid);
        const double across = qSqrt(double(d.x()) * d.x() + double(d.y()) * d.y());
        double bearing = qRadiansToDegrees(qAtan2(-double(d.x()), double(d.y())));
        if (bearing < 0.0)
            bearing += 360.0;
        rp.fixedAim = true;
        rp.panZeroDir = float(qRound(bearing * 10.0) / 10.0);
        rp.aimDown = float(qRound(qRadiansToDegrees(qAtan2(-double(d.z()), across)) * 10.0) / 10.0);
    }
    props->setFixtureRigProps(fid, rp);
}

QList<quint32> FocusSession::stageOrder(Doc *doc, const QList<quint32> &fixtures)
{
    MonitorProperties *props = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (props == nullptr)
        return fixtures;

    QList<QPair<quint32, QVector3D>> placed;
    for (quint32 fid : fixtures)
        placed << qMakePair(fid, props->fixtureRigPosition(fid));

    // Front (downstage, +Y) first...
    std::stable_sort(placed.begin(), placed.end(),
                     [](const QPair<quint32, QVector3D> &a, const QPair<quint32, QVector3D> &b) {
        return a.second.y() > b.second.y();
    });

    // ...then each row house left to right: stage right (-X) first.
    QList<quint32> out;
    int i = 0;
    while (i < placed.size())
    {
        const float rowY = placed.at(i).second.y();
        int j = i;
        while (j < placed.size() && rowY - placed.at(j).second.y() <= kRowDepth)
            ++j;
        QList<QPair<quint32, QVector3D>> row = placed.mid(i, j - i);
        std::stable_sort(row.begin(), row.end(),
                         [](const QPair<quint32, QVector3D> &a, const QPair<quint32, QVector3D> &b) {
            return a.second.x() < b.second.x();
        });
        for (const QPair<quint32, QVector3D> &p : row)
            out << p.first;
        i = j;
    }
    return out;
}

bool FocusSession::start(const Options &options, QString *why)
{
    auto refuse = [why](const QString &reason) {
        if (why != nullptr)
            *why = reason;
        return false;
    };
    if (m_doc == nullptr || m_doc->monitorProperties() == nullptr)
        return refuse(tr("There is no rig to focus."));
    /* A Design-mode tool: in Perform the rig belongs to the show. */
    if (m_doc->mode() == Doc::Operate)
        return refuse(tr("Focus mode works in Design mode only: switch out of Operate first."));
    QList<quint32> heads = focusable(m_doc, options.fixtures);
    if (heads.isEmpty())
        return refuse(tr("None of these fixtures can be focused: they need a place on the "
                         "plot, and pan and tilt or a light of their own."));
    const QList<quint32> fixed = fixedLights(m_doc, heads);
    // A fixed light needs no reference: it is moved to where it really lands.
    if (!options.first.isValid() && fixed.size() < heads.size())
        return refuse(tr("Choose what the heads should aim at."));

    if (m_active)
        stop();

    m_options = options;
    m_order = (options.order == ByStagePosition) ? stageOrder(m_doc, heads) : heads;
    m_heads.clear();
    m_checked.clear();
    m_undo.clear();
    m_useSecond = false;

    MonitorProperties *props = m_doc->monitorProperties();
    for (quint32 fid : m_order)
    {
        const FixtureRigProps rp = props->fixtureRigProps(fid);
        Head h;
        h.saved.pan = rp.panOffsetDeg;
        h.saved.tilt = rp.tiltOffsetDeg;
        h.live = h.saved;
        if (fixed.contains(fid))
        {
            h.fixed = true;
            h.origAim = rp.fixedAim;
            h.origPan = rp.panZeroDir;
            h.origDown = rp.aimDown;
        }
        double lo = 0.0, hi = 0.0;
        const Fixture *fxb = m_doc->fixture(fid);
        if (RigHeads::lensRange(fxb, lo, hi))
        {
            h.beam = RigHeads::hasZoomChannel(fxb) ? int(ZoomChannel) : int(LensSetByHand);
            h.lensSaved = h.lensLive = rp.lensDeg;
        }
        m_heads.insert(fid, h);
    }
    resolveReferences();

    // Only heads with somewhere to aim.
    QList<quint32> kept;
    for (quint32 fid : m_order)
        if (m_heads.value(fid).hasFirst)
            kept << fid;
        else
            m_heads.remove(fid);
    m_order = kept;
    if (m_order.isEmpty())
    {
        m_heads.clear();
        return refuse(tr("None of these heads has a point to aim at in the chosen reference."));
    }

    m_index = 0;
    m_active = true;
    rebuildWrites();
    emit started();
    emit currentChanged(currentFixture());
    return true;
}

void FocusSession::stop()
{
    if (!m_active)
        return;
    discardAll();
    m_active = false;
    m_index = -1;
    {
        QMutexLocker locker(&m_writesMutex);
        m_writes.clear();
    }
    m_layer.clear();
    m_heads.clear();
    m_order.clear();
    m_undo.clear();
    for (int s = 0; s < 2; ++s)
        m_stick[s][0] = m_stick[s][1] = 0.5f;
    m_trigger[0] = m_trigger[1] = 0.0f;
    emit stopped();
}

/*****************************************************************************
 * Where it is
 *****************************************************************************/

quint32 FocusSession::currentFixture() const
{
    if (!m_active || m_index < 0 || m_index >= m_order.size())
        return 0xFFFFFFFF;
    return m_order.at(m_index);
}

void FocusSession::setIndex(int index)
{
    if (!m_active || m_order.isEmpty())
        return;
    const int n = m_order.size();
    index = ((index % n) + n) % n;
    if (index == m_index)
        return;
    m_index = index;
    // A head with no second reference is aimed at its first.
    if (m_useSecond && !m_heads.value(currentFixture()).hasSecond)
    {
        m_useSecond = false;
        emit referenceChanged();
    }
    rebuildWrites();
    emit currentChanged(currentFixture());
}

void FocusSession::next()
{
    setIndex(m_index + 1);
}

void FocusSession::previous()
{
    setIndex(m_index - 1);
}

bool FocusSession::toggleReference()
{
    if (!m_active)
        return false;
    if (!m_useSecond && !m_heads.value(currentFixture()).hasSecond)
        return false;
    m_useSecond = !m_useSecond;
    rebuildWrites();
    emit referenceChanged();
    return true;
}

bool FocusSession::referencePoint(quint32 fid, QVector3D &point) const
{
    const auto it = m_heads.constFind(fid);
    if (it == m_heads.constEnd())
        return false;
    if (m_useSecond && it->hasSecond && !it->fixed)
    {
        point = it->second;
        return true;
    }
    if (!it->hasFirst)
        return false;
    point = it->first;
    return true;
}

/*****************************************************************************
 * What the views draw
 *****************************************************************************/

bool FocusSession::intendedCentred(quint32 fid, FixtureRigProps &rp,
                                   double &panCentredDeg, double &tiltCentredDeg) const
{
    if (!m_active || fid != currentFixture() || m_heads.value(fid).fixed)
        return false;
    QVector3D point;
    if (!referencePoint(fid, point))
        return false;
    AimSolver::Frame frame;
    if (!AimSolver::frameFor(m_doc, fid, frame))
        return false;
    /* Solved as though the head were true: the offsets correct the PHYSICAL
       head, so they are in the DMX and nowhere in what it is meant to do. */
    frame.panOffsetDeg = 0.0f;
    frame.tiltOffsetDeg = 0.0f;
    const AimSolver::Solution sol = AimSolver::solve(frame, point);
    panCentredDeg = double(sol.panDeg) - double(frame.panMax) / 2.0;
    tiltCentredDeg = double(sol.tiltDeg) - double(frame.tiltMax) / 2.0;
    rp = m_doc->monitorProperties()->fixtureRigProps(fid);
    rp.panOffsetDeg = 0.0f;
    rp.tiltOffsetDeg = 0.0f;
    return true;
}

bool FocusSession::intendedAngles(quint32 fid, double &panRelDeg, double &tiltRelDeg) const
{
    FixtureRigProps rp;
    double pan = 0.0, tilt = 0.0;
    if (!intendedCentred(fid, rp, pan, tilt))
        return false;
    AimSolver::intendedAngles(rp, pan, tilt, panRelDeg, tiltRelDeg);
    return true;
}

bool FocusSession::intendedDirection(quint32 fid, QVector3D &dir) const
{
    if (m_active && fid == currentFixture() && m_heads.value(fid).fixed)
    {
        // Toward where it lands.
        dir = m_heads.value(fid).first - m_doc->monitorProperties()->fixtureRigPosition(fid);
        if (dir.lengthSquared() < 1e-9f)
            return false;
        dir.normalize();
        return true;
    }
    FixtureRigProps rp;
    double pan = 0.0, tilt = 0.0;
    if (!intendedCentred(fid, rp, pan, tilt))
        return false;
    return AimSolver::beamDirection(rp, pan, tilt, dir);
}

QString FocusSession::referenceName() const
{
    if (m_heads.value(currentFixture()).fixed)
        return tr("where it lands, at head height");
    const Reference &ref = (m_useSecond && m_options.second.isValid())
                           ? m_options.second : m_options.first;
    if (ref.kind == Reference::SceneAim)
    {
        const Function *f = m_doc->function(ref.id);
        return f != nullptr ? tr("Aim of %1").arg(f->name()) : tr("Scene aim");
    }
    if (ref.kind == Reference::Target)
    {
        const StageTarget *t = m_doc->monitorProperties()->stageTarget(ref.id);
        return t != nullptr ? t->name() : tr("Target");
    }
    return QString();
}

QHash<quint32, QVector3D> FocusSession::resolve(const Reference &ref) const
{
    QHash<quint32, QVector3D> out;
    if (ref.kind == Reference::SceneAim)
    {
        const Scene *scene = qobject_cast<const Scene *>(m_doc->function(ref.id));
        if (scene == nullptr)
            return out;
        // Each head's own point, as the scene's Aim look resolves it -- a line
        // target gives every head its own place on the line.
        for (const AimSolver::SceneAim &a : AimSolver::sceneAims(m_doc, scene))
            if (!out.contains(a.fixtureId))
                out.insert(a.fixtureId, a.point);
    }
    else if (ref.kind == Reference::Target)
    {
        const StageTarget *t = m_doc->monitorProperties()->stageTarget(ref.id);
        if (t == nullptr)
            return out;
        if (t->isLine())
        {
            // The heads spread along it, in focus order.
            const QHash<quint32, double> fr =
                AimSolver::lineFractions(m_doc, t, m_order, LineSpread::Settings());
            for (auto it = fr.constBegin(); it != fr.constEnd(); ++it)
                out.insert(it.key(), AimSolver::linePoint(m_doc, t, it.value()));
        }
        else
        {
            for (quint32 fid : m_order)
                out.insert(fid, AimSolver::aimPointFor(m_doc, fid, t, false));
        }
    }
    return out;
}

void FocusSession::resolveReferences()
{
    const QHash<quint32, QVector3D> first = resolve(m_options.first);
    const QHash<quint32, QVector3D> second = resolve(m_options.second);
    for (auto it = m_heads.begin(); it != m_heads.end(); ++it)
    {
        if (it->fixed)
        {
            // Its own point: where it lands now, at head height.
            it->first = it->landSaved = fixedLanding(it.key());
            it->hasFirst = true;
            it->hasSecond = false;
            continue;
        }
        it->hasFirst = first.contains(it.key());
        it->first = first.value(it.key());
        it->hasSecond = second.contains(it.key());
        it->second = second.value(it.key());
    }
}

/*****************************************************************************
 * Offsets
 *****************************************************************************/

FocusSession::Offset FocusSession::liveOffset(quint32 fid) const
{
    return m_heads.value(fid).live;
}

FocusSession::Offset FocusSession::savedOffset(quint32 fid) const
{
    return m_heads.value(fid).saved;
}

bool FocusSession::hasUnsaved() const
{
    for (auto it = m_heads.constBegin(); it != m_heads.constEnd(); ++it)
        if (it->dirty())
            return true;
    return false;
}

QList<quint32> FocusSession::unsavedFixtures() const
{
    QList<quint32> out;
    for (quint32 fid : m_order)
        if (isDirty(fid))
            out << fid;
    return out;
}

void FocusSession::applyOffset(quint32 fid, const Offset &offset)
{
    MonitorProperties *props = m_doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(fid);
    if (rp.panOffsetDeg == offset.pan && rp.tiltOffsetDeg == offset.tilt)
        return;
    rp.panOffsetDeg = offset.pan;
    rp.tiltOffsetDeg = offset.tilt;
    props->setFixtureRigProps(fid, rp);
}

void FocusSession::nudge(float dPan, float dTilt)
{
    const quint32 fid = currentFixture();
    if (!m_heads.contains(fid) || (dPan == 0.0f && dTilt == 0.0f))
        return;
    Head &h = m_heads[fid];
    if (h.fixed)
    {
        /* Move where it lands: right is toward stage left (+X), up is
           upstage (-Y), at head height on whatever is there. */
        MonitorProperties *props = m_doc->monitorProperties();
        QVector3D at = h.first + QVector3D(dPan * metresPerDegree(), -dTilt * metresPerDegree(), 0.0f);
        at = QVector3D(float(qRound(double(at.x()) * 1000.0) / 1000.0),
                       float(qRound(double(at.y()) * 1000.0) / 1000.0), 0.0f);
        at.setZ(props->platformHeightAt(at.x(), at.y()) + subjectHeight());
        if (at == h.first)
            return;
        h.first = at;
        applyLanding(fid, h, false);
        rebuildWrites();
        emit offsetChanged(fid);
        return;
    }
    Offset o = h.live;
    o.pan = roundOffset(qBound(-kPanOffsetLimit, o.pan + dPan, kPanOffsetLimit));
    o.tilt = roundOffset(qBound(-kTiltOffsetLimit, o.tilt + dTilt, kTiltOffsetLimit));
    if (o == h.live)
        return;
    h.live = o;
    /* Into the rig now, but not "modified": an unsaved nudge is a question,
       not an edit, until A says so. */
    applyOffset(fid, o);
    rebuildWrites();
    emit offsetChanged(fid);
}

void FocusSession::setStepDegrees(float step)
{
    m_step = qBound(0.01f, step, 10.0f);
}

void FocusSession::markModified()
{
    m_doc->setModified();
}

void FocusSession::save()
{
    const quint32 fid = currentFixture();
    if (!m_heads.contains(fid))
        return;
    Head &h = m_heads[fid];
    m_undo.append({ fid, h.saved, m_checked.contains(fid), h.landSaved, h.savedIsOriginal, h.lensSaved });
    if (h.fixed)
    {
        if (h.first != h.landSaved || !h.savedIsOriginal)
        {
            h.landSaved = h.first;
            h.savedIsOriginal = false;
            applyLanding(fid, h, false);
        }
    }
    else
    {
        h.saved = h.live;
        applyOffset(fid, h.live);
    }
    h.lensSaved = h.lensLive;
    m_checked.insert(fid);
    markModified();
    emit offsetChanged(fid);
}

void FocusSession::applyLens(quint32 fid, float deg)
{
    MonitorProperties *props = m_doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(fid);
    if (rp.lensDeg == deg)
        return;
    rp.lensDeg = deg;
    props->setFixtureRigProps(fid, rp);
}

FocusSession::BeamKind FocusSession::beamKind(quint32 fid) const
{
    return BeamKind(m_heads.value(fid).beam);
}

bool FocusSession::beamAngle(quint32 fid, double &deg, double &lo, double &hi) const
{
    const auto it = m_heads.constFind(fid);
    if (it == m_heads.constEnd() || it->beam == NoBeamControl
            || !RigHeads::lensRange(m_doc->fixture(fid), lo, hi))
        return false;
    if (it->beam == LensSetByHand)
        deg = it->lensLive > 0.0f ? qBound(lo, double(it->lensLive), hi) : hi;
    else
        deg = it->zoomDeg > 0.0 ? it->zoomDeg : hi;
    return true;
}

void FocusSession::nudgeBeam(float dDeg)
{
    const quint32 fid = currentFixture();
    double deg = 0.0, lo = 0.0, hi = 0.0;
    if (!m_heads.contains(fid) || dDeg == 0.0f || !beamAngle(fid, deg, lo, hi))
        return;
    Head &h = m_heads[fid];
    const double to = qBound(lo, double(qRound((deg + double(dDeg)) * 10.0)) / 10.0, hi);
    if (qFuzzyCompare(to, deg))
        return;
    if (h.beam == LensSetByHand)
    {
        h.lensLive = float(to);
        applyLens(fid, h.lensLive);
    }
    else
        h.zoomDeg = to;
    rebuildWrites();
    emit offsetChanged(fid);
}

static int focusChannelOf(const Fixture *fx, bool *reversed = nullptr)
{
    const QLCFixtureMode *mode = (fx != nullptr) ? fx->fixtureMode() : nullptr;
    if (mode == nullptr)
        return -1;
    for (quint32 c = 0; c < quint32(mode->channels().size()); ++c)
        if (const QLCChannel *ch = mode->channel(c))
            if (ch->preset() == QLCChannel::BeamFocusNearFar || ch->preset() == QLCChannel::BeamFocusFarNear)
            {
                if (reversed != nullptr)
                    *reversed = ch->preset() == QLCChannel::BeamFocusFarNear;
                return int(c);
            }
    return -1;
}

bool FocusSession::hasFocusControl(quint32 fid) const
{
    return m_heads.contains(fid) && focusChannelOf(m_doc->fixture(fid)) >= 0;
}

bool FocusSession::focusLevel(quint32 fid, double &level) const
{
    if (!hasFocusControl(fid))
        return false;
    const double f = m_heads.value(fid).focus;
    level = f < 0.0 ? 0.5 : f;
    return true;
}

void FocusSession::nudgeFocus(float d)
{
    const quint32 fid = currentFixture();
    double level = 0.0;
    if (d == 0.0f || !focusLevel(fid, level))
        return;
    const double to = qBound(0.0, double(qRound((level + double(d)) * 100.0)) / 100.0, 1.0);
    m_heads[fid].focus = to;
    rebuildWrites();
    emit offsetChanged(fid);
}

void FocusSession::setTrigger(int side, float norm)
{
    if (m_input == KeyboardInput || side < 0 || side > 1)
        return;
    m_trigger[side] = qBound(0.0f, norm, 1.0f);
}

void FocusSession::backToSaved(quint32 fid, Head &h)
{
    if (h.beam == LensSetByHand && h.lensLive != h.lensSaved)
    {
        h.lensLive = h.lensSaved;
        applyLens(fid, h.lensLive);
    }
    if (h.fixed)
    {
        h.first = h.landSaved;
        applyLanding(fid, h, h.savedIsOriginal);
    }
    else
    {
        h.live = h.saved;
        applyOffset(fid, h.live);
    }
}

void FocusSession::revert()
{
    const quint32 fid = currentFixture();
    if (!m_heads.contains(fid))
        return;
    Head &h = m_heads[fid];
    if (!h.dirty())
        return;
    backToSaved(fid, h);
    rebuildWrites();
    emit offsetChanged(fid);
}

void FocusSession::zero()
{
    const quint32 fid = currentFixture();
    if (!m_heads.contains(fid))
        return;
    Head &h = m_heads[fid];
    if (h.fixed)
    {
        // As it pointed when the session began.
        if (!h.dirty())
            return;
        backToSaved(fid, h);
        rebuildWrites();
        emit offsetChanged(fid);
        return;
    }
    if (h.live == Offset())
        return;
    h.live = Offset();
    applyOffset(fid, h.live);
    rebuildWrites();
    emit offsetChanged(fid);
}

bool FocusSession::undoSave()
{
    if (m_undo.isEmpty())
        return false;
    const UndoEntry e = m_undo.takeLast();
    if (!m_heads.contains(e.fid))
        return false;
    Head &h = m_heads[e.fid];
    h.saved = e.before;
    h.live = e.before;
    h.landSaved = e.landBefore;
    h.savedIsOriginal = e.wasOriginal;
    h.lensSaved = e.lensBefore;
    if (!e.wasChecked)
        m_checked.remove(e.fid);
    backToSaved(e.fid, h);
    markModified();
    rebuildWrites();
    emit offsetChanged(e.fid);
    return true;
}

void FocusSession::saveAll()
{
    bool any = false;
    for (quint32 fid : m_order)
    {
        Head &h = m_heads[fid];
        if (!h.dirty())
            continue;
        m_undo.append({ fid, h.saved, m_checked.contains(fid), h.landSaved, h.savedIsOriginal, h.lensSaved });
        if (h.fixed)
        {
            if (h.first != h.landSaved || !h.savedIsOriginal)
            {
                h.landSaved = h.first;
                h.savedIsOriginal = false;
                applyLanding(fid, h, false);
            }
        }
        else
        {
            h.saved = h.live;
            applyOffset(fid, h.live);
        }
        h.lensSaved = h.lensLive;
        m_checked.insert(fid);
        any = true;
        emit offsetChanged(fid);
    }
    if (any)
        markModified();
}

void FocusSession::discardAll()
{
    for (quint32 fid : m_order)
    {
        Head &h = m_heads[fid];
        if (!h.dirty())
            continue;
        backToSaved(fid, h);
        emit offsetChanged(fid);
    }
    rebuildWrites();
}

/*****************************************************************************
 * Controller
 *****************************************************************************/

FocusSession::Control FocusSession::controlFor(GamepadRole::Role role)
{
    switch (role)
    {
        case GamepadRole::RB:        return Next;
        case GamepadRole::LB:        return Previous;
        case GamepadRole::A:         return Save;
        case GamepadRole::B:         return Revert;
        case GamepadRole::X:         return Zero;
        case GamepadRole::Y:         return ToggleReference;
        case GamepadRole::Back:      return UndoSave;
        case GamepadRole::Start:     return Exit;
        // Zoom up and down, focus left and right (Q103); the sticks nudge.
        case GamepadRole::DpadUp:    return ZoomWider;
        case GamepadRole::DpadDown:  return ZoomNarrower;
        case GamepadRole::DpadLeft:  return FocusNearer;
        case GamepadRole::DpadRight: return FocusFarther;
        default:                     return NoControl;
    }
}

void FocusSession::apply(Control control)
{
    if (!m_active)
        return;
    switch (control)
    {
        case Next:            next(); break;
        case Previous:        previous(); break;
        case Save:            save(); break;
        case Revert:          revert(); break;
        case Zero:            zero(); break;
        case ToggleReference: toggleReference(); break;
        case UndoSave:        undoSave(); break;
        case Exit:            emit exitRequested(); break;
        case StepPanUp:       nudge(m_step, 0.0f); break;
        case StepPanDown:     nudge(-m_step, 0.0f); break;
        case StepTiltUp:      nudge(0.0f, m_step); break;
        case StepTiltDown:    nudge(0.0f, -m_step); break;
        case ZoomWider:       nudgeBeam(1.0f); break;
        case ZoomNarrower:    nudgeBeam(-1.0f); break;
        case FocusNearer:     nudgeFocus(-0.02f); break;
        case FocusFarther:    nudgeFocus(0.02f); break;
        case NoControl:       break;
    }
}

float FocusSession::stickVelocity(float norm, float deadzone)
{
    // followspot.js axisVelocity(), so a stick feels the same in both.
    const float dz = qBound(0.0f, deadzone, 0.45f);
    const float delta = norm - 0.5f;
    if (qAbs(delta) < dz)
        return 0.0f;
    const float sign = delta > 0.0f ? 1.0f : -1.0f;
    return sign * qMin(1.0f, (qAbs(delta) - dz) / (0.5f - dz));
}

void FocusSession::setStick(Stick stick, float xNorm, float yNorm)
{
    if (m_input == KeyboardInput)
        return;
    m_stick[stick][0] = qBound(0.0f, xNorm, 1.0f);
    m_stick[stick][1] = qBound(0.0f, yNorm, 1.0f);
}

void FocusSession::setStickResponse(float deadzone, float sensitivity)
{
    m_deadzone = deadzone;
    m_sensitivity = sensitivity > 0.0f ? sensitivity : 1.0f;
}

void FocusSession::setInputDevice(InputDevice device)
{
    if (device == m_input)
        return;
    m_input = device;
    if (device == KeyboardInput)
    {
        // Whatever the stick last said, it says nothing now.
        for (int s = 0; s < 2; ++s)
            m_stick[s][0] = m_stick[s][1] = 0.5f;
        m_carryPan = m_carryTilt = 0.0f;
    }
    emit inputDeviceChanged(device);
}

void FocusSession::tick(double dt)
{
    if (!m_active || m_input == KeyboardInput)
        return;
    // The triggers: RT widens, LT narrows, in tenths carried between ticks.
    {
        const float d = (m_trigger[1] - m_trigger[0]) * beamRate() * m_sensitivity * float(dt);
        if (qAbs(m_trigger[1] - m_trigger[0]) < 0.05f)
            m_carryBeam = 0.0f;
        else
        {
            m_carryBeam += d;
            const float step = float(qRound(double(m_carryBeam) * 10.0) / 10.0);
            m_carryBeam -= step;
            nudgeBeam(step);
        }
    }
    float dPan = 0.0f, dTilt = 0.0f;
    for (int s = 0; s < 2; ++s)
    {
        const float rate = (s == Coarse ? coarseRate() : fineRate()) * m_sensitivity * float(dt);
        dPan += stickVelocity(m_stick[s][0], m_deadzone) * rate;
        // Pushed UP reads low (0 = up): up is more tilt offset.
        dTilt -= stickVelocity(m_stick[s][1], m_deadzone) * rate;
    }
    /* Offsets are held to hundredths, so a light touch on the fine stick
       (a few thousandths per tick) would round away to nothing: carry the
       remainder between ticks instead. */
    if (dPan == 0.0f && dTilt == 0.0f)
    {
        m_carryPan = m_carryTilt = 0.0f;
        return;
    }
    m_carryPan += dPan;
    m_carryTilt += dTilt;
    const float stepPan = float(qRound(double(m_carryPan) * 100.0) / 100.0);
    const float stepTilt = float(qRound(double(m_carryTilt) * 100.0) / 100.0);
    m_carryPan -= stepPan;
    m_carryTilt -= stepTilt;
    nudge(stepPan, stepTilt);
}

/*****************************************************************************
 * DMX
 *****************************************************************************/

void FocusSession::rebuildWrites()
{
    QHash<QPair<int, int>, uchar> writes;
    if (m_active)
    {
        auto put = [&](Fixture *fx, quint32 ch, uchar value) {
            const int addr = int(fx->address()) + int(ch);
            // Nothing past the end of the universe (an over-addressed fixture).
            if (addr < UNIVERSE_SIZE)
                writes.insert(qMakePair(int(fx->universe()), addr), value);
        };
        auto lightLevel = [&](Fixture *fx, float level, bool white) {
            QLCFixtureMode *mode = fx->fixtureMode();
            for (quint32 c = 0; c < fx->channels(); ++c)
            {
                const QLCChannel *ch = mode->channel(c);
                if (ch == nullptr)
                    continue;
                if (ch->group() == QLCChannel::Intensity && ch->colour() == QLCChannel::NoColour)
                    put(fx, c, uchar(qRound(qBound(0.0f, level, 1.0f) * 255.0f)));
                else if (!white)
                    continue;
                else if (ch->group() == QLCChannel::Intensity || ch->group() == QLCChannel::Colour)
                {
                    switch (ch->colour())
                    {
                        case QLCChannel::Red: case QLCChannel::Green: case QLCChannel::Blue:
                        case QLCChannel::White: case QLCChannel::Lime: case QLCChannel::Indigo:
                            put(fx, c, 255);
                            break;
                        case QLCChannel::Cyan: case QLCChannel::Magenta: case QLCChannel::Yellow:
                        case QLCChannel::Amber: case QLCChannel::UV:
                            put(fx, c, 0);
                            break;
                        default:
                            break;
                    }
                }
                else if (ch->group() == QLCChannel::Shutter)
                {
                    // Open, if the definition says which values are.
                    for (const QLCCapability *cap : ch->capabilities())
                        if (cap->preset() == QLCCapability::ShutterOpen)
                        {
                            put(fx, c, uchar((int(cap->min()) + int(cap->max())) / 2));
                            break;
                        }
                }
            }
        };

        const quint32 current = currentFixture();
        for (quint32 fid : m_order)
        {
            Fixture *fx = m_doc->fixture(fid);
            if (fx == nullptr || fx->fixtureMode() == nullptr || !fx->isPatched())
                continue;
            if (fid != current)
            {
                if (m_options.others == OthersDark)
                    lightLevel(fx, 0.0f, false);
                else if (m_options.others == OthersDim)
                    lightLevel(fx, m_options.dimLevel, false);
                continue;
            }

            // Aimed through the solver, which reads the live offsets from the
            // rig (applyOffset()): the same DMX an Aim look would send.
            QVector3D point;
            AimSolver::Solution sol;
            if (!m_heads.value(fid).fixed            // a fixed light only lights
                && referencePoint(fid, point) && AimSolver::reach(m_doc, fid, point, sol))
            {
                for (const SceneValue &sv : fx->positionToValues(QLCChannel::Pan, sol.panDeg))
                    put(fx, sv.channel, sv.value);
                for (const SceneValue &sv : fx->positionToValues(QLCChannel::Tilt, sol.tiltDeg))
                    put(fx, sv.channel, sv.value);
            }
            lightLevel(fx, m_options.level, true);
            // A zoom channel, to the beam asked for while focusing.
            const Head hd = m_heads.value(fid);
            bool rev = false;
            const int fc = focusChannelOf(fx, &rev);
            if (fc >= 0 && hd.focus >= 0.0)
                put(fx, quint32(fc), uchar(qRound((rev ? 1.0 - hd.focus : hd.focus) * 255.0)));
            double lo = 0.0, hi = 0.0;
            if (hd.beam == ZoomChannel && hd.zoomDeg > 0.0 && RigHeads::lensRange(fx, lo, hi))
            {
                const double v = (hd.zoomDeg - lo) / (hi - lo);
                QLCFixtureMode *mode = fx->fixtureMode();
                for (quint32 c = 0; c < fx->channels(); ++c)
                    if (const QLCChannel *ch = mode->channel(c))
                    {
                        if (ch->preset() == QLCChannel::BeamZoomSmallBig)
                            put(fx, c, uchar(qRound(qBound(0.0, v, 1.0) * 255.0)));
                        else if (ch->preset() == QLCChannel::BeamZoomBigSmall)
                            put(fx, c, uchar(qRound(qBound(0.0, 1.0 - v, 1.0) * 255.0)));
                    }
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

QHash<QPair<int, int>, uchar> FocusSession::pendingWrites() const
{
    QMutexLocker locker(&m_writesMutex);
    return m_writes;
}
