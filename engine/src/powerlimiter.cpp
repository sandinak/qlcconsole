/*
  Q Light Controller Plus
  powerlimiter.cpp

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

#include <QTimer>

#include "inputoutputmap.h"
#include "powerlimiter.h"
#include "universe.h"
#include "fixture.h"
#include "doc.h"

PowerLimiter::PowerLimiter(Doc *doc)
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

void PowerLimiter::scheduleRebuild()
{
    if (m_pending)
        return;
    m_pending = true;
    QTimer::singleShot(0, this, [this]() { rebuild(); });
}

void PowerLimiter::setMeasuring(quint32 fid, bool on)
{
    if (on)
        m_measuring.insert(fid);
    else
        m_measuring.remove(fid);
    rebuild();
}

void PowerLimiter::rebuild()
{
    m_pending = false;
    InputOutputMap *io = m_doc->inputOutputMap();
    QHash<quint32, QVector<Universe::PowerLimit> > byUniverse;
    int count = 0;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        const PowerProfile pp = fx->powerProfile();
        if (pp.maxAmps <= 0.0 || !fx->isPatched() || m_measuring.contains(fx->id()))
            continue;
        if (fx->address() + fx->channels() > 512)
            continue;                       // runs into the next universe: not modelled
        const PowerModel model = PowerModel::build(fx, pp);
        if (!model.isValid())
            continue;
        byUniverse[fx->universe()] << Universe::PowerLimit{ fx->id(), int(fx->address()), model };
        ++count;
    }
    m_limitedCount = count;

    QList<Universe *> ua = io->claimUniverses();
    for (int i = 0; i < ua.size(); ++i)
        if (ua.at(i) != nullptr)
            ua.at(i)->setPowerLimits(byUniverse.value(quint32(i)));
    io->releaseUniverses(false);
}

QHash<quint32, QPair<double, double> > PowerLimiter::heldBack() const
{
    QHash<quint32, QPair<double, double> > out;
    InputOutputMap *io = m_doc->inputOutputMap();
    QList<Universe *> ua = io->claimUniverses();
    foreach (Universe *u, ua)
        if (u != nullptr)
        {
            const QHash<quint32, QPair<double, double> > h = u->powerHeldBack();
            for (auto it = h.constBegin(); it != h.constEnd(); ++it)
                out.insert(it.key(), it.value());
        }
    io->releaseUniverses(false);
    return out;
}
