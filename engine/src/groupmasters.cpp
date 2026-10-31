/*
  Q Light Controller Plus
  groupmasters.cpp

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

#include <QByteArray>
#include <QHash>
#include <QTimer>

#include "inputoutputmap.h"
#include "qlcfixturehead.h"
#include "groupmasters.h"
#include "fixturegroup.h"
#include "universe.h"
#include "fixture.h"
#include "doc.h"

GroupMasters::GroupMasters(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
    connect(doc, &Doc::fixtureGroupAdded, this, &GroupMasters::slotGroupAdded);
    connect(doc, &Doc::fixtureGroupRemoved, this, [this](quint32) { scheduleRebuild(); emit changed(); });
    connect(doc, &Doc::fixtureGroupChanged, this, [this](quint32) { scheduleRebuild(); emit changed(); });
    connect(doc, &Doc::fixtureAdded, this, [this](quint32) { scheduleRebuild(); });
    connect(doc, &Doc::fixtureRemoved, this, [this](quint32) { scheduleRebuild(); });
    connect(doc, &Doc::fixtureChanged, this, [this](quint32) { scheduleRebuild(); });
    connect(doc, &Doc::loaded, this, [this]() { scheduleRebuild(); emit changed(); });
    connect(doc, &Doc::cleared, this, [this]() { scheduleRebuild(); emit changed(); });
    connect(doc->inputOutputMap(), &InputOutputMap::universeAdded, this, [this](quint32) { scheduleRebuild(); });
}

QList<FixtureGroup *> GroupMasters::groups() const
{
    QList<FixtureGroup *> out;
    foreach (FixtureGroup *grp, m_doc->fixtureGroups())   // a QMap's values: id order
        if (grp->hasMaster())
            out << grp;
    return out;
}

void GroupMasters::resetLevels()
{
    foreach (FixtureGroup *grp, groups())
        grp->setMasterLevel(UCHAR_MAX);
}

void GroupMasters::slotGroupAdded(quint32 id)
{
    FixtureGroup *grp = m_doc->fixtureGroup(id);
    if (grp != nullptr)
        connect(grp, &FixtureGroup::masterChanged, this, &GroupMasters::slotMasterChanged,
                Qt::UniqueConnection);
    scheduleRebuild();
    emit changed();
}

void GroupMasters::slotMasterChanged(quint32 id)
{
    // A fader: at once, so the stage follows the hand.
    rebuild();
    if (FixtureGroup *grp = m_doc->fixtureGroup(id))
        emit levelChanged(id, grp->masterLevel());
}

void GroupMasters::scheduleRebuild()
{
    if (m_pending)
        return;
    m_pending = true;
    QTimer::singleShot(0, this, [this]() { rebuild(); });
}

void GroupMasters::rebuild()
{
    m_pending = false;

    /* Each universe touched: one byte per channel, 255 = no master. A
       channel twice in one group (its head and the fixture's master
       dimmer) is scaled once; in two groups, by both. */
    QHash<quint32, QByteArray> scales;
    foreach (FixtureGroup *grp, groups())
    {
        const uint level = grp->masterLevel();
        if (level == UCHAR_MAX)
            continue;

        QSet<quint32> channels;              // absolute addresses
        QHash<quint32, int> headsOf;         // fixture -> heads of it in the group
        foreach (const GroupHead &gh, grp->headList())
        {
            Fixture *fxi = m_doc->fixture(gh.fxi);
            if (fxi == nullptr || !fxi->isPatched())
                continue;
            headsOf[gh.fxi] += 1;
            foreach (quint32 ch, fxi->head(gh.head).channels())
                channels.insert(fxi->universeAddress() + ch);
        }
        for (auto it = headsOf.constBegin(); it != headsOf.constEnd(); ++it)
        {
            Fixture *fxi = m_doc->fixture(it.key());
            if (it.value() < fxi->heads() && fxi->heads() > 0)
                continue;                    // part of it: its own dimmer is not the group's
            QSet<quint32> inHeads;
            for (int h = 0; h < fxi->heads(); ++h)
                foreach (quint32 ch, fxi->head(h).channels())
                    inHeads.insert(ch);
            for (quint32 ch = 0; ch < fxi->channels(); ++ch)
                if (!inHeads.contains(ch))
                    channels.insert(fxi->universeAddress() + ch);
        }

        foreach (quint32 abs, channels)
        {
            const quint32 uni = abs >> 9;
            const int c = int(abs & 0x1FF);
            QByteArray &bytes = scales[uni];
            if (bytes.isEmpty())
                bytes = QByteArray(UNIVERSE_SIZE, char(UCHAR_MAX));
            const uint now = uchar(bytes.at(c));
            bytes[c] = char((now * level + 127) / 255);
        }
    }

    InputOutputMap *io = m_doc->inputOutputMap();
    QList<Universe *> ua = io->claimUniverses();
    QSet<quint32> scaled;
    for (int i = 0; i < ua.size(); ++i)
    {
        const quint32 u = quint32(i);
        if (ua.at(i) == nullptr || (!scales.contains(u) && !m_scaled.contains(u)))
            continue;
        ua.at(i)->setGroupMasters(scales.value(u));
        if (scales.contains(u))
            scaled.insert(u);
    }
    io->releaseUniverses(false);
    m_scaled = scaled;
}
