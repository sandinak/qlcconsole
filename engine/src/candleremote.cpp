/*
  Q Light Controller Plus
  candleremote.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "candleremote.h"
#include "qlcfixturedef.h"
#include "stagetarget.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"

CandleRemote::Command CandleRemote::command(int select)
{
    return (select >= int(On) && select <= int(Brighten)) ? Command(select) : Nothing;
}

CandleRemote::State CandleRemote::apply(State s, Command c)
{
    switch (c)
    {
    case On:
        s.known = true;
        s.lit = true;
        s.dimmed = false;       // "it starts full on every start"
        break;
    case Off:
        s.known = true;
        s.lit = false;
        break;
    case Candle:
    case Light:
        s.known = true;
        s.lit = true;
        s.flicker = (c == Candle);
        break;
    case Dim:
        s.dimmed = true;        // one step: the real level is unknowable
        break;
    case Brighten:
        s.dimmed = false;
        break;
    case Nothing:
    default:
        break;
    }
    return s;
}

QString CandleRemote::manufacturer()
{
    return QStringLiteral("Generic");
}

QString CandleRemote::model()
{
    return QStringLiteral("IR Candle Remote");
}

bool CandleRemote::isRemote(const Fixture *fx)
{
    if (fx == nullptr)
        return false;
    const QLCFixtureDef *def = fx->fixtureDef();
    return def != nullptr && def->manufacturer() == manufacturer() && def->model() == model();
}

CandleRemote *CandleRemote::of(Doc *doc)
{
    if (doc == nullptr)
        return nullptr;
    if (CandleRemote *r = doc->findChild<CandleRemote *>(QString(), Qt::FindDirectChildrenOnly))
        return r;
    return new CandleRemote(doc);
}

CandleRemote::CandleRemote(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
    connect(doc, &Doc::fixtureAdded, this, [this](quint32 fid) { watch(fid); });
    connect(doc, &Doc::fixtureChanged, this, [this](quint32 fid) { watch(fid); });
    connect(doc, &Doc::fixtureRemoved, this, [this](quint32 fid) {
        QObject::disconnect(m_watching.take(fid));
        if (m_seen.remove(fid))
            emit changed();
    });
    connect(doc, &Doc::cleared, this, &CandleRemote::reset);
    /* Made after the show opened: what SELECT is held at now is the last
       button sent (a look parks it there), as near as can be known. */
    for (Fixture *fx : doc->fixtures())
        watch(fx->id());
}

CandleRemote::~CandleRemote()
{
}

void CandleRemote::watch(quint32 fid)
{
    Fixture *fx = m_doc->fixture(fid);
    if (!isRemote(fx))
    {
        QObject::disconnect(m_watching.take(fid));
        m_seen.remove(fid);
        return;
    }
    if (!m_watching.contains(fid))
        m_watching.insert(fid, connect(fx, &Fixture::valuesChanged, this, [this, fid]() { readValues(fid); }));
    readValues(fid);
}

void CandleRemote::readValues(quint32 fid)
{
    Fixture *fx = m_doc->fixture(fid);
    if (fx == nullptr || fx->channels() < 2)
        return;
    // SELECT and WINDOW by name, else the first two channels.
    int sel = 0, win = 1;
    for (quint32 c = 0; c < fx->channels(); ++c)
        if (const QLCChannel *ch = fx->channel(c))
        {
            if (ch->name().compare(QLatin1String("Select"), Qt::CaseInsensitive) == 0)
                sel = int(c);
            else if (ch->name().compare(QLatin1String("Window"), Qt::CaseInsensitive) == 0)
                win = int(c);
        }
    feed(fid, fx->channelValueAt(sel), fx->channelValueAt(win));
}

void CandleRemote::feed(quint32 fid, int select, int window)
{
    Seen &s = m_seen[fid];
    if (s.select == select && s.window == window)
        return;                 // held: the blaster sends nothing new
    s.select = select;
    s.window = window;
    const State was = s.state;
    s.state = apply(s.state, command(select));
    if (s.state != was)
        emit changed();
}

void CandleRemote::reset()
{
    for (auto it = m_seen.begin(); it != m_seen.end(); ++it)
        *it = Seen();
    emit changed();
}

QList<quint32> CandleRemote::remotes() const
{
    QList<quint32> out;
    for (Fixture *fx : m_doc->fixtures())
        if (isRemote(fx))
            out << fx->id();
    return out;
}

quint32 CandleRemote::remoteFor(const StageTarget *candle) const
{
    if (candle == nullptr)
        return UINT_MAX;
    if (candle->litBy() != UINT_MAX && isRemote(m_doc->fixture(candle->litBy())))
        return candle->litBy();
    const QList<quint32> all = remotes();
    return all.size() == 1 ? all.first() : UINT_MAX;
}

CandleRemote::State CandleRemote::state(quint32 fid) const
{
    return m_seen.value(fid).state;
}

CandleRemote::State CandleRemote::stateFor(const StageTarget *candle) const
{
    const quint32 fid = remoteFor(candle);
    return fid == UINT_MAX ? State() : state(fid);
}
