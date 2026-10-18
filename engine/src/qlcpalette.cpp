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
#include <QFileInfo>
#include <QSet>
#include <QDir>
#include <QtMath>
#include <QDebug>

#include "monitorproperties.h"
#include "effectscriptrunner.h"
#include "aimsolver.h"
#include <QMutex>
#include <QFile>
#include <QHash>
#include "qlcpalette.h"
#include "fixture.h"
#include "grouphead.h"
#include "effectscriptcache.h"
#include "qlcchannel.h"
#include "qlccapability.h"
#include "qlcfixturehead.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "scenevalue.h"
#include "scene.h"
#include "doc.h"

#define KXMLQLCPaletteType      QStringLiteral("Type")
#define KXMLQLCPaletteName      QStringLiteral("Name")
#define KXMLQLCPalettePath      QStringLiteral("Path")
/* Present (="1") on palettes whose Path is type-independent ("Palettes/01-Song/").
   Absent: an older show, whose paths start with the palette type -- migrated
   on load. Older versions ignore it and show the folders at the top level. */
#define KXMLQLCPaletteFolders   QStringLiteral("Folders")
#define KXMLQLCPalettePersistent QStringLiteral("Persistent")
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
    copy->setStageTargetId(this->stageTargetId());
    copy->setGoboName(this->goboName());
    copy->setGoboValues(this->goboValues());
    copy->setColourMacro(this->colourMacro());
    copy->setScriptPath(this->scriptPath());
    copy->setPersistent(this->persistent());
    copy->setEffectPreset(this->effectPreset());
    copy->setWledEffect(this->wledEffect());
    copy->setEffectInputBindings(this->effectInputBindings());
    for (auto it = m_effectPaletteBindings.constBegin();
         it != m_effectPaletteBindings.constEnd(); ++it)
        copy->setEffectPaletteBinding(it.key(), it.value());
    for (auto it = m_effectTargetBindings.constBegin();
         it != m_effectTargetBindings.constEnd(); ++it)
        copy->setEffectTargetBinding(it.key(), it.value());
    for (auto it = m_effectParamValues.constBegin();
         it != m_effectParamValues.constEnd(); ++it)
        copy->setEffectParamValue(it.key(), it.value());
    for (auto it = m_effectStringParams.constBegin();
         it != m_effectStringParams.constEnd(); ++it)
        copy->setEffectStringParam(it.key(), it.value());

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
        case Aim:       return "Aim";
        case Shutter:   return "Shutter";
        case Strobe:    return "Strobe";
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
    else if (str == "Aim")
        return Aim;
    else if (str == "Shutter")
        return Shutter;
    else if (str == "Strobe")
        return Strobe;
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
        case Aim: return QString("%1:/target.%2").arg(prefix).arg(ext);
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

bool QLCPalette::isTypeFolderName(const QString &name)
{
    static const PaletteType kTypes[] = { Dimmer, Color, Pan, Tilt, PanTilt, Aim,
                                          Shutter, Strobe, Gobo, Zoom, Beam, Effect };
    for (PaletteType t : kTypes)
        if (name == typeToString(t))
            return true;
    return false;
}

QString QLCPalette::migrateLegacyPath(const QString &path, PaletteType type, bool anyTypeFolder)
{
    QString norm = path;
    while (norm.endsWith('/'))
        norm.chop(1);
    if (norm.isEmpty() || norm == QLatin1String("Palettes"))
        return QString();

    QStringList segs = norm.split('/');
    if (segs.first() != QLatin1String("Palettes") || segs.size() < 2)
        return path;

    /* Only what is genuinely the old layout is taken apart. Older versions
       showed palettes under a heading per TYPE, and stored that heading as
       the path's first folder: each palette under its own type
       ("Palettes/Color/01-Song/" for a colour) -- or, dragged there, under
       another type's ("Palettes/Color/01-Song/" for an effect). So:
         - its own type's name first: the old layout, always dropped;
         - another type's name first: dropped only when the whole show is
           in the old layout (@a anyTypeFolder, see migrateLegacyPaths());
           otherwise it is a folder the user made and named so -- kept;
         - anything else ("Colour", "Warm", "01-Song"): the user's, kept. */
    if (type == Undefined || !isTypeFolderName(segs.at(1))
        || (segs.at(1) != typeToString(type) && !anyTypeFolder))
        return path;

    // An effect auto-filed under Palettes/Effect/<Category>/<Engine>: that
    // was bookkeeping, not the user's folder.
    static const char *kEffectCategories[] = { "Color", "Dimmer", "Position", "Beam",
                                               "Other", "WLED" };
    if (type == Effect && segs.at(1) == QLatin1String("Effect") && segs.size() == 4)
    {
        for (const char *c : kEffectCategories)
            if (segs.at(2) == QLatin1String(c))
                return QString();
    }

    segs.removeAt(1);
    return segs.size() < 2 ? QString() : segs.join('/') + '/';
}

void QLCPalette::migrateLegacyPaths(const QList<QLCPalette *> &palettes)
{
    /* The whole show in the old layout: every palette loaded without the
       Folders marker that has a folder has a palette TYPE's name as its
       first one (as the old per-type headings stored it). Then any type
       name first was a heading, and goes. One folder that isn't a type's
       name (a show saved by a build with shared folders but no marker yet)
       and only a palette's own type name goes. */
    bool oldLayout = true, any = false;
    for (const QLCPalette *p : palettes)
    {
        // A marked path: the show was saved with shared folders.
        if (p != nullptr && p->m_loadedMarked)
            oldLayout = false;
        if (p == nullptr || p->m_unmarkedPath.isNull())
            continue;
        QStringList segs = p->m_unmarkedPath.split('/', Qt::SkipEmptyParts);
        if (segs.size() < 2 || segs.first() != QLatin1String("Palettes"))
            continue;
        any = true;
        if (!isTypeFolderName(segs.at(1)))
            oldLayout = false;
    }
    for (QLCPalette *p : palettes)
    {
        if (p != nullptr)
            p->m_loadedMarked = false;
        if (p == nullptr || p->m_unmarkedPath.isNull())
            continue;
        if (any && oldLayout)
            p->setPath(migrateLegacyPath(p->m_unmarkedPath, p->type(), true));
        p->m_unmarkedPath = QString();
    }
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

/** True if @a scene carries a follow-spot effect palette, which is what makes
 *  an Aim look in that scene track a subject rather than a static point. */
static bool sceneHasFollowSpot(Doc *doc, const Scene *scene)
{
    return AimSolver::sceneFollowsSubject(doc, scene);
}

QList<SceneValue> QLCPalette::valuesFromFixtures(Doc *doc, QList<quint32> fixtures,
                                                 const Scene *owner, int colorSetOffset,
                                                 const QHash<quint32, QSet<int> > *onlyHeads)
{
    QList<SceneValue> list;
    // Whether head @p h of fixture @p fid is one this is for (see onlyHeads).
    auto headWanted = [onlyHeads](quint32 fid, int h) {
        if (onlyHeads == nullptr)
            return true;
        auto it = onlyHeads->constFind(fid);
        return it == onlyHeads->constEnd() || it->contains(h) || it->contains(-1);
    };

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

    // Aim at a line target: each head's point, worked out on first use.
    bool lineReady = false;
    QHash<quint32, QVector3D> linePts;

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
                {
                    const QLCChannel *ch = fixture->channel(masterIntensityChannel);
                    int ceiling = (ch != NULL) ? int(ch->dimmerCeiling()) : 255;
                    list << SceneValue(id, masterIntensityChannel, uchar(dValue * ceiling / 255));
                }

                for (int i = 0; i < fixture->heads(); i++)
                {
                    if (!headWanted(id, i))
                        continue;           // not one of the group's heads
                    quint32 headDimmerChannel = fixture->channelNumber(QLCChannel::Intensity, QLCChannel::MSB, i);
                    if (headDimmerChannel != QLCChannel::invalid())
                    {
                        const QLCChannel *ch = fixture->channel(headDimmerChannel);
                        int ceiling = (ch != NULL) ? int(ch->dimmerCeiling()) : 255;
                        list << SceneValue(id, headDimmerChannel, uchar(dValue * ceiling / 255));
                    }
                }
            }
            break;
            case Color:
            {

                QColor startColor = rgbValue();
                QColor col = startColor;
                QColor wauv = wauvValue();

                // Ordered look colours: values()[0] is the primary, [1], [2]… are
                // secondary/tertiary, applied in order to a fixture's extra RGB
                // sets (WLED-style). Fewer colours than sets → the extra sets are
                // left untouched.
                QVector<QColor> lookColours;
                for (const QVariant &cv : values())
                {
                    QColor rgb, w;
                    stringToColor(cv.toString(), rgb, w);
                    lookColours << rgb;
                }
                if (lookColours.isEmpty())
                    lookColours << startColor;

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

                bool anyHead = false;
                for (int i = 0; i < fixture->heads(); i++)
                {
                    /* Only the group's own heads: a group of a bar's RGB pixels
                       must not light its separate white pixels too ("lighting
                       the white leds as well as the RGB"). */
                    if (!headWanted(id, i))
                        continue;
                    anyHead = true;
                    // White/Amber/UV emitter values: explicit wauv, else auto
                    // White = min(R,G,B) so RGBW fixtures light their white LED.
                    QColor emitters = wauv;
                    if (emitters.isValid() == false)
                        emitters = QColor(qMin(col.red(), qMin(col.green(), col.blue())), 0, 0);

                    quint32 whiteCh = fixture->channelNumber(QLCChannel::White, QLCChannel::MSB, i);
                    // Proper RGBW: extract the white the fixture will emit back
                    // OUT of the RGB primaries, so the rendered/emitted colour
                    // matches the one that was picked instead of washing toward
                    // white (R+W, G+W, B+W). Only when there's a white channel.
                    int wSub = (whiteCh != QLCChannel::invalid()) ? int(uchar(emitters.red())) : 0;

                    // Apply the look's colours in order to this head's RGB sets.
                    // Set 0 (primary) uses the fanned colour + white extraction;
                    // additional sets use the secondary/tertiary look colours
                    // literally. Sets past the supplied colours are left untouched.
                    // colour[k] -> set[colorSetOffset + k]. Set 0 (traditional
                    // primary) uses the fanned colour + white extraction; other
                    // sets take the look colours literally. Sets past the fixture's
                    // count are skipped, so stacking more colour looks than a
                    // fixture has sets simply ignores the extras on that fixture.
                    const QVector<QVector<quint32> > rgbSets = fixture->rgbChannelSets(i);
                    for (int k = 0; k < lookColours.size(); k++)
                    {
                        const int setIdx = colorSetOffset + k;
                        if (setIdx >= rgbSets.size())
                            break;
                        const QColor c = (setIdx == 0) ? col : lookColours.at(k);
                        const int wsub = (setIdx == 0) ? wSub : 0;
                        const QVector<quint32> &set = rgbSets.at(setIdx);
                        list << SceneValue(id, set.at(0), uchar(qMax(0, c.red()   - wsub)));
                        list << SceneValue(id, set.at(1), uchar(qMax(0, c.green() - wsub)));
                        list << SceneValue(id, set.at(2), uchar(qMax(0, c.blue()  - wsub)));
                    }
                    /* CMY, a colour wheel and the white/amber/UV emitters are
                       the fixture's ONE colour: colour 1's. A second Colour look
                       (colour 2, for a second RGB set) left them alone -- it
                       used to move the wheel to its own slot. */
                    const bool primary = (colorSetOffset == 0);
                    QVector<quint32> cmyCh = fixture->cmyChannels(i);
                    if (primary && cmyCh.size() == 3)
                    {
                        list << SceneValue(id, cmyCh.at(0), uchar(col.cyan()));
                        list << SceneValue(id, cmyCh.at(1), uchar(col.magenta()));
                        list << SceneValue(id, cmyCh.at(2), uchar(col.yellow()));
                    }

                    // Colour wheels and colour macros (a mover's ring): the
                    // slot nearest the colour, or the macro the look names.
                    // Only on a head that mixes no colour (RGB/CMY above
                    // already made it exactly).
                    if (primary)
                    {
                        const QLCFixtureMode *mode = fixture->fixtureMode();
                        for (quint32 wheelCh : colourSlotChannels(fixture, i))
                        {
                            const int v = colourSlotValue(mode != nullptr ? mode->channel(wheelCh) : nullptr, col);
                            if (v >= 0)
                                list << SceneValue(id, wheelCh, uchar(v));
                        }
                    }

                    // Extra emitter channels. White carries the value extracted
                    // from RGB above (so R'+W reconstructs the picked colour);
                    // Amber/UV are additive and default to 0.
                    if (primary && whiteCh != QLCChannel::invalid())
                        list << SceneValue(id, whiteCh, uchar(emitters.red()));
                    if (primary)
                    {
                        quint32 channel = fixture->channelNumber(QLCChannel::Amber, QLCChannel::MSB, i);
                        if (channel != QLCChannel::invalid())
                            list << SceneValue(id, channel, uchar(emitters.green()));
                        channel = fixture->channelNumber(QLCChannel::UV, QLCChannel::MSB, i);
                        if (channel != QLCChannel::invalid())
                            list << SceneValue(id, channel, uchar(emitters.blue()));
                    }
                }
                /* A wheel or ring in no head is the whole fixture's: a look on
                   any of its heads sets it, as a master dimmer is (the old
                   one-head definitions of a mover with a ring left the ring
                   out of the head in one mode and in it in the other). */
                if (anyHead && colorSetOffset == 0)
                {
                    const QLCFixtureMode *mode = fixture->fixtureMode();
                    for (quint32 wheelCh : colourSlotChannels(fixture, -1))
                    {
                        const int v = colourSlotValue(mode != nullptr ? mode->channel(wheelCh) : nullptr, col);
                        if (v >= 0)
                            list << SceneValue(id, wheelCh, uchar(v));
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
                // Raw pan/tilt degrees stored in m_values[0] (pan) and m_values[1] (tilt).
                if (m_values.count() == 2)
                {
                    int panDegrees  = m_values.at(0).toInt();
                    int tiltDegrees = m_values.at(1).toInt();

                    if (fType != Flat)
                    {
                        int offset = int(qreal(intFanValue) * factor);
                        if (fLayout == XCentered || fLayout == YCentered || fLayout == ZCentered)
                            offset *= centeredSign;
                        panDegrees  = int((qreal(panDegrees)  + qreal(offset)));
                        tiltDegrees = int((qreal(tiltDegrees) + qreal(offset)));
                    }

                    list << fixture->positionToValues(QLCChannel::Pan,  panDegrees);
                    list << fixture->positionToValues(QLCChannel::Tilt, tiltDegrees);
                }
            }
            break;
            case Aim:
            {
                // Per-fixture pan/tilt computed from rig geometry toward a StageTarget.
                // Gate on whether the fixture is PLACED in the plot (has a position),
                // NOT on having a rig-props entry: a floor fixture is in the position
                // map but only gets a rig-props entry once it's bound to a truss or
                // edited in properties — so hasFixtureRigProps() wrongly excluded
                // floor fixtures. fixtureRigProps() returns sensible free-placed
                // defaults when no explicit entry exists.
                if (m_stageTargetId == UINT_MAX || !mProps->containsFixture(id))
                    break;

                StageTarget *tgt = mProps->stageTarget(m_stageTargetId);
                if (tgt == nullptr || fixture->fixtureMode() == nullptr)
                    break;

                // Is this target driven by a follow spot? The followspot reads its
                // target IMPLICITLY from the scene's own Aim look, so subject mode
                // is now keyed off the SCENE: if this Aim look lives in a scene that
                // also carries a followspot effect, the beam tracks a SUBJECT (a
                // person) — aim at (platform + subject height), i.e. the body,
                // overriding the target's own Z. Otherwise it is a static aim point
                // and its Z is an ABSOLUTE height above the floor, used as-is.
                // Answer it from the OWNING scene when the caller told us which
                // one it is — that is exact, O(palettes), and touches nothing
                // shared. The doc->functions() scan below is the fallback for
                // GUI callers with no scene context: it copies Doc's whole
                // function map and dereferences every Function, which races
                // addFunction/deleteFunction if it ever runs on the DMX thread.
                bool subjectMode = false;
                if (owner != nullptr)
                {
                    subjectMode = sceneHasFollowSpot(doc, owner);
                }
                else
                {
                    foreach (Function *fn, doc->functions())
                    {
                        const Scene *sc = qobject_cast<const Scene*>(fn);
                        if (sc == NULL || !sc->palettes().contains(m_id))
                            continue;
                        if (sceneHasFollowSpot(doc, sc))
                        {
                            subjectMode = true;
                            break;
                        }
                    }
                }

                /* One rule, in MonitorProperties, shared with the rig view.
                   This used to be written out here AND again in the renderer,
                   and the two disagreed: a follow-spot aimed at chest height in
                   Edit while the rig view pointed at the target's own Z. */
                QVector3D tgtPos;
                if (tgt->isLine())
                {
                    /* A line gives each head its own point, so where THIS head
                       goes depends on every head the look aims: the owning
                       scene's (groups and fixtures together, which this
                       palette is expanded for in two calls), else the list
                       this call was given. Worked out once per call. */
                    if (!lineReady)
                    {
                        const QList<quint32> heads = (owner != nullptr)
                                ? AimSolver::aimHeads(doc, owner, m_id)
                                : AimSolver::aimable(doc, fixtures);
                        linePts = AimSolver::linePoints(doc, this, tgt, heads);
                        lineReady = true;
                    }
                    tgtPos = linePts.value(id, mProps->targetAimPoint(tgt));
                }
                else
                {
                    tgtPos = AimSolver::aimPointFor(doc, id, tgt, subjectMode);
                }

                // Shared aim geometry — identical to the follow-spot effect's, so
                // a fixture points the same way in Edit and Run.
                float panDeg = 0.0f, tiltDeg = 0.0f;
                if (!AimSolver::aimDegrees(doc, id, tgtPos, panDeg, tiltDeg))
                    break;

                list << fixture->positionToValues(QLCChannel::Pan,  panDeg);
                list << fixture->positionToValues(QLCChannel::Tilt, tiltDeg);

                // Set pan/tilt speed to maximum (DMX 0 for FastSlow, 255 for SlowFast)
                // so the fixture responds at full speed when following a target.
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
                        // Preset strobe channel: DMX 0 = beam open, 1-255 = strobe.
                        dmxVal = 0;
                    }
                    else
                    {
                        // Capability-defined channel: search for ShutterOpen first
                        // (fixture-aware), then fall back to stored value / 255.
                        if (ch != nullptr)
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
                        if (dmxVal == 0 && !m_values.isEmpty())
                            dmxVal = uchar(value().toUInt());
                        if (dmxVal == 0)
                            dmxVal = 255; // last resort
                    }
                    list << SceneValue(id, shCh, dmxVal);
                }
            }
            break;
            case Strobe:
            {
                quint32 shCh = (fixture->fixtureMode() != nullptr)
                    ? fixture->fixtureMode()->channelNumber(QLCChannel::Shutter, QLCChannel::MSB)
                    : QLCChannel::invalid();
                if (shCh == QLCChannel::invalid())
                {
                    // Fixtures like the Betopper LM70 pack strobe into the top
                    // of the intensity channel (dimmerCeiling() < 255 flags the
                    // effect band). Fall back to that channel so a strobe look
                    // still works; the capability interpolation below finds the
                    // Strobe* range on it.
                    quint32 iCh = fixture->masterIntensityChannel();
                    const QLCChannel *ich = (iCh != QLCChannel::invalid())
                        ? fixture->channel(iCh) : nullptr;
                    if (ich != nullptr && ich->dimmerCeiling() < 255)
                        shCh = iCh;
                    else
                    {
                        for (int i = 0; i < fixture->heads(); i++)
                        {
                            quint32 hCh = fixture->channelNumber(QLCChannel::Intensity, QLCChannel::MSB, i);
                            const QLCChannel *hch = (hCh != QLCChannel::invalid())
                                ? fixture->channel(hCh) : nullptr;
                            if (hch != nullptr && hch->dimmerCeiling() < 255)
                            {
                                shCh = hCh;
                                break;
                            }
                        }
                    }
                }
                if (shCh == QLCChannel::invalid())
                    break;
                const QLCChannel *ch = fixture->channel(shCh);
                if (ch == nullptr)
                    break;

                // rate is 0.0 (off/slowest) to 1.0 (fastest)
                float rate = m_values.isEmpty() ? 0.0f : value().toFloat();
                rate = qBound(0.0f, rate, 1.0f);

                uchar dmxVal = 0;
                bool found = false;

                if (ch->preset() == QLCChannel::ShutterStrobeSlowFast)
                {
                    // 0 = open; 1-255 = strobe slow → fast
                    dmxVal = (rate <= 0.0f) ? 1 : uchar(1 + qRound(rate * 254.0f));
                    found = true;
                }
                else if (ch->preset() == QLCChannel::ShutterStrobeFastSlow)
                {
                    // 0 = open; 1-255 = strobe fast → slow (rate inverted)
                    dmxVal = (rate <= 0.0f) ? 1 : uchar(1 + qRound((1.0f - rate) * 254.0f));
                    found = true;
                }
                else
                {
                    // Custom channel: find strobe capability range and interpolate.
                    for (const QLCCapability *cap : ch->capabilities())
                    {
                        int lo = int(cap->min()), hi = int(cap->max());
                        if (cap->preset() == QLCCapability::StrobeSlowToFast ||
                            cap->preset() == QLCCapability::StrobeFrequency   ||
                            cap->preset() == QLCCapability::StrobeFreqRange)
                        {
                            dmxVal = uchar(lo + qRound(rate * (hi - lo)));
                            found = true;
                            break;
                        }
                        else if (cap->preset() == QLCCapability::StrobeFastToSlow)
                        {
                            dmxVal = uchar(lo + qRound((1.0f - rate) * (hi - lo)));
                            found = true;
                            break;
                        }
                        else if (cap->preset() == QLCCapability::StrobeRandom          ||
                                 cap->preset() == QLCCapability::StrobeRandomSlowToFast ||
                                 cap->preset() == QLCCapability::StrobeRandomFastToSlow)
                        {
                            // Prefer an ordered strobe range; accept random as fallback.
                            if (!found)
                            {
                                dmxVal = uchar(lo + qRound(rate * (hi - lo)));
                                found = true;
                            }
                        }
                    }
                }

                if (found)
                    list << SceneValue(id, shCh, dmxVal);
            }
            break;
            case Gobo:
            {
                // Per FIXTURE, not per head: a head's channel map never lists
                // gobo channels, so the head lookup found nothing and a gobo
                // look wrote no value at all.
                const quint32 goboCh = fixture->fixtureMode() != NULL
                    ? fixture->fixtureMode()->channelNumber(QLCChannel::Gobo, QLCChannel::MSB)
                    : QLCChannel::invalid();
                // Named: the slot of that name on this fixture's own wheel
                // (or the one picked for its type); none, and it ignores it.
                const int gv = goboValueFor(fixture);
                if (goboCh != QLCChannel::invalid() && gv >= 0)
                    list << SceneValue(id, goboCh, uchar(gv));
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

int QLCPalette::colorSetOffset(Doc *doc, const QList<quint32> &orderedPalettes, quint32 paletteId)
{
    int offset = 0;
    for (quint32 pid : orderedPalettes)
    {
        if (pid == paletteId)
            break;
        QLCPalette *p = doc->palette(pid);
        if (p != NULL && p->type() == Color)
            offset += qMax(1, p->values().size()); // each colour look consumes >=1 set
    }
    return offset;
}

bool QLCPalette::fixtureHasDimmer(const Fixture *fixture)
{
    if (fixture == NULL)
        return false;
    if (fixture->masterIntensityChannel() != QLCChannel::invalid())
        return true;
    for (int h = 0; h < fixture->heads(); h++)
    {
        const quint32 ch = fixture->channelNumber(QLCChannel::Intensity, QLCChannel::MSB, h);
        const QLCChannel *c = ch != QLCChannel::invalid() ? fixture->channel(ch) : NULL;
        if (c != NULL && c->colour() == QLCChannel::NoColour)
            return true;
    }
    return false;
}

int QLCPalette::lookDimmerValue(Doc *doc, const QList<quint32> &orderedPalettes)
{
    int value = -1;
    for (quint32 pid : orderedPalettes)
    {
        QLCPalette *p = doc->palette(pid);
        if (p != NULL && p->type() == Dimmer && !isEffectScoped(doc, orderedPalettes, pid))
            value = qBound(0, p->value().toInt(), 255);     // the lowest wins
    }
    return value;
}

uchar QLCPalette::dimScaleWithoutDimmer(const Fixture *fixture, quint32 channel, uchar value, int lookDimmer)
{
    if (lookDimmer < 0 || fixture == NULL || fixtureHasDimmer(fixture))
        return value;
    const QLCChannel *c = fixture->channel(channel);
    if (c == NULL || c->group() != QLCChannel::Intensity || c->colour() == QLCChannel::NoColour)
        return value;                       // not a colour emitter
    return uchar(qRound(double(value) * lookDimmer / 255.0));
}

bool QLCPalette::effectUsesLookColours(const QLCPalette *effect)
{
    /* Whether an effect takes the look's colours and dimmer as its own input:
       a script reading palettes.look (directly, or through the WLED library),
       or the WLED device effect, which the host hands the look's colours. A
       movement, beam or fixed-colour effect (ballyhoo, gobo chase, amber
       pulse) doesn't -- a colour under it is just part of the look. Read once
       a script; asked from the DMX thread too, hence the lock. */
    if (effect == NULL || effect->type() != Effect)
        return false;
    const QString path = effect->scriptPath();
    if (path.isEmpty())
        return false;
    static QMutex lock;
    static QHash<QString, bool> known;
    QMutexLocker locker(&lock);
    auto it = known.constFind(path);
    if (it != known.constEnd())
        return it.value();
    bool uses = path.endsWith(QLatin1String("wled-device.js"));
    QFile f(path);
    if (!uses && f.open(QIODevice::ReadOnly))
    {
        const QByteArray src = f.readAll();
        uses = src.contains("palettes.look") || src.contains("WLED.segment");
    }
    known.insert(path, uses);
    return uses;
}

bool QLCPalette::isEffectScoped(Doc *doc, const QList<quint32> &orderedPalettes, quint32 paletteId)
{
    /* Ordering paradigm for a look's palette list, read top to bottom: a
       Colour or Dimmer palette placed under an Effect (until the next effect)
       "belongs to the effect" -- it applies to that effect only ("colors,
       dimmers, etc. assigned to effects only apply to that effect"), so it is
       NOT painted onto the fixtures as the look's static base. An effect that
       takes the look's colours uses them as its colours; one that doesn't (a
       ballyhoo) has the host apply them with its output (EffectInstance).
       Those above every effect are the look's own, across all its effects.
       An Aim, Pan/Tilt, Gobo... is never an effect's: an Aim after an effect
       never aimed. */
    QLCPalette *self = doc->palette(paletteId);
    if (self == NULL || (self->type() != Color && self->type() != Dimmer))
        return false;
    for (quint32 pid : orderedPalettes)
    {
        if (pid == paletteId)
            return false;
        QLCPalette *p = doc->palette(pid);
        if (p != NULL && p->type() == Effect)
            return true;                    // under an effect
    }
    return false;
}

QList<SceneValue> QLCPalette::valuesFromFixtureGroups(Doc *doc, QList<quint32> groups,
                                                      const Scene *owner, int colorSetOffset)
{
    QList<quint32> fixturesList;
    QHash<quint32, QSet<int> > heads;       // each fixture's heads that are in a group

    foreach (quint32 id, groups)
    {
        FixtureGroup *group = doc->fixtureGroup(id);
        if (group == NULL)
            continue;

        foreach (quint32 fid, group->fixtureList())
            if (!fixturesList.contains(fid))
                fixturesList.append(fid);
        foreach (const GroupHead &gh, group->headList())
            heads[gh.fxi].insert(gh.head);
    }

    return valuesFromFixtures(doc, fixturesList, owner, colorSetOffset, &heads);
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
        // Resolve the Effect palette's stored script reference.
        // New format: a portable name ("ramp-chase").
        // Old format: an absolute path that may not exist on this machine.
        // Either way, look it up in the cache; fall back to the stored value
        // so a valid absolute path still works.
        if (palette->type() == Effect && !palette->scriptPath().isEmpty()
                && doc->effectScriptRunner())
        {
            const EffectScriptCache *cache = doc->effectScriptRunner()->cache();
            const QString stored = palette->scriptPath();
            // Try stored value as a script name first (new format or bare basename).
            QString resolved = cache->scriptPath(stored);
            if (resolved.isEmpty())
            {
                // Old format: extract basename from absolute path and look up.
                resolved = cache->scriptPath(QFileInfo(stored).completeBaseName());
            }
            if (!resolved.isEmpty())
                palette->setScriptPath(resolved);
            // else: keep the stored value; may still be a valid absolute path
        }
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

    // NOTE on the skipCurrentElement() calls below: on SUCCESS this function
    // consumes the whole <Palette> element, so Doc::loadXML no longer skips it
    // for us. A bare `return false` would therefore leave the reader parked on
    // the <Palette> StartElement — Doc's `while (readNextStartElement())` then
    // reads the closing tag, gets false, and EXITS, silently dropping every
    // Function, FixtureGroup and MonitorProperties element after this palette.
    // One malformed palette must cost us that palette, not the rest of the file.
    bool ok = false;
    quint32 id = attrs.value(KXMLQLCPaletteID).toString().toUInt(&ok);
    if (ok == false)
    {
        qWarning() << "Invalid Palette ID:" << attrs.value(KXMLQLCPaletteID).toString();
        doc.skipCurrentElement();
        return false;
    }

    setID(id);

    if (attrs.hasAttribute(KXMLQLCPaletteType) == false)
    {
        qWarning() << "Palette type not found!";
        doc.skipCurrentElement();
        return false;
    }

    m_type = stringToType(attrs.value(KXMLQLCPaletteType).toString());

    // Migration: old files stored target-aimed palettes as PanTilt + StageTarget attribute.
    // Upgrade them to the dedicated Aim type on load.
    if (m_type == PanTilt && attrs.hasAttribute(KXMLQLCPaletteStageTarget))
        m_type = Aim;

    if (attrs.hasAttribute(KXMLQLCPaletteName))
        setName(attrs.value(KXMLQLCPaletteName).toString());

    if (attrs.hasAttribute(KXMLQLCPalettePath))
    {
        const QString path = attrs.value(KXMLQLCPalettePath).toString();
        if (attrs.value(KXMLQLCPaletteFolders) == QLatin1String("1"))
        {
            setPath(path);
            m_loadedMarked = true;
        }
        else
        {
            // The palette's own type first is the old layout whatever else
            // the show holds; another type's name waits for the whole show
            // (Doc load: migrateLegacyPaths()).
            setPath(migrateLegacyPath(path, m_type));
            m_unmarkedPath = path;
        }
    }

    // Effect looks only: keep the script engine alive across stop/start.
    if (attrs.hasAttribute(KXMLQLCPalettePersistent))
        m_persistent = (attrs.value(KXMLQLCPalettePersistent).toString() == QLatin1String("1"));

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
            case Aim:
                // Aim palettes don't store values — position is computed from geometry.
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
            case Strobe:
                setValue(strVal.toFloat()); // 0.0–1.0 relative rate
            break;
            case Effect:
            case Undefined: break;
        }
    }

    if (attrs.hasAttribute(KXMLQLCPaletteStageTarget))
        m_stageTargetId = attrs.value(KXMLQLCPaletteStageTarget).toUInt();

    /* a colour macro by name */
    if (m_type == Color)
        m_colourMacro = attrs.value(QStringLiteral("ColourMacro")).toString();

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
                case Aim:       break;
                case Beam:      break;
                case Shutter:   break;
                case Strobe:    break;
                case Gobo:      break;
                case Effect:    break;
                case Undefined: break;
            }
        }
    }

    // Effect-palette sub-elements; all other palette types have no children.
    // Either way, advance to </Palette> EndElement so the caller's
    // readNextStartElement() loop can move on cleanly (matching the
    // convention used by Function::loadXML and Fixture::loadXML).
    if (m_type == Effect)
    {
        while (doc.readNextStartElement())
        {
            if (doc.name() == QLatin1String("EffectScript"))
                m_scriptPath = doc.readElementText();
            else if (doc.name() == QLatin1String("EffectPreset"))
                m_effectPreset = doc.readElementText();
            else if (doc.name() == QLatin1String("WledEffect"))
                m_wledEffect = doc.readElementText();
            else if (doc.name() == QLatin1String("EffectInputBinding"))
            {
                QString slot   = doc.attributes().value("slot").toString();
                quint32 uni    = doc.attributes().value("universe").toUInt();
                quint32 ch     = doc.attributes().value("channel").toUInt();
                if (!slot.isEmpty())
                    m_effectInputBindings[slot] = qMakePair(uni, ch);
                doc.skipCurrentElement();
            }
            else if (doc.name() == QLatin1String("EffectPaletteBinding"))
            {
                QString slot = doc.attributes().value("slot").toString();
                quint32 pid  = doc.attributes().value("palette").toUInt();
                if (!slot.isEmpty())
                    m_effectPaletteBindings[slot] = pid;
                doc.skipCurrentElement();
            }
            else if (doc.name() == QLatin1String("EffectTargetBinding"))
            {
                QString slot = doc.attributes().value("slot").toString();
                quint32 tid  = doc.attributes().value("target").toUInt();
                if (!slot.isEmpty())
                    m_effectTargetBindings[slot] = tid;
                doc.skipCurrentElement();
            }
            else if (doc.name() == QLatin1String("EffectParam"))
            {
                QString name = doc.attributes().value("name").toString();
                double  val  = doc.attributes().value("value").toDouble();
                if (!name.isEmpty())
                    m_effectParamValues[name] = val;
                doc.skipCurrentElement();
            }
            else if (doc.name() == QLatin1String("EffectStringParam"))
            {
                QString name = doc.attributes().value("name").toString();
                QString val  = doc.attributes().value("value").toString();
                if (!name.isEmpty())
                    m_effectStringParams[name] = val;
                doc.skipCurrentElement();
            }
            else
                doc.skipCurrentElement();
        }
        // readNextStartElement() has consumed </Palette> — reader is at EndElement.
    }
    else if (m_type == Gobo)
    {
        m_goboName = attrs.value(QStringLiteral("GoboName")).toString();
        while (doc.readNextStartElement())
        {
            if (doc.name() == QLatin1String("GoboValue"))
            {
                const QString def = doc.attributes().value(QStringLiteral("Fixture")).toString();
                if (!def.isEmpty())
                    m_goboValues[def] = doc.attributes().value(QStringLiteral("Value")).toInt();
            }
            doc.skipCurrentElement();
        }
    }
    else if (m_type == Aim)
    {
        // A line's spread, group and cross (see saveXML()).
        while (doc.readNextStartElement())
        {
            if (doc.name() == QLatin1String("EffectParam"))
            {
                const QString name = doc.attributes().value("name").toString();
                if (!name.isEmpty())
                    m_effectParamValues[name] = doc.attributes().value("value").toDouble();
            }
            doc.skipCurrentElement();
        }
    }
    else
    {
        // No children: skip from <Palette> StartElement to </Palette> EndElement.
        doc.skipCurrentElement();
    }

    return true;
}

bool QLCPalette::saveXML(QXmlStreamWriter *doc)
{
    Q_ASSERT(doc != NULL);

    // Shutter, Gobo, Effect, and Aim palettes are valid without m_values.
    // A value-less palette of any OTHER type is malformed — but it must still
    // be written out. Scenes persist their looks as <Palette ID="n"/> refs
    // (see Scene::saveXML), so skipping the palette here would leave those refs
    // dangling and the look would silently disappear on the next load. Write it
    // with a zeroed value instead and warn; the ref stays resolvable and the
    // user can see and fix the palette rather than losing it.
    if (m_values.isEmpty() && m_type != Shutter && m_type != Gobo
        && m_type != Effect && m_type != Aim)
    {
        qWarning() << "Saving palette" << this->id() << name()
                   << "of type" << typeToString(m_type)
                   << "with no value — writing a zeroed value to keep scene"
                      " references intact.";
    }

    /* write a Palette entry */
    doc->writeStartElement(KXMLQLCPalette);
    doc->writeAttribute(KXMLQLCPaletteID, QString::number(this->id()));
    doc->writeAttribute(KXMLQLCPaletteType, typeToString(m_type));
    doc->writeAttribute(KXMLQLCPaletteName, this->name());
    if (m_type == Effect && m_persistent)
        doc->writeAttribute(KXMLQLCPalettePersistent, QStringLiteral("1"));
    if (m_path.isEmpty() == false)
    {
        doc->writeAttribute(KXMLQLCPalettePath, m_path);
        doc->writeAttribute(KXMLQLCPaletteFolders, QStringLiteral("1"));
    }

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
        {
            // Count-guarded like Beam below: a PanTilt whose stored Value did
            // not split into exactly two parts leaves m_values short, and a
            // bare at(1) is then an out-of-range QList access.
            int pan  = (m_values.count() > 0) ? m_values.at(0).toInt() : 0;
            int tilt = (m_values.count() > 1) ? m_values.at(1).toInt() : 0;
            doc->writeAttribute(KXMLQLCPaletteValue,
                                QString("%1,%2").arg(pan).arg(tilt));
        }
        break;
        case Aim:
            // No m_values — position is computed per-fixture from rig geometry.
            // StageTarget ID is written below.
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
        case Strobe:
            if (!m_values.isEmpty())
                doc->writeAttribute(KXMLQLCPaletteValue,
                                    QString::number(value().toFloat(), 'f', 4));
        break;
        case Effect:
            // Script path and bindings saved as child elements below
        break;
        case Undefined: break;
    }

    /* a gobo by name */
    if (m_type == Gobo && !m_goboName.isEmpty())
        doc->writeAttribute(QStringLiteral("GoboName"), m_goboName);

    /* a colour macro by name */
    if (m_type == Color && !m_colourMacro.isEmpty())
        doc->writeAttribute(QStringLiteral("ColourMacro"), m_colourMacro);

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

    /* a gobo's slot picked per fixture type */
    if (m_type == Gobo)
    {
        for (auto it = m_goboValues.constBegin(); it != m_goboValues.constEnd(); ++it)
        {
            doc->writeStartElement(QStringLiteral("GoboValue"));
            doc->writeAttribute(QStringLiteral("Fixture"), it.key());
            doc->writeAttribute(QStringLiteral("Value"), QString::number(it.value()));
            doc->writeEndElement();
        }
    }

    /* write Effect-palette sub-elements */
    if (m_type == Effect)
    {
        if (!m_scriptPath.isEmpty())
        {
            // Store the portable script name (no directory, no extension) so
            // workspaces stay valid when moved between machines.
            doc->writeStartElement("EffectScript");
            doc->writeCharacters(EffectScriptCache::portableName(m_scriptPath));
            doc->writeEndElement();
        }
        if (!m_effectPreset.isEmpty())
        {
            // Preset identity (display only); script+params above are authoritative.
            doc->writeStartElement("EffectPreset");
            doc->writeCharacters(m_effectPreset);
            doc->writeEndElement();
        }
        if (!m_wledEffect.isEmpty())
        {
            // Run ON WLED fixtures by the device (see wledEffect()).
            doc->writeStartElement("WledEffect");
            doc->writeCharacters(m_wledEffect);
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
        for (auto it = m_effectTargetBindings.constBegin();
             it != m_effectTargetBindings.constEnd(); ++it)
        {
            doc->writeStartElement("EffectTargetBinding");
            doc->writeAttribute("slot",   it.key());
            doc->writeAttribute("target", QString::number(it.value()));
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
        for (auto it = m_effectStringParams.constBegin();
             it != m_effectStringParams.constEnd(); ++it)
        {
            doc->writeStartElement("EffectStringParam");
            doc->writeAttribute("name",  it.key());
            doc->writeAttribute("value", it.value());
            doc->writeEndElement();
        }
    }
    /* An Aim look on a line carries its spread (LineSpread's "__line*"
       params), one element each so each can be inherited on its own. A build
       that predates lines skips an Aim palette's children. */
    else if (m_type == Aim)
    {
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

/****************************************************************************
 * Gobo by name
 ****************************************************************************/

QString QLCPalette::definitionKey(const Fixture *fixture)
{
    if (fixture == nullptr || fixture->fixtureDef() == nullptr)
        return QString();
    return QStringLiteral("%1/%2").arg(fixture->fixtureDef()->manufacturer(),
                                        fixture->fixtureDef()->model());
}

static QString goboKey(const QString &name)
{
    QString out;
    for (const QChar c : name)
        if (!c.isSpace() && c != QLatin1Char('-') && c != QLatin1Char('_'))
            out += c.toLower();
    return out;
}

bool QLCPalette::sameGoboName(const QString &a, const QString &b)
{
    const QString ka = goboKey(a);
    return !ka.isEmpty() && ka == goboKey(b);
}

/** Tier 2's key: the words without "gobo" and numbers, run together, with
    trailing digits and a plural 's' dropped ("Gobo 3 Dots" -> "dot"). */
static QString goboStem(const QString &name)
{
    QString out;
    QString word;
    auto flush = [&out, &word]() {
        bool digits = !word.isEmpty();
        for (const QChar c : word)
            if (!c.isDigit())
                digits = false;
        if (!digits && word != QLatin1String("gobo") && word != QLatin1String("gobos"))
            out += word;
        word.clear();
    };
    for (const QChar c : name)
    {
        if (c.isLetterOrNumber())
            word += c.toLower();
        else
            flush();
    }
    flush();
    while (!out.isEmpty() && out.at(out.size() - 1).isDigit())
        out.chop(1);
    if (out.size() > 2 && out.endsWith(QLatin1Char('s')) && !out.endsWith(QLatin1String("ss")))
        out.chop(1);
    return out;
}

int QLCPalette::matchGoboName(const QString &wanted, const QList<QStringList> &gobos,
                              GoboMatch *how, QList<int> *ambiguous)
{
    if (how != nullptr)
        *how = GoboNoMatch;
    if (ambiguous != nullptr)
        ambiguous->clear();
    if (goboKey(wanted).isEmpty())
        return -1;

    auto decide = [how, ambiguous, &gobos](const QList<int> &found, GoboMatch tier) {
        /* The same name twice on a wheel (an "Open", a glass repeated in its
           shake range) is one gobo: its first slot. Different names are a
           guess, not taken. */
        QSet<QString> names;
        for (int i : found)
            names.insert(goboKey(gobos.at(i).value(0)));
        if (found.count() == 1 || names.count() == 1)
        {
            if (how != nullptr)
                *how = tier;
            return found.first();
        }
        if (how != nullptr)
            *how = GoboAmbiguous;
        if (ambiguous != nullptr)
            *ambiguous = found;
        return -1;
    };

    // 1. Exact: the slot's name or its picture.
    QList<int> found;
    for (int i = 0; i < gobos.count(); ++i)
        for (const QString &n : gobos.at(i))
            if (sameGoboName(n, wanted))
            {
                found << i;
                break;
            }
    if (!found.isEmpty())
        return decide(found, GoboExact);

    // A picture ("ClayPaky/gobo00006") is that glass or nothing.
    if (wanted.contains(QLatin1Char('/')))
        return -1;

    // 2. Singular/plural, without "gobo" and numbers.
    const QString stem = goboStem(wanted);
    if (stem.isEmpty())
        return -1;
    for (int i = 0; i < gobos.count(); ++i)
        if (!gobos.at(i).isEmpty() && goboStem(gobos.at(i).first()) == stem)
            found << i;
    if (!found.isEmpty())
        return decide(found, GoboSingular);

    // 3. The one slot whose name starts with or contains it.
    if (stem.size() < 3)
        return -1;
    for (int i = 0; i < gobos.count(); ++i)
        if (!gobos.at(i).isEmpty() && goboKey(gobos.at(i).first()).contains(stem))
            found << i;
    if (!found.isEmpty())
        return decide(found, GoboPartial);
    return -1;
}

QLCPalette::GoboResolution QLCPalette::resolveGobo(const Fixture *fixture) const
{
    GoboResolution r;
    if (m_type != Gobo || fixture == nullptr || fixture->fixtureMode() == nullptr)
        return r;
    const quint32 goboCh = fixture->fixtureMode()->channelNumber(QLCChannel::Gobo, QLCChannel::MSB);
    if (goboCh == QLCChannel::invalid())
        return r;

    const QString def = definitionKey(fixture);
    if (!def.isEmpty() && m_goboValues.contains(def))
    {
        r.value = qBound(0, m_goboValues.value(def), 255);
        r.how = GoboPicked;
        return r;
    }

    if (m_goboName.isEmpty())
    {
        if (!m_values.isEmpty())
        {
            r.value = qBound(0, value().toInt(), 255);
            r.how = GoboRaw;
        }
        return r;
    }

    // Its wheel's slots: the capability's own name, then its picture
    // ("ClayPaky/gobo00006.svg" -> "ClayPaky/gobo00006").
    const QLCChannel *ch = fixture->channel(goboCh);
    if (ch == nullptr)
        return r;
    const QList<QLCCapability *> caps = ch->capabilities();
    QList<QStringList> gobos;
    for (QLCCapability *cap : caps)
    {
        QStringList names(cap->name());
        if (cap->presetType() == QLCCapability::Picture)
        {
            // The resource may be resolved to a full path: its folder and
            // file name are what name the glass.
            const QFileInfo picture(cap->resource(0).toString());
            names << picture.dir().dirName() + QLatin1Char('/') + picture.completeBaseName();
        }
        gobos << names;
    }
    QList<int> ambiguous;
    const int at = matchGoboName(m_goboName, gobos, &r.how, &ambiguous);
    if (at >= 0)
        r.value = (int(caps.at(at)->min()) + int(caps.at(at)->max())) / 2;
    for (int i : ambiguous)
        r.candidates << caps.at(i)->name();
    return r;
}

/*****************************************************************************
 * Colour wheels and colour macros
 *****************************************************************************/

QColor QLCPalette::capabilityColour(const QLCCapability *cap)
{
    if (cap == nullptr)
        return QColor();
    QLCCapability *c = const_cast<QLCCapability *>(cap);
    const QColor c1 = c->resource(0).value<QColor>();
    const QColor c2 = c->resource(1).value<QColor>();
    if (c1.isValid())
        return c2.isValid() && c2 != c1 ? QColor() : c1;   // a split is no one colour

    QString name = cap->name().trimmed().toLower();
    // "Red 2", "Blue (deep)": the colour is the words before.
    const int paren = name.indexOf(QLatin1Char('('));
    if (paren > 0)
        name = name.left(paren).trimmed();
    while (!name.isEmpty() && (name.at(name.size() - 1).isDigit() || name.at(name.size() - 1).isSpace()))
        name.chop(1);
    if (name.isEmpty())
        return QColor();

    static const QStringList off = { "off", "none", "no color", "no colour", "blackout", "black",
                                     "dark", "closed", "close" };
    static const QStringList open = { "open", "white", "open white", "open/white", "no filter" };
    if (off.contains(name))
        return QColor(Qt::black);
    if (open.contains(name))
        return QColor(Qt::white);

    // Not one colour: a split, a mix of two, a chase, a rainbow...
    static const QStringList moving = { "/", "+", "&", " and ", "chase", "random", "rainbow", "scroll",
                                        "rotat", "fade", "jump", "auto", "sound", "music", "strobe",
                                        "split", "half", "cycle", "change", "speed", "mix", "effect" };
    for (const QString &m : moving)
        if (name.contains(m))
            return QColor();

    QString key = name;
    key.remove(QLatin1Char(' ')).remove(QLatin1Char('-')).remove(QLatin1Char('_'));
    static const QHash<QString, QColor> extra = {
        { "amber", QColor(255, 191, 0) }, { "uv", QColor(148, 0, 211) },
        { "ultraviolet", QColor(148, 0, 211) }, { "warmwhite", QColor(255, 220, 180) },
        { "coolwhite", QColor(220, 235, 255) }, { "coldwhite", QColor(220, 235, 255) },
        { "cto", QColor(255, 200, 140) }, { "ctb", QColor(200, 220, 255) },
        { "lightgreen", QColor(144, 238, 144) }, { "darkblue", QColor(0, 0, 139) } };
    if (extra.contains(key))
        return extra.value(key);
    const QColor named = QColor::fromString(key);
    return named.isValid() ? named : QColor();
}

int QLCPalette::nearestColourValue(const QLCChannel *ch, const QColor &col, QString *slotName)
{
    if (ch == nullptr || !col.isValid())
        return -1;

    struct Slot { int value; QColor colour; QString name; };
    QList<Slot> black, grey, chroma;
    for (const QLCCapability *cap : ch->capabilities())
    {
        const QColor c = capabilityColour(cap);
        if (!c.isValid())
            continue;
        Slot s { int(cap->middle()), c, cap->name() };
        if (c.value() < 16)
            black << s;
        else if (c.hsvSaturation() < 51)
            grey << s;
        else
            chroma << s;
    }
    if (black.isEmpty() && grey.isEmpty() && chroma.isEmpty())
        return -1;

    auto pick = [slotName](const Slot &s) {
        if (slotName != nullptr)
            *slotName = s.name;
        return s.value;
    };
    // The plain RGB distance: the last word, between slots of one kind.
    auto nearestRgb = [&col](const QList<Slot> &cands) {
        int best = 0;
        qint64 bestDist = LLONG_MAX;
        for (int i = 0; i < cands.size(); ++i)
        {
            const qint64 dr = col.red() - cands.at(i).colour.red();
            const qint64 dg = col.green() - cands.at(i).colour.green();
            const qint64 db = col.blue() - cands.at(i).colour.blue();
            const qint64 d = dr * dr + dg * dg + db * db;
            if (d < bestDist)
            {
                bestDist = d;
                best = i;
            }
        }
        return cands.at(best);
    };

    if (col.value() < 16)
    {
        // Black: off where there is an off; a wheel has none, and stays open
        // (its dimmer makes it dark).
        if (!black.isEmpty())
            return pick(black.first());
        if (!grey.isEmpty())
            return pick(nearestRgb(QList<Slot>() << grey));
        return -1;
    }
    if (col.hsvSaturation() < 51 || chroma.isEmpty())
    {
        // White or a grey: the white slot -- a grey is a dim white.
        if (!grey.isEmpty())
        {
            Slot best = grey.first();
            for (const Slot &s : grey)
                if (s.colour.value() > best.colour.value())
                    best = s;
            return pick(best);
        }
        return chroma.isEmpty() ? -1 : pick(nearestRgb(chroma));
    }

    // A colour: the slot of the nearest hue, then the nearest saturation.
    const int hue = col.hsvHue();
    int best = -1, bestHue = INT_MAX, bestSat = INT_MAX;
    for (int i = 0; i < chroma.size(); ++i)
    {
        int d = qAbs(chroma.at(i).colour.hsvHue() - hue);
        d = qMin(d, 360 - d);
        const int ds = qAbs(chroma.at(i).colour.hsvSaturation() - col.hsvSaturation());
        if (d < bestHue || (d == bestHue && ds < bestSat))
        {
            best = i;
            bestHue = d;
            bestSat = ds;
        }
    }
    return pick(chroma.at(best));
}

int QLCPalette::namedSlotValue(const QLCChannel *ch, const QString &name)
{
    if (ch == nullptr || name.trimmed().isEmpty())
        return -1;
    for (const QLCCapability *cap : ch->capabilities())
        if (sameGoboName(cap->name(), name))
            return int(cap->middle());
    return -1;
}

int QLCPalette::colourSlotValue(const QLCChannel *ch, const QColor &col) const
{
    if (!m_colourMacro.isEmpty())
    {
        const int v = namedSlotValue(ch, m_colourMacro);
        if (v >= 0)
            return v;
    }
    return nearestColourValue(ch, col);
}

QList<quint32> QLCPalette::colourSlotChannels(const Fixture *fixture, int head)
{
    QList<quint32> out;
    const QLCFixtureMode *mode = fixture != nullptr ? fixture->fixtureMode() : nullptr;
    if (mode == nullptr)
        return out;
    if (head >= 0)
    {
        if (head >= fixture->heads() || !fixture->rgbChannelSets(head).isEmpty()
            || fixture->cmyChannels(head).size() >= 3)
            return out;
        for (quint32 c : fixture->head(head).colorWheels())
            out << c;
        return out;
    }

    // In no head: the fixture's, when it mixes colour nowhere.
    QList<quint32> loose;
    for (quint32 c = 0; c < quint32(mode->channels().size()); ++c)
    {
        const QLCChannel *ch = mode->channel(c);
        if (ch == nullptr)
            continue;
        if (ch->group() == QLCChannel::Intensity)
        {
            switch (ch->colour())
            {
            case QLCChannel::Red: case QLCChannel::Green: case QLCChannel::Blue:
            case QLCChannel::Cyan: case QLCChannel::Magenta: case QLCChannel::Yellow:
                return out;                 // it mixes: leave its macros alone
            default:
                break;
            }
        }
        if (ch->group() == QLCChannel::Colour && ch->controlByte() == QLCChannel::MSB
            && mode->headForChannel(c) < 0)
            loose << c;
    }
    return loose;
}
