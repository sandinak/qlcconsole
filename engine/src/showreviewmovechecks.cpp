/*
  Q Light Controller Plus - qlcconsole
  showreviewmovechecks.cpp

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

/* Show Review's movement checks (docs/design/SHOW_REVIEW.md section 3.1):
   MOV-01 moves in light, MOV-02 can't move in black, MOV-08 not marked
   before its first cue. They read each cue's resolved Position and Dimmer
   per head and use MoveSpeed for how long the move really takes. */

#include <QCoreApplication>
#include <QHash>
#include <algorithm>

#include "showreviewcontext.h"
#include "showtreecontroller.h"
#include "programmercontroller.h"
#include "qlcpalette.h"
#include "movespeed.h"
#include "function.h"
#include "fixture.h"
#include "doc.h"

using namespace ShowTree;

static QString tr(const char *text)
{
    return QCoreApplication::translate("ShowReview", text);
}

static QString trn(const char *text, int n)
{
    return QCoreApplication::translate("ShowReview", text, nullptr, n);
}

namespace
{
/** Where a head is, and whether it is lit, in one cue. */
struct Head
{
    bool hasPos = false;
    double pan = 0.0;
    double tilt = 0.0;
    bool hasDim = false;
    int dim = 0;                //!< 0..255, highest of the layers that light it
    bool lit() const { return hasDim && dim > 0; }
    bool dark() const { return !hasDim || dim == 0; }
};

/** Below this a "move" is the same place to the eye (degrees). */
const double kSameSpotDeg = 2.0;
/** A head with no position set sits at the middle of its range. */
const double kHome = 0.5;

QList<QPair<const Production *, const CueList *>> plainLists(Doc *doc)
{
    QList<QPair<const Production *, const CueList *>> lists;
    for (const Production &p : doc->showTree()->productions())
        for (const CueList &l : p.lists)
            if (!l.looks && !l.isSubCues())
                lists << qMakePair(&p, &l);
    return lists;
}

bool isMover(const Fixture *fx)
{
    return fx != nullptr
           && (fx->channelNumber(QLCChannel::Pan, QLCChannel::MSB) != QLCChannel::invalid()
               || fx->channelNumber(QLCChannel::Tilt, QLCChannel::MSB) != QLCChannel::invalid())
           && QLCPalette::fixtureHasDimmer(fx);
}

/** The movers of one cue, resolved: Position (later layers win) and Dimmer
 *  (the brightest layer). */
QHash<quint32, Head> headsOf(ReviewContext &ctx, const ResolvedCue &cue,
                             QHash<QPair<quint32, quint32>, QList<SceneValue>> &cache)
{
    Doc *doc = ctx.doc();
    QHash<quint32, Head> heads;
    for (const ResolvedLayer &layer : cue.layers)
    {
        if (layer.target.isOpaque() || layer.release)
            continue;
        const QList<quint32> fixtures = ctx.targetFixtures(layer.target);
        const auto dimIt = layer.values.constFind(KeyDimmer);
        const auto posIt = layer.values.constFind(KeyPosition);
        for (quint32 fid : fixtures)
        {
            Fixture *fx = doc->fixture(fid);
            if (!isMover(fx))
                continue;
            Head &h = heads[fid];
            if (dimIt != layer.values.constEnd())
            {
                const Look &look = dimIt.value().look;
                const int v = look.none ? 0 : QLCPalette::lookDimmerValue(doc, look.palettes);
                if (v >= 0)
                {
                    h.hasDim = true;
                    h.dim = qMax(h.dim, v);
                }
            }
            if (posIt != layer.values.constEnd() && !posIt.value().look.none)
            {
                for (quint32 pid : posIt.value().look.palettes)
                {
                    QLCPalette *pal = doc->palette(pid);
                    if (pal == nullptr)
                        continue;
                    const QPair<quint32, quint32> key(pid, fid);
                    auto c = cache.find(key);
                    if (c == cache.end())
                        c = cache.insert(key, pal->valuesFromFixtures(doc, { fid }));
                    double pan, tilt;
                    bool hasPan, hasTilt;
                    MoveSpeed::positionOf(fx, c.value(), pan, tilt, hasPan, hasTilt);
                    if (hasPan) { h.pan = pan; h.hasPos = true; }
                    if (hasTilt) { h.tilt = tilt; h.hasPos = true; }
                }
            }
        }
    }
    return heads;
}

double degreesMoved(const Fixture *fx, const Head &a, const Head &b)
{
    // Degrees along the pointing band (a banded head's DMX range is not its travel).
    return MoveSpeed::travelMs(fx, a.pan, a.tilt, b.pan, b.tilt) * MoveSpeed::degreesPerSecond(fx) / 1000.0;
}

QString secs(double ms)
{
    return QString::number(ms / 1000.0, 'f', 1);
}

QString names(ReviewContext &ctx, const QList<quint32> &ids, int max = 3)
{
    QStringList n;
    for (int i = 0; i < ids.count() && i < max; ++i)
        n << ctx.fixtureName(ids.at(i));
    QString s = n.join(QStringLiteral(", "));
    if (ids.count() > max)
        s = trn("%1 and %n more", ids.count() - max).arg(s);
    return s;
}

struct Mover
{
    quint32 fid;
    double deg;
    double ms;
};

QStringList keysFor(const CueList &list, const Cue &cue, const QList<Mover> &movers)
{
    QStringList keys { QStringLiteral("list:%1/cue:%2").arg(list.id).arg(cue.id) };
    for (const Mover &m : movers)
        keys << QStringLiteral("fx:%1").arg(m.fid);
    return keys;
}

QByteArray printOf(const QList<Mover> &movers)
{
    QStringList ids;
    for (const Mover &m : movers)
        ids << QString::number(m.fid);
    return ids.join(QLatin1Char(',')).toUtf8();
}

/** The slowest head of a group and its names, for a message. */
void summary(ReviewContext &ctx, QList<Mover> &movers, QString &who, double &deg, double &ms)
{
    std::sort(movers.begin(), movers.end(), [](const Mover &a, const Mover &b) { return a.fid < b.fid; });
    QList<quint32> ids;
    deg = ms = 0;
    for (const Mover &m : movers)
    {
        ids << m.fid;
        deg = qMax(deg, m.deg);
        ms = qMax(ms, m.ms);
    }
    who = names(ctx, ids);
}

/** One walk for the three checks; @a which is 1, 2 or 8. */
void walk(ReviewContext &ctx, QList<ReviewFinding> &out, int which)
{
    Doc *doc = ctx.doc();
    const bool showMib = doc->programmer() != nullptr && doc->programmer()->isAutoMoveInBlack();
    const int darkGap = doc->programmer() != nullptr ? doc->programmer()->moveInBlackDarkGapMs() : 800;
    QHash<QPair<quint32, quint32>, QList<SceneValue>> cache;

    for (const auto &pl : plainLists(doc))
    {
        const CueList &list = *pl.second;
        const QList<ResolvedCue> &res = ctx.resolved(list);
        if (res.count() != list.cues.count())
            continue;
        QHash<int, QHash<quint32, Head>> state;     // by cue index, built on demand
        auto headsAt = [&](int i) -> const QHash<quint32, Head> & {
            auto it = state.find(i);
            if (it == state.end())
                it = state.insert(i, headsOf(ctx, res.at(i), cache));
            return it.value();
        };
        for (int i = 0; i < list.cues.count(); ++i)
        {
            const Cue &cue = list.cues.at(i);
            if (cue.skip)
                continue;
            const int before = playedBefore(list, i);
            const QHash<quint32, Head> &now = headsAt(i);
            QHash<quint32, Head> none;
            const QHash<quint32, Head> &prev = before >= 0 ? headsAt(before) : none;

            QList<Mover> moving;                    // MOV-01
            QList<Mover> late;                      // MOV-02
            QList<Mover> unmarked;                  // MOV-08
            for (auto it = now.constBegin(); it != now.constEnd(); ++it)
            {
                const Fixture *fx = doc->fixture(it.key());
                const Head &h = it.value();
                if (!h.hasPos)
                    continue;
                const auto pit = prev.constFind(it.key());
                const bool hadPrev = pit != prev.constEnd();
                const Head p = hadPrev ? pit.value() : Head();
                if (which == 8)
                {
                    const bool firstCue = before < 0;
                    const bool block = effectiveMode(list, cue) == CueMode::Block;
                    if (!h.lit() || !(firstCue || block))
                        continue;
                    if (hadPrev && p.hasPos && (p.lit() || p.hasDim))
                        continue;                   // the cue before positioned it: MOV-01 / MOV-02
                    Head from;
                    from.pan = from.tilt = kHome;
                    if (hadPrev && p.hasPos)
                        from = p;
                    const double deg = degreesMoved(fx, from, h);
                    if (deg >= kSameSpotDeg)
                        unmarked << Mover{ it.key(), deg, MoveSpeed::travelMs(fx, from.pan, from.tilt, h.pan, h.tilt) };
                    continue;
                }
                if (!hadPrev || !p.hasPos || !h.lit())
                    continue;
                const double deg = degreesMoved(fx, p, h);
                if (deg < kSameSpotDeg)
                    continue;
                const double ms = MoveSpeed::travelMs(fx, p.pan, p.tilt, h.pan, h.tilt);
                if (which == 1 && p.lit())
                    moving << Mover{ it.key(), deg, ms };
                else if (which == 2 && p.hasDim && p.dark())
                    late << Mover{ it.key(), deg, ms };
            }

            const QString cueName = ctx.cueText(list, i);
            const QVariantList jump = QVariantList() << cue.id;
            if (!moving.isEmpty())
            {
                QString who;
                double deg, ms;
                summary(ctx, moving, who, deg, ms);
                out << ReviewContext::make(QStringLiteral("MOV-01"), ReviewFinding::Warning,
                    trn("%1 move while lit into %2 (up to %3 degrees, about %4 s to travel). Move %n "
                        "head(s) in the cue before, or accept a live move.", moving.count())
                        .arg(who, cueName).arg(qRound(deg)).arg(secs(ms)),
                    keysFor(list, cue, moving), printOf(moving), QStringLiteral("cue"), jump);
            }
            if (!late.isEmpty())
            {
                // Why it can't be hidden: off for the show, the list or the cue, or no time.
                const bool allowed = effectiveMoveInBlack(showMib, list.moveInBlack, cue.noMoveInBlack);
                const Cue &prevCue = list.cues.at(before);
                // The dark cue before waits for GO (Function::infiniteSpeed()), or goes on after its hold.
                const bool waits = prevCue.hold == Function::infiniteSpeed();
                QString why;
                QList<Mover> bad;
                if (!allowed)
                {
                    bad = late;
                    why = cue.noMoveInBlack ? tr("this cue says not to move in black")
                          : list.moveInBlack == MoveInBlack::Off ? tr("move in black is off for this list")
                          : tr("move in black is off for the show");
                }
                else if (!waits)
                {
                    for (const Mover &m : late)
                        if (double(prevCue.hold) < qMax(double(darkGap), m.ms))
                            bad << m;
                    if (!bad.isEmpty())
                        why = tr("only %1 s of dark before it").arg(secs(double(prevCue.hold)));
                }
                if (!bad.isEmpty())
                {
                    QString who;
                    double deg, ms;
                    summary(ctx, bad, who, deg, ms);
                    out << ReviewContext::make(QStringLiteral("MOV-02"), ReviewFinding::Warning,
                        trn("%1 move into %2 in the dark but can't move in black: %3 (they need %4 s). "
                            "They'll be seen arriving.", bad.count())
                            .arg(who, cueName, why, secs(qMax(double(darkGap), ms))),
                        keysFor(list, cue, bad), printOf(bad), QStringLiteral("cue"), jump);
                }
            }
            if (!unmarked.isEmpty())
            {
                QString who;
                double deg, ms;
                summary(ctx, unmarked, who, deg, ms);
                out << ReviewContext::make(QStringLiteral("MOV-08"), ReviewFinding::Warning,
                    trn("%1 lights %2 but nothing sets their position before it: they'll swing in from "
                        "where they are (up to %3 degrees). Add a preset cue before it, or mark them.",
                        unmarked.count())
                        .arg(cueName, who).arg(qRound(deg)),
                    keysFor(list, cue, unmarked), printOf(unmarked), QStringLiteral("cue"), jump);
            }
        }
    }
}

void mov01(ReviewContext &ctx, QList<ReviewFinding> &out) { walk(ctx, out, 1); }
void mov02(ReviewContext &ctx, QList<ReviewFinding> &out) { walk(ctx, out, 2); }
void mov08(ReviewContext &ctx, QList<ReviewFinding> &out) { walk(ctx, out, 8); }
}

QList<ShowReview::Check> ShowReviewChecks::moveChecks()
{
    typedef ShowReview S;
    return {
        { QStringLiteral("MOV-01"), tr("Moves in light"), S::Movement,
          tr("A head whose position changes between cues while it is lit in both: the audience "
             "watches the beam slide across the stage. Often deliberate (a drift with the "
             "soloist); otherwise move it in the cue before, in the dark."), mov01 },
        { QStringLiteral("MOV-02"), tr("Can't move in black"), S::Movement,
          tr("A head that is dark in the cue before and repositioned in this one gets there in "
             "the dark only when move in black is on and the dark lasts as long as the head "
             "needs to travel (the lead time, or its real travel time if longer). Otherwise it "
             "fades up while still travelling."), mov02 },
        { QStringLiteral("MOV-08"), tr("Not marked before its first cue"), S::Movement,
          tr("Heads lit in the first cue of a list, or in a Block cue the cue before doesn't "
             "position them for, swing in from where they are, in the light. A preset cue "
             "before it puts them there in the dark."), mov08 },
    };
}
