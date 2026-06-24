/*
  Q Light Controller Plus
  hidprofileeditor.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "hidprofileeditor.h"
#include "hidprofile.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QDebug>

static const QStringList AXIS_SLOTS  = { "", "pan", "tilt", "zoom", "iris",
                                          "dimmer", "gobo", "color" };
static const QStringList BTN_ACTIONS = { "",
                                          "chaser_step_forward",
                                          "chaser_step_back",
                                          "followspot_toggle",
                                          "highlight_toggle",
                                          "programmer_clear" };

static const int COL_NAME   = 0;
static const int COL_ROLE   = 1; // slot for axes, action for buttons

HIDProfileEditor::HIDProfileEditor(HIDProfile *profile, QWidget *parent)
    : QDialog(parent)
    , m_profile(profile)
{
    Q_ASSERT(profile);
    setWindowTitle(tr("Edit HID Profile — %1").arg(profile->name()));
    setMinimumSize(520, 400);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *nameLabel = new QLabel(
        tr("<b>%1</b>  (VID 0x%2, %3 products)")
            .arg(profile->name())
            .arg(profile->vendorId(), 4, 16, QChar('0')).toUpper()
            .arg(profile->productIds().size()), this);
    mainLayout->addWidget(nameLabel);

    QTabWidget *tabs = new QTabWidget(this);
    mainLayout->addWidget(tabs, 1);

    /* Axes tab */
    m_axisTable = new QTableWidget(this);
    m_axisTable->setColumnCount(2);
    m_axisTable->setHorizontalHeaderLabels({ tr("Name"), tr("Slot") });
    m_axisTable->horizontalHeader()->setStretchLastSection(true);
    m_axisTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_axisTable->verticalHeader()->setDefaultSectionSize(26);
    buildAxisTable();
    tabs->addTab(m_axisTable, tr("Axes"));

    /* Buttons tab */
    m_buttonTable = new QTableWidget(this);
    m_buttonTable->setColumnCount(2);
    m_buttonTable->setHorizontalHeaderLabels({ tr("Name"), tr("Action") });
    m_buttonTable->horizontalHeader()->setStretchLastSection(true);
    m_buttonTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_buttonTable->verticalHeader()->setDefaultSectionSize(26);
    buildButtonTable();
    tabs->addTab(m_buttonTable, tr("Buttons"));

    /* Settings tab */
    QWidget *settingsWidget = new QWidget(this);
    QFormLayout *settingsForm = new QFormLayout(settingsWidget);
    settingsForm->setContentsMargins(8, 8, 8, 8);

    m_sensSpin = new QDoubleSpinBox(settingsWidget);
    m_sensSpin->setRange(0.1, 3.0);
    m_sensSpin->setSingleStep(0.05);
    m_sensSpin->setDecimals(2);
    m_sensSpin->setValue(double(profile->sensitivity()));
    m_sensSpin->setToolTip(tr("Multiplier applied to axis travel (0.1 = very slow, 1.0 = normal, 3.0 = very fast)"));
    settingsForm->addRow(tr("Sensitivity:"), m_sensSpin);

    m_dzSpin = new QDoubleSpinBox(settingsWidget);
    m_dzSpin->setRange(0.0, 0.5);
    m_dzSpin->setSingleStep(0.01);
    m_dzSpin->setDecimals(3);
    m_dzSpin->setValue(double(profile->deadzone()));
    m_dzSpin->setToolTip(tr("Fraction of axis travel to ignore around centre (0 = no deadzone, 0.1 = 10%)"));
    settingsForm->addRow(tr("Deadzone:"), m_dzSpin);

    tabs->addTab(settingsWidget, tr("Settings"));

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &HIDProfileEditor::slotAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void HIDProfileEditor::buildAxisTable()
{
    const int n = m_profile->axisCount();
    m_axisTable->setRowCount(n);

    // Collect all axis indices in order
    QList<int> indices;
    for (int i = 0; i < 32; ++i)
        if (!m_profile->axisName(i).isEmpty())
            indices.append(i);

    for (int row = 0; row < indices.size(); ++row)
    {
        const int idx = indices[row];
        m_axisTable->setVerticalHeaderItem(row,
            new QTableWidgetItem(QString::number(idx)));

        auto *nameItem = new QTableWidgetItem(m_profile->axisName(idx));
        m_axisTable->setItem(row, COL_NAME, nameItem);

        auto *slotCombo = new QComboBox(m_axisTable);
        slotCombo->addItems(AXIS_SLOTS);
        int si = AXIS_SLOTS.indexOf(m_profile->axisSlot(idx));
        slotCombo->setCurrentIndex(si >= 0 ? si : 0);
        m_axisTable->setCellWidget(row, COL_ROLE, slotCombo);
    }
    m_axisTable->resizeColumnToContents(COL_NAME);
}

void HIDProfileEditor::buildButtonTable()
{
    QList<int> indices;
    for (int i = 0; i < 64; ++i)
        if (!m_profile->buttonName(i).isEmpty())
            indices.append(i);

    m_buttonTable->setRowCount(indices.size());

    for (int row = 0; row < indices.size(); ++row)
    {
        const int idx = indices[row];
        m_buttonTable->setVerticalHeaderItem(row,
            new QTableWidgetItem(QString::number(idx)));

        auto *nameItem = new QTableWidgetItem(m_profile->buttonName(idx));
        m_buttonTable->setItem(row, COL_NAME, nameItem);

        auto *actionCombo = new QComboBox(m_buttonTable);
        actionCombo->addItems(BTN_ACTIONS);
        int ai = BTN_ACTIONS.indexOf(m_profile->buttonAction(idx));
        actionCombo->setCurrentIndex(ai >= 0 ? ai : 0);
        m_buttonTable->setCellWidget(row, COL_ROLE, actionCombo);
    }
    m_buttonTable->resizeColumnToContents(COL_NAME);
}

void HIDProfileEditor::applyTableToProfile()
{
    /* Settings */
    if (m_sensSpin)
        m_profile->setSensitivity(float(m_sensSpin->value()));
    if (m_dzSpin)
        m_profile->setDeadzone(float(m_dzSpin->value()));

    /* Axes */
    QList<int> axisIndices;
    for (int i = 0; i < 32; ++i)
        if (!m_profile->axisName(i).isEmpty())
            axisIndices.append(i);

    for (int row = 0; row < m_axisTable->rowCount(); ++row)
    {
        const int idx = axisIndices.value(row, -1);
        if (idx < 0) continue;

        const QString name = m_axisTable->item(row, COL_NAME)
                             ? m_axisTable->item(row, COL_NAME)->text()
                             : QString();
        m_profile->setAxisName(idx, name);

        auto *combo = qobject_cast<QComboBox*>(m_axisTable->cellWidget(row, COL_ROLE));
        m_profile->setAxisSlot(idx, combo ? combo->currentText() : QString());
    }

    /* Buttons */
    QList<int> btnIndices;
    for (int i = 0; i < 64; ++i)
        if (!m_profile->buttonName(i).isEmpty())
            btnIndices.append(i);

    for (int row = 0; row < m_buttonTable->rowCount(); ++row)
    {
        const int idx = btnIndices.value(row, -1);
        if (idx < 0) continue;

        const QString name = m_buttonTable->item(row, COL_NAME)
                             ? m_buttonTable->item(row, COL_NAME)->text()
                             : QString();
        m_profile->setButtonName(idx, name);

        auto *combo = qobject_cast<QComboBox*>(m_buttonTable->cellWidget(row, COL_ROLE));
        m_profile->setButtonAction(idx, combo ? combo->currentText() : QString());
    }
}

void HIDProfileEditor::slotAccepted()
{
    applyTableToProfile();
    if (!m_profile->save())
    {
        QMessageBox::warning(this, tr("Save Failed"),
                             tr("Could not save the profile to %1")
                             .arg(m_profile->path()));
        return;
    }
    accept();
}
