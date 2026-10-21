/*
  Q Light Controller Plus
  vccuerunnerproperties.h

  Copyright (C) Branson Matheson

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

#ifndef VCCUERUNNERPROPERTIES_H
#define VCCUERUNNERPROPERTIES_H

#include <QDialog>
#include <QList>

#include "vccuerunner.h"

class InputSelectionWidget;
class QStackedWidget;
class QListWidget;
class QTabWidget;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class Doc;

/** @addtogroup ui_vc_props
 * @{
 */

/**
 * The Cue Runner widget's properties: General (caption, the list it plays,
 * its size, the parts it shows) and External Input (each action's input
 * source and key, with auto-detect and custom feedback, as a VC Cue
 * List's).
 */
class VCCueRunnerProperties final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(VCCueRunnerProperties)

public:
    VCCueRunnerProperties(VCCueRunner *runner, Doc *doc);
    ~VCCueRunnerProperties();

    /** The input and key editor of @a action (for the tests). */
    InputSelectionWidget *inputWidget(VCCueRunner::Action action) const;
    QTabWidget *tabs() const { return m_tabs; }

public slots:
    void accept() override;

private slots:
    void slotStopAutoDetection();

private:
    VCCueRunner *m_runner;
    Doc *m_doc;
    QTabWidget *m_tabs;
    QLineEdit *m_caption;
    QComboBox *m_follow;
    QComboBox *m_size;
    QSpinBox *m_upcoming;
    QCheckBox *m_timecode;
    QCheckBox *m_secondary;
    QCheckBox *m_subGo;
    QListWidget *m_actions;
    QStackedWidget *m_inputs;
    QList<InputSelectionWidget *> m_inputWidgets;
};

/** @} */

#endif
