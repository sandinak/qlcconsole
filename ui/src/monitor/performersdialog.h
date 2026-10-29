/*
  Q Light Controller Plus
  performersdialog.h

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

#ifndef PERFORMERSDIALOG_H
#define PERFORMERSDIALOG_H

#include <QDialog>
#include <QList>

#include "performerfill.h"
#include "../qlcui_export.h"

class MonitorProperties;
class ColourSwatchButton;
class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QLabel;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

/** "Fill with People": how many to a row, how tall, what they wear. Says
 *  how many that comes to on the risers chosen before anything is made. */
class QLC_UI_EXPORT PerformersDialog final : public QDialog
{
    Q_OBJECT

public:
    PerformersDialog(MonitorProperties *props, const QList<quint32> &platformIds,
                     QWidget *parent = nullptr);

    PerformerFill::Options options() const;
    /** How many people the options give on the chosen risers. */
    int count() const;

private:
    void refreshCount();

    MonitorProperties *m_props;
    QList<quint32> m_platformIds;
    bool m_feet = false;
    QLineEdit *m_name;
    QComboBox *m_by;
    QDoubleSpinBox *m_spacing;
    QSpinBox *m_perRow;
    QCheckBox *m_stagger;
    QComboBox *m_height;
    ColourSwatchButton *m_costume;
    QCheckBox *m_replace;
    QLabel *m_count;
};

/** @} */

#endif
