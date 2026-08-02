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
#include <QtMath>

#include <algorithm>
#include <cmath>

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
#define KXMLQLCMonitorOriginX       QStringLiteral("OriginX")
#define KXMLQLCMonitorOriginY       QStringLiteral("OriginY")
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

#define KXMLQLCMonitorFixtureLayerId        QStringLiteral("LayerId")
#define KXMLQLCMonitorFixtureGroupId        QStringLiteral("GroupId")
#define KXMLQLCMonitorFixtureVertStrip      QStringLiteral("VertStrip")
#define KXMLQLCMonitorFixtureHiddenFlag     QStringLiteral("Hidden")
#define KXMLQLCMonitorFixtureInvPanFlag     QStringLiteral("InvertedPan")
#define KXMLQLCMonitorFixtureInvTiltFlag    QStringLiteral("InvertedTilt")

#define KXMLQLCMonitorLayer          QStringLiteral("MonitorLayer")
#define KXMLQLCMonitorLayerID        QStringLiteral("ID")
#define KXMLQLCMonitorLayerName      QStringLiteral("Name")
#define KXMLQLCMonitorLayerVisible   QStringLiteral("Visible")
#define KXMLQLCMonitorLayerLocked    QStringLiteral("Locked")
#define KXMLQLCMonitorLayerOrder     QStringLiteral("Order")
#define KXMLQLCMonitorActiveLayer    QStringLiteral("ActiveLayer")

#define KXMLQLCMonitorGroup          QStringLiteral("MonitorGroup")
#define KXMLQLCMonitorGroupID        QStringLiteral("ID")
#define KXMLQLCMonitorGroupName      QStringLiteral("Name")
#define KXMLQLCMonitorGroupLayer     QStringLiteral("LayerId")
#define KXMLQLCMonitorGroupParent    QStringLiteral("ParentId")

#define GRID_DEFAULT_WIDTH  5
#define GRID_DEFAULT_HEIGHT 3
#define GRID_DEFAULT_DEPTH  5

const quint32 MonitorProperties::defaultLayerId;

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
    , m_showLabels(true)
{
    ensureDefaultLayer();
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
    m_stageOrigin = QPointF(0, 0);
    m_showLabels = true;
    m_fixtureItems.clear();
    m_genericItems.clear();
    m_commonBackgroundImage = QString();
    qDeleteAll(m_trusses);
    m_trusses.clear();
    qDeleteAll(m_platforms);
    m_platforms.clear();
    qDeleteAll(m_pipes);
    m_pipes.clear();
    qDeleteAll(m_stands);
    m_stands.clear();
    qDeleteAll(m_towers);
    m_towers.clear();
    qDeleteAll(m_stageTargets);
    m_stageTargets.clear();
    m_rigProps.clear();
    m_layers.clear();
    m_activeLayerId = defaultLayerId;
    ensureDefaultLayer();
    m_groups.clear();
    m_images.clear();
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

void MonitorProperties::setFixtureLayer(quint32 fid, quint32 layerId)
{
    m_fixtureItems[fid].m_baseItem.m_layerId = layerId;
}

quint32 MonitorProperties::fixtureLayer(quint32 fid) const
{
    if (!m_fixtureItems.contains(fid))
        return defaultLayerId;
    return m_fixtureItems[fid].m_baseItem.m_layerId;
}

void MonitorProperties::setFixtureGroup(quint32 fid, quint32 groupId)
{
    m_fixtureItems[fid].m_baseItem.m_groupId = groupId;
}

quint32 MonitorProperties::fixtureGroup(quint32 fid) const
{
    if (!m_fixtureItems.contains(fid))
        return 0;
    return m_fixtureItems[fid].m_baseItem.m_groupId;
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
 * Layers
 *********************************************************************/

void MonitorProperties::ensureDefaultLayer()
{
    if (m_layers.contains(defaultLayerId))
        return;

    MonitorLayer def;
    def.id      = defaultLayerId;
    def.name    = QStringLiteral("Default");
    def.visible = true;
    def.locked  = false;
    def.order   = 0;
    m_layers.insert(def.id, def);
}

QList<MonitorProperties::MonitorLayer> MonitorProperties::layers() const
{
    QList<MonitorLayer> list = m_layers.values();
    std::sort(list.begin(), list.end(), [](const MonitorLayer &a, const MonitorLayer &b) {
        if (a.order != b.order)
            return a.order < b.order;
        return a.id < b.id;
    });
    return list;
}

MonitorProperties::MonitorLayer MonitorProperties::layer(quint32 id) const
{
    // Unknown ids resolve to Default so items on a deleted layer still render.
    return m_layers.value(id, m_layers.value(defaultLayerId));
}

quint32 MonitorProperties::nextLayerId() const
{
    quint32 id = 1; // 0 is reserved for Default
    while (m_layers.contains(id))
        ++id;
    return id;
}

quint32 MonitorProperties::addLayer(const QString &name)
{
    MonitorLayer l;
    l.id      = nextLayerId();
    l.name    = name;
    l.visible = true;
    l.locked  = false;
    // Stack the new layer on top of the current highest order.
    int maxOrder = -1;
    foreach (const MonitorLayer &m, m_layers)
        maxOrder = qMax(maxOrder, m.order);
    l.order = maxOrder + 1;
    m_layers.insert(l.id, l);
    return l.id;
}

void MonitorProperties::removeLayer(quint32 id)
{
    if (id == defaultLayerId)
        return; // Default is permanent
    m_layers.remove(id);
    if (m_activeLayerId == id)
        m_activeLayerId = defaultLayerId;
}

void MonitorProperties::setLayerName(quint32 id, const QString &name)
{
    if (m_layers.contains(id))
        m_layers[id].name = name;
}

void MonitorProperties::setLayerVisible(quint32 id, bool visible)
{
    if (m_layers.contains(id))
        m_layers[id].visible = visible;
}

void MonitorProperties::setLayerLocked(quint32 id, bool locked)
{
    if (m_layers.contains(id))
        m_layers[id].locked = locked;
}

void MonitorProperties::setLayerLabels(quint32 id, bool labels)
{
    if (m_layers.contains(id))
        m_layers[id].labels = labels;
}

void MonitorProperties::setLayerOrder(quint32 id, int order)
{
    if (m_layers.contains(id))
        m_layers[id].order = order;
}

/*********************************************************************
 * Groups
 *********************************************************************/

quint32 MonitorProperties::nextGroupId() const
{
    quint32 id = 1; // 0 means "ungrouped"
    while (m_groups.contains(id))
        ++id;
    return id;
}

void MonitorProperties::createGroup(quint32 id, const QString &name,
                                    quint32 layerId, quint32 parentGroupId)
{
    if (id == 0)
        return;
    MonitorGroup g;
    g.id            = id;
    g.name          = name;
    g.layerId       = layerId;
    g.parentGroupId = parentGroupId;
    m_groups.insert(id, g);
}

void MonitorProperties::ensureGroup(quint32 id, quint32 layerId)
{
    if (id == 0 || m_groups.contains(id))
        return;
    createGroup(id, QStringLiteral("Group %1").arg(id), layerId, 0);
}

void MonitorProperties::removeGroup(quint32 id)
{
    m_groups.remove(id);
}

void MonitorProperties::setGroupName(quint32 id, const QString &name)
{
    if (m_groups.contains(id))
        m_groups[id].name = name;
}

void MonitorProperties::setGroupLayer(quint32 id, quint32 layerId)
{
    if (m_groups.contains(id))
        m_groups[id].layerId = layerId;
}

void MonitorProperties::setGroupParent(quint32 id, quint32 parentGroupId)
{
    if (m_groups.contains(id))
        m_groups[id].parentGroupId = parentGroupId;
}

void MonitorProperties::setGroupAnchor(quint32 id, const QString &kind, quint32 anchorId)
{
    if (m_groups.contains(id))
    {
        m_groups[id].anchorKind = kind;
        m_groups[id].anchorId   = anchorId;
    }
}

QList<MonitorProperties::MonitorGroup> MonitorProperties::childGroups(quint32 parentGroupId) const
{
    QList<MonitorGroup> out;
    foreach (const MonitorGroup &g, m_groups)
        if (g.parentGroupId == parentGroupId)
            out << g;
    return out;
}

void MonitorProperties::setGroupLocked(quint32 gid, bool locked)
{
    if (m_groups.contains(gid))
        m_groups[gid].locked = locked;
}

bool MonitorProperties::groupChainLocked(quint32 gid) const
{
    int guard = 0;   // cycle safety
    while (gid != 0 && m_groups.contains(gid) && guard++ < 64)
    {
        const MonitorGroup &g = m_groups[gid];
        if (g.locked)
            return true;
        gid = g.parentGroupId;
    }
    return false;
}

void MonitorProperties::setGroupHasFrame(quint32 id, bool on)
{
    if (m_groups.contains(id))
        m_groups[id].hasFrame = on;
}

void MonitorProperties::setGroupFrame(quint32 id, const QVector3D &origin, float rotationDeg)
{
    if (m_groups.contains(id))
    {
        m_groups[id].origin   = origin;
        m_groups[id].rotation = rotationDeg;
    }
}

void MonitorProperties::setGroupOrigin(quint32 id, const QVector3D &origin)
{
    if (m_groups.contains(id))
        m_groups[id].origin = origin;
}

void MonitorProperties::setGroupRotation(quint32 id, float rotationDeg)
{
    if (m_groups.contains(id))
        m_groups[id].rotation = rotationDeg;
}

void MonitorProperties::setGroupBinding(quint32 id, quint32 fxGroupId, float pitchX, float pitchY)
{
    if (m_groups.contains(id))
    {
        m_groups[id].boundFxGroup = fxGroupId;
        m_groups[id].pitchX = pitchX;
        m_groups[id].pitchY = pitchY;
    }
}

quint32 MonitorProperties::fixtureFrameGroup(quint32 fid) const
{
    if (!m_fixtureItems.contains(fid))
        return 0;
    quint32 gid = m_fixtureItems[fid].m_baseItem.m_groupId;
    int guard = 0;   // cycle safety
    while (gid != 0 && m_groups.contains(gid) && guard++ < 64)
    {
        const MonitorGroup &g = m_groups[gid];
        if (g.hasFrame)
            return gid;
        gid = g.parentGroupId;
    }
    return 0;
}

QVector3D MonitorProperties::groupLocalToWorld(quint32 groupId, const QVector3D &local) const
{
    if (!m_groups.contains(groupId))
        return local;
    const MonitorGroup &g = m_groups[groupId];
    const float r = qDegreesToRadians(g.rotation);
    const float c = qCos(r), s = qSin(r);
    return QVector3D(g.origin.x() + local.x() * c - local.y() * s,
                     g.origin.y() + local.x() * s + local.y() * c,
                     g.origin.z() + local.z());
}

QVector3D MonitorProperties::worldToGroupLocal(quint32 groupId, const QVector3D &world) const
{
    if (!m_groups.contains(groupId))
        return world;
    const MonitorGroup &g = m_groups[groupId];
    const float r = qDegreesToRadians(g.rotation);
    const float c = qCos(r), s = qSin(r);
    const float dx = world.x() - g.origin.x();
    const float dy = world.y() - g.origin.y();
    return QVector3D(dx * c + dy * s,
                     -dx * s + dy * c,
                     world.z() - g.origin.z());
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

// Unit stage-space vector pointing OUT of a truss face. Stage convention here:
// +X = stage-right, +Y = downstage (toward audience), +Z = up.
static QVector3D barFaceVector(int face)
{
    switch (face)
    {
        case Truss::FaceTop:        return QVector3D(0, 0,  1);
        case Truss::FaceDownstage:  return QVector3D(0,  1, 0);
        case Truss::FaceUpstage:    return QVector3D(0, -1, 0);
        case Truss::FaceStageRight: return QVector3D( 1, 0, 0);
        case Truss::FaceStageLeft:  return QVector3D(-1, 0, 0);
        case Truss::FaceBottom:
        default:                    return QVector3D(0, 0, -1);
    }
}

void MonitorProperties::recomputeAnchoredFrames()
{
    // A studio group anchored to a platform is SLAVED to it: its local frame
    // origin tracks the platform's reference corner (upstage-left, at floor
    // level) with no rotation (platforms are axis-aligned). Members stored in
    // group-local metres therefore follow the platform when it moves/resizes —
    // and the group's Front plane IS the riser face. Call after any platform
    // move and once after load.
    for (auto it = m_groups.begin(); it != m_groups.end(); ++it)
    {
        MonitorGroup &g = it.value();
        if (!g.hasFrame || g.anchorKind != QStringLiteral("platform"))
            continue;
        const StagePlatform *pl = m_platforms.value(g.anchorId, nullptr);
        if (pl == nullptr)
            continue;
        g.origin   = QVector3D(pl->originX(), pl->originY(), 0.0f);
        g.rotation = 0.0f;
    }
}

void MonitorProperties::recomputeChildTrusses()
{
    // Derive each bar's full world geometry (origin/direction/type) from its
    // truss-LOCAL mount params: Along (parentOffset) · Face · Stand-off · Run.
    // A few passes let a bar-on-a-bar settle; the cap guards accidental cycles.
    for (int pass = 0; pass < 4; ++pass)
    {
        bool changed = false;
        foreach (Truss *t, m_trusses)
        {
            if (!t->isChildBar())
                continue;
            Truss *parent = m_trusses.value(t->parentTrussId(), nullptr);
            if (parent == nullptr || parent == t)
                continue;

            const QVector3D attach = parent->positionAt(t->parentOffset());
            const QVector3D faceN  = barFaceVector(t->barFace());
            const float half = parent->width() * 0.5f;
            const QVector3D base = attach + faceN * (half + t->barStandoff());

            QVector3D newOrigin;
            Truss::TrussType newType;
            QPointF newDir = parent->direction();

            if (t->barRun() == Truss::RunDrop)
            {
                // Hangs straight down from the attach face (child Vertical goes
                // downward — see Truss::positionAt).
                newType   = Truss::Vertical;
                newOrigin = base;
            }
            else
            {
                newType = Truss::Horizontal;
                if (t->barRun() == Truss::RunAcross)
                {
                    // A crossbar runs perpendicular to BOTH the truss axis and the
                    // mount face — i.e. across the face horizontally. On a tower's
                    // downstage face that's stage-left↔right (X), not toward the
                    // audience. cross = trussAxis × faceNormal.
                    const QVector3D trussAxis = (parent->type() == Truss::Vertical)
                        ? QVector3D(0, 0, 1)
                        : QVector3D(parent->direction().x(), parent->direction().y(), 0);
                    const QVector3D c = QVector3D::crossProduct(trussAxis, faceN);
                    if (!qFuzzyIsNull(c.x()) || !qFuzzyIsNull(c.y()))
                        newDir = QPointF(c.x(), c.y());
                    else
                        newDir = QPointF(-parent->direction().y(), parent->direction().x());
                }
                // else RunAlong → parallel to the parent (newDir already set).
                const double dl = std::hypot(newDir.x(), newDir.y());
                if (dl > 0.0) newDir /= dl;
                // Centre the bar on the attach point along its run, then apply the
                // cross-shift (slides the bar left/right so the truss meets it
                // off-centre).
                const QVector3D runVec(float(newDir.x()), float(newDir.y()), 0.0f);
                newOrigin = base - runVec * (t->length() * 0.5f - t->barCrossShift());
            }

            if (newOrigin != t->origin() || newType != t->type()
                || newDir != t->direction())
            {
                t->setOrigin(newOrigin);
                t->setType(newType);
                t->setDirection(newDir);
                changed = true;
            }
        }
        if (!changed)
            break;
    }
    // Stands first (they set a stand-mounted boom's base), then pipe anchors
    // (truss-hung + bars-on-pipes, which derive from a parent whose base the
    // above passes have already resolved).
    recomputeStandMounts();
    recomputePipeAnchors();
}

void MonitorProperties::recomputePipeAnchors()
{
    foreach (Pipe *b, m_pipes)
    {
        if (!b->isTrussHung())
            continue;
        Truss *t = m_trusses.value(b->parentTrussId(), nullptr);
        if (t == nullptr)
            continue;
        // Pipe TOP sits at the truss point; the base (bottom) hangs below it by
        // the pipe height. A truss-hung pipe has no stand.
        const QVector3D p = t->positionAt(b->trussOffset());
        b->setOriginX(p.x());
        b->setOriginY(p.y());
        b->setBaseZ(qMax(0.0f, p.z() - b->height()));
        b->setBaseRadius(0.0f);
    }

    // Bars/crossbars hung on another pipe (e.g. a horizontal crossbar on a
    // vertical boom). Separate pass so the parent's base — possibly itself
    // stand-mounted or truss-hung above — is already resolved.
    foreach (Pipe *b, m_pipes)
    {
        if (!b->isBarOnPipe())
            continue;
        Pipe *parent = m_pipes.value(b->parentPipeId(), nullptr);
        if (parent == nullptr || parent == b)
            continue;
        const QVector3D p = parent->positionAt(b->parentPipeOffset());
        b->setOriginX(p.x());
        b->setOriginY(p.y());
        b->setBaseZ(p.z());
        b->setBaseRadius(0.0f);   // the parent pipe carries it
    }
}

void MonitorProperties::removeTruss(quint32 id)
{
    // The group the truss anchored (its bound fixtures live here).
    quint32 gid = 0;
    if (Truss *t = m_trusses.value(id, nullptr))
        gid = t->groupId();

    // Un-assign any fixtures that were on this truss.
    for (auto it = m_rigProps.begin(); it != m_rigProps.end(); ++it)
        if (it->trussId == id)
            it->trussId = Truss::invalidId();

    // Detach any child bars hung on this truss — they become free trusses in
    // place (their last derived origin is kept).
    foreach (Truss *t, m_trusses)
        if (t->parentTrussId() == id)
        {
            t->setParentTrussId(Truss::invalidId());
            t->setParentOffset(0.0f);
            t->setBarStandoff(0.0f);
        }

    // Deleting the truss out from under its fixtures also DISASSOCIATES them
    // from its group — otherwise they'd be left orphaned in a phantom folder
    // whose anchor (the truss) no longer exists. Pull every member out and, if
    // that group was dedicated to this truss, drop the group itself.
    if (gid != 0)
    {
        for (auto it = m_fixtureItems.begin(); it != m_fixtureItems.end(); ++it)
            if (it->m_baseItem.m_groupId == gid)
                it->m_baseItem.m_groupId = 0;
        const MonitorGroup g = m_groups.value(gid);
        if (g.anchorKind == QStringLiteral("truss") && g.anchorId == id)
            m_groups.remove(gid);
    }

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
    // Un-mount any fixtures that were rigged on this riser (face or deck).
    for (auto it = m_rigProps.begin(); it != m_rigProps.end(); ++it)
    {
        if (it->riserPlatformId == id)
            it->riserPlatformId = FixtureRigProps::invalidPlatformId();
        if (it->deckPlatformId == id)
            it->deckPlatformId = FixtureRigProps::invalidPlatformId();
    }

    delete m_platforms.take(id);
}

quint32 MonitorProperties::nextPipeId() const
{
    quint32 id = 0;
    while (m_pipes.contains(id))
        ++id;
    return id;
}

Pipe *MonitorProperties::addPipe()
{
    quint32 id = nextPipeId();
    Pipe *b = new Pipe(id, this);
    m_pipes.insert(id, b);
    return b;
}

void MonitorProperties::removePipe(quint32 id)
{
    // Un-mount any fixtures that were rigged on this pipe.
    for (auto it = m_rigProps.begin(); it != m_rigProps.end(); ++it)
        if (it->pipeId == id)
            it->pipeId = UINT_MAX;

    // Detach any crossbars hung on this pipe — they stay put as free bars.
    foreach (Pipe *p, m_pipes)
        if (p->parentPipeId() == id)
            p->setParentPipeId(Pipe::invalidId());

    delete m_pipes.take(id);
}

quint32 MonitorProperties::nextStandId() const
{
    quint32 id = 0;
    while (m_stands.contains(id))
        ++id;
    return id;
}

Stand *MonitorProperties::addStand()
{
    quint32 id = nextStandId();
    Stand *s = new Stand(id, this);
    m_stands.insert(id, s);
    return s;
}

void MonitorProperties::removeStand(quint32 id)
{
    // Detach any pipes that stood on it (they revert to their own base).
    foreach (Pipe *p, m_pipes)
        if (p->standId() == id)
            p->setStandId(Stand::invalidId());
    // …and any trusses that stood on it (they stay put at their last origin).
    foreach (Truss *t, m_trusses)
        if (t->standId() == id)
            t->setStandId(Stand::invalidId());
    delete m_stands.take(id);
}

void MonitorProperties::recomputeStandMounts()
{
    foreach (Pipe *p, m_pipes)
    {
        if (!p->isStandMounted())
            continue;
        Stand *s = m_stands.value(p->standId(), nullptr);
        if (s == nullptr)
            continue;
        const QVector3D top = s->topPos();
        p->setOriginX(top.x());
        p->setOriginY(top.y());
        // Mount at the stand top by default, or anywhere up the post if the
        // pipe carries an explicit stand offset (a bar clamped along the height).
        const float z = p->hasStandOffset()
                            ? qBound(0.0f, p->standOffset(), s->height())
                            : top.z();
        p->setBaseZ(z);
        p->setBaseRadius(0.0f);   // the stand provides the base
    }

    // Trusses standing ON a stand: origin rides the stand top. The truss keeps
    // its own type/direction/length (a vertical truss rises from there; a
    // horizontal one lies at that height).
    foreach (Truss *t, m_trusses)
    {
        if (!t->isStandMounted())
            continue;
        Stand *s = m_stands.value(t->standId(), nullptr);
        if (s == nullptr)
            continue;
        t->setOrigin(s->topPos());
    }
}

quint32 MonitorProperties::nextTowerId() const
{
    quint32 id = 0;
    while (m_towers.contains(id))
        ++id;
    return id;
}

Tower *MonitorProperties::addTower()
{
    quint32 id = nextTowerId();
    Tower *t = new Tower(id, this);
    m_towers.insert(id, t);
    return t;
}

void MonitorProperties::removeTower(quint32 id)
{
    for (auto it = m_rigProps.begin(); it != m_rigProps.end(); ++it)
        if (it->towerId == id)
            it->towerId = UINT_MAX;
    delete m_towers.take(id);
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

float MonitorProperties::platformBaseZ(quint32 id) const
{
    const StagePlatform *b = m_platforms.value(id, nullptr);
    if (b == nullptr)
        return 0.0f;
    const float cx = b->originX() + b->width() * 0.5f;
    const float cy = b->originY() + b->depth() * 0.5f;
    float base = 0.0f;
    foreach (const StagePlatform *a, m_platforms)
    {
        if (a == b || a->height() >= b->height())   // must be a LOWER platform
            continue;
        if (a->containsPoint(cx, cy))               // this one sits within it
            base = qMax(base, a->height());
    }
    return base;
}

quint32 MonitorProperties::nextImageId() const
{
    quint32 id = 0;
    while (m_images.contains(id))
        ++id;
    return id;
}

quint32 MonitorProperties::addImage(const QString &source)
{
    MonitorImage img;
    img.id = nextImageId();
    img.source = source;
    img.name = QStringLiteral("Image %1").arg(img.id + 1);
    m_images.insert(img.id, img);
    return img.id;
}

void MonitorProperties::setImage(const MonitorImage &img)
{
    m_images.insert(img.id, img);
}

void MonitorProperties::removeImage(quint32 id)
{
    m_images.remove(id);
}

quint32 MonitorProperties::platformIdAt(float xMetres, float yMetres) const
{
    quint32 best = FixtureRigProps::invalidPlatformId();
    float bestH = -1.0f;
    foreach (const StagePlatform *p, m_platforms)
    {
        if (p->containsPoint(xMetres, yMetres) && p->height() > bestH)
        {
            bestH = p->height();
            best = p->id();
        }
    }
    return best;
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

    // Studio frame takes precedence: if the fixture sits under a frame-bearing
    // group, its world position is derived from that group's local frame
    // (origin + Rz(rotation) * groupLocal). See MonitorGroup::hasFrame.
    const quint32 frameGid = fixtureFrameGroup(fid);
    if (frameGid != 0)
    {
        const FixtureRigProps &grp = m_rigProps.value(fid, FixtureRigProps());
        return groupLocalToWorld(frameGid, grp.groupLocal);
    }

    const FixtureRigProps &rp = m_rigProps.value(fid, FixtureRigProps());
    const Truss *t = (rp.trussId != Truss::invalidId()) ? m_trusses.value(rp.trussId, nullptr) : nullptr;
    if (t != nullptr)
    {
        QVector3D p = t->positionAt(rp.trussOffset);
        // Offset Z by the mount side (elevation only). Half the truss thickness.
        const float half = t->width() * 0.5f;
        if (rp.trussMountSide == FixtureRigProps::TopMounted)
            p.setZ(p.z() + half);
        else if (rp.trussMountSide == FixtureRigProps::UnderHung)
            p.setZ(p.z() - half);
        // Centered: leave on the chord.
        // Horizontal cross-position: slide across the truss width, perpendicular
        // to the run in the horizontal plane (Left/Right chord vs centred).
        if (rp.trussCross != 0.0f)
        {
            if (t->type() == Truss::Vertical)
            {
                // A tower's run is vertical — "across" is stage left/right (X).
                p.setX(p.x() + rp.trussCross);
            }
            else
            {
                const QPointF d = t->direction();
                const double dl = std::hypot(d.x(), d.y());
                if (dl > 0.0)
                {
                    const float px = float(-d.y() / dl), py = float(d.x() / dl);
                    p.setX(p.x() + px * rp.trussCross);
                    p.setY(p.y() + py * rp.trussCross);
                }
            }
        }
        p.setZ(p.z() + rp.mountZOffset);   // fine height nudge (e.g. followspot on top)
        return p;
    }

    // Boom mount: rides a pipe pipe at pipeOffset up from the base, nudged to the
    // pipe surface in the facing direction. Derived so it follows the pipe.
    const Pipe *bm = (rp.pipeId != Pipe::invalidId()) ? m_pipes.value(rp.pipeId, nullptr) : nullptr;
    if (bm != nullptr)
    {
        QVector3D p = bm->positionAt(rp.pipeOffset);   // pipe centre at that height
        const double th = qDegreesToRadians(double(rp.pipeAngle));
        const float r = bm->diameter() * 0.5f;
        p.setX(p.x() + r * float(qSin(th)));   // 0° = downstage (-Y)
        p.setY(p.y() - r * float(qCos(th)));
        p.setZ(p.z() + rp.mountZOffset);
        return p;
    }

    // Tower-shelf mount: sits on a tower shelf at (towerU, towerV) in the
    // footprint, at the shelf height. Derived so it follows the tower.
    const Tower *tw = (rp.towerId != Tower::invalidId()) ? m_towers.value(rp.towerId, nullptr) : nullptr;
    if (tw != nullptr)
    {
        QVector3D p = tw->shelfPos(rp.towerShelf, rp.towerU, rp.towerV);
        p.setZ(p.z() + rp.mountZOffset);
        // Hung (base on top) = hangs UNDER the shelf; drop it a little below.
        if (rp.mountingType == Truss::TopHung)
            p.setZ(qMax(0.0f, p.z() - 0.15f));
        return p;
    }

    // Deck mount: standing on top of a platform. Keep the free XY, derive Z from
    // the platform's top height (+ offset) so it follows the platform.
    if (rp.onDeck())
    {
        const StagePlatform *pl = m_platforms.value(rp.deckPlatformId, nullptr);
        if (pl != nullptr)
        {
            const QVector3D p = m_fixtureItems[fid].m_baseItem.m_position;   // mm
            return QVector3D(p.x() / 1000.0f, p.y() / 1000.0f,
                             pl->height() + rp.deckHeightOffset);
        }
    }

    // Riser (platform) face mount: derive the world position from the platform
    // geometry so the fixture follows the riser. (metres)
    if (rp.onRiser())
    {
        const StagePlatform *pl = m_platforms.value(rp.riserPlatformId, nullptr);
        if (pl != nullptr)
        {
            if (rp.riserFace == FixtureRigProps::RiserTop)
                return QVector3D(pl->originX() + rp.riserU,
                                 pl->originY() + rp.riserV, pl->height());
            // Front (downstage) face: Y = the DOWNSTAGE edge (originY + depth —
            // originY is the upstage edge in this plan view), X across the
            // width, Z = up the face FROM the platform base (which may sit on a
            // lower platform, not the floor).
            return QVector3D(pl->originX() + rp.riserU,
                             pl->originY() + pl->depth(),
                             platformBaseZ(pl->id()) + rp.riserV);
        }
    }

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

    if (attrs.hasAttribute(KXMLQLCMonitorOriginX) || attrs.hasAttribute(KXMLQLCMonitorOriginY))
        setStageOrigin(QPointF(attrs.value(KXMLQLCMonitorOriginX).toString().toDouble(),
                               attrs.value(KXMLQLCMonitorOriginY).toString().toDouble()));

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
            if (tAttrs.hasAttribute(KXMLQLCMonitorFixtureVertStrip))
                item.m_flags |= VerticalStripFlag;

            // Layer & group membership are per-fixture (base item) properties.
            if (headIndex == 0 && linkedIndex == 0
                    && tAttrs.hasAttribute(KXMLQLCMonitorFixtureLayerId))
                item.m_layerId = tAttrs.value(KXMLQLCMonitorFixtureLayerId).toString().toUInt();
            if (headIndex == 0 && linkedIndex == 0
                    && tAttrs.hasAttribute(KXMLQLCMonitorFixtureGroupId))
                item.m_groupId = tAttrs.value(KXMLQLCMonitorFixtureGroupId).toString().toUInt();

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
        else if (root.name() == KXMLQLCMonitorLayer)
        {
            QXmlStreamAttributes a = root.attributes();
            MonitorLayer l;
            l.id      = a.value(KXMLQLCMonitorLayerID).toUInt();
            l.name    = a.value(KXMLQLCMonitorLayerName).toString();
            // Absent Visible defaults to true (older files / hand edits).
            l.visible = a.hasAttribute(KXMLQLCMonitorLayerVisible)
                            ? a.value(KXMLQLCMonitorLayerVisible).toInt() != 0
                            : true;
            l.locked  = a.value(KXMLQLCMonitorLayerLocked).toInt() != 0;
            // Absent Labels defaults to true (older files / hand edits).
            l.labels  = a.hasAttribute(QStringLiteral("Labels"))
                            ? a.value(QStringLiteral("Labels")).toInt() != 0
                            : true;
            l.order   = a.value(KXMLQLCMonitorLayerOrder).toInt();
            if (l.name.isEmpty())
                l.name = (l.id == defaultLayerId) ? QStringLiteral("Default")
                                                  : QStringLiteral("Layer %1").arg(l.id);
            m_layers.insert(l.id, l); // overwrites the pristine Default if id 0
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCMonitorActiveLayer)
        {
            m_activeLayerId = root.attributes().value(KXMLQLCMonitorLayerID).toUInt();
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCMonitorGroup)
        {
            QXmlStreamAttributes a = root.attributes();
            const quint32 gid = a.value(KXMLQLCMonitorGroupID).toUInt();
            if (gid != 0)
            {
                MonitorGroup g;
                g.id            = gid;
                g.name          = a.value(KXMLQLCMonitorGroupName).toString();
                g.layerId       = a.value(KXMLQLCMonitorGroupLayer).toUInt();
                g.parentGroupId = a.value(KXMLQLCMonitorGroupParent).toUInt();
                g.anchorKind    = a.value(QStringLiteral("AnchorKind")).toString();
                g.anchorId      = a.value(QStringLiteral("AnchorId")).toUInt();
                g.locked        = a.value(QStringLiteral("Locked")).toInt() != 0;
                g.hasFrame      = a.value(QStringLiteral("Frame")).toInt() != 0;
                if (g.hasFrame)
                {
                    g.origin   = QVector3D(a.value(QStringLiteral("OX")).toFloat(),
                                           a.value(QStringLiteral("OY")).toFloat(),
                                           a.value(QStringLiteral("OZ")).toFloat());
                    g.rotation = a.value(QStringLiteral("Rot")).toFloat();
                }
                if (a.hasAttribute(QStringLiteral("BoundFG")))
                {
                    g.boundFxGroup = a.value(QStringLiteral("BoundFG")).toUInt();
                    g.pitchX = a.value(QStringLiteral("PitchX")).toFloat();
                    g.pitchY = a.value(QStringLiteral("PitchY")).toFloat();
                }
                if (g.name.isEmpty())
                    g.name = QStringLiteral("Group %1").arg(gid);
                m_groups.insert(gid, g);
            }
            root.skipCurrentElement();
        }
        else if (root.name() == QStringLiteral("MonitorImage"))
        {
            QXmlStreamAttributes a = root.attributes();
            MonitorImage img;
            img.id      = a.value(QStringLiteral("ID")).toUInt();
            img.name    = a.value(QStringLiteral("Name")).toString();
            img.source  = a.value(QStringLiteral("Source")).toString();
            img.plane   = a.value(QStringLiteral("Plane")).toInt();
            img.originX = a.value(QStringLiteral("X")).toFloat();
            img.originY = a.value(QStringLiteral("Y")).toFloat();
            img.originZ = a.value(QStringLiteral("Z")).toFloat();
            img.width   = a.value(QStringLiteral("W")).toFloat();
            img.height  = a.value(QStringLiteral("H")).toFloat();
            img.rotation = a.value(QStringLiteral("Rot")).toFloat();
            img.layerId = a.value(QStringLiteral("LayerId")).toUInt();
            img.groupId = a.value(QStringLiteral("GroupId")).toUInt();
            img.locked  = a.value(QStringLiteral("Locked")).toInt() != 0;
            if (img.name.isEmpty())
                img.name = QStringLiteral("Image %1").arg(img.id + 1);
            m_images.insert(img.id, img);
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
        else if (root.name() == QStringLiteral("Pipe") || root.name() == QStringLiteral("Boom"))
        {
            Pipe *b = new Pipe(nextPipeId(), this);
            if (b->loadXML(root) && !m_pipes.contains(b->id()))
            {
                m_pipes.insert(b->id(), b);
            }
            else
            {
                qWarning() << "Discarding pipe with missing or duplicate id" << b->id();
                delete b;
            }
        }
        else if (root.name() == QStringLiteral("Stand"))
        {
            Stand *s = new Stand(nextStandId(), this);
            if (s->loadXML(root) && !m_stands.contains(s->id()))
            {
                m_stands.insert(s->id(), s);
            }
            else
            {
                qWarning() << "Discarding stand with missing or duplicate id" << s->id();
                delete s;
            }
        }
        else if (root.name() == QStringLiteral("Tower"))
        {
            Tower *t = new Tower(nextTowerId(), this);
            if (t->loadXML(root) && !m_towers.contains(t->id()))
            {
                m_towers.insert(t->id(), t);
            }
            else
            {
                qWarning() << "Discarding tower with missing or duplicate id" << t->id();
                delete t;
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
            if (a.hasAttribute("Riser"))
            {
                rp.riserPlatformId = a.value("Riser").toUInt();
                rp.riserFace       = a.value("RiserFace").toInt();
                rp.riserU          = a.value("RiserU").toFloat();
                rp.riserV          = a.value("RiserV").toFloat();
            }
            if (a.hasAttribute("TrussSide"))
                rp.trussMountSide = a.value("TrussSide").toInt();
            if (a.hasAttribute("TrussCross"))
                rp.trussCross = a.value("TrussCross").toFloat();
            if (a.hasAttribute("MountZ"))
                rp.mountZOffset = a.value("MountZ").toFloat();
            if (a.hasAttribute("Deck"))
            {
                rp.deckPlatformId   = a.value("Deck").toUInt();
                rp.deckHeightOffset = a.value("DeckH").toFloat();
            }
            if (a.hasAttribute("GLX") || a.hasAttribute("GLY") || a.hasAttribute("GLZ"))
                rp.groupLocal = QVector3D(a.value("GLX").toFloat(),
                                          a.value("GLY").toFloat(),
                                          a.value("GLZ").toFloat());
            if (a.hasAttribute("SM")) rp.studioMount = a.value("SM").toInt();
            if (a.hasAttribute("SA")) rp.studioAngle = a.value("SA").toFloat();
            if (a.hasAttribute("Boom"))
            {
                rp.pipeId     = a.value("Boom").toUInt();
                rp.pipeOffset = a.value("BoomOfs").toFloat();
                rp.pipeAngle  = a.value("BoomAng").toFloat();
            }
            if (a.hasAttribute("Tower"))
            {
                rp.towerId    = a.value("Tower").toUInt();
                rp.towerShelf = a.value("TShelf").toInt();
                rp.towerU     = a.value("TU").toFloat();
                rp.towerV     = a.value("TV").toFloat();
            }
            m_rigProps[fid] = rp;
            root.skipCurrentElement();
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown MonitorProperties tag:" << root.name();
            root.skipCurrentElement();
        }
    }

    // Guarantee the Default layer survives even if the file listed none, and
    // that the active layer references something that actually exists.
    ensureDefaultLayer();
    if (!m_layers.contains(m_activeLayerId))
        m_activeLayerId = defaultLayerId;

    // MIGRATION: the old single global background image is now a placeable Floor
    // image object. Convert it (once) so existing workspaces keep their plot.
    if (!m_commonBackgroundImage.isEmpty())
    {
        MonitorImage img;
        img.id     = nextImageId();
        img.name   = QStringLiteral("Background");
        img.source = m_commonBackgroundImage;
        img.plane  = MonitorImage::Floor;
        img.originX = 0.0f;
        img.originY = 0.0f;
        img.width   = m_gridSize.x();   // span the whole grid (metres/feet cells)
        img.height  = m_gridSize.z();
        m_images.insert(img.id, img);
        m_commonBackgroundImage.clear();
    }

    // Sync child-bar truss origins to their parents.
    recomputeChildTrusses();
    // Re-slave platform-anchored studio frames to their platforms.
    recomputeAnchoredFrames();

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
    if (!stageOrigin().isNull())
    {
        doc->writeAttribute(KXMLQLCMonitorOriginX, QString::number(stageOrigin().x()));
        doc->writeAttribute(KXMLQLCMonitorOriginY, QString::number(stageOrigin().y()));
    }

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
            if (item.m_flags & VerticalStripFlag)
                doc->writeAttribute(KXMLQLCMonitorFixtureVertStrip, KXMLQLCTrue);

            // always write position
            doc->writeAttribute(KXMLQLCMonitorItemXPosition, QString::number(item.m_position.x()));
            doc->writeAttribute(KXMLQLCMonitorItemYPosition, QString::number(item.m_position.y()));

#ifndef QMLUI
            // Persist the mounting height (Z) in the widgets build too — the
            // elevation views use it. Only when set, to keep files clean.
            if (headIndex == 0 && linkedIndex == 0 && item.m_position.z() != 0.0)
                doc->writeAttribute(KXMLQLCMonitorItemZPosition, QString::number(item.m_position.z()));
#endif

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

            // Layer & group membership are per-fixture (base item) properties.
            if (headIndex == 0 && linkedIndex == 0 && item.m_layerId != 0)
                doc->writeAttribute(KXMLQLCMonitorFixtureLayerId, QString::number(item.m_layerId));
            if (headIndex == 0 && linkedIndex == 0 && item.m_groupId != 0)
                doc->writeAttribute(KXMLQLCMonitorFixtureGroupId, QString::number(item.m_groupId));

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

    // Layers. Skip entirely when the layer system is untouched (only a
    // pristine Default, active layer = Default) so non-users get no XML noise.
    {
        const MonitorLayer def = m_layers.value(defaultLayerId);
        const bool pristine = m_layers.size() == 1
                && m_activeLayerId == defaultLayerId
                && def.visible && !def.locked && def.order == 0
                && def.name == QStringLiteral("Default");
        if (!pristine)
        {
            foreach (const MonitorLayer &l, layers())
            {
                doc->writeStartElement(KXMLQLCMonitorLayer);
                doc->writeAttribute(KXMLQLCMonitorLayerID,      QString::number(l.id));
                doc->writeAttribute(KXMLQLCMonitorLayerName,    l.name);
                doc->writeAttribute(KXMLQLCMonitorLayerVisible, QString::number(l.visible ? 1 : 0));
                if (l.locked)
                    doc->writeAttribute(KXMLQLCMonitorLayerLocked, QStringLiteral("1"));
                if (!l.labels)   // labels default on; only persist when hidden
                    doc->writeAttribute(QStringLiteral("Labels"), QStringLiteral("0"));
                doc->writeAttribute(KXMLQLCMonitorLayerOrder,   QString::number(l.order));
                doc->writeEndElement();
            }
            if (m_activeLayerId != defaultLayerId)
            {
                doc->writeStartElement(KXMLQLCMonitorActiveLayer);
                doc->writeAttribute(KXMLQLCMonitorLayerID, QString::number(m_activeLayerId));
                doc->writeEndElement();
            }
        }
    }

    // Groups (structural — always written).
    foreach (const MonitorGroup &g, m_groups)
    {
        doc->writeStartElement(KXMLQLCMonitorGroup);
        doc->writeAttribute(KXMLQLCMonitorGroupID,    QString::number(g.id));
        doc->writeAttribute(KXMLQLCMonitorGroupName,  g.name);
        doc->writeAttribute(KXMLQLCMonitorGroupLayer, QString::number(g.layerId));
        if (g.parentGroupId != 0)
            doc->writeAttribute(KXMLQLCMonitorGroupParent, QString::number(g.parentGroupId));
        if (!g.anchorKind.isEmpty())
        {
            doc->writeAttribute(QStringLiteral("AnchorKind"), g.anchorKind);
            doc->writeAttribute(QStringLiteral("AnchorId"),   QString::number(g.anchorId));
        }
        if (g.locked)
            doc->writeAttribute(QStringLiteral("Locked"), QStringLiteral("1"));
        if (g.hasFrame)
        {
            doc->writeAttribute(QStringLiteral("Frame"), QStringLiteral("1"));
            doc->writeAttribute(QStringLiteral("OX"), QString::number(double(g.origin.x()), 'f', 3));
            doc->writeAttribute(QStringLiteral("OY"), QString::number(double(g.origin.y()), 'f', 3));
            doc->writeAttribute(QStringLiteral("OZ"), QString::number(double(g.origin.z()), 'f', 3));
            doc->writeAttribute(QStringLiteral("Rot"), QString::number(double(g.rotation), 'f', 3));
        }
        if (g.boundFxGroup != 0)
        {
            doc->writeAttribute(QStringLiteral("BoundFG"), QString::number(g.boundFxGroup));
            doc->writeAttribute(QStringLiteral("PitchX"), QString::number(double(g.pitchX), 'f', 3));
            doc->writeAttribute(QStringLiteral("PitchY"), QString::number(double(g.pitchY), 'f', 3));
        }
        doc->writeEndElement();
    }

    // Placeable images
    foreach (const MonitorImage &img, m_images)
    {
        doc->writeStartElement(QStringLiteral("MonitorImage"));
        doc->writeAttribute(QStringLiteral("ID"),     QString::number(img.id));
        doc->writeAttribute(QStringLiteral("Name"),   img.name);
        doc->writeAttribute(QStringLiteral("Source"), img.source);
        doc->writeAttribute(QStringLiteral("Plane"),  QString::number(img.plane));
        doc->writeAttribute(QStringLiteral("X"),      QString::number(double(img.originX), 'f', 3));
        doc->writeAttribute(QStringLiteral("Y"),      QString::number(double(img.originY), 'f', 3));
        doc->writeAttribute(QStringLiteral("Z"),      QString::number(double(img.originZ), 'f', 3));
        doc->writeAttribute(QStringLiteral("W"),      QString::number(double(img.width),  'f', 3));
        doc->writeAttribute(QStringLiteral("H"),      QString::number(double(img.height), 'f', 3));
        if (img.rotation != 0.0f)
            doc->writeAttribute(QStringLiteral("Rot"), QString::number(double(img.rotation), 'f', 1));
        if (img.layerId != 0)
            doc->writeAttribute(QStringLiteral("LayerId"), QString::number(img.layerId));
        if (img.groupId != 0)
            doc->writeAttribute(QStringLiteral("GroupId"), QString::number(img.groupId));
        if (img.locked)
            doc->writeAttribute(QStringLiteral("Locked"), QStringLiteral("1"));
        doc->writeEndElement();
    }

    // Trusses
    foreach (const Truss *t, m_trusses)
        t->saveXML(doc);

    // Stage platforms
    foreach (const StagePlatform *p, m_platforms)
        p->saveXML(doc);

    // Booms
    foreach (const Pipe *b, m_pipes)
        b->saveXML(doc);

    // Stands
    foreach (const Stand *s, m_stands)
        s->saveXML(doc);

    // Towers
    foreach (const Tower *t, m_towers)
        t->saveXML(doc);

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
        if (rp.onRiser())
        {
            doc->writeAttribute(QStringLiteral("Riser"),     QString::number(rp.riserPlatformId));
            doc->writeAttribute(QStringLiteral("RiserFace"), QString::number(rp.riserFace));
            doc->writeAttribute(QStringLiteral("RiserU"),    QString::number(double(rp.riserU), 'f', 3));
            doc->writeAttribute(QStringLiteral("RiserV"),    QString::number(double(rp.riserV), 'f', 3));
        }
        if (rp.trussMountSide != FixtureRigProps::UnderHung)
            doc->writeAttribute(QStringLiteral("TrussSide"), QString::number(rp.trussMountSide));
        if (rp.trussCross != 0.0f)
            doc->writeAttribute(QStringLiteral("TrussCross"), QString::number(double(rp.trussCross), 'f', 3));
        if (rp.mountZOffset != 0.0f)
            doc->writeAttribute(QStringLiteral("MountZ"), QString::number(double(rp.mountZOffset), 'f', 3));
        if (rp.onDeck())
        {
            doc->writeAttribute(QStringLiteral("Deck"),  QString::number(rp.deckPlatformId));
            doc->writeAttribute(QStringLiteral("DeckH"), QString::number(double(rp.deckHeightOffset), 'f', 3));
        }
        if (rp.onPipe())
        {
            doc->writeAttribute(QStringLiteral("Boom"),    QString::number(rp.pipeId));
            doc->writeAttribute(QStringLiteral("BoomOfs"), QString::number(double(rp.pipeOffset), 'f', 3));
            doc->writeAttribute(QStringLiteral("BoomAng"), QString::number(double(rp.pipeAngle), 'f', 1));
        }
        if (rp.onTower())
        {
            doc->writeAttribute(QStringLiteral("Tower"),  QString::number(rp.towerId));
            doc->writeAttribute(QStringLiteral("TShelf"), QString::number(rp.towerShelf));
            doc->writeAttribute(QStringLiteral("TU"),     QString::number(double(rp.towerU), 'f', 3));
            doc->writeAttribute(QStringLiteral("TV"),     QString::number(double(rp.towerV), 'f', 3));
        }
        if (!rp.groupLocal.isNull())
        {
            doc->writeAttribute(QStringLiteral("GLX"), QString::number(double(rp.groupLocal.x()), 'f', 3));
            doc->writeAttribute(QStringLiteral("GLY"), QString::number(double(rp.groupLocal.y()), 'f', 3));
            doc->writeAttribute(QStringLiteral("GLZ"), QString::number(double(rp.groupLocal.z()), 'f', 3));
            doc->writeAttribute(QStringLiteral("SM"), QString::number(rp.studioMount));
            doc->writeAttribute(QStringLiteral("SA"), QString::number(double(rp.studioAngle), 'f', 1));
        }
        doc->writeEndElement();
    }

    doc->writeEndElement();

    return true;
}
