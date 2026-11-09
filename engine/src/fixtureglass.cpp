/*
  Q Light Controller Plus
  fixtureglass.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QMap>
#include <QPair>

#include "fixtureglass.h"
#include "monitorproperties.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "doc.h"

namespace
{
    MonitorProperties *propsOf(const Doc *doc)
    {
        return doc != nullptr ? const_cast<Doc *>(doc)->monitorProperties() : nullptr;
    }
}

const Doc *FixtureGlass::docOf(const Fixture *fx)
{
    return fx != nullptr ? qobject_cast<const Doc *>(fx->parent()) : nullptr;
}

QString FixtureGlass::loadedName(const Doc *doc, quint32 fid, quint32 channel, const QLCCapability *cap)
{
    const MonitorProperties *props = propsOf(doc);
    if (props == nullptr || cap == nullptr)
        return QString();
    return props->goboName(fid, channel, cap->min());
}

QString FixtureGlass::loadedName(const Doc *doc, quint32 fid, quint32 channel, uchar value)
{
    const Fixture *fx = doc != nullptr ? const_cast<Doc *>(doc)->fixture(fid) : nullptr;
    const QLCChannel *ch = fx != nullptr ? fx->channel(channel) : nullptr;
    return ch != nullptr ? loadedName(doc, fid, channel, ch->searchCapability(value)) : QString();
}

QString FixtureGlass::slotName(const Doc *doc, quint32 fid, quint32 channel, const QLCCapability *cap)
{
    if (cap == nullptr)
        return QString();
    const QString loaded = loadedName(doc, fid, channel, cap);
    return loaded.isEmpty() ? cap->name() : loaded;
}

QString FixtureGlass::slotName(const Doc *doc, quint32 fid, quint32 channel, uchar value)
{
    const Fixture *fx = doc != nullptr ? const_cast<Doc *>(doc)->fixture(fid) : nullptr;
    const QLCChannel *ch = fx != nullptr ? fx->channel(channel) : nullptr;
    return ch != nullptr ? slotName(doc, fid, channel, ch->searchCapability(value)) : QString();
}

int FixtureGlass::loadedValue(const Doc *doc, const Fixture *fx, quint32 channel, const QString &name)
{
    const MonitorProperties *props = propsOf(doc);
    if (props == nullptr || fx == nullptr || name.trimmed().isEmpty())
        return -1;
    const QMap<QPair<quint32, int>, QString> names = props->goboNames(fx->id());
    if (names.isEmpty())
        return -1;
    const QLCChannel *ch = fx->channel(channel);
    if (ch == nullptr)
        return -1;
    // In slot order: the glass and its shake share a name -- the slot first.
    for (auto it = names.constBegin(); it != names.constEnd(); ++it)
    {
        if (it.key().first != channel || !QLCPalette::sameGoboName(it.value(), name))
            continue;
        const QLCCapability *cap = ch->searchCapability(uchar(it.key().second));
        if (cap != nullptr)
            return (int(cap->min()) + int(cap->max())) / 2;
    }
    return -1;
}

QStringList FixtureGlass::loadedNames(const Doc *doc, quint32 fid)
{
    QStringList out;
    const MonitorProperties *props = propsOf(doc);
    if (props == nullptr)
        return out;
    const QMap<QPair<quint32, int>, QString> names = props->goboNames(fid);
    for (auto it = names.constBegin(); it != names.constEnd(); ++it)
    {
        bool seen = false;
        for (const QString &n : out)
            if (QLCPalette::sameGoboName(n, it.value()))
                seen = true;
        if (!seen)
            out << it.value();
    }
    return out;
}
