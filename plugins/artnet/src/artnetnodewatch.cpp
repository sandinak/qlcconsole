/*
  Q Light Controller Plus
  artnetnodewatch.cpp

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

#include "artnetnodewatch.h"

bool ArtNetNodeWatch::pollSent(qint64 nowMs)
{
    Q_UNUSED(nowMs)
    bool changed = false;

    /* The first poll only opens an interval: there is nothing to judge yet. */
    if (m_intervalOpen)
    {
        for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it)
        {
            NodeState &n = it.value();
            if (n.heardThisInterval)
            {
                n.missedPolls = 0;
                n.answeredPolls++;
                if (n.silent && n.answeredPolls >= AnsweringAfterPolls)
                {
                    n.silent = false;
                    changed = true;
                }
            }
            else
            {
                n.answeredPolls = 0;
                n.missedPolls++;
                if (n.silent == false && n.missedPolls >= SilentAfterPolls)
                {
                    n.silent = true;
                    n.silentSince = n.lastReplyMs;
                    changed = true;
                }
            }
            n.heardThisInterval = false;
        }
    }
    m_intervalOpen = true;
    return changed;
}

bool ArtNetNodeWatch::updatePort(PortState &ps, Fault observed, qint64 nowMs)
{
    if (observed == ps.reported)
    {
        ps.candidate = NoFault;
        ps.candidateReplies = 0;
        ps.cleanReplies = 0;
        return false;
    }

    if (observed == NoFault)
    {
        // A fault is showing and this reply is clean: count toward clearing
        ps.candidate = NoFault;
        ps.candidateReplies = 0;
        if (ps.cleanReplies == 0)
            ps.cleanSince = nowMs;
        ps.cleanReplies++;
        if (ps.cleanReplies >= 2 && nowMs - ps.cleanSince >= PortClearAfterMs)
        {
            ps.reported = NoFault;
            ps.cleanReplies = 0;
            return true;
        }
        return false;
    }

    // A fault that isn't the one showing (if any): count toward showing it
    ps.cleanReplies = 0;
    if (ps.candidate != observed)
    {
        ps.candidate = observed;
        ps.candidateReplies = 0;
        ps.candidateSince = nowMs;
    }
    ps.candidateReplies++;
    if (ps.candidateReplies >= PortFaultAfterReplies)
    {
        ps.reported = observed;
        ps.reportedSince = ps.candidateSince;
        ps.candidate = NoFault;
        ps.candidateReplies = 0;
        return true;
    }
    return false;
}

bool ArtNetNodeWatch::replyHeard(const QHostAddress &from, const ArtNetNodeInfo &info,
                                 qint64 nowMs)
{
    NodeState &n = m_nodes[from];
    n.name = info.longName.isEmpty() ? info.shortName : info.longName;
    n.ip = info.ipAddress.isEmpty() ? from.toString() : info.ipAddress;
    n.heardThisInterval = true;
    n.lastReplyMs = nowMs;

    bool changed = false;
    const int bind = info.bindIndex > 0 ? info.bindIndex : 1;
    for (int p = 0; p < qMin(info.portsNumber, 4); p++)
    {
        if ((info.portTypes[p] & 0x80) == 0)
            continue;   // not an output port

        PortState &ps = n.ports[bind * 4 + p];
        ps.portAddress = quint16(((info.netSwitch & 0x7f) << 8)
                                 | ((info.subSwitch & 0x0f) << 4)
                                 | (info.swOut[p] & 0x0f));
        ps.physicalPort = (bind - 1) * 4 + p + 1;

        const uchar good = info.goodOutput[p];
        Fault observed = NoFault;
        if (good & GoodOutputShort)
            observed = Short;
        else if (good & GoodOutputData)
            ps.everOutput = true;
        else if (ps.everOutput)
            observed = NotOutputting;

        if (updatePort(ps, observed, nowMs))
            changed = true;
    }
    return changed;
}

QList<ArtNetNodeWatch::Issue> ArtNetNodeWatch::issuesFor(const QHostAddress &target,
                                                         bool broadcast,
                                                         quint16 portAddress) const
{
    QList<Issue> out;
    const QString targetText = target.toString();

    for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it)
    {
        const NodeState &n = it.value();

        bool carries = false;
        QList<const PortState *> ours;
        for (auto pit = n.ports.constBegin(); pit != n.ports.constEnd(); ++pit)
            if (pit.value().portAddress == portAddress)
                ours << &pit.value();

        if (broadcast)
            carries = ours.isEmpty() == false;
        else
            carries = (it.key() == target || n.ip == targetText);
        if (carries == false)
            continue;

        Issue base;
        base.node = it.key();
        base.name = n.name;
        base.ip = n.ip;
        base.port = 0;

        if (n.silent)
        {
            Issue i = base;
            i.kind = Issue::NodeSilent;
            i.sinceMs = n.silentSince;
            out << i;
            continue;   // what its ports last said is stale
        }

        foreach (const PortState *ps, ours)
        {
            if (ps->reported == NoFault)
                continue;
            Issue i = base;
            i.kind = ps->reported == Short ? Issue::PortShort : Issue::PortNotOutputting;
            i.port = ps->physicalPort;
            i.portLabel = QString("%1:%2:%3").arg(ps->portAddress >> 8)
                              .arg((ps->portAddress >> 4) & 0x0f)
                              .arg(ps->portAddress & 0x0f);
            i.sinceMs = ps->reportedSince;
            out << i;
        }
    }
    return out;
}
