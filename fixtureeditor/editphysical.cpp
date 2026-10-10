/*
  Q Light Controller Plus - Fixture Editor
  editphysical.cpp

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

#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QSpinBox>

#include "editphysical.h"
#include "ui_editphysical.h"

EditPhysical::EditPhysical(QLCPhysical physical, QWidget *parent)
    : QWidget(parent)
    , m_physical(physical)

{
    setupUi(this);

    QString str;

    m_bulbTypeCombo->setEditText(m_physical.bulbType());
    m_bulbLumensSpin->setValue(m_physical.bulbLumens());
    m_bulbTempCombo->setEditText(str.setNum(m_physical.bulbColourTemperature()));

    m_weightSpin->setValue(m_physical.weight());
    m_widthSpin->setValue(m_physical.width());
    m_heightSpin->setValue(m_physical.height());
    m_depthSpin->setValue(m_physical.depth());

    m_lensNameCombo->setEditText(m_physical.lensName());
    m_lensDegreesMinSpin->setValue(m_physical.lensDegreesMin());
    m_lensDegreesMaxSpin->setValue(m_physical.lensDegreesMax());
    m_lensBeamsPerHeadSpin->setValue(m_physical.lensBeamsPerHead());

    m_focusTypeCombo->setEditText(m_physical.focusType());
    m_panMaxSpin->setValue(m_physical.focusPanMax());
    m_tiltMaxSpin->setValue(m_physical.focusTiltMax());
    m_layoutColsSpin->setValue(m_physical.layoutSize().width());
    m_layoutRowsSpin->setValue(m_physical.layoutSize().height());

    m_powerConsumptionSpin->setValue(m_physical.powerConsumption());
    m_dmxConnectorCombo->setEditText(m_physical.dmxConnector());

    /* What the rig view draws, and how its light leaves it: a lantern in a
       cage glowing all round, not a box throwing a beam. */
    QGroupBox *look = new QGroupBox(tr("Look in the rig view"), this);
    QFormLayout *lookLay = new QFormLayout(look);
    m_bodyCombo = new QComboBox(look);
    m_bodyCombo->addItem(tr("Box"), int(QLCPhysical::BoxBody));
    m_bodyCombo->addItem(tr("Lantern"), int(QLCPhysical::LanternBody));
    m_bodyCombo->setCurrentIndex(qMax(0, m_bodyCombo->findData(int(m_physical.bodyShape()))));
    lookLay->addRow(tr("Body"), m_bodyCombo);
    m_emissionCombo = new QComboBox(look);
    m_emissionCombo->addItem(tr("Beam"), int(QLCPhysical::BeamEmission));
    m_emissionCombo->addItem(tr("All round"), int(QLCPhysical::OmniEmission));
    m_emissionCombo->addItem(tr("Laser beam"), int(QLCPhysical::LaserEmission));
    m_emissionCombo->addItem(tr("Star field"), int(QLCPhysical::StarEmission));
    m_emissionCombo->addItem(tr("Silk flame"), int(QLCPhysical::FlameEmission));
    m_emissionCombo->setCurrentIndex(qMax(0, m_emissionCombo->findData(int(m_physical.emission()))));
    m_emissionCombo->setToolTip(tr("Beam: a cone at the lens's angle. All round: a bare "
                                   "source -- a lantern, a bulb -- lighting what is near it. "
                                   "Laser beam: pencil-thin beams (Beams per head of them, "
                                   "fanned across the lens angle), a sharp dot where each "
                                   "lands. Star field: points of light thrown over everything "
                                   "its spread reaches -- a sky laser. Silk flame: a fan "
                                   "blowing silk up over its own orange and yellow lights, "
                                   "on when its channel is past half (a relay)."));
    lookLay->addRow(tr("Light"), m_emissionCombo);
    /* What makes the light: one point, or a set of LEDs drawn one by one --
       a WLED lantern's sixteen on a wire, an effect running along them. */
    QWidget *ledsBox = new QWidget(look);
    QHBoxLayout *ledsLay = new QHBoxLayout(ledsBox);
    ledsLay->setContentsMargins(0, 0, 0, 0);
    m_ledsSpin = new QSpinBox(ledsBox);
    m_ledsSpin->setRange(0, 1024);
    m_ledsSpin->setSpecialValueText(tr("One point"));
    m_ledsSpin->setSuffix(tr(" LEDs"));
    m_ledsSpin->setValue(m_physical.leds());
    m_ledsSpin->setToolTip(tr("One point of light, or that many LEDs drawn one by one in "
                              "the rig view -- an effect on a WLED shows along them. A "
                              "fixture can say otherwise for its own build."));
    m_ledLayoutCombo = new QComboBox(ledsBox);
    m_ledLayoutCombo->addItem(tr("Hanging LED cluster"), int(QLCPhysical::ClusterLeds));
    m_ledLayoutCombo->addItem(tr("LED tape"),            int(QLCPhysical::TapeLeds));
    m_ledLayoutCombo->addItem(tr("On the lens face"),    int(QLCPhysical::FaceLeds));
    m_ledLayoutCombo->setCurrentIndex(qMax(0, m_ledLayoutCombo->findData(int(m_physical.ledLayout()))));
    m_ledLayoutCombo->setToolTip(tr("How the LEDs sit in the body: a wire string let fall "
                                    "through the cage, tape wound tight round a core, or an "
                                    "array on the lens face -- a wash's 36 x 18 W behind its "
                                    "lenses"));
    m_ledLayoutCombo->setEnabled(m_physical.leds() > 0);
    connect(m_ledsSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_ledLayoutCombo,
            [this](int n) { m_ledLayoutCombo->setEnabled(n > 0); });
    ledsLay->addWidget(m_ledsSpin, 1);
    ledsLay->addWidget(m_ledLayoutCombo);
    lookLay->addRow(tr("Source"), ledsBox);
    m_glassCombo = new QComboBox(look);
    m_glassCombo->addItem(tr("Diffuse"), 0);
    m_glassCombo->addItem(tr("Clear"), 1);
    m_glassCombo->setCurrentIndex(m_physical.clearGlass() ? 1 : 0);
    m_glassCombo->setToolTip(tr("What the light is seen through: diffusing glass glows, "
                                "the LEDs a blur behind it; clear glass shows every LED."));
    lookLay->addRow(tr("Glass"), m_glassCombo);
    if (QGridLayout *grid = qobject_cast<QGridLayout *>(layout()))
        grid->addWidget(look, grid->rowCount(), 0, 1, qMax(1, grid->columnCount()));

    connect(copyClipboardButton, SIGNAL(clicked()),
            this, SLOT(slotCopyToClipboard()));
    connect(pasteClipboardButton, SIGNAL(clicked()),
            this, SLOT(slotPasteFromClipboard()));
}

EditPhysical::~EditPhysical()
{

}

QLCPhysical EditPhysical::physical()
{
    m_physical.setBulbType(m_bulbTypeCombo->currentText());
    m_physical.setBulbLumens(m_bulbLumensSpin->value());
    m_physical.setBulbColourTemperature(m_bulbTempCombo->currentText().toInt());
    m_physical.setWeight(m_weightSpin->value());
    m_physical.setWidth(m_widthSpin->value());
    m_physical.setHeight(m_heightSpin->value());
    m_physical.setDepth(m_depthSpin->value());
    m_physical.setLensName(m_lensNameCombo->currentText());
    m_physical.setLensDegreesMin(m_lensDegreesMinSpin->value());
    m_physical.setLensDegreesMax(m_lensDegreesMaxSpin->value());
    m_physical.setLensBeamsPerHead(m_lensBeamsPerHeadSpin->value());
    m_physical.setFocusType(m_focusTypeCombo->currentText());
    m_physical.setFocusPanMax(m_panMaxSpin->value());
    m_physical.setFocusTiltMax(m_tiltMaxSpin->value());
    m_physical.setLayoutSize(QSize(m_layoutColsSpin->value(), m_layoutRowsSpin->value()));
    m_physical.setPowerConsumption(m_powerConsumptionSpin->value());
    m_physical.setDmxConnector(m_dmxConnectorCombo->currentText());
    m_physical.setBodyShape(QLCPhysical::BodyShape(m_bodyCombo->currentData().toInt()));
    m_physical.setEmission(QLCPhysical::Emission(m_emissionCombo->currentData().toInt()));
    m_physical.setLeds(m_ledsSpin->value());
    m_physical.setLedLayout(QLCPhysical::LedLayout(m_ledLayoutCombo->currentData().toInt()));
    m_physical.setClearGlass(m_glassCombo->currentData().toInt() == 1);

    return m_physical;
}

void EditPhysical::slotCopyToClipboard()
{
    emit copyToClipboard(m_physical);
}

void EditPhysical::slotPasteFromClipboard()
{
    emit requestPasteFromClipboard();
}

void EditPhysical::pasteFromClipboard(QLCPhysical clipboard)
{
    m_bulbLumensSpin->setValue(clipboard.bulbLumens());
    m_bulbTypeCombo->setEditText(clipboard.bulbType());
    m_bulbTempCombo->setEditText(QString::number(clipboard.bulbColourTemperature()));
    m_weightSpin->setValue(clipboard.weight());
    m_widthSpin->setValue(clipboard.width());
    m_heightSpin->setValue(clipboard.height());
    m_depthSpin->setValue(clipboard.depth());
    m_lensNameCombo->setEditText(clipboard.lensName());
    m_lensDegreesMinSpin->setValue(clipboard.lensDegreesMin());
    m_lensDegreesMaxSpin->setValue(clipboard.lensDegreesMax());
    m_lensBeamsPerHeadSpin->setValue(clipboard.lensBeamsPerHead());
    m_focusTypeCombo->setEditText(clipboard.focusType());
    m_panMaxSpin->setValue(clipboard.focusPanMax());
    m_tiltMaxSpin->setValue(clipboard.focusTiltMax());
    m_layoutColsSpin->setValue(clipboard.layoutSize().width());
    m_layoutRowsSpin->setValue(clipboard.layoutSize().height());
    m_powerConsumptionSpin->setValue(clipboard.powerConsumption());
    m_dmxConnectorCombo->setEditText(clipboard.dmxConnector());
    m_bodyCombo->setCurrentIndex(qMax(0, m_bodyCombo->findData(int(clipboard.bodyShape()))));
    m_emissionCombo->setCurrentIndex(qMax(0, m_emissionCombo->findData(int(clipboard.emission()))));
    m_ledsSpin->setValue(clipboard.leds());
    m_ledLayoutCombo->setCurrentIndex(qMax(0, m_ledLayoutCombo->findData(int(clipboard.ledLayout()))));
    m_glassCombo->setCurrentIndex(clipboard.clearGlass() ? 1 : 0);
}
