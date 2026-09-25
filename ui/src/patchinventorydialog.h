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
 * The Patch view: gives inventory fixtures a real universe and address.
 *
 * It opens on PatchPlanner's proposal (DMX packed first-fit, one universe per
 * distinct unicast IP) with every universe and address editable, flags rows that
 * collide with each other or with the show, and commits on Apply. The point is
 * the LD reviews a plan rather than being handed a result.
 */
class PatchInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    /** @param fixtureIds  fixtures to patch; empty = every unpatched fixture */
    PatchInventoryDialog(Doc *doc, const QList<quint32> &fixtureIds = QList<quint32>(),
                         QWidget *parent = NULL);

    /** The plan as currently edited. Exposed for tests. */
    QList<PatchPlanner::Assignment> currentPlan() const;

public slots:
    void accept() override;

private slots:
    void slotReplan();
    void slotCellEdited();
    void slotModeChanged();

private:
    void fill(const QList<PatchPlanner::Assignment> &plan,
              const QHash<quint32, QLCFixtureMode *> &modes = QHash<quint32, QLCFixtureMode *>(),
              const QHash<quint32, quint32> &dimmerChannels = QHash<quint32, quint32>());

    /** Channels each row will have once its chosen mode is applied. */
    QHash<quint32, quint32> channelOverrides() const;
    quint32 rowChannels(int row) const;
    void refreshConflicts();

private:
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
    bool m_filling;
};

/** @} */

#endif
