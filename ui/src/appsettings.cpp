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
#include <QMessageBox>
#include <QLineEdit>

#include <QFormLayout>

#include "appsettings.h"
#include "showtreeview.h"
#include "measurementunits.h"
#include "monitor.h"
#include "inputoutputmap.h"
#include "rignetwork.h"
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

    // --- Network: rig networks and the transmit gate ---
    // The networks are the SHOW's (saved in it); "only transmit on them" is
    // this machine's; "transmit anyway" lasts this session and is never saved.
    InputOutputMap *ioMap = m_app->doc()->inputOutputMap();
    QGroupBox *netGroup = new QGroupBox(tr("Network"), this);
    netGroup->setObjectName(QStringLiteral("settingsRigNetwork"));
    netGroup->setProperty("helpTopic", QStringLiteral("other-improvements.md#only-transmit-on-the-rig-network"));
    QVBoxLayout *netLayout = new QVBoxLayout(netGroup);
    QHBoxLayout *rigRow = new QHBoxLayout();
    rigRow->addWidget(new QLabel(tr("This show's rig networks:"), netGroup));
    m_rigNetworksEdit = new QLineEdit(netGroup);
    m_rigNetworksEdit->setObjectName(QStringLiteral("settingsRigNetworks"));
    m_rigNetworksEdit->setText(ioMap->rigNetworks().join(QStringLiteral(", ")));
    m_rigNetworksEdit->setToolTip(tr("The IPv4 networks this show's Art-Net, sACN (E1.31), OSC "
                                     "and WLED output belongs on, e.g. 172.18.0.0/20 -- several "
                                     "separated by commas. Saved with the show."));
    const QStringList suggested = ioMap->suggestedRigNetworks();
    m_rigNetworksEdit->setPlaceholderText(suggested.isEmpty()
        ? tr("e.g. 172.18.0.0/20")
        : tr("e.g. %1 (from this show's patches)").arg(suggested.join(QStringLiteral(", "))));
    rigRow->addWidget(m_rigNetworksEdit, 1);
    if (ioMap->rigNetworks().isEmpty() && suggested.isEmpty() == false)
    {
        // Offered, never written by itself: the operator says yes.
        QPushButton *useSuggested = new QPushButton(tr("Use these"), netGroup);
        useSuggested->setObjectName(QStringLiteral("settingsRigUseSuggested"));
        useSuggested->setToolTip(tr("Fill in the networks this show's patches were saved "
                                    "with: %1").arg(suggested.join(QStringLiteral(", "))));
        connect(useSuggested, &QPushButton::clicked, this, [this, suggested]() {
            m_rigNetworksEdit->setText(suggested.join(QStringLiteral(", ")));
        });
        rigRow->addWidget(useSuggested);
    }
    netLayout->addLayout(rigRow);
    m_rigNetworksError = new QLabel(netGroup);
    m_rigNetworksError->setObjectName(QStringLiteral("settingsRigNetworksError"));
    m_rigNetworksError->setStyleSheet(QStringLiteral("color: #e60000;"));
    m_rigNetworksError->setWordWrap(true);
    m_rigNetworksError->hide();
    netLayout->addWidget(m_rigNetworksError);
    m_rigOnlyCheck = new QCheckBox(tr("Only transmit on rig networks"), netGroup);
    m_rigOnlyCheck->setObjectName(QStringLiteral("settingsRigNetworkOnly"));
    m_rigOnlyCheck->setChecked(ioMap->transmitGateEnabled());
    m_rigOnlyCheck->setToolTip(tr("Network output is held -- kept, nothing sent -- while this "
                                  "machine is not on one of the show's rig networks, and "
                                  "resumes by itself when it is. Loopback, USB DMX and MIDI are "
                                  "not affected. A setting of this machine, not the show."));
    netLayout->addWidget(m_rigOnlyCheck);
    QHBoxLayout *anywayRow = new QHBoxLayout();
    m_transmitAnywayButton = new QPushButton(netGroup);
    m_transmitAnywayButton->setObjectName(QStringLiteral("settingsTransmitAnyway"));
    connect(m_transmitAnywayButton, &QPushButton::clicked, this, &AppSettings::slotTransmitAnyway);
    anywayRow->addWidget(m_transmitAnywayButton);
    anywayRow->addStretch();
    netLayout->addLayout(anywayRow);
    QLabel *netInfo = new QLabel(
        tr("Keeps a laptop on some other network (a school's, a venue office's) from putting "
           "show traffic on it."), netGroup);
    netInfo->setWordWrap(true);
    netInfo->setStyleSheet("color: gray; font-size: 10px;");
    netLayout->addWidget(netInfo);
    mainLayout->addWidget(netGroup);
    updateTransmitAnywayButton();

    // Add stretch to push buttons to bottom
    mainLayout->addStretch();

    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttonBox = buttonBox;
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    mainLayout->addWidget(buttonBox);
    connect(m_rigNetworksEdit, &QLineEdit::textChanged, this, &AppSettings::validateRigNetworks);
    validateRigNetworks();

    // Connect enable checkbox to enable/disable interval spinbox
    connect(m_autosaveEnabledCheck, &QCheckBox::toggled,
            m_autosaveIntervalSpin, &QSpinBox::setEnabled);
    m_autosaveIntervalSpin->setEnabled(m_autosaveEnabledCheck->isChecked());
}

AppSettings::~AppSettings()
{
}

void AppSettings::focusRigNetworks()
{
    m_rigNetworksEdit->setFocus(Qt::OtherFocusReason);
    m_rigNetworksEdit->selectAll();
}

void AppSettings::validateRigNetworks()
{
    QStringList bad, why;
    RigNetwork::parseList(m_rigNetworksEdit->text(), &bad, &why);
    QStringList lines;
    for (int i = 0; i < bad.count(); i++)
        lines << tr("%1: %2").arg(bad.at(i), why.value(i));
    m_rigNetworksError->setText(lines.join(QStringLiteral("\n")));
    m_rigNetworksError->setVisible(lines.isEmpty() == false);
    if (QPushButton *ok = m_buttonBox->button(QDialogButtonBox::Ok))
        ok->setEnabled(lines.isEmpty());
}

void AppSettings::updateTransmitAnywayButton()
{
    InputOutputMap *ioMap = m_app->doc()->inputOutputMap();
    if (ioMap->transmitOverride())
    {
        m_transmitAnywayButton->setText(tr("Check the rig network again"));
        m_transmitAnywayButton->setToolTip(tr("End \"Transmit anyway\": hold network output "
                                              "that is not on a rig network again."));
        m_transmitAnywayButton->setEnabled(true);
    }
    else
    {
        m_transmitAnywayButton->setText(tr("Transmit anyway, this session…"));
        m_transmitAnywayButton->setToolTip(tr("Send network output wherever this machine is "
                                              "connected, until it quits or another show is "
                                              "opened. Asks first; never saved."));
        m_transmitAnywayButton->setEnabled(ioMap->transmitGateActive());
    }
}

void AppSettings::slotTransmitAnyway()
{
    InputOutputMap *ioMap = m_app->doc()->inputOutputMap();
    if (ioMap->transmitOverride())
    {
        ioMap->setTransmitOverride(false);
    }
    else
    {
        const InputOutputMap::TransmitGateStatus st = ioMap->transmitGateStatus();
        const QMessageBox::StandardButton answer = QMessageBox::warning(this,
            tr("Transmit anyway"),
            tr("This machine is connected to: %1.\n\n"
               "The show's rig networks are: %2.\n\n"
               "Transmitting anyway sends this show's Art-Net, sACN and WLED output on "
               "whatever network this machine is on -- broadcasts included -- until "
               "qlcconsole quits or another show is opened. Transmit anyway?")
                .arg(st.connected.isEmpty() ? tr("no network") : st.connected.join(QStringLiteral(", ")),
                     st.networks.join(QStringLiteral(", "))),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        ioMap->setTransmitOverride(true);
    }
    updateTransmitAnywayButton();
}

void AppSettings::accept()
{
    // The show's rig networks (saved with it) and this machine's gate choice
    {
        QStringList bad;
        const QStringList nets = RigNetwork::parseList(m_rigNetworksEdit->text(), &bad);
        if (bad.isEmpty() == false)
        {
            validateRigNetworks();
            m_rigNetworksEdit->setFocus();
            return;
        }
        InputOutputMap *ioMap = m_app->doc()->inputOutputMap();
        if (nets != ioMap->rigNetworks())
        {
            ioMap->setRigNetworks(nets);
            m_app->doc()->setModified();
        }
        QSettings().setValue(App::rigNetworkGateSettingKey(), m_rigOnlyCheck->isChecked());
        ioMap->setTransmitGateEnabled(m_rigOnlyCheck->isChecked());
    }

    // Apply autosave settings
    m_app->setAutosaveEnabled(m_autosaveEnabledCheck->isChecked());
    m_app->setAutosaveInterval(m_autosaveIntervalSpin->value());

    // Apply tab label mode
    const int mode = m_tabLabelModeCombo->currentData().toInt();
    m_app->setTabLabelMode(mode);

    // Show Tree: what Record records when the choices differ.
    QSettings().setValue(ShowTreeView::recordSettingKey(false), m_recordDesignCombo->currentData().toString());
    QSettings().setValue(ShowTreeView::recordSettingKey(true), m_recordPerformCombo->currentData().toString());
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

