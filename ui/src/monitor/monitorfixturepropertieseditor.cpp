/*
  Q Light Controller Plus
  monitorfixturepropertieseditor.cpp

  Copyright (C) Massimo Callegari

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

#include <QColorDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include "monitorfixturepropertieseditor.h"
#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "monitorproperties.h"
#include "truss.h"

MonitorFixturePropertiesEditor::MonitorFixturePropertiesEditor(
        MonitorFixtureItem *fxItem, MonitorGraphicsView *gfxView,
        MonitorProperties *props, QWidget *parent) :
    QWidget(parent)
  , m_fxItem(fxItem)
  , m_gfxView(gfxView)
  , m_props(props)
{
    Q_ASSERT(fxItem != NULL);
    Q_ASSERT(gfxView != NULL);
    Q_ASSERT(props != NULL);

    setupUi(this);

    m_xPosSpin->setMaximum(m_gfxView->gridSize().width());
    m_yPosSpin->setMaximum(m_gfxView->gridSize().height());

    if (m_props->gridUnits() == MonitorProperties::Feet)
    {
        m_xPosSpin->setSuffix("ft");
        m_yPosSpin->setSuffix("ft");
    }
    else
    {
        m_xPosSpin->setSuffix("m");
        m_yPosSpin->setSuffix("m");
    }

    m_fxName->setText(m_fxItem->name());
    m_xPosSpin->setValue(m_fxItem->realPosition().x() / 1000);
    m_yPosSpin->setValue(m_fxItem->realPosition().y() / 1000);
    m_rotationSpin->setValue(m_fxItem->rotation());

    QPixmap pm(28, 28);
    if (m_fxItem->getColor().isValid())
    {
        pm.fill(m_fxItem->getColor());
        m_gelColorButton->setIcon(QIcon(pm));
    }

    connect(m_xPosSpin, SIGNAL(valueChanged(double)),
            this, SLOT(slotSetPosition()));
    connect(m_yPosSpin, SIGNAL(valueChanged(double)),
            this, SLOT(slotSetPosition()));
    connect(m_rotationSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotRotationChanged(int)));
    connect(m_gelColorButton, SIGNAL(clicked()),
            this, SLOT(slotGelColorClicked()));
    connect(m_gelResetButton, SIGNAL(clicked()),
            this, SLOT(slotGelResetClicked()));

    // --- Rig Assignment group ---
    QGroupBox *rigBox = new QGroupBox(tr("Rig Assignment"), this);
    QFormLayout *rigForm = new QFormLayout(rigBox);

    m_trussCb = new QComboBox(rigBox);
    m_trussCb->addItem(tr("(none)"), QVariant(Truss::invalidId()));
    for (Truss *t : m_props->trusses())
        m_trussCb->addItem(t->name(), t->id());
    rigForm->addRow(tr("Truss:"), m_trussCb);

    m_trussOffsetSpin = new QDoubleSpinBox(rigBox);
    m_trussOffsetSpin->setRange(-100, 100);
    m_trussOffsetSpin->setSuffix(" m");
    m_trussOffsetSpin->setDecimals(2);
    rigForm->addRow(tr("Offset along truss:"), m_trussOffsetSpin);

    m_mountingCb = new QComboBox(rigBox);
    m_mountingCb->addItem(tr("Top-hung"),      Truss::TopHung);
    m_mountingCb->addItem(tr("Floor mounted"), Truss::FloorMounted);
    m_mountingCb->addItem(tr("Side arm"),      Truss::SideArm);
    rigForm->addRow(tr("Mounting:"), m_mountingCb);

    m_panZeroSpin = new QDoubleSpinBox(rigBox);
    m_panZeroSpin->setRange(0, 359);
    m_panZeroSpin->setSuffix(QString::fromUtf8("°"));
    m_panZeroSpin->setDecimals(1);
    rigForm->addRow(tr("Pan-zero direction:"), m_panZeroSpin);

    // Populate from existing rig props if any
    FixtureRigProps rp = m_props->fixtureRigProps(m_fxItem->fixtureID());
    for (int i = 0; i < m_trussCb->count(); ++i)
    {
        if (m_trussCb->itemData(i).toUInt() == rp.trussId)
        {
            m_trussCb->setCurrentIndex(i);
            break;
        }
    }
    m_trussOffsetSpin->setValue(double(rp.trussOffset));
    m_mountingCb->setCurrentIndex(static_cast<int>(rp.mountingType));
    m_panZeroSpin->setValue(double(rp.panZeroDir));

    layout()->addWidget(rigBox);

    connect(m_trussCb,        SIGNAL(currentIndexChanged(int)), this, SLOT(slotRigChanged()));
    connect(m_trussOffsetSpin, SIGNAL(valueChanged(double)),    this, SLOT(slotRigChanged()));
    connect(m_mountingCb,     SIGNAL(currentIndexChanged(int)), this, SLOT(slotRigChanged()));
    connect(m_panZeroSpin,    SIGNAL(valueChanged(double)),     this, SLOT(slotRigChanged()));
}

MonitorFixturePropertiesEditor::~MonitorFixturePropertiesEditor()
{

}

void MonitorFixturePropertiesEditor::slotSetPosition()
{
    QPointF itemPos(m_xPosSpin->value() * 1000, m_yPosSpin->value() * 1000);
    m_fxItem->setPos(m_gfxView->realPositionToPixels(itemPos.x(), itemPos.y()));
    m_fxItem->setRealPosition(itemPos);
    m_props->setFixturePosition(m_fxItem->fixtureID(), 0, 0,QVector3D(itemPos.x(), itemPos.y(), 0));
}

void MonitorFixturePropertiesEditor::slotRotationChanged(int value)
{
    m_fxItem->setRotation(value);
    m_props->setFixtureRotation(m_fxItem->fixtureID(), 0, 0, QVector3D(0, value, 0));
}

void MonitorFixturePropertiesEditor::slotGelColorClicked()
{
    QColor color = m_fxItem->getColor();
    QColor newColor = QColorDialog::getColor(color);

    if (newColor.isValid())
    {
        m_fxItem->setGelColor(newColor);
        m_props->setFixtureGelColor(m_fxItem->fixtureID(), 0, 0, newColor);
        QPixmap pm(28, 28);
        pm.fill(newColor);
        m_gelColorButton->setIcon(QIcon(pm));
        m_fxItem->slotUpdateValues();
    }
}

void MonitorFixturePropertiesEditor::slotGelResetClicked()
{
    m_gelColorButton->setIcon(QIcon());
    m_fxItem->setGelColor(QColor());
    m_props->setFixtureGelColor(m_fxItem->fixtureID(), 0, 0, QColor());
    m_fxItem->slotUpdateValues();
}

void MonitorFixturePropertiesEditor::slotRigChanged()
{
    FixtureRigProps rp;
    rp.trussId     = m_trussCb->currentData().toUInt();
    rp.trussOffset = float(m_trussOffsetSpin->value());
    rp.mountingType = static_cast<Truss::MountingType>(m_mountingCb->currentData().toInt());
    rp.panZeroDir  = float(m_panZeroSpin->value());
    m_props->setFixtureRigProps(m_fxItem->fixtureID(), rp);
}

