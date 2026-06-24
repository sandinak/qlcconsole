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

class QCheckBox;
class QComboBox;
class QSpinBox;
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

protected slots:
    void accept() override;

private:
    App *m_app;

    // Autosave widgets
    QCheckBox *m_autosaveEnabledCheck;
    QSpinBox  *m_autosaveIntervalSpin;

    // Tab label mode
    QComboBox *m_tabLabelModeCombo;
};

/** @} */

#endif // APPSETTINGS_H

