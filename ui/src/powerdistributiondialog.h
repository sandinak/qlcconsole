/*
  Q Light Controller Plus
  powerdistributiondialog.h

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

#ifndef POWERDISTRIBUTIONDIALOG_H
#define POWERDISTRIBUTIONDIALOG_H

#include <QDialog>
#include <QHash>

class QTreeWidget;
class QTreeWidgetItem;
class QStackedWidget;
class QGroupBox;
class QPushButton;
class QLineEdit;
class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

/**
 * Editor for the workspace power-distribution model: power sources (each with a
 * voltage) containing circuits (breakers with a rated amperage), and the
 * fixtures fed by each circuit. Shows the live per-circuit load (designed peak,
 * from the running preview) and flags overloaded breakers, and offers a greedy
 * auto-assign that packs fixtures into circuits up to their derated limit.
 */
class PowerDistributionDialog : public QDialog
{
    Q_OBJECT

public:
    PowerDistributionDialog(Doc *doc, QWidget *parent = NULL);
    ~PowerDistributionDialog();

private:
    /** Snapshot each fixture's designed-peak watts from the running preview. */
    void computeFixtureWatts();
    /** Rebuild the sources/circuits tree from the model. */
    void rebuildTree();
    /** (Re)render one circuit row from the model, in place. */
    void populateCircuitItem(QTreeWidgetItem *ci, int sourceIdx, int circuitIdx);
    /** (Re)render a source row's summary cells (load / kVA / runtime). */
    void updateSourceRow(QTreeWidgetItem *si, int sourceIdx);
    /** Rebuild the fixtures list with current assignments + watts. */
    void rebuildFixtures();
    /** Circuit amps from an arbitrary fixtureId->watts map. */
    double circuitAmpsFrom(int sourceIdx, int circuitIdx,
                           const QHash<quint32, double> &watts) const;
    /** Total live amps on a circuit from the watts snapshot. */
    double circuitAmps(int sourceIdx, int circuitIdx) const;
    /** Total full-rated amps on a circuit (everything at full intensity). */
    double circuitFullAmps(int sourceIdx, int circuitIdx) const;
    /** Total live watts on a source (sum of its circuits' fixtures). */
    double sourceWatts(int sourceIdx) const;
    /** Sync the UPS/battery panel to the currently-selected source. */
    void refreshSourcePanel();

private slots:
    void slotAddSource();
    void slotAddCircuit();
    void slotRemoveSelected();
    void slotTreeSelectionChanged();
    void slotTreeItemChanged(QTreeWidgetItem *item, int column);
    void slotAssign();
    void slotUnassign();
    void slotAutoAssign();
    void slotSourceUpsChanged();
    void slotSceneCheck();
    void slotExportVenue();
    void slotImportVenue();

private:
    Doc *m_doc;
    QHash<quint32, double> m_fixtureWatts;
    QHash<quint32, double> m_fixtureFullWatts;

    QTreeWidget *m_tree;            //!< sources → circuits
    QPushButton *m_addSourceBtn;
    QPushButton *m_addCircuitBtn;
    QPushButton *m_removeBtn;

    QTreeWidget *m_fixtureList;     //!< fixtures with assignment + watts
    QPushButton *m_assignBtn;
    QPushButton *m_unassignBtn;
    QPushButton *m_autoBtn;
    QPushButton *m_sceneCheckBtn;

    // UPS / battery panel — edits the selected source; disabled otherwise.
    QGroupBox      *m_sourceBox;
    QDoubleSpinBox *m_vaSpin;
    QDoubleSpinBox *m_runMinSpin;
    QDoubleSpinBox *m_runWattSpin;
    QLabel         *m_sourceSummary;
    int             m_panelSource = -1;   //!< source index the panel is editing
    bool            m_panelUpdating = false;
};

/** @} */

#endif
