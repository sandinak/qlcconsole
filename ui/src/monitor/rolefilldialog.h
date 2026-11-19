/*
  Q Light Controller Plus
  rolefilldialog.h

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

#ifndef ROLEFILLDIALOG_H
#define ROLEFILLDIALOG_H

#include <QDialog>
#include <QList>
#include <QMap>

#include "houserole.h"

class QTableWidget;
class QPushButton;
class QCheckBox;
class QLabel;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Fill Roles (docs/design/VENUE_ROLES_DATUM.md section 3.5), from the venue
 * page's Roles box and the note New Venue from a Show closes with: one row
 * per house role with the venue's lights suggested for it and why, a Use
 * tick and a Flash button to find them in the room; one Fill Roles click
 * makes them the roles' fills (and, the first time, the role groups, the
 * standard cues and their House hits).
 */
class RoleFillDialog final : public QDialog
{
    Q_OBJECT

public:
    RoleFillDialog(Doc *doc, const QString &venue, QWidget *parent = nullptr);

    /** Open it for @p venue (not modal). */
    static RoleFillDialog *open(Doc *doc, QWidget *parent, const QString &venue);

    /** Columns of table(). */
    enum Column { RoleCol = 0, LightsCol, WhyCol, UseCol, FlashCol };

    QTableWidget *table() const { return m_table; }
    QPushButton *fillButton() const { return m_fill; }
    QPushButton *editButton() const { return m_edit; }
    QLabel *notUsedLabel() const { return m_notUsed; }
    QLabel *statusLabel() const { return m_status; }
    /** Row @p row's Use tick / Flash button (for tests and the shots). */
    QCheckBox *useBox(int row) const;
    QPushButton *flashButton(int row) const;
    /** The row of @p role. */
    int rowOf(HouseRole::Role role) const;

    /** Answer the "use your group" question before Fill Roles asks it
     *  (tests): 1 use the show's groups, 0 make new ones, -1 ask. */
    void setUseExistingAnswer(int answer) { m_useExistingAnswer = answer; }

    /** Fill Roles: the ticked rows become the roles' fills. False, said,
     *  when it can't. */
    bool fillRoles();

private:
    void refresh();
    void flashRow(int row);
    void editRow(int row);

private:
    Doc *m_doc;
    QString m_venue;
    QLabel *m_intro;
    QTableWidget *m_table;
    QLabel *m_notUsed;
    QLabel *m_status;
    QPushButton *m_fill;
    QPushButton *m_edit;
    QList<HouseRole::Role> m_rowRoles;
    QMap<HouseRole::Role, QList<quint32>> m_suggested;
    int m_useExistingAnswer = -1;
};

/** @} */

#endif
