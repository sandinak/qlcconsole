/*
  Q Light Controller Plus
  powerdistributionwidget.h

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

#ifndef POWERDISTRIBUTIONWIDGET_H
#define POWERDISTRIBUTIONWIDGET_H

#include <QTreeWidget>
#include <QWidget>
#include <QHash>
#include <QList>

class QTreeWidgetItem;
class QGroupBox;
class QPushButton;
class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QLabel;
class QTimer;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

/**
 * Sources/circuits tree that also accepts fixtures dragged from the Fixture
 * Manager's tree: dropping a fixture (or a selection) onto a circuit row assigns
 * them to that circuit. Emits fixturesDroppedOnCircuit for the widget to apply.
 */
class PowerCircuitTree : public QTreeWidget
{
    Q_OBJECT

public:
    explicit PowerCircuitTree(QWidget *parent = NULL);

signals:
    void fixturesDroppedOnCircuit(int sourceIdx, int circuitIdx,
                                  const QList<quint32> &fixtureIds);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    /** Encode selected fixture rows so they can be dragged between circuits.
     *  (Qt5 passes the list by value; Qt6 by const-ref.) */
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override;
#else
    QMimeData *mimeData(const QList<QTreeWidgetItem *> items) const override;
#endif
};

/**
 * Editor for the workspace power-distribution model, embeddable anywhere (the
 * Universes page, the Power Distribution dialog): power sources (each with a
 * voltage) containing circuits (breakers with a rated amperage), and the
 * fixtures fed by each circuit.
 *
 * Shows three load figures per circuit and source, updated live as fixtures are
 * patched or (re)assigned:
 *   - Max load     — every fixture on the circuit at full intensity (worst case)
 *   - Design load  — Max x the circuit's demand/diversity factor, i.e. the load
 *                    planned around when "not everything is lit at once"
 *   - Live load    — the currently-designed output (from the running preview)
 * Overage (Design load - derated breaker limit) is flagged so circuits can be
 * balanced. Derate trims the breaker's limit; demand trims the expected load —
 * they are independent knobs.
 */
class PowerDistributionWidget : public QWidget
{
    Q_OBJECT

public:
    /** @param withFixtureList  build the right-hand fixtures list + Assign
     *  buttons. Off when embedded next to a fixture tree that already drives
     *  assignment (the Fixture Manager); on for the standalone dialog. */
    PowerDistributionWidget(Doc *doc, bool withFixtureList = true, QWidget *parent = NULL);
    ~PowerDistributionWidget();

    /** Recompute per-fixture watts and rebuild the tree/fixtures/footer. */
    void refresh();

protected:
    /** @reimp — refresh when shown so the view reflects any offscreen edits. */
    void showEvent(QShowEvent *ev) override;

private:
    /** Snapshot each fixture's designed-peak + full-rated watts. */
    void computeFixtureWatts();
    /** Rebuild the sources/circuits tree from the model. */
    void rebuildTree();
    /** (Re)render one circuit row from the model, in place. */
    void populateCircuitItem(QTreeWidgetItem *ci, int sourceIdx, int circuitIdx);
    /** Fill the voltage/amps/demand/load/max cells of a circuit onto an item
     *  (a circuit row, or a wall-socket source row standing in for its circuit). */
    void renderCircuitLoadCells(QTreeWidgetItem *it, int sourceIdx, int circuitIdx,
                                bool sourceRow);
    /** Fill a source row's own cells (name/type/feed connector) and, for a wall
     *  socket, its implicit circuit's load cells + nested fixtures. */
    void fillSourceRow(QTreeWidgetItem *si, int sourceIdx);
    /** Append a display-only fixture row under a circuit / wall-socket source. */
    void addFixtureRow(QTreeWidgetItem *parent, quint32 fxId);
    /** Guarantee a wall socket has its single implicit circuit (index 0). */
    void ensureWallSocketCircuit(int sourceIdx);
    /** React to a source's Type change (e.g. wall socket → collapse to 1 circuit). */
    void applySourceType(int sourceIdx);
    /** (Re)render a source row's summary cells (load / kVA / runtime). */
    void updateSourceRow(QTreeWidgetItem *si, int sourceIdx);
    /** Rebuild the fixtures list with current assignments + watts. */
    void rebuildFixtures();
    /** Remove the selected sources/circuits, warning first if the removal is
     *  bulk (multiple items, a source with circuits, or assigned fixtures). */
    void removeSelectedTreeItems();
    /** Refresh the running-total footer from the current model + watts. */
    void updateFooter();
    /** Circuit amps from an arbitrary fixtureId->watts map. */
    double circuitAmpsFrom(int sourceIdx, int circuitIdx,
                           const QHash<quint32, double> &watts) const;
    /** Total live amps on a circuit from the watts snapshot. */
    double circuitAmps(int sourceIdx, int circuitIdx) const;
    /** Total full-rated amps on a circuit (everything at full intensity). */
    double circuitFullAmps(int sourceIdx, int circuitIdx) const;
    /** Full-rated amps scaled by the circuit's effective demand factor. */
    double circuitDesignAmps(int sourceIdx, int circuitIdx) const;
    /** Total live watts on a source (sum of its circuits' fixtures). */
    double sourceWatts(int sourceIdx) const;
    /** The workspace default demand factor (percent) from QSettings. */
    int defaultDemandPercent() const;
    /** Update the plain-language gloss next to the demand-factor spinbox. */
    void updateDemandHint();
    /** Sync the UPS/battery panel to the currently-selected source. */
    void refreshSourcePanel();
    /** Assign the fixtures-list selection to source/circuit s/c (c<0 = unassign). */
    void assignSelectionTo(int s, int c);

private slots:
    /** Right-click a fixture (or selection) in the list → assign / unassign. */
    void slotFixtureContextMenu(const QPoint &pos);
    /** Right-click the tree → add source/circuit, remove, or unassign a fixture. */
    void slotTreeContextMenu(const QPoint &pos);
    /** Fixtures dropped onto a circuit row → assign them to that circuit. */
    void slotFixturesDroppedOnCircuit(int sourceIdx, int circuitIdx,
                                      const QList<quint32> &fixtureIds);
    void slotAddSource();
    void slotAddCircuit();
    void slotRemoveSelected();
    void slotTreeSelectionChanged();
    void slotTreeItemChanged(QTreeWidgetItem *item, int column);
    void slotAssign();
    void slotUnassign();
    void slotAutoAssign();
    void slotSourceUpsChanged();
    void slotSourceTypeChanged();
    void slotDefaultDemandChanged(int percent);
    void slotSceneCheck();
    void slotExportVenue();
    void slotImportVenue();
    /** Coalesced refresh after fixtures are added/removed/patched. */
    void slotFixturesChanged();

private:
    Doc *m_doc;
    bool m_withFixtureList;
    QHash<quint32, double> m_fixtureWatts;
    QHash<quint32, double> m_fixtureFullWatts;

    QSpinBox    *m_demandSpin;      //!< workspace default demand factor (%)
    QLabel      *m_demandHint;      //!< plain-language gloss of the current %

    PowerCircuitTree *m_tree;       //!< sources → circuits (accepts fixture drops)
    QPushButton *m_addSourceBtn;
    QPushButton *m_addCircuitBtn;
    QPushButton *m_removeBtn;

    QTreeWidget *m_fixtureList;     //!< fixtures with assignment + watts
    QPushButton *m_assignBtn;
    QPushButton *m_unassignBtn;
    QPushButton *m_autoBtn;
    QPushButton *m_sceneCheckBtn;
    QPushButton *m_importBtn;
    QPushButton *m_exportBtn;

    QLabel      *m_footer;          //!< running totals across the whole rig

    // Source-properties panel — edits the selected source; disabled otherwise.
    QGroupBox      *m_sourceBox;
    QComboBox      *m_typeCombo;    //!< source wiring type (mirrors the Type column)
    QDoubleSpinBox *m_vaSpin;
    QDoubleSpinBox *m_runMinSpin;
    QDoubleSpinBox *m_runWattSpin;
    QLabel         *m_sourceSummary;
    int             m_panelSource = -1;   //!< source index the panel is editing
    bool            m_panelUpdating = false;

    QTimer         *m_refreshTimer;       //!< debounces bursts of fixture edits
};

/** @} */

#endif
