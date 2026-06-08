/*
  Q Light Controller
  monitor.h

  Copyright (c) Heikki Junnila

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

#ifndef MONITOR_H
#define MONITOR_H

#include <QWidget>
#include <QHash>
#include <QList>

#include "monitorproperties.h"

class MonitorGraphicsView;
class MonitorFixture;
class MonitorLayout;
class QScrollArea;
class QComboBox;
class QSplitter;
class QToolBar;
class QSpinBox;
class QAction;
class Fixture;
class Monitor;
class QTimer;
class Doc;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class Monitor final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(Monitor)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    /** Get the monitor singleton instance. Can be NULL. */
    static Monitor* instance();

    /** Create or show Monitor */
    static void createAndShow(QWidget* parent, Doc* doc);

    /** Normal public destructor */
    ~Monitor();

    /** Refresh fixtures & show current view */
    void updateView();
protected:
    /** Initialize the monitor view */
    void initView();

    /** Initialize the monitor view in DMX mode */
    void initDMXView();

    /** Initialize settings & fill fixtures */
    void fillDMXView();

    /** show */
    void showDMXView();

    /** Initialize the monitor view in 2D graphics mode */
    void initGraphicsView();

    /** Initialize settings & fill fixtures */
    void fillGraphicsView();

    /** show */
    void showGraphicsView();

    void showCurrentView();
protected:
    void saveSettings();

    /** Protected constructor to prevent multiple instances. */
    Monitor(QWidget* parent, Doc* doc, Qt::WindowFlags f = Qt::Widget);

protected:
    /** The singleton Monitor instance */
    static Monitor* s_instance;
    Doc* m_doc;
    MonitorProperties *m_props;

    /*********************************************************************
     * Running functions
     *********************************************************************/
protected slots:
    void slotFunctionStarted(quint32 id);

    /*********************************************************************
     * Menu
     *********************************************************************/
protected:

    /** Create DMX view toolbar */
    void initDMXToolbar();

    void initGraphicsToolbar();

protected slots:
    /** Menu action slot for font selection */
    void slotChooseFont();

    /** Menu action slot for channel style selection */
    void slotChannelStyleTriggered();

    /** Menu action slot for value style selection */
    void slotValueStyleTriggered();

    /** Menu action slot to trigger display mode switch */
    void slotSwitchMode();

    /********************************************************************
     * Monitor Fixtures
     ********************************************************************/
public:
    /** Update monitor fixture labels */
    void updateFixtureLabelStyles();

protected:
    /** Create a new MonitorFixture* and append it to the layout */
    void createMonitorFixture(Fixture* fxi);

protected slots:
    /** Slot for fixture additions (to append the new fixture to layout) */
    void slotFixtureAdded(quint32 fxi_id);

    /** Slot for fixture contents & layout changes */
    void slotFixtureChanged(quint32 fxi_id);

    /** Slot for fixture removals (to remove the fixture from layout) */
    void slotFixtureRemoved(quint32 fxi_id);

    /** Slot called when a universe combo item is selected */
    void slotUniverseSelected(int index);

signals:
    void channelStyleChanged(MonitorProperties::ChannelStyle style);
    void valueStyleChanged(MonitorProperties::ValueStyle style);

protected:
    QToolBar* m_DMXToolBar;
    QScrollArea* m_scrollArea;
    QWidget* m_monitorWidget;
    MonitorLayout* m_monitorLayout;
    QList <MonitorFixture*> m_monitorFixtures;
    quint32 m_currentUniverse;

    /********************************************************************
     * Graphics View
     ********************************************************************/
protected:
    /** Hides the Fixture Item editor on the right side of the view */
    void hideFixtureItemEditor();

    /** Shows the Fixture Item editor on the right side of the view */
    void showFixtureItemEditor();

protected slots:
    /** Slot called when the grid width changes */
    void slotGridWidthChanged(int value);

    /** Slot called when the grid height changes */
    void slotGridHeightChanged(int value);

    /** Slot called when the unit metrics changes */
    void slotGridUnitsChanged(int index);

    /** Slot called when the grid sub-divisions value changes */
    void slotGridSubdivisionsChanged(int value);

    /** Slot called when the snap-to-grid level changes */
    void slotSnapChanged(int index);

    /** Slot called when the layout lock state is toggled */
    void slotLockToggled(bool locked);

    /** Slot called when the user wants to add
     *  a fixture to the graphics view */
    void slotAddFixture();

    /** Slot called when the user wants to remove
     *  a fixture from the graphics view */
    void slotRemoveFixture();

    /** Slot called when the user wants to set
     *  a background picture on the graphics view */
    void slotSetBackground();

    /** Add a new truss interactively and refresh the canvas. */
    void slotAddTruss();

    /** Open the truss-management dialog (edit / remove existing trusses). */
    void slotManageTrusses();

    /** Open the truss-edit form for a specific truss ID (from double-click). */
    void slotEditTruss(quint32 tid);

    /** Remove whichever fixture or truss is currently selected in the view. */
    void slotRemoveSelected();

    /** Open the background color picker. */
    void slotSetBackgroundColor();

    /** Show the fixture properties editor on double-click. */
    void slotFixtureDoubleClicked(quint32 fid);

    /** Open truss edit dialog on double-click. */
    void slotTrussDoubleClicked(quint32 tid);

    /** Show the Add context menu at the given scene position. */
    void slotCanvasContextMenu(QPointF scenePos);

    /** Add a fixture onto a truss at the given offset (from TrussItem right-click). */
    void slotAddFixtureToTruss(quint32 trussId, float offsetMetres);

    /** Slot called when the user wants to show
     *  or hide fixtures labels */
    void slotShowLabels(bool visible);

    /** Slot called when a fixture is moved in the graphics view */
    void slotFixtureMoved(quint32 fid, QPointF pos);

    /** Slot called when the graphics view is clicked */
    void slotViewClicked();

protected:
    QToolBar* m_graphicsToolBar;
    QSplitter* m_splitter;
    /** Scene-space position of the last canvas right-click; used to place new
     *  fixtures/trusses at the cursor location. */
    QPointF m_pendingAddScenePos;
    MonitorGraphicsView* m_graphicsView;
    QWidget *m_fixtureItemEditor;
    QSpinBox* m_gridWSpin;
    QSpinBox *m_gridHSpin;
    QComboBox *m_unitsCombo;
    QSpinBox *m_gridSubdivSpin;
    QComboBox *m_snapCombo;
    QAction *m_labelsAction;
    QAction *m_lockAction;
};

/** @} */

#endif
