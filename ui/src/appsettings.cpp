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

#include <QNetworkInterface>
#include <QGridLayout>

#include "showcontrollistener.h"
#include "appsettings.h"
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

    // --- Show control (QLab) ---
    ShowControlListener *listener = m_app->showControlListener();
    QGroupBox *scGroup = new QGroupBox(tr("Show control (QLab)"), this);
    scGroup->setObjectName(QStringLiteral("settingsShowControl"));
    scGroup->setProperty("helpTopic", QStringLiteral("show-control.md"));
    QGridLayout *scLayout = new QGridLayout(scGroup);

    m_oscCheck = new QCheckBox(tr("Listen for OSC cue commands on port"), scGroup);
    m_oscCheck->setChecked(ShowControlListener::oscEnabled());
    m_oscCheck->setToolTip(tr("QLab's Network cue (or anything that sends OSC) can GO a cue: "
                              "/go, /cue/12/start, /stop... Anyone on the network can, so it's off "
                              "until you turn it on."));
    m_oscPortSpin = new QSpinBox(scGroup);
    m_oscPortSpin->setRange(1, 65535);
    m_oscPortSpin->setValue(ShowControlListener::oscPort());
    m_oscPortSpin->setToolTip(tr("Not 53000 or 53001 if QLab runs on this computer: it uses those."));
    scLayout->addWidget(m_oscCheck, 0, 0);
    scLayout->addWidget(m_oscPortSpin, 0, 1);

    QStringList addresses;
    for (const QHostAddress &a : QNetworkInterface::allAddresses())
        if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback())
            addresses << a.toString();
    QLabel *where = new QLabel(addresses.isEmpty()
        ? tr("Send to this computer as localhost (127.0.0.1).")
        : tr("Send to this computer at %1 (or 127.0.0.1 on this computer).")
              .arg(addresses.join(tr(" or "))), scGroup);
    where->setWordWrap(true);
    where->setTextInteractionFlags(Qt::TextSelectableByMouse);
    scLayout->addWidget(where, 1, 0, 1, 3);

    m_mscCheck = new QCheckBox(tr("Answer MIDI Show Control as device"), scGroup);
    m_mscCheck->setChecked(ShowControlListener::mscEnabled());
    m_mscCheck->setToolTip(tr("MSC GO, STOP, RESUME, LOAD, ALL_OFF, RESET and GO_OFF arriving on "
                              "a patched MIDI input (Connections), for lighting, moving lights or "
                              "all types. The all-call (127) is always answered."));
    m_mscDeviceSpin = new QSpinBox(scGroup);
    m_mscDeviceSpin->setRange(0, 0x6F);
    m_mscDeviceSpin->setValue(ShowControlListener::mscDeviceId());
    scLayout->addWidget(m_mscCheck, 2, 0);
    scLayout->addWidget(m_mscDeviceSpin, 2, 1);

    QLabel *status = new QLabel(scGroup);
    status->setObjectName(QStringLiteral("showControlStatus"));
    QLabel *last = new QLabel(scGroup);
    last->setObjectName(QStringLiteral("showControlLast"));
    last->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto showLast = [last](const QString &what) {
        last->setText(what.isEmpty() ? tr("Nothing received yet.") : tr("Last: %1").arg(what));
    };
    if (listener != nullptr)
    {
        status->setText(listener->status());
        showLast(listener->lastCommand());
        connect(listener, &ShowControlListener::statusChanged, status, &QLabel::setText);
        connect(listener, &ShowControlListener::lastCommandChanged, last, showLast);
    }
    scLayout->addWidget(status, 3, 0, 1, 3);
    scLayout->addWidget(last, 4, 0, 1, 3);
    scLayout->setColumnStretch(2, 1);
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

    // Effect script editor preference (read by the Look Editor authoring flow).
    QSettings().setValue(QStringLiteral("effectscript/editor"),
                         m_scriptEditorCombo->currentData().toString());

    ShowControlListener::setSettings(m_oscCheck->isChecked(), m_oscPortSpin->value(),
                                     m_mscCheck->isChecked(), m_mscDeviceSpin->value());
    if (ShowControlListener *listener = m_app->showControlListener())
        listener->apply();

    QDialog::accept();
}

