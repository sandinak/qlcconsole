/*
  Q Light Controller Plus
  presses.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "presses.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "collection.h"
#include "chaserstep.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

namespace Presses
{

QSet<Key> channels(Doc *doc)
{
    QSet<Key> out;
    if (doc == nullptr)
        return out;
    for (Fixture *fx : doc->fixtures())
    {
        if (fx == nullptr)
            continue;
        const quint32 n = fx->channels();
        for (quint32 i = 0; i < n; ++i)
        {
            const QLCChannel *ch = fx->channel(i);
            if (ch != nullptr && ch->hasPress())
                out.insert(key(fx->id(), i));
        }
    }
    return out;
}

static const QLCChannel *channelOfKey(Doc *doc, Key k)
{
    const Fixture *fx = doc == nullptr ? nullptr : doc->fixture(fixtureOf(k));
    return fx == nullptr ? nullptr : fx->channel(channelOf(k));
}

bool isPress(Doc *doc, Key k, uchar value)
{
    const QLCChannel *ch = channelOfKey(doc, k);
    return ch != nullptr && ch->isPressValue(value);
}

QString buttonName(Doc *doc, Key k, uchar value)
{
    const QLCChannel *ch = channelOfKey(doc, k);
    const QLCCapability *cap = ch == nullptr ? nullptr : ch->searchCapability(value);
    return cap == nullptr || cap->name().isEmpty() ? QString::number(value) : cap->name();
}

static void collect(Doc *doc, quint32 fid, const QSet<Key> &among, QHash<Key, uchar> &out,
                    QSet<quint32> &seen)
{
    if (seen.contains(fid))
        return;                                 // a collection in itself
    seen.insert(fid);
    Function *f = doc->function(fid);
    if (const Scene *s = qobject_cast<const Scene *>(f))
    {
        for (const SceneValue &sv : s->values())
        {
            const Key k = key(sv.fxi, sv.channel);
            if (among.contains(k))
                out.insert(k, sv.value);
        }
    }
    else if (const Collection *c = qobject_cast<const Collection *>(f))
    {
        for (quint32 member : c->functions())
            collect(doc, member, among, out, seen);
    }
    seen.remove(fid);
}

QHash<Key, uchar> written(Doc *doc, quint32 functionId, const QSet<Key> &among)
{
    QHash<Key, uchar> out;
    if (doc == nullptr || among.isEmpty())
        return out;
    QSet<quint32> seen;
    collect(doc, functionId, among, out, seen);
    return out;
}

static bool repeatsIn(Doc *doc, quint32 fid, const QSet<Key> &among, Key *pressKey, uchar *pressValue,
                      QSet<quint32> &seen)
{
    if (seen.contains(fid))
        return false;
    seen.insert(fid);
    Function *f = doc->function(fid);
    auto pressIn = [&](const QHash<Key, uchar> &values) {
        for (auto it = values.constBegin(); it != values.constEnd(); ++it)
            if (isPress(doc, it.key(), it.value()))
            {
                if (pressKey != nullptr)
                    *pressKey = it.key();
                if (pressValue != nullptr)
                    *pressValue = it.value();
                return true;
            }
        return false;
    };
    bool found = false;
    if (const Chaser *c = qobject_cast<const Chaser *>(f))
    {
        for (const ChaserStep &step : c->steps())
        {
            QHash<Key, uchar> values;
            for (const SceneValue &sv : step.values)    // a Sequence's step
                if (among.contains(key(sv.fxi, sv.channel)))
                    values.insert(key(sv.fxi, sv.channel), sv.value);
            if (pressIn(values) || pressIn(written(doc, step.fid, among))
                || repeatsIn(doc, step.fid, among, pressKey, pressValue, seen))
            {
                found = true;
                break;
            }
        }
    }
    else if (const Collection *coll = qobject_cast<const Collection *>(f))
    {
        for (quint32 member : coll->functions())
            if (repeatsIn(doc, member, among, pressKey, pressValue, seen))
            {
                found = true;
                break;
            }
    }
    seen.remove(fid);
    return found;
}

bool repeats(Doc *doc, quint32 functionId, const QSet<Key> &among, Key *pressKey, uchar *pressValue)
{
    if (doc == nullptr || among.isEmpty())
        return false;
    QSet<quint32> seen;
    return repeatsIn(doc, functionId, among, pressKey, pressValue, seen);
}

} // namespace Presses
