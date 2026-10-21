/*
  Q Light Controller Plus
  artnetnodewatch.h

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

#ifndef ARTNETNODEWATCH_H
#define ARTNETNODEWATCH_H

#include <QHostAddress>
#include <QString>
#include <QList>
#include <QHash>
#include <QMap>

#include "artnetpacketizer.h"

/**
 * Whether the Art-Net nodes we send to are still there, and whether the port
 * carrying our universe says it is in trouble -- read from the ArtPollReplies
 * the plugin already asks for, with hysteresis so neither flaps.
 *
 * Sending can succeed into a void: a datagram to a broadcast address, or to a
 * node whose ARP entry is still cached, leaves this machine fine whether or
 * not anything receives it. SendHealth catches only "can't send". This class
 * answers the next question, "is the node at the far end still answering, and
 * is it outputting", for nodes this session has HEARD from at least once.
 * A target that has never answered is not flagged here: nodes are routinely
 * powered down between calls, and a desk that cries wolf on every load gets
 * ignored.
 *
 * Fed by the controller: pollSent() on every periodic ArtPoll (every 3 s,
 * Art-Net's own cadence), replyHeard() on every ArtPollReply. Owns no socket
 * and reads no clock, so a test can drive it with fabricated replies and
 * times. Not thread-safe: the controller calls it from the GUI thread only.
 *
 * The rules:
 *
 *  - SILENT: no reply in SilentAfterPolls consecutive poll intervals (the
 *    Art-Net spec's own rule of thumb is three). One lost reply is not a
 *    dead node.
 *  - ANSWERING again: replies in AnsweringAfterPolls consecutive intervals.
 *  - PORT FAULT, for an output port: GoodOutput bit 2 ("DMX output short
 *    detected"), or bit 7 ("data is being output") clear on a port that has
 *    been seen outputting this session -- a node that never sets bit 7 is
 *    not flagged for it. Declared when the same fault is in
 *    PortFaultAfterReplies consecutive replies (one odd reply is not a
 *    fault), cleared when replies have been clean for PortClearAfterMs.
 *
 * Bit numbering per Art-Net 4 GoodOutputA: 7 data output, 6 test packets,
 * 5 SIPs, 4 text, 3 merging, 2 short detected, 1 LTP, 0 sACN.
 */
class ArtNetNodeWatch
{
public:
    static constexpr int SilentAfterPolls = 3;
    static constexpr int AnsweringAfterPolls = 2;
    static constexpr int PortFaultAfterReplies = 2;
    static constexpr qint64 PortClearAfterMs = 10000;

    static constexpr uchar GoodOutputData = 0x80;
    static constexpr uchar GoodOutputMerging = 0x08;
    static constexpr uchar GoodOutputShort = 0x04;

    struct Issue
    {
        enum Kind { NodeSilent, PortShort, PortNotOutputting };
        Kind kind;
        QHostAddress node;  //!< where its replies come from
        QString name;       //!< long name, else short name, else empty
        QString ip;         //!< the node's address, as text
        int port;           //!< physical port, 1-based (0 for NodeSilent)
        QString portLabel;  //!< "Net:Sub:Universe" (empty for NodeSilent)
        qint64 sinceMs;     //!< caller's monotonic ms the trouble began
    };

    /** A periodic poll has gone out; closes the previous interval. Returns
     *  true if any reported state changed. */
    bool pollSent(qint64 nowMs);

    /** An ArtPollReply from @a from. Returns true if any reported state
     *  changed. */
    bool replyHeard(const QHostAddress &from, const ArtNetNodeInfo &info, qint64 nowMs);

    /** The trouble that affects a universe sent to @a target on Port-Address
     *  @a portAddress (15 bits). For a unicast @a target that is a node heard
     *  from, that node. For a @a broadcast target, every node heard to have
     *  an output port on @a portAddress -- the ones that would output it. */
    QList<Issue> issuesFor(const QHostAddress &target, bool broadcast,
                           quint16 portAddress) const;

    /** Every node heard so far (for tests and diagnostics) */
    int nodeCount() const { return m_nodes.count(); }

private:
    enum Fault { NoFault, Short, NotOutputting };

    struct PortState
    {
        quint16 portAddress = 0;
        int physicalPort = 0;
        bool everOutput = false;
        Fault reported = NoFault;
        qint64 reportedSince = 0;
        Fault candidate = NoFault;
        int candidateReplies = 0;
        qint64 candidateSince = 0;
        int cleanReplies = 0;
        qint64 cleanSince = 0;
    };

    struct NodeState
    {
        QString name;
        QString ip;
        bool heardThisInterval = false;
        int missedPolls = 0;
        int answeredPolls = 0;
        bool silent = false;
        qint64 lastReplyMs = 0;
        qint64 silentSince = 0;
        /** Keyed by bindIndex * 4 + port: a node with more than four ports
         *  answers once per group of four, each with its own bindIndex. */
        QMap<int, PortState> ports;
    };

    bool updatePort(PortState &ps, Fault observed, qint64 nowMs);

    QHash<QHostAddress, NodeState> m_nodes;
    bool m_intervalOpen = false;
};

#endif
