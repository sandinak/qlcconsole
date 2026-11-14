/*
  Q Light Controller Plus
  monitorgraphicsview_heldprops.cpp

  Props someone holds on the plot (docs/design/BLOCKING.md, "Props someone
  holds"): a staff or a candle stands on the deck at its own place, or is
  in a person's hand, or is not on stage, in each blocking. The plot draws
  it where the one reader (MonitorProperties::targetPosition) puts it in
  the blocking it shows, hands it over when it is dropped on a person, and
  offers Held By / Give Each a Prop / Lit By on the right-click.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QTimer>
#include <cmath>

#include "monitorgraphicsview.h"
#include "monitorproperties.h"
#include "contextmenus.h"
#include "blockingview.h"
#include "stagetarget.h"
#include "targetitem.h"
#include "heldprops.h"
#include "handprops.h"
#include "doc.h"

void MonitorGraphicsView::decorateHandProp(TargetItem *ti, StageTarget *t)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (ti == nullptr || t == nullptr || props == nullptr || m_cellPixels == 0)
        return;
    const QVector3D at = props->targetPosition(t);          // in the blocking shown
    const StageTarget *holder = props->targetHolder(t);
    const float len = t->propLength();
    /* A staff stands on the deck, in the hand that holds it; a candle is
       held up in the hand, or stands where it was put down. */
    float bottom = at.z();
    if (holder != nullptr && t->kind() == StageTarget::Kind::Staff)
        bottom = props->holderFeet(holder).z() + 0.02f;
    const QPointF tp = projectMm(at.x() * 1000.0, at.y() * 1000.0, at.z() * 1000.0);
    const QPointF feet = projectMm(at.x() * 1000.0, at.y() * 1000.0, bottom * 1000.0) - tp;
    const QPointF crown = projectMm(at.x() * 1000.0, at.y() * 1000.0, (bottom + len) * 1000.0) - tp;
    ti->setFigure(feet, crown, 1000.0 * m_cellPixels / m_unitValue, m_pov == PovSide);
    ti->setLabelShown(holder == nullptr);
    ti->setToolTip(holder != nullptr ? tr("%1, held by %2").arg(t->name(), holder->name()) : t->name());
}

bool MonitorGraphicsView::anyPropHeld() const
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr)
        return false;
    for (const StageTarget *t : props->stageTargets())
        if (props->targetHolder(t) != nullptr)
            return true;
    return false;
}

bool MonitorGraphicsView::dropHandProp(TargetItem *item, const QList<TargetItem *> &moved)
{
    MonitorProperties *props = m_doc->monitorProperties();
    StageTarget *t = item != nullptr ? item->target() : nullptr;
    if (props == nullptr || t == nullptr || moved.size() != 1 || !StageTarget::isHandPropKind(t->kind()))
        return false;
    BlockingView *bv = BlockingView::of(m_doc);
    const quint32 set = bv != nullptr ? bv->blocking() : MonitorProperties::defaultBlocking();
    const quint32 id = t->id();

    const QVector3D cur = props->targetPosition(t, set);
    const QVector3D dropMm = unprojectMm(item->pos(), cur * 1000.0f);
    QVector3D drop = dropMm / 1000.0f;
    if (std::hypot(double(drop.x() - cur.x()), double(drop.y() - cur.y())) < 0.01
        && (m_pov == PovTop || qAbs(drop.z() - cur.z()) < 0.01f))
    {
        repositionTargetItems();         // not moved: back where it is drawn
        return true;
    }

    /* Onto a person (within an arm's reach of where they stand): theirs.
       The nearest one wins; nobody near, it is put down there. */
    const StageTarget *to = nullptr;
    double best = 0.6;
    for (const StageTarget *p : props->stageTargets())
    {
        if (p == nullptr || p->kind() != StageTarget::Kind::Person)
            continue;
        const QVector3D f = props->holderFeet(p, set);
        const double d = std::hypot(double(f.x() - drop.x()), double(f.y() - drop.y()));
        if (d < best)
        {
            best = d;
            to = p;
        }
    }

    const QByteArray before = m_rigSnapshotter ? m_rigSnapshotter() : QByteArray();
    if (to != nullptr)
    {
        HeldProps::setHold(props, { id }, set, StageTarget::Hold::HeldBy, to->id());
    }
    else
    {
        drop.setZ(floorHeightAt(drop.x(), drop.y()));
        if (set == MonitorProperties::defaultBlocking() || !props->hasBlockingSet(set))
        {
            t->setPosition(drop);
            props->setTargetHold(t, MonitorProperties::defaultBlocking(), int(StageTarget::Hold::OwnPlace));
        }
        else
        {
            StageTarget::Blocked b = t->blocked(set);
            b.hasOffset = true;
            b.offset = drop - t->position();
            b.hold = StageTarget::Hold::OwnPlace;
            b.holder = UINT_MAX;
            t->setBlocked(set, b);
        }
    }
    if (!before.isEmpty())
        pushRigUndo(before);
    m_doc->setModified();
    props->notifyStageTargetsChanged();
    // The item whose release this is must live until it returns: move the
    // items now, rebuild them (readouts, visibility) after.
    repositionTargetItems();
    QTimer::singleShot(0, this, [this]() { updateTargets(); });
    return true;
}

void MonitorGraphicsView::addHandPropItems(ContextMenu &cm, quint32 id)
{
    QList<quint32> targets { id };
    for (const QPair<QString, quint32> &o : menuObjects(QStringLiteral("target"), id))
        if (o.first == QLatin1String("target") && !targets.contains(o.second))
            targets << o.second;
    BlockingView *bv = BlockingView::of(m_doc);
    const quint32 set = bv != nullptr ? bv->blocking() : MonitorProperties::defaultBlocking();
    HandProps::fillMenu(cm, m_doc, targets, set, [this](const std::function<void()> &fn) {
        const QByteArray before = m_rigSnapshotter ? m_rigSnapshotter() : QByteArray();
        fn();
        if (!before.isEmpty() && (!m_rigSnapshotter || m_rigSnapshotter() != before))
            pushRigUndo(before);
        m_doc->setModified();
        m_doc->monitorProperties()->notifyStageTargetsChanged();
        updateTargets();
    });
}
