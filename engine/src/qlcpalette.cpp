/*
  Q Light Controller Plus
  qlcpalette.cpp

  Copyright (C) Massimo Callegari

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
#include <QXmlStreamWriter>
#include <QtMath>
#include <QDebug>

#include "monitorproperties.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "qlccapability.h"
#include "qlcfixturehead.h"
#include "qlcfixturemode.h"
#include "scenevalue.h"
#include "doc.h"

#define KXMLQLCPaletteType      QStringLiteral("Type")
#define KXMLQLCPaletteName      QStringLiteral("Name")
#define KXMLQLCPalettePath      QStringLiteral("Path")
#define KXMLQLCPaletteValue     QStringLiteral("Value")
#define KXMLQLCPaletteFanning     QStringLiteral("Fan")
#define KXMLQLCPaletteFanLayout   QStringLiteral("Layout")
#define KXMLQLCPaletteFanAmount   QStringLiteral("Amount")
#define KXMLQLCPaletteFanValue    QStringLiteral("FanValue")
#define KXMLQLCPaletteStageTarget QStringLiteral("StageTarget")

QLCPalette::QLCPalette(QLCPalette::PaletteType type, QObject *parent)
    : QObject(parent)
    , m_id(QLCPalette::invalidId())
    , m_type(type)
    , m_fanningType(Flat)
    , m_fanningLayout(XAscending)
    , m_fanningAmount(100)
{
}

QLCPalette *QLCPalette::createCopy()
{
    QLCPalette *copy = new QLCPalette(type());
    copy->setValues(this->values());
    copy->setName(this->name());
    copy->setPath(this->path());
    copy->setFanningType(this->fanningType());
    copy->setFanningLayout(this->fanningLayout());
    copy->setFanningAmount(this->fanningAmount());
    copy->setFanningValue(this->fanningValue());

    return copy;
}

QLCPalette::~QLCPalette()
{

}

/************************************************************************
 * Properties
 ************************************************************************/

quint32 QLCPalette::id() const
{
    return m_id;
}

void QLCPalette::setID(quint32 id)
{
    m_id = id;
}

quint32 QLCPalette::invalidId()
{
    return UINT_MAX;
}

QLCPalette::PaletteType QLCPalette::type() const
{
    return m_type;
}

QString QLCPalette::typeToString(QLCPalette::PaletteType type)
{
    switch (type)
    {
        case Dimmer:    return "Dimmer";
        case Color:     return "Color";
        case Pan:       return "Pan";
        case Tilt:      return "Tilt";
        case PanTilt:   return "PanTilt";
        case Shutter:   return "Shutter";
        case Gobo:      return "Gobo";
        case Zoom:      return "Zoom";
        case Beam:      return "Beam";
        case Effect:    return "Effect";
        case Undefined: return "";
    }

    return "";
}

QLCPalette::PaletteType QLCPalette::stringToType(const QString &str)
{
    if (str == "Dimmer")
        return Dimmer;
    else if (str == "Color")
        return Color;
    else if (str == "Pan")
        return Pan;
    else if (str == "Tilt")
        return Tilt;
    else if (str == "PanTilt")
        return PanTilt;
    else if (str == "Shutter")
        return Shutter;
    else if (str == "Gobo")
        return Gobo;
    else if (str == "Zoom")
        return Zoom;
    else if (str == "Beam")
        return Beam;
    else if (str == "Effect")
        return Effect;

    return Undefined;
}

QString QLCPalette::iconResource(bool svg) const
{
    QString prefix = svg ? "qrc" : "";
    QString ext = svg ? "svg" : "png";

    switch(type())
    {
        case Dimmer: return QString("%1:/intensity.%2").arg(prefix).arg(ext);
        case Color: return QString("%1:/color.%2").arg(prefix).arg(ext);
        case Pan: return QString("%1:/pan.%2").arg(prefix).arg(ext);
        case Tilt: return QString("%1:/tilt.%2").arg(prefix).arg(ext);
        case PanTilt: return QString("%1:/position.%2").arg(prefix).arg(ext);
        case Shutter: return QString("%1:/shutter.%2").arg(prefix).arg(ext);
        case Gobo: return QString("%1:/gobo.%2").arg(prefix).arg(ext);
        case Zoom: return QString("%1:/beam.%2").arg(prefix).arg(ext);
        case Beam:   return QString("%1:/beam.%2").arg(prefix).arg(ext);
        case Effect: return QString("%1:/script.%2").arg(prefix).arg(ext);
        default: return "";
    }
}

QString QLCPalette::name() const
{
    return m_name;
}

void QLCPalette::setName(const QString &name)
{
    if (name == m_name)
        return;

    m_name = QString(name);
    emit nameChanged();
}

QString QLCPalette::path() const
{
    return m_path;
}

void QLCPalette::setPath(const QString &path)
{
    m_path = path;
}

QVariant QLCPalette::value() const
{
    if (m_values.isEmpty())
        return QVariant();

    return m_values.first();
}

int QLCPalette::intValue1() const
{
    if (m_values.isEmpty())
        return -1;

    return m_values.at(0).toInt();
}

int QLCPalette::intValue2() const
{
    if (m_values.count() < 2)
        return -1;

    return m_values.at(1).toInt();
}

float QLCPalette::floatValue1() const
{
    if (m_values.isEmpty())
        return -1;

    return m_values.at(0).toFloat();
}

QString QLCPalette::strValue1() const
{
    if (m_values.isEmpty())
        return QString();

    return m_values.at(0).toString();
}

QColor QLCPalette::rgbValue() const
{
    if (m_values.isEmpty())
        return QColor();

    QColor rgb, wauv;
    stringToColor(m_values.at(0).toString(), rgb, wauv);

    return rgb;
}

QColor QLCPalette::wauvValue() const
{
    if (m_values.isEmpty())
        return QColor();

    QColor rgb, wauv;
    stringToColor(m_values.at(0).toString(), rgb, wauv);

    return wauv;
}

void QLCPalette::setValue(QVariant val)
{
    m_values.clear();
    m_values.append(val);
}

void QLCPalette::setValue(QVariant val1, QVariant val2)
{
    m_values.clear();
    m_values.append(val1);
    m_values.append(val2);
}

QVariantList QLCPalette::values() const
{
    return m_values;
}

void QLCPalette::setValues(QVariantList values)
{
    m_values = values;
}

void QLCPalette::resetValues()
{
    m_values.clear();
}

QList<SceneValue> QLCPalette::valuesFromFixtures(Doc *doc, QList<quint32> fixtures)
{
    QList<SceneValue> list;

    int fxCount = fixtures.count();
    // normalized progress in [ 0.0, 1.0 ] range
    qreal progress = 0.0;
    int intFanValue = fanningValue().toInt();
    FanningType fType = fanningType();
    FanningLayout fLayout = fanningLayout();
    MonitorProperties *mProps = doc->monitorProperties();
    qreal centerCoord = 0.0;
    qreal maxCenterDistance = 0.0;

    if (fLayout == XCentered || fLayout == YCentered || fLayout == ZCentered)
    {
        bool hasCoord = false;
        qreal minCoord = 0.0;
        qreal maxCoord = 0.0;

        foreach (quint32 id, fixtures)
        {
            QVector3D pos = mProps->fixturePosition(id, 0, 0);
            qreal coord = 0.0;

            switch (fLayout)
            {
                case XCentered: coord = pos.x(); break;
                case YCentered: coord = pos.y(); break;
                case ZCentered: coord = pos.z(); break;
                default: break;
            }

            if (hasCoord == false)
            {
                minCoord = coord;
                maxCoord = coord;
                hasCoord = true;
            }
            else
            {
                minCoord = qMin(minCoord, coord);
                maxCoord = qMax(maxCoord, coord);
            }
        }

        centerCoord = (minCoord + maxCoord) / 2.0;
        maxCenterDistance = qMax(qAbs(minCoord - centerCoord), qAbs(maxCoord - centerCoord));
    }

    // sort the fixtures list based on selected layout
    std::sort(fixtures.begin(), fixtures.end(),
        [fLayout, mProps, centerCoord](quint32 a, quint32 b) {
            QVector3D posA = mProps->fixturePosition(a, 0, 0);
            QVector3D posB = mProps->fixturePosition(b, 0, 0);

            switch(fLayout)
            {
                case XAscending: return posA.x() < posB.x();
                case XDescending: return posB.x() < posA.x();
                case XCentered:
                {
                    qreal distA = qAbs(posA.x() - centerCoord);
                    qreal distB = qAbs(posB.x() - centerCoord);
                    return (distA == distB) ? (posA.x() < posB.x()) : (distA < distB);
                }
                case YAscending: return posA.y() < posB.y();
                case YDescending: return posB.y() < posA.y();
                case YCentered:
                {
                    qreal distA = qAbs(posA.y() - centerCoord);
                    qreal distB = qAbs(posB.y() - centerCoord);
                    return (distA == distB) ? (posA.y() < posB.y()) : (distA < distB);
                }
                case ZAscending: return posA.z() < posB.z();
                case ZDescending: return posB.z() < posA.z();
                case ZCentered:
                {
                    qreal distA = qAbs(posA.z() - centerCoord);
                    qreal distB = qAbs(posB.z() - centerCoord);
                    return (distA == distB) ? (posA.z() < posB.z()) : (distA < distB);
                }
                default: return false;
            }
        });

    foreach (quint32 id, fixtures)
    {
        Fixture *fixture = doc->fixture(id);
        if (fixture == NULL)
            continue;

        qreal factor = valueFactor(progress);
        int centeredSign = 1;

        if (fLayout == XCentered || fLayout == YCentered || fLayout == ZCentered)
        {
            QVector3D pos = mProps->fixturePosition(id, 0, 0);
            qreal coord = 0.0;

            switch (fLayout)
            {
                case XCentered: coord = pos.x(); break;
                case YCentered: coord = pos.y(); break;
                case ZCentered: coord = pos.z(); break;
                default: break;
            }

            if (coord > centerCoord)
                centeredSign = 1;
            else if (coord < centerCoord)
                centeredSign = -1;
            else
                centeredSign = 0;

            if (maxCenterDistance > 0.0)
            {
                qreal distance = qAbs(coord - centerCoord);
                factor = valueFactor(distance / maxCenterDistance);
            }
            else
            {
                factor = valueFactor(0.0);
            }
        }

        switch(type())
        {
            case Dimmer:
            {
                int dValue = value().toInt();
                quint32 masterIntensityChannel = fixture->type() == QLCFixtureDef::Dimmer ?
                            0 : fixture->masterIntensityChannel();

                if (fType != Flat)
                    dValue = int((qreal(intFanValue - dValue) * factor) + dValue);

                if (masterIntensityChannel != QLCChannel::invalid())
                    list << SceneValue(id, masterIntensityChannel, uchar(dValue));

                for (int i = 0; i < fixture->heads(); i++)
                {
                    quint32 headDimmerChannel = fixture->channelNumber(QLCChannel::Intensity, QLCChannel::MSB, i);
                    if (headDimmerChannel != QLCChannel::invalid())
                        list << SceneValue(id, headDimmerChannel, uchar(dValue));
                }
            }
            break;
            case Color:
            {

                QColor startColor = rgbValue();
                QColor col = startColor;
                QColor wauv = wauvValue();

                if (fType != Flat)
                {
                    QColor endColor = fanningValue().value<QColor>();
                    qreal rDelta = endColor.red() - startColor.red();
                    qreal gDelta = endColor.green() - startColor.green();
                    qreal bDelta = endColor.blue() - startColor.blue();
                    col.setRed(startColor.red() + qRound(rDelta * factor));
                    col.setGreen(startColor.green() + qRound(gDelta * factor));
                    col.setBlue(startColor.blue() + qRound(bDelta * factor));
                }

                for (int i = 0; i < fixture->heads(); i++)
                {
                    QVector<quint32> rgbCh = fixture->rgbChannels(i);
                    if (rgbCh.size() == 3)
                    {
                        list << SceneValue(id, rgbCh.at(0), uchar(col.red()));
                        list << SceneValue(id, rgbCh.at(1), uchar(col.green()));
                        list << SceneValue(id, rgbCh.at(2), uchar(col.blue()));
                    }
                    QVector<quint32> cmyCh = fixture->cmyChannels(i);
                    if (cmyCh.size() == 3)
                    {
                        list << SceneValue(id, cmyCh.at(0), uchar(col.cyan()));
                        list << SceneValue(id, cmyCh.at(1), uchar(col.magenta()));
                        list << SceneValue(id, cmyCh.at(2), uchar(col.yellow()));
                    }

                    // Color-wheel fixtures: snap to the nearest wheel slot.
                    // Only applies when the fixture has no RGB/CMY channels
                    // (those are already handled above with exact mixing).
                    if (rgbCh.size() < 3 && cmyCh.size() < 3)
                    {
                        QLCFixtureMode *mode = fixture->fixtureMode();
                        QLCFixtureHead fHead = fixture->head(i);
                        const QVector<quint32> &wheels = fHead.colorWheels();
                        for (quint32 wheelCh : wheels)
                        {
                            QLCChannel *ch = mode ? mode->channel(wheelCh) : nullptr;
                            if (!ch)
                                continue;
                            // Walk all 256 slots; keep the one whose colour is
                            // closest (squared RGB distance) to col.
                            int bestDmx  = -1;
                            qint64 bestDist = LLONG_MAX;
                            for (int v = 0; v < 256; ++v)
                            {
                                QLCCapability *cap = ch->searchCapability(uchar(v));
                                if (!cap)
                                    continue;
                                QColor slotCol = cap->resource(0).value<QColor>();
                                if (!slotCol.isValid())
                                {
                                    // Fall back to name-based colour (e.g. "Blue")
                                    slotCol = QColor(cap->name().trimmed());
                                }
                                if (!slotCol.isValid())
                                    continue;
                                qint64 dr = col.red()   - slotCol.red();
                                qint64 dg = col.green() - slotCol.green();
                                qint64 db = col.blue()  - slotCol.blue();
                                qint64 dist = dr*dr + dg*dg + db*db;
                                if (dist < bestDist)
                                {
                                    bestDist = dist;
                                    bestDmx  = v;
                                }
                                // Advance v to skip identical caps in the range
                                if (cap->max() > uchar(v))
                                    v = int(cap->max());
                            }
                            if (bestDmx >= 0)
                                list << SceneValue(id, wheelCh, uchar(bestDmx));
                        }
                    }

                    // Extra emitters (White/Amber/UV). Use the explicit wauv
                    // value if the palette has one; otherwise auto-derive White
                    // additively (W = min(R,G,B), per fixture) so RGBW fixtures
                    // light their white emitter for whites/pastels — Amber/UV
                    // default to 0. (Covers legacy RGB-only colour palettes.)
                    QColor emitters = wauv;
                    if (emitters.isValid() == false)
                        emitters = QColor(qMin(col.red(), qMin(col.green(), col.blue())), 0, 0);
                    {
                        quint32 channel = fixture->channelNumber(QLCChannel::White, QLCChannel::MSB, i);
                        if (channel != QLCChannel::invalid())
                            list << SceneValue(id, channel, uchar(emitters.red()));
                        channel = fixture->channelNumber(QLCChannel::Amber, QLCChannel::MSB, i);
                        if (channel != QLCChannel::invalid())
                            list << SceneValue(id, channel, uchar(emitters.green()));
                        channel = fixture->channelNumber(QLCChannel::UV, QLCChannel::MSB, i);
                        if (channel != QLCChannel::invalid())
                            list << SceneValue(id, channel, uchar(emitters.blue()));
                    }
                }
            }
            break;
            case Pan:
            {
                int degrees = value().toInt();

                if (fType != Flat)
                {
                    int offset = int(qreal(intFanValue) * factor);
                    if (fLayout == XCentered || fLayout == YCentered || fLayout == ZCentered)
                        offset *= centeredSign;
                    degrees = int(qreal(degrees) + qreal(offset));
                }

                list << fixture->positionToValues(QLCChannel::Pan, degrees);
            }
            break;
            case Tilt:
            {
                int degrees = m_values.count() == 2 ? m_values.at(1).toInt() : value().toInt();

                if (fType != Flat)
                {
                    int offset = int(qreal(intFanValue) * factor);
                    if (fLayout == XCentered || fLayout == YCentered || fLayout == ZCentered)
                        offset *= centeredSign;
                    degrees = int(qreal(degrees) + qreal(offset));
                }

                list << fixture->positionToValues(QLCChannel::Tilt, degrees);
            }
            break;
            case PanTilt:
            {
                bool usedTarget = false;

                // If this palette is linked to a stage target and the fixture has
                // a truss rig assignment, compute per-fixture pan/tilt from geometry.
                if (m_stageTargetId != UINT_MAX && mProps->hasFixtureRigProps(id))
                {
                    StageTarget *tgt = mProps->stageTarget(m_stageTargetId);
                    FixtureRigProps rp = mProps->fixtureRigProps(id);

                    if (tgt != nullptr && rp.trussId != Truss::invalidId() &&
                        fixture->fixtureMode() != nullptr)
                    {
                        // Fixture world position in metres (derived from truss geometry)
                        QVector3D fixPos = mProps->fixtureRigPosition(id);
                        // Target Z is stored as absolute floor height; add platform height
                        // at the target's XY so a target on a raised platform is correct.
                        QVector3D tgtPos = tgt->position();
                        tgtPos.setZ(tgtPos.z() + mProps->platformHeightAt(tgtPos.x(), tgtPos.y()));

                        float dx = tgtPos.x() - fixPos.x();
                        float dy = tgtPos.y() - fixPos.y();
                        float dz = tgtPos.z() - fixPos.z();
                        float horizDist = qSqrt(dx * dx + dy * dy);

                        // Horizontal azimuth: clockwise degrees from downstage.
                        // Convention: Y+ = upstage, so downstage direction = decreasing Y.
                        // atan2(dx, -dy): when target is downstage (dy<0), -dy>0 → 0°; SR→90°; SL→270°.
                        float azimuthDeg = float(qRadiansToDegrees(
                            qAtan2(double(dx), double(-dy))));
                        if (azimuthDeg < 0.0f) azimuthDeg += 360.0f;

                        // Pan relative to fixture's panZeroDir, centred at panMax/2
                        float relativePan = azimuthDeg - rp.panZeroDir;
                        while (relativePan >  180.0f) relativePan -= 360.0f;
                        while (relativePan < -180.0f) relativePan += 360.0f;

                        QLCPhysical phy = fixture->fixtureMode()->physical();
                        float panMax  = float(phy.focusPanMax());  if (panMax  == 0.0f) panMax  = 360.0f;
                        float tiltMax = float(phy.focusTiltMax()); if (tiltMax == 0.0f) tiltMax = 270.0f;

                        float panRaw = panMax / 2.0f + relativePan + rp.panOffsetDeg;
                        if (rp.panInvert) panRaw = panMax - panRaw;
                        float panDeg = qBound(0.0f, panRaw, panMax);

                        // Elevation angle from horizontal (negative = beam points downward)
                        float elevDeg = float(qRadiansToDegrees(
                            qAtan2(double(dz), double(horizDist))));

                        // For TopHung: tilt centre = straight down (elevation = -90°).
                        // Deviation from straight-down = 90° + elevation
                        // (0 when directly below; 90° when horizontal)
                        float tiltOffset = 90.0f + elevDeg;
                        float tiltDeg;
                        if (rp.mountingType == Truss::FloorMounted)
                            tiltDeg = tiltMax / 2.0f - tiltOffset + rp.tiltOffsetDeg;
                        else   // TopHung / SideArm
                            tiltDeg = tiltMax / 2.0f + tiltOffset + rp.tiltOffsetDeg;
                        if (rp.tiltInvert) tiltDeg = tiltMax - tiltDeg;
                        tiltDeg = qBound(0.0f, tiltDeg, tiltMax);

                        list << fixture->positionToValues(QLCChannel::Pan, panDeg);
                        list << fixture->positionToValues(QLCChannel::Tilt, tiltDeg);

                        // Set pan/tilt speed to maximum (DMX 0 for FastSlow, 255 for SlowFast)
                        // so the fixture responds at full speed when following a target.
                        if (fixture->fixtureMode())
                        {
                            for (int ci = 0; ci < fixture->fixtureMode()->channels().count(); ci++)
                            {
                                const QLCChannel *ch = fixture->fixtureMode()->channel(ci);
                                if (!ch || ch->group() != QLCChannel::Speed) continue;
                                uchar fastVal = 0;
                                if (ch->preset() == QLCChannel::SpeedPanTiltSlowFast ||
                                    ch->preset() == QLCChannel::SpeedPanSlowFast ||
                                    ch->preset() == QLCChannel::SpeedTiltSlowFast)
                                    fastVal = 255;
                                list << SceneValue(fixture->id(), ci, fastVal);
                            }
                        }

                        usedTarget = true;
                    }
                }

                if (!usedTarget && m_values.count() == 2)
                {
                    int panDegrees = m_values.at(0).toInt();
                    int tiltDegrees = m_values.at(1).toInt();

                    if (fType != Flat)
                    {
                        int offset = int(qreal(intFanValue) * factor);
                        if (fLayout == XCentered || fLayout == YCentered || fLayout == ZCentered)
                            offset *= centeredSign;
                        panDegrees = int((qreal(panDegrees) + qreal(offset)));
                        tiltDegrees = int((qreal(tiltDegrees) + qreal(offset)));
                    }

                    list << fixture->positionToValues(QLCChannel::Pan, panDegrees);
                    list << fixture->positionToValues(QLCChannel::Tilt, tiltDegrees);
                }
            }
            break;
            case Shutter:
            {
                // channelNumber(Shutter, MSB) via the head map never resolves Shutter
                // because heads store shutter in m_shutterChannels, not m_channelsMap.
                // Query the mode directly instead.
                quint32 shCh = (fixture->fixtureMode() != nullptr)
                    ? fixture->fixtureMode()->channelNumber(QLCChannel::Shutter, QLCChannel::MSB)
                    : QLCChannel::invalid();
                if (shCh != QLCChannel::invalid())
                {
                    const QLCChannel *ch = fixture->channel(shCh);
                    uchar dmxVal = 0;
                    if (ch != nullptr &&
                        (ch->preset() == QLCChannel::ShutterStrobeSlowFast ||
                         ch->preset() == QLCChannel::ShutterStrobeFastSlow))
                    {
                        // Pure preset strobe channel: DMX 0 = open, 1-255 = strobe.
                        // Always send 0 to open regardless of the palette's stored value.
                        dmxVal = 0;
                    }
                    else
                    {
                        // Custom (capability-defined) shutter channel: use the palette's
                        // explicit value, or auto-detect the ShutterOpen capability midpoint
                        // when no value is set (e.g. after loading an old workspace).
                        dmxVal = m_values.isEmpty() ? 0 : uchar(value().toUInt());
                        if (dmxVal == 0 && ch != nullptr)
                        {
                            for (const QLCCapability *cap : ch->capabilities())
                            {
                                if (cap->preset() == QLCCapability::ShutterOpen)
                                {
                                    dmxVal = uchar((int(cap->min()) + int(cap->max())) / 2);
                                    break;
                                }
                            }
                        }
                        if (dmxVal == 0) dmxVal = 255; // last resort
                    }
                    list << SceneValue(id, shCh, dmxVal);
                }
            }
            break;
            case Gobo:
            {
                quint32 goboCh = fixture->channelNumber(QLCChannel::Gobo, QLCChannel::MSB);
                if (goboCh != QLCChannel::invalid())
                    list << SceneValue(id, goboCh, uchar(value().toUInt()));
            }
            break;
            case Zoom:
            {
                for (int i = 0; i < int(fixture->channels()); i++)
                {
                    const QLCChannel *ch = fixture->channel(quint32(i));
                    if (ch == nullptr)
                        continue;

                    if (ch->group() == QLCChannel::Beam &&
                        (ch->preset() == QLCChannel::BeamZoomSmallBig || ch->preset() == QLCChannel::BeamZoomBigSmall))
                    {
                        list << fixture->zoomToValues(value().toFloat(), false);
                        break;
                    }
                }
            }
            break;
            case Beam:
            {
                // Values: [0]=Focus 0-255, [1]=Frost 0-255, [2]=Iris 0-255
                uchar focusVal = (m_values.count() > 0) ? uchar(m_values.at(0).toInt()) : 0;
                uchar frostVal = (m_values.count() > 1) ? uchar(m_values.at(1).toInt()) : 0;
                uchar irisVal  = (m_values.count() > 2) ? uchar(m_values.at(2).toInt()) : 0;

                for (quint32 i = 0; i < fixture->channels(); i++)
                {
                    const QLCChannel *ch = fixture->channel(i);
                    if (ch == nullptr || ch->group() != QLCChannel::Beam)
                        continue;

                    if (ch->preset() == QLCChannel::BeamFocusNearFar)
                        list << SceneValue(id, i, focusVal);
                    else if (ch->preset() == QLCChannel::BeamFocusFarNear)
                        list << SceneValue(id, i, 255 - focusVal);
                    else if (ch->preset() == QLCChannel::Custom &&
                             ch->name().contains("frost", Qt::CaseInsensitive))
                        list << SceneValue(id, i, frostVal);
                }
                // Iris lives in the Shutter group
                for (quint32 i = 0; i < fixture->channels(); i++)
                {
                    const QLCChannel *ch = fixture->channel(i);
                    if (ch == nullptr || ch->group() != QLCChannel::Shutter)
                        continue;
                    if (ch->preset() == QLCChannel::ShutterIrisMinToMax)
                        list << SceneValue(id, i, irisVal);
                    else if (ch->preset() == QLCChannel::ShutterIrisMaxToMin)
                        list << SceneValue(id, i, 255 - irisVal);
                }
            }
            break;
            case Effect:
            case Undefined:
            break;
        }

        progress += (1.0 / qreal(fxCount - 1));
    }

    return list;
}

QList<SceneValue> QLCPalette::valuesFromFixtureGroups(Doc *doc, QList<quint32> groups)
{
    QList<quint32> fixturesList;

    foreach (quint32 id, groups)
    {
        FixtureGroup *group = doc->fixtureGroup(id);
        if (group == NULL)
            continue;

        fixturesList.append(group->fixtureList());
    }

    return valuesFromFixtures(doc, fixturesList);
}

qreal QLCPalette::valueFactor(qreal progress)
{
    qreal factor = 1.0;
    qreal normalizedAmount = qreal(m_fanningAmount) / 100.0;

    switch (m_fanningType)
    {
        case Flat:
            // nothing to do. Factor is always 1.0
        break;
        case Linear:
        {
            if (normalizedAmount < 1.0)
            {
                if (progress > normalizedAmount)
                    factor = 1.0;
                else
                    factor = progress * normalizedAmount;
            }
            else if (normalizedAmount > 1.0)
            {
                factor = progress / normalizedAmount;
            }
            else
            {
                factor = progress;
            }
        }
        break;
        case Sine:
        {
            qreal degrees = (progress * 360.0) + 270.0;
            factor = (qSin(normalizedAmount * qDegreesToRadians(degrees)) + 1.0) / 2.0;
        }
        break;
        case Square:
        {
            factor = qSin(normalizedAmount * qDegreesToRadians(progress * 360.0)) < 0 ? 1 : 0;
        }
        break;
        case Saw:
        break;
    }

    return factor;
}

/************************************************************************
 * Fanning
 ************************************************************************/

QLCPalette::FanningType QLCPalette::fanningType() const
{
    return m_fanningType;
}

void QLCPalette::setFanningType(QLCPalette::FanningType type)
{
    if (type == m_fanningType)
        return;

    m_fanningType = type;

    emit fanningTypeChanged();
}

QString QLCPalette::fanningTypeToString(QLCPalette::FanningType type)
{
    switch (type)
    {
        case Flat:      return "Flat";
        case Linear:    return "Linear";
        case Sine:      return "Sine";
        case Square:    return "Square";
        case Saw:       return "Saw";
    }

    return "";
}

QLCPalette::FanningType QLCPalette::stringToFanningType(const QString &str)
{
    if (str == "Flat")
        return Flat;
    else if (str == "Linear")
        return Linear;
    else if (str == "Sine")
        return Sine;
    else if (str == "Square")
        return Square;
    else if (str == "Saw")
        return Saw;

    return Flat;
}

QLCPalette::FanningLayout QLCPalette::fanningLayout() const
{
    return m_fanningLayout;
}

void QLCPalette::setFanningLayout(QLCPalette::FanningLayout layout)
{
    if (layout == m_fanningLayout)
        return;

    m_fanningLayout = layout;

    emit fanningLayoutChanged();
}

QString QLCPalette::fanningLayoutToString(QLCPalette::FanningLayout layout)
{
    switch (layout)
    {
        case XAscending:    return "XAscending";
        case XDescending:   return "XDescending";
        case XCentered:     return "XCentered";
        case YAscending:    return "YAscending";
        case YDescending:   return "YDescending";
        case YCentered:     return "YCentered";
        case ZAscending:    return "ZAscending";
        case ZDescending:   return "ZDescending";
        case ZCentered:     return "ZCentered";
    }

    return "";
}

QLCPalette::FanningLayout QLCPalette::stringToFanningLayout(const QString &str)
{
    if (str == "XAscending")
        return XAscending;
    else if (str == "XDescending")
        return XDescending;
    else if (str == "XCentered")
        return XCentered;
    else     if (str == "YAscending")
        return YAscending;
    else if (str == "YDescending")
        return YDescending;
    else if (str == "YCentered")
        return YCentered;
    else if (str == "ZAscending")
        return ZAscending;
    else if (str == "ZDescending")
        return ZDescending;
    else if (str == "ZCentered")
        return ZCentered;

    return XAscending;
}

int QLCPalette::fanningAmount() const
{
    return m_fanningAmount;
}

void QLCPalette::setFanningAmount(int amount)
{
    if (amount == m_fanningAmount)
        return;

    m_fanningAmount = amount;

    emit fanningAmountChanged();
}

QVariant QLCPalette::fanningValue() const
{
    return m_fanningValue;
}

void QLCPalette::setFanningValue(QVariant value)
{
    if (value == m_fanningValue)
        return;

    m_fanningValue = value;

    emit fanningValueChanged();
}

/************************************************************************
 * Color helpers
 ************************************************************************/

QString QLCPalette::colorToString(QColor rgb, QColor wauv)
{
    QString final = rgb.name();
    final.append(wauv.name().right(6));
    return final;
}

bool QLCPalette::stringToColor(QString str, QColor &rgb, QColor &wauv)
{
    // string must be like #rrggbb or #rrggbbwwaauv
    if (str.length() != 7 && str.length() != 13)
        return false;

    rgb = QColor(str.left(7));

    if (str.length() == 13)
        wauv = QColor("#" + str.right(6));
    else
        wauv = QColor();

    return true;
}

/************************************************************************
 * Load & Save
 ************************************************************************/

bool QLCPalette::loader(QXmlStreamReader &xmlDoc, Doc *doc)
{
    QLCPalette *palette = new QLCPalette(Dimmer, doc);
    Q_ASSERT(palette != NULL);

    if (palette->loadXML(xmlDoc) == true)
    {
        doc->addPalette(palette, palette->id());
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "QLCPalette" << palette->name() << "cannot be loaded.";
        delete palette;
        return false;
    }

    return true;
}

bool QLCPalette::loadXML(QXmlStreamReader &doc)
{
    if (doc.name() != KXMLQLCPalette)
    {
        qWarning() << Q_FUNC_INFO << "Palette node not found";
        return false;
    }

    QXmlStreamAttributes attrs = doc.attributes();

    bool ok = false;
    quint32 id = attrs.value(KXMLQLCPaletteID).toString().toUInt(&ok);
    if (ok == false)
    {
        qWarning() << "Invalid Palette ID:" << attrs.value(KXMLQLCPaletteID).toString();
        return false;
    }

    setID(id);

    if (attrs.hasAttribute(KXMLQLCPaletteType) == false)
    {
        qWarning() << "Palette type not found!";
        return false;
    }

    m_type = stringToType(attrs.value(KXMLQLCPaletteType).toString());

    if (attrs.hasAttribute(KXMLQLCPaletteName))
        setName(attrs.value(KXMLQLCPaletteName).toString());

    if (attrs.hasAttribute(KXMLQLCPalettePath))
        setPath(attrs.value(KXMLQLCPalettePath).toString());

    if (attrs.hasAttribute(KXMLQLCPaletteValue))
    {
        QString strVal = attrs.value(KXMLQLCPaletteValue).toString();
        switch (m_type)
        {
            case Dimmer:
            case Pan:
            case Tilt:
                setValue(strVal.toInt());
            break;
            case Color:
                setValue(strVal);
            break;
            case PanTilt:
            {
                QStringList posList = strVal.split(",");
                if (posList.count() == 2)
                    setValue(posList.at(0).toInt(), posList.at(1).toInt());
            }
            break;
            case Zoom:
                setValue(strVal.toFloat());
            break;
            case Beam:
            {
                QStringList beamList = strVal.split(",");
                QVariantList vals;
                vals << (beamList.count() > 0 ? beamList.at(0).toInt() : 0);
                vals << (beamList.count() > 1 ? beamList.at(1).toInt() : 0);
                vals << (beamList.count() > 2 ? beamList.at(2).toInt() : 0);
                setValues(vals);
            }
            break;
            case Shutter:
            case Gobo:
                setValue(strVal.toInt());
            break;
            case Effect:
            case Undefined: break;
        }
    }

    if (attrs.hasAttribute(KXMLQLCPaletteStageTarget))
        m_stageTargetId = attrs.value(KXMLQLCPaletteStageTarget).toUInt();

    if (attrs.hasAttribute(KXMLQLCPaletteFanning))
    {
        setFanningType(stringToFanningType(attrs.value(KXMLQLCPaletteFanning).toString()));

        if (attrs.hasAttribute(KXMLQLCPaletteFanLayout))
            setFanningLayout(stringToFanningLayout(attrs.value(KXMLQLCPaletteFanLayout).toString()));

        if (attrs.hasAttribute(KXMLQLCPaletteFanAmount))
            setFanningAmount(attrs.value(KXMLQLCPaletteFanAmount).toInt());

        if (attrs.hasAttribute(KXMLQLCPaletteFanValue))
        {
            QString strVal = attrs.value(KXMLQLCPaletteFanValue).toString();
            switch (m_type)
            {
                case Dimmer:
                case Pan:
                case Tilt:
                case PanTilt:
                case Zoom:
                    setFanningValue(strVal.toInt());
                break;
                case Color:
                    setFanningValue(strVal);
                break;
                case Beam:      break;
                case Shutter:   break;
                case Gobo:      break;
                case Effect:    break;
                case Undefined: break;
            }
        }
    }

    // Effect-palette sub-elements
    if (m_type == Effect)
    {
        while (doc.readNextStartElement())
        {
            if (doc.name() == "EffectScript")
                m_scriptPath = doc.readElementText();
            else if (doc.name() == "EffectInputBinding")
            {
                QString slot   = doc.attributes().value("slot").toString();
                quint32 uni    = doc.attributes().value("universe").toUInt();
                quint32 ch     = doc.attributes().value("channel").toUInt();
                if (!slot.isEmpty())
                    m_effectInputBindings[slot] = qMakePair(uni, ch);
                doc.skipCurrentElement();
            }
            else if (doc.name() == "EffectPaletteBinding")
            {
                QString slot = doc.attributes().value("slot").toString();
                quint32 pid  = doc.attributes().value("palette").toUInt();
                if (!slot.isEmpty())
                    m_effectPaletteBindings[slot] = pid;
                doc.skipCurrentElement();
            }
            else if (doc.name() == "EffectParam")
            {
                QString name = doc.attributes().value("name").toString();
                double  val  = doc.attributes().value("value").toDouble();
                if (!name.isEmpty())
                    m_effectParamValues[name] = val;
                doc.skipCurrentElement();
            }
            else
                doc.skipCurrentElement();
        }
    }

    return true;
}

bool QLCPalette::saveXML(QXmlStreamWriter *doc)
{
    Q_ASSERT(doc != NULL);

    // Shutter, Gobo, and Effect palettes are valid without m_values.
    if (m_values.isEmpty() && m_type != Shutter && m_type != Gobo
        && m_type != Effect)
    {
        qWarning() << "Unable to save a Palette without value!";
        return false;
    }

    /* write a Palette entry */
    doc->writeStartElement(KXMLQLCPalette);
    doc->writeAttribute(KXMLQLCPaletteID, QString::number(this->id()));
    doc->writeAttribute(KXMLQLCPaletteType, typeToString(m_type));
    doc->writeAttribute(KXMLQLCPaletteName, this->name());
    if (m_path.isEmpty() == false)
        doc->writeAttribute(KXMLQLCPalettePath, m_path);

    /* write value */
    switch (m_type)
    {
        case Dimmer:
        case Pan:
        case Tilt:
        case Zoom:
        case Color:
            doc->writeAttribute(KXMLQLCPaletteValue, value().toString());
        break;
        case PanTilt:
            doc->writeAttribute(KXMLQLCPaletteValue,
                                QString("%1,%2").arg(m_values.at(0).toInt()).arg(m_values.at(1).toInt()));
        break;
        case Beam:
        {
            int focus = (m_values.count() > 0) ? m_values.at(0).toInt() : 0;
            int frost  = (m_values.count() > 1) ? m_values.at(1).toInt() : 0;
            int iris   = (m_values.count() > 2) ? m_values.at(2).toInt() : 0;
            doc->writeAttribute(KXMLQLCPaletteValue,
                                QString("%1,%2,%3").arg(focus).arg(frost).arg(iris));
        }
        break;
        case Shutter:
        case Gobo:
            if (!m_values.isEmpty())
                doc->writeAttribute(KXMLQLCPaletteValue, value().toString());
        break;
        case Effect:
            // Script path and bindings saved as child elements below
        break;
        case Undefined: break;
    }

    /* write stage target linkage */
    if (m_stageTargetId != UINT_MAX)
        doc->writeAttribute(KXMLQLCPaletteStageTarget, QString::number(m_stageTargetId));

    /* write fanning */
    if (m_fanningType != Flat)
    {
        doc->writeAttribute(KXMLQLCPaletteFanning, fanningTypeToString(m_fanningType));
        doc->writeAttribute(KXMLQLCPaletteFanLayout, fanningLayoutToString(m_fanningLayout));
        doc->writeAttribute(KXMLQLCPaletteFanAmount, QString::number(m_fanningAmount));
        doc->writeAttribute(KXMLQLCPaletteFanValue, fanningValue().toString());
    }

    /* write Effect-palette sub-elements */
    if (m_type == Effect)
    {
        if (!m_scriptPath.isEmpty())
        {
            doc->writeStartElement("EffectScript");
            doc->writeCharacters(m_scriptPath);
            doc->writeEndElement();
        }
        for (auto it = m_effectInputBindings.constBegin();
             it != m_effectInputBindings.constEnd(); ++it)
        {
            doc->writeStartElement("EffectInputBinding");
            doc->writeAttribute("slot",    it.key());
            doc->writeAttribute("universe", QString::number(it.value().first));
            doc->writeAttribute("channel",  QString::number(it.value().second));
            doc->writeEndElement();
        }
        for (auto it = m_effectPaletteBindings.constBegin();
             it != m_effectPaletteBindings.constEnd(); ++it)
        {
            doc->writeStartElement("EffectPaletteBinding");
            doc->writeAttribute("slot",    it.key());
            doc->writeAttribute("palette", QString::number(it.value()));
            doc->writeEndElement();
        }
        for (auto it = m_effectParamValues.constBegin();
             it != m_effectParamValues.constEnd(); ++it)
        {
            doc->writeStartElement("EffectParam");
            doc->writeAttribute("name",  it.key());
            doc->writeAttribute("value", QString::number(it.value()));
            doc->writeEndElement();
        }
    }

    doc->writeEndElement();

    return true;
}

QColor QLCPalette::colorValue() const
{
    if (m_type == Color && !m_values.isEmpty())
    {
        QColor rgb, wauv;
        if (stringToColor(m_values.at(0).toString(), rgb, wauv))
            return rgb;
    }
    return QColor();
}
