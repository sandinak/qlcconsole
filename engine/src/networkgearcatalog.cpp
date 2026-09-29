/*
  Q Light Controller Plus
  networkgearcatalog.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

#include "networkgearcatalog.h"
#include "inputoutputmap.h"
#include "qlcfile.h"

#define USERNETWORKGEARDIR "Library/Application Support/qlcconsole/NetworkGear"

namespace
{
    QString &overridePath()
    {
        static QString path;
        return path;
    }

    NetworkGearModel gear(const char *manufacturer, const char *model, int transport, int ports)
    {
        NetworkGearModel m;
        m.manufacturer = QString::fromUtf8(manufacturer);
        m.model = QString::fromUtf8(model);
        m.transport = transport;
        m.ports = ports;
        return m;
    }

    bool byName(const NetworkGearModel &a, const NetworkGearModel &b)
    {
        const int c = a.manufacturer.compare(b.manufacturer, Qt::CaseInsensitive);
        if (c != 0)
            return c < 0;
        return a.model.compare(b.model, Qt::CaseInsensitive) < 0;
    }

    bool fromJson(const QJsonObject &o, NetworkGearModel &m)
    {
        m.manufacturer = o.value("manufacturer").toString().trimmed();
        m.model = o.value("model").toString().trimmed();
        m.transport = NetworkGearCatalog::transportFromName(o.value("transport").toString());
        m.ports = o.value("ports").toInt(0);
        m.builtIn = false;
        return !m.model.isEmpty() && m.transport >= 0 && m.ports >= 1 && m.ports <= 256;
    }
}

QString NetworkGearModel::displayName() const
{
    return manufacturer.isEmpty() ? model : QString("%1 %2").arg(manufacturer, model);
}

QList<NetworkGearModel> NetworkGearCatalog::builtIn()
{
    const int A = NetworkNode::ArtNet, S = NetworkNode::sACN,
              I = NetworkNode::Interface, W = NetworkNode::WirelessDMX;
    QList<NetworkGearModel> list;
    list << gear("ENTTEC", "DMX USB Pro", I, 1)
         << gear("ENTTEC", "DMX USB Pro Mk2", I, 2)
         << gear("ENTTEC", "ODE Mk3", A, 2)
         << gear("ENTTEC", "Storm 8", A, 8)
         << gear("ENTTEC", "Storm 24", A, 24)
         << gear("DMXking", "eDMX1 PRO", A, 1)
         << gear("DMXking", "eDMX2 PRO", A, 2)
         << gear("DMXking", "eDMX4 PRO", A, 4)
         << gear("DMXking", "ultraDMX Micro", I, 1)
         << gear("Chauvet Professional", "Net-X II", A, 8)
         << gear("ETC", "Response Mk2 Four-Port Gateway", S, 4)
         << gear("Wireless Solution", "BlackBox F-1 G5", W, 1)
         << gear("Generic", "ArtNet node, 1 port", A, 1)
         << gear("Generic", "ArtNet node, 2 ports", A, 2)
         << gear("Generic", "ArtNet node, 4 ports", A, 4)
         << gear("Generic", "ArtNet node, 8 ports", A, 8)
         << gear("Generic", "sACN node, 4 ports", S, 4)
         << gear("Generic", "USB DMX interface", I, 1)
         << gear("Generic", "Wireless DMX transmitter", W, 1);
    std::stable_sort(list.begin(), list.end(), byName);
    return list;
}

void NetworkGearCatalog::setUserDirectoryOverride(const QString &path)
{
    overridePath() = path;
}

QDir NetworkGearCatalog::userDirectory()
{
    if (!overridePath().isEmpty())
    {
        QDir dir(overridePath());
        if (!dir.exists())
            dir.mkpath(".");
        dir.setFilter(QDir::Files);
        dir.setNameFilters(QStringList() << "*.json");
        return dir;
    }
    return QLCFile::userDirectory(QString(USERNETWORKGEARDIR), QString(), QStringList() << "*.json");
}

QList<NetworkGearModel> NetworkGearCatalog::userModels()
{
    QList<NetworkGearModel> list;
    const QDir dir = userDirectory();
    foreach (const QString &fn, dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name))
    {
        QFile f(dir.absoluteFilePath(fn));
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        QJsonArray items;
        if (doc.isArray())
            items = doc.array();
        else if (doc.isObject())
            items.append(doc.object());
        for (const QJsonValue &v : items)
        {
            NetworkGearModel m;
            if (v.isObject() && fromJson(v.toObject(), m))
                list << m;
        }
    }
    std::stable_sort(list.begin(), list.end(), byName);
    return list;
}

QList<NetworkGearModel> NetworkGearCatalog::all()
{
    QList<NetworkGearModel> list = userModels();
    QSet<QString> theirs;
    for (const NetworkGearModel &m : list)
        theirs.insert(m.displayName().toLower());
    for (const NetworkGearModel &m : builtIn())
        if (!theirs.contains(m.displayName().toLower()))
            list << m;
    return list;
}

NetworkNode NetworkGearCatalog::makeNode(const NetworkGearModel &model, const QString &name)
{
    NetworkNode node;
    node.name = name.trimmed().isEmpty() ? model.model : name.trimmed();
    node.transport = model.transport;
    node.model = model.displayName();
    for (int i = 0; i < model.ports; ++i)
    {
        NetworkPort port;
        port.universe = InputOutputMap::invalidUniverse();
        if (node.isWireless())
            port.receiver.name = model.ports > 1 ? QString("%1 RX %2").arg(node.name).arg(i + 1)
                                                 : QString("%1 RX").arg(node.name);
        node.ports << port;
    }
    return node;
}

QString NetworkGearCatalog::saveUserModel(const NetworkGearModel &model)
{
    if (model.model.trimmed().isEmpty() || model.ports < 1)
        return QString();
    QJsonObject o;
    o.insert("manufacturer", model.manufacturer.trimmed());
    o.insert("model", model.model.trimmed());
    o.insert("transport", transportToName(model.transport));
    o.insert("ports", model.ports);

    // A file name from the model's name, with nothing a filesystem minds.
    QString base = model.displayName().trimmed();
    base.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9 ._+-]")), QStringLiteral("_"));
    const QString path = userDirectory().absoluteFilePath(base + ".json");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return path;
}

int NetworkGearCatalog::transportFromName(const QString &name)
{
    const QString n = name.trimmed().toLower();
    if (n == "artnet" || n == "art-net")        return NetworkNode::ArtNet;
    if (n == "sacn" || n == "e1.31")            return NetworkNode::sACN;
    if (n == "interface" || n == "usb")         return NetworkNode::Interface;
    if (n == "wireless dmx" || n == "wireless") return NetworkNode::WirelessDMX;
    return -1;
}

QString NetworkGearCatalog::transportToName(int transport)
{
    return NetworkNode::transportName(transport);
}
