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
class QFormLayout;
class QLabel;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

/** People on the stage. With risers: "Fill with People" -- how many to a
 *  row, and a performer line per tier, counted before anything is made.
 *  Without: one performer line -- how many on it. Either way how tall
 *  (a mean, give or take) and what they wear. */
class QLC_UI_EXPORT PerformersDialog final : public QDialog
{
    Q_OBJECT

public:
    PerformersDialog(MonitorProperties *props, const QList<quint32> &platformIds,
                     QWidget *parent = nullptr);

    PerformerFill::Options options() const;
    /** How many people the options give on the chosen risers (fill), or on
     *  the line. */
    int count() const;
    /** True when it fills risers; false for one line. */
    bool fills() const { return !m_platformIds.isEmpty(); }
    /** People on the one line (not filling). */
    int lineCount() const;

private:
    void refreshCount();

    MonitorProperties *m_props;
    QList<quint32> m_platformIds;
    bool m_feet = false;
    QLineEdit *m_name;
    QComboBox *m_by;
    QDoubleSpinBox *m_spacing;
    QSpinBox *m_perRow;
    QSpinBox *m_lineCount;
    QCheckBox *m_stagger;
    QComboBox *m_heightPreset;
    QDoubleSpinBox *m_height;
    QDoubleSpinBox *m_spread;
    ColourSwatchButton *m_costume;
    QCheckBox *m_replace;
    QLabel *m_count;
};

/** @} */

#endif
