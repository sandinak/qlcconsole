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
#include "teachinglog.h"
#include "scene.h"
#include "qlcpalette.h"
#include "hazardfastpath.h"
#include "controllerkeepalive.h"
#include "showtreecontroller.h"
#include "chaserstep.h"
#include "collection.h"
#include "chaser.h"
#include "functionparent.h"
#include <QDateTime>
#include <climits>


#define KXMLQLCOutputRuleOnAtOpen QStringLiteral("OnAtOpen")

OutputRuleKeeper::OutputRuleKeeper(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
    , m_machineLayer(new OutputLayer(doc, Universe::Park, FadeChannel::Override, QStringLiteral("Curtain stopped")))
    , m_editLayer(new OutputLayer(doc, Universe::Park, FadeChannel::Override, QStringLiteral("Curtain held while editing")))
    , m_backLayer(new OutputLayer(doc, Universe::Park, FadeChannel::Override, QStringLiteral("One-shot held after Back")))
    , m_hazardLayer(new OutputLayer(doc, Universe::Park, FadeChannel::Override | FadeChannel::PastGM,
                                    QStringLiteral("Hazard held safe")))
{
    // The hazards' hold over every Park, laid after it or not (053p review).
    m_hazardLayer->setOnTop(true);
    // Park wins over every hold here (board Q351).
    if (doc->programmer() != nullptr)
        connect(doc->programmer(), &ProgrammerController::parkChanged, this, &OutputRuleKeeper::parkChanged);
    /* A Show-only fixture added, moved or (re)patched -- mid-show too: its
       new address is held at once, not after the event loop comes round
       (one frame there could fire it). Anything else waits, coalesced. */
    auto changed = [this](quint32 id) {
        markNow(id);
        layHazardNow(id);       // a hazard held from this frame on
        scheduleRebuild();      // the rest, once, coalesced
    };
    connect(doc, &Doc::fixtureAdded, this, changed);
    connect(doc, &Doc::fixtureRemoved, this, [this](quint32 id) {
        layHazardNow(id);       // gone now: its id may come back as another fixture
        scheduleRebuild();
    });
    connect(doc, &Doc::fixtureChanged, this, changed);
    connect(doc, &Doc::loaded, this, [this]() {
        // E2 (R6-7): a show loaded -- over the web while in Operate too --
        // starts a new session: nothing it brought in is the show.
        startSession();
        // Its hazards disarmed: a Hold already down counts once let go.
        disarmHazards(tr("the show was opened"));
        // Held now, before a startup function or Operate can write them.
        layHazards();
        scheduleRebuild();
    });
    // Another show: a new show's setting, and OnAtOperate waits for its
    // own first Operate.
    connect(doc, &Doc::clearing, this, [this]() {
        m_clearing = true;
        QMutexLocker locker(&m_hazardMutex);
        m_runLists.clear();
    });
    // A list removed leaves the Run's lists before it is deleted.
    connect(doc, &Doc::functionRemoved, this, [this](quint32 id) {
        QMutexLocker locker(&m_hazardMutex);
        m_runLists.remove(id);
        for (auto it = m_runLists.begin(); it != m_runLists.end();)
            it = it.value() != nullptr && it.value()->id() == id ? m_runLists.erase(it) : std::next(it);
    });
    connect(doc, &Doc::cleared, this, [this]() {
        m_clearing = false;
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
        // Every show opens with its hazards disarmed (Q352/Q353).
        disarmHazards(tr("another show was opened"));
        m_hazardsSkipped.clear();
        m_hazardSkippedBurst.clear();
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
        /* No list carries a GO's stamp across a mode switch (R1): what plays
           on fires a hazard again only from the next GO. */
        for (Function *f : m_doc->functions())
            if (Chaser *c = qobject_cast<Chaser *>(f))
                c->clearRunStamp();
        /* Either way, nothing a run let fire stays out (an edit never fires
           a hazard; Operate's show starts from its own GO). The spotter's
           arming stays theirs. */
        revokeHazards();
        if (!m_hazardsSkipped.isEmpty())
        {
            m_hazardsSkipped.clear();
            m_hazardSkippedBurst.clear();
            emitMachineryChanged();
            emitHazardsChanged();
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

    /* Hazard arming's fire layers (board Q352/Q353, second review): on top
       of the safe hold, each writing only while its gate is open. */
    m_burstGate.flag = &m_hazardHold;
    m_burstGate.grant = &m_hazardGrant;
    m_burstGate.epoch = &m_hazardStep;
    m_runningGate.flag = &m_hazardArmed;
    m_runningGate.grant = &m_hazardGrant;
    m_runningGate.epoch = &m_hazardStep;
    m_burstLayer.reset(new OutputLayer(doc, Universe::Park, FadeChannel::Override | FadeChannel::PastGM,
                                       QStringLiteral("Hazard fired by its cue: Burst")));
    m_runningLayer.reset(new OutputLayer(doc, Universe::Park, FadeChannel::Override | FadeChannel::PastGM,
                                         QStringLiteral("Hazard fired by its cue: Running")));
    m_burstLayer->setOnTop(true);
    m_runningLayer->setOnTop(true);
    m_burstLayer->setGate(&m_burstGate);
    m_runningLayer->setGate(&m_runningGate);
    // The Hold let go and the Kill, from the input thread itself.
    // The Hold held too long counts as let go (N3; 15 s).
    m_holdLimitMs = qEnvironmentVariableIsSet("QLC_TEST_HAZARD_HOLD_MS")
                        ? qMax(100, qEnvironmentVariableIntValue("QLC_TEST_HAZARD_HOLD_MS")) : 15000;
    m_holdLimit = new QTimer(this);
    m_holdLimit->setSingleShot(true);
    connect(m_holdLimit, &QTimer::timeout, this, [this]() {
        if (!m_hazardHold.exchange(false, std::memory_order_acq_rel))
            return;
        m_hazardHoldNeedsRelease.store(true, std::memory_order_relaxed);
        m_holdShown = false;
        m_holdTimedOut = true;
        logHazard(tr("Hold timed out after %n s: release and press again", "", m_holdLimitMs / 1000));
        logHazardOutputs(tr("Hold timed out"));
        emitMachineryChanged();
        emitHazardsChanged();
    });
    // This show's own (its InputOutputMap's): another Doc has its own.
    m_fast = doc->inputOutputMap()->hazardFastPath();
    m_fast->attach(&m_hazardHold, &m_hazardArmed, &m_hazardStep);
    // Every list's every step, whatever moves it.
    foreach (Function *f, doc->functions())
        watchChaser(qobject_cast<Chaser *>(f));
    connect(doc, &Doc::functionAdded, this, [this](quint32 id) {
        watchChaser(qobject_cast<Chaser *>(m_doc->function(id)));
    });
    refreshRunLists();
    // A laser class, a scenery, a symbol changed a guess: laid again now.
    Fixture::addGuessListener(this, [this](const void *context) {
        // Another show's plot is not this one's (a definition is every show's).
        if (context != nullptr && context != static_cast<const void *>(m_doc->monitorProperties()))
            return;
        if (thread() == QThread::currentThread())
            guessesChanged();
        else
            QMetaObject::invokeMethod(this, [this]() { guessesChanged(); }, Qt::QueuedConnection);
    });
    if (ProgrammerController *pc = doc->programmer())
    {
        // Sleep and wake disarm: the spotter's device was gone meanwhile.
        if (ControllerKeepAlive *ka = pc->controllerKeepAlive())
            connect(ka, &ControllerKeepAlive::systemStateChanged, this,
                    [this](ControllerKeepAlive::SystemState st) {
                        if (st == ControllerKeepAlive::Asleep)
                            disarmHazards(tr("the computer went to sleep"));
                        else if (st == ControllerKeepAlive::Waking)
                            disarmHazards(tr("the computer woke"));
                    });
        // A programmer edit after a Design run: what the run granted goes.
        connect(pc, &ProgrammerController::programmerDirtyChanged, this, [this](bool dirty) {
            if (dirty && m_doc->mode() == Doc::Design)
                revokeHazards();
        });
    }
}

OutputRuleKeeper::~OutputRuleKeeper()
{
    shutdownHazards();
    Fixture::removeGuessListener(this);
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
    // Every hazard off and disarmed, before anything else (Q352/Q353).
    disarmHazards(tr("the emergency stop"));
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
        backGen = m_backHeld.isEmpty() || m_backFresh.load() ? 0 : m_backGen;
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
                    m_editLayer->dropGlides();
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
    // The hazards: every step a list reaches is watched (watchChaser()).
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
        m.onAtOpen = (fx->outputRules() & Fixture::OnAtOpen) != 0;
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
    QHash<int, QByteArray> pre, post;
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
            /* On when not in use and nothing sets it: at full on the wire,
               0 before the Grand Master -- held at what is sent. */
            if (m.onAtOpen)
            {
                auto pt = post.find(u);
                if (pt == post.end())
                    pt = post.insert(u, *ua.at(u)->postGMValues());
                if (a < pt.value().size() && uchar(pt.value().at(a)) > uchar(values.at(int(i))))
                    values[int(i)] = pt.value().at(a);
            }
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
    // A stop: every hazard safe and disarmed (Q352/Q353).
    disarmHazards(tr("a stop"));
    // What the last GO's hazards skipped is history.
    if (thread() == QThread::currentThread() && !m_hazardsSkipped.isEmpty())
    {
        m_hazardsSkipped.clear();
        m_hazardSkippedBurst.clear();
        emitHazardsChanged();
        if (machines.isEmpty())
            emitMachineryChanged();
    }
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
    // An edit pick never fires a hazard, armed or not (Q352/Q353).
    revokeHazards();
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
    QHash<quint32, QByteArray> motors;
    bool others = false;
    {
        QMutexLocker locker(&m_machineMutex);
        if (m_editHeld.isEmpty())
            return;
        /* Only a motor travels: a switch or a one-shot is let go of at
           once, never mixed through its switch point (or its trigger). */
        for (auto it = m_editHeld.constBegin(); it != m_editHeld.constEnd(); ++it)
        {
            bool motor = false;
            for (const Machine &m : std::as_const(m_machines))
                if (m.fixture == it.key())
                    motor = m.behaviour == Fixture::MachineryBehaviour;
            if (motor)
                motors.insert(it.key(), it.value());
            else
                others = true;
        }
        m_editHeld.clear();
        m_editDrawn.clear();
    }
    if (others)
        m_editLayer->set(levelsOf(motors), [](const OutputLayer::Level &) { return false; });
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
    /* The Back's own GO (its claim, right after) is not the next GO: it
       must not let go of what this Back holds. Over once the event loop
       comes round. */
    m_backFresh = true;
    QTimer::singleShot(0, this, [this]() { m_backFresh = false; });
    emitMachineryChanged();
}

bool OutputRuleKeeper::oneShotsHeldForBack() const
{
    QMutexLocker locker(&m_machineMutex);
    return !m_backHeld.isEmpty();
}

OutputRuleKeeper::Hazard OutputRuleKeeper::hazardOf(const Fixture *fx) const
{
    Hazard h;
    h.base = fx->universeAddress();
    h.channels = fx->channels();
    h.burst = FixtureRest::behaviour(fx) == Fixture::HazardBurstBehaviour;
    h.parked = isParked(fx->id());
    h.name = fx->name();
    return h;
}

void OutputRuleKeeper::layHazards()
{
    QHash<quint32, Hazard> hazards;
    QList<quint32> order;
    QList<QPair<Fixture *, bool> > stays;
    QHash<quint32, Hazard> was;
    {
        QMutexLocker locker(&m_hazardMutex);
        was = m_hazards;
    }
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == nullptr || !fx->isPatched())
            continue;
        /* Once a hazard, a hazard until set to something else by hand
           (second review, 8): a guess that changed keeps it one. */
        if (!FixtureRest::isHazard(fx) && was.contains(fx->id()) && !fx->behaviourOverridden())
            stays << qMakePair(fx, was.value(fx->id()).burst);
        else if (!FixtureRest::isHazard(fx))
            continue;
        Hazard h = hazardOf(fx);
        if (!FixtureRest::isHazard(fx))
            h.burst = was.value(fx->id()).burst;
        hazards.insert(fx->id(), h);
        order << fx->id();
    }
    for (const auto &st : std::as_const(stays))
    {
        st.first->setBehaviourSetting(st.second ? Fixture::HazardBurstBehaviour : Fixture::HazardRunningBehaviour);
        logHazard(tr("%1 stays a hazard (its name or definition no longer says so): set its "
                     "Behaves as by hand to change it").arg(st.first->name()));
    }
    bool changed = false;
    {
        QMutexLocker locker(&m_hazardMutex);
        changed = order != m_hazardOrder;
        for (auto it = hazards.constBegin(); !changed && it != hazards.constEnd(); ++it)
        {
            const Hazard was = m_hazards.value(it.key());
            changed = was.burst != it.value().burst || was.channels != it.value().channels
                      || was.parked != it.value().parked || was.base != it.value().base
                      || was.name != it.value().name;
        }
        m_hazards = hazards;
        m_hazardOrder = order;
    }
    {
        // Every universe told again (one added since has none of it).
        QMutexLocker apply(&m_hazardApplyMutex);
        m_passForce = true;
    }
    // The rig changed under the grants: worked out again from the steps.
    if (changed)
        grantHazards(Function::invalidId());
    else
        applyHazards();
    if (changed)
        emitHazardsChanged();
}

void OutputRuleKeeper::layHazardNow(quint32 fixtureId)
{
    Fixture *fx = m_doc->fixture(fixtureId);
    bool hazard = fx != nullptr && fx->isPatched() && FixtureRest::isHazard(fx);
    bool was = false;
    Hazard old;
    {
        QMutexLocker locker(&m_hazardMutex);
        was = m_hazards.contains(fixtureId);
        old = m_hazards.value(fixtureId);
    }
    if (!hazard && !was)
        return;             // a light stays a light: nothing to lay
    /* Once a hazard, a hazard until someone sets it to something else by
       hand (the second review, 8): a rename ("CO2 jet" -> "DS Jet") or a
       definition's change that loses the guess keeps it one, set by hand
       to its kind now, said in the log and listed in Show Review. */
    if (!hazard && was && fx != nullptr && fx->isPatched() && !fx->behaviourOverridden())
    {
        fx->setBehaviourSetting(old.burst ? Fixture::HazardBurstBehaviour : Fixture::HazardRunningBehaviour);
        logHazard(tr("%1 stays a hazard (its name or definition no longer says so): set its "
                     "Behaves as by hand to change it").arg(fx->name()));
        return;             // its changed() comes back here as a hazard
    }
    bool changed = false;
    {
        QMutexLocker locker(&m_hazardMutex);
        if (hazard)
        {
            const Hazard h = hazardOf(fx);
            changed = !was || old.base != h.base || old.channels != h.channels || old.burst != h.burst
                      || old.parked != h.parked || old.name != h.name;
            m_hazards.insert(fixtureId, h);
            if (!was)
                m_hazardOrder << fixtureId;
        }
        else
        {
            m_hazards.remove(fixtureId);
            m_hazardOrder.removeAll(fixtureId);
            changed = true;
        }
    }
    if (!changed)
        return;
    if (was && !hazard && fx != nullptr && fx->behaviourOverridden())
        logHazard(tr("%1 set to %2 by hand: no longer a hazard")
                      .arg(fx->name(), Fixture::behaviourName(Fixture::Behaviour(fx->behaviourSetting()))));
    // Moved, re-patched, of another kind, or new: what it was granted is gone.
    grantHazards(Function::invalidId());
    emitHazardsChanged();
}

void OutputRuleKeeper::guessesChanged()
{
    // The show going away (clearContents(), ~Doc): its definitions with it.
    if (m_clearing)
        return;
    // Many at once while a show loads: it is laid when the load ends.
    if (m_doc->loadStatus() == Doc::Loading)
    {
        scheduleRebuild();
        return;
    }
    layHazards();
}

void OutputRuleKeeper::applyHazards()
{
    QMutexLocker apply(&m_hazardApplyMutex);
    QList<OutputLayer::Level> safe, burst, running;
    QSet<quint32> passHeld;
    {
        QMutexLocker locker(&m_hazardMutex);
        for (quint32 id : std::as_const(m_hazardOrder))
        {
            const Hazard &h = m_hazards[id];
            // Off: a hazard's every channel rests at 0 (FixtureRest::restValue()).
            for (quint32 i = 0; i < h.channels; ++i)
            {
                safe << OutputLayer::Level { id, i, uchar(0) };
                passHeld.insert(h.base + i);
            }
            if (h.parked)
                continue;           // Park wins: at 0
            const QHash<quint32, QByteArray> &grant = h.burst ? m_burstGrant : m_runningGrant;
            auto g = grant.constFind(id);
            if (g == grant.constEnd())
                continue;
            /* A Burst snaps; a Running one follows its cue's fade (second
               review N7): from where it was, over the step's own time. */
            const QByteArray from = h.burst ? QByteArray() : m_runningFrom.value(id);
            const uint ms = h.burst ? 0 : m_runningFadeMs.value(id);
            for (quint32 i = 0; i < h.channels && int(i) < g.value().size(); ++i)
            {
                OutputLayer::Level l { id, i, uchar(g.value().at(int(i))) };
                if (!h.burst && int(i) < from.size())
                {
                    l.from = uchar(from.at(int(i)));
                    l.fadeMs = ms;
                }
                (h.burst ? burst : running) << l;
            }
        }
    }
    /* The input passthrough never reaches a hazard held safe (053p
       review): it merges over every fader. Held before the layers move
       (second review, 11): never a frame where it could leak. */
    if (passHeld != m_passHeld || m_passForce)
    {
        if (m_passForce)
            m_lastSafe.clear();     // a universe added: laid again there too
        m_passForce = false;
        const QList<Universe *> ua = m_doc->inputOutputMap()->universesSnapshot();
        auto mark = [&ua](quint32 abs, bool held) {
            const int u = int(abs >> 9);
            if (u < ua.size() && ua.at(u) != nullptr)
                ua.at(u)->setPassthroughHeld(int(abs & 0x1ff), held);
        };
        for (quint32 abs : std::as_const(passHeld))
            mark(abs, true);
        for (quint32 abs : std::as_const(m_passHeld))
            if (!passHeld.contains(abs))
                mark(abs, false);
        m_passHeld = passHeld;
    }
    bool newSafeFader = false;
    if (safe.isEmpty())
        m_hazardLayer->clear();
    else if (safe != m_lastSafe)
        newSafeFader = m_hazardLayer->set(safe, [](const OutputLayer::Level &) { return true; });
    m_lastSafe = safe;
    /* The fire layers sit over the safe one: a fader asked for later sits
       above (Universe::requestTopFader()). Laid in place -- a live hazard
       never drops for a frame (N8) -- and anew only when the safe layer
       had to take a new fader (a universe added). */
    if (newSafeFader)
    {
        m_burstLayer->clear();
        m_runningLayer->clear();
        m_lastBurst.clear();
        m_lastRunning.clear();
    }
    if (burst != m_lastBurst)
    {
        if (burst.isEmpty())
            m_burstLayer->clear();
        else
            m_burstLayer->set(burst, [](const OutputLayer::Level &) { return true; });
        m_lastBurst = burst;
    }
    if (running != m_lastRunning)
    {
        if (running.isEmpty())
            m_runningLayer->clear();
        else
            m_runningLayer->set(running, [](const OutputLayer::Level &) { return true; });
        m_lastRunning = running;
    }
}

QList<quint32> OutputRuleKeeper::hazards() const
{
    QMutexLocker locker(&m_hazardMutex);
    return m_hazardOrder;
}

bool OutputRuleKeeper::isBurstHazard(quint32 fixtureId) const
{
    QMutexLocker locker(&m_hazardMutex);
    auto it = m_hazards.constFind(fixtureId);
    return it != m_hazards.constEnd() && it.value().burst;
}

bool OutputRuleKeeper::isParkedHazard(quint32 fixtureId) const
{
    QMutexLocker locker(&m_hazardMutex);
    return m_hazards.value(fixtureId).parked;
}

QByteArray OutputRuleKeeper::hazardOutput(quint32 fixtureId) const
{
    QMutexLocker locker(&m_hazardMutex);
    auto it = m_hazards.constFind(fixtureId);
    if (it == m_hazards.constEnd() || it.value().parked)
        return QByteArray();
    const bool burst = it.value().burst;
    const FaderGate &gate = burst ? m_burstGate : m_runningGate;
    if (!gate.open())
        return QByteArray();
    const QByteArray v = (burst ? m_burstGrant : m_runningGrant).value(fixtureId);
    for (char c : v)
        if (c != 0)
            return v;
    return QByteArray();
}

bool OutputRuleKeeper::hazardLive(quint32 fixtureId) const
{
    return !hazardOutput(fixtureId).isEmpty();
}

void OutputRuleKeeper::logHazardOutputs(const QString &why)
{
    QList<quint32> order;
    QHash<quint32, QString> names;
    {
        QMutexLocker locker(&m_hazardMutex);
        order = m_hazardOrder;
        for (quint32 id : std::as_const(order))
            names.insert(id, m_hazards.value(id).name);
    }
    QHash<quint32, QByteArray> now;
    for (quint32 id : std::as_const(order))
    {
        const QByteArray out = hazardOutput(id);
        if (!out.isEmpty())
            now.insert(id, out);
    }
    auto level = [](const QByteArray &v) {
        int top = 0;
        for (char c : v)
            top = qMax(top, int(uchar(c)));
        return top;
    };
    for (quint32 id : std::as_const(order))
    {
        const int was = level(m_lastOut.value(id)), is = level(now.value(id));
        if (was == is)
            continue;
        if (is == 0)
            logHazard(tr("%1 back to safe (%2)").arg(names.value(id), why));
        else if (was == 0)
            logHazard(isBurstHazard(id) ? tr("%1 fired at %2 (%3)").arg(names.value(id)).arg(is).arg(why)
                                        : tr("%1 on at %2 (%3)").arg(names.value(id)).arg(is).arg(why));
        else
            logHazard(tr("%1 %2 → %3 (%4)").arg(names.value(id)).arg(was).arg(is).arg(why));
    }
    m_lastOut = now;
}

void OutputRuleKeeper::setHazardHold(bool down)
{
    if (!down)
    {
        m_holdLimit->stop();
        m_hazardHoldNeedsRelease.store(false, std::memory_order_relaxed);
        m_hazardHold.store(false, std::memory_order_release);
        if (!m_holdShown)
            return;
        m_holdShown = false;
    }
    else
    {
        // A Hold that was down through a disarm counts once let go of.
        if (m_hazardHoldNeedsRelease.load(std::memory_order_relaxed))
            return;
        if (m_hazardHold.exchange(true, std::memory_order_acq_rel) && m_holdShown)
            return;
        m_holdShown = true;
        m_holdTimedOut = false;
        /* A Hold held longer than a burst ever needs (N3) -- a stuck
           button, a weight on it -- counts as let go. */
        m_holdLimit->start(m_holdLimitMs);
    }
    logHazardOutputs(down ? tr("Hold held") : tr("Hold let go"));
    emitHazardsChanged();
}

void OutputRuleKeeper::armHazards()
{
    if (m_hazardArmed.exchange(true, std::memory_order_acq_rel))
        return;
    QStringList running;
    {
        QMutexLocker locker(&m_hazardMutex);
        for (quint32 id : std::as_const(m_hazardOrder))
            if (!m_hazards.value(id).burst)
                running << m_hazards.value(id).name;
    }
    logHazard(running.isEmpty() ? tr("Armed (no Running hazard in the show)")
                                : tr("Armed: %1 fire from the next cue that sets them")
                                      .arg(running.join(QStringLiteral(", "))));
    emitHazardsChanged();
}

void OutputRuleKeeper::killHazards()
{
    // The input thread may have closed it all already (HazardFastPath).
    m_hazardArmed.store(false, std::memory_order_release);
    m_hazardHold.store(false, std::memory_order_release);
    m_holdShown = false;
    bool any = false;
    {
        QMutexLocker locker(&m_hazardMutex);
        any = !m_reach.isEmpty() || !m_burstGrant.isEmpty() || !m_runningGrant.isEmpty();
    }
    revokeHazards();
    logHazard(any ? tr("Kill: every hazard off, disarmed") : tr("Kill pressed (nothing was armed)"));
    emitHazardsChanged();
}

void OutputRuleKeeper::disarmHazards(const QString &why)
{
    const bool armed = m_hazardArmed.exchange(false, std::memory_order_acq_rel);
    const bool held = m_hazardHold.exchange(false, std::memory_order_acq_rel) || m_holdShown;
    // Still down on the device: it must be let go of and pressed again.
    if (held)
        m_hazardHoldNeedsRelease.store(true, std::memory_order_relaxed);
    bool granted = false;
    {
        QMutexLocker locker(&m_hazardMutex);
        granted = !m_burstGrant.isEmpty() || !m_runningGrant.isEmpty() || !m_reach.isEmpty();
        m_reach.clear();
        m_granting.clear();
        m_burstGrant.clear();
        m_runningGrant.clear();
    }
    m_hazardStep.fetch_add(1, std::memory_order_acq_rel);
    if (!armed && !held && !granted)
        return;
    if (thread() != QThread::currentThread())
    {
        QMetaObject::invokeMethod(this, [this, why]() {
            m_holdShown = false;
            applyHazards();
            logHazardOutputs(why);
            logHazard(tr("Disarmed: %1").arg(why));
            emitHazardsChanged();
        }, Qt::QueuedConnection);
        return;
    }
    m_holdShown = false;
    applyHazards();
    logHazardOutputs(why);
    logHazard(tr("Disarmed: %1").arg(why));
    emitHazardsChanged();
}

void OutputRuleKeeper::revokeHazards()
{
    m_hazardStep.fetch_add(1, std::memory_order_acq_rel);
    {
        QMutexLocker locker(&m_hazardMutex);
        if (m_reach.isEmpty() && m_burstGrant.isEmpty() && m_runningGrant.isEmpty())
            return;
        m_reach.clear();
        m_granting.clear();
        m_burstGrant.clear();
        m_runningGrant.clear();
    }
    if (thread() != QThread::currentThread())
    {
        QMetaObject::invokeMethod(this, [this]() {
            applyHazards();
            logHazardOutputs(tr("taken back"));
            emitHazardsChanged();
        }, Qt::QueuedConnection);
        return;
    }
    applyHazards();
    logHazardOutputs(tr("taken back"));
    emitHazardsChanged();
}

void OutputRuleKeeper::setHazardControls(quint32 holdUniverse, quint32 holdChannel,
                                         quint32 killUniverse, quint32 killChannel)
{
    m_fast->holdKey.store(holdUniverse == UINT_MAX ? HazardFastPath::kNone
                                                   : HazardFastPath::key(holdUniverse, holdChannel));
    m_fast->killKey.store(killUniverse == UINT_MAX ? HazardFastPath::kNone
                                                   : HazardFastPath::key(killUniverse, killChannel));
}

void OutputRuleKeeper::watchChaser(Chaser *chaser)
{
    if (chaser == nullptr || m_watched.contains(chaser->id()))
        return;
    m_watched.insert(chaser->id());
    const quint32 id = chaser->id();
    // On the thread the step comes on (the MasterTimer's): closed at once.
    // The chaser's own signals: it is alive while they come.
    m_chaserConnections << connect(chaser, &Chaser::currentStepChanged, this,
                                   [this, chaser](int step) { hazardStepReached(chaser, step); }, Qt::DirectConnection);
    m_chaserConnections << connect(chaser, QOverload<quint32>::of(&Function::stopped), this,
                                   [this, chaser](quint32) { hazardStepReached(chaser, -1); }, Qt::DirectConnection);
    connect(chaser, &QObject::destroyed, this, [this, id]() { m_watched.remove(id); });
}

void HazardFastPath::detach()
{
    // seq_cst both: the other half of feed()'s handshake (R3, see there).
    m_alive.store(false, std::memory_order_seq_cst);
    while (m_users.load(std::memory_order_seq_cst) != 0)
        QThread::yieldCurrentThread();
    holdKey.store(kNone);
    killKey.store(kNone);
}

void OutputRuleKeeper::shutdownHazards()
{
    if (m_shutDown)
        return;
    m_shutDown = true;
    // No feed from an input thread reaches our flags after this.
    if (m_fast != nullptr)
        m_fast->detach();
    // No list's step or stop reaches us.
    for (const QMetaObject::Connection &c : std::as_const(m_chaserConnections))
        disconnect(c);
    m_chaserConnections.clear();
    // The fire layers' faders closed for good (they point at our gates),
    // then let go of.
    m_burstLayer->setGate(closedFaderGate());
    m_runningLayer->setGate(closedFaderGate());
    m_burstLayer->clear();
    m_runningLayer->clear();
}

void OutputRuleKeeper::hazardStepReached(Chaser *chaser, int step)
{
    const quint32 chaserId = chaser->id();
    Reach r;
    r.step = step;
    r.held = m_hazardHold.load(std::memory_order_acquire);
    r.armed = m_hazardArmed.load(std::memory_order_acquire);
    r.blind = m_doc->inputOutputMap() != nullptr && m_doc->inputOutputMap()->outputInhibited();
    bool close = false;
    bool grant = true;
    {
        QMutexLocker locker(&m_hazardMutex);
        if (m_hazardOrder.isEmpty())
            return;
        /* Granted per step, by the Run's stamp (fourth review R1/R2): a
           show list's step that ShowControl's GO, Back, jump or Sub GO
           brought, or that the list went on to by itself after one. A step
           from anywhere else -- a VC button, the programmer's step, a web
           start, the startup function, the editor's Test -- is no run. */
        Chaser *holder = m_runLists.value(chaserId, nullptr);
        const bool run = step >= 0 && holder != nullptr && holder->runStamped();
        auto was = m_reach.constFind(chaserId);
        const bool had = was != m_reach.constEnd() || m_granting.contains(chaserId);
        if (!run)
        {
            /* Not a run's (R4): no reach, nothing closed and no grant asked
               -- an effect chase stepping every 60 ms never touches the fire
               layers -- unless this list had a run's step, whose grant this
               step ends. */
            if (!had)
            {
                if (step < 0 || m_unrunPending.contains(chaserId))
                    return;
                m_unrunPending.insert(chaserId);
                grant = false;
            }
            else
            {
                m_reach.remove(chaserId);
                close = true;
            }
        }
        else
        {
            /* A step that is no new step (a VC cue list's crossfade ending
               says the same step again, N5): no new reach -- a skipped flame
               never fires late. */
            if (was != m_reach.constEnd() && was.value().step == step)
                return;
            /* Every step of a list that has a reach closes the grants (N6): a
               grant worked out for the step before never opens under this one. */
            close = had;
            m_reach.insert(chaserId, r);
        }
    }
    // What this list granted closes now, before its next cue writes.
    if (close)
        m_hazardStep.fetch_add(1, std::memory_order_acq_rel);
    const Qt::ConnectionType how = thread() == QThread::currentThread() ? Qt::DirectConnection : Qt::QueuedConnection;
    if (grant)
        QMetaObject::invokeMethod(this, [this, chaserId]() { grantHazards(chaserId); }, how);
    else
        QMetaObject::invokeMethod(this, [this, chaserId]() { noteUnrunStep(chaserId); }, how);
}

void OutputRuleKeeper::refreshRunLists()
{
    QHash<quint32, Chaser *> lists;
    if (ShowTreeController *st = m_doc->showTree())
        for (const ShowTree::Production &p : st->productions())
            for (const ShowTree::CueList &l : p.lists)
            {
                if (l.looks)
                    continue;
                Chaser *own = qobject_cast<Chaser *>(m_doc->function(l.chaser));
                if (own == nullptr)
                    continue;
                Chaser *holder = own;
                if (l.isSubCues())
                {
                    const ShowTree::CueList *parent = st->listOfCue(l.subOf);
                    holder = parent != nullptr ? qobject_cast<Chaser *>(m_doc->function(parent->chaser)) : nullptr;
                }
                if (holder != nullptr)
                    lists.insert(own->id(), holder);
            }
    QMutexLocker locker(&m_hazardMutex);
    m_runLists = lists;
}

void OutputRuleKeeper::noteUnrunStep(quint32 chaserId)
{
    QHash<quint32, Hazard> hazards;
    QList<quint32> order;
    {
        QMutexLocker locker(&m_hazardMutex);
        m_unrunPending.remove(chaserId);
        hazards = m_hazards;
        order = m_hazardOrder;
    }
    Chaser *c = qobject_cast<Chaser *>(m_doc->function(chaserId));
    if (c == nullptr || !c->isRunning())
        return;
    const QHash<quint32, QByteArray> levels = hazardStepLevels(chaserId, c->currentStepIndex());
    QList<quint32> skipped;
    for (quint32 id : std::as_const(order))
    {
        if (hazards.value(id).parked)
            continue;
        const QByteArray v = levels.value(id);
        if (std::any_of(v.cbegin(), v.cend(), [](char ch) { return ch != 0; }))
            skipped << id;
    }
    if (skipped.isEmpty())
        return;                 // the step sets no hazard: nothing to say
    QHash<quint32, int> why;
    for (quint32 id : std::as_const(skipped))
        why.insert(id, 2);
    if (skipped == m_hazardsSkipped && why == m_hazardSkippedBurst)
        return;
    m_hazardsSkipped = skipped;
    m_hazardSkippedBurst = why;
    for (quint32 id : std::as_const(skipped))
        logHazard(tr("%1 held: not fired from Run").arg(hazards.value(id).name));
    emitMachineryChanged();
    emitHazardsChanged();
}

uint OutputRuleKeeper::stepFadeMs(const Chaser *c, int step) const
{
    Chaser *ch = const_cast<Chaser *>(c);
    if (ch == nullptr || step < 0)
        return 0;
    ChaserStep *cs = ch->stepAt(step);
    if (cs == nullptr)
        return 0;
    uint ms = 0;
    switch (ch->fadeInMode())
    {
        case Chaser::Common:
            ms = ch->fadeInSpeed();
            break;
        case Chaser::PerStep:
            ms = cs->fadeIn;
            break;
        default:
            if (const Function *f = m_doc->function(cs->fid))
                ms = f->fadeInSpeed();
            break;
    }
    // Infinite (held for a manual move): no fade, at the level at once.
    return ms == Function::infiniteSpeed() ? 0 : ms;
}

QHash<quint32, QByteArray> OutputRuleKeeper::hazardStepLevels(quint32 chaserId, int step) const
{
    QHash<quint32, QByteArray> levels;
    Chaser *c = qobject_cast<Chaser *>(m_doc->function(chaserId));
    if (c == nullptr || step < 0)
        return levels;
    // A Show Tree list: its cue as it plays (tracked, included, its own).
    if (ShowTreeController *st = m_doc->showTree())
        for (const ShowTree::Production &p : st->productions())
            for (const ShowTree::CueList &l : p.lists)
            {
                if (l.looks || l.chaser != chaserId)
                    continue;
                quint32 cueId = Function::invalidId();
                if (!l.isSubCues())
                    cueId = st->cueOfStep(l.id, step);
                else if (step < l.cues.count())
                    cueId = l.cues.at(step).id;
                return cueId != Function::invalidId() ? st->cueLevels(cueId) : levels;
            }
    // Any other list: its step's scene, or the scenes of its collection.
    ChaserStep *cs = c->stepAt(step);
    if (cs == nullptr)
        return levels;
    QList<quint32> fids { cs->fid };
    for (int i = 0; i < fids.size() && i < 64; ++i)
    {
        Function *f = m_doc->function(fids.at(i));
        if (const Collection *coll = qobject_cast<const Collection *>(f))
            fids << coll->functions();
        else if (const Scene *sc = qobject_cast<const Scene *>(f))
        {
            const QList<SceneValue> vals = (cs->fid == sc->id() && !cs->values.isEmpty()) ? cs->values : sc->values();
            for (const SceneValue &v : vals)
            {
                const Fixture *fx = m_doc->fixture(v.fxi);
                if (fx == nullptr || v.channel >= fx->channels())
                    continue;
                QByteArray &b = levels[v.fxi];
                if (b.size() != int(fx->channels()))
                    b = QByteArray(int(fx->channels()), 0);
                b[int(v.channel)] = char(qMax(uchar(b.at(int(v.channel))), v.value));
            }
        }
    }
    return levels;
}

void OutputRuleKeeper::grantHazards(quint32 stepped)
{
    const quint32 epoch = m_hazardStep.load(std::memory_order_acquire);
    QHash<quint32, Reach> reach;
    QHash<quint32, Hazard> hazards;
    QList<quint32> order;
    {
        QMutexLocker locker(&m_hazardMutex);
        reach = m_reach;
        hazards = m_hazards;
        order = m_hazardOrder;
    }
    QHash<quint32, QByteArray> burst, running;
    QHash<quint32, uint> runningMs;
    QSet<quint32> granting;
    QList<quint32> skipped;
    QHash<quint32, int> skippedWhy;
    for (auto it = reach.constBegin(); it != reach.constEnd(); ++it)
    {
        Chaser *c = qobject_cast<Chaser *>(m_doc->function(it.key()));
        // Only while that step is current.
        if (c == nullptr || !c->isRunning() || c->currentStepIndex() != it.value().step)
            continue;
        /* Only a run's step is reached at all (N1, R1): hazardStepReached()
           records none for any other route. */
        const bool run = true;
        const uint fade = stepFadeMs(c, it.value().step);
        const QHash<quint32, QByteArray> levels = hazardStepLevels(it.key(), it.value().step);
        for (quint32 id : std::as_const(order))
        {
            const Hazard &h = hazards[id];
            const QByteArray v = levels.value(id);
            bool any = false;
            for (char ch : v)
                any = any || ch != 0;
            if (!any)
                continue;           // the cue sets it to 0, or not at all: 0
            const bool let = run && !it.value().blind && !h.parked
                             && (h.burst ? it.value().held : it.value().armed);
            if (!let)
            {
                if (it.key() == stepped && !h.parked && !skipped.contains(id))
                {
                    skipped << id;
                    skippedWhy.insert(id, !run ? 2 : h.burst ? 0 : 1);
                }
                continue;
            }
            if (!h.burst)
                runningMs.insert(id, fade);
            QHash<quint32, QByteArray> &into = h.burst ? burst : running;
            QByteArray &b = into[id];
            if (b.size() != v.size())
                b = QByteArray(v.size(), 0);
            for (int i = 0; i < v.size(); ++i)
                b[i] = char(qMax(uchar(b.at(i)), uchar(v.at(i))));
            granting.insert(it.key());
        }
    }
    /* A Running hazard armed at the step before that this one sets lower
       or not at all: it fades down with this step (N7), not cut -- a Kill,
       a stop or a loss still cut it (the gate). */
    {
        QMutexLocker locker(&m_hazardMutex);
        for (auto it = m_runningGrant.constBegin(); it != m_runningGrant.constEnd(); ++it)
        {
            if (running.contains(it.key()) || !hazards.contains(it.key()) || hazards.value(it.key()).parked)
                continue;
            if (!m_hazardArmed.load(std::memory_order_acquire))
                continue;
            uint ms = 0;
            if (stepped != Function::invalidId())
                if (Chaser *c = qobject_cast<Chaser *>(m_doc->function(stepped)))
                    if (reach.contains(stepped))
                        ms = stepFadeMs(c, reach.value(stepped).step);
            if (ms == 0)
                continue;
            running.insert(it.key(), QByteArray(it.value().size(), 0));
            runningMs.insert(it.key(), ms);
        }
        // Each Running grant fades from what the one before laid.
        QHash<quint32, QByteArray> from;
        for (auto it = running.constBegin(); it != running.constEnd(); ++it)
            from.insert(it.key(), m_runningGrant.value(it.key(), QByteArray(it.value().size(), 0)));
        m_runningFrom = from;
        m_runningFadeMs = runningMs;
        m_burstGrant = burst;
        m_runningGrant = running;
        m_granting = granting;
    }
    applyHazards();
    // Open only if no step, Kill or stop came meanwhile.
    m_hazardGrant.store(epoch, std::memory_order_release);
    for (quint32 id : std::as_const(skipped))
        logHazard(skippedWhy.value(id) == 2 ? tr("%1 held: not fired from Run").arg(hazards.value(id).name)
                  : skippedWhy.value(id) == 0 ? tr("%1 skipped: not held").arg(hazards.value(id).name)
                                              : tr("%1 skipped: not armed").arg(hazards.value(id).name));
    if (stepped != Function::invalidId() && (skipped != m_hazardsSkipped || skippedWhy != m_hazardSkippedBurst))
    {
        m_hazardsSkipped = skipped;
        m_hazardSkippedBurst = skippedWhy;
        emitMachineryChanged();
    }
    logHazardOutputs(tr("cue reached"));
    emitHazardsChanged();
}

QString OutputRuleKeeper::hazardSkippedText() const
{
    QStringList notHeld, notArmed, notRun;
    for (quint32 id : m_hazardsSkipped)
    {
        const Fixture *fx = m_doc->fixture(id);
        const QString name = fx != nullptr ? fx->name() : tr("A hazard");
        const int why = m_hazardSkippedBurst.value(id);
        (why == 2 ? notRun : why == 0 ? notHeld : notArmed) << name;
    }
    QStringList parts;
    if (!notRun.isEmpty())
        parts << tr("%1 held — fire it from Run").arg(notRun.join(QStringLiteral(", ")));
    if (!notHeld.isEmpty())
        parts << tr("%1 skipped — not held").arg(notHeld.join(QStringLiteral(", ")));
    if (!notArmed.isEmpty())
        parts << tr("%1 skipped — not armed").arg(notArmed.join(QStringLiteral(", ")));
    return parts.join(QStringLiteral(" · "));
}

OutputRuleKeeper::HazardState OutputRuleKeeper::hazardState() const
{
    {
        QMutexLocker locker(&m_hazardMutex);
        if (m_hazardOrder.isEmpty())
            return NoHazards;
    }
    if (hazardHeld())
        return HazardsHeld;
    return hazardsArmed() ? HazardsArmed : HazardsDisarmed;
}

QString OutputRuleKeeper::hazardChipText() const
{
    QString state;
    switch (hazardState())
    {
        case NoHazards:
            return QString();
        case HazardsHeld:
            state = hazardsArmed() ? tr("ARMED · HELD") : tr("HELD");
            break;
        case HazardsArmed:
            state = tr("ARMED");
            break;
        case HazardsDisarmed:
            state = tr("DISARMED");
            break;
    }
    QString text = tr("Hazards: %1").arg(state);
    if (m_holdTimedOut)
        text += QStringLiteral(" · ") + tr("Hold timed out — release and press again");
    const QString skipped = hazardSkippedText();
    if (!skipped.isEmpty())
        text += QStringLiteral(" · ") + skipped;
    return text;
}

void OutputRuleKeeper::noteHandOnHazard(quint32 fixtureId)
{
    bool burst = true;
    QString name;
    {
        QMutexLocker locker(&m_hazardMutex);
        auto it = m_hazards.constFind(fixtureId);
        if (it == m_hazards.constEnd())
            return;
        burst = it.value().burst;
        name = it.value().name;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_handSaidAt.value(fixtureId, 0) < 5000)
        return;
    m_handSaidAt.insert(fixtureId, now);
    emit handHeldOnHazard(burst
        ? tr("%1 is a hazard (Burst): a slider or the desk never drives it. Only a cue fires it, at the "
             "cue's level, while the spotter holds Hazard hold.").arg(name)
        : tr("%1 is a hazard (Running): a slider or the desk never drives it. Only a cue fires it, at "
             "the cue's level, once the spotter has armed it (Hazard arm).").arg(name));
}

QList<quint32> OutputRuleKeeper::hazardsCued() const
{
    QSet<quint32> hazards;
    {
        QMutexLocker locker(&m_hazardMutex);
        for (quint32 id : m_hazardOrder)
            hazards.insert(id);
    }
    if (hazards.isEmpty())
        return QList<quint32>();
    QSet<quint32> cued;
    for (Function *f : m_doc->functions())
    {
        const Scene *sc = qobject_cast<const Scene *>(f);
        if (sc == nullptr)
            continue;
        for (const SceneValue &v : sc->values())
            if (v.value > 0 && hazards.contains(v.fxi))
                cued.insert(v.fxi);
        // A Show Tree look: a palette on its targets.
        for (quint32 pid : sc->palettes())
        {
            QLCPalette *p = m_doc->palette(pid);
            if (p == nullptr)
                continue;
            QList<SceneValue> vals = p->valuesFromFixtures(m_doc, sc->fixtures(), sc);
            vals << p->valuesFromFixtureGroups(m_doc, sc->fixtureGroups(), sc);
            for (const SceneValue &v : std::as_const(vals))
                if (v.value > 0 && hazards.contains(v.fxi))
                    cued.insert(v.fxi);
        }
    }
    QList<quint32> out = cued.values();
    std::sort(out.begin(), out.end());
    return out;
}

QString OutputRuleKeeper::hazardLoadNotice() const
{
    const int n = int(hazardsCued().size());
    if (n == 0)
        return QString();
    return tr("%n hazard(s) stay off until the spotter lets them fire — Show Review lists them", "", n);
}

void OutputRuleKeeper::logHazard(const QString &line)
{
    if (line.isEmpty())
        return;
    auto add = [this, line]() {
        const QString stamped = QTime::currentTime().toString(QStringLiteral("HH:mm:ss")) + QLatin1Char(' ') + line;
        m_hazardLog << stamped;
        while (m_hazardLog.size() > kHazardLogLines)
            m_hazardLog.removeFirst();
        qInfo().noquote() << "hazard:" << line;
        if (GoLog *log = m_doc->goLog())
            log->note(tr("Hazard: %1").arg(line));
        emit hazardLogged(stamped);
    };
    if (thread() == QThread::currentThread())
        add();
    else
        QMetaObject::invokeMethod(this, add, Qt::QueuedConnection);
}

void OutputRuleKeeper::emitHazardsChanged()
{
    if (thread() == QThread::currentThread())
        emit hazardsChanged();
    else
        QMetaObject::invokeMethod(this, [this]() { emit hazardsChanged(); }, Qt::QueuedConnection);
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
    refreshRunLists();
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
