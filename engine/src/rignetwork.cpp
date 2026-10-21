/*
  Q Light Controller Plus
  rignetwork.cpp

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
#include <QRegularExpression>

#include "rignetwork.h"

namespace
{
    QString tr(const char *text)
    {
        return QCoreApplication::translate("RigNetwork", text);
    }
}

QString RigNetwork::normalise(const QString &text, QString *error)
{
    const QString t = text.trimmed();
    const int slash = t.indexOf('/');
    if (slash < 0)
    {
        if (error)
            *error = tr("add the network size, e.g. 172.18.2.0/24");
        return QString();
    }

    /* QHostAddress accepts shorthand ("10.1" is 10.0.0.1); a network
       someone types into a show file should be the dotted four. */
    static const QRegularExpression dotted(QStringLiteral("^\\d{1,3}(\\.\\d{1,3}){3}$"));
    const QString addr = t.left(slash).trimmed();
    const QHostAddress ip(addr);
    if (dotted.match(addr).hasMatch() == false || ip.isNull()
        || ip.protocol() != QAbstractSocket::IPv4Protocol)
    {
        if (error)
            *error = tr("not an IPv4 network");
        return QString();
    }

    bool ok = false;
    const int prefix = t.mid(slash + 1).trimmed().toInt(&ok);
    if (ok == false || prefix < 1 || prefix > 32)
    {
        if (error)
            *error = tr("the size after / must be 1 to 32");
        return QString();
    }

    const quint32 mask = 0xFFFFFFFFu << (32 - prefix);
    return QString("%1/%2").arg(QHostAddress(ip.toIPv4Address() & mask).toString())
                           .arg(prefix);
}

QStringList RigNetwork::parseList(const QString &text, QStringList *bad, QStringList *errors)
{
    static const QRegularExpression sep(QStringLiteral("[,;\\s]+"));
    QStringList out;
    foreach (const QString &entry, text.split(sep, Qt::SkipEmptyParts))
    {
        QString why;
        const QString net = normalise(entry, &why);
        if (net.isEmpty())
        {
            if (bad)
                *bad << entry;
            if (errors)
                *errors << why;
            continue;
        }
        if (out.contains(net) == false)
            out << net;
    }
    return out;
}

bool RigNetwork::contains(const QStringList &networks, const QHostAddress &ip)
{
    if (ip.isNull() || ip.protocol() != QAbstractSocket::IPv4Protocol)
        return false;
    foreach (const QString &net, networks)
    {
        const QPair<QHostAddress, int> subnet = QHostAddress::parseSubnet(net);
        if (subnet.first.protocol() == QAbstractSocket::IPv4Protocol && ip.isInSubnet(subnet))
            return true;
    }
    return false;
}
