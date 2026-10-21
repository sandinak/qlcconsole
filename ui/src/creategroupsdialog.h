/*
  Q Light Controller Plus
  creategroupsdialog.h

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

#ifndef CREATEGROUPSDIALOG_H
#define CREATEGROUPSDIALOG_H

#include <QDialog>
#include <QHash>
#include <QList>

#include "headtypegroups.h"

class GroupPreviewWidget;
class QTreeWidgetItem;
class QRadioButton;
class QTreeWidget;
class QPushButton;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QFormLayout;
class QLineEdit;
class QLabel;
class Doc;

/** @addtogroup ui_fixtures
 * @{
 */

/** Group ▸ Create Group(s) from Selection…: one group of everything (the
 *  groups selected as blocks), a group per head type, or a group per head
 *  type for each fixture (HeadTypeGroups), with a live list of the groups it
 *  will make -- each one's name editable and its make-it box -- and the
 *  layout of the one picked in the list. */
class CreateGroupsDialog final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(CreateGroupsDialog)

public:
    /** @param groupIds  groups selected too: a block each in one group,
     *                   their fixtures' heads in a group per type
     *  @param baseName  what the groups are named after; empty: the groups'
     *                   names when only groups are selected, else what the
     *                   fixtures' names share */
    CreateGroupsDialog(Doc *doc, const QList<quint32> &fixtureIds,
                       const QList<quint32> &groupIds = QList<quint32>(),
                       const QString &baseName = QString(), QWidget *parent = nullptr);

    /** How many heads the selection has (0: nothing to make). */
    int heads() const { return m_headCount; }

    /** The groups as they will be made (ticked ones, names as edited). */
    QList<HeadTypeGroups::Planned> chosenGroups() const;
    /** The groups accept() made. */
    QList<FixtureGroup *> createdGroups() const { return m_created; }

    void setSplit(HeadTypeGroups::Split split);

public slots:
    void accept() override;

private slots:
    void slotReplan();
    void slotItemChanged(QTreeWidgetItem *item, int column);
    void slotCurrentChanged();

private:
    HeadTypeGroups::Options options() const;
    /** Made unless unticked: all but one that already exists -- except the
     *  one group, asked for by name. */
    bool tickedByDefault(const HeadTypeGroups::Planned &p) const;

private:
    Doc *m_doc;
    QList<quint32> m_loose;               ///< the fixtures selected
    QList<quint32> m_groupIds;            ///< the groups selected
    QList<quint32> m_ids;                 ///< both, as fixtures
    int m_headCount = 0;
    QRadioButton *m_oneRadio;
    QRadioButton *m_perTypeRadio;
    QRadioButton *m_perFixtureRadio;
    QCheckBox *m_modelsApart;
    QComboBox *m_viewCombo;
    QCheckBox *m_closeGaps;
    QCheckBox *m_keepBlocks = nullptr;
    QFormLayout *m_form;
    QWidget *m_gridRow;
    QSpinBox *m_gridWidth;
    QSpinBox *m_gridHeight;
    QLabel *m_howLabel = nullptr;
    QLineEdit *m_baseEdit;
    QLineEdit *m_folderEdit;
    bool m_folderEdited = false;
    QString m_givenBase;                  ///< the name the dialog was opened with
    QString m_suggestedBase;              ///< what the fixtures' names share
    QTreeWidget *m_list;
    GroupPreviewWidget *m_preview;
    QLabel *m_summary;
    QPushButton *m_createButton;

    QList<HeadTypeGroups::Planned> m_plan;
    QHash<QString, QString> m_names;      ///< key -> name as edited
    QHash<QString, bool> m_ticks;         ///< key -> make it, as set by hand
    bool m_filling = false;
    QList<FixtureGroup *> m_created;
};

/** @} */

#endif
