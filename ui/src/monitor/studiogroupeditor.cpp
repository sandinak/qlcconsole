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
#include <QLabel>

#include "studiogroupeditor.h"
#include "doc.h"
#include "fixture.h"

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
    root->addWidget(bb);

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
    m_doc->setModified();
    emit changed();
}

void StudioGroupEditor::snapshot()
{
    MonitorProperties *props = m_doc->monitorProperties();
    m_snapGroup = props->group(m_groupId);
    m_snapRig.clear();
    foreach (quint32 fid, members())
        m_snapRig.insert(fid, props->fixtureRigProps(fid));
}

void StudioGroupEditor::revert()
{
    MonitorProperties *props = m_doc->monitorProperties();
    props->setGroupName(m_groupId, m_snapGroup.name);
    props->setGroupHasFrame(m_groupId, m_snapGroup.hasFrame);
    props->setGroupFrame(m_groupId, m_snapGroup.origin, m_snapGroup.rotation);
    for (auto it = m_snapRig.constBegin(); it != m_snapRig.constEnd(); ++it)
        props->setFixtureRigProps(it.key(), it.value());
    m_doc->setModified();
    emit changed();
}
