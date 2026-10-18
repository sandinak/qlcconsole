/*
  Q Light Controller Plus
  networkroute.h

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

#ifndef NETWORKROUTE_H
#define NETWORKROUTE_H

#include <QHostAddress>
#include <QStringList>
#include <QVariant>
#include <QString>
#include <QList>
#include <QMap>

#include <functional>

/** @addtogroup engine Engine
 * @{
 */

/**
 * Patches follow the NETWORK, not the interface.
 *
 * Network output plugins (Art-Net, E1.31/sACN, OSC) name each of their
 * lines after a local interface address, and a workspace stores that
 * address as the patch's LineUID. Taken literally that ties a show to the
 * one machine -- and the one DHCP lease -- it was built on: a laptop off the
 * show VLAN lost every such patch, and the same show opened on another
 * console on the very same network could not find "its" interface either.
 *
 * resolve() answers the question that actually matters: which of the lines
 * this plugin offers RIGHT NOW reaches the network this patch was built
 * for? In order:
 *
 *  1. Unicast target (outputIP / ucastIP parameter): ask the OS which local
 *     address it would send from to reach it -- a directly attached subnet
 *     or a specific route (router, VPN) alike. A route that is merely the
 *     DEFAULT route does not count unless the target is on that interface's
 *     own subnet: on a laptop at home 172.18.2.60 "routes" out the Wi-Fi to
 *     the internet gateway, which is not the show network.
 *  2. The saved LineUID itself, when this machine still has that address.
 *  3. The saved network (Network="172.18.2.0/24"), or for older workspaces
 *     that never stored one, the subnet of the saved address: any local line
 *     whose own subnet contains it, or failing a known netmask, the same /24.
 *  4. A broadcast/directed target inside a local line's subnet.
 *
 * When several lines qualify the first in the plugin's own (stable) line
 * order wins and Resolution::how says which rule picked it. -1 means no
 * local line reaches that network: the caller keeps the patch OFFLINE.
 *
 * The OS lookups are behind setResolverForTest() so unit tests can model a
 * different machine without touching the network.
 */
namespace NetworkRoute
{
    struct LocalAddress
    {
        QString iface;          //!< human readable interface name ("en5")
        QHostAddress ip;
        int prefix;             //!< netmask length, -1 when unknown
    };

    struct Resolution
    {
        int line = -1;
        QString how;            //!< "route", "uid", "network", "target"
        QString network;        //!< the matched network, "a.b.c.d/p"
        QString via;            //!< "en5 (172.18.2.219)"
    };

    /** The local IPv4 addresses (with interface and prefix) of this machine. */
    QList<LocalAddress> localAddresses();

    /** The local address the OS would send from to reach @a target
     *  (null when it has no route at all). */
    QHostAddress sourceFor(const QHostAddress &target);

    /** "172.18.2.0/24" for 172.18.2.219 with prefix 24. */
    QString networkOf(const QHostAddress &ip, int prefix);

    /** The unicast/broadcast target among a patch's plugin parameters
     *  (Art-Net/OSC "outputIP", E1.31 "ucastIP"), or empty. */
    QString targetFromParameters(const QMap<QString, QVariant> &params);

    /** Is @a uid shaped like an address-named network line? */
    bool isAddressIdentity(const QString &uid);

    /** See the namespace comment. @a lines are the plugin's current line
     *  names, which are only considered when they are addresses. */
    Resolution resolve(const QStringList &lines, const QString &uid,
                       const QString &network, const QString &target);

    /** The network ("a.b.c.d/p") of the local line named @a lineName, or
     *  empty when it is not an address this machine has. */
    QString networkForLine(const QString &lineName);

    /** Human readable "en5 (172.18.2.219)" for a local line name. */
    QString describeLine(const QString &lineName);

    /** Replace the OS lookups (both, or reset with empty functions). */
    void setResolverForTest(std::function<QList<LocalAddress>()> addresses,
                            std::function<QHostAddress(const QHostAddress &)> source);
}

/** @} */

#endif
