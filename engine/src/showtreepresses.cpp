/*
  Q Light Controller Plus
  showtreepresses.cpp

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

/* Presses don't track (docs/features/stage-targets.md "Candles"). A press --
   the IR candle remote's Dim or Brighten, QLCCapability::isPress() -- makes
   every candle in the room take one more step each time the blaster sees
   SELECT (or WINDOW) change to it. A layer that tracks plays on (the same
   Scene across GO), so tracking alone wouldn't re-send it; but a Back to a
   cue it tracked into, a jump there, a later Window change, or a Release to
   a base cue that has it would. So after the cue that sets it, the cue
   sends 0 there instead: a generated scene, the cue's last member, that
   sets only the press channels (and never fades: QLCChannel::hasPress()). */

#include <QStringList>
#include <algorithm>
#include <climits>

#include "showtreecontroller.h"
#include "presses.h"

using namespace ShowTree;

QString ShowTreeController::pressGuardSignature(const QHash<quint64, uchar> &values)
{
    QList<quint64> keys = values.keys();
    std::sort(keys.begin(), keys.end());
    QStringList parts;
    for (quint64 k : keys)
        parts << QStringLiteral("%1:%2=%3").arg(Presses::fixtureOf(k)).arg(Presses::channelOf(k))
                     .arg(int(values.value(k)));
    return parts.join(QLatin1Char(','));
}

QList<QHash<quint64, uchar>> ShowTreeController::pressGuards(const CueList &list) const
{
    const QSet<quint64> channels = Presses::channels(m_doc);
    if (channels.isEmpty() || list.looks || list.isSubCues())
        return QList<QHash<quint64, uchar>>(list.cues.count());
    return pressGuards(list, resolved(list), channels);
}

QList<QHash<quint64, uchar>> ShowTreeController::pressGuards(const CueList &list,
                                                              const QList<ResolvedCue> &resolved,
                                                              const QSet<quint64> &pressChannels) const
{
    QList<QHash<quint64, uchar>> out(list.cues.count());
    if (pressChannels.isEmpty() || list.looks || list.isSubCues())
        return out;

    // What each function a layer plays as it is puts on the press channels,
    // asked once.
    QHash<quint32, QHash<quint64, uchar>> byFunction;
    auto writes = [&](quint32 fid) -> const QHash<quint64, uchar> & {
        auto it = byFunction.find(fid);
        if (it == byFunction.end())
            it = byFunction.insert(fid, Presses::written(m_doc, fid, pressChannels));
        return it.value();
    };
    QHash<quint64, QHash<uchar, bool>> pressCache;
    auto isPress = [&](quint64 k, uchar v) {
        QHash<uchar, bool> &known = pressCache[k];
        auto it = known.constFind(v);
        if (it != known.constEnd())
            return it.value();
        return known.insert(v, Presses::isPress(m_doc, k, v)).value();
    };

    // The channels this list presses somewhere: only those are guarded.
    QSet<quint64> pressed;
    const int n = qMin(list.cues.count(), resolved.count());
    for (int i = 0; i < n; ++i)
        for (const ResolvedLayer &rl : resolved.at(i).layers)
        {
            if (!rl.target.isOpaque())
                continue;
            const QHash<quint64, uchar> &w = writes(rl.target.function);
            for (auto it = w.constBegin(); it != w.constEnd(); ++it)
                if (isPress(it.key(), it.value()))
                    pressed.insert(it.key());
        }
    if (pressed.isEmpty())
        return out;

    QHash<quint32, int> order;
    for (int i = 0; i < list.cues.count(); ++i)
        order.insert(list.cues.at(i).id, i);

    struct LastOwn
    {
        uchar value = 0;
        int rank = -1;
    };
    QHash<quint64, LastOwn> lastOwn;    // the last value a cue's own layer set there

    for (int i = 0; i < n; ++i)
    {
        const Cue &cue = list.cues.at(i);
        const ResolvedCue &rc = resolved.at(i);
        for (quint64 k : std::as_const(pressed))
        {
            bool own = false, carried = false, carriedPress = false;
            uchar ownValue = 0, newest = 0;
            int newestRank = INT_MIN;
            // In layer order: carried-in layers first, the cue's own after
            // (ShowTree::resolve) -- the later of two wins as it starts later.
            for (const ResolvedLayer &rl : rc.layers)
            {
                if (!rl.target.isOpaque())
                    continue;
                const QHash<quint64, uchar> &w = writes(rl.target.function);
                const auto it = w.constFind(k);
                if (it == w.constEnd())
                    continue;
                if (rl.own)
                {
                    own = true;
                    ownValue = it.value();
                    continue;
                }
                carriedPress = carriedPress || isPress(k, it.value());
                const int rank = order.value(rl.fromCue, -1);
                if (!carried || rank >= newestRank)
                {
                    newest = it.value();
                    newestRank = rank;
                }
                carried = true;
            }

            if (own)
            {
                // Newly programmed here: it plays. A skipped cue passes on
                // nothing of its own.
                if (!cue.skip)
                    lastOwn[k] = LastOwn { ownValue, i };
                if (carriedPress)
                    out[i].insert(k, ownValue);
                continue;
            }
            if (!carried)
            {
                out[i].insert(k, 0);
                continue;
            }
            uchar value = isPress(k, newest) ? 0 : newest;
            const auto lo = lastOwn.constFind(k);
            // The last value a cue set here was a press, at or after what
            // tracks in (a cue that set a state and then the press: the
            // press is what it sent).
            if (lo != lastOwn.constEnd() && lo->rank >= newestRank && isPress(k, lo->value))
                value = 0;
            out[i].insert(k, value);
        }
    }
    return out;
}
