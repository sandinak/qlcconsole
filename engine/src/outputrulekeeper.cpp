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
#include <QHash>

#include "outputrulekeeper.h"
#include "inputoutputmap.h"
#include "qlcchannel.h"
#include "universe.h"

#define KXMLQLCOutputRuleOnAtOpen QStringLiteral("OnAtOpen")

OutputRuleKeeper::OutputRuleKeeper(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
    connect(doc, &Doc::fixtureAdded, this, [this](quint32) { scheduleRebuild(); });
    connect(doc, &Doc::fixtureRemoved, this, [this](quint32) { scheduleRebuild(); });
    connect(doc, &Doc::fixtureChanged, this, [this](quint32) { scheduleRebuild(); });
    connect(doc, &Doc::loaded, this, [this]() { scheduleRebuild(); });
    // Another show: a new show's setting, and OnAtOperate waits for its
    // own first Operate.
    connect(doc, &Doc::cleared, this, [this]() {
        m_onAtOpenWhen = kDefaultOnAtOpenWhen;
        m_seenOperate = m_doc->mode() == Doc::Operate;
        scheduleRebuild();
    });
    connect(doc, &Doc::modeChanged, this, [this](Doc::Mode mode) {
        if (mode == Doc::Operate && m_seenOperate == false)
        {
            m_seenOperate = true;
            pushPolicy();
        }
    });
    connect(doc->inputOutputMap(), &InputOutputMap::universeAdded, this, [this](quint32) { scheduleRebuild(); });
    connect(doc->inputOutputMap(), &InputOutputMap::universeRemoved, this, [this](quint32) { scheduleRebuild(); });
    m_seenOperate = doc->mode() == Doc::Operate;
}

void OutputRuleKeeper::scheduleRebuild()
{
    if (m_pending)
        return;
    m_pending = true;
    QTimer::singleShot(0, this, [this]() { rebuild(); });
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
    if (rule == Fixture::ExemptBlackout || rule == Fixture::ExemptGrandMaster)
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
        if (rules & Fixture::ExemptBlackout)
            mark(Fixture::ExemptBlackout, char(Universe::BlackoutExempt));
        if (rules & Fixture::Switch)
            mark(Fixture::Switch, char(Universe::SwitchOutput));
        if (rules & Fixture::OnAtOpen)
            mark(Fixture::OnAtOpen, char(Universe::OnAtOpenOutput));
    }
    return bits;
}

void OutputRuleKeeper::rebuild()
{
    m_pending = false;
    int count = 0;
    foreach (Fixture *fx, m_doc->fixtures())
        if (fx->outputRules() != Fixture::OutputRules())
            ++count;
    m_count = count;

    InputOutputMap *io = m_doc->inputOutputMap();
    const int n = int(io->universesCount());
    QVector<QByteArray> bits;
    for (int i = 0; i < n; i++)
        bits << bitsFor(quint32(i));

    const bool armed = onAtOpenArmed();
    QList<Universe *> ua = io->claimUniverses();
    for (int i = 0; i < ua.size(); ++i)
    {
        Universe *u = ua.at(i);
        if (u == nullptr)
            continue;
        u->setOnAtOpenArmed(armed);
        u->setBlindHoldsExempt(m_blindHoldsExempt);
        u->setOutputRules(i < bits.size() ? bits.at(i) : QByteArray());
    }
    io->releaseUniverses(false);
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
    QStringList out;
    if (ignores.isEmpty() == false)
        out << tr("ignores %1").arg(ignores.join(tr(", ")));
    out << more;
    QString text = out.join(QStringLiteral("; "));
    if (text.isEmpty() == false)
        text[0] = text.at(0).toUpper();
    return text;
}
