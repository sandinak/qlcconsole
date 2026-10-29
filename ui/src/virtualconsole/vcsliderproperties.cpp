/*
  Q Light Controller
  vcsliderproperties.cpp

  Copyright (c) Heikki Junnila

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

#include <QTreeWidgetItem>
#include <QInputDialog>
#include <QRadioButton>
#include <QTreeWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QComboBox>
#include <QFormLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QAction>

#include "qlccapability.h"
#include "qlcchannel.h"

#include "inputselectionwidget.h"
#include "vcsliderproperties.h"
#include "functionselection.h"
#include "vcslider.h"
#include "fixture.h"
#include "speedmasters.h"
#include "doc.h"
#include "itemicons.h"

#define KColumnName  0
#define KColumnType  1
#define KColumnRange 2
#define KColumnID    3

VCSliderProperties::VCSliderProperties(VCSlider* slider, Doc* doc)
    : QDialog(slider)
    , m_doc(doc)
{
    Q_ASSERT(doc != NULL);
    Q_ASSERT(slider != NULL);
    m_slider = slider;
    m_ovrResetSelWidget = NULL;

    setupUi(this);
    // Sorting is enabled in the .ui. Inserting the full fixture/channel/capability
    // tree (tens of thousands of nodes on a large rig) with live re-sort is O(n^2)
    // and can hang the dialog for minutes. Build with sorting OFF, then sort once
    // after the tree is populated (see below).
    m_levelList->setSortingEnabled(false);

    QAction* action = new QAction(this);
    action->setShortcut(QKeySequence(QKeySequence::Close));
    connect(action, SIGNAL(triggered(bool)), this, SLOT(reject()));
    addAction(action);

    /* Level page connections */
    connect(m_levelLowLimitSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotLevelLowSpinChanged(int)));
    connect(m_levelHighLimitSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotLevelHighSpinChanged(int)));
    connect(m_levelCapabilityButton, SIGNAL(clicked()),
            this, SLOT(slotLevelCapabilityClicked()));
    connect(m_levelList, SIGNAL(itemClicked(QTreeWidgetItem*,int)),
            this, SLOT(slotLevelListClicked(QTreeWidgetItem*)));
    connect(m_levelAllButton, SIGNAL(clicked()),
            this, SLOT(slotLevelAllClicked()));
    connect(m_levelNoneButton, SIGNAL(clicked()),
            this, SLOT(slotLevelNoneClicked()));
    connect(m_levelInvertButton, SIGNAL(clicked()),
            this, SLOT(slotLevelInvertClicked()));
    connect(m_levelByGroupButton, SIGNAL(clicked()),
            this, SLOT(slotLevelByGroupClicked()));
    connect(m_switchToLevelModeButton, SIGNAL(clicked()),
            this, SLOT(slotModeLevelClicked()));

    /* Playback page connections */
    connect(m_switchToPlaybackModeButton, SIGNAL(clicked()),
            this, SLOT(slotModePlaybackClicked()));
    connect(m_attachPlaybackFunctionButton, SIGNAL(clicked()),
            this, SLOT(slotAttachPlaybackFunctionClicked()));
    connect(m_detachPlaybackFunctionButton, SIGNAL(clicked()),
            this, SLOT(slotDetachPlaybackFunctionClicked()));

    /* Submaster page connections */
    connect(m_switchToSubmasterModeButton, SIGNAL(clicked()),
            this, SLOT(slotModeSubmasterClicked()));

    /* Parameter page connections */
    connect(m_switchToParameterModeButton, SIGNAL(clicked()),
            this, SLOT(slotModeParameterClicked()));

    /* Populate role combo (preserve enum index → combo index = enum value) */
    static const VCSlider::ParameterRole roleOrder[] = {
        VCSlider::RoleDimmer, VCSlider::RoleRed, VCSlider::RoleGreen,
        VCSlider::RoleBlue, VCSlider::RoleWhite, VCSlider::RoleCyan,
        VCSlider::RoleMagenta, VCSlider::RoleYellow, VCSlider::RoleAmber,
        VCSlider::RoleUV, VCSlider::RoleColour, VCSlider::RolePan,
        VCSlider::RoleTilt, VCSlider::RoleShutter, VCSlider::RoleGobo,
        VCSlider::RoleGobo2, VCSlider::RoleSpeed, VCSlider::RoleEffect,
        VCSlider::RoleEffect2, VCSlider::RoleBeam, VCSlider::RoleFocus,
        VCSlider::RoleZoom, VCSlider::RolePrism, VCSlider::RoleMaintenance
    };
    for (VCSlider::ParameterRole r : roleOrder)
        m_parameterRoleCombo->addItem(VCSlider::parameterRoleToString(r),
                                      static_cast<int>(r));
    m_parameterRoleCombo->setCurrentIndex(
        m_parameterRoleCombo->findData(static_cast<int>(m_slider->parameterRole())));

    if (m_slider->parameterControlByte() == 1)
        m_parameterLSBRadio->setChecked(true);
    else
        m_parameterMSBRadio->setChecked(true);

    /*********************************************************************
     * General page
     *********************************************************************/

    /* Name */
    m_nameEdit->setText(m_slider->caption());

    /* Widget appearance */
    if (m_slider->widgetStyle() == VCSlider::WKnob)
        m_widgetKnobRadio->setChecked(true);
    else
        m_widgetSliderRadio->setChecked(true);

    /* Slider mode */
    m_sliderMode = m_slider->sliderMode();

    /* Slider movement (Qt understands inverted appearance vice versa) */
    if (m_slider->invertedAppearance() == true)
        m_sliderMovementInvertedRadio->setChecked(true);
    else
        m_sliderMovementNormalRadio->setChecked(true);

    /* Value display style */
    switch (m_slider->valueDisplayStyle())
    {
        default:
        case VCSlider::ExactValue:
            m_valueExactRadio->setChecked(true);
        break;
        case VCSlider::PercentageValue:
            m_valuePercentageRadio->setChecked(true);
        break;
    }

    /* Values catching */
    if (m_slider->catchValues())
        m_catchValueCheck->setChecked(true);

    /********************************************************************
     * External input
     ********************************************************************/

    m_inputSelWidget = new InputSelectionWidget(m_doc, this);
    m_inputSelWidget->setKeyInputVisibility(false);
    m_inputSelWidget->setInputSource(m_slider->inputSource(VCSlider::sliderInputSourceId));
    m_inputSelWidget->setWidgetPage(m_slider->page());
    m_inputSelWidget->show();
    m_extControlLayout->addWidget(m_inputSelWidget);

    /*********************************************************************
     * Level page
     *********************************************************************/

    /* Level limit spins */
    m_levelLowLimitSpin->setValue(m_slider->levelLowLimit());
    m_levelHighLimitSpin->setValue(m_slider->levelHighLimit());

    /* Tree widget contents. Populate with repaints + sorting suppressed, then
       re-enable and sort exactly once — otherwise every insertion re-sorts the
       whole tree (O(n^2)) and the dialog takes minutes to open on a large rig. */
    m_levelList->setUpdatesEnabled(false);
    levelUpdateFixtures();
    levelUpdateChannelSelections();
    m_levelList->setSortingEnabled(true);
    m_levelList->sortByColumn(KColumnName, Qt::AscendingOrder);
    m_levelList->setUpdatesEnabled(true);

    connect(m_levelList, SIGNAL(expanded(QModelIndex)),
            this, SLOT(slotItemExpanded()));
    connect(m_levelList, SIGNAL(collapsed(QModelIndex)),
            this, SLOT(slotItemExpanded()));
    connect(m_monitorValuesCheck, SIGNAL(clicked(bool)),
            this, SLOT(slotMonitorCheckClicked(bool)));

    m_ovrResetSelWidget = new InputSelectionWidget(m_doc, this);
    m_ovrResetSelWidget->setTitle(tr("Override reset control"));
    m_ovrResetSelWidget->setCustomFeedbackVisibility(true);
    m_ovrResetSelWidget->setKeySequence(m_slider->overrideResetKeySequence());
    m_ovrResetSelWidget->setInputSource(m_slider->inputSource(VCSlider::overrideResetInputSourceId));
    m_ovrResetSelWidget->setWidgetPage(m_slider->page());
    m_monitorResetControl->addWidget(m_ovrResetSelWidget);

    if (m_sliderMode == VCSlider::Level && m_slider->channelsMonitorEnabled())
        m_ovrResetSelWidget->show();
    else
        m_ovrResetSelWidget->hide();
    m_monitorValuesCheck->setChecked(m_slider->channelsMonitorEnabled());

    /*********************************************************************
     * Playback page
     *********************************************************************/

    /* Function */
    m_playbackFunctionId = m_slider->playbackFunction();
    updatePlaybackFunctionName();

    m_flashInputWidget = new InputSelectionWidget(m_doc, this);
    m_flashInputWidget->setKeySequence(m_slider->playbackFlashKeySequence());
    m_flashInputWidget->setInputSource(m_slider->inputSource(VCSlider::flashButtonInputSourceId));
    m_flashInputWidget->setWidgetPage(m_slider->page());
    m_flashInputWidget->show();
    m_flashInputWidget->setEnabled(m_slider->playbackFlashEnable());
    m_flashButtonLayout->addWidget(m_flashInputWidget);

    connect(m_flashButtonCheck, SIGNAL(clicked(bool)),
            this, SLOT(slotFlashCheckClicked(bool)));
    m_flashButtonCheck->setChecked(m_slider->playbackFlashEnable());

    buildSpeedMasterPage();

    /* At last, sort out visibility */
    switch (m_sliderMode)
    {
        case VCSlider::GrandMaster:
            // Made by the programmer frame wizard: no page of its own, kept.
            setLevelPageVisibility(false);
            setPlaybackPageVisibility(false);
            setSubmasterPageVisibility(false);
            setParameterPageVisibility(false);
            setSpeedMasterPageVisibility(false);
        break;
        case VCSlider::SpeedMaster:
            slotModeSpeedMasterClicked();
        break;
        default:
        case VCSlider::Level:
            slotModeLevelClicked();
        break;
        case VCSlider::Playback:
            slotModePlaybackClicked();
        break;
        case VCSlider::Submaster:
            slotModeSubmasterClicked();
        break;
        case VCSlider::Parameter:
            slotModeParameterClicked();
        break;
    }
}

VCSliderProperties::~VCSliderProperties()
{
    delete m_inputSelWidget;
    delete m_ovrResetSelWidget;
}

/*****************************************************************************
 * General page
 *****************************************************************************/

void VCSliderProperties::slotModeLevelClicked()
{
    m_sliderMode = VCSlider::Level;

    m_nameEdit->setEnabled(true);

    setLevelPageVisibility(true);
    setPlaybackPageVisibility(false);
    setSubmasterPageVisibility(false);
    setParameterPageVisibility(false);
    setSpeedMasterPageVisibility(false);

    int cngType = m_slider->clickAndGoType();
    switch(cngType)
    {
        case ClickAndGoWidget::Red:
        case ClickAndGoWidget::Green:
        case ClickAndGoWidget::Blue:
        case ClickAndGoWidget::Cyan:
        case ClickAndGoWidget::Magenta:
        case ClickAndGoWidget::Yellow:
        case ClickAndGoWidget::Amber:
        case ClickAndGoWidget::White:
        case ClickAndGoWidget::UV:
        case ClickAndGoWidget::Lime:
        case ClickAndGoWidget::Indigo:
            m_cngColorCheck->setChecked(true);
        break;
        case ClickAndGoWidget::RGB:
            m_cngRGBCheck->setChecked(true);
        break;
        case ClickAndGoWidget::CMY:
            m_cngCMYCheck->setChecked(true);
        break;
        case ClickAndGoWidget::Preset:
            m_cngPresetCheck->setChecked(true);
        break;
        default:
        case ClickAndGoWidget::None:
            m_cngNoneCheck->setChecked(true);
        break;

    }
}

void VCSliderProperties::slotModePlaybackClicked()
{
    m_sliderMode = VCSlider::Playback;

    m_nameEdit->setEnabled(true);

    setLevelPageVisibility(false);
    setPlaybackPageVisibility(true);
    setSubmasterPageVisibility(false);
    setParameterPageVisibility(false);
    setSpeedMasterPageVisibility(false);
}

void VCSliderProperties::slotModeSubmasterClicked()
{
    m_sliderMode = VCSlider::Submaster;

    setLevelPageVisibility(false);
    setPlaybackPageVisibility(false);
    setSubmasterPageVisibility(true);
    setParameterPageVisibility(false);
    setSpeedMasterPageVisibility(false);
}

void VCSliderProperties::slotModeParameterClicked()
{
    m_sliderMode = VCSlider::Parameter;

    m_nameEdit->setEnabled(true);

    setLevelPageVisibility(false);
    setPlaybackPageVisibility(false);
    setSubmasterPageVisibility(false);
    setParameterPageVisibility(true);
    setSpeedMasterPageVisibility(false);
}

void VCSliderProperties::slotModeSpeedMasterClicked()
{
    m_sliderMode = VCSlider::SpeedMaster;

    m_nameEdit->setEnabled(true);

    setLevelPageVisibility(false);
    setPlaybackPageVisibility(false);
    setSubmasterPageVisibility(false);
    setParameterPageVisibility(false);
    setSpeedMasterPageVisibility(true);
}

void VCSliderProperties::buildSpeedMasterPage()
{
    m_speedMasterPage = new QWidget(m_tab);
    m_speedMasterPage->setObjectName(QStringLiteral("speedMasterTab"));
    QVBoxLayout *vbox = new QVBoxLayout(m_speedMasterPage);
    m_speedMasterBox = new QWidget(m_speedMasterPage);
    QVBoxLayout *box = new QVBoxLayout(m_speedMasterBox);
    box->setContentsMargins(0, 0, 0, 0);
    QLabel *info = new QLabel(tr("Speed master mode moves a speed master: its rate (the middle is "
                                 "the tempo as set, the top double, the bottom stopped) or its tempo "
                                 "between two BPMs. The slider follows the master when it is tapped "
                                 "or moved elsewhere. A master following another has neither: the "
                                 "slider reads n/a."), m_speedMasterBox);
    info->setWordWrap(true);
    box->addWidget(info);

    QFormLayout *form = new QFormLayout();
    const SpeedMasters *sm = m_doc->speedMasters();
    m_speedMasterCombo = new QComboBox(m_speedMasterBox);
    m_speedMasterCombo->setObjectName(QStringLiteral("speedMasterCombo"));
    for (int i = 0; i < qMax(sm->count(), m_slider->speedMaster() + 1); ++i)
        m_speedMasterCombo->addItem(sm->assignmentName(SpeedMasters::Master1 + i), i);
    m_speedMasterCombo->setCurrentIndex(m_speedMasterCombo->findData(m_slider->speedMaster()));
    form->addRow(tr("Master"), m_speedMasterCombo);

    QWidget *controls = new QWidget(m_speedMasterBox);
    QHBoxLayout *ch = new QHBoxLayout(controls);
    ch->setContentsMargins(0, 0, 0, 0);
    m_speedRateRadio = new QRadioButton(tr("Rate (0–200 %)"), controls);
    m_speedTempoRadio = new QRadioButton(tr("Tempo (BPM)"), controls);
    m_speedRateRadio->setObjectName(QStringLiteral("speedRateRadio"));
    m_speedTempoRadio->setObjectName(QStringLiteral("speedTempoRadio"));
    ch->addWidget(m_speedRateRadio);
    ch->addWidget(m_speedTempoRadio);
    ch->addStretch();
    form->addRow(tr("Controls"), controls);

    QWidget *range = new QWidget(m_speedMasterBox);
    QHBoxLayout *rh = new QHBoxLayout(range);
    rh->setContentsMargins(0, 0, 0, 0);
    m_tempoMinSpin = new QSpinBox(range);
    m_tempoMaxSpin = new QSpinBox(range);
    m_tempoMinSpin->setRange(1, 998);
    m_tempoMaxSpin->setRange(2, 999);
    m_tempoMinSpin->setSuffix(tr(" BPM"));
    m_tempoMaxSpin->setSuffix(tr(" BPM"));
    m_tempoMinSpin->setValue(m_slider->tempoMin());
    m_tempoMaxSpin->setValue(m_slider->tempoMax());
    rh->addWidget(m_tempoMinSpin);
    rh->addWidget(new QLabel(tr("to"), range));
    rh->addWidget(m_tempoMaxSpin);
    rh->addStretch();
    form->addRow(tr("Tempo range"), range);
    box->addLayout(form);

    connect(m_speedTempoRadio, &QRadioButton::toggled, range, &QWidget::setEnabled);
    if (m_slider->speedControl() == VCSlider::SpeedTempo)
        m_speedTempoRadio->setChecked(true);
    else
        m_speedRateRadio->setChecked(true);
    range->setEnabled(m_speedTempoRadio->isChecked());

    vbox->addWidget(m_speedMasterBox);
    vbox->addStretch();
    m_switchToSpeedMasterModeButton = new QPushButton(tr("Switch to Speed Master Mode"), m_speedMasterPage);
    m_switchToSpeedMasterModeButton->setToolTip(tr("Make the slider move a speed master"));
    connect(m_switchToSpeedMasterModeButton, &QPushButton::clicked,
            this, &VCSliderProperties::slotModeSpeedMasterClicked);
    vbox->addWidget(m_switchToSpeedMasterModeButton);
    m_tab->addTab(m_speedMasterPage, tr("Speed master"));
}

void VCSliderProperties::setSpeedMasterPageVisibility(bool visible)
{
    if (m_speedMasterBox == nullptr)
        return;
    m_speedMasterBox->setVisible(visible);
    m_switchToSpeedMasterModeButton->setVisible(!visible);
}

void VCSliderProperties::slotTabChanged()
{
    m_inputSelWidget->stopAutoDetection();
    m_ovrResetSelWidget->stopAutoDetection();
    m_flashInputWidget->stopAutoDetection();
}

void VCSliderProperties::setLevelPageVisibility(bool visible)
{
    m_levelValueRangeGroup->setVisible(visible);
    m_levelList->setVisible(visible);
    m_levelAllButton->setVisible(visible);
    m_levelNoneButton->setVisible(visible);
    m_levelInvertButton->setVisible(visible);
    m_levelByGroupButton->setVisible(visible);
    m_clickngoGroup->setVisible(visible);
    m_monitorValuesCheck->setVisible(visible);
    if (m_monitorValuesCheck->isChecked() && m_ovrResetSelWidget != NULL)
        m_ovrResetSelWidget->setVisible(visible);

    if (visible == true)
    {
        m_switchToLevelModeButton->hide();
        //m_levelSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
    }
    else
    {
        m_switchToLevelModeButton->show();
        //m_levelSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
}

void VCSliderProperties::setPlaybackPageVisibility(bool visible)
{
    m_playbackFunctionGroup->setVisible(visible);
    m_flashButtonCheck->setVisible(visible);
    m_flashInputWidget->setVisible(visible);

    if (visible == true)
    {
        m_switchToPlaybackModeButton->hide();
        m_playbackSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
    }
    else
    {
        m_switchToPlaybackModeButton->show();
        m_playbackSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
}

void VCSliderProperties::setSubmasterPageVisibility(bool visible)
{
    m_submasterInfo->setVisible(visible);

    if (visible == true)
    {
        m_switchToSubmasterModeButton->hide();
        m_submasterSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
    }
    else
    {
        m_switchToSubmasterModeButton->show();
        m_submasterSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
}

void VCSliderProperties::setParameterPageVisibility(bool visible)
{
    m_parameterInfo->setVisible(visible);
    m_parameterRoleLabel->setVisible(visible);
    m_parameterRoleCombo->setVisible(visible);
    m_parameterByteLabel->setVisible(visible);
    m_parameterMSBRadio->setVisible(visible);
    m_parameterLSBRadio->setVisible(visible);

    if (visible == true)
    {
        m_switchToParameterModeButton->hide();
        m_parameterSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
    }
    else
    {
        m_switchToParameterModeButton->show();
        m_parameterSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
}

/*****************************************************************************
 * Level page
 *****************************************************************************/

void VCSliderProperties::levelUpdateFixtures()
{
    foreach (Fixture* fixture, m_doc->fixtures())
    {
        Q_ASSERT(fixture != NULL);
        levelUpdateFixtureNode(fixture->id());
    }
    m_levelList->header()->resizeSections(QHeaderView::ResizeToContents);
}

void VCSliderProperties::levelUpdateFixtureNode(quint32 id)
{
    QTreeWidgetItem* item;
    Fixture* fxi;
    QString str;

    fxi = m_doc->fixture(id);
    Q_ASSERT(fxi != NULL);

    item = levelFixtureNode(id);
    if (item == NULL)
    {
        item = new QTreeWidgetItem(m_levelList);
        item->setText(KColumnID, str.setNum(id));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
    }

    item->setText(KColumnName, fxi->name());
    item->setIcon(KColumnName, ItemIcons::fixtureIcon(fxi));
    item->setText(KColumnType, fxi->typeString());

    levelUpdateChannels(item, fxi);

}

QTreeWidgetItem* VCSliderProperties::levelFixtureNode(quint32 id)
{
    for (int i = 0; i < m_levelList->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *item = m_levelList->topLevelItem(i);
        if (item->text(KColumnID).toUInt() == id)
            return item;
    }

    return NULL;
}

void VCSliderProperties::levelUpdateChannels(QTreeWidgetItem* parent,
                                             Fixture* fxi)
{
    quint32 channels = 0;
    quint32 ch = 0;

    Q_ASSERT(parent != NULL);
    Q_ASSERT(fxi != NULL);

    channels = fxi->channels();
    for (ch = 0; ch < channels; ch++)
        levelUpdateChannelNode(parent, fxi, ch);
}

void VCSliderProperties::levelUpdateChannelNode(QTreeWidgetItem* parent,
                                                Fixture* fxi, quint32 ch)
{
    Q_ASSERT(parent != NULL);

    if (fxi == NULL)
        return;

    const QLCChannel* channel = fxi->channel(ch);
    if (channel == NULL)
        return;

    QTreeWidgetItem* item = levelChannelNode(parent, ch);
    if (item == NULL)
    {
        item = new QTreeWidgetItem(parent);
        item->setText(KColumnID, QString::number(ch));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(KColumnName, Qt::Unchecked);
    }

    item->setText(KColumnName, QString("%1:%2").arg(ch + 1, 3, 10, QChar('0'))
                  .arg(channel->name()));
    item->setIcon(KColumnName, channel->getIcon());
    if (channel->group() == QLCChannel::Intensity &&
        channel->colour() != QLCChannel::NoColour)
        item->setText(KColumnType, QLCChannel::colourToString(channel->colour()));
    else
        item->setText(KColumnType, QLCChannel::groupToString(channel->group()));


    levelUpdateCapabilities(item, channel);
}

QTreeWidgetItem* VCSliderProperties::levelChannelNode(QTreeWidgetItem* parent,
                                                      quint32 ch)
{
    Q_ASSERT(parent != NULL);

    for (int i = 0; i < parent->childCount(); i++)
    {
        QTreeWidgetItem* item = parent->child(i);
        if (item->text(KColumnID).toUInt() == ch)
            return item;
    }

    return NULL;
}

void VCSliderProperties::levelUpdateCapabilities(QTreeWidgetItem* parent,
                                                 const QLCChannel* channel)
{
    Q_ASSERT(parent != NULL);
    Q_ASSERT(channel != NULL);

    QListIterator <QLCCapability*> it(channel->capabilities());
    while (it.hasNext() == true)
        levelUpdateCapabilityNode(parent, it.next());
}

void VCSliderProperties::levelUpdateCapabilityNode(QTreeWidgetItem* parent,
        QLCCapability* cap)
{
    QTreeWidgetItem* item;
    QString str;

    Q_ASSERT(parent != NULL);
    Q_ASSERT(cap != NULL);

    item = new QTreeWidgetItem(parent);
    item->setText(KColumnName, cap->name());
    item->setText(KColumnRange, str.asprintf("%.3d - %.3d",
                                             cap->min(), cap->max()));
    item->setFlags(item->flags() & (~Qt::ItemIsUserCheckable));
}

void VCSliderProperties::levelUpdateChannelSelections()
{
    /* Check all items that are present in the slider's list of
       controlled channels. We don't need to set other items off,
       because this function is run only during init when everything
       is off. */
    QListIterator <VCSlider::LevelChannel> it(m_slider->m_levelChannels);
    while (it.hasNext() == true)
    {
        VCSlider::LevelChannel lch(it.next());

        QTreeWidgetItem* fxiNode = levelFixtureNode(lch.fixture);
        if (fxiNode == NULL)
            continue;

        QTreeWidgetItem* chNode = levelChannelNode(fxiNode, lch.channel);
        if (chNode == NULL)
            continue;

        chNode->setCheckState(KColumnName, Qt::Checked);
    }
}

void VCSliderProperties::levelSelectChannelsByGroup(QString group)
{
    QTreeWidgetItem* fxi_item;
    QTreeWidgetItem* ch_item;
    int i;
    int j;

    /* Go thru only channel items. Fixture items get (partially) selected
       according to their children's state */
    for (i = 0; i < m_levelList->topLevelItemCount(); i++)
    {
        fxi_item = m_levelList->topLevelItem(i);
        Q_ASSERT(fxi_item != NULL);

        for (j = 0; j < fxi_item->childCount(); j++)
        {
            ch_item = fxi_item->child(j);
            Q_ASSERT(ch_item != NULL);

            if (ch_item->text(KColumnType) == group)
                ch_item->setCheckState(KColumnName, Qt::Checked);
        }
    }
}

void VCSliderProperties::slotLevelLowSpinChanged(int value)
{
    if (value >= m_levelHighLimitSpin->value())
        m_levelHighLimitSpin->setValue(value + 1);
}

void VCSliderProperties::slotLevelHighSpinChanged(int value)
{
    if (value <= m_levelLowLimitSpin->value())
        m_levelLowLimitSpin->setValue(value - 1);
}

void VCSliderProperties::slotLevelCapabilityClicked()
{
    QTreeWidgetItem* item;
    QStringList list;

    item = m_levelList->currentItem();
    if (item == NULL || item->parent() == NULL ||
            item->parent()->parent() == NULL)
        return;

    list = item->text(KColumnRange).split("-");
    Q_ASSERT(list.size() == 2);

    m_levelLowLimitSpin->setValue(list[0].toInt());
    m_levelHighLimitSpin->setValue(list[1].toInt());
}

void VCSliderProperties::slotLevelListClicked(QTreeWidgetItem* item)
{
    /* Enable the capability button if a capability has been selected */
    if (item != NULL && item->parent() != NULL &&
            item->parent()->parent() != NULL)
    {
        m_levelCapabilityButton->setEnabled(true);
    }
    else
    {
        m_levelCapabilityButton->setEnabled(false);
    }
}

void VCSliderProperties::slotLevelAllClicked()
{
    /* Set all fixture items selected, their children should get selected
       as well because the fixture items are Controller items. */
    for (int i = 0; i < m_levelList->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *fxi_item = m_levelList->topLevelItem(i);
        Q_ASSERT(fxi_item != NULL);

        fxi_item->setCheckState(KColumnName, Qt::Checked);
    }
}

void VCSliderProperties::slotLevelNoneClicked()
{
    /* Set all fixture items unselected, their children should get unselected
       as well because the fixture items are Controller items. */
    for (int i = 0; i < m_levelList->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *fxi_item = m_levelList->topLevelItem(i);
        Q_ASSERT(fxi_item != NULL);

        fxi_item->setCheckState(KColumnName, Qt::Unchecked);
    }
}

void VCSliderProperties::slotLevelInvertClicked()
{
    /* Go through only channel items. Fixture items get (partially) selected
       according to their children's state */
    for (int i = 0; i < m_levelList->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *fxi_item = m_levelList->topLevelItem(i);
        Q_ASSERT(fxi_item != NULL);

        for (int j = 0; j < fxi_item->childCount(); j++)
        {
            QTreeWidgetItem *ch_item = fxi_item->child(j);
            Q_ASSERT(ch_item != NULL);

            if (ch_item->checkState(KColumnName) == Qt::Checked)
                ch_item->setCheckState(KColumnName,
                                       Qt::Unchecked);
            else
                ch_item->setCheckState(KColumnName,
                                       Qt::Checked);
        }
    }
}

void VCSliderProperties::slotLevelByGroupClicked()
{
    bool ok = false;
    QString group;
    QStringList groups;

    foreach (Fixture* fixture, m_doc->fixtures())
    {
        Q_ASSERT(fixture != NULL);

        for (quint32 i = 0; i < fixture->channels(); i++)
        {
            const QLCChannel *channel = fixture->channel(i);

            QString property = QLCChannel::groupToString(channel->group());

            if (channel->group() == QLCChannel::Intensity &&
                channel->colour() != QLCChannel::NoColour)
            {
                property = QLCChannel::colourToString(channel->colour());
            }

            if (groups.contains(property) == false)
                groups.append(property);
        }
    }

    group = QInputDialog::getItem(this,
                                  tr("Select channels by group"),
                                  tr("Select a channel group"),
                                  groups, 0,
                                  false, &ok);

    if (ok == true)
        levelSelectChannelsByGroup(group);
}

void VCSliderProperties::slotItemExpanded()
{
    m_levelList->header()->resizeSections(QHeaderView::ResizeToContents);
}

void VCSliderProperties::slotMonitorCheckClicked(bool checked)
{
    if (checked == true)
        m_ovrResetSelWidget->show();
    else
        m_ovrResetSelWidget->hide();
}

/*****************************************************************************
 * Playback page
 *****************************************************************************/

void VCSliderProperties::slotAttachPlaybackFunctionClicked()
{
    FunctionSelection fs(this, m_doc);
    fs.setMultiSelection(false);
    fs.setFilter(Function::SceneType | Function::ChaserType | Function::SequenceType |
                 Function::EFXType | Function::AudioType | Function::RGBMatrixType |
                 Function::CollectionType | Function::VideoType, false);
    fs.disableFilters(Function::ScriptType | Function::ShowType);

    if (fs.exec() != QDialog::Accepted)
        return;

    if (fs.selection().size() == 0)
        return;

    m_playbackFunctionId = fs.selection().first();
    updatePlaybackFunctionName();
}

void VCSliderProperties::slotDetachPlaybackFunctionClicked()
{
    m_playbackFunctionId = Function::invalidId();
    updatePlaybackFunctionName();
}

void VCSliderProperties::slotFlashCheckClicked(bool checked)
{
    m_flashInputWidget->setEnabled(checked);
}

void VCSliderProperties::updatePlaybackFunctionName()
{
    Function* function = m_doc->function(m_playbackFunctionId);
    if (function != NULL)
    {
        m_playbackFunctionEdit->setText(function->name());
        if (m_nameEdit->text().simplified().isEmpty() == true)
            m_nameEdit->setText(function->name());
    }
    else
    {
        m_playbackFunctionId = Function::invalidId(); // Ensure
        m_playbackFunctionEdit->setText(tr("No function"));
    }
}

/*****************************************************************************
 * OK & Cancel
 *****************************************************************************/

void VCSliderProperties::checkMajorColor(int *comp, int *max, int type)
{
    if (*comp > *max)
    {
        *max = *comp;
        m_slider->setClickAndGoType((ClickAndGoWidget::ClickAndGo)type);
    }
}

void VCSliderProperties::storeLevelChannels()
{
    int red = 0, green = 0, blue = 0;
    int cyan = 0, magenta = 0, yellow = 0, amber = 0, white = 0, uv = 0, lime = 0, indigo = 0;
    int majorColor = 0;
    /* Clear all channels from the slider first */
    m_slider->clearLevelChannels();

    /* Go thru all fixtures and their channels, add checked channels */
    for (int i = 0; i < m_levelList->topLevelItemCount(); i++)
    {
        QTreeWidgetItem* fxi_item = m_levelList->topLevelItem(i);
        Q_ASSERT(fxi_item != NULL);

        quint32 fxi_id = fxi_item->text(KColumnID).toUInt();
        Fixture *fxi = m_doc->fixture(fxi_id);

        for (int j = 0; j < fxi_item->childCount(); j++)
        {
            QTreeWidgetItem* ch_item = fxi_item->child(j);
            Q_ASSERT(ch_item != NULL);

            if (ch_item->checkState(KColumnName) == Qt::Checked)
            {
                quint32 ch_num = ch_item->text(KColumnID).toUInt();
                if (fxi != NULL)
                {
                    const QLCChannel *ch = fxi->channel(ch_num);
                    if (ch->group() == QLCChannel::Intensity &&
                        m_cngColorCheck->isChecked())
                    {
                        if (ch->colour() == QLCChannel::Red)
                        {
                            red++;
                            checkMajorColor(&red, &majorColor, ClickAndGoWidget::Red);
                        }
                        else if (ch->colour() == QLCChannel::Green)
                        {
                            green++;
                            checkMajorColor(&green, &majorColor, ClickAndGoWidget::Green);
                        }
                        else if (ch->colour() == QLCChannel::Blue)
                        {
                            blue++;
                            checkMajorColor(&blue, &majorColor, ClickAndGoWidget::Blue);
                        }
                        else if (ch->colour() == QLCChannel::Cyan)
                        {
                            cyan++;
                            checkMajorColor(&cyan, &majorColor, ClickAndGoWidget::Cyan);
                        }
                        else if (ch->colour() == QLCChannel::Magenta)
                        {
                            magenta++;
                            checkMajorColor(&magenta, &majorColor, ClickAndGoWidget::Magenta);
                        }
                        else if (ch->colour() == QLCChannel::Yellow)
                        {
                            yellow++;
                            checkMajorColor(&yellow, &majorColor, ClickAndGoWidget::Yellow);
                        }
                        else if (ch->colour() == QLCChannel::Amber)
                        {
                            amber++;
                            checkMajorColor(&amber, &majorColor, ClickAndGoWidget::Amber);
                        }
                        else if (ch->colour() == QLCChannel::White)
                        {
                            white++;
                            checkMajorColor(&white, &majorColor, ClickAndGoWidget::White);
                        }
                        else if (ch->colour() == QLCChannel::UV)
                        {
                            uv++;
                            checkMajorColor(&uv, &majorColor, ClickAndGoWidget::UV);
                        }
                        else if (ch->colour() == QLCChannel::Lime)
                        {
                            lime++;
                            checkMajorColor(&lime, &majorColor, ClickAndGoWidget::Lime);
                        }
                        else if (ch->colour() == QLCChannel::Indigo)
                        {
                            indigo++;
                            checkMajorColor(&indigo, &majorColor, ClickAndGoWidget::Indigo);
                        }
                    }
                }
                m_slider->addLevelChannel(fxi_id, ch_num);
            }
        }
    }
}

void VCSliderProperties::accept()
{
    /* Widget appearance */
    if (m_widgetKnobRadio->isChecked())
        m_slider->setWidgetStyle(VCSlider::WKnob);
    else
        m_slider->setWidgetStyle(VCSlider::WSlider);

    /* Values catching */
    if (m_catchValueCheck->isChecked())
        m_slider->setCatchValues(true);
    else
        m_slider->setCatchValues(false);

    /* Level page */
    bool limitDiff =
        (m_slider->levelLowLimit() != m_levelLowLimitSpin->value()) ||
        (m_slider->levelHighLimit() != m_levelLowLimitSpin->value());
    m_slider->setLevelLowLimit(m_levelLowLimitSpin->value());
    m_slider->setLevelHighLimit(m_levelHighLimitSpin->value());
    storeLevelChannels();

    /* Click & Go group */
    /* Color doesn't have a case cause it is calculated
     * in storeLevelChannels */
    if (m_cngNoneCheck->isChecked())
        m_slider->setClickAndGoType(ClickAndGoWidget::None);
    else if (m_cngRGBCheck->isChecked())
        m_slider->setClickAndGoType(ClickAndGoWidget::RGB);
    else if (m_cngCMYCheck->isChecked())
        m_slider->setClickAndGoType(ClickAndGoWidget::CMY);
    else if (m_cngPresetCheck->isChecked())
        m_slider->setClickAndGoType(ClickAndGoWidget::Preset);

    /* Playback page */
    m_slider->setPlaybackFunction(m_playbackFunctionId);
    m_slider->setPlaybackFlashEnable(m_flashButtonCheck->isChecked());
    m_slider->setPlaybackFlashKeySequence(m_flashInputWidget->keySequence());
    m_slider->setInputSource(m_flashInputWidget->inputSource(), VCSlider::flashButtonInputSourceId);

    /* Parameter page */
    m_slider->setParameterRole(static_cast<VCSlider::ParameterRole>(
        m_parameterRoleCombo->currentData().toInt()));
    m_slider->setParameterControlByte(m_parameterLSBRadio->isChecked() ? 1 : 0);

    /* Speed master page */
    m_slider->setSpeedMaster(m_speedMasterCombo->currentData().toInt());
    m_slider->setSpeedControl(m_speedTempoRadio->isChecked() ? VCSlider::SpeedTempo : VCSlider::SpeedRate);
    m_slider->setTempoRange(qMin(m_tempoMinSpin->value(), m_tempoMaxSpin->value() - 1),
                            qMax(m_tempoMaxSpin->value(), m_tempoMinSpin->value() + 1));

    /* Slider mode */
    if (m_slider->sliderMode() != m_sliderMode)
    {
        m_slider->setSliderMode(VCSlider::SliderMode(m_sliderMode));
        if (m_slider->sliderMode() == VCSlider::Submaster)
        {
            m_slider->setLevelValue(UCHAR_MAX);
            m_slider->setSliderValue(UCHAR_MAX);
        }
    }
    else if (limitDiff && m_slider->sliderMode() == VCSlider::Level)
    {
        // Force the refresh of the slider range
        m_slider->setSliderMode(VCSlider::Level);
    }

    if (m_slider->sliderMode() == VCSlider::Level)
    {
        m_slider->setChannelsMonitorEnabled(m_monitorValuesCheck->isChecked());
        m_slider->setOverrideResetKeySequence(m_ovrResetSelWidget->keySequence());
        m_slider->setInputSource(m_ovrResetSelWidget->inputSource(), VCSlider::overrideResetInputSourceId);
    }

    m_slider->setCaption(m_nameEdit->text());

    /* Value style */
    if (m_valueExactRadio->isChecked() == true)
        m_slider->setValueDisplayStyle(VCSlider::ExactValue);
    else
        m_slider->setValueDisplayStyle(VCSlider::PercentageValue);

    /* Slider movement */
    if (m_sliderMovementNormalRadio->isChecked() == true)
        m_slider->setInvertedAppearance(false);
    else
        m_slider->setInvertedAppearance(true);

    /* External input */
    m_slider->setInputSource(m_inputSelWidget->inputSource(), VCSlider::sliderInputSourceId);

    /* Close dialog */
    QDialog::accept();
}
