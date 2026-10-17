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

#include <QFormLayout>

#include "appsettings.h"
#include "showtreeview.h"
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

    // --- Show Tree ---
    // Record as new cue: what to record when the selected cue, the list's
    // running cue and everything lit differ. Ask, or one of them -- per mode.
    QGroupBox *stGroup = new QGroupBox(tr("Show Tree"), this);
    stGroup->setObjectName(QStringLiteral("settingsShowTree"));
    stGroup->setProperty("helpTopic", QStringLiteral("show-tree.md"));
    QFormLayout *stLayout = new QFormLayout(stGroup);
    auto recordCombo = [this, stGroup](bool perform) {
        QComboBox *c = new QComboBox(stGroup);
        c->setObjectName(perform ? QStringLiteral("settingsRecordPerform") : QStringLiteral("settingsRecordDesign"));
        c->addItem(tr("Ask"), QStringLiteral("ask"));
        c->addItem(tr("The selected cue"), QStringLiteral("selected"));
        c->addItem(tr("The running cue"), QStringLiteral("running"));
        c->addItem(tr("Everything lit"), QStringLiteral("lit"));
        c->setToolTip(tr("Record as new cue, when what is on stage differs: ask, or record this. "
                         "When it names something not on stage (nothing running), it asks."));
        const int idx = c->findData(ShowTreeView::recordSetting(perform));
        c->setCurrentIndex(idx < 0 ? 0 : idx);
        return c;
    };
    m_recordDesignCombo = recordCombo(false);
    m_recordPerformCombo = recordCombo(true);
    stLayout->addRow(tr("Record in Design:"), m_recordDesignCombo);
    stLayout->addRow(tr("Record in Perform (Operate):"), m_recordPerformCombo);
    mainLayout->addWidget(stGroup);

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

    // Show Tree: what Record records when the choices differ.
    QSettings().setValue(ShowTreeView::recordSettingKey(false), m_recordDesignCombo->currentData().toString());
    QSettings().setValue(ShowTreeView::recordSettingKey(true), m_recordPerformCombo->currentData().toString());

    // Effect script editor preference (read by the Look Editor authoring flow).
    QSettings().setValue(QStringLiteral("effectscript/editor"),
                         m_scriptEditorCombo->currentData().toString());

    QDialog::accept();
}

