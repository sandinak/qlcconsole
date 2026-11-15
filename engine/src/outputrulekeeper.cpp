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
    connect(doc, &Doc::loaded, this, [this]() { scheduleRebuild(); });
    // Another show: a new show's setting, and OnAtOperate waits for its
    // own first Operate.
    connect(doc, &Doc::cleared, this, [this]() {
        // Another show: its own start, no emergency stop carried over.
        setEmergencyStop(false);
        m_onAtOpenWhen = kDefaultOnAtOpenWhen;
        m_seenOperate = m_doc->mode() == Doc::Operate;
        scheduleRebuild();
    });
    connect(doc, &Doc::modeChanged, this, [this](Doc::Mode mode) {
        /* Show only, Operate starting: a new session -- what was started
           or set in Design is not the show until a GO in Operate -- with
           every running function's gate worked out now, and only then the
           universes live. Before anything else hears of Operate. */
        if (mode == Doc::Operate)
        {
            Function::startLiveSession();
            if (m_doc->masterTimer() != nullptr)
                m_doc->masterTimer()->refreshPreviewGates();
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
    connect(doc->inputOutputMap(), &InputOutputMap::universeAdded, this, [this](quint32) { scheduleRebuild(); });
    connect(doc->inputOutputMap(), &InputOutputMap::universeRemoved, this, [this](quint32) { scheduleRebuild(); });
    m_seenOperate = doc->mode() == Doc::Operate;
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
                ua.at(u)->markShowOnly(int(abs & 0x1ff), false, 0);
        }
    }
    if (showOnly)
    {
        for (quint32 i = 0; i < fx->channels(); i++)
        {
            const quint32 abs = fx->universeAddress() + i;
            const int u = int(abs >> 9);
            if (u < ua.size() && ua.at(u) != nullptr)
                ua.at(u)->markShowOnly(int(abs & 0x1ff), true, safeValue(fx, i));
        }
        m_showOnlyAt.insert(id, qMakePair(fx->universeAddress(), fx->channels()));
    }
    else
    {
        m_showOnlyAt.remove(id);
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
    if (m_onAtOpenWhen == kDefaultOnAtOpenWhen)
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

void OutputRuleKeeper::emergencyStop()
{
    /* Every Show-only channel out at once -- no fade -- and held there,
       whatever drives it (Park, the Simple Desk, a VC slider), until the
       next GO in Operate. The stop cleans up what it stops: a new session
       (what ran before is not the show after), every hand's fader left up
       frozen until moved again, the stored values back to safe, every
       last look without them. Nothing from before the stop comes back when
       it lets go. Safe from any thread (a plugin's: OSC, MSC). */
    const quint32 stopBefore = GenericFader::stopEpoch();
    GenericFader::bumpStopEpoch();
    Function::startLiveSession();
    if (m_doc->masterTimer() != nullptr)
        m_doc->masterTimer()->refreshPreviewGates();
    m_emergencyStop.store(true, std::memory_order_relaxed);
    const QList<Universe *> ua = m_doc->inputOutputMap()->universesSnapshot();
    for (Universe *u : ua)
    {
        if (u == nullptr)
            continue;
        u->freezeFadersForStop(stopBefore);
        u->setShowOnlyEmergencyStop(true);      // and the stored values to safe
    }
    // At once, not at the next frame.
    for (Universe *u : ua)
        if (u != nullptr && u->showOnlyChannelCount() > 0)
            u->dumpOutput(u->postGMValues()->mid(0, u->usedChannels()), true);
    // The last look and the footer: on the keeper's own thread.
    QMetaObject::invokeMethod(this, [this]() {
        if (m_doc->lastLook() != nullptr)
            m_doc->lastLook()->dropShowOnly();
        m_lastHeldBy = showOnlyHeldBy();
        emit showOnlyChanged();
        emit emergencyStopChanged(true);
    }, thread() == QThread::currentThread() ? Qt::DirectConnection : Qt::QueuedConnection);
}

void OutputRuleKeeper::releaseEmergencyStop()
{
    if (emergencyStopped() && m_doc->mode() == Doc::Operate)
        setEmergencyStop(false);
}

void OutputRuleKeeper::setEmergencyStop(bool stop)
{
    if (m_emergencyStop.exchange(stop, std::memory_order_relaxed) == stop)
        return;
    for (Universe *u : m_doc->inputOutputMap()->universesSnapshot())
        if (u != nullptr)
            u->setShowOnlyEmergencyStop(stop);
    m_lastHeldBy = showOnlyHeldBy();
    emit showOnlyChanged();
    emit emergencyStopChanged(stop);
}

OutputRuleKeeper::HeldBy OutputRuleKeeper::showOnlyHeldBy() const
{
    if (m_showOnly.isEmpty())
        return NotHeld;
    if (emergencyStopped())
        return HeldByEmergencyStop;
    if (m_doc->mode() != Doc::Operate)
        return HeldByDesign;
    if (m_doc->inputOutputMap()->outputInhibited())
        return HeldByBlind;
    return NotHeld;
}

QString OutputRuleKeeper::showOnlyHeldText() const
{
    const HeldBy by = showOnlyHeldBy();
    if (by == NotHeld)
        return QString();
    const QString why = by == HeldByEmergencyStop ? tr("emergency stop, until the next GO")
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
    foreach (Fixture *fx, m_doc->fixtures())
        if ((fx->outputRules() & Fixture::ShowOnly) && fx->isPatched())
            m_showOnlyAt.insert(fx->id(), qMakePair(fx->universeAddress(), fx->channels()));
    m_count = count;
    const bool showOnlyListChanged = showOnly != m_showOnly;
    m_showOnly = showOnly;
    // With none, a function never walks what started it (Function).
    Function::setShowOnlyInUse(showOnly.isEmpty() == false);

    InputOutputMap *io = m_doc->inputOutputMap();
    const int n = int(io->universesCount());
    QVector<QByteArray> bits, safe;
    for (int i = 0; i < n; i++)
    {
        bits << bitsFor(quint32(i));
        safe << (showOnly.isEmpty() ? QByteArray() : safeValuesFor(quint32(i)));
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
        u->setOutputRules(i < bits.size() ? bits.at(i) : QByteArray());
        u->setShowOnlyLive(m_doc->mode() == Doc::Operate);
        u->setShowOnlyEmergencyStop(emergencyStopped());
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
