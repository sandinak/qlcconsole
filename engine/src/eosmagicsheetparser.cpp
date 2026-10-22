/*
  Q Light Controller
  eosmagicsheetparser.cpp

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

#include <QXmlStreamReader>
#include <QFileInfo>
#include <QRectF>
#include <QFile>

#include "eosmagicsheetparser.h"

QList<EosMagicSheetObject> EosMagicSheet::channels() const
{
    QList<EosMagicSheetObject> out;
    for (const EosMagicSheetObject &o : objects)
        if (o.kind == EosMagicSheetObject::Channel)
            out << o;
    return out;
}

EosMagicSheetParser::EosMagicSheetParser()
{
}

EosMagicSheetParser::~EosMagicSheetParser()
{
}

EosMagicSheet EosMagicSheetParser::parse(const QString &path, QStringList &warnings)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        warnings << QObject::tr("Could not open magic sheet %1: %2").arg(path, f.errorString());
        return EosMagicSheet();
    }
    return parseXml(f.readAll(), QFileInfo(path).completeBaseName(), warnings);
}

namespace
{

EosMagicSheetObject::Kind kindOf(int type)
{
    switch (type)
    {
        case EosMagicSheetObject::Channel:
        case EosMagicSheetObject::Cue:
        case EosMagicSheetObject::Group:
        case EosMagicSheetObject::Macro:
        case EosMagicSheetObject::Palette:
        case EosMagicSheetObject::Preset:
        case EosMagicSheetObject::Sub:
        case EosMagicSheetObject::Effect:
        case EosMagicSheetObject::NestedSheet:
        case EosMagicSheetObject::Fader:
            return EosMagicSheetObject::Kind(type);
        default:
            return EosMagicSheetObject::Unknown;
    }
}

} // namespace

EosMagicSheet EosMagicSheetParser::parseXml(const QByteArray &xml, const QString &name, QStringList &warnings)
{
    EosMagicSheet sheet;
    QXmlStreamReader r(xml);
    bool rooted = false;
    QString version;

    // Inside an ITEM: what it says so far.
    int itemDepth = -1, depth = 0;
    EosMagicSheetObject obj;
    bool hasTarget = false, hasRect = false;
    double posX = 0, posY = 0;
    QRectF rect;

    while (!r.atEnd())
    {
        const QXmlStreamReader::TokenType t = r.readNext();
        if (t == QXmlStreamReader::StartElement)
        {
            ++depth;
            const QString el = r.name().toString().toUpper();
            const QXmlStreamAttributes a = r.attributes();
            if (depth == 1)
            {
                if (el != QLatin1String("MAGICSHEET"))
                {
                    warnings << QObject::tr("%1 is not an Eos magic sheet export.").arg(name);
                    return EosMagicSheet();
                }
                rooted = true;
                version = a.value(QStringLiteral("SHOWFILE_VERSION")).toString();
                continue;
            }
            if (el == QLatin1String("ITEM") && itemDepth < 0)
            {
                itemDepth = depth;
                obj = EosMagicSheetObject();
                obj.key = a.value(QStringLiteral("KEY")).toString();
                posX = a.value(QStringLiteral("POSX")).toDouble();
                posY = a.value(QStringLiteral("POSY")).toDouble();
                hasTarget = hasRect = false;
                rect = QRectF();
                continue;
            }
            if (itemDepth < 0)
                continue;
            if (el == QLatin1String("RECT") && !hasRect)
            {
                hasRect = true;
                rect = QRectF(a.value(QStringLiteral("X")).toDouble(), a.value(QStringLiteral("Y")).toDouble(),
                              a.value(QStringLiteral("W")).toDouble(), a.value(QStringLiteral("H")).toDouble());
            }
            else if (el == QLatin1String("TARGET") && !hasTarget)
            {
                obj.targetType = a.value(QStringLiteral("TARGETTYPE")).toInt();
                obj.targetId = a.value(QStringLiteral("TARGETID")).toString();
                obj.targetListId = a.value(QStringLiteral("TARGETLISTID")).toInt();
                hasTarget = obj.targetType != 0 && !obj.targetId.isEmpty();
            }
            else if (el == QLatin1String("TEXT") && obj.label.isEmpty())
            {
                obj.label = a.value(QStringLiteral("STR")).toString();
            }
        }
        else if (t == QXmlStreamReader::EndElement)
        {
            if (depth == itemDepth)
            {
                itemDepth = -1;
                obj.position = hasRect ? QPointF(posX + rect.x() + rect.width() / 2,
                                                 posY + rect.y() + rect.height() / 2)
                                       : QPointF(posX, posY);
                if (!hasTarget)
                {
                    sheet.imagesDropped++;
                }
                else
                {
                    obj.kind = kindOf(obj.targetType);
                    if (obj.kind == EosMagicSheetObject::Unknown)
                        sheet.unknownKindsDropped++;
                    else
                        sheet.objects << obj;
                }
            }
            --depth;
        }
    }
    if (r.hasError() || !rooted)
    {
        warnings << QObject::tr("Magic sheet %1 could not be read: %2").arg(name, r.hasError() ? r.errorString()
                                                                                       : QObject::tr("empty"));
        return EosMagicSheet();
    }
    sheet.name = name;
    sheet.uid = version.isEmpty() ? name : QStringLiteral("%1/%2").arg(version, name);
    return sheet;
}
