/*
  Q Light Controller Plus
  universeusagewidget.cpp

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

#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <algorithm>

#include "universeusagewidget.h"
#include "inputoutputmap.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"

#define KCols 32

// Deterministic pastel colour per fixture ID, so the same fixture always
// reads as the same colour across cells (and matches the legend swatch).
static QColor fixtureColor(quint32 fixtureId)
{
    return QColor::fromHsv(int((fixtureId * 47) % 360), 140, 235);
}

UniverseUsageWidget::UniverseUsageWidget(Doc *doc, quint32 universe, QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *top = new QVBoxLayout(this);

    const QString uniName = doc->inputOutputMap()->getUniverseNameByID(universe);
    QLabel *title = new QLabel(tr("<H2>%1 — Address Usage</H2>").arg(uniName), this);
    top->addWidget(title);

    // 512-address grid, 16 rows x 32 columns, one cell per DMX channel.
    QFrame *gridFrame = new QFrame(this);
    QGridLayout *grid = new QGridLayout(gridFrame);
    grid->setSpacing(2);

    QList<Fixture*> present; // fixtures seen on this universe, for the legend
    int used = 0;

    for (int ch = 0; ch < 512; ch++)
    {
        const quint32 fid = doc->fixtureForAddress(universe * 512 + quint32(ch));
        QLabel *cell = new QLabel(gridFrame);
        cell->setFixedSize(20, 20);
        cell->setAlignment(Qt::AlignCenter);

        if (fid == Fixture::invalidId())
        {
            cell->setStyleSheet(
                "QLabel { background: palette(base); border: 1px solid palette(mid); }");
            cell->setToolTip(tr("Ch %1 — free").arg(ch + 1));
        }
        else
        {
            used++;
            Fixture *fxi = doc->fixture(fid);
            cell->setStyleSheet(QString(
                "QLabel { background: %1; border: 1px solid palette(mid); }")
                .arg(fixtureColor(fid).name()));

            QString chName;
            if (fxi != NULL)
            {
                const QLCChannel *qc = fxi->channel(quint32(ch) - fxi->address());
                if (qc != NULL)
                    chName = qc->name();
                if (present.contains(fxi) == false)
                    present << fxi;
            }
            cell->setToolTip(chName.isEmpty()
                ? tr("Ch %1 — %2").arg(ch + 1).arg(fxi != NULL ? fxi->name() : tr("Unknown"))
                : tr("Ch %1 — %2 (%3)").arg(ch + 1).arg(fxi != NULL ? fxi->name() : tr("Unknown"), chName));
        }

        grid->addWidget(cell, ch / KCols, ch % KCols);
    }

    QLabel *summary = new QLabel(
        tr("%1 / 512 addresses used (%2%)").arg(used).arg(int(used * 100.0 / 512.0)), this);
    top->addWidget(summary);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidget(gridFrame);
    scroll->setWidgetResizable(true);
    top->addWidget(scroll, 1);

    // Legend: one row per fixture present, sorted by starting address.
    std::sort(present.begin(), present.end(), [](Fixture *a, Fixture *b) {
        return a->address() < b->address();
    });

    QFrame *legendFrame = new QFrame(this);
    QGridLayout *legend = new QGridLayout(legendFrame);
    int row = 0;
    foreach (Fixture *fxi, present)
    {
        QLabel *swatch = new QLabel(legendFrame);
        swatch->setFixedSize(14, 14);
        swatch->setStyleSheet(QString("background: %1; border: 1px solid palette(mid);")
            .arg(fixtureColor(fxi->id()).name()));
        legend->addWidget(swatch, row, 0);

        const QString range = fxi->channels() > 1
            ? tr("%1–%2").arg(fxi->address() + 1).arg(fxi->address() + fxi->channels())
            : QString::number(fxi->address() + 1);
        legend->addWidget(new QLabel(tr("%1  (Ch %2)").arg(fxi->name(), range), legendFrame), row, 1);
        row++;
    }
    legend->setColumnStretch(1, 1);

    QScrollArea *legendScroll = new QScrollArea(this);
    legendScroll->setWidget(legendFrame);
    legendScroll->setWidgetResizable(true);
    legendScroll->setMaximumHeight(160);
    top->addWidget(legendScroll);
}
