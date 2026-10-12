/*
  Q Light Controller Plus
  patchinventorydialog.h

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

#ifndef PATCHINVENTORYDIALOG_H
#define PATCHINVENTORYDIALOG_H

#include <QDialog>
#include <QList>

#include "patchplanner.h"

class QTableWidget;
class QLabel;
class QPushButton;
class Doc;
class QLCFixtureMode;

/** @addtogroup ui_functions
 * @{
 */

/**
 * The Patch view: gives fixtures a universe and address -- inventory fixtures
 * their first, patched ones a new one.
 *
 * It opens on PatchPlanner's proposal for the inventory fixtures (DMX packed
 * first-fit, one universe per distinct unicast IP) and on where the patched
 * ones already are, with every universe and address editable. "Place" puts the
 * selected rows (or all of them) at a universe and address, as a block that
 * keeps its spacing or packed tight. Rows that collide with each other or with
 * the rest of the show are flagged, and Apply commits the lot as one step --
 * all of it or none. The point is the LD reviews a plan rather than being
 * handed a result.
 */
class PatchInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    /** @param fixtureIds  fixtures to patch or re-address; empty = every
     *                     unpatched fixture (address clashes included) */
    PatchInventoryDialog(Doc *doc, const QList<quint32> &fixtureIds = QList<quint32>(),
                         QWidget *parent = NULL);

    /** The plan as currently edited. Exposed for tests. */
    QList<PatchPlanner::Assignment> currentPlan() const;

    /** Put @p rows (empty = every row) at @p universe / @p address, both
     *  zero-based, keeping the patched ones' spacing or packing them all --
     *  what the Place button does with the selection. */
    /** One universe each: the first of @p rows (all, when empty) at
     *  @p universe / @p address, the next at the universe after, and on. */
    void placeCountingUp(quint32 universe, quint32 address, const QList<int> &rows = QList<int>());

    /** Patch the other structures among these fixtures like @p pattern
     *  ("truss:1"): each fixture at the address of the one in its place on
     *  the pattern, in its own structure's universe (PatchPlanner::
     *  matchLike()). What could not be paired is said under the table.
     *  Returns how many rows it set. */
    int matchStructures(const QString &pattern);
    void placeRows(quint32 universe, quint32 address, bool keepSpacing,
                   const QList<int> &rows = QList<int>());
    /** Arrange @p rows (all, when empty) in @p universe from @p address
     *  around what is there (PatchPlanner::fit()): what has no room is
     *  flagged and left where it was -- unless @p carryOn -- and the bar
     *  under the table offers to carry on or to fit by changing modes.
     *  Returns how many have no room. */
    int fitRows(quint32 universe, quint32 address, bool carryOn = false,
                const QList<int> &rows = QList<int>());
    /** Smaller modes for the last fitRows() so they all fit, then fit
     *  again. Returns how many still have no room. */
    int fitByChangingModes();
    /** Put the rows in @p order (PatchPlanner::AddressOrder), the order
     *  Place addresses them in. */
    void setAddressOrder(int order);

public slots:
    void accept() override;

private slots:
    void slotReplan();
    void slotCellEdited();
    void slotModeChanged();
    void slotPlace();

private:
    void fill(const QList<PatchPlanner::Assignment> &plan,
              const QHash<quint32, QLCFixtureMode *> &modes = QHash<quint32, QLCFixtureMode *>(),
              const QHash<quint32, quint32> &dimmerChannels = QHash<quint32, quint32>());

    /** Channels each row will have once its chosen mode is applied. */
    QHash<quint32, quint32> channelOverrides() const;
    quint32 rowChannels(int row) const;
    void refreshConflicts();

private:
    /** Inventory fixtures as planned, patched ones where they are now. */
    QList<PatchPlanner::Assignment> initialPlan(
        const QHash<quint32, quint32> &channelOverride = QHash<quint32, quint32>()) const;
    /** Write @p plan's universe/address into the matching rows. */
    void setRowAddresses(const QList<PatchPlanner::Assignment> &plan);

    /** A planner set up the way the dialog is (nearest-node placement on/off). */
    PatchPlanner planner() const;
    /** "SR Node · port 2 · 3.2 m" for the node port carrying @p universe to a
     *  row, or a dash when no node port carries it. */
    QString viaText(int row, quint32 universe) const;

    Doc *m_doc;
    QList<quint32> m_ids;
    class QCheckBox *m_proximity;   //!< "Place by nearest node"
    QTableWidget *m_table;
    QLabel *m_summary;
    QPushButton *m_apply;
    class UniverseCombo *m_placeUni; //!< Place: universe, by name
    class QSpinBox *m_placeAddr;    //!< Place: address (1-based)
    class QComboBox *m_placeHow;    //!< Place: keep spacing / pack
    class QComboBox *m_order = nullptr; //!< the addressing scheme: by name, kind, as they hang
    /** The rows' ids in the addressing order. */
    QList<quint32> inAddressOrder(const QList<quint32> &ids) const;
    void sortRows();
    class QComboBox *m_matchPattern = nullptr; //!< Match: the structure to copy
    QLabel *m_matchProblems = nullptr;
    /** Under the table when a fit left fixtures without room. */
    QWidget *m_fitBar = nullptr;
    QLabel *m_fitText = nullptr;
    class QPushButton *m_fitModes = nullptr;
    /** The last fit, for its bar's buttons. */
    quint32 m_fitUniverse = 0, m_fitAddress = 0;
    QList<quint32> m_fitIds;
    bool m_filling;
};

/** @} */

#endif
