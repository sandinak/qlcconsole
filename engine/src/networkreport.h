/*
  Q Light Controller Plus
  networkreport.h

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

#ifndef NETWORKREPORT_H
#define NETWORKREPORT_H

#include <QStringList>
#include <QString>
#include <QList>

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The network summary for the data package: what gear there is, what each
 * port carries, how long the runs are, what is left over, and which fixtures
 * sit on the network in their own right. Derived on demand from the
 * NetworkTopology, the patch and the plot (see NETWORK_CABLING_DESIGN.md, N5);
 * nothing here is stored.
 */
class NetworkReport
{
public:
    /** One node port. */
    struct Port
    {
        int     nodeIndex = -1;
        int     portIndex = -1;
        bool    used = false;            //!< carries a universe
        quint32 universe = 0;            //!< valid when used
        QString receiver;                //!< wireless: the placed receiver it lands at
        int     fixtures = 0;            //!< patched DMX fixtures on the universe
        int     channels = 0;            //!< channels they use
        bool    anchored = false;        //!< the run has somewhere to start from
        double  lengthMetres = 0;        //!< straight-line estimate, when anchored
        int     unplotted = 0;           //!< fixtures on the run with no plot position

        /** Something to measure: a start point and at least one fixture on the plot. */
        bool hasRun() const { return used && anchored && fixtures > unplotted; }
    };

    /** One node. */
    struct Node
    {
        QString name;
        QString transport;
        QString model;                   //!< the gear, when made from a catalog model
        QString ip;
        bool    placed = false;
        int     ports = 0;
        int     portsUsed = 0;
        QList<quint32> universes;        //!< in port order
        int     fixtures = 0;
        int     channels = 0;
    };

    /** A fixture that is its own network endpoint. */
    struct NetworkFixture
    {
        quint32 id = 0;
        QString name;
        QString ip;                      //!< empty: not set yet
        bool    patched = false;
        quint32 universe = 0;            //!< valid when patched
    };

    /** Build the report for @p doc's topology, patch and plot. */
    static NetworkReport build(Doc *doc);

    QList<Node> nodes;
    QList<Port> ports;                   //!< every port of every node, in order
    QList<NetworkFixture> networkFixtures;
    QList<quint32> unserved;             //!< patched DMX fixtures no port carries
    QStringList unservedNames;
    QStringList warnings;                //!< NetworkTopology::warnings()

    int portsTotal() const { return ports.size(); }
    int portsUsed() const;
    int portsFree() const { return portsTotal() - portsUsed(); }
    /** Distinct universes the nodes carry. */
    int universesCarried() const;
    /** Index into ports of the longest anchored run, or -1 if none. */
    int longestRun() const;

    /** A readable summary, lengths in feet when @p feet. */
    QString toText(bool feet = false) const;

    /** Two CSV tables for a spreadsheet, separated by a blank line: the ports,
     *  then the network fixtures. Lengths in feet when @p feet. */
    QString toCsv(bool feet = false) const;
};

/** @} */

#endif
