/*
  Q Light Controller Plus
  rignetwork.h

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

#ifndef RIGNETWORK_H
#define RIGNETWORK_H

#include <QHostAddress>
#include <QStringList>
#include <QString>

/** @addtogroup engine Engine
 * @{
 */

/**
 * The show's rig networks: the IPv4 networks ("172.18.0.0/20") its network
 * output belongs on. A laptop that is somewhere else -- a school's network,
 * a venue's office Wi-Fi -- must not put Art-Net, sACN or WLED traffic on
 * it; see InputOutputMap's transmit gate, which holds such output.
 *
 * Only the text and matching live here. IPv6 is never a rig network and an
 * IPv6 address never matches one (it is not refused loudly, it just does
 * not count); loopback is always allowed by the gate itself.
 */
namespace RigNetwork
{
    /** "172.18.2.5/24" -> "172.18.2.0/24". Empty, with @a error set, when
     *  @a text is not an IPv4 network with a prefix from 1 to 32. */
    QString normalise(const QString &text, QString *error = nullptr);

    /** Split @a text on commas, semicolons and white space and normalise
     *  each; duplicates dropped, order kept. Entries that do not parse are
     *  listed (as typed) in @a bad, with the reason in @a errors. */
    QStringList parseList(const QString &text, QStringList *bad = nullptr,
                          QStringList *errors = nullptr);

    /** Is @a ip inside one of @a networks (normalised)? False for IPv6 and
     *  null addresses. */
    bool contains(const QStringList &networks, const QHostAddress &ip);
}

/** @} */

#endif
