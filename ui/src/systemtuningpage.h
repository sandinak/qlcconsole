/*
  Q Light Controller Plus - qlcconsole
  systemtuningpage.h

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

#ifndef SYSTEMTUNINGPAGE_H
#define SYSTEMTUNINGPAGE_H

#include <QWidget>
#include <functional>

#include "systemtuning.h"
#include "qlcui_export.h"

class QCheckBox;
class QTreeWidget;
class QLabel;
class QPlainTextEdit;
class QPushButton;

/** @addtogroup ui UI
 * @{
 */

/**
 * Settings ▸ Performance: the "during Operate" settings, and this
 * computer's check (SystemTuning::check()) -- each item's status, and for
 * the selected one what it does, and the command to copy or the steps.
 */
class QLC_UI_EXPORT SystemTuningPage final : public QWidget
{
    Q_OBJECT

public:
    explicit SystemTuningPage(Doc *doc, QWidget *parent = nullptr);

    /** Save the settings and apply them (the dialog's OK) */
    void apply();

    /** Tests: what the check looks at */
    void setProbe(const std::function<SystemTuning::Probe()> &probeFn);

    /** Run the check again (the "Check again" button) */
    void refresh();

    /** Copy the selected item's command to the clipboard */
    void copyCommand();

    QTreeWidget *itemsTree() const { return m_tree; }
    QCheckBox *fullSpeedCheck() const { return m_fullSpeed; }
    QCheckBox *displayCheck() const { return m_display; }
    QCheckBox *warnCheck() const { return m_warn; }
    QPlainTextEdit *commandEdit() const { return m_command; }
    QPushButton *copyButton() const { return m_copy; }
    QLabel *fullCheckLabel() const { return m_fullCheck; }

private:
    void showSelected();
    /** The list again from what was last found, without looking again */
    void showFound();

private:
    Doc *m_doc;
    std::function<SystemTuning::Probe()> m_probe;
    SystemTuning::Probe m_found;
    QList<SystemTuning::Item> m_items;

    QCheckBox *m_fullSpeed;
    QCheckBox *m_display;
    QCheckBox *m_warn;
    QTreeWidget *m_tree;
    QLabel *m_what;
    QLabel *m_howTo;
    QPlainTextEdit *m_command;
    QPushButton *m_copy;
    QLabel *m_fullCheck;
};

/** @} */

#endif
