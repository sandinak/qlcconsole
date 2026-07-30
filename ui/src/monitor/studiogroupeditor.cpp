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
#include <QListWidget>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>

#include <climits>
#include <QSet>

#include "studiogroupeditor.h"
#include "studiotemplate.h"
#include "studioplaneview.h"
#include "doc.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "grouphead.h"
#include "stageplatform.h"

StudioGroupEditor::StudioGroupEditor(Doc *doc, quint32 groupId, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_groupId(groupId)
{
    Q_ASSERT(m_doc != nullptr);
    MonitorProperties *props = m_doc->monitorProperties();
    const MonitorProperties::MonitorGroup g = props->group(m_groupId);

    setWindowTitle(tr("Studio Group — %1").arg(g.name));
    resize(760, 500);
    snapshot();

    QHBoxLayout *outer = new QHBoxLayout(this);

    // Left: the group's fixtures. Multi-select here, then Distribute / Put on
    // face acts on the selection — like selecting in the 2D Layers panel.
    QVBoxLayout *leftCol = new QVBoxLayout;
    leftCol->addWidget(new QLabel(tr("Fixtures (multi-select)")));
    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setMinimumWidth(190);
    leftCol->addWidget(m_list, 1);
    QPushButton *addFxBtn = new QPushButton(tr("Add fixtures…"), this);
    addFxBtn->setToolTip(tr("Pull patched fixtures into this studio group "
                            "(they keep their current position)"));
    leftCol->addWidget(addFxBtn);
    outer->addLayout(leftCol);
    connect(m_list, &QListWidget::itemSelectionChanged,
            this, &StudioGroupEditor::syncHighlight);
    connect(addFxBtn, &QPushButton::clicked, this, &StudioGroupEditor::addFixtures);

    QVBoxLayout *root = new QVBoxLayout;
    outer->addLayout(root, 1);

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

    // FixtureGroup → face: put the picked group's fixtures onto a chosen face.
    QHBoxLayout *faceRow = new QHBoxLayout;
    m_faceCombo = new QComboBox(bindBox);
    m_faceCombo->addItem(tr("Top"),   0);
    m_faceCombo->addItem(tr("Front"), 1);
    m_faceCombo->addItem(tr("Side"),  2);
    m_faceCombo->setCurrentIndex(1);
    QPushButton *grpFaceBtn = new QPushButton(tr("Put group on face"), bindBox);
    grpFaceBtn->setToolTip(tr("Assign the picked FixtureGroup's fixtures to this "
                              "face and distribute them on it"));
    faceRow->addWidget(new QLabel(tr("onto")));
    faceRow->addWidget(m_faceCombo);
    faceRow->addWidget(grpFaceBtn);
    faceRow->addStretch(1);
    bindForm->addRow(QString(), faceRow);
    root->addWidget(bindBox);

    connect(seedBtn,  &QPushButton::clicked, this, &StudioGroupEditor::seedFromFixtureGroup);
    connect(adoptBtn, &QPushButton::clicked, this, &StudioGroupEditor::adoptFromFixtureGroup);
    connect(grpFaceBtn, &QPushButton::clicked, this, &StudioGroupEditor::putGroupOnFace);

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
    QPushButton *putBtn = new QPushButton(tr("Put on this face"), planeBox);
    putBtn->setToolTip(tr("Move the selected fixtures onto the face shown, "
                          "keeping their arrangement"));
    planeTop->addWidget(putBtn);
    QPushButton *distBtn = new QPushButton(tr("Distribute"), planeBox);
    distBtn->setToolTip(tr("Spread the selected fixtures evenly on this face, in "
                           "name order (all on this face if none selected)"));
    planeTop->addWidget(distBtn);
    planeTop->addStretch(1);
    planeLay->addLayout(planeTop);
    connect(distBtn, &QPushButton::clicked, this, &StudioGroupEditor::distributeEvenly);
    connect(putBtn, &QPushButton::clicked, this, &StudioGroupEditor::putSelectedOnFace);
    m_plane = new StudioPlaneView(m_doc, m_groupId, planeBox);
    planeLay->addWidget(m_plane, 1);
    root->addWidget(planeBox, 1);

    connect(m_planeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
        m_plane->setPlane(StudioPlaneView::Plane(m_planeCombo->currentData().toInt()));
    });
    connect(m_plane, &StudioPlaneView::changed, this, [this]() {
        if (!m_loading) { rebuildList(); populateSelectionStrip(); emit changed(); }
    });

    // --- Selected fixture(s): compact property strip (replaces the table) ---
    QGroupBox *selBox = new QGroupBox(tr("Selected fixture(s)"), this);
    QFormLayout *selForm = new QFormLayout(selBox);
    m_selFace = new QComboBox(selBox);
    m_selFace->addItem(tr("Top"), 0);
    m_selFace->addItem(tr("Front"), 1);
    m_selFace->addItem(tr("Side"), 2);
    selForm->addRow(tr("Face"), m_selFace);
    m_selAngle = mkSpin(-360.0, 360.0, tr(" °")); m_selAngle->setSingleStep(1.0);
    selForm->addRow(tr("Angle"), m_selAngle);
    m_selX = mkSpin(-1000.0, 1000.0, tr(" m"));
    m_selY = mkSpin(-1000.0, 1000.0, tr(" m"));
    m_selZ = mkSpin(-1000.0, 1000.0, tr(" m"));
    QHBoxLayout *xyzRow = new QHBoxLayout;
    xyzRow->addWidget(new QLabel(tr("X"))); xyzRow->addWidget(m_selX);
    xyzRow->addWidget(new QLabel(tr("Y"))); xyzRow->addWidget(m_selY);
    xyzRow->addWidget(new QLabel(tr("Z"))); xyzRow->addWidget(m_selZ);
    selForm->addRow(tr("Local"), xyzRow);
    root->addWidget(selBox);
    root->addStretch(1);

    connect(m_selFace, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { applySelection(); });
    connect(m_selAngle, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &StudioGroupEditor::applySelection);
    connect(m_selX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &StudioGroupEditor::applySelection);
    connect(m_selY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &StudioGroupEditor::applySelection);
    connect(m_selZ, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &StudioGroupEditor::applySelection);

    QDialogButtonBox *bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QPushButton *saveTplBtn = bb->addButton(tr("Save as Component…"),
                                            QDialogButtonBox::ActionRole);
    saveTplBtn->setToolTip(tr("Save this layout to the browsable component library"));
    root->addWidget(bb);

    connect(saveTplBtn, &QPushButton::clicked, this, [this]() {
        // Save into the shared library under a display name, so it shows up in the
        // component browser (no path hunting). Default the name to the group name.
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Save as Component"),
            tr("Component name:"), QLineEdit::Normal,
            m_name ? m_name->text() : QString(), &ok);
        if (!ok || name.trimmed().isEmpty())
            return;
        QString err;
        const QString path = StudioTemplate::saveToLibrary(m_doc, m_groupId, name, &err);
        if (path.isEmpty())
        {
            QMessageBox::warning(this, tr("Save as Component"),
                                 tr("Could not save component: %1").arg(err));
            return;
        }
        QMessageBox::information(this, tr("Save as Component"),
            tr("Saved \"%1\" to the component library.").arg(name.trimmed()));
    });

    rebuildList();
    populateSelectionStrip();

    connect(m_name, &QLineEdit::editingFinished, this, &StudioGroupEditor::applyFrame);
    connect(m_ox,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &StudioGroupEditor::applyFrame);
    connect(m_oy,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &StudioGroupEditor::applyFrame);
    connect(m_oz,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &StudioGroupEditor::applyFrame);
    connect(m_rot, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &StudioGroupEditor::applyFrame);
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

void StudioGroupEditor::rebuildList()
{
    if (m_list == nullptr)
        return;
    m_loading = true;
    MonitorProperties *props = m_doc->monitorProperties();

    // Preserve the selection across the rebuild.
    QSet<quint32> wasSel;
    foreach (QListWidgetItem *it, m_list->selectedItems())
        wasSel.insert(it->data(Qt::UserRole).toUInt());

    m_list->blockSignals(true);
    m_list->clear();
    static const char *faceName[3] = { "Top", "Front", "Side" };
    foreach (quint32 fid, members())
    {
        Fixture *fx = m_doc->fixture(fid);
        const int mount = qBound(0, props->fixtureRigProps(fid).studioMount, 2);
        QListWidgetItem *li = new QListWidgetItem(
            tr("%1  ·  %2").arg(fx ? fx->name() : tr("Fixture %1").arg(fid))
                           .arg(QString::fromLatin1(faceName[mount])));
        li->setData(Qt::UserRole, fid);
        m_list->addItem(li);
        if (wasSel.contains(fid))
            li->setSelected(true);
    }
    m_list->blockSignals(false);
    m_loading = false;
}

void StudioGroupEditor::populateSelectionStrip()
{
    const QList<quint32> ids = selectedFixtureIds();
    const bool one = !ids.isEmpty();
    m_selFace->setEnabled(one);
    m_selAngle->setEnabled(one);
    m_selX->setEnabled(one);
    m_selY->setEnabled(one);
    m_selZ->setEnabled(one);
    if (!one)
        return;

    // Show the first selected fixture's values (edits apply to the whole set).
    const FixtureRigProps rp = m_doc->monitorProperties()->fixtureRigProps(ids.first());
    m_loading = true;
    m_selFace->setCurrentIndex(qBound(0, rp.studioMount, 2));
    m_selAngle->setValue(double(rp.studioAngle));
    m_selX->setValue(double(rp.groupLocal.x()));
    m_selY->setValue(double(rp.groupLocal.y()));
    m_selZ->setValue(double(rp.groupLocal.z()));
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

void StudioGroupEditor::applySelection()
{
    if (m_loading)
        return;
    const QList<quint32> ids = selectedFixtureIds();
    if (ids.isEmpty())
        return;
    MonitorProperties *props = m_doc->monitorProperties();
    const int mount = m_selFace->currentData().toInt();
    const QVector3D local(float(m_selX->value()), float(m_selY->value()), float(m_selZ->value()));
    const float angle = float(m_selAngle->value());

    foreach (quint32 fid, ids)
    {
        FixtureRigProps rp = props->fixtureRigProps(fid);
        rp.studioMount = mount;
        rp.studioAngle = angle;
        // For a single selection, set the exact local; for many, only the face/
        // angle apply (leaving each where it sits) — moving them all to one XYZ
        // would stack them.
        if (ids.size() == 1)
            rp.groupLocal = local;
        props->setFixtureRigProps(fid, rp);
    }
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
    rebuildList();
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
    rebuildList();
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
    rebuildList();
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

QList<quint32> StudioGroupEditor::selectedFixtureIds() const
{
    QList<quint32> out;
    if (m_list != nullptr)
        foreach (QListWidgetItem *it, m_list->selectedItems())
            out << it->data(Qt::UserRole).toUInt();
    return out;
}

void StudioGroupEditor::syncHighlight()
{
    if (m_plane != nullptr)
        m_plane->setHighlight(selectedFixtureIds());
    populateSelectionStrip();
}

void StudioGroupEditor::distributeEvenly()
{
    // The plane view owns the geometry; it lays out the current selection (or
    // this face's fixtures if none selected). Its changed() refreshes our list.
    if (m_plane != nullptr)
        m_plane->distributeEvenly(selectedFixtureIds());
}

void StudioGroupEditor::putSelectedOnFace()
{
    if (m_plane != nullptr)
        m_plane->assignToFace(selectedFixtureIds());
}

void StudioGroupEditor::addFixtures()
{
    MonitorProperties *props = m_doc->monitorProperties();

    // Candidates: every fixture on the 2D map that is not already a member here.
    QList<quint32> cand;
    foreach (quint32 fid, props->fixtureItemsID())
        if (props->fixtureFrameGroup(fid) != m_groupId)
            cand << fid;
    std::sort(cand.begin(), cand.end(), [this](quint32 a, quint32 b) {
        Fixture *fa = m_doc->fixture(a), *fb = m_doc->fixture(b);
        return (fa ? fa->name() : QString()) < (fb ? fb->name() : QString());
    });
    if (cand.isEmpty())
    {
        QMessageBox::information(this, tr("Add fixtures"),
            tr("Every patched fixture on the map is already in this studio group."));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Add fixtures to studio group"));
    dlg.resize(360, 420);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->addWidget(new QLabel(tr("Selected fixtures are pulled in at their current "
                                 "position (adopted into the frame)."), &dlg));
    QListWidget *pick = new QListWidget(&dlg);
    pick->setSelectionMode(QAbstractItemView::ExtendedSelection);
    foreach (quint32 fid, cand)
    {
        Fixture *fx = m_doc->fixture(fid);
        QString label = fx ? fx->name() : tr("Fixture %1").arg(fid);
        // Flag a fixture that currently lives in ANOTHER studio group.
        const quint32 other = props->fixtureFrameGroup(fid);
        if (other != 0 && other != m_groupId)
            label += tr("  ·  in %1").arg(props->group(other).name);
        QListWidgetItem *li = new QListWidgetItem(label, pick);
        li->setData(Qt::UserRole, fid);
    }
    lay->addWidget(pick, 1);
    QDialogButtonBox *bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QList<quint32> chosen;
    foreach (QListWidgetItem *it, pick->selectedItems())
        chosen << it->data(Qt::UserRole).toUInt();
    if (chosen.isEmpty())
        return;

    // This IS a studio group now (a plain group opened via promote already has a
    // frame, but be defensive so "add" also works as a bootstrap).
    props->setGroupHasFrame(m_groupId, true);
    foreach (quint32 fid, chosen)
    {
        // Read the world position BEFORE it joins the frame, then convert to a
        // group-local offset so it does not visually jump (same as adopt).
        const QVector3D world = props->fixtureRigPosition(fid);
        rememberFixture(fid);
        props->setFixtureGroup(fid, m_groupId);
        FixtureRigProps rp = props->fixtureRigProps(fid);
        rp.groupLocal = props->worldToGroupLocal(m_groupId, world);
        props->setFixtureRigProps(fid, rp);
    }
    rebuildList();
    if (m_plane) m_plane->reload();
    m_doc->setModified();
    emit changed();
}

void StudioGroupEditor::putGroupOnFace()
{
    const quint32 fgid = selectedFxGroup();
    FixtureGroup *fg = m_doc->fixtureGroup(fgid);
    if (fg == nullptr || m_plane == nullptr || m_faceCombo == nullptr)
        return;
    MonitorProperties *props = m_doc->monitorProperties();

    QList<quint32> ids;
    foreach (quint32 fid, fg->fixtureList())
    {
        if (!props->containsFixture(fid))
            continue;
        rememberFixture(fid);
        props->setFixtureGroup(fid, m_groupId);   // join the studio group
        ids << fid;
    }
    if (ids.isEmpty())
        return;

    // Switch the view to the chosen face, then assign + distribute on it.
    const int face = m_faceCombo->currentData().toInt();
    m_planeCombo->setCurrentIndex(face);          // triggers m_plane->setPlane
    m_plane->distributeEvenly(ids);
    rebuildList();
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
