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
#include <QTimer>
#include <algorithm>

#include "venuecorrections.h"
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
    // The selection's flash: a few frames a blink, then it stops itself.
    m_flashTimer = new QTimer(this);
    m_flashTimer->setInterval(30);
    connect(m_flashTimer, &QTimer::timeout, this, &FocusSession::onFlashTick);
    /* Into Perform, the rig belongs to the show: Focus mode ends, and a head
       still being nudged goes back to what was saved. */
    if (m_doc != nullptr)
        connect(m_doc, &Doc::modeChanged, this, [this](Doc::Mode mode) {
            if (mode == Doc::Operate && !m_options.allowOperate)
                stop();
        });
    /* Which heads are unsaved can change with any of these; tell it once. */
    connect(this, &FocusSession::offsetChanged, this, &FocusSession::checkUnsaved);
    connect(this, &FocusSession::started, this, &FocusSession::checkUnsaved);
    connect(this, &FocusSession::stopped, this, &FocusSession::checkUnsaved);
    m_blinkTimer.setInterval(kBlinkPhaseMs);
    connect(&m_blinkTimer, &QTimer::timeout, this, &FocusSession::advanceBlink);
    // The unsaved light's slow breath (gap 10).
    m_breathTimer = new QTimer(this);
    m_breathTimer->setInterval(50);
    connect(m_breathTimer, &QTimer::timeout, this, &FocusSession::rebuildWrites);
}

float FocusSession::breathLevel(qint64 ms)
{
    const double phase = double(ms % breathPeriodMs()) / breathPeriodMs();
    const double up = 0.5 + 0.5 * qCos(phase * 2.0 * M_PI);       // 1 at the start
    return float(breathLow() + (1.0 - breathLow()) * up);
}

bool FocusSession::isBreathing() const
{
    return m_active && m_flashEnabled && m_lightsFeedback && m_rigFeedback && !m_holding
        && m_scope == FixtureScope && !m_targetPage && isDirty(currentFixture());
}

void FocusSession::setHolding(bool on)
{
    if (on == m_holding)
        return;
    m_holding = on;
    // Every light as it is: no blink, no breath, until released.
    m_flashClock.invalidate();
    if (m_flashTimer != nullptr)
        m_flashTimer->stop();
    m_hold.reset();
    rebuildWrites();
}

bool FocusSession::openBeamValue(const QLCChannel *ch, uchar &value)
{
    if (ch == nullptr)
        return false;
    const QLCChannel::Group g = ch->group();
    const bool frost = ch->name().contains(QLatin1String("frost"), Qt::CaseInsensitive);
    // An index or a rotation is not in the beam's way; colour and intensity are the look's.
    if (g == QLCChannel::Gobo && (ch->preset() == QLCChannel::GoboIndex || ch->preset() == QLCChannel::GoboIndexFine))
        return false;
    if (g == QLCChannel::Prism && (ch->preset() == QLCChannel::PrismRotationSlowFast
                                   || ch->preset() == QLCChannel::PrismRotationFastSlow
                                   || ch->name().contains(QLatin1String("rot"), Qt::CaseInsensitive)))
        return false;
    if (g != QLCChannel::Gobo && g != QLCChannel::Prism && !(frost && g != QLCChannel::Intensity
                                                              && g != QLCChannel::Colour))
        return false;
    for (const QLCCapability *cap : ch->capabilities())
    {
        const QString n = cap->name().trimmed().toLower();
        const bool open = n.startsWith(QLatin1String("open")) || n == QLatin1String("off")
                          || n.startsWith(QLatin1String("no ")) || cap->preset() == QLCCapability::PrismEffectOff;
        if (open)
        {
            value = uchar((int(cap->min()) + int(cap->max())) / 2);
            return true;
        }
    }
    // A plain frost (0 none .. 255 full): none.
    if (frost && g != QLCChannel::Gobo && g != QLCChannel::Prism)
    {
        value = 0;
        return true;
    }
    return false;
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
    if (m_doc->mode() == Doc::Operate && !options.allowOperate)
        return refuse(tr("Quick focus works in Design mode only: switch out of Operate first."));
    QString busy;
    if (m_startGuard && !m_startGuard(&busy))
        return refuse(busy);
    const bool runCalls = !options.calls.isEmpty();
    QList<quint32> heads;
    if (runCalls)
    {
        for (const Call &c : options.calls)
            for (quint32 fid : focusable(m_doc, c.fixtures))
                if (!heads.contains(fid))
                    heads << fid;
    }
    else
        heads = focusable(m_doc, options.fixtures);
    if (heads.isEmpty())
        return refuse(tr("None of these fixtures can be focused: they need a place on the "
                         "plot, and pan and tilt or a light of their own."));
    const QList<quint32> fixed = fixedLights(m_doc, heads);
    // A fixed light needs no reference: it is moved to where it really lands.
    if (!runCalls && !options.first.isValid() && fixed.size() < heads.size())
        return refuse(tr("Choose what the heads should aim at."));

    if (m_active)
        stop();

    m_options = options;
    m_targetPage = false;
    m_order = (options.order == ByStagePosition) ? stageOrder(m_doc, heads) : heads;
    m_heads.clear();
    m_checked.clear();
    m_undo.clear();
    m_calls.clear();
    m_callIndex = -1;
    m_useSecond = false;
    m_scope = FixtureScope;

    MonitorProperties *props = m_doc->monitorProperties();
    for (quint32 fid : m_order)
    {
        /* With a venue applied, the show's calibration is held before any
           of it moves: nothing this session does reaches the show file. */
        if (m_venues != nullptr && m_venues->isActive())
            m_venues->ensureBase(fid);
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
        if (h.fixed)
        {
            // Its own point: where it lands now, at head height.
            h.first = h.landSaved = fixedLanding(fid);
            h.hasFirst = true;
        }
        m_heads.insert(fid, h);
    }

    if (runCalls)
    {
        /* Each call: its heads, in order, and the point each focuses to.
           A head with nowhere to go in a call is left out of it; a call
           with no heads left is left out. */
        QSet<quint32> used;
        for (const Call &c : options.calls)
        {
            CallState cs;
            cs.call = c;
            const QList<quint32> mine = focusable(m_doc, c.fixtures);
            m_order = (options.order == ByStagePosition) ? stageOrder(m_doc, mine) : mine;
            const QHash<quint32, QVector3D> pts = resolve(c.target);
            for (quint32 fid : m_order)
            {
                if (m_heads.value(fid).fixed)
                    cs.points.insert(fid, m_heads.value(fid).first);
                else if (pts.contains(fid))
                    cs.points.insert(fid, pts.value(fid));
                else
                    continue;
                cs.order << fid;
                used.insert(fid);
            }
            if (!cs.order.isEmpty())
                m_calls << cs;
        }
        m_all.clear();
        for (const CallState &cs : m_calls)
            for (quint32 fid : cs.order)
                if (!m_all.contains(fid))
                    m_all << fid;
        for (quint32 fid : m_heads.keys())
            if (!used.contains(fid))
                m_heads.remove(fid);
        if (m_calls.isEmpty())
        {
            m_heads.clear();
            m_all.clear();
            m_order.clear();
            return refuse(tr("None of the focus calls has a fixture with a point to focus to."));
        }
        m_active = true;
        if (m_venues != nullptr)
            m_venues->setHeld(m_all);
        m_callIndex = -1;
        emit starting();
        enterCall(0);
        emit started();
        return true;
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
    m_all = kept;
    if (m_order.isEmpty())
    {
        m_heads.clear();
        return refuse(tr("None of these heads has a point to aim at in the chosen reference."));
    }

    m_index = 0;
    m_active = true;
    if (m_venues != nullptr)
        m_venues->setHeld(m_all);
    rebuildWrites();
    emit starting();
    emit started();
    emit currentChanged(currentFixture());
    flashSelection();
    emit announce(wherePhrase());
    return true;
}

void FocusSession::enterCall(int index)
{
    if (!m_active || m_calls.isEmpty())
        return;
    const int n = m_calls.size();
    index = ((index % n) + n) % n;
    m_callIndex = index;
    const CallState &cs = m_calls.at(index);
    m_order = cs.order;
    m_index = 0;
    for (quint32 fid : m_order)
    {
        Head &h = m_heads[fid];
        if (h.fixed)
            continue;
        h.first = cs.points.value(fid);
        h.hasFirst = true;
        h.hasSecond = false;
    }
    if (m_useSecond)
    {
        m_useSecond = false;
        emit referenceChanged();
    }
    rebuildWrites();
    emit callChanged(m_callIndex);
    emit currentChanged(currentFixture());
    flashSelection();
    emit announce(wherePhrase(true));
}

bool FocusSession::jumpTo(quint32 fid)
{
    if (!m_active || m_holding || !m_all.contains(fid) || targetBlocks())
        return false;
    if (fid == currentFixture() && m_scope == FixtureScope)
        return true;
    m_targetPage = false;
    if (isRunningCalls() && !m_order.contains(fid))
        for (int c = 0; c < m_calls.size(); ++c)
            if (m_calls.at(c).order.contains(fid))
            {
                enterCall(c);
                break;
            }
    m_scope = FixtureScope;
    setIndex(m_order.indexOf(fid));
    return true;
}

bool FocusSession::toggleDead()
{
    const quint32 fid = currentFixture();
    const Fixture *fx = m_doc->fixture(fid);
    if (!m_active || fx == nullptr)
        return false;
    if (!savesToVenue())
    {
        emit announce(tr("Apply a venue to mark a light dead."), ControllerScheme::ImportantPriority);
        return false;
    }
    const bool dead = !m_venues->isDead(fid);
    if (!m_venues->setDead(fid, dead))
        return false;
    emit announce(dead ? tr("%1 marked dead.").arg(fx->name()) : tr("%1 not dead.").arg(fx->name()),
                  ControllerScheme::ConfirmPriority);
    emit offsetChanged(fid);
    return true;
}

FocusSession::Call FocusSession::currentCall() const
{
    if (m_callIndex < 0 || m_callIndex >= m_calls.size())
        return Call();
    return m_calls.at(m_callIndex).call;
}

void FocusSession::setCallIndex(int index)
{
    if (!m_active || m_calls.isEmpty())
        return;
    enterCall(index);
}

void FocusSession::nextCall()
{
    if (m_calls.isEmpty())
        return;
    if (m_options.endAfterLastCall && m_callIndex == m_calls.size() - 1)
    {
        // The last call done: leave (asking about anything unsaved).
        emit announce(tr("Last call done."), ControllerScheme::ConfirmPriority);
        emit exitRequested();
        return;
    }
    enterCall(m_callIndex + 1);
}

void FocusSession::previousCall()
{
    if (m_calls.isEmpty())
        return;
    enterCall(m_callIndex - 1);
}

void FocusSession::setScope(Scope scope)
{
    if (!m_active || scope == m_scope)
        return;
    m_scope = scope;
    rebuildWrites();
    emit scopeChanged(m_scope);
    flashSelection();
    emit announce(wherePhrase());
}

void FocusSession::cycleScope()
{
    setScope(m_scope == FixtureScope ? GroupScope : FixtureScope);
}

QList<quint32> FocusSession::scopeFixtures() const
{
    if (!m_active)
        return QList<quint32>();
    if (m_scope == GroupScope)
        return m_order;
    const quint32 fid = currentFixture();
    return fid == 0xFFFFFFFF ? QList<quint32>() : QList<quint32>{ fid };
}

QString FocusSession::callPositionPhrase() const
{
    const Call c = currentCall();
    if (c.target.kind != Reference::Target || m_doc == nullptr || m_doc->monitorProperties() == nullptr)
        return QString();
    const StageTarget *t = m_doc->monitorProperties()->stageTarget(c.target.id);
    if (t == nullptr)
        return QString();
    return m_doc->monitorProperties()->stagePositionText(t->position(), MonitorProperties::SpokenOffset);
}

QString FocusSession::wherePhrase(bool withPosition) const
{
    if (!m_active)
        return QString();
    /* Worded by the phrase table at the Voice detail chosen. The call is
       said on entering it (@p withPosition), and at Verbose on every step,
       with where its target is when entering. */
    QStringList parts;
    if (isRunningCalls() && (withPosition || m_voiceDetail == ControllerScheme::VerboseVoice))
    {
        const Call c = currentCall();
        const QString where = withPosition ? callPositionPhrase() : QString();
        parts << ControllerScheme::callPhrase(m_voiceDetail, m_callIndex + 1, int(m_calls.size()),
                                              c.who.isEmpty() ? c.name : c.who,
                                              c.targetName.isEmpty() ? referenceName() : c.targetName, where);
    }
    if (m_scope == GroupScope)
        parts << (isRunningCalls() ? tr("All %1, group.").arg(m_order.size())
                                   : tr("All %1 fixtures.").arg(m_order.size()));
    else
    {
        parts << ControllerScheme::stepPhrase(m_voiceDetail, m_index + 1, int(m_order.size()),
                                              lightName(currentFixture()));
        // Its venue correction was skipped: another fixture type now (Q133).
        if (m_venues != nullptr && m_venues->needsFocus(currentFixture()))
            parts << tr("Needs focus: the fixture changed.");
        if (m_venues != nullptr && m_venues->isDead(currentFixture()))
            parts << tr("Marked dead.");
    }
    return parts.join(QLatin1Char(' '));
}

QString FocusSession::lightPhrase() const
{
    if (!m_active)
        return QString();
    // The light first, by name, always; then where, at the detail chosen.
    QString light;
    if (m_scope == GroupScope)
        light = isRunningCalls() && !currentCall().who.isEmpty() ? currentCall().who
                                                                 : tr("All %1").arg(m_order.size());
    else
        light = lightName(currentFixture());
    if (m_voiceDetail == ControllerScheme::TerseVoice && !light.isEmpty())
        return tr("%1.").arg(light);
    QString where = wherePhrase(true);
    if (m_scope != GroupScope && !light.isEmpty())
    {
        // Its name is said first: the step part without it.
        const QString named = ControllerScheme::stepPhrase(m_voiceDetail, m_index + 1, int(m_order.size()), light);
        where.replace(named, ControllerScheme::stepPhrase(m_voiceDetail, m_index + 1, int(m_order.size()),
                                                          QString()));
    }
    return light.isEmpty() ? where : tr("%1. %2").arg(light, where);
}

void FocusSession::whereAmI()
{
    if (!m_active)
        return;
    flashSelection();
    emit sayLight(lightPhrase());
}

bool FocusSession::flashLit(qint64 ms)
{
    if (ms < 0 || ms >= flashDurationMs())
        return true;
    return (ms % (flashOnMs() + flashOffMs())) < flashOnMs();
}

void FocusSession::flashSelection()
{
    if (!m_active || !m_flashEnabled || !m_lightsFeedback || m_holding)
        return;
    m_warnFlash = false;
    m_flashClock.start();
    if (m_flashTimer != nullptr)
        m_flashTimer->start();
    rebuildWrites();
}

void FocusSession::flashWarning()
{
    if (!m_active || !m_flashEnabled || !m_lightsFeedback || m_holding)
        return;
    m_warnFlash = true;
    m_flashClock.start();
    if (m_flashTimer != nullptr)
        m_flashTimer->start();
    rebuildWrites();
}

bool FocusSession::isFlashing() const
{
    return m_active && m_flashClock.isValid()
        && m_flashClock.elapsed() < (m_warnFlash ? warnDarkMs() : flashDurationMs());
}

void FocusSession::onFlashTick()
{
    if (!isFlashing())
    {
        m_flashTimer->stop();
        m_flashClock.invalidate();
    }
    rebuildWrites();
}

bool FocusSession::savesToVenue() const
{
    return m_venues != nullptr && m_venues->isActive();
}

void FocusSession::savedChanged(const QList<quint32> &fids, bool checked)
{
    if (fids.isEmpty())
        return;
    if (savesToVenue())
    {
        // Into the venue's corrections, and its file now; the show is untouched.
        for (quint32 fid : fids)
            m_venues->record(fid);
        // Who checked them, and when: the focus report's (Q134).
        if (checked)
            m_venues->markChecked(fids);
        return;
    }
    markModified();
}

void FocusSession::finish()
{
    m_endedByOperator = true;
    stop();
    m_endedByOperator = false;
}

void FocusSession::stop()
{
    if (!m_active)
        return;
    discardAll();
    // Back as saved: from here on a change to them is anyone's edit again.
    if (m_venues != nullptr)
        m_venues->setHeld(QList<quint32>());
    m_active = false;
    m_index = -1;
    m_hold.reset();
    m_blinkTimer.stop();
    m_blinkPhase = 0;
    m_blinkFid = 0xFFFFFFFF;
    {
        QMutexLocker locker(&m_writesMutex);
        m_writes.clear();
    }
    m_layer.clear();
    m_heads.clear();
    m_order.clear();
    m_all.clear();
    m_calls.clear();
    m_callIndex = -1;
    m_undo.clear();
    for (int s = 0; s < 2; ++s)
        m_stick[s][0] = m_stick[s][1] = 0.5f;
    m_trigger[0].reset();
    m_trigger[1].reset();
    if (m_flashTimer != nullptr)
        m_flashTimer->stop();
    m_flashClock.invalidate();
    if (m_breathTimer != nullptr)
        m_breathTimer->stop();
    m_breathClock.invalidate();
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
    flashSelection();
    emit announce(wherePhrase());
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
    const Call call = currentCall();
    if (isRunningCalls() && !call.targetName.isEmpty())
        return call.targetName;
    const Reference ref = isRunningCalls() ? call.target
                        : (m_useSecond && m_options.second.isValid())
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
    for (quint32 fid : m_all)
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
    if (m_holding)
        return;
    if (m_targetPage)
    {
        // Right is across the stage, up is upstage.
        nudgeTarget(dPan * targetMetresPerDegree(), dTilt * targetMetresPerDegree());
        return;
    }
    for (quint32 fid : scopeFixtures())
        nudgeHead(fid, dPan, dTilt);
}

void FocusSession::nudgeHead(quint32 fid, float dPan, float dTilt)
{
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
    if (m_targetPage)
    {
        keepTarget();
        return;
    }
    const int batch = ++m_batch;
    QList<quint32> saved;
    for (quint32 fid : scopeFixtures())
        if (saveHead(fid, batch))
            saved << fid;
    savedChanged(saved);
    // "Saved fixture 1." -- or how many, for a whole call (the phrase table).
    if (saved.size() == 1)
        emit announce(ControllerScheme::savedPhrase(m_voiceDetail, voiceIndex(saved.first()),
                                                    lightName(saved.first()), savedVenue()),
                      ControllerScheme::ConfirmPriority);
    else if (!saved.isEmpty())
        emit announce(ControllerScheme::savedSomePhrase(m_voiceDetail, int(saved.size()), savedVenue()),
                      ControllerScheme::ConfirmPriority);
}

FocusSession::SaveStatus FocusSession::saveStatus() const
{
    const QList<quint32> unsaved = unsavedFixtures();
    const bool here = unsaved.contains(currentFixture());
    const bool elsewhere = unsaved.size() > (here ? 1 : 0);
    return elsewhere ? (here ? BothUnsaved : OthersUnsaved) : here ? ThisUnsaved : AllSaved;
}

void FocusSession::saveAllHeld()
{
    if (!m_active)
        return;
    saveAll();
    /* Confirmed, every time (Branson's test, 2026-10-25): "All saved, 6
       lights." -- every light worked here is saved now, said with the
       venue they are saved to. Said and shown on the panel alike. */
    emit announce(saveAllPhrase(), ControllerScheme::ConfirmPriority);
}

void FocusSession::revertAllHeld()
{
    if (!m_active)
        return;
    QList<quint32> unsaved = unsavedFixtures();
    // B's press, just before the hold, put this light back already.
    const QList<UndoEntry> pressed = m_pressReverted;
    m_pressReverted.clear();
    QList<quint32> back;
    for (const UndoEntry &e : pressed)
        back << e.fid;
    if (unsaved.isEmpty() && back.isEmpty())
    {
        emit announce(tr("Nothing to put back: all saved."), ControllerScheme::ConfirmPriority);
        return;
    }
    // One step: View brings every one of them back, unsaved, as it was.
    const int batch = ++m_batch;
    for (UndoEntry e : pressed)
    {
        e.batch = batch;
        m_undo.append(e);
    }
    for (quint32 fid : unsaved)
        if (!back.contains(fid))
            back << fid;
    unsaved = back;
    for (quint32 fid : unsavedFixtures())
    {
        Head &h = m_heads[fid];
        m_undo.append({ fid, h.saved, m_checked.contains(fid), h.landSaved, h.savedIsOriginal, h.lensSaved,
                        batch, true, h.live, h.first, h.lensLive });
        backToSaved(fid, h);
        emit offsetChanged(fid);
    }
    rebuildWrites();
    if (unsaved.size() == 1)
        emit announce(ControllerScheme::revertedPhrase(m_voiceDetail, voiceIndex(unsaved.first()),
                                                       lightName(unsaved.first())),
                      ControllerScheme::ConfirmPriority);
    else
        emit announce(ControllerScheme::revertedAllPhrase(m_voiceDetail, int(unsaved.size())),
                      ControllerScheme::ConfirmPriority);
}

void FocusSession::revertLight()
{
    const quint32 fid = currentFixture();
    if (!m_heads.contains(fid))
        return;
    Head &h = m_heads[fid];
    if (h.dirty())
    {
        backToSaved(fid, h);
        rebuildWrites();
        emit offsetChanged(fid);
    }
    if (m_doc->fixture(fid) != nullptr)
        emit announce(ControllerScheme::revertedPhrase(m_voiceDetail, voiceIndex(fid), lightName(fid)),
                      ControllerScheme::ConfirmPriority);
}

bool FocusSession::nextUnsaved(quint32 after)
{
    if (!m_active)
        return false;
    const QList<quint32> unsaved = unsavedFixtures();
    if (unsaved.isEmpty())
    {
        emit announce(tr("All saved."), ControllerScheme::ConfirmPriority);
        return false;
    }
    // After the head we are on, round to the start: in call order.
    const int from = m_all.indexOf(after != 0xFFFFFFFF ? after : currentFixture());
    quint32 to = unsaved.first();
    for (int i = 1; i <= m_all.size(); ++i)
    {
        const quint32 fid = m_all.at((from + i) % m_all.size());
        if (unsaved.contains(fid))
        {
            to = fid;
            break;
        }
    }
    if (isRunningCalls() && !m_order.contains(to))
        for (int c = 0; c < m_calls.size(); ++c)
            if (m_calls.at(c).order.contains(to))
            {
                enterCall(c);
                break;
            }
    m_scope = FixtureScope;
    setIndex(m_order.indexOf(to));
    if (const Fixture *fx = m_doc->fixture(to))
        emit announce(tr("%1, not saved.").arg(fx->name()), ControllerScheme::ConfirmPriority);
    return true;
}

bool FocusSession::savedGhost(quint32 fid, QVector3D &point) const
{
    if (!m_active || fid != currentFixture() || !m_heads.contains(fid))
        return false;
    const Head &h = m_heads[fid];
    if (h.fixed)
    {
        if (h.first == h.landSaved)
            return false;
        point = h.landSaved;
        return true;
    }
    if (h.live == h.saved)
        return false;
    FixtureRigProps rp;
    double pan = 0.0, tilt = 0.0;
    QVector3D target;
    if (!intendedCentred(fid, rp, pan, tilt) || !referencePoint(fid, target))
        return false;
    /* The live offsets land the head on the reference; with the saved ones
       its DMX would be that much the other way. */
    QVector3D dir;
    if (!AimSolver::beamDirection(rp, pan + double(h.saved.pan - h.live.pan),
                                  tilt + double(h.saved.tilt - h.live.tilt), dir))
        return false;
    const QVector3D from = m_doc->monitorProperties()->fixtureRigPosition(fid);
    if (qAbs(dir.z()) < 1e-4f)
        return false;
    const float t = (target.z() - from.z()) / dir.z();
    if (t <= 0.0f)
        return false;
    point = from + dir * t;
    return true;
}

QString FocusSession::deltaText(quint32 fid) const
{
    if (!m_heads.contains(fid))
        return QString();
    const Head &h = m_heads[fid];
    auto sign = [](double v, int places) {
        // A real minus sign, as the panel's numbers have.
        return (v < 0.0 ? QStringLiteral("\u2212") : QStringLiteral("+"))
               + QString::number(qAbs(v), 'f', places);
    };
    QStringList parts;
    if (h.fixed)
    {
        /* In the show's units: a show laid out in feet reads a nudge in
           inches (to the half inch), "4½\" stage left", "1' 2\" upstage". */
        const MonitorProperties *props = m_doc->monitorProperties();
        const bool feet = props != nullptr && props->gridUnits() == MonitorProperties::Feet;
        auto distance = [feet](float metres) {
            const double v = qAbs(double(metres));
            if (!feet)
                return tr("%1 m").arg(v, 0, 'f', 2);
            const double halves = std::round(v / 0.0254 * 2.0);
            if (halves >= 24.0)
                return MonitorProperties::formatStageDistance(float(v), MonitorProperties::Feet);
            const int whole = int(halves) / 2;
            const bool half = (int(halves) % 2) != 0;
            return QStringLiteral("%1%2\"").arg(whole > 0 || !half ? QString::number(whole) : QString(),
                                                 half ? QStringLiteral("\u00BD") : QString());
        };
        const QVector3D d = h.first - h.landSaved;
        if (qAbs(d.x()) >= 0.005f)
            parts << tr("%1 %2").arg(distance(d.x()),
                                     d.x() > 0 ? tr("stage left") : tr("stage right"));
        if (qAbs(d.y()) >= 0.005f)
            parts << tr("%1 %2").arg(distance(d.y()),
                                     d.y() > 0 ? tr("downstage") : tr("upstage"));
        return parts.join(QStringLiteral(", "));
    }
    const float dp = h.live.pan - h.saved.pan, dt = h.live.tilt - h.saved.tilt;
    if (qAbs(dp) >= 0.005f)
        parts << tr("%1\u00b0 pan").arg(sign(dp, qAbs(dp) < 10.0f ? 1 : 0));
    if (qAbs(dt) >= 0.005f)
        parts << tr("%1\u00b0 tilt").arg(sign(dt, qAbs(dt) < 10.0f ? 1 : 0));
    return parts.join(QStringLiteral(", "));
}

bool FocusSession::saveHead(quint32 fid, int batch)
{
    if (!m_heads.contains(fid))
        return false;
    Head &h = m_heads[fid];
    m_undo.append({ fid, h.saved, m_checked.contains(fid), h.landSaved, h.savedIsOriginal, h.lensSaved, batch,
                    false, h.live, h.first, h.lensLive });
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
    /* Kept: the real light says so, dipping to dark and back twice. */
    if (m_rigFeedback)
    {
        m_blinkFid = fid;
        m_blinkPhase = 1;
        if (m_blinkTimed)
            m_blinkTimer.start();
        rebuildWrites();
    }
    emit offsetChanged(fid);
    emit headSaved(fid);
    return true;
}

void FocusSession::advanceBlink()
{
    if (m_blinkPhase <= 0)
    {
        m_blinkTimer.stop();
        return;
    }
    if (++m_blinkPhase > kBlinkPhases)
    {
        stopBlink();
        return;
    }
    rebuildWrites();
}

void FocusSession::stopBlink()
{
    m_blinkTimer.stop();
    if (m_blinkPhase == 0)
        return;
    m_blinkPhase = 0;
    m_blinkFid = 0xFFFFFFFF;
    rebuildWrites();
}

void FocusSession::setRigFeedback(bool on)
{
    m_rigFeedback = on;
    if (!on)
        stopBlink();
}

void FocusSession::checkUnsaved()
{
    const QList<quint32> now = unsavedFixtures();
    if (now == m_lastUnsaved)
        return;
    m_lastUnsaved = now;
    emit unsavedChanged();
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
    for (quint32 fid : scopeFixtures())
        nudgeBeamOf(fid, dDeg);
}

void FocusSession::nudgeBeamOf(quint32 fid, float dDeg)
{
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
    for (quint32 fid : scopeFixtures())
        nudgeFocusOf(fid, d);
}

void FocusSession::nudgeFocusOf(quint32 fid, float d)
{
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
    if (m_trigger[side].update(qBound(0.0f, norm, 1.0f)))
        apply(side == 0 ? PreviousCall : NextCall);
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
    if (m_targetPage)
    {
        revertTarget();
        return;
    }
    bool any = false;
    m_pressReverted.clear();
    for (quint32 fid : scopeFixtures())
    {
        if (!m_heads.contains(fid))
            continue;
        Head &h = m_heads[fid];
        if (!h.dirty())
            continue;
        m_pressReverted.append({ fid, h.saved, m_checked.contains(fid), h.landSaved, h.savedIsOriginal,
                                 h.lensSaved, 0, true, h.live, h.first, h.lensLive });
        backToSaved(fid, h);
        any = true;
        emit offsetChanged(fid);
    }
    if (!any)
        return;
    rebuildWrites();
    emit announce(m_scope == GroupScope ? tr("Reverted.")
                  : ControllerScheme::revertedPhrase(m_voiceDetail, voiceIndex(currentFixture()),
                                                     lightName(currentFixture())),
                  ControllerScheme::ConfirmPriority);
}

void FocusSession::zero()
{
    bool any = false;
    for (quint32 fid : scopeFixtures())
    {
        if (!m_heads.contains(fid))
            continue;
        Head &h = m_heads[fid];
        if (h.fixed)
        {
            // As it pointed when the session began.
            if (!h.dirty())
                continue;
            backToSaved(fid, h);
        }
        else
        {
            /* In a venue, X is the show's own calibration -- what the venue
               corrects from -- not absolute zero (the review). */
            Offset base;
            if (savesToVenue())
            {
                const RigCalibration c = m_venues->baseOf(fid);
                base.pan = c.panOffsetDeg;
                base.tilt = c.tiltOffsetDeg;
            }
            if (h.live == base)
                continue;
            h.live = base;
            applyOffset(fid, h.live);
        }
        any = true;
        emit offsetChanged(fid);
    }
    if (!any)
        return;
    rebuildWrites();
    emit announce(savesToVenue() ? tr("Back to the show's calibration.") : tr("Zeroed."),
                  ControllerScheme::ConfirmPriority);
}

bool FocusSession::undoSave()
{
    if (m_undo.isEmpty())
        return false;
    // A save of a whole group is undone as one.
    const int batch = m_undo.last().batch;
    QList<quint32> undone;
    bool reverted = false;
    while (!m_undo.isEmpty() && m_undo.last().batch == batch)
    {
        const UndoEntry e = m_undo.takeLast();
        if (!m_heads.contains(e.fid))
            continue;
        Head &h = m_heads[e.fid];
        if (e.revert)
        {
            // Hold B undone: the unsaved change back, still unsaved.
            reverted = true;
            if (h.beam == LensSetByHand && h.lensLive != e.lensLiveBefore)
            {
                h.lensLive = e.lensLiveBefore;
                applyLens(e.fid, h.lensLive);
            }
            if (h.fixed)
            {
                h.first = e.firstBefore;
                applyLanding(e.fid, h, false);
            }
            else
            {
                h.live = e.liveBefore;
                applyOffset(e.fid, h.live);
            }
            emit offsetChanged(e.fid);
            continue;
        }
        h.saved = e.before;
        h.live = e.before;
        h.landSaved = e.landBefore;
        h.savedIsOriginal = e.wasOriginal;
        h.lensSaved = e.lensBefore;
        if (!e.wasChecked)
            m_checked.remove(e.fid);
        backToSaved(e.fid, h);
        undone << e.fid;
        emit offsetChanged(e.fid);
    }
    if (reverted)
    {
        rebuildWrites();
        emit announce(tr("Undone: the changes are back, not saved."), ControllerScheme::ConfirmPriority);
        return true;
    }
    if (undone.isEmpty())
        return false;
    savedChanged(undone, false);
    rebuildWrites();
    emit announce(tr("Undone."), ControllerScheme::ConfirmPriority);
    return true;
}

void FocusSession::saveAll()
{
    QList<quint32> saved;
    for (quint32 fid : m_all)
    {
        if (!m_heads.value(fid).dirty())
            continue;
        // One undo step each, as before.
        if (saveHead(fid, ++m_batch))
            saved << fid;
    }
    savedChanged(saved);
}

QString FocusSession::saveAllPhrase() const
{
    return ControllerScheme::savedAllPhrase(m_voiceDetail, int(m_all.size()), savedVenue());
}

QString FocusSession::savedVenue() const
{
    return savesToVenue() ? m_venues->activeVenue() : QString();
}

int FocusSession::voiceIndex(quint32 fid) const
{
    return int(m_order.indexOf(fid)) + 1;
}

QString FocusSession::lightName(quint32 fid) const
{
    const Fixture *fx = m_doc->fixture(fid);
    return fx != nullptr ? fx->name() : QString();
}

QString FocusSession::unsavedCountPhrase() const
{
    const int n = unsavedFixtures().size();
    return n == 0 ? tr("All saved.") : tr("%1 not saved.").arg(n);
}

bool FocusSession::targetMoved() const
{
    if (!m_targetPage || m_doc == nullptr)
        return false;
    const StageTarget *t = m_doc->monitorProperties()->stageTarget(m_targetId);
    return t != nullptr && t->position() != m_targetKept;
}

bool FocusSession::targetBlocks()
{
    if (!targetMoved())
        return false;
    const StageTarget *t = m_doc->monitorProperties()->stageTarget(m_targetId);
    emit announce(tr("%1 moved: A keeps it, B puts it back.").arg(t->name()),
                  ControllerScheme::ImportantPriority);
    return true;
}

bool FocusSession::toggleTargetPage()
{
    if (!m_active)
        return false;
    if (m_targetPage)
    {
        if (targetBlocks())
            return false;
        m_targetPage = false;
        emit referenceChanged();
        emit announce(tr("Target page off: the stick nudges the light."), ControllerScheme::ConfirmPriority);
        return true;
    }
    const Call c = currentCall();
    const StageTarget *t = (isRunningCalls() && c.target.kind == Reference::Target)
                           ? m_doc->monitorProperties()->stageTarget(c.target.id) : nullptr;
    if (t == nullptr)
    {
        emit announce(tr("This call has no target to move."), ControllerScheme::ImportantPriority);
        return false;
    }
    m_targetPage = true;
    m_targetId = t->id();
    m_targetKept = t->position();
    emit referenceChanged();
    emit announce(tr("Target page: the stick moves %1. A keeps it, B puts it back.").arg(t->name()),
                  ControllerScheme::ConfirmPriority);
    return true;
}

void FocusSession::reresolveCall()
{
    if (m_callIndex < 0 || m_callIndex >= m_calls.size())
        return;
    CallState &cs = m_calls[m_callIndex];
    const QHash<quint32, QVector3D> pts = resolve(cs.call.target);
    for (quint32 fid : cs.order)
    {
        Head &h = m_heads[fid];
        if (h.fixed || !pts.contains(fid))
            continue;
        cs.points.insert(fid, pts.value(fid));
        h.first = pts.value(fid);
    }
    rebuildWrites();
    emit referenceChanged();
}

void FocusSession::nudgeTarget(float dx, float dy)
{
    StageTarget *t = m_targetPage ? m_doc->monitorProperties()->stageTarget(m_targetId) : nullptr;
    if (t == nullptr || (dx == 0.0f && dy == 0.0f))
        return;
    t->setPosition(t->position() + QVector3D(dx, dy, 0.0f));
    // Every head aimed at it follows.
    reresolveCall();
}

bool FocusSession::keepTarget()
{
    StageTarget *t = m_targetPage ? m_doc->monitorProperties()->stageTarget(m_targetId) : nullptr;
    if (t == nullptr)
        return false;
    QString where;
    if (m_venues != nullptr && m_venues->isActive())
    {
        // The room's: the venue file, never the show (Q128).
        if (!m_venues->recordTarget(t->id()))
            return false;
        where = m_venues->activeVenue();
    }
    else
        m_doc->setModified();
    m_targetKept = t->position();
    m_doc->monitorProperties()->notifyStageTargetsChanged();
    emit announce(where.isEmpty() ? tr("%1 kept in the show.").arg(t->name())
                                  : tr("%1 kept for %2.").arg(t->name(), where),
                  ControllerScheme::ConfirmPriority);
    return true;
}

void FocusSession::revertTarget()
{
    StageTarget *t = m_targetPage ? m_doc->monitorProperties()->stageTarget(m_targetId) : nullptr;
    if (t == nullptr)
        return;
    t->setPosition(m_targetKept);
    reresolveCall();
    emit announce(tr("%1 back.").arg(t->name()), ControllerScheme::ConfirmPriority);
}

void FocusSession::discardAll()
{
    // A target moved and not kept goes back too.
    if (m_targetPage)
    {
        if (StageTarget *t = m_doc->monitorProperties()->stageTarget(m_targetId))
            t->setPosition(m_targetKept);
        m_targetPage = false;
    }
    for (quint32 fid : m_all)
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
    // The one layout every on-stage tool shares (ControllerScheme).
    switch (ControllerScheme::verbFor(role))
    {
        case ControllerScheme::NextItem:      return Next;
        case ControllerScheme::PreviousItem:  return Previous;
        case ControllerScheme::NextGroup:     return NextCall;
        case ControllerScheme::PreviousGroup: return PreviousCall;
        case ControllerScheme::CycleScope:    return CycleScope;
        case ControllerScheme::Keep:          return Save;
        case ControllerScheme::Revert:        return Revert;
        case ControllerScheme::Reset:         return Zero;
        case ControllerScheme::Undo:          return UndoSave;
        case ControllerScheme::Exit:          return Exit;
        // Zoom and focus on the D-pad (Q103; which arm is ControllerScheme's).
        case ControllerScheme::ZoomWider:     return ZoomWider;
        case ControllerScheme::ZoomNarrower:  return ZoomNarrower;
        case ControllerScheme::FocusNearer:   return FocusNearer;
        case ControllerScheme::FocusFarther:  return FocusFarther;
        case ControllerScheme::Alternate:     return ToggleReference;
        case ControllerScheme::WhereAmI:      return WhereAmI;
        case ControllerScheme::NoVerb:        break;
    }
    return NoControl;
}

void FocusSession::apply(Control control)
{
    // Holding (the freeze chord): nothing moves until it is released.
    if (!m_active || m_holding)
        return;
    // Only a hold that follows B's press takes what the press put back.
    if (control != Revert)
        m_pressReverted.clear();
    switch (control)
    {
        // A bumper picks a fixture: from the whole group, that one.
        case Next:            if (m_scope == GroupScope) setScope(FixtureScope); else next(); break;
        case Previous:        if (m_scope == GroupScope) setScope(FixtureScope); else previous(); break;
        case NextCall:        if (!targetBlocks()) { m_targetPage = false; nextCall(); } break;
        case PreviousCall:    if (!targetBlocks()) { m_targetPage = false; previousCall(); } break;
        case CycleScope:      cycleScope(); break;
        case WhereAmI:        whereAmI(); break;
        case Save:            save(); break;
        case Revert:          revert(); break;
        case Zero:            zero(); break;
        // Focus calls: the target page (Q128); Focus mode: the second reference.
        case ToggleReference: isRunningCalls() ? void(toggleTargetPage()) : void(toggleReference()); break;
        case UndoSave:        if (!undoSave()) emit announce(tr("Nothing to undo."), ControllerScheme::ConfirmPriority); break;
        case Exit:
        {
            // "2 lights not saved." -- the panel then asks what to do.
            const int n = unsavedFixtures().size();
            if (n == 1)
            {
                const Fixture *fx = m_doc->fixture(unsavedFixtures().first());
                emit announce(fx != nullptr ? tr("%1 not saved.").arg(fx->name()) : tr("1 light not saved."), ControllerScheme::ImportantPriority);
            }
            else if (n > 1)
                emit announce(tr("%1 lights not saved.").arg(n), ControllerScheme::ImportantPriority);
            emit exitRequested();
            break;
        }
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

bool FocusSession::repeats(Control control)
{
    return control == ZoomWider || control == ZoomNarrower
           || control == FocusNearer || control == FocusFarther;
}

void FocusSession::press(Control control, bool down)
{
    if (!down)
    {
        m_hold.release(int(control));
        return;
    }
    m_hold.press(int(control), repeats(control));
    apply(control);
    // The crew's word for a beam press (Q140), once, where the light has it.
    const quint32 fid = currentFixture();
    const bool zoom = control == ZoomWider || control == ZoomNarrower;
    const bool edge = control == FocusNearer || control == FocusFarther;
    if ((zoom && beamKind(fid) != NoBeamControl) || (edge && hasFocusControl(fid)))
        emit announce(ControllerScheme::crewWord(zoom ? (control == ZoomWider ? ControllerScheme::ZoomWider
                                                                              : ControllerScheme::ZoomNarrower)
                                                      : (control == FocusFarther ? ControllerScheme::FocusFarther
                                                                                 : ControllerScheme::FocusNearer)));
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

void FocusSession::releaseControls()
{
    for (int s = 0; s < 2; ++s)
        m_stick[s][0] = m_stick[s][1] = 0.5f;
    m_carryPan = m_carryTilt = 0.0f;
    m_hold.reset();
    m_trigger[0].reset();
    m_trigger[1].reset();
}

void FocusSession::tick(double dt)
{
    if (!m_active || m_input == KeyboardInput || m_holding)
        return;
    // A held D-pad arm repeats (press()).
    for (int n = m_hold.due(dt); n > 0 && m_hold.held() >= 0; --n)
        apply(Control(m_hold.held()));
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
                else if (uchar open = 0; openBeamValue(ch, open))
                {
                    // Gobo, prism and frost out (Q140): the bare beam to focus.
                    put(fx, c, open);
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

        /* A save's blink, in its dark phase: every intensity the light
           has, colour mixing too, to zero (advanceBlink()). */
        auto blackout = [&](Fixture *fx) {
            QLCFixtureMode *mode = fx->fixtureMode();
            for (quint32 c = 0; c < fx->channels(); ++c)
                if (const QLCChannel *ch = mode->channel(c))
                    if (ch->group() == QLCChannel::Intensity)
                        put(fx, c, 0);
        };
        const bool blinkDark = m_blinkPhase > 0 && (m_blinkPhase % 2) == 1;

        /* What is selected is lit (blinking while it flashes); the rest of
           the call (list) is dark or dim, as chosen. */
        const QList<quint32> selected = scopeFixtures();
        const bool breathing = isBreathing();
        if (breathing && !m_breathClock.isValid())
            m_breathClock.start();
        else if (!breathing)
            m_breathClock.invalidate();
        if (m_breathTimer != nullptr && breathing != m_breathTimer->isActive())
            breathing ? m_breathTimer->start() : m_breathTimer->stop();
        const bool dark = m_flashClock.isValid()
                          && !(m_warnFlash ? warnLit(m_flashClock.elapsed()) : flashLit(m_flashClock.elapsed()));
        for (quint32 fid : m_order)
        {
            Fixture *fx = m_doc->fixture(fid);
            if (fx == nullptr || fx->fixtureMode() == nullptr || !fx->isPatched())
                continue;
            if (!selected.contains(fid))
            {
                if (m_options.others == OthersDark)
                    lightLevel(fx, 0.0f, false);
                else if (m_options.others == OthersDim)
                    lightLevel(fx, m_options.dimLevel, false);
                // Moved on mid-blink: it still finishes its blink.
                if (blinkDark && fid == m_blinkFid)
                    blackout(fx);
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
            float level = m_options.level;
            if (breathing && fid == currentFixture() && !isFlashing())
                level *= breathLevel(m_breathClock.elapsed());
            lightLevel(fx, dark ? 0.0f : level, true);
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
            if (blinkDark && fid == m_blinkFid)
                blackout(fx);
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
