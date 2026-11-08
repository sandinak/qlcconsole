/*
  Q Light Controller Plus
  stringanchorsdialog.h

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

#ifndef STRINGANCHORSDIALOG_H
#define STRINGANCHORSDIALOG_H

#include <QDialog>
#include <QVector>

#include "truss.h"

class QTableWidget;
class QLabel;

/** @addtogroup ui UI
 * @{
 */

/**
 * The anchors of a Free string pixel path (PixelPath::FreeString), as a
 * table for exact values: one row per anchor in the order the string runs
 * -- where it is across and up or down the stage and how high (in the unit
 * Settings > Units picks), the LED pinned there, and whether the span from
 * it is reversed. Checked before it is taken (pinned LEDs count up along
 * the string, none past its end).
 */
class StringAnchorsDialog final : public QDialog
{
    Q_OBJECT

public:
    /** For a string of @p leds LEDs (0 = not known); a new row is put
     *  @p height high (the fixture's own height). */
    StringAnchorsDialog(const QVector<StringAnchor> &anchors, int leds, float height,
                        QWidget *parent = nullptr);

    /** The table as it stands. */
    QVector<StringAnchor> anchors() const;

    /** What is wrong with it, empty when it can be taken. */
    QString problem() const;

public slots:
    void accept() override;

private:
    void addRow(const StringAnchor &a);
    void renumber();

    QTableWidget *m_table;
    QLabel *m_error;
    int m_leds;
    float m_height;
};

/** @} */

#endif
