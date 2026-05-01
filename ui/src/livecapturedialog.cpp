/*
  Q Light Controller Plus
  livecapturedialog.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "doc.h"
#include "fixture.h"
#include "livecapturedialog.h"
#include "qlcchannel.h"
#include "qlcfixturemode.h"
#include "scene.h"

namespace
{
    enum RowRole
    {
        SceneIdRole = Qt::UserRole + 1,
        FixtureIdRole,
        ChannelRole
    };
}

LiveCaptureDialog::LiveCaptureDialog(Doc* doc, QWidget* parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_tree(nullptr)
    , m_applyButton(nullptr)
    , m_saveAsNewButton(nullptr)
{
    Q_ASSERT(doc != nullptr);
    setWindowTitle(tr("Live Edit Captures"));
    resize(720, 480);

    CaptureManager *cm = m_doc->captureManager();
    if (cm != nullptr)
        m_plan = cm->buildPlan();

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *header = new QLabel(this);
    if (m_plan.isEmpty())
    {
        header->setText(tr("No captured overrides could be matched to a "
                           "running scene. Edits were dropped."));
    }
    else
    {
        int totalChannels = 0;
        for (const CaptureManager::ScenePlan& p : m_plan)
            totalChannels += p.overrides.size();
        header->setText(tr("Captured %1 channel change(s) across %2 scene(s). "
                           "Uncheck rows you don't want to apply. Conflict rows "
                           "are channels asserted by other running scenes; "
                           "chaser-driven rows are pre-disabled.")
                            .arg(totalChannels).arg(m_plan.size()));
    }
    header->setWordWrap(true);
    layout->addWidget(header);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(5);
    m_tree->setHeaderLabels(QStringList()
                            << tr("Scene / Channel")
                            << tr("Fixture")
                            << tr("Old")
                            << tr("New")
                            << tr("Save as new (name)"));
    m_tree->header()->setStretchLastSection(true);
    layout->addWidget(m_tree, 1);

    buildTree();
    connect(m_tree, &QTreeWidget::itemChanged,
            this, &LiveCaptureDialog::slotItemChanged);

    QDialogButtonBox *buttons = new QDialogButtonBox(this);
    m_applyButton = buttons->addButton(tr("Apply to Scenes"),
                                       QDialogButtonBox::AcceptRole);
    m_saveAsNewButton = buttons->addButton(tr("Save as New"),
                                           QDialogButtonBox::ActionRole);
    QPushButton *cancel = buttons->addButton(tr("Discard"),
                                             QDialogButtonBox::RejectRole);

    bool hasAnything = !m_plan.isEmpty();
    m_applyButton->setEnabled(hasAnything);
    m_saveAsNewButton->setEnabled(hasAnything);

    connect(m_applyButton, &QPushButton::clicked, this, &LiveCaptureDialog::slotApply);
    connect(m_saveAsNewButton, &QPushButton::clicked, this, &LiveCaptureDialog::slotSaveAsNew);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString LiveCaptureDialog::fixtureLabel(quint32 fxi) const
{
    Fixture* fx = m_doc->fixture(fxi);
    if (fx == nullptr)
        return tr("(missing fixture %1)").arg(fxi);
    return fx->name();
}

QString LiveCaptureDialog::channelLabel(quint32 fxi, quint32 ch) const
{
    Fixture* fx = m_doc->fixture(fxi);
    if (fx == nullptr)
        return tr("Ch %1").arg(ch + 1);
    const QLCChannel* qch = fx->channel(ch);
    if (qch == nullptr)
        return tr("Ch %1").arg(ch + 1);
    return tr("Ch %1: %2").arg(ch + 1).arg(qch->name());
}

void LiveCaptureDialog::buildTree()
{
    // Building the tree fires itemChanged signals; suppress while we
    // populate so slotItemChanged doesn't try to mutate parents that
    // don't exist yet.
    const bool wasBlocked = m_tree->blockSignals(true);

    m_renameFields.clear();
    m_tree->clear();

    for (const CaptureManager::ScenePlan& p : m_plan)
    {
        if (p.scene == nullptr)
            continue;

        QTreeWidgetItem *sceneItem = new QTreeWidgetItem(m_tree);
        sceneItem->setText(0, p.scene->name());
        sceneItem->setData(0, SceneIdRole, p.scene->id());
        sceneItem->setFirstColumnSpanned(false);
        sceneItem->setExpanded(true);
        sceneItem->setFlags(sceneItem->flags()
                            | Qt::ItemIsUserCheckable
                            | Qt::ItemIsAutoTristate);
        sceneItem->setCheckState(0, Qt::Checked);

        QLineEdit *rename = new QLineEdit(m_tree);
        rename->setPlaceholderText(tr("%1 (Edit)").arg(p.scene->name()));
        m_renameFields.insert(p.scene->id(), rename);
        m_tree->setItemWidget(sceneItem, 4, rename);

        for (const CaptureManager::Override& o : p.overrides)
        {
            QPair<quint32, quint32> key = qMakePair(o.fxi, o.channel);
            QTreeWidgetItem *row = new QTreeWidgetItem(sceneItem);
            row->setData(0, FixtureIdRole, o.fxi);
            row->setData(0, ChannelRole, o.channel);
            row->setText(0, channelLabel(o.fxi, o.channel));
            row->setText(1, fixtureLabel(o.fxi));

            uchar oldValue = 0;
            for (const SceneValue& sv : p.scene->values())
            {
                if (sv.fxi == o.fxi && sv.channel == o.channel)
                {
                    oldValue = sv.value;
                    break;
                }
            }
            row->setText(2, QString::number(oldValue));
            row->setText(3, QString::number(o.value));

            QStringList tags;
            if (p.conflicts.contains(key))
                tags << tr("conflict");
            bool isChaser = p.chaserDriven.contains(key);
            if (isChaser)
                tags << tr("chaser-driven");
            if (!tags.isEmpty())
                row->setText(0, row->text(0) + "  [" + tags.join(", ") + "]");

            // Default checked. Chaser-driven rows pre-unchecked AND
            // disabled so the user can't override the safety skip.
            row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
            if (isChaser)
            {
                row->setCheckState(0, Qt::Unchecked);
                row->setFlags(row->flags() & ~Qt::ItemIsEnabled);
            }
            else
            {
                row->setCheckState(0, Qt::Checked);
            }
        }
    }
    for (int c = 0; c < 4; ++c)
        m_tree->resizeColumnToContents(c);

    m_tree->blockSignals(wasBlocked);
}

void LiveCaptureDialog::slotItemChanged(QTreeWidgetItem* item, int column)
{
    if (column != 0)
        return;
    // Cascade scene-level toggle to children that aren't permanently
    // disabled (chaser-driven). The Qt::ItemIsAutoTristate flag handles
    // child-to-parent propagation; we only need to do parent-to-child.
    if (item->parent() == nullptr)
    {
        Qt::CheckState s = item->checkState(0);
        if (s == Qt::PartiallyChecked)
            return;
        const bool wasBlocked = m_tree->blockSignals(true);
        for (int i = 0; i < item->childCount(); ++i)
        {
            QTreeWidgetItem *child = item->child(i);
            if ((child->flags() & Qt::ItemIsEnabled) == 0)
                continue; // chaser-driven rows stay unchecked
            child->setCheckState(0, s);
        }
        m_tree->blockSignals(wasBlocked);
    }
}

QList<CaptureManager::ScenePlan> LiveCaptureDialog::filteredPlan() const
{
    QHash<quint32, QSet<QPair<quint32, quint32> > > approvedByScene;

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *sceneItem = m_tree->topLevelItem(i);
        quint32 sceneId = sceneItem->data(0, SceneIdRole).toUInt();
        QSet<QPair<quint32, quint32> > approved;
        for (int j = 0; j < sceneItem->childCount(); ++j)
        {
            QTreeWidgetItem *row = sceneItem->child(j);
            if (row->checkState(0) != Qt::Checked)
                continue;
            quint32 fxi = row->data(0, FixtureIdRole).toUInt();
            quint32 ch = row->data(0, ChannelRole).toUInt();
            approved.insert(qMakePair(fxi, ch));
        }
        if (!approved.isEmpty())
            approvedByScene.insert(sceneId, approved);
    }

    QList<CaptureManager::ScenePlan> out;
    for (const CaptureManager::ScenePlan& p : m_plan)
    {
        if (p.scene == nullptr)
            continue;
        if (!approvedByScene.contains(p.scene->id()))
            continue;
        const QSet<QPair<quint32, quint32> >& approved = approvedByScene.value(p.scene->id());
        CaptureManager::ScenePlan filtered;
        filtered.scene = p.scene;
        filtered.conflicts = p.conflicts;
        filtered.chaserDriven = p.chaserDriven;
        for (const CaptureManager::Override& o : p.overrides)
        {
            if (approved.contains(qMakePair(o.fxi, o.channel)))
                filtered.overrides.append(o);
        }
        if (!filtered.overrides.isEmpty())
            out.append(filtered);
    }
    return out;
}

void LiveCaptureDialog::slotApply()
{
    CaptureManager *cm = m_doc->captureManager();
    if (cm != nullptr)
        cm->applyInPlace(filteredPlan());
    accept();
}

void LiveCaptureDialog::slotSaveAsNew()
{
    QHash<quint32, QString> names;
    for (auto it = m_renameFields.constBegin(); it != m_renameFields.constEnd(); ++it)
    {
        QString text = it.value()->text().trimmed();
        if (!text.isEmpty())
            names.insert(it.key(), text);
    }
    CaptureManager *cm = m_doc->captureManager();
    if (cm != nullptr)
        cm->saveAsNew(filteredPlan(), names);
    accept();
}
