/*
  Q Light Controller Plus
  handprops.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QCoreApplication>
#include <QPainterPath>
#include <QActionGroup>
#include <QPainter>
#include <QMenu>
#include <cmath>

#include "monitorproperties.h"
#include "contextmenus.h"
#include "candleremote.h"
#include "heldprops.h"
#include "handprops.h"
#include "fixture.h"
#include "doc.h"

static QString tr(const char *s)
{
    return QCoreApplication::translate("HandProps", s);
}

HandProps::Flame HandProps::flameFor(const CandleRemote::State &s)
{
    if (!s.known)
        return Flame::Unknown;
    if (!s.lit)
        return Flame::Dark;
    if (s.flicker)
        return s.dimmed ? Flame::FlickerDimmed : Flame::Flicker;
    return s.dimmed ? Flame::Dimmed : Flame::Full;
}

QString HandProps::flameText(Flame f)
{
    switch (f)
    {
    case Flame::Dark:          return tr("off");
    case Flame::Full:          return tr("lit, steady");
    case Flame::Dimmed:        return tr("lit, dimmed");
    case Flame::Flicker:       return tr("lit, flickering");
    case Flame::FlickerDimmed: return tr("lit, flickering, dimmed");
    case Flame::Unknown:
    default:                   return tr("not known yet (nothing sent)");
    }
}

/* Sizes in pixels: real size, with floors so a prop still reads with the
   whole stage on screen. */
static double staffWidth(double ppm)  { return qMax(2.5, 0.035 * ppm); }
static double candleWidth(double ppm) { return qMax(6.0, 0.07 * ppm); }
static double flameHeight(double ppm) { return qMax(9.0, 0.08 * ppm); }

/* The two strokes every prop line gets: a dark one under a light one, so it
   reads on the light plot and the dark one alike. */
static void twoTone(QPainter *p, const QPainterPath &path, double width, const QColor &colour, bool round)
{
    const Qt::PenCapStyle cap = round ? Qt::RoundCap : Qt::FlatCap;
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(QColor(20, 20, 20, 210), width + 2.0, Qt::SolidLine, cap, Qt::RoundJoin));
    p->drawPath(path);
    p->setPen(QPen(colour, width, Qt::SolidLine, cap, Qt::RoundJoin));
    p->drawPath(path);
}

static void drawFlame(QPainter *p, HandProps::Flame flame, const QPointF &base, const QPointF &up,
                      double height, qint64 ms)
{
    using F = HandProps::Flame;
    if (flame == F::Unknown || flame == F::Dark)
        return;
    const bool flicker = flame == F::Flicker || flame == F::FlickerDimmed;
    const bool dimmed = flame == F::Dimmed || flame == F::FlickerDimmed;
    double h = height * (dimmed ? 0.6 : 1.0);
    if (flicker)
    {
        // Cheap: two slow waves of the clock, never a random generator.
        const double t = double(ms);
        h *= 0.82 + 0.28 * (0.5 + 0.5 * std::sin(t * 0.021) * std::cos(t * 0.0083));
    }
    const QPointF across(-up.y(), up.x());
    auto at = [&](double x, double y) { return base + across * x + up * y; };
    QPainterPath tear;
    tear.moveTo(at(0, 0));
    tear.cubicTo(at(h * 0.45, h * 0.12), at(h * 0.26, h * 0.62), at(0, h));
    tear.cubicTo(at(-h * 0.26, h * 0.62), at(-h * 0.45, h * 0.12), at(0, 0));
    QColor fill(255, 186, 56);
    if (dimmed)
        fill.setAlpha(120);         // and smaller: the size says it, not only the shade
    p->setPen(QPen(QColor(150, 64, 0), 1.0));
    p->setBrush(fill);
    p->drawPath(tear);
    if (flicker)
    {
        // Two short strokes either side: "it is moving", in shape, not colour.
        p->setPen(QPen(QColor(150, 64, 0), 1.0));
        for (int s : { -1, 1 })
            p->drawLine(at(s * h * 0.55, h * 0.45), at(s * h * 0.75, h * 0.75));
    }
}

void HandProps::paint(QPainter *p, StageTarget::Kind kind, Flame flame, const QPointF &feet,
                      const QPointF &crown, double ppm, const QColor &colour, bool selected, qint64 ms)
{
    if (p == nullptr || !StageTarget::isHandPropKind(kind))
        return;
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    const QPointF upPx = crown - feet;
    const double len = std::hypot(upPx.x(), upPx.y());
    const bool fromAbove = len < 4.0;
    const QPointF up = fromAbove ? QPointF(0.0, -1.0) : upPx / len;    // up the screen when from above
    const QColor col = colour.isValid() ? colour : QColor(176, 120, 64);

    if (kind == StageTarget::Kind::Staff)
    {
        const double w = staffWidth(ppm);
        if (fromAbove)
        {
            // End on: the pole's top, a ring round a dot.
            const double r = qMax(4.0, w * 1.4);
            p->setPen(QPen(QColor(20, 20, 20, 210), 1.4));
            p->setBrush(col);
            p->drawEllipse(QPointF(0, 0), r, r);
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(20, 20, 20, 200));
            p->drawEllipse(QPointF(0, 0), r * 0.35, r * 0.35);
        }
        else
        {
            QPainterPath pole;
            pole.moveTo(feet);
            pole.lineTo(crown);
            twoTone(p, pole, w, col, true);
        }
    }
    else    // Candle
    {
        const double w = candleWidth(ppm);
        const QPointF across(-up.y(), up.x());
        QPolygonF body;
        QPointF top;
        if (fromAbove)
        {
            body << QPointF(-w / 2, -w / 2) << QPointF(w / 2, -w / 2) << QPointF(w / 2, w / 2)
                 << QPointF(-w / 2, w / 2);
            top = QPointF(0, -w / 2);
        }
        else
        {
            const QPointF a = feet, b = len < 6.0 ? feet + up * 6.0 : crown;
            body << a - across * (w / 2) << a + across * (w / 2) << b + across * (w / 2) << b - across * (w / 2);
            top = b;
        }
        // Not known: a dashed body. Dark or lit: a solid one.
        QPen edge(QColor(20, 20, 20, 210), 1.3);
        if (flame == Flame::Unknown)
            edge.setStyle(Qt::DashLine);
        p->setPen(edge);
        p->setBrush(QColor(250, 240, 220));
        p->drawPolygon(body);
        drawFlame(p, flame, top, up, flameHeight(ppm), ms);
    }

    if (selected)
    {
        p->setPen(QPen(col.lighter(130), 2.0));
        p->setBrush(Qt::NoBrush);
        p->drawEllipse(QPointF(0, 0), 9.0, 9.0);
    }
    p->restore();
}

QRectF HandProps::bounds(StageTarget::Kind kind, const QPointF &feet, const QPointF &crown, double ppm)
{
    QRectF r(-11, -11, 22, 22);
    const double pad = qMax(staffWidth(ppm), candleWidth(ppm)) + flameHeight(ppm) + 4.0;
    r |= QRectF(feet, crown).normalized().adjusted(-pad, -pad, pad, pad);
    if (kind == StageTarget::Kind::Candle)
        r |= QRectF(-pad, -pad, 2 * pad, 2 * pad);
    return r;
}

/****************************************************************************
 * The right-click
 ****************************************************************************/

void HandProps::fillMenu(ContextMenu &cm, Doc *doc, const QList<quint32> &targets, quint32 blocking,
                         const std::function<void(const std::function<void()> &)> &edit)
{
    MonitorProperties *props = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (props == nullptr || targets.isEmpty() || !edit)
        return;

    QList<quint32> handProps, people;
    for (quint32 id : targets)
        if (const StageTarget *t = props->stageTarget(id))
        {
            if (StageTarget::isHandPropKind(t->kind()))
                handProps << id;
            else if (t->kind() == StageTarget::Kind::Person)
                people << id;
        }
    QList<const StageTarget *> everyone;
    for (const StageTarget *t : props->stageTargets())
        if (t != nullptr && t->kind() == StageTarget::Kind::Person)
            everyone << t;
    std::sort(everyone.begin(), everyone.end(), [](const StageTarget *a, const StageTarget *b) {
        return a->name().localeAwareCompare(b->name()) < 0;
    });

    /* Held By ▸ -- in the blocking the plot shows: who has it in this
       number. A cue whose blocking first says "held by" is where they get
       it; one that says otherwise is where it is taken. */
    if (!handProps.isEmpty())
    {
        QMenu *held = cm.addMenu(ContextMenu::Kind, tr("Held By"), QStringLiteral("plotHeldBy"));
        held->setToolTip(tr("Who holds it in %1: it goes where they go").arg(props->blockingName(blocking)));
        const StageTarget *first = props->stageTarget(handProps.first());
        quint32 who = UINT_MAX;
        const StageTarget::Hold now = first->holdIn(blocking, &who);
        QActionGroup *group = new QActionGroup(held);
        auto item = [&](const QString &text, StageTarget::Hold hold, quint32 holder, const QString &name) {
            QAction *a = held->addAction(text);
            a->setObjectName(name);
            a->setCheckable(true);
            a->setChecked(now == hold && (hold != StageTarget::Hold::HeldBy || who == holder));
            group->addAction(a);
            const QList<quint32> ids = handProps;
            QObject::connect(a, &QAction::triggered, held, [doc, ids, blocking, hold, holder, edit]() {
                edit([doc, ids, blocking, hold, holder]() {
                    HeldProps::setHold(doc->monitorProperties(), ids, blocking, hold, holder);
                });
            });
        };
        item(tr("Its Own Place"), StageTarget::Hold::OwnPlace, UINT_MAX, QStringLiteral("heldByNobody"));
        item(tr("Not on Stage"), StageTarget::Hold::OffStage, UINT_MAX, QStringLiteral("heldByOffStage"));
        held->addSeparator();
        for (const StageTarget *p : std::as_const(everyone))
            item(p->name(), StageTarget::Hold::HeldBy, p->id(), QStringLiteral("heldBy%1").arg(p->id()));
        if (everyone.isEmpty())
            held->addAction(tr("No people on the plot"))->setEnabled(false);
    }

    /* Give Each a Prop ▸ -- one new prop each, held by each selected
       person in this blocking (the candles): one step, one undo. */
    if (!people.isEmpty())
    {
        QMenu *give = cm.addMenu(ContextMenu::Kind,
                                 people.size() > 1 ? tr("Give Each a Prop") : tr("Give a Prop"),
                                 QStringLiteral("plotGiveProp"));
        give->setToolTip(tr("A new prop in the hand of each selected person, in %1")
                             .arg(props->blockingName(blocking)));
        for (StageTarget::Kind k : { StageTarget::Kind::Staff, StageTarget::Kind::Candle })
        {
            QAction *a = give->addAction(HeldProps::kindName(k));
            a->setObjectName(QStringLiteral("give%1").arg(StageTarget::kindToString(k)));
            const QList<quint32> ids = people;
            QObject::connect(a, &QAction::triggered, give, [doc, ids, k, blocking, edit]() {
                edit([doc, ids, k, blocking]() {
                    HeldProps::giveEach(doc->monitorProperties(), ids, k, blocking);
                });
            });
        }
    }

    /* Lit By ▸ -- which remote a candle hears; with one in the show, that
       one, and nothing to set. */
    QList<quint32> candles;
    for (quint32 id : handProps)
        if (props->stageTarget(id)->kind() == StageTarget::Kind::Candle)
            candles << id;
    if (!candles.isEmpty())
    {
        QMenu *lit = cm.addMenu(ContextMenu::Kind, tr("Lit By"), QStringLiteral("plotLitBy"));
        const QList<quint32> remotes = CandleRemote::of(doc)->remotes();
        const quint32 cur = props->stageTarget(candles.first())->litBy();
        QActionGroup *group = new QActionGroup(lit);
        auto item = [&](const QString &text, quint32 fid) {
            QAction *a = lit->addAction(text);
            a->setCheckable(true);
            a->setChecked(cur == fid);
            group->addAction(a);
            QObject::connect(a, &QAction::triggered, lit, [doc, candles, fid, edit]() {
                edit([doc, candles, fid]() {
                    for (quint32 id : candles)
                        if (StageTarget *t = doc->monitorProperties()->stageTarget(id))
                            t->setLitBy(fid);
                });
            });
        };
        item(remotes.size() == 1 ? tr("The Show's Remote (%1)").arg(doc->fixture(remotes.first())->name())
                                 : tr("The Only Remote in the Show"), UINT_MAX);
        for (quint32 fid : remotes)
            if (Fixture *fx = doc->fixture(fid))
                item(fx->name(), fid);
        if (remotes.isEmpty())
        {
            lit->addSeparator();
            lit->addAction(tr("Add a Generic > IR Candle Remote fixture to light them"))->setEnabled(false);
        }
    }
}
