/*
  Q Light Controller Plus
  sprinklespansdialog.h

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

#ifndef SPRINKLESPANSDIALOG_H
#define SPRINKLESPANSDIALOG_H

#include <QDialog>
#include <QVector>

#include "truss.h"

class QTableWidget;
class QLabel;

/** @addtogroup ui UI
 * @{
 */

/**
 * The span table of a Sprinkle loops pixel path (PixelPath::SprinkleLoops):
 * one row per span across a fountain's gap -- its first and last LED and
 * where round the U it hangs. Checked before it is taken (no overlaps, on
 * the string); an empty table is the even layout.
 */
class SprinkleSpansDialog final : public QDialog
{
    Q_OBJECT

public:
    /** For a string of @p leds LEDs (0 = not known). */
    SprinkleSpansDialog(const QVector<SprinkleSpan> &spans, int leds, QWidget *parent = nullptr);

    /** The table as it stands (rows that do not read are left out). */
    QVector<SprinkleSpan> spans() const;

    /** What is wrong with it, empty when it can be taken. */
    QString problem() const;

public slots:
    void accept() override;

private:
    void addRow(const SprinkleSpan &sp);
    void renumber();

    QTableWidget *m_table;
    QLabel *m_error;
    int m_leds;
};

/** @} */

#endif
