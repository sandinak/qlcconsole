/*
  Q Light Controller Plus
  plotnumbering.h

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

#ifndef PLOTNUMBERING_H
#define PLOTNUMBERING_H

#include <QList>
#include <QDialog>

#include "doc.h"

class QSpinBox;
class QCheckBox;
class QComboBox;
class QTableWidget;

/** Number fixtures in plot order (Capture's "number by position"): Unit # and
 *  / or Channel from N, ordered stage right to left, upstage to downstage or
 *  along a truss, optionally patching them in that order into the next free
 *  addresses. Pure planning here; NumberInPlotOrderDialog previews and applies. */
namespace PlotNumbering
{
    enum Order { StageRightToLeft = 0, UpstageToDownstage = 1, AlongTruss = 2 };

    /** @p fids in @p order. Ties break by the other axis, then by id. */
    QList<quint32> ordered(Doc *doc, const QList<quint32> &fids, Order order);

    struct Row
    {
        quint32 fixture = 0;
        int unit = 0;               ///< 0 = leave as it is
        int channel = 0;            ///< 0 = leave as it is
        bool patch = false;
        quint32 universe = 0;
        quint32 address = 0;
    };

    /** The numbers and, with @p patch, the addresses each fixture would get.
     *  Patching starts at the lowest address the selection holds now (else
     *  universe 1, channel 1) and takes the next free run for each, never one
     *  another fixture holds and never past channel 512; a fixture that does
     *  not fit gets patch == false. */
    QList<Row> plan(Doc *doc, const QList<quint32> &fids, Order order,
                    bool setUnit, int firstUnit, bool setChannel, int firstChannel,
                    bool patch);

    /** Write the numbers; the patch as one Doc::patchFixtures() step. False
     *  with @p why when the doc refuses the patch (nothing changed then). */
    bool apply(Doc *doc, const QList<Row> &rows, QString *why = nullptr);
}

class NumberInPlotOrderDialog final : public QDialog
{
    Q_OBJECT
public:
    NumberInPlotOrderDialog(Doc *doc, const QList<quint32> &fids, QWidget *parent = nullptr);
    QList<PlotNumbering::Row> rows() const;

private slots:
    void refresh();

private:
    Doc *m_doc;
    QList<quint32> m_fids;
    QComboBox *m_order;
    QCheckBox *m_unitOn;
    QSpinBox *m_unit;
    QCheckBox *m_channelOn;
    QSpinBox *m_channel;
    QCheckBox *m_patchOn;
    QTableWidget *m_table;
};

#endif
