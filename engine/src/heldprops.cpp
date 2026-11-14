/*
  Q Light Controller Plus
  heldprops.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QCoreApplication>

#include "monitorproperties.h"
#include "heldprops.h"

QString HeldProps::kindName(StageTarget::Kind kind)
{
    switch (kind)
    {
    case StageTarget::Kind::Staff:  return QCoreApplication::translate("HeldProps", "Staff");
    case StageTarget::Kind::Candle: return QCoreApplication::translate("HeldProps", "Candle");
    default:                        return QString();
    }
}

QList<quint32> HeldProps::giveEach(MonitorProperties *props, const QList<quint32> &people,
                                   StageTarget::Kind kind, quint32 blocking)
{
    QList<quint32> made;
    if (props == nullptr || !StageTarget::isHandPropKind(kind))
        return made;
    if (blocking != MonitorProperties::defaultBlocking() && !props->hasBlockingSet(blocking))
        blocking = MonitorProperties::defaultBlocking();
    for (quint32 pid : people)
    {
        const StageTarget *who = props->stageTarget(pid);
        if (who == nullptr || who->kind() != StageTarget::Kind::Person)
            continue;
        StageTarget *prop = props->addStageTarget();
        prop->setKind(kind);
        prop->setName(QStringLiteral("%1 (%2)").arg(kindName(kind), who->name()));
        prop->setColor(kind == StageTarget::Kind::Candle ? QColor(255, 196, 110) : QColor(176, 120, 64));
        prop->setLayerId(who->layerId());
        /* Put down, it stands beside where they stand in Default. */
        const QVector3D hand = props->handPoint(who, MonitorProperties::defaultBlocking());
        const QVector3D feet = props->holderFeet(who, MonitorProperties::defaultBlocking());
        prop->setPosition(QVector3D(hand.x(), hand.y(), feet.z()));
        if (blocking != MonitorProperties::defaultBlocking())
            props->setTargetHold(prop, MonitorProperties::defaultBlocking(), int(StageTarget::Hold::OffStage));
        props->setTargetHold(prop, blocking, int(StageTarget::Hold::HeldBy), pid);
        made << prop->id();
    }
    if (!made.isEmpty())
        props->notifyStageTargetsChanged();
    return made;
}

int HeldProps::setHold(MonitorProperties *props, const QList<quint32> &propIds, quint32 blocking,
                       StageTarget::Hold hold, quint32 holder)
{
    if (props == nullptr)
        return 0;
    if (hold == StageTarget::Hold::HeldBy)
    {
        const StageTarget *who = props->stageTarget(holder);
        if (who == nullptr || who->kind() != StageTarget::Kind::Person)
            return 0;
    }
    int n = 0;
    for (quint32 id : propIds)
    {
        StageTarget *t = props->stageTarget(id);
        if (t == nullptr || !StageTarget::isHandPropKind(t->kind()))
            continue;
        quint32 was = UINT_MAX;
        const StageTarget::Hold before = t->holdIn(blocking, &was);
        props->setTargetHold(t, blocking, int(hold), holder);
        quint32 now = UINT_MAX;
        if (t->holdIn(blocking, &now) != before || now != was)
            ++n;
    }
    if (n > 0)
        props->notifyStageTargetsChanged();
    return n;
}
