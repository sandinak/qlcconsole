/*
  Q Light Controller Plus
  webaccessnetwork.h

  Copyright (c) Massimo Callegari

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

#ifndef WEBACCESSNETWORK_H
#define WEBACCESSNETWORK_H

#include <QObject>

typedef struct
{
    bool enabled;
    QString devName;
    QString connName;
    QString connUUID;
    bool isStatic;
    bool isWireless;
    bool isHotspot;
    QString address;
    QString netmask;
    QString gateway;
    QString dns1;
    QString dns2;
    QString wpaConfFile;
    QString ssid;
    QString wpaPass;
} InterfaceInfo;

class WebAccessNetwork final : public QObject
{
    Q_OBJECT
public:
    WebAccessNetwork(QObject *parent = 0);

    void resetInterface(InterfaceInfo *iface);
    void appendInterface(InterfaceInfo iface);
    QString getInterfaceHTML(InterfaceInfo *iface);
    QString getNetworkHTML();
    QString getHTML();

    bool updateNetworkSettings(QStringList cmdList);
    bool createWiFiHotspot(QString SSID, QString password);
    bool deleteWiFiHotspot();

    /** The nmcli argument lists for a connection profile on @p iface: the
     *  add (static: @p address/@p prefixBits, @p gateway), the Wi-Fi
     *  password when there is one, then up. Each value typed on the web
     *  page (an SSID, a password, an address) is ONE argument: they were
     *  split on spaces, so "My Show Net" was three, and a password could
     *  carry nmcli options of its own. */
    static QList<QStringList> connectionCommands(const InterfaceInfo &iface, bool isStatic,
                                                 const QString &address, int prefixBits,
                                                 const QString &gateway, const QString &ssid,
                                                 const QString &password);

    /** The nmcli argument lists that make the Wi-Fi hotspot (see above) */
    static QList<QStringList> hotspotCommands(const QString &SSID, const QString &password);

protected:
    QStringList getNmcliOutput(QStringList args, bool verbose = false);
    void refreshConnectionsList();

protected:
    QList<InterfaceInfo> m_interfaces;
};

#endif // WEBACCESSNETWORK_H
