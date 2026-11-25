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

#define KXMLQLCOutputRuleOnAtOpen QStringLiteral("OnAtOpen")

OutputRuleKeeper::OutputRuleKeeper(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
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
                ua.at(u)->setShowOnlyHoldsPosition(int(abs & 0x1ff), FixtureRest::holdsPosition(fx->channel(i)));
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
    // Q326: the one rule the Simple Desk and the VC sliders keep too.
    QByteArray pos(UNIVERSE_SIZE, char(0));
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if ((fx->outputRules() & Fixture::ShowOnly) == 0 || fx->isPatched() == false)
            continue;
        for (quint32 i = 0; i < fx->channels(); i++)
        {
            const quint32 abs = fx->universeAddress() + i;
            if ((abs >> 9) == universe && FixtureRest::holdsPosition(fx->channel(i)))
                pos[int(abs & 0x1ff)] = char(1);
        }
    }
    return pos;
}

void OutputRuleKeeper::emergencyStop()
{
    /* Q326: a Show-only head stays where it is -- its pan and tilt held at
       what went out last, before the new session, so no frame between
       sends it to its safe value. Only the rest goes safe. */
    const QList<Universe *> held = m_doc->inputOutputMap()->universesSnapshot();
    for (Universe *u : held)
        if (u != nullptr && u->showOnlyChannelCount() > 0)
            u->holdShowOnlyPositions();
    // E2: a new live session -- every stamp from before is old now, so
    // every writer is held from its next write on (a function's gate at
    // its next tick: the generation moved).
    Function::startLiveSession();
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
    if (m_stopEpoch.exchange(0, std::memory_order_relaxed) != 0)
        QMetaObject::invokeMethod(this, [this]() {
            m_lastHeldBy = showOnlyHeldBy();
            emit showOnlyChanged();
            emit emergencyStopEnded();
        }, thread() == QThread::currentThread() ? Qt::DirectConnection : Qt::QueuedConnection);
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
