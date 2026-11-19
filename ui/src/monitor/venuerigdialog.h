/*
  Q Light Controller Plus
  venuerigdialog.h

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

#ifndef VENUERIGDIALOG_H
#define VENUERIGDIALOG_H

#include <QDialog>

class RoomOutputPicker;
class LengthSpinBox;
class QTableWidget;
class QGroupBox;
class QListWidget;
class QPushButton;
class QLabel;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The venue page (board Q137/Q138, docs/design/VENUES.md section 9), from the Focus
 * calls dialog's venue manager: what one venue owns -- its house rig and the
 * groups it fills, the show's universes (slots) with this room's patch and
 * what is not ours -- and the clash check against the show's tour package.
 */
class VenueRigDialog final : public QDialog
{
    Q_OBJECT

public:
    VenueRigDialog(Doc *doc, const QString &venue, QWidget *parent = nullptr);

    /** Open it for @p venue (not modal); an empty name says why not. */
    static VenueRigDialog *open(Doc *doc, QWidget *parent, const QString &venue);

    /** Before @p venue goes live: its clashes listed, Apply anyway or
     *  Cancel. True (nothing asked) when it has none, or it is not a venue
     *  (None, a new one). */
    static bool confirmClashes(Doc *doc, QWidget *parent, const QString &venue);

    QString venue() const { return m_venue; }
    QTableWidget *universeTable() const { return m_table; }
    QListWidget *clashList() const { return m_clashes; }
    /** A venue made from another show: its universe's output in this room. */
    RoomOutputPicker *roomOutput() const { return m_roomOutput; }
    QPushButton *useRoomOutputButton() const { return m_useRoomOutput; }
    QPushButton *matchGroupsButton() const { return m_matchGroups; }
    /** The Room box (docs/design/VENUE_ROLES_DATUM.md section 2.4): how the
     *  room is lined up under the show, Line Up the Room, and how far
     *  downstage of the room's piece the show's drape hangs. */
    QGroupBox *lineUpBox() const { return m_lineUpBox; }
    QLabel *lineUpText() const { return m_lineUpText; }
    QPushButton *lineUpButton() const { return m_lineUp; }
    LengthSpinBox *downstageField() const { return m_downstage; }
    /** The Roles box (docs/design/VENUE_ROLES_DATUM.md section 3.5): which
     *  house roles this venue's lights fill, and Fill Roles... */
    QGroupBox *rolesBox() const { return m_rolesBox; }
    QLabel *rolesText() const { return m_rolesText; }
    QPushButton *fillRolesButton() const { return m_fillRoles; }
    /** Fill Roles...: the suggestions for this venue's lights (not modal). */
    QDialog *openFillRoles();
    /** Line Up the Room: from @p path, else the room's file beside the
     *  show, else one chosen (Browse). False, said, when it can't. */
    bool lineUpRoom(const QString &path = QString());
    /** Match Groups...: the room's groups waiting for a match, against the
     *  show's (not modal). */
    QDialog *openMatchGroups();

    /** Columns of universeTable(). */
    enum Column { UniverseCol = 0, SlotCol, PatchCol, NotOursCol };

    /** Read the table's Not ours cell of @p row and keep it ("all",
     *  "401-512", "1-24, 500-512"; empty: none). False when it does not
     *  read (the cell is marked, nothing kept). */
    bool commitNotOurs(int row);

private:
    void refresh();

private:
    Doc *m_doc;
    QString m_venue;
    bool m_loading = false;
    QLabel *m_state;
    QLabel *m_house;
    QPushButton *m_moveIn;
    QTableWidget *m_table;
    QPushButton *m_keepPatch;
    QPushButton *m_forgetPatch;
    QListWidget *m_clashes;
    QLabel *m_noClashes;
    QGroupBox *m_room;
    QLabel *m_roomText;
    RoomOutputPicker *m_roomOutput;
    QPushButton *m_useRoomOutput;
    QLabel *m_waiting;
    QPushButton *m_matchGroups;
    QGroupBox *m_lineUpBox;
    QLabel *m_lineUpText;
    QLabel *m_downstageLabel;
    LengthSpinBox *m_downstage;
    QPushButton *m_lineUp;
    QGroupBox *m_rolesBox;
    QLabel *m_rolesText;
    QPushButton *m_fillRoles;
};

/** @} */

#endif
