/*
  Q Light Controller Plus
  studiogroupeditor.h

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

#ifndef STUDIOGROUPEDITOR_H
#define STUDIOGROUPEDITOR_H

#include <QDialog>
#include <QMap>

#include "truss.h"                 // FixtureRigProps
#include "monitorproperties.h"     // MonitorGroup

class QDoubleSpinBox;
class QLineEdit;
class QTableWidget;
class QComboBox;
class QListWidget;
class StudioPlaneView;
class Doc;

/** Fixture Studio — group editor.
 *
 *  Edits a studio group (a MonitorGroup with hasFrame == true): its local-frame
 *  origin + rotation (rigid move/rotate of the whole unit) and each member's
 *  group-local offset. Changes apply live to the document; the host connects
 *  changed() to refresh the 2D map. Cancel reverts to the on-open snapshot.
 *
 *  Phase 1 of FIXTURESTUDIO_DESIGN.md — form-based; the graphical 3-plane
 *  (Top/Front/Side) canvas layers on top of this in a later slice.
 */
class StudioGroupEditor : public QDialog
{
    Q_OBJECT

public:
    StudioGroupEditor(Doc *doc, quint32 groupId, QWidget *parent = nullptr);

signals:
    /** Emitted whenever the group frame or a member offset changes (live), and
     *  once more on Cancel after the revert, so the map redraws either way. */
    void changed();

private slots:
    void applyFrame();          ///< push origin/rotation/name → doc, emit changed()
    void applyMemberCell(int row, int col);
    void recenterOrigin();      ///< move origin to member centroid; members stay put
    void seedFromFixtureGroup();   ///< lay members out from the bound FG cell matrix
    void adoptFromFixtureGroup();  ///< pull bound FG members in at their current pos
    void distributeEvenly();       ///< distribute the list selection on the current face
    void putSelectedOnFace();      ///< move the list selection onto the current face
    void putGroupOnFace();         ///< assign a whole FixtureGroup to the current face
    void syncHighlight();          ///< push the list selection to the canvas

private:
    QList<quint32> members() const;   ///< fixtures under this frame group
    QList<quint32> selectedFixtureIds() const;   ///< current left-list selection
    void rebuildTable();
    void snapshot();
    void revert();
    /** Record a fixture's pre-edit group + rig once, so Cancel can fully unwind
     *  membership pulled in by seed/adopt. No-op if already remembered. */
    void rememberFixture(quint32 fid);

private:
    Doc     *m_doc;
    quint32  m_groupId;
    bool     m_loading = false;   ///< guard against feedback while populating

    QLineEdit       *m_name = nullptr;
    QDoubleSpinBox  *m_ox = nullptr;
    QDoubleSpinBox  *m_oy = nullptr;
    QDoubleSpinBox  *m_oz = nullptr;
    QDoubleSpinBox  *m_rot = nullptr;
    QTableWidget    *m_table = nullptr;

    QComboBox       *m_fgCombo = nullptr;
    QDoubleSpinBox  *m_pitchX = nullptr;
    QDoubleSpinBox  *m_pitchY = nullptr;
    quint32 selectedFxGroup() const;

    StudioPlaneView *m_plane = nullptr;
    QComboBox       *m_planeCombo = nullptr;
    QListWidget     *m_list = nullptr;
    QComboBox       *m_faceCombo = nullptr;   ///< face for FixtureGroup → face

    // On-open snapshot for Cancel/revert.
    MonitorProperties::MonitorGroup   m_snapGroup;
    QMap<quint32, FixtureRigProps>    m_snapRig;
    QMap<quint32, quint32>            m_snapGroupOf;   ///< fid -> prior monitor groupId
};

#endif // STUDIOGROUPEDITOR_H
