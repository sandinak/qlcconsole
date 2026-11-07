/*
  Q Light Controller Plus
  sprinklespansdialog.cpp

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

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QLabel>

#include "sprinklespansdialog.h"
#include "pixelpath.h"

namespace
{
enum Column { SpanCol = 0, FirstCol, LastCol, PointCol, Columns };
}

SprinkleSpansDialog::SprinkleSpansDialog(const QVector<SprinkleSpan> &spans, int leds, QWidget *parent)
    : QDialog(parent)
    , m_leds(leds)
{
    setObjectName(QStringLiteral("sprinkleSpansDialog"));
    setWindowTitle(tr("Sprinkle Spans"));

    QLabel *intro = new QLabel(tr("One row per span across the gap: its first and last LED on the "
                                  "string, and where round the wall it hangs (0% the stage-right "
                                  "end, 100% the stage-left end). LEDs in no span run along the "
                                  "wall top to the next one. No rows: spread evenly."), this);
    intro->setWordWrap(true);

    m_table = new QTableWidget(0, Columns, this);
    m_table->setObjectName(QStringLiteral("sprinkleSpansTable"));
    m_table->setHorizontalHeaderLabels(QStringList() << tr("Span") << tr("First LED")
                                                     << tr("Last LED") << tr("Point at (%)"));
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    foreach (const SprinkleSpan &sp, spans)
        addRow(sp);

    QPushButton *add = new QPushButton(tr("Add Span"), this);
    add->setObjectName(QStringLiteral("sprinkleAddSpan"));
    QPushButton *remove = new QPushButton(tr("Remove"), this);
    remove->setObjectName(QStringLiteral("sprinkleRemoveSpan"));
    QPushButton *clear = new QPushButton(tr("Clear (even spread)"), this);
    clear->setObjectName(QStringLiteral("sprinkleClearSpans"));
    connect(add, &QPushButton::clicked, this, [this]() {
        // After the last one: the next LEDs, as many as it has, at the same place.
        SprinkleSpan sp;
        const QVector<SprinkleSpan> now = this->spans();
        if (!now.isEmpty())
        {
            const SprinkleSpan &last = now.last();
            sp.firstLed = last.lastLed + 1;
            sp.lastLed = last.lastLed + (last.lastLed - last.firstLed + 1);
            sp.point = last.point;
        }
        else
            sp.lastLed = 10;
        if (m_leds > 0)
            sp.lastLed = qMin(sp.lastLed, m_leds);
        addRow(sp);
    });
    connect(remove, &QPushButton::clicked, this, [this]() {
        const int r = m_table->currentRow();
        if (r >= 0)
        {
            m_table->removeRow(r);
            renumber();
        }
    });
    connect(clear, &QPushButton::clicked, this, [this]() {
        m_table->setRowCount(0);
    });
    QHBoxLayout *rowBtns = new QHBoxLayout;
    rowBtns->addWidget(add);
    rowBtns->addWidget(remove);
    rowBtns->addStretch();
    rowBtns->addWidget(clear);

    m_error = new QLabel(this);
    m_error->setObjectName(QStringLiteral("sprinkleSpansError"));
    m_error->setStyleSheet(QStringLiteral("color:#e06060;"));
    m_error->setWordWrap(true);
    m_error->hide();

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &SprinkleSpansDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *vl = new QVBoxLayout(this);
    vl->addWidget(intro);
    vl->addWidget(m_table, 1);
    vl->addLayout(rowBtns);
    vl->addWidget(m_error);
    vl->addWidget(bb);
    resize(460, 380);
}

void SprinkleSpansDialog::addRow(const SprinkleSpan &sp)
{
    const int r = m_table->rowCount();
    m_table->insertRow(r);
    QTableWidgetItem *num = new QTableWidgetItem;
    num->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_table->setItem(r, SpanCol, num);
    const int top = m_leds > 0 ? m_leds : 100000;
    QSpinBox *first = new QSpinBox(m_table);
    first->setRange(1, top);
    first->setValue(sp.firstLed);
    QSpinBox *last = new QSpinBox(m_table);
    last->setRange(1, top);
    last->setValue(sp.lastLed);
    QDoubleSpinBox *at = new QDoubleSpinBox(m_table);
    at->setRange(0.0, 100.0);
    at->setDecimals(1);
    at->setSuffix(QStringLiteral(" %"));
    at->setValue(double(sp.point) * 100.0);
    m_table->setCellWidget(r, FirstCol, first);
    m_table->setCellWidget(r, LastCol, last);
    m_table->setCellWidget(r, PointCol, at);
    renumber();
}

void SprinkleSpansDialog::renumber()
{
    for (int r = 0; r < m_table->rowCount(); ++r)
        if (QTableWidgetItem *it = m_table->item(r, SpanCol))
            it->setText(QString::number(r + 1));
}

QVector<SprinkleSpan> SprinkleSpansDialog::spans() const
{
    QVector<SprinkleSpan> out;
    for (int r = 0; r < m_table->rowCount(); ++r)
    {
        auto *first = qobject_cast<QSpinBox *>(m_table->cellWidget(r, FirstCol));
        auto *last = qobject_cast<QSpinBox *>(m_table->cellWidget(r, LastCol));
        auto *at = qobject_cast<QDoubleSpinBox *>(m_table->cellWidget(r, PointCol));
        if (first == nullptr || last == nullptr || at == nullptr)
            continue;
        SprinkleSpan sp;
        sp.firstLed = first->value();
        sp.lastLed = last->value();
        sp.point = float(at->value() / 100.0);
        out << sp;
    }
    return out;
}

QString SprinkleSpansDialog::problem() const
{
    return PixelPath::checkSprinkleSpans(spans(), m_leds);
}

void SprinkleSpansDialog::accept()
{
    const QString why = problem();
    if (!why.isEmpty())
    {
        m_error->setText(why);
        m_error->show();
        return;
    }
    QDialog::accept();
}
