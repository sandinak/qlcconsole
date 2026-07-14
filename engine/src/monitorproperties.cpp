/*
  Q Light Controller Plus
  monitorproperties.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>
#include <QFont>

#include "monitorproperties.h"
#include "truss.h"
#include "stageplatform.h"
#include "stagetarget.h"
#include "qlcconfig.h"
#include "qlcfile.h"
#include "doc.h"

#define KXMLQLCMonitorDisplay       QStringLiteral("DisplayMode")
#define KXMLQLCMonitorChannels      QStringLiteral("ChannelStyle")
#define KXMLQLCMonitorValues        QStringLiteral("ValueStyle")
#define KXMLQLCMonitorFont          QStringLiteral("Font")
#define KXMLQLCMonitorGrid          QStringLiteral("Grid")
#define KXMLQLCMonitorGridWidth     QStringLiteral("Width")
#define KXMLQLCMonitorGridHeight    QStringLiteral("Height")
#define KXMLQLCMonitorGridDepth     QStringLiteral("Depth")
#define KXMLQLCMonitorGridUnits     QStringLiteral("Units")
#define KXMLQLCMonitorGridSubdiv    QStringLiteral("Subdivisions")
#define KXMLQLCMonitorPointOfView   QStringLiteral("POV")
#define KXMLQLCMonitorLayoutLocked  QStringLiteral("LayoutLocked")
#define KXMLQLCMonitorSnapDivisions QStringLiteral("SnapDivisions")
#define KXMLQLCMonitorAimSubjectHeight QStringLiteral("AimSubjectHeight")
#define KXMLQLCMonitorItemID        QStringLiteral("ID")
#define KXMLQLCMonitorShowLabels    QStringLiteral("ShowLabels")

#define KXMLQLCMonitorCommonBackground  QStringLiteral("Background")
#define KXMLQLCMonitorBgColor           QStringLiteral("BackgroundColor")
#define KXMLQLCMonitorCustomBgItem      QStringLiteral("BackgroundItem")

#define KXMLQLCMonitorFixtureItem   QStringLiteral("FxItem")
#define KXMLQLCMonitorStageItem     QStringLiteral("StageItem")
#define KXMLQLCMonitorMeshItem      QStringLiteral("MeshItem")
#define KXMLQLCMonitorItemName      QStringLiteral("Name")
#define KXMLQLCMonitorItemRes       QStringLiteral("Res")

#define KXMLQLCMonitorItemXPosition     QStringLiteral("XPos")
#define KXMLQLCMonitorItemYPosition     QStringLiteral("YPos")
#define KXMLQLCMonitorItemZPosition     QStringLiteral("ZPos")
#define KXMLQLCMonitorItemXRotation     QStringLiteral("XRot")
#define KXMLQLCMonitorItemYRotation     QStringLiteral("YRot")
#define KXMLQLCMonitorItemZRotation     QStringLiteral("ZRot")
#define KXMLQLCMonitorFixtureRotation   QStringLiteral("Rotation") // LEGACY
#define KXMLQLCMonitorItemXScale        QStringLiteral("XScale")
#define KXMLQLCMonitorItemYScale        QStringLiteral("YScale")
#define KXMLQLCMonitorItemZScale        QStringLiteral("ZScale")

#define KXMLQLCMonitorFixtureHeadIndex      QStringLiteral("Head")
#define KXMLQLCMonitorFixtureLinkedIndex    QStringLiteral("Linked")

#define KXMLQLCMonitorFixtureGelColor   QStringLiteral("GelColor")
#define KXMLQLCMonitorFixtureFixedZoom  QStringLiteral("FixedZoom")

#define KXMLQLCMonitorFixtureHiddenFlag     QStringLiteral("Hidden")
#define KXMLQLCMonitorFixtureInvPanFlag     QStringLiteral("InvertedPan")
#define KXMLQLCMonitorFixtureInvTiltFlag    QStringLiteral("InvertedTilt")

#define GRID_DEFAULT_WIDTH  5
#define GRID_DEFAULT_HEIGHT 3
#define GRID_DEFAULT_DEPTH  5

MonitorProperties::MonitorProperties()
    : m_font(QFont("Arial", 12))
    , m_displayMode(DMX)
    , m_channelStyle(DMXChannels)
    , m_valueStyle(DMXValues)
    , m_gridSize(QVector3D(GRID_DEFAULT_WIDTH, GRID_DEFAULT_HEIGHT, GRID_DEFAULT_DEPTH))
    , m_gridUnits(Meters)
    , m_pointOfView(Undefined)
    , m_stageType(StageSimple)
    , m_layoutLocked(false)
    , m_gridSubdivisions(1)
    , m_snapDivisions(0)
    , m_aimSubjectHeight(1.4f)
    , m_showLabels(false)
{
}

void MonitorProperties::reset()
{
    m_gridSize = QVector3D(GRID_DEFAULT_WIDTH, GRID_DEFAULT_HEIGHT, GRID_DEFAULT_DEPTH);
    m_gridUnits = Meters;
    m_pointOfView = Undefined;
    m_stageType = StageSimple;
    m_layoutLocked = false;
    m_gridSubdivisions = 1;
    m_snapDivisions = 0;
    m_aimSubjectHeight = 1.4f;
    m_showLabels = false;
    m_fixtureItems.clear();
    m_genericItems.clear();
    m_commonBackgroundImage = QString();
    qDeleteAll(m_trusses);
    m_trusses.clear();
    qDeleteAll(m_platforms);
    m_platforms.clear();
    qDeleteAll(m_stageTargets);
    m_stageTargets.clear();
    m_rigProps.clear();
}

/********************************************************************
 * Environment
 ********************************************************************/

void MonitorProperties::setPointOfView(MonitorProperties::PointOfView pov)
{
    if (pov == m_pointOfView)
        return;

    if (m_pointOfView == Undefined)
    {
        QVector3D gSize = gridSize();
        float units = gridUnits() == MonitorProperties::Meters ? 1000.0 : 304.8;

        if (gSize.z() == 0)
        {
            // convert the grid size first
            switch (pov)
            {
                case TopView:
                    setGridSize(QVector3D(gSize.x(), GRID_DEFAULT_HEIGHT, gSize.y()));
                break;
                case RightSideView:
                case LeftSideView:
                    setGridSize(QVector3D(GRID_DEFAULT_WIDTH, gSize.x(), gSize.x()));
                break;
                default:
                break;
            }
        }

        foreach (quint32 fid, fixtureItemsID())
        {
            foreach (quint32 subID, fixtureIDList(fid))
            {
                QVector3D pos = fixturePosition(fid, fixtureHeadIndex(subID), fixtureLinkedIndex(subID));
                QVector3D newPos;

                switch (pov)
                {
                    case TopView:
                    {
                        newPos = QVector3D(pos.x(), 1000, pos.y());
                    }
                    break;
                    case RightSideView:
                    {
                        newPos = QVector3D(0, pos.y(), (gridSize().z() * units) - pos.x());
                    }
                    break;
                    case LeftSideView:
                    {
                        newPos = QVector3D(0, pos.y(), pos.x());
                    }
                    break;
                    default:
                        newPos = QVector3D(pos.x(), (gridSize().y() * units) - pos.y(), 1000);
                    break;
                }
                setFixturePosition(fid, fixtureHeadIndex(subID), fixtureLinkedIndex(subID), newPos);
            }
        }
    }
    m_pointOfView = pov;
}

/********************************************************************
 * Fixture items
 ********************************************************************/

void MonitorProperties::removeFixture(quint32 fid)
{
    if (m_fixtureItems.contains(fid))
        m_fixtureItems.take(fid);
}

void MonitorProperties::removeFixture(quint32 fid, quint16 head, quint16 linked)
{
    if (m_fixtureItems.contains(fid) == false)
        return;

    // if no sub items are present,
    // the fixture can be removed completely
    if (m_fixtureItems[fid].m_subItems.count() == 0)
    {
        m_fixtureItems.take(fid);
        return;
    }

    quint32 subID = fixtureSubID(head, linked);
    m_fixtureItems[fid].m_subItems.remove(subID);
}

quint32 MonitorProperties::fixtureSubID(quint32 headIndex, quint32 linkedIndex) const
{
    return ((headIndex << 16) | linkedIndex);
}

quint16 MonitorProperties::fixtureHeadIndex(quint32 mapID) const
{
    return (quint16)(mapID >> 16);
}

quint16 MonitorProperties::fixtureLinkedIndex(quint32 mapID) const
{
    return (quint16)(mapID & 0x0000FFFF);
}

bool MonitorProperties::containsItem(quint32 fid, quint16 head, quint16 linked)
{
    if (m_fixtureItems.contains(fid) == false)
        return false;

    if (head == 0 && linked == 0)
        return true;

    quint32 subID = fixtureSubID(head, linked);
    return m_fixtureItems[fid].m_subItems.contains(subID);
}

void MonitorProperties::setFixturePosition(quint32 fid, quint16 head, quint16 linked, QVector3D pos)
{
    //qDebug() << Q_FUNC_INFO << "X:" << pos.x() << "Y:" << pos.y();
    if (head == 0 && linked == 0)
    {
        m_fixtureItems[fid].m_baseItem.m_position = pos;
    }
    else
    {
        quint32 subID = fixtureSubID(head, linked);
        m_fixtureItems[fid].m_subItems[subID].m_position = pos;
    }
}

QVector3D MonitorProperties::fixturePosition(quint32 fid, quint16 head, quint16 linked) const
{
    if (head == 0 && linked == 0)
    {
        return m_fixtureItems[fid].m_baseItem.m_position;
    }
    else
    {
        quint32 subID = fixtureSubID(head, linked);
        return m_fixtureItems[fid].m_subItems[subID].m_position;
    }
}

void MonitorProperties::setFixtureRotation(quint32 fid, quint16 head, quint16 linked, QVector3D degrees)
{
    if (head == 0 && linked == 0)
    {
        m_fixtureItems[fid].m_baseItem.m_rotation = degrees;
    }
    else
    {
        quint32 subID = fixtureSubID(head, linked);
        m_fixtureItems[fid].m_subItems[subID].m_rotation = degrees;
    }
}

QVector3D MonitorProperties::fixtureRotation(quint32 fid, quint16 head, quint16 linked) const
{
    if (head == 0 && linked == 0)
    {
        return m_fixtureItems[fid].m_baseItem.m_rotation;
    }
    else
    {
        quint32 subID = fixtureSubID(head, linked);
        return m_fixtureItems[fid].m_subItems[subID].m_rotation;
    }
}

void MonitorProperties::setFixtureGelColor(quint32 fid, quint16 head, quint16 linked, QColor col)
{
    //qDebug() << Q_FUNC_INFO << "Gel color:" << col;
    if (head == 0 && linked == 0)
    {
        m_fixtureItems[fid].m_baseItem.m_color = col;
    }
    else
    {
        quint32 subID = fixtureSubID(head, linked);
        m_fixtureItems[fid].m_subItems[subID].m_color = col;
    }
}

QColor MonitorProperties::fixtureGelColor(quint32 fid, quint16 head, quint16 linked) const
{
    if (head == 0 && linked == 0)
    {
        return m_fixtureItems[fid].m_baseItem.m_color;
    }
    else
    {
        quint32 subID = fixtureSubID(head, linked);
        return m_fixtureItems[fid].m_subItems[subID].m_color;
    }
}

void MonitorProperties::setFixtureFixedZoom(quint32 fid, quint16 head, quint16 linked, int degrees)
{
    if (head == 0 && linked == 0)
    {
        m_fixtureItems[fid].m_baseItem.m_zoom = degrees;
    }
    else
    {
        quint32 subID = fixtureSubID(head, linked);
        m_fixtureItems[fid].m_subItems[subID].m_zoom = degrees;
    }
}

int MonitorProperties::fixtureFixedZoom(quint32 fid, quint16 head, quint16 linked) const
{
    if (head == 0 && linked == 0)
    {
        return m_fixtureItems[fid].m_baseItem.m_zoom;
    }
    else
    {
        quint32 subID = fixtureSubID(head, linked);
        return m_fixtureItems[fid].m_subItems[subID].m_zoom;
    }
}

void MonitorProperties::setFixtureName(quint32 fid, quint16 head, quint16 linked, QString name)
{
    if (head == 0 && linked == 0)
    {
        m_fixtureItems[fid].m_baseItem.m_name = name;
    }
    else
    {
        quint32 subID = fixtureSubID(head, linked);
        m_fixtureItems[fid].m_subItems[subID].m_name = name;
    }
}

QString MonitorProperties::fixtureName(quint32 fid, quint16 head, quint16 linked) const
{
    if (head == 0 && linked == 0)
    {
        return m_fixtureItems[fid].m_baseItem.m_name;
    }
    else
    {
        quint32 subID = fixtureSubID(head, linked);
        return m_fixtureItems[fid].m_subItems[subID].m_name;
    }
}

void MonitorProperties::setFixtureFlags(quint32 fid, quint16 head, quint16 linked, quint32 flags)
{
    if (head == 0 && linked == 0)
    {
        m_fixtureItems[fid].m_baseItem.m_flags = flags;
    }
    else
    {
        quint32 subID = fixtureSubID(head, linked);
        m_fixtureItems[fid].m_subItems[subID].m_flags = flags;
    }
}

quint32 MonitorProperties::fixtureFlags(quint32 fid, quint16 head, quint16 linked) const
{
    if (head == 0 && linked == 0)
    {
        return m_fixtureItems[fid].m_baseItem.m_flags;
    }
    else
    {
        quint32 subID = fixtureSubID(head, linked);
        return m_fixtureItems[fid].m_subItems[subID].m_flags;
    }
}

PreviewItem MonitorProperties::fixtureItem(quint32 fid, quint16 head, quint16 linked) const
{
    if (head == 0 && linked == 0)
    {
        return m_fixtureItems[fid].m_baseItem;
    }
    else
    {
        quint32 subID = fixtureSubID(head, linked);
        return m_fixtureItems[fid].m_subItems[subID];
    }
}

void MonitorProperties::setFixtureItem(quint32 fid, quint16 head, quint16 linked, PreviewItem props)
{
    if (head == 0 && linked == 0)
    {
        m_fixtureItems[fid].m_baseItem = props;
    }
    else
    {
        quint32 subID = fixtureSubID(head, linked);
        m_fixtureItems[fid].m_subItems[subID] = props;
    }
}

QList<quint32> MonitorProperties::fixtureIDList(quint32 fid) const
{
    QList<quint32> list;

    // always add the basic fixture item ID
    list.append(0);

    if (m_fixtureItems.contains(fid) == false)
        return list;

    FixturePreviewItem fxItem = m_fixtureItems[fid];
    list.append(fxItem.m_subItems.keys());

    return list;
}

/********************************************************************
 * Generic items
 ********************************************************************/

QList<quint32> MonitorProperties::genericItemsID()
{
    return m_genericItems.keys();
}

QString MonitorProperties::itemName(quint32 itemID)
{
    if (m_genericItems[itemID].m_name.isEmpty())
    {
        QFileInfo rName(m_genericItems[itemID].m_resource);
        return rName.baseName();
    }

    return m_genericItems[itemID].m_name;
}

void MonitorProperties::setItemName(quint32 itemID, QString name)
{
    m_genericItems[itemID].m_name = name;
}

QString MonitorProperties::itemResource(quint32 itemID)
{
    return m_genericItems[itemID].m_resource;
}

void MonitorProperties::setItemResource(quint32 itemID, QString resource)
{
    m_genericItems[itemID].m_resource = resource;
}

QVector3D MonitorProperties::itemPosition(quint32 itemID)
{
    return m_genericItems[itemID].m_position;
}

void MonitorProperties::setItemPosition(quint32 itemID, QVector3D pos)
{
    m_genericItems[itemID].m_position = pos;
}

QVector3D MonitorProperties::itemRotation(quint32 itemID)
{
    return m_genericItems[itemID].m_rotation;
}

void MonitorProperties::setItemRotation(quint32 itemID, QVector3D rot)
{
    m_genericItems[itemID].m_rotation = rot;
}

QVector3D MonitorProperties::itemScale(quint32 itemID)
{
    if (m_genericItems[itemID].m_scale.isNull())
        return QVector3D(1.0, 1.0, 1.0);

    return m_genericItems[itemID].m_scale;
}

void MonitorProperties::setItemScale(quint32 itemID, QVector3D scale)
{
    m_genericItems[itemID].m_scale = scale;
}

quint32 MonitorProperties::itemFlags(quint32 itemID)
{
    return m_genericItems[itemID].m_flags;
}

void MonitorProperties::setItemFlags(quint32 itemID, quint32 flags)
{
    m_genericItems[itemID].m_flags = flags;
}

/********************************************************************
 * 2D view background
 ********************************************************************/

QString MonitorProperties::customBackground(quint32 fid)
{
    return m_customBackgroundImages.value(fid, QString());
}

/*********************************************************************
 * Trusses
 *********************************************************************/

quint32 MonitorProperties::nextTrussId() const
{
    quint32 id = 0;
    while (m_trusses.contains(id))
        ++id;
    return id;
}

Truss *MonitorProperties::addTruss()
{
    quint32 id = nextTrussId();
    Truss *t = new Truss(id, this);
    m_trusses.insert(id, t);
    return t;
}

void MonitorProperties::removeTruss(quint32 id)
{
    // Un-assign any fixtures that were on this truss.
    for (auto it = m_rigProps.begin(); it != m_rigProps.end(); ++it)
        if (it->trussId == id)
            it->trussId = Truss::invalidId();

    delete m_trusses.take(id);
}

/*********************************************************************
 * Stage platforms
 *********************************************************************/

quint32 MonitorProperties::nextPlatformId() const
{
    quint32 id = 0;
    while (m_platforms.contains(id))
        ++id;
    return id;
}

StagePlatform *MonitorProperties::addPlatform()
{
    quint32 id = nextPlatformId();
    StagePlatform *p = new StagePlatform(id, this);
    m_platforms.insert(id, p);
    return p;
}

void MonitorProperties::removePlatform(quint32 id)
{
    delete m_platforms.take(id);
}

float MonitorProperties::platformHeightAt(float xMetres, float yMetres) const
{
    float h = 0.0f;
    foreach (const StagePlatform *p, m_platforms)
    {
        if (p->containsPoint(xMetres, yMetres))
            h = qMax(h, p->height());
    }
    return h;
}

/*********************************************************************
 * Stage targets
 *********************************************************************/

quint32 MonitorProperties::nextStageTargetId() const
{
    quint32 id = 0;
    while (m_stageTargets.contains(id))
        ++id;
    return id;
}

StageTarget *MonitorProperties::addStageTarget()
{
    quint32 id = nextStageTargetId();
    StageTarget *t = new StageTarget(id, this);
    m_stageTargets.insert(id, t);
    return t;
}

void MonitorProperties::removeStageTarget(quint32 id)
{
    delete m_stageTargets.take(id);
}

/*********************************************************************
 * Fixture rig properties
 *********************************************************************/

FixtureRigProps MonitorProperties::fixtureRigProps(quint32 fid) const
{
    return m_rigProps.value(fid, FixtureRigProps());
}

void MonitorProperties::setFixtureRigProps(quint32 fid, const FixtureRigProps &props)
{
    m_rigProps[fid] = props;
    emit rigPropsChanged(fid);
}

QVector3D MonitorProperties::fixtureRigPosition(quint32 fid) const
{
    if (!m_fixtureItems.contains(fid))
        return QVector3D();

    const FixtureRigProps &rp = m_rigProps.value(fid, FixtureRigProps());
    const Truss *t = (rp.trussId != Truss::invalidId()) ? m_trusses.value(rp.trussId, nullptr) : nullptr;
    if (t != nullptr)
        return t->positionAt(rp.trussOffset);

    // Free-placed: stored X/Y are in MILLIMETRES (see setFixturePosition callers,
    // which pass raw mm), but this function must return METRES to match the truss
    // branch above and the target/platform world coordinates that callers (aim
    // solver, effect engine, Aim palette) subtract it from. Convert X/Y; Z is
    // already in metres (always 0 today).
    const QVector3D p = m_fixtureItems[fid].m_baseItem.m_position;
    return QVector3D(p.x() / 1000.0f, p.y() / 1000.0f, p.z());
}

/*********************************************************************
 * Load & Save
 *********************************************************************/

bool MonitorProperties::loadXML(QXmlStreamReader &root, const Doc *mainDocument)
{
    if (root.name() != KXMLQLCMonitorProperties)
    {
        qWarning() << Q_FUNC_INFO << "Monitor node not found";
        return false;
    }

    QXmlStreamAttributes attrs = root.attributes();

    if (attrs.hasAttribute(KXMLQLCMonitorDisplay) == false)
    {
        qWarning() << Q_FUNC_INFO << "Cannot determine Monitor display mode !";
        return false;
    }

    setDisplayMode(DisplayMode(attrs.value(KXMLQLCMonitorDisplay).toString().toInt()));
    if (attrs.hasAttribute(KXMLQLCMonitorShowLabels))
    {
        if (attrs.value(KXMLQLCMonitorShowLabels).toString() == "1")
            setLabelsVisible(true);
        else
            setLabelsVisible(false);
    }

    if (attrs.hasAttribute(KXMLQLCMonitorLayoutLocked))
        setLayoutLocked(attrs.value(KXMLQLCMonitorLayoutLocked).toString() == "1");

    if (attrs.hasAttribute(KXMLQLCMonitorSnapDivisions))
        setSnapDivisions(attrs.value(KXMLQLCMonitorSnapDivisions).toString().toInt());

    if (attrs.hasAttribute(KXMLQLCMonitorAimSubjectHeight))
        setAimSubjectHeight(attrs.value(KXMLQLCMonitorAimSubjectHeight).toString().toFloat());

    while (root.readNextStartElement())
    {
        QXmlStreamAttributes tAttrs = root.attributes();
        if (root.name() == KXMLQLCMonitorFont)
        {
            QFont fn;
            fn.fromString(root.readElementText());
            setFont(fn);
        }
        else if (root.name() == KXMLQLCMonitorChannels)
            setChannelStyle(ChannelStyle(root.readElementText().toInt()));
        else if (root.name() == KXMLQLCMonitorValues)
            setValueStyle(ValueStyle(root.readElementText().toInt()));
        else if (root.name() == KXMLQLCMonitorCommonBackground)
            setCommonBackgroundImage(mainDocument->denormalizeComponentPath(root.readElementText()));
        else if (root.name() == KXMLQLCMonitorBgColor)
            setCommonBackgroundColor(QColor(root.readElementText()));
        else if (root.name() == KXMLQLCMonitorCustomBgItem)
        {
            if (tAttrs.hasAttribute(KXMLQLCMonitorItemID))
            {
                quint32 fid = tAttrs.value(KXMLQLCMonitorItemID).toString().toUInt();
                setCustomBackgroundItem(fid, mainDocument->denormalizeComponentPath(root.readElementText()));
            }
        }
        else if (root.name() == KXMLQLCMonitorGrid)
        {
            int w = 5, h = 3, d = 5;
            if (tAttrs.hasAttribute(KXMLQLCMonitorGridWidth))
                w = tAttrs.value(KXMLQLCMonitorGridWidth).toString().toInt();
            if (tAttrs.hasAttribute(KXMLQLCMonitorGridHeight))
                h = tAttrs.value(KXMLQLCMonitorGridHeight).toString().toInt();
            if (tAttrs.hasAttribute(KXMLQLCMonitorGridDepth))
                d = tAttrs.value(KXMLQLCMonitorGridDepth).toString().toInt();
            else
                d = h; // backward compatibility

            if (tAttrs.hasAttribute(KXMLQLCMonitorGridUnits))
                setGridUnits(GridUnits(tAttrs.value(KXMLQLCMonitorGridUnits).toString().toInt()));

            if (tAttrs.hasAttribute(KXMLQLCMonitorGridSubdiv))
                setGridSubdivisions(tAttrs.value(KXMLQLCMonitorGridSubdiv).toString().toInt());

            if (tAttrs.hasAttribute(KXMLQLCMonitorPointOfView))
                setPointOfView(PointOfView(tAttrs.value(KXMLQLCMonitorPointOfView).toString().toInt()));
            else
                setPointOfView(Undefined);

            setGridSize(QVector3D(w, h, d));
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCMonitorStageItem)
        {
            setStageType(StageType(root.readElementText().toInt()));
        }
        else if (root.name() == KXMLQLCMonitorFixtureItem)
        {
            // Fixture ID is mandatory. Skip the whole entry if not found.
            if (tAttrs.hasAttribute(KXMLQLCMonitorItemID) == false)
            {
                root.skipCurrentElement();
                continue;
            }

            PreviewItem item;
            quint32 fid = tAttrs.value(KXMLQLCMonitorItemID).toString().toUInt();
            quint16 headIndex = 0;
            quint16 linkedIndex = 0;
            QVector3D pos(0, 0, 0);
            QVector3D rot(0, 0, 0);

            item.m_flags = 0;
            item.m_zoom = 0;

            if (tAttrs.hasAttribute(KXMLQLCMonitorFixtureHeadIndex))
                headIndex = tAttrs.value(KXMLQLCMonitorFixtureHeadIndex).toString().toUInt();

            if (tAttrs.hasAttribute(KXMLQLCMonitorFixtureLinkedIndex))
            {
                linkedIndex = tAttrs.value(KXMLQLCMonitorFixtureLinkedIndex).toString().toUInt();

                if (tAttrs.hasAttribute(KXMLQLCMonitorItemName))
                    item.m_name = tAttrs.value(KXMLQLCMonitorItemName).toString();
            }

            if (tAttrs.hasAttribute(KXMLQLCMonitorItemXPosition))
                pos.setX(tAttrs.value(KXMLQLCMonitorItemXPosition).toString().toDouble());
            if (tAttrs.hasAttribute(KXMLQLCMonitorItemYPosition))
                pos.setY(tAttrs.value(KXMLQLCMonitorItemYPosition).toString().toDouble());
            if (tAttrs.hasAttribute(KXMLQLCMonitorItemZPosition))
                pos.setZ(tAttrs.value(KXMLQLCMonitorItemZPosition).toString().toDouble());
            item.m_position = pos;

            if (tAttrs.hasAttribute(KXMLQLCMonitorFixtureRotation)) // check legacy first
            {
                rot.setY(tAttrs.value(KXMLQLCMonitorFixtureRotation).toString().toDouble());
            }
            else
            {
                if (tAttrs.hasAttribute(KXMLQLCMonitorItemXRotation))
                    rot.setX(tAttrs.value(KXMLQLCMonitorItemXRotation).toString().toDouble());
                if (tAttrs.hasAttribute(KXMLQLCMonitorItemYRotation))
                    rot.setY(tAttrs.value(KXMLQLCMonitorItemYRotation).toString().toDouble());
                if (tAttrs.hasAttribute(KXMLQLCMonitorItemZRotation))
                    rot.setZ(tAttrs.value(KXMLQLCMonitorItemZRotation).toString().toDouble());
            }
            item.m_rotation = rot;

            if (tAttrs.hasAttribute(KXMLQLCMonitorFixtureGelColor))
                item.m_color = QColor(tAttrs.value(KXMLQLCMonitorFixtureGelColor).toString());

            if (tAttrs.hasAttribute(KXMLQLCMonitorFixtureFixedZoom))
                item.m_zoom = tAttrs.value(KXMLQLCMonitorFixtureFixedZoom).toString().toInt();

            if (tAttrs.hasAttribute(KXMLQLCMonitorFixtureHiddenFlag))
                item.m_flags |= HiddenFlag;
            if (tAttrs.hasAttribute(KXMLQLCMonitorFixtureInvPanFlag))
                item.m_flags |= InvertedPanFlag;
            if (tAttrs.hasAttribute(KXMLQLCMonitorFixtureInvTiltFlag))
                item.m_flags |= InvertedTiltFlag;

            setFixtureItem(fid, headIndex, linkedIndex, item);
            root.skipCurrentElement();

        }
        else if (root.name() == KXMLQLCMonitorMeshItem)
        {
            // Item ID is mandatory. Skip the whole entry if not found.
            if (tAttrs.hasAttribute(KXMLQLCMonitorItemID) == false)
            {
                root.skipCurrentElement();
                continue;
            }

            PreviewItem item;
            quint32 itemID = tAttrs.value(KXMLQLCMonitorItemID).toString().toUInt();
            QVector3D pos(0, 0, 0);
            QVector3D rot(0, 0, 0);
            QVector3D scale(1.0, 1.0, 1.0);

            item.m_flags = 0;

            if (tAttrs.hasAttribute(KXMLQLCMonitorItemXPosition))
                pos.setX(tAttrs.value(KXMLQLCMonitorItemXPosition).toString().toDouble());
            if (tAttrs.hasAttribute(KXMLQLCMonitorItemYPosition))
                pos.setY(tAttrs.value(KXMLQLCMonitorItemYPosition).toString().toDouble());
            if (tAttrs.hasAttribute(KXMLQLCMonitorItemZPosition))
                pos.setZ(tAttrs.value(KXMLQLCMonitorItemZPosition).toString().toDouble());
            item.m_position = pos;

            if (tAttrs.hasAttribute(KXMLQLCMonitorItemXRotation))
                rot.setX(tAttrs.value(KXMLQLCMonitorItemXRotation).toString().toDouble());
            if (tAttrs.hasAttribute(KXMLQLCMonitorItemYRotation))
                rot.setY(tAttrs.value(KXMLQLCMonitorItemYRotation).toString().toDouble());
            if (tAttrs.hasAttribute(KXMLQLCMonitorItemZRotation))
                rot.setZ(tAttrs.value(KXMLQLCMonitorItemZRotation).toString().toDouble());
            item.m_rotation = rot;

            if (tAttrs.hasAttribute(KXMLQLCMonitorItemXScale))
                scale.setX(tAttrs.value(KXMLQLCMonitorItemXScale).toString().toDouble());
            if (tAttrs.hasAttribute(KXMLQLCMonitorItemYScale))
                scale.setY(tAttrs.value(KXMLQLCMonitorItemYScale).toString().toDouble());
            if (tAttrs.hasAttribute(KXMLQLCMonitorItemZScale))
                scale.setZ(tAttrs.value(KXMLQLCMonitorItemZScale).toString().toDouble());
            item.m_scale = scale;

            if (tAttrs.hasAttribute(KXMLQLCMonitorFixtureHiddenFlag))
                item.m_flags |= HiddenFlag;

            if (tAttrs.hasAttribute(KXMLQLCMonitorItemRes))
                item.m_resource = tAttrs.value(KXMLQLCMonitorItemRes).toString();

            if (tAttrs.hasAttribute(KXMLQLCMonitorItemName))
                item.m_name = tAttrs.value(KXMLQLCMonitorItemName).toString();

            m_genericItems[itemID] = item;
            root.skipCurrentElement();
        }
        else if (root.name() == QStringLiteral("Truss"))
        {
            Truss *t = new Truss(nextTrussId(), this);
            if (t->loadXML(root))
                m_trusses.insert(t->id(), t);
            else
                delete t;
        }
        else if (root.name() == QStringLiteral("StagePlatform"))
        {
            StagePlatform *p = new StagePlatform(nextPlatformId(), this);
            // Ids now come from the file, so a malformed/hand-edited workspace
            // can repeat one. insert() would overwrite and leak the first.
            if (p->loadXML(root) && !m_platforms.contains(p->id()))
            {
                m_platforms.insert(p->id(), p);
            }
            else
            {
                qWarning() << "Discarding stage platform with missing or duplicate id" << p->id();
                delete p;
            }
        }
        else if (root.name() == QStringLiteral("StageTarget"))
        {
            StageTarget *t = new StageTarget(nextStageTargetId(), this);
            if (t->loadXML(root) && !m_stageTargets.contains(t->id()))
            {
                m_stageTargets.insert(t->id(), t);
            }
            else
            {
                qWarning() << "Discarding stage target with missing or duplicate id" << t->id();
                delete t;
            }
        }
        else if (root.name() == QStringLiteral("FixtureRig"))
        {
            QXmlStreamAttributes a = root.attributes();
            quint32 fid = a.value("FID").toUInt();
            FixtureRigProps rp;
            // No Truss attribute means free-placed, which is what the default
            // (Truss::invalidId()) encodes. toUInt() on a missing attribute
            // returns 0 — a VALID truss id — which would make the fixture read
            // as hung on truss 0 and flip the tilt sign in AimSolver.
            if (a.hasAttribute("Truss"))
                rp.trussId = a.value("Truss").toUInt();
            rp.trussOffset    = a.value("Offset").toFloat();
            rp.mountingType   = Truss::stringToMounting(a.value("Mounting").toString());
            rp.panZeroDir     = a.value("PanZero").toFloat();
            rp.panOffsetDeg   = a.value("PanOfs").toFloat();
            rp.tiltOffsetDeg  = a.value("TiltOfs").toFloat();
            rp.panInvert      = a.value("PanInv").toString() == QStringLiteral("1");
            rp.tiltInvert     = a.value("TiltInv").toString() == QStringLiteral("1");
            m_rigProps[fid] = rp;
            root.skipCurrentElement();
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown MonitorProperties tag:" << root.name();
            root.skipCurrentElement();
        }
    }
    return true;
}

bool MonitorProperties::saveXML(QXmlStreamWriter *doc, const Doc *mainDocument) const
{
    Q_ASSERT(doc != NULL);

    /* Create the master Monitor node */
    doc->writeStartElement(KXMLQLCMonitorProperties);
    doc->writeAttribute(KXMLQLCMonitorDisplay, QString::number(displayMode()));
    doc->writeAttribute(KXMLQLCMonitorShowLabels, QString::number(labelsVisible()));
    if (layoutLocked())
        doc->writeAttribute(KXMLQLCMonitorLayoutLocked, QString::number(1));
    if (snapDivisions() > 0)
        doc->writeAttribute(KXMLQLCMonitorSnapDivisions, QString::number(snapDivisions()));
        doc->writeAttribute(KXMLQLCMonitorAimSubjectHeight, QString::number(aimSubjectHeight()));

    /* Font */
    doc->writeTextElement(KXMLQLCMonitorFont, font().toString());
    /* Channels style */
    doc->writeTextElement(KXMLQLCMonitorChannels, QString::number(channelStyle()));
    /* Values style */
    doc->writeTextElement(KXMLQLCMonitorValues, QString::number(valueStyle()));

    /* Background */
    if (commonBackgroundImage().isEmpty() == false)
    {
        doc->writeTextElement(KXMLQLCMonitorCommonBackground,
                              mainDocument->normalizeComponentPath(commonBackgroundImage()));
    }
    if (commonBackgroundColor().isValid())
        doc->writeTextElement(KXMLQLCMonitorBgColor, commonBackgroundColor().name());
    else if (customBackgroundList().isEmpty() == false)
    {
        QMapIterator <quint32, QString> it(customBackgroundList());
        while (it.hasNext() == true)
        {
            it.next();
            doc->writeStartElement(KXMLQLCMonitorCustomBgItem);
            quint32 fid = it.key();
            doc->writeAttribute(KXMLQLCMonitorItemID, QString::number(fid));
            doc->writeCharacters(mainDocument->normalizeComponentPath(it.value()));
            doc->writeEndElement();
        }
    }

    doc->writeStartElement(KXMLQLCMonitorGrid);
    doc->writeAttribute(KXMLQLCMonitorGridWidth, QString::number(gridSize().x()));
    doc->writeAttribute(KXMLQLCMonitorGridHeight, QString::number(gridSize().y()));
    doc->writeAttribute(KXMLQLCMonitorGridDepth, QString::number(gridSize().z()));
    doc->writeAttribute(KXMLQLCMonitorGridUnits, QString::number(gridUnits()));
    if (gridSubdivisions() > 1)
        doc->writeAttribute(KXMLQLCMonitorGridSubdiv, QString::number(gridSubdivisions()));
    if (m_pointOfView != Undefined)
        doc->writeAttribute(KXMLQLCMonitorPointOfView, QString::number(pointOfView()));

    doc->writeEndElement();

#ifdef QMLUI
    doc->writeTextElement(KXMLQLCMonitorStageItem, QString::number(stageType()));
#endif

    // ***********************************************************
    // *                write fixtures information               *
    // ***********************************************************

    foreach (quint32 fid, fixtureItemsID())
    {
        foreach (quint32 subID, fixtureIDList(fid))
        {
            quint16 headIndex = fixtureHeadIndex(subID);
            quint16 linkedIndex = fixtureLinkedIndex(subID);
            PreviewItem item = fixtureItem(fid, headIndex, linkedIndex);

            doc->writeStartElement(KXMLQLCMonitorFixtureItem);
            doc->writeAttribute(KXMLQLCMonitorItemID, QString::number(fid));

            if (headIndex)
                doc->writeAttribute(KXMLQLCMonitorFixtureHeadIndex, QString::number(headIndex));

            if (linkedIndex)
            {
                doc->writeAttribute(KXMLQLCMonitorFixtureLinkedIndex, QString::number(linkedIndex));
                if (item.m_name.isEmpty() == false)
                    doc->writeAttribute(KXMLQLCMonitorItemName, item.m_name);
            }

            // write flags, if present
            if (item.m_flags & HiddenFlag)
                doc->writeAttribute(KXMLQLCMonitorFixtureHiddenFlag, KXMLQLCTrue);
            if (item.m_flags & InvertedPanFlag)
                doc->writeAttribute(KXMLQLCMonitorFixtureInvPanFlag, KXMLQLCTrue);
            if (item.m_flags & InvertedTiltFlag)
                doc->writeAttribute(KXMLQLCMonitorFixtureInvTiltFlag, KXMLQLCTrue);

            // always write position
            doc->writeAttribute(KXMLQLCMonitorItemXPosition, QString::number(item.m_position.x()));
            doc->writeAttribute(KXMLQLCMonitorItemYPosition, QString::number(item.m_position.y()));

#ifdef QMLUI
            doc->writeAttribute(KXMLQLCMonitorItemZPosition, QString::number(item.m_position.z()));

            // write rotation, if set
            if (item.m_rotation.x() != 0)
                doc->writeAttribute(KXMLQLCMonitorItemXRotation, QString::number(item.m_rotation.x()));
            if (item.m_rotation.y() != 0)
                doc->writeAttribute(KXMLQLCMonitorItemYRotation, QString::number(item.m_rotation.y()));
            if (item.m_rotation.z() != 0)
                doc->writeAttribute(KXMLQLCMonitorItemZRotation, QString::number(item.m_rotation.z()));
#else
            if (item.m_rotation != QVector3D(0, 0, 0))
                doc->writeAttribute(KXMLQLCMonitorFixtureRotation, QString::number(item.m_rotation.y()));
#endif
            if (item.m_color.isValid())
                doc->writeAttribute(KXMLQLCMonitorFixtureGelColor, item.m_color.name());

            if (item.m_zoom > 0)
                doc->writeAttribute(KXMLQLCMonitorFixtureFixedZoom, QString::number(item.m_zoom));

            doc->writeEndElement();
        }
    }
#ifdef QMLUI
    QDir dir = QDir::cleanPath(QLCFile::systemDirectory(MESHESDIR).path());
    QString meshDirAbsPath = dir.absolutePath() + QDir::separator();
#endif

    // ***********************************************************
    // *             write generic items information             *
    // ***********************************************************
    QMapIterator<quint32, PreviewItem> it(m_genericItems);
    while (it.hasNext())
    {
        it.next();
        quint32 itemID = it.key();
        PreviewItem item = it.value();

        doc->writeStartElement(KXMLQLCMonitorMeshItem);
        doc->writeAttribute(KXMLQLCMonitorItemID, QString::number(itemID));

        // write flags, if present
        if (item.m_flags & HiddenFlag)
            doc->writeAttribute(KXMLQLCMonitorFixtureHiddenFlag, KXMLQLCTrue);

        // always write position
        doc->writeAttribute(KXMLQLCMonitorItemXPosition, QString::number(item.m_position.x()));
        doc->writeAttribute(KXMLQLCMonitorItemYPosition, QString::number(item.m_position.y()));
        doc->writeAttribute(KXMLQLCMonitorItemZPosition, QString::number(item.m_position.z()));

        // write rotation, if set
        if (item.m_rotation.x() != 0)
            doc->writeAttribute(KXMLQLCMonitorItemXRotation, QString::number(item.m_rotation.x()));
        if (item.m_rotation.y() != 0)
            doc->writeAttribute(KXMLQLCMonitorItemYRotation, QString::number(item.m_rotation.y()));
        if (item.m_rotation.z() != 0)
            doc->writeAttribute(KXMLQLCMonitorItemZRotation, QString::number(item.m_rotation.z()));

        // write scale, if set
        if (item.m_scale.x() != 1.0)
            doc->writeAttribute(KXMLQLCMonitorItemXScale, QString::number(item.m_scale.x()));
        if (item.m_scale.y() != 1.0)
            doc->writeAttribute(KXMLQLCMonitorItemYScale, QString::number(item.m_scale.y()));
        if (item.m_scale.z() != 1.0)
            doc->writeAttribute(KXMLQLCMonitorItemZScale, QString::number(item.m_scale.z()));

        if (item.m_resource.isEmpty() == false)
        {
            // perform normalization depending on the mesh location
            // (mesh folder, project path, absolute path)
            QFileInfo res(item.m_resource);

            if (res.isRelative())
            {
                doc->writeAttribute(KXMLQLCMonitorItemRes, item.m_resource);
            }
#ifdef QMLUI
            else if (item.m_resource.startsWith(meshDirAbsPath))
            {
                item.m_resource.remove(meshDirAbsPath);
                doc->writeAttribute(KXMLQLCMonitorItemRes, item.m_resource);
            }
#endif
            else
            {
                doc->writeAttribute(KXMLQLCMonitorItemRes, mainDocument->normalizeComponentPath(item.m_resource));
            }
        }

        if (item.m_name.isEmpty() == false)
            doc->writeAttribute(KXMLQLCMonitorItemName, item.m_name);

        doc->writeEndElement();
    }

    // Trusses
    foreach (const Truss *t, m_trusses)
        t->saveXML(doc);

    // Stage platforms
    foreach (const StagePlatform *p, m_platforms)
        p->saveXML(doc);

    // Stage targets
    foreach (const StageTarget *t, m_stageTargets)
        t->saveXML(doc);

    // Fixture rig props (only write entries that differ from defaults)
    for (auto it = m_rigProps.constBegin(); it != m_rigProps.constEnd(); ++it)
    {
        const FixtureRigProps &rp = it.value();
        doc->writeStartElement(QStringLiteral("FixtureRig"));
        doc->writeAttribute(QStringLiteral("FID"),     QString::number(it.key()));
        doc->writeAttribute(QStringLiteral("Truss"),   QString::number(rp.trussId));
        doc->writeAttribute(QStringLiteral("Offset"),  QString::number(double(rp.trussOffset), 'f', 3));
        doc->writeAttribute(QStringLiteral("Mounting"), Truss::mountingToString(rp.mountingType));
        doc->writeAttribute(QStringLiteral("PanZero"),  QString::number(double(rp.panZeroDir),     'f', 1));
        if (rp.panOffsetDeg  != 0.0f)
            doc->writeAttribute(QStringLiteral("PanOfs"),  QString::number(double(rp.panOffsetDeg),  'f', 2));
        if (rp.tiltOffsetDeg != 0.0f)
            doc->writeAttribute(QStringLiteral("TiltOfs"), QString::number(double(rp.tiltOffsetDeg), 'f', 2));
        if (rp.panInvert)
            doc->writeAttribute(QStringLiteral("PanInv"),  QStringLiteral("1"));
        if (rp.tiltInvert)
            doc->writeAttribute(QStringLiteral("TiltInv"), QStringLiteral("1"));
        doc->writeEndElement();
    }

    doc->writeEndElement();

    return true;
}
