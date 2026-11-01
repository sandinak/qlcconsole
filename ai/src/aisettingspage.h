/*
  Q Light Controller Plus - qlcconsole
  aisettingspage.h

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

#ifndef AISETTINGSPAGE_H
#define AISETTINGSPAGE_H

#include <QWidget>

#include "aisettings.h"

class QPushButton;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QLabel;

/** @addtogroup ai AI Assist
 * @{
 */

/**
 * Settings ▸ AI Assist (docs/design/AI_ASSIST.md section 5): the toggle,
 * provider, endpoint, model, API key (cloud providers; kept in the
 * keychain), what is sent, the timeout and Test connection -- with a plain
 * note whenever the show's context would leave this computer. One page of
 * the app's Settings dialog; apply() on OK.
 */
class AiSettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit AiSettingsPage(QWidget *parent = nullptr);

    /** The settings as typed (not saved yet). */
    AiSettings settings() const;
    /** Save them (and the key, to the keychain); AI Assist rereads them. */
    void apply();

private slots:
    void slotProviderChanged();
    void updateMcpNote();
    void slotTest();
    void updateState();

private:
    QCheckBox *m_enabled;
    QComboBox *m_provider;
    QLineEdit *m_endpoint;
    QComboBox *m_model;
    QLineEdit *m_key;
    QLabel *m_keyLabel;
    QComboBox *m_context;
    QSpinBox *m_timeout;
    QPushButton *m_test;
    QLabel *m_testResult;
    QLabel *m_cloudNote;
    /** The MCP server (P3). */
    QCheckBox *m_mcp;
    QCheckBox *m_mcpChanges;
    QLabel *m_mcpNote;
    QString m_lastProvider;
    bool m_keyEdited = false;
};

/** @} */

#endif
