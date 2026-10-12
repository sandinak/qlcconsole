/*
  Q Light Controller Plus
  stagegroupdialog.h

  Make a fixture group laid out the way the fixtures stand on stage, from the
  view it will be seen in: from the front, as the audience sees the rig (the
  step fronts as one picture), or from above (the step tops as a plan). Each
  fixture's own LED grid is placed where the fixture is and turned the way it
  is rigged. Named here, previewed here, and checked against the groups that
  already exist so the same one is not made twice.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef STAGEGROUPDIALOG_H
#define STAGEGROUPDIALOG_H

#include <QDialog>
#include <QMap>

#include "grouphead.h"
#include "qlcpoint.h"

class QComboBox;
class QCheckBox;
class QLineEdit;
class QLabel;
class FixtureGroup;
class Doc;

/** @addtogroup ui_fixtures
 * @{
 */

/** A fixture group's grid as it will be made: a cell per head, coloured per
 *  fixture, scaled to fit. */
class GroupPreviewWidget final : public QWidget
{
    Q_OBJECT
public:
    explicit GroupPreviewWidget(QWidget *parent = nullptr);
    void setLayout(const QSize &size, const QMap<QLCPoint, GroupHead> &heads);
    QSize sizeHint() const override { return QSize(520, 220); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QSize m_size;
    QMap<QLCPoint, GroupHead> m_heads;
};

class StageGroupDialog final : public QDialog
{
    Q_OBJECT
public:
    /** @p groupIds: made from these fixture groups (@p fixtureIds their
     *  fixtures) -- each can go in as a block keeping its own layout. */
    StageGroupDialog(Doc *doc, const QList<quint32> &fixtureIds, const QString &name,
                     QWidget *parent = nullptr, const QList<quint32> &groupIds = QList<quint32>());

    /** Lay out @p existing again instead of making a new group: its name,
     *  "Apply" rewrites its head layout in place. Call before exec(). */
    void setEditing(FixtureGroup *existing);

    /** The group made -- or the existing identical one kept -- after accept(). */
    FixtureGroup *group() const { return m_group; }

    /** True when any of @p fixtureIds stands somewhere on the stage plot, so
     *  a stage layout means something. */
    static bool anyOnStage(Doc *doc, const QList<quint32> &fixtureIds);

public slots:
    void accept() override;

private slots:
    void rebuild();

private:
    Doc *m_doc;
    QList<quint32> m_ids;
    QList<quint32> m_groupIds;
    QCheckBox *m_keepGroups = nullptr;  ///< from groups: each keeps its own layout
    QLineEdit *m_nameEdit;
    QComboBox *m_viewCombo;
    QCheckBox *m_closeGaps;
    GroupPreviewWidget *m_preview;
    QLabel *m_sizeLabel;
    QLabel *m_sameNote;
    QSize m_size;
    QMap<QLCPoint, GroupHead> m_heads;
    QMap<QLCPoint, quint32> m_tags;
    FixtureGroup *m_same = nullptr;     ///< an existing group with these heads
    FixtureGroup *m_group = nullptr;
    FixtureGroup *m_editing = nullptr;  ///< laying out this group again
    class QPushButton *m_okButton = nullptr;
};

/** @} */

#endif
