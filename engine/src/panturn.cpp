/*
  Q Light Controller Plus
  panturn.cpp

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

#include <QCoreApplication>
#include <QPolygonF>
#include <QDataStream>
#include <QMutex>
#include <QHash>
#include <QLineF>
#include <cmath>
#include <QtMath>
#include <algorithm>

#include "panturn.h"
#include "monitorproperties.h"
#include "stageplatform.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "truss.h"
#include "fixturegroup.h"
#include "scenevalue.h"
#include "scene.h"
#include "doc.h"

namespace
{
QString tr(const char *text)
{
    return QCoreApplication::translate("PanTurn", text);
}

float wrap360(float deg)
{
    float d = std::fmod(deg, 360.0f);
    if (d < 0.0f)
        d += 360.0f;
    return d;
}

/* Where turn @p turn's answers start on the pan channel, before invert:
   the window of one turn solve() answers in (AimSolver::turnPan()). Turn 0
   is half a turn either side of the pan zero; another turn the turn nearest
   its centre, held inside the range. */
float windowStart(const AimSolver::Frame &f, int turn)
{
    const float zero = f.panMax / 2.0f + f.panOffsetDeg;
    if (turn == 0 || f.panMax <= 360.0f + 1e-3f)
        return zero - 180.0f;
    const float centre = zero + 360.0f * float(turn);
    return qBound(0.0f, centre - 180.0f, f.panMax - 360.0f);
}

/* The same as a bearing from the pan zero: the seam, relative. */
float seamRelative(const AimSolver::Frame &f, int turn)
{
    return windowStart(f, turn) - f.panMax / 2.0f - f.panOffsetDeg;
}

/* The stage outline: the decks when there are any, else the grid. */
QList<QPolygonF> stageOutline(const MonitorProperties *mp)
{
    QList<QPolygonF> out;
    for (const StagePlatform *p : mp->stageDecks())
    {
        const QList<QPointF> fp = p->footprint();
        if (fp.size() >= 3)
            out << QPolygonF(QVector<QPointF>(fp.begin(), fp.end()));
    }
    if (out.isEmpty())
    {
        const float mpu = mp->gridUnitMetres();
        const QVector3D g = mp->gridSize();
        const double w = double(g.x() * mpu), d = double(g.z() * mpu);
        if (w > 0.0 && d > 0.0)
            out << QPolygonF(QVector<QPointF>{ QPointF(0, 0), QPointF(w, 0), QPointF(w, d), QPointF(0, d) });
    }
    return out;
}

bool isMover(const Fixture *fx)
{
    return fx != nullptr && fx->channelNumber(QLCChannel::Pan, QLCChannel::MSB) != QLCChannel::invalid();
}
}

float PanTurn::Span::to() const
{
    return wrap360(from + width);
}

void PanTurn::turnRange(const AimSolver::Frame &frame, int &lowest, int &highest)
{
    lowest = highest = 0;
    if (frame.panMax <= 360.0f + 1e-3f)
        return;
    for (int k = 1; k <= 8 && windowStart(frame, k) > windowStart(frame, k - 1) + 0.01f; ++k)
        highest = k;
    for (int k = -1; k >= -8 && windowStart(frame, k) < windowStart(frame, k + 1) - 0.01f; --k)
        lowest = k;
}

float PanTurn::seamBearing(const AimSolver::Frame &frame, int turn)
{
    return wrap360(frame.panZeroDir + seamRelative(frame, turn));
}

float PanTurn::margin(const AimSolver::Frame &frame, const Span &span, int turn)
{
    if (!span.valid || span.overStage)
        return 0.0f;
    /* In bearings from the pan zero: the turn answers from its seam round
       to the same seam a turn on. Put the span where its middle is nearest
       the middle of that, and see how much room it has at each end. */
    const float r0 = seamRelative(frame, turn);
    float a = span.from - frame.panZeroDir;
    const float wc = r0 + 180.0f;
    float sc = a + span.width / 2.0f;
    while (sc - wc > 180.0f) { a -= 360.0f; sc -= 360.0f; }
    while (sc - wc < -180.0f) { a += 360.0f; sc += 360.0f; }
    return qMin(a - r0, r0 + 360.0f - (a + span.width));
}

namespace
{
/* How far along a ray from @p head at bearing @p deg the stage reaches:
   the furthest outline edge it crosses (0 when it crosses none). */
double rayReach(const QList<QPolygonF> &outline, const QPointF &head, double deg)
{
    const double b = qDegreesToRadians(deg);
    const QPointF dir(-qSin(b), qCos(b));       // 0 downstage (+Y), 90 stage right (-X)
    double reach = 0.0;
    for (const QPolygonF &poly : outline)
        for (int e = 0; e < poly.size(); ++e)
        {
            const QPointF p = poly.at(e), q = poly.at((e + 1) % poly.size());
            const QPointF d = q - p;
            const double den = dir.x() * d.y() - dir.y() * d.x();
            if (qAbs(den) < 1e-12)
                continue;
            const QPointF w = p - head;
            const double t = (w.x() * d.y() - w.y() * d.x()) / den;     // along the ray
            const double u = (w.x() * dir.y() - w.y() * dir.x()) / den; // along the edge
            if (t > 0.0 && u >= 0.0 && u <= 1.0)
                reach = qMax(reach, t);
        }
    return reach;
}

/* The span of one head over one stage, worked out once: the views and Show
   Review ask about every head again and again, and the rays cost about a
   millisecond a head. Keyed by everything the answer reads (where the head
   is and the outline), so a stale entry can't be found. */
QMutex s_spanMutex;
QHash<QByteArray, PanTurn::Span> s_spans;

QByteArray spanKey(const QVector3D &head, const QList<QPolygonF> &outline)
{
    QByteArray key;
    QDataStream ds(&key, QIODevice::WriteOnly);
    ds << head.x() << head.y() << head.z();
    for (const QPolygonF &poly : outline)
    {
        ds << qint32(poly.size());
        for (const QPointF &pt : poly)
            ds << pt.x() << pt.y();
    }
    return key;
}
}

PanTurn::Span PanTurn::stageSpan(Doc *doc, quint32 fixtureId)
{
    Span span;
    AimSolver::Frame f;
    MonitorProperties *mp = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (mp == nullptr || !AimSolver::frameFor(doc, fixtureId, f))
        return span;
    const QList<QPolygonF> outline = stageOutline(mp);
    if (outline.isEmpty())
        return span;
    const QByteArray key = spanKey(f.position, outline);
    {
        QMutexLocker lock(&s_spanMutex);
        const auto it = s_spans.constFind(key);
        if (it != s_spans.constEnd())
            return it.value();
    }
    span.valid = true;

    /* The stage close under a hung head is in its near-vertical cone
       (kNearConeDeg from straight down to a subject): there the beam swings
       through every bearing anyway as someone walks under it, whatever the
       turn, so it is left out. What is left is where the pan's bearing
       matters. A head out front, or over the edge of the stage, sees that
       as an arc; one over the middle sees it all round. */
    const double headZ = double(f.position.z());
    const double coneRadius = qMax(0.3, (headZ - 1.0) * qTan(qDegreesToRadians(kNearConeDeg)));
    const QPointF head(double(f.position.x()), double(f.position.y()));
    auto sees = [&](double deg) { return rayReach(outline, head, deg) > coneRadius; };

    // A ray at each half degree first, to find the arc.
    const int steps = 720;
    const double stepDeg = 360.0 / steps;
    QVector<bool> seen(steps, false);
    int seenCount = 0;
    for (int i = 0; i < steps; ++i)
        if (sees(i * stepDeg))
        {
            seen[i] = true;
            ++seenCount;
        }
    auto keep = [&](const Span &s) {
        QMutexLocker lock(&s_spanMutex);
        if (s_spans.size() > 4096)
            s_spans.clear();
        s_spans.insert(key, s);
        return s;
    };
    if (seenCount == 0)
        return keep(Span());                        // nothing of it beyond the cone
    if (seenCount == steps)
    {
        span.overStage = true;
        span.width = 360.0f;
        return keep(span);
    }
    // The widest run it doesn't see is the way it isn't looking.
    int bestLen = 0, bestEnd = 0;
    for (int i = 0; i < steps; ++i)
    {
        if (seen.at(i))
            continue;
        int len = 0;
        while (len < steps && !seen.at((i + len) % steps))
            ++len;
        if (len > bestLen)
        {
            bestLen = len;
            bestEnd = (i + len) % steps;
        }
        if (i + len >= steps)
            break;
        i += len;
    }
    if (bestLen * stepDeg < 10.0)
    {
        span.overStage = true;
        span.width = 360.0f;
        return keep(span);
    }
    /* Then each end exactly, between the last ray that missed and the
       first that saw: a margin read off half-degree rays put a head 9.8
       degrees clear on one side of a 10 degree line or the other by chance. */
    auto edge = [&](double missed, double hit) {
        for (int i = 0; i < 30; ++i)
        {
            const double mid = (missed + hit) / 2.0;
            (sees(mid) ? hit : missed) = mid;
        }
        return hit;
    };
    const double from = edge(bestEnd * stepDeg - stepDeg, bestEnd * stepDeg);
    const int gapStart = (bestEnd - bestLen + steps) % steps;     // first ray of the gap
    double to = edge(gapStart * stepDeg, gapStart * stepDeg - stepDeg);
    while (to < from)
        to += 360.0;
    span.from = wrap360(float(from));
    span.width = float(to - from);
    return keep(span);
}

PanTurn::Info PanTurn::info(Doc *doc, quint32 fixtureId)
{
    Info in;
    const Fixture *fx = doc != nullptr ? doc->fixture(fixtureId) : nullptr;
    AimSolver::Frame f;
    if (!isMover(fx) || !AimSolver::frameFor(doc, fixtureId, f))
        return in;
    in.valid = true;
    in.turn = f.panTurn;
    turnRange(f, in.lowest, in.highest);
    in.hasTurns = in.highest > in.lowest;
    in.seam = seamBearing(f, in.turn);
    in.centre = wrap360(in.seam + 180.0f);
    in.span = stageSpan(doc, fixtureId);
    in.margin = margin(f, in.span, in.turn);
    in.proposal = 0;
    in.proposalMargin = margin(f, in.span, 0);
    // Turn 0 while the stage clears its seam with room: the person's pan zero rules.
    if (!in.hasTurns || !in.span.valid || in.span.overStage || in.proposalMargin >= kClearDeg)
        return in;
    for (int k = in.lowest; k <= in.highest; ++k)
    {
        const float m = margin(f, in.span, k);
        /* A head on the stage's centre line ties: it takes the later turn,
           not whichever side the rounding favoured. */
        const float tieDeg = 0.05f;
        const bool better = m > in.proposalMargin + tieDeg;
        const bool tie = qAbs(m - in.proposalMargin) <= tieDeg
                         && (qAbs(k) < qAbs(in.proposal) || (qAbs(k) == qAbs(in.proposal) && k > in.proposal));
        if (better || tie)
        {
            in.proposal = k;
            in.proposalMargin = m;
        }
    }
    return in;
}

int PanTurn::proposal(Doc *doc, quint32 fixtureId)
{
    return info(doc, fixtureId).proposal;
}

int PanTurn::proposalAfterMove(Doc *doc, quint32 fixtureId)
{
    const Info in = info(doc, fixtureId);
    if (!in.crossesSeam())
        return in.turn;
    return in.proposalMargin > in.margin + 0.5f ? in.proposal : in.turn;
}

bool PanTurn::setTurn(Doc *doc, quint32 fixtureId, int turn)
{
    MonitorProperties *mp = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (mp == nullptr || doc->fixture(fixtureId) == nullptr)
        return false;
    AimSolver::Frame f;
    int lo = 0, hi = 0;
    if (AimSolver::frameFor(doc, fixtureId, f))
        turnRange(f, lo, hi);
    turn = qBound(lo, turn, hi);
    FixtureRigProps rp = mp->fixtureRigProps(fixtureId);
    if (rp.panTurn == turn)
        return false;
    rp.panTurn = turn;
    mp->setFixtureRigProps(fixtureId, rp);
    return true;
}

bool PanTurn::centre(Doc *doc, quint32 fixtureId)
{
    return setTurn(doc, fixtureId, proposal(doc, fixtureId));
}

bool PanTurn::isLive(Doc *doc, quint32 fixtureId)
{
    if (doc == nullptr)
        return false;
    if (doc->mode() == Doc::Operate)
        return true;
    for (Function *fn : doc->functions())
    {
        const Scene *sc = qobject_cast<const Scene *>(fn);
        if (sc == nullptr || !sc->isRunning())
            continue;
        if (sc->fixtures().contains(fixtureId))
            return true;
        for (const SceneValue &sv : sc->values())
            if (sv.fxi == fixtureId)
                return true;
        for (quint32 gid : sc->fixtureGroups())
            if (const FixtureGroup *g = doc->fixtureGroup(gid))
                if (g->fixtureList().contains(fixtureId))
                    return true;
    }
    return false;
}

QString PanTurn::bearingName(float b)
{
    static const char *const names[8] = {
        QT_TRANSLATE_NOOP("PanTurn", "downstage"), QT_TRANSLATE_NOOP("PanTurn", "downstage right"),
        QT_TRANSLATE_NOOP("PanTurn", "stage right"), QT_TRANSLATE_NOOP("PanTurn", "upstage right"),
        QT_TRANSLATE_NOOP("PanTurn", "upstage"), QT_TRANSLATE_NOOP("PanTurn", "upstage left"),
        QT_TRANSLATE_NOOP("PanTurn", "stage left"), QT_TRANSLATE_NOOP("PanTurn", "downstage left") };
    const int i = int(std::floor(wrap360(b) / 45.0f + 0.5f)) % 8;
    return tr(names[i]);
}

static QString degrees(float b)
{
    return QString::number(qRound(wrap360(b)) % 360) + QString::fromUtf8("\302\260");
}

QString PanTurn::worksAround(const Info &in)
{
    if (!in.valid)
        return QString();
    if (!in.hasTurns)
        return tr("one turn of pan, nothing to choose");
    return tr("%1 %2 \302\261180\302\260 (turn %3)")
            .arg(degrees(in.centre), bearingName(in.centre)).arg(in.turn);
}

QString PanTurn::stageText(const Info &in)
{
    if (!in.valid || !in.hasTurns)
        return QString();
    const QString seam = tr("The seam is at %1 %2.").arg(degrees(in.seam), bearingName(in.seam));
    if (!in.span.valid)
        return seam;
    if (in.span.overStage)
        return seam + QLatin1Char(' ')
               + tr("It is over the stage, so the seam is on the stage whatever the turn: its pan "
                    "zero decides where.");
    const QString span = tr("the stage (%1 to %2)").arg(degrees(in.span.from), degrees(in.span.to()));
    if (in.margin >= kClearDeg)
        return seam + QLatin1Char(' ') + tr("It is %1\302\260 clear of %2.").arg(qRound(in.margin)).arg(span);
    QString s = seam + QLatin1Char(' ')
                + (in.margin >= 0.0f
                       ? tr("It is only %1\302\260 clear of %2: a move just past its edge swings the head a "
                            "whole turn.").arg(qRound(in.margin)).arg(span)
                       : tr("It is on %1: a move across it swings the head a whole turn.").arg(span));
    if (in.fixable())
        s += QLatin1Char(' ')
             + (in.proposalMargin >= kClearDeg
                    ? tr("Centre on Stage sets turn %1, %2\302\260 clear.").arg(in.proposal).arg(qRound(in.proposalMargin))
                    : tr("Centre on Stage sets turn %1, as far off the stage as it goes.").arg(in.proposal));
    return s;
}

QString PanTurn::changeText(Doc *doc, quint32 fixtureId, int from, int to)
{
    const Fixture *fx = doc != nullptr ? doc->fixture(fixtureId) : nullptr;
    AimSolver::Frame f;
    if (fx == nullptr || !AimSolver::frameFor(doc, fixtureId, f))
        return QString();
    const float a = seamBearing(f, from), b = seamBearing(f, to);
    return tr("%1: pan turn %2 to %3, its seam from %4 %5 to %6 %7")
            .arg(fx->name()).arg(from).arg(to)
            .arg(degrees(a), bearingName(a), degrees(b), bearingName(b));
}
