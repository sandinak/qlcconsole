/*
  Q Light Controller Plus
  outputrulekeeper.cpp

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
#include <QTimer>
#include <QThread>
#include <QHash>
#include <QSet>
#include <algorithm>

#include "outputrulekeeper.h"
#include "inputoutputmap.h"
#include "fixturerest.h"
#include "qlcchannel.h"
#include "qlcfixturedef.h"
#include "universe.h"
#include "function.h"
#include "mastertimer.h"
#include "lastlookeffect.h"
#include "genericfader.h"
#include "outputlayer.h"
#include "fadechannel.h"
#include "scenery.h"
#include "programmercontroller.h"

#define KXMLQLCOutputRuleOnAtOpen QStringLiteral("OnAtOpen")

OutputRuleKeeper::OutputRuleKeeper(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
    , m_machineLayer(new OutputLayer(doc, Universe::Park, FadeChannel::Override, QStringLiteral("Curtain stopped")))
    , m_editLayer(new OutputLayer(doc, Universe::Park, FadeChannel::Override, QStringLiteral("Curtain held while editing")))
    , m_backLayer(new OutputLayer(doc, Universe::Park, FadeChannel::Override, QStringLiteral("One-shot held after Back")))
    , m_hazardLayer(new OutputLayer(doc, Universe::Park, FadeChannel::Override | FadeChannel::PastGM,
                                    QStringLiteral("Hazard held: needs arming")))
{
    // Park wins over every hold here (board Q351).
    if (doc->programmer() != nullptr)
        connect(doc->programmer(), &ProgrammerController::parkChanged, this, &OutputRuleKeeper::parkChanged);
    /* A Show-only fixture added, moved or (re)patched -- mid-show too: its
       new address is held at once, not after the event loop comes round
       (one frame there could fire it). Anything else waits, coalesced. */
    auto changed = [this](quint32 id) {
        markNow(id);
        scheduleRebuild();      // the rest, once, coalesced
    };
    connect(doc, &Doc::fixtureAdded, this, changed);
    connect(doc, &Doc::fixtureRemoved, this, [this](quint32) { scheduleRebuild(); });
    connect(doc, &Doc::fixtureChanged, this, changed);
    connect(doc, &Doc::loaded, this, [this]() {
        // E2 (R6-7): a show loaded -- over the web while in Operate too --
        // starts a new session: nothing it brought in is the show.
        startSession();
        scheduleRebuild();
    });
    // Another show: a new show's setting, and OnAtOperate waits for its
    // own first Operate.
    connect(doc, &Doc::cleared, this, [this]() {
        // Another show: its own start, no emergency stop carried over, no
        // automatic widget armed.
        const bool wasStopped = m_stopEpoch.exchange(0, std::memory_order_relaxed) != 0;
        releaseStageMachinery();
        releaseMachineryForEdit();
        {
            QMutexLocker locker(&m_machineMutex);
            m_backHeld.clear();
            ++m_backGen;
        }
        m_backLayer->clear();
        m_editLayer->clear();
        m_hazardsRefused.clear();
        Function::clearAutoSources();
        startSession();
        m_onAtOpenWhen = defaultOnAtOpenWhen();
        m_seenOperate = m_doc->mode() == Doc::Operate;
        scheduleRebuild();
        if (wasStopped)
            emit emergencyStopEnded();
    });
    connect(doc, &Doc::modeChanged, this, [this](Doc::Mode mode) {
        /* Show only, Operate starting: a new session -- what was started
           or set in Design is not the show until a GO in Operate -- with
           every running function's gate worked out now, and only then the
           universes live. Before anything else hears of Operate. */
        if (mode == Doc::Operate)
            startSession();
        if (!m_hazardsRefused.isEmpty())
        {
            m_hazardsRefused.clear();
            emitMachineryChanged();
        }
        pushShowOnlyLive();
        if (mode == Doc::Operate && m_seenOperate == false)
        {
            m_seenOperate = true;
            pushPolicy();
        }
    });
    connect(doc->inputOutputMap(), &InputOutputMap::outputInhibitedChanged, this, [this](bool) {
        pushShowOnlyLive();     // Blind holds them: the chip says so
    });
    /* R7-4: Blind ending in Operate starts a new session, before anything
       is sent again: a GO taken in Blind was checked on screen, and fires
       only when GO'd again. */
    connect(doc->inputOutputMap(), &InputOutputMap::outputResuming, this, [this]() {
        if (m_doc->mode() == Doc::Operate)
            startSession();
    });
    connect(doc->inputOutputMap(), &InputOutputMap::universeAdded, this, [this](quint32) { scheduleRebuild(); });
    connect(doc->inputOutputMap(), &InputOutputMap::universeRemoved, this, [this](quint32) { scheduleRebuild(); });
    m_seenOperate = doc->mode() == Doc::Operate;
}

OutputRuleKeeper::~OutputRuleKeeper()
{
}

void OutputRuleKeeper::startSession()
{
    // E2: a new live session, with every running function's gate worked
    // out now, between two ticks.
    Function::startLiveSession();
    if (m_doc->masterTimer() != nullptr)
        m_doc->masterTimer()->refreshPreviewGates();
    // A stop from before is over with it (emergencyStopped() reads false now).
    if (m_stopEpoch.exchange(0, std::memory_order_relaxed) != 0)
        emit emergencyStopEnded();
}

void OutputRuleKeeper::markNow(quint32 id)
{
    /* A Show-only fixture added, moved, patched or given the rule -- mid-
       show too: its channels are held where it is now, at once, channel
       by channel (a move that changes universe and address is two changes:
       each is cheap, and the full rebuild runs once after). */
    const Fixture *fx = m_doc->fixture(id);
    const bool showOnly = fx != nullptr && fx->isPatched() && (fx->outputRules() & Fixture::ShowOnly);
    const auto was = m_showOnlyAt.constFind(id);
    if (!showOnly && was == m_showOnlyAt.constEnd())
        return;
    const QList<Universe *> ua = m_doc->inputOutputMap()->universes();
    /* The first Show-only fixture: every gate worked out for real before
       its channels are marked, so not even the first frame lets a preview
       drive it. */
    if (showOnly && m_showOnly.isEmpty() && m_showOnlyAt.isEmpty())
    {
        Function::setShowOnlyInUse(true);
        if (m_doc->masterTimer() != nullptr)
            m_doc->masterTimer()->refreshPreviewGates();
    }
    /* N2 (outputpatch.h): a hardware output of a universe this marks is
       held once every channel is marked -- its closing frame has them all
       at safe. */
    InputOutputMap::ShowOnlyMarks marks(m_doc->inputOutputMap());
    QSet<quint32> now;
    if (showOnly)
        for (quint32 i = 0; i < fx->channels(); i++)
            now.insert(fx->universeAddress() + i);
    if (was != m_showOnlyAt.constEnd())
    {
        for (quint32 i = 0; i < was.value().second; i++)
        {
            const quint32 abs = was.value().first + i;
            const int u = int(abs >> 9);
            if (!now.contains(abs) && u < ua.size() && ua.at(u) != nullptr)
            {
                ua.at(u)->markShowOnly(int(abs & 0x1ff), false, 0);
                ua.at(u)->setShowOnlyHoldsPosition(int(abs & 0x1ff), false);
            }
        }
    }
    if (showOnly)
    {
        /* R8-3: moved (by any number of channels) or re-moded: a fresh mark
           on all its channels, those that were Show only already too -- a
           writer from before the move counts nowhere it lands. */
        const bool moved = was != m_showOnlyAt.constEnd()
                           && (was.value().first != fx->universeAddress()
                               || was.value().second != fx->channels()
                               || m_showOnlyMode.value(id) != fx->fixtureMode());
        for (quint32 i = 0; i < fx->channels(); i++)
        {
            const quint32 abs = fx->universeAddress() + i;
            const int u = int(abs >> 9);
            if (u < ua.size() && ua.at(u) != nullptr)
            {
                ua.at(u)->markShowOnly(int(abs & 0x1ff), true, safeValue(fx, i), moved);
                ua.at(u)->setShowOnlyHoldsPosition(int(abs & 0x1ff), FixtureRest::holdsPosition(fx->channel(i))
                                                                       || FixtureRest::freezesOnStop(fx));
            }
        }
        m_showOnlyAt.insert(id, qMakePair(fx->universeAddress(), fx->channels()));
        m_showOnlyMode.insert(id, fx->fixtureMode());
    }
    else
    {
        m_showOnlyAt.remove(id);
        m_showOnlyMode.remove(id);
    }
}

void OutputRuleKeeper::scheduleRebuild()
{
    /* One timer, not a singleShot a call: a rebuild() run straight (a
       test, rebuildNow) used to leave its queued one behind, and the next
       change queued another -- several rebuilds for one change. */
    if (m_rebuildTimer == nullptr)
    {
        m_rebuildTimer = new QTimer(this);
        m_rebuildTimer->setSingleShot(true);
        m_rebuildTimer->setInterval(0);
        connect(m_rebuildTimer, &QTimer::timeout, this, [this]() { rebuild(); });
    }
    m_pending = true;
    if (!m_rebuildTimer->isActive())
        m_rebuildTimer->start();
}

void OutputRuleKeeper::setOnAtOpenWhen(OnAtOpenWhen when)
{
    if (when == m_onAtOpenWhen)
        return;
    m_onAtOpenWhen = when;
    m_doc->setModified();
    pushPolicy();
}

void OutputRuleKeeper::saveXML(QXmlStreamWriter *doc) const
{
    // Only when it differs from a new show's: shows that never touched it
    // save as they always did.
    if (m_onAtOpenWhen == defaultOnAtOpenWhen())
        return;
    doc->writeStartElement(KXMLQLCOutputRuleSettings);
    doc->writeAttribute(KXMLQLCOutputRuleOnAtOpen, m_onAtOpenWhen == OnAtShowOpen
                                                       ? QStringLiteral("ShowOpen")
                                                       : QStringLiteral("Operate"));
    doc->writeEndElement();
}

void OutputRuleKeeper::loadXML(QXmlStreamReader &root)
{
    const QString v = root.attributes().value(KXMLQLCOutputRuleOnAtOpen).toString();
    root.skipCurrentElement();
    m_onAtOpenWhen = v == QLatin1String("ShowOpen") ? OnAtShowOpen : OnAtOperate;
    pushPolicy();
}

bool OutputRuleKeeper::onAtOpenArmed() const
{
    switch (m_onAtOpenWhen)
    {
        case OnAtOperate:   return m_seenOperate;
        case OnAtShowOpen:  return true;
    }
    return false;
}

void OutputRuleKeeper::setBlindHoldsExempt(bool hold)
{
    if (hold == m_blindHoldsExempt)
        return;
    m_blindHoldsExempt = hold;
    pushPolicy();
}

void OutputRuleKeeper::pushPolicy()
{
    InputOutputMap *io = m_doc->inputOutputMap();
    const bool armed = onAtOpenArmed();
    QList<Universe *> ua = io->claimUniverses();
    foreach (Universe *u, ua)
    {
        if (u == nullptr)
            continue;
        u->setOnAtOpenArmed(armed);
        u->setBlindHoldsExempt(m_blindHoldsExempt);
    }
    io->releaseUniverses(false);
}

QList<quint32> OutputRuleKeeper::coveredChannels(const Fixture *fx, Fixture::OutputRule rule)
{
    QList<quint32> all, intensity;
    if (fx == nullptr)
        return all;
    for (quint32 i = 0; i < fx->channels(); i++)
    {
        all << i;
        const QLCChannel *ch = fx->channel(i);
        if (ch != nullptr && ch->group() == QLCChannel::Intensity)
            intensity << i;
    }
    if (rule == Fixture::ExemptBlackout || rule == Fixture::ExemptGrandMaster
        || rule == Fixture::ShowOnly)
        return all;
    return intensity.isEmpty() ? all : intensity;
}

QByteArray OutputRuleKeeper::bitsFor(quint32 universe) const
{
    QByteArray bits(UNIVERSE_SIZE, char(0));
    foreach (Fixture *fx, m_doc->fixtures())
    {
        const Fixture::OutputRules rules = fx->outputRules();
        if (rules == Fixture::OutputRules() || fx->isPatched() == false)
            continue;

        auto mark = [&bits, fx, universe](Fixture::OutputRule rule, char bit) {
            foreach (quint32 i, coveredChannels(fx, rule))
            {
                const quint32 abs = fx->universeAddress() + i;   // runs on past 512 when cross-universe
                if ((abs >> 9) == universe)
                    bits[int(abs & 0x1ff)] = char(bits.at(int(abs & 0x1ff)) | bit);
            }
        };
        if (rules & Fixture::ExemptGrandMaster)
            mark(Fixture::ExemptGrandMaster, char(Universe::GMExempt));
        // Blackout always wins over Show only: Ignore Blackout does not
        // apply to a Show-only fixture.
        if ((rules & Fixture::ExemptBlackout) && (rules & Fixture::ShowOnly) == 0)
            mark(Fixture::ExemptBlackout, char(Universe::BlackoutExempt));
        if (rules & Fixture::Switch)
            mark(Fixture::Switch, char(Universe::SwitchOutput));
        // Show only wins over On when not in use: "full when nothing sets
        // it" would drive a laser or a flame with no GO at all.
        if ((rules & Fixture::OnAtOpen) && (rules & Fixture::ShowOnly) == 0)
            mark(Fixture::OnAtOpen, char(Universe::OnAtOpenOutput));
        if (rules & Fixture::ShowOnly)
            mark(Fixture::ShowOnly, char(Universe::ShowOnlyOutput));
    }
    /* R8-2: a channel a Show-only fixture shares with another (patched over
       it): Show only wins there too -- never on when not in use, never past
       Blackout, whichever fixture asked. */
    for (int c = 0; c < UNIVERSE_SIZE; c++)
    {
        const uchar b = uchar(bits.at(c));
        if (b & Universe::ShowOnlyOutput)
            bits[c] = char(b & ~(Universe::OnAtOpenOutput | Universe::BlackoutExempt));
    }
    return bits;
}

uchar OutputRuleKeeper::safeValue(const Fixture *fx, quint32 channel)
{
    const QLCChannel *ch = fx != nullptr ? fx->channel(channel) : nullptr;
    if (ch == nullptr)
        return 0;
    // Dark: a relay open, a dimmer at 0, whatever its definition's default.
    if (ch->group() == QLCChannel::Intensity)
        return 0;
    // A fixture with no definition (a generic dimmer pack, a relay) has no
    // groups to go by: every channel of it is its output.
    if (fx->fixtureDef() == nullptr || fx->fixtureDef()->type() == QLCFixtureDef::Dimmer)
        return 0;
    return FixtureRest::restValue(fx, channel);
}

QByteArray OutputRuleKeeper::safeValuesFor(quint32 universe) const
{
    QByteArray safe(UNIVERSE_SIZE, char(0));
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if ((fx->outputRules() & Fixture::ShowOnly) == 0 || fx->isPatched() == false)
            continue;
        for (quint32 i = 0; i < fx->channels(); i++)
        {
            const quint32 abs = fx->universeAddress() + i;
            if ((abs >> 9) == universe)
                safe[int(abs & 0x1ff)] = char(safeValue(fx, i));
        }
    }
    return safe;
}

QByteArray OutputRuleKeeper::positionsFor(quint32 universe) const
{
    // Q326: the one rule the Simple Desk and the VC sliders keep too. A
    // curtain is held whole (FixtureRest::freezesOnStop()).
    QByteArray pos(UNIVERSE_SIZE, char(0));
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if ((fx->outputRules() & Fixture::ShowOnly) == 0 || fx->isPatched() == false)
            continue;
        const bool machine = FixtureRest::freezesOnStop(fx);
        for (quint32 i = 0; i < fx->channels(); i++)
        {
            const quint32 abs = fx->universeAddress() + i;
            if ((abs >> 9) == universe && (machine || FixtureRest::holdsPosition(fx->channel(i))))
                pos[int(abs & 0x1ff)] = char(1);
        }
    }
    return pos;
}

void OutputRuleKeeper::emergencyStop()
{
    /* Q326: a Show-only head stays where it is -- its pan and tilt held at
       what went out last, before the new session, so no frame between
       sends it to its safe value. Only the rest goes safe. A curtain
       stops where it is, mid-move too: it could be dragging a set piece. */
    FixtureRest::holdShowOnlyHeads(m_doc);
    // E2: a new live session -- every stamp from before is old now, so
    // every writer is held from its next write on (a function's gate at
    // its next tick: the generation moved).
    Function::startLiveSession();
    // Read before the cut below, stamped in the new session.
    holdStageMachinery();
    const quint32 session = GenericFader::liveEpoch();
    m_stopEpoch.store(session, std::memory_order_relaxed);
    // E1: the cut first -- stored values to safe and a frame sent, now,
    // waiting on no function and no list.
    const QList<Universe *> ua = m_doc->inputOutputMap()->universesSnapshot();
    for (Universe *u : ua)
    {
        if (u == nullptr || u->showOnlyChannelCount() == 0)
            continue;
        u->cutShowOnly();
        // Always sent, Blind or not (R8-6): the Show-only channels at safe,
        // the rest as they are (frozen in Blind).
        u->sendHeld();
    }
    // E1, E5: then every function stopped, queued ones too; each is held
    // at once (no source left), so its fade-out is kept off. The Doc's
    // function list is the main thread's: from another thread (an OSC or
    // MSC plugin's) this part is queued there -- the cut is already out.
    auto stopAll = [this]() {
        for (Function *f : m_doc->functions())
            if (f != nullptr && (f->isRunning() || f->sources().isEmpty() == false))
                f->stop(FunctionParent::master());
        // The gates of whatever is still running, worked out as a tick would.
        if (m_doc->masterTimer() != nullptr)
            m_doc->masterTimer()->refreshPreviewGates();
        /* Then every fixture at rest, as Stop all does (one path:
           FixtureRest::restAfterStop()): a fogger or a WLED effect not
           marked Show only kept the last value its cue sent (UX-70). Here,
           on the main thread; it takes the universes' frame locks as a patch
           does and never waits for the MasterTimer. */
        FixtureRest::restAfterStop(m_doc);
    };
    if (thread() == QThread::currentThread())
        stopAll();
    else
        QMetaObject::invokeMethod(this, stopAll, Qt::QueuedConnection);
    // E7: only the footer and the Simple Desk hear of it later; they check
    // the session they were told of.
    emit emergencyStopStruck(session);
    QMetaObject::invokeMethod(this, [this]() {
        m_lastHeldBy = showOnlyHeldBy();
        emit showOnlyChanged();
    }, thread() == QThread::currentThread() ? Qt::DirectConnection : Qt::QueuedConnection);
}

bool OutputRuleKeeper::emergencyStopped() const
{
    const quint32 stop = m_stopEpoch.load(std::memory_order_relaxed);
    return stop != 0 && stop == GenericFader::liveEpoch();
}

void OutputRuleKeeper::noteGo()
{
    /* What the footer says only: the first GO after the stop ends "held:
       emergency stop". It lets nothing go (E4) -- the rule holds what that
       GO does not drive. An automatic writer's (an audio bar stepping a cue
       list: a StampScope) is no GO at all. */
    if (GenericFader::stampOverridden())
        return;
    /* The curtains a stop held carry on with what this GO plays. A few
       frames later, so the cue it starts writes first: let go before it,
       a curtain on a relay (intensity) would drop for a frame. A stop in
       between holds them again, and that hold stays. */
    quint32 gen = 0, backGen = 0;
    {
        QMutexLocker locker(&m_machineMutex);
        gen = m_machineHeld.isEmpty() ? 0 : m_machineGen;
        backGen = m_backHeld.isEmpty() ? 0 : m_backGen;
    }
    if (gen != 0)
        QMetaObject::invokeMethod(this, [this, gen]() {
            QTimer::singleShot(4 * int(MasterTimer::tick()), this, [this, gen]() {
                {
                    QMutexLocker locker(&m_machineMutex);
                    if (m_machineGen != gen)
                        return;
                }
                /* An edit's let-go still gliding when the stop came (the
                   053p review): the GO drops what is left of it, so the
                   cue owns the fixture from where the stop froze it --
                   not a glide fader still mixing over the cue. */
                if (!machineryHeldForEdit())
                    m_editLayer->clear();
                releaseStageMachinery();
            });
        }, Qt::QueuedConnection);
    // A Back's one-shots: this GO plays them again (a Back's own noteGo()
    // comes before its noteBack(), which holds them anew).
    if (backGen != 0)
        QMetaObject::invokeMethod(this, [this, backGen]() {
            QTimer::singleShot(4 * int(MasterTimer::tick()), this, [this, backGen]() {
                {
                    QMutexLocker locker(&m_machineMutex);
                    if (m_backGen != backGen)
                        return;
                    m_backHeld.clear();
                    ++m_backGen;
                }
                m_backLayer->clear();
                emitMachineryChanged();
            });
        }, Qt::QueuedConnection);
    // What the hazards' hold kept off, once the GO's cue is writing.
    QMetaObject::invokeMethod(this, [this]() {
        if (m_hazards.isEmpty() && m_hazardsRefused.isEmpty())
            return;
        QTimer::singleShot(4 * int(MasterTimer::tick()), this, [this]() { checkHazardsRefused(); });
    }, Qt::QueuedConnection);
    if (m_stopEpoch.exchange(0, std::memory_order_relaxed) != 0)
        QMetaObject::invokeMethod(this, [this]() {
            m_lastHeldBy = showOnlyHeldBy();
            emit showOnlyChanged();
            emit emergencyStopEnded();
        }, thread() == QThread::currentThread() ? Qt::DirectConnection : Qt::QueuedConnection);
}

/****************************************************************************
 * Stage machinery: a stop leaves a curtain where it is
 ****************************************************************************/

QVector<OutputRuleKeeper::Machine> OutputRuleKeeper::machinesNow() const
{
    QVector<Machine> out;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == nullptr || !fx->isPatched() || !FixtureRest::freezesOnStop(fx) || isParked(fx->id()))
            continue;
        Machine m;
        m.fixture = fx->id();
        m.behaviour = FixtureRest::behaviour(fx);
        m.base = fx->universeAddress();
        m.channels = fx->channels();
        m.showOnly = (fx->outputRules() & Fixture::ShowOnly) != 0;
        m.intensity = QByteArray(int(m.channels), char(0));
        for (quint32 i = 0; i < m.channels; ++i)
        {
            const QLCChannel *ch = fx->channel(i);
            if (ch != nullptr && ch->group() == QLCChannel::Intensity)
                m.intensity[int(i)] = char(1);
        }
        out << m;
    }
    return out;
}

QHash<quint32, QByteArray> OutputRuleKeeper::capture(const QVector<Machine> &machines,
                                                     QList<OutputLayer::Level> *levels) const
{
    /* Where each one is now: what the faders built last frame (its cue's
       fade part way, a hold, or what it was left at). */
    const QList<Universe *> ua = m_doc->inputOutputMap()->universesSnapshot();
    QHash<int, QByteArray> pre;
    QHash<quint32, QByteArray> held;
    for (const Machine &m : machines)
    {
        QByteArray values(int(m.channels), char(0));
        for (quint32 i = 0; i < m.channels; ++i)
        {
            const quint32 abs = m.base + i;
            const int u = int(abs >> 9);
            if (u >= ua.size() || ua.at(u) == nullptr)
                continue;
            auto it = pre.find(u);
            if (it == pre.end())
                it = pre.insert(u, ua.at(u)->preGMValues());
            const int a = int(abs & 0x1ff);
            if (a < it.value().size())
                values[int(i)] = it.value().at(a);
            // Over everything that plays, a Show-only one too (held as its
            // pan and tilt are as well: Universe::holdShowOnlyPositions()).
            if (levels != nullptr)
                *levels << OutputLayer::Level { m.fixture, i, uchar(values.at(int(i))) };
        }
        held.insert(m.fixture, values);
    }
    return held;
}

QList<OutputLayer::Level> OutputRuleKeeper::levelsOf(const QHash<quint32, QByteArray> &held)
{
    QList<OutputLayer::Level> levels;
    for (auto it = held.constBegin(); it != held.constEnd(); ++it)
        for (int i = 0; i < it.value().size(); ++i)
            levels << OutputLayer::Level { it.key(), quint32(i), uchar(it.value().at(i)) };
    return levels;
}

bool OutputRuleKeeper::isParked(quint32 fixtureId) const
{
    const ProgrammerController *pc = m_doc->programmer();
    return pc != nullptr && pc->isFixtureParked(fixtureId);
}

void OutputRuleKeeper::holdStageMachinery()
{
    QVector<Machine> machines;
    {
        QMutexLocker locker(&m_machineMutex);
        // The rig as it is now, when the fixture list is ours to read.
        if (thread() == QThread::currentThread())
            m_machines = machinesNow();
        machines = m_machines;
    }
    // A stop: what the last GO's hazards were refused is history.
    if (thread() == QThread::currentThread())
        m_hazardsRefused.clear();
    if (machines.isEmpty())
        return;

    // Before anything stops.
    QList<OutputLayer::Level> levels;
    const QHash<quint32, QByteArray> held = capture(machines, &levels);
    {
        QMutexLocker locker(&m_machineMutex);
        m_machineHeld = held;
        m_machinesHeld = machines;
        if (++m_machineGen == 0)
            ++m_machineGen;
    }
    /* Stamped now, in this session (the emergency stop calls this once its
       new session has started): the levels count on a Show-only curtain,
       so nothing played after the stop moves it either. */
    m_machineLayer->set(levels, [](const OutputLayer::Level &) { return true; });
    emitMachineryChanged();
}

void OutputRuleKeeper::releaseStageMachinery()
{
    QVector<Machine> machines;
    {
        QMutexLocker locker(&m_machineMutex);
        if (m_machineHeld.isEmpty())
            return;
        m_machineHeld.clear();
        machines = m_machinesHeld;
        m_machinesHeld.clear();
        if (++m_machineGen == 0)
            ++m_machineGen;
    }
    m_machineLayer->clear();
    /* A Show-only curtain's intensity (a relay given the Curtain symbol):
       the stop's hold there goes too -- the GO's cue at 0 writes nothing
       there, so it would never be let go. Its other channels go at the
       cue's first write there, as a head's pan and tilt do. */
    const QList<Universe *> ua = m_doc->inputOutputMap()->universesSnapshot();
    for (const Machine &m : std::as_const(machines))
    {
        if (!m.showOnly)
            continue;
        for (quint32 i = 0; i < m.channels; ++i)
        {
            const quint32 abs = m.base + i;
            const int u = int(abs >> 9);
            if (i >= quint32(m.intensity.size()) || m.intensity.at(int(i)) == 0
                || u >= ua.size() || ua.at(u) == nullptr)
                continue;
            ua.at(u)->setShowOnlyHoldsPosition(int(abs & 0x1ff), false);
            ua.at(u)->setShowOnlyHoldsPosition(int(abs & 0x1ff), true);
        }
    }
    emitMachineryChanged();
}

void OutputRuleKeeper::holdMachineryForEdit(const QHash<quint32, QByteArray> &drawn)
{
    bool changed = false;
    bool take = false;
    QVector<Machine> machines;
    {
        QMutexLocker locker(&m_machineMutex);
        if (m_editHeld.isEmpty())
        {
            m_machines = machinesNow();
            for (const Machine &m : std::as_const(m_machines))
                if (!m.showOnly)            // Design holds a Show-only one already
                    machines << m;
            take = !machines.isEmpty();
        }
    }
    if (take)
    {
        /* Where each one is now, before the pick's scenes write: what the
           faders built last frame (a run's fade part way, a stop's hold,
           or what it was left at). */
        QList<OutputLayer::Level> levels;
        const QHash<quint32, QByteArray> held = capture(machines, &levels);
        {
            QMutexLocker locker(&m_machineMutex);
            m_editHeld = held;
        }
        m_editLayer->set(levels, [](const OutputLayer::Level &) { return true; });
        changed = true;
    }
    {
        QMutexLocker locker(&m_machineMutex);
        QHash<quint32, QByteArray> mine;
        for (auto it = drawn.constBegin(); it != drawn.constEnd(); ++it)
            if (m_editHeld.contains(it.key()))
                mine.insert(it.key(), it.value());
        if (mine != m_editDrawn)
        {
            m_editDrawn = mine;
            changed = true;
        }
    }
    if (changed)
        emitMachineryChanged();
}

void OutputRuleKeeper::releaseMachineryForEdit(uint glideMs)
{
    {
        QMutexLocker locker(&m_machineMutex);
        if (m_editHeld.isEmpty())
            return;
        m_editHeld.clear();
        m_editDrawn.clear();
    }
    m_editLayer->glideOut(glideMs);
    emitMachineryChanged();
}

bool OutputRuleKeeper::machineryHeldForEdit() const
{
    QMutexLocker locker(&m_machineMutex);
    return !m_editHeld.isEmpty();
}

bool OutputRuleKeeper::heldForEdit(quint32 fixtureId, QByteArray *drawn) const
{
    QMutexLocker locker(&m_machineMutex);
    if (!m_editHeld.contains(fixtureId))
        return false;
    if (drawn != nullptr)
        *drawn = m_editDrawn.value(fixtureId);
    return true;
}

bool OutputRuleKeeper::stageMachineryHeld() const
{
    QMutexLocker locker(&m_machineMutex);
    return !m_machineHeld.isEmpty();
}

QString OutputRuleKeeper::stageMachineryHeldText() const
{
    QHash<quint32, QByteArray> held;
    {
        QMutexLocker locker(&m_machineMutex);
        held = m_machineHeld;
    }
    if (held.isEmpty())
        return QString();
    /* The curtains first -- the operator's next move depends on where
       they stopped -- then how many more (relays, drops, a turntable). */
    QList<quint32> curtains;
    for (auto it = held.constBegin(); it != held.constEnd(); ++it)
        if (Scenery::isCurtainFixture(m_doc->fixture(it.key())))
            curtains << it.key();
    const int others = int(held.size()) - int(curtains.size());
    QString what;
    if (curtains.size() == 1)
    {
        const Fixture *fx = m_doc->fixture(curtains.first());
        if (fx == nullptr || fx->fixtureMode() == nullptr)
            what = tr("Curtain stopped");
        else
            what = tr("Curtain stopped at %1%").arg(qRound(
                Scenery::poseFromValues(fx->fixtureMode(), held.value(curtains.first())).open * 100.0));
    }
    else if (curtains.size() > 1)
        what = tr("%n curtains stopped", "", int(curtains.size()));
    if (others > 0)
    {
        if (!what.isEmpty())
            what += tr(" · %n more held", "", others);
        else if (others == 1)
        {
            const Fixture *fx = m_doc->fixture(held.constBegin().key());
            what = tr("%1 held").arg(fx != nullptr ? fx->name() : tr("Stage machinery"));
        }
        else
            what = tr("%n held where they are", "", others);
    }
    return tr("%1 — GO to carry on").arg(what);
}

void OutputRuleKeeper::noteBack()
{
    if (GenericFader::stampOverridden())
        return;
    QVector<Machine> shots;
    {
        QMutexLocker locker(&m_machineMutex);
        m_machines = machinesNow();
        for (const Machine &m : std::as_const(m_machines))
            if (m.behaviour == Fixture::OneShotBehaviour)
                shots << m;
    }
    if (shots.isEmpty())
        return;
    // Where each is now, before the Back's cue writes (the action waits
    // for the next tick): dropped stays dropped.
    QList<OutputLayer::Level> levels;
    const QHash<quint32, QByteArray> held = capture(shots, &levels);
    {
        QMutexLocker locker(&m_machineMutex);
        m_backHeld = held;
        if (++m_backGen == 0)
            ++m_backGen;
    }
    m_backLayer->set(levels, [](const OutputLayer::Level &) { return true; });
    emitMachineryChanged();
}

bool OutputRuleKeeper::oneShotsHeldForBack() const
{
    QMutexLocker locker(&m_machineMutex);
    return !m_backHeld.isEmpty();
}

void OutputRuleKeeper::layHazards()
{
    QHash<quint32, QPair<quint32, quint32> > hazards;
    QList<OutputLayer::Level> levels;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == nullptr || !fx->isPatched() || !FixtureRest::isHazard(fx) || isParked(fx->id()))
            continue;
        hazards.insert(fx->id(), qMakePair(fx->universeAddress(), fx->channels()));
        for (quint32 i = 0; i < fx->channels(); ++i)
            levels << OutputLayer::Level { fx->id(), i, safeValue(fx, i) };
    }
    m_hazards = hazards;
    if (levels.isEmpty())
        m_hazardLayer->clear();
    else
        m_hazardLayer->set(levels, [](const OutputLayer::Level &) { return true; });
}

QList<quint32> OutputRuleKeeper::heldHazards() const
{
    return m_hazards.keys();
}

void OutputRuleKeeper::checkHazardsRefused()
{
    /* What would drive each hazard above off: a fader under the Park ones
       (every hold here sits at Park) with a target there. On a GO only,
       never per frame. */
    QList<quint32> refused;
    const QList<Universe *> ua = m_doc->inputOutputMap()->universesSnapshot();
    for (auto it = m_hazards.constBegin(); it != m_hazards.constEnd(); ++it)
    {
        const Fixture *fx = m_doc->fixture(it.key());
        bool driven = false;
        for (quint32 i = 0; fx != nullptr && i < it.value().second && !driven; ++i)
        {
            const quint32 abs = it.value().first + i;
            const int u = int(abs >> 9);
            if (u >= ua.size() || ua.at(u) == nullptr)
                continue;
            const uchar safe = safeValue(fx, i);
            for (const QSharedPointer<GenericFader> &f : ua.at(u)->faders())
            {
                if (f.isNull() || f->priority() >= int(Universe::Park))
                    continue;
                FadeChannel fc;
                if (f->peekAddress(abs & 0x1ff, &fc) && fc.target(0) != safe)
                {
                    driven = true;
                    break;
                }
            }
        }
        if (driven)
            refused << it.key();
    }
    std::sort(refused.begin(), refused.end());
    if (refused != m_hazardsRefused)
    {
        m_hazardsRefused = refused;
        emitMachineryChanged();
    }
}

QList<quint32> OutputRuleKeeper::hazardsRefused() const
{
    return m_hazardsRefused;
}

QString OutputRuleKeeper::hazardRefusedText() const
{
    return m_hazardsRefused.isEmpty() ? QString() : tr("Hazard — needs arming (coming)");
}

void OutputRuleKeeper::parkChanged()
{
    /* Park wins: a parked fixture is let go of by a stop's hold, an edit's
       and a Back's, and by the hazards' (it is parked at off). */
    QHash<quint32, QByteArray> stopHeld, editHeld, backHeld;
    bool stop = false, edit = false, back = false;
    {
        QMutexLocker locker(&m_machineMutex);
        m_machines = machinesNow();
        auto drop = [this](QHash<quint32, QByteArray> &held) {
            bool any = false;
            for (auto it = held.begin(); it != held.end();)
                if (isParked(it.key()))
                {
                    it = held.erase(it);
                    any = true;
                }
                else
                    ++it;
            return any;
        };
        stop = drop(m_machineHeld);
        edit = drop(m_editHeld);
        back = drop(m_backHeld);
        for (auto it = m_editDrawn.begin(); it != m_editDrawn.end();)
            it = m_editHeld.contains(it.key()) ? std::next(it) : m_editDrawn.erase(it);
        stopHeld = m_machineHeld;
        editHeld = m_editHeld;
        backHeld = m_backHeld;
    }
    if (stop)
        m_machineLayer->set(levelsOf(stopHeld), [](const OutputLayer::Level &) { return false; });
    if (edit)
        m_editLayer->set(levelsOf(editHeld), [](const OutputLayer::Level &) { return false; });
    if (back)
        m_backLayer->set(levelsOf(backHeld), [](const OutputLayer::Level &) { return false; });
    layHazards();
    if (stop || edit || back)
        emitMachineryChanged();
}

void OutputRuleKeeper::emitMachineryChanged()
{
    QMetaObject::invokeMethod(this, [this]() { emit stageMachineryChanged(); },
                              thread() == QThread::currentThread() ? Qt::DirectConnection : Qt::QueuedConnection);
}

OutputRuleKeeper::HeldBy OutputRuleKeeper::showOnlyHeldBy() const
{
    if (m_showOnly.isEmpty())
        return NotHeld;
    if (m_doc->mode() != Doc::Operate)
        return HeldByDesign;
    if (emergencyStopped())
        return HeldByEmergencyStop;
    if (m_doc->inputOutputMap()->outputInhibited())
        return HeldByBlind;
    return NotHeld;
}

QString OutputRuleKeeper::showOnlyHeldText() const
{
    const HeldBy by = showOnlyHeldBy();
    if (by == NotHeld)
        return QString();
    const QString why = by == HeldByEmergencyStop ? tr("emergency stop -- a GO or a control you move drives them again")
                      : by == HeldByBlind ? tr("Blind") : tr("Design");

    /* Named by what they are when they are all one kind: "Lasers held". */
    int lasers = 0, smoke = 0;
    foreach (quint32 id, m_showOnly)
    {
        const Fixture *fx = m_doc->fixture(id);
        if (FixtureRest::isLaser(fx))
            ++lasers;
        else if (FixtureRest::makesAtmosphere(fx))
            ++smoke;
    }
    const int n = int(m_showOnly.size());
    if (n == 1)
    {
        const Fixture *fx = m_doc->fixture(m_showOnly.first());
        return tr("%1 held: %2").arg(fx != nullptr ? fx->name() : tr("Show-only fixture"), why);
    }
    if (lasers == n)
        return tr("Lasers held: %1").arg(why);
    if (smoke == n)
        return tr("Fog held: %1").arg(why);
    return tr("%1 Show-only fixtures held: %2").arg(n).arg(why);
}

void OutputRuleKeeper::pushShowOnlyLive()
{
    const bool live = m_doc->mode() == Doc::Operate;
    InputOutputMap *io = m_doc->inputOutputMap();
    // The universes as they are: never claimed -- a mode change can land
    // inside a tick, and the flag is an atomic each universe reads.
    foreach (Universe *u, io->universes())
        if (u != nullptr)
            u->setShowOnlyLive(live);
    const HeldBy by = showOnlyHeldBy();
    if (by != m_lastHeldBy)
    {
        m_lastHeldBy = by;
        emit showOnlyChanged();
    }
}

void OutputRuleKeeper::rebuild()
{
    m_pending = false;
    if (m_rebuildTimer != nullptr)
        m_rebuildTimer->stop();     // this is the one it would have run
    ++m_rebuilds;
    int count = 0;
    QList<quint32> showOnly;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx->outputRules() != Fixture::OutputRules())
            ++count;
        if (fx->outputRules() & Fixture::ShowOnly)
            showOnly << fx->id();
    }
    m_showOnlyAt.clear();
    m_showOnlyMode.clear();
    foreach (Fixture *fx, m_doc->fixtures())
        if ((fx->outputRules() & Fixture::ShowOnly) && fx->isPatched())
        {
            m_showOnlyAt.insert(fx->id(), qMakePair(fx->universeAddress(), fx->channels()));
            m_showOnlyMode.insert(fx->id(), fx->fixtureMode());
        }
    m_count = count;
    {
        QMutexLocker locker(&m_machineMutex);
        m_machines = machinesNow();
    }
    layHazards();
    const bool showOnlyListChanged = showOnly != m_showOnly;
    m_showOnly = showOnly;
    // With none, a function never walks what started it (Function).
    Function::setShowOnlyInUse(showOnly.isEmpty() == false);

    InputOutputMap *io = m_doc->inputOutputMap();
    const int n = int(io->universesCount());
    QVector<QByteArray> bits, safe, positions;
    for (int i = 0; i < n; i++)
    {
        bits << bitsFor(quint32(i));
        safe << (showOnly.isEmpty() ? QByteArray() : safeValuesFor(quint32(i)));
        positions << (showOnly.isEmpty() ? QByteArray() : positionsFor(quint32(i)));
    }

    const bool armed = onAtOpenArmed();
    /* The universes as they are, not claimed: rebuild() runs straight from
       a fixture change, which can come from code that holds the claim. A
       universe takes its rules under its own lock. */
    const QList<Universe *> ua = io->universes();
    for (int i = 0; i < ua.size(); ++i)
    {
        Universe *u = ua.at(i);
        if (u == nullptr)
            continue;
        u->setOnAtOpenArmed(armed);
        u->setBlindHoldsExempt(m_blindHoldsExempt);
        // The safe values first: the bits make a channel Show only, and
        // the very next frame sends its safe value.
        u->setShowOnlySafeValues(i < safe.size() ? safe.at(i) : QByteArray());
        u->setShowOnlyPositions(i < positions.size() ? positions.at(i) : QByteArray());
        u->setOutputRules(i < bits.size() ? bits.at(i) : QByteArray());
        u->setShowOnlyLive(m_doc->mode() == Doc::Operate);
    }
    const HeldBy by = showOnlyHeldBy();
    if (showOnlyListChanged || by != m_lastHeldBy)
    {
        m_lastHeldBy = by;
        emit showOnlyChanged();
    }
}

QString OutputRuleKeeper::describe(Fixture::OutputRules rules)
{
    QStringList ignores, more;
    if (rules & Fixture::ExemptBlackout)
        ignores << tr("Blackout");
    if (rules & Fixture::ExemptGrandMaster)
        ignores << tr("Grand Master");
    if (rules & Fixture::Switch)
        more << tr("switches (relay)");
    if (rules & Fixture::OnAtOpen)
        more << tr("on when not in use");
    if (rules & Fixture::ShowOnly)
        more << tr("show only (held outside a real run in Operate)");
    QStringList out;
    if (ignores.isEmpty() == false)
        out << tr("ignores %1").arg(ignores.join(tr(", ")));
    out << more;
    QString text = out.join(QStringLiteral("; "));
    if (text.isEmpty() == false)
        text[0] = text.at(0).toUpper();
    return text;
}
