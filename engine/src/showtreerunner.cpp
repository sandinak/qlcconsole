/*
  Q Light Controller Plus
  showtreerunner.cpp

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

/* What the cue runner (docs/design/SHOW_TREE_DESIGN.md §6B, P7) reads of the tree: which
   cue a chaser step is, and the "what changes" line -- computed from the
   resolved tracking, not from DMX, so it reads as the operator thinks:
   "Towers → Red, spread +5; BackStage in → Green, Intensity 45%". */

#include "showtreecontroller.h"
#include "qlcpalette.h"
#include "doc.h"

using namespace ShowTree;

quint32 ShowTreeController::cueOfStep(quint32 listId, int step, int subStep) const
{
    const CueList *list = cueList(listId);
    if (list == nullptr || step < 0 || step >= list->cues.count())
        return invalidId();
    // A step is the cue at that place in tonight's running order.
    const quint32 cueId = list->cues.at(playOrderOf(listId).at(step)).id;
    if (subStep < 0)
        return cueId;
    const CueList *subs = cueList(subCueList(cueId));
    if (subs == nullptr || subStep >= subs->cues.count())
        return invalidId();
    return subs->cues.at(subStep).id;
}

bool ShowTreeController::resolvedCue(quint32 cueId, ResolvedCue *out) const
{
    const CueList *list = listOfCue(cueId);
    if (list == nullptr)
        return false;
    const QList<ResolvedCue> rs = resolved(*list);
    const int at = list->indexOf(cueId);
    if (at < 0 || at >= rs.count())
        return false;
    *out = rs.at(at);
    return true;
}

QString ShowTreeController::changeLine(quint32 fromCue, quint32 toCue) const
{
    ResolvedCue to, from;
    if (!resolvedCue(toCue, &to))
        return QString();
    if (fromCue != invalidId() && !resolvedCue(fromCue, &from))
        from = ResolvedCue();

    // What a layer plays, per key, as a person reads it.
    auto values = [this](const ResolvedLayer &l) {
        QList<QPair<QString, QString>> out;
        QList<Look> played = l.playedLooks();
        for (const QString &k : { KeySpread, KeyGrouping, KeyCross })
            if (l.values.contains(k) && !l.values.value(k).look.none)
                played << l.values.value(k).look;
        for (const Look &look : played)
        {
            QStringList names;
            for (quint32 pid : look.palettes)
            {
                const QLCPalette *p = m_doc->palette(pid);
                names << (p == nullptr ? tr("(missing)") : p->name());
            }
            for (quint32 pid : look.own)
                if (const QLCPalette *p = m_doc->palette(pid))
                    names << tr("own %1").arg(p->name());
            QString text = names.join(QStringLiteral(" + "));
            if (isLineParamKey(look.key))
                text = lineParamText(look.key, look.value);
            // Colour and Dimmer bare ("Red", "Intensity 45%"); the rest
            // say what they are ("gobo Breakup", "spread +5").
            if (look.key != KeyColour && look.key != KeyDimmer)
                text = keyLabel(look.key).toLower() + QLatin1Char(' ') + text;
            out << qMakePair(look.key, text);
        }
        return out;
    };
    auto label = [this](const ResolvedLayer &l) {
        return l.name.isEmpty() ? targetLabel(l.target) : l.name;
    };

    QStringList parts;
    for (const ResolvedLayer &l : to.layers)
    {
        const QList<QPair<QString, QString>> now = values(l);
        if (now.isEmpty())
            continue;
        const ResolvedLayer *was = from.layer(l.target);
        QHash<QString, QString> before;
        if (was != nullptr)
            for (const auto &kv : values(*was))
                before.insert(kv.first, kv.second);
        QStringList changed;
        for (const auto &kv : now)
            if (!before.contains(kv.first) || before.value(kv.first) != kv.second)
                changed << kv.second;
        // Turned off here: what it had that it no longer plays.
        if (was != nullptr)
            for (auto it = before.constBegin(); it != before.constEnd(); ++it)
            {
                bool still = false;
                for (const auto &kv : now)
                    still = still || kv.first == it.key();
                if (!still)
                    changed << tr("no %1").arg(keyLabel(it.key()).toLower());
            }
        if (changed.isEmpty())
            continue;
        const bool fresh = was == nullptr || values(*was).isEmpty();
        parts << (fresh ? tr("%1 in → %2") : tr("%1 → %2")).arg(label(l), changed.join(QStringLiteral(", ")));
    }
    for (const ResolvedLayer &l : from.layers)
        if (!values(l).isEmpty() && (to.layer(l.target) == nullptr || values(*to.layer(l.target)).isEmpty()))
            parts << tr("%1 out").arg(label(l));
    return parts.join(QStringLiteral("; "));
}
