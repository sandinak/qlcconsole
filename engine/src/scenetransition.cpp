/*
  Q Light Controller Plus
  scenetransition.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QCoreApplication>
#include <QVector3D>
#include <QtMath>

#include "qlcfixturehead.h"
#include "scenetransition.h"
#include "righeads.h"
#include "fixture.h"
#include "doc.h"

namespace
{
const char *const kShapeNames[] = {
    "None", "Fade", "WipeToStageLeft", "WipeToStageRight", "WipeUp", "WipeDown",
    "WipeUpstage", "WipeDownstage", "FromCentre", "ToCentre", "Dissolve", "AlongBars" };
const int kShapes = int(sizeof(kShapeNames) / sizeof(kShapeNames[0]));
}

void SceneTransition::timing(double place, uint &delay, uint &fade) const
{
    const double soft = (shape == Fade) ? 1.0 : qBound(0.05, softness, 1.0);
    const double p = (shape == Fade) ? 0.0 : qBound(0.0, place, 1.0);
    delay = uint(qRound(p * (1.0 - soft) * time));
    fade = qMax(1u, uint(qRound(soft * time)));
}

double SceneTransition::progress(double place, uint elapsed) const
{
    uint delay = 0, fade = 1;
    timing(place, delay, fade);
    if (elapsed <= delay)
        return 0.0;
    return qMin(1.0, double(elapsed - delay) / double(fade));
}

QString SceneTransition::shapeToString(Shape shape)
{
    const int i = int(shape);
    return QLatin1String(kShapeNames[(i >= 0 && i < kShapes) ? i : 0]);
}

SceneTransition::Shape SceneTransition::stringToShape(const QString &str)
{
    for (int i = 0; i < kShapes; ++i)
        if (str == QLatin1String(kShapeNames[i]))
            return Shape(i);
    return None;
}

QStringList SceneTransition::shapeLabels()
{
    return QStringList()
        << QCoreApplication::translate("SceneTransition", "None (the scene's fade)")
        << QCoreApplication::translate("SceneTransition", "Fade")
        << QCoreApplication::translate("SceneTransition", "Wipe to stage left")
        << QCoreApplication::translate("SceneTransition", "Wipe to stage right")
        << QCoreApplication::translate("SceneTransition", "Wipe up")
        << QCoreApplication::translate("SceneTransition", "Wipe down")
        << QCoreApplication::translate("SceneTransition", "Wipe front to back")
        << QCoreApplication::translate("SceneTransition", "Wipe back to front")
        << QCoreApplication::translate("SceneTransition", "From the centre")
        << QCoreApplication::translate("SceneTransition", "To the centre")
        << QCoreApplication::translate("SceneTransition", "Dissolve")
        << QCoreApplication::translate("SceneTransition", "Along each bar");
}

void SceneTransition::saveXML(QXmlStreamWriter *doc, const QString &kind) const
{
    if (!isActive())
        return;
    doc->writeStartElement(QStringLiteral("Transition"));
    doc->writeAttribute(QStringLiteral("Kind"), kind);
    doc->writeAttribute(QStringLiteral("Shape"), shapeToString(shape));
    doc->writeAttribute(QStringLiteral("Time"), QString::number(time));
    doc->writeAttribute(QStringLiteral("Soft"), QString::number(softness, 'f', 2));
    doc->writeEndElement();
}

bool SceneTransition::loadXML(QXmlStreamReader &root, QString &kind)
{
    if (root.name() != QLatin1String("Transition"))
        return false;
    const QXmlStreamAttributes a = root.attributes();
    kind = a.value(QStringLiteral("Kind")).toString();
    shape = stringToShape(a.value(QStringLiteral("Shape")).toString());
    if (a.hasAttribute(QStringLiteral("Time")))
        time = a.value(QStringLiteral("Time")).toUInt();
    if (a.hasAttribute(QStringLiteral("Soft")))
        softness = qBound(0.0, a.value(QStringLiteral("Soft")).toDouble(), 1.0);
    root.skipCurrentElement();
    return true;
}

TransitionMap::TransitionMap(Doc *doc, const QList<quint32> &fixtures, SceneTransition::Shape shape)
{
    if (doc == nullptr || shape == SceneTransition::None || shape == SceneTransition::Fade)
        return;

    // Every head where it is on the stage.
    struct Head { quint32 fid; int head; QVector3D at; int heads; };
    QList<Head> heads;
    for (quint32 fid : fixtures)
    {
        const QVector<QVector3D> at = RigHeads::headPositions(doc, fid);
        for (int h = 0; h < at.size(); ++h)
            heads << Head{ fid, h, at.at(h), int(at.size()) };
    }
    if (heads.isEmpty())
        return;

    QVector3D lo = heads.first().at, hi = lo, centre;
    for (const Head &h : heads)
    {
        lo = QVector3D(qMin(lo.x(), h.at.x()), qMin(lo.y(), h.at.y()), qMin(lo.z(), h.at.z()));
        hi = QVector3D(qMax(hi.x(), h.at.x()), qMax(hi.y(), h.at.y()), qMax(hi.z(), h.at.z()));
        centre += h.at;
    }
    centre /= float(heads.size());
    double reach = 0.0;
    for (const Head &h : heads)
        reach = qMax(reach, double((h.at - centre).length()));

    auto along = [](float v, float a, float b) {
        return (b - a) > 1e-3f ? double((v - a) / (b - a)) : 0.0;
    };
    QHash<quint32, QPair<double, int> > sums;       // for each fixture as a whole
    for (const Head &h : heads)
    {
        double u = 0.0;
        switch (shape)
        {
        case SceneTransition::WipeToStageLeft:  u = along(h.at.x(), lo.x(), hi.x()); break;
        case SceneTransition::WipeToStageRight: u = 1.0 - along(h.at.x(), lo.x(), hi.x()); break;
        case SceneTransition::WipeUp:           u = along(h.at.z(), lo.z(), hi.z()); break;
        case SceneTransition::WipeDown:         u = 1.0 - along(h.at.z(), lo.z(), hi.z()); break;
        // +Y is downstage: the front first, then back.
        case SceneTransition::WipeUpstage:      u = 1.0 - along(h.at.y(), lo.y(), hi.y()); break;
        case SceneTransition::WipeDownstage:    u = along(h.at.y(), lo.y(), hi.y()); break;
        case SceneTransition::FromCentre:       u = reach > 1e-3 ? double((h.at - centre).length()) / reach : 0.0; break;
        case SceneTransition::ToCentre:         u = reach > 1e-3 ? 1.0 - double((h.at - centre).length()) / reach : 0.0; break;
        case SceneTransition::Dissolve:
        {
            // The same order every time it runs (a hash, not a dice roll).
            quint32 x = h.fid * 2654435761u ^ quint32(h.head + 1) * 2246822519u;
            x ^= x >> 15; x *= 2246822519u; x ^= x >> 13;
            u = double(x % 10000u) / 10000.0;
            break;
        }
        case SceneTransition::AlongBars:        u = h.heads > 1 ? double(h.head) / (h.heads - 1) : 0.0; break;
        default: break;
        }
        u = qBound(0.0, u, 1.0);
        m_places.insert(key(h.fid, h.head), u);
        sums[h.fid].first += u;
        sums[h.fid].second += 1;
    }
    for (auto it = sums.constBegin(); it != sums.constEnd(); ++it)
        m_places.insert(key(it.key(), -1), it.value().first / qMax(1, it.value().second));
}

double TransitionMap::place(quint32 fid, int head) const
{
    auto it = m_places.constFind(key(fid, head));
    if (it != m_places.constEnd())
        return it.value();
    return m_places.value(key(fid, -1), 0.0);
}

double TransitionMap::placeForChannel(const Fixture *fx, quint32 channel) const
{
    if (fx == nullptr)
        return 0.0;
    for (int h = 0; h < fx->heads(); ++h)
        if (fx->head(h).channels().contains(channel))
            return place(fx->id(), h);
    return place(fx->id(), -1);
}
