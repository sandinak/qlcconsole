/*
  Q Light Controller Plus
  stagegroupdialog.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPainter>
#include <QLabel>

#include "createfixturegroup.h"
#include "monitorproperties.h"
#include "stagegroupdialog.h"
#include "fixturegroup.h"
#include "fixture.h"
#include "doc.h"

/*****************************************************************************
 * GroupPreviewWidget
 *****************************************************************************/

GroupPreviewWidget::GroupPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(320, 120);
}

void GroupPreviewWidget::setLayout(const QSize &size, const QMap<QLCPoint, GroupHead> &heads)
{
    m_size = size;
    m_heads = heads;
    update();
}

void GroupPreviewWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(28, 30, 36));
    if (!m_size.isValid() || m_heads.isEmpty())
        return;
    // Square cells, as big as fit.
    const double cell = qMin((width() - 4.0) / m_size.width(), (height() - 4.0) / m_size.height());
    const double ox = (width() - cell * m_size.width()) / 2.0;
    const double oy = (height() - cell * m_size.height()) / 2.0;
    QMapIterator<QLCPoint, GroupHead> it(m_heads);
    while (it.hasNext())
    {
        it.next();
        // One colour per fixture, so where each one went reads at a glance.
        const QColor c = QColor::fromHsv(int((it.value().fxi * 47) % 360), 150, 220);
        const QRectF r(ox + it.key().x() * cell, oy + it.key().y() * cell,
                       qMax(1.0, cell - (cell > 4 ? 1 : 0)), qMax(1.0, cell - (cell > 4 ? 1 : 0)));
        p.fillRect(r, c);
    }
}

/*****************************************************************************
 * StageGroupDialog
 *****************************************************************************/

bool StageGroupDialog::anyOnStage(Doc *doc, const QList<quint32> &fixtureIds)
{
    MonitorProperties *mp = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (mp == nullptr)
        return false;
    foreach (quint32 fid, fixtureIds)
        if (!mp->fixtureRigPosition(fid).isNull())
            return true;
    return false;
}

StageGroupDialog::StageGroupDialog(Doc *doc, const QList<quint32> &fixtureIds,
                                   const QString &name, QWidget *parent,
                                   const QList<quint32> &groupIds)
    : QDialog(parent)
    , m_doc(doc)
    , m_ids(fixtureIds)
    , m_groupIds(groupIds)
{
    setWindowTitle(tr("New Fixture Group"));
    QVBoxLayout *root = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout;
    m_nameEdit = new QLineEdit(name, this);
    form->addRow(tr("Name:"), m_nameEdit);

    m_viewCombo = new QComboBox(this);
    m_viewCombo->addItem(tr("From the front — as the audience sees it"), int(CreateFixtureGroup::ViewFront));
    m_viewCombo->addItem(tr("From above — the plan"), int(CreateFixtureGroup::ViewTop));
    m_viewCombo->addItem(tr("Automatic"), int(CreateFixtureGroup::ViewAuto));
    m_viewCombo->setToolTip(tr("Which way the fixtures are laid out. From the front: across the "
                               "stage, and up the grid is up -- the step fronts become one "
                               "picture. From above: across, and up the grid is upstage -- for "
                               "the tops."));
    form->addRow(tr("Laid out:"), m_viewCombo);

    m_closeGaps = new QCheckBox(tr("Close up the gaps"), this);
    m_closeGaps->setChecked(true);
    m_closeGaps->setToolTip(tr("Keep where each fixture is relative to the others, but drop "
                               "empty rows and columns -- a solid pixel map rather than the "
                               "rig to scale."));
    form->addRow(QString(), m_closeGaps);

    if (!m_groupIds.isEmpty())
    {
        m_keepGroups = new QCheckBox(tr("Keep each group's own layout"), this);
        m_keepGroups->setChecked(true);
        m_keepGroups->setToolTip(tr("Each group goes in as a block, as it is laid out now, and "
                                    "the view sets where the blocks go. Unchecked, every "
                                    "fixture is laid out afresh from the studio in this view."));
        form->addRow(QString(), m_keepGroups);
        connect(m_keepGroups, &QCheckBox::toggled, this, &StageGroupDialog::rebuild);
    }
    root->addLayout(form);

    m_preview = new GroupPreviewWidget(this);
    root->addWidget(m_preview, 1);
    m_sizeLabel = new QLabel(this);
    root->addWidget(m_sizeLabel);
    m_sameNote = new QLabel(this);
    m_sameNote->setWordWrap(true);
    m_sameNote->setStyleSheet("color:#e0a040;");
    m_sameNote->hide();
    root->addWidget(m_sameNote);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_okButton = bb->addButton(tr("Make Group"), QDialogButtonBox::AcceptRole);
    root->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_viewCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StageGroupDialog::rebuild);
    connect(m_closeGaps, &QCheckBox::toggled, this, &StageGroupDialog::rebuild);
    resize(620, 440);
    rebuild();
}

void StageGroupDialog::rebuild()
{
    // Worked out on a group that is not in the show, then copied.
    {
        FixtureGroup probe(m_doc);
        probe.setSize(QSize(1, 1));
        const int view = m_viewCombo->currentData().toInt();
        if (m_keepGroups != nullptr && m_keepGroups->isChecked())
            CreateFixtureGroup::layoutGroupsByStage(m_doc, m_groupIds, &probe, view,
                                                    m_closeGaps->isChecked());
        else
            CreateFixtureGroup::layoutFixturesByStage(m_doc, m_ids, &probe, view,
                                                      m_closeGaps->isChecked());
        m_size = probe.size();
        m_heads = probe.headsMap();
        m_tags = probe.headSubGroupMap();
    }
    m_preview->setLayout(m_size, m_heads);
    m_sizeLabel->setText(tr("%1 × %2 cells, %3 heads from %4 fixtures")
                             .arg(m_size.width()).arg(m_size.height())
                             .arg(m_heads.size()).arg(m_ids.size()));
    m_same = CreateFixtureGroup::findSameGroup(m_doc, m_heads, m_editing);
    m_sameNote->setVisible(m_same != nullptr);
    if (m_same != nullptr)
        m_sameNote->setText(tr("“%1” already has exactly these heads.").arg(m_same->name()));
}

void StageGroupDialog::setEditing(FixtureGroup *existing)
{
    m_editing = existing;
    if (existing == nullptr)
        return;
    setWindowTitle(tr("Lay Out “%1” from the Stage").arg(existing->name()));
    m_nameEdit->setText(existing->name());
    m_okButton->setText(tr("Apply"));
    rebuild();
}

void StageGroupDialog::accept()
{
    if (m_heads.isEmpty())
        return;
    if (m_editing != nullptr)
    {
        // The same group, laid out again: anything using it keeps it.
        const QString name = m_nameEdit->text().trimmed();
        if (!name.isEmpty())
            m_editing->setName(name);
        m_editing->restoreState(m_size, m_heads, m_tags);
        m_doc->setModified();
        m_group = m_editing;
        QDialog::accept();
        return;
    }
    if (m_same != nullptr)
    {
        QMessageBox box(QMessageBox::Question, windowTitle(),
                        tr("“%1” already has exactly these heads. Make another group of them "
                           "anyway?").arg(m_same->name()), QMessageBox::Cancel, this);
        QPushButton *keep = box.addButton(tr("Keep “%1”").arg(m_same->name()), QMessageBox::AcceptRole);
        QPushButton *another = box.addButton(tr("Make Another"), QMessageBox::DestructiveRole);
        box.setDefaultButton(keep);
        box.exec();
        if (box.clickedButton() == keep)
        {
            m_group = m_same;
            QDialog::accept();
            return;
        }
        if (box.clickedButton() != another)
            return;
    }
    FixtureGroup *grp = new FixtureGroup(m_doc);
    const QString name = m_nameEdit->text().trimmed();
    grp->setName(name.isEmpty() ? tr("New Group") : name);
    m_doc->addFixtureGroup(grp);
    grp->restoreState(m_size, m_heads, m_tags);
    m_doc->setModified();
    m_group = grp;
    QDialog::accept();
}
