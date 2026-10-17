/*
  Q Light Controller Plus
  creategroupsdialog.cpp

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

#include <QDialogButtonBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QTreeWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QSplitter>
#include <QLabel>

#include "createfixturegroup.h"
#include "creategroupsdialog.h"
#include "stagegroupdialog.h"
#include "fixturegroup.h"
#include "headlayout.h"
#include "doc.h"

#define COL_NAME  0
#define COL_HEADS 1
#define COL_GRID  2
#define COL_NOTE  3

CreateGroupsDialog::CreateGroupsDialog(Doc *doc, const QList<quint32> &fixtureIds,
                                       const QString &baseName, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_ids(fixtureIds)
{
    setWindowTitle(tr("Create Groups from Selection"));
    setProperty("helpTopic", QStringLiteral("fixture-manager.md#create-groups-from-selection"));
    QVBoxLayout *root = new QVBoxLayout(this);

    // What there is: "8 fixtures · 128 RGB, 32 White".
    QStringList parts;
    int heads = 0;
    foreach (const auto &tc, HeadTypeGroups::typeCounts(m_doc, m_ids))
    {
        parts << tr("%1 %2").arg(tc.second).arg(HeadKinds::typeName(tc.first));
        heads += tc.second;
    }
    const QString types = parts.join(QStringLiteral(", "));
    QLabel *what = new QLabel(m_ids.size() == 1
                                  ? tr("1 fixture, %1 heads: %2").arg(heads).arg(types)
                                  : tr("%1 fixtures, %2 heads: %3").arg(m_ids.size()).arg(heads).arg(types),
                              this);
    what->setWordWrap(true);
    root->addWidget(what);

    m_oneRadio = new QRadioButton(tr("One group with every head"), this);
    m_perTypeRadio = new QRadioButton(tr("A group per head type, across all the fixtures"), this);
    m_perFixtureRadio = new QRadioButton(tr("A group per head type for each fixture"), this);
    m_oneRadio->setToolTip(tr("Every head of every fixture in one group."));
    m_perTypeRadio->setToolTip(tr("Eight bars of 16 RGB pixels and 4 whites: one group of the "
                                  "128 RGB heads, one of the 32 white ones."));
    m_perFixtureRadio->setToolTip(tr("Eight bars of RGB and white heads: an RGB group and a "
                                     "white group for each bar -- 16 groups, in a folder."));
    QButtonGroup *splits = new QButtonGroup(this);
    splits->addButton(m_oneRadio, HeadTypeGroups::OneGroup);
    splits->addButton(m_perTypeRadio, HeadTypeGroups::PerType);
    splits->addButton(m_perFixtureRadio, HeadTypeGroups::PerTypePerFixture);
    root->addWidget(m_oneRadio);
    root->addWidget(m_perTypeRadio);
    QHBoxLayout *apartRow = new QHBoxLayout;
    apartRow->addSpacing(24);
    m_modelsApart = new QCheckBox(tr("Keep fixture models apart"), this);
    m_modelsApart->setToolTip(tr("Heads of the same type on different models -- RGB pixels on a "
                                 "bar and on a wash -- share one group unless this is on."));
    apartRow->addWidget(m_modelsApart);
    apartRow->addStretch();
    root->addLayout(apartRow);
    root->addWidget(m_perFixtureRadio);
    // The usual want for heads of more than one type is a group per type.
    const bool severalTypes = parts.size() > 1;
    (severalTypes ? m_perTypeRadio : m_oneRadio)->setChecked(true);

    QFormLayout *form = new QFormLayout;
    /* Given a name (the groups selected), every group is named after it;
       left as suggested, each group after its own fixtures. */
    m_givenBase = baseName.trimmed();
    m_suggestedBase = m_givenBase.isEmpty() ? HeadTypeGroups::baseNameFor(m_doc, m_ids) : QString();
    m_baseEdit = new QLineEdit(m_givenBase.isEmpty() ? m_suggestedBase : m_givenBase, this);
    m_baseEdit->setToolTip(tr("The groups are named after this and the head type: "
                              "“Bar · RGB”. A group per fixture takes the fixture's name."));
    form->addRow(tr("Name:"), m_baseEdit);
    m_folderEdit = new QLineEdit(this);
    m_folderEdit->setPlaceholderText(tr("none"));
    m_folderEdit->setToolTip(tr("The folder the groups go in (“Bars/Front”); a group per "
                                "fixture goes in one named after the selection."));
    form->addRow(tr("Folder:"), m_folderEdit);

    m_viewCombo = new QComboBox(this);
    m_viewCombo->addItem(tr("From the front — as the audience sees it"), int(CreateFixtureGroup::ViewFront));
    m_viewCombo->addItem(tr("From above — the plan"), int(CreateFixtureGroup::ViewTop));
    m_viewCombo->addItem(tr("Automatic"), int(CreateFixtureGroup::ViewAuto));
    m_closeGaps = new QCheckBox(tr("Close up the gaps"), this);
    m_closeGaps->setChecked(true);
    m_closeGaps->setToolTip(tr("Keep where each fixture is relative to the others, but drop "
                               "empty rows and columns -- a solid pixel map rather than the "
                               "rig to scale."));
    if (StageGroupDialog::anyOnStage(m_doc, m_ids))
    {
        m_viewCombo->setToolTip(tr("The heads are laid out where their fixtures stand in the "
                                   "Lighting Studio, seen this way."));
        form->addRow(tr("Laid out:"), m_viewCombo);
        form->addRow(QString(), m_closeGaps);
    }
    else
    {
        // Nowhere to take a picture from: say how they will be laid out.
        m_viewCombo->hide();
        m_closeGaps->hide();
        QLabel *how = new QLabel(tr("Not placed in the Lighting Studio: each fixture's heads "
                                    "go on rows of their own, one fixture under the other "
                                    "(single-head fixtures fill a square)."), this);
        how->setWordWrap(true);
        how->setEnabled(false);
        form->addRow(tr("Laid out:"), how);
    }
    root->addLayout(form);

    QSplitter *split = new QSplitter(Qt::Vertical, this);
    m_list = new QTreeWidget(split);
    m_list->setRootIsDecorated(false);
    m_list->setHeaderLabels(QStringList() << tr("Group") << tr("Heads") << tr("Grid") << QString());
    m_list->header()->setSectionResizeMode(COL_NAME, QHeaderView::Stretch);
    m_list->header()->setStretchLastSection(false);
    m_list->setToolTip(tr("The groups to make. Untick one to leave it out; double-click a "
                          "name to change it."));
    m_preview = new GroupPreviewWidget(split);
    split->addWidget(m_list);
    split->addWidget(m_preview);
    root->addWidget(split, 1);
    m_summary = new QLabel(this);
    root->addWidget(m_summary);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_createButton = bb->button(QDialogButtonBox::Ok);
    m_createButton->setText(tr("Create"));
    root->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(splits, &QButtonGroup::idToggled, this, [this](int, bool on) { if (on) slotReplan(); });
    connect(m_modelsApart, &QCheckBox::toggled, this, &CreateGroupsDialog::slotReplan);
    connect(m_viewCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CreateGroupsDialog::slotReplan);
    connect(m_closeGaps, &QCheckBox::toggled, this, &CreateGroupsDialog::slotReplan);
    connect(m_baseEdit, &QLineEdit::textEdited, this, &CreateGroupsDialog::slotReplan);
    connect(m_folderEdit, &QLineEdit::textEdited, this, [this]() {
        m_folderEdited = true;
        slotReplan();
    });
    connect(m_list, &QTreeWidget::itemChanged, this, &CreateGroupsDialog::slotItemChanged);
    connect(m_list, &QTreeWidget::currentItemChanged, this, &CreateGroupsDialog::slotCurrentChanged);

    resize(560, 560);
    slotReplan();
}

void CreateGroupsDialog::setSplit(HeadTypeGroups::Split split)
{
    (split == HeadTypeGroups::OneGroup ? m_oneRadio
     : split == HeadTypeGroups::PerType ? m_perTypeRadio : m_perFixtureRadio)->setChecked(true);
}

HeadTypeGroups::Options CreateGroupsDialog::options() const
{
    HeadTypeGroups::Options o;
    o.split = m_oneRadio->isChecked() ? HeadTypeGroups::OneGroup
            : m_perTypeRadio->isChecked() ? HeadTypeGroups::PerType
                                          : HeadTypeGroups::PerTypePerFixture;
    o.keepModelsApart = m_modelsApart->isChecked();
    o.view = m_viewCombo->currentData().toInt();
    o.closeGaps = m_closeGaps->isChecked();
    o.baseName = m_baseEdit->text().trimmed() == m_suggestedBase ? QString() : m_baseEdit->text();
    o.folder = m_folderEdit->text();
    return o;
}

void CreateGroupsDialog::slotReplan()
{
    const bool perFixture = m_perFixtureRadio->isChecked();
    m_modelsApart->setEnabled(m_perTypeRadio->isChecked());
    /* A group per fixture is many groups: they go in a folder named after
       the selection, until a folder is typed. */
    if (!m_folderEdited)
        m_folderEdit->setText(perFixture ? m_baseEdit->text().trimmed() : QString());

    const QString keep = m_list->currentItem() != nullptr
                             ? m_list->currentItem()->data(COL_NAME, Qt::UserRole).toString()
                             : QString();
    m_plan = HeadTypeGroups::plan(m_doc, m_ids, options());

    m_filling = true;
    m_list->clear();
    QTreeWidgetItem *current = nullptr;
    foreach (const HeadTypeGroups::Planned &p, m_plan)
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_list);
        item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsUserCheckable);
        item->setData(COL_NAME, Qt::UserRole, p.key);
        item->setText(COL_NAME, m_names.value(p.key, p.name));
        item->setText(COL_HEADS, QString::number(p.heads.size()));
        item->setTextAlignment(COL_HEADS, Qt::AlignRight | Qt::AlignVCenter);
        item->setText(COL_GRID, QStringLiteral("%1 × %2").arg(p.size.width()).arg(p.size.height()));
        // One already holding exactly these heads is left out unless ticked.
        bool make = true;
        if (FixtureGroup *same = m_doc->fixtureGroup(p.sameAs))
        {
            item->setText(COL_NOTE, tr("exists as “%1”").arg(same->name()));
            item->setToolTip(COL_NOTE, tr("A group with exactly these heads already exists. "
                                          "Tick it to make another anyway."));
            make = false;
        }
        item->setCheckState(COL_NAME, m_ticks.value(p.key, make) ? Qt::Checked : Qt::Unchecked);
        if (p.key == keep)
            current = item;
    }
    for (int c = COL_HEADS; c <= COL_NOTE; ++c)
        m_list->resizeColumnToContents(c);
    m_filling = false;
    m_list->setCurrentItem(current != nullptr ? current : m_list->topLevelItem(0));
    slotItemChanged(nullptr, -1);
    slotCurrentChanged();
}

void CreateGroupsDialog::slotItemChanged(QTreeWidgetItem *item, int column)
{
    if (m_filling)
        return;
    if (item != nullptr)
    {
        const QString key = item->data(COL_NAME, Qt::UserRole).toString();
        foreach (const HeadTypeGroups::Planned &p, m_plan)
        {
            if (p.key != key)
                continue;
            if (column == COL_NAME)
            {
                const QString name = item->text(COL_NAME).trimmed();
                if (name.isEmpty() || name == p.name)
                    m_names.remove(key);
                else
                    m_names.insert(key, name);
                const bool make = item->checkState(COL_NAME) == Qt::Checked;
                const bool byDefault = (m_doc->fixtureGroup(p.sameAs) == nullptr);
                if (make == byDefault)
                    m_ticks.remove(key);
                else
                    m_ticks.insert(key, make);
            }
            break;
        }
    }
    const int n = chosenGroups().size();
    m_summary->setText(n == 1 ? tr("1 group to make") : tr("%1 groups to make").arg(n));
    m_createButton->setEnabled(n > 0);
}

void CreateGroupsDialog::slotCurrentChanged()
{
    QTreeWidgetItem *item = m_list->currentItem();
    const QString key = item != nullptr ? item->data(COL_NAME, Qt::UserRole).toString() : QString();
    foreach (const HeadTypeGroups::Planned &p, m_plan)
        if (p.key == key)
        {
            m_preview->setLayout(p.size, p.heads);
            return;
        }
    m_preview->setLayout(QSize(), QMap<QLCPoint, GroupHead>());
}

QList<HeadTypeGroups::Planned> CreateGroupsDialog::chosenGroups() const
{
    QList<HeadTypeGroups::Planned> out;
    for (int i = 0; i < m_list->topLevelItemCount() && i < m_plan.size(); ++i)
    {
        const QTreeWidgetItem *item = m_list->topLevelItem(i);
        if (item->checkState(COL_NAME) != Qt::Checked)
            continue;
        HeadTypeGroups::Planned p = m_plan.at(i);
        const QString name = item->text(COL_NAME).trimmed();
        if (!name.isEmpty())
            p.name = name;
        out << p;
    }
    return out;
}

void CreateGroupsDialog::accept()
{
    const QList<HeadTypeGroups::Planned> groups = chosenGroups();
    if (groups.isEmpty())
        return;
    m_created = HeadTypeGroups::create(m_doc, groups);
    QDialog::accept();
}
