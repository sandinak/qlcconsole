/*
  Q Light Controller Plus
  monitorgraphicsview.cpp

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

#ifndef MONITORGRAPHICSVIEW_H
#define MONITORGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QHash>

#include "fixture.h"

class MonitorProperties;
class MonitorFixtureItem;
class Doc;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class MonitorGraphicsView final : public QGraphicsView
{
    Q_OBJECT

public:
    MonitorGraphicsView(Doc *doc, QWidget *parent = 0);
    ~MonitorGraphicsView();

    /** Set the graphics view size in monitor units */
    void setGridSize(QSize size);

    /** Get the grid size in monitor units */
    QSize gridSize() const { return m_gridSize; }

    /** Set the number of sub-divisions drawn inside each grid cell */
    void setGridSubdivisions(int subdivisions);

    /** Get the current number of grid sub-divisions */
    int gridSubdivisions() const { return m_gridSubdivisions; }

    /** Lock/unlock the layout. When locked fixtures cannot be moved
     *  but can still be selected (rubber-band selection surface) */
    void setLayoutLocked(bool locked);

    /** Return true if the layout is currently locked */
    bool layoutLocked() const { return m_layoutLocked; }

    /** Set the snap-to-grid level (0 = off, 1 = full, 2 = 1/2, 4 = 1/4) */
    void setSnapDivisions(int divisions);

    /** Get the current snap-to-grid level */
    int snapDivisions() const { return m_snapDivisions; }

    /** Set the measure unit to use */
    void setGridMetrics(float value);

    /** Get the currently selected fixture ID.
     *  Fixture::invalidId is returned if none is selected */
    quint32 selectedFixtureID();

    /** Return a list of the fixture IDs in the current view */
    QList <quint32> fixturesID() const;

    /** Retrieve the currently selected MonitorFixtureItem.
     *  Return NULL if none */
    MonitorFixtureItem *getSelectedItem();

    /** Set the gel color of the fixture with the given ID */
    void setFixtureGelColor(quint32 id, QColor col);

    /** Set the rotation degrees of the fixture with the given ID */
    void setFixtureRotation(quint32 id, ushort degrees);

    /** Show/hide fixtures items labels */
    void showFixturesLabels(bool visible);

    /** Return the gel color of the fixture with the given ID */
    QColor fixtureGelColor(quint32 id);

    /** Add a fixture to the current view */
    void addFixture(quint32 id, QPointF pos = QPointF(0, 0));

    /** Remove the fixture with the given ID from the view
     *  If no ID is specified, the currently selected
     *  fixture will be removed (if possible)
     */
    bool removeFixture(quint32 id = Fixture::invalidId());

    /** Remove all fixtures from the current view */
    void clearFixtures();

    /** Support function to convert a position in millimeters
     *  to a position in pixels */
    QPointF realPositionToPixels(qreal xpos, qreal ypos);

    /** Update the position and the scale of the fixture with
     *  the given ID
     */
    void updateFixture(quint32 id);

    /** Set a background image for the view */
    void setBackgroundImage(QString filename);

    /** Retrieve the path to the background image currently set */
    QString backgroundImage() { return m_backgroundImage; }

protected:
    /** Triggers the whole view repaint and metrics
     *  computation */
    void updateGrid();

    /** Event caught when the GraphicsView is resized */
    void resizeEvent(QResizeEvent *event) override;

    /** Scroll wheel zooms the view around the cursor. */
    void wheelEvent(QWheelEvent *event) override;

    /** Trackpad pinch-to-zoom (macOS native zoom gesture). */
    bool viewportEvent(QEvent *event) override;

    /** Shift+left-drag on empty canvas pans the view. */
    void mousePressEvent(QMouseEvent *event) override;

    /** Push the current snap configuration to a single fixture item */
    void applySnapToItem(MonitorFixtureItem *item);

    /** Push the current snap configuration to every fixture item */
    void applySnapToAllItems();

public:
    /** Undo the last fixture move (Cmd/Ctrl+Z). */
    void undoLastMove();

public slots:
    void mouseReleaseEvent(QMouseEvent *e) override;

protected slots:
    /** Slot called when a MonitorFixtureItem is dropped after a drag */
    void slotFixtureMoved(MonitorFixtureItem *item);

private:
    /** Snap a scene-pixel coordinate to the current grid/subdivision. */
    QPointF snapScenePos(const QPointF &p) const;
    /** Snapshot the real positions of the items that are about to move. */
    void captureMoveUndo();

signals:
    /** Signal emitted after fixture point -> metrics conversion */
    void fixtureMoved(quint32 id, QPointF pos);

    /** Signal emitted when the graphics view is clicked */
    void viewClicked(QMouseEvent *e);

private:
    Doc *m_doc;
    QGraphicsScene *m_scene;

    /** Size of the grid. How many horizontal and vertical cells */
    QSize m_gridSize;

    /** Size of a grid cell in pixels */
    int m_cellPixels;

    /** X offset of the grid to keep it centered */
    qreal m_xOffset;

    /** Y offset of the grid to keep it centered */
    qreal m_yOffset;

    /** The unit used by the grid. Meters = 1000mm, Feet = 304.8mm */
    float m_unitValue;

    /** List of Fixture items represented graphically */
    QList <QGraphicsLineItem *> m_gridItems;

    /** Flag to enable/disable the grid rendering */
    bool m_gridEnabled;

    /** Number of sub-divisions drawn inside each grid cell */
    int m_gridSubdivisions;

    /** Flag indicating whether the layout is locked (fixtures not movable) */
    bool m_layoutLocked;

    /** Snap-to-grid level (0 = off, 1 = full, 2 = 1/2, 4 = 1/4) */
    int m_snapDivisions;

    /** Path to the view background image */
    QString m_backgroundImage;

    QPixmap m_bgPixmap;

    QGraphicsPixmapItem *m_bgItem;

    /** Map of the rendered MonitorFixtureItem with their ID */
    QHash <quint32, MonitorFixtureItem*> m_fixtures;

    /** Undo stack of fixture-move operations (fixture id -> previous real
     *  position in mm). Most recent at the back. */
    QList<QHash<quint32, QPointF> > m_moveUndo;

    /** Positions snapshot taken at move-start, committed to m_moveUndo on
     *  drop if anything actually moved. */
    QHash<quint32, QPointF> m_pendingMoveUndo;
};

/** @} */

#endif // MONITORGRAPHICSVIEW_H
