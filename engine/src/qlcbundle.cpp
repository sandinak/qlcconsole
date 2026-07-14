/*
  Q Light Controller Plus
  qlcbundle.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "qlcbundle.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QFile>
#include <QRegularExpression>
#include <QDebug>

// ─── BundleEntry ─────────────────────────────────────────────────────────────

QJsonObject BundleEntry::toJson() const
{
    QJsonObject o;
    o["type"] = type;
    if (!name.isEmpty()) o["name"] = name;

    if (type == "Color")
    {
        o["color"] = color;
        if (!wauv.isEmpty()) o["wauv"] = wauv;
    }
    else if (type == "Dimmer")
    {
        o["value"] = dimmerValue;
    }
    else if (type == "Effect")
    {
        o["script"] = script;
        if (!effectPreset.isEmpty()) o["effect"] = effectPreset;
        if (!params.isEmpty())
        {
            QJsonObject p;
            for (auto it = params.constBegin(); it != params.constEnd(); ++it)
                p[it.key()] = it.value();
            o["params"] = p;
        }
    }
    else if (type == "PanTilt")
    {
        o["pan"]  = pan;
        o["tilt"] = tilt;
    }
    else if (type == "Beam")
    {
        o["focus"] = focus;
        o["frost"] = frost;
        o["iris"]  = iris;
    }
    return o;
}

BundleEntry BundleEntry::fromJson(const QJsonObject &o)
{
    BundleEntry e;
    e.type = o["type"].toString();
    e.name = o["name"].toString();

    if (e.type == "Color")
    {
        e.color = o["color"].toString();
        e.wauv  = o["wauv"].toString();
    }
    else if (e.type == "Dimmer")
    {
        e.dimmerValue = o["value"].toInt(128);
    }
    else if (e.type == "Effect")
    {
        e.script       = o["script"].toString();
        e.effectPreset = o["effect"].toString();
        const QJsonObject p = o["params"].toObject();
        for (auto it = p.constBegin(); it != p.constEnd(); ++it)
            e.params[it.key()] = it.value().toDouble();
    }
    else if (e.type == "PanTilt")
    {
        e.pan  = o["pan"].toDouble();
        e.tilt = o["tilt"].toDouble();
    }
    else if (e.type == "Beam")
    {
        e.focus = o["focus"].toInt(128);
        e.frost = o["frost"].toInt(0);
        e.iris  = o["iris"].toInt(255);
    }
    return e;
}

// ─── BundleAttributes ────────────────────────────────────────────────────────

QJsonObject BundleAttributes::toJson() const
{
    QJsonObject o;
    if (!category.isEmpty()) o["category"] = category;
    if (!contains.isEmpty())
    {
        QJsonArray a;
        for (const QString &s : contains) a.append(s);
        o["contains"] = a;
    }
    if (!tempo.isEmpty()) o["tempo"] = tempo;
    if (!mood.isEmpty())  o["mood"]  = mood;
    return o;
}

BundleAttributes BundleAttributes::fromJson(const QJsonObject &o)
{
    BundleAttributes a;
    a.category = o["category"].toString();
    a.tempo    = o["tempo"].toString();
    a.mood     = o["mood"].toString();
    for (const QJsonValue &v : o["contains"].toArray())
        a.contains << v.toString();
    return a;
}

// ─── QLCBundle ───────────────────────────────────────────────────────────────

QLCBundle QLCBundle::fromFile(const QString &filePath)
{
    QLCBundle b;
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "[QLCBundle] cannot read" << filePath;
        return b;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject())
    {
        qWarning() << "[QLCBundle] bad JSON in" << filePath << ":" << err.errorString();
        return b;
    }

    const QJsonObject o = doc.object();
    b.name        = o["name"].toString();
    b.description = o["description"].toString();
    b.author      = o["author"].toString();
    b.created     = o["created"].toString();
    b.modified    = o["modified"].toString();
    b.version     = o["version"].toInt(1);
    b.path        = filePath;

    for (const QJsonValue &v : o["keywords"].toArray())
        b.keywords << v.toString();

    b.attributes = BundleAttributes::fromJson(o["attributes"].toObject());

    for (const QJsonValue &v : o["palettes"].toArray())
        b.palettes << BundleEntry::fromJson(v.toObject());

    return b;
}

bool QLCBundle::saveToFile(const QString &filePath) const
{
    QJsonObject o;
    o["name"]    = name;
    o["version"] = version;
    if (!description.isEmpty()) o["description"] = description;
    if (!author.isEmpty())      o["author"]      = author;
    if (!created.isEmpty())     o["created"]     = created;
    if (!modified.isEmpty())    o["modified"]    = modified;

    if (!keywords.isEmpty())
    {
        QJsonArray kw;
        for (const QString &k : keywords) kw.append(k);
        o["keywords"] = kw;
    }

    const QJsonObject attrsJson = attributes.toJson();
    if (!attrsJson.isEmpty()) o["attributes"] = attrsJson;

    QJsonArray pal;
    for (const BundleEntry &e : palettes) pal.append(e.toJson());
    o["palettes"] = pal;

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        qWarning() << "[QLCBundle] cannot write" << filePath;
        return false;
    }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

QString QLCBundle::sanitiseFilename(const QString &name)
{
    QString s = name;
    s.replace(QRegularExpression("[/\\\\:*?\"<>|]"), "_");
    return s;
}

bool QLCBundle::matches(const QString &text) const
{
    if (text.isEmpty()) return true;
    const QString t = text.toLower();
    if (name.toLower().contains(t))        return true;
    if (description.toLower().contains(t)) return true;
    if (author.toLower().contains(t))      return true;
    for (const QString &kw : keywords)
        if (kw.toLower().contains(t)) return true;
    return false;
}
