/*
  Q Light Controller Plus
  appsettings.h

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

#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QDialog>
#include <QMap>

class QCheckBox;
class QComboBox;
class QSpinBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QDialogButtonBox;
class QTabWidget;
class App;

/** @addtogroup ui UI
 * @{
 */

class AppSettings : public QDialog
{
    Q_OBJECT

public:
    AppSettings(App *app, QWidget *parent = nullptr);
    ~AppSettings();

    /** Put the cursor in the show's rig networks (the footer chip's click) */
    void focusRigNetworks();

    /** Open on the Advanced tab */
    void showAdvanced();

protected slots:
    void accept() override;

private slots:
    /** Check the rig networks as typed; OK only while they parse */
    void validateRigNetworks();
    /** "Transmit anyway, this session" -- confirmed, never saved -- or,
     *  while that is in force, turn the check back on */
    void slotTransmitAnyway();

private:
    App *m_app;
    QTabWidget *m_tabs = nullptr;
    QWidget *m_advancedPage = nullptr;
    // Advanced: the show's auto-open shutters (ShutterAutoOpen)
    QCheckBox *m_autoOpenCheck = nullptr;
    class QComboBox *m_onAtOpenCombo = nullptr;

    // Autosave widgets
    QCheckBox *m_autosaveEnabledCheck;
    QSpinBox  *m_autosaveIntervalSpin;

    // Appearance: theme, toolbar style (tab label mode), footer chips and
    // the speed masters' beat -- this computer's (was the View menu)
    QComboBox *m_themeCombo = nullptr;
    QComboBox *m_tabLabelModeCombo = nullptr;
    QMap<int, QCheckBox *> m_footerChecks;      ///< App::FooterChip -> its box
    QComboBox *m_beatStyleCombo = nullptr;
    QComboBox *m_beatWhenCombo = nullptr;
    QComboBox *m_workspaceViewCombo = nullptr;   ///< the show's view (View ▸ Workspace)
    // Show controls: top bar or status bar (App::ShowControlsPlacement)
    QComboBox *m_showControlsCombo;
    QComboBox *m_scriptEditorCombo;

    // Show Tree: Record as new cue, when the choices differ
    QComboBox *m_recordDesignCombo;
    QComboBox *m_recordPerformCombo;
    // Auto move-in-black: the show's (ProgrammerController), saved with it
    QCheckBox *m_mibCheck = nullptr;
    class QDoubleSpinBox *m_mibLeadSpin = nullptr;
    // Measurement units (MeasurementUnits::preferredLength)
    QComboBox *m_unitsCombo;

    // Network: the show's rig networks and the transmit gate
    QLineEdit *m_rigNetworksEdit;
    QLabel *m_rigNetworksError;
    QCheckBox *m_rigOnlyCheck;
    QPushButton *m_transmitAnywayButton;

    // Software Update (UpdateChecker): this machine's, not the show's
    QCheckBox *m_updateCheck = nullptr;
    QComboBox *m_updateChannel = nullptr;
    QCheckBox *m_updateAuto = nullptr;
    QDialogButtonBox *m_buttonBox;
    void updateTransmitAnywayButton();
};

/** @} */

#endif // APPSETTINGS_H

