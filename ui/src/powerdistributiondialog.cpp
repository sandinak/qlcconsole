/*
  Q Light Controller Plus
  powerdistributiondialog.cpp

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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTreeWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QSplitter>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QSettings>
#include <QBrush>
#include <QColor>
#include <algorithm>

#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>

#include "powerdistributiondialog.h"
#include "powerdistribution.h"
#include "venue.h"
#include "inputoutputmap.h"
#include "universe.h"
#include "fixture.h"
#include "function.h"
#include "scene.h"
#include "doc.h"

#define SETTINGS_GEOMETRY     "powerdistribution/geometry"
#define KDefaultVoltageKey    "power/defaultVoltage"
#define KDefaultDerateKey     "power/deratePercent"
#define KDefaultBreakerKey    "power/defaultBreakerAmps"
#define KPowerFactorKey       "power/powerFactor"

// Item data roles on the sources/circuits tree
enum { RoleType = Qt::UserRole, RoleSource, RoleCircuit, RoleFixtureId };
enum { TypeSource = 0, TypeCircuit = 1, TypeFixture = 2 };

// Sources/circuits tree columns
enum { ColName = 0, ColVolts = 1, ColAmps = 2, ColConnector = 3, ColLoad = 4, ColFull = 5 };

/** Rough US connector suggestion from voltage + breaker rating, so the user
 *  knows what plug a circuit needs. Best-effort hint, not exhaustive. */
static QString suggestedConnector(double volts, double amps)
{
    const int v = int(volts + 0.5);
    const int a = int(amps + 0.5);
    if (v >= 100 && v <= 130) // 120 V leg
    {
        if (a <= 15) return QStringLiteral("Edison 5-15");
        if (a <= 20) return QStringLiteral("Edison 5-20 / L5-20");
        if (a <= 30) return QStringLiteral("L5-30");
        if (a <= 50) return QStringLiteral("CS6364 / 14-50");
    }
    else if (v >= 200 && v <= 250) // 208/240 V
    {
        if (a <= 20) return QStringLiteral("L6-20");
        if (a <= 30) return QStringLiteral("L6-30 / L14-30");
        if (a <= 50) return QStringLiteral("CS6365 / 14-50");
    }
    return QString();
}

PowerDistributionDialog::PowerDistributionDialog(Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setWindowTitle(tr("Power Distribution"));
    resize(940, 480);

    QVBoxLayout *top = new QVBoxLayout(this);

    QSplitter *split = new QSplitter(Qt::Horizontal, this);

    // --- Left: sources / circuits ---
    QWidget *left = new QWidget(this);
    QVBoxLayout *lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->addWidget(new QLabel(tr("Power sources & circuits"), left));

    m_tree = new QTreeWidget(left);
    m_tree->setColumnCount(6);
    m_tree->setHeaderLabels(QStringList()
                            << tr("Name") << tr("Voltage") << tr("Rated A")
                            << tr("Connector") << tr("Live load") << tr("Full load"));
    m_tree->header()->setStretchLastSection(true);
    m_tree->setRootIsDecorated(true);
    lv->addWidget(m_tree, 1);

    QHBoxLayout *lb = new QHBoxLayout();
    m_addSourceBtn = new QPushButton(tr("Add source"), left);
    m_addCircuitBtn = new QPushButton(tr("Add circuit"), left);
    m_removeBtn = new QPushButton(tr("Remove"), left);
    lb->addWidget(m_addSourceBtn);
    lb->addWidget(m_addCircuitBtn);
    lb->addWidget(m_removeBtn);
    lb->addStretch(1);
    m_sceneCheckBtn = new QPushButton(tr("Power Check…"), left);
    m_sceneCheckBtn->setToolTip(tr("Report every scene and collection's load on "
                                   "each circuit, flagging overloads."));
    lb->addWidget(m_sceneCheckBtn);
    lv->addLayout(lb);

    // UPS / battery properties for the selected source. A non-zero VA rating
    // turns a source into a UPS: its load is checked against the VA capacity and
    // runtime is estimated from the datasheet point (minutes @ watts).
    m_sourceBox = new QGroupBox(tr("Source power (UPS / battery)"), left);
    QFormLayout *sf = new QFormLayout(m_sourceBox);
    m_vaSpin = new QDoubleSpinBox(m_sourceBox);
    m_vaSpin->setRange(0, 1000000);
    m_vaSpin->setDecimals(0);
    m_vaSpin->setSuffix(tr(" VA"));
    m_vaSpin->setSpecialValueText(tr("— mains (no UPS) —")); // shown at 0
    sf->addRow(tr("VA rating:"), m_vaSpin);

    QWidget *runRow = new QWidget(m_sourceBox);
    QHBoxLayout *rl = new QHBoxLayout(runRow);
    rl->setContentsMargins(0, 0, 0, 0);
    m_runMinSpin = new QDoubleSpinBox(runRow);
    m_runMinSpin->setRange(0, 100000);
    m_runMinSpin->setDecimals(0);
    m_runMinSpin->setSuffix(tr(" min"));
    m_runWattSpin = new QDoubleSpinBox(runRow);
    m_runWattSpin->setRange(0, 1000000);
    m_runWattSpin->setDecimals(0);
    m_runWattSpin->setSuffix(tr(" W"));
    rl->addWidget(m_runMinSpin);
    rl->addWidget(new QLabel(tr("at"), runRow));
    rl->addWidget(m_runWattSpin);
    rl->addStretch(1);
    sf->addRow(tr("Rated runtime:"), runRow);

    m_sourceSummary = new QLabel(m_sourceBox);
    m_sourceSummary->setWordWrap(true);
    sf->addRow(m_sourceSummary);

    lv->addWidget(m_sourceBox);
    split->addWidget(left);

    // --- Right: fixtures ---
    QWidget *right = new QWidget(this);
    QVBoxLayout *rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->addWidget(new QLabel(tr("Fixtures"), right));

    m_fixtureList = new QTreeWidget(right);
    m_fixtureList->setColumnCount(3);
    m_fixtureList->setHeaderLabels(QStringList()
                                   << tr("Fixture") << tr("Circuit") << tr("Watts"));
    m_fixtureList->setRootIsDecorated(false);
    m_fixtureList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fixtureList->header()->setStretchLastSection(true);
    rv->addWidget(m_fixtureList, 1);

    QHBoxLayout *rb = new QHBoxLayout();
    m_assignBtn = new QPushButton(tr("Assign to selected circuit"), right);
    m_unassignBtn = new QPushButton(tr("Unassign"), right);
    m_autoBtn = new QPushButton(tr("Auto-assign…"), right);
    rb->addWidget(m_assignBtn);
    rb->addWidget(m_unassignBtn);
    rb->addStretch(1);
    rb->addWidget(m_autoBtn);
    rv->addLayout(rb);
    split->addWidget(right);

    split->setStretchFactor(0, 5);
    split->setStretchFactor(1, 3);
    top->addWidget(split, 1);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    // Venue import/export — circuit infrastructure portable across shows (no
    // fixture assignments). Reachable here while Power is the only venue section;
    // moves to a dedicated Venue menu as trusses/platforms/etc. are added.
    QPushButton *importBtn = bb->addButton(tr("Import Venue…"), QDialogButtonBox::ActionRole);
    QPushButton *exportBtn = bb->addButton(tr("Export Venue…"), QDialogButtonBox::ActionRole);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(importBtn, &QPushButton::clicked, this, &PowerDistributionDialog::slotImportVenue);
    connect(exportBtn, &QPushButton::clicked, this, &PowerDistributionDialog::slotExportVenue);
    top->addWidget(bb);

    connect(m_addSourceBtn, &QPushButton::clicked, this, &PowerDistributionDialog::slotAddSource);
    connect(m_addCircuitBtn, &QPushButton::clicked, this, &PowerDistributionDialog::slotAddCircuit);
    connect(m_removeBtn, &QPushButton::clicked, this, &PowerDistributionDialog::slotRemoveSelected);
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &PowerDistributionDialog::slotTreeSelectionChanged);
    connect(m_tree, &QTreeWidget::itemChanged,
            this, &PowerDistributionDialog::slotTreeItemChanged);
    connect(m_assignBtn, &QPushButton::clicked, this, &PowerDistributionDialog::slotAssign);
    connect(m_unassignBtn, &QPushButton::clicked, this, &PowerDistributionDialog::slotUnassign);
    connect(m_autoBtn, &QPushButton::clicked, this, &PowerDistributionDialog::slotAutoAssign);
    connect(m_sceneCheckBtn, &QPushButton::clicked, this, &PowerDistributionDialog::slotSceneCheck);
    connect(m_vaSpin, SIGNAL(valueChanged(double)), this, SLOT(slotSourceUpsChanged()));
    connect(m_runMinSpin, SIGNAL(valueChanged(double)), this, SLOT(slotSourceUpsChanged()));
    connect(m_runWattSpin, SIGNAL(valueChanged(double)), this, SLOT(slotSourceUpsChanged()));

    QSettings settings;
    QVariant geom = settings.value(SETTINGS_GEOMETRY);
    if (geom.isValid())
        restoreGeometry(geom.toByteArray());

    computeFixtureWatts();
    rebuildTree();
    rebuildFixtures();
    slotTreeSelectionChanged();
}

PowerDistributionDialog::~PowerDistributionDialog()
{
    QSettings settings;
    settings.setValue(SETTINGS_GEOMETRY, saveGeometry());
}

void PowerDistributionDialog::computeFixtureWatts()
{
    m_fixtureWatts.clear();
    QList<Universe*> universes = m_doc->inputOutputMap()->claimUniverses();
    PowerEstimator::PreGMReader reader =
        [&universes](quint32 uni, quint32 addr) -> uchar
        {
            if (uni >= quint32(universes.size()))
                return 0;
            return universes[uni]->preGMValue(int(addr));
        };
    PowerEstimator::estimateWatts(m_doc, reader, &m_fixtureWatts);
    m_doc->inputOutputMap()->releaseUniverses(false);

    // Full-rated draw per fixture (everything at full) — for the worst-case
    // "would this trip if everything went full" check. No universe read needed.
    m_fixtureFullWatts.clear();
    foreach (Fixture *fx, m_doc->fixtures())
        if (fx != NULL)
            m_fixtureFullWatts.insert(fx->id(), PowerEstimator::fixtureFullWatts(fx));
}

double PowerDistributionDialog::circuitAmpsFrom(int sourceIdx, int circuitIdx,
                                                const QHash<quint32, double> &watts) const
{
    PowerDistribution *pd = m_doc->powerDistribution();
    const QList<PowerSource> &srcs = pd->sources();
    if (sourceIdx < 0 || sourceIdx >= srcs.size())
        return 0.0;
    const PowerSource &src = srcs.at(sourceIdx);
    if (circuitIdx < 0 || circuitIdx >= src.circuits.size())
        return 0.0;

    QSettings settings;
    const double dflt = settings.value(KDefaultVoltageKey, 120.0).toDouble();
    const double srcV = (src.voltage > 0.0) ? src.voltage : dflt;
    const PowerCircuit &cir = src.circuits.at(circuitIdx);
    const double v = cir.effectiveVoltage(srcV);

    double w = 0.0;
    foreach (quint32 fxId, cir.fixtures)
        w += watts.value(fxId, 0.0);
    return w / v;
}

double PowerDistributionDialog::circuitAmps(int sourceIdx, int circuitIdx) const
{
    return circuitAmpsFrom(sourceIdx, circuitIdx, m_fixtureWatts);
}

double PowerDistributionDialog::circuitFullAmps(int sourceIdx, int circuitIdx) const
{
    return circuitAmpsFrom(sourceIdx, circuitIdx, m_fixtureFullWatts);
}

double PowerDistributionDialog::sourceWatts(int sourceIdx) const
{
    PowerDistribution *pd = m_doc->powerDistribution();
    if (sourceIdx < 0 || sourceIdx >= pd->sources().size())
        return 0.0;
    double w = 0.0;
    foreach (const PowerCircuit &cir, pd->sources().at(sourceIdx).circuits)
        foreach (quint32 fxId, cir.fixtures)
            w += m_fixtureWatts.value(fxId, 0.0);
    return w;
}

void PowerDistributionDialog::refreshSourcePanel()
{
    QTreeWidgetItem *it = m_tree->currentItem();
    int s = -1;
    if (it != NULL && it->data(0, RoleType).toInt() == TypeSource)
        s = it->data(0, RoleSource).toInt();
    else if (it != NULL && it->data(0, RoleType).toInt() == TypeCircuit)
        s = it->data(0, RoleSource).toInt(); // editing the circuit's parent source

    m_panelSource = s;
    PowerDistribution *pd = m_doc->powerDistribution();
    const bool valid = (s >= 0 && s < pd->sources().size());
    m_sourceBox->setEnabled(valid);

    m_panelUpdating = true;
    if (valid)
    {
        const PowerSource &src = pd->sources().at(s);
        m_sourceBox->setTitle(tr("Source power — %1").arg(src.name));
        m_vaSpin->setValue(src.vaRating);
        m_runMinSpin->setValue(src.runtimeMinutes);
        m_runWattSpin->setValue(src.runtimeWatts);

        const double w = sourceWatts(s);
        QSettings settings;
        const double pf = settings.value(KPowerFactorKey, 0.9).toDouble();
        const double va = (pf > 0.0) ? w / pf : w;
        QString txt = tr("Load: %1 kW / %2 kVA (PF %3)")
                .arg(w / 1000.0, 0, 'f', 2).arg(va / 1000.0, 0, 'f', 2)
                .arg(pf, 0, 'f', 2);
        if (src.isUPS())
        {
            const double rt = src.estimateRuntimeMinutes(w);
            if (va > src.vaRating)
                txt += tr("  ⚠ exceeds %1 VA").arg(src.vaRating, 0, 'f', 0);
            txt += rt >= 0.0 ? tr("\nEstimated runtime: ~%1 min").arg(rt, 0, 'f', 0)
                             : tr("\nEstimated runtime: set a rated-runtime point above");
        }
        m_sourceSummary->setText(txt);
    }
    else
    {
        m_sourceBox->setTitle(tr("Source power (UPS / battery)"));
        m_vaSpin->setValue(0);
        m_runMinSpin->setValue(0);
        m_runWattSpin->setValue(0);
        m_sourceSummary->setText(tr("Select a source to set UPS / battery limits."));
    }
    m_panelUpdating = false;
}

void PowerDistributionDialog::slotSourceUpsChanged()
{
    if (m_panelUpdating)
        return;
    PowerDistribution *pd = m_doc->powerDistribution();
    if (m_panelSource < 0 || m_panelSource >= pd->sources().size())
        return;

    pd->sources()[m_panelSource].vaRating = m_vaSpin->value();
    pd->sources()[m_panelSource].runtimeMinutes = m_runMinSpin->value();
    pd->sources()[m_panelSource].runtimeWatts = m_runWattSpin->value();
    m_doc->setModified();

    // Refresh the source row's summary column + the panel's live readout.
    m_tree->blockSignals(true);
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *si = m_tree->topLevelItem(i);
        if (si->data(0, RoleSource).toInt() == m_panelSource)
        {
            updateSourceRow(si, m_panelSource);
            break;
        }
    }
    m_tree->blockSignals(false);
    refreshSourcePanel();
}

void PowerDistributionDialog::populateCircuitItem(QTreeWidgetItem *ci, int s, int c)
{
    PowerDistribution *pd = m_doc->powerDistribution();
    const PowerSource &src = pd->sources().at(s);
    const PowerCircuit &cir = src.circuits.at(c);
    const double v = cir.effectiveVoltage(src.voltage);

    ci->setText(ColName, cir.name);
    // Show the effective voltage; italic when inherited from the source so it's
    // clear which circuits carry an explicit override.
    ci->setText(ColVolts, QString::number(v, 'f', 0));
    QFont f = ci->font(ColVolts);
    f.setItalic(cir.voltage <= 0.0);
    ci->setFont(ColVolts, f);
    ci->setToolTip(ColVolts, cir.voltage <= 0.0
                   ? tr("Inherited from source (edit to override; clear to re-inherit)")
                   : QString());

    ci->setText(ColAmps, QString::number(cir.ratedAmps, 'f', 0));
    ci->setText(ColConnector, suggestedConnector(v, cir.ratedAmps));

    const double limit = cir.deratedLimit();
    const double amps = circuitAmps(s, c);
    ci->setText(ColLoad, tr("%1 / %2 A (%3%)")
                .arg(amps, 0, 'f', 1).arg(limit, 0, 'f', 1).arg(cir.deratePercent));

    // Full-rated worst case: everything on the circuit at full intensity.
    const double full = circuitFullAmps(s, c);
    const bool fullOver = full > limit;
    ci->setText(ColFull, tr("%1 A%2").arg(full, 0, 'f', 1)
                .arg(fullOver ? tr("  ⚠ TRIPS") : QString()));
    ci->setToolTip(ColFull, tr("Load if every fixture on this circuit goes to "
                               "full intensity. ⚠ = exceeds the %1 A derated limit.")
                   .arg(limit, 0, 'f', 1));

    ci->setData(0, RoleType, TypeCircuit);
    ci->setData(0, RoleSource, s);
    ci->setData(0, RoleCircuit, c);
    ci->setFlags(ci->flags() | Qt::ItemIsEditable);

    // Red row = LIVE overload (happening now); red Full-load text = would trip
    // at full intensity (planning warning).
    const bool over = amps > limit;
    const QBrush red(QColor(192, 57, 43));
    for (int col = 0; col < m_tree->columnCount(); col++)
    {
        ci->setForeground(col, over ? QBrush(Qt::white) : QBrush());
        ci->setBackground(col, over ? red : QBrush());
    }
    if (fullOver && !over)
        ci->setForeground(ColFull, QBrush(QColor(192, 57, 43)));
}

void PowerDistributionDialog::updateSourceRow(QTreeWidgetItem *si, int s)
{
    PowerDistribution *pd = m_doc->powerDistribution();
    if (s < 0 || s >= pd->sources().size())
        return;
    const PowerSource &src = pd->sources().at(s);
    const double w = sourceWatts(s);

    QSettings settings;
    const double pf = settings.value(KPowerFactorKey, 0.9).toDouble();
    const double va = (pf > 0.0) ? w / pf : w;

    bool over = false;
    if (src.isUPS())
    {
        si->setText(ColConnector, tr("UPS"));
        const double rt = src.estimateRuntimeMinutes(w);
        QString load = tr("%1 / %2 kVA").arg(va / 1000.0, 0, 'f', 2)
                       .arg(src.vaRating / 1000.0, 0, 'f', 2);
        if (rt >= 0.0)
            load += tr(" · ~%1 min").arg(rt, 0, 'f', 0);
        si->setText(ColLoad, load);
        over = (va > src.vaRating);
    }
    else
    {
        si->setText(ColConnector, QString());
        si->setText(ColLoad, tr("Σ %1 kW").arg(w / 1000.0, 0, 'f', 2));
    }

    const QBrush red(QColor(192, 57, 43));
    for (int col = 0; col < m_tree->columnCount(); col++)
    {
        si->setForeground(col, over ? QBrush(Qt::white) : QBrush());
        si->setBackground(col, over ? red : QBrush());
    }
}

void PowerDistributionDialog::rebuildTree()
{
    m_tree->blockSignals(true);
    m_tree->clear();

    PowerDistribution *pd = m_doc->powerDistribution();
    const QList<PowerSource> &srcs = pd->sources();
    for (int s = 0; s < srcs.size(); s++)
    {
        const PowerSource &src = srcs.at(s);
        QTreeWidgetItem *si = new QTreeWidgetItem(m_tree);
        si->setText(ColName, src.name);
        si->setText(ColVolts, QString::number(src.voltage, 'f', 0));
        si->setData(0, RoleType, TypeSource);
        si->setData(0, RoleSource, s);
        si->setFlags(si->flags() | Qt::ItemIsEditable);
        si->setExpanded(true);

        for (int c = 0; c < src.circuits.size(); c++)
        {
            QTreeWidgetItem *ci = new QTreeWidgetItem(si);
            populateCircuitItem(ci, s, c);

            // Nest each circuit's assigned fixtures beneath it, so you can see
            // circuit → fixtures at a glance (display-only rows).
            foreach (quint32 fxId, src.circuits.at(c).fixtures)
            {
                Fixture *fx = m_doc->fixture(fxId);
                QTreeWidgetItem *fi = new QTreeWidgetItem(ci);
                fi->setText(ColName, fx ? fx->name() : tr("(missing fixture %1)").arg(fxId));
                fi->setText(ColFull, tr("%1 W").arg(m_fixtureFullWatts.value(fxId, 0.0), 0, 'f', 0));
                fi->setData(0, RoleType, TypeFixture);
                fi->setData(0, RoleFixtureId, fxId);
                fi->setForeground(ColName, QBrush(Qt::gray));
            }
            ci->setExpanded(true);
        }

        updateSourceRow(si, s);   // summary depends on the just-added circuits
    }
    m_tree->blockSignals(false);
}

void PowerDistributionDialog::rebuildFixtures()
{
    m_fixtureList->clear();
    PowerDistribution *pd = m_doc->powerDistribution();

    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == NULL)
            continue;
        QTreeWidgetItem *it = new QTreeWidgetItem(m_fixtureList);
        it->setText(0, fx->name());
        it->setData(0, Qt::UserRole, fx->id());

        int s = -1, c = -1;
        pd->circuitOf(fx->id(), s, c);
        if (s >= 0 && c >= 0 && s < pd->sources().size()
                && c < pd->sources().at(s).circuits.size())
        {
            it->setText(1, tr("%1 / %2")
                        .arg(pd->sources().at(s).name)
                        .arg(pd->sources().at(s).circuits.at(c).name));
        }
        else
        {
            it->setText(1, tr("(unassigned)"));
        }
        it->setText(2, QString::number(m_fixtureWatts.value(fx->id(), 0.0), 'f', 0));
    }
    m_fixtureList->resizeColumnToContents(0);
}

void PowerDistributionDialog::slotTreeSelectionChanged()
{
    QTreeWidgetItem *it = m_tree->currentItem();
    const bool isSource = it && it->data(0, RoleType).toInt() == TypeSource;
    const bool isCircuit = it && it->data(0, RoleType).toInt() == TypeCircuit;
    // Add-circuit needs a source (or circuit) selected; assign needs a circuit.
    m_addCircuitBtn->setEnabled(isSource || isCircuit);
    m_removeBtn->setEnabled(isSource || isCircuit);
    m_assignBtn->setEnabled(isCircuit);
    refreshSourcePanel();
}

void PowerDistributionDialog::slotTreeItemChanged(QTreeWidgetItem *item, int column)
{
    if (item == NULL)
        return;
    PowerDistribution *pd = m_doc->powerDistribution();
    const int type = item->data(0, RoleType).toInt();
    const int s = item->data(0, RoleSource).toInt();

    // NOTE: refresh the affected rows IN PLACE — never rebuildTree() here. The
    // tree is still committing this edit, and clear()ing it would delete the
    // item mid-commit and drop the typed value (that was the "Rated A
    // disappears" bug). rebuildFixtures() is safe: it's a different widget.
    m_tree->blockSignals(true);

    if (type == TypeSource)
    {
        if (s < 0 || s >= pd->sources().size()) { m_tree->blockSignals(false); return; }
        if (column == ColName)
            pd->sources()[s].name = item->text(ColName);
        else if (column == ColVolts)
        {
            const double v = item->text(ColVolts).toDouble();
            if (v > 0.0)
                pd->sources()[s].voltage = v;
            item->setText(ColVolts, QString::number(pd->sources()[s].voltage, 'f', 0));
        }
        // A source-voltage change ripples to every circuit that inherits it.
        for (int i = 0; i < item->childCount(); i++)
            populateCircuitItem(item->child(i), s, i);
    }
    else if (type == TypeCircuit)
    {
        const int c = item->data(0, RoleCircuit).toInt();
        if (s < 0 || s >= pd->sources().size()
                || c < 0 || c >= pd->sources()[s].circuits.size())
        { m_tree->blockSignals(false); return; }

        if (column == ColName)
            pd->sources()[s].circuits[c].name = item->text(ColName);
        else if (column == ColVolts)
            // Explicit per-circuit override; 0/blank reverts to inheriting.
            pd->sources()[s].circuits[c].voltage = item->text(ColVolts).toDouble();
        else if (column == ColAmps)
        {
            const double a = item->text(ColAmps).toDouble();
            if (a > 0.0)
                pd->sources()[s].circuits[c].ratedAmps = a;
        }
        populateCircuitItem(item, s, c);   // re-render this row from the model
    }

    m_tree->blockSignals(false);
    m_doc->setModified();
    rebuildFixtures();   // assignment column may reference an edited circuit name
}

void PowerDistributionDialog::slotAddSource()
{
    QSettings settings;
    PowerDistribution *pd = m_doc->powerDistribution();
    PowerSource src;
    src.name = tr("Source %1").arg(pd->sources().size() + 1);
    src.voltage = settings.value(KDefaultVoltageKey, 120.0).toDouble();
    pd->sources().append(src);
    m_doc->setModified();
    rebuildTree();
}

void PowerDistributionDialog::slotAddCircuit()
{
    QTreeWidgetItem *it = m_tree->currentItem();
    if (it == NULL)
        return;
    const int s = it->data(0, RoleSource).toInt();
    PowerDistribution *pd = m_doc->powerDistribution();
    if (s < 0 || s >= pd->sources().size())
        return;

    QSettings settings;
    PowerCircuit cir;
    cir.name = tr("Circuit %1").arg(pd->sources()[s].circuits.size() + 1);
    cir.ratedAmps = settings.value(KDefaultBreakerKey, 20.0).toDouble();
    cir.deratePercent = settings.value(KDefaultDerateKey, 80).toInt();
    pd->sources()[s].circuits.append(cir);
    m_doc->setModified();
    rebuildTree();
}

void PowerDistributionDialog::slotRemoveSelected()
{
    QTreeWidgetItem *it = m_tree->currentItem();
    if (it == NULL)
        return;
    PowerDistribution *pd = m_doc->powerDistribution();
    const int type = it->data(0, RoleType).toInt();
    const int s = it->data(0, RoleSource).toInt();
    if (s < 0 || s >= pd->sources().size())
        return;

    if (type == TypeSource)
    {
        pd->sources().removeAt(s);
    }
    else if (type == TypeCircuit)
    {
        const int c = it->data(0, RoleCircuit).toInt();
        if (c >= 0 && c < pd->sources()[s].circuits.size())
            pd->sources()[s].circuits.removeAt(c);
    }
    m_doc->setModified();
    rebuildTree();
    rebuildFixtures();
}

void PowerDistributionDialog::slotAssign()
{
    QTreeWidgetItem *cit = m_tree->currentItem();
    if (cit == NULL || cit->data(0, RoleType).toInt() != TypeCircuit)
        return;
    const int s = cit->data(0, RoleSource).toInt();
    const int c = cit->data(0, RoleCircuit).toInt();

    PowerDistribution *pd = m_doc->powerDistribution();
    foreach (QTreeWidgetItem *fit, m_fixtureList->selectedItems())
        pd->assignFixture(fit->data(0, Qt::UserRole).toUInt(), s, c);

    m_doc->setModified();
    rebuildTree();
    rebuildFixtures();
}

void PowerDistributionDialog::slotUnassign()
{
    PowerDistribution *pd = m_doc->powerDistribution();
    foreach (QTreeWidgetItem *fit, m_fixtureList->selectedItems())
        pd->unassignFixture(fit->data(0, Qt::UserRole).toUInt());

    m_doc->setModified();
    rebuildTree();
    rebuildFixtures();
}

void PowerDistributionDialog::slotAutoAssign()
{
    // Greedy bin-pack: heaviest fixtures first into the first circuit with
    // headroom under its derated limit; spawn new circuits (seeded from the
    // saved defaults) on a single auto source when everything is full.
    QSettings settings;
    const double defV = settings.value(KDefaultVoltageKey, 120.0).toDouble();
    const double defBreaker = settings.value(KDefaultBreakerKey, 20.0).toDouble();
    const int defDerate = settings.value(KDefaultDerateKey, 80).toInt();

    PowerDistribution *pd = m_doc->powerDistribution();

    // Sort fixtures by watts descending.
    QList<QPair<double, quint32> > fixturesByLoad;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == NULL)
            continue;
        pd->unassignFixture(fx->id());
        fixturesByLoad.append(qMakePair(m_fixtureWatts.value(fx->id(), 0.0), fx->id()));
    }
    std::sort(fixturesByLoad.begin(), fixturesByLoad.end(),
              [](const QPair<double, quint32> &a, const QPair<double, quint32> &b)
              { return a.first > b.first; });

    // Ensure there's at least one source to pack into.
    if (pd->sources().isEmpty())
    {
        PowerSource src;
        src.name = tr("Auto source");
        src.voltage = defV;
        pd->sources().append(src);
    }

    // Running load (amps) per existing circuit, addressed as "s:c".
    QHash<QString, double> loadAmps;
    for (int s = 0; s < pd->sources().size(); s++)
        for (int c = 0; c < pd->sources()[s].circuits.size(); c++)
            loadAmps.insert(QString("%1:%2").arg(s).arg(c), 0.0);

    for (int i = 0; i < fixturesByLoad.size(); i++)
    {
        const double watts = fixturesByLoad[i].first;
        const quint32 fxId = fixturesByLoad[i].second;
        bool placed = false;

        for (int s = 0; s < pd->sources().size() && !placed; s++)
        {
            const double srcV = (pd->sources()[s].voltage > 0.0)
                                ? pd->sources()[s].voltage : defV;
            for (int c = 0; c < pd->sources()[s].circuits.size() && !placed; c++)
            {
                const QString key = QString("%1:%2").arg(s).arg(c);
                const PowerCircuit &cir = pd->sources()[s].circuits.at(c);
                const double amps = watts / cir.effectiveVoltage(srcV);
                if (loadAmps[key] + amps <= cir.deratedLimit())
                {
                    pd->assignFixture(fxId, s, c);
                    loadAmps[key] += amps;
                    placed = true;
                }
            }
        }

        if (!placed)
        {
            // No circuit has headroom — add a new one on the first source.
            const int s = 0;
            const double srcV = (pd->sources()[s].voltage > 0.0)
                                ? pd->sources()[s].voltage : defV;
            PowerCircuit cir;
            cir.name = tr("Circuit %1").arg(pd->sources()[s].circuits.size() + 1);
            cir.ratedAmps = defBreaker;
            cir.deratePercent = defDerate;
            pd->sources()[s].circuits.append(cir);
            const int c = pd->sources()[s].circuits.size() - 1;
            pd->assignFixture(fxId, s, c);
            loadAmps.insert(QString("%1:%2").arg(s).arg(c),
                            watts / pd->sources()[s].circuits[c].effectiveVoltage(srcV));
        }
    }

    m_doc->setModified();
    rebuildTree();
    rebuildFixtures();
}

void PowerDistributionDialog::slotSceneCheck()
{
    PowerDistribution *pd = m_doc->powerDistribution();
    QSettings settings;
    const double dflt = settings.value(KDefaultVoltageKey, 120.0).toDouble();
    const QBrush red(QColor(192, 57, 43));

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Power Check — scenes & collections"));
    dlg.resize(620, 460);
    QVBoxLayout *vl = new QVBoxLayout(&dlg);
    QLabel *summary = new QLabel(&dlg);
    vl->addWidget(summary);

    QTreeWidget *rt = new QTreeWidget(&dlg);
    rt->setColumnCount(3);
    rt->setHeaderLabels(QStringList() << tr("Scene / Collection")
                        << tr("Type") << tr("Load (circuit: amps / limit)"));
    vl->addWidget(rt, 1);

    int funcs = 0, overloadFuncs = 0;
    const bool haveCircuits = !pd->sources().isEmpty();

    foreach (Function *fn, m_doc->functions())
    {
        const bool isScene = (fn->type() == Function::SceneType);
        const bool isColl  = (fn->type() == Function::CollectionType);
        if (!isScene && !isColl)
            continue;
        funcs++;

        const QHash<quint32, double> w = PowerEstimator::functionFixtureWatts(m_doc, fn);

        // Per-circuit load for this function + this function's total watts.
        double totalW = 0.0;
        bool funcOver = false;
        QList<QTreeWidgetItem*> circuitRows;
        for (int s = 0; s < pd->sources().size(); s++)
        {
            const PowerSource &src = pd->sources().at(s);
            const double srcV = (src.voltage > 0.0) ? src.voltage : dflt;
            for (int c = 0; c < src.circuits.size(); c++)
            {
                const PowerCircuit &cir = src.circuits.at(c);
                double cw = 0.0;
                foreach (quint32 fid, cir.fixtures)
                    cw += w.value(fid, 0.0);
                if (cw <= 0.0)
                    continue;                      // skip circuits this look doesn't load
                totalW += cw;
                const double amps = cw / cir.effectiveVoltage(srcV);
                const double limit = cir.deratedLimit();
                const bool over = amps > limit;
                funcOver = funcOver || over;

                QTreeWidgetItem *crow = new QTreeWidgetItem();
                crow->setText(0, QString("%1 / %2").arg(src.name).arg(cir.name));
                crow->setText(2, tr("%1 / %2 A%3").arg(amps, 0, 'f', 1)
                              .arg(limit, 0, 'f', 1).arg(over ? tr("  ⚠") : QString()));
                if (over)
                    for (int col = 0; col < 3; col++)
                    { crow->setForeground(col, Qt::white); crow->setBackground(col, red); }
                circuitRows.append(crow);
            }
        }
        if (funcOver)
            overloadFuncs++;

        QTreeWidgetItem *top = new QTreeWidgetItem(rt);
        top->setText(0, fn->name());
        top->setText(1, isColl ? tr("Collection") : tr("Scene"));
        top->setText(2, tr("%1 kW%2").arg(totalW / 1000.0, 0, 'f', 2)
                     .arg(funcOver ? tr("  ⚠ OVERLOAD") : QString()));
        top->addChildren(circuitRows);
        if (funcOver)
        {
            top->setForeground(2, red);
            top->setExpanded(true);
        }
    }

    if (!haveCircuits)
        summary->setText(tr("No circuits defined yet — add sources/circuits and "
                            "assign fixtures to see per-circuit load. "
                            "(%1 scenes/collections, total kW shown.)").arg(funcs));
    else if (overloadFuncs == 0)
        summary->setText(tr("✓ No overloads. %1 scenes/collections checked.").arg(funcs));
    else
        summary->setText(tr("⚠ %1 of %2 scenes/collections overload a circuit:")
                         .arg(overloadFuncs).arg(funcs));

    rt->resizeColumnToContents(0);
    rt->resizeColumnToContents(1);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
    vl->addWidget(bb);
    dlg.exec();
}

void PowerDistributionDialog::slotExportVenue()
{
    QSettings settings;
    QString dir = settings.value(QStringLiteral("venue/lastdir")).toString();
    QString path = QFileDialog::getSaveFileName(this, tr("Export Venue"),
                       dir, tr("Venue files (*.venue)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".venue"), Qt::CaseInsensitive))
        path += QStringLiteral(".venue");
    settings.setValue(QStringLiteral("venue/lastdir"), QFileInfo(path).absolutePath());

    if (Venue::exportToFile(m_doc, path, Venue::AllSections))
        QMessageBox::information(this, tr("Export Venue"),
            tr("Venue exported to:\n%1\n\n(Circuit infrastructure only — fixture "
               "assignments stay with the show.)").arg(path));
    else
        QMessageBox::warning(this, tr("Export Venue"),
            tr("Could not write the venue file."));
}

void PowerDistributionDialog::slotImportVenue()
{
    QSettings settings;
    QString dir = settings.value(QStringLiteral("venue/lastdir")).toString();
    QString path = QFileDialog::getOpenFileName(this, tr("Import Venue"),
                       dir, tr("Venue files (*.venue)"));
    if (path.isEmpty())
        return;
    settings.setValue(QStringLiteral("venue/lastdir"), QFileInfo(path).absolutePath());

    const QStringList sections = Venue::sectionsInFile(path);
    if (sections.isEmpty())
    {
        QMessageBox::warning(this, tr("Import Venue"),
            tr("Not a valid venue file (no recognizable sections)."));
        return;
    }

    if (QMessageBox::question(this, tr("Import Venue"),
            tr("Import these venue sections, replacing the current "
               "power distribution?\n\n• %1\n\nFixture assignments will be "
               "cleared — reassign your rig after importing.").arg(sections.join("\n• ")),
            QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
        return;

    if (Venue::importFromFile(m_doc, path))
    {
        computeFixtureWatts();
        rebuildTree();
        rebuildFixtures();
        slotTreeSelectionChanged();
    }
    else
    {
        QMessageBox::warning(this, tr("Import Venue"),
            tr("Could not read the venue file."));
    }
}
