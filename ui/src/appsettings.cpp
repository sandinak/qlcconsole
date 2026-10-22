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
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QLineEdit>

#include <QFormLayout>
#include <QTabWidget>

#include "appsettings.h"
#include "controllertestdialog.h"
#include "showtreeview.h"
#include "measurementunits.h"
#include "monitor.h"
#include "inputoutputmap.h"
#include "rignetwork.h"
#include "updatechecker.h"
#include "shutterautoopen.h"
#include "outputrulekeeper.h"
#include "programmercontroller.h"
#include "doc.h"
#include "app.h"

AppSettings::AppSettings(App *app, QWidget *parent)
    : QDialog(parent)
    , m_app(app)
{
    Q_ASSERT(app != nullptr);

    setWindowTitle(tr("Application Settings"));
    setMinimumWidth(400);

    // Two tabs: General, what everyone sets, and Advanced -- settings for
    // the experienced, and the show's output behaviour (auto-open shutters).
    QVBoxLayout *dialogLayout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("settingsTabs"));
    dialogLayout->addWidget(m_tabs);
    QWidget *generalPage = new QWidget(m_tabs);
    generalPage->setObjectName(QStringLiteral("settingsGeneral"));
    QVBoxLayout *mainLayout = new QVBoxLayout(generalPage);
    m_tabs->addTab(generalPage, tr("General"));
    QWidget *advancedPage = new QWidget(m_tabs);
    advancedPage->setObjectName(QStringLiteral("settingsAdvanced"));
    advancedPage->setProperty("helpTopic", QStringLiteral("other-improvements.md#advanced-settings"));
    QVBoxLayout *advLayout = new QVBoxLayout(advancedPage);
    m_tabs->addTab(advancedPage, tr("Advanced"));

    // --- Advanced: Output (this show) ---
    // The show's, saved with it: it changes what the rig does.
    QGroupBox *outGroup = new QGroupBox(tr("Output (saved with this show)"), advancedPage);
    outGroup->setObjectName(QStringLiteral("settingsOutput"));
    outGroup->setProperty("helpTopic", QStringLiteral("other-improvements.md#auto-open-shutters"));
    QVBoxLayout *outLayout = new QVBoxLayout(outGroup);
    m_autoOpenCheck = new QCheckBox(tr("Auto-open shutters when a light is given a level"), outGroup);
    m_autoOpenCheck->setObjectName(QStringLiteral("settingsAutoOpenShutters"));
    m_autoOpenCheck->setChecked(m_app->doc()->shutterAutoOpen()->isEnabled());
    m_autoOpenCheck->setToolTip(tr("A light given a level by a look, a cue or a slider, whose "
                                   "shutter nothing sets, has its shutter opened -- so a spot "
                                   "whose shutter rests closed lights from a dimmer look alone. "
                                   "A shutter look (Closed, Strobe...) always wins, and a light "
                                   "with no level is left alone. Saved with the show."));
    outLayout->addWidget(m_autoOpenCheck);
    QLabel *outInfo = new QLabel(
        tr("Like a lighting console's auto-open. Off: a closed shutter stays closed until a "
           "look opens it (Shutter → Open), as in QLC+."), outGroup);
    outInfo->setWordWrap(true);
    outInfo->setStyleSheet("color: gray; font-size: 10px;");
    outLayout->addWidget(outInfo);
    // Output rules: when the "On when not in use" relays and lights come on.
    {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *lbl = new QLabel(tr("\"On when not in use\" fixtures come on:"), outGroup);
        m_onAtOpenCombo = new QComboBox(outGroup);
        m_onAtOpenCombo->setObjectName(QStringLiteral("settingsOnAtOpenWhen"));
        m_onAtOpenCombo->addItem(tr("When you switch to Operate"), int(OutputRuleKeeper::OnAtOperate));
        m_onAtOpenCombo->addItem(tr("When the show opens"), int(OutputRuleKeeper::OnAtShowOpen));
        m_onAtOpenCombo->setCurrentIndex(m_onAtOpenCombo->findData(
            int(m_app->doc()->outputRuleKeeper()->onAtOpenWhen())));
        m_onAtOpenCombo->setToolTip(tr("Power relays and house or work lights set to \"On when not "
                                       "in use\" (Output Rules…) go to full whenever nothing in the "
                                       "show sets them -- from the first switch to Operate (the "
                                       "safer choice), or as soon as the show is open. Saved with "
                                       "the show."));
        row->addWidget(lbl);
        row->addWidget(m_onAtOpenCombo, 1);
        outLayout->addLayout(row);
    }
    advLayout->addWidget(outGroup);

    // --- Advanced: Game controllers ---
    QGroupBox *padGroup = new QGroupBox(tr("Game controllers"), advancedPage);
    padGroup->setObjectName(QStringLiteral("settingsControllers"));
    padGroup->setProperty("helpTopic", QStringLiteral("lighting-studio.md#controller-test"));
    QHBoxLayout *padLayout = new QHBoxLayout(padGroup);
    QLabel *padInfo = new QLabel(tr("See every input a controller sends and what it maps to."),
                                 padGroup);
    padInfo->setWordWrap(true);
    padLayout->addWidget(padInfo, 1);
    QPushButton *padTest = new QPushButton(tr("Controller test\342\200\246"), padGroup);
    padTest->setObjectName(QStringLiteral("settingsControllerTest"));
    padTest->setAutoDefault(false);
    connect(padTest, &QPushButton::clicked, this, [this]() {
        ControllerTestDialog::open(m_app->doc(), this);
    });
    padLayout->addWidget(padTest);
    advLayout->addWidget(padGroup);

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
    // Which tabs show (View ▸ Workspace): the show's, saved with it.
    tabLayout->addSpacing(12);
    tabLayout->addWidget(new QLabel(tr("View (this show):"), tabGroup));
    m_workspaceViewCombo = new QComboBox(tabGroup);
    m_workspaceViewCombo->setObjectName(QStringLiteral("settingsWorkspaceView"));
    for (App::WorkspaceView v : { App::ClassicView, App::ConsoleView, App::EverythingView })
        m_workspaceViewCombo->addItem(App::workspaceViewName(v), int(v));
    m_workspaceViewCombo->setCurrentIndex(m_workspaceViewCombo->findData(int(m_app->workspaceView())));
    m_workspaceViewCombo->setToolTip(tr("Which tabs this show opens with (View ▸ Workspace): "
                                        "Classic QLC+, qlcconsole or Everything. Saved with the show; "
                                        "only the tabs change."));
    tabLayout->addWidget(m_workspaceViewCombo);
    tabLayout->addStretch();
    mainLayout->addWidget(tabGroup);

    // --- Show controls: Stop all, Blackout, Blind, Design | Operate ---
    // One row at the top with the tab's own tools (the default), or in the
    // status bar, leaving the top to the tab. Applied on OK, no restart.
    QGroupBox *scGroupBox = new QGroupBox(tr("Show Controls"), this);
    scGroupBox->setProperty("helpTopic", QStringLiteral("getting-started.md#finding-your-way-around"));
    QHBoxLayout *showCtlLayout = new QHBoxLayout(scGroupBox);
    showCtlLayout->addWidget(new QLabel(tr("Stop all, Blackout, Blind and Design | Operate:"), scGroupBox));
    m_showControlsCombo = new QComboBox(scGroupBox);
    m_showControlsCombo->setObjectName(QStringLiteral("settingsShowControls"));
    m_showControlsCombo->addItem(tr("In the top bar, beside the tab's tools"), App::ShowControlsInTopBar);
    m_showControlsCombo->addItem(tr("In the status bar"), App::ShowControlsInStatusBar);
    m_showControlsCombo->setCurrentIndex(qMax(0, m_showControlsCombo->findData(m_app->showControlsPlacement())));
    m_showControlsCombo->setToolTip(tr("The top bar: one row above the tabs, the tab's own tools on the "
                                       "left and the show controls on the right. The status bar: the "
                                       "show controls at the right end of the bottom edge, beside "
                                       "Ready and the other show chips, and the top is the tab's alone. "
                                       "The shortcuts are the same either way."));
    showCtlLayout->addWidget(m_showControlsCombo);
    showCtlLayout->addStretch();
    mainLayout->addWidget(scGroupBox);

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
    QGroupBox *fxGroup = new QGroupBox(tr("Effects"), advancedPage);
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
    advLayout->addWidget(fxGroup);

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
    // Auto move-in-black: the show's, saved with it -- the same one setting as
    // the Show Tree's MIB button and Programming's Move in black menu.
    {
        ProgrammerController *pc = m_app->doc()->programmer();
        QWidget *mibRow = new QWidget(stGroup);
        QHBoxLayout *mibLayout = new QHBoxLayout(mibRow);
        mibLayout->setContentsMargins(0, 0, 0, 0);
        m_mibCheck = new QCheckBox(tr("Auto move-in-black"), mibRow);
        m_mibCheck->setObjectName(QStringLiteral("settingsMoveInBlack"));
        m_mibCheck->setChecked(pc != nullptr && pc->isAutoMoveInBlack());
        m_mibCheck->setToolTip(tr("While the show runs, look ahead to the next cue and pre-position any "
                                  "dark mover it will reveal (pan/tilt, colour, gobo, beam). Saved with "
                                  "the show; the same switch as the Show Tree's MIB button and "
                                  "Programming → Move in black."));
        mibLayout->addWidget(m_mibCheck);
        mibLayout->addSpacing(12);
        mibLayout->addWidget(new QLabel(tr("Lead time:"), mibRow));
        m_mibLeadSpin = new QDoubleSpinBox(mibRow);
        m_mibLeadSpin->setObjectName(QStringLiteral("settingsMoveInBlackLead"));
        m_mibLeadSpin->setRange(0.1, 10.0);
        m_mibLeadSpin->setSingleStep(0.1);
        m_mibLeadSpin->setDecimals(1);
        m_mibLeadSpin->setSuffix(tr(" s"));
        m_mibLeadSpin->setValue((pc != nullptr ? pc->moveInBlackDarkGapMs() : 800) / 1000.0);
        m_mibLeadSpin->setToolTip(tr("The next cue must be at least this far away for a dark mover to "
                                     "be pre-set. Below it there's no time to move in black."));
        mibLayout->addWidget(m_mibLeadSpin);
        mibLayout->addStretch();
        stLayout->addRow(tr("Move in black:"), mibRow);
        QLabel *mibNote = new QLabel(tr("Move in black is saved with this show, not this computer: "
                                        "another show keeps its own."), stGroup);
        mibNote->setObjectName(QStringLiteral("settingsMoveInBlackNote"));
        mibNote->setWordWrap(true);
        mibNote->setStyleSheet("color: gray; font-size: 10px;");
        stLayout->addRow(QString(), mibNote);
    }
    mainLayout->addWidget(stGroup);

    // --- Show control (QLab) ---
    // Edited in one place: Connections' Show control & sync page, next to
    // the MIDI inputs MSC and timecode arrive on. This just points there.
    QGroupBox *scGroup = new QGroupBox(tr("Show control (QLab)"), advancedPage);
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
    advLayout->addWidget(scGroup);
    advLayout->addStretch();

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

    // --- Software Update ---
    // GitHub Releases, this machine's choice. IT can turn it off for every
    // account (UpdateChecker::isDisabledByPolicy()); then this says so.
    QGroupBox *updGroup = new QGroupBox(tr("Software Update"), this);
    updGroup->setObjectName(QStringLiteral("settingsUpdates"));
    updGroup->setProperty("helpTopic", QStringLiteral("updates.md"));
    QVBoxLayout *updLayout = new QVBoxLayout(updGroup);
    m_updateCheck = new QCheckBox(tr("Check for updates (once a day, from GitHub)"), updGroup);
    m_updateCheck->setObjectName(QStringLiteral("settingsUpdateCheck"));
    m_updateCheck->setChecked(UpdateChecker::checkEnabled());
    m_updateCheck->setToolTip(tr("Asks GitHub's list of qlcconsole releases -- nothing is sent "
                                 "about this computer or the show."));
    updLayout->addWidget(m_updateCheck);
    QHBoxLayout *chanRow = new QHBoxLayout();
    chanRow->addWidget(new QLabel(tr("Channel:"), updGroup));
    m_updateChannel = new QComboBox(updGroup);
    m_updateChannel->setObjectName(QStringLiteral("settingsUpdateChannel"));
    m_updateChannel->addItem(tr("Stable releases"), int(UpdateChecker::Stable));
    m_updateChannel->addItem(tr("Pre-releases too (e.g. Show Tree)"), int(UpdateChecker::PreRelease));
    m_updateChannel->setCurrentIndex(m_updateChannel->findData(int(UpdateChecker::channel())));
    chanRow->addWidget(m_updateChannel);
    chanRow->addStretch();
    updLayout->addLayout(chanRow);
    m_updateAuto = new QCheckBox(tr("Install updates automatically"), updGroup);
    m_updateAuto->setObjectName(QStringLiteral("settingsUpdateAuto"));
    m_updateAuto->setChecked(UpdateChecker::autoInstall());
    m_updateAuto->setToolTip(tr("Download and check an update in the background, and install it "
                                "the next time you quit qlcconsole -- never during a show. "
                                "Off: the footer says an update is there and you choose."));
    updLayout->addWidget(m_updateAuto);
    connect(m_updateCheck, &QCheckBox::toggled, m_updateChannel, &QWidget::setEnabled);
    connect(m_updateCheck, &QCheckBox::toggled, m_updateAuto, &QWidget::setEnabled);
    m_updateChannel->setEnabled(m_updateCheck->isChecked());
    m_updateAuto->setEnabled(m_updateCheck->isChecked());
    if (UpdateChecker::isDisabledByPolicy())
    {
        updGroup->setEnabled(false);
        QLabel *off = new QLabel(tr("Turned off for this computer by its administrator."), updGroup);
        off->setObjectName(QStringLiteral("settingsUpdatePolicy"));
        updLayout->addWidget(off);
    }
    mainLayout->addWidget(updGroup);

    // Add stretch to push buttons to bottom
    mainLayout->addStretch();

    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttonBox = buttonBox;
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    dialogLayout->addWidget(buttonBox);
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

void AppSettings::showAdvanced()
{
    m_tabs->setCurrentIndex(1);
}

void AppSettings::focusRigNetworks()
{
    m_tabs->setCurrentIndex(0);
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

    // Auto-open shutters: the show's (ShutterAutoOpen marks it modified)
    m_app->doc()->shutterAutoOpen()->setEnabled(m_autoOpenCheck->isChecked());
    // When "On when not in use" fixtures come on: the show's (marks it modified)
    if (m_onAtOpenCombo != nullptr)
        m_app->doc()->outputRuleKeeper()->setOnAtOpenWhen(
            OutputRuleKeeper::OnAtOpenWhen(m_onAtOpenCombo->currentData().toInt()));

    // Apply autosave settings
    m_app->setAutosaveEnabled(m_autosaveEnabledCheck->isChecked());
    m_app->setAutosaveInterval(m_autosaveIntervalSpin->value());

    // Apply tab label mode
    const int mode = m_tabLabelModeCombo->currentData().toInt();
    m_app->setTabLabelMode(mode);

    // The show's view (marks the show modified only when it changes)
    m_app->setWorkspaceView(App::WorkspaceView(m_workspaceViewCombo->currentData().toInt()));

    // Where the show controls sit (live)
    m_app->setShowControlsPlacement(m_showControlsCombo->currentData().toInt());

    // Show Tree: what Record records when the choices differ.
    QSettings().setValue(ShowTreeView::recordSettingKey(false), m_recordDesignCombo->currentData().toString());
    QSettings().setValue(ShowTreeView::recordSettingKey(true), m_recordPerformCombo->currentData().toString());
    // Move in black: the show's (the controller marks it modified on a change)
    if (ProgrammerController *pc = m_app->doc()->programmer())
    {
        pc->setMoveInBlackDarkGapMs(int(m_mibLeadSpin->value() * 1000.0 + 0.5));
        pc->setAutoMoveInBlack(m_mibCheck->isChecked());
    }
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

    if (UpdateChecker::isDisabledByPolicy() == false)
    {
        UpdateChecker::setCheckEnabled(m_updateCheck->isChecked());
        UpdateChecker::setChannel(UpdateChecker::Channel(m_updateChannel->currentData().toInt()));
        UpdateChecker::setAutoInstall(m_updateAuto->isChecked());
    }

    QDialog::accept();
}

