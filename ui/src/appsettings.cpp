/*
  Q Light Controller Plus
  appsettings.cpp

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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSettings>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>

#include "appsettings.h"
#include "measurementunits.h"
#include "monitor.h"
#include "app.h"

AppSettings::AppSettings(App *app, QWidget *parent)
    : QDialog(parent)
    , m_app(app)
{
    Q_ASSERT(app != nullptr);

    setWindowTitle(tr("Application Settings"));
    setMinimumWidth(400);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Autosave Group
    QGroupBox *autosaveGroup = new QGroupBox(tr("Autosave"), this);
    QVBoxLayout *autosaveLayout = new QVBoxLayout(autosaveGroup);

    // Enable checkbox
    m_autosaveEnabledCheck = new QCheckBox(tr("Enable autosave"), autosaveGroup);
    m_autosaveEnabledCheck->setChecked(m_app->isAutosaveEnabled());
    m_autosaveEnabledCheck->setToolTip(tr("Automatically save a backup of your workspace periodically"));
    autosaveLayout->addWidget(m_autosaveEnabledCheck);

    // Interval row
    QHBoxLayout *intervalLayout = new QHBoxLayout();
    QLabel *intervalLabel = new QLabel(tr("Autosave interval:"), autosaveGroup);
    intervalLayout->addWidget(intervalLabel);

    m_autosaveIntervalSpin = new QSpinBox(autosaveGroup);
    m_autosaveIntervalSpin->setMinimum(1);
    m_autosaveIntervalSpin->setMaximum(60);
    m_autosaveIntervalSpin->setValue(m_app->autosaveInterval());
    m_autosaveIntervalSpin->setSuffix(tr(" minutes"));
    m_autosaveIntervalSpin->setToolTip(tr("How often to save a backup (1-60 minutes)"));
    intervalLayout->addWidget(m_autosaveIntervalSpin);

    intervalLayout->addStretch();
    autosaveLayout->addLayout(intervalLayout);

    // Info label
    QLabel *infoLabel = new QLabel(
        tr("Autosave creates a backup file (.qxw.autosave) that can be\n"
           "recovered if the application closes unexpectedly."),
        autosaveGroup);
    infoLabel->setStyleSheet("color: gray; font-size: 10px;");
    autosaveLayout->addWidget(infoLabel);

    mainLayout->addWidget(autosaveGroup);

    // Tab Label Mode Group
    QGroupBox *tabGroup = new QGroupBox(tr("Tab Bar"), this);
    QHBoxLayout *tabLayout = new QHBoxLayout(tabGroup);
    tabLayout->addWidget(new QLabel(tr("Tab labels:"), tabGroup));
    m_tabLabelModeCombo = new QComboBox(tabGroup);
    m_tabLabelModeCombo->addItem(tr("Icons and text"), App::TabIconAndText);
    m_tabLabelModeCombo->addItem(tr("Icons only"),     App::TabIconOnly);
    m_tabLabelModeCombo->addItem(tr("Text only"),      App::TabTextOnly);
    m_tabLabelModeCombo->setCurrentIndex(m_tabLabelModeCombo->findData(m_app->tabLabelMode()));
    tabLayout->addWidget(m_tabLabelModeCombo);
    tabLayout->addStretch();
    mainLayout->addWidget(tabGroup);

    // --- Units ---
    // One choice for the person, whatever show is open: fixture dimensions
    // and weights (here and in the fixture definition editor) and the
    // Lighting Studio's grid (App::applyUnitPreference) show in it.
    QGroupBox *unitsGroup = new QGroupBox(tr("Units"), this);
    unitsGroup->setObjectName(QStringLiteral("settingsUnits"));
    unitsGroup->setProperty("helpTopic", QStringLiteral("lighting-studio.md"));
    QHBoxLayout *unitsLayout = new QHBoxLayout(unitsGroup);
    unitsLayout->addWidget(new QLabel(tr("Lengths and weights in:"), unitsGroup));
    m_unitsCombo = new QComboBox(unitsGroup);
    m_unitsCombo->setObjectName(QStringLiteral("settingsUnitsCombo"));
    for (MeasurementUnits::LengthUnit u : { MeasurementUnits::Millimetres,
                                            MeasurementUnits::Centimetres,
                                            MeasurementUnits::Metres,
                                            MeasurementUnits::Inches,
                                            MeasurementUnits::Feet })
        m_unitsCombo->addItem(MeasurementUnits::name(u), int(u));
    m_unitsCombo->setCurrentIndex(qMax(0, m_unitsCombo->findData(int(MeasurementUnits::preferredLength()))));
    m_unitsCombo->setToolTip(tr("How lengths and weights are shown: fixture sizes here and in "
                                "the fixture definition editor (any unit can still be typed: "
                                "7.5\", 19 cm, 26 lb), and the Lighting Studio's grid, rulers "
                                "and fields -- metres for mm, cm and m, feet for inches and "
                                "feet. Files keep millimetres and kilograms either way."));
    unitsLayout->addWidget(m_unitsCombo);
    unitsLayout->addStretch();
    mainLayout->addWidget(unitsGroup);

    // --- Effects ---
    QGroupBox *fxGroup = new QGroupBox(tr("Effects"), this);
    QHBoxLayout *fxLayout = new QHBoxLayout(fxGroup);
    fxLayout->addWidget(new QLabel(tr("Effect script editor:"), fxGroup));
    m_scriptEditorCombo = new QComboBox(fxGroup);
    m_scriptEditorCombo->addItem(tr("In-app editor"), QStringLiteral("internal"));
    m_scriptEditorCombo->addItem(tr("External editor (system default)"), QStringLiteral("external"));
    m_scriptEditorCombo->setToolTip(
        tr("Where \"New/Edit effect script…\" opens a .js — the built-in editor or "
           "your operating system's default editor for .js files."));
    {
        QSettings s;
        const QString cur = s.value(QStringLiteral("effectscript/editor"),
                                    QStringLiteral("internal")).toString();
        int idx = m_scriptEditorCombo->findData(cur);
        m_scriptEditorCombo->setCurrentIndex(idx < 0 ? 0 : idx);
    }
    fxLayout->addWidget(m_scriptEditorCombo);
    fxLayout->addStretch();
    mainLayout->addWidget(fxGroup);

    // --- Show control (QLab) ---
    // Edited in one place: Connections' Show control & sync page, next to
    // the MIDI inputs MSC and timecode arrive on. This just points there.
    QGroupBox *scGroup = new QGroupBox(tr("Show control (QLab)"), this);
    scGroup->setObjectName(QStringLiteral("settingsShowControl"));
    scGroup->setProperty("helpTopic", QStringLiteral("show-control.md"));
    QHBoxLayout *scLayout = new QHBoxLayout(scGroup);
    QLabel *scWhere = new QLabel(tr("OSC and MIDI Show Control in, and MIDI timecode, are set "
                                    "up in Connections → Show control & sync."), scGroup);
    scWhere->setWordWrap(true);
    scLayout->addWidget(scWhere, 1);
    QPushButton *scOpen = new QPushButton(tr("Open"), scGroup);
    scOpen->setObjectName(QStringLiteral("settingsOpenShowControl"));
    scOpen->setToolTip(tr("Keeps the changes made here and opens Connections at "
                          "Show control & sync."));
    connect(scOpen, &QPushButton::clicked, this, [this]() {
        accept();
        m_app->showShowControlSettings();
    });
    scLayout->addWidget(scOpen, 0, Qt::AlignTop);
    mainLayout->addWidget(scGroup);

    // Add stretch to push buttons to bottom
    mainLayout->addStretch();

    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    mainLayout->addWidget(buttonBox);

    // Connect enable checkbox to enable/disable interval spinbox
    connect(m_autosaveEnabledCheck, &QCheckBox::toggled,
            m_autosaveIntervalSpin, &QSpinBox::setEnabled);
    m_autosaveIntervalSpin->setEnabled(m_autosaveEnabledCheck->isChecked());
}

AppSettings::~AppSettings()
{
}

void AppSettings::accept()
{
    // Apply autosave settings
    m_app->setAutosaveEnabled(m_autosaveEnabledCheck->isChecked());
    m_app->setAutosaveInterval(m_autosaveIntervalSpin->value());

    // Apply tab label mode
    const int mode = m_tabLabelModeCombo->currentData().toInt();
    m_app->setTabLabelMode(mode);

    const MeasurementUnits::LengthUnit unit =
            MeasurementUnits::LengthUnit(m_unitsCombo->currentData().toInt());
    if (unit != MeasurementUnits::preferredLength())
    {
        MeasurementUnits::setPreferredLength(unit);
        m_app->applyUnitPreference(false);
        if (Monitor::instance() != nullptr)
            Monitor::instance()->unitsChanged();
    }

    // Effect script editor preference (read by the Look Editor authoring flow).
    QSettings().setValue(QStringLiteral("effectscript/editor"),
                         m_scriptEditorCombo->currentData().toString());

    QDialog::accept();
}

