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
#include "universe.h"
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
    if (m_registered && m_doc != nullptr && m_doc->masterTimer() != nullptr)
        m_doc->masterTimer()->unregisterDMXSource(this);
}

/*****************************************************************************
 * Starting and stopping
 *****************************************************************************/

QList<quint32> FocusSession::focusable(Doc *doc, const QList<quint32> &fixtures)
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
        return refuse(tr("Focus mode works in Design mode only: switch out of Perform first."));
    if (!options.first.isValid())
        return refuse(tr("Choose what the heads should aim at."));

    QList<quint32> heads = focusable(m_doc, options.fixtures);
    if (heads.isEmpty())
        return refuse(tr("None of these fixtures can be focused: they need pan or tilt "
                         "and a place on the plot."));

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
    if (!m_registered && m_doc->masterTimer() != nullptr)
    {
        m_doc->masterTimer()->registerDMXSource(this);
        m_registered = true;
    }
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
    if (m_registered && m_doc->masterTimer() != nullptr)
    {
        m_doc->masterTimer()->unregisterDMXSource(this);
        m_registered = false;
    }
    m_heads.clear();
    m_order.clear();
    m_undo.clear();
    for (int s = 0; s < 2; ++s)
        m_stick[s][0] = m_stick[s][1] = 0.5f;
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
    if (m_useSecond && it->hasSecond)
    {
        point = it->second;
        return true;
    }
    if (!it->hasFirst)
        return false;
    point = it->first;
    return true;
}

QString FocusSession::referenceName() const
{
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
        if (it->live != it->saved)
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
    m_undo.append({ fid, h.saved, m_checked.contains(fid) });
    h.saved = h.live;
    m_checked.insert(fid);
    applyOffset(fid, h.live);
    markModified();
    emit offsetChanged(fid);
}

void FocusSession::revert()
{
    const quint32 fid = currentFixture();
    if (!m_heads.contains(fid))
        return;
    Head &h = m_heads[fid];
    if (h.live == h.saved)
        return;
    h.live = h.saved;
    applyOffset(fid, h.live);
    rebuildWrites();
    emit offsetChanged(fid);
}

void FocusSession::zero()
{
    const quint32 fid = currentFixture();
    if (!m_heads.contains(fid))
        return;
    Head &h = m_heads[fid];
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
    if (!e.wasChecked)
        m_checked.remove(e.fid);
    applyOffset(e.fid, h.live);
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
        if (h.live == h.saved)
            continue;
        m_undo.append({ fid, h.saved, m_checked.contains(fid) });
        h.saved = h.live;
        m_checked.insert(fid);
        applyOffset(fid, h.live);
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
        if (h.live == h.saved)
            continue;
        h.live = h.saved;
        applyOffset(fid, h.live);
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
        case GamepadRole::DpadUp:    return StepTiltUp;
        case GamepadRole::DpadDown:  return StepTiltDown;
        case GamepadRole::DpadLeft:  return StepPanDown;
        case GamepadRole::DpadRight: return StepPanUp;
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
    m_stick[stick][0] = qBound(0.0f, xNorm, 1.0f);
    m_stick[stick][1] = qBound(0.0f, yNorm, 1.0f);
}

void FocusSession::setStickResponse(float deadzone, float sensitivity)
{
    m_deadzone = deadzone;
    m_sensitivity = sensitivity > 0.0f ? sensitivity : 1.0f;
}

void FocusSession::tick(double dt)
{
    if (!m_active)
        return;
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
            // Universe::write() only Q_ASSERTs the bound (see HighlightEffect).
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
            if (referencePoint(fid, point) && AimSolver::reach(m_doc, fid, point, sol))
            {
                for (const SceneValue &sv : fx->positionToValues(QLCChannel::Pan, sol.panDeg))
                    put(fx, sv.channel, sv.value);
                for (const SceneValue &sv : fx->positionToValues(QLCChannel::Tilt, sol.tiltDeg))
                    put(fx, sv.channel, sv.value);
            }
            lightLevel(fx, m_options.level, true);
        }
    }
    QMutexLocker locker(&m_writesMutex);
    m_writes = writes;
}

QHash<QPair<int, int>, uchar> FocusSession::pendingWrites() const
{
    QMutexLocker locker(&m_writesMutex);
    return m_writes;
}

void FocusSession::writeDMX(MasterTimer *timer, QList<Universe *> universes)
{
    Q_UNUSED(timer)
    QMutexLocker locker(&m_writesMutex);
    for (auto it = m_writes.constBegin(); it != m_writes.constEnd(); ++it)
    {
        const int u = it.key().first;
        if (u < 0 || u >= universes.size() || universes.at(u) == nullptr)
            continue;
        universes.at(u)->write(it.key().second, it.value(), /*forceLTP=*/true);
    }
}
