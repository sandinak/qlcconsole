/*
  Q Light Controller Plus
  studiogroupeditor.cpp

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

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QGroupBox>
#include <QComboBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>

#include <climits>

#include "studiogroupeditor.h"
#include "studiotemplate.h"
#include "studioplaneview.h"
#include "doc.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "grouphead.h"

StudioGroupEditor::StudioGroupEditor(Doc *doc, quint32 groupId, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_groupId(groupId)
{
    Q_ASSERT(m_doc != nullptr);
    MonitorProperties *props = m_doc->monitorProperties();
    const MonitorProperties::MonitorGroup g = props->group(m_groupId);

    setWindowTitle(tr("Studio Group — %1").arg(g.name));
    resize(460, 420);
    snapshot();

    QVBoxLayout *root = new QVBoxLayout(this);

    // --- Frame (rigid move/rotate of the whole unit) -----------------------
    QGroupBox *frameBox = new QGroupBox(tr("Frame"), this);
    QFormLayout *form = new QFormLayout(frameBox);

    m_name = new QLineEdit(g.name, frameBox);
    form->addRow(tr("Name"), m_name);

    auto mkSpin = [&](double lo, double hi, const QString &suffix) {
        QDoubleSpinBox *s = new QDoubleSpinBox(frameBox);
        s->setRange(lo, hi);
        s->setDecimals(3);
        s->setSingleStep(0.05);
        s->setSuffix(suffix);
        return s;
    };
    m_ox  = mkSpin(-1000.0, 1000.0, tr(" m"));
    m_oy  = mkSpin(-1000.0, 1000.0, tr(" m"));
    m_oz  = mkSpin(-1000.0, 1000.0, tr(" m"));
    m_rot = mkSpin(-360.0, 360.0, tr(" °"));
    m_rot->setSingleStep(1.0);
    m_ox->setValue(double(g.origin.x()));
    m_oy->setValue(double(g.origin.y()));
    m_oz->setValue(double(g.origin.z()));
    m_rot->setValue(double(g.rotation));

    QHBoxLayout *originRow = new QHBoxLayout;
    originRow->addWidget(new QLabel(tr("X"))); originRow->addWidget(m_ox);
    originRow->addWidget(new QLabel(tr("Y"))); originRow->addWidget(m_oy);
    originRow->addWidget(new QLabel(tr("Z"))); originRow->addWidget(m_oz);
    form->addRow(tr("Origin"), originRow);
    form->addRow(tr("Rotation"), m_rot);

    QPushButton *recenter = new QPushButton(tr("Re-center origin on members"), frameBox);
    form->addRow(QString(), recenter);
    root->addWidget(frameBox);

    // --- FixtureGroup binding (Phase 2) ------------------------------------
    QGroupBox *bindBox = new QGroupBox(tr("FixtureGroup binding"), this);
    QFormLayout *bindForm = new QFormLayout(bindBox);
    m_fgCombo = new QComboBox(bindBox);
    m_fgCombo->addItem(tr("(none)"), 0u);
    foreach (FixtureGroup *fg, m_doc->fixtureGroups())
    {
        m_fgCombo->addItem(fg->name(), fg->id());
        if (fg->id() == g.boundFxGroup)
            m_fgCombo->setCurrentIndex(m_fgCombo->count() - 1);
    }
    bindForm->addRow(tr("Bind to"), m_fgCombo);

    m_pitchX = mkSpin(0.001, 100.0, tr(" m/cell"));
    m_pitchY = mkSpin(0.001, 100.0, tr(" m/cell"));
    m_pitchX->setValue(double(g.pitchX > 0 ? g.pitchX : 0.5f));
    m_pitchY->setValue(double(g.pitchY > 0 ? g.pitchY : 0.5f));
    QHBoxLayout *pitchRow = new QHBoxLayout;
    pitchRow->addWidget(new QLabel(tr("X"))); pitchRow->addWidget(m_pitchX);
    pitchRow->addWidget(new QLabel(tr("Y"))); pitchRow->addWidget(m_pitchY);
    bindForm->addRow(tr("Cell pitch"), pitchRow);

    QHBoxLayout *bindBtns = new QHBoxLayout;
    QPushButton *seedBtn  = new QPushButton(tr("Seed from cells"), bindBox);
    QPushButton *adoptBtn = new QPushButton(tr("Adopt current positions"), bindBox);
    seedBtn->setToolTip(tr("Lay the bound group's fixtures out on a grid at the "
                           "cell pitch (their matrix layout → metric offsets)"));
    adoptBtn->setToolTip(tr("Pull the bound group's fixtures in at where they "
                            "already sit on the map (non-destructive)"));
    bindBtns->addWidget(seedBtn);
    bindBtns->addWidget(adoptBtn);
    bindForm->addRow(QString(), bindBtns);
    root->addWidget(bindBox);

    connect(seedBtn,  &QPushButton::clicked, this, &StudioGroupEditor::seedFromFixtureGroup);
    connect(adoptBtn, &QPushButton::clicked, this, &StudioGroupEditor::adoptFromFixtureGroup);

    // --- Graphical layout (drag members in Top / Front / Side) -------------
    QGroupBox *planeBox = new QGroupBox(tr("Layout"), this);
    QVBoxLayout *planeLay = new QVBoxLayout(planeBox);
    QHBoxLayout *planeTop = new QHBoxLayout;
    m_planeCombo = new QComboBox(planeBox);
    m_planeCombo->addItem(tr("Top (X → / Y ↓)"),  int(StudioPlaneView::Top));
    m_planeCombo->addItem(tr("Front (X → / Z ↑)"), int(StudioPlaneView::Front));
    m_planeCombo->addItem(tr("Side (Y → / Z ↑)"),  int(StudioPlaneView::Side));
    planeTop->addWidget(new QLabel(tr("View")));
    planeTop->addWidget(m_planeCombo);
    planeTop->addStretch(1);
    planeTop->addWidget(new QLabel(tr("drag a fixture to place it")));
    planeLay->addLayout(planeTop);
    m_plane = new StudioPlaneView(m_doc, m_groupId, planeBox);
    planeLay->addWidget(m_plane, 1);
    root->addWidget(planeBox, 1);

    connect(m_planeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
        m_plane->setPlane(StudioPlaneView::Plane(m_planeCombo->currentData().toInt()));
    });
    connect(m_plane, &StudioPlaneView::changed, this, [this]() {
        if (!m_loading) { rebuildTable(); emit changed(); }
    });

    // --- Members (per-member group-local offset) ---------------------------
    QGroupBox *memBox = new QGroupBox(tr("Members (group-local offset, metres)"), this);
    QVBoxLayout *memLay = new QVBoxLayout(memBox);
    m_table = new QTableWidget(memBox);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels(QStringList()
        << tr("Fixture") << tr("Local X") << tr("Local Y") << tr("Local Z"));
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    memLay->addWidget(m_table);
    root->addWidget(memBox, 1);

    QDialogButtonBox *bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QPushButton *saveTplBtn = bb->addButton(tr("Save as Template…"),
                                            QDialogButtonBox::ActionRole);
    root->addWidget(bb);

    connect(saveTplBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(this,
            tr("Save Studio Template"), QString(),
            tr("Fixture Studio Template (*.json)"));
        if (path.isEmpty())
            return;
        QString err;
        if (!StudioTemplate::saveGroup(m_doc, m_groupId, path, &err))
            QMessageBox::warning(this, tr("Save Template"),
                                 tr("Could not save template: %1").arg(err));
    });

    rebuildTable();

    connect(m_name, &QLineEdit::editingFinished, this, &StudioGroupEditor::applyFrame);
    connect(m_ox,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &StudioGroupEditor::applyFrame);
    connect(m_oy,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &StudioGroupEditor::applyFrame);
    connect(m_oz,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &StudioGroupEditor::applyFrame);
    connect(m_rot, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &StudioGroupEditor::applyFrame);
    connect(m_table, &QTableWidget::cellChanged, this, &StudioGroupEditor::applyMemberCell);
    connect(recenter, &QPushButton::clicked, this, &StudioGroupEditor::recenterOrigin);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, [this]() { revert(); reject(); });
}

QList<quint32> StudioGroupEditor::members() const
{
    QList<quint32> out;
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (quint32 fid, props->fixtureItemsID())
        if (props->fixtureFrameGroup(fid) == m_groupId)
            out << fid;
    return out;
}

void StudioGroupEditor::rebuildTable()
{
    m_loading = true;
    MonitorProperties *props = m_doc->monitorProperties();
    const QList<quint32> ids = members();
    m_table->setRowCount(ids.size());
    for (int r = 0; r < ids.size(); ++r)
    {
        const quint32 fid = ids[r];
        Fixture *fx = m_doc->fixture(fid);
        const QString nm = fx ? fx->name() : tr("Fixture %1").arg(fid);
        const QVector3D lp = props->fixtureRigProps(fid).groupLocal;

        QTableWidgetItem *n = new QTableWidgetItem(nm);
        n->setData(Qt::UserRole, fid);
        n->setFlags(n->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(r, 0, n);
        const float v[3] = { lp.x(), lp.y(), lp.z() };
        for (int c = 0; c < 3; ++c)
            m_table->setItem(r, c + 1,
                new QTableWidgetItem(QString::number(double(v[c]), 'f', 3)));
    }
    m_loading = false;
}

void StudioGroupEditor::applyFrame()
{
    if (m_loading)
        return;
    MonitorProperties *props = m_doc->monitorProperties();
    props->setGroupName(m_groupId, m_name->text());
    props->setGroupFrame(m_groupId,
        QVector3D(float(m_ox->value()), float(m_oy->value()), float(m_oz->value())),
        float(m_rot->value()));
    m_doc->setModified();
    emit changed();
}

void StudioGroupEditor::applyMemberCell(int row, int col)
{
    if (m_loading || col < 1 || col > 3)
        return;
    QTableWidgetItem *n = m_table->item(row, 0);
    if (n == nullptr)
        return;
    const quint32 fid = n->data(Qt::UserRole).toUInt();
    MonitorProperties *props = m_doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(fid);
    QVector3D lp = rp.groupLocal;
    const float val = float(m_table->item(row, col)->text().toDouble());
    if (col == 1) lp.setX(val);
    else if (col == 2) lp.setY(val);
    else lp.setZ(val);
    rp.groupLocal = lp;
    props->setFixtureRigProps(fid, rp);
    if (m_plane) m_plane->reload();
    m_doc->setModified();
    emit changed();
}

void StudioGroupEditor::recenterOrigin()
{
    MonitorProperties *props = m_doc->monitorProperties();
    const QList<quint32> ids = members();
    if (ids.isEmpty())
        return;

    // Current world position of each member (derived from the present frame).
    QMap<quint32, QVector3D> world;
    QVector3D sum;
    foreach (quint32 fid, ids)
    {
        const QVector3D w = props->groupLocalToWorld(m_groupId,
                                props->fixtureRigProps(fid).groupLocal);
        world.insert(fid, w);
        sum += w;
    }
    const QVector3D newOrigin = sum / float(ids.size());

    // Move origin, then recompute each local so members do not visually move.
    props->setGroupOrigin(m_groupId, newOrigin);
    foreach (quint32 fid, ids)
    {
        FixtureRigProps rp = props->fixtureRigProps(fid);
        rp.groupLocal = props->worldToGroupLocal(m_groupId, world.value(fid));
        props->setFixtureRigProps(fid, rp);
    }

    m_loading = true;
    m_ox->setValue(double(newOrigin.x()));
    m_oy->setValue(double(newOrigin.y()));
    m_oz->setValue(double(newOrigin.z()));
    m_loading = false;
    rebuildTable();
    if (m_plane) m_plane->reload();
    m_doc->setModified();
    emit changed();
}

quint32 StudioGroupEditor::selectedFxGroup() const
{
    return m_fgCombo ? m_fgCombo->currentData().toUInt() : 0u;
}

void StudioGroupEditor::seedFromFixtureGroup()
{
    const quint32 fgid = selectedFxGroup();
    FixtureGroup *fg = m_doc->fixtureGroup(fgid);
    if (fg == nullptr)
        return;
    MonitorProperties *props = m_doc->monitorProperties();

    const float px = float(m_pitchX->value());
    const float py = float(m_pitchY->value());
    props->setGroupBinding(m_groupId, fgid, px, py);
    props->setGroupHasFrame(m_groupId, true);

    // Cell layout → metric local offsets. The cell matrix itself is left
    // untouched — this only projects it into the studio frame (decoupled).
    const QMap<QLCPoint, GroupHead> heads = fg->headsMap();

    // Centre on the ACTUALLY OCCUPIED cell bounds (not the declared grid size,
    // which can be larger than the occupied area or not start at 0,0) so the
    // seeded block is centred on the frame origin and lands where expected.
    int minX = INT_MAX, maxX = INT_MIN, minY = INT_MAX, maxY = INT_MIN;
    for (auto b = heads.constBegin(); b != heads.constEnd(); ++b)
    {
        minX = qMin(minX, b.key().x()); maxX = qMax(maxX, b.key().x());
        minY = qMin(minY, b.key().y()); maxY = qMax(maxY, b.key().y());
    }
    const float cx = (minX <= maxX) ? (minX + maxX) / 2.0f : 0.0f;
    const float cy = (minY <= maxY) ? (minY + maxY) / 2.0f : 0.0f;
    int placed = 0;
    for (auto it = heads.constBegin(); it != heads.constEnd(); ++it)
    {
        const quint32 fid = it.value().fxi;
        if (fid == Fixture::invalidId() || !props->containsFixture(fid))
            continue;   // fixture must be on the 2D map to be placed
        rememberFixture(fid);
        props->setFixtureGroup(fid, m_groupId);
        FixtureRigProps rp = props->fixtureRigProps(fid);
        rp.groupLocal = QVector3D((it.key().x() - cx) * px,
                                  (it.key().y() - cy) * py, 0.0f);
        props->setFixtureRigProps(fid, rp);
        ++placed;
    }
    Q_UNUSED(placed);
    rebuildTable();
    if (m_plane) m_plane->reload();
    m_doc->setModified();
    emit changed();
}

void StudioGroupEditor::adoptFromFixtureGroup()
{
    const quint32 fgid = selectedFxGroup();
    FixtureGroup *fg = m_doc->fixtureGroup(fgid);
    if (fg == nullptr)
        return;
    MonitorProperties *props = m_doc->monitorProperties();

    props->setGroupBinding(m_groupId, fgid,
                           float(m_pitchX->value()), float(m_pitchY->value()));
    props->setGroupHasFrame(m_groupId, true);

    // Pull the bound group's fixtures in, keeping where they already sit — read
    // each world position BEFORE it joins the frame (else fixtureRigPosition
    // returns the not-yet-set derived value), then convert to a local offset.
    foreach (quint32 fid, fg->fixtureList())
    {
        if (!props->containsFixture(fid))
            continue;
        const QVector3D world = props->fixtureRigPosition(fid);
        rememberFixture(fid);
        props->setFixtureGroup(fid, m_groupId);
        FixtureRigProps rp = props->fixtureRigProps(fid);
        rp.groupLocal = props->worldToGroupLocal(m_groupId, world);
        props->setFixtureRigProps(fid, rp);
    }
    rebuildTable();
    if (m_plane) m_plane->reload();
    m_doc->setModified();
    emit changed();
}

void StudioGroupEditor::rememberFixture(quint32 fid)
{
    if (m_snapRig.contains(fid))
        return;
    MonitorProperties *props = m_doc->monitorProperties();
    m_snapRig.insert(fid, props->fixtureRigProps(fid));
    m_snapGroupOf.insert(fid, props->fixtureGroup(fid));
}

void StudioGroupEditor::snapshot()
{
    MonitorProperties *props = m_doc->monitorProperties();
    m_snapGroup = props->group(m_groupId);
    m_snapRig.clear();
    m_snapGroupOf.clear();
    foreach (quint32 fid, members())
        rememberFixture(fid);
}

void StudioGroupEditor::revert()
{
    MonitorProperties *props = m_doc->monitorProperties();
    props->setGroupName(m_groupId, m_snapGroup.name);
    props->setGroupHasFrame(m_groupId, m_snapGroup.hasFrame);
    props->setGroupFrame(m_groupId, m_snapGroup.origin, m_snapGroup.rotation);
    props->setGroupBinding(m_groupId, m_snapGroup.boundFxGroup,
                           m_snapGroup.pitchX, m_snapGroup.pitchY);
    for (auto it = m_snapRig.constBegin(); it != m_snapRig.constEnd(); ++it)
        props->setFixtureRigProps(it.key(), it.value());
    // Restore prior group membership (unwinds seed/adopt pull-ins).
    for (auto it = m_snapGroupOf.constBegin(); it != m_snapGroupOf.constEnd(); ++it)
        props->setFixtureGroup(it.key(), it.value());
    m_doc->setModified();
    emit changed();
}
