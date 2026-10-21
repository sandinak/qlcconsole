/*
  Q Light Controller Plus
  shutterautoopen.cpp

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

#include "shutterautoopen.h"
#include "inputoutputmap.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"

#define KXMLQLCShutterAutoOpenEnabled QStringLiteral("Enabled")

ShutterAutoOpen::ShutterAutoOpen(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
    connect(doc, &Doc::fixtureAdded, this, [this](quint32) { scheduleRebuild(); });
    connect(doc, &Doc::fixtureRemoved, this, [this](quint32) { scheduleRebuild(); });
    connect(doc, &Doc::fixtureChanged, this, [this](quint32) { scheduleRebuild(); });
    connect(doc, &Doc::loaded, this, [this]() { scheduleRebuild(); });
    connect(doc->inputOutputMap(), &InputOutputMap::universeAdded, this, [this](quint32) { scheduleRebuild(); });
    connect(doc->inputOutputMap(), &InputOutputMap::universeRemoved, this, [this](quint32) { scheduleRebuild(); });
}

void ShutterAutoOpen::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;
    m_doc->setModified();
    rebuild();
    emit enabledChanged(on);
}

void ShutterAutoOpen::reset()
{
    if (m_enabled == false)
    {
        m_enabled = true;
        emit enabledChanged(true);
    }
    scheduleRebuild();
}

void ShutterAutoOpen::scheduleRebuild()
{
    if (m_pending)
        return;
    m_pending = true;
    QTimer::singleShot(0, this, [this]() { rebuild(); });
}

static bool isIris(const QLCChannel *ch)
{
    return ch->preset() == QLCChannel::ShutterIrisMinToMax
        || ch->preset() == QLCChannel::ShutterIrisMaxToMin
        || ch->preset() == QLCChannel::ShutterIrisFine;
}

/* A capability that means "the beam is open", by name, for definitions that
   never tagged their capabilities: "Open", "Shutter open", "Open (shutter)".
   Not "Slow open - fast close" or "Pulse open": those are effects. */
static bool namedOpen(const QLCCapability *cap)
{
    QString n = cap->name().toLower().simplified();
    n.remove(QLatin1Char('(')).remove(QLatin1Char(')'));
    return n == QLatin1String("open") || n == QLatin1String("shutter open")
        || n == QLatin1String("open shutter") || n == QLatin1String("opened")
        || n == QLatin1String("shutter opened") || n == QLatin1String("beam open");
}

int ShutterAutoOpen::openValue(const QLCChannel *ch)
{
    if (ch == nullptr || ch->group() != QLCChannel::Shutter
        || ch->controlByte() != QLCChannel::MSB || isIris(ch))
        return -1;

    // A plain strobe channel: 0 is the beam open, the rest strobes.
    if (ch->preset() == QLCChannel::ShutterStrobeSlowFast
        || ch->preset() == QLCChannel::ShutterStrobeFastSlow)
        return 0;

    const QLCCapability *atDefault = ch->searchCapability(ch->defaultValue());
    if (atDefault != nullptr && (atDefault->preset() == QLCCapability::ShutterOpen
                                 || namedOpen(atDefault)))
        return ch->defaultValue();

    foreach (const QLCCapability *cap, ch->capabilities())
        if (cap->preset() == QLCCapability::ShutterOpen)
            return cap->middle();
    foreach (const QLCCapability *cap, ch->capabilities())
        if (namedOpen(cap))
            return cap->middle();
    return -1;
}

QVector<Universe::AutoOpenRule> ShutterAutoOpen::rulesFor(const Fixture *fx)
{
    QVector<Universe::AutoOpenRule> rules;
    const QLCFixtureMode *mode = fx != nullptr ? fx->fixtureMode() : nullptr;
    if (mode == nullptr)
        return rules;

    const int base = int(fx->address());
    auto addr = [base](quint32 ch) {
        return ch == QLCChannel::invalid() ? -1 : base + int(ch);
    };
    auto inUniverse = [](int a) { return a >= 0 && a < UNIVERSE_SIZE; };

    // Each head's level: its dimmer and the fixture's master dimmer; a head
    // with no dimmer of its own is lit by any of its colours.
    const int master = addr(fx->masterIntensityChannel());
    QVector<Universe::AutoOpenHead> heads;
    QVector<QList<quint32> > headChannels;
    const QVector<QLCFixtureHead> &modeHeads = mode->heads();
    for (int h = 0; h < modeHeads.size(); h++)
    {
        const QLCFixtureHead &head = modeHeads.at(h);
        Universe::AutoOpenHead level;
        if (inUniverse(master))
            level.gates << master;
        const int dimmer = addr(head.channelNumber(QLCChannel::Intensity, QLCChannel::MSB));
        if (inUniverse(dimmer) && dimmer != master)
            level.gates << dimmer;
        if (dimmer < 0)
        {
            foreach (quint32 c, head.channels())
            {
                const QLCChannel *ch = fx->channel(c);
                if (ch != nullptr && ch->group() == QLCChannel::Intensity
                    && ch->colour() != QLCChannel::NoColour
                    && ch->controlByte() == QLCChannel::MSB && inUniverse(addr(c)))
                    level.anyOf << addr(c);
            }
        }
        heads << level;
        headChannels << head.channels();
    }
    if (heads.isEmpty())
    {
        // No heads: the whole fixture is one.
        Universe::AutoOpenHead level;
        for (quint32 c = 0; c < fx->channels(); c++)
        {
            const QLCChannel *ch = fx->channel(c);
            if (ch == nullptr || ch->group() != QLCChannel::Intensity
                || ch->controlByte() != QLCChannel::MSB || inUniverse(addr(c)) == false)
                continue;
            if (ch->colour() == QLCChannel::NoColour)
                level.gates << addr(c);
            else
                level.anyOf << addr(c);
        }
        heads << level;
        headChannels << QList<quint32>();
    }

    for (quint32 c = 0; c < fx->channels(); c++)
    {
        const int open = openValue(fx->channel(c));
        const int a = addr(c);
        if (open < 0 || inUniverse(a) == false)
            continue;
        Universe::AutoOpenRule rule;
        rule.shutter = a;
        rule.open = uchar(open);
        // The heads it belongs to; one in no head shutters them all.
        for (int h = 0; h < heads.size(); h++)
            if (headChannels.at(h).contains(c))
                rule.heads << heads.at(h);
        if (rule.heads.isEmpty())
            rule.heads = heads;
        // A head with nothing that gives it a level never opens anything.
        for (int h = rule.heads.size() - 1; h >= 0; h--)
            if (rule.heads.at(h).gates.isEmpty() && rule.heads.at(h).anyOf.isEmpty())
                rule.heads.remove(h);
        if (rule.heads.isEmpty() == false)
            rules << rule;
    }
    return rules;
}

void ShutterAutoOpen::rebuild()
{
    m_pending = false;
    InputOutputMap *io = m_doc->inputOutputMap();
    QHash<quint32, QVector<Universe::AutoOpenRule> > byUniverse;
    int count = 0;
    if (m_enabled)
    {
        foreach (Fixture *fx, m_doc->fixtures())
        {
            if (fx->isPatched() == false)
                continue;
            const QVector<Universe::AutoOpenRule> rules = rulesFor(fx);
            byUniverse[fx->universe()] << rules;
            count += rules.size();
        }
    }
    m_ruleCount = count;

    QList<Universe *> ua = io->claimUniverses();
    for (int i = 0; i < ua.size(); ++i)
        if (ua.at(i) != nullptr)
            ua.at(i)->setAutoOpenRules(byUniverse.value(quint32(i)));
    io->releaseUniverses(false);
}

void ShutterAutoOpen::saveXML(QXmlStreamWriter *doc) const
{
    // Only when it differs from a new show's: shows that never touched it
    // save as they always did, and open with it on.
    if (m_enabled)
        return;
    doc->writeStartElement(KXMLQLCShutterAutoOpen);
    doc->writeAttribute(KXMLQLCShutterAutoOpenEnabled, QStringLiteral("0"));
    doc->writeEndElement();
}

void ShutterAutoOpen::loadXML(QXmlStreamReader &root)
{
    const QString v = root.attributes().value(KXMLQLCShutterAutoOpenEnabled).toString();
    const bool on = v.isEmpty() || v != QLatin1String("0");
    root.skipCurrentElement();
    if (on != m_enabled)
    {
        m_enabled = on;
        emit enabledChanged(on);
    }
    scheduleRebuild();
}
