/*
  Q Light Controller Plus
  paletteeditdialog.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QColorDialog>

#include "paletteeditdialog.h"
#include "qlcpalette.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "doc.h"

// Palette types we expose (the ones QLCPalette resolves per-fixture).
// Stored as itemData on the type combo.
static const struct { QLCPalette::PaletteType type; const char *label; } kTypes[] = {
    { QLCPalette::Color,   "Color" },
    { QLCPalette::Dimmer,  "Dimmer" },
    { QLCPalette::PanTilt, "Pan/Tilt" },
    { QLCPalette::Aim,     "Aim at Target" },
    { QLCPalette::Gobo,    "Gobo" },
    { QLCPalette::Shutter, "Shutter" },
};

PaletteEditDialog::PaletteEditDialog(QWidget *parent, Doc *doc)
    : QDialog(parent)
    , m_existing(nullptr)
    , m_palette(nullptr)
    , m_doc(doc)
    , m_color(Qt::white)
{
    buildUi();
    setWindowTitle(tr("New palette"));
    syncEditorsToType();
}

PaletteEditDialog::PaletteEditDialog(QLCPalette *existing, QWidget *parent, Doc *doc)
    : QDialog(parent)
    , m_existing(existing)
    , m_palette(nullptr)
    , m_doc(doc)
    , m_color(Qt::white)
{
    buildUi();
    setWindowTitle(tr("Edit palette"));

    if (m_existing != nullptr)
    {
        m_nameEdit->setText(m_existing->name());
        // Lock the type — changing it would invalidate the stored value.
        for (int i = 0; i < m_typeCombo->count(); i++)
        {
            if (m_typeCombo->itemData(i).toInt() == int(m_existing->type()))
            {
                m_typeCombo->setCurrentIndex(i);
                break;
            }
        }
        m_typeCombo->setEnabled(false);

        switch (m_existing->type())
        {
        case QLCPalette::Color:
            m_color = m_existing->rgbValue();
            if (!m_color.isValid()) m_color = Qt::white;
            break;
        case QLCPalette::PanTilt:
            m_value1Spin->setValue(m_existing->intValue1());
            m_value2Spin->setValue(m_existing->intValue2());
            break;
        case QLCPalette::Aim:
            // Pre-select the linked target
            for (int i = 0; i < m_targetCombo->count(); ++i)
            {
                if (quint32(m_targetCombo->itemData(i).toUInt()) == m_existing->stageTargetId())
                {
                    m_targetCombo->setCurrentIndex(i);
                    break;
                }
            }
            break;
        default:
            m_value1Spin->setValue(m_existing->intValue1());
            break;
        }
    }
    syncEditorsToType();
}

PaletteEditDialog::~PaletteEditDialog()
{
}

void PaletteEditDialog::buildUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout();
    root->addLayout(form);

    m_nameEdit = new QLineEdit(this);
    form->addRow(tr("Name"), m_nameEdit);

    m_typeCombo = new QComboBox(this);
    for (const auto &t : kTypes)
        m_typeCombo->addItem(tr(t.label), int(t.type));
    form->addRow(tr("Type"), m_typeCombo);

    m_colorButton = new QPushButton(tr("Pick color…"), this);
    form->addRow(tr("Color"), m_colorButton);

    m_value1Label = new QLabel(tr("Value"), this);
    m_value1Spin = new QSpinBox(this);
    m_value1Spin->setRange(0, 540);
    form->addRow(m_value1Label, m_value1Spin);

    m_value2Label = new QLabel(tr("Tilt"), this);
    m_value2Spin = new QSpinBox(this);
    m_value2Spin->setRange(0, 360);
    form->addRow(m_value2Label, m_value2Spin);

    m_targetLabel = new QLabel(tr("Stage target"), this);
    m_targetCombo = new QComboBox(this);
    m_targetCombo->addItem(tr("(none)"), QVariant(StageTarget::invalidId()));
    if (m_doc && m_doc->monitorProperties())
    {
        for (StageTarget *t : m_doc->monitorProperties()->stageTargets())
            m_targetCombo->addItem(t->name(), QVariant(t->id()));
    }
    m_targetCombo->setToolTip(tr("Associate this palette with a named stage target position"));
    form->addRow(m_targetLabel, m_targetCombo);

    QDialogButtonBox *bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(bb);

    connect(m_typeCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotTypeChanged()));
    connect(m_colorButton, SIGNAL(clicked()), this, SLOT(slotPickColor()));
    connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(bb, SIGNAL(rejected()), this, SLOT(reject()));
}

void PaletteEditDialog::slotTypeChanged()
{
    syncEditorsToType();
}

void PaletteEditDialog::slotPickColor()
{
    const QColor c = QColorDialog::getColor(m_color, this, tr("Pick a color"));
    if (c.isValid())
    {
        m_color = c;
        m_colorButton->setText(m_color.name());
    }
}

void PaletteEditDialog::syncEditorsToType()
{
    const QLCPalette::PaletteType type =
        QLCPalette::PaletteType(m_typeCombo->currentData().toInt());

    const bool isColor = (type == QLCPalette::Color);
    const bool isPanTilt = (type == QLCPalette::PanTilt);
    const bool isAim = (type == QLCPalette::Aim);

    m_colorButton->setVisible(isColor);
    if (isColor)
        m_colorButton->setText(m_color.name());

    m_value1Label->setVisible(!isColor && !isAim);
    m_value1Spin->setVisible(!isColor && !isAim);
    m_value2Label->setVisible(isPanTilt);
    m_value2Spin->setVisible(isPanTilt);
    m_targetLabel->setVisible(isAim);
    m_targetCombo->setVisible(isAim);

    switch (type)
    {
    case QLCPalette::Dimmer:
        m_value1Label->setText(tr("Level (0-255)"));
        m_value1Spin->setRange(0, 255);
        break;
    case QLCPalette::Gobo:
    case QLCPalette::Shutter:
        m_value1Label->setText(tr("DMX (0-255)"));
        m_value1Spin->setRange(0, 255);
        break;
    case QLCPalette::PanTilt:
        m_value1Label->setText(tr("Pan (deg)"));
        m_value1Spin->setRange(0, 540);
        m_value2Label->setText(tr("Tilt (deg)"));
        break;
    default:
        break;
    }
}

void PaletteEditDialog::accept()
{
    const QLCPalette::PaletteType type = m_existing != nullptr
        ? m_existing->type()
        : QLCPalette::PaletteType(m_typeCombo->currentData().toInt());

    QLCPalette *p = m_existing != nullptr
        ? m_existing
        : new QLCPalette(type);

    p->resetValues();
    switch (type)
    {
    case QLCPalette::Color:
        // 7-char "#rrggbb" => RGB only (no forced W/A/UV assertion).
        p->setValue(m_color.name());
        break;
    case QLCPalette::PanTilt:
        p->setValue(m_value1Spin->value(), m_value2Spin->value());
        break;
    case QLCPalette::Aim:
        p->setStageTargetId(m_targetCombo->currentData().toUInt());
        break;
    default:
        p->setValue(m_value1Spin->value());
        break;
    }

    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty())
    {
        // Sensible default if the user didn't name it.
        if (type == QLCPalette::Color)
            name = QString("Color %1").arg(m_color.name());
        else if (type == QLCPalette::PanTilt)
            name = QString("Pos P%1/T%2")
                       .arg(m_value1Spin->value()).arg(m_value2Spin->value());
        else if (type == QLCPalette::Aim)
            name = tr("Aim");
        else
            name = QString("%1 %2")
                       .arg(QLCPalette::typeToString(type))
                       .arg(m_value1Spin->value());
    }
    p->setName(name);

    m_palette = p;
    QDialog::accept();
}
